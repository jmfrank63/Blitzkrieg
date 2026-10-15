//! ELK, the Blitzkrieg localisation kit (D-01): its own executable, no engine.
//!
//!   ELK [elk.xml] [--developer | --short | -short]
//!   ELK --smoke <database folder> <scratch folder>
//!   ELK --auto <elk.xml>            (the schedule in BK_ELK_AUTO)
//!
//! Without a path ELK reopens the most recent database from `elk.cfg`. This
//! slice is the extended layout (D-04's default); `--short` is accepted and
//! starts the extended layout until the translator layout exists.
//! `--smoke` copies the database's `elk.xml` and `<element>_data_base/`
//! folders into the scratch folder, opens the copy in a hidden window and
//! replays a fixed schedule over the list, the edit boxes and every save
//! point; `--auto` replays `BK_ELK_AUTO` on the database given.
const std = @import("std");
const builtin = @import("builtin");
const kit = @import("editor_kit");
const sdl3 = @import("sdl3");
const elk = @import("elk_core");
const bk_memory = @import("bk_memory_allocator");
const window_mod = @import("window.zig");
const fonts = @import("fonts.zig");
const session_mod = @import("session.zig");
const settings_mod = @import("settings.zig");
const ui_mod = @import("ui.zig");
const auto = @import("auto.zig");

const crt = kit.crt;
const c = sdl3.c;

/// The smoke's text and what it types into it: one leave, one Ctrl+S, one
/// Close and one Exit, each adding a letter the disk must then hold.
const smoke_key = "medals/1/name";
const smoke_final = "ELK-SMOKEXYZ";
const smoke_schedule =
    "2:expect=open,2:expect=texts:2852,2:expect=glyphs,3:expect=rows,3:expect=title:elk.xml," ++
    "4:do=select_key:" ++ smoke_key ++ ",6:expect=original_has:Medal,6:expect=dirty:0," ++
    "6:do=focus:translation,9:text=ELK-SMOKE,12:expect=translation:ELK-SMOKE,12:expect=dirty:1," ++
    "12:expect=disk:" ++ smoke_key ++ "=," ++
    // Leaving the text is a save point; visiting row 0 writes nothing.
    "13:do=select:0,15:expect=disk:" ++ smoke_key ++ "=ELK-SMOKE,15:expect=state:" ++ smoke_key ++ "=translated," ++
    "16:do=select_key:" ++ smoke_key ++ ",17:expect=untouched:0," ++
    // Ctrl+S, typed while the translation box has the keyboard.
    "18:do=focus:translation,21:key=end+ctrl,23:text=X,25:expect=translation:ELK-SMOKEX," ++
    "26:key=s+ctrl,28:expect=disk:" ++ smoke_key ++ "=ELK-SMOKEX,28:expect=dirty:0," ++
    // Close.
    "29:key=end+ctrl,31:text=Y,33:expect=translation:ELK-SMOKEXY,33:do=close,35:expect=closed," ++
    "35:expect=disk:" ++ smoke_key ++ "=ELK-SMOKEXY," ++
    // Exit, checked on disk after the window is gone.
    "36:do=reopen,38:expect=open,38:do=select_key:" ++ smoke_key ++ ",40:do=focus:translation," ++
    "43:key=end+ctrl,45:text=Z,47:expect=dirty:1,48:exit";

/// A scripted run that has not exited by then has hung.
const smoke_frame_limit = 200;
const auto_frame_limit = 100_000;

pub fn main(minimal: std.process.Init.Minimal) !void {
    crt.routeCrtReportsToStderr();
    // The exit comes after run's own defers, so BkMemory's exit report
    // counts no argument list as leaked.
    std.process.exit(try run(minimal));
}

