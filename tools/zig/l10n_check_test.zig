//! Seeded-fault fixtures of the l10n-check gate (M002 S03): each case writes a minimal passing tree
//! under zig-out/local-test/l10n-check/<case>/ (all nine overlays, two scope keys, a tiny
//! Data/ELK base; the UTF-16LE texts are made here, so no binary fixture is tracked), breaks one
//! thing, runs the built checker with `--root` and asserts the exit status and the exact line.
//! BK_L10N_CHECK_EXE names the checker; `zig build test-l10n-check` sets it. Nothing writes into
//! Data/ or Localization/.
const std = @import("std");
const testing = std.testing;

const scratch = "zig-out/local-test/l10n-check";
const codes = [_][]const u8{ "ru", "zh-Hans", "ja", "hi", "es", "fr", "de", "pt-BR", "id" };
const keys = [_][]const u8{ "textes/strings/alpha", "scenarios/demo/beta" };
/// Two base texts and two scope keys in each of nine languages.
const checked_total = keys.len + codes.len * keys.len;

/// The notes of the scopes no case enables: nothing is checked there and the log says so.
const note_fit = "l10n-check: fit: not active\n";
const note_tables = "l10n-check: tables: not active\n";
const note_documents = "l10n-check: documents: not active\n";
const note_literals = "l10n-check: literals: not active\n";
/// One scope note per language, in the order of the language table; every fixture language is on the seed scope.
const seed_scope_notes = blk: {
    var notes: []const u8 = "";
    for (codes) |code| notes = notes ++ "l10n-check: scope: " ++ code ++ ": seed, 2 keys checked\n";
    break :blk notes;
};
const inactive_but_fit = note_tables ++ note_documents ++ note_literals;
const all_inactive = note_fit ++ inactive_but_fit ++ seed_scope_notes;

const glossary_text = "# Glossary: fixture\r\n\r\n## Interface\r\n\r\n| English term | Translation | Note |\r\n|---|---|---|\r\n| zzterm | zzterm |  |\r\n";

const state_xml = "<?xml version=\"1.0\"?>\r\n<base State=\"{d}\" Changed=\"1\"/>\r\n";

fn utf16le(allocator: std.mem.Allocator, text: []const u8) ![]u8 {
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(allocator, &.{ 0xff, 0xfe });
    for (text) |c| try out.appendSlice(allocator, &.{ c, 0 });
    return out.toOwnedSlice(allocator);
}

fn write(io: std.Io, dir: std.Io.Dir, path: []const u8, data: []const u8) !void {
    if (std.fs.path.dirname(path)) |parent| try dir.createDirPath(io, parent);
    try dir.writeFile(io, .{ .sub_path = path, .data = data });
}

fn writeState(io: std.Io, allocator: std.mem.Allocator, dir: std.Io.Dir, path: []const u8, state: u8) !void {
    const xml = try std.fmt.allocPrint(allocator, state_xml, .{state});
    defer allocator.free(xml);
    try write(io, dir, path, xml);
}

/// A tree that passes: wiped first, then written fresh.
fn buildTree(io: std.Io, allocator: std.mem.Allocator, case: []const u8) !std.Io.Dir {
    const path = try std.fmt.allocPrint(allocator, "{s}/{s}", .{ scratch, case });
    defer allocator.free(path);
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, path) catch {};
    try cwd.createDirPath(io, path);
    var root = try cwd.openDir(io, path, .{ .iterate = true });
    errdefer root.close(io);

    try write(io, root, "Localization/game/scope.txt", "# fixture scope\r\ntextes/strings/alpha\r\nscenarios/demo/beta\r\n");
    // A glossary with one term no fixture text holds, so the glossary-term rule has a table to read.
    for (codes) |code| {
        const glossary = try std.fmt.allocPrint(allocator, "Localization/glossary/{s}.md", .{code});
        defer allocator.free(glossary);
        try write(io, root, glossary, glossary_text);
    }
    const text = try utf16le(allocator, "Hello");
    defer allocator.free(text);
    for (codes) |code| for (keys) |key| {
        const base = try std.fmt.allocPrint(allocator, "Localization/game/{s}/game_data_base/{s}", .{ code, key });
        defer allocator.free(base);
        const txt = try std.fmt.allocPrint(allocator, "{s}.txt", .{base});
        defer allocator.free(txt);
        const xml = try std.fmt.allocPrint(allocator, "{s}.xml", .{base});
        defer allocator.free(xml);
        try write(io, root, txt, text);
        try writeState(io, allocator, root, xml, 3);
    };
    // The base is spelled with another case than the scope keys, as a Data tree of the original game may be.
    try setBase(io, allocator, root, alpha_base, "Alpha original");
    try setBase(io, allocator, root, beta_base, "Beta original");
    try writeManifest(io, allocator, root, "");
    return root;
}

const alpha_base = "Data/ELK/game_data_base/Textes/Strings/ALPHA.elk";
const beta_base = "Data/ELK/game_data_base/scenarios/demo/beta.elk";

fn setBase(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, path: []const u8, text: []const u8) !void {
    const bytes = try utf16le(allocator, text);
    defer allocator.free(bytes);
    try write(io, root, path, bytes);
}

/// The manifest of the two base files as they are now, then `extra` (whole lines) after them.
fn writeManifest(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, extra: []const u8) !void {
    var out: std.ArrayList(u8) = .empty;
    defer out.deinit(allocator);
    try out.appendSlice(allocator, "# fixture manifest\r\n");
    const pairs = [_][2][]const u8{ .{ keys[0], alpha_base }, .{ keys[1], beta_base } };
    for (pairs) |pair| {
        const bytes = try root.readFileAlloc(io, pair[1], allocator, .limited(1 << 20));
        defer allocator.free(bytes);
        var digest: [32]u8 = undefined;
        std.crypto.hash.sha2.Sha256.hash(bytes, &digest, .{});
        const hex = std.fmt.bytesToHex(digest, .lower);
        const line = try std.fmt.allocPrint(allocator, "{s} {s}\r\n", .{ pair[0], hex });
        defer allocator.free(line);
        try out.appendSlice(allocator, line);
    }
    try out.appendSlice(allocator, extra);
    try write(io, root, "Localization/game/base-hashes.txt", out.items);
}

const Outcome = struct { code: u8, stderr: []u8 };

fn runChecker(io: std.Io, allocator: std.mem.Allocator, case: []const u8) !Outcome {
    return runCheckerWith(io, allocator, case, &.{});
}

fn runCheckerWith(io: std.Io, allocator: std.mem.Allocator, case: []const u8, extra: []const []const u8) !Outcome {
    var map = try testing.environ.createMap(allocator);
    defer map.deinit();
    const exe = map.get("BK_L10N_CHECK_EXE") orelse {
        std.debug.print("BK_L10N_CHECK_EXE is not set: run zig build test-l10n-check\n", .{});
        return error.MissingEnvironment;
    };
    const root = try std.fmt.allocPrint(allocator, "{s}/{s}", .{ scratch, case });
    defer allocator.free(root);
    var argv: std.ArrayList([]const u8) = .empty;
    defer argv.deinit(allocator);
    try argv.appendSlice(allocator, &.{ exe, "--root", root, "--fonts", "vendor/fonts", "--no-default-docs" });
    try argv.appendSlice(allocator, extra);
    const result = try std.process.run(allocator, io, .{ .argv = argv.items });
    allocator.free(result.stdout);
    const code: u8 = switch (result.term) {
        .exited => |c| c,
        else => 255,
    };
    return .{ .code = code, .stderr = result.stderr };
}

