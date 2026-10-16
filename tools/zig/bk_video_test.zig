//! Tests for `bk-video`, through the built executable; the command-line
//! contract (exit codes, streams) is in `bk_tool_conventions_test.zig`.
//!
//! The movies are tracked Bink sources (`Sources/src/data/Movies`): `smoke1`
//! has video only, `transition/open` and `transition/close` have audio too.
//! The `--what-if` listing was taken from `convert_bik_to_ogv.ps1 -WhatIf` on
//! the same folder (an existing `close.ogv` included), with the script's
//! absolute paths written as the input root gives them. The tests that run
//! ffmpeg or ffprobe skip, saying so, where they are not on PATH; the missing
//! ffmpeg test needs neither.

const std = @import("std");
const bk_video = @import("bk_video.zig");
const bk_tools = @import("bk_tools");

const sep = std.fs.path.sep_str;
const movies = "Sources/src/data/Movies/";

const Result = struct {
    code: u8,
    stdout: []u8,
    stderr: []u8,

    fn deinit(result: Result, gpa: std.mem.Allocator) void {
        gpa.free(result.stdout);
        gpa.free(result.stderr);
    }
};

fn runIn(cwd: std.Io.Dir, argv: []const []const u8) !Result {
    const result = try std.process.run(std.testing.allocator, std.testing.io, .{
        .argv = argv,
        .cwd = .{ .dir = cwd },
        .stdout_limit = .limited(16 * 1024 * 1024),
        .stderr_limit = .limited(16 * 1024 * 1024),
    });
    const code: u8 = switch (result.term) {
        .exited => |code| code,
        else => 255,
    };
    return .{ .code = code, .stdout = result.stdout, .stderr = result.stderr };
}

fn runVideo(cwd: std.Io.Dir, args: []const []const u8) !Result {
    const gpa = std.testing.allocator;
    const exe = try std.Io.Dir.cwd().realPathFileAlloc(std.testing.io, bk_tools.bk_video, gpa);
    defer gpa.free(exe);
    var argv: std.ArrayList([]const u8) = .empty;
    defer argv.deinit(gpa);
    try argv.append(gpa, exe);
    try argv.appendSlice(gpa, args);
    return runIn(cwd, argv.items);
}

/// Skips the calling test, loudly, when ffmpeg or ffprobe is not on PATH.
fn requireFfmpeg() !void {
    for ([_][]const u8{ "ffmpeg", "ffprobe" }) |name| {
        const result = runIn(std.Io.Dir.cwd(), &.{ name, "-version" }) catch |err| {
            std.debug.print("test-bk-video: skipped: {s} not on PATH ({s})\n", .{ name, @errorName(err) });
            return error.SkipZigTest;
        };
        result.deinit(std.testing.allocator);
    }
}

fn copyMovie(dir: std.Io.Dir, from: []const u8, to: []const u8) !void {
    const io = std.testing.io;
    if (std.fs.path.dirname(to)) |parent| try dir.createDirPath(io, parent);
    try std.Io.Dir.copyFile(std.Io.Dir.cwd(), from, dir, to, io, .{});
}

fn fileExists(dir: std.Io.Dir, path: []const u8) bool {
    dir.access(std.testing.io, path, .{}) catch return false;
    return true;
}

/// ffprobe's answer for one entry of a file, as comma-joined lines.
fn probe(dir: std.Io.Dir, path: []const u8, args: []const []const u8) ![]u8 {
    const gpa = std.testing.allocator;
    var argv: std.ArrayList([]const u8) = .empty;
    defer argv.deinit(gpa);
    try argv.appendSlice(gpa, &.{ "ffprobe", "-v", "error" });
    try argv.appendSlice(gpa, args);
    try argv.appendSlice(gpa, &.{ "-of", "csv=p=0", path });
    const result = try runIn(dir, argv.items);
    defer gpa.free(result.stderr);
    try std.testing.expectEqual(@as(u8, 0), result.code);
    const text = std.mem.trimEnd(u8, result.stdout, "\r\n");
    std.mem.replaceScalar(u8, result.stdout[0..text.len], '\n', ',');
    const joined = try gpa.dupe(u8, std.mem.trimEnd(u8, text, ","));
    gpa.free(result.stdout);
    return joined;
}

