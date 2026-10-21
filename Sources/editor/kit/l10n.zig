//! String tables for the Zig editors (D061): one `.lang` file per language under `kit/lang/`, all ten
//! embedded in the binary, English the reference. The completeness gate (tools/zig/l10n_check.zig)
//! reads the same files, so its table rules and this parser describe one format.
//!
//! A table is UTF-8 without a byte order mark, CRLF lines, `#` comment lines, and `key=value`
//! (the first `=` splits, so a value may hold more). A value may use the escapes `\n`, `\t` and
//! `\\`, and the placeholders `{0}`..`{9}` that `format` fills and the printf codes the caller
//! formats itself. A table stores the text only; `id` appends the ImGui ID suffix `###<key>`, so a
//! widget keeps its identity when the language changes.
//!
//! Keys are checked against `en.lang` at compile time: `t("no.such.key")` does not build. A key a
//! language lacks shows the English text and logs `l10n: missing <code> <key>` once. The test seam
//! `BK_L10N_DIR` (see `setOverrideDir`) loads `<dir>/<code>.lang` instead of the embedded table.
//!
//! Hindi text goes through `fonts.encodeHindi` so each shaped cluster is one Private Use Area
//! character; load the fonts before the first Hindi lookup, since a table is encoded when it is
//! first needed. Pointers `t` and `id` return stay valid for the life of the process (a language's
//! table is kept once built, until `setOverrideDir` replaces the tables).
const std = @import("std");
const languages = @import("l10n_languages");
const fonts = @import("fonts.zig");
const imgui = @import("editor_imgui");

/// The environment variable apps pass to `setOverrideDir`.
pub const override_env = "BK_L10N_DIR";

/// The `ui_language` setting value that follows the operating system.
pub const system_setting = "system";

/// `en` first, then the nine of M002 in the order of `l10n_languages`.
pub const codes = blk: {
    var out: [languages.languages.len + 1][]const u8 = undefined;
    out[0] = "en";
    for (languages.languages, 1..) |entry, i| out[i] = entry.code;
    break :blk out;
};

const embedded = blk: {
    var out: [codes.len][]const u8 = undefined;
    for (codes, 0..) |code, i| out[i] = @embedFile("lang/" ++ code ++ ".lang");
    break :blk out;
};

/// The name a language is listed under in the picker: its own, in its own script.
pub fn nativeName(code: []const u8) []const u8 {
    if (std.mem.eql(u8, code, "en")) return "English";
    for (languages.languages) |entry| {
        if (std.mem.eql(u8, entry.code, code)) return entry.name;
    }
    return code;
}

// ---------------------------------------------------------------- the format

pub const ParseError = error{ InvalidUtf8, ByteOrderMark, MissingEquals, EmptyKey, DuplicateKey, BadEscape, OutOfMemory };

pub const Entry = struct { key: []const u8, value: []const u8 };

/// Where `parse` stopped: the 1-based line of the error.
pub const Diagnostic = struct { line: usize = 0 };

const Line = union(enum) { skip, entry: struct { key: []const u8, raw: []const u8 }, bad: ParseError };

fn classify(text_in: []const u8) Line {
    const text = if (text_in.len > 0 and text_in[text_in.len - 1] == '\r') text_in[0 .. text_in.len - 1] else text_in;
    const trimmed = std.mem.trim(u8, text, " \t");
    if (trimmed.len == 0 or trimmed[0] == '#') return .skip;
    const eq = std.mem.indexOfScalar(u8, text, '=') orelse return .{ .bad = error.MissingEquals };
    if (eq == 0) return .{ .bad = error.EmptyKey };
    return .{ .entry = .{ .key = text[0..eq], .raw = text[eq + 1 ..] } };
}

fn unescape(arena: std.mem.Allocator, raw: []const u8) ParseError![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    var at: usize = 0;
    while (at < raw.len) : (at += 1) {
        if (raw[at] != '\\') {
            try out.append(arena, raw[at]);
            continue;
        }
        at += 1;
        if (at == raw.len) return error.BadEscape;
        try out.append(arena, switch (raw[at]) {
            'n' => '\n',
            't' => '\t',
            '\\' => '\\',
            else => return error.BadEscape,
        });
    }
    return out.items;
}

