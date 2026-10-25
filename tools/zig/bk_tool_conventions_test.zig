//! The common test of the `bk-*` command-line conventions (Phase 8, D-04),
//! run against the built executables rather than their `run` functions, so it
//! checks what a modder's shell sees: exit codes and which stream says what.
//! The rules themselves are documented in `bk_tool.zig`.
//!
//! Each tool contributes one row to `tools`: arguments that are bad usage, an
//! operation that fails, and one that succeeds, all run in a fresh temporary
//! directory its `setup` fills. build.zig passes each executable's path in the
//! `bk_tools` options module; adding a tool is a row here and an option there.
//!
//! The rules hold in every language (M002 S07, D062): the test runs each tool once per table code,
//! chosen with `BK_LANG`, because the prefixes, exit codes and line counts are parsed by scripts.

const std = @import("std");
const bk_tools = @import("bk_tools");
const bk_l10n = @import("bk_l10n.zig");

const Tool = struct {
    name: []const u8,
    path: []const u8,
    /// Invocations that must exit 2 with the usage on stderr.
    bad_usage: []const []const []const u8,
    /// An invocation that must exit 1 with one `<name>: ` line on stderr.
    failing: []const []const u8,
    /// An invocation that must exit 0 and print a one-line summary last.
    succeeding: []const []const u8,
    /// Lines the success may print before its summary (a listing).
    success_lines: usize = 1,
    setup: *const fn (io: std.Io, dir: std.Io.Dir) anyerror!void,
};

