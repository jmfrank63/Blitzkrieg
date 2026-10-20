//! The completeness gate of M002 (D063, D076): `zig build l10n-check` reads the tracked tree and
//! fails when a translation is missing, stale or malformed. Every finding is one line on stderr,
//! `l10n-check: <rule>: <code>: <where>: <detail>`, where `<code>` is a language code (or `all`
//! for a finding that belongs to no language) and `<where>` names the ELK key or file. The run
//! ends with `l10n-check: <n> findings, <m> checked`, and one `l10n-check: <scope>: not active`
//! note per scope that has nothing to check yet, so a CI log shows what was and was not checked.
//! Exit status is 1 on any finding, 2 on a bad command line.
//!
//! The rules are `pub` and take a `Report`, so tests can run them on in-memory data and later
//! rules plug in beside `checkOverlays` without touching the format.
//!
//! Scope: `Localization/game/scope.txt` lists the ELK keys (relative to `game_data_base`, no
//! extension) every language overlay must hold. An overlay text is `<key>.txt` (UTF-16LE with a
//! byte order mark) beside its state file `<key>.xml` (`<base State="n" .../>`, 0 not translated,
//! 1 outdated, 2 translated, 3 approved, as Sources/editor/elk/core/state.zig names them).
//!
//! The English source of a key is its `Data/ELK/game_data_base/<key>.elk` (the original; the
//! `.txt` beside it is a translation working copy, used only when no `.elk` exists). An overlay
//! .xml cannot record which English text a translation was made from, so D075 keeps a tracked
//! manifest, `Localization/game/base-hashes.txt`: `<key> <sha256 hex of the English bytes>` per
//! scope key, plus `same-as-base <code> <key>` lines naming translations that may equal the
//! English byte for byte (a proper name). Rule `stale` fires when the English bytes no longer
//! match their hash, `base-hash-missing` when a scope key has no line, `untranslated` when an
//! overlay text equals the English bytes without a `same-as-base` line. `--update-base` rewrites
//! the manifest (`zig build l10n-check-update-base`) after a person reviewed the translations.
//! Some Data/ELK originals are Russian; every comparison is against the base of that key.
//!
//! Rule `placeholder`: the multiset of substituted or interpreted tokens must be the same in the
//! English and the translation. Scanning the 2,852 originals of Data/ELK found no printf code, no
//! `{n}` and no `$name`; the tokens present are hotkey names in angle brackets (`<Shift>`, `<Ctrl-1>`,
//! `<F5>`), square-bracket keys (`[Q]`, `[DISABLED]`) and `#n` counters. The 25 in-scope originals
//! hold none of them, only CRLF line breaks. The rule therefore counts printf codes (`%s`, `%d`,
//! `%5.2f`; `%%` is a literal), `{n}`, `$name`, `<tag>` (no white space inside, at most 24 characters)
//! and a literal `\n`; square-bracket keys are left to the translator, who may write the local key.
//!
//! Rule `glyph` (D063, D068): every code point of an overlay text must be drawable. A language with a
//! face (ru, zh-Hans, ja, hi) needs each code point in its face or in NotoSans-Regular (the Hindi
//! texts hold ASCII and punctuation the Devanagari face lacks), read from the fonts' `cmap`
//! (tools/zig/l10n_cmap.zig); the five Latin languages draw with the game's cp1252 bitmap fonts, so
//! each code point must be in cp1252. CR, LF and TAB are ignored. The tracked Unicode overlay is
//! checked, never a generated pak (the Hindi private-use forms live only there, D058). The fonts are
//! read from `--fonts <dir>`, default `<root>/vendor/fonts`.
const std = @import("std");
const languages = @import("l10n_languages.zig");
const cmap = @import("l10n_cmap.zig");

pub const Finding = struct {
    rule: []const u8,
    code: []const u8,
    where: []const u8,
    detail: []const u8,
};