/// The case fails with exactly one finding, `line`, and the summary of a tree whose checks all ran.
fn expectFinding(io: std.Io, case: []const u8, root: *std.Io.Dir, line: []const u8) !void {
    defer root.close(io);
    const outcome = try runChecker(io, testing.allocator, case);
    defer testing.allocator.free(outcome.stderr);
    const expected = try std.fmt.allocPrint(testing.allocator, "{s}\n" ++ all_inactive ++ "l10n-check: 1 findings, {d} checked\n", .{ line, checked_total });
    defer testing.allocator.free(expected);
    try testing.expectEqual(@as(u8, 1), outcome.code);
    try testing.expectEqualStrings(expected, outcome.stderr);
}

const de_alpha = "Localization/game/de/game_data_base/textes/strings/alpha";

test "a complete tree passes with the summary and nothing else" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "pass");
    root.close(io);
    const outcome = try runChecker(io, testing.allocator, "pass");
    defer testing.allocator.free(outcome.stderr);
    const expected = try std.fmt.allocPrint(testing.allocator, all_inactive ++ "l10n-check: 0 findings, {d} checked\n", .{checked_total});
    defer testing.allocator.free(expected);
    try testing.expectEqual(@as(u8, 0), outcome.code);
    try testing.expectEqualStrings(expected, outcome.stderr);
}

test "missing_key: a scope key without a translation" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "missing_key");
    try root.deleteFile(io, de_alpha ++ ".txt");
    try expectFinding(io, "missing_key", &root, "l10n-check: missing: de: textes/strings/alpha: no translation");
}

test "missing_xml: a translation without its state file" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "missing_xml");
    try root.deleteFile(io, de_alpha ++ ".xml");
    try expectFinding(io, "missing_xml", &root, "l10n-check: missing-xml: de: textes/strings/alpha: no state file");
}

test "bad_state: a text that is not translated" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "bad_state");
    try writeState(io, testing.allocator, root, de_alpha ++ ".xml", 0);
    try expectFinding(io, "bad_state", &root, "l10n-check: state: de: textes/strings/alpha: State=0 (not translated), want 2 or 3");
}

test "outdated_state: an outdated text reports as stale" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "outdated_state");
    try writeState(io, testing.allocator, root, de_alpha ++ ".xml", 1);
    try expectFinding(io, "outdated_state", &root, "l10n-check: stale: de: textes/strings/alpha: State=1 (outdated): the English text changed after the translation");
}

test "bad_encoding: a text without the byte order mark" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "bad_encoding");
    try write(io, root, de_alpha ++ ".txt", "H\x00i\x00");
    try expectFinding(io, "bad_encoding", &root, "l10n-check: encoding: de: textes/strings/alpha: text lacks the UTF-16LE byte order mark");
}

test "empty_text: a text with nothing in it" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "empty_text");
    try write(io, root, de_alpha ++ ".txt", "\xff\xfe \x00");
    try expectFinding(io, "empty_text", &root, "l10n-check: empty: de: textes/strings/alpha: text holds no characters");
}

test "extra_key: an overlay text outside the scope" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "extra_key");
    try write(io, root, "Localization/game/de/game_data_base/textes/strings/zzz.txt", "\xff\xfeA\x00");
    try expectFinding(io, "extra_key", &root, "l10n-check: extra: de: textes/strings/zzz: not in the seed list of Localization/game/scope.txt (no prefix or whole-database line for this language)");
}

test "base_missing: a scope key the English base does not hold" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "base_missing");
    try root.deleteFile(io, beta_base);
    try expectFinding(io, "base_missing", &root, "l10n-check: base-missing: all: scenarios/demo/beta: no original text in Data/ELK/game_data_base");
}

test "stale_text: the English text changed after the manifest was written" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "stale_text");
    try setBase(io, testing.allocator, root, alpha_base, "Alpha revised");
    try expectFinding(io, "stale_text", &root, "l10n-check: stale: all: textes/strings/alpha: the English source changed since it was hashed in Localization/game/base-hashes.txt; review the translations, then run zig build l10n-check-update-base");
}

test "base_hash_missing: a scope key without a manifest line" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "base_hash_missing");
    try write(io, root, "Localization/game/base-hashes.txt", "# no lines\r\nscenarios/demo/beta 00\r\n");
    // beta's recorded hash is wrong too, so the case keeps alpha missing and beta stale.
    defer root.close(io);
    const outcome = try runChecker(io, testing.allocator, "base_hash_missing");
    defer testing.allocator.free(outcome.stderr);
    const expected = try std.fmt.allocPrint(testing.allocator, "{s}\n{s}\n" ++ all_inactive ++ "l10n-check: 2 findings, {d} checked\n", .{
        "l10n-check: stale: all: scenarios/demo/beta: the English source changed since it was hashed in Localization/game/base-hashes.txt; review the translations, then run zig build l10n-check-update-base",
        "l10n-check: base-hash-missing: all: textes/strings/alpha: no line in Localization/game/base-hashes.txt; run zig build l10n-check-update-base",
        checked_total,
    });
    defer testing.allocator.free(expected);
    try testing.expectEqual(@as(u8, 1), outcome.code);
    try testing.expectEqualStrings(expected, outcome.stderr);
}

test "untranslated: an overlay text equal to the English original" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "untranslated");
    try setBase(io, testing.allocator, root, de_alpha ++ ".txt", "Alpha original");
    try expectFinding(io, "untranslated", &root, "l10n-check: untranslated: de: textes/strings/alpha: text equals the English original; translate it or add `same-as-base de textes/strings/alpha` to Localization/game/base-hashes.txt");
}

test "same_as_base_allowed: a listed translation may equal the English" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "same_as_base_allowed");
    try setBase(io, testing.allocator, root, de_alpha ++ ".txt", "Alpha original");
    try writeManifest(io, testing.allocator, root, "same-as-base de textes/strings/alpha\r\n");
    root.close(io);
    const outcome = try runChecker(io, testing.allocator, "same_as_base_allowed");
    defer testing.allocator.free(outcome.stderr);
    const expected = try std.fmt.allocPrint(testing.allocator, all_inactive ++ "l10n-check: 0 findings, {d} checked\n", .{checked_total});
    defer testing.allocator.free(expected);
    try testing.expectEqual(@as(u8, 0), outcome.code);
    try testing.expectEqualStrings(expected, outcome.stderr);
}

test "broken_placeholder: a translation that lost a %s of the English" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "broken_placeholder");
    try setBase(io, testing.allocator, root, alpha_base, "Alpha %s of %d");
    try writeManifest(io, testing.allocator, root, "");
    try setBase(io, testing.allocator, root, de_alpha ++ ".txt", "Alpha %d");
    // The other eight languages still hold "Hello": they lose both tokens too.
    root.close(io);
    const outcome = try runChecker(io, testing.allocator, "broken_placeholder");
    defer testing.allocator.free(outcome.stderr);
    try testing.expectEqual(@as(u8, 1), outcome.code);
    const de_line = "l10n-check: placeholder: de: textes/strings/alpha: placeholders differ: English has %d %s, translation has %d\n";
    try testing.expect(std.mem.indexOf(u8, outcome.stderr, de_line) != null);
    const fr_line = "l10n-check: placeholder: fr: textes/strings/alpha: placeholders differ: English has %d %s, translation has none\n";
    try testing.expect(std.mem.indexOf(u8, outcome.stderr, fr_line) != null);
}

