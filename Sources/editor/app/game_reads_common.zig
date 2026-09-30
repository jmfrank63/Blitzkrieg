//! What --game-reads-it (main.zig) and --game-reads-it-m2 (game_reads_m2.zig)
//! share: starting the hidden host, opening the map through the core Editor
//! on the real bridge, the test copy's paths, launching the real Game and
//! waiting for it, and sweeping its screenshot dump. One copy, so a change to
//! how a test game is started or waited for is made once.
//!
//! Every failure prints `map-editor: <label> FAIL: <reason>` where `label` is
//! the calling scenario's own ("game reads it", "game reads it M2"), so the
//! two scenarios keep their own line prefixes.
const std = @import("std");
const sdl3 = @import("sdl3");
const core = @import("editor_core");
const host_mod = @import("host.zig");
const c_bridge = @import("c_bridge.zig");
const panels_logic = @import("panels_logic.zig");
const crt = @import("crt.zig");
const testlaunch = @import("testlaunch.zig");
const c = host_mod.c;

/// The hidden host, the real bridge on it and the core Editor on that. The
/// editor holds a pointer into `real`, so a Rig is initialised in place
/// (`open`) and never moved afterwards.
pub const Rig = struct {
    host: host_mod.Host = undefined,
    real: c_bridge.RealBridge = undefined,
    editor: core.editor.Editor = undefined,
    std_files: core.files.StdFiles = undefined,
    host_started: bool = false,
    editor_started: bool = false,

    /// Starts the host, applies -mod=, opens `map` and draws two settle
    /// frames (main.zig's smokeRun/check convention: the bridge needs a drawn
    /// frame before it can resolve a screen point against the camera).
    /// False after printing why; `deinit` is safe either way.
    pub fn open(self: *Rig, gpa: std.mem.Allocator, io: std.Io, label: []const u8, map: []const u8, mod_folder: ?[]const u8, mod_requested: bool) bool {
        // See crt.attachParentConsole's doc comment: a packaged (.windows
        // subsystem) MapEditor.exe run from a terminal still needs this
        // mode's PASS/FAIL line to be visible there.
        crt.attachParentConsole();
        self.host = host_mod.Host.start(.{ .title = "Map Editor", .hidden = true }) catch |err| {
            std.debug.print("map-editor: {s} FAIL: the host did not start ({s}: {s})\n", .{ label, @errorName(err), host_mod.failureReason() });
            return false;
        };
        self.host_started = true;
        self.real = c_bridge.RealBridge.init(self.host.session);
        if (applyModArg(&self.real, mod_folder, mod_requested)) |reason| {
            std.debug.print("map-editor: {s} FAIL: the mod would not load: {s}\n", .{ label, reason });
            return false;
        }
        self.editor = core.editor.Editor.init(gpa, self.real.bridge());
        self.editor_started = true;
        // Never exercised in these modes (D-01: a test copy goes through
        // saveCopy, never editor.save), but wired for the same reason every
        // other mode is: nothing here should depend on save silently no-op'ing.
        self.std_files = .{ .io = io, .dir = .cwd() };
        self.editor.files = self.std_files.files();
        var path_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
        const path = mapArgument(io, &path_buffer, map) orelse {
            std.debug.print("map-editor: {s} FAIL: the path {s} is too long\n", .{ label, map });
            return false;
        };
        self.editor.open(path) catch {
            std.debug.print("map-editor: {s} FAIL: {s} did not open: {s}\n", .{ label, map, self.editor.status() });
            return false;
        };
        return self.settle(label, 2);
    }

    /// `frames` empty frames, so the engine has drawn what was just edited.
    pub fn settle(self: *Rig, label: []const u8, frames: u32) bool {
        var frame: u32 = 0;
        while (frame < frames) : (frame += 1) {
            var event: sdl3.c.SDL_Event = undefined;
            while (sdl3.c.SDL_PollEvent(&event)) _ = self.host.handleEvent(&event);
            self.host.beginFrame();
            self.host.endFrame() catch |err| {
                std.debug.print("map-editor: {s} FAIL: frame {d}: {s}\n", .{ label, frame, @errorName(err) });
                return false;
            };
        }
        return true;
    }

    pub fn deinit(self: *Rig) void {
        if (self.editor_started) self.editor.deinit();
        if (self.host_started) self.host.stop();
        self.* = .{};
    }
};

/// Where a test copy is written and which Game plays it. The buffers live
/// here, so the slices stay valid as long as this struct does.
pub const TestPaths = struct {
    test_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined,
    game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined,
    test_path: []const u8 = "",
    game_path: []const u8 = "",

    pub fn resolve(self: *TestPaths, rig: *Rig, io: std.Io, label: []const u8) bool {
        self.test_path = rig.real.testMapPath(testlaunch.profile_name, null, testlaunch.map_file_name, &self.test_buffer) orelse {
            std.debug.print("map-editor: {s} FAIL: no test map path: {s}\n", .{ label, std.mem.span(c.BkEditorLastMessage(rig.host.session)) });
            return false;
        };
        self.game_path = testlaunch.gamePath(io, &self.game_buffer) catch |err| {
            std.debug.print("map-editor: {s} FAIL: no Game beside MapEditor: {s}\n", .{ label, @errorName(err) });
            return false;
        };
        return true;
    }
};