pub const Report = struct {
    arena: std.mem.Allocator,
    findings: std.ArrayList(Finding) = .empty,
    inactive: std.ArrayList([]const u8) = .empty,
    checked: usize = 0,

    pub fn add(self: *Report, rule: []const u8, code: []const u8, where: []const u8, detail: []const u8) !void {
        try self.findings.append(self.arena, .{
            .rule = rule,
            .code = code,
            .where = try self.arena.dupe(u8, where),
            .detail = try self.arena.dupe(u8, detail),
        });
    }

    pub fn addFmt(self: *Report, rule: []const u8, code: []const u8, where: []const u8, comptime fmt: []const u8, args: anytype) !void {
        try self.add(rule, code, where, try std.fmt.allocPrint(self.arena, fmt, args));
    }

    /// The finding as it is printed, without the line break.
    pub fn line(self: *Report, finding: Finding) ![]u8 {
        return std.fmt.allocPrint(self.arena, "l10n-check: {s}: {s}: {s}: {s}", .{ finding.rule, finding.code, finding.where, finding.detail });
    }
};

/// The state numbers of state.zig.
pub const state_translated = 2;
pub const state_approved = 3;

pub const TextProblem = union(enum) {
    ok,
    no_bom,
    odd_length,
    unpaired_surrogate: usize,
    empty,
};

/// Whether `bytes` is a UTF-16LE text with a byte order mark, an even length, paired surrogates
/// and at least one character that is not white space.
pub fn textProblem(bytes: []const u8) TextProblem {
    if (bytes.len < 2 or bytes[0] != 0xff or bytes[1] != 0xfe) return .no_bom;
    if (bytes.len % 2 != 0) return .odd_length;
    var visible = false;
    var unit_index: usize = 0;
    var at: usize = 2;
    while (at < bytes.len) : (at += 2) {
        const unit = std.mem.readInt(u16, bytes[at..][0..2], .little);
        if (unit >= 0xD800 and unit <= 0xDBFF) {
            const next: ?u16 = if (at + 3 < bytes.len) std.mem.readInt(u16, bytes[at + 2 ..][0..2], .little) else null;
            if (next == null or next.? < 0xDC00 or next.? > 0xDFFF) return .{ .unpaired_surrogate = unit_index };
            at += 2;
            unit_index += 1;
            visible = true;
        } else if (unit >= 0xDC00 and unit <= 0xDFFF) {
            return .{ .unpaired_surrogate = unit_index };
        } else if (unit != ' ' and unit != '\t' and unit != '\r' and unit != '\n' and unit != 0xFEFF) {
            visible = true;
        }
        unit_index += 1;
    }
    return if (visible) .ok else .empty;
}

/// The `State` of a state file, or null when it holds no readable `State="n"`.
pub fn parseState(xml: []const u8) ?u8 {
    const marker = "State=\"";
    const start = (std.mem.indexOf(u8, xml, marker) orelse return null) + marker.len;
    const end = std.mem.indexOfScalarPos(u8, xml, start, '"') orelse return null;
    return std.fmt.parseInt(u8, xml[start..end], 10) catch null;
}

/// The scope keys of a `scope.txt`: one per line, `#` lines and blank lines skipped.
pub fn parseScope(arena: std.mem.Allocator, text: []const u8) ![]const []const u8 {
    var keys: std.ArrayList([]const u8) = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw| {
        const key = std.mem.trim(u8, raw, " \t\r");
        if (key.len == 0 or key[0] == '#') continue;
        try keys.append(arena, key);
    }
    return keys.items;
}

fn readOptional(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, path: []const u8) !?[]u8 {
    return root.readFileAlloc(io, path, arena, .limited(16 << 20)) catch |err| switch (err) {
        error.FileNotFound, error.NotDir => null,
        else => err,
    };
}