/// The entries of a table in file order, values unescaped. Everything is allocated from `arena`.
pub fn parse(arena: std.mem.Allocator, bytes: []const u8, diag: ?*Diagnostic) ParseError![]const Entry {
    if (std.mem.startsWith(u8, bytes, "\xEF\xBB\xBF")) return error.ByteOrderMark;
    if (!std.unicode.utf8ValidateSlice(bytes)) return error.InvalidUtf8;
    var entries: std.ArrayList(Entry) = .empty;
    var seen: std.StringHashMapUnmanaged(void) = .empty;
    var line_no: usize = 0;
    var lines = std.mem.splitScalar(u8, bytes, '\n');
    while (lines.next()) |text| {
        line_no += 1;
        if (diag) |d| d.line = line_no;
        switch (classify(text)) {
            .skip => {},
            .bad => |err| return err,
            .entry => |e| {
                const gop = try seen.getOrPut(arena, e.key);
                if (gop.found_existing) return error.DuplicateKey;
                try entries.append(arena, .{ .key = e.key, .value = try unescape(arena, e.raw) });
            },
        }
    }
    return entries.items;
}

fn countKeys(comptime bytes: []const u8) usize {
    @setEvalBranchQuota(1_000_000);
    var n: usize = 0;
    var lines = std.mem.splitScalar(u8, bytes, '\n');
    while (lines.next()) |text| {
        switch (classify(text)) {
            .skip => {},
            .bad => @compileError("l10n: lang/en.lang has a line without a key"),
            .entry => n += 1,
        }
    }
    return n;
}

/// The keys of `en.lang`, in file order. Every language indexes its values by position here.
const en_keys = blk: {
    @setEvalBranchQuota(10_000_000);
    var out: [countKeys(embedded[0])][]const u8 = undefined;
    var n: usize = 0;
    var lines = std.mem.splitScalar(u8, embedded[0], '\n');
    while (lines.next()) |text| {
        switch (classify(text)) {
            .skip, .bad => {},
            .entry => |e| {
                out[n] = e.key;
                n += 1;
            },
        }
    }
    break :blk out;
};

/// The key lookup, built once at compile time: a comptime scan of a thousand keys per call site
/// would multiply the compiler's work by the number of `t`/`id` calls.
const key_map = blk: {
    @setEvalBranchQuota(10_000_000);
    var pairs: [en_keys.len]struct { []const u8, usize } = undefined;
    for (en_keys, 0..) |key, i| pairs[i] = .{ key, i };
    const map = std.StaticStringMap(usize).initComptime(pairs);
    for (en_keys, 0..) |key, i| {
        if (map.get(key).? != i) @compileError("l10n: duplicate key " ++ key);
    }
    break :blk map;
};

pub fn keyIndex(comptime key: []const u8) usize {
    // A table past a thousand keys has long buckets: the lookup needs more than the default quota.
    return comptime blk: {
        @setEvalBranchQuota(100_000);
        break :blk key_map.get(key) orelse @compileError("l10n: unknown key " ++ key);
    };
}

// ------------------------------------------------------------------ rect log

/// Opt-in (the apps turn it on for `--l10n-auto`): `noteRect` prints where the last widget drew.
pub var rect_log = false;

/// After a widget labelled by `key`: logs `l10n-auto: rect <key> x y w h` (screen pixels) when
/// `rect_log` is on, so the l10n tier can measure each label's frame. Normal runs print nothing.
pub fn noteRect(comptime key: []const u8) void {
    _ = keyIndex(key);
    if (!rect_log) return;
    const min = imgui.c.igGetItemRectMin();
    const max = imgui.c.igGetItemRectMax();
    std.debug.print("l10n-auto: rect {s} {d:.0} {d:.0} {d:.0} {d:.0}\n", .{ key, min.x, min.y, max.x - min.x, max.y - min.y });
}

/// After a slider, input, checkbox or combo labelled by `key`: like `noteRect`, for the label's own
/// text at the right end of the widget's rect (ImGui draws the label after the control), so the tier
/// measures the label and not the control.
pub fn noteLabelRect(comptime key: []const u8) void {
    _ = keyIndex(key);
    if (!rect_log) return;
    const min = imgui.c.igGetItemRectMin();
    const max = imgui.c.igGetItemRectMax();
    const width = imgui.c.igCalcTextSize(t(key)).x;
    std.debug.print("l10n-auto: rect {s} {d:.0} {d:.0} {d:.0} {d:.0}\n", .{ key, max.x - width, min.y, width, max.y - min.y });
}

