//! MapEditor:
//!   MapEditor [-mod=<Folder>|-mod=None] [<map>]                       interactive
//!   MapEditor [-mod=...] --check <map> [<out.tga>]     headless host check
//!   MapEditor [-mod=...] --smoke <map> [<out.bzm>]     scripted run of the real loop
//!   MapEditor [-mod=...] --game-reads-it <map> [<log>] headless test-launch, played by Game
//!   MapEditor [-mod=...] --game-reads-it-m2 <map> [<log>] the M2 scenario: what the game reports it read
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
//! map, draws frames with an orange ImGui window at a known place, captures
//! one frame as it was presented and checks that both the panel and the map
//! are in it, printing "map-editor: host check PASS (<driver>, <w>x<h>)". A
//! runner with no GPU device prints "map-editor: host check skipped: no GPU
//! device (<reason>)" and exits 0, the engine tier's own rule for the same
//! failure.
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
const builtin = @import("builtin");
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
const auto_mod = @import("auto.zig");
const testlaunch = @import("testlaunch.zig");
const game_reads_common = @import("game_reads_common.zig");
const game_reads_m2 = @import("game_reads_m2.zig");
const c = host_mod.c;

const default_output = "zig-out/local-test/map-editor-check.tga";
const default_smoke_output = "zig-out/local-test/map-editor-smoke.bzm";
const default_game_reads_it_log = "zig-out/local-test/map-editor-game-reads-it.log";
const default_game_reads_it_m2_log = "zig-out/local-test/map-editor-game-reads-it-m2.log";
/// BK_EDITOR_AUTO_DIR's own default (03-12-PLAN.md Task 1): shots and
/// references live here unless the environment overrides it.
const default_auto_dir = "zig-out/local-test/map-editor-auto";

/// The point Task 1's headless test launch places its unit at: the smoke's
/// own measured free ground (smoke.zig's place_at), so this mode never has
/// to characterise a shipped map's terrain a second time.
const game_reads_it_offset = struct {
    const dx: f32 = -40;
    const dy: f32 = -120;
};

/// 03-15 gap fix (lone soldier crash): the single soldiers --game-reads-it
/// tries to place - the sniper type Johannes placed from the palette's unit
/// group, and a Bren gunner - each with the squad the bridge's refusal must
/// name. Placed as units, both crashed the game's first AI segment
/// (CSoldierRestState::Segment on a null formation).
const lone_soldiers = [_]struct { soldier: []const u8, squad: []const u8 }{
    .{ .soldier = "Us_Sniper", .squad = "US_sniper" },
    .{ .soldier = "Allies_Bren", .squad = "GB_bren_43" },
};

/// The squads placed beside the unit instead, in map units from it, with the
/// soldiers each brings (Data/Squads/<name>/1.xml's first formation): the
/// one-man sniper squad and the nine-man Bren squad.
const game_reads_it_squads = [_]struct { name: []const u8, dx: f32, dy: f32, soldiers: u32 }{
    .{ .name = "US_sniper", .dx = 128, .dy = 0, .soldiers = 1 },
    .{ .name = "GB_bren_43", .dx = 0, .dy = 128, .soldiers = 9 },
};

