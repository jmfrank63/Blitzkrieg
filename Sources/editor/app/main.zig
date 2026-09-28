//! MapEditor:
//!   MapEditor [-mod=<Folder>|-mod=None] [<map>]                       interactive
//!   MapEditor [-mod=...] --check <map> [<out.tga>]     headless host check
//!   MapEditor [-mod=...] --smoke <map> [<out.bzm>]     scripted run of the real loop
//!   MapEditor [-mod=...] --game-reads-it <map> [<log>] headless test-launch, played by Game
//!
//! -mod=<Folder> or -mod=None (D-26, like the game's own -mod=) is accepted
//! anywhere before the positional arguments, in every mode above: it is
//! pulled out of the argument list first, then applied through
//! RealBridge.setMod right after the host starts and before anything reads
//! the catalogue or opens a map - the mod changes what both see. A refusal
//! (an unknown folder, or a bad one) is reported the way each mode already
//! reports its other startup failures: interactive shows it the same
//! message-box way as any other startup step (fatal), --check prints a
//! "FAIL:" line and exits 1. Given and valid, --check also prints
//! "map-editor: mod <folder> (<name> <version>)" once the mod is loaded.
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
const testlaunch = @import("testlaunch.zig");
const c = host_mod.c;

const default_output = "zig-out/local-test/map-editor-check.tga";
const default_smoke_output = "zig-out/local-test/map-editor-smoke.bzm";
const default_game_reads_it_log = "zig-out/local-test/map-editor-game-reads-it.log";

/// The point Task 1's headless test launch places its unit at: the smoke's
/// own measured free ground (smoke.zig's place_at), so this mode never has
/// to characterise a shipped map's terrain a second time.
const game_reads_it_offset = struct {
    const dx: f32 = -40;
    const dy: f32 = -120;
};

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
    // Not global_single_threaded: its allocator is .failing by design (a
    // minimal, no-concurrency fallback), which every spawnPosix/spawnWindows
    // allocation under Test in game's std.process.spawn would then fail with
    // OutOfMemory before the child ever runs. A real Threaded instance, with
    // the process's own environment so PATH scanning and env-var inheritance
    // see it too.
    var threaded: std.Io.Threaded = .init(gpa, .{ .environ = minimal.environ });
    defer threaded.deinit();
    const io = threaded.io();

    var args = try std.process.Args.Iterator.initAllocator(minimal.args, gpa);
    defer args.deinit();
    _ = args.next();

    // -mod=<Folder>/-mod=None is pulled out of the argument list first, so it
    // is accepted in any position before the positional arguments below, in
    // every mode - matching the doc comment atop this file.
    var mod_arg: ?[]const u8 = null; // the raw text after "=", "None" included
    var rest: std.ArrayList([]const u8) = .empty;
    defer rest.deinit(gpa);
    while (args.next()) |arg| {
        if (parseModArg(arg)) |raw| {
            mod_arg = raw;
        } else {
            try rest.append(gpa, arg);
        }
    }
    const mod_requested = mod_arg != null;
    const mod_folder: ?[]const u8 = if (mod_arg) |raw| (if (std.mem.eql(u8, raw, "None")) null else raw) else null;

    var index: usize = 0;
    const first = nextArg(rest.items, &index);
    if (first) |arg| {
        if (std.mem.eql(u8, arg, "--check")) {
            const map = nextArg(rest.items, &index) orelse usage();
            const output = nextArg(rest.items, &index) orelse default_output;
            if (nextArg(rest.items, &index) != null) usage();
            const passed = try check(gpa, io, minimal.environ, map, output, mod_folder, mod_requested);
            std.process.exit(if (passed) 0 else 1);
        }
        if (std.mem.eql(u8, arg, "--smoke")) {
            const map = nextArg(rest.items, &index) orelse usage();
            const output = nextArg(rest.items, &index) orelse default_smoke_output;
            if (nextArg(rest.items, &index) != null) usage();
            const passed = try smokeRun(gpa, io, minimal.environ, map, output, mod_folder, mod_requested);
            std.process.exit(if (passed) 0 else 1);
        }
        if (std.mem.eql(u8, arg, "--game-reads-it")) {
            const map = nextArg(rest.items, &index) orelse usage();
            const log_path = nextArg(rest.items, &index) orelse default_game_reads_it_log;
            if (nextArg(rest.items, &index) != null) usage();
            const passed = try gameReadsIt(gpa, io, minimal.environ, map, log_path, mod_folder, mod_requested);
            std.process.exit(if (passed) 0 else 1);
        }
        if (nextArg(rest.items, &index) != null) usage();
        try interactive(gpa, io, minimal.environ, arg, mod_folder, mod_requested);
        return;
    }
    try interactive(gpa, io, minimal.environ, null, mod_folder, mod_requested);
}