fn expectProbe(dir: std.Io.Dir, path: []const u8, args: []const []const u8, expected: []const u8) !void {
    const got = try probe(dir, path, args);
    defer std.testing.allocator.free(got);
    std.testing.expectEqualStrings(expected, got) catch |err| {
        std.debug.print("ffprobe {s} {any}\n", .{ path, args });
        return err;
    };
}

test "a missing ffmpeg fails with one clear line before any job" {
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try copyMovie(tmp.dir, movies ++ "interface/smoke1.bik", "movies/smoke1.bik");
    const result = try runVideo(tmp.dir, &.{ "convert", "--input-root", "movies", "--ffmpeg", "bk-video-no-such-ffmpeg" });
    defer result.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(u8, 1), result.code);
    try std.testing.expectEqualStrings("", result.stdout);
    try std.testing.expectEqualStrings("bk-video: ffmpeg was not found (bk-video-no-such-ffmpeg). Install ffmpeg or pass --ffmpeg <path-to-ffmpeg>.\n", result.stderr);
    try std.testing.expect(!fileExists(tmp.dir, "movies/smoke1.ogv"));
}

test "--what-if lists the script's jobs and writes nothing" {
    try requireFfmpeg();
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try copyMovie(tmp.dir, movies ++ "interface/smoke1.bik", "movies/smoke1.bik");
    try copyMovie(tmp.dir, movies ++ "transition/open.bik", "movies/transition/open.bik");
    try copyMovie(tmp.dir, movies ++ "transition/close.bik", "movies/transition/close.bik");
    try tmp.dir.writeFile(std.testing.io, .{ .sub_path = "movies/transition/close.ogv", .data = "" });

    {
        // Backslashes on input, whatever the host.
        const result = try runVideo(tmp.dir, &.{ "convert", "--input-root", "movies\\", "--what-if" });
        defer result.deinit(gpa);
        try std.testing.expectEqualStrings("", result.stderr);
        try std.testing.expectEqual(@as(u8, 0), result.code);
        try std.testing.expectEqualStrings(
            "Would convert: movies" ++ sep ++ "smoke1.bik -> movies" ++ sep ++ "smoke1.ogv\n" ++
                "Skip existing: movies" ++ sep ++ "transition" ++ sep ++ "close.ogv\n" ++
                "Would extract audio: movies" ++ sep ++ "transition" ++ sep ++ "close.bik -> movies" ++ sep ++ "transition" ++ sep ++ "close.audio.ogg\n" ++
                "Would convert: movies" ++ sep ++ "transition" ++ sep ++ "open.bik -> movies" ++ sep ++ "transition" ++ sep ++ "open.ogv\n" ++
                "Would extract audio: movies" ++ sep ++ "transition" ++ sep ++ "open.bik -> movies" ++ sep ++ "transition" ++ sep ++ "open.audio.ogg\n" ++
                "BIK to OGV conversion finished. Converted=0 Skipped=1 AudioConverted=0 AudioSkipped=0 Failed=0\n",
            result.stdout,
        );
    }
    {
        const result = try runVideo(tmp.dir, &.{ "convert", "--input-root", "movies", "--what-if", "--no-recurse" });
        defer result.deinit(gpa);
        try std.testing.expectEqual(@as(u8, 0), result.code);
        try std.testing.expectEqualStrings(
            "Would convert: movies" ++ sep ++ "smoke1.bik -> movies" ++ sep ++ "smoke1.ogv\n" ++
                "BIK to OGV conversion finished. Converted=0 Skipped=0 AudioConverted=0 AudioSkipped=0 Failed=0\n",
            result.stdout,
        );
    }
    for ([_][]const u8{ "movies/smoke1.ogv", "movies/transition/open.ogv", "movies/transition/open.audio.ogg", "movies/transition/close.audio.ogg" }) |path| {
        try std.testing.expect(!fileExists(tmp.dir, path));
    }
}