const tools = [_]Tool{
    .{
        .name = "bk-pak",
        .path = bk_tools.bk_pak,
        .bad_usage = &.{
            &.{},
            &.{"frobnicate"},
            &.{"pack"},
            &.{ "list", "a.pak", "extra" },
            &.{ "verify", "a.pak", "--force" },
            &.{ "pack", "tree", "out.pak", "--level=9" },
        },
        .failing = &.{ "verify", "missing.pak" },
        // Backslashes on input, whatever the host.
        .succeeding = &.{ "pack", "mod\\tree", "mod\\out.pak" },
        .setup = setupPak,
    },
    .{
        .name = "bk-font",
        .path = bk_tools.bk_font,
        .bad_usage = &.{
            &.{},
            &.{"--frobnicate"},
            &.{"--ttf"},
            &.{ "--ttf", "font.ttf", "stray" },
            &.{ "--ttf", "font.ttf", "--height", "tall" },
            &.{ "--ttf", "font.ttf", "--charset", "klingon" },
        },
        .failing = &.{ "--ttf", "missing.ttf", "--out", "out" },
        .succeeding = &.{ "--ttf", "fonts\\ProggyClean.ttf", "--height", "13", "--out", "mod\\font" },
        .setup = setupFont,
    },
    .{
        .name = "bk-table",
        .path = bk_tools.bk_table,
        .bad_usage = &.{
            &.{},
            &.{"frobnicate"},
            &.{"export"},
            &.{ "export", "*.xml", "stats" },
            &.{ "export", "*.xml", "stats", "out.csv" },
            &.{ "export", "*.xml", "stats", "out.tsv", "--only" },
            &.{ "export", "*.xml", "stats", "out.tsv", "--only", "f.txt", "--ignore", "f.txt" },
            &.{"import"},
            &.{ "import", "sheet.csv" },
            &.{ "import", "sheet.tsv", "--no-recurse" },
        },
        .failing = &.{ "import", "missing.tsv" },
        .succeeding = &.{ "export", "*.xml", "mod\\stats", "mod\\out\\sheet.tsv" },
        .setup = setupTable,
    },
    .{
        .name = "bk-map",
        .path = bk_tools.bk_map,
        .bad_usage = &.{
            &.{},
            &.{"frobnicate"},
            &.{"to-bzm"},
            &.{ "to-bzm", "a.xml", "b.xml" },
            &.{ "to-xml", "a.bzm", "--size", "64" },
            &.{ "minimap", "a.bzm" },
            &.{ "minimap", "a.bzm", "--size", "0" },
            &.{ "obj2mod", "a.obj" },
            &.{ "validate-obj2mod", "a.obj", "--modules", "lib" },
        },
        .failing = &.{ "to-bzm", "missing.xml" },
        // Backslashes on input, and an output folder that is not there yet.
        .succeeding = &.{ "obj2mod", "mod\\tank.obj", "mod\\out\\tank.mod" },
        .setup = setupMap,
    },
    .{
        .name = "bk-sprite",
        .path = bk_tools.bk_sprite,
        .bad_usage = &.{
            &.{},
            &.{"frobnicate"},
            &.{"compose"},
            &.{ "compose", "desc" },
            &.{ "compose", "desc", "out", "extra" },
            &.{ "compose", "desc", "out", "--modules" },
            &.{ "compose", "desc", "out", "--data", "Data" },
            &.{ "compose-pack", "a.png", "b.png" },
            &.{ "compose-pack", "a.png", "b.png", "out" },
            &.{ "compose-pack", "a.png", "b.png", "out", "--zero", "1" },
            &.{ "compose-pack", "a.png", "b.png", "out", "--zero", "1,2", "--pass", "01" },
        },
        .failing = &.{ "compose", "missing", "out" },
        // Backslashes on input, and an output folder that is not there yet.
        .succeeding = &.{ "compose", "mod\\sprite", "mod\\out\\fixture" },
        .setup = setupSprite,
    },
    .{
        .name = "bk-image",
        .path = bk_tools.bk_image,
        .bad_usage = &.{
            &.{},
            &.{"frobnicate"},
            &.{"offset-romb"},
            &.{ "offset-romb", "a.tga", "b.tga" },
            &.{ "offset-romb", "a.tga", "--out" },
            &.{ "offset-romb", "a.tga", "--force" },
        },
        .failing = &.{ "offset-romb", "missing.tga" },
        .succeeding = &.{ "offset-romb", "mod\\tile.tga", "--out", "mod\\out\\tile.tga" },
        .setup = setupImage,
    },
    .{
        .name = "bk-views",
        .path = bk_tools.bk_views,
        .bad_usage = &.{
            &.{},
            &.{"frobnicate"},
            &.{"decompose"},
            &.{ "decompose", "house" },
            &.{ "decompose", "house", "out", "extra" },
            &.{ "decompose", "house", "--force" },
            &.{"camera"},
            &.{ "camera", "house", "--out" },
        },
        .failing = &.{ "decompose", "missing", "out" },
        // Backslashes on input, and an output folder that is not there yet.
        .succeeding = &.{ "decompose", "mod\\house", "mod\\out\\house" },
        .setup = setupViews,
    },
    .{
        .name = "bk-video",
        .path = bk_tools.bk_video,
        .bad_usage = &.{
            &.{},
            &.{"frobnicate"},
            &.{ "convert", "stray" },
            &.{ "convert", "--input-root" },
            &.{ "convert", "--key-frame-interval", "often" },
            &.{ "convert", "--out", "x" },
        },
        .failing = &.{ "convert", "--input-root", "missing" },
        // A folder without movies succeeds without ffmpeg, so the row runs
        // everywhere; bk_video_test.zig converts real ones.
        .succeeding = &.{ "convert", "--input-root", "mod\\movies" },
        .setup = setupVideo,
    },
};

/// The committed fixture font (D-15), copied from the build root, which is
/// the test's working directory.
fn setupFont(io: std.Io, dir: std.Io.Dir) !void {
    try dir.createDirPath(io, "fonts");
    try std.Io.Dir.copyFile(std.Io.Dir.cwd(), "tools/zig/fixtures/fonts/ProggyClean.ttf", dir, "fonts/ProggyClean.ttf", io, .{});
}

