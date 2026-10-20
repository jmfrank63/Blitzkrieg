//! Tests for the bk-* tools' string-table runtime (M002 S07): the language order, the locale forms,
//! the fallback to English, `{n}` formatting, the `--lang` option, the Windows language ids, and a
//! conformance fixture that the editor kit's parser must read the same way.
const std = @import("std");
const l10n = @import("bk_l10n.zig");

const testing = std.testing;

test "the order is --lang, BK_LANG, LC_ALL, LC_MESSAGES, LANG, LANGUAGE, the OS, then English" {
    const os = [_][]const u8{"ja-JP"};
    const all = l10n.EnvView{ .bk_lang = "fr", .lc_all = "de_DE.UTF-8", .lc_messages = "es_ES.UTF-8", .lang = "id_ID.UTF-8", .language = "hi" };
    try testing.expectEqualStrings("ru", l10n.resolve("ru", all, &os).code);
    try testing.expectEqual(l10n.Source.lang_arg, l10n.resolve("ru", all, &os).source);
    try testing.expectEqualStrings("fr", l10n.resolve(null, all, &os).code);
    try testing.expectEqual(l10n.Source.bk_lang, l10n.resolve(null, all, &os).source);

    var rest = all;
    rest.bk_lang = null;
    try testing.expectEqualStrings("de", l10n.resolve(null, rest, &os).code);
    rest.lc_all = null;
    try testing.expectEqualStrings("es", l10n.resolve(null, rest, &os).code);
    rest.lc_messages = null;
    try testing.expectEqualStrings("id", l10n.resolve(null, rest, &os).code);
    rest.lang = null;
    try testing.expectEqualStrings("hi", l10n.resolve(null, rest, &os).code);
    rest.language = null;
    const from_os = l10n.resolve(null, rest, &os);
    try testing.expectEqualStrings("ja", from_os.code);
    try testing.expectEqual(l10n.Source.os_locale, from_os.source);
    const none = l10n.resolve(null, rest, &.{});
    try testing.expectEqualStrings("en", none.code);
    try testing.expectEqual(l10n.Source.default, none.source);
}

test "C, POSIX and empty values count as unset, and an unknown language falls through" {
    try testing.expectEqualStrings("de", l10n.resolve(null, .{ .lc_all = "C", .lc_messages = "POSIX", .lang = "", .language = "de" }, &.{}).code);
    try testing.expectEqualStrings("fr", l10n.resolve(null, .{ .lc_all = "C.UTF-8", .lang = "fr_FR.UTF-8" }, &.{}).code);
    try testing.expectEqualStrings("de", l10n.resolve(null, .{ .lang = "ko_KR.UTF-8", .language = "xx:zh_TW:de:fr" }, &.{}).code);
    try testing.expectEqualStrings("fr", l10n.resolve("klingon", .{ .bk_lang = "also-wrong", .lang = "fr_FR" }, &.{}).code);
}

test "a locale on a charset other than UTF-8 gets English, an explicit choice does not" {
    const latin1 = l10n.resolve(null, .{ .lang = "de_DE.ISO-8859-1" }, &.{});
    try testing.expectEqualStrings("en", latin1.code);
    try testing.expectEqual(l10n.Source.charset, latin1.source);
    try testing.expectEqualStrings("de", l10n.resolve(null, .{ .lang = "de_DE.utf8" }, &.{}).code);
    try testing.expectEqualStrings("de", l10n.resolve(null, .{ .lang = "de_DE" }, &.{}).code);
    try testing.expectEqualStrings("de", l10n.resolve(null, .{ .lang = "de_DE.UTF-8@euro" }, &.{}).code);
    try testing.expectEqualStrings("ja", l10n.resolve("ja", .{ .lang = "de_DE.ISO-8859-1" }, &.{}).code);
    try testing.expectEqualStrings("ja", l10n.resolve(null, .{ .bk_lang = "ja", .lang = "de_DE.ISO-8859-1" }, &.{}).code);
}