/// `-mod=<Folder>` or `-mod=None`: the raw text after `=`, or null when `arg`
/// is not a `-mod=` argument at all. Kept apart from "None" itself (which the
/// caller turns into a null `mod_folder`) so a plain launch with no `-mod=`
/// never calls `RealBridge.setMod` at all - unlike the bridge's own null/""
/// convention, which treats "not given" and "explicitly cleared" the same,
/// the app keeps them apart so a normal launch never pays a mod-switch's
/// FilesInspector/managers/LoadDB cost for nothing.
fn parseModArg(arg: []const u8) ?[]const u8 {
    const prefix = "-mod=";
    if (!std.mem.startsWith(u8, arg, prefix)) return null;
    return arg[prefix.len..];
}

/// The next argument after `-mod=`/`-mod=None` has been pulled out of the
/// list, in order, once each - `std.process.Args.Iterator`'s own `next()`
/// shape, over the filtered slice instead of the raw argv.
fn nextArg(items: []const []const u8, index: *usize) ?[]const u8 {
    if (index.* >= items.len) return null;
    defer index.* += 1;
    return items[index.*];
}

/// Applies `-mod=`'s request, right after the host starts and before
/// anything reads the catalogue or opens a map. Returns null on success, or
/// the bridge's reason on a refusal/bad argument - `real.session`'s own
/// message, valid only until the next bridge call, so callers use it at
/// once (fatal/a FAIL line) rather than store it. A no-op, returning null,
/// when `-mod=` was never given at all.
fn applyModArg(real: *c_bridge.RealBridge, mod_folder: ?[]const u8, mod_requested: bool) ?[]const u8 {
    if (!mod_requested) return null;
    if (real.setMod(mod_folder) != .ok) return std.mem.span(c.BkEditorLastMessage(real.session));
    return null;
}

/// The interactive mode: one window, the engine on it, the view driving the
/// core's tools, until the window closes or the process is asked to quit
/// (SDL maps SIGINT/SIGTERM to SDL_EVENT_QUIT by default).
fn interactive(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: ?[]const u8, mod_folder: ?[]const u8, mod_requested: bool) !void {
    var host = host_mod.Host.start(.{ .title = "Map Editor" }) catch |err| {
        const reason = host_mod.failureReason();
        fatal(startupStepName(err), if (reason.len != 0) reason else @errorName(err));
    };
    defer host.stop();

    var real = c_bridge.RealBridge.init(host.session);
    if (applyModArg(&real, mod_folder, mod_requested)) |reason| fatal("the mod", reason);
    var editor = core.editor.Editor.init(gpa, real.bridge());
    defer editor.deinit();
    // Plan 6's safe save (D-19): every mode that can save gets one real
    // StdFiles, living as long as editor does (Files.ptr points into it).
    var std_files: core.files.StdFiles = .{ .io = io, .dir = .cwd() };
    editor.files = std_files.files();
    var view = view_mod.View.init(gpa);
    defer view.deinit(gpa);

    if (map) |typed| {
        var path_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
        const path = mapArgument(&path_buffer, typed) orelse fatal("map open", "the map's path is too long");
        editor.open(path) catch {
            const reason = editor.status();
            fatal("map open", if (reason.len != 0) reason else "the map did not open");
        };
    }
    // Centres the view on the map opened above, if any (State.mapOpened).
    var state = panels.State.init(gpa, &editor, &view, &real, host.window, io, environ, mod_folder);
    defer state.deinit();

    // D-24: mapeditor.cfg, independent of game profiles - never read or
    // written by --check/--smoke/--game-reads-it, only interactive. Missing
    // is not an error (a fresh install); unreadable falls back to defaults
    // too, with a status line naming why.
    var settings_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const settings_path = resolveSettingsPath(&settings_path_buffer, gpa, environ, std.mem.sliceTo(&state.paths.user_root, 0));
    if (settings_path) |path| {
        state.settings = readSettingsFile(io, gpa, path) catch |err| switch (err) {
            error.FileNotFound => core.settings.Settings{},
            else => blk: {
                view.setStatus("", "the settings file did not read: using defaults");
                break :blk core.settings.Settings{};
            },
        };
    }
    view.wheel_sensitivity = state.settings.scroll_speed;

    // D-22, spec Errors -> Crashes: offered back once, at startup, before the
    // main loop's own autosave tick could ever write a fresh one under the
    // same name.
    panels.scanRecoveryOffers(&state);

    run(&host, &editor, &view, &real, &state, null, settings_path, true);
    // A plain return, not std.process.exit, so the deferred view.deinit(),
    // editor.deinit() and host.stop() above run: host.stop() takes the
    // overlay and ImGui down, BkEditorStop deletes the world, and the window
    // goes. The engine's renderer and its GPU device are not shut down - they
    // live until the process exits, as in the game - which is why one Host
    // per process is the contract (host.zig Host.stop).
}