/// One overlay fixture whose English holds `english` and whose German translation holds `german`;
/// the other eight languages hold the English plus a mark (so they are translated and keep every code), and only the German finding is asserted.
fn expectPlaceholderCase(case: []const u8, english: []const u8, german: []const u8, want: []const u8) !void {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, case);
    try setBase(io, testing.allocator, root, alpha_base, english);
    try writeManifest(io, testing.allocator, root, "");
    for (codes) |code| {
        const path = try std.fmt.allocPrint(testing.allocator, "Localization/game/{s}/game_data_base/textes/strings/alpha.txt", .{code});
        defer testing.allocator.free(path);
        const others = try std.fmt.allocPrint(testing.allocator, "{s} !", .{english});
        defer testing.allocator.free(others);
        try setBase(io, testing.allocator, root, path, if (std.mem.eql(u8, code, "de")) german else others);
    }
    root.close(io);
    const outcome = try runChecker(io, testing.allocator, case);
    defer testing.allocator.free(outcome.stderr);
    if (want.len == 0) {
        try testing.expectEqual(@as(u8, 0), outcome.code);
        try testing.expect(std.mem.indexOf(u8, outcome.stderr, "placeholder") == null);
        return;
    }
    try testing.expectEqual(@as(u8, 1), outcome.code);
    const line = try std.fmt.allocPrint(testing.allocator, "l10n-check: placeholder: de: textes/strings/alpha: placeholders differ: {s}\n", .{want});
    defer testing.allocator.free(line);
    try testing.expect(std.mem.indexOf(u8, outcome.stderr, line) != null);
}

test "placeholder_length: %ld changed to %ls" {
    try expectPlaceholderCase("placeholder_length", "Count %ld", "Anzahl %ls", "English has %ld, translation has %ls");
}

test "placeholder_long_long: %lld changed to %ld" {
    try expectPlaceholderCase("placeholder_long_long", "Total %lld", "Summe %ld", "English has %lld, translation has %ld");
}

test "placeholder_size: %zu changed to %u" {
    try expectPlaceholderCase("placeholder_size", "Bytes %zu", "Bytes %u", "English has %zu, translation has %u");
}

test "placeholder_positional_dropped: %2$d lost" {
    try expectPlaceholderCase("placeholder_positional_dropped", "%1$s has %2$d", "%1$s hat", "English has %1$s %2$d, translation has %1$s");
}

test "placeholder_positional_swapped: %1$s %2$d became %1$d %2$s" {
    try expectPlaceholderCase("placeholder_positional_swapped", "%1$s has %2$d", "%1$d hat %2$s", "English has %1$s %2$d, translation has %1$d %2$s");
}

test "placeholder_width_precision: %5.2f changed to %f" {
    try expectPlaceholderCase("placeholder_width_precision", "Value %5.2f", "Wert %f", "English has %5.2f, translation has %f");
}

test "placeholder_reordered_positional: swapped word order with the same codes passes" {
    try expectPlaceholderCase("placeholder_reordered_positional", "%1$s has %2$d", "%2$d hat %1$s", "");
}

test "placeholder_unknown_conversion: %y is not a code" {
    try expectPlaceholderCase("placeholder_unknown_conversion", "Rate 5%y", "Rate 5 Prozent", "");
}

test "placeholder_percent_percent: %% is not a code" {
    try expectPlaceholderCase("placeholder_percent_percent", "100%% of %d", "100 Prozent von %d", "");
}

fn utf16Units(allocator: std.mem.Allocator, units: []const u16) ![]u8 {
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(allocator, &.{ 0xff, 0xfe });
    for (units) |unit| try out.appendSlice(allocator, &.{ @truncate(unit), @truncate(unit >> 8) });
    return out.toOwnedSlice(allocator);
}

const ru_alpha = "Localization/game/ru/game_data_base/textes/strings/alpha";

test "uncovered_glyph_cjk: a Han character in the Russian overlay" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "uncovered_glyph_cjk");
    const bytes = try utf16Units(testing.allocator, &.{ 0x041F, 0x0440, 0x76F4 });
    defer testing.allocator.free(bytes);
    try write(io, root, ru_alpha ++ ".txt", bytes);
    try expectFinding(io, "uncovered_glyph_cjk", &root, "l10n-check: glyph: ru: textes/strings/alpha: U+76F4 not in NotoSans-Regular.ttf");
}

test "uncovered_glyph_latin: a Cyrillic letter in the German overlay" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "uncovered_glyph_latin");
    const bytes = try utf16Units(testing.allocator, &.{ 'G', 0x00FC, 0x0416 });
    defer testing.allocator.free(bytes);
    try write(io, root, de_alpha ++ ".txt", bytes);
    try expectFinding(io, "uncovered_glyph_latin", &root, "l10n-check: glyph: de: textes/strings/alpha: U+0416 not in cp1252");
}

test "covered_hindi_with_ascii: Devanagari with ASCII and a Western digit passes" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "covered_hindi_with_ascii");
    const bytes = try utf16Units(testing.allocator, &.{ 0x0915, 0x094D, 0x0937, ' ', 'A', 'B', '1', '.', '\r', '\n' });
    defer testing.allocator.free(bytes);
    try write(io, root, "Localization/game/hi/game_data_base/textes/strings/alpha.txt", bytes);
    root.close(io);
    const outcome = try runChecker(io, testing.allocator, "covered_hindi_with_ascii");
    defer testing.allocator.free(outcome.stderr);
    const expected = try std.fmt.allocPrint(testing.allocator, all_inactive ++ "l10n-check: 0 findings, {d} checked\n", .{checked_total});
    defer testing.allocator.free(expected);
    try testing.expectEqual(@as(u8, 0), outcome.code);
    try testing.expectEqualStrings(expected, outcome.stderr);
}

const fixtures = "tools/zig/fixtures/l10n";
const table_dir = "tools/zig/lang";

fn readFixture(io: std.Io, allocator: std.mem.Allocator, name: []const u8) ![]u8 {
    const path = try std.fmt.allocPrint(allocator, "{s}/{s}", .{ fixtures, name });
    defer allocator.free(path);
    return std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(1 << 20));
}

/// The fixture pair as a table directory: en.lang, and de.lang copied to the other eight codes.
fn addTables(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir) !void {
    const en = try readFixture(io, allocator, "tables/en.lang");
    defer allocator.free(en);
    const de = try readFixture(io, allocator, "tables/de.lang");
    defer allocator.free(de);
    try write(io, root, table_dir ++ "/en.lang", en);
    for (codes) |code| {
        const path = try std.fmt.allocPrint(allocator, "{s}/{s}.lang", .{ table_dir, code });
        defer allocator.free(path);
        try write(io, root, path, de);
    }
}

const doc_names = [_][]const u8{ "Russian", "Chinese", "Hindi", "Spanish", "French", "German", "Portuguese", "Japanese", "Indonesian" };

/// The fixture document set under docs/: README.md, and the German file copied to the other eight names.
fn addDocuments(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir) !void {
    const en = try readFixture(io, allocator, "docs/README.md");
    defer allocator.free(en);
    const de = try readFixture(io, allocator, "docs/README_German.md");
    defer allocator.free(de);
    try write(io, root, "docs/README.md", en);
    for (doc_names) |name| {
        const path = try std.fmt.allocPrint(allocator, "docs/README_{s}.md", .{name});
        defer allocator.free(path);
        try write(io, root, path, de);
    }
}