fn run(minimal: std.process.Init.Minimal) !u8 {
    const gpa = bk_memory.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{ .environ = minimal.environ });
    defer threaded.deinit();
    const io = threaded.io();

    var args = try std.process.Args.Iterator.initAllocator(minimal.args, gpa);
    defer args.deinit();
    _ = args.next();
    var rest: std.ArrayList([]const u8) = .empty;
    defer rest.deinit(gpa);
    var short = false;
    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--short") or std.mem.eql(u8, arg, "-short")) {
            short = true;
        } else if (std.mem.eql(u8, arg, "--developer") or std.mem.eql(u8, arg, "-developer")) {
            short = false;
        } else try rest.append(gpa, arg);
    }
    const items = rest.items;
    if (items.len > 0 and std.mem.eql(u8, items[0], "--smoke")) {
        if (items.len != 3) usage();
        return smoke(gpa, io, minimal.environ, items[1], items[2]);
    }
    if (items.len > 0 and std.mem.eql(u8, items[0], "--auto")) {
        if (items.len != 2) usage();
        return autoMode(gpa, io, minimal.environ, items[1]);
    }
    if (items.len > 1 or (items.len == 1 and std.mem.startsWith(u8, items[0], "-"))) usage();
    if (short) std.debug.print("elk: the short translator layout comes in a later slice; starting the extended layout\n", .{});
    return interactive(gpa, io, minimal.environ, if (items.len == 1) items[0] else null);
}

fn usage() noreturn {
    crt.attachParentConsole();
    std.debug.print(
        \\usage: ELK [elk.xml] [--developer | --short]
        \\       ELK --smoke <database folder> <scratch folder>
        \\       ELK --auto <elk.xml>    (the schedule in BK_ELK_AUTO)
        \\
    , .{});
    std.process.exit(2);
}

fn interactive(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, first: ?[]const u8) u8 {
    var window = window_mod.Window.start(.{ .title = ui_mod.app_title }) catch |err| {
        crt.attachParentConsole();
        std.debug.print("elk: the window did not start ({s}: {s})\n", .{ @errorName(err), window_mod.failureReason() });
        return 1;
    };
    defer window.stop();
    const windir = envValue(gpa, environ, "WINDIR");
    defer if (windir) |value| gpa.free(value);
    _ = fonts.load(io, windir);

    var settings: settings_mod.Settings = .{};
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    var root_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const override = envValue(gpa, environ, settings_mod.settings_env);
    defer if (override) |value| gpa.free(value);
    const settings_path = settings_mod.settingsPath(&path_buffer, override, userRoot(gpa, environ, &root_buffer)) orelse "";
    if (settings_path.len != 0) {
        if (std.Io.Dir.cwd().readFileAlloc(io, settings_path, gpa, .limited(64 * 1024))) |bytes| {
            defer gpa.free(bytes);
            settings = settings_mod.parse(bytes);
        } else |_| {}
    }

    var session = session_mod.Session.init(gpa, io);
    defer session.deinit();
    var ui: ui_mod.Ui = .{ .gpa = gpa, .io = io, .session = &session, .settings = &settings, .settings_path = settings_path };
    var recent_copy: [settings_mod.max_path]u8 = undefined;
    if (first) |path| {
        ui.openDatabase(path);
    } else if (settings.recentCount() != 0) {
        const recent = settings.recentAt(0);
        @memcpy(recent_copy[0..recent.len], recent);
        ui.openDatabase(recent_copy[0..recent.len]);
    }
    const clean = loop(&window, &ui, null, std.math.maxInt(u32));
    ui.saveSettings();
    if (!clean) std.debug.print("elk: a frame failed: {s}\n", .{window_mod.failureReason()});
    return if (clean) 0 else 1;
}

/// The frame loop every mode shares. With a runner, its schedule's entries
/// for a frame run before that frame; returns false when one failed, the
/// frame failed or `frame_limit` passed first.
fn loop(window: *window_mod.Window, ui: *ui_mod.Ui, runner: ?*auto.Runner, frame_limit: u32) bool {
    var frame: u32 = 0;
    while (!ui.quit) : (frame += 1) {
        var event: c.SDL_Event = undefined;
        while (c.SDL_PollEvent(&event)) {
            _ = window.handleEvent(&event);
            if (event.type == c.SDL_EVENT_QUIT or event.type == c.SDL_EVENT_WINDOW_CLOSE_REQUESTED) ui.requestExit();
        }
        if (runner) |r| {
            if (!r.step(frame, window, ui)) return false;
            if (ui.quit) break;
        }
        if (frame >= frame_limit) {
            if (runner) |r| r.failure = "the schedule did not exit in time";
            return false;
        }
        window.beginFrame();
        ui.draw(window);
        window.endFrame() catch {
            if (runner) |r| r.failure = window_mod.failureReason();
            return false;
        };
    }
    return true;
}