/// SGVOGT_SOUND: the catalogue's game type for a sound, the one the Sounds
/// panel lists (panels.zig's own `sound_names`).
const sound_game_type: i32 = 100;

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

    // -mod=<Folder>/-mod=None and --hidden are pulled out of the argument
    // list first, so both are accepted in any position before the positional
    // arguments below - matching the doc comment atop this file. --hidden
    // (03-12-PLAN.md Task 1) starts the interactive mode's own host hidden,
    // like --smoke's, so `zig build map-editor-auto` never pops a window.
    var mod_arg: ?[]const u8 = null; // the raw text after "=", "None" included
    var hidden = false;
    var rest: std.ArrayList([]const u8) = .empty;
    defer rest.deinit(gpa);
    while (args.next()) |arg| {
        if (parseModArg(arg)) |raw| {
            mod_arg = raw;
        } else if (std.mem.eql(u8, arg, "--hidden")) {
            hidden = true;
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
        if (std.mem.eql(u8, arg, "--game-reads-it-m2")) {
            const map = nextArg(rest.items, &index) orelse usage();
            const log_path = nextArg(rest.items, &index) orelse default_game_reads_it_m2_log;
            if (nextArg(rest.items, &index) != null) usage();
            const passed = try game_reads_m2.run(gpa, io, minimal.environ, map, log_path, mod_folder, mod_requested);
            std.process.exit(if (passed) 0 else 1);
        }
        if (nextArg(rest.items, &index) != null) usage();
        const passed = try interactive(gpa, io, minimal.environ, arg, mod_folder, mod_requested, hidden);
        std.process.exit(if (passed) 0 else 1);
    }
    const passed = try interactive(gpa, io, minimal.environ, null, mod_folder, mod_requested, hidden);
    std.process.exit(if (passed) 0 else 1);
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

const applyModArg = game_reads_common.applyModArg;

/// The interactive mode: one window, the engine on it, the view driving the
/// core's tools, until the window closes or the process is asked to quit
/// (SDL maps SIGINT/SIGTERM to SDL_EVENT_QUIT by default). `hidden` starts
/// the host hidden (--hidden, like --smoke's own host) - used together with
/// BK_EDITOR_AUTO (03-12-PLAN.md), whose schedule this reads below.
/// Returns false only for an automated (BK_EDITOR_AUTO) run whose schedule
/// failed - `main` turns that into exit code 1; a plain interactive session
/// always returns true.
fn interactive(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: ?[]const u8, mod_folder: ?[]const u8, mod_requested: bool, hidden: bool) !bool {
    var host = host_mod.Host.start(.{ .title = "Map Editor", .hidden = hidden }) catch |err| {
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
        const path = mapArgument(io, &path_buffer, typed) orelse fatal("map open", "the map's path is too long");
        editor.open(path) catch {
            const reason = editor.status();
            fatal("map open", if (reason.len != 0) reason else "the map did not open");
        };
    }
    // Centres the view on the map opened above, if any (State.mapOpened).
    var state = panels.State.init(gpa, &editor, &view, &real, host.window, io, environ, mod_folder);
    defer state.deinit();

    // BK_EDITOR_AUTO (03-12-PLAN.md): read before settings/recovery below, so
    // an automated run never reaches either (the 03-07 rule, carried into
    // this plan's own binding constraints: automated runs never read or
    // write the user's settings, recent list or recovery folder). Leaked
    // deliberately, like BK_EDITOR_SETTINGS's override elsewhere in this
    // file - a single-shot process, and auto.zig's parsed Scheduled entries
    // borrow slices of this text for as long as the process runs.
    const auto_text: ?[]const u8 = environ.getAlloc(gpa, "BK_EDITOR_AUTO") catch null;
    const automated = auto_text != null;
    // A packaged (.windows subsystem) MapEditor.exe run under BK_EDITOR_AUTO
    // still needs its frame/action log to reach the terminal or CI's log
    // (crt.attachParentConsole's doc comment); a plain interactive session
    // never calls this - there is nothing to attach to and nothing it prints.
    if (automated) crt.attachParentConsole();

    var settings_path: ?[]const u8 = null;
    var settings_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    if (!automated) {
        // D-24: mapeditor.cfg, independent of game profiles - never read or
        // written by --check/--smoke/--game-reads-it/BK_EDITOR_AUTO, only a
        // plain interactive run. Missing is not an error (a fresh install);
        // unreadable falls back to defaults too, with a status line naming
        // why.
        settings_path = resolveSettingsPath(&settings_path_buffer, gpa, environ, std.mem.sliceTo(&state.paths.user_root, 0));
        if (settings_path) |path| {
            state.settings = readSettingsFile(io, gpa, path) catch |err| switch (err) {
                error.FileNotFound => core.settings.Settings{},
                else => blk: {
                    view.setStatus("", "the settings file did not read: using defaults");
                    break :blk core.settings.Settings{};
                },
            };
        }
    }
    view.wheel_sensitivity = state.settings.scroll_speed;

    if (!automated) {
        // D-22, spec Errors -> Crashes: offered back once, at startup, before
        // the main loop's own autosave tick could ever write a fresh one
        // under the same name.
        panels.scanRecoveryOffers(&state);
    }

    var auto_runner: ?smoke.AutoRunner = null;
    if (auto_text) |text| {
        // Automated modes must never open a real OS dialog, or land a
        // synthetic press on a panel under the real cursor - the same two
        // rules --smoke already follows (main.zig's smokeRun).
        state.os_dialogs = false;
        state.automated = true;
        imgui.c.bk_imgui_backend_use_global_mouse(false);
        var failure: auto_mod.Failure = .{};
        const schedule = auto_mod.parse(gpa, text, &failure) catch |err| {
            std.debug.print("map-editor: BK_EDITOR_AUTO: bad token '{s}': {s} ({s})\n", .{ failure.token, failure.reason, @errorName(err) });
            std.process.exit(2);
        };
        const dir: []const u8 = environ.getAlloc(gpa, "BK_EDITOR_AUTO_DIR") catch default_auto_dir;
        const game_env: ?[]const u8 = environ.getAlloc(gpa, "BK_EDITOR_AUTO_GAME") catch null;
        auto_runner = smoke.AutoRunner.init(&editor, &view, &real, &state, host.window, io, schedule, dir, game_env);
        if (environ.getAlloc(gpa, "BK_EDITOR_AUTO_GAME_TRACE")) |trace| {
            auto_runner.?.game_trace = trace.len != 0;
            gpa.free(trace);
        } else |_| {}
    }

    run(&host, &editor, &view, &real, &state, if (auto_runner) |*r| smoke.Driver{ .auto = r } else null, settings_path, !automated);
    // A plain return, not std.process.exit, so the deferred view.deinit(),
    // editor.deinit() and host.stop() above run: host.stop() takes the
    // overlay and ImGui down, BkEditorStop deletes the world, and the window
    // goes. The engine's renderer and its GPU device are not shut down - they
    // live until the process exits, as in the game - which is why one Host
    // per process is the contract (host.zig Host.stop). `main` does the
    // actual std.process.exit, after this return has let those defers run.
    return if (auto_runner) |r| !r.failed else true;
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
/// smoke, BK_EDITOR_AUTO and the panel smoke run their loops with no tick at
/// all, so they never write a map file or a recovery copy no one asked for.
///
/// `driver`: `.table` for --smoke's fixed script, `.auto` for BK_EDITOR_AUTO's
/// parsed schedule (smoke.zig's `Driver`) - null for a plain interactive run.
fn run(host: *host_mod.Host, editor: *core.editor.Editor, view: *view_mod.View, real: *c_bridge.RealBridge, state: *panels.State, driver: ?smoke.Driver, settings_path: ?[]const u8, is_interactive: bool) void {
    var running = true;
    var last_ticks: u64 = sdl3.c.SDL_GetTicks();
    while (running) {
        if (driver) |d| if (!d.beforeFrame()) break;
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) {
            if (driver) |d| d.observe(&event);
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
        // M3 (D-26): a double-click or Enter/Space on a selection opens the
        // Properties window (O15) - the view owns the ask, the panels own
        // the window.
        if (view.props_open_request) {
            view.props_open_request = false;
            state.properties_open = true;
        }
        view.update(editor, real, host.window, dt_seconds);

        host.beginFrame();
        panels.draw(state);
        // A lost frame is reported until the next frame presents: it is
        // the frame's own message, not an edit's, so no edit is needed to
        // clear it (the status line's source tag, WINDOWS.md 1).
        if (host.endFrame()) |_| view.clearStatusFrom(.frame) else |err| view.setStatusFrom(.frame, "failed: ", @errorName(err));
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
        if (driver) |d| {
            if (!d.afterFrame()) running = false;
        }
    }
}

/// --smoke: the interactive mode's setup, hidden, and its loop under
/// smoke.zig's script. Failures print a "smoke FAIL:" line; there is no
/// person to show a message box to.
fn smokeRun(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: []const u8, output: []const u8, mod_folder: ?[]const u8, mod_requested: bool) !bool {
    // See crt.attachParentConsole's doc comment: a packaged (.windows
    // subsystem) MapEditor.exe run from a terminal still needs this mode's
    // PASS/FAIL line to be visible there.
    crt.attachParentConsole();
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
    const path = mapArgument(io, &path_buffer, map) orelse {
        std.debug.print("map-editor: smoke FAIL: the path {s} is too long\n", .{map});
        return false;
    };
    editor.open(path) catch {
        std.debug.print("map-editor: smoke FAIL: {s} did not open: {s}\n", .{ map, editor.status() });
        return false;
    };
    var state = panels.State.init(gpa, &editor, &view, &real, host.window, io, environ, mod_folder);
    defer state.deinit();
    // The script's due autosave (smoke.zig's `autosave_due`) writes a
    // recovery copy under the user root: beside the smoke's output, never
    // into the person's own user folder, where the next real start would
    // offer it back.
    if (!pointUserRootBeside(&state, output)) {
        std.debug.print("map-editor: smoke FAIL: the smoke's own user root beside {s} does not fit\n", .{output});
        return false;
    }
    // The script answers every Open and Save As through the dialog slot
    // itself; a real OS dialog would stay up for the rest of the run and
    // take the window's focus and pointer whenever it appeared (Windows).
    state.os_dialogs = false;
    // Isolates the script's synthetic pointer from the real cursor
    // (03-12-PLAN.md Task 2, plan 5 Task 7.1 carried): see
    // bk_imgui_backend_use_global_mouse's own doc comment for what this
    // does and does not close off.
    imgui.c.bk_imgui_backend_use_global_mouse(false);

    var script = smoke.Script.init(&editor, &view, &real, &state, host.window, output);
    run(&host, &editor, &view, &real, &state, smoke.Driver{ .table = &script }, null, false);
    if (!script.passed) {
        // A step that failed has said so; a loop that ended otherwise (a
        // quit event) has not.
        if (!script.reported) std.debug.print("map-editor: smoke FAIL: the loop ended at step {d} of {d}\n", .{ script.step + 1, smoke.script.len });
        return false;
    }
    script.printNote();
    std.debug.print("map-editor: smoke PASS ({d} steps, {d} objects, saved and reopened {s})\n", .{ smoke.script.len, script.original_objects, output });
    return true;
}

/// `state.paths.user_root` becomes `<dir of output>/smoke.smoke_user_root/`
/// (with the OS's trailing separator, as BkEditorPaths gives it). False when
/// it does not fit.
fn pointUserRootBeside(state: *panels.State, output: []const u8) bool {
    const dir = std.fs.path.dirname(output) orelse ".";
    var buffer: [@sizeOf(@TypeOf(state.paths.user_root))]u8 = undefined;
    const root = std.fmt.bufPrint(&buffer, "{s}{c}{s}{c}", .{ dir, std.fs.path.sep, smoke.smoke_user_root, std.fs.path.sep }) catch return false;
    if (root.len >= state.paths.user_root.len) return false;
    @memset(&state.paths.user_root, 0);
    @memcpy(state.paths.user_root[0..root.len], root);
    return true;
}

/// The game-reads-it tier of the spec's test-launch section: a unit the
/// editor placed is played by the real `Game`, headlessly, proving the whole
/// route (BkEditorTestMapPath's generated-data mount, Game's -editor-test)
/// without a person watching. Modelled on `smokeRun` and `check`, but the
/// thing under test here is the game, not the editor's own frame.
fn gameReadsIt(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: []const u8, log_path: []const u8, mod_folder: ?[]const u8, mod_requested: bool) !bool {
    // The shared start (game_reads_common.zig): host, -mod=, the map opened
    // through the core Editor on the real bridge, two settle frames - this
    // mode places directly through the editor, no panels involved, but the
    // bridge still needs a drawn frame before it can resolve a screen point
    // against the camera it placed.
    var rig: game_reads_common.Rig = .{};
    defer rig.deinit();
    if (!rig.open(gpa, io, "game reads it", map, mod_folder, mod_requested)) return false;
    const real = &rig.real;
    const editor = &rig.editor;
    // D-01: the document below must still be exactly this, unsaved - a test
    // copy going through saveCopy, not editor.save, must never touch it.
    const original_path = try gpa.dupe(u8, editor.document.path.items);
    defer gpa.free(original_path);

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
        if (entry.game_type == view_mod.unit_game_type and entry.placeable != 0) break std.mem.sliceTo(&entry.name, 0);
    } else null;
    const name = unit_name orelse {
        std.debug.print("map-editor: game reads it FAIL: no SGVOGT_UNIT in the catalogue\n", .{});
        return false;
    };
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
    var paths: game_reads_common.TestPaths = .{};
    if (!paths.resolve(&rig, io, "game reads it")) return false;
    const test_path = paths.test_path;
    const game_path = paths.game_path;

    // The baseline: player 0's units at that spot on the map as shipped,
    // counted by the game before anything is placed, so the check below
    // asserts what the edits added rather than relying on the spot being
    // empty ground (03-VERIFICATION.md: an undeclared precondition).
    if (!game_reads_common.saveTestCopy(&rig, "game reads it", "unedited test copy", test_path)) return false;
    var baseline_log_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const baseline_log = std.fmt.bufPrint(&baseline_log_buffer, "{s}.baseline.log", .{log_path}) catch {
        std.debug.print("map-editor: game reads it FAIL: the path {s} is too long\n", .{log_path});
        return false;
    };
    const baseline = gameReadsItBaseline(gpa, io, environ, game_path, baseline_log, units_x, units_y) orelse return false;
    std.debug.print("map-editor: game reads it: player 0 has {d} units there on the unedited map\n", .{baseline});

    // D-04: player 0, the map's own diplomacy - a normal mission start.
    _ = editor.addObject(name, point.map_x, point.map_y, 0, 0) catch {
        std.debug.print("map-editor: game reads it FAIL: placing {s} failed: {s}\n", .{ name, editor.status() });
        return false;
    };
    // A single soldier must be refused, naming its squad. One the editor
    // does place anyway is remembered and failed only after the game has
    // run, so a regression shows what the game makes of it too.
    var lone_placed: ?[]const u8 = null;
    for (lone_soldiers) |lone| {
        if (editor.addObject(lone.soldier, point.map_x + 64, point.map_y + 64, 0, 0)) |_| {
            lone_placed = lone.soldier;
            std.debug.print("map-editor: game reads it: the single soldier {s} was placed\n", .{lone.soldier});
        } else |_| {
            if (std.mem.indexOf(u8, editor.status(), lone.squad) == null) {
                std.debug.print("map-editor: game reads it FAIL: {s} was refused without naming the squad {s}: {s}\n", .{ lone.soldier, lone.squad, editor.status() });
                return false;
            }
            std.debug.print("map-editor: game reads it: {s} refused: {s}\n", .{ lone.soldier, editor.status() });
        }
    }
    var squad_soldiers: u32 = 0;
    for (game_reads_it_squads) |squad| {
        _ = editor.addObject(squad.name, point.map_x + squad.dx, point.map_y + squad.dy, 0, 0) catch {
            std.debug.print("map-editor: game reads it FAIL: placing the squad {s} failed: {s}\n", .{ squad.name, editor.status() });
            return false;
        };
        squad_soldiers += squad.soldiers;
    }

    // 03-15 gap fix (map sound in the test game): the Sounds panel's own
    // default sound, placed where the unit stands. The game plays a map
    // sound only while the view is near it, so the schedule below moves the
    // view there (camera=, world units - a sound's own) and BK_SOUND_TRACE
    // names what the sound scene registers and starts.
    var sound_names: std.ArrayListUnmanaged([]const u8) = .empty;
    defer sound_names.deinit(gpa);
    for (entries) |*entry| {
        if (entry.game_type == sound_game_type and entry.name[0] != 0) try sound_names.append(gpa, std.mem.sliceTo(&entry.name, 0));
    }
    const sound_name = panels_logic.defaultSoundName(sound_names.items) orelse {
        std.debug.print("map-editor: game reads it FAIL: no sound in the catalogue\n", .{});
        return false;
    };
    var sound: core.bridge.SoundRecord = .{ .x = point.world_x, .y = point.world_y };
    sound.setName(sound_name);
    editor.addSound(-1, sound) catch {
        std.debug.print("map-editor: game reads it FAIL: placing the sound {s} failed: {s}\n", .{ sound_name, editor.status() });
        return false;
    };

    if (!game_reads_common.saveTestCopy(&rig, "game reads it", "test copy", test_path)) return false;

    var auto_ui_buffer: [160]u8 = undefined;
    // camera= at frame 150: the mission (and its own start view) is up by
    // then (frame 120 already reports game time), and the sound scene
    // looks at the map's sounds near the view every 3 s. Exit at 700, about
    // 23 s of a debug build's game time: a map sound cell runs again 3 to
    // 13 s after it last did, so a loop still stopped and restarted there
    // (the looped_starts check below) shows a second start well before.
    const auto_ui = std.fmt.bufPrint(&auto_ui_buffer, "150:camera={d:.0}x{d:.0},400:units={d}x{d}x{d},420:shot,700:exit", .{ point.world_x, point.world_y, units_x, units_y, game_reads_it_radius }) catch unreachable;
    // BK_AUDIO_NULL: the sound scene runs in full (the trace needs it), but
    // into the null device - nothing plays through the Mac's own output.
    const log_bytes = game_reads_common.runGame(gpa, io, environ, "game reads it", "game", game_path, log_path, &.{ .{ "BK_AUTO_UI", auto_ui }, .{ "BK_NO_HELP", "1" }, .{ "BK_SOUND_TRACE", "1" }, .{ "BK_AUDIO_NULL", "1" } }) orelse return false;
    defer gpa.free(log_bytes);
    if (std.mem.indexOf(u8, log_bytes, "BK_AUTO_UI: shot written") == null) {
        std.debug.print("map-editor: game reads it FAIL: no \"BK_AUTO_UI: shot written\" line; see {s}\n", .{log_path});
        return false;
    }
    const units_count = testlaunch.playerUnitsNear(log_bytes, 0) orelse {
        std.debug.print("map-editor: game reads it FAIL: no units= line; see {s}\n", .{log_path});
        return false;
    };
    // What was there already, plus the unit (at least itself) and every
    // soldier of both squads.
    if (units_count < baseline + 1 + squad_soldiers) {
        std.debug.print("map-editor: game reads it FAIL: player 0 has {d} units near the placed ones, fewer than the {d} there before plus the unit and the squads' {d} soldiers; see {s}\n", .{ units_count, baseline, squad_soldiers, log_path });
        return false;
    }
    if (lone_placed) |soldier| {
        std.debug.print("map-editor: game reads it FAIL: the editor placed the single soldier {s}; a soldier goes on a map only inside a squad\n", .{soldier});
        return false;
    }
    const sound_trace = testlaunch.mapSoundTrace(log_bytes, sound_name, point.world_x, point.world_y);
    if (!sound_trace.registered) {
        std.debug.print("map-editor: game reads it FAIL: the game never handed the map's sound {s} at {d:.0},{d:.0} to its sound scene; see {s}\n", .{ sound_name, point.world_x, point.world_y, log_path });
        return false;
    }
    if (!sound_trace.started) {
        std.debug.print("map-editor: game reads it FAIL: the map's sound {s} at {d:.0},{d:.0} was registered but never started with the view on it; see {s}\n", .{ sound_name, point.world_x, point.world_y, log_path });
        return false;
    }
    // The view never leaves it, so a loop starts once and plays on.
    if (sound_trace.looped_starts > 1) {
        std.debug.print("map-editor: game reads it FAIL: the map's looped sound {s} was started {d} times with the view on it - stopped and started again, not playing on; see {s}\n", .{ sound_name, sound_trace.looped_starts, log_path });
        return false;
    }
    if (!editor.dirty() or !std.mem.eql(u8, editor.document.path.items, original_path)) {
        std.debug.print("map-editor: game reads it FAIL: the document changed - dirty {}, path {s} (was {s})\n", .{ editor.dirty(), editor.document.path.items, original_path });
        return false;
    }

    deleteAutoshots(io, game_path);
    std.debug.print("map-editor: game reads it PASS ({d} units of player 0 near the placed unit, {d} before placing, plus the unit and the squads' {d} soldiers; single soldiers refused; the map's sound {s} started; game exit 0)\n", .{ units_count, baseline, squad_soldiers, sound_name });
    return true;
}

/// A radius of 5 (320 map units, about 10 AI tiles) comfortably covers the
/// placed unit and both squads (128 map units off, their soldiers spread
/// around that) despite the world-to-units rounding; it is not trying to
/// bound "nearby" tightly. The baseline query uses the same radius.
const game_reads_it_radius = 5;

/// --game-reads-it's baseline run: the unedited test copy (already saved)
/// played by the game just long enough to answer `units=` at the spot the
/// edits will go, frame 400 as in the edited run. Player 0's count there, or
/// null after printing why it could not be had.
fn gameReadsItBaseline(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, game_path: []const u8, log_path: []const u8, units_x: i32, units_y: i32) ?u32 {
    var auto_ui_buffer: [96]u8 = undefined;
    const auto_ui = std.fmt.bufPrint(&auto_ui_buffer, "400:units={d}x{d}x{d},410:exit", .{ units_x, units_y, game_reads_it_radius }) catch unreachable;
    const log_bytes = game_reads_common.runGame(gpa, io, environ, "game reads it", "baseline game", game_path, log_path, &.{ .{ "BK_AUTO_UI", auto_ui }, .{ "BK_NO_HELP", "1" }, .{ "BK_AUDIO_NULL", "1" } }) orelse return null;
    defer gpa.free(log_bytes);
    return testlaunch.playerUnitsNear(log_bytes, 0) orelse {
        std.debug.print("map-editor: game reads it FAIL: the baseline run printed no units= line; see {s}\n", .{log_path});
        return null;
    };
}

const mapArgument = game_reads_common.mapArgument;
const deleteAutoshots = game_reads_common.deleteAutoshots;

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
    // See crt.attachParentConsole's doc comment: a packaged (.windows
    // subsystem) MapEditor.exe run with bad arguments from a terminal still
    // needs this message to be visible there.
    crt.attachParentConsole();
    std.debug.print("usage: MapEditor [-mod=<Folder>|-mod=None] [<map>]\n       MapEditor [-mod=...] --check <map> [<out.tga>]\n       MapEditor [-mod=...] --smoke <map> [<out.bzm>]\n       MapEditor [-mod=...] --game-reads-it <map> [<log>]\n       MapEditor [-mod=...] --game-reads-it-m2 <map> [<log>]\n", .{});
    std.process.exit(2);
}