/// `<user_root>mapeditor/mapeditor.cfg`, or `BK_EDITOR_SETTINGS` when the
/// environment carries it - a test seam (Task 1's own verify uses it from
/// `--check`'s panel smoke; nothing else in this app ever sets it). Null
/// when `user_root` is empty (no engine paths, which does not happen once
/// the engine has started) or neither path fits `buffer`.
fn resolveSettingsPath(buffer: []u8, gpa: std.mem.Allocator, environ: std.process.Environ, user_root: []const u8) ?[]const u8 {
    if (environ.getAlloc(gpa, "BK_EDITOR_SETTINGS")) |override| {
        defer gpa.free(override);
        if (override.len > buffer.len) return null;
        @memcpy(buffer[0..override.len], override);
        return buffer[0..override.len];
    } else |_| {}
    if (user_root.len == 0) return null;
    return std.fmt.bufPrint(buffer, "{s}mapeditor{c}mapeditor.cfg", .{ user_root, std.fs.path.sep }) catch null;
}

/// Reads and parses `path`, capped at 64 KiB (T-03-07-01: a malformed or huge
/// settings file must not be a denial-of-service or an unbounded read).
/// `error.FileNotFound` is the caller's "use the defaults" case, same as any
/// other read failure - both are handled by the caller, not here.
fn readSettingsFile(io: std.Io, gpa: std.mem.Allocator, path: []const u8) !core.settings.Settings {
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(64 * 1024));
    defer gpa.free(bytes);
    return core.settings.parse(bytes);
}

/// Writes `settings` to `path` through a temporary file beside it and a
/// rename over the real path (D-24's own "atomic write"), matching the
/// safe-save recipe's shape without needing its read-back verification (a
/// settings file is not the user's map - losing this write to a crash mid-
/// write is not the D-19 concern that recipe exists for).
fn writeSettingsFile(io: std.Io, path: []const u8, settings: *const core.settings.Settings) !void {
    var text_buffer: [8192]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&text_buffer);
    try core.settings.format(settings, &writer);
    if (std.fs.path.dirname(path)) |dir| try std.Io.Dir.cwd().createDirPath(io, dir);
    var temp_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const temp_path = std.fmt.bufPrint(&temp_buffer, "{s}.tmp", .{path}) catch return error.NameTooLong;
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = temp_path, .data = writer.buffered() });
    try std.Io.Dir.rename(std.Io.Dir.cwd(), temp_path, std.Io.Dir.cwd(), path, io);
}

