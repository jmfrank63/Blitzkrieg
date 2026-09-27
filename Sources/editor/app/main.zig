//! MapEditor:
//!   MapEditor [<map>]                       interactive
//!   MapEditor --check <map> [<out.tga>]     headless host check
//!   MapEditor --smoke <map> [<out.bzm>]     scripted run of the real loop
//!
//! The interactive mode opens a visible window, starts the engine on it,
//! opens <map> if one was given, and runs view.View's camera and tools under
//! panels.zig's panels until the window closes or File > Quit. A step that fails before there is a window to show
//! anything in (SDL, the window, the engine, ImGui, or the map itself) is
//! reported through SDL_ShowSimpleMessageBox, naming the step, and exits
//! non-zero; --check has no window to show a dialog over, so it keeps
//! printing to stderr instead.
//!
//! The host check starts the engine hidden with ImGui over it, opens the
//! map, draws frames with a magenta ImGui window at a known place, captures
//! one frame as it was presented and checks that both the panel and the map
//! are in it, printing "map-editor: host check PASS (<driver>, <w>x<h>)".
//! Then the panel smoke: the real panels drawn over the map with a State
//! from the opened map, and the file actions a dialog would start - Save As
//! to the output's directory, and opening that file again - run without the
//! dialog, printing "map-editor: panel smoke PASS (...)". Exits 0 when both
//! passed, or prints a "FAIL:" line naming what was wrong and exits 1.
//!
//! The smoke runs the interactive mode's own loop (`run`) with the window
//! hidden and smoke.zig's script feeding it synthetic SDL events: the tools
//! chosen by key, a brush stroke, an object placed, one of the map's
//! selected by a click, the placed one clicked, turned, dragged and deleted,
//! all of it undone, Save As to <out.bzm> (deleted first, so an old file
//! cannot pass) and that file opened again with the original's object count.
//! Prints "map-editor: smoke PASS"
//! and exits 0, or a "smoke FAIL:" line naming the step and exits 1.
const std = @import("std");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const core = @import("editor_core");
const host_mod = @import("host.zig");
const c_bridge = @import("c_bridge.zig");
const view_mod = @import("view.zig");
const view_math = @import("view_math.zig");
const panels = @import("panels.zig");
const panels_logic = @import("panels_logic.zig");
const crt = @import("crt.zig");
const smoke = @import("smoke.zig");
const c = host_mod.c;

const default_output = "zig-out/local-test/map-editor-check.tga";
const default_smoke_output = "zig-out/local-test/map-editor-smoke.bzm";

/// The probe window, in screen pixels (a window point is a screen pixel).
const probe = struct {
    const x = 40;
    const y = 40;
    const w = 120;
    const h = 80;
};

const probe_frames = 10;

/// A tile's side in world units: fWorldCellSize (Formats/fmtTerrain.h), 32 * sqrt(2).
/// Shared with view.zig so the two never drift apart.
const world_cell_size: f32 = view_mod.world_cell_size;

pub fn main(minimal: std.process.Init.Minimal) !void {
    crt.routeCrtReportsToStderr();
    const gpa = std.heap.smp_allocator;
    const io = std.Io.Threaded.global_single_threaded.io();

    var args = try std.process.Args.Iterator.initAllocator(minimal.args, gpa);
    defer args.deinit();
    _ = args.next();
    const first = args.next();
    if (first) |arg| {
        if (std.mem.eql(u8, arg, "--check")) {
            const map = args.next() orelse usage();
            const output = args.next() orelse default_output;
            if (args.next() != null) usage();
            const passed = try check(gpa, io, map, output);
            std.process.exit(if (passed) 0 else 1);
        }
        if (std.mem.eql(u8, arg, "--smoke")) {
            const map = args.next() orelse usage();
            const output = args.next() orelse default_smoke_output;
            if (args.next() != null) usage();
            const passed = try smokeRun(gpa, io, map, output);
            std.process.exit(if (passed) 0 else 1);
        }
        if (args.next() != null) usage();
        try interactive(gpa, arg);
        return;
    }
    try interactive(gpa, null);
}