/// Replaces `old` by `new` once in the file of the tree; the case is wrong when `old` is not there.
fn edit(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, path: []const u8, old: []const u8, new: []const u8) !void {
    const bytes = try root.readFileAlloc(io, path, allocator, .limited(1 << 20));
    defer allocator.free(bytes);
    try testing.expect(std.mem.indexOf(u8, bytes, old) != null);
    const changed = try std.mem.replaceOwned(u8, allocator, bytes, old, new);
    defer allocator.free(changed);
    try write(io, root, path, changed);
}

/// Closes the tree, runs the checker with `extra` and expects `code` and exactly `want` on stderr.
fn expectRun(io: std.Io, case: []const u8, root: *std.Io.Dir, extra: []const []const u8, code: u8, want: []const u8) !void {
    root.close(io);
    const outcome = try runCheckerWith(io, testing.allocator, case, extra);
    defer testing.allocator.free(outcome.stderr);
    try testing.expectEqual(code, outcome.code);
    try testing.expectEqualStrings(want, outcome.stderr);
}

/// One finding in a tree where only the tables are active (ten files checked).
fn expectTableFinding(io: std.Io, case: []const u8, root: *std.Io.Dir, line: []const u8) !void {
    const want = try std.fmt.allocPrint(testing.allocator, "{s}\n" ++ note_fit ++ note_documents ++ note_literals ++ seed_scope_notes ++ "l10n-check: 1 findings, {d} checked\n", .{ line, checked_total + 10 });
    defer testing.allocator.free(want);
    try expectRun(io, case, root, &.{}, 1, want);
}

fn expectDocumentFinding(io: std.Io, case: []const u8, root: *std.Io.Dir, line: []const u8) !void {
    const want = try std.fmt.allocPrint(testing.allocator, "{s}\n" ++ note_fit ++ note_tables ++ note_literals ++ seed_scope_notes ++ "l10n-check: 1 findings, {d} checked\n", .{ line, checked_total + 10 });
    defer testing.allocator.free(want);
    try expectRun(io, case, root, &.{ "--doc-set", "docs/README" }, 1, want);
}

const de_table = table_dir ++ "/de.lang";

test "tables_pass: the fixture table in nine languages passes" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "tables_pass");
    try addTables(io, testing.allocator, root);
    const want = try std.fmt.allocPrint(testing.allocator, note_fit ++ note_documents ++ note_literals ++ seed_scope_notes ++ "l10n-check: 0 findings, {d} checked\n", .{checked_total + 10});
    defer testing.allocator.free(want);
    try expectRun(io, "tables_pass", &root, &.{}, 0, want);
}

test "table_missing_key: a key en.lang has and de.lang lacks" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "table_missing_key");
    try addTables(io, testing.allocator, root);
    try edit(io, testing.allocator, root, de_table, "file.save=Speichern\r\n", "");
    try expectTableFinding(io, "table_missing_key", &root, "l10n-check: table-missing: de: tools/zig/lang/de.lang:file.save: key is in en.lang but not here");
}

test "table_missing_file: a language without its table" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "table_missing_file");
    try addTables(io, testing.allocator, root);
    try root.deleteFile(io, table_dir ++ "/fr.lang");
    try expectTableFinding(io, "table_missing_file", &root, "l10n-check: table-missing: fr: tools/zig/lang/fr.lang: no table file although en.lang exists");
}

test "table_extra_key: a key en.lang no longer has" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "table_extra_key");
    try addTables(io, testing.allocator, root);
    try edit(io, testing.allocator, root, de_table, "file.save=Speichern\r\n", "file.save=Speichern\r\nfile.close=Schließen\r\n");
    try expectTableFinding(io, "table_extra_key", &root, "l10n-check: table-extra: de: tools/zig/lang/de.lang:file.close: key is not in en.lang");
}

test "table_placeholder: a printf code that differs from en" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "table_placeholder");
    try addTables(io, testing.allocator, root);
    try edit(io, testing.allocator, root, de_table, "%d von", "%s von");
    try expectTableFinding(io, "table_placeholder", &root, "l10n-check: table-placeholder: de: tools/zig/lang/de.lang:status.count: placeholders differ: English has %d {0}, translation has %s {0}");
}

test "table_bad_id_suffix: an ID suffix that differs from en" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "table_bad_id_suffix");
    try addTables(io, testing.allocator, root);
    try edit(io, testing.allocator, root, de_table, "###file.open", "###open");
    try expectTableFinding(io, "table_bad_id_suffix", &root, "l10n-check: table-id: de: tools/zig/lang/de.lang:file.open: ID suffix ###open differs from English ###file.open");
}

test "table_id_not_key: a ### suffix that is not the key" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "table_id_not_key");
    try addTables(io, testing.allocator, root);
    try edit(io, testing.allocator, root, table_dir ++ "/en.lang", "###file.open", "###other");
    root.close(io);
    const outcome = try runChecker(io, testing.allocator, "table_id_not_key");
    defer testing.allocator.free(outcome.stderr);
    try testing.expectEqual(@as(u8, 1), outcome.code);
    try testing.expect(std.mem.indexOf(u8, outcome.stderr, "l10n-check: table-id: en: tools/zig/lang/en.lang:file.open: ID suffix ###other must be ###file.open\n") != null);
}

test "table_lf_line: a line that ends with LF only" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "table_lf_line");
    try addTables(io, testing.allocator, root);
    try edit(io, testing.allocator, root, de_table, "file.menu=Datei\r\n", "file.menu=Datei\n");
    try expectTableFinding(io, "table_lf_line", &root, "l10n-check: table-format: de: tools/zig/lang/de.lang:2: line ends with LF only, want CRLF");
}

test "table_glyph: a Han character in a German value" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "table_glyph");
    try addTables(io, testing.allocator, root);
    try edit(io, testing.allocator, root, de_table, "Speichern\r\n", "Speichern直\r\n");
    try expectTableFinding(io, "table_glyph", &root, "l10n-check: glyph: de: tools/zig/lang/de.lang:file.save: U+76F4 not in NotoSans-Regular.ttf");
}

test "docs_pass: the fixture document set passes" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "docs_pass");
    try addDocuments(io, testing.allocator, root);
    const want = try std.fmt.allocPrint(testing.allocator, note_fit ++ note_tables ++ note_literals ++ seed_scope_notes ++ "l10n-check: 0 findings, {d} checked\n", .{checked_total + 10});
    defer testing.allocator.free(want);
    try expectRun(io, "docs_pass", &root, &.{ "--doc-set", "docs/README" }, 0, want);
}

test "doc_structure: a heading at another level" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "doc_structure");
    try addDocuments(io, testing.allocator, root);
    try edit(io, testing.allocator, root, "docs/README_German.md", "## Bauen", "### Bauen");
    try expectDocumentFinding(io, "doc_structure", &root, "l10n-check: doc-structure: de: docs/README_German.md: heading 2 is level 3, English has level 2");
}

test "doc_code: a translation that lost its fenced code block" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "doc_code");
    try addDocuments(io, testing.allocator, root);
    try edit(io, testing.allocator, root, "docs/README_German.md", "```sh\r\nzig build\r\n```\r\n", "");
    try expectDocumentFinding(io, "doc_code", &root, "l10n-check: doc-code: de: docs/README_German.md: has 0 fenced code blocks, English has 1");
}