/// The app's loop, shared by the interactive mode and --smoke: events to
/// ImGui and then, if ImGui does not want them, to the view; the view's
/// per-frame scrolling; a frame of the panels over the map; the panels' file
/// actions. With a script, the script pushes its synthetic events before
/// each frame's poll and checks what they did after it, and ends the loop
/// when it is done or has failed.
///
/// `settings_path`, non-null only from the interactive mode (D-24): after the
/// frame a Settings-window control (or --check's own round trip) marks
/// `state.settings_changed`, writes `mapeditor.cfg` back through a temporary
/// file and rename, then clears the flag - never per keystroke, and the
/// automated modes (which always pass null here) never write at all.
///
/// `is_interactive` (D-20..D-22): only the interactive mode ticks autosave -
/// smoke and the panel smoke run their loops with no tick at all, so they
/// never write a map file or a recovery copy no one asked for.
fn run(host: *host_mod.Host, editor: *core.editor.Editor, view: *view_mod.View, real: *c_bridge.RealBridge, state: *panels.State, script: ?*smoke.Script, settings_path: ?[]const u8, is_interactive: bool) void {
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
                // D-23: quitting or closing the window goes through the same
                // unsaved-changes guard as the menu's Quit - the loop ends
                // only once `act` (below) says so.
                sdl3.c.SDL_EVENT_QUIT, sdl3.c.SDL_EVENT_WINDOW_CLOSE_REQUESTED => state.actions.quit_requested = true,
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
        view.update(editor, real, host.window, dt_seconds);

        host.beginFrame();
        panels.draw(state);
        host.endFrame() catch |err| view.setStatus("failed: ", @errorName(err));
        // After the frame: Save, the dialogs Open and Save As show, a path
        // one of them delivered during this frame's events, and Quit.
        if (panels.act(state)) running = false;
        if (state.settings_changed) {
            if (settings_path) |path| writeSettingsFile(state.io, path, &state.settings) catch {};
            state.settings_changed = false;
        }
        if (is_interactive) panels.tickAutosave(state, ticks);
        // A running test game is polled every frame, script or not - the
        // smoke never presses Test, so this is a no-op there, but the smoke's
        // own State still owns one (D-03: quitting leaves it running).
        panels.pollTestGame(state);
        if (script) |s| {
            if (!s.afterFrame()) running = false;
        }
    }
}

/// --smoke: the interactive mode's setup, hidden, and its loop under
/// smoke.zig's script. Failures print a "smoke FAIL:" line; there is no
/// person to show a message box to.
fn smokeRun(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: []const u8, output: []const u8, mod_folder: ?[]const u8, mod_requested: bool) !bool {
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
        std.debug.print("map-editor: smoke FAIL: the host did not start ({s}: {s})\n", .{ @errorName(err), host_mod.failureReason() });
        return false;
    };
    defer host.stop();

    var real = c_bridge.RealBridge.init(host.session);
    if (applyModArg(&real, mod_folder, mod_requested)) |reason| {
        std.debug.print("map-editor: smoke FAIL: the mod would not load: {s}\n", .{reason});
        return false;
    }
    var editor = core.editor.Editor.init(gpa, real.bridge());
    defer editor.deinit();
    var std_files: core.files.StdFiles = .{ .io = io, .dir = .cwd() };
    editor.files = std_files.files();
    var view = view_mod.View.init(gpa);
    defer view.deinit(gpa);
    var path_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
    const path = mapArgument(&path_buffer, map) orelse {
        std.debug.print("map-editor: smoke FAIL: the path {s} is too long\n", .{map});
        return false;
    };
    editor.open(path) catch {
        std.debug.print("map-editor: smoke FAIL: {s} did not open: {s}\n", .{ map, editor.status() });
        return false;
    };
    var state = panels.State.init(gpa, &editor, &view, &real, host.window, io, environ, mod_folder);
    defer state.deinit();

    var script = smoke.Script.init(&editor, &view, &real, &state, host.window, output);
    run(&host, &editor, &view, &real, &state, &script, null, false);
    if (!script.passed) {
        // A step that failed has said so; a loop that ended otherwise (a
        // quit event) has not.
        if (!script.reported) std.debug.print("map-editor: smoke FAIL: the loop ended at step {d} of {d}\n", .{ script.step + 1, smoke.script.len });
        return false;
    }
    std.debug.print("map-editor: smoke PASS ({d} steps, {d} objects, saved and reopened {s})\n", .{ smoke.script.len, script.original_objects, output });
    return true;
}