/// The spelling of `rel` (forward slashes) that exists below `root`, matching every component
/// case-insensitively when the exact spelling is absent, as the Data tree of a case-sensitive file
/// system needs; null when it does not exist.
pub fn findIgnoreCase(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, rel: []const u8) !?[]const u8 {
    if (root.statFile(io, rel, .{})) |_| return rel else |_| {}
    var current = try root.openDir(io, ".", .{ .iterate = true });
    var names: std.ArrayList([]const u8) = .empty;
    var parts = std.mem.tokenizeScalar(u8, rel, '/');
    while (parts.next()) |part| {
        const last = parts.peek() == null;
        var found: ?[]const u8 = null;
        var it = current.iterate();
        while (try it.next(io)) |entry| {
            if (!std.ascii.eqlIgnoreCase(entry.name, part)) continue;
            if (last != (entry.kind != .directory)) continue;
            found = try arena.dupe(u8, entry.name);
            break;
        }
        const name = found orelse {
            current.close(io);
            return null;
        };
        try names.append(arena, name);
        if (last) {
            current.close(io);
            return try std.mem.join(arena, "/", names.items);
        }
        const next = current.openDir(io, name, .{ .iterate = true }) catch {
            current.close(io);
            return null;
        };
        current.close(io);
        current = next;
    }
    current.close(io);
    return null;
}

pub fn existsIgnoreCase(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, rel: []const u8) !bool {
    return (try findIgnoreCase(io, arena, root, rel)) != null;
}

/// The English bytes of `key`: its `.elk` original, else its `.txt`; null when neither exists.
pub fn readBase(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, key: []const u8) !?[]u8 {
    for ([_][]const u8{ "elk", "txt" }) |ext| {
        const rel = try std.fmt.allocPrint(arena, "Data/ELK/game_data_base/{s}.{s}", .{ key, ext });
        if (try findIgnoreCase(io, arena, root, rel)) |found| return try readOptional(io, arena, root, found);
    }
    return null;
}

pub const manifest_path = "Localization/game/base-hashes.txt";

pub const SameAsBase = struct { code: []const u8, key: []const u8 };

pub const Manifest = struct {
    keys: []const []const u8,
    hashes: []const []const u8,
    same_as_base: []const SameAsBase,

    pub fn hashOf(self: Manifest, key: []const u8) ?[]const u8 {
        for (self.keys, self.hashes) |one, hash| {
            if (std.mem.eql(u8, one, key)) return hash;
        }
        return null;
    }

    pub fn allowsSame(self: Manifest, code: []const u8, key: []const u8) bool {
        for (self.same_as_base) |one| {
            if (std.mem.eql(u8, one.code, code) and std.mem.eql(u8, one.key, key)) return true;
        }
        return false;
    }
};

/// The lines of a `base-hashes.txt`: `<key> <hash>` and `same-as-base <code> <key>`; `#` lines and
/// blank lines are skipped, and a line of any other shape is ignored so the scope check names it.
pub fn parseManifest(arena: std.mem.Allocator, text: []const u8) !Manifest {
    var keys: std.ArrayList([]const u8) = .empty;
    var hashes: std.ArrayList([]const u8) = .empty;
    var same: std.ArrayList(SameAsBase) = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0 or line[0] == '#') continue;
        var fields = std.mem.tokenizeScalar(u8, line, ' ');
        const first = fields.next() orelse continue;
        if (std.mem.eql(u8, first, "same-as-base")) {
            const code = fields.next() orelse continue;
            const key = fields.next() orelse continue;
            try same.append(arena, .{ .code = code, .key = key });
        } else {
            const hash = fields.next() orelse continue;
            try keys.append(arena, first);
            try hashes.append(arena, hash);
        }
    }
    return .{ .keys = keys.items, .hashes = hashes.items, .same_as_base = same.items };
}

pub fn hashHex(bytes: []const u8) [64]u8 {
    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(bytes, &digest, .{});
    return std.fmt.bytesToHex(digest, .lower);
}

fn lessString(_: void, a: []const u8, b: []const u8) bool {
    return std.mem.lessThan(u8, a, b);
}