test "doc_bad_bar: two links of the language bar swapped" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "doc_bad_bar");
    try addDocuments(io, testing.allocator, root);
    try edit(io, testing.allocator, root, "docs/README_German.md", "[Français](README_French.md)        [Deutsch](README_German.md)", "[Deutsch](README_German.md)        [Français](README_French.md)");
    try expectDocumentFinding(io, "doc_bad_bar", &root, "l10n-check: doc-bar: de: docs/README_German.md: link 6 targets README_German.md, want README_French.md");
}

test "doc_missing_translation: a language without its document" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "doc_missing_translation");
    try addDocuments(io, testing.allocator, root);
    try root.deleteFile(io, "docs/README_Hindi.md");
    try expectDocumentFinding(io, "doc_missing_translation", &root, "l10n-check: doc-missing: hi: docs/README_Hindi.md: translation file does not exist");
}

test "doc_english_extra: a README_English.md beside the set" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "doc_english_extra");
    try addDocuments(io, testing.allocator, root);
    try write(io, root, "docs/README_English.md", "# copy\r\n");
    try expectDocumentFinding(io, "doc_english_extra", &root, "l10n-check: doc-extra: en: docs/README_English.md: the English file is README.md; remove this copy");
}

/// Every literal the fixture must report, in line order: the call name and the literal it takes.
const literal_findings = [_]struct { call: []const u8, literal: []const u8 }{
    .{ .call = "igButton", .literal = "Save" },
    .{ .call = "igBeginCombo", .literal = "Mode" },
    .{ .call = "igInputText", .literal = "Name" },
    .{ .call = "igInputTextWithHint", .literal = "Type a name" },
    .{ .call = "igInputFloat", .literal = "Scale" },
    .{ .call = "igInputFloatEx", .literal = "Offset" },
    .{ .call = "igInputInt", .literal = "Count" },
    .{ .call = "igSliderInt", .literal = "Level" },
    .{ .call = "igSliderFloat", .literal = "Zoom" },
    .{ .call = "igSliderFloatEx", .literal = "Gamma" },
    .{ .call = "igOpenPopup", .literal = "Confirm" },
    .{ .call = "igBeginPopup", .literal = "Context" },
    .{ .call = "igBeginTabItem", .literal = "Terrain" },
    .{ .call = "igBeginChild", .literal = "Objects" },
    .{ .call = "igSetItemTooltip", .literal = "Hint text" },
    .{ .call = "igCollapsingHeader", .literal = "Advanced" },
    .{ .call = "igTreeNode", .literal = "Branch" },
    .{ .call = "text", .literal = "Hello" },
    .{ .call = "beginPanel", .literal = "Tools" },
    .{ .call = "setStatus", .literal = "Saved the map" },
    .{ .call = "setStatusFrom", .literal = "Failed" },
    .{ .call = "note", .literal = "Careful" },
    .{ .call = "say", .literal = "Saved the map" },
    .{ .call = "sayImport", .literal = "Imported" },
};

test "literal_in_strict_dir: an English literal in each kind of UI call" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "literal_in_strict_dir");
    const ui = try readFixture(io, testing.allocator, "literals/ui.zig");
    defer testing.allocator.free(ui);
    try write(io, root, "literals/ui.zig", ui);
    var lines: std.ArrayList(u8) = .empty;
    defer lines.deinit(testing.allocator);
    // The first finding is on line 6; the rest start on line 9 of the fixture, one call per line.
    for (literal_findings, 0..) |one, index| {
        const line: usize = if (index == 0) 6 else index + 8;
        const text = try std.fmt.allocPrint(testing.allocator, "l10n-check: literal: all: literals/ui.zig:{d}: {s} takes the English literal \"{s}\"; wrap it in l10n.t, l10n.id or l10n.format\n", .{ line, one.call, one.literal });
        defer testing.allocator.free(text);
        try lines.appendSlice(testing.allocator, text);
    }
    const want = try std.fmt.allocPrint(testing.allocator, "{s}" ++ note_fit ++ note_tables ++ note_documents ++ seed_scope_notes ++ "l10n-check: {d} findings, {d} checked\n", .{
        lines.items,
        literal_findings.len,
        checked_total + 1,
    });
    defer testing.allocator.free(want);
    try expectRun(io, "literal_in_strict_dir", &root, &.{ "--strict-dir", "literals" }, 1, want);
}

/// The ID-bearing calls of literal-ids/ui.zig that take l10n.t as their label, with the fixture line of each.
const id_findings = [_]struct { call: []const u8, line: usize }{
    .{ .call = "igButton", .line = 6 },
    .{ .call = "igSmallButton", .line = 7 },
    .{ .call = "igMenuItemEx", .line = 8 },
    .{ .call = "igMenuItemBoolPtr", .line = 9 },
    .{ .call = "igBeginMenuEx", .line = 10 },
    .{ .call = "igSelectableEx", .line = 11 },
    .{ .call = "igCheckbox", .line = 12 },
    .{ .call = "igCombo", .line = 13 },
    .{ .call = "igBeginCombo", .line = 14 },
    .{ .call = "igInputIntEx", .line = 15 },
    .{ .call = "igBeginTabItem", .line = 16 },
    .{ .call = "igCollapsingHeader", .line = 17 },
    .{ .call = "igBeginPopupModal", .line = 18 },
    .{ .call = "igOpenPopup", .line = 19 },
    .{ .call = "igBegin", .line = 20 },
    .{ .call = "igTableSetupColumn", .line = 21 },
    .{ .call = "igTableSetupColumnEx", .line = 22 },
    .{ .call = "igInputTextWithHint", .line = 41 },
};

test "literal_id_label: an ig* label that is l10n.t fails, l10n.id and plain text pass" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "literal_id_label");
    const ui = try readFixture(io, testing.allocator, "literal-ids/ui.zig");
    defer testing.allocator.free(ui);
    try write(io, root, "literal_ids/ui.zig", ui);
    var lines: std.ArrayList(u8) = .empty;
    defer lines.deinit(testing.allocator);
    for (id_findings) |one| {
        const text = try std.fmt.allocPrint(testing.allocator, "l10n-check: literal: all: literal_ids/ui.zig:{d}: {s} takes l10n.t as its label; use l10n.id so the ImGui ID does not depend on the language\n", .{ one.line, one.call });
        defer testing.allocator.free(text);
        try lines.appendSlice(testing.allocator, text);
    }
    const want = try std.fmt.allocPrint(testing.allocator, "{s}" ++ note_fit ++ note_tables ++ note_documents ++ seed_scope_notes ++ "l10n-check: {d} findings, {d} checked\n", .{
        lines.items,
        id_findings.len,
        checked_total + 1,
    });
    defer testing.allocator.free(want);
    try expectRun(io, "literal_id_label", &root, &.{ "--strict-dir", "literal_ids" }, 1, want);
}

// ---- Per-language scope, the whole-database manifest and the glossary-term rule (M003 S01 T01) ----

const gamma_key = "misc/gamma";
const gamma_base = "Data/ELK/game_data_base/misc/gamma.elk";

/// A third original outside the seed scope, hashed in the manifest after `extra` lines.
fn addGamma(io: std.Io, root: std.Io.Dir, extra: []const u8) !void {
    try setBase(io, testing.allocator, root, gamma_base, "Gamma original");
    const bytes = try root.readFileAlloc(io, gamma_base, testing.allocator, .limited(1 << 20));
    defer testing.allocator.free(bytes);
    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(bytes, &digest, .{});
    const hex = std.fmt.bytesToHex(digest, .lower);
    const lines = try std.fmt.allocPrint(testing.allocator, "{s} {s}\r\n{s}", .{ gamma_key, hex, extra });
    defer testing.allocator.free(lines);
    try writeManifest(io, testing.allocator, root, lines);
}

