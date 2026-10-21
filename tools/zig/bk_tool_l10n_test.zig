//! The per-language half of the `bk-*` conventions (M002 S07, D062, R047): every built tool, in
//! every language, prints its help in that language behind the English `usage: <tool> ` synopsis,
//! the language order holds for a real process (not only for `bk_l10n.resolve`), and a table that
//! lacks a key shows the English text. `bk_tool_conventions_test.zig` runs the exit-code and
//! stream rules in every language; this file is about what the help says.
//!
//! Runs the same executables as that test (build.zig hands over the `bk_tools` options module).

const std = @import("std");
const bk_tools = @import("bk_tools");
const bk_l10n = @import("bk_l10n.zig");

const Tool = struct {
    name: []const u8,
    path: []const u8,
    /// A key whose text is in the help of this tool, to find the table's paragraph in the output.
    body_key: []const u8,
};

const tools = [_]Tool{
    .{ .name = "bk-pak", .path = bk_tools.bk_pak, .body_key = "pak.usage.body" },
    .{ .name = "bk-table", .path = bk_tools.bk_table, .body_key = "table.usage.body" },
    .{ .name = "bk-image", .path = bk_tools.bk_image, .body_key = "image.usage.body" },
    .{ .name = "bk-video", .path = bk_tools.bk_video, .body_key = "video.usage.body" },
    .{ .name = "bk-font", .path = bk_tools.bk_font, .body_key = "font.usage.intro" },
    .{ .name = "bk-map", .path = bk_tools.bk_map, .body_key = "map.usage.commands" },
    .{ .name = "bk-sprite", .path = bk_tools.bk_sprite, .body_key = "sprite.usage.compose" },
};

const tables = blk: {
    var out: [bk_l10n.codes.len][]const u8 = undefined;
    for (bk_l10n.codes, 0..) |code, i| out[i] = @embedFile("lang/" ++ code ++ ".lang");
    break :blk out;
};

const Result = struct {
    code: u8,
    stdout: []u8,
    stderr: []u8,

    fn deinit(result: Result, gpa: std.mem.Allocator) void {
        gpa.free(result.stdout);
        gpa.free(result.stderr);
    }
};

/// The variables the language order reads; every run starts without them.
const language_variables = [_][]const u8{ "BK_LANG", "BK_L10N_DIR", "BK_DEBUG_LOG", "LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE", "BK_OS_LOCALES" };

const Env = struct { name: []const u8, value: []const u8 };