fn smoke(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) u8 {
    crt.attachParentConsole();
    copyDatabase(gpa, io, source, scratch) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, scratch, @errorName(err) });
        return 1;
    };
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ scratch, std.fs.path.sep }) catch return 1;
    const result = scripted(gpa, io, environ, elk_path, smoke_schedule, source, smoke_frame_limit, "smoke");
    if (result.code != 0) return result.code;

    // Exit was the last save point: the disk holds the last letter typed.
    var database = elk.database.Database.open(gpa, io, elk_path) catch |err| {
        std.debug.print("elk: smoke FAIL: the copy did not reopen: {s}\n", .{@errorName(err)});
        return 1;
    };
    defer database.deinit();
    const saved = database.readTranslation(gpa, 0, smoke_key) catch |err| {
        std.debug.print("elk: smoke FAIL: {s} did not read back: {s}\n", .{ smoke_key, @errorName(err) });
        return 1;
    };
    defer gpa.free(saved);
    if (!std.mem.eql(u8, saved, smoke_final)) {
        std.debug.print("elk: smoke FAIL: after Exit {s} holds \"{s}\", wanted \"{s}\"\n", .{ smoke_key, saved, smoke_final });
        return 1;
    }
    std.debug.print("elk: smoke PASS: {d} frames, {d} fonts, list, edit and the four save points on {s}\n", .{ result.frames, result.fonts, elk_path });
    return 0;
}

fn autoMode(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, elk_path: []const u8) u8 {
    crt.attachParentConsole();
    const text = envValue(gpa, environ, "BK_ELK_AUTO") orelse {
        std.debug.print("elk: auto FAIL: BK_ELK_AUTO is not set\n", .{});
        return 2;
    };
    defer gpa.free(text);
    const result = scripted(gpa, io, environ, elk_path, text, "", auto_frame_limit, "auto");
    if (result.code == 0) std.debug.print("elk: auto PASS: {d} frames\n", .{result.frames});
    return result.code;
}

const Scripted = struct { code: u8, frames: u32 = 0, fonts: usize = 0 };

/// A hidden window on `elk_path`, the schedule replayed over it. Settings
/// are neither read nor written. No GPU device is a failure, never a pass
/// (the spec's app tier).
fn scripted(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, elk_path: []const u8, text: []const u8, source: []const u8, frame_limit: u32, tier: []const u8) Scripted {
    var failure: kit.auto_schedule.Failure = .{};
    const entries = kit.auto_schedule.parse(gpa, text, &failure) catch |err| {
        std.debug.print("elk: {s} FAIL: the schedule does not parse at \"{s}\": {s} ({s})\n", .{ tier, failure.token, failure.reason, @errorName(err) });
        return .{ .code = 2 };
    };
    defer gpa.free(entries);
    var window = window_mod.Window.start(.{ .title = ui_mod.app_title, .hidden = true }) catch |err| {
        std.debug.print("elk: {s} FAIL: the window did not start ({s}: {s})\n", .{ tier, @errorName(err), window_mod.failureReason() });
        return .{ .code = 1 };
    };
    defer window.stop();
    const windir = envValue(gpa, environ, "WINDIR");
    defer if (windir) |value| gpa.free(value);
    const loaded = fonts.load(io, windir);

    var session = session_mod.Session.init(gpa, io);
    defer session.deinit();
    var ui: ui_mod.Ui = .{ .gpa = gpa, .io = io, .session = &session, .settings = null };
    ui.openDatabase(elk_path);
    var runner: auto.Runner = .{ .entries = entries, .source_folder = source };
    runner.remember(session.openPath());
    var frames: u32 = 0;
    const passed = loopCounting(&window, &ui, &runner, frame_limit, &frames);
    if (!passed) {
        std.debug.print("elk: {s} FAIL: {s}\n", .{ tier, runner.failure });
        return .{ .code = 1, .frames = frames };
    }
    if (!runner.done()) {
        std.debug.print("elk: {s} FAIL: the run ended before its schedule did\n", .{tier});
        return .{ .code = 1, .frames = frames };
    }
    return .{ .code = 0, .frames = frames, .fonts = loaded };
}