/// The interactive mode: one window, the engine on it, the view driving the
/// core's tools, until the window closes or the process is asked to quit
/// (SDL maps SIGINT/SIGTERM to SDL_EVENT_QUIT by default).
fn interactive(gpa: std.mem.Allocator, map: ?[]const u8) !void {
    var host = host_mod.Host.start(.{ .title = "Map Editor" }) catch |err|
        fatal(startupStepName(err), @errorName(err));
    defer host.stop();

    var real = c_bridge.RealBridge.init(host.session);
    var editor = core.editor.Editor.init(gpa, real.bridge());
    defer editor.deinit();
    var view = view_mod.View.init(gpa);
    defer view.deinit(gpa);

    if (map) |path| {
        editor.open(path) catch {
            const reason = editor.status();
            fatal("map open", if (reason.len != 0) reason else "the map did not open");
        };
    }
    // Centres the view on the map opened above, if any (State.mapOpened).
    var state = panels.State.init(gpa, &editor, &view, &real, host.window);
    defer state.deinit();

    run(&host, &editor, &view, &real, &state, null);
    // A plain return, not std.process.exit: the deferred host.stop(),
    // editor.deinit() and view.deinit() above must run so the engine and its
    // GPU device shut down cleanly, which an immediate process exit would
    // skip.
}

/// The app's loop, shared by the interactive mode and --smoke: events to
/// ImGui and then, if ImGui does not want them, to the view; the view's
/// per-frame scrolling; a frame of the panels over the map; the panels' file
/// actions. With a script, the script pushes its synthetic events before
/// each frame's poll and checks what they did after it, and ends the loop
/// when it is done or has failed.
fn run(host: *host_mod.Host, editor: *core.editor.Editor, view: *view_mod.View, real: *c_bridge.RealBridge, state: *panels.State, script: ?*smoke.Script) void {
    var running = true;
    var last_ticks: u64 = sdl3.c.SDL_GetTicks();
    while (running) {
        if (script) |s| if (!s.beforeFrame()) break;
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) {
            // ImGui's backend always gets first look, so it can update its
            // own IO state (and follow a resize) - but its bool return is
            // ImGui_ImplSDL3_ProcessEvent's "I processed this", true for
            // every mouse/keyboard event on our window, not "I want this".
            // Routing the view instead goes through view_math.shouldDeliver,
            // reading igGetIO()'s capture flags fresh after this call.
            _ = host.handleEvent(&event);
            switch (event.type) {
                sdl3.c.SDL_EVENT_QUIT, sdl3.c.SDL_EVENT_WINDOW_CLOSE_REQUESTED => running = false,
                else => {
                    const kind = view_mod.inputKindOf(event.type);
                    const capture = view_mod.captureFlags();
                    if (view_math.shouldDeliver(kind, capture, view.hasActiveMouseGesture()))
                        view.handleEvent(editor, real, &event);
                },
            }
        }
        const ticks = sdl3.c.SDL_GetTicks();
        const dt_seconds = @as(f32, @floatFromInt(ticks -% last_ticks)) / 1000.0;
        last_ticks = ticks;
        view.update(real, host.window, dt_seconds);

        host.beginFrame();
        panels.draw(state);
        host.endFrame() catch |err| view.setStatus("failed: ", @errorName(err));
        // After the frame: Save, the dialogs Open and Save As show, a path
        // one of them delivered during this frame's events, and Quit.
        if (panels.act(state)) running = false;
        if (script) |s| {
            if (!s.afterFrame()) running = false;
        }
    }
}

