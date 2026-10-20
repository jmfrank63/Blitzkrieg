//! `bk-video`: `convert_bik_to_ogv.ps1` as a portable tool (Phase 8, D-22),
//! so a modder on a machine without PowerShell can turn Bink `.bik` movies
//! into the Ogg Theora `.ogv` the game plays. std-only Zig (D-03) and a thin
//! wrapper: the codecs are `ffmpeg`'s, found on `PATH` or named with
//! `--ffmpeg`/`--ffprobe`, and none is built in.
//!
//! The script's flags, output naming (`<name>.ogv` beside the `.bik`, and the
//! first audio track again as the `<name>.audio.ogg` sidecar), ffmpeg
//! arguments, skip rules, job lines and summary line are kept as they are.
//! Three differences, all for the bk conventions (`bk_tool.zig`):
//! - Paths are printed as built from `--input-root` (host separators), not
//!   made absolute, and the `.bik` files are taken in sorted path order.
//! - ffmpeg's own output is captured; a failed run reports its last line on
//!   stderr as `bk-video: ...`, and the batch goes on and exits 1 at the end,
//!   as the script did. A successful run leaves stderr empty.
//! - A folder without `.bik` files succeeds before ffmpeg is looked for,
//!   since there is nothing to run it on.

const std = @import("std");
const bk_tool = @import("bk_tool.zig");

const usage: bk_tool.Usage = .{
    .synopsis =
    \\usage: bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
    \\                        [--ffmpeg <path>] [--ffprobe <path>]
    \\                        [--key-frame-interval <n>] [--audio-suffix <suffix>]
    \\
    ,
    .body = &.{"video.usage.body"},
};

pub fn main(init: std.process.Init) u8 {
    return bk_tool.main(init, "bk-video", usage, run);
}

pub const Options = struct {
    input_root: []const u8 = "Data/movies",
    ffmpeg: []const u8 = "ffmpeg",
    ffprobe: []const u8 = "ffprobe",
    key_frame_interval: u32 = 1,
    audio_suffix: []const u8 = ".audio",
    force: bool = false,
    recurse: bool = true,
    what_if: bool = false,
};

fn run(ctx: *bk_tool.Context) !void {
    if (ctx.args.len == 0 or !std.mem.eql(u8, ctx.args[0], "convert")) return error.Usage;
    var options: Options = .{};
    var i: usize = 1;
    while (i < ctx.args.len) : (i += 1) {
        const arg = ctx.args[i];
        if (std.mem.eql(u8, arg, "--force")) {
            options.force = true;
        } else if (std.mem.eql(u8, arg, "--no-recurse")) {
            options.recurse = false;
        } else if (std.mem.eql(u8, arg, "--what-if")) {
            options.what_if = true;
        } else {
            const value_flags = [_][]const u8{ "--input-root", "--ffmpeg", "--ffprobe", "--key-frame-interval", "--audio-suffix" };
            const flag = for (value_flags) |name| {
                if (std.mem.eql(u8, arg, name)) break name;
            } else return error.Usage;
            if (i + 1 == ctx.args.len) return error.Usage;
            i += 1;
            const value = ctx.args[i];
            if (std.mem.eql(u8, flag, "--input-root")) {
                options.input_root = value;
            } else if (std.mem.eql(u8, flag, "--ffmpeg")) {
                options.ffmpeg = value;
            } else if (std.mem.eql(u8, flag, "--ffprobe")) {
                options.ffprobe = value;
            } else if (std.mem.eql(u8, flag, "--key-frame-interval")) {
                options.key_frame_interval = std.fmt.parseInt(u32, value, 10) catch return error.Usage;
            } else {
                options.audio_suffix = value;
            }
        }
    }
    try convert(ctx, options);
}