/// The substituted or interpreted tokens of a UTF-16LE text, sorted (see the module comment for
/// which). A text without the byte order mark has none.
pub fn placeholderTokens(arena: std.mem.Allocator, bytes: []const u8) ![]const []const u8 {
    var tokens: std.ArrayList([]const u8) = .empty;
    if (bytes.len < 2 or bytes[0] != 0xff or bytes[1] != 0xfe) return tokens.items;
    // Tokens are ASCII: every other unit becomes 0x80, which ends any token.
    const count = (bytes.len - 2) / 2;
    const ascii = try arena.alloc(u8, count);
    for (ascii, 0..) |*c, i| {
        const unit = std.mem.readInt(u16, bytes[2 + i * 2 ..][0..2], .little);
        c.* = if (unit < 0x80) @intCast(unit) else 0x80;
    }
    var at: usize = 0;
    while (at < ascii.len) {
        const c = ascii[at];
        var end: usize = at + 1;
        var is_token = false;
        switch (c) {
            '%' => {
                if (at + 1 < ascii.len and ascii[at + 1] == '%') {
                    at += 2;
                    continue;
                }
                var i = at + 1;
                while (i < ascii.len and std.mem.indexOfScalar(u8, "-+ #0", ascii[i]) != null) i += 1;
                while (i < ascii.len and std.ascii.isDigit(ascii[i])) i += 1;
                if (i < ascii.len and ascii[i] == '.') {
                    i += 1;
                    while (i < ascii.len and std.ascii.isDigit(ascii[i])) i += 1;
                }
                if (i < ascii.len and std.ascii.isAlphabetic(ascii[i])) {
                    end = i + 1;
                    is_token = true;
                }
            },
            '{' => {
                var i = at + 1;
                while (i < ascii.len and std.ascii.isDigit(ascii[i])) i += 1;
                if (i > at + 1 and i < ascii.len and ascii[i] == '}') {
                    end = i + 1;
                    is_token = true;
                }
            },
            '$' => {
                var i = at + 1;
                while (i < ascii.len and (std.ascii.isAlphanumeric(ascii[i]) or ascii[i] == '_')) i += 1;
                if (i > at + 1) {
                    end = i;
                    is_token = true;
                }
            },
            '<' => {
                var i = at + 1;
                while (i < ascii.len and i - at <= 24 and ascii[i] != '>' and ascii[i] != '<' and ascii[i] > ' ' and ascii[i] != 0x80) i += 1;
                if (i > at + 1 and i < ascii.len and ascii[i] == '>') {
                    end = i + 1;
                    is_token = true;
                }
            },
            '\\' => {
                if (at + 1 < ascii.len and ascii[at + 1] == 'n') {
                    end = at + 2;
                    is_token = true;
                }
            },
            else => {},
        }
        if (is_token) try tokens.append(arena, ascii[at..end]);
        at = end;
    }
    std.mem.sort([]const u8, tokens.items, {}, lessString);
    return tokens.items;
}

fn tokensEqual(a: []const []const u8, b: []const []const u8) bool {
    if (a.len != b.len) return false;
    for (a, b) |x, y| {
        if (!std.mem.eql(u8, x, y)) return false;
    }
    return true;
}

fn joinTokens(arena: std.mem.Allocator, tokens: []const []const u8) ![]const u8 {
    if (tokens.len == 0) return "none";
    return std.mem.join(arena, " ", tokens);
}