fn loopCounting(window: *window_mod.Window, ui: *ui_mod.Ui, runner: *auto.Runner, frame_limit: u32, frames: *u32) bool {
    const passed = loop(window, ui, runner, frame_limit);
    frames.* = if (runner.next > 0) runner.entries[runner.next - 1].frame else 0;
    return passed;
}

/// The database's project file and its `<element>_data_base/` folders, the
/// rest of the folder (the old binaries, packs and updates) left out.
fn copyDatabase(gpa: std.mem.Allocator, io: std.Io, source: []const u8, scratch: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, scratch) catch {};
    try cwd.createDirPath(io, scratch);
    var from = try cwd.openDir(io, source, .{ .iterate = true });
    defer from.close(io);
    var to = try cwd.openDir(io, scratch, .{});
    defer to.close(io);
    try copyFile(gpa, io, from, to, "elk.xml");
    var entries = from.iterate();
    while (try entries.next(io)) |entry| {
        if (entry.kind != .directory) continue;
        if (!std.ascii.endsWithIgnoreCase(entry.name, elk.database.data_base_suffix)) continue;
        var folder = try from.openDir(io, entry.name, .{ .iterate = true });
        defer folder.close(io);
        var walker = try folder.walk(gpa);
        defer walker.deinit();
        while (try walker.next(io)) |file| {
            if (file.kind != .file) continue;
            const relative = try std.fs.path.join(gpa, &.{ entry.name, file.path });
            defer gpa.free(relative);
            if (std.fs.path.dirname(relative)) |parent| try to.createDirPath(io, parent);
            try copyFile(gpa, io, from, to, relative);
        }
    }
}

fn copyFile(gpa: std.mem.Allocator, io: std.Io, from: std.Io.Dir, to: std.Io.Dir, relative: []const u8) !void {
    const bytes = try from.readFileAlloc(io, relative, gpa, .limited(16 << 20));
    defer gpa.free(bytes);
    try to.writeFile(io, .{ .sub_path = relative, .data = bytes });
}

fn envValue(gpa: std.mem.Allocator, environ: std.process.Environ, name: []const u8) ?[]u8 {
    return environ.getAlloc(gpa, name) catch null;
}

/// The game's user root, found as Platform/Paths.cpp finds it but without
/// the engine: `BK_USER_ROOT` or SDL's pref path on Windows, the XDG rule on
/// Linux and macOS. "" when it cannot be known.
fn userRoot(gpa: std.mem.Allocator, environ: std.process.Environ, buffer: []u8) []const u8 {
    if (builtin.os.tag == .windows) {
        if (envValue(gpa, environ, "BK_USER_ROOT")) |root| {
            defer gpa.free(root);
            if (root.len != 0) return std.fmt.bufPrint(buffer, "{s}{s}", .{ root, if (std.mem.endsWith(u8, root, "\\")) "" else "\\" }) catch "";
        }
        const preference = c.SDL_GetPrefPath("Nival", "Blitzkrieg") orelse return "";
        defer c.SDL_free(preference);
        return std.fmt.bufPrint(buffer, "{s}", .{std.mem.span(preference)}) catch "";
    }
    const xdg = envValue(gpa, environ, "XDG_DATA_HOME");
    defer if (xdg) |value| gpa.free(value);
    const home = envValue(gpa, environ, "HOME");
    defer if (home) |value| gpa.free(value);
    return settings_mod.posixUserRoot(buffer, xdg, home);
}