fn convert(ctx: *bk_tool.Context, options: Options) !void {
    var arena_state: std.heap.ArenaAllocator = .init(ctx.gpa);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = ctx.io;
    const cwd = std.Io.Dir.cwd();
    const root = try bk_tool.inputPath(arena, options.input_root);
    const ffmpeg = try bk_tool.inputPath(arena, options.ffmpeg);
    const ffprobe = try bk_tool.inputPath(arena, options.ffprobe);

    var names: std.ArrayList([]const u8) = .empty;
    {
        var dir = cwd.openDir(io, root, .{ .iterate = true }) catch |err| switch (err) {
            error.FileNotFound, error.NotDir => return ctx.fail("input root does not exist: {s}", .{root}),
            else => return ctx.fail("{s}: {s}", .{ root, @errorName(err) }),
        };
        defer dir.close(io);
        if (options.recurse) {
            var walker = try dir.walk(ctx.gpa);
            defer walker.deinit();
            while (try walker.next(io)) |entry| {
                if (entry.kind == .file and isBik(entry.basename)) try names.append(arena, try arena.dupe(u8, entry.path));
            }
        } else {
            var iterator = dir.iterate();
            while (try iterator.next(io)) |entry| {
                if (entry.kind == .file and isBik(entry.name)) try names.append(arena, try arena.dupe(u8, entry.name));
            }
        }
    }
    std.mem.sort([]const u8, names.items, {}, lessThanBytes);
    if (names.items.len == 0) {
        try ctx.out.print("No .bik files found under {s}.\n", .{root});
        return;
    }
    try requireTool(ctx, ffmpeg, "ffmpeg", "--ffmpeg");
    try requireTool(ctx, ffprobe, "ffprobe", "--ffprobe");

    var converted: usize = 0;
    var skipped: usize = 0;
    var failed: usize = 0;
    var audio_converted: usize = 0;
    var audio_skipped: usize = 0;
    var key_frames_buffer: [10]u8 = undefined;
    const key_frames = std.fmt.bufPrint(&key_frames_buffer, "{d}", .{options.key_frame_interval}) catch unreachable;

    for (names.items) |name| {
        const bik = try std.fs.path.join(arena, &.{ root, name });
        const ogv = try outputPath(arena, bik, ".ogv");
        const audio = try outputPath(arena, bik, try std.mem.concat(arena, u8, &.{ options.audio_suffix, ".ogg" }));

        if (exists(io, ogv) and !options.force) {
            try ctx.out.print("Skip existing: {s}\n", .{ogv});
            skipped += 1;
        } else if (options.what_if) {
            try ctx.out.print("Would convert: {s} -> {s}\n", .{ bik, ogv });
        } else {
            try ctx.out.print("Converting: {s} -> {s}\n", .{ bik, ogv });
            try ctx.out.flush();
            const argv = [_][]const u8{
                ffmpeg,         "-hide_banner", "-y",        "-i",   bik,
                "-map",         "0:v:0",        "-map",      "0:a?", "-c:v",
                "libtheora",    "-q:v",         "7",         "-g",   key_frames,
                "-c:a",         "libvorbis",    "-q:a",      "4",    ogv,
            };
            if (try runFfmpeg(ctx, &argv, "ffmpeg failed", bik)) converted += 1 else failed += 1;
        }

        if (!try hasAudioStream(ctx, ffprobe, bik)) continue;
        if (exists(io, audio) and !options.force) {
            try ctx.out.print("Skip existing audio: {s}\n", .{audio});
            audio_skipped += 1;
            continue;
        }
        if (options.what_if) {
            try ctx.out.print("Would extract audio: {s} -> {s}\n", .{ bik, audio });
            continue;
        }
        try ctx.out.print("Extracting audio: {s} -> {s}\n", .{ bik, audio });
        try ctx.out.flush();
        const argv = [_][]const u8{
            ffmpeg, "-hide_banner", "-y",        "-i",   bik,  "-map", "0:a:0",
            "-vn",  "-c:a",         "libvorbis", "-q:a", "4",  audio,
        };
        if (try runFfmpeg(ctx, &argv, "ffmpeg audio extraction failed", bik)) audio_converted += 1 else failed += 1;
    }

    try ctx.out.print("BIK to OGV conversion finished. Converted={d} Skipped={d} AudioConverted={d} AudioSkipped={d} Failed={d}\n", .{ converted, skipped, audio_converted, audio_skipped, failed });
    if (failed != 0) return error.Failed;
}

/// `.bik` in any case, as PowerShell's `-Filter *.bik` matched it on Windows.
pub fn isBik(name: []const u8) bool {
    return name.len > 4 and std.ascii.endsWithIgnoreCase(name, ".bik");
}

/// `<bik without its extension><suffix>`, the script's
/// `ChangeExtension(.ogv)` and `<name><AudioSuffix>.ogg`.
pub fn outputPath(gpa: std.mem.Allocator, bik: []const u8, suffix: []const u8) ![]u8 {
    return std.mem.concat(gpa, u8, &.{ bik[0 .. bik.len - ".bik".len], suffix });
}

fn lessThanBytes(_: void, a: []const u8, b: []const u8) bool {
    return std.mem.lessThan(u8, a, b);
}