/// --smoke: the interactive mode's setup, hidden, and its loop under
/// smoke.zig's script. Failures print a "smoke FAIL:" line; there is no
/// person to show a message box to.
fn smokeRun(gpa: std.mem.Allocator, io: std.Io, map: []const u8, output: []const u8) !bool {
    if (std.fs.path.dirname(output)) |directory| try std.Io.Dir.cwd().createDirPath(io, directory);
    // A file left by an earlier run would let the reopen step pass on a save
    // that never happened.
    std.Io.Dir.cwd().deleteFile(io, output) catch |err| switch (err) {
        error.FileNotFound => {},
        else => {
            std.debug.print("map-editor: smoke FAIL: the last run's {s} would not go ({s})\n", .{ output, @errorName(err) });
            return false;
        },
    };
    var host = host_mod.Host.start(.{ .title = "Map Editor", .hidden = true }) catch |err| {
        std.debug.print("map-editor: smoke FAIL: the host did not start ({s})\n", .{@errorName(err)});
        return false;
    };
    defer host.stop();

    var real = c_bridge.RealBridge.init(host.session);
    var editor = core.editor.Editor.init(gpa, real.bridge());
    defer editor.deinit();
    var view = view_mod.View.init(gpa);
    defer view.deinit(gpa);
    editor.open(map) catch {
        std.debug.print("map-editor: smoke FAIL: {s} did not open: {s}\n", .{ map, editor.status() });
        return false;
    };
    var state = panels.State.init(gpa, &editor, &view, &real, host.window);
    defer state.deinit();

    var script = smoke.Script.init(&editor, &view, &real, &state, host.window, output);
    run(&host, &editor, &view, &real, &state, &script);
    if (!script.passed) {
        // A step that failed has said so; a loop that ended otherwise (a
        // quit event) has not.
        if (!script.reported) std.debug.print("map-editor: smoke FAIL: the loop ended at step {d} of {d}\n", .{ script.step + 1, smoke.script.len });
        return false;
    }
    std.debug.print("map-editor: smoke PASS ({d} steps, {d} objects, saved and reopened {s})\n", .{ smoke.script.len, script.original_objects, output });
    return true;
}

fn startupStepName(err: host_mod.HostError) []const u8 {
    return switch (err) {
        error.SdlInitFailed => "SDL init",
        error.WindowFailed => "the window",
        error.EngineFailed => "the engine start",
        error.NoDevice => "no GPU device",
        error.ImguiFailed => "ImGui",
        error.FrameFailed => "the frame",
    };
}

/// Names the step that failed before there was a window to show anything in,
/// through the platform's own message box, and exits. Never reached by
/// --check, which has no window and keeps failing to stderr (see `fail`).
fn fatal(step: []const u8, reason: []const u8) noreturn {
    var buffer: [768]u8 = undefined;
    const message = std.fmt.bufPrintZ(&buffer, "{s} failed: {s}", .{ step, reason }) catch "Map Editor failed to start";
    _ = sdl3.c.SDL_ShowSimpleMessageBox(sdl3.c.SDL_MESSAGEBOX_ERROR, "Map Editor", message, null);
    std.process.exit(1);
}

// The C main mainCRTStartup calls on Windows (crt.zig minimalFromPeb says why).
comptime {
    if (crt.exports_c_main) @export(&crtMain, .{ .name = "main" });
}

fn crtMain(argc: c_int, argv: ?*anyopaque) callconv(.c) c_int {
    _ = argc;
    _ = argv;
    main(crt.minimalFromPeb()) catch |err| {
        std.debug.print("map-editor: {s}\n", .{@errorName(err)});
        return 1;
    };
    return 0;
}

fn usage() noreturn {
    std.debug.print("usage: MapEditor [<map>]\n       MapEditor --check <map> [<out.tga>]\n       MapEditor --smoke <map> [<out.bzm>]\n", .{});
    std.process.exit(2);
}

fn fail(comptime format: []const u8, args: anytype) bool {
    std.debug.print("map-editor: host check FAIL: " ++ format ++ "\n", args);
    return false;
}

