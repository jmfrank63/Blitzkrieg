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
    var map = try testing.environ.createMap(allocator);
    defer map.deinit();
    const exe = map.get("BK_L10N_CHECK_EXE") orelse {
        std.debug.print("BK_L10N_CHECK_EXE is not set: run zig build test-l10n-check\n", .{});
        return error.MissingEnvironment;
    };
    const root = try std.fmt.allocPrint(allocator, "{s}/{s}", .{ scratch, case });
    defer allocator.free(root);
    const result = try std.process.run(allocator, io, .{ .argv = &.{ exe, "--root", root } });
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
    const expected = try std.fmt.allocPrint(testing.allocator, "{s}\nl10n-check: 1 findings, {d} checked\n", .{ line, checked_total });
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
    const expected = try std.fmt.allocPrint(testing.allocator, "l10n-check: 0 findings, {d} checked\n", .{checked_total});
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
    try expectFinding(io, "extra_key", &root, "l10n-check: extra: de: textes/strings/zzz: not listed in Localization/game/scope.txt");
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
    const expected = try std.fmt.allocPrint(testing.allocator, "{s}\n{s}\nl10n-check: 2 findings, {d} checked\n", .{
        "l10n-check: base-hash-missing: all: textes/strings/alpha: no line in Localization/game/base-hashes.txt; run zig build l10n-check-update-base",
        "l10n-check: stale: all: scenarios/demo/beta: the English source changed since it was hashed in Localization/game/base-hashes.txt; review the translations, then run zig build l10n-check-update-base",
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
    const expected = try std.fmt.allocPrint(testing.allocator, "l10n-check: 0 findings, {d} checked\n", .{checked_total});
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