/// The sprite fixture's description and frames
/// (tools/zig/fixtures/small-tools/sprite).
fn setupSprite(io: std.Io, dir: std.Io.Dir) !void {
    for ([_][]const u8{ "anims.txt", "idle/000.tga", "idle/001.tga", "idle/002.tga", "death/000.tga", "death/001.tga" }) |name| {
        const from = try std.fmt.allocPrint(std.testing.allocator, "tools/zig/fixtures/small-tools/sprite/{s}", .{name});
        defer std.testing.allocator.free(from);
        const to = try std.fmt.allocPrint(std.testing.allocator, "mod/sprite/{s}", .{name});
        defer std.testing.allocator.free(to);
        try dir.createDirPath(io, std.fs.path.dirname(to).?);
        try std.Io.Dir.copyFile(std.Io.Dir.cwd(), from, dir, to, io, .{});
    }
}

/// The 16x16 offset-romb fixture tile.
fn setupImage(io: std.Io, dir: std.Io.Dir) !void {
    try dir.createDirPath(io, "mod");
    try std.Io.Dir.copyFile(std.Io.Dir.cwd(), "tools/zig/fixtures/small-tools/offset-romb/tile16.tga", dir, "mod/tile.tga", io, .{});
}

/// One damage state of a tracked building: its pack, atlas and stats.
fn setupViews(io: std.Io, dir: std.Io.Dir) !void {
    try dir.createDirPath(io, "mod/house");
    for ([_][]const u8{ "1.san", "1_h.dds", "1.xml" }) |name| {
        var from_buffer: [128]u8 = undefined;
        var to_buffer: [64]u8 = undefined;
        const from = try std.fmt.bufPrint(&from_buffer, "Data/Buildings/europe/summer/e_ctownhouse01_1/{s}", .{name});
        const to = try std.fmt.bufPrint(&to_buffer, "mod/house/{s}", .{name});
        try std.Io.Dir.copyFile(std.Io.Dir.cwd(), from, dir, to, io, .{});
    }
}

/// An empty movie folder.
fn setupVideo(io: std.Io, dir: std.Io.Dir) !void {
    try dir.createDirPath(io, "mod/movies");
}

/// The obj2mod fixture's OBJ (tools/zig/fixtures/small-tools/obj2mod).
fn setupMap(io: std.Io, dir: std.Io.Dir) !void {
    try dir.createDirPath(io, "mod");
    try std.Io.Dir.copyFile(std.Io.Dir.cwd(), "tools/zig/fixtures/small-tools/obj2mod/tank.obj", dir, "mod/tank.obj", io, .{});
}

fn setupTable(io: std.Io, dir: std.Io.Dir) !void {
    try dir.createDirPath(io, "mod/stats");
    try dir.writeFile(io, .{ .sub_path = "mod/stats/1.xml", .data = "<base><RPG HP=\"1\"/></base>\n" });
}

fn setupPak(io: std.Io, dir: std.Io.Dir) !void {
    try dir.createDirPath(io, "mod/tree/units");
    var file = try dir.createFile(io, "mod/tree/units/1.xml", .{});
    defer file.close(io);
    try file.writeStreamingAll(io, "<RPG/>\n");
}

const Result = struct {
    code: u8,
    stdout: []u8,
    stderr: []u8,

    fn deinit(result: Result, gpa: std.mem.Allocator) void {
        gpa.free(result.stdout);
        gpa.free(result.stderr);
    }
};