fn check(gpa: std.mem.Allocator, io: std.Io, map: []const u8, output: []const u8) !bool {
    if (std.fs.path.dirname(output)) |directory| try std.Io.Dir.cwd().createDirPath(io, directory);
    const map_z = try gpa.dupeZ(u8, map);
    defer gpa.free(map_z);
    const output_z = try gpa.dupeZ(u8, output);
    defer gpa.free(output_z);

    var host = host_mod.Host.start(.{ .title = "Map Editor", .hidden = true }) catch |err|
        return fail("the host did not start ({s})", .{@errorName(err)});
    defer host.stop();

    var summary: c.BkEditorMapSummary = std.mem.zeroes(c.BkEditorMapSummary);
    if (c.BkEditorOpenMap(host.session, map_z.ptr, &summary) != c.BK_EDITOR_OK)
        return fail("{s} did not open: {s}", .{ map, std.mem.span(c.BkEditorLastMessage(host.session)) });
    // The camera opens on the map's corner, where half the screen is off the
    // map; the middle of the map has ground under all of it.
    const centre_x = @as(f32, @floatFromInt(summary.width_tiles)) * world_cell_size / 2;
    const centre_y = @as(f32, @floatFromInt(summary.height_tiles)) * world_cell_size / 2;
    if (c.BkEditorSetCamera(host.session, centre_x, centre_y) != c.BK_EDITOR_OK)
        return fail("the camera would not move to the map's middle: {s}", .{std.mem.span(c.BkEditorLastMessage(host.session))});

    var frame: u32 = 0;
    while (frame < probe_frames) : (frame += 1) {
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
        host.beginFrame();
        drawProbe();
        host.endFrame() catch |err| return fail("frame {d}: {s}: {s}", .{ frame, @errorName(err), std.mem.span(c.BkEditorLastMessage(host.session)) });
    }
    // The last frame's draw data stays ImGui's until the next igNewFrame, so
    // the captured frame has the probe over it too.
    if (c.BkEditorCaptureFrame(host.session, output_z.ptr) != c.BK_EDITOR_OK)
        return fail("the frame was not captured: {s}", .{std.mem.span(c.BkEditorLastMessage(host.session))});

    var width: c_int = 0;
    var height: c_int = 0;
    if (c.BkEditorScreenSize(host.session, &width, &height) != c.BK_EDITOR_OK)
        return fail("no screen size: {s}", .{std.mem.span(c.BkEditorLastMessage(host.session))});
    var device: ?*anyopaque = null;
    var format: c_uint = 0;
    _ = c.BkEditorGpuDevice(host.session, &device, &format);
    const driver_name = if (device != null) sdl3.c.SDL_GetGPUDeviceDriver(@ptrCast(device)) else null;
    const driver: []const u8 = if (driver_name != null) std.mem.span(driver_name) else "unknown";

    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, output, gpa, .limited(64 << 20));
    defer gpa.free(bytes);
    const image = Tga.parse(bytes) catch |err| return fail("{s} is not an uncompressed 32-bit TGA ({s})", .{ output, @errorName(err) });
    if (image.width != width or image.height != height)
        return fail("{s} is {d}x{d}, the screen {d}x{d}", .{ output, image.width, image.height, width, height });

    const inside_x = probe.x + probe.w / 2;
    const inside_y = probe.y + probe.h / 2;
    const inside = image.pixel(inside_x, inside_y);
    if (!inside.near(magenta))
        return fail("the probe window's centre ({d},{d}) is ({d},{d},{d}), not magenta", .{ inside_x, inside_y, inside.r, inside.g, inside.b });
    // The screen's centre is far from the probe; the map is drawn there.
    const outside_x: u32 = @intCast(@divTrunc(width, 2));
    const outside_y: u32 = @intCast(@divTrunc(height, 2));
    const outside = image.pixel(outside_x, outside_y);
    if (outside.near(magenta) or outside.near(clear_colour))
        return fail("the screen's centre ({d},{d}) is ({d},{d},{d}), not the map", .{ outside_x, outside_y, outside.r, outside.g, outside.b });

    std.debug.print("map-editor: host check PASS ({s}, {d}x{d})\n", .{ driver, width, height });
    return panelSmoke(gpa, &host, map, output);
}

