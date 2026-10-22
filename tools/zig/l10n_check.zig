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
//! Scope (M003 D096): `Localization/game/scope.txt` lists the seed ELK keys (relative to
//! `game_data_base`, no extension) every language overlay must hold, and two directives widen one
//! language: `whole-database <code>` (every `.elk` original, found case-insensitively) and
//! `prefix <code> <key prefix>` (the seed keys plus every `.elk` key under the prefix). A language
//! with neither is seed-only; both for one language is a `scope` finding, as are an unknown code and a
//! malformed line. `extra` names the scope rule an overlay text breaks. The run prints one
//! `l10n-check: scope: <code>: <scope>, <n> keys checked` note per language. An overlay text is
//! `<key>.txt` (UTF-16LE with a byte order mark) beside its state file `<key>.xml`
//! (`<base State="n" .../>`, 0 not translated, 1 outdated, 2 translated, 3 approved, as
//! Sources/editor/elk/core/state.zig names them).
//!
//! The English source of a key is its `Data/ELK/game_data_base/<key>.elk` (the original; the
//! `.txt` beside it is a translation working copy, used only when no `.elk` exists). An overlay
//! .xml cannot record which English text a translation was made from, so D075 keeps a tracked
//! manifest, `Localization/game/base-hashes.txt`: `<key> <sha256 hex of the English bytes>` per
//! `.elk` original (and seed key), plus `same-as-base <code> <key>` lines naming translations that may equal the
//! English byte for byte (a proper name). Rule `stale` fires when the English bytes no longer
//! match their hash, `base-hash-missing` when a key in any language's scope has no line, `untranslated` when an
//! overlay text equals the English bytes without a `same-as-base` line. `--update-base` rewrites
//! the manifest (`zig build l10n-check-update-base`) after a person reviewed the translations.
//! Some Data/ELK originals are Russian; every comparison is against the base of that key.
//!
//! Rule `glossary-term` (D098): for each in-scope key, every term of `Localization/glossary/<code>.md`
//! (its `English term | Translation | Note` rows) that is a whole word of the English original,
//! ignoring case and line breaks, must be a case-insensitive substring of the overlay text. A row
//! whose Note holds `no-term-check` is skipped; a line `<code> <key> <english term> # <reason>` of
//! `Localization/game/term-exceptions.txt` excuses one key and term (`term-exception` for a line without a
//! reason, `term-exception-unused` for one that excuses nothing). A missing glossary is a finding.
//!
//! Rule `fit` (D097): a translated text must fit the room the game draws it in. `checkFit` measures every
//! in-scope text of a language with the bk_text module, the Game's own text engine: HarfBuzz-shaped
//! Liberation Serif or Noto from `--fonts <dir>` (default `<root>/vendor/fonts`) at the Game's size
//! table (`bk_text.sizes`) and bk_text's wrap, against the box `Localization/game/fit-map.txt` gives its key
//! family: a `Data/UI` layout element, a rectangle, a tooltip or a scrolling text. Findings are `fit` (too
//! many lines for the box), `fit-word` (a word wider than the box, which the game splits), `fit-unmapped`
//! (no map line for a key), `fit-map` and `fit-font`. An English original that already overflows is
//! `fit-base` information in the language's note. All nine languages are measured on every run, with no
//! generated font. Without a fit map the scope is `not active`. See `checkFit`.
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
//! Rule `glyph` (D063, D068): every code point of an overlay text must be drawable. Each language's
//! mod ships fonts bk-font generates from its face (Liberation Serif for the Latin languages, Noto for
//! the others) with NotoSans-Regular as the fallback for what the face lacks (the Devanagari face holds
//! no Latin letters), so each code point must be in one of the two, read from the fonts' `cmap`
//! (tools/zig/l10n_cmap.zig). The game's own bitmap fonts are no measure: they
//! hold ASCII and the Cyrillic set, none of the accented Latin letters. CR, LF and TAB are ignored. The tracked Unicode overlay is
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
//! in the language face or in NotoSans-Regular. `table_dirs` lists the real tree's directories; with none holding an
//! `en.lang` the scope is `not active`.
//!
//! Rule `doc-*`: a document set is an English `<stem>.md` and nine `<stem>_<Language>.md`.
//! Each needs to exist (`doc-missing`), have the English file's sequence of heading levels
//! outside fenced code (`doc-structure`) and its number of fenced code blocks (`doc-code`), and
//! start with the language bar (`doc-bar`): ten links in the fixed order, each target one of the
//! set's files (`doc-missing` reports one that does not exist), English pointing at the English file. A `<stem>_English.md` is `doc-extra`.
//! S08 fills `document_sets` (the root README set, the tools guide, the test projects); `--doc-set <stem>` adds one and `--no-default-docs` drops the built-in sets (the fixture tests).
//!
//! Rule `literal`: in the `strict_dirs` (the Map Editor, ELK and Resource Editor app directories;
//! `--strict-dir <dir>` adds one), a known UI call whose first argument is a string literal is a
//! finding: it must go through `l10n.t`, `l10n.id` or `l10n.format`.
const std = @import("std");
const languages = @import("l10n_languages.zig");
const cmap = @import("l10n_cmap.zig");
/// The Game's own text engine, which the fit rule measures with (M004 S04, D097): shaping and the size table.
const bk_text = @import("bk_text");
/// The game font reader and wrap, public for the l10n-game-reads-it tier, which predicts the lines of a
/// captured text with them.
pub const font_file = @import("l10n_font.zig");
pub const wrap = @import("l10n_wrap");

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
    /// One printed line per language: its scope and how many keys were checked.
    scope_notes: std.ArrayList([]const u8) = .empty,
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

pub const scope_path = "Localization/game/scope.txt";
pub const exceptions_path = "Localization/game/term-exceptions.txt";

pub const ScopeProblem = struct { line: usize, detail: []const u8 };
pub const WholeLine = struct { code: []const u8, line: usize };
pub const PrefixLine = struct { code: []const u8, prefix: []const u8, line: usize };

/// The lines of a `scope.txt`: seed keys (every other line), `whole-database <code>` and
/// `prefix <code> <key prefix>`. A key may hold a space, so only the two words followed by a space
/// start a directive. Problems are returned, not reported, so the parser stays pure.
pub const ScopeFile = struct {
    seed: []const []const u8,
    whole: []const WholeLine,
    prefixes: []const PrefixLine,
    problems: []const ScopeProblem,
};

fn knownCode(code: []const u8) bool {
    for (languages.languages) |language| {
        if (std.mem.eql(u8, language.code, code)) return true;
    }
    return false;
}

pub fn parseScopeFile(arena: std.mem.Allocator, text: []const u8) !ScopeFile {
    var seed: std.ArrayList([]const u8) = .empty;
    var whole: std.ArrayList(WholeLine) = .empty;
    var prefixes: std.ArrayList(PrefixLine) = .empty;
    var problems: std.ArrayList(ScopeProblem) = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    var number: usize = 0;
    while (lines.next()) |raw| {
        number += 1;
        const line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0 or line[0] == '#') continue;
        if (std.mem.eql(u8, line, "whole-database") or std.mem.eql(u8, line, "prefix")) {
            try problems.append(arena, .{ .line = number, .detail = "directive without arguments" });
        } else if (std.mem.startsWith(u8, line, "whole-database ")) {
            const code = std.mem.trim(u8, line["whole-database ".len..], " \t");
            if (code.len == 0 or std.mem.indexOfScalar(u8, code, ' ') != null) {
                try problems.append(arena, .{ .line = number, .detail = "whole-database wants exactly one language code" });
            } else if (!knownCode(code)) {
                try problems.append(arena, .{ .line = number, .detail = try std.fmt.allocPrint(arena, "unknown language code `{s}`", .{code}) });
            } else try whole.append(arena, .{ .code = code, .line = number });
        } else if (std.mem.startsWith(u8, line, "prefix ")) {
            const rest = std.mem.trimStart(u8, line["prefix ".len..], " \t");
            const space = std.mem.indexOfScalar(u8, rest, ' ');
            const prefix = if (space) |s| std.mem.trimStart(u8, rest[s + 1 ..], " \t") else "";
            if (space == null or prefix.len == 0) {
                try problems.append(arena, .{ .line = number, .detail = "prefix wants a language code and a key prefix" });
            } else if (!knownCode(rest[0..space.?])) {
                try problems.append(arena, .{ .line = number, .detail = try std.fmt.allocPrint(arena, "unknown language code `{s}`", .{rest[0..space.?]}) });
            } else try prefixes.append(arena, .{ .code = rest[0..space.?], .prefix = prefix, .line = number });
        } else try seed.append(arena, line);
    }
    return .{ .seed = seed.items, .whole = whole.items, .prefixes = prefixes.items, .problems = problems.items };
}

/// The seed keys of a `scope.txt`: one per line, `#` lines, blank lines and directives skipped.
pub fn parseScope(arena: std.mem.Allocator, text: []const u8) ![]const []const u8 {
    return (try parseScopeFile(arena, text)).seed;
}

pub const ScopeKind = enum { seed, prefixes, whole };

/// The keys one language must translate, in file order (seed first), with a set for membership.
pub const LanguageScope = struct {
    code: []const u8,
    kind: ScopeKind,
    keys: []const []const u8,
    set: std.StringHashMapUnmanaged(void),
    prefixes: []const []const u8,

    pub fn contains(self: LanguageScope, key: []const u8) bool {
        return self.set.contains(key);
    }

    /// The scope as the run prints it: `seed`, `seed + prefixes a, b` or `whole-database`.
    pub fn describe(self: LanguageScope, arena: std.mem.Allocator) ![]const u8 {
        return switch (self.kind) {
            .seed => "seed",
            .whole => "whole-database",
            .prefixes => try std.fmt.allocPrint(arena, "seed + prefixes {s}", .{try std.mem.join(arena, ", ", self.prefixes)}),
        };
    }

    /// The rule a key outside the scope breaks, for the `extra` finding.
    pub fn rule(self: LanguageScope) []const u8 {
        return switch (self.kind) {
            .seed => "not in the seed list of Localization/game/scope.txt (no prefix or whole-database line for this language)",
            .prefixes => "not in the seed list or under a `prefix` line of this language in Localization/game/scope.txt",
            .whole => "no .elk original in Data/ELK/game_data_base (whole-database scope)",
        };
    }
};

pub const Scopes = struct {
    /// One per entry of `languages.languages`, in that order.
    per_language: []const LanguageScope,
    /// Every key any language has in scope, sorted: the keys whose English base is hashed.
    all_keys: []const []const u8,
};

/// Every `.elk` original below Data/ELK/game_data_base as a key (relative, no extension, forward
/// slashes, the spelling on disk), sorted. Empty when the directory does not exist.
pub fn elkKeys(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir) ![]const []const u8 {
    const base = (try findIgnoreCase(io, arena, root, "Data/ELK/game_data_base")) orelse return &.{};
    var dir = try root.openDir(io, base, .{ .iterate = true });
    defer dir.close(io);
    var keys: std.ArrayList([]const u8) = .empty;
    var walker = try dir.walk(arena);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file or entry.path.len <= 4) continue;
        if (!std.ascii.eqlIgnoreCase(entry.path[entry.path.len - 4 ..], ".elk")) continue;
        const key = try arena.dupe(u8, entry.path[0 .. entry.path.len - 4]);
        for (key) |*c| {
            if (c.* == '\\') c.* = '/';
        }
        try keys.append(arena, key);
    }
    std.mem.sort([]const u8, keys.items, {}, lessString);
    return keys.items;
}

/// Whether `key` is new to `seen`, which holds keys ASCII-lowercased: a Data tree may spell a
/// directory with another case than a seed key, and both name one text.
fn firstSight(arena: std.mem.Allocator, seen: *std.StringHashMapUnmanaged(void), key: []const u8) !bool {
    const lower = try std.ascii.allocLowerString(arena, key);
    return (try seen.fetchPut(arena, lower, {})) == null;
}