/// Like `noteRect`, for the window being drawn (between its Begin and End): the title bar's text is
/// the window's own `key`.
pub fn noteWindowRect(comptime key: []const u8) void {
    _ = keyIndex(key);
    if (!rect_log) return;
    const pos = imgui.c.igGetWindowPos();
    const size = imgui.c.igGetWindowSize();
    std.debug.print("l10n-auto: rect {s} {d:.0} {d:.0} {d:.0} {d:.0}\n", .{ key, pos.x, pos.y, size.x, size.y });
}

// -------------------------------------------------------------------- tables

/// One language, built once: the text of every key as the UI draws it (Hindi encoded), with and
/// without the ID suffix. A key the language lacks points at the English text and is `missing`.
const Loaded = struct {
    arena: std.heap.ArenaAllocator,
    raw: [en_keys.len]?[:0]const u8 = @splat(null),
    shown: [en_keys.len][:0]const u8 = undefined,
    shown_id: [en_keys.len][:0]const u8 = undefined,
    announced: [en_keys.len]bool = @splat(false),
};

var tables: [codes.len]?*Loaded = @splat(null);
var current: usize = 0;
var override_dir: ?[]const u8 = null;
var override_io: ?std.Io = null;

const page = std.heap.page_allocator;

/// Loads tables from `<dir>/<code>.lang` when that file exists, else from the embedded ones. The
/// directory is copied. Drops every table built so far. Apps pass the `BK_L10N_DIR` value.
pub fn setOverrideDir(io: std.Io, dir: ?[]const u8) void {
    for (&tables) |*slot| {
        if (slot.*) |loaded| {
            loaded.arena.deinit();
            page.destroy(loaded);
            slot.* = null;
        }
    }
    if (override_dir) |old| page.free(old);
    override_dir = null;
    override_io = io;
    if (dir) |d| {
        if (d.len != 0) override_dir = page.dupe(u8, d) catch null;
    }
}

fn langIndex(code: []const u8) ?usize {
    for (codes, 0..) |known, i| {
        if (std.mem.eql(u8, known, code)) return i;
    }
    return null;
}

fn tableBytes(arena: std.mem.Allocator, index: usize) []const u8 {
    const dir = override_dir orelse return embedded[index];
    const io = override_io orelse return embedded[index];
    const path = std.fmt.allocPrint(arena, "{s}{c}{s}.lang", .{ dir, std.fs.path.sep, codes[index] }) catch return embedded[index];
    return std.Io.Dir.cwd().readFileAlloc(io, path, arena, .limited(1 << 20)) catch embedded[index];
}

fn build(index: usize) *Loaded {
    const loaded = page.create(Loaded) catch @panic("l10n: out of memory");
    loaded.* = .{ .arena = std.heap.ArenaAllocator.init(page) };
    const arena = loaded.arena.allocator();
    var diag: Diagnostic = .{};
    const entries = parse(arena, tableBytes(arena, index), &diag) catch |err| blk: {
        std.debug.print("l10n: {s}.lang line {d}: {s}\n", .{ codes[index], diag.line, @errorName(err) });
        break :blk if (index == 0) &[_]Entry{} else parse(arena, embedded[index], null) catch &[_]Entry{};
    };
    for (entries) |entry| {
        if (key_map.get(entry.key)) |i| loaded.raw[i] = arena.dupeSentinel(u8, entry.value, 0) catch @panic("l10n: out of memory");
    }
    const english: ?*Loaded = if (index == 0) null else table(0);
    for (en_keys, 0..) |key, i| {
        if (loaded.raw[i]) |text| {
            loaded.shown[i] = display(arena, index, text);
            loaded.shown_id[i] = std.fmt.allocPrintSentinel(arena, "{s}###{s}", .{ loaded.shown[i], key }, 0) catch @panic("l10n: out of memory");
        } else if (english) |en| {
            loaded.shown[i] = en.shown[i];
            loaded.shown_id[i] = en.shown_id[i];
        } else {
            loaded.shown[i] = arena.dupeSentinel(u8, key, 0) catch @panic("l10n: out of memory");
            loaded.shown_id[i] = std.fmt.allocPrintSentinel(arena, "{s}###{s}", .{ key, key }, 0) catch @panic("l10n: out of memory");
        }
    }
    return loaded;
}