/// One frame of the real panels over the map, with a State from the opened
/// map, and the file actions the dialogs start, run with the path handed to
/// the slot as a dialog's callback would hand it: Save As into the output's
/// directory, then Open of what was saved. Nothing here needs a person, so
/// CI runs the panel code on both GPU runners.
fn panelSmoke(gpa: std.mem.Allocator, host: *host_mod.Host, map: []const u8, output: []const u8) !bool {
    var real = c_bridge.RealBridge.init(host.session);
    var editor = core.editor.Editor.init(gpa, real.bridge());
    defer editor.deinit();
    var view = view_mod.View.init(gpa);
    defer view.deinit(gpa);
    editor.open(map) catch return fail("panels: {s} did not open through the editor: {s}", .{ map, editor.status() });
    var state = panels.State.init(gpa, &editor, &view, &real, host.window);
    defer state.deinit();
    if (state.catalogue.len == 0) return fail("panels: the object palette has no catalogue", .{});
    if (state.tile_count == 0) return fail("panels: the brush has no tiles from the map's tileset", .{});
    if (std.mem.indexOfScalar(u8, state.tiles(), 1) != null) return fail("panels: tile 1, in no shipped tileset, is offered", .{});
    // An object the properties panel can edit, so its fields are drawn too.
    const editable: ?i32 = for (editor.document.objects.items) |object| {
        if (panels_logic.readOnlyReason(editor.document.objects.items, object) == null) break object.link_id;
    } else null;
    editor.selection = editable orelse return fail("panels: no object of {s} is editable", .{map});

    if (!panelFrame(host, &state)) return false;
    const objects = editor.document.objects.items.len;

    const directory = std.fs.path.dirname(output) orelse ".";
    var saved_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const saved = std.fmt.bufPrint(&saved_buffer, "{s}{c}map-editor-check-saved.bzm", .{ directory, std.fs.path.sep }) catch
        return fail("panels: the save path is too long", .{});
    if (!state.actions.dialog.request(.save_as)) return fail("panels: the dialog slot was not free", .{});
    state.actions.dialog.deliver(saved);
    if (panels.act(&state)) return fail("panels: Save As quit the editor", .{});
    if (view.statusLine().len != 0 or editor.status().len != 0)
        return fail("panels: Save As to {s} failed: {s}{s}", .{ saved, view.statusLine(), editor.status() });
    if (!std.mem.eql(u8, panels_logic.baseName(editor.document.path.items), "map-editor-check-saved.bzm"))
        return fail("panels: after Save As the document's path is {s}", .{editor.document.path.items});

    if (!state.actions.dialog.request(.open)) return fail("panels: the dialog slot was not free after Save As", .{});
    state.actions.dialog.deliver(saved);
    if (panels.act(&state)) return fail("panels: Open quit the editor", .{});
    if (view.statusLine().len != 0 or editor.status().len != 0)
        return fail("panels: opening {s} again failed: {s}{s}", .{ saved, view.statusLine(), editor.status() });
    if (editor.document.objects.items.len != objects)
        return fail("panels: {s} reopened with {d} objects, saved with {d}", .{ saved, editor.document.objects.items.len, objects });
    // Opening forgets the selection; select again so the capture below
    // has the properties panel's fields in it.
    editor.selection = editable orelse unreachable;
    if (!panelFrame(host, &state)) return false;
    // What the panels look like, beside the probe's capture, for a person
    // to look at; nothing is measured in it.
    var shot_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const shot = std.fmt.bufPrintZ(&shot_buffer, "{s}{c}map-editor-panels.tga", .{ directory, std.fs.path.sep }) catch
        return fail("panels: the capture path is too long", .{});
    if (c.BkEditorCaptureFrame(host.session, shot.ptr) != c.BK_EDITOR_OK)
        return fail("panels: the frame was not captured: {s}", .{std.mem.span(c.BkEditorLastMessage(host.session))});

    std.debug.print("map-editor: panel smoke PASS ({d} catalogue entries, {d} tiles, saved and reopened {s})\n", .{ state.catalogue.len, state.tile_count, saved });
    return true;
}

