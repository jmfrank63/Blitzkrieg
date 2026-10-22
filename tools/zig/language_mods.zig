//! The language mods (M002 D060, D068; texts only since M004 S05): `lang-ru`, `lang-zh-hans`,
//! `lang-ja`, `lang-hi`, `lang-es`, `lang-fr`, `lang-de`, `lang-pt-br` and `lang-id` of the installed
//! game, built by ELK's own pak export from the tracked `Data/ELK` and the overlays of `Localization/`.
//! A mod ships texts and a `mod.xml`, nothing else: the Game draws with bk_text, which shapes the plain
//! Unicode itself and chains the shipped faces, so there is no bk-font stage, no atlas, no Private Use
//! Area table and no Hindi shaper. (A mod that does ship `fonts/<size>/1.tfd` still draws that size with
//! its bitmap font; no language mod does.)
//! Per language: the database is copied to a scratch folder and the overlay laid over it; the fit rule
//! (D097) measures every text with bk_text in the room the Game draws it in, and a text that does not fit
//! fails the language, except what `Localization/game/fit-known.txt` lists (D108; the same reader as
//! l10n-check, so the two gates agree and `fit-known-unused` is reported here too); the export then
//! carries the texts into `mods/lang-<code>/data/texts.pak` with a `mod.xml`, and the pak is read back
//! with the engine's own reader: the overlay's text unchanged, no Private Use Area code point in any
//! text, no font in the archive. Data/ELK is hashed before and after: nothing writes it.
//! BK_LM_GAME names the installed Game executable; `zig build language-mods` sets it. BK_LM_LANGS
//! (`-Dlm-langs`, comma separated codes) limits a shard to some of the languages; the default is all of
//! them. A language whose overlay is missing fails the run; BK_LM_ALLOW_MISSING=1 alone lets it be left
//! out. BK_LM_OVERLAYS names another overlay root (default `Localization/game`), so a test can take a
//! language away from a copy.
const std = @import("std");
const elk = @import("elk");
const zip = @import("zip");
const database = elk.database;
const packages = elk.packages;
const testing = std.testing;

const scratch = "zig-out/local-test/language-mods";

const l10n_languages = @import("l10n_languages.zig");
const check = @import("l10n_check.zig");
const Language = l10n_languages.Language;
const languages = l10n_languages.languages;

/// The overlay key that must come out of the pak as the overlay says.
const probe_key = "textes/strings/imheader-gbutton-cutscenes";

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

/// Whether BK_LM_LANGS (when set) lists `code`.
fn selected(filter: ?[]const u8, code: []const u8) bool {
    const list = filter orelse return true;
    var codes = std.mem.tokenizeAny(u8, list, ", ");
    while (codes.next()) |one| if (std.ascii.eqlIgnoreCase(one, code)) return true;
    return false;
}

fn required(allocator: std.mem.Allocator, key: []const u8) ![:0]const u8 {
    return (try envValue(allocator, key)) orelse {
        std.debug.print("{s} is not set: run zig build language-mods\n", .{key});
        return error.MissingEnvironment;
    };
}

/// Where the overlays are read from; BK_LM_OVERLAYS replaces it.
var overlay_root: []const u8 = "Localization/game";

/// The stage a failure happened in, named on the failing line.
var stage: []const u8 = "assemble";

test "the language mods are built from the overlays through ELK's export" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const game = try required(allocator, "BK_LM_GAME");
    const game_root = std.fs.path.dirname(game).?;

    const filter = try envValue(allocator, "BK_LM_LANGS");
    if (try envValue(allocator, "BK_LM_OVERLAYS")) |root| overlay_root = root;
    const allow_missing = if (try envValue(allocator, "BK_LM_ALLOW_MISSING")) |value| std.mem.eql(u8, value, "1") else false;
    const elk_before = try treeHash("Data/ELK");
    for (languages) |language| {
        if (!selected(filter, language.code)) continue;
        build(io, allocator, language, game_root, allow_missing) catch |err| {
            std.debug.print("language-mods: {s}: {s} failed: {s} (scratch: {s}/{s})\n", .{ language.code, stage, @errorName(err), scratch, language.code });
            return err;
        };
    }
    // Nothing above writes the database it was built from.
    try testing.expectEqual(elk_before, try treeHash("Data/ELK"));
}