/// Runs `exe` in `cwd` with the language variables cleared, the operating system pinned to name no
/// language (`BK_OS_LOCALES` empty: a Mac or Windows user's own language must not leak in), and then
/// `env` set, so the result depends on nothing of the machine's locale.
fn run(gpa: std.mem.Allocator, io: std.Io, exe: []const u8, cwd: std.Io.Dir, env: []const Env, args: []const []const u8) !Result {
    var argv = std.ArrayList([]const u8).empty;
    defer argv.deinit(gpa);
    try argv.append(gpa, exe);
    try argv.appendSlice(gpa, args);
    var environ = try std.testing.environ.createMap(gpa);
    defer environ.deinit();
    for (language_variables) |name| _ = environ.swapRemove(name);
    try environ.put("BK_OS_LOCALES", "");
    for (env) |entry| try environ.put(entry.name, entry.value);
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

fn exePath(gpa: std.mem.Allocator, io: std.Io, tool: Tool) ![:0]u8 {
    // The build hands over a path relative to the build root, the test's cwd.
    return std.Io.Dir.cwd().realPathFileAlloc(io, tool.path, gpa);
}

/// The synopsis: everything before the first blank line, which no language changes.
fn synopsisOf(help: []const u8) []const u8 {
    const end = std.mem.indexOf(u8, help, "\n\n") orelse help.len;
    return help[0..end];
}

fn tableBytes(code: []const u8) []const u8 {
    for (bk_l10n.codes, tables) |known, bytes| {
        if (std.mem.eql(u8, known, code)) return bytes;
    }
    unreachable;
}

fn tableValue(arena: std.mem.Allocator, code: []const u8, key: []const u8) ![]const u8 {
    for (bk_l10n.codes, tables) |known, bytes| {
        if (!std.mem.eql(u8, known, code)) continue;
        for (try bk_l10n.parse(arena, bytes, null)) |entry| {
            if (std.mem.eql(u8, entry.key, key)) return entry.value;
        }
    }
    return error.KeyNotInTable;
}

/// The Unicode block a language's own script lives in, where the language is not Latin.
fn scriptRange(code: []const u8) ?[2]u21 {
    if (std.mem.eql(u8, code, "ru")) return .{ 0x0400, 0x04FF };
    if (std.mem.eql(u8, code, "zh-Hans")) return .{ 0x4E00, 0x9FFF };
    if (std.mem.eql(u8, code, "ja")) return .{ 0x3040, 0x30FF };
    if (std.mem.eql(u8, code, "hi")) return .{ 0x0900, 0x097F };
    return null;
}

fn hasCodePointIn(text: []const u8, range: [2]u21) bool {
    var it = (std.unicode.Utf8View.init(text) catch return false).iterator();
    while (it.nextCodepoint()) |cp| {
        if (cp >= range[0] and cp <= range[1]) return true;
    }
    return false;
}

/// The first line where two texts differ, for a failure message.
fn firstDifference(a: []const u8, b: []const u8) []const u8 {
    var lines_a = std.mem.splitScalar(u8, a, '\n');
    var lines_b = std.mem.splitScalar(u8, b, '\n');
    while (lines_a.next()) |line_a| {
        const line_b = lines_b.next() orelse return line_a;
        if (!std.mem.eql(u8, line_a, line_b)) return line_a;
    }
    return lines_b.next() orelse "";
}

fn checkHelp(gpa: std.mem.Allocator, io: std.Io, tool: Tool, exe: []const u8, cwd: std.Io.Dir, english: []const u8, code: []const u8, via_option: bool) !void {
    var arena_state = std.heap.ArenaAllocator.init(gpa);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const result = if (via_option)
        try run(gpa, io, exe, cwd, &.{}, &.{ try std.fmt.allocPrint(arena, "--lang={s}", .{code}), "--help" })
    else
        try run(gpa, io, exe, cwd, &.{.{ .name = "BK_LANG", .value = code }}, &.{"--help"});
    defer result.deinit(gpa);
    const how: []const u8 = if (via_option) "--lang" else "BK_LANG";
    errdefer std.debug.print("{s} {s}={s}: help broke at: {s}\n", .{ tool.name, how, code, firstDifference(result.stdout, english) });

    try std.testing.expectEqual(@as(u8, 0), result.code);
    try std.testing.expectEqualStrings("", result.stderr);
    const prefix = try std.fmt.allocPrint(arena, "usage: {s} ", .{tool.name});
    try std.testing.expect(std.mem.startsWith(u8, result.stdout, prefix));
    // Every option name and <placeholder> of the English synopsis is there: the synopsis is the
    // same block, byte for byte.
    try std.testing.expect(std.mem.startsWith(u8, result.stdout, synopsisOf(english)));
    if (std.mem.eql(u8, code, "en")) {
        try std.testing.expectEqualStrings(english, result.stdout);
        return;
    }
    try std.testing.expect(!std.mem.eql(u8, english, result.stdout));
    try std.testing.expect(std.mem.indexOf(u8, result.stdout, try tableValue(arena, code, tool.body_key)) != null);
    if (scriptRange(code)) |range| try std.testing.expect(hasCodePointIn(result.stdout, range));
}

test "every tool prints its help in every language, by --lang and by BK_LANG" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    for (tools) |tool| {
        const exe = try exePath(gpa, io, tool);
        defer gpa.free(exe);
        const english = try run(gpa, io, exe, tmp.dir, &.{}, &.{ "--lang=en", "--help" });
        defer english.deinit(gpa);
        try std.testing.expectEqual(@as(u8, 0), english.code);
        for (bk_l10n.codes) |code| {
            for ([_]bool{ true, false }) |via_option| {
                try checkHelp(gpa, io, tool, exe, tmp.dir, english.stdout, code, via_option);
            }
        }
    }
}

const pak = tools[0];