fn addOverlay(io: std.Io, root: std.Io.Dir, code: []const u8, key: []const u8, text: []const u8) !void {
    const base = try std.fmt.allocPrint(testing.allocator, "Localization/game/{s}/game_data_base/{s}", .{ code, key });
    defer testing.allocator.free(base);
    const txt = try std.fmt.allocPrint(testing.allocator, "{s}.txt", .{base});
    defer testing.allocator.free(txt);
    const xml = try std.fmt.allocPrint(testing.allocator, "{s}.xml", .{base});
    defer testing.allocator.free(xml);
    try setBase(io, testing.allocator, root, txt, text);
    try writeState(io, testing.allocator, root, xml, 2);
}

/// The scope notes of the nine languages when `special` has its own and the rest are seed-only.
fn scopeNotes(comptime special: []const u8, comptime note: []const u8) []const u8 {
    comptime var notes: []const u8 = "";
    inline for (codes) |code| {
        notes = notes ++ "l10n-check: scope: " ++ code ++ ": " ++ (if (std.mem.eql(u8, code, special)) note else "seed, 2 keys checked") ++ "\n";
    }
    return notes;
}

fn appendScope(io: std.Io, root: std.Io.Dir, lines: []const u8) !void {
    const old = try root.readFileAlloc(io, "Localization/game/scope.txt", testing.allocator, .limited(1 << 20));
    defer testing.allocator.free(old);
    const joined = try std.fmt.allocPrint(testing.allocator, "{s}{s}", .{ old, lines });
    defer testing.allocator.free(joined);
    try write(io, root, "Localization/game/scope.txt", joined);
}

test "scope_whole_database: german holds every original, the others stay on the seed list" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "scope_whole_database");
    try addGamma(io, root, "");
    try appendScope(io, root, "whole-database de\r\n");
    // gamma is not in the seed list, so only German must translate it; the base counts as one more checked key.
    try addOverlay(io, root, "de", gamma_key, "Gamma uebersetzt");
    const want = comptime note_fit ++ note_tables ++ note_documents ++ note_literals ++ scopeNotes("de", "whole-database, 3 keys checked") ++ "l10n-check: 0 findings, 22 checked\n";
    try expectRun(io, "scope_whole_database", &root, &.{}, 0, want);
}

test "scope_whole_database_missing: german lacks an original that no other language needs" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "scope_whole_database_missing");
    try addGamma(io, root, "");
    try appendScope(io, root, "whole-database de\r\n");
    const want = comptime "l10n-check: missing: de: misc/gamma: no translation\n" ++ "l10n-check: missing-xml: de: misc/gamma: no state file\n" ++
        note_fit ++ note_tables ++ note_documents ++ note_literals ++ scopeNotes("de", "whole-database, 3 keys checked") ++ "l10n-check: 2 findings, 22 checked\n";
    try expectRun(io, "scope_whole_database_missing", &root, &.{}, 1, want);
}

test "scope_whole_database_extra: a german text without an original names the whole-database rule" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "scope_whole_database_extra");
    try appendScope(io, root, "whole-database de\r\n");
    try addOverlay(io, root, "de", "misc/orphan", "Waise");
    const want = comptime "l10n-check: extra: de: misc/orphan: no .elk original in Data/ELK/game_data_base (whole-database scope)\n" ++
        note_fit ++ note_tables ++ note_documents ++ note_literals ++ scopeNotes("de", "whole-database, 2 keys checked") ++ "l10n-check: 1 findings, 20 checked\n";
    try expectRun(io, "scope_whole_database_extra", &root, &.{}, 1, want);
}

test "scope_prefix: a prefix line adds the originals under it for that language only" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "scope_prefix");
    try addGamma(io, root, "");
    try appendScope(io, root, "prefix es misc/\r\nprefix es nothing/under/this\r\n");
    try addOverlay(io, root, "es", gamma_key, "Gamma traducido");
    const want = comptime note_fit ++ note_tables ++ note_documents ++ note_literals ++ scopeNotes("es", "seed + prefixes misc/, nothing/under/this, 3 keys checked") ++ "l10n-check: 0 findings, 22 checked\n";
    try expectRun(io, "scope_prefix", &root, &.{}, 0, want);
}

test "scope_prefix_extra: a text outside the seed list and the prefixes" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "scope_prefix_extra");
    try addGamma(io, root, "");
    try appendScope(io, root, "prefix es misc/\r\n");
    try addOverlay(io, root, "es", gamma_key, "Gamma traducido");
    try addOverlay(io, root, "es", "other/key", "Otro");
    const want = comptime "l10n-check: extra: es: other/key: not in the seed list or under a `prefix` line of this language in Localization/game/scope.txt\n" ++
        note_fit ++ note_tables ++ note_documents ++ note_literals ++ scopeNotes("es", "seed + prefixes misc/, 3 keys checked") ++ "l10n-check: 1 findings, 22 checked\n";
    try expectRun(io, "scope_prefix_extra", &root, &.{}, 1, want);
}

test "scope_lines_invalid: unknown code, malformed prefix, and a prefix beside whole-database" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "scope_lines_invalid");
    try appendScope(io, root, "whole-database xx\r\nprefix de\r\nwhole-database\r\nwhole-database de\r\nprefix de misc/\r\n");
    const want = comptime "l10n-check: scope: all: Localization/game/scope.txt: line 4: unknown language code `xx`\n" ++
        "l10n-check: scope: all: Localization/game/scope.txt: line 5: prefix wants a language code and a key prefix\n" ++
        "l10n-check: scope: all: Localization/game/scope.txt: line 6: directive without arguments\n" ++
        "l10n-check: scope: de: Localization/game/scope.txt: line 8: prefix `misc/` is redundant, de is whole-database\n" ++
        note_fit ++ note_tables ++ note_documents ++ note_literals ++ scopeNotes("de", "whole-database, 2 keys checked") ++ "l10n-check: 4 findings, 20 checked\n";
    try expectRun(io, "scope_lines_invalid", &root, &.{}, 1, want);
}

test "base_hashes_all_originals: update-base hashes every original, scope or not" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "base_hashes_all");
    try addGamma(io, root, "same-as-base de textes/strings/alpha\r\n");
    // A manifest that lacks gamma and a stale line for a key that vanished.
    try write(io, root, "Localization/game/base-hashes.txt", "old/key 00\r\nsame-as-base de textes/strings/alpha\r\n");
    root.close(io);
    const outcome = try runCheckerWith(io, testing.allocator, "base_hashes_all", &.{"--update-base"});
    defer testing.allocator.free(outcome.stderr);
    try testing.expectEqual(@as(u8, 0), outcome.code);
    try testing.expectEqualStrings("l10n-check: wrote Localization/game/base-hashes.txt for 3 keys\n", outcome.stderr);
    var after = try std.Io.Dir.cwd().openDir(io, scratch ++ "/base_hashes_all", .{});
    defer after.close(io);
    const manifest = try after.readFileAlloc(io, "Localization/game/base-hashes.txt", testing.allocator, .limited(1 << 20));
    defer testing.allocator.free(manifest);
    try testing.expect(std.mem.indexOf(u8, manifest, "\r\nmisc/gamma ") != null);
    try testing.expect(std.mem.indexOf(u8, manifest, "\r\nscenarios/demo/beta ") != null);
    try testing.expect(std.mem.indexOf(u8, manifest, "\r\ntextes/strings/alpha ") != null);
    try testing.expect(std.mem.indexOf(u8, manifest, "old/key") == null);
    try testing.expect(std.mem.endsWith(u8, manifest, "\r\nsame-as-base de textes/strings/alpha\r\n"));
}