/// The game-reads-it tier of the spec's test-launch section: a unit the
/// editor placed is played by the real `Game`, headlessly, proving the whole
/// route (BkEditorTestMapPath's generated-data mount, Game's -editor-test)
/// without a person watching. Modelled on `smokeRun` and `check`, but the
/// thing under test here is the game, not the editor's own frame.
fn gameReadsIt(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: []const u8, log_path: []const u8, mod_folder: ?[]const u8, mod_requested: bool) !bool {
    var host = host_mod.Host.start(.{ .title = "Map Editor", .hidden = true }) catch |err| {
        std.debug.print("map-editor: game reads it FAIL: the host did not start ({s}: {s})\n", .{ @errorName(err), host_mod.failureReason() });
        return false;
    };
    defer host.stop();

    var real = c_bridge.RealBridge.init(host.session);
    if (applyModArg(&real, mod_folder, mod_requested)) |reason| {
        std.debug.print("map-editor: game reads it FAIL: the mod would not load: {s}\n", .{reason});
        return false;
    }
    var editor = core.editor.Editor.init(gpa, real.bridge());
    defer editor.deinit();
    // Never exercised in this mode (D-01: a test copy goes through
    // saveCopy, never editor.save), but wired for the same reason every
    // other mode is: nothing here should depend on save silently no-op'ing.
    var std_files: core.files.StdFiles = .{ .io = io, .dir = .cwd() };
    editor.files = std_files.files();
    var path_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
    const path = mapArgument(&path_buffer, map) orelse {
        std.debug.print("map-editor: game reads it FAIL: the path {s} is too long\n", .{map});
        return false;
    };
    editor.open(path) catch {
        std.debug.print("map-editor: game reads it FAIL: {s} did not open: {s}\n", .{ map, editor.status() });
        return false;
    };
    // D-01: the document below must still be exactly this, unsaved - a test
    // copy going through saveCopy, not editor.save, must never touch it.
    const original_path = try gpa.dupe(u8, editor.document.path.items);
    defer gpa.free(original_path);

    // Two settle frames (main.zig's smokeRun/check convention): the panels
    // have never been drawn here (this mode places directly through the
    // editor, no panels involved), but the bridge still needs a drawn frame
    // before it can resolve a screen point against the camera it placed.
    var frame: u32 = 0;
    while (frame < 2) : (frame += 1) {
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
        host.beginFrame();
        host.endFrame() catch |err| {
            std.debug.print("map-editor: game reads it FAIL: frame {d}: {s}\n", .{ frame, @errorName(err) });
            return false;
        };
    }

    const size = real.screenSize() orelse {
        std.debug.print("map-editor: game reads it FAIL: no screen size\n", .{});
        return false;
    };
    const sx = @as(f32, @floatFromInt(size[0])) / 2 + game_reads_it_offset.dx;
    const sy = @as(f32, @floatFromInt(size[1])) / 2 + game_reads_it_offset.dy;
    const point = editor.resolve(sx, sy) catch {
        std.debug.print("map-editor: game reads it FAIL: {d},{d} is off the terrain\n", .{ sx, sy });
        return false;
    };

    const entries = real.catalogue(gpa) catch {
        std.debug.print("map-editor: game reads it FAIL: the object catalogue did not read\n", .{});
        return false;
    };
    defer gpa.free(entries);
    const unit_name: ?[]const u8 = for (entries) |entry| {
        if (entry.game_type == view_mod.unit_game_type) break std.mem.sliceTo(&entry.name, 0);
    } else null;
    const name = unit_name orelse {
        std.debug.print("map-editor: game reads it FAIL: no SGVOGT_UNIT in the catalogue\n", .{});
        return false;
    };
    // D-04: player 0, the map's own diplomacy - a normal mission start.
    _ = editor.addObject(name, point.map_x, point.map_y, 0, 0) catch {
        std.debug.print("map-editor: game reads it FAIL: placing {s} failed: {s}\n", .{ name, editor.status() });
        return false;
    };

    var test_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const test_path = real.testMapPath(testlaunch.profile_name, null, testlaunch.map_file_name, &test_path_buffer) orelse {
        std.debug.print("map-editor: game reads it FAIL: no test map path: {s}\n", .{std.mem.span(c.BkEditorLastMessage(host.session))});
        return false;
    };
    if (real.saveCopy(test_path) != .ok) {
        std.debug.print("map-editor: game reads it FAIL: the test copy would not save: {s}\n", .{std.mem.span(c.BkEditorLastMessage(host.session))});
        return false;
    }

    // The units= verb's coordinates are SMiniMapUnitInfo's own scale, factor
    // 64 (03-01-SUMMARY.md) - but over the AI's own coordinate, which bridge.h
    // documents as MAP units, not the scene "world" units BkEditorSetCamera
    // takes (bridge.h: "Map units are the file's and the AI's"). Confirmed
    // empirically against this same test map: querying at map_x/64,map_y/64
    // with radius 1 finds the placed unit; the scene-world equivalent misses
    // it by roughly 20 units of this scale (~1300 world units away) - the two
    // coordinate systems really do disagree by more than rounding.
    const units_x: i32 = @intFromFloat(@floor(point.map_x / 64.0));
    const units_y: i32 = @intFromFloat(@floor(point.map_y / 64.0));
    var game_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const game_path = testlaunch.gamePath(io, &game_path_buffer) catch |err| {
        std.debug.print("map-editor: game reads it FAIL: no Game beside MapEditor: {s}\n", .{@errorName(err)});
        return false;
    };
    var auto_ui_buffer: [96]u8 = undefined;
    // A radius of 3 (192 world units, about 6 AI tiles) comfortably covers
    // the placed unit despite the world-to-units rounding above; it is not
    // trying to bound "nearby" tightly.
    const auto_ui = std.fmt.bufPrint(&auto_ui_buffer, "400:units={d}x{d}x3,420:shot,440:exit", .{ units_x, units_y }) catch unreachable;
    var running = testlaunch.start(gpa, io, environ, .{
        .game_path = game_path,
        .log_path = log_path,
        .extra_env = &.{ .{ "BK_AUTO_UI", auto_ui }, .{ "BK_NO_HELP", "1" } },
    }) catch |err| {
        std.debug.print("map-editor: game reads it FAIL: the game would not start: {s}\n", .{@errorName(err)});
        return false;
    };
    const exit = running.waitBlocking(io, 240_000) orelse {
        running.terminate(io);
        std.debug.print("map-editor: game reads it FAIL: the game did not exit within 240 s; its log: {s}\n", .{log_path});
        return false;
    };
    if ((exit.code orelse 1) != 0 or exit.signal != null) {
        std.debug.print("map-editor: game reads it FAIL: the game exited code={?d} signal={?d}; its log: {s}\n", .{ exit.code, exit.signal, log_path });
        return false;
    }

    const log_bytes = std.Io.Dir.cwd().readFileAlloc(io, log_path, gpa, .limited(4 << 20)) catch |err| {
        std.debug.print("map-editor: game reads it FAIL: the log at {s} would not read: {s}\n", .{ log_path, @errorName(err) });
        return false;
    };
    defer gpa.free(log_bytes);
    if (std.mem.indexOf(u8, log_bytes, "BK_AUTO_UI: shot written") == null) {
        std.debug.print("map-editor: game reads it FAIL: no \"BK_AUTO_UI: shot written\" line; see {s}\n", .{log_path});
        return false;
    }
    const units_count = playerZeroUnits(log_bytes) orelse {
        std.debug.print("map-editor: game reads it FAIL: no units line naming player 0; see {s}\n", .{log_path});
        return false;
    };
    if (units_count < 1) {
        std.debug.print("map-editor: game reads it FAIL: player 0 has {d} units near the placed one; see {s}\n", .{ units_count, log_path });
        return false;
    }
    if (!editor.dirty() or !std.mem.eql(u8, editor.document.path.items, original_path)) {
        std.debug.print("map-editor: game reads it FAIL: the document changed - dirty {}, path {s} (was {s})\n", .{ editor.dirty(), editor.document.path.items, original_path });
        return false;
    }

    deleteAutoshots(io);
    std.debug.print("map-editor: game reads it PASS ({d} units of player 0 near the placed unit, game exit 0)\n", .{units_count});
    return true;
}