fn panelFrame(host: *host_mod.Host, state: *panels.State) bool {
    var event: sdl3.c.SDL_Event = undefined;
    while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
    host.beginFrame();
    panels.draw(state);
    host.endFrame() catch |err| return fail("panels: the frame failed: {s}: {s}", .{ @errorName(err), std.mem.span(c.BkEditorLastMessage(host.session)) });
    if (state.view.statusLine().len != 0) return fail("panels: the frame left a failure: {s}", .{state.view.statusLine()});
    return true;
}

fn drawProbe() void {
    imgui.c.igSetNextWindowPos(.{ .x = probe.x, .y = probe.y }, imgui.c.ImGuiCond_Always);
    imgui.c.igSetNextWindowSize(.{ .x = probe.w, .y = probe.h }, imgui.c.ImGuiCond_Always);
    imgui.c.igPushStyleColorImVec4(imgui.c.ImGuiCol_WindowBg, .{ .x = 1, .y = 0, .z = 1, .w = 1 });
    _ = imgui.c.igBegin("probe", null, imgui.c.ImGuiWindowFlags_NoDecoration | imgui.c.ImGuiWindowFlags_NoMove | imgui.c.ImGuiWindowFlags_NoSavedSettings);
    imgui.c.igEnd();
    imgui.c.igPopStyleColor();
}

const Rgb = struct {
    r: u8,
    g: u8,
    b: u8,

    fn near(self: Rgb, other: Rgb) bool {
        return close(self.r, other.r) and close(self.g, other.g) and close(self.b, other.b);
    }

    fn close(a: u8, b: u8) bool {
        return @abs(@as(i16, a) - @as(i16, b)) <= 2;
    }
};

const magenta = Rgb{ .r = 255, .g = 0, .b = 255 };
/// What DrawSessionFrame clears to before the scene is drawn.
const clear_colour = Rgb{ .r = 0, .g = 0, .b = 0 };

/// An uncompressed 32-bit TGA: an 18-byte header, an optional ID, then BGRA
/// rows, bottom row first unless bit 5 of the descriptor (byte 17) is set.
const Tga = struct {
    width: u32,
    height: u32,
    top_first: bool,
    pixels: []const u8,

    fn parse(bytes: []const u8) !Tga {
        if (bytes.len < 18) return error.Truncated;
        if (bytes[1] != 0 or bytes[2] != 2) return error.NotUncompressedTrueColour;
        if (bytes[16] != 32) return error.Not32Bit;
        const width = std.mem.readInt(u16, bytes[12..14], .little);
        const height = std.mem.readInt(u16, bytes[14..16], .little);
        const start = 18 + @as(usize, bytes[0]);
        const length = @as(usize, width) * height * 4;
        if (bytes.len < start + length) return error.Truncated;
        return .{ .width = width, .height = height, .top_first = bytes[17] & 0x20 != 0, .pixels = bytes[start .. start + length] };
    }

    fn pixel(self: Tga, x: u32, y: u32) Rgb {
        const row = if (self.top_first) y else self.height - 1 - y;
        const i = (@as(usize, row) * self.width + x) * 4;
        return .{ .r = self.pixels[i + 2], .g = self.pixels[i + 1], .b = self.pixels[i] };
    }
};