fn runTool(gpa: std.mem.Allocator, io: std.Io, exe: []const u8, lang: []const u8, cwd: std.Io.Dir, args: []const []const u8) !Result {
    var argv = std.ArrayList([]const u8).empty;
    defer argv.deinit(gpa);
    try argv.append(gpa, exe);
    try argv.appendSlice(gpa, args);
    // The language is set with BK_LANG: the tools would otherwise follow the machine's locale, and
    // the run must not depend on the machine it happens on.
    var environ = try std.testing.environ.createMap(gpa);
    defer environ.deinit();
    try environ.put("BK_LANG", lang);
    try environ.put("BK_OS_LOCALES", "");
    const result = try std.process.run(gpa, io, .{
        .argv = argv.items,
        .cwd = .{ .dir = cwd },
        .environ_map = &environ,
        .stdout_limit = .limited(1024 * 1024),
        .stderr_limit = .limited(1024 * 1024),
    });
    const code: u8 = switch (result.term) {
        .exited => |code| code,
        else => 255,
    };
    return .{ .code = code, .stdout = result.stdout, .stderr = result.stderr };
}

fn lineCount(text: []const u8) usize {
    return std.mem.count(u8, text, "\n");
}

fn checkTool(tool: Tool, lang: []const u8) !void {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try tool.setup(io, tmp.dir);
    // The build hands over a path relative to the build root (the test's cwd),
    // and the tool runs inside the temporary directory.
    const exe = try std.Io.Dir.cwd().realPathFileAlloc(io, tool.path, gpa);
    defer gpa.free(exe);
    const usage_prefix = try std.fmt.allocPrint(gpa, "usage: {s} ", .{tool.name});
    defer gpa.free(usage_prefix);

    for ([_][]const []const u8{ &.{"--help"}, &.{"-h"}, &.{ tool.succeeding[0], "--help" } }) |args| {
        const result = try runTool(gpa, io, exe, lang, tmp.dir, args);
        defer result.deinit(gpa);
        try std.testing.expectEqual(@as(u8, 0), result.code);
        try std.testing.expect(std.mem.startsWith(u8, result.stdout, usage_prefix));
        try std.testing.expectEqualStrings("", result.stderr);
    }

    for (tool.bad_usage) |args| {
        const result = try runTool(gpa, io, exe, lang, tmp.dir, args);
        defer result.deinit(gpa);
        std.testing.expectEqual(@as(u8, 2), result.code) catch |err| {
            std.debug.print("{s}: bad usage {any} exited {d}\n", .{ tool.name, args, result.code });
            return err;
        };
        try std.testing.expectEqualStrings("", result.stdout);
        try std.testing.expect(std.mem.startsWith(u8, result.stderr, usage_prefix));
    }

    {
        const result = try runTool(gpa, io, exe, lang, tmp.dir, tool.failing);
        defer result.deinit(gpa);
        try std.testing.expectEqual(@as(u8, 1), result.code);
        try std.testing.expectEqualStrings("", result.stdout);
        try std.testing.expect(std.mem.startsWith(u8, result.stderr, tool.name));
        try std.testing.expect(std.mem.startsWith(u8, result.stderr[tool.name.len..], ": "));
        try std.testing.expectEqual(@as(usize, 1), lineCount(result.stderr));
    }

    {
        const result = try runTool(gpa, io, exe, lang, tmp.dir, tool.succeeding);
        defer result.deinit(gpa);
        std.testing.expectEqual(@as(u8, 0), result.code) catch |err| {
            std.debug.print("{s}: success case failed: {s}\n", .{ tool.name, result.stderr });
            return err;
        };
        try std.testing.expectEqualStrings("", result.stderr);
        try std.testing.expectEqual(tool.success_lines, lineCount(result.stdout));
        // A path the tool prints uses the host's separator.
        const other_sep: u8 = if (std.fs.path.sep == '/') '\\' else '/';
        try std.testing.expect(std.mem.indexOfScalar(u8, result.stdout, other_sep) == null);
    }
}

test "every bk tool follows the shared command-line conventions in every language" {
    for (bk_l10n.codes) |lang| {
        for (tools) |tool| {
            checkTool(tool, lang) catch |err| {
                std.debug.print("convention broken by {s} in {s}\n", .{ tool.name, lang });
                return err;
            };
        }
    }
}