/// The count after "player 0: " in a `units=` line
/// ("BK_AUTO_UI: units near X,Y r R: total N; player 0: M"), or null if the
/// log has no such line - a player with a zero count is never printed
/// (GameMain.cpp's units= handler), so its absence here is a real "0", not a
/// parse failure to confuse with one.
fn playerZeroUnits(log: []const u8) ?u32 {
    const marker = "player 0: ";
    const at = std.mem.indexOf(u8, log, marker) orelse return null;
    const rest = log[at + marker.len ..];
    var end: usize = 0;
    while (end < rest.len and std.ascii.isDigit(rest[end])) : (end += 1) {}
    if (end == 0) return null;
    return std.fmt.parseInt(u32, rest[0..end], 10) catch null;
}

/// The game's own screenshot dump (BK_AUTO_UI's `shot` action), left in the
/// working directory (the installation this mode ran from) rather than
/// zig-out/local-test - swept up so a repeat run is not mistaken for a stale
/// leftover.
fn deleteAutoshots(io: std.Io) void {
    var dir = std.Io.Dir.cwd().openDir(io, ".", .{ .iterate = true }) catch return;
    defer dir.close(io);
    var it = dir.iterate();
    while (it.next(io) catch null) |entry| {
        if (entry.kind != .file) continue;
        if (std.mem.startsWith(u8, entry.name, "autoshot_") and std.mem.endsWith(u8, entry.name, ".rgba"))
            dir.deleteFile(io, entry.name) catch {};
    }
}