/// A glossary for `code` with the given table rows (whole lines).
fn setGlossary(io: std.Io, root: std.Io.Dir, code: []const u8, rows: []const u8) !void {
    const path = try std.fmt.allocPrint(testing.allocator, "Localization/glossary/{s}.md", .{code});
    defer testing.allocator.free(path);
    const text = try std.fmt.allocPrint(testing.allocator, "# Glossary: fixture\r\n\r\n## Interface\r\n\r\n| English term | Translation | Note |\r\n|---|---|---|\r\n{s}", .{rows});
    defer testing.allocator.free(text);
    try write(io, root, path, text);
}

const de_term_finding = "l10n-check: glossary-term: de: textes/strings/alpha: English \"alpha\" needs \"Alfa\" in the translation (Localization/glossary/de.md); use it, or add `de textes/strings/alpha alpha # reason` to Localization/game/term-exceptions.txt\n";

test "glossary_term_missing: an English term whose translation is absent" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "glossary_term_missing");
    try setGlossary(io, root, "de", "| alpha | Alfa |  |\r\n| gamma | Gamma |  |\r\n");
    // gamma occurs in neither English text, so only alpha is required: the German text says Hello.
    const want = comptime de_term_finding ++ all_inactive ++ "l10n-check: 1 findings, 20 checked\n";
    try expectRun(io, "glossary_term_missing", &root, &.{}, 1, want);
}

test "glossary_term_present: any case, inside a longer word of the translation, passes" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "glossary_term_present");
    try setGlossary(io, root, "de", "| Alpha | Alfa |  |\r\n");
    try addOverlay(io, root, "de", "textes/strings/alpha", "Das ALFABET");
    try expectRun(io, "glossary_term_present", &root, &.{}, 0, all_inactive ++ "l10n-check: 0 findings, 20 checked\n");
}

test "glossary_term_whole_word: alpha in alphabet and a multi-word term over a line break" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "glossary_term_whole_word");
    // The German texts say Hello, so any match of the English text would be a finding.
    try setBase(io, testing.allocator, root, alpha_base, "The alphabet");
    try setBase(io, testing.allocator, root, beta_base, "Anti-tank\r\n  gun here");
    try writeManifest(io, testing.allocator, root, "");
    try setGlossary(io, root, "de", "| alpha | Alfa |  |\r\n| anti-tank gun | Pak |  |\r\n");
    const want = comptime "l10n-check: glossary-term: de: scenarios/demo/beta: English \"anti-tank gun\" needs \"Pak\" in the translation (Localization/glossary/de.md); use it, or add `de scenarios/demo/beta anti-tank gun # reason` to Localization/game/term-exceptions.txt\n" ++
        all_inactive ++ "l10n-check: 1 findings, 20 checked\n";
    try expectRun(io, "glossary_term_whole_word", &root, &.{}, 1, want);
}

test "glossary_term_no_check: a row marked no-term-check is skipped" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "glossary_term_no_check");
    try setGlossary(io, root, "de", "| alpha | Alfa | ordinary English, no-term-check |\r\n");
    try expectRun(io, "glossary_term_no_check", &root, &.{}, 0, all_inactive ++ "l10n-check: 0 findings, 20 checked\n");
}

test "glossary_term_exception: an exception with a reason excuses the term" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "glossary_term_exception");
    try setGlossary(io, root, "de", "| alpha | Alfa |  |\r\n");
    try write(io, root, "Localization/game/term-exceptions.txt", "# fixture\r\nde textes/strings/alpha alpha # the name stays Latin here\r\n");
    try expectRun(io, "glossary_term_exception", &root, &.{}, 0, all_inactive ++ "l10n-check: 0 findings, 20 checked\n");
}

test "term_exception_unused: an exception for a term the translation now holds" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "term_exception_unused");
    try setGlossary(io, root, "de", "| alpha | Alfa |  |\r\n");
    try addOverlay(io, root, "de", "textes/strings/alpha", "Alfa");
    try write(io, root, "Localization/game/term-exceptions.txt", "de textes/strings/alpha alpha # was needed once\r\n");
    const want = comptime "l10n-check: term-exception-unused: de: Localization/game/term-exceptions.txt:1: `textes/strings/alpha alpha` matches no missing glossary term; remove the line\n" ++
        all_inactive ++ "l10n-check: 1 findings, 20 checked\n";
    try expectRun(io, "term_exception_unused", &root, &.{}, 1, want);
}

test "term_exception_format: a line without a reason" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "term_exception_format");
    try write(io, root, "Localization/game/term-exceptions.txt", "de textes/strings/alpha alpha\r\n");
    const want = comptime "l10n-check: term-exception: all: Localization/game/term-exceptions.txt:1: an exception needs a reason after `#`\n" ++
        all_inactive ++ "l10n-check: 1 findings, 20 checked\n";
    try expectRun(io, "term_exception_format", &root, &.{}, 1, want);
}

test "glossary_missing: a language without its glossary" {
    const io = testing.io;
    var root = try buildTree(io, testing.allocator, "glossary_missing");
    try root.deleteFile(io, "Localization/glossary/fr.md");
    const want = comptime "l10n-check: glossary-term: fr: Localization/glossary/fr.md: glossary file is missing\n" ++ all_inactive ++ "l10n-check: 1 findings, 20 checked\n";
    try expectRun(io, "glossary_missing", &root, &.{}, 1, want);
}

// The fit rule (M003 D097): the fixture draws `textes/strings/alpha` in a 100 x 21 box of the medium font,
// where "Hello Hello" (81 px) fits one line and "Hello Hello Hello" (124 px) takes two.
const fit_layout = "<?xml version=\"1.0\"?>\r\n<base><Children>\r\n" ++
    "<item ClassTypeID=\"0x10001105\" ElementID=\"100\"><WindowSize x=\"100\" y=\"21\"/></item>\r\n" ++
    "</Children></base>\r\n";
const fit_box_line = "box textes/strings/alpha Data/UI/Fit.xml 100\r\n";
const fit_rest_line = "area scenarios/** 100x21 medium\r\n";
const fit_alpha_where = "Data/UI/Fit.xml#100";

/// The fit notes of nine languages when each Latin one measured `boxed` boxed, `tooltips` tooltip and
/// `scrolls` scroll keys and `base` of them have an English original that overflows.
fn fitNotes(comptime boxed: usize, comptime tooltips: usize, comptime scrolls: usize, comptime base: usize) []const u8 {
    comptime var notes: []const u8 = "";
    inline for (codes) |code| {
        if (comptime std.mem.eql(u8, code, "ru") or std.mem.eql(u8, code, "zh-Hans") or std.mem.eql(u8, code, "ja") or std.mem.eql(u8, code, "hi")) {
            notes = notes ++ "l10n-check: fit: " ++ code ++ ": not measured (font built by language-mods)\n";
        } else {
            notes = notes ++ std.fmt.comptimePrint("l10n-check: fit: {s}: {d} boxed, {d} tooltip, {d} scroll keys measured, {d} with an English original that overflows (fit-base)\n", .{ code, boxed, tooltips, scrolls, base });
        }
    }
    return notes;
}

