//! The host check's second half (S05 T12): the docks over the preview
//! scene, measured in a captured frame by code. On the session the first
//! half used (a new project of <kind>, wpn in resource-editor-host-check):
//!
//! * the preview of a kind without one is refused, naming the kind;
//! * Import through the lifecycle's own call: a kind that has none yet is
//!   refused naming it, and the shipped Data/Units/Humans/German/Gunner
//!   folder builds an unsaved, untitled infantry project;
//! * the preview scene is begun for that infantry project, and Run reports
//!   the bridge's reason it cannot show it yet (no ported exporter);
//! * the thumbnail list reads a folder holding a copy of the tracked
//!   fixture picture, the engine decodes it, and the captured frame has the
//!   picture's own colour where the list drew it;
//! * the screen's middle, where the preview scene is drawn, is the engine's
//!   frame: every sampled pixel of it is the scene's clear colour, so no
//!   dock covers the preview.
const std = @import("std");
const sdl3 = @import("sdl3");
const kit = @import("editor_kit");
const core = @import("resource_core");
const app = @import("main.zig");
const c_bridge = @import("c_bridge.zig");
const logic = @import("panels_logic.zig");
const docks_mod = @import("docks.zig");

const c = c_bridge.c;
const Kind = core.bridge.Kind;
const host_mod = kit.host;

/// How many frames the thumbnail may take to decode and draw.
const max_frames = 120;

/// The middle the preview owns: this far either side of the screen's centre.
const preview_half_w = 200;
const preview_half_h = 120;
const preview_step = 8;

pub const Outcome = struct {
    thumbnail: app.Rgb = .{ .r = 0, .g = 0, .b = 0 },
    preview_samples: usize = 0,
};

