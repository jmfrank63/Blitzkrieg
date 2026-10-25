//! The Achtung Panzer 2 mod's own save under lang-ru, on the real Game, where the mod is at hand:
//! its content is not ours to track, so the tier reads the mod and a save from the paths
//! -Dap2-mod and -Dap2-save name (the mod's folder, holding data/, and a .sav written with it) and
//! is skipped without them. The mod is linked, never copied or written, into a scratch mods folder
//! (BK_MODS_ROOT) beside a copy of the installed lang-ru; the save is copied into a scratch profile
//! whose language option is lang-ru.
//!
//! The Game loads the save from its command line and quits from the running mission. It must:
//! - load it to the end and stay Russian: no `language:` line after the load (a save holds the
//!   globals of its moment, and a save written in English switched the session back);
//! - draw the mod's sizes on bk_text after the load: the last medium font line is the fallback's
//!   (`reason mod font lacks the language's letters`), not the mod's bitmap font without Cyrillic;
//! - draw no character as a missing glyph, and leak nothing at exit. "bridgehead established" left
//!   1,477 blocks: 64 gun wrecks held by their own turrets and five gun crews held by their states
//!   (CUnitTurret::DetachAllOwners, CFormation::ReleaseAllStates).
//!
//! The log is kept in zig-out/local-test/ap2-save/game-reads-it. The user's data is never touched:
//! XDG_DATA_HOME, BK_USER_ROOT, BK_PROFILES_ROOT and BK_MODS_ROOT are the scratch folder.
//! BK_AP2_GAME names the installed Game executable; `zig build ap2-save-game-reads-it` sets it.
const std = @import("std");
const options = @import("ap2_options");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/ap2-save/game-reads-it";
const language = "lang-ru";
const profile = "Ap2Save";
const save_name = "ap2.sav";

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

test "Achtung Panzer 2's save loads in Russian, stays Russian on bk_text and leaks nothing at exit" {
    if (options.mod.len == 0 or options.save.len == 0) return error.SkipZigTest;
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_AP2_GAME")) orelse {
        std.debug.print("BK_AP2_GAME is not set: run zig build ap2-save-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    const mods = try std.fs.path.join(allocator, &.{ root, "mods" });
    const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
    try cwd.createDirPath(io, configs);
    try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" }), try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }));
    const language_data = try std.fs.path.join(allocator, &.{ mods, language, "data" });
    try cwd.createDirPath(io, language_data);
    for ([_][]const u8{ "mod.xml", "texts.pak" }) |file|
        try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "mods", language, "data", file }), try std.fs.path.join(allocator, &.{ language_data, file }));
    try cwd.symLink(io, options.mod, try std.fs.path.join(allocator, &.{ mods, "AchtungPanzer2" }), .{ .is_directory = true });
    const saves = try std.fs.path.join(allocator, &.{ user, "profiles", profile, "mods", "achtungpanzer2", "saves" });
    try cwd.createDirPath(io, saves);
    try copyFile(io, allocator, options.save, try std.fs.path.join(allocator, &.{ saves, save_name }));

    _ = try play(io, allocator, game, user, mods, root, "choose", &.{}, "30:set=GamePlay.Language=" ++ language ++ ",60:exit");
    const log = try play(io, allocator, game, user, mods, root, "load", &.{ "-mod=achtungpanzer2", "-" ++ save_name }, "t3000:exit");
    const end = std.mem.indexOf(u8, log, "LOADTRACE: CICLoad::Exec end") orelse {
        std.debug.print("the save did not load to its end; the Game's output is in {s}/load.log\n", .{root});
        return error.LoadFailed;
    };
    try expectLine(log[0..end], root, "language: \"" ++ language ++ "\" mounted");
    if (std.mem.indexOf(u8, log[end..], "language: \"") != null) {
        std.debug.print("the load switched the session's language; the Game's output is in {s}/load.log\n", .{root});
        return error.LanguageSwitchedByLoad;
    }
    const last_bitmap = std.mem.lastIndexOf(u8, log, "font bitmap fonts\\medium:");
    const last_fallback = std.mem.lastIndexOf(u8, log, "font ttf fonts\\medium:") orelse {
        std.debug.print("the medium size never drew on bk_text; the Game's output is in {s}/load.log\n", .{root});
        return error.FontEngine;
    };
    if (last_bitmap != null and last_bitmap.? > last_fallback) {
        std.debug.print("the medium size draws with the mod's bitmap font after the load; the Game's output is in {s}/load.log\n", .{root});
        return error.FontEngine;
    }
    try expectLine(log[last_fallback..], root, "reason mod font lacks the language's letters");
}

/// Runs the Game in the scratch folder; it must exit 0 without a leak or a missing glyph.
fn play(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, mods: []const u8, root: []const u8, name: []const u8, args: []const []const u8, schedule: []const u8) ![]u8 {
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ root, name });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MODS_ROOT", mods);
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_GLYPH_TRACE", "1");
    try environ.put("BK_NO_HELP", "1");
    try environ.put("BK_AUTO_UI", schedule);
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(allocator, &.{ game, "-windowed", "-profile=" ++ profile });
    try argv.appendSlice(allocator, args);
    const result = try timed_run.run(allocator, io, .{
        .argv = argv.items,
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(900), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    try l10n_measure.expectNoMissingGlyph(log, log_path);
    return log;
}

fn copyFile(io: std.Io, allocator: std.mem.Allocator, from: []const u8, to: []const u8) !void {
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, from, allocator, .limited(512 << 20));
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = to, .data = bytes });
}

fn expectLine(log: []const u8, root: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its output is in {s}/load.log\n", .{ line, root });
    return error.GameLineMissing;
}
