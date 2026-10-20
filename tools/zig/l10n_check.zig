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
//! hold none of them, only CRLF line breaks. The rule therefore counts complete printf codes (`%s`, `%5.2f`,
//! `%ld`, `%1$s`, `%-*.*lld`: position, flags, width, precision and length all belong to the token;
//! `%%` is a literal), `{n}`, `$name`, `<tag>` (no white space inside, at most 24 characters)
//! and a literal `\n`; square-bracket keys are left to the translator, who may write the local key.
//!
//! Rule `glyph` (D063, D068): every code point of an overlay text must be drawable. A language with a
//! face (ru, zh-Hans, ja, hi) needs each code point in its face or in NotoSans-Regular (the Hindi
//! texts hold ASCII and punctuation the Devanagari face lacks), read from the fonts' `cmap`
//! (tools/zig/l10n_cmap.zig); the five Latin languages draw with the game's cp1252 bitmap fonts, so
//! each code point must be in cp1252. CR, LF and TAB are ignored. The tracked Unicode overlay is
//! checked, never a generated pak (the Hindi private-use forms live only there, D058). The fonts are
//! read from `--fonts <dir>`, default `<root>/vendor/fonts`.
//!
//! Rule `table-*` (D061): a string table directory holds `en.lang` and one `<code>.lang` of the nine
//! languages, UTF-8, CRLF, `key=value` lines, `#` comment lines and blank lines. `en.lang` is the
//! reference: `table-missing` (a key, or a whole file, absent), `table-extra` (a key en no longer
//! has), `table-placeholder` (the printf codes and `{n}` differ), `table-id` (a `##`/`###` ImGui
//! ID suffix differs from en, or a `###` suffix is not the key itself), `table-format` (a line
//! without `=`, a duplicate key, an LF-only line, invalid UTF-8); `glyph` covers each value. The
//! editors draw every table with Noto Sans plus the language's own face, so a code point must be
//! in the language face or in NotoSans-Regular, for the Latin languages too (not cp1252 as the
//! game's bitmap fonts need). `table_dirs` lists the real tree's directories; with none holding an
//! `en.lang` the scope is `not active`.
//!
//! Rule `doc-*`: a document set is an English `<stem>.md` and nine `<stem>_<Language>.md`.
//! Each needs to exist (`doc-missing`), have the English file's sequence of heading levels
//! outside fenced code (`doc-structure`) and its number of fenced code blocks (`doc-code`), and
//! start with the language bar (`doc-bar`): ten links in the fixed order, each target one of the
//! set's files (`doc-missing` reports one that does not exist), English pointing at the English file. A `<stem>_English.md` is `doc-extra`.
//! `document_sets` is empty until S08 adds the root README set (`--doc-set <stem>` adds one).
//!
//! Rule `literal`: in the `strict_dirs` (S04 to S07 add the migrated source directories;
//! `--strict-dir <dir>` adds one), a known UI call whose first argument is a string literal is a
//! finding: it must go through `l10n.t`, `l10n.id` or `l10n.format`.
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
    if (bytes.len < 2 or bytes[0] != 0xff or bytes[1] != 0xfe) return &.{};
    // Tokens are ASCII: every other unit becomes 0x80, which ends any token.
    const count = (bytes.len - 2) / 2;
    const ascii = try arena.alloc(u8, count);
    for (ascii, 0..) |*c, i| {
        const unit = std.mem.readInt(u16, bytes[2 + i * 2 ..][0..2], .little);
        c.* = if (unit < 0x80) @intCast(unit) else 0x80;
    }
    return scanTokens(arena, ascii, true);
}