fn exists(io: std.Io, path: []const u8) bool {
    std.Io.Dir.cwd().access(io, path, .{}) catch return false;
    return true;
}

/// Starts `<exe> -version` once, so a missing ffmpeg is one clear line before
/// any job instead of a failure per file.
fn requireTool(ctx: *bk_tool.Context, exe: []const u8, comptime name: []const u8, comptime flag: []const u8) !void {
    const result = std.process.run(ctx.gpa, ctx.io, .{
        .argv = &.{ exe, "-version" },
        .stdout_limit = .limited(1024 * 1024),
        .stderr_limit = .limited(1024 * 1024),
    }) catch |err| switch (err) {
        error.FileNotFound => return ctx.fail(name ++ " was not found ({s}). Install ffmpeg or pass " ++ flag ++ " <path-to-" ++ name ++ ">.", .{exe}),
        else => return ctx.fail(name ++ " could not be started ({s}): {s}", .{ exe, @errorName(err) }),
    };
    ctx.gpa.free(result.stdout);
    ctx.gpa.free(result.stderr);
}

fn hasAudioStream(ctx: *bk_tool.Context, ffprobe: []const u8, bik: []const u8) !bool {
    const result = try std.process.run(ctx.gpa, ctx.io, .{
        .argv = &.{ ffprobe, "-v", "error", "-select_streams", "a:0", "-show_entries", "stream=index", "-of", "csv=p=0", bik },
        .stdout_limit = .limited(1024 * 1024),
        .stderr_limit = .limited(1024 * 1024),
    });
    defer ctx.gpa.free(result.stdout);
    defer ctx.gpa.free(result.stderr);
    var lines = std.mem.splitScalar(u8, result.stdout, '\n');
    return std.mem.trim(u8, lines.first(), " \t\r").len != 0;
}

/// Runs one ffmpeg job with its output captured; on failure reports the
/// script's warning plus ffmpeg's last line, and returns false so the batch
/// goes on.
fn runFfmpeg(ctx: *bk_tool.Context, argv: []const []const u8, comptime what: []const u8, bik: []const u8) !bool {
    // ffmpeg's progress lines grow with the movie's length; 64 MiB is far
    // beyond the longest shipped movie's.
    const result = try std.process.run(ctx.gpa, ctx.io, .{
        .argv = argv,
        .stdout_limit = .limited(64 * 1024 * 1024),
        .stderr_limit = .limited(64 * 1024 * 1024),
    });
    defer ctx.gpa.free(result.stdout);
    defer ctx.gpa.free(result.stderr);
    const code: ?u8 = switch (result.term) {
        .exited => |code| code,
        else => null,
    };
    if (code == @as(?u8, 0)) return true;
    const last = lastLine(result.stderr);
    if (code) |c| {
        ctx.err.print("bk-video: " ++ what ++ " for {s} with exit code {d}: {s}\n", .{ bik, c, last }) catch {};
    } else {
        ctx.err.print("bk-video: " ++ what ++ " for {s}, ffmpeg did not exit normally: {s}\n", .{ bik, last }) catch {};
    }
    return false;
}

/// The last non-empty line of ffmpeg's output, which names the error.
pub fn lastLine(text: []const u8) []const u8 {
    var lines = std.mem.splitBackwardsAny(u8, text, "\r\n");
    while (lines.next()) |line| {
        const trimmed = std.mem.trim(u8, line, " \t");
        if (trimmed.len != 0) return trimmed;
    }
    return "(no output)";
}

test "output names are the script's, whatever the case of .bik" {
    const gpa = std.testing.allocator;
    const ogv = try outputPath(gpa, "movies/Intro.BIK", ".ogv");
    defer gpa.free(ogv);
    try std.testing.expectEqualStrings("movies/Intro.ogv", ogv);
    const audio = try outputPath(gpa, "movies/intro.bik", ".audio.ogg");
    defer gpa.free(audio);
    try std.testing.expectEqualStrings("movies/intro.audio.ogg", audio);
    try std.testing.expect(isBik("a.Bik"));
    try std.testing.expect(!isBik(".bik"));
    try std.testing.expect(!isBik("a.bik.ogv"));
}

test "the last line of ffmpeg's output names the error" {
    try std.testing.expectEqualStrings("Conversion failed!", lastLine("frame=1\rframe=2\r\nError opening\nConversion failed!\r\n"));
    try std.testing.expectEqualStrings("(no output)", lastLine("\n \n"));
}