/// The scope of each language. `report` null reads the file without reporting its problems (a rule
/// that only needs the key sets; `checkOverlays` reports them once).
pub fn loadScopes(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, report: ?*Report) !?Scopes {
    const text = (try readOptional(io, arena, root, scope_path)) orelse {
        if (report) |r| try r.add("scope", "all", scope_path, "file is missing");
        return null;
    };
    const file = try parseScopeFile(arena, text);
    if (report) |r| {
        for (file.problems) |problem| {
            try r.addFmt("scope", "all", scope_path, "line {d}: {s}", .{ problem.line, problem.detail });
        }
        if (file.seed.len == 0) try r.add("scope", "all", scope_path, "lists no key");
    }
    const needs_elk = file.whole.len != 0 or file.prefixes.len != 0;
    const elk: []const []const u8 = if (needs_elk) try elkKeys(io, arena, root) else &.{};

    var per_language: std.ArrayList(LanguageScope) = .empty;
    var all: std.StringHashMapUnmanaged(void) = .empty;
    var all_keys: std.ArrayList([]const u8) = .empty;
    for (languages.languages) |language| {
        var kind: ScopeKind = .seed;
        var set: std.StringHashMapUnmanaged(void) = .empty;
        var seen: std.StringHashMapUnmanaged(void) = .empty;
        var keys: std.ArrayList([]const u8) = .empty;
        var prefixes: std.ArrayList([]const u8) = .empty;
        for (file.seed) |key| {
            if (try firstSight(arena, &seen, key)) {
                try set.put(arena, key, {});
                try keys.append(arena, key);
            }
        }
        for (file.whole) |one| {
            if (std.mem.eql(u8, one.code, language.code)) kind = .whole;
        }
        for (file.prefixes) |one| {
            if (!std.mem.eql(u8, one.code, language.code)) continue;
            if (kind == .whole) {
                if (report) |r| try r.addFmt("scope", language.code, scope_path, "line {d}: prefix `{s}` is redundant, {s} is whole-database", .{ one.line, one.prefix, language.code });
                continue;
            }
            kind = .prefixes;
            try prefixes.append(arena, one.prefix);
        }
        if (kind == .whole) {
            for (elk) |key| {
                if (!try firstSight(arena, &seen, key)) continue;
                try set.put(arena, key, {});
                try keys.append(arena, key);
            }
        } else for (prefixes.items) |prefix| {
            for (elk) |key| {
                if (!std.ascii.startsWithIgnoreCase(key, prefix)) continue;
                if (!try firstSight(arena, &seen, key)) continue;
                try set.put(arena, key, {});
                try keys.append(arena, key);
            }
        }
        for (keys.items) |key| {
            if (try firstSight(arena, &all, key)) try all_keys.append(arena, key);
        }
        try per_language.append(arena, .{ .code = language.code, .kind = kind, .keys = keys.items, .set = set, .prefixes = prefixes.items });
    }
    std.mem.sort([]const u8, all_keys.items, {}, lessString);
    return .{ .per_language = per_language.items, .all_keys = all_keys.items };
}

pub fn readOptional(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, path: []const u8) !?[]u8 {
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
    /// Key to its position in `keys`, built by `parseManifest`; the database has about 2,850 keys.
    index: std.StringHashMapUnmanaged(usize) = .empty,
    same_index: std.StringHashMapUnmanaged(void) = .empty,

    pub fn hashOf(self: Manifest, key: []const u8) ?[]const u8 {
        if (self.index.count() != 0) return if (self.index.get(key)) |at| self.hashes[at] else null;
        for (self.keys, self.hashes) |one, hash| {
            if (std.mem.eql(u8, one, key)) return hash;
        }
        return null;
    }

    pub fn allowsSame(self: Manifest, code: []const u8, key: []const u8) bool {
        if (self.same_index.count() != 0) {
            var buffer: [512]u8 = undefined;
            const joined = std.fmt.bufPrint(&buffer, "{s} {s}", .{ code, key }) catch return false;
            return self.same_index.contains(joined);
        }
        for (self.same_as_base) |one| {
            if (std.mem.eql(u8, one.code, code) and std.mem.eql(u8, one.key, key)) return true;
        }
        return false;
    }
};