/// The tokens of an ASCII view of a text (see `placeholderTokens`), sorted. A string table value
/// counts only printf codes and `{n}` (`all_kinds` false): `$`, `<tag>` and a literal `\n` are
/// ordinary characters there.
fn scanTokens(arena: std.mem.Allocator, ascii: []const u8, all_kinds: bool) ![]const []const u8 {
    var tokens: std.ArrayList([]const u8) = .empty;
    var at: usize = 0;
    while (at < ascii.len) {
        const c = if (!all_kinds and (ascii[at] == '$' or ascii[at] == '<' or ascii[at] == '\\')) 0 else ascii[at];
        var end: usize = at + 1;
        var is_token = false;
        switch (c) {
            '%' => {
                if (at + 1 < ascii.len and ascii[at + 1] == '%') {
                    at += 2;
                    continue;
                }
                if (printfEnd(ascii, at)) |code_end| {
                    end = code_end;
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

/// The end of the complete C printf code that starts at the `%` at `at`, or null when the text
/// there is not a complete code. The grammar is `%[n$][flags][width][.precision][length]conversion`:
/// flags `-+ #0'`, width and precision as digits or `*` (`*n$`), lengths hh h l ll j z t L q, conversions diouxXeEfFgGaAcspn plus C and S (the Windows CRT). The
/// whole code is the token, so `%ld` and `%ls` or `%1$s` and `%2$s` differ.
fn printfEnd(ascii: []const u8, at: usize) ?usize {
    var i = at + 1;
    const position = i;
    while (i < ascii.len and std.ascii.isDigit(ascii[i])) i += 1;
    if (i == position or i >= ascii.len or ascii[i] != '$') i = position else i += 1;
    while (i < ascii.len and std.mem.indexOfScalar(u8, "-+ #0'", ascii[i]) != null) i += 1;
    i = starOrDigits(ascii, i);
    if (i < ascii.len and ascii[i] == '.') i = starOrDigits(ascii, i + 1);
    for ([_][]const u8{ "hh", "ll", "h", "l", "j", "z", "t", "L", "q" }) |length| {
        if (std.mem.startsWith(u8, ascii[i..], length)) {
            i += length.len;
            break;
        }
    }
    if (i < ascii.len and std.mem.indexOfScalar(u8, "diouxXeEfFgGaAcspnCS", ascii[i]) != null) return i + 1;
    return null;
}

/// Past a width or precision at `i`: digits, or `*` with an optional `n$`.
fn starOrDigits(ascii: []const u8, start: usize) usize {
    var i = start;
    if (i < ascii.len and ascii[i] == '*') {
        i += 1;
        const digits = i;
        while (i < ascii.len and std.ascii.isDigit(ascii[i])) i += 1;
        if (i == digits or i >= ascii.len or ascii[i] != '$') {
            i = digits;
        } else {
            i += 1;
        }
        return i;
    }
    while (i < ascii.len and std.ascii.isDigit(ascii[i])) i += 1;
    return i;
}

/// The tokens of a UTF-8 string table value: printf codes and `{n}` only, sorted.
pub fn tableTokens(arena: std.mem.Allocator, value: []const u8) ![]const []const u8 {
    const ascii = try arena.dupe(u8, value);
    for (ascii) |*c| {
        if (c.* >= 0x80) c.* = 0x80;
    }
    return scanTokens(arena, ascii, false);
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

/// The string table directories of the real tree, relative to the repository root.
pub const table_dirs = [_][]const u8{ "Sources/editor/kit/lang", "tools/zig/lang" };

pub const TableEntry = struct { key: []const u8, value: []const u8, line: usize };
pub const TableProblem = struct { line: usize, detail: []const u8 };
pub const Table = struct { entries: []const TableEntry, problems: []const TableProblem };

/// The entries of a `.lang` file and what is wrong with its form. A problem on line 0 concerns the
/// whole file. An LF-only line still yields its entry, so one fault is one finding.
pub fn parseTable(arena: std.mem.Allocator, bytes: []const u8) !Table {
    var entries: std.ArrayList(TableEntry) = .empty;
    var problems: std.ArrayList(TableProblem) = .empty;
    var seen: std.StringHashMapUnmanaged(usize) = .empty;
    if (!std.unicode.utf8ValidateSlice(bytes)) try problems.append(arena, .{ .line = 0, .detail = "file is not valid UTF-8" });
    var line_no: usize = 0;
    var at: usize = 0;
    while (at < bytes.len) {
        line_no += 1;
        const end = std.mem.indexOfScalarPos(u8, bytes, at, '\n');
        var text = bytes[at .. end orelse bytes.len];
        at = if (end) |e| e + 1 else bytes.len;
        if (end != null) {
            if (text.len > 0 and text[text.len - 1] == '\r') {
                text = text[0 .. text.len - 1];
            } else try problems.append(arena, .{ .line = line_no, .detail = "line ends with LF only, want CRLF" });
        }
        const trimmed = std.mem.trim(u8, text, " \t");
        if (trimmed.len == 0 or trimmed[0] == '#') continue;
        const eq = std.mem.indexOfScalar(u8, text, '=') orelse {
            try problems.append(arena, .{ .line = line_no, .detail = "line has no `=`" });
            continue;
        };
        const key = text[0..eq];
        if (key.len == 0) {
            try problems.append(arena, .{ .line = line_no, .detail = "line has an empty key" });
            continue;
        }
        if (seen.get(key)) |first| {
            try problems.append(arena, .{ .line = line_no, .detail = try std.fmt.allocPrint(arena, "duplicate key `{s}` (first on line {d})", .{ key, first }) });
            continue;
        }
        try seen.put(arena, key, line_no);
        try entries.append(arena, .{ .key = key, .value = text[eq + 1 ..], .line = line_no });
    }
    return .{ .entries = entries.items, .problems = problems.items };
}

/// The ImGui ID suffix of a table value: everything from the first `##` (`##hidden` or `###id`).
pub fn idSuffix(value: []const u8) ?[]const u8 {
    const at = std.mem.indexOf(u8, value, "##") orelse return null;
    return value[at..];
}

fn optionalEql(a: ?[]const u8, b: ?[]const u8) bool {
    if (a == null or b == null) return a == null and b == null;
    return std.mem.eql(u8, a.?, b.?);
}

/// The face that lacks `code_point` for a table value, or null when it is covered (or no face
/// could be read; the overlay glyph rule reports a missing font).
fn missingFace(own: ?Face, latin: Face, code_point: u21) ?[]const u8 {
    if (latin.table == null) return null;
    if (own) |face| {
        if (face.table == null or face.covers(code_point) or latin.covers(code_point)) return null;
        return face.name;
    }
    return if (latin.covers(code_point)) null else latin.name;
}

fn checkValueGlyphs(report: *Report, code: []const u8, where: []const u8, value: []const u8, own: ?Face, latin: Face) !void {
    var seen: std.ArrayList(u21) = .empty;
    var view = std.unicode.Utf8View.init(value) catch return;
    var it = view.iterator();
    while (it.nextCodepoint()) |code_point| {
        if (std.mem.indexOfScalar(u21, seen.items, code_point) != null) continue;
        try seen.append(report.arena, code_point);
        if (missingFace(own, latin, code_point)) |name| {
            try report.addFmt("glyph", code, where, "U+{X:0>4} not in {s}", .{ code_point, name });
        }
    }
}

fn reportTableProblems(report: *Report, code: []const u8, path: []const u8, table: Table) !void {
    for (table.problems) |problem| {
        const where = if (problem.line == 0) path else try std.fmt.allocPrint(report.arena, "{s}:{d}", .{ path, problem.line });
        try report.add("table-format", code, where, problem.detail);
    }
}

/// A `###` suffix must be the key itself, so a renamed key cannot keep a stale widget ID.
fn checkKeySuffix(report: *Report, code: []const u8, where: []const u8, key: []const u8, value: []const u8) !void {
    const suffix = idSuffix(value) orelse return;
    if (std.mem.startsWith(u8, suffix, "###") and !std.mem.eql(u8, suffix[3..], key)) {
        try report.addFmt("table-id", code, where, "ID suffix {s} must be ###{s}", .{ suffix, key });
    }
}

/// Rules table-* for every table directory of `table_dirs` that holds an `en.lang`; `fonts` null
/// skips the glyph rule (the caller already reported why).
pub fn checkTables(io: std.Io, report: *Report, root: std.Io.Dir, fonts: ?std.Io.Dir) !void {
    const arena = report.arena;
    var active = false;
    for (table_dirs) |dir| {
        const en_path = try std.fmt.allocPrint(arena, "{s}/en.lang", .{dir});
        const en_bytes = (try readOptional(io, arena, root, en_path)) orelse continue;
        active = true;
        try checkTableDir(io, report, root, fonts, dir, en_path, en_bytes);
    }
    if (!active) try report.inactive.append(arena, "tables");
}

fn checkTableDir(io: std.Io, report: *Report, root: std.Io.Dir, fonts: ?std.Io.Dir, dir: []const u8, en_path: []const u8, en_bytes: []const u8) !void {
    const arena = report.arena;
    const latin: Face = if (fonts) |f| Face.load(io, arena, f, "NotoSans-Regular.ttf") else .{ .name = "NotoSans-Regular.ttf" };
    const en = try parseTable(arena, en_bytes);
    var en_index: std.StringHashMapUnmanaged(usize) = .empty;
    for (en.entries, 0..) |entry, i| try en_index.put(arena, entry.key, i);
    report.checked += 1;
    try reportTableProblems(report, "en", en_path, en);
    for (en.entries) |entry| {
        const where = try std.fmt.allocPrint(arena, "{s}:{s}", .{ en_path, entry.key });
        try checkKeySuffix(report, "en", where, entry.key, entry.value);
        try checkValueGlyphs(report, "en", where, entry.value, null, latin);
    }

    for (languages.languages) |language| {
        report.checked += 1;
        const path = try std.fmt.allocPrint(arena, "{s}/{s}.lang", .{ dir, language.code });
        const bytes = (try readOptional(io, arena, root, path)) orelse {
            try report.add("table-missing", language.code, path, "no table file although en.lang exists");
            continue;
        };
        const own: ?Face = if (language.face) |name| (if (fonts) |f| Face.load(io, arena, f, name) else null) else null;
        const table = try parseTable(arena, bytes);
        try reportTableProblems(report, language.code, path, table);
        var have_index: std.StringHashMapUnmanaged(usize) = .empty;
        for (table.entries, 0..) |entry, i| try have_index.put(arena, entry.key, i);

        for (en.entries) |entry| {
            if (have_index.contains(entry.key)) continue;
            const where = try std.fmt.allocPrint(arena, "{s}:{s}", .{ path, entry.key });
            try report.add("table-missing", language.code, where, "key is in en.lang but not here");
        }
        for (table.entries) |entry| {
            const where = try std.fmt.allocPrint(arena, "{s}:{s}", .{ path, entry.key });
            try checkValueGlyphs(report, language.code, where, entry.value, own, latin);
            const english = en.entries[
                en_index.get(entry.key) orelse {
                    try report.add("table-extra", language.code, where, "key is not in en.lang");
                    continue;
                }
            ];
            const want = try tableTokens(arena, english.value);
            const got = try tableTokens(arena, entry.value);
            if (!tokensEqual(want, got)) {
                try report.addFmt("table-placeholder", language.code, where, "placeholders differ: English has {s}, translation has {s}", .{ try joinTokens(arena, want), try joinTokens(arena, got) });
            }
            const want_id = idSuffix(english.value);
            const got_id = idSuffix(entry.value);
            if (!optionalEql(want_id, got_id)) {
                try report.addFmt("table-id", language.code, where, "ID suffix {s} differs from English {s}", .{ got_id orelse "none", want_id orelse "none" });
            } else try checkKeySuffix(report, language.code, where, entry.key, entry.value);
        }
    }
}

/// A document set of the real tree, as a path without the language part: `README` names README.md
/// and README_<Language>.md in the repository root. S08 adds the root README set here.
pub const document_sets = [_][]const u8{};

const DocLanguage = struct { code: []const u8, name: []const u8 };

/// The nine translations in the order of the language bar.
const doc_languages = [_]DocLanguage{
    .{ .code = "ru", .name = "Russian" },
    .{ .code = "zh-Hans", .name = "Chinese" },
    .{ .code = "hi", .name = "Hindi" },
    .{ .code = "es", .name = "Spanish" },
    .{ .code = "fr", .name = "French" },
    .{ .code = "de", .name = "German" },
    .{ .code = "pt-BR", .name = "Portuguese" },
    .{ .code = "ja", .name = "Japanese" },
    .{ .code = "id", .name = "Indonesian" },
};

pub const Document = struct {
    /// The `#` count of each heading line outside fenced code.
    levels: []const u8,
    code_blocks: usize,
    /// Line 1 without byte order mark and line break.
    first_line: []const u8,
};

pub fn parseDocument(arena: std.mem.Allocator, bytes: []const u8) !Document {
    var levels: std.ArrayList(u8) = .empty;
    var code_blocks: usize = 0;
    var in_fence = false;
    const text = if (std.mem.startsWith(u8, bytes, "\xEF\xBB\xBF")) bytes[3..] else bytes;
    var first_line: []const u8 = "";
    var lines = std.mem.splitScalar(u8, text, '\n');
    var number: usize = 0;
    while (lines.next()) |raw| {
        number += 1;
        const line = std.mem.trimEnd(u8, raw, "\r");
        if (number == 1) first_line = line;
        if (std.mem.startsWith(u8, std.mem.trimStart(u8, line, " "), "```")) {
            if (!in_fence) code_blocks += 1;
            in_fence = !in_fence;
            continue;
        }
        if (in_fence) continue;
        var hashes: usize = 0;
        while (hashes < line.len and line[hashes] == '#') hashes += 1;
        if (hashes >= 1 and hashes <= 6 and (hashes == line.len or line[hashes] == ' ')) try levels.append(arena, @intCast(hashes));
    }
    return .{ .levels = levels.items, .code_blocks = code_blocks, .first_line = first_line };
}

/// The targets of the `[label](target)` links of a line, in order.
pub fn linkTargets(arena: std.mem.Allocator, line: []const u8) ![]const []const u8 {
    var targets: std.ArrayList([]const u8) = .empty;
    var at: usize = 0;
    while (std.mem.indexOfPos(u8, line, at, "](")) |open| {
        const start = open + 2;
        const close = std.mem.indexOfScalarPos(u8, line, start, ')') orelse break;
        try targets.append(arena, line[start..close]);
        at = close + 1;
    }
    return targets.items;
}

fn checkBar(report: *Report, stem: []const u8, code: []const u8, path: []const u8, document: Document) !void {
    const arena = report.arena;
    const base = std.fs.path.basename(stem);
    const targets = try linkTargets(arena, document.first_line);
    const want_count = doc_languages.len + 1;
    if (targets.len != want_count) {
        try report.addFmt("doc-bar", code, path, "line 1 holds {d} links, want {d}", .{ targets.len, want_count });
        return;
    }
    for (targets, 0..) |target, i| {
        const want = if (i == 0)
            try std.fmt.allocPrint(arena, "{s}.md", .{base})
        else
            try std.fmt.allocPrint(arena, "{s}_{s}.md", .{ base, doc_languages[i - 1].name });
        if (!std.mem.eql(u8, target, want)) {
            try report.addFmt("doc-bar", code, path, "link {d} targets {s}, want {s}", .{ i + 1, target, want });
            return;
        }
    }
}

fn existsPlain(io: std.Io, root: std.Io.Dir, path: []const u8) !bool {
    root.access(io, path, .{}) catch return false;
    return true;
}

/// Rules doc-* for each document set (the constant ones plus `extra`).
pub fn checkDocuments(io: std.Io, report: *Report, root: std.Io.Dir, extra: []const []const u8) !void {
    const arena = report.arena;
    if (document_sets.len == 0 and extra.len == 0) {
        try report.inactive.append(arena, "documents");
        return;
    }
    for ([_][]const []const u8{ &document_sets, extra }) |sets| for (sets) |stem| {
        const en_path = try std.fmt.allocPrint(arena, "{s}.md", .{stem});
        report.checked += 1;
        const en_bytes = (try readOptional(io, arena, root, en_path)) orelse {
            try report.add("doc-missing", "en", en_path, "English file does not exist");
            continue;
        };
        const english = try parseDocument(arena, en_bytes);
        try checkBar(report, stem, "en", en_path, english);
        for (doc_languages) |language| {
            report.checked += 1;
            const path = try std.fmt.allocPrint(arena, "{s}_{s}.md", .{ stem, language.name });
            const bytes = (try readOptional(io, arena, root, path)) orelse {
                try report.add("doc-missing", language.code, path, "translation file does not exist");
                continue;
            };
            const document = try parseDocument(arena, bytes);
            try checkBar(report, stem, language.code, path, document);
            try compareStructure(report, language.code, path, english, document);
            if (document.code_blocks != english.code_blocks) {
                try report.addFmt("doc-code", language.code, path, "has {d} fenced code blocks, English has {d}", .{ document.code_blocks, english.code_blocks });
            }
        }
        const stray = try std.fmt.allocPrint(arena, "{s}_English.md", .{stem});
        if (try existsPlain(io, root, stray)) {
            try report.addFmt("doc-extra", "en", stray, "the English file is {s}.md; remove this copy", .{std.fs.path.basename(stem)});
        }
    };
}

fn compareStructure(report: *Report, code: []const u8, path: []const u8, english: Document, document: Document) !void {
    const shared = @min(english.levels.len, document.levels.len);
    for (english.levels[0..shared], document.levels[0..shared], 0..) |want, got, i| {
        if (want != got) {
            try report.addFmt("doc-structure", code, path, "heading {d} is level {d}, English has level {d}", .{ i + 1, got, want });
            return;
        }
    }
    if (english.levels.len != document.levels.len) {
        try report.addFmt("doc-structure", code, path, "has {d} headings, English has {d}; they differ from heading {d}", .{ document.levels.len, english.levels.len, shared + 1 });
    }
}

/// Directories whose Zig sources must not hold an English UI literal. S04 to S07 add the source
/// directories they migrate to `l10n.t`; `--strict-dir <dir>` adds one for a fixture.
pub const strict_dirs = [_][]const u8{"Sources/editor/elk/app"};

/// The UI calls whose first argument is text a person reads.
const ui_calls = [_][]const u8{
    "igText",            "igTextWrapped",  "igTextDisabled",    "igTextUnformatted", "igButton",        "igSmallButton",
    "igMenuItem",        "igMenuItemEx",   "igMenuItemBoolPtr", "igBeginMenu",       "igBeginMenuEx",   "igBegin",
    "igSelectable",      "igSelectableEx", "igCheckbox",        "igRadioButton",     "igSeparatorText", "igSetTooltip",
    "igBeginPopupModal", "igBeginCombo",   "igInputText",       "igInputTextWithHint", "igInputFloat",  "igInputFloatEx",
    "igInputInt",        "igSliderInt",    "igSliderFloat",     "igSliderFloatEx",   "igOpenPopup",     "igBeginPopup",
    "igBeginTabItem",    "igBeginChild",   "igSetItemTooltip",  "igCollapsingHeader", "igTreeNode",    "text",
    "beginPanel",
};

/// Calls whose second argument is user-visible too: the hint of an input field.
const hint_calls = [_][]const u8{"igInputTextWithHint"};

/// Status-line helpers: every argument that is a string literal is shown to the user.
const status_calls = [_][]const u8{ "setStatus", "setStatusFrom", "note" };

fn inList(list: []const []const u8, name: []const u8) bool {
    for (list) |item| if (std.mem.eql(u8, item, name)) return true;
    return false;
}

fn isIdentChar(c: u8) bool {
    return std.ascii.isAlphanumeric(c) or c == '_';
}

/// Whether a literal holds a letter outside its printf codes: `"%s"` and `"##id"` are not English.
fn hasLetters(literal: []const u8) bool {
    var i: usize = 0;
    while (i < literal.len) : (i += 1) {
        if (literal[i] == '%') {
            while (i + 1 < literal.len and std.mem.indexOfScalar(u8, "%-+ #0123456789.", literal[i + 1]) != null) i += 1;
            if (i + 1 < literal.len) i += 1;
            continue;
        }
        if (literal[i] == '#' and i + 1 < literal.len and literal[i + 1] == '#') break;
        if (std.ascii.isAlphabetic(literal[i])) return true;
    }
    return false;
}

/// The text of the string literal starting at `line[at]` (a quote), or null when the argument is
/// not one or the literal has no letters.
fn literalAt(line: []const u8, at: usize) ?[]const u8 {
    if (at >= line.len or line[at] != '"') return null;
    var close = at + 1;
    while (close < line.len and line[close] != '"') close += if (line[close] == '\\') 2 else 1;
    if (close >= line.len) return null;
    const literal = line[at + 1 .. close];
    return if (hasLetters(literal)) literal else null;
}

/// The first English literal among the arguments of the call opened at `line[open]`: the first
/// argument, the second as well for `hint_calls`, every top-level argument for `status_calls`.
fn literalArg(line: []const u8, open: usize, name: []const u8) ?[]const u8 {
    const all = inList(&status_calls, name);
    const limit: usize = if (all) std.math.maxInt(usize) else if (inList(&hint_calls, name)) 2 else 1;
    var depth: usize = 0;
    var index: usize = 0;
    var arg_start = true;
    var i = open;
    while (i < line.len) : (i += 1) {
        const ch = line[i];
        if (arg_start and ch != ' ' and (ch != '(' or depth != 0)) {
            arg_start = false;
            if (depth == 1 and index < limit) {
                if (literalAt(line, i)) |literal| return literal;
            }
        }
        switch (ch) {
            '"' => {
                i += 1;
                while (i < line.len and line[i] != '"') i += if (line[i] == '\\') 2 else 1;
            },
            '(', '[', '{' => depth += 1,
            ')', ']', '}' => {
                if (depth <= 1) return null;
                depth -= 1;
            },
            ',' => if (depth == 1) {
                index += 1;
                arg_start = true;
                if (index >= limit) return null;
            },
            else => {},
        }
    }
    return null;
}

/// The first UI call on `line` with an English string literal argument, as `call("text")`.
fn literalCall(line: []const u8) ?struct { call: []const u8, literal: []const u8 } {
    const trimmed = std.mem.trimStart(u8, line, " \t");
    if (std.mem.startsWith(u8, trimmed, "//")) return null;
    var best: ?struct { at: usize, call: []const u8, literal: []const u8 } = null;
    for ([_][]const []const u8{ &ui_calls, &status_calls }) |names| for (names) |call| {
        var from: usize = 0;
        while (std.mem.indexOfPos(u8, line, from, call)) |at| {
            from = at + call.len;
            if (at > 0 and isIdentChar(line[at - 1])) continue;
            if (from >= line.len or line[from] != '(') continue;
            const literal = literalArg(line, from, call) orelse continue;
            if (best == null or at < best.?.at) best = .{ .at = at, .call = call, .literal = literal };
        }
    };
    const found = best orelse return null;
    return .{ .call = found.call, .literal = found.literal };
}

fn lessPath(_: void, a: []const u8, b: []const u8) bool {
    return std.mem.lessThan(u8, a, b);
}

/// Rule literal for each directory of `strict_dirs` plus `extra`.
pub fn checkLiterals(io: std.Io, report: *Report, root: std.Io.Dir, extra: []const []const u8) !void {
    const arena = report.arena;
    if (strict_dirs.len == 0 and extra.len == 0) {
        try report.inactive.append(arena, "literals");
        return;
    }
    var scanned = false;
    for ([_][]const []const u8{ &strict_dirs, extra }, 0..) |dirs, pass| for (dirs) |dir_path| {
        var dir = root.openDir(io, dir_path, .{ .iterate = true }) catch |err| {
            // A tree without the built-in directories (a seeded-fault fixture) has nothing of theirs to check.
            if (pass == 0 and err == error.FileNotFound) continue;
            try report.addFmt("literal", "all", dir_path, "cannot open the strict directory ({s})", .{@errorName(err)});
            continue;
        };
        defer dir.close(io);
        scanned = true;
        var files: std.ArrayList([]const u8) = .empty;
        var walker = try dir.walk(arena);
        defer walker.deinit();
        while (try walker.next(io)) |entry| {
            if (entry.kind != .file or !std.mem.endsWith(u8, entry.path, ".zig")) continue;
            try files.append(arena, try arena.dupe(u8, entry.path));
        }
        std.mem.sort([]const u8, files.items, {}, lessPath);
        for (files.items) |rel| {
            report.checked += 1;
            const path = try std.fmt.allocPrint(arena, "{s}/{s}", .{ dir_path, rel });
            for (path) |*c| {
                if (c.* == '\\') c.* = '/';
            }
            const bytes = (try readOptional(io, arena, root, path)) orelse continue;
            var lines = std.mem.splitScalar(u8, bytes, '\n');
            var number: usize = 0;
            while (lines.next()) |raw| {
                number += 1;
                const found = literalCall(std.mem.trimEnd(u8, raw, "\r")) orelse continue;
                try report.addFmt("literal", "all", try std.fmt.allocPrint(arena, "{s}:{d}", .{ path, number }), "{s} takes the English literal \"{s}\"; wrap it in l10n.t, l10n.id or l10n.format", .{ found.call, found.literal });
            }
        }
    };
    if (!scanned and extra.len == 0) try report.inactive.append(arena, "literals");
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
    std.debug.print("usage: l10n_check [--root <dir>] [--fonts <dir>] [--doc-set <stem>] [--strict-dir <dir>] [--update-base]\n", .{});
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
    var doc_sets: std.ArrayList([]const u8) = .empty;
    var strict: std.ArrayList([]const u8) = .empty;
    while (iterator.next()) |arg| {
        if (std.mem.eql(u8, arg, "--root")) {
            root_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--fonts")) {
            fonts_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--doc-set")) {
            try doc_sets.append(arena, try arena.dupe(u8, iterator.next() orelse usage()));
        } else if (std.mem.eql(u8, arg, "--strict-dir")) {
            try strict.append(arena, try arena.dupe(u8, iterator.next() orelse usage()));
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
        try checkTables(io, &report, root, dir.*);
    } else |err| {
        try report.addFmt("glyph", "all", fonts_path orelse "vendor/fonts", "cannot open the fonts directory ({s})", .{@errorName(err)});
        try checkTables(io, &report, root, null);
    }
    try checkDocuments(io, &report, root, doc_sets.items);
    try checkLiterals(io, &report, root, strict.items);

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

test "placeholderTokens keeps the whole printf code: position, flags, width, precision, length" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const tokens = try tableTokens(arena_state.allocator(), "%ld %ls %lld %hd %zu %1$s %2$d %-8.3lf %*d %.*s %2$*1$d %05d 100%% %%d %qd");
    const want = [_][]const u8{ "%*d", "%-8.3lf", "%.*s", "%05d", "%1$s", "%2$*1$d", "%2$d", "%hd", "%ld", "%lld", "%ls", "%qd", "%zu" };
    try testing.expectEqual(want.len, tokens.len);
    for (want, tokens) |w, tok| try testing.expectEqualStrings(w, tok);
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

test "parseTable reads entries and names format problems" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const good = try parseTable(arena, "# c\r\n\r\na=1\r\nb=x=y\r\n");
    try testing.expectEqual(@as(usize, 0), good.problems.len);
    try testing.expectEqual(@as(usize, 2), good.entries.len);
    try testing.expectEqualStrings("x=y", good.entries[1].value);
    try testing.expectEqual(@as(usize, 4), good.entries[1].line);
    const bad = try parseTable(arena, "a=1\r\na=2\r\nnoequals\r\nb=\xff\r\nc=1\n");
    try testing.expectEqual(@as(usize, 4), bad.problems.len);
    try testing.expectEqualStrings("file is not valid UTF-8", bad.problems[0].detail);
    try testing.expectEqualStrings("duplicate key `a` (first on line 1)", bad.problems[1].detail);
    try testing.expectEqualStrings("line has no `=`", bad.problems[2].detail);
    try testing.expectEqual(@as(usize, 5), bad.problems[3].line);
}

test "tableTokens counts printf codes and numbered slots only" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const tokens = try tableTokens(arena_state.allocator(), "%d of {0} <b> $x \\n 100%%");
    try testing.expectEqual(@as(usize, 2), tokens.len);
    try testing.expectEqualStrings("%d", tokens[0]);
    try testing.expectEqualStrings("{0}", tokens[1]);
}

test "literalCall finds an English literal and skips wrapped text, format strings and comments" {
    const testing = std.testing;
    const found = literalCall("    _ = c.igButton(\"Save\", .{});").?;
    try testing.expectEqualStrings("igButton", found.call);
    try testing.expectEqualStrings("Save", found.literal);
    try testing.expect(literalCall("c.igButton(l10n.t(\"file.save\"), .{})") == null);
    try testing.expect(literalCall("c.igText(\"%s\", name)") == null);
    try testing.expect(literalCall("// c.igText(\"Hello\")") == null);
    try testing.expect(literalCall("c.my_igText(\"Hello\")") == null);
}

test "the document rule parses the real README.md and README_German.md" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const cwd = std.Io.Dir.cwd();
    const english = try parseDocument(arena, try cwd.readFileAlloc(testing.io, "README.md", arena, .limited(1 << 20)));
    const german = try parseDocument(arena, try cwd.readFileAlloc(testing.io, "README_German.md", arena, .limited(1 << 20)));
    var top_level: usize = 0;
    for (english.levels) |level| top_level += @intFromBool(level == 1);
    try testing.expectEqual(@as(usize, 7), top_level);
    // README_German.md is not yet a full translation (S08); it only has to parse, with its bar.
    try testing.expect(german.levels.len > 0);
    try testing.expectEqual(@as(usize, 10), (try linkTargets(arena, english.first_line)).len);
    try testing.expectEqual(@as(usize, 10), (try linkTargets(arena, german.first_line)).len);
}