fn fail(comptime format: []const u8, args: anytype) bool {
    std.debug.print("map-editor: host check FAIL: " ++ format ++ "\n", args);
    return false;
}

fn check(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: []const u8, output: []const u8, mod_folder: ?[]const u8, mod_requested: bool) !bool {
    // See crt.attachParentConsole's doc comment: a packaged (.windows
    // subsystem) MapEditor.exe run from a terminal still needs this mode's
    // PASS/FAIL line to be visible there.
    crt.attachParentConsole();
    if (std.fs.path.dirname(output)) |directory| try std.Io.Dir.cwd().createDirPath(io, directory);
    var path_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
    const path = mapArgument(io, &path_buffer, map) orelse return fail("the path {s} is too long", .{map});
    const map_z = try gpa.dupeZ(u8, path);
    defer gpa.free(map_z);
    const output_z = try gpa.dupeZ(u8, output);
    defer gpa.free(output_z);

    var host = host_mod.Host.start(.{ .title = "Map Editor", .hidden = true }) catch |err| {
        // A real window on which the renderer would not start - what a
        // runner without a GPU reports - is a skip, not a failure, the same
        // rule test-editor-bridge's own engine tier already follows for
        // BK_EDITOR_NO_DEVICE. Every other start failure still fails.
        if (err == error.NoDevice) {
            std.debug.print("map-editor: host check skipped: no GPU device ({s})\n", .{host_mod.failureReason()});
            return true;
        }
        return fail("the host did not start ({s}: {s})", .{ @errorName(err), host_mod.failureReason() });
    };
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
    const inside = image.pixel(inside_x, inside_y) orelse
        return fail("the probe window's centre ({d},{d}) is outside the {d}x{d} capture", .{ inside_x, inside_y, image.width, image.height });
    if (!inside.isProbeColour())
        return fail("the probe window's centre ({d},{d}) is ({d},{d},{d}), not orange", .{ inside_x, inside_y, inside.r, inside.g, inside.b });
    // The screen's centre is far from the probe; the map is drawn there.
    const outside_x: u32 = @intCast(@divTrunc(width, 2));
    const outside_y: u32 = @intCast(@divTrunc(height, 2));
    const outside = image.pixel(outside_x, outside_y) orelse
        return fail("the screen's centre ({d},{d}) is outside the {d}x{d} capture", .{ outside_x, outside_y, image.width, image.height });
    if (outside.isProbeColour() or outside.near(clear_colour))
        return fail("the screen's centre ({d},{d}) is ({d},{d},{d}), not the map", .{ outside_x, outside_y, outside.r, outside.g, outside.b });

    // Cmd+W is File > Close, not SDL's Window > Close (which quits).
    if (builtin.os.tag == .macos and !host_mod.command_w_freed)
        return fail("Cmd+W still belongs to the Window menu's Close, which would quit the editor", .{});

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
    const path = mapArgument(io, &path_buffer, map) orelse return fail("panels: the path {s} is too long", .{map});
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
    if (mod_folder) |folder| return modSwitchClosesMap(host, &state, folder);
    return true;
}