test "localeCode maps operating system locales to table codes" {
    try testing.expectEqualStrings("zh-Hans", l10n.localeCode("zh_CN").?);
    try testing.expectEqualStrings("zh-Hans", l10n.localeCode("zh-Hans-CN").?);
    try testing.expectEqualStrings("zh-Hans", l10n.localeCode("ZH").?);
    try testing.expect(l10n.localeCode("zh_TW") == null);
    try testing.expect(l10n.localeCode("zh-Hant") == null);
    try testing.expect(l10n.localeCode("zh_HK.UTF-8") == null);
    try testing.expectEqualStrings("pt-BR", l10n.localeCode("pt_PT").?);
    try testing.expectEqualStrings("pt-BR", l10n.localeCode("PT-br").?);
    try testing.expectEqualStrings("ja", l10n.localeCode("ja_JP.UTF-8").?);
    try testing.expectEqualStrings("hi", l10n.localeCode("hi_IN").?);
    try testing.expectEqualStrings("de", l10n.localeCode("de_DE@euro").?);
    try testing.expectEqualStrings("en", l10n.localeCode("en_US").?);
    try testing.expect(l10n.localeCode("ko_KR") == null);
    try testing.expect(l10n.localeCode("") == null);
    try testing.expect(l10n.localeCode(".UTF-8") == null);
}

test "parseCode takes a table code in any case with _ for -" {
    try testing.expectEqualStrings("pt-BR", l10n.parseCode("pt_br").?);
    try testing.expectEqualStrings("zh-Hans", l10n.parseCode("ZH-HANS").?);
    try testing.expectEqualStrings("en", l10n.parseCode(" en ").?);
    try testing.expect(l10n.parseCode("zh") == null);
    try testing.expect(l10n.parseCode("ko") == null);
    try testing.expect(l10n.parseCode("") == null);
}

test "stripLangArg removes both forms and keeps everything else in order" {
    var bad: []const u8 = "";
    const eq = try l10n.stripLangArg(testing.allocator, &.{ "list", "--lang=ja", "a.pak", "-v" }, &bad);
    defer testing.allocator.free(eq.args);
    try testing.expectEqualStrings("ja", eq.lang.?);
    try testing.expectEqual(@as(usize, 3), eq.args.len);
    try testing.expectEqualStrings("list", eq.args[0]);
    try testing.expectEqualStrings("a.pak", eq.args[1]);
    try testing.expectEqualStrings("-v", eq.args[2]);

    const spaced = try l10n.stripLangArg(testing.allocator, &.{ "--lang", "pt_br", "x" }, &bad);
    defer testing.allocator.free(spaced.args);
    try testing.expectEqualStrings("pt-BR", spaced.lang.?);
    try testing.expectEqual(@as(usize, 1), spaced.args.len);

    const last_wins = try l10n.stripLangArg(testing.allocator, &.{ "--lang=de", "--lang", "fr" }, &bad);
    defer testing.allocator.free(last_wins.args);
    try testing.expectEqualStrings("fr", last_wins.lang.?);
    try testing.expectEqual(@as(usize, 0), last_wins.args.len);

    const none = try l10n.stripLangArg(testing.allocator, &.{ "a", "b" }, &bad);
    defer testing.allocator.free(none.args);
    try testing.expect(none.lang == null);

    const after_dashes = try l10n.stripLangArg(testing.allocator, &.{ "--", "--lang=ja" }, &bad);
    defer testing.allocator.free(after_dashes.args);
    try testing.expect(after_dashes.lang == null);
    try testing.expectEqual(@as(usize, 2), after_dashes.args.len);
}

test "stripLangArg: a missing value or an unknown code is an error" {
    var bad: []const u8 = "";
    try testing.expectError(error.MissingValue, l10n.stripLangArg(testing.allocator, &.{"--lang"}, &bad));
    try testing.expectError(error.MissingValue, l10n.stripLangArg(testing.allocator, &.{ "x", "--lang" }, &bad));
    try testing.expectError(error.MissingValue, l10n.stripLangArg(testing.allocator, &.{"--lang="}, &bad));
    try testing.expectError(error.MissingValue, l10n.stripLangArg(testing.allocator, &.{ "--lang", "--help" }, &bad));
    try testing.expectError(error.UnknownLanguage, l10n.stripLangArg(testing.allocator, &.{"--lang=klingon"}, &bad));
    try testing.expectEqualStrings("klingon", bad);
    try testing.expectError(error.UnknownLanguage, l10n.stripLangArg(testing.allocator, &.{ "--lang", "ko" }, &bad));
    try testing.expectEqualStrings("ko", bad);
    try testing.expect(std.mem.startsWith(u8, l10n.valid_codes, "en, ru, zh-Hans"));
}