/// Overlay completeness and state (rules missing, missing-xml, state, stale, encoding, empty,
/// extra, base-missing): every language of the table holds every scope key as a text with a
/// Translated or Approved state, and holds nothing else; every scope key exists in the base.
pub fn checkOverlays(io: std.Io, report: *Report, root: std.Io.Dir) !void {
    const arena = report.arena;
    const scope_text = (try readOptional(io, arena, root, "Localization/game/scope.txt")) orelse {
        try report.add("scope", "all", "Localization/game/scope.txt", "file is missing");
        return;
    };
    const keys = try parseScope(arena, scope_text);
    if (keys.len == 0) try report.add("scope", "all", "Localization/game/scope.txt", "lists no key");

    const manifest: ?Manifest = if (try readOptional(io, arena, root, manifest_path)) |text| try parseManifest(arena, text) else null;
    if (manifest == null) try report.add("base-hash-missing", "all", manifest_path, "file is missing; run zig build l10n-check-update-base");

    const bases = try arena.alloc(?[]u8, keys.len);
    for (keys, bases) |key, *base_bytes| {
        report.checked += 1;
        base_bytes.* = try readBase(io, arena, root, key);
        const bytes = base_bytes.* orelse {
            try report.add("base-missing", "all", key, "no original text in Data/ELK/game_data_base");
            continue;
        };
        const m = manifest orelse continue;
        if (m.hashOf(key)) |recorded| {
            const now = hashHex(bytes);
            if (!std.mem.eql(u8, recorded, &now)) {
                try report.add("stale", "all", key, "the English source changed since it was hashed in Localization/game/base-hashes.txt; review the translations, then run zig build l10n-check-update-base");
            }
        } else {
            try report.add("base-hash-missing", "all", key, "no line in Localization/game/base-hashes.txt; run zig build l10n-check-update-base");
        }
    }

    for (languages.languages) |language| {
        const base = try std.fmt.allocPrint(arena, "Localization/game/{s}/game_data_base", .{language.code});
        for (keys, bases) |key, base_bytes| {
            report.checked += 1;
            const txt_path = try std.fmt.allocPrint(arena, "{s}/{s}.txt", .{ base, key });
            const xml_path = try std.fmt.allocPrint(arena, "{s}/{s}.xml", .{ base, key });
            if (try readOptional(io, arena, root, txt_path)) |bytes| {
                switch (textProblem(bytes)) {
                    .ok => if (base_bytes) |english| try checkAgainstBase(report, manifest, language.code, key, english, bytes),
                    .no_bom => try report.add("encoding", language.code, key, "text lacks the UTF-16LE byte order mark"),
                    .odd_length => try report.addFmt("encoding", language.code, key, "text has an odd byte count ({d})", .{bytes.len}),
                    .unpaired_surrogate => |unit| try report.addFmt("encoding", language.code, key, "unpaired surrogate at unit {d}", .{unit}),
                    .empty => try report.add("empty", language.code, key, "text holds no characters"),
                }
            } else {
                try report.add("missing", language.code, key, "no translation");
            }
            if (try readOptional(io, arena, root, xml_path)) |xml| {
                const state = parseState(xml);
                if (state == null) {
                    try report.add("state", language.code, key, "state file has no State attribute");
                } else switch (state.?) {
                    state_translated, state_approved => {},
                    1 => try report.add("stale", language.code, key, "State=1 (outdated): the English text changed after the translation"),
                    0 => try report.add("state", language.code, key, "State=0 (not translated), want 2 or 3"),
                    else => try report.addFmt("state", language.code, key, "State={d} is not a state, want 2 or 3", .{state.?}),
                }
            } else {
                try report.add("missing-xml", language.code, key, "no state file");
            }
        }
        try checkExtra(io, report, root, language.code, base, keys);
    }
}

/// A font read once and its `cmap`, or the reason it cannot be used.
const Face = struct {
    name: []const u8,
    bytes: []u8 = &.{},
    table: ?cmap.Cmap = null,

    fn load(io: std.Io, arena: std.mem.Allocator, fonts: std.Io.Dir, name: []const u8) Face {
        var face: Face = .{ .name = name };
        face.bytes = fonts.readFileAlloc(io, name, arena, .limited(64 << 20)) catch return face;
        face.table = cmap.Cmap.init(face.bytes) catch null;
        return face;
    }

    fn covers(self: Face, code_point: u21) bool {
        const table = self.table orelse return false;
        return table.covers(code_point) catch false;
    }
};

fn lessCodePoint(_: void, a: u21, b: u21) bool {
    return a < b;
}