/// D-26, revised 2026-09-29 in the hand try: with `-mod=<folder>` (the
/// host check's own -mod=EditorTestMod run), File > Mod through the panels'
/// own `act`: choosing the active mod again changes nothing; choosing None on
/// a dirty map asks first (D-23) and changes nothing yet; Don't save then
/// closes the map and switches, the palette re-read from the new database;
/// and switching back with no map open goes straight through. Prints
/// "map-editor: mod switch PASS (...)".
fn modSwitchClosesMap(host: *host_mod.Host, state: *panels.State, folder: []const u8) bool {
    const editor = state.editor;
    if (!panels.mapIsOpen(editor)) return fail("mod switch: no map is open to start with", .{});

    state.actions.requestSwitchMod(folder, state.modFolder());
    if (panels.act(state)) return fail("mod switch: choosing the active mod quit the editor", .{});
    if (!panels.mapIsOpen(editor) or state.actions.prompt.isAsking())
        return fail("mod switch: choosing the active mod {s} again was not a no-op", .{folder});

    panels.addSoundAtViewCentre(state);
    if (!editor.dirty()) return fail("mod switch: adding a sound did not dirty the map: {s}{s}", .{ state.view.statusLine(), editor.status() });
    const generation = state.catalogue_generation;
    state.actions.requestSwitchMod("", state.modFolder());
    if (panels.act(state)) return fail("mod switch: File > Mod None quit the editor", .{});
    if (!state.actions.prompt.isAsking()) return fail("mod switch: File > Mod on a dirty map did not ask first", .{});
    if (!panels.mapIsOpen(editor) or !editor.dirty()) return fail("mod switch: the dirty map changed before the prompt was answered", .{});
    if (!modFolderIs(state, folder)) return fail("mod switch: the mod changed before the prompt was answered", .{});
    if (!panelFrame(host, state)) return false;

    state.actions.answer_pending = .dont_save;
    if (panels.act(state)) return fail("mod switch: Don't save quit the editor", .{});
    if (state.view.statusLine().len != 0) return fail("mod switch: {s}", .{state.view.statusLine()});
    if (panels.mapIsOpen(editor)) return fail("mod switch: {s} is still open after switching to None", .{editor.document.path.items});
    if (editor.dirty() or editor.history.canUndo()) return fail("mod switch: the closed map's undo history was kept", .{});
    if (!modFolderIs(state, null)) return fail("mod switch: the mod is not None after switching to None", .{});
    if (state.catalogue_generation == generation or state.catalogue.len == 0)
        return fail("mod switch: the palette was not re-read from the base game", .{});
    const base_entries = state.catalogue.len;
    if (!panelFrame(host, state)) return false;

    const generation_none = state.catalogue_generation;
    state.actions.requestSwitchMod(folder, state.modFolder());
    if (panels.act(state)) return fail("mod switch: switching back quit the editor", .{});
    if (state.actions.prompt.isAsking()) return fail("mod switch: a switch with no map open asked", .{});
    if (state.view.statusLine().len != 0) return fail("mod switch: {s}", .{state.view.statusLine()});
    if (panels.mapIsOpen(editor)) return fail("mod switch: a map opened on switching back", .{});
    if (!modFolderIs(state, folder)) return fail("mod switch: the mod is not {s} after switching back", .{folder});
    if (state.catalogue_generation == generation_none or state.catalogue.len == 0)
        return fail("mod switch: the palette was not re-read from {s}", .{folder});
    if (!panelFrame(host, state)) return false;

    std.debug.print("map-editor: mod switch PASS ({s} -> None -> {s}; asked first, map closed, palette {d} -> {d} entries)\n", .{ folder, folder, base_entries, state.catalogue.len });
    return true;
}