test "the Windows language id map covers the nine languages and not Traditional Chinese" {
    try testing.expectEqualStrings("ru", l10n.langIdCode(0x0419).?);
    try testing.expectEqualStrings("zh-Hans", l10n.langIdCode(0x0804).?);
    try testing.expectEqualStrings("zh-Hans", l10n.langIdCode(0x0004).?);
    try testing.expect(l10n.langIdCode(0x0404) == null);
    try testing.expect(l10n.langIdCode(0x0C04) == null);
    try testing.expectEqualStrings("ja", l10n.langIdCode(0x0411).?);
    try testing.expectEqualStrings("hi", l10n.langIdCode(0x0439).?);
    try testing.expectEqualStrings("es", l10n.langIdCode(0x0C0A).?);
    try testing.expectEqualStrings("fr", l10n.langIdCode(0x040C).?);
    try testing.expectEqualStrings("de", l10n.langIdCode(0x0407).?);
    try testing.expectEqualStrings("pt-BR", l10n.langIdCode(0x0416).?);
    try testing.expectEqualStrings("pt-BR", l10n.langIdCode(0x0816).?);
    try testing.expectEqualStrings("id", l10n.langIdCode(0x0421).?);
    try testing.expectEqualStrings("en", l10n.langIdCode(0x0409).?);
    try testing.expect(l10n.langIdCode(0x0412) == null);
}

test "OsLocales is empty on Linux and the console helper is a no-op" {
    if (@import("builtin").os.tag != .linux) return error.SkipZigTest;
    const os = l10n.OsLocales.query();
    try testing.expectEqual(@as(usize, 0), os.count);
    var out: [l10n.OsLocales.max][]const u8 = undefined;
    try testing.expectEqual(@as(usize, 0), os.slice(&out).len);
    var console = l10n.Console.enableUtf8();
    try testing.expect(!console.active);
    console.restore();
}

// ------------------------------------------------------------------ the tables

const english = "greet=Hello\r\nonly.en=English only\r\nfiles={0} files in {1}\r\nnl=a\\nb\r\n";
const german = "greet=Hallo\r\nfiles={1}: {0} Dateien\r\n";
const embedded = [_]l10n.Embedded{
    .{ .code = "en", .bytes = english },
    .{ .code = "de", .bytes = german },
};

var missing_seen: [8][2][]const u8 = undefined;
var missing_count: usize = 0;

fn recordMissing(code: []const u8, key: []const u8) void {
    if (missing_count < missing_seen.len) missing_seen[missing_count] = .{ code, key };
    missing_count += 1;
}

test "get follows the language, falls back to English and is never empty" {
    missing_count = 0;
    var set = l10n.Set.init(testing.allocator, testing.io, &embedded, .{ .debug_log = true, .on_missing = recordMissing });
    defer set.deinit();
    try testing.expectEqualStrings("Hello", set.get("greet"));
    set.setLanguage("de");
    try testing.expectEqualStrings("de", set.language());
    try testing.expectEqualStrings("Hallo", set.get("greet"));
    try testing.expectEqualStrings("English only", set.get("only.en"));
    try testing.expectEqualStrings("English only", set.get("only.en"));
    try testing.expectEqualStrings("no.such.key", set.get("no.such.key"));
    // Once per (language, key); English itself never reports.
    try testing.expectEqual(@as(usize, 2), missing_count);
    try testing.expectEqualStrings("de", missing_seen[0][0]);
    try testing.expectEqualStrings("only.en", missing_seen[0][1]);
    set.setLanguage("en");
    _ = set.get("no.such.key");
    try testing.expectEqual(@as(usize, 2), missing_count);
    // A language with no embedded table, and a code that is no language, show English.
    set.setLanguage("ja");
    try testing.expectEqualStrings("Hello", set.get("greet"));
    set.setLanguage("xx");
    try testing.expectEqualStrings("en", set.language());
}

test "a missing key is silent unless BK_DEBUG_LOG is on" {
    missing_count = 0;
    var set = l10n.Set.init(testing.allocator, testing.io, &embedded, .{ .on_missing = recordMissing });
    defer set.deinit();
    set.setLanguage("de");
    try testing.expectEqualStrings("English only", set.get("only.en"));
    try testing.expectEqual(@as(usize, 0), missing_count);
    try testing.expect(!(l10n.EnvView{}).debugLog());
    try testing.expect((l10n.EnvView{ .bk_debug_log = "1" }).debugLog());
    try testing.expect(!(l10n.EnvView{ .bk_debug_log = "0" }).debugLog());
}