/// Saves the open map as the test copy the game will play (BkEditorSaveMap's
/// copy form: the document and the dirty flag stay as they were).
pub fn saveTestCopy(rig: *Rig, label: []const u8, what: []const u8, test_path: []const u8) bool {
    if (rig.real.saveCopy(test_path) != .ok) {
        std.debug.print("map-editor: {s} FAIL: the {s} would not save: {s}\n", .{ label, what, std.mem.span(c.BkEditorLastMessage(rig.host.session)) });
        return false;
    }
    return true;
}

/// Starts the game with `extra_env`, waits for it to exit (240 s at most) and
/// returns its log, which the caller frees. `what` names the run in the
/// failure lines ("game", "baseline game"). Null after printing why: it would
/// not start, it did not exit in time, it exited nonzero or by a signal, or
/// its log would not read.
pub fn runGame(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, label: []const u8, what: []const u8, game_path: []const u8, log_path: []const u8, extra_env: []const [2][]const u8) ?[]u8 {
    var running = testlaunch.start(gpa, io, environ, .{
        .game_path = game_path,
        .log_path = log_path,
        .extra_env = extra_env,
    }) catch |err| {
        std.debug.print("map-editor: {s} FAIL: the {s} would not start: {s}\n", .{ label, what, @errorName(err) });
        return null;
    };
    const exit = running.waitBlocking(io, 240_000) orelse {
        running.terminate(io);
        std.debug.print("map-editor: {s} FAIL: the {s} did not exit within 240 s; its log: {s}\n", .{ label, what, log_path });
        return null;
    };
    if ((exit.code orelse 1) != 0 or exit.signal != null) {
        std.debug.print("map-editor: {s} FAIL: the {s} exited code={?d} signal={?d}; its log: {s}\n", .{ label, what, exit.code, exit.signal, log_path });
        return null;
    }
    return std.Io.Dir.cwd().readFileAlloc(io, log_path, gpa, .limited(4 << 20)) catch |err| {
        std.debug.print("map-editor: {s} FAIL: the {s}'s log at {s} would not read: {s}\n", .{ label, what, log_path, @errorName(err) });
        return null;
    };
}

/// The game's own screenshot dump (BK_AUTO_UI's `shot` action), left in the
/// game's working directory - its installation, where testlaunch.start runs
/// it - rather than zig-out/local-test, swept up so a repeat run is not
/// mistaken for a stale leftover.
pub fn deleteAutoshots(io: std.Io, game_path: []const u8) void {
    const game_dir = std.fs.path.dirname(game_path) orelse return;
    var dir = std.Io.Dir.cwd().openDir(io, game_dir, .{ .iterate = true }) catch return;
    defer dir.close(io);
    var it = dir.iterate();
    while (it.next(io) catch null) |entry| {
        if (entry.kind != .file) continue;
        if (std.mem.startsWith(u8, entry.name, "autoshot_") and std.mem.endsWith(u8, entry.name, ".rgba"))
            dir.deleteFile(io, entry.name) catch {};
    }
}

/// Applies `-mod=`'s request, right after the host starts and before
/// anything reads the catalogue or opens a map. Returns null on success, or
/// the bridge's reason on a refusal/bad argument - `real.session`'s own
/// message, valid only until the next bridge call, so callers use it at
/// once (fatal/a FAIL line) rather than store it. A no-op, returning null,
/// when `-mod=` was never given at all.
pub fn applyModArg(real: *c_bridge.RealBridge, mod_folder: ?[]const u8, mod_requested: bool) ?[]const u8 {
    if (!mod_requested) return null;
    if (real.setMod(mod_folder) != .ok) return std.mem.span(c.BkEditorLastMessage(real.session));
    return null;
}

/// A map path from the command line in the engine's form. It arrives as the
/// person typed it or the shell expanded it - an absolute macOS path has
/// forward slashes - and the engine's file layer splits only on '\', so it
/// goes through the conversion the file dialogs' paths go through
/// (panels_logic.enginePath). Null when it does not fit the buffer.
///
/// A relative path is relative to the directory the editor was launched from
/// and is made absolute here (panels_logic.absoluteFromLaunchDir), so nothing
/// after it - the document path, Open Recent, recovery, the shipped-map check
/// - depends on the working directory. The installation itself never does:
/// the engine finds it from the executable (host.zig's Options.data_root).
pub fn mapArgument(io: std.Io, buffer: *[panels_logic.PathSlot.max_path]u8, typed: []const u8) ?[]const u8 {
    var cwd_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const cwd_len = std.process.currentPath(io, &cwd_buffer) catch return panels_logic.enginePath(buffer, typed, .open, .bzm);
    var absolute_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
    const absolute = panels_logic.absoluteFromLaunchDir(&absolute_buffer, cwd_buffer[0..cwd_len], typed) orelse return null;
    return panels_logic.enginePath(buffer, absolute, .open, .bzm);
}