/// The text as drawn: Hindi through the cluster encoder, everything else as is.
fn display(arena: std.mem.Allocator, index: usize, text: [:0]const u8) [:0]const u8 {
    if (!std.mem.eql(u8, codes[index], "hi")) return text;
    const buffer = arena.alloc(u8, text.len + 16) catch return text;
    return fonts.encodeHindi(buffer, text) orelse text;
}

fn table(index: usize) *Loaded {
    if (tables[index]) |loaded| return loaded;
    const loaded = build(index);
    tables[index] = loaded;
    return loaded;
}

/// True the first time a (language, key) pair is reported.
fn firstMissing(loaded: *Loaded, index: usize, key_index: usize) bool {
    if (index == 0 or loaded.raw[key_index] != null or loaded.announced[key_index]) return false;
    loaded.announced[key_index] = true;
    return true;
}

fn noteMissing(index: usize, comptime key: []const u8) void {
    const loaded = table(index);
    if (firstMissing(loaded, index, keyIndex(key))) std.debug.print("l10n: missing {s} {s}\n", .{ codes[index], key });
}

/// Switches the language; `code` is one of `codes`. Anything else logs
/// `l10n: unknown ui_language <code>, using en` and selects English.
pub fn setLanguage(code: []const u8) void {
    current = langIndex(code) orelse blk: {
        std.debug.print("l10n: unknown ui_language {s}, using en\n", .{code});
        break :blk 0;
    };
    _ = table(current);
}

pub fn language() []const u8 {
    return codes[current];
}

/// The text of `key` in the current language.
pub fn t(comptime key: []const u8) [*:0]const u8 {
    noteMissing(current, key);
    return table(current).shown[keyIndex(key)].ptr;
}

/// The text of `key` followed by `###<key>`, for ImGui labels whose identity must not change with
/// the language.
pub fn id(comptime key: []const u8) [*:0]const u8 {
    noteMissing(current, key);
    return table(current).shown_id[keyIndex(key)].ptr;
}

/// `key` with `{0}`..`{9}` replaced by the entries of `args` (strings or integers), into `buffer`.
/// A slot beyond `args` is left as written. Hindi results are cluster-encoded.
pub fn format(buffer: []u8, comptime key: []const u8, args: anytype) error{NoSpaceLeft}![]u8 {
    noteMissing(current, key);
    const loaded = table(current);
    const template = loaded.raw[keyIndex(key)] orelse table(0).raw[keyIndex(key)] orelse key;
    var writer = std.Io.Writer.fixed(buffer);
    var at: usize = 0;
    while (at < template.len) {
        if (template[at] == '{' and at + 2 < template.len and template[at + 2] == '}' and std.ascii.isDigit(template[at + 1])) {
            const slot = template[at + 1] - '0';
            var found = false;
            inline for (args, 0..) |arg, i| {
                if (i == slot) {
                    const Arg = @TypeOf(arg);
                    switch (@typeInfo(Arg)) {
                        .int, .comptime_int => writer.print("{d}", .{arg}) catch return error.NoSpaceLeft,
                        else => writer.print("{s}", .{arg}) catch return error.NoSpaceLeft,
                    }
                    found = true;
                }
            }
            if (found) {
                at += 3;
                continue;
            }
        }
        writer.writeByte(template[at]) catch return error.NoSpaceLeft;
        at += 1;
    }
    var result = writer.buffered();
    if (std.mem.eql(u8, codes[current], "hi")) {
        var scratch: [2048]u8 = undefined;
        if (result.len < scratch.len) {
            @memcpy(scratch[0..result.len], result);
            scratch[result.len] = 0;
            if (fonts.encodeHindi(buffer, scratch[0..result.len :0])) |encoded| result = buffer[0..encoded.len];
        }
    }
    return result;
}

/// The text of `key` as a slice, for the callers that take one (status lines, notes).
pub fn str(comptime key: []const u8) []const u8 {
    return std.mem.span(t(key));
}