/// The distinct code points of a UTF-16LE text (after its byte order mark, surrogates combined),
/// ascending, without CR, LF and TAB. An unpaired surrogate is left to the encoding rule.
pub fn codePoints(arena: std.mem.Allocator, bytes: []const u8) ![]const u21 {
    var found: std.ArrayList(u21) = .empty;
    var at: usize = 2;
    while (at + 1 < bytes.len) : (at += 2) {
        var unit: u21 = std.mem.readInt(u16, bytes[at..][0..2], .little);
        if (unit >= 0xD800 and unit <= 0xDBFF) {
            if (at + 3 >= bytes.len) break;
            const low = std.mem.readInt(u16, bytes[at + 2 ..][0..2], .little);
            if (low < 0xDC00 or low > 0xDFFF) continue;
            unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
            at += 2;
        } else if (unit >= 0xDC00 and unit <= 0xDFFF) continue;
        if (unit == '\r' or unit == '\n' or unit == '\t') continue;
        if (std.mem.indexOfScalar(u21, found.items, unit) == null) try found.append(arena, unit);
    }
    std.mem.sort(u21, found.items, {}, lessCodePoint);
    return found.items;
}

/// Rule glyph: every code point of every in-scope overlay text is covered by the language's font
/// (or NotoSans-Regular), or by cp1252 for a language without a face. One finding per code point.
pub fn checkGlyphs(io: std.Io, report: *Report, root: std.Io.Dir, fonts: std.Io.Dir) !void {
    const arena = report.arena;
    const scope_text = (try readOptional(io, arena, root, "Localization/game/scope.txt")) orelse return;
    const keys = try parseScope(arena, scope_text);
    const latin = Face.load(io, arena, fonts, "NotoSans-Regular.ttf");
    if (latin.table == null) try report.add("glyph", "all", latin.name, "font is missing or has no usable cmap");
    for (languages.languages) |language| {
        var own: ?Face = null;
        if (language.face) |name| {
            own = Face.load(io, arena, fonts, name);
            if (own.?.table == null) try report.add("glyph", language.code, name, "font is missing or has no usable cmap");
        }
        for (keys) |key| {
            const path = try std.fmt.allocPrint(arena, "Localization/game/{s}/game_data_base/{s}.txt", .{ language.code, key });
            const bytes = (try readOptional(io, arena, root, path)) orelse continue;
            if (textProblem(bytes) != .ok) continue;
            for (try codePoints(arena, bytes)) |code_point| {
                if (own) |face| {
                    if (face.table == null or face.covers(code_point) or latin.covers(code_point)) continue;
                    try report.addFmt("glyph", language.code, key, "U+{X:0>4} not in {s}", .{ code_point, face.name });
                } else if (code_point > 0xFFFF or !languages.inCp1252(@intCast(code_point))) {
                    try report.addFmt("glyph", language.code, key, "U+{X:0>4} not in cp1252", .{code_point});
                }
            }
        }
    }
}

/// Rules untranslated and placeholder for one overlay text against the English bytes of its key.
fn checkAgainstBase(report: *Report, manifest: ?Manifest, code: []const u8, key: []const u8, english: []const u8, translation: []const u8) !void {
    const arena = report.arena;
    if (std.mem.eql(u8, english, translation)) {
        const allowed = if (manifest) |m| m.allowsSame(code, key) else false;
        if (!allowed) try report.addFmt("untranslated", code, key, "text equals the English original; translate it or add `same-as-base {s} {s}` to {s}", .{ code, key, manifest_path });
    }
    const want = try placeholderTokens(arena, english);
    const have = try placeholderTokens(arena, translation);
    if (!tokensEqual(want, have)) {
        try report.addFmt("placeholder", code, key, "placeholders differ: English has {s}, translation has {s}", .{ try joinTokens(arena, want), try joinTokens(arena, have) });
    }
}