/// bk-pak's help in `code`, the reference the precedence cases compare with.
fn pakHelp(gpa: std.mem.Allocator, io: std.Io, exe: []const u8, cwd: std.Io.Dir, code: []const u8) ![]u8 {
    var arg_buffer: [32]u8 = undefined;
    const arg = try std.fmt.bufPrint(&arg_buffer, "--lang={s}", .{code});
    const result = try run(gpa, io, exe, cwd, &.{}, &.{ arg, "--help" });
    defer gpa.free(result.stderr);
    errdefer gpa.free(result.stdout);
    try std.testing.expectEqual(@as(u8, 0), result.code);
    return result.stdout;
}

const Case = struct {
    /// What the process is given, and the language its help must be in.
    args: []const []const u8,
    env: []const Env,
    expect: []const u8,
};

test "the language order holds for a real process" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const exe = try exePath(gpa, io, pak);
    defer gpa.free(exe);

    const cases = [_]Case{
        .{ .args = &.{ "--lang=de", "--help" }, .env = &.{.{ .name = "BK_LANG", .value = "ja" }}, .expect = "de" },
        .{ .args = &.{ "--lang", "de", "--help" }, .env = &.{.{ .name = "BK_LANG", .value = "ja" }}, .expect = "de" },
        .{ .args = &.{"--help"}, .env = &.{ .{ .name = "BK_LANG", .value = "ja" }, .{ .name = "LANG", .value = "de_DE.UTF-8" } }, .expect = "ja" },
        .{ .args = &.{"--help"}, .env = &.{.{ .name = "LC_ALL", .value = "fr_FR.UTF-8" }}, .expect = "fr" },
        .{ .args = &.{"--help"}, .env = &.{ .{ .name = "LANG", .value = "C" }, .{ .name = "LANGUAGE", .value = "ru" } }, .expect = "ru" },
        .{ .args = &.{"--help"}, .env = &.{.{ .name = "LANG", .value = "xx_YY" }}, .expect = "en" },
        // The terminal cannot show UTF-8, so the machine's locale gets English (the non-UTF-8 policy).
        .{ .args = &.{"--help"}, .env = &.{.{ .name = "LANG", .value = "ru_RU.KOI8-R" }}, .expect = "en" },
        .{ .args = &.{"--help"}, .env = &.{}, .expect = "en" },
        // The operating system comes after the environment variables and before English.
        .{ .args = &.{"--help"}, .env = &.{.{ .name = "BK_OS_LOCALES", .value = "fr" }}, .expect = "fr" },
        .{ .args = &.{"--help"}, .env = &.{.{ .name = "BK_OS_LOCALES", .value = "xx,ru-RU,en-GB" }}, .expect = "ru" },
        .{ .args = &.{"--help"}, .env = &.{ .{ .name = "BK_OS_LOCALES", .value = "fr" }, .{ .name = "LANG", .value = "de_DE.UTF-8" } }, .expect = "de" },
        .{ .args = &.{"--help"}, .env = &.{ .{ .name = "BK_OS_LOCALES", .value = "fr" }, .{ .name = "BK_LANG", .value = "ja" } }, .expect = "ja" },
        .{ .args = &.{ "--lang=de", "--help" }, .env = &.{.{ .name = "BK_OS_LOCALES", .value = "fr" }}, .expect = "de" },
    };
    for (cases) |case| {
        const expected = try pakHelp(gpa, io, exe, tmp.dir, case.expect);
        defer gpa.free(expected);
        const result = try run(gpa, io, exe, tmp.dir, case.env, case.args);
        defer result.deinit(gpa);
        std.testing.expectEqualStrings(expected, result.stdout) catch |err| {
            std.debug.print("expected {s} for {any} with {any}\n", .{ case.expect, case.args, case.env });
            return err;
        };
        try std.testing.expectEqualStrings("", result.stderr);
    }
}