/// `format` into `buffer`, or the key's unfilled text when `buffer` is too small.
pub fn say(buffer: []u8, comptime key: []const u8, args: anytype) []const u8 {
    return format(buffer, key, args) catch str(key);
}

/// `format` with a terminating NUL, for the C API; null when `buffer` is too small.
pub fn formatZ(buffer: []u8, comptime key: []const u8, args: anytype) ?[:0]const u8 {
    if (buffer.len == 0) return null;
    const text = format(buffer[0 .. buffer.len - 1], key, args) catch return null;
    buffer[text.len] = 0;
    return buffer[0..text.len :0];
}

// ---------------------------------------------------------------- resolution

pub const Resolution = struct {
    code: []const u8,
    /// The setting named a language this build does not have; the caller logs it.
    unknown_setting: bool = false,
};

/// The table code an operating system locale names (`zh_CN`, `zh-Hans-CN`, `pt_PT`, `ja_JP.UTF-8`),
/// or null. Simplified Chinese is `zh`, `zh-Hans*`, `zh-CN`, `zh-SG`; Traditional Chinese has no
/// table. Every Portuguese is `pt-BR`.
pub fn localeCode(locale: []const u8) ?[]const u8 {
    var end: usize = 0;
    while (end < locale.len and locale[end] != '.' and locale[end] != '@') end += 1;
    var parts = std.mem.tokenizeAny(u8, locale[0..end], "_-");
    const lang = parts.next() orelse return null;
    if (std.ascii.eqlIgnoreCase(lang, "zh")) {
        while (parts.next()) |part| {
            if (std.ascii.eqlIgnoreCase(part, "Hant") or std.ascii.eqlIgnoreCase(part, "TW") or
                std.ascii.eqlIgnoreCase(part, "HK") or std.ascii.eqlIgnoreCase(part, "MO")) return null;
        }
        return "zh-Hans";
    }
    if (std.ascii.eqlIgnoreCase(lang, "pt")) return "pt-BR";
    for (codes) |code| {
        if (std.ascii.eqlIgnoreCase(lang, code)) return code;
    }
    return null;
}

/// The language for a `ui_language` setting. `system` takes the first of `os_locales` (as
/// SDL_GetPreferredLocales lists them) that has a table, else `en`. A table code is itself. Any
/// other value is `unknown_setting` and falls back as `system` does.
pub fn resolveLanguage(setting: []const u8, os_locales: []const []const u8) Resolution {
    const trimmed = std.mem.trim(u8, setting, " \t");
    if (trimmed.len == 0 or std.ascii.eqlIgnoreCase(trimmed, system_setting)) return .{ .code = fromLocales(os_locales) };
    for (codes) |code| {
        if (std.ascii.eqlIgnoreCase(trimmed, code)) return .{ .code = code };
    }
    return .{ .code = fromLocales(os_locales), .unknown_setting = true };
}

fn fromLocales(os_locales: []const []const u8) []const u8 {
    for (os_locales) |locale| {
        if (localeCode(locale)) |code| return code;
    }
    return "en";
}

/// `resolveLanguage` plus the log line for an unknown value and `setLanguage`.
pub fn applySetting(setting: []const u8, os_locales: []const []const u8) void {
    const resolved = resolveLanguage(setting, os_locales);
    if (resolved.unknown_setting) std.debug.print("l10n: unknown ui_language {s}, using {s}\n", .{ setting, resolved.code });
    setLanguage(resolved.code);
}

// --------------------------------------------------------------------- tests

fn resetForTest() void {
    setOverrideDir(std.testing.io, null);
    current = 0;
}

test "parse reads CRLF, comments, blank lines and escapes" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const entries = try parse(arena_state.allocator(), "# c\r\n\r\na=1\r\nb=x=y\\n\\t\\\\z\r\n  # indented\r\nc=\r\nd={0} of {1}", null);
    try std.testing.expectEqual(@as(usize, 4), entries.len);
    try std.testing.expectEqualStrings("a", entries[0].key);
    try std.testing.expectEqualStrings("x=y\n\t\\z", entries[1].value);
    try std.testing.expectEqualStrings("", entries[2].value);
    try std.testing.expectEqualStrings("{0} of {1}", entries[3].value);
}