/// Runs the docks half. `fixture` is the tracked picture the thumbnail list
/// shows (a copy, beside `output`); the capture is `<output>-docks.tga`.
pub fn run(gpa: std.mem.Allocator, io: std.Io, host: *host_mod.Host, kind: Kind, output: []const u8, fixture: []const u8) !?Outcome {
    var real = c_bridge.RealResBridge.init(gpa, host.session);
    const b = real.bridge();
    var docks = docks_mod.Docks.init(gpa, io, &real);
    defer docks.deinit();
    docks.fixed = true;

    // A kind without a preview: refused, the message naming it.
    if (kind == .weapon) {
        if (docks.preview.sync(b, true, kind) != .refused) return fail("the preview of .wpn was not refused", .{});
        if (std.mem.indexOf(u8, docks.preview.message(), ".wpn") == null)
            return fail("the preview's refusal does not name .wpn: {s}", .{docks.preview.message()});
    }

    // Import through Lifecycle.importFromGame, the call the Import window's
    // guarded action ends in (lifecycle.zig perform).
    var paths = std.mem.zeroes(c.BkEditorPathSet);
    if (c.BkEditorPaths(real.session, &paths) != c.BK_EDITOR_OK) return fail("no installation paths: {s}", .{b.lastMessage()});
    var gunner_buffer: [logic.path_capacity]u8 = undefined;
    const base_root = std.mem.sliceTo(&paths.base_root, 0);
    const gunner = std.fmt.bufPrint(&gunner_buffer, "{s}{s}Data/Units/Humans/German/Gunner", .{ base_root, if (std.mem.endsWith(u8, base_root, "/") or std.mem.endsWith(u8, base_root, "\\")) "" else "/" }) catch
        return fail("the installation path is too long", .{});
    var life: logic.Lifecycle = .{};
    defer life.deinit(gpa);
    if (life.importFromGame(gpa, b, .bridge, gunner)) |_| {
        return fail("importing .bdg was not refused", .{});
    } else |_| {}
    if (std.mem.indexOf(u8, b.lastMessage(), "not ported yet") == null or std.mem.indexOf(u8, b.lastMessage(), ".bdg") == null)
        return fail("the .bdg import refusal does not name the kind: {s}", .{b.lastMessage()});
    life.importFromGame(gpa, b, .animation_infantry, gunner) catch
        return fail("importing {s} as .unt failed: {s}", .{ gunner, b.lastMessage() });
    if (!life.is_open or life.doc.kind != .animation_infantry or life.doc.pathSlice() != null or !life.dirty())
        return fail("the import is not an open, untitled, unsaved .unt project", .{});
    if (life.doc.tree.nodes.items.len < 2) return fail("the imported project has {d} nodes", .{life.doc.tree.nodes.items.len});

    // The preview scene behind the docks, begun for the imported project.
    docks.syncPreview(&life);
    if (docks.preview.begun != .animation_infantry) return fail("the .unt preview was not begun: {s}", .{docks.preview.message()});
    // An unsaved import has no folder to read its frames from, so the bridge
    // refuses before the exporter runs.
    docks.runPreview();
    if (docks.preview.running or std.mem.indexOf(u8, docks.preview.message(), "save the project first") == null)
        return fail("Run on an unsaved .unt did not ask for a save: {s}", .{docks.preview.message()});

    // Saved into a scratch folder with no art beside it, the exporter runs
    // and gives its own reason (infantry composes no valid animation), no
    // longer "not ported".
    const directory = std.fs.path.dirname(output) orelse ".";
    var import_buffer: [logic.path_capacity]u8 = undefined;
    const import_folder = std.fmt.bufPrint(&import_buffer, "{s}/import", .{directory}) catch return fail("the output path is too long", .{});
    std.Io.Dir.cwd().deleteTree(io, import_folder) catch {};
    try std.Io.Dir.cwd().createDirPath(io, import_folder);
    var saved_buffer: [logic.path_capacity]u8 = undefined;
    const saved = std.fmt.bufPrint(&saved_buffer, "{s}/imported.unt", .{import_folder}) catch return fail("the output path is too long", .{});
    life.saveProject(gpa, b, saved) catch return fail("saving the import to {s} failed: {s}", .{ saved, b.lastMessage() });
    docks.runPreview();
    const reason = docks.preview.message();
    if (docks.preview.running or std.mem.indexOf(u8, reason, "not shown") == null or std.mem.indexOf(u8, reason, "not ported") != null)
        return fail("Run on the saved .unt with no art did not report the exporter's reason: {s}", .{reason});
    std.debug.print("resource-editor: the saved .unt Run says: {s}\n", .{reason});

    // The thumbnail list over a copy of the tracked fixture picture.
    var folder_buffer: [logic.path_capacity]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}/thumbs", .{directory}) catch return fail("the output path is too long", .{});
    std.Io.Dir.cwd().deleteTree(io, folder) catch {};
    try std.Io.Dir.cwd().createDirPath(io, folder);
    const picture = try std.Io.Dir.cwd().readFileAlloc(io, fixture, gpa, .limited(1 << 20));
    defer gpa.free(picture);
    const expected = fixtureColour(picture) orelse return fail("{s} is not an uncompressed 24- or 32-bit TGA", .{fixture});
    var copy_buffer: [logic.path_capacity]u8 = undefined;
    const copy = std.fmt.bufPrint(&copy_buffer, "{s}/{s}", .{ folder, std.fs.path.basename(fixture) }) catch return fail("the output path is too long", .{});
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = copy, .data = picture });
    docks.setThumbnailFolder(folder);
    docks.show_thumbnails = true;
    docks.show_direction = true;
    docks.show_function = true;
    docks.direction_angle = std.math.pi / 4.0;

    var frame: u32 = 0;
    var drawn_after: u32 = 0;
    while (frame < max_frames and drawn_after < 3) : (frame += 1) {
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
        host.beginFrame();
        docks.drawDocks(null, &life, null);
        app.drawProbe();
        host.endFrame() catch |err| return fail("docks frame {d}: {s}: {s}", .{ frame, @errorName(err), b.lastMessage() });
        if (docks.first_thumbnail != null) drawn_after += 1;
    }
    const shown = docks.first_thumbnail orelse return fail("the thumbnail of {s} was not drawn in {d} frames", .{ copy, max_frames });

    var capture_buffer: [logic.path_capacity]u8 = undefined;
    const stem = if (std.mem.endsWith(u8, output, ".tga")) output[0 .. output.len - 4] else output;
    const capture = std.fmt.bufPrintZ(&capture_buffer, "{s}-docks.tga", .{stem}) catch return fail("the output path is too long", .{});
    if (c.BkEditorCaptureFrame(real.session, capture.ptr) != c.BK_EDITOR_OK) return fail("the docks frame was not captured: {s}", .{b.lastMessage()});
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, capture, gpa, .limited(64 << 20));
    defer gpa.free(bytes);
    const image = app.Tga.parse(bytes) catch |err| return fail("{s} is not an uncompressed 32-bit TGA ({s})", .{ capture, @errorName(err) });

    const probe_pixel = image.pixel(app.probe.x + app.probe.w / 2, app.probe.y + app.probe.h / 2) orelse return fail("the probe is outside the capture", .{});
    if (!probe_pixel.isProbeColour()) return fail("the docks frame's probe is ({d},{d},{d}), not orange", .{ probe_pixel.r, probe_pixel.g, probe_pixel.b });

    const tx: u32 = @intFromFloat(shown.x + shown.w / 2);
    const ty: u32 = @intFromFloat(shown.y + shown.h / 2);
    const thumbnail = image.pixel(tx, ty) orelse return fail("the thumbnail ({d},{d}) is outside the capture", .{ tx, ty });
    if (!closeTo(thumbnail, expected, 6))
        return fail("the thumbnail's middle ({d},{d}) is ({d},{d},{d}), the fixture ({d},{d},{d})", .{ tx, ty, thumbnail.r, thumbnail.g, thumbnail.b, expected.r, expected.g, expected.b });

    const cx: i64 = @divTrunc(image.width, 2);
    const cy: i64 = @divTrunc(image.height, 2);
    var samples: usize = 0;
    var y: i64 = cy - preview_half_h;
    while (y <= cy + preview_half_h) : (y += preview_step) {
        var x: i64 = cx - preview_half_w;
        while (x <= cx + preview_half_w) : (x += preview_step) {
            const pixel = image.pixel(@intCast(x), @intCast(y)) orelse return fail("the preview sample ({d},{d}) is outside the capture", .{ x, y });
            if (!pixel.near(app.clear_colour))
                return fail("the preview's middle ({d},{d}) is ({d},{d},{d}), not the scene's clear colour: something covers the preview", .{ x, y, pixel.r, pixel.g, pixel.b });
            samples += 1;
        }
    }

    // Run on a saved .spt, last because it draws into the preview's middle.
    if (!try runSavedSprite(gpa, io, host, &docks, &life, b, directory, fixture, stem)) return null;
    return .{ .thumbnail = thumbnail, .preview_samples = samples };
}