/// Rewrites `Localization/game/base-hashes.txt` from the English bytes of every scope key, keeping
/// the `same-as-base` lines. A scope key without an original is a finding and no file is written.
pub fn updateBase(io: std.Io, report: *Report, root: std.Io.Dir) !void {
    const arena = report.arena;
    const scope_text = (try readOptional(io, arena, root, "Localization/game/scope.txt")) orelse {
        try report.add("scope", "all", "Localization/game/scope.txt", "file is missing");
        return;
    };
    const keys = try arena.dupe([]const u8, try parseScope(arena, scope_text));
    std.mem.sort([]const u8, keys, {}, lessString);
    const old: Manifest = if (try readOptional(io, arena, root, manifest_path)) |text| try parseManifest(arena, text) else .{ .keys = &.{}, .hashes = &.{}, .same_as_base = &.{} };

    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(arena, manifest_header);
    for (keys) |key| {
        report.checked += 1;
        const bytes = (try readBase(io, arena, root, key)) orelse {
            try report.add("base-missing", "all", key, "no original text in Data/ELK/game_data_base");
            continue;
        };
        const hash = hashHex(bytes);
        try out.appendSlice(arena, try std.fmt.allocPrint(arena, "{s} {s}\r\n", .{ key, hash }));
    }
    if (report.findings.items.len != 0) return;
    const same = try arena.dupe(SameAsBase, old.same_as_base);
    std.mem.sort(SameAsBase, same, {}, lessSame);
    for (same) |one| try out.appendSlice(arena, try std.fmt.allocPrint(arena, "same-as-base {s} {s}\r\n", .{ one.code, one.key }));
    try root.writeFile(io, .{ .sub_path = manifest_path, .data = out.items });
}

fn lessSame(_: void, a: SameAsBase, b: SameAsBase) bool {
    if (!std.mem.eql(u8, a.code, b.code)) return std.mem.lessThan(u8, a.code, b.code);
    return std.mem.lessThan(u8, a.key, b.key);
}

const manifest_header =
    "# Base-text hashes of the l10n-check gate (M002 D075), one `<key> <sha256 hex>` line per scope key.\r\n" ++
    "# The hash is of the English bytes in Data/ELK/game_data_base (the .elk original, else the .txt).\r\n" ++
    "# l10n-check reports `stale` when the English text no longer matches its hash: review the\r\n" ++
    "# translations of that key in every language, then rewrite this file with\r\n" ++
    "#   zig build l10n-check-update-base\r\n" ++
    "# A `same-as-base <code> <key>` line lets that translation equal the English (a proper name);\r\n" ++
    "# the update keeps those lines.\r\n";

fn checkExtra(io: std.Io, report: *Report, root: std.Io.Dir, code: []const u8, base: []const u8, keys: []const []const u8) !void {
    const arena = report.arena;
    var dir = root.openDir(io, base, .{ .iterate = true }) catch return;
    defer dir.close(io);
    var walker = try dir.walk(arena);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file or !std.mem.endsWith(u8, entry.path, ".txt")) continue;
        const key = try arena.dupe(u8, entry.path[0 .. entry.path.len - ".txt".len]);
        for (key) |*c| {
            if (c.* == '\\') c.* = '/';
        }
        var in_scope = false;
        for (keys) |one| {
            if (std.mem.eql(u8, one, key)) {
                in_scope = true;
                break;
            }
        }
        if (!in_scope) try report.add("extra", code, key, "not listed in Localization/game/scope.txt");
    }
}

fn usage() noreturn {
    std.debug.print("usage: l10n_check [--root <dir>] [--fonts <dir>] [--update-base]\n", .{});
    std.process.exit(2);
}