/// A tree with the fit rule on: the three shipped fonts, one layout and the given map.
fn buildFitTree(io: std.Io, case: []const u8, map: []const u8) !std.Io.Dir {
    var root = try buildTree(io, testing.allocator, case);
    errdefer root.close(io);
    for ([_][]const u8{ "small", "medium", "large" }) |size| {
        const path = try std.fmt.allocPrint(testing.allocator, "Data/Fonts/{s}/1.tfd", .{size});
        defer testing.allocator.free(path);
        const bytes = try std.Io.Dir.cwd().readFileAlloc(io, path, testing.allocator, .limited(1 << 20));
        defer testing.allocator.free(bytes);
        try write(io, root, path, bytes);
    }
    try write(io, root, "Data/UI/Fit.xml", fit_layout);
    try write(io, root, "Localization/game/fit-map.txt", map);
    return root;
}

/// The run's findings, then the notes of a fit-active tree.
fn fitWant(comptime findings: []const u8, comptime count: usize, comptime notes: []const u8) []const u8 {
    return findings ++ inactive_but_fit ++ seed_scope_notes ++ notes ++ std.fmt.comptimePrint("l10n-check: {d} findings, {d} checked\n", .{ count, checked_total });
}

test "fit_pass: texts that fit their boxes pass and the notes count the keys" {
    const io = testing.io;
    var root = try buildFitTree(io, "fit_pass", "# fixture\r\n" ++ fit_box_line ++ "scroll scenarios/** # prose\r\n");
    try addOverlay(io, root, "de", "textes/strings/alpha", "Hello Hello");
    try expectRun(io, "fit_pass", &root, &.{}, 0, comptime fitWant("", 0, fitNotes(1, 0, 1, 0)));
}

test "fit_overflow: a text with more lines than the box has room for" {
    const io = testing.io;
    var root = try buildFitTree(io, "fit_overflow", fit_box_line ++ fit_rest_line);
    try addOverlay(io, root, "de", "textes/strings/alpha", "Hello Hello Hello");
    const want = comptime fitWant("l10n-check: fit: de: textes/strings/alpha: 2 lines x 21 - 1 > 21 in " ++ fit_alpha_where ++ "\n", 1, fitNotes(2, 0, 0, 0));
    try expectRun(io, "fit_overflow", &root, &.{}, 1, want);
}

test "fit_word: a word wider than the box is split, which costs lines as well" {
    const io = testing.io;
    var root = try buildFitTree(io, "fit_word", fit_box_line ++ fit_rest_line);
    try addOverlay(io, root, "de", "textes/strings/alpha", "Supercalifragilisticexpialidocious");
    const want = comptime fitWant("l10n-check: fit: de: textes/strings/alpha: 3 lines x 21 - 1 > 21 in " ++ fit_alpha_where ++ "\n" ++
        "l10n-check: fit-word: de: textes/strings/alpha: \"Supercalifragilisticexpialidocious\" is 230 px wide, the room is 100 px in " ++ fit_alpha_where ++ "\n", 2, fitNotes(2, 0, 0, 0));
    try expectRun(io, "fit_word", &root, &.{}, 1, want);
}

test "fit_tooltip_word: a tooltip has no height limit but a word wider than 585 px" {
    const io = testing.io;
    var root = try buildFitTree(io, "fit_tooltip_word", "tooltip textes/strings/alpha\r\n" ++ fit_rest_line);
    try addOverlay(io, root, "de", "textes/strings/alpha", "Hello Hello Hello Hello Hello Hello Hello Hello Hello Hello Hello Hello Hello Hello Hello Hello");
    try addOverlay(io, root, "fr", "textes/strings/alpha", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    const want = comptime fitWant("l10n-check: fit-word: fr: textes/strings/alpha: \"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\" is 720 px wide, the room is 585 px in tooltip\n", 1, fitNotes(1, 1, 0, 0));
    try expectRun(io, "fit_tooltip_word", &root, &.{}, 1, want);
}

test "fit_unmapped: a key in scope that no line of the map matches" {
    const io = testing.io;
    var root = try buildFitTree(io, "fit_unmapped", fit_box_line);
    try addOverlay(io, root, "de", "textes/strings/alpha", "Hello");
    const want = comptime fitWant("l10n-check: fit-unmapped: all: scenarios/demo/beta: no line of Localization/game/fit-map.txt matches this key; add an exact line or a family\n", 1, fitNotes(1, 0, 0, 0));
    try expectRun(io, "fit_unmapped", &root, &.{}, 1, want);
}

test "fit_scroll_reason: a scroll line without a reason is a finding" {
    const io = testing.io;
    var root = try buildFitTree(io, "fit_scroll_reason", fit_box_line ++ "scroll scenarios/demo/beta\r\n" ++ fit_rest_line);
    const want = comptime fitWant("l10n-check: fit-map: all: Localization/game/fit-map.txt:2: scroll `scenarios/demo/beta` gives no reason; write `scroll scenarios/demo/beta # why this text scrolls`\n", 1, fitNotes(2, 0, 0, 0));
    try expectRun(io, "fit_scroll_reason", &root, &.{}, 1, want);
}

test "fit_base: an English original that already overflows is information, not a finding" {
    const io = testing.io;
    var root = try buildFitTree(io, "fit_base", fit_box_line ++ fit_rest_line);
    try setBase(io, testing.allocator, root, alpha_base, "Hello Hello Hello");
    try writeManifest(io, testing.allocator, root, "");
    // German needs the same two lines as the original: no worse than 2003. French needs three.
    try addOverlay(io, root, "de", "textes/strings/alpha", "Hallo Hallo Hallo");
    try addOverlay(io, root, "fr", "textes/strings/alpha", "Bonjour Bonjour Bonjour");
    const want = comptime fitWant("l10n-check: fit: fr: textes/strings/alpha: 3 lines x 21 - 1 > 21 in " ++ fit_alpha_where ++ "\n", 1, fitNotes(2, 0, 0, 1));
    try expectRun(io, "fit_base", &root, &.{}, 1, want);
}

test "fit_font_missing: the fonts the rule measures with are gone" {
    const io = testing.io;
    var root = try buildFitTree(io, "fit_font_missing", fit_box_line ++ fit_rest_line);
    try root.deleteFile(io, "Data/Fonts/large/1.tfd");
    const want = comptime "l10n-check: fit-font: all: Data/Fonts/large/1.tfd: cannot read the font the fit rule measures with (file is missing)\n" ++ inactive_but_fit ++ seed_scope_notes ++ "l10n-check: 1 findings, " ++ std.fmt.comptimePrint("{d}", .{checked_total}) ++ " checked\n";
    try expectRun(io, "fit_font_missing", &root, &.{}, 1, want);
}

test "fit_map_layout: a box line whose element is not in its layout" {
    const io = testing.io;
    var root = try buildFitTree(io, "fit_map_layout", "box textes/strings/alpha Data/UI/Fit.xml 999\r\nbox textes/strings/alpha Data/UI/Nope.xml 1\r\n" ++ fit_rest_line);
    const want = comptime fitWant("l10n-check: fit-map: all: Localization/game/fit-map.txt:1: Data/UI/Fit.xml holds no element 999\n" ++
        "l10n-check: fit-map: all: Localization/game/fit-map.txt:2: layout Data/UI/Nope.xml does not exist\n", 2, fitNotes(1, 0, 0, 0));
    try expectRun(io, "fit_map_layout", &root, &.{}, 1, want);
}