test "parse rejects a duplicate key, bad UTF-8, a BOM, a bad escape and a line without a key" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var diag: Diagnostic = .{};
    try std.testing.expectError(error.DuplicateKey, parse(arena, "a=1\r\nb=2\r\na=3\r\n", &diag));
    try std.testing.expectEqual(@as(usize, 3), diag.line);
    try std.testing.expectError(error.InvalidUtf8, parse(arena, "a=\xff\r\n", null));
    try std.testing.expectError(error.ByteOrderMark, parse(arena, "\xEF\xBB\xBFa=1\r\n", null));
    try std.testing.expectError(error.BadEscape, parse(arena, "a=\\q\r\n", null));
    try std.testing.expectError(error.BadEscape, parse(arena, "a=end\\\r\n", null));
    try std.testing.expectError(error.MissingEquals, parse(arena, "just text\r\n", null));
    try std.testing.expectError(error.EmptyKey, parse(arena, "=v\r\n", null));
}

test "every embedded table parses, has exactly the English keys and no ID suffix" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    for (codes, embedded) |code, bytes| {
        const entries = try parse(arena_state.allocator(), bytes, null);
        try std.testing.expectEqual(en_keys.len, entries.len);
        for (entries, en_keys) |entry, key| {
            try std.testing.expectEqualStrings(key, entry.key);
            try std.testing.expect(std.mem.indexOf(u8, entry.value, "##") == null);
            try std.testing.expect(entry.value.len != 0);
        }
        _ = code;
    }
}

test "the language list is en plus the nine and every code has a native name" {
    try std.testing.expectEqual(@as(usize, 10), codes.len);
    try std.testing.expectEqualStrings("English", nativeName("en"));
    try std.testing.expectEqualStrings("हिन्दी", nativeName("hi"));
    try std.testing.expectEqualStrings("Português (Brasil)", nativeName("pt-BR"));
}

test "t and id follow the language and id appends the key" {
    defer resetForTest();
    resetForTest();
    setLanguage("en");
    try std.testing.expectEqualStrings("Language", std.mem.span(t("kit.language.label")));
    try std.testing.expectEqualStrings("Language###kit.language.label", std.mem.span(id("kit.language.label")));
    setLanguage("de");
    try std.testing.expectEqualStrings("Sprache", std.mem.span(t("kit.language.label")));
    try std.testing.expectEqualStrings("Sprache###kit.language.label", std.mem.span(id("kit.language.label")));
    setLanguage("ja");
    try std.testing.expectEqualStrings("言語", std.mem.span(t("kit.language.label")));
    try std.testing.expectEqual(t("kit.language.label"), t("kit.language.label"));
    try std.testing.expectEqualStrings("ja", language());
}

test "id gives equal translations different ImGui IDs, and the same ID in every language" {
    defer resetForTest();
    resetForTest();
    // Two buttons that read the same in a language share the text; ImGui hashes the part after ### when there is one.
    const first = "resource_app.unsaved.cancel";
    const second = "resource_app.common.cancel";
    for (codes) |code| {
        setLanguage(code);
        try std.testing.expectEqualStrings(std.mem.span(t(first)), std.mem.span(t(second)));
        const one = std.mem.span(id(first));
        const two = std.mem.span(id(second));
        try std.testing.expect(!std.mem.eql(u8, one, two));
        try std.testing.expectEqualStrings("###" ++ first, one[std.mem.indexOf(u8, one, "###").?..]);
        try std.testing.expectEqualStrings("###" ++ second, two[std.mem.indexOf(u8, two, "###").?..]);
    }
}

test "an unknown code selects English" {
    defer resetForTest();
    resetForTest();
    setLanguage("de");
    setLanguage("xx");
    try std.testing.expectEqualStrings("en", language());
}

test "a missing key falls back to English and is reported once" {
    defer resetForTest();
    resetForTest();
    const loaded = table(langIndex("fr").?);
    const k = comptime keyIndex("kit.language.system");
    loaded.raw[k] = null;
    loaded.shown[k] = table(0).shown[k];
    setLanguage("fr");
    try std.testing.expect(firstMissing(loaded, langIndex("fr").?, k));
    try std.testing.expect(!firstMissing(loaded, langIndex("fr").?, k));
    try std.testing.expectEqualStrings("System", std.mem.span(t("kit.language.system")));
    try std.testing.expect(!firstMissing(table(0), 0, k));
}