/// A map path from the command line in the engine's form. It arrives as the
/// person typed it or the shell expanded it - an absolute macOS path has
/// forward slashes - and the engine's file layer splits only on '\', so it
/// goes through the conversion the file dialogs' paths go through
/// (panels_logic.enginePath). Null when it does not fit the buffer.
fn mapArgument(buffer: *[panels_logic.PathSlot.max_path]u8, typed: []const u8) ?[]const u8 {
    return panels_logic.enginePath(buffer, typed, .open);
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
    std.debug.print("usage: MapEditor [-mod=<Folder>|-mod=None] [<map>]\n       MapEditor [-mod=...] --check <map> [<out.tga>]\n       MapEditor [-mod=...] --smoke <map> [<out.bzm>]\n       MapEditor [-mod=...] --game-reads-it <map> [<log>]\n", .{});
    std.process.exit(2);
}

fn fail(comptime format: []const u8, args: anytype) bool {
    std.debug.print("map-editor: host check FAIL: " ++ format ++ "\n", args);
    return false;
}

fn check(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: []const u8, output: []const u8, mod_folder: ?[]const u8, mod_requested: bool) !bool {
    if (std.fs.path.dirname(output)) |directory| try std.Io.Dir.cwd().createDirPath(io, directory);
    var path_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
    const path = mapArgument(&path_buffer, map) orelse return fail("the path {s} is too long", .{map});
    const map_z = try gpa.dupeZ(u8, path);
    defer gpa.free(map_z);
    const output_z = try gpa.dupeZ(u8, output);
    defer gpa.free(output_z);

    var host = host_mod.Host.start(.{ .title = "Map Editor", .hidden = true }) catch |err|
        return fail("the host did not start ({s}: {s})", .{ @errorName(err), host_mod.failureReason() });
    defer host.stop();

    var real = c_bridge.RealBridge.init(host.session);
    if (applyModArg(&real, mod_folder, mod_requested)) |reason|
        return fail("the mod would not load: {s}", .{reason});
    if (mod_folder != null) {
        if (real.activeMod()) |active|
            std.debug.print("map-editor: mod {s} ({s} {s})\n", .{ std.mem.sliceTo(&active.folder, 0), std.mem.sliceTo(&active.name, 0), std.mem.sliceTo(&active.version, 0) });
    }

    var summary: c.BkEditorMapSummary = std.mem.zeroes(c.BkEditorMapSummary);
    if (c.BkEditorOpenMap(host.session, map_z.ptr, &summary) != c.BK_EDITOR_OK)
        return fail("{s} did not open: {s}", .{ map, std.mem.span(c.BkEditorLastMessage(host.session)) });
    // The open already places the camera on the map's middle; placed there
    // again here so the check covers BkEditorSetCamera too.
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
    return panelSmoke(gpa, io, environ, &host, map, output, mod_folder);
}