test "format fills numbered slots with strings and integers" {
    var set = l10n.Set.init(testing.allocator, testing.io, &embedded, .{});
    defer set.deinit();
    var buffer: [64]u8 = undefined;
    try testing.expectEqualStrings("3 files in x.pak", try set.format(&buffer, "files", .{ 3, "x.pak" }));
    set.setLanguage("de");
    try testing.expectEqualStrings("x.pak: 3 Dateien", try set.format(&buffer, "files", .{ 3, "x.pak" }));
    try testing.expectEqualStrings("{1}: a Dateien", try set.format(&buffer, "files", .{"a"}));
    var tiny: [4]u8 = undefined;
    try testing.expectError(error.NoSpaceLeft, set.format(&tiny, "files", .{ 3, "x.pak" }));
    try testing.expectEqualStrings("a\nb", set.get("nl"));
}

test "BK_L10N_DIR replaces a table, and a broken file falls back to the embedded text" {
    var tmp = testing.tmpDir(.{});
    defer tmp.cleanup();
    try tmp.dir.writeFile(testing.io, .{ .sub_path = "de.lang", .data = "greet=Servus\r\n" });
    try tmp.dir.writeFile(testing.io, .{ .sub_path = "en.lang", .data = "greet=Hi\r\ngreet=twice\r\n" });
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const len = try tmp.dir.realPath(testing.io, &path_buffer);
    var set = l10n.Set.init(testing.allocator, testing.io, &embedded, .{ .override_dir = path_buffer[0..len] });
    defer set.deinit();
    set.setLanguage("de");
    try testing.expectEqualStrings("Servus", set.get("greet"));
    try testing.expectEqualStrings("English only", set.get("only.en"));
    set.setLanguage("en");
    try testing.expectEqualStrings("Hello", set.get("greet"));
}

// --------------------------------------------------------------------- the format

const fixture = @embedFile("fixtures/l10n/bk-tools/conformance.lang");

/// What the editor kit's parser reads from the fixture. The kit imports ImGui and cannot be built
/// into this test, so the list is checked in; Sources/editor/kit/l10n.zig tests the same rules
/// ("parse reads CRLF, comments, blank lines and escapes").
const expected = [_]l10n.Entry{
    .{ .key = "plain", .value = "Hello" },
    .{ .key = "equals", .value = "a=b=c" },
    .{ .key = "escapes", .value = "line one\nline two\ttabbed \\ slash" },
    .{ .key = "slots", .value = "{1} of {0}: {2} {9}" },
    .{ .key = "empty", .value = "" },
    .{ .key = "unicode", .value = "日本語 – ünï" },
    .{ .key = "brace", .value = "literal { and {x} stay" },
};

test "the conformance fixture parses to the checked-in entry list" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const entries = try l10n.parse(arena_state.allocator(), fixture, null);
    try testing.expectEqual(expected.len, entries.len);
    for (entries, expected) |got, want| {
        try testing.expectEqualStrings(want.key, got.key);
        try testing.expectEqualStrings(want.value, got.value);
    }
}

test "parse rejects a duplicate key, bad UTF-8, a BOM, a bad escape and a line without a key" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var diag: l10n.Diagnostic = .{};
    try testing.expectError(error.DuplicateKey, l10n.parse(arena, "a=1\r\nb=2\r\na=3\r\n", &diag));
    try testing.expectEqual(@as(usize, 3), diag.line);
    try testing.expectError(error.InvalidUtf8, l10n.parse(arena, "a=\xff\r\n", null));
    try testing.expectError(error.ByteOrderMark, l10n.parse(arena, "\xEF\xBB\xBFa=1\r\n", null));
    try testing.expectError(error.BadEscape, l10n.parse(arena, "a=\\q\r\n", null));
    try testing.expectError(error.BadEscape, l10n.parse(arena, "a=end\\\r\n", null));
    try testing.expectError(error.MissingEquals, l10n.parse(arena, "just text\r\n", null));
    try testing.expectError(error.EmptyKey, l10n.parse(arena, "=v\r\n", null));
}

// ------------------------------------------------------------------ the keys the tools use

/// The key prefixes of the tools' tables: the Zig tools' own, `cxx` for what the three C++ tools
/// share, and `tool` for what every tool does.
const key_prefixes = [_][]const u8{ "tool", "pak", "image", "table", "video", "cxx", "sprite", "font", "map", "obj2mod" };

/// The sources that ask for a key, as the build sees them.
const tool_sources = [_][]const u8{
    @embedFile("bk_tool.zig"),
    @embedFile("bk_pak.zig"),
    @embedFile("bk_image.zig"),
    @embedFile("bk_table.zig"),
    @embedFile("bk_table_xml.zig"),
    @embedFile("bk_video.zig"),
    @embedFile("bk_font.cpp"),
    @embedFile("bk_map.cpp"),
    @embedFile("bk_map_obj2mod.cpp"),
    @embedFile("bk_sprite.cpp"),
    @embedFile("tool_host.cpp"),
};