test "format fills numbered slots with strings and integers" {
    defer resetForTest();
    resetForTest();
    const loaded = table(0);
    const k = comptime keyIndex("kit.language.label");
    const saved = loaded.raw[k];
    defer loaded.raw[k] = saved;
    loaded.raw[k] = "{1} of {0}: {2} {9}";
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("3 of files: 7 {9}", try format(&buffer, "kit.language.label", .{ "files", 3, @as(u8, 7) }));
    var tiny: [4]u8 = undefined;
    try std.testing.expectError(error.NoSpaceLeft, format(&tiny, "kit.language.label", .{ "files", 3, 7 }));
    try std.testing.expect(formatZ(&tiny, "kit.language.label", .{ "files", 3, 7 }) == null);
    const z = formatZ(&buffer, "kit.language.label", .{ "a", "b", "c" }).?;
    try std.testing.expectEqualStrings("b of a: c {9}", z);
    try std.testing.expectEqual(@as(u8, 0), buffer[z.len]);
}

test "BK_L10N_DIR replaces a table and a key it lacks falls back to English" {
    defer resetForTest();
    resetForTest();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try tmp.dir.writeFile(std.testing.io, .{ .sub_path = "de.lang", .data = "kit.language.label=Zunge\r\n" });
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const len = try tmp.dir.realPath(std.testing.io, &path_buffer);
    setOverrideDir(std.testing.io, path_buffer[0..len]);
    setLanguage("de");
    try std.testing.expectEqualStrings("Zunge", std.mem.span(t("kit.language.label")));
    try std.testing.expectEqualStrings("System", std.mem.span(t("kit.language.system")));
    try std.testing.expect(table(langIndex("de").?).raw[comptime keyIndex("kit.language.system")] == null);
    setLanguage("fr");
    try std.testing.expectEqualStrings("Langue", std.mem.span(t("kit.language.label")));
}

test "localeCode maps operating system locales to table codes" {
    try std.testing.expectEqualStrings("zh-Hans", localeCode("zh_CN").?);
    try std.testing.expectEqualStrings("zh-Hans", localeCode("zh-Hans-CN").?);
    try std.testing.expectEqualStrings("zh-Hans", localeCode("zh").?);
    try std.testing.expect(localeCode("zh_TW") == null);
    try std.testing.expect(localeCode("zh-Hant") == null);
    try std.testing.expectEqualStrings("pt-BR", localeCode("pt_PT").?);
    try std.testing.expectEqualStrings("pt-BR", localeCode("pt-BR").?);
    try std.testing.expectEqualStrings("ja", localeCode("ja_JP.UTF-8").?);
    try std.testing.expectEqualStrings("hi", localeCode("hi_IN").?);
    try std.testing.expectEqualStrings("de", localeCode("de_DE@euro").?);
    try std.testing.expectEqualStrings("en", localeCode("en_US").?);
    try std.testing.expect(localeCode("ko_KR") == null);
    try std.testing.expect(localeCode("") == null);
}

test "resolveLanguage: system takes the first known locale, a code is itself, junk falls back" {
    const locales = [_][]const u8{ "ko_KR", "zh_TW", "fr_CA", "de_DE" };
    try std.testing.expectEqualStrings("fr", resolveLanguage("system", &locales).code);
    try std.testing.expectEqualStrings("fr", resolveLanguage("", &locales).code);
    try std.testing.expectEqualStrings("en", resolveLanguage("system", &.{}).code);
    try std.testing.expectEqualStrings("en", resolveLanguage("system", &.{"ko_KR"}).code);
    try std.testing.expectEqualStrings("hi", resolveLanguage("hi", &locales).code);
    try std.testing.expectEqualStrings("pt-BR", resolveLanguage("pt-br", &locales).code);
    try std.testing.expectEqualStrings("zh-Hans", resolveLanguage("zh-Hans", &.{}).code);
    const junk = resolveLanguage("klingon", &locales);
    try std.testing.expect(junk.unknown_setting);
    try std.testing.expectEqualStrings("fr", junk.code);
    try std.testing.expect(!resolveLanguage("system", &locales).unknown_setting);
    const junk_en = resolveLanguage("klingon", &.{});
    try std.testing.expectEqualStrings("en", junk_en.code);
}