/// One frame of the real panels over the map, with a State from the opened
/// map, and the file actions the dialogs start, run with the path handed to
/// the slot as a dialog's callback would hand it: Save As into the output's
/// directory, then Open of what was saved. Nothing here needs a person, so
/// CI runs the panel code on both GPU runners.
/// Task 1's own verify: the BK_EDITOR_SETTINGS test seam. Skipped (true,
/// nothing printed) when the env var is unset - every real `--check` run.
/// With it set: loads the file it names, checks the view picked up its
/// scroll_speed, changes autosave_minutes through `panels.applySettings` (the
/// same path the Settings window's own controls use), writes the file back
/// and reads it again to confirm the change landed, then prints
/// "map-editor: settings round trip PASS (<path>)".
fn settingsRoundTrip(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, state: *panels.State) !bool {
    const path = environ.getAlloc(gpa, "BK_EDITOR_SETTINGS") catch |err| {
        if (err == error.EnvironmentVariableMissing) return true;
        return fail("settings: BK_EDITOR_SETTINGS did not read: {s}", .{@errorName(err)});
    };
    defer gpa.free(path);

    const loaded = std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(64 * 1024)) catch |err|
        return fail("settings: {s} did not read: {s}", .{ path, @errorName(err) });
    defer gpa.free(loaded);
    state.settings = core.settings.parse(loaded);
    panels.applySettings(state);
    if (state.view.wheel_sensitivity != state.settings.scroll_speed)
        return fail("settings: the view's sensitivity did not follow scroll_speed", .{});

    state.settings.autosave_minutes = if (state.settings.autosave_minutes < core.settings.max_autosave_minutes)
        state.settings.autosave_minutes + 1
    else
        state.settings.autosave_minutes - 1;
    panels.applySettings(state);

    var text_buffer: [8192]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&text_buffer);
    try core.settings.format(&state.settings, &writer);
    std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = writer.buffered() }) catch |err|
        return fail("settings: {s} did not write: {s}", .{ path, @errorName(err) });

    const reread = std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(64 * 1024)) catch |err|
        return fail("settings: {s} did not read back: {s}", .{ path, @errorName(err) });
    defer gpa.free(reread);
    const reparsed = core.settings.parse(reread);
    if (reparsed.autosave_minutes != state.settings.autosave_minutes)
        return fail("settings: autosave_minutes did not round-trip", .{});

    std.debug.print("map-editor: settings round trip PASS ({s})\n", .{path});
    return true;
}

fn panelSmoke(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, host: *host_mod.Host, map: []const u8, output: []const u8, mod_folder: ?[]const u8) !bool {
    var real = c_bridge.RealBridge.init(host.session);
    var editor = core.editor.Editor.init(gpa, real.bridge());
    defer editor.deinit();
    var std_files: core.files.StdFiles = .{ .io = io, .dir = .cwd() };
    editor.files = std_files.files();
    var view = view_mod.View.init(gpa);
    defer view.deinit(gpa);
    var path_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
    const path = mapArgument(&path_buffer, map) orelse return fail("panels: the path {s} is too long", .{map});
    editor.open(path) catch return fail("panels: {s} did not open through the editor: {s}", .{ map, editor.status() });
    var state = panels.State.init(gpa, &editor, &view, &real, host.window, io, environ, mod_folder);
    defer state.deinit();
    if (!try settingsRoundTrip(gpa, io, environ, &state)) return false;
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

    // Task 1 (spec Errors -> Open): test-editor-bridge's own copy of
    // coldwinter with one object renamed to a type no database knows
    // (TestUnknownObjectDoesNotStopTheOpen) lands in the same zig-out/local-test
    // directory `output` lives in - both are always b.pathFromRoot-absolute,
    // from build.zig's own run steps, so `directory` (already computed above)
    // finds it regardless of this process's own cwd. Opened through the
    // panels' own Open path, the way a person's Open would, so the warning
    // this proves is the one State.mapOpened actually builds. Skipped, not
    // failed, when that engine-tier test has not written it into this
    // zig-out yet.
    var unknown_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const unknown_path = std.fmt.bufPrint(&unknown_buffer, "{s}{c}coldwinter-unknown-object.bzm", .{ directory, std.fs.path.sep }) catch
        return fail("panels: the unknown-object path is too long", .{});
    if (std.Io.Dir.cwd().access(io, unknown_path, .{})) |_| {
        if (!state.actions.dialog.request(.open)) return fail("panels: the dialog slot was not free for the unknown-object check", .{});
        state.actions.dialog.deliver(unknown_path);
        if (panels.act(&state)) return fail("panels: opening the unknown-object map quit the editor", .{});
        if (view.statusLine().len != 0 or editor.status().len != 0)
            return fail("panels: opening {s} failed: {s}{s}", .{ unknown_path, view.statusLine(), editor.status() });
        if (state.unknown_types_count != 1 or state.unknown_objects_total != 1)
            return fail("panels: {s} opened with {d} unknown types / {d} unknown objects, want 1 / 1", .{ unknown_path, state.unknown_types_count, state.unknown_objects_total });
        std.debug.print("map-editor: unknown-object warning PASS\n", .{});
    } else |_| {
        std.debug.print("map-editor: unknown-object warning skipped (run test-editor-bridge first)\n", .{});
    }

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