fn isKeyToken(token: []const u8) bool {
    const dot = std.mem.indexOfScalar(u8, token, '.') orelse return false;
    var known = false;
    for (key_prefixes) |prefix| {
        if (std.mem.eql(u8, token[0..dot], prefix)) known = true;
    }
    if (!known or dot + 1 == token.len) return false;
    for (token) |c| {
        if (!(std.ascii.isLower(c) or std.ascii.isDigit(c) or c == '.' or c == '_')) return false;
    }
    return true;
}

test "every key the tools ask for is in en.lang and every en.lang key is asked for" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const entries = try l10n.parse(arena, @embedFile("lang/en.lang"), null);

    var used: std.StringHashMapUnmanaged(void) = .empty;
    for (tool_sources) |source| {
        var at: usize = 0;
        while (std.mem.indexOfScalarPos(u8, source, at, '"')) |open| {
            const close = std.mem.indexOfScalarPos(u8, source, open + 1, '"') orelse break;
            // Every quote opens a candidate, so a '"' character literal cannot shift the pairing.
            at = open + 1;
            const token = source[open + 1 .. close];
            if (std.mem.indexOfScalar(u8, token, '\n') != null) continue;
            if (isKeyToken(token)) try used.put(arena, token, {});
        }
    }
    var in_table: std.StringHashMapUnmanaged(void) = .empty;
    for (entries) |entry| try in_table.put(arena, entry.key, {});

    var failures: usize = 0;
    var asked = used.keyIterator();
    while (asked.next()) |key| {
        if (!in_table.contains(key.*)) {
            std.debug.print("key '{s}' is used by a tool but is not in en.lang\n", .{key.*});
            failures += 1;
        }
    }
    for (entries) |entry| {
        if (!used.contains(entry.key)) {
            std.debug.print("key '{s}' is in en.lang but no tool uses it\n", .{entry.key});
            failures += 1;
        }
    }
    try testing.expectEqual(@as(usize, 0), failures);
}

const tables = blk: {
    var out: [l10n.codes.len][]const u8 = undefined;
    for (l10n.codes, 0..) |code, i| out[i] = @embedFile("lang/" ++ code ++ ".lang");
    break :blk out;
};

/// The `{0}`..`{9}` slots a text uses, as a bit set.
fn slotsOf(text: []const u8) u16 {
    var slots: u16 = 0;
    var at: usize = 0;
    while (at + 2 < text.len) : (at += 1) {
        if (text[at] == '{' and text[at + 2] == '}' and std.ascii.isDigit(text[at + 1])) slots |= @as(u16, 1) << @intCast(text[at + 1] - '0');
    }
    return slots;
}

test "every table has the keys of en.lang and uses the same slots" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const reference = try l10n.parse(arena, tables[0], null);
    var failures: usize = 0;
    for (l10n.codes[1..], tables[1..]) |code, bytes| {
        const entries = try l10n.parse(arena, bytes, null);
        var by_key: std.StringHashMapUnmanaged([]const u8) = .empty;
        for (entries) |entry| try by_key.put(arena, entry.key, entry.value);
        for (reference) |entry| {
            const text = by_key.get(entry.key) orelse {
                std.debug.print("{s}.lang lacks '{s}'\n", .{ code, entry.key });
                failures += 1;
                continue;
            };
            if (slotsOf(text) != slotsOf(entry.value)) {
                std.debug.print("{s}.lang '{s}' uses other slots than en.lang\n", .{ code, entry.key });
                failures += 1;
            }
        }
        if (entries.len != reference.len) {
            std.debug.print("{s}.lang has {d} keys, en.lang {d}\n", .{ code, entries.len, reference.len });
            failures += 1;
        }
    }
    try testing.expectEqual(@as(usize, 0), failures);
}

test "no failure or message text in any table has a line break" {
    // A failure is one `<tool>: ` line on stderr (D-04), so only the usage texts may span lines.
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var failures: usize = 0;
    for (l10n.codes, tables) |code, bytes| {
        for (try l10n.parse(arena, bytes, null)) |entry| {
            if (std.mem.indexOf(u8, entry.key, ".usage.") != null) continue;
            if (std.mem.indexOfAny(u8, entry.value, "\r\n") != null) {
                std.debug.print("{s}.lang '{s}' has a line break\n", .{ code, entry.key });
                failures += 1;
            }
        }
    }
    try testing.expectEqual(@as(usize, 0), failures);
}