test "a language that is not a table code, or no code at all, exits 2 with an English message and the usage" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const exe = try exePath(gpa, io, pak);
    defer gpa.free(exe);
    const english = try pakHelp(gpa, io, exe, tmp.dir, "en");
    defer gpa.free(english);

    const bad = [_]struct { args: []const []const u8, first_line: []const u8 }{
        .{ .args = &.{ "--lang=xx", "list", "a.pak" }, .first_line = "bk-pak: unknown language 'xx'; use one of: " },
        .{ .args = &.{ "--lang", "xx", "--help" }, .first_line = "bk-pak: unknown language 'xx'; use one of: " },
        .{ .args = &.{ "list", "a.pak", "--lang" }, .first_line = "bk-pak: --lang needs a language code; use one of: " },
    };
    for (bad) |case| {
        // A language in the environment must not change the message: nothing is chosen yet.
        const result = try run(gpa, io, exe, tmp.dir, &.{.{ .name = "BK_LANG", .value = "de" }}, case.args);
        defer result.deinit(gpa);
        try std.testing.expectEqual(@as(u8, 2), result.code);
        try std.testing.expectEqualStrings("", result.stdout);
        try std.testing.expect(std.mem.startsWith(u8, result.stderr, case.first_line));
        const line_end = std.mem.indexOfScalar(u8, result.stderr, '\n').?;
        try std.testing.expect(std.mem.endsWith(u8, result.stderr[0..line_end], bk_l10n.valid_codes));
        try std.testing.expectEqualStrings(english, result.stderr[line_end + 1 ..]);
    }
}

test "a table without a key shows the English paragraph, and says so only with BK_DEBUG_LOG" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var arena_state = std.heap.ArenaAllocator.init(gpa);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const dropped = "pak.usage.note";
    var kept = std.ArrayList(u8).empty;
    var lines = std.mem.splitScalar(u8, tableBytes("de"), '\n');
    var removed: usize = 0;
    while (lines.next()) |line| {
        if (std.mem.startsWith(u8, line, dropped ++ "=")) {
            removed += 1;
            continue;
        }
        try kept.appendSlice(arena, line);
        if (lines.peek() != null) try kept.append(arena, '\n');
    }
    try std.testing.expectEqual(@as(usize, 1), removed);

    // Under zig-out/local-test, never beside the shipped tables.
    const override = "zig-out/local-test/bk-tools-l10n/dropped-key";
    try std.Io.Dir.cwd().createDirPath(io, override);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = override ++ "/de.lang", .data = kept.items });
    const override_abs = try std.Io.Dir.cwd().realPathFileAlloc(io, override, arena);

    const exe = try exePath(gpa, io, pak);
    defer gpa.free(exe);
    const german = try pakHelp(gpa, io, exe, tmp.dir, "de");
    defer gpa.free(german);
    const english_note = try tableValue(arena, "en", dropped);
    const german_note = try tableValue(arena, "de", dropped);

    {
        const result = try run(gpa, io, exe, tmp.dir, &.{.{ .name = "BK_L10N_DIR", .value = override_abs }}, &.{ "--lang=de", "--help" });
        defer result.deinit(gpa);
        try std.testing.expectEqual(@as(u8, 0), result.code);
        try std.testing.expectEqualStrings("", result.stderr);
        try std.testing.expect(std.mem.indexOf(u8, result.stdout, english_note) != null);
        try std.testing.expect(std.mem.indexOf(u8, result.stdout, german_note) == null);
        // The rest is still German.
        try std.testing.expect(std.mem.indexOf(u8, result.stdout, try tableValue(arena, "de", "pak.usage.body")) != null);
    }
    {
        const result = try run(gpa, io, exe, tmp.dir, &.{ .{ .name = "BK_L10N_DIR", .value = override_abs }, .{ .name = "BK_DEBUG_LOG", .value = "1" } }, &.{ "--lang=de", "--help" });
        defer result.deinit(gpa);
        try std.testing.expectEqual(@as(u8, 0), result.code);
        try std.testing.expect(std.mem.indexOf(u8, result.stderr, "l10n: missing de " ++ dropped) != null);
        try std.testing.expect(std.mem.indexOf(u8, result.stdout, english_note) != null);
    }
    {
        // The override is only a test seam: without it the embedded German table is whole.
        const result = try run(gpa, io, exe, tmp.dir, &.{.{ .name = "BK_DEBUG_LOG", .value = "1" }}, &.{ "--lang=de", "--help" });
        defer result.deinit(gpa);
        try std.testing.expectEqualStrings("", result.stderr);
        try std.testing.expectEqualStrings(german, result.stdout);
    }
}