fn build(io: std.Io, allocator: std.mem.Allocator, language: Language, game_root: []const u8, allow_missing: bool) !void {
    const cwd = std.Io.Dir.cwd();
    // A missing overlay is an error: a sparse checkout without Localization/ must stop here, not
    // leave the mod out for a later step to miss. Only BK_LM_ALLOW_MISSING=1 leaves one out.
    const overlay_folder = try std.fs.path.join(allocator, &.{ overlay_root, language.code, "game_data_base" });
    cwd.access(io, overlay_folder, .{}) catch {
        if (allow_missing) {
            std.debug.print("language-mods: {s}: no overlay at {s}, skipped (BK_LM_ALLOW_MISSING)\n", .{ language.code, overlay_folder });
            return;
        }
        std.debug.print("language-mods: {s}: no overlay at {s}; check out Localization/ or pass -Dlm-allow-missing (BK_LM_ALLOW_MISSING=1) for a deliberate subset\n", .{ language.code, overlay_folder });
        return error.OverlayMissing;
    };
    const root = try std.fs.path.join(allocator, &.{ scratch, language.code });
    cwd.deleteTree(io, root) catch {};

    // assemble: Data/ELK copied, the overlay over it.
    const db_folder = try std.fs.path.join(allocator, &.{ root, "db" });
    const elk_xml = try std.fs.path.join(allocator, &.{ db_folder, "elk.xml" });
    {
        stage = "assemble";
        try database.copyTree(io, "Data/ELK/game_data_base", try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
        try cwd.writeFile(io, .{ .sub_path = elk_xml, .data = try cwd.readFileAlloc(io, "Data/ELK/elk.xml", allocator, .limited(1 << 20)) });
        const overlay = try std.fs.path.join(allocator, &.{ overlay_root, language.code, "game_data_base" });
        try database.copyTree(io, overlay, try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
    }
    var db = try database.Database.open(testing.allocator, io, elk_xml);
    defer db.deinit();

    // fit: every text in the room the Game draws it in, measured with the Game's own text engine
    // (bk_text, the shipped faces at the Game's size table; D097). checkFit leaves out what
    // fit-known.txt lists, as l10n-check does, and reports a listed text that fits as fit-known-unused.
    {
        stage = "fit";
        var report = check.Report{ .arena = allocator };
        var repository = try cwd.openDir(io, ".", .{ .iterate = true });
        defer repository.close(io);
        try check.checkFit(io, &report, repository, "vendor/fonts", language.code);
        for (report.scope_notes.items) |note| std.debug.print("{s}\n", .{note});
        for (report.findings.items) |finding| std.debug.print("{s}\n", .{try report.line(finding)});
        if (report.findings.items.len != 0) {
            std.debug.print("language-mods: {s}: {d} fit findings\n", .{ language.code, report.findings.items.len });
            return error.TextDoesNotFit;
        }
    }

    // export and install: the texts into the mod of the install.
    const mod_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", language.mod });
    cwd.deleteTree(io, mod_folder) catch {};
    var pak_path: []u8 = undefined;
    {
        stage = "install";
        const written = try packages.exportTo(testing.allocator, io, &db, 0, game_root, .{ .mod = language.mod }, .{});
        defer testing.allocator.free(written.path);
        pak_path = try allocator.dupe(u8, written.path);
        const mod_xml = try std.fmt.allocPrint(allocator, "<?xml version=\"1.0\"?>\r\n<base><MODName>{s}</MODName><MODVersion>1.0</MODVersion><MODType>language</MODType><MODDesc></MODDesc></base>\r\n", .{language.name});
        try database.writeFileAtomic(io, testing.allocator, try std.fs.path.join(allocator, &.{ std.fs.path.dirname(written.path).?, "mod.xml" }), mod_xml);
    }

    // The pak read back with the engine's own reader.
    stage = "readback";
    var package = try packages.readArchive(testing.allocator, io, pak_path);
    defer package.deinit();
    var texts: usize = 0;
    for (package.entries) |entry| {
        if (!std.mem.endsWith(u8, entry.name, ".txt")) continue;
        texts += 1;
        var at: usize = 2;
        while (at + 1 < entry.bytes.len) : (at += 2) {
            const unit = std.mem.readInt(u16, entry.bytes[at..][0..2], .little);
            if (unit >= 0xE000 and unit <= 0xF8FF) {
                std.debug.print("language-mods: {s}: {s} holds U+{X:0>4}, a Private Use Area code point\n", .{ language.code, entry.name, unit });
                return error.PrivateUseCharacter;
            }
        }
    }
    // The reader of importable texts skips the fonts, so the archive itself is opened for them.
    const pak_bytes = try cwd.readFileAlloc(io, pak_path, allocator, .limited(256 << 20));
    var archive = try zip.Archive.parse(testing.allocator, pak_bytes);
    defer archive.deinit();
    for (archive.entries) |*entry| {
        if (std.mem.startsWith(u8, entry.name, "fonts/")) {
            std.debug.print("language-mods: {s}: {s} is in {s}; language mods ship texts only\n", .{ language.code, entry.name, pak_path });
            return error.FontInMod;
        }
    }
    const overlay_text = try cwd.readFileAlloc(io, try std.mem.concat(allocator, u8, &.{ overlay_root, "/", language.code, "/game_data_base/", probe_key, ".txt" }), allocator, .limited(1 << 20));
    const packed_text = for (package.entries) |entry| {
        if (std.mem.eql(u8, entry.name, probe_key ++ ".txt")) break entry.bytes;
    } else return error.OverlayTextMissing;
    try testing.expectEqualSlices(u8, overlay_text, packed_text);
    std.debug.print("language-mods: {s}: {d} texts, plain Unicode, no fonts, installed {s}\n", .{ language.code, texts, pak_path });
}

test "BK_LM_LANGS selects languages by code" {
    try testing.expect(selected(null, "ru"));
    try testing.expect(selected("de,ru", "ru"));
    try testing.expect(!selected("de, fr", "ru"));
    try testing.expect(selected("pt-br", "pt-BR"));
}

/// A hash of every file below `path` that does not depend on the walk order.
fn treeHash(path: []const u8) !u64 {
    const io = testing.io;
    var dir = try std.Io.Dir.cwd().openDir(io, path, .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(testing.allocator);
    defer walker.deinit();
    var sum: u64 = 0;
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        const bytes = try dir.readFileAlloc(io, entry.path, testing.allocator, .limited(64 << 20));
        defer testing.allocator.free(bytes);
        sum +%= std.hash.Wyhash.hash(std.hash.Wyhash.hash(0, entry.path), bytes);
    }
    return sum;
}