pub fn main(init: std.process.Init) !void {
    var arena_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = init.io;

    var iterator = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    defer iterator.deinit();
    _ = iterator.skip();
    var root_path: []const u8 = ".";
    var fonts_path: ?[]const u8 = null;
    var update_base = false;
    while (iterator.next()) |arg| {
        if (std.mem.eql(u8, arg, "--root")) {
            root_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--fonts")) {
            fonts_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--update-base")) {
            update_base = true;
        } else usage();
    }

    var root = std.Io.Dir.cwd().openDir(io, root_path, .{ .iterate = true }) catch |err| {
        std.debug.print("l10n-check: root: all: {s}: cannot open ({s})\n", .{ root_path, @errorName(err) });
        std.process.exit(1);
    };
    defer root.close(io);

    var report: Report = .{ .arena = arena };
    if (update_base) {
        try updateBase(io, &report, root);
        for (report.findings.items) |finding| std.debug.print("{s}\n", .{try report.line(finding)});
        if (report.findings.items.len != 0) std.process.exit(1);
        std.debug.print("l10n-check: wrote {s} for {d} keys\n", .{ manifest_path, report.checked });
        return;
    }
    try checkOverlays(io, &report, root);
    var fonts = if (fonts_path) |path| std.Io.Dir.cwd().openDir(io, path, .{}) else root.openDir(io, "vendor/fonts", .{});
    if (fonts) |*dir| {
        defer dir.close(io);
        try checkGlyphs(io, &report, root, dir.*);
    } else |err| try report.addFmt("glyph", "all", fonts_path orelse "vendor/fonts", "cannot open the fonts directory ({s})", .{@errorName(err)});

    for (report.findings.items) |finding| std.debug.print("{s}\n", .{try report.line(finding)});
    for (report.inactive.items) |scope| std.debug.print("l10n-check: {s}: not active\n", .{scope});
    std.debug.print("l10n-check: {d} findings, {d} checked\n", .{ report.findings.items.len, report.checked });
    if (report.findings.items.len != 0) std.process.exit(1);
}

test {
    _ = cmap;
}

test "textProblem tells the byte order mark, length, surrogates and empty text apart" {
    const testing = std.testing;
    try testing.expectEqual(TextProblem.ok, textProblem(&.{ 0xff, 0xfe, 'A', 0 }));
    try testing.expectEqual(TextProblem.no_bom, textProblem(&.{ 'A', 0 }));
    try testing.expectEqual(TextProblem.odd_length, textProblem(&.{ 0xff, 0xfe, 'A', 0, 'B' }));
    try testing.expectEqual(TextProblem.empty, textProblem(&.{ 0xff, 0xfe, ' ', 0, '\r', 0 }));
    try testing.expectEqual(TextProblem{ .unpaired_surrogate = 1 }, textProblem(&.{ 0xff, 0xfe, 'A', 0, 0x3d, 0xd8 }));
    try testing.expectEqual(TextProblem.ok, textProblem(&.{ 0xff, 0xfe, 0x3d, 0xd8, 0x00, 0xde }));
}

test "parseState reads the State attribute" {
    const testing = std.testing;
    try testing.expectEqual(@as(?u8, 3), parseState("<?xml version=\"1.0\"?>\r\n<base State=\"3\" Changed=\"1\"/>\r\n"));
    try testing.expectEqual(@as(?u8, null), parseState("<base/>"));
}

test "placeholderTokens finds printf codes, numbered slots, tags and a literal backslash n" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const text = "\xff\xfe" ++ "A %s and 100%% %5.2f {0} $name <br> <Ctrl-1> a < b \\n [Q]";
    var bytes: std.ArrayList(u8) = .empty;
    try bytes.appendSlice(arena, "\xff\xfe");
    for (text[2..]) |c| try bytes.appendSlice(arena, &.{ c, 0 });
    const tokens = try placeholderTokens(arena, bytes.items);
    const want = [_][]const u8{ "$name", "%5.2f", "%s", "<Ctrl-1>", "<br>", "\\n", "{0}" };
    try testing.expectEqual(want.len, tokens.len);
    for (want, tokens) |w, t| try testing.expectEqualStrings(w, t);
}

test "codePoints combines surrogates, drops CR LF TAB and repeats" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const points = try codePoints(arena_state.allocator(), &.{ 0xff, 0xfe, 'b', 0, '\r', 0, '\n', 0, 'a', 0, 'b', 0, 0x3d, 0xd8, 0x00, 0xde });
    try testing.expectEqualSlices(u21, &.{ 'a', 'b', 0x1F600 }, points);
}

test "parseManifest reads hashes and same-as-base lines" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const manifest = try parseManifest(arena_state.allocator(), "# c\r\nsome/key abc\r\n\r\nsame-as-base de some/key\r\n");
    try testing.expectEqualStrings("abc", manifest.hashOf("some/key").?);
    try testing.expect(manifest.allowsSame("de", "some/key"));
    try testing.expect(!manifest.allowsSame("fr", "some/key"));
}