/// The panels' mod and the bridge's both `want` (null: None).
fn modFolderIs(state: *panels.State, want: ?[]const u8) bool {
    const want_text = want orelse "";
    if (!std.mem.eql(u8, state.modFolder() orelse "", want_text)) return false;
    // `|*m|`: a slice of a by-value capture would dangle once the `if` ends.
    var active = state.real.activeMod();
    const bridge_folder: []const u8 = if (active) |*m| std.mem.sliceTo(&m.folder, 0) else "";
    return std.mem.eql(u8, bridge_folder, want_text);
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
    // Orange (255,128,0), not magenta: a channel-swapped readback (red and
    // blue exchanged) turns magenta (255,0,255) right back into magenta, so
    // that colour could never see the bug it was meant to catch. Orange's
    // channels are all different, so any swap moves the measured colour
    // outside isProbeColour's asymmetric range below.
    imgui.c.igPushStyleColorImVec4(imgui.c.ImGuiCol_WindowBg, .{ .x = 1, .y = 128.0 / 255.0, .z = 0, .w = 1 });
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

    /// The probe's orange (255,128,0), with enough slack for capture
    /// rounding but asymmetric enough between channels that a red/blue
    /// swap in the readback (magenta could never show this: swapping its
    /// (255,0,255) channels gives back (255,0,255)) fails the check instead
    /// of passing it.
    fn isProbeColour(self: Rgb) bool {
        return self.r > 200 and self.g > 100 and self.g < 160 and self.b < 40;
    }
};

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

    /// Null for a point outside the image (a tiny capture on a small
    /// screen), so a caller prints a FAIL line naming the point and the
    /// size instead of an out-of-bounds panic.
    fn pixel(self: Tga, x: u32, y: u32) ?Rgb {
        if (x >= self.width or y >= self.height) return null;
        const row = if (self.top_first) y else self.height - 1 - y;
        const i = (@as(usize, row) * self.width + x) * 4;
        return .{ .r = self.pixels[i + 2], .g = self.pixels[i + 1], .b = self.pixels[i] };
    }
};