/// The tracked sprite project and its one frame copied into a scratch folder
/// (the project's directory pointed at frames\\, where the frame is), opened
/// as the lifecycle's project, and Run on it. False after printing why not.
fn runSavedSprite(gpa: std.mem.Allocator, io: std.Io, host: *host_mod.Host, docks: *docks_mod.Docks, life: *logic.Lifecycle, b: core.bridge.ResBridge, directory: []const u8, picture: []const u8, stem: []const u8) !bool {
    var folder_buffer: [logic.path_capacity]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}/sprite", .{directory}) catch return failed("the output path is too long", .{});
    std.Io.Dir.cwd().deleteTree(io, folder) catch {};
    var frames_buffer: [logic.path_capacity]u8 = undefined;
    const frames = std.fmt.bufPrint(&frames_buffer, "{s}/frames", .{folder}) catch return failed("the output path is too long", .{});
    try std.Io.Dir.cwd().createDirPath(io, frames);

    const picture_bytes = try std.Io.Dir.cwd().readFileAlloc(io, picture, gpa, .limited(1 << 20));
    defer gpa.free(picture_bytes);
    var frame_buffer: [logic.path_capacity]u8 = undefined;
    const frame = std.fmt.bufPrint(&frame_buffer, "{s}/{s}", .{ frames, std.fs.path.basename(picture) }) catch return failed("the output path is too long", .{});
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = frame, .data = picture_bytes });

    var source_buffer: [logic.path_capacity]u8 = undefined;
    const source = std.fmt.bufPrint(&source_buffer, "{s}/project.spt", .{std.fs.path.dirname(picture) orelse "."}) catch return failed("the picture path is too long", .{});
    const text = try std.Io.Dir.cwd().readFileAlloc(io, source, gpa, .limited(1 << 20));
    defer gpa.free(text);
    const needle = "<string_value>_.</string_value>";
    const at = std.mem.indexOf(u8, text, needle) orelse return failed("{s} has no default directory to point at the frame", .{source});
    const edited = try std.mem.concat(gpa, u8, &.{ text[0..at], "<string_value>frames\\</string_value>", text[at + needle.len ..] });
    defer gpa.free(edited);
    var project_buffer: [logic.path_capacity]u8 = undefined;
    const project = std.fmt.bufPrint(&project_buffer, "{s}/project.spt", .{folder}) catch return failed("the output path is too long", .{});
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = project, .data = edited });

    _ = life.openProject(gpa, b, project, "resource-editor-check") catch return failed("opening {s} failed: {s}", .{ project, b.lastMessage() });
    docks.syncPreview(life);
    if (docks.preview.begun != .sprite) return failed("the .spt preview was not begun: {s}", .{docks.preview.message()});
    docks.runPreview();
    if (!docks.preview.running or std.mem.indexOf(u8, docks.preview.message(), "showing the project") == null)
        return failed("Run on the saved .spt did not show the preview: {s}", .{docks.preview.message()});

    // The sprite is drawn in the preview's middle: measured in a capture by
    // counting the pixels there that are not the scene's clear colour.
    docks.show_thumbnails = false;
    docks.show_direction = false;
    docks.show_function = false;
    var drawn_frames: u32 = 0;
    while (drawn_frames < 30) : (drawn_frames += 1) {
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
        host.beginFrame();
        docks.drawDocks(null, life, null);
        host.endFrame() catch |err| return failed("sprite frame {d}: {s}: {s}", .{ drawn_frames, @errorName(err), b.lastMessage() });
    }
    var capture_buffer: [logic.path_capacity]u8 = undefined;
    const capture = std.fmt.bufPrintZ(&capture_buffer, "{s}-sprite.tga", .{stem}) catch return failed("the output path is too long", .{});
    if (c.BkEditorCaptureFrame(docks.real.session, capture.ptr) != c.BK_EDITOR_OK) return failed("the sprite frame was not captured: {s}", .{b.lastMessage()});
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, capture, gpa, .limited(64 << 20));
    defer gpa.free(bytes);
    const image = app.Tga.parse(bytes) catch |err| return failed("{s} is not an uncompressed 32-bit TGA ({s})", .{ capture, @errorName(err) });
    const cx: i64 = @divTrunc(image.width, 2);
    const cy: i64 = @divTrunc(image.height, 2);
    var drawn: usize = 0;
    var y: i64 = cy - preview_half_h;
    while (y <= cy + preview_half_h) : (y += 1) {
        var x: i64 = cx - preview_half_w;
        while (x <= cx + preview_half_w) : (x += 1) {
            const pixel = image.pixel(@intCast(x), @intCast(y)) orelse continue;
            if (!pixel.near(app.clear_colour)) drawn += 1;
        }
    }
    std.debug.print("resource-editor: the .spt preview drew {d} pixels in the scene's middle ({s})\n", .{ drawn, capture });
    if (drawn < 50) return failed("the .spt preview drew {d} pixels in the scene's middle, expected a sprite", .{drawn});

    docks.stopPreview();
    if (docks.preview.running) return failed("Stop left the .spt preview running", .{});
    return true;
}

fn failed(comptime format: []const u8, args: anytype) bool {
    std.debug.print("resource-editor: host check FAIL: " ++ format ++ "\n", args);
    return false;
}

/// The first pixel of an uncompressed true-colour TGA (the fixtures are
/// solid colour), as R, G, B.
fn fixtureColour(bytes: []const u8) ?app.Rgb {
    if (bytes.len < 18 or bytes[1] != 0 or bytes[2] != 2) return null;
    const depth = bytes[16];
    if (depth != 24 and depth != 32) return null;
    const start = 18 + @as(usize, bytes[0]);
    if (bytes.len < start + depth / 8) return null;
    return .{ .r = bytes[start + 2], .g = bytes[start + 1], .b = bytes[start] };
}

fn closeTo(a: app.Rgb, b: app.Rgb, tolerance: i16) bool {
    return @abs(@as(i16, a.r) - b.r) <= tolerance and @abs(@as(i16, a.g) - b.g) <= tolerance and @abs(@as(i16, a.b) - b.b) <= tolerance;
}

fn fail(comptime format: []const u8, args: anytype) ?Outcome {
    std.debug.print("resource-editor: host check FAIL: " ++ format ++ "\n", args);
    return null;
}