test "convert writes Theora with Vorbis, every frame a key frame, and keeps what exists" {
    try requireFfmpeg();
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try copyMovie(tmp.dir, movies ++ "interface/smoke1.bik", "movies/smoke1.bik");
    try copyMovie(tmp.dir, movies ++ "transition/open.bik", "movies/transition/open.bik");

    {
        const result = try runVideo(tmp.dir, &.{ "convert", "--input-root", "movies" });
        defer result.deinit(gpa);
        std.testing.expectEqual(@as(u8, 0), result.code) catch |err| {
            std.debug.print("bk-video convert: {s}\n", .{result.stderr});
            return err;
        };
        try std.testing.expectEqualStrings("", result.stderr);
        try std.testing.expectEqualStrings(
            "Converting: movies" ++ sep ++ "smoke1.bik -> movies" ++ sep ++ "smoke1.ogv\n" ++
                "Converting: movies" ++ sep ++ "transition" ++ sep ++ "open.bik -> movies" ++ sep ++ "transition" ++ sep ++ "open.ogv\n" ++
                "Extracting audio: movies" ++ sep ++ "transition" ++ sep ++ "open.bik -> movies" ++ sep ++ "transition" ++ sep ++ "open.audio.ogg\n" ++
                "BIK to OGV conversion finished. Converted=2 Skipped=0 AudioConverted=1 AudioSkipped=0 Failed=0\n",
            result.stdout,
        );
    }
    const streams = [_][]const u8{ "-show_entries", "stream=codec_name" };
    try expectProbe(tmp.dir, "movies/smoke1.ogv", &streams, "theora");
    try expectProbe(tmp.dir, "movies/transition/open.ogv", &streams, "theora,vorbis");
    try expectProbe(tmp.dir, "movies/transition/open.audio.ogg", &streams, "vorbis");
    try std.testing.expect(!fileExists(tmp.dir, "movies/smoke1.audio.ogg"));
    // The script's key-frame setting, -g 1: every frame a key frame.
    {
        const keys = try probe(tmp.dir, "movies/smoke1.ogv", &.{ "-select_streams", "v:0", "-show_entries", "frame=key_frame" });
        defer gpa.free(keys);
        try std.testing.expect(keys.len > 1);
        var it = std.mem.splitScalar(u8, keys, ',');
        while (it.next()) |key| try std.testing.expectEqualStrings("1", key);
    }

    {
        const result = try runVideo(tmp.dir, &.{ "convert", "--input-root", "movies" });
        defer result.deinit(gpa);
        try std.testing.expectEqual(@as(u8, 0), result.code);
        try std.testing.expect(std.mem.endsWith(u8, result.stdout, "Converted=0 Skipped=2 AudioConverted=0 AudioSkipped=1 Failed=0\n"));
    }
    {
        const result = try runVideo(tmp.dir, &.{ "convert", "--input-root", "movies", "--force", "--no-recurse" });
        defer result.deinit(gpa);
        try std.testing.expectEqual(@as(u8, 0), result.code);
        try std.testing.expect(std.mem.endsWith(u8, result.stdout, "Converted=1 Skipped=0 AudioConverted=0 AudioSkipped=0 Failed=0\n"));
    }
}

test "a movie ffmpeg cannot read fails the batch, which still runs the rest" {
    try requireFfmpeg();
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try tmp.dir.createDirPath(std.testing.io, "movies");
    try tmp.dir.writeFile(std.testing.io, .{ .sub_path = "movies/broken.bik", .data = "not a bink movie" });
    try copyMovie(tmp.dir, movies ++ "interface/smoke1.bik", "movies/smoke1.bik");
    const result = try runVideo(tmp.dir, &.{ "convert", "--input-root", "movies" });
    defer result.deinit(gpa);
    try std.testing.expectEqual(@as(u8, 1), result.code);
    try std.testing.expect(std.mem.startsWith(u8, result.stderr, "bk-video: ffmpeg failed for movies" ++ sep ++ "broken.bik with exit code "));
    try std.testing.expectEqual(@as(usize, 1), std.mem.count(u8, result.stderr, "\n"));
    try std.testing.expect(std.mem.endsWith(u8, result.stdout, "Converted=1 Skipped=0 AudioConverted=0 AudioSkipped=0 Failed=1\n"));
    try std.testing.expect(fileExists(tmp.dir, "movies/smoke1.ogv"));
}

test {
    _ = bk_video;
}