/// The lines of a `base-hashes.txt`: `<key> <hash>` and `same-as-base <code> <key>`; `#` lines and
/// blank lines are skipped, and a line of any other shape is ignored so the scope check names it.
/// A key may hold a space (one original is `hawker_hurricane_ mk1_gb`), so the hash is the text
/// after the last space and the key everything before it.
pub fn parseManifest(arena: std.mem.Allocator, text: []const u8) !Manifest {
    var keys: std.ArrayList([]const u8) = .empty;
    var hashes: std.ArrayList([]const u8) = .empty;
    var same: std.ArrayList(SameAsBase) = .empty;
    var index: std.StringHashMapUnmanaged(usize) = .empty;
    var same_index: std.StringHashMapUnmanaged(void) = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0 or line[0] == '#') continue;
        if (std.mem.startsWith(u8, line, "same-as-base ")) {
            const rest = std.mem.trimStart(u8, line["same-as-base ".len..], " ");
            const space = std.mem.indexOfScalar(u8, rest, ' ') orelse continue;
            const key = std.mem.trimStart(u8, rest[space + 1 ..], " ");
            if (key.len == 0) continue;
            try same.append(arena, .{ .code = rest[0..space], .key = key });
            try same_index.put(arena, try std.fmt.allocPrint(arena, "{s} {s}", .{ rest[0..space], key }), {});
        } else {
            const space = std.mem.lastIndexOfScalar(u8, line, ' ') orelse continue;
            const key = std.mem.trimEnd(u8, line[0..space], " ");
            if (key.len == 0) continue;
            try index.put(arena, key, keys.items.len);
            try keys.append(arena, key);
            try hashes.append(arena, line[space + 1 ..]);
        }
    }
    return .{ .keys = keys.items, .hashes = hashes.items, .same_as_base = same.items, .index = index, .same_index = same_index };
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
/// extra, base-missing, glossary-term): every language of the table holds every key of its scope
/// as a text with a Translated or Approved state, and holds nothing else; every key in any scope
/// exists in the base. A run prints one `scope` note per language.
pub fn checkOverlays(io: std.Io, report: *Report, root: std.Io.Dir) !void {
    const arena = report.arena;
    const scopes = (try loadScopes(io, arena, root, report)) orelse return;

    const manifest: ?Manifest = if (try readOptional(io, arena, root, manifest_path)) |text| try parseManifest(arena, text) else null;
    if (manifest == null) try report.add("base-hash-missing", "all", manifest_path, "file is missing; run zig build l10n-check-update-base");

    var bases: std.StringHashMapUnmanaged(?[]u8) = .empty;
    for (scopes.all_keys) |key| {
        report.checked += 1;
        const base_bytes = try readBase(io, arena, root, key);
        try bases.put(arena, key, base_bytes);
        const bytes = base_bytes orelse {
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

    var exceptions = try loadTermExceptions(io, report, root);
    for (languages.languages, scopes.per_language) |language, scope| {
        const terms = try loadGlossary(io, report, root, language.code);
        const dir = try std.fmt.allocPrint(arena, "Localization/game/{s}/game_data_base", .{language.code});
        for (scope.keys) |key| {
            report.checked += 1;
            const base_bytes = bases.get(key) orelse null;
            const txt_path = try std.fmt.allocPrint(arena, "{s}/{s}.txt", .{ dir, key });
            const xml_path = try std.fmt.allocPrint(arena, "{s}/{s}.xml", .{ dir, key });
            if (try readOptional(io, arena, root, txt_path)) |bytes| {
                switch (textProblem(bytes)) {
                    .ok => if (base_bytes) |english| {
                        try checkAgainstBase(report, manifest, language.code, key, english, bytes);
                        try checkTerms(report, &exceptions, terms, language.code, key, english, bytes);
                    },
                    .no_bom => try report.add("encoding", language.code, key, "text lacks the UTF-16LE byte order mark"),
                    .odd_length => try report.addFmt("encoding", language.code, key, "text has an odd byte count ({d})", .{bytes.len}),
                    .unpaired_surrogate => |unit| try report.addFmt("encoding", language.code, key, "unpaired surrogate at unit {d}", .{unit}),
                    // Two tutorial steps (tutorial4/22 and /25) have an empty 2003 original; an empty translation is the faithful one.
                    .empty => if (base_bytes == null or textProblem(base_bytes.?) != .empty) {
                        try report.add("empty", language.code, key, "text holds no characters");
                    },
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
        try checkExtra(io, report, root, scope, dir);
        try report.scope_notes.append(arena, try std.fmt.allocPrint(arena, "l10n-check: scope: {s}: {s}, {d} keys checked", .{ language.code, try scope.describe(arena), scope.keys.len }));
    }
    for (exceptions.items, exceptions.used) |one, used| {
        if (used) continue;
        try report.addFmt("term-exception-unused", one.code, try std.fmt.allocPrint(arena, "{s}:{d}", .{ exceptions_path, one.line }), "`{s}` matches no missing glossary term; remove the line", .{one.rest});
    }
}

/// One glossary row: the English term, its translation and whether the check skips it.
pub const Term = struct { english: []const u8, translation: []const u8, skip: bool };

fn cells(arena: std.mem.Allocator, line: []const u8) !?[]const []const u8 {
    const trimmed = std.mem.trim(u8, line, " \t\r");
    if (trimmed.len < 2 or trimmed[0] != '|') return null;
    var out: std.ArrayList([]const u8) = .empty;
    var parts = std.mem.splitScalar(u8, std.mem.trimEnd(u8, trimmed[1..], "|"), '|');
    while (parts.next()) |part| try out.append(arena, std.mem.trim(u8, part, " \t"));
    return out.items;
}

/// The rows of every table in a glossary (`English term | Translation | Note`). A row whose Note
/// holds `no-term-check` is marked to skip (a word that is also ordinary English).
pub fn parseGlossary(arena: std.mem.Allocator, text: []const u8) ![]const Term {
    var terms: std.ArrayList(Term) = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |line| {
        const row = (try cells(arena, line)) orelse continue;
        if (row.len < 2 or row[0].len == 0 or row[1].len == 0) continue;
        if (std.mem.eql(u8, row[0], "English term") or std.mem.startsWith(u8, row[0], "---")) continue;
        const note = if (row.len > 2) row[2] else "";
        try terms.append(arena, .{ .english = row[0], .translation = row[1], .skip = std.mem.indexOf(u8, note, "no-term-check") != null });
    }
    return terms.items;
}

fn loadGlossary(io: std.Io, report: *Report, root: std.Io.Dir, code: []const u8) ![]const Term {
    const arena = report.arena;
    const path = try std.fmt.allocPrint(arena, "Localization/glossary/{s}.md", .{code});
    const text = (try readOptional(io, arena, root, path)) orelse {
        try report.add("glossary-term", code, path, "glossary file is missing");
        return &.{};
    };
    return parseGlossary(arena, text);
}

pub const TermException = struct { code: []const u8, rest: []const u8, line: usize };

/// The exceptions of `term-exceptions.txt` and whether each one excused a missing term.
pub const TermExceptions = struct {
    items: []const TermException = &.{},
    used: []bool = &.{},

    fn excuses(self: *TermExceptions, code: []const u8, key: []const u8, term: []const u8, scratch: *std.ArrayList(u8), arena: std.mem.Allocator) !bool {
        scratch.clearRetainingCapacity();
        try scratch.appendSlice(arena, key);
        try scratch.append(arena, ' ');
        try scratch.appendSlice(arena, term);
        for (self.items, 0..) |one, i| {
            if (std.mem.eql(u8, one.code, code) and std.ascii.eqlIgnoreCase(one.rest, scratch.items)) {
                self.used[i] = true;
                return true;
            }
        }
        return false;
    }
};

/// The lines `<code> <key> <english term> # <reason>`. A key may hold a space, so the entry is kept
/// as `<key> <english term>` and compared whole. A line without a reason, or of another shape, is
/// a `term-exception` finding.
pub fn parseTermExceptions(arena: std.mem.Allocator, text: []const u8, problems: *std.ArrayList(ScopeProblem)) ![]const TermException {
    var items: std.ArrayList(TermException) = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    var number: usize = 0;
    while (lines.next()) |raw| {
        number += 1;
        const line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0 or line[0] == '#') continue;
        const hash = std.mem.indexOfScalar(u8, line, '#');
        if (hash == null or std.mem.trim(u8, line[hash.? + 1 ..], " \t").len == 0) {
            try problems.append(arena, .{ .line = number, .detail = "an exception needs a reason after `#`" });
            continue;
        }
        const entry = std.mem.trim(u8, line[0..hash.?], " \t");
        const space = std.mem.indexOfScalar(u8, entry, ' ');
        const rest = if (space) |s| std.mem.trimStart(u8, entry[s + 1 ..], " ") else "";
        if (space == null or std.mem.indexOfScalar(u8, rest, ' ') == null) {
            try problems.append(arena, .{ .line = number, .detail = "want `<code> <key> <english term> # <reason>`" });
        } else if (!knownCode(entry[0..space.?])) {
            try problems.append(arena, .{ .line = number, .detail = try std.fmt.allocPrint(arena, "unknown language code `{s}`", .{entry[0..space.?]}) });
        } else try items.append(arena, .{ .code = entry[0..space.?], .rest = rest, .line = number });
    }
    return items.items;
}

fn loadTermExceptions(io: std.Io, report: *Report, root: std.Io.Dir) !TermExceptions {
    const arena = report.arena;
    const text = (try readOptional(io, arena, root, exceptions_path)) orelse return .{};
    var problems: std.ArrayList(ScopeProblem) = .empty;
    const items = try parseTermExceptions(arena, text, &problems);
    for (problems.items) |problem| {
        try report.addFmt("term-exception", "all", try std.fmt.allocPrint(arena, "{s}:{d}", .{ exceptions_path, problem.line }), "{s}", .{problem.detail});
    }
    const used = try arena.alloc(bool, items.len);
    @memset(used, false);
    return .{ .items = items, .used = used };
}

/// Simple case folding for the scripts the nine languages use: ASCII, Latin-1, Latin Extended-A,
/// Greek and Cyrillic. Other code points fold to themselves.
fn foldCase(c: u21) u21 {
    if (c < 0x80) return std.ascii.toLower(@intCast(c));
    if (c >= 0xC0 and c <= 0xDE and c != 0xD7) return c + 0x20;
    if (c >= 0x100 and c <= 0x137) return c | 1;
    if (c >= 0x139 and c <= 0x148) return if (c % 2 == 1) c + 1 else c;
    if (c >= 0x14A and c <= 0x177) return c | 1;
    if (c == 0x178) return 0xFF;
    if (c >= 0x179 and c <= 0x17E) return if (c % 2 == 1) c + 1 else c;
    if (c >= 0x391 and c <= 0x3A9) return c + 0x20;
    if (c >= 0x410 and c <= 0x42F) return c + 0x20;
    if (c >= 0x400 and c <= 0x40F) return c + 0x50;
    return c;
}

fn isSpaceUnit(c: u21) bool {
    return c == ' ' or c == '\t' or c == '\r' or c == '\n' or c == 0xA0 or c == 0xFEFF;
}

/// A word unit for the whole-word rule: an ASCII letter or digit, or a letter beyond Latin-1.
fn isWordUnit(c: u21) bool {
    return (c < 0x80 and std.ascii.isAlphanumeric(@intCast(c))) or c >= 0xC0;
}

/// Case-folded code points of a UTF-16LE text after its byte order mark (surrogates combined, white
/// space runs one space), so a term finds a phrase that wraps over a line break.
fn foldedUnits(arena: std.mem.Allocator, bytes: []const u8) ![]u21 {
    var out: std.ArrayList(u21) = .empty;
    var at: usize = 2;
    while (at + 1 < bytes.len) : (at += 2) {
        var unit: u21 = std.mem.readInt(u16, bytes[at..][0..2], .little);
        if (unit >= 0xD800 and unit <= 0xDBFF and at + 3 < bytes.len) {
            const low = std.mem.readInt(u16, bytes[at + 2 ..][0..2], .little);
            if (low >= 0xDC00 and low <= 0xDFFF) {
                unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                at += 2;
            }
        }
        try appendFolded(arena, &out, unit);
    }
    return out.items;
}

fn appendFolded(arena: std.mem.Allocator, out: *std.ArrayList(u21), unit: u21) !void {
    if (isSpaceUnit(unit)) {
        if (out.items.len != 0 and out.items[out.items.len - 1] == ' ') return;
        try out.append(arena, ' ');
    } else try out.append(arena, foldCase(unit));
}

fn foldedUtf8(arena: std.mem.Allocator, text: []const u8) ![]u21 {
    var out: std.ArrayList(u21) = .empty;
    var view = std.unicode.Utf8View.init(text) catch return out.items;
    var it = view.iterator();
    while (it.nextCodepoint()) |c| try appendFolded(arena, &out, c);
    return out.items;
}

fn findUnits(hay: []const u21, needle: []const u21, whole_word: bool) bool {
    if (needle.len == 0 or needle.len > hay.len) return false;
    var at: usize = 0;
    while (at + needle.len <= hay.len) : (at += 1) {
        if (!std.mem.eql(u21, hay[at..][0..needle.len], needle)) continue;
        if (!whole_word) return true;
        const before = at == 0 or !isWordUnit(hay[at - 1]);
        const after = at + needle.len == hay.len or !isWordUnit(hay[at + needle.len]);
        if (before and after) return true;
    }
    return false;
}

/// Rule glossary-term (D098): each glossary term (not marked no-term-check) that occurs as a whole
/// word in the English original must occur, case-insensitively, in the translation. A line of
/// `term-exceptions.txt` excuses one key and term.
fn checkTerms(report: *Report, exceptions: *TermExceptions, terms: []const Term, code: []const u8, key: []const u8, english: []const u8, translation: []const u8) !void {
    if (terms.len == 0) return;
    const arena = report.arena;
    const english_units = try foldedUnits(arena, english);
    var translated: ?[]u21 = null;
    var scratch: std.ArrayList(u8) = .empty;
    for (terms) |term| {
        if (term.skip) continue;
        if (!findUnits(english_units, try foldedUtf8(arena, term.english), true)) continue;
        if (translated == null) translated = try foldedUnits(arena, translation);
        if (findUnits(translated.?, try foldedUtf8(arena, term.translation), false)) continue;
        if (try exceptions.excuses(code, key, term.english, &scratch, arena)) continue;
        try report.addFmt("glossary-term", code, key, "English \"{s}\" needs \"{s}\" in the translation (Localization/glossary/{s}.md); use it, or add `{s} {s} {s} # reason` to {s}", .{ term.english, term.translation, code, code, key, term.english, exceptions_path });
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

/// Rule glyph: every code point of every in-scope overlay text is covered by the language's face or by
/// NotoSans-Regular, the two faces bk-font generates its mod's fonts from (language-mods passes Noto
/// Sans as `--fallback-ttf`). One finding per code point.
pub fn checkGlyphs(io: std.Io, report: *Report, root: std.Io.Dir, fonts: std.Io.Dir) !void {
    const arena = report.arena;
    const scopes = (try loadScopes(io, arena, root, null)) orelse return;
    const latin = Face.load(io, arena, fonts, "NotoSans-Regular.ttf");
    if (latin.table == null) try report.add("glyph", "all", latin.name, "font is missing or has no usable cmap");
    for (languages.languages, scopes.per_language) |language, scope| {
        const own = Face.load(io, arena, fonts, language.face);
        if (own.table == null) try report.add("glyph", language.code, language.face, "font is missing or has no usable cmap");
        for (scope.keys) |key| {
            const path = try std.fmt.allocPrint(arena, "Localization/game/{s}/game_data_base/{s}.txt", .{ language.code, key });
            const bytes = (try readOptional(io, arena, root, path)) orelse continue;
            if (textProblem(bytes) != .ok) continue;
            for (try codePoints(arena, bytes)) |code_point| {
                if (own.table == null or own.covers(code_point) or latin.covers(code_point)) continue;
                try report.addFmt("glyph", language.code, key, "U+{X:0>4} not in {s}", .{ code_point, own.name });
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
/// `units` without a byte order mark and the white space around the text.
fn trimUnits(units: []const u16) []const u16 {
    var text = units;
    while (text.len != 0 and (text[0] == 0xfeff or text[0] == ' ' or text[0] == '\r' or text[0] == '\n')) text = text[1..];
    while (text.len != 0 and (text[text.len - 1] == ' ' or text[text.len - 1] == '\r' or text[text.len - 1] == '\n')) text = text[0 .. text.len - 1];
    return text;
}

/// The face the editors draw a language with besides Noto Sans (Sources/editor/kit/fonts.zig merges
/// them): Noto Sans alone for the Latin and Cyrillic languages, whatever face their game mods use.
fn editorFace(code: []const u8) ?[]const u8 {
    if (std.mem.eql(u8, code, "zh-Hans")) return "NotoSansSC-Regular.otf";
    if (std.mem.eql(u8, code, "ja")) return "NotoSansJP-Regular.otf";
    if (std.mem.eql(u8, code, "hi")) return "NotoSansDevanagari-Regular.ttf";
    return null;
}

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
        const own: ?Face = if (fonts) |f| (if (editorFace(language.code)) |name| Face.load(io, arena, f, name) else null) else null;
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
/// and README_<Language>.md in the repository root.
pub const document_sets = [_][]const u8{
    "README",
    "tools/guide/README",
    "Data/Editor/TestProjects/README",
    "Data/Editor/TestProjects/01_GUI/README",
    "Data/Editor/TestProjects/02_InfantryAnimation/README",
    "Data/Editor/TestProjects/03_Sprite/README",
    "Data/Editor/TestProjects/04_Effect/README",
    "Data/Editor/TestProjects/05_Object/README",
    "Data/Editor/TestProjects/06_Mesh/README",
    "Data/Editor/TestProjects/07_Weapon/README",
    "Data/Editor/TestProjects/08_Building/README",
    "Data/Editor/TestProjects/09_TileSet/README",
    "Data/Editor/TestProjects/10_Fence/README",
    "Data/Editor/TestProjects/11_Particle/README",
    "Data/Editor/TestProjects/12_Trench/README",
    "Data/Editor/TestProjects/13_Squad/README",
    "Data/Editor/TestProjects/14_Mine/README",
    "Data/Editor/TestProjects/15_Bridge/README",
    "Data/Editor/TestProjects/16_Mission/README",
    "Data/Editor/TestProjects/17_Chapter/README",
    "Data/Editor/TestProjects/18_Campaign/README",
    "Data/Editor/TestProjects/19_3DRoad/README",
    "Data/Editor/TestProjects/20_3DRiver/README",
    "Data/Editor/TestProjects/21_Medal/README",
};

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
pub fn checkDocuments(io: std.Io, report: *Report, root: std.Io.Dir, defaults: []const []const u8, extra: []const []const u8) !void {
    const arena = report.arena;
    if (defaults.len == 0 and extra.len == 0) {
        try report.inactive.append(arena, "documents");
        return;
    }
    for ([_][]const []const u8{ defaults, extra }) |sets| for (sets) |stem| {
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

/// Directories whose Zig sources must not hold an English UI literal (M002 S04 to S06 migrated the
/// app directories to `l10n.t`); `--strict-dir <dir>` adds one for a fixture.
pub const strict_dirs = [_][]const u8{ "Sources/editor/elk/app", "Sources/editor/app", "Sources/editor/resource_app" };

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

/// The calls whose first argument is a label ImGui hashes into a widget ID. A translation there must
/// be `l10n.id` (`shown###key`), or the ID changes with the language and equal translations collide.
/// Plain text (igText*, igSeparatorText, tooltips, igCalcTextSize) is not on the list and keeps `l10n.t`.
const id_calls = [_][]const u8{
    "igButton",        "igSmallButton",        "igMenuItem",       "igMenuItemEx",      "igMenuItemBoolPtr", "igBeginMenu",
    "igBeginMenuEx",   "igSelectable",         "igSelectableEx",   "igCheckbox",        "igRadioButton",     "igCombo",
    "igBeginCombo",    "igInputText",          "igInputTextWithHint", "igInputFloat",   "igInputFloatEx",    "igInputInt",
    "igInputIntEx",    "igSliderInt",          "igSliderFloat",    "igSliderFloatEx",   "igBeginTabItem",    "igCollapsingHeader",
    "igTreeNode",      "igBeginPopup",         "igBeginPopupModal", "igOpenPopup",      "igBegin",           "igBeginChild",
    "igTableSetupColumn", "igTableSetupColumnEx",
};

/// Calls whose second argument is user-visible too: the hint of an input field.
const hint_calls = [_][]const u8{"igInputTextWithHint"};

/// Status-line helpers: every argument that is a string literal is shown to the user.
const status_calls = [_][]const u8{ "setStatus", "setStatusFrom", "note", "say", "sayImport" };

/// The status calls whose literal is a string table key, not a text: `say(&buffer, "app.saved", .{})`.
const key_calls = [_][]const u8{ "say", "sayImport" };

/// Whether a literal is shaped like a string table key: lowercase words joined by dots.
fn isKeyShaped(literal: []const u8) bool {
    if (std.mem.indexOfScalar(u8, literal, '.') == null) return false;
    for (literal) |ch| {
        if (!std.ascii.isLower(ch) and !std.ascii.isDigit(ch) and ch != '_' and ch != '.') return false;
    }
    return true;
}

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
            while (i + 1 < literal.len and std.mem.indexOfScalar(u8, "%-+ #0123456789.*", literal[i + 1]) != null) i += 1;
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
                if (literalAt(line, i)) |literal| {
                    if (!(inList(&key_calls, name) and isKeyShaped(literal))) return literal;
                }
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

/// `bytes` with comments and the inside of string and character literals blanked to spaces, line
/// breaks kept, so a scan over the whole file never matches text that is not code.
fn maskCode(arena: std.mem.Allocator, bytes: []const u8) ![]u8 {
    const out = try arena.dupe(u8, bytes);
    var i: usize = 0;
    while (i < out.len) {
        const ch = out[i];
        if (ch == '/' and i + 1 < out.len and out[i + 1] == '/') {
            while (i < out.len and out[i] != '\n') : (i += 1) out[i] = ' ';
        } else if (ch == '\\' and i + 1 < out.len and out[i + 1] == '\\') {
            while (i < out.len and out[i] != '\n') : (i += 1) out[i] = ' ';
        } else if (ch == '"' or ch == '\'') {
            i += 1;
            while (i < out.len and out[i] != ch and out[i] != '\n') {
                const step: usize = if (out[i] == '\\') 2 else 1;
                var k: usize = 0;
                while (k < step and i < out.len) : (k += 1) {
                    if (out[i] != '\n') out[i] = ' ';
                    i += 1;
                }
            }
            if (i < out.len and out[i] == ch) i += 1;
        } else i += 1;
    }
    return out;
}

fn skipSpace(code: []const u8, from: usize) usize {
    var i = from;
    while (i < code.len and std.ascii.isWhitespace(code[i])) i += 1;
    return i;
}

fn lineOf(bytes: []const u8, at: usize) usize {
    return std.mem.count(u8, bytes[0..at], "\n") + 1;
}

/// Rule literal, ID part, over the whole file so a call `zig fmt` wrapped is seen: an ID-bearing call
/// whose first argument is `l10n.t(...)`, and a binding of `l10n.t` (an alias) outside kit/l10n.zig,
/// which would hide such calls from this scan.
fn scanIdRule(report: *Report, path: []const u8, bytes: []const u8) !void {
    const arena = report.arena;
    const code = try maskCode(arena, bytes);
    for (id_calls) |call| {
        var from: usize = 0;
        while (std.mem.indexOfPos(u8, code, from, call)) |at| {
            from = at + call.len;
            if (at > 0 and (isIdentChar(code[at - 1]) or code[at - 1] == '@')) continue;
            if (from < code.len and isIdentChar(code[from])) continue;
            const open = skipSpace(code, from);
            if (open >= code.len or code[open] != '(') continue;
            const arg = skipSpace(code, open + 1);
            if (!std.mem.startsWith(u8, code[arg..], "l10n.t(")) continue;
            const where = try std.fmt.allocPrint(arena, "{s}:{d}", .{ path, lineOf(code, at) });
            try report.addFmt("literal", "all", where, "{s} takes l10n.t as its label; use l10n.id so the ImGui ID does not depend on the language", .{call});
        }
    }
    if (std.mem.endsWith(u8, path, "kit/l10n.zig")) return;
    var from: usize = 0;
    while (std.mem.indexOfPos(u8, code, from, "l10n.t")) |at| {
        from = at + "l10n.t".len;
        if (at > 0 and isIdentChar(code[at - 1])) continue;
        if (from < code.len and isIdentChar(code[from])) continue;
        const next = skipSpace(code, from);
        if (next >= code.len or (code[next] != ';' and code[next] != ',')) continue;
        const where = try std.fmt.allocPrint(arena, "{s}:{d}", .{ path, lineOf(code, at) });
        try report.addFmt("literal", "all", where, "l10n.t is bound to a name here; call l10n.t directly so the ID rule can see which labels are translated", .{});
    }
}

/// Orders one file's findings by line; the sort is stable enough because a call reports once per line.
fn byLine(_: void, a: Finding, b: Finding) bool {
    return lineNumber(a.where) < lineNumber(b.where);
}

fn lineNumber(where: []const u8) usize {
    const colon = std.mem.lastIndexOfScalar(u8, where, ':') orelse return 0;
    return std.fmt.parseInt(usize, where[colon + 1 ..], 10) catch 0;
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
            const first_finding = report.findings.items.len;
            var lines = std.mem.splitScalar(u8, bytes, '\n');
            var number: usize = 0;
            while (lines.next()) |raw| {
                number += 1;
                const line = std.mem.trimEnd(u8, raw, "\r");
                const where = try std.fmt.allocPrint(arena, "{s}:{d}", .{ path, number });
                const found = literalCall(line) orelse continue;
                try report.addFmt("literal", "all", where, "{s} takes the English literal \"{s}\"; wrap it in l10n.t, l10n.id or l10n.format", .{ found.call, found.literal });
            }
            try scanIdRule(report, path, bytes);
            std.mem.sort(Finding, report.findings.items[first_finding..], {}, byLine);
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

/// Rewrites `Localization/game/base-hashes.txt` from the English bytes of every `.elk` original
/// (the whole database, not only the scope keys, so a language can widen its scope without a new
/// hash run) and every seed key, keeping the `same-as-base` lines. A seed key without an original
/// is a finding and no file is written.
pub fn updateBase(io: std.Io, report: *Report, root: std.Io.Dir) !void {
    const arena = report.arena;
    const scope_text = (try readOptional(io, arena, root, scope_path)) orelse {
        try report.add("scope", "all", scope_path, "file is missing");
        return;
    };
    const seed = (try parseScopeFile(arena, scope_text)).seed;
    var seen: std.StringHashMapUnmanaged(void) = .empty;
    var keys: std.ArrayList([]const u8) = .empty;
    for ([_][]const []const u8{ seed, try elkKeys(io, arena, root) }) |list| for (list) |key| {
        if (try firstSight(arena, &seen, key)) try keys.append(arena, key);
    };
    std.mem.sort([]const u8, keys.items, {}, lessString);
    const old: Manifest = if (try readOptional(io, arena, root, manifest_path)) |text| try parseManifest(arena, text) else .{ .keys = &.{}, .hashes = &.{}, .same_as_base = &.{} };

    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(arena, manifest_header);
    for (keys.items) |key| {
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
    "# Base-text hashes of the l10n-check gate (M002 D075), one `<key> <sha256 hex>` line per .elk original and seed key.\r\n" ++
    "# The hash is of the English bytes in Data/ELK/game_data_base (the .elk original, else the .txt).\r\n" ++
    "# l10n-check reports `stale` when the English text no longer matches its hash: review the\r\n" ++
    "# translations of that key in every language, then rewrite this file with\r\n" ++
    "#   zig build l10n-check-update-base\r\n" ++
    "# A `same-as-base <code> <key>` line lets that translation equal the English (a proper name);\r\n" ++
    "# the update keeps those lines.\r\n";

fn checkExtra(io: std.Io, report: *Report, root: std.Io.Dir, scope: LanguageScope, base: []const u8) !void {
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
        if (!scope.contains(key)) try report.add("extra", scope.code, key, scope.rule());
    }
}

pub const fit_map_path = "Localization/game/fit-map.txt";
pub const fit_known_path = "Localization/game/fit-known.txt";

pub const FitKind = enum { box, area, tooltip, scroll };
pub const FitFont = enum { small, medium, large };

/// One line of `fit-map.txt`. A `box` line names a layout element, an `area` line gives the
/// rectangle itself, a `tooltip` line the hint, a `scroll` line a text that scrolls.
pub const FitLine = struct {
    kind: FitKind,
    /// Lowercase, as the keys are compared.
    glob: []const u8,
    layout: []const u8 = "",
    /// `<ElementID>` or `<ElementID>#<n>` for the n-th element of the file with that ID.
    element: []const u8 = "",
    width: f32 = 0,
    height: f32 = 0,
    font: FitFont = .medium,
    line: usize,
};

fn parseElementRef(ref: []const u8) ?struct { id: i64, nth: usize } {
    const hash = std.mem.indexOfScalar(u8, ref, '#');
    const id = std.fmt.parseInt(i64, if (hash) |h| ref[0..h] else ref, 10) catch return null;
    const nth = if (hash) |h| std.fmt.parseInt(usize, ref[h + 1 ..], 10) catch return null else 1;
    if (nth == 0) return null;
    return .{ .id = id, .nth = nth };
}

/// The lines of a `fit-map.txt`. A line is a directive and its words; ` # ` starts a comment, and
/// is the reason a `scroll` line must give. Problems are returned, not reported, so the parser
/// stays pure.
pub fn parseFitMap(arena: std.mem.Allocator, text: []const u8, problems: *std.ArrayList(ScopeProblem)) ![]const FitLine {
    var out: std.ArrayList(FitLine) = .empty;
    var raw_lines = std.mem.splitScalar(u8, text, '\n');
    var number: usize = 0;
    while (raw_lines.next()) |raw| {
        number += 1;
        var line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0 or line[0] == '#') continue;
        var reason: []const u8 = "";
        if (std.mem.indexOf(u8, line, " # ")) |at| {
            reason = std.mem.trim(u8, line[at + 3 ..], " \t");
            line = std.mem.trimEnd(u8, line[0..at], " \t");
        } else if (std.mem.endsWith(u8, line, " #")) line = std.mem.trimEnd(u8, line[0 .. line.len - 2], " \t");
        var words: [6][]const u8 = undefined;
        var count: usize = 0;
        var split = std.mem.tokenizeAny(u8, line, " \t");
        while (split.next()) |word| {
            if (count == words.len) {
                count += 1;
                break;
            }
            words[count] = word;
            count += 1;
        }
        const glob = if (count > 1 and count <= words.len) try std.ascii.allocLowerString(arena, words[1]) else "";
        if (std.mem.eql(u8, words[0], "box")) {
            if (count != 4) {
                try problems.append(arena, .{ .line = number, .detail = "box wants `box <glob> <Data/UI layout> <ElementID>`" });
            } else if (parseElementRef(words[3]) == null) {
                try problems.append(arena, .{ .line = number, .detail = try std.fmt.allocPrint(arena, "`{s}` is not an ElementID (a number, then optionally `#n`)", .{words[3]}) });
            } else try out.append(arena, .{ .kind = .box, .glob = glob, .layout = try arena.dupe(u8, words[2]), .element = try arena.dupe(u8, words[3]), .line = number });
        } else if (std.mem.eql(u8, words[0], "area")) {
            const x = if (count == 4) std.mem.indexOfScalar(u8, words[2], 'x') else null;
            const width = if (x) |at| std.fmt.parseInt(u32, words[2][0..at], 10) catch null else null;
            const height = if (x) |at| std.fmt.parseInt(u32, words[2][at + 1 ..], 10) catch null else null;
            const font = if (count == 4) std.meta.stringToEnum(FitFont, words[3]) else null;
            if (width == null or height == null or font == null) {
                try problems.append(arena, .{ .line = number, .detail = "area wants `area <glob> <W>x<H> <small|medium|large>`" });
            } else try out.append(arena, .{ .kind = .area, .glob = glob, .width = @floatFromInt(width.?), .height = @floatFromInt(height.?), .font = font.?, .line = number });
        } else if (std.mem.eql(u8, words[0], "tooltip")) {
            if (count != 2) {
                try problems.append(arena, .{ .line = number, .detail = "tooltip wants `tooltip <glob>`" });
            } else try out.append(arena, .{ .kind = .tooltip, .glob = glob, .font = .large, .line = number });
        } else if (std.mem.eql(u8, words[0], "scroll")) {
            if (count != 2) {
                try problems.append(arena, .{ .line = number, .detail = "scroll wants `scroll <glob> # <reason>`" });
            } else if (reason.len == 0) {
                try problems.append(arena, .{ .line = number, .detail = try std.fmt.allocPrint(arena, "scroll `{s}` gives no reason; write `scroll {s} # why this text scrolls`", .{ words[1], words[1] }) });
            } else try out.append(arena, .{ .kind = .scroll, .glob = glob, .line = number });
        } else try problems.append(arena, .{ .line = number, .detail = try std.fmt.allocPrint(arena, "unknown directive `{s}`", .{words[0]}) });
    }
    return out.items;
}

/// Whether the lowercase `text` matches the lowercase `pattern`: `*` runs over any characters but
/// `/`, `**` over any, `[0-9]+` over one or more digits; every other character stands for itself.
pub fn globMatch(pattern: []const u8, text: []const u8) bool {
    if (pattern.len == 0) return text.len == 0;
    if (std.mem.startsWith(u8, pattern, "**")) {
        var at: usize = 0;
        while (at <= text.len) : (at += 1) {
            if (globMatch(pattern[2..], text[at..])) return true;
        }
        return false;
    }
    if (pattern[0] == '*') {
        var at: usize = 0;
        while (true) : (at += 1) {
            if (globMatch(pattern[1..], text[at..])) return true;
            if (at == text.len or text[at] == '/') return false;
        }
    }
    if (std.mem.startsWith(u8, pattern, "[0-9]+")) {
        var at: usize = 0;
        while (at < text.len and std.ascii.isDigit(text[at])) {
            at += 1;
            if (globMatch(pattern[6..], text[at..])) return true;
        }
        return false;
    }
    if (text.len == 0 or pattern[0] != text[0]) return false;
    return globMatch(pattern[1..], text[1..]);
}

/// The layout elements text is drawn in: UI elements (`item` or `Element` tags with a ClassTypeID)
/// with their size, text inset, text alignment and font size.
pub const LayoutElement = struct {
    id: i64,
    /// FontSize attribute; the engine's default is 1 (medium).
    font: u8 = 1,
    /// TextAlign attribute; the engine's default is 0x0011 (left, top).
    text_align: u32 = 0x0011,
    width: f32 = 0,
    height: f32 = 0,
    inset_x: f32 = 0,
    inset_y: f32 = 0,
    has_size: bool = false,

    /// The room the text has (CSimpleWindow::VisitText): a left or right aligned text loses the
    /// TextPos inset in x, a top or bottom aligned one in y; centred text keeps the whole size.
    pub fn textWidth(self: LayoutElement) f32 {
        const horizontal = self.text_align & 0xf;
        return if (horizontal == 1 or horizontal == 3) self.width - self.inset_x else self.width;
    }

    pub fn textHeight(self: LayoutElement) f32 {
        const vertical = self.text_align & 0xf0;
        return if (vertical == 0x10 or vertical == 0x30) self.height - self.inset_y else self.height;
    }
};

fn tagAttribute(tag: []const u8, name: []const u8) ?[]const u8 {
    var at: usize = 0;
    while (std.mem.indexOfPos(u8, tag, at, name)) |found| {
        at = found + name.len;
        const before_ok = found == 0 or tag[found - 1] == ' ' or tag[found - 1] == '\t' or tag[found - 1] == '\r' or tag[found - 1] == '\n';
        if (!before_ok) continue;
        var cursor = at;
        while (cursor < tag.len and tag[cursor] == ' ') cursor += 1;
        if (cursor >= tag.len or tag[cursor] != '=') continue;
        cursor += 1;
        while (cursor < tag.len and tag[cursor] == ' ') cursor += 1;
        if (cursor >= tag.len or (tag[cursor] != '"' and tag[cursor] != '\'')) continue;
        const quote = tag[cursor];
        const end = std.mem.indexOfScalarPos(u8, tag, cursor + 1, quote) orelse return null;
        return tag[cursor + 1 .. end];
    }
    return null;
}

fn attributeNumber(value: ?[]const u8) f32 {
    const text = std.mem.trim(u8, value orelse return 0, " \t");
    return std.fmt.parseFloat(f32, text) catch 0;
}

/// The elements of a layout in document order. A hand-written scan, not an XML parser: the layouts
/// are well-formed, and only the tags `item`/`Element` (with ClassTypeID), `WindowSize` and
/// `TextPos` directly inside one matter.
pub fn parseLayout(arena: std.mem.Allocator, xml: []const u8) ![]const LayoutElement {
    var elements: std.ArrayList(LayoutElement) = .empty;
    const Open = struct { name: []const u8, element: ?usize };
    var stack: std.ArrayList(Open) = .empty;
    var at: usize = 0;
    while (std.mem.indexOfScalarPos(u8, xml, at, '<')) |open| {
        if (std.mem.startsWith(u8, xml[open..], "<!--")) {
            at = if (std.mem.indexOfPos(u8, xml, open, "-->")) |end| end + 3 else xml.len;
            continue;
        }
        if (std.mem.startsWith(u8, xml[open..], "<?") or std.mem.startsWith(u8, xml[open..], "<!")) {
            at = if (std.mem.indexOfScalarPos(u8, xml, open, '>')) |end| end + 1 else xml.len;
            continue;
        }
        // The end of the tag: the first `>` outside quotes.
        var cursor = open + 1;
        var quote: u8 = 0;
        while (cursor < xml.len) : (cursor += 1) {
            const c = xml[cursor];
            if (quote != 0) {
                if (c == quote) quote = 0;
            } else if (c == '"' or c == '\'') {
                quote = c;
            } else if (c == '>') break;
        }
        if (cursor >= xml.len) break;
        at = cursor + 1;
        var tag = xml[open + 1 .. cursor];
        if (tag.len != 0 and tag[0] == '/') {
            if (stack.items.len != 0) _ = stack.pop();
            continue;
        }
        const self_closing = tag.len != 0 and tag[tag.len - 1] == '/';
        if (self_closing) tag = tag[0 .. tag.len - 1];
        const name_end = std.mem.indexOfAny(u8, tag, " \t\r\n") orelse tag.len;
        const name = tag[0..name_end];
        const parent: ?usize = if (stack.items.len != 0) stack.items[stack.items.len - 1].element else null;
        var element: ?usize = null;
        if ((std.mem.eql(u8, name, "item") or std.mem.eql(u8, name, "Element")) and tagAttribute(tag, "ClassTypeID") != null) {
            var one: LayoutElement = .{ .id = std.fmt.parseInt(i64, std.mem.trim(u8, tagAttribute(tag, "ElementID") orelse "0", " \t"), 10) catch 0 };
            if (tagAttribute(tag, "FontSize")) |value| one.font = std.fmt.parseInt(u8, std.mem.trim(u8, value, " \t"), 10) catch 1;
            if (tagAttribute(tag, "TextAlign")) |value| one.text_align = std.fmt.parseInt(u32, std.mem.trim(u8, value, " \t"), 0) catch 0x0011;
            try elements.append(arena, one);
            element = elements.items.len - 1;
        } else if (parent) |owner| {
            if (std.mem.eql(u8, name, "WindowSize")) {
                elements.items[owner].width = attributeNumber(tagAttribute(tag, "x"));
                elements.items[owner].height = attributeNumber(tagAttribute(tag, "y"));
                elements.items[owner].has_size = true;
            } else if (std.mem.eql(u8, name, "TextPos")) {
                elements.items[owner].inset_x = attributeNumber(tagAttribute(tag, "x"));
                elements.items[owner].inset_y = attributeNumber(tagAttribute(tag, "y"));
            }
        }
        if (!self_closing) try stack.append(arena, .{ .name = name, .element = element });
    }
    return elements.items;
}

/// The three bitmap fonts of a mod, `<folder>/{small,medium,large}/1.tfd`. The fit rule no longer measures
/// with them (M004 S04: it measures with bk_text); the l10n-game-reads-it tier still predicts captured
/// lines with them until S05 retires the bitmap fonts.
pub const FitFonts = struct {
    fonts: [3]font_file.Font,

    pub fn get(self: *const FitFonts, size: FitFont) *const font_file.Font {
        return &self.fonts[@intFromEnum(size)];
    }

    /// SceneInternal.cpp SetToolTip: the hint wraps at TOOLTIP_MAX_WIDTH (300, written for the medium
    /// font) grown by the ratio of the large line space to the medium one.
    pub fn tooltipWidth(self: *const FitFonts) f32 {
        const large = self.get(.large).lineSpace();
        const medium = self.get(.medium).lineSpace();
        if (medium <= 0 or large <= medium) return 300;
        return @floatFromInt(@divTrunc(300 * large, medium));
    }
};

pub const FontProblem = struct { path: []const u8, reason: []const u8 };

/// The three fonts below `folder` of `root`.
pub fn loadFitFontsIn(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, folder: []const u8, problem: *FontProblem) !FitFonts {
    var fonts: FitFonts = undefined;
    for (std.meta.tags(FitFont)) |size| {
        const rel = try std.fmt.allocPrint(arena, "{s}/{s}/1.tfd", .{ folder, @tagName(size) });
        problem.path = rel;
        const found = (try findIgnoreCase(io, arena, root, rel)) orelse {
            problem.reason = "file is missing";
            return error.FontUnreadable;
        };
        const bytes = (try readOptional(io, arena, root, found)) orelse {
            problem.reason = "file is missing";
            return error.FontUnreadable;
        };
        fonts.fonts[@intFromEnum(size)] = font_file.Font.read(arena, bytes) catch |err| {
            problem.reason = @errorName(err);
            return error.FontUnreadable;
        };
    }
    return fonts;
}

/// SceneInternal.cpp SetToolTip: the hint wraps at TOOLTIP_MAX_WIDTH (300, written for the medium
/// font) grown by the ratio of the large line space to the medium one, in integer division.
pub fn tooltipWidth(medium_space: f32, large_space: f32) f32 {
    const medium: i64 = @intFromFloat(medium_space);
    const large: i64 = @intFromFloat(large_space);
    if (medium <= 0 or large <= medium) return 300;
    return @floatFromInt(@divTrunc(300 * large, medium));
}

/// A fit line with its room worked out: for a `box` line the layout element's text room, for an
/// `area` line its rectangle, for a `tooltip` line the hint width and no height limit.
pub const FitResolved = struct {
    ok: bool = false,
    kind: FitKind = .scroll,
    width: f32 = 0,
    height: f32 = std.math.inf(f32),
    font: FitFont = .medium,
    /// How a finding names the room: `<layout>#<ElementID>`, `area (fit-map line n)` or `tooltip`.
    where: []const u8 = "",
};

/// Works out the room of every line. A `box` line whose layout or element cannot be found is a
/// problem and stays `ok = false`; so does one whose element has no WindowSize.
pub fn resolveFitLines(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, lines: []const FitLine, medium_space: f32, large_space: f32, problems: *std.ArrayList(ScopeProblem)) ![]FitResolved {
    var layouts: std.StringHashMapUnmanaged(?[]const LayoutElement) = .empty;
    const resolved = try arena.alloc(FitResolved, lines.len);
    for (lines, resolved) |line, *one| {
        one.* = .{ .kind = line.kind, .font = line.font };
        switch (line.kind) {
            .scroll => one.ok = true,
            .tooltip => {
                one.ok = true;
                one.width = tooltipWidth(medium_space, large_space);
                one.where = "tooltip";
            },
            .area => {
                one.ok = true;
                one.width = line.width;
                one.height = line.height;
                one.where = try std.fmt.allocPrint(arena, "area {d}x{d} (fit-map line {d})", .{ @as(u32, @intFromFloat(line.width)), @as(u32, @intFromFloat(line.height)), line.line });
            },
            .box => {
                const entry = try layouts.getOrPut(arena, line.layout);
                if (!entry.found_existing) {
                    entry.value_ptr.* = null;
                    if (try findIgnoreCase(io, arena, root, line.layout)) |found| {
                        if (try readOptional(io, arena, root, found)) |xml| entry.value_ptr.* = try parseLayout(arena, xml);
                    }
                }
                const elements = entry.value_ptr.* orelse {
                    try problems.append(arena, .{ .line = line.line, .detail = try std.fmt.allocPrint(arena, "layout {s} does not exist", .{line.layout}) });
                    continue;
                };
                const ref = parseElementRef(line.element).?;
                var seen: usize = 0;
                const element: ?LayoutElement = for (elements) |candidate| {
                    if (candidate.id != ref.id) continue;
                    seen += 1;
                    if (seen == ref.nth) break candidate;
                } else null;
                const found = element orelse {
                    try problems.append(arena, .{ .line = line.line, .detail = try std.fmt.allocPrint(arena, "{s} holds no element {s}", .{ line.layout, line.element }) });
                    continue;
                };
                if (!found.has_size or found.textWidth() <= 0 or found.textHeight() <= 0) {
                    try problems.append(arena, .{ .line = line.line, .detail = try std.fmt.allocPrint(arena, "{s} element {s} has no room for text ({d} x {d})", .{ line.layout, line.element, found.textWidth(), found.textHeight() }) });
                    continue;
                }
                one.ok = true;
                one.width = found.textWidth();
                one.height = found.textHeight();
                one.font = switch (found.font) {
                    0 => .small,
                    2 => .large,
                    else => .medium,
                };
                one.where = try std.fmt.allocPrint(arena, "{s}#{s}", .{ line.layout, line.element });
            },
        }
    }
    return resolved;
}

/// The lines of `fit-map.txt` that apply to `key` (lowercase): the first match, and every later
/// line with the same glob. Empty when no line matches.
pub fn fitMatches(arena: std.mem.Allocator, lines: []const FitLine, key: []const u8) ![]const usize {
    var out: std.ArrayList(usize) = .empty;
    var glob: ?[]const u8 = null;
    for (lines, 0..) |line, at| {
        if (glob) |g| {
            if (std.mem.eql(u8, g, line.glob)) try out.append(arena, at);
        } else if (globMatch(line.glob, key)) {
            glob = line.glob;
            try out.append(arena, at);
        }
    }
    return out.items;
}

/// The UTF-16 units of a text after its byte order mark.
fn textUnits(arena: std.mem.Allocator, bytes: []const u8) ![]u16 {
    const start: usize = if (bytes.len >= 2 and bytes[0] == 0xff and bytes[1] == 0xfe) 2 else 0;
    const units = try arena.alloc(u16, (bytes.len - start) / 2);
    for (units, 0..) |*unit, at| unit.* = std.mem.readInt(u16, bytes[start + at * 2 ..][0..2], .little);
    return units;
}

/// The shapers of the fit rule: one bk_text face set, and a Shaper per language and size class made when
/// first asked for, at the Game's own pixel size (`bk_text.sizes`). The last slot is English, which
/// measures the 2003 originals. A language's shapers report a glyph no face has into `missing`.
pub const Fit = struct {
    allocator: std.mem.Allocator,
    set: *bk_text.faces.FaceSet,
    shapers: [languages.languages.len + 1][3]?*bk_text.shape.Shaper = @splat(@splat(null)),
    missing: [languages.languages.len]std.AutoArrayHashMapUnmanaged(u21, void) = @splat(.empty),
    sinks: [languages.languages.len]MissingGlyphs = undefined,
    lines: std.ArrayList(bk_text.wrap.Line) = .empty,

    pub const english = languages.languages.len;

    pub fn init(allocator: std.mem.Allocator, io: std.Io, fonts_dir: []const u8) !Fit {
        const set = try allocator.create(bk_text.faces.FaceSet);
        errdefer allocator.destroy(set);
        set.* = try bk_text.faces.FaceSet.load(allocator, io, fonts_dir);
        return .{ .allocator = allocator, .set = set };
    }

    pub fn deinit(self: *Fit) void {
        for (&self.shapers) |*row| for (row) |cell| if (cell) |one| {
            one.deinit();
            self.allocator.destroy(one);
        };
        for (&self.missing) |*one| one.deinit(self.allocator);
        self.lines.deinit(self.allocator);
        self.set.deinit();
        self.allocator.destroy(self.set);
    }

    fn noteMissing(context: ?*anyopaque, cp: u21, _: u32, _: []const u8) void {
        const found: *MissingGlyphs = @ptrCast(@alignCast(context.?));
        found.set.put(found.allocator, cp, {}) catch {};
    }

    const MissingGlyphs = struct {
        allocator: std.mem.Allocator,
        set: *std.AutoArrayHashMapUnmanaged(u21, void),
    };

    /// The shaper of language slot `slot` (an index of `languages.languages`, or `english`) at `size`.
    pub fn shaper(self: *Fit, slot: usize, size: FitFont) !*bk_text.shape.Shaper {
        const cell = &self.shapers[slot][@intFromEnum(size)];
        if (cell.*) |one| return one;
        const code = if (slot == english) "en" else languages.bkTextCode(languages.languages[slot].code);
        const class: bk_text.sizes.SizeClass = switch (size) {
            .small => .small,
            .medium => .medium,
            .large => .large,
        };
        const one = try self.allocator.create(bk_text.shape.Shaper);
        errdefer self.allocator.destroy(one);
        one.* = try bk_text.shape.Shaper.init(self.allocator, self.set, code, bk_text.sizes.pixelSize(class, 1.0));
        if (slot != english) {
            // The Fit stays where it is declared, so the shaper may keep a pointer into it.
            self.sinks[slot] = .{ .allocator = self.allocator, .set = &self.missing[slot] };
            one.sink = .{ .context = &self.sinks[slot], .report = noteMissing };
        }
        cell.* = one;
        return one;
    }

    /// The distance between baselines of `size` in `slot`'s language, in whole pixels.
    pub fn lineSpace(self: *Fit, slot: usize, size: FitFont) !f32 {
        return bk_text.measure.lineSpace(try self.shaper(slot, size));
    }
};

const Measure = struct { lines: usize, word: f32 };

/// The height `lines` lines of `shaper` take: each costs the line space, but the last only its glyph
/// height (the primary face's ascent plus descent), since the leading below a line is not drawn and
/// the game clips at the box (a one line text fits a box as high as the font).
fn fitNeeds(shaper: *const bk_text.shape.Shaper, lines: usize) f32 {
    if (lines == 0) return 0;
    const metrics = bk_text.measure.faceMetrics(shaper, shaper.chain[0]);
    return @as(f32, @floatFromInt(lines - 1)) * bk_text.measure.lineSpace(shaper) + @ceil(metrics.ascent + metrics.descent);
}

/// The lines `units` wrap to in `width` and the widest word. Empty lines at the end draw nothing
/// (a text ends with a line break), so they do not count.
fn measureText(fit: *Fit, shaper: *bk_text.shape.Shaper, units: []const u16, width: f32) !Measure {
    try bk_text.wrap.wrapText(fit.allocator, shaper, units, width, &fit.lines);
    var count = fit.lines.items.len;
    while (count > 0 and fit.lines.items[count - 1].start == fit.lines.items[count - 1].end) count -= 1;
    return .{ .lines = count, .word = try bk_text.wrap.widestUnit(fit.allocator, shaper, units) };
}

/// The space-separated word of `units` that is widest, for a finding to name.
fn wordText(arena: std.mem.Allocator, fit: *Fit, shaper: *bk_text.shape.Shaper, units: []const u16) ![]const u8 {
    var best: []const u16 = &.{};
    var widest: f32 = -1;
    var words = std.mem.tokenizeAny(u16, units, &.{ ' ', '\r', '\n' });
    while (words.next()) |word| {
        const width = try bk_text.wrap.widestUnit(fit.allocator, shaper, word);
        if (width > widest) {
            widest = width;
            best = word;
        }
    }
    return std.unicode.utf16LeToUtf8Alloc(arena, best) catch "?";
}

/// One line of `fit-known.txt`: a text of `code` that overflows its room today and is excused until
/// the owner named in the comment (`S06` rewords it, `M003` translates it) deals with it.
pub const FitKnown = struct { code: []const u8, key: []const u8, line: usize };

/// The lines `<code> <key> # S06|M003` (M004 D108). The key may hold a space, so it is the rest of the
/// entry after the code. A line of another shape, an unknown code or an owner other than S06 and M003
/// is a `fit-known` finding.
pub fn parseFitKnown(arena: std.mem.Allocator, text: []const u8, problems: *std.ArrayList(ScopeProblem)) ![]const FitKnown {
    var items: std.ArrayList(FitKnown) = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    var number: usize = 0;
    while (lines.next()) |raw| {
        number += 1;
        const line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0 or line[0] == '#') continue;
        const hash = std.mem.indexOfScalar(u8, line, '#');
        const owner = if (hash) |h| std.mem.trim(u8, line[h + 1 ..], " \t") else "";
        if (!std.mem.eql(u8, owner, "S06") and !std.mem.eql(u8, owner, "M003")) {
            try problems.append(arena, .{ .line = number, .detail = "want `<code> <key> # S06` or `# M003`: the comment names who fixes the text" });
            continue;
        }
        const entry = std.mem.trim(u8, line[0..hash.?], " \t");
        const space = std.mem.indexOfScalar(u8, entry, ' ');
        const key = if (space) |s| std.mem.trimStart(u8, entry[s + 1 ..], " ") else "";
        if (space == null or key.len == 0) {
            try problems.append(arena, .{ .line = number, .detail = "want `<code> <key> # S06` or `# M003`" });
        } else if (!knownCode(entry[0..space.?])) {
            try problems.append(arena, .{ .line = number, .detail = try std.fmt.allocPrint(arena, "unknown language code `{s}`", .{entry[0..space.?]}) });
        } else try items.append(arena, .{ .code = entry[0..space.?], .key = key, .line = number });
    }
    return items.items;
}

/// Rule `fit` (M003 D097, on bk_text since M004 S04): every translated text must fit the room the game
/// draws it in, measured with the Game's own text engine (HarfBuzz-shaped faces from `fonts_dir` at the
/// Game's size table) and its wrap. `Localization/game/fit-map.txt` says which room each key family has;
/// the first line a key matches decides, and every later line with the same glob applies too, so a text
/// shown in two places is measured in both.
///
/// - `fit`: the wrapped lines times the line space are taller than the box (the game clips at
///   the box, so the last lines vanish). The detail is `<n> lines x <line space> > <height> in <room>`.
/// - `fit-word`: a word is wider than the box or the tooltip, so the game splits it where it overflows.
/// - `fit-unmapped`: no line of the map matches a key in some language's scope (code `all`).
/// - `fit-map` and `fit-font`: the map or the faces it needs are malformed or missing.
///
/// An English original that already overflows its room (a 2003 flaw) is not held against a
/// translation that needs no more lines and no wider a word than the original: those are counted as
/// `fit-base` in the language's note, not reported. A text in a `scroll` family is counted, not measured.
/// A text listed in `Localization/game/fit-known.txt` (D108) is counted as `known` in its language's note and
/// not reported; a listed line that no longer overflows is `fit-known-unused`, a malformed one `fit-known`.
/// Every language is measured, with no generated font; `only` limits the run to one language code, which
/// language-mods uses for the language it builds. Without a fit map the scope is `not active`.
pub fn checkFit(io: std.Io, report: *Report, root: std.Io.Dir, fonts_dir: []const u8, only: ?[]const u8) !void {
    const arena = report.arena;
    const map_text = (try readOptional(io, arena, root, fit_map_path)) orelse {
        try report.inactive.append(arena, "fit");
        return;
    };
    var problems: std.ArrayList(ScopeProblem) = .empty;
    const lines = try parseFitMap(arena, map_text, &problems);
    const scopes = (try loadScopes(io, arena, root, null)) orelse return;
    var fit = Fit.init(std.heap.smp_allocator, io, fonts_dir) catch |err| {
        try report.addFmt("fit-font", "all", fonts_dir, "cannot read the font the fit rule measures with ({s})", .{@errorName(err)});
        return;
    };
    defer fit.deinit();
    // The map's own problems, once, with English line spaces; each language resolves its rooms again.
    _ = try resolveFitLines(io, arena, root, lines, try fit.lineSpace(Fit.english, .medium), try fit.lineSpace(Fit.english, .large), &problems);
    for (problems.items) |problem| {
        try report.addFmt("fit-map", "all", try std.fmt.allocPrint(arena, "{s}:{d}", .{ fit_map_path, problem.line }), "{s}", .{problem.detail});
    }

    var known_problems: std.ArrayList(ScopeProblem) = .empty;
    const known: []const FitKnown = if (try readOptional(io, arena, root, fit_known_path)) |text| try parseFitKnown(arena, text, &known_problems) else &.{};
    for (known_problems.items) |problem| {
        try report.addFmt("fit-known", "all", try std.fmt.allocPrint(arena, "{s}:{d}", .{ fit_known_path, problem.line }), "{s}", .{problem.detail});
    }
    const known_used = try arena.alloc(bool, known.len);
    @memset(known_used, false);

    var matches: std.StringHashMapUnmanaged([]const usize) = .empty;
    for (scopes.all_keys) |key| {
        const found = try fitMatches(arena, lines, try std.ascii.allocLowerString(arena, key));
        if (found.len == 0) try report.add("fit-unmapped", "all", key, "no line of Localization/game/fit-map.txt matches this key; add an exact line or a family");
        try matches.put(arena, key, found);
    }

    var english_cache: std.StringHashMapUnmanaged(?[]const u16) = .empty;
    for (languages.languages, scopes.per_language, 0..) |language, scope, slot| {
        if (only) |code| if (!std.mem.eql(u8, code, language.code)) continue;
        // The rooms again with this language's line spaces: the tooltip's width follows the ones the
        // Game has loaded. The map's problems were reported above.
        var own_problems: std.ArrayList(ScopeProblem) = .empty;
        const resolved = try resolveFitLines(io, arena, root, lines, try fit.lineSpace(slot, .medium), try fit.lineSpace(slot, .large), &own_problems);
        var boxed: usize = 0;
        var tooltips: usize = 0;
        var scrolls: usize = 0;
        var base_overflow: usize = 0;
        var known_keys: usize = 0;
        for (scope.keys) |key| {
            const found = matches.get(key) orelse continue;
            if (found.len == 0) continue;
            const path = try std.fmt.allocPrint(arena, "Localization/game/{s}/game_data_base/{s}.txt", .{ language.code, key });
            const bytes = (try readOptional(io, arena, root, path)) orelse continue;
            if (textProblem(bytes) != .ok) continue;
            const units = try textUnits(arena, bytes);
            const english_entry = try english_cache.getOrPut(arena, key);
            if (!english_entry.found_existing) {
                english_entry.value_ptr.* = if (try readBase(io, arena, root, key)) |english| try textUnits(arena, english) else null;
            }
            const english = english_entry.value_ptr.*;
            var is_boxed = false;
            var is_tooltip = false;
            var is_scroll = false;
            var overflowed = false;
            var key_known = false;
            const known_at = for (known, 0..) |one, i| {
                if (std.mem.eql(u8, one.code, language.code) and std.mem.eql(u8, one.key, key)) break i;
            } else null;
            for (found) |at| {
                const room = resolved[at];
                if (!room.ok) continue;
                switch (room.kind) {
                    .scroll => {
                        is_scroll = true;
                        continue;
                    },
                    .tooltip => is_tooltip = true,
                    .box, .area => is_boxed = true,
                }
                // The translation in its language's chain; the English original in the English one at the
                // same size, so fit-base only excuses a text no worse than what shipped in 2003.
                const shaper = try fit.shaper(slot, room.font);
                const shipped = try fit.shaper(Fit.english, room.font);
                const mine = try measureText(&fit, shaper, units, room.width);
                const theirs: ?Measure = if (english) |original| try measureText(&fit, shipped, original, room.width) else null;
                const too_tall = fitNeeds(shaper, mine.lines) > room.height;
                const too_wide = mine.word > room.width;
                const english_tall = if (theirs) |t| fitNeeds(shipped, t.lines) > room.height else false;
                const english_wide = if (theirs) |t| t.word > room.width else false;
                if (english_tall or english_wide) overflowed = true;
                // A text left as the original ("Ping", "Rel") overflows where the original did, a pixel
                // more or less with the face: nothing a translator can change.
                const unchanged = if (english) |original| std.mem.eql(u16, trimUnits(units), trimUnits(original)) else false;
                if (unchanged and (english_tall or english_wide)) continue;
                if (too_tall and !(english_tall and mine.lines <= theirs.?.lines)) {
                    if (known_at) |i| {
                        known_used[i] = true;
                        key_known = true;
                    } else try report.addFmt("fit", language.code, key, "{d} lines x {d} > {d} in {s}", .{ mine.lines, @as(i64, @intFromFloat(bk_text.measure.lineSpace(shaper))), @as(i64, @intFromFloat(room.height)), room.where });
                }
                if (too_wide and !(english_wide and mine.word <= theirs.?.word)) {
                    if (known_at) |i| {
                        known_used[i] = true;
                        key_known = true;
                    } else try report.addFmt("fit-word", language.code, key, "\"{s}\" is {d} px wide, the room is {d} px in {s}", .{ try wordText(arena, &fit, shaper, units), @as(i64, @intFromFloat(mine.word)), @as(i64, @intFromFloat(@floor(room.width))), room.where });
                }
            }
            if (is_boxed) boxed += 1;
            if (is_tooltip) tooltips += 1;
            if (is_scroll) scrolls += 1;
            if (overflowed) base_overflow += 1;
            if (key_known) known_keys += 1;
        }
        try report.scope_notes.append(arena, try std.fmt.allocPrint(arena, "l10n-check: fit: {s}: {d} boxed, {d} tooltip, {d} scroll keys measured, {d} with an English original that overflows (fit-base), {d} known", .{ language.code, boxed, tooltips, scrolls, base_overflow, known_keys }));
        // The glyph rule guards coverage against the faces; a gap here means a face lacks a character the text holds.
        const missing = fit.missing[slot].count();
        if (missing != 0) try report.scope_notes.append(arena, try std.fmt.allocPrint(arena, "l10n-check: fit: {s}: {d} characters have no glyph in the faces and were measured with the missing glyph", .{ language.code, missing }));
        for (known, known_used) |one, used| {
            if (!used and std.mem.eql(u8, one.code, language.code)) {
                try report.addFmt("fit-known-unused", language.code, one.key, "{s}:{d} lists a text that fits its room now; delete the line", .{ fit_known_path, one.line });
            }
        }
    }
}

fn usage() noreturn {
    std.debug.print("usage: l10n_check [--root <dir>] [--fonts <dir>] [--doc-set <stem>] [--no-default-docs] [--strict-dir <dir>] [--update-base]\n", .{});
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
    var default_docs: []const []const u8 = &document_sets;
    var strict: std.ArrayList([]const u8) = .empty;
    while (iterator.next()) |arg| {
        if (std.mem.eql(u8, arg, "--root")) {
            root_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--fonts")) {
            fonts_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--doc-set")) {
            try doc_sets.append(arena, try arena.dupe(u8, iterator.next() orelse usage()));
        } else if (std.mem.eql(u8, arg, "--no-default-docs")) {
            default_docs = &.{};
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
    try checkFit(io, &report, root, fonts_path orelse try std.fs.path.join(arena, &.{ root_path, "vendor/fonts" }), null);
    var fonts = if (fonts_path) |path| std.Io.Dir.cwd().openDir(io, path, .{}) else root.openDir(io, "vendor/fonts", .{});
    if (fonts) |*dir| {
        defer dir.close(io);
        try checkGlyphs(io, &report, root, dir.*);
        try checkTables(io, &report, root, dir.*);
    } else |err| {
        try report.addFmt("glyph", "all", fonts_path orelse "vendor/fonts", "cannot open the fonts directory ({s})", .{@errorName(err)});
        try checkTables(io, &report, root, null);
    }
    try checkDocuments(io, &report, root, default_docs, doc_sets.items);
    try checkLiterals(io, &report, root, strict.items);

    for (report.findings.items) |finding| std.debug.print("{s}\n", .{try report.line(finding)});
    for (report.inactive.items) |scope| std.debug.print("l10n-check: {s}: not active\n", .{scope});
    for (report.scope_notes.items) |note| std.debug.print("{s}\n", .{note});
    std.debug.print("l10n-check: {d} findings, {d} checked\n", .{ report.findings.items.len, report.checked });
    if (report.findings.items.len != 0) std.process.exit(1);
}

test {
    _ = cmap;
    _ = font_file;
    _ = wrap;
}

test "fit: bk_text shapes a line with the vendored faces at the medium size" {
    var set = try bk_text.faces.FaceSet.load(std.testing.allocator, std.testing.io, "vendor/fonts");
    defer set.deinit();
    var shaper = try bk_text.shape.Shaper.init(std.testing.allocator, &set, "en", bk_text.sizes.pixelSize(.medium, 1));
    defer shaper.deinit();
    const run = try shaper.shape(std.unicode.utf8ToUtf16LeStringLiteral("Start"));
    defer run.deinit(std.testing.allocator);
    try std.testing.expect(run.glyphs.len == 5);
    var width: f32 = 0;
    for (run.glyphs) |glyph| width += glyph.x_advance;
    try std.testing.expect(width > 0);
}

test "parseFitMap reads the four directives and names each malformed line" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var problems: std.ArrayList(ScopeProblem) = .empty;
    const text = "# c\r\nbox Textes/Ui/** Data/UI/A.xml 20000#2\r\narea a/** 410x30 large # why\r\ntooltip t/**\r\n" ++
        "scroll s/** # long prose\r\nscroll s2/**\r\nscroll # no glob\r\nbox x Data/UI/A.xml abc\r\narea y 10 medium\r\narea y 10x10 huge\r\nbogus z\r\nbox a b\r\n";
    const lines = try parseFitMap(arena, text, &problems);
    try testing.expectEqual(@as(usize, 4), lines.len);
    try testing.expectEqual(FitKind.box, lines[0].kind);
    try testing.expectEqualStrings("textes/ui/**", lines[0].glob);
    try testing.expectEqualStrings("20000#2", lines[0].element);
    try testing.expectEqual(@as(f32, 410), lines[1].width);
    try testing.expectEqual(FitFont.large, lines[1].font);
    try testing.expectEqual(FitKind.tooltip, lines[2].kind);
    try testing.expectEqual(FitKind.scroll, lines[3].kind);
    try testing.expectEqual(@as(usize, 7), problems.items.len);
    try testing.expectEqual(@as(usize, 6), problems.items[0].line);
    try testing.expect(std.mem.indexOf(u8, problems.items[0].detail, "gives no reason") != null);
    try testing.expectEqual(@as(usize, 12), problems.items[6].line);
}

test "globMatch: * stops at a slash, ** does not, [0-9]+ is a digit run" {
    const testing = std.testing;
    try testing.expect(globMatch("textes/ui/**/tt*", "textes/ui/intermission/warehouse/tt_button_back"));
    try testing.expect(!globMatch("textes/ui/**/tt*", "textes/ui/intermission/warehouse/button_back"));
    try testing.expect(globMatch("units/**/name", "units/technics/allies/auto/m5/name"));
    try testing.expect(!globMatch("units/*/name", "units/technics/allies/name"));
    try testing.expect(globMatch("units/*/name", "units/humans/name"));
    try testing.expect(globMatch("scenarios/**/[0-9]+h", "scenarios/scenariomissions/german/poland/12h"));
    try testing.expect(!globMatch("scenarios/**/[0-9]+h", "scenarios/scenariomissions/german/poland/h"));
    try testing.expect(!globMatch("scenarios/**/[0-9]+", "scenarios/scenariomissions/german/poland/1h"));
    try testing.expect(globMatch("textes/strings/**", "textes/strings/ok"));
    try testing.expect(globMatch("a**", "a"));
    try testing.expect(!globMatch("a", "ab"));
}

test "parseLayout reads size, inset, alignment and font of each element in document order" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const xml = "<?xml version=\"1.0\"?><!-- <item ClassTypeID=\"1\"> --><base><WindowSize x=\"9\" y=\"9\"/><Children>" ++
        "<item ClassTypeID=\"0x10001105\" ElementID=\"7\" TextAlign=\"0x0021\" FontSize=\"2\"><TextPos x=\"30\" y=\"4\"/><WindowSize x=\"850\" y=\"70\"/>" ++
        "<States><item EventID=\"-1\"><WindowSize x=\"1\" y=\"1\"/></item></States>" ++
        "<Children><Element ClassTypeID=\"0x10001103\" ElementID=\"-1\"><WindowSize x=\"132\" y=\"62\"></WindowSize></Element></Children></item></Children></base>";
    const elements = try parseLayout(arena_state.allocator(), xml);
    try testing.expectEqual(@as(usize, 2), elements.len);
    try testing.expectEqual(@as(i64, 7), elements[0].id);
    try testing.expectEqual(@as(u8, 2), elements[0].font);
    try testing.expectEqual(@as(f32, 820), elements[0].textWidth());
    try testing.expectEqual(@as(f32, 70), elements[0].textHeight());
    try testing.expectEqual(@as(i64, -1), elements[1].id);
    try testing.expectEqual(@as(u8, 1), elements[1].font);
    try testing.expectEqual(@as(f32, 132), elements[1].textWidth());
    try testing.expectEqual(@as(f32, 62), elements[1].textHeight());
}

test "fit_real_tree: the repository has no fit finding and every language prints a fit note" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const io = testing.io;
    var root = try std.Io.Dir.cwd().openDir(io, ".", .{ .iterate = true });
    defer root.close(io);
    var report: Report = .{ .arena = arena_state.allocator() };
    try checkFit(io, &report, root, "vendor/fonts", null);
    for (report.findings.items) |finding| std.debug.print("{s}\n", .{try report.line(finding)});
    for (report.findings.items) |finding| {
        try testing.expect(!std.mem.startsWith(u8, finding.rule, "fit"));
    }
    var notes: usize = 0;
    for (report.scope_notes.items) |note| {
        if (std.mem.indexOf(u8, note, "keys measured") != null) notes += 1;
    }
    try testing.expectEqual(@as(usize, 9), notes);
}

test "fit_map_covers_database: every ELK key has a line and every line finds its room" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = testing.io;
    var root = try std.Io.Dir.cwd().openDir(io, ".", .{ .iterate = true });
    defer root.close(io);
    var problems: std.ArrayList(ScopeProblem) = .empty;
    const lines = try parseFitMap(arena, (try readOptional(io, arena, root, fit_map_path)).?, &problems);
    const resolved = try resolveFitLines(io, arena, root, lines, 23, 46, &problems);
    for (problems.items) |problem| std.debug.print("{s}:{d}: {s}\n", .{ fit_map_path, problem.line, problem.detail });
    try testing.expectEqual(@as(usize, 0), problems.items.len);
    for (resolved) |one| try testing.expect(one.ok);

    const keys = try elkKeys(io, arena, root);
    try testing.expect(keys.len > 2800);
    var unmapped: usize = 0;
    for (keys) |key| {
        if ((try fitMatches(arena, lines, try std.ascii.allocLowerString(arena, key))).len != 0) continue;
        std.debug.print("fit-unmapped: {s}\n", .{key});
        unmapped += 1;
    }
    try testing.expectEqual(@as(usize, 0), unmapped);
}

test "fit: the shipped English originals measured in their rooms" {
    // Information for the maintainer: how many 2003 originals already overflow a room (fit-base),
    // so a translation is judged against a fair box. It fails on nothing but a broken map.
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = testing.io;
    var root = try std.Io.Dir.cwd().openDir(io, ".", .{ .iterate = true });
    defer root.close(io);
    var problems: std.ArrayList(ScopeProblem) = .empty;
    const lines = try parseFitMap(arena, (try readOptional(io, arena, root, fit_map_path)).?, &problems);
    var fit = try Fit.init(testing.allocator, io, "vendor/fonts");
    defer fit.deinit();
    const resolved = try resolveFitLines(io, arena, root, lines, try fit.lineSpace(Fit.english, .medium), try fit.lineSpace(Fit.english, .large), &problems);
    var tall: usize = 0;
    var wide: usize = 0;
    var measured: usize = 0;
    for (try elkKeys(io, arena, root)) |key| {
        const english = (try readBase(io, arena, root, key)) orelse continue;
        const units = try textUnits(arena, english);
        for (try fitMatches(arena, lines, try std.ascii.allocLowerString(arena, key))) |at| {
            const room = resolved[at];
            if (room.kind == .scroll) continue;
            const shaper = try fit.shaper(Fit.english, room.font);
            const mine = try measureText(&fit, shaper, units, room.width);
            measured += 1;
            if (fitNeeds(shaper, mine.lines) > room.height) tall += 1;
            if (mine.word > room.width) wide += 1;
        }
    }
    std.debug.print("fit-base: {d} measured, {d} too tall, {d} too wide\n", .{ measured, tall, wide });
    try testing.expect(measured > 1000);
}

test "fit: the tooltip width follows the ratio of the large line space to the medium one" {
    try std.testing.expectEqual(@as(f32, 600), tooltipWidth(23, 46));
    try std.testing.expectEqual(@as(f32, 300), tooltipWidth(23, 23));
    try std.testing.expectEqual(@as(f32, 300), tooltipWidth(0, 40));
    try std.testing.expectEqual(@as(f32, 391), tooltipWidth(23, 30));
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

/// How many findings the ID rule reports for `source` as file `path`, and the line of the first.
fn idFindings(path: []const u8, source: []const u8, first_line: *usize) !usize {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    var report: Report = .{ .arena = arena_state.allocator() };
    try scanIdRule(&report, path, source);
    if (report.findings.items.len != 0) {
        const where = report.findings.items[0].where;
        first_line.* = try std.fmt.parseInt(usize, where[std.mem.lastIndexOfScalar(u8, where, ':').? + 1 ..], 10);
    }
    return report.findings.items.len;
}

test "the ID rule sees wrapped calls and l10n.t aliases and keeps plain text, l10n.id and later arguments allowed" {
    const testing = std.testing;
    var line: usize = 0;
    // (a) a wrapped call reports at the call's line
    try testing.expectEqual(@as(usize, 1), try idFindings("app/a.zig", "const x = 1;\n    if (c.igButton(\n        l10n.t(\"file.save\"),\n        .{})) {}\n", &line));
    try testing.expectEqual(@as(usize, 2), line);
    // (b) whitespace between the name and the bracket, and the bracket and l10n.t
    try testing.expectEqual(@as(usize, 1), try idFindings("app/a.zig", "c.igCheckbox  (\n\n  l10n.t(\"x\"), &v);\n", &line));
    try testing.expectEqual(@as(usize, 1), line);
    // (c) aliases outside kit/l10n.zig
    try testing.expectEqual(@as(usize, 1), try idFindings("app/a.zig", "const tr = l10n.t;\n", &line));
    try testing.expectEqual(@as(usize, 1), try idFindings("app/a.zig", "const t = @import(\"kit\").l10n.t;\n", &line));
    try testing.expectEqual(@as(usize, 1), try idFindings("app/a.zig", "pub const t = l10n.t;\n", &line));
    // (d) the same inside kit/l10n.zig
    try testing.expectEqual(@as(usize, 0), try idFindings("Sources/editor/kit/l10n.zig", "pub const t = l10n.t;\n", &line));
    // (e) plain text stays allowed, wrapped or not
    try testing.expectEqual(@as(usize, 0), try idFindings("app/a.zig", "c.igTextUnformatted(l10n.t(\"a\"));\nc.igText(\n  l10n.t(\"b\"));\nc.igSetTooltip(\n l10n.t(\"c\"));\n", &line));
    // (f) l10n.id stays allowed, wrapped or not
    try testing.expectEqual(@as(usize, 0), try idFindings("app/a.zig", "c.igButton(l10n.id(\"a\"));\nc.igButton(\n  l10n.id(\"b\"));\n", &line));
    // (g) l10n.t after the first argument stays allowed; so do calls in comments and strings, and a longer name
    try testing.expectEqual(@as(usize, 0), try idFindings("app/a.zig", "c.igButton(label, l10n.t(\"x\"));\n// c.igButton(l10n.t(\"x\"))\nconst s = \"igButton(l10n.t(\";\nc.igButtonFoo(l10n.t(\"x\"));\nconst r = l10n.t(\"k\");\n", &line));
}

test "the document rule finds no fault in the real README set and every translation is a real one" {
    const testing = std.testing;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const cwd = std.Io.Dir.cwd();
    var report: Report = .{ .arena = arena };
    try checkDocuments(testing.io, &report, cwd, &document_sets, &.{});
    for (report.findings.items) |finding| std.debug.print("{s}\n", .{try report.line(finding)});
    try testing.expectEqual(@as(usize, 0), report.findings.items.len);
    const english = try parseDocument(arena, try cwd.readFileAlloc(testing.io, "README.md", arena, .limited(1 << 20)));
    var top_level: usize = 0;
    for (english.levels) |level| top_level += @intFromBool(level == 1);
    try testing.expectEqual(@as(usize, 7), top_level);
    // The rule reads only headings, fences and the bar, so an empty or copied file needs its own check.
    for (document_sets) |stem| {
        const en_bytes = try cwd.readFileAlloc(testing.io, try std.fmt.allocPrint(arena, "{s}.md", .{stem}), arena, .limited(1 << 22));
        for (doc_languages) |language| {
            const path = try std.fmt.allocPrint(arena, "{s}_{s}.md", .{ stem, language.name });
            const bytes = try cwd.readFileAlloc(testing.io, path, arena, .limited(1 << 22));
            try testing.expect(!std.mem.eql(u8, bytes, en_bytes));
            try testing.expect(bytes.len * 3 >= en_bytes.len and bytes.len <= en_bytes.len * 3);
        }
    }
}
