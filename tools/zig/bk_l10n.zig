//! String tables for the bk-* tools (M002 S07, D062): the editor kit's `.lang` format without
//! ImGui, so a std-only tool can link it. One `tools/zig/lang/<code>.lang` per language, embedded
//! by the tool with `@embedFile`, English the reference.
//!
//! The format is the kit's (Sources/editor/kit/l10n.zig): UTF-8 without a byte order mark, CRLF
//! lines, `#` comment lines, `key=value` (the first `=` splits, so a value may hold more), the
//! escapes `\n`, `\t` and `\\`, and the placeholders `{0}`..`{9}` that `format` fills.
//!
//! The language is chosen by `--lang`, then `BK_LANG`, then the operating system, then English;
//! `resolve` is pure, so the order is tested without touching the process. A key a language lacks
//! shows the English text and logs `l10n: missing <code> <key>` once on stderr, only when
//! `BK_DEBUG_LOG=1`, so a successful run keeps an empty stderr. `BK_L10N_DIR` loads
//! `<dir>/<code>.lang` instead of the embedded table, for tests.
//!
//! A tool embeds its tables where its own path resolves them:
//!
//!     const embedded = blk: {
//!         var out: [bk_l10n.codes.len]bk_l10n.Embedded = undefined;
//!         for (bk_l10n.codes, 0..) |code, i| out[i] = .{ .code = code, .bytes = @embedFile("lang/" ++ code ++ ".lang") };
//!         break :blk out;
//!     };
const std = @import("std");
const builtin = @import("builtin");
const languages = @import("l10n_languages");

/// The environment variable that replaces the embedded tables.
pub const override_env = "BK_L10N_DIR";

/// `en` first, then the nine of M002 in the order of `l10n_languages`.
pub const codes = blk: {
    var out: [languages.languages.len + 1][]const u8 = undefined;
    out[0] = "en";
    for (languages.languages, 1..) |entry, i| out[i] = entry.code;
    break :blk out;
};

// ---------------------------------------------------------------- the format

pub const ParseError = error{ InvalidUtf8, ByteOrderMark, MissingEquals, EmptyKey, DuplicateKey, BadEscape, OutOfMemory };

pub const Entry = struct { key: []const u8, value: []const u8 };

/// Where `parse` stopped: the 1-based line of the error.
pub const Diagnostic = struct { line: usize = 0 };

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
    while (lines.next()) |raw_line| {
        line_no += 1;
        if (diag) |d| d.line = line_no;
        const text = if (raw_line.len > 0 and raw_line[raw_line.len - 1] == '\r') raw_line[0 .. raw_line.len - 1] else raw_line;
        const trimmed = std.mem.trim(u8, text, " \t");
        if (trimmed.len == 0 or trimmed[0] == '#') continue;
        const eq = std.mem.indexOfScalar(u8, text, '=') orelse return error.MissingEquals;
        if (eq == 0) return error.EmptyKey;
        const key = text[0..eq];
        const gop = try seen.getOrPut(arena, key);
        if (gop.found_existing) return error.DuplicateKey;
        try entries.append(arena, .{ .key = key, .value = try unescape(arena, text[eq + 1 ..]) });
    }
    return entries.items;
}

/// `template` with `{0}`..`{9}` replaced by the entries of `args`, into `buffer`. A slot beyond
/// `args` is left as written.
pub fn expand(buffer: []u8, template: []const u8, args: []const []const u8) error{NoSpaceLeft}![]u8 {
    var writer = std.Io.Writer.fixed(buffer);
    var at: usize = 0;
    while (at < template.len) {
        if (template[at] == '{' and at + 2 < template.len and template[at + 2] == '}' and std.ascii.isDigit(template[at + 1])) {
            const slot = template[at + 1] - '0';
            if (slot < args.len) {
                writer.writeAll(args[slot]) catch return error.NoSpaceLeft;
                at += 3;
                continue;
            }
        }
        writer.writeByte(template[at]) catch return error.NoSpaceLeft;
        at += 1;
    }
    return writer.buffered();
}

// ---------------------------------------------------------------- resolution

/// The table code a `--lang` or `BK_LANG` value names: a code of `codes`, any case, `_` for `-`.
pub fn parseCode(value: []const u8) ?[]const u8 {
    const trimmed = std.mem.trim(u8, value, " \t");
    for (codes) |code| {
        if (trimmed.len != code.len) continue;
        var same = true;
        for (trimmed, code) |a, b| {
            const left = if (a == '_') '-' else std.ascii.toLower(a);
            if (left != std.ascii.toLower(b)) same = false;
        }
        if (same) return code;
    }
    return null;
}

/// The table code an operating system locale names (`zh_CN`, `zh-Hans-CN`, `pt_PT`, `ja_JP.UTF-8`),
/// or null. Simplified Chinese is `zh`, `zh-Hans*`, `zh-CN`, `zh-SG`; Traditional Chinese has no
/// table; neither has Korean. Every Portuguese is `pt-BR`. A `.charset` or `@modifier` is ignored.
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

/// True when `locale` names a charset (`de_DE.ISO-8859-1`) that is not UTF-8. A tool prints UTF-8
/// only, so such a terminal gets English rather than mojibake.
pub fn nonUtf8Charset(locale: []const u8) bool {
    const dot = std.mem.indexOfScalar(u8, locale, '.') orelse return false;
    var rest = locale[dot + 1 ..];
    if (std.mem.indexOfScalar(u8, rest, '@')) |at| rest = rest[0..at];
    if (rest.len == 0) return false;
    return !(std.ascii.eqlIgnoreCase(rest, "utf-8") or std.ascii.eqlIgnoreCase(rest, "utf8"));
}

/// The environment variables language resolution reads, as the caller found them (null when unset).
pub const EnvView = struct {
    bk_lang: ?[]const u8 = null,
    lc_all: ?[]const u8 = null,
    lc_messages: ?[]const u8 = null,
    lang: ?[]const u8 = null,
    language: ?[]const u8 = null,
    bk_l10n_dir: ?[]const u8 = null,
    bk_debug_log: ?[]const u8 = null,

    /// `map` is anything with `get(name) ?[]const u8`, such as `init.environ_map`.
    pub fn fromMap(map: anytype) EnvView {
        return .{
            .bk_lang = map.get("BK_LANG"),
            .lc_all = map.get("LC_ALL"),
            .lc_messages = map.get("LC_MESSAGES"),
            .lang = map.get("LANG"),
            .language = map.get("LANGUAGE"),
            .bk_l10n_dir = map.get(override_env),
            .bk_debug_log = map.get("BK_DEBUG_LOG"),
        };
    }

    pub fn debugLog(self: EnvView) bool {
        const value = self.bk_debug_log orelse return false;
        return std.mem.eql(u8, value, "1");
    }
};

pub const Source = enum { lang_arg, bk_lang, env_locale, os_locale, charset, default };

pub const Resolution = struct {
    code: []const u8,
    source: Source,
};

fn usable(locale: ?[]const u8) ?[]const u8 {
    const value = std.mem.trim(u8, locale orelse return null, " \t");
    if (value.len == 0 or std.mem.eql(u8, value, "C") or std.mem.eql(u8, value, "POSIX")) return null;
    if (std.mem.startsWith(u8, value, "C.") or std.mem.startsWith(u8, value, "POSIX.")) return null;
    return value;
}

/// Names the first locale of `candidates` that has a table; a locale on a non-UTF-8 charset that
/// would have won gives English instead.
fn fromLocale(locale: []const u8, source: Source) ?Resolution {
    const code = localeCode(locale) orelse return null;
    if (nonUtf8Charset(locale)) return .{ .code = "en", .source = .charset };
    return .{ .code = code, .source = source };
}

/// The language of a run. `lang_arg` is the value `stripLangArg` returned (already a code, or null).
/// Then `BK_LANG`, then `LC_ALL`, `LC_MESSAGES`, `LANG` (`C` and `POSIX` count as unset), then the
/// `LANGUAGE` colon list, then `os_locales` (the platform list, see `OsLocales`), then English.
pub fn resolve(lang_arg: ?[]const u8, env: EnvView, os_locales: []const []const u8) Resolution {
    if (lang_arg) |value| {
        if (parseCode(value)) |code| return .{ .code = code, .source = .lang_arg };
    }
    if (env.bk_lang) |value| {
        if (parseCode(value)) |code| return .{ .code = code, .source = .bk_lang };
    }
    for ([_]?[]const u8{ env.lc_all, env.lc_messages, env.lang }) |candidate| {
        if (usable(candidate)) |locale| {
            if (fromLocale(locale, .env_locale)) |found| return found;
        }
    }
    if (usable(env.language)) |list| {
        var items = std.mem.tokenizeScalar(u8, list, ':');
        while (items.next()) |locale| {
            if (fromLocale(locale, .env_locale)) |found| return found;
        }
    }
    for (os_locales) |locale| {
        if (fromLocale(locale, .os_locale)) |found| return found;
    }
    return .{ .code = "en", .source = .default };
}

pub const StripError = error{ MissingValue, UnknownLanguage, OutOfMemory };

pub const Stripped = struct {
    /// `args` without the `--lang` options; owned by the caller's allocator.
    args: []const []const u8,
    /// The table code the last `--lang` named, or null.
    lang: ?[]const u8,
};

/// Removes `--lang=<code>` and `--lang <code>` from `args` (scanning stops at a bare `--`). A
/// missing value (`--lang`, `--lang=`, or `--lang` before another option) is `MissingValue`, a value
/// that names no table is `UnknownLanguage`; `bad_value` then holds the value so the caller can
/// print it. The caller turns both into the English message listing `validCodes` and exit 2.
pub fn stripLangArg(gpa: std.mem.Allocator, args: []const []const u8, bad_value: *[]const u8) StripError!Stripped {
    var kept: std.ArrayList([]const u8) = .empty;
    errdefer kept.deinit(gpa);
    var lang: ?[]const u8 = null;
    var at: usize = 0;
    var options_done = false;
    while (at < args.len) : (at += 1) {
        const arg = args[at];
        if (options_done) {
            try kept.append(gpa, arg);
            continue;
        }
        if (std.mem.eql(u8, arg, "--")) {
            options_done = true;
            try kept.append(gpa, arg);
            continue;
        }
        var value: []const u8 = undefined;
        if (std.mem.eql(u8, arg, "--lang")) {
            if (at + 1 >= args.len or std.mem.startsWith(u8, args[at + 1], "-")) {
                bad_value.* = "";
                return error.MissingValue;
            }
            at += 1;
            value = args[at];
        } else if (std.mem.startsWith(u8, arg, "--lang=")) {
            value = arg["--lang=".len..];
        } else {
            try kept.append(gpa, arg);
            continue;
        }
        if (value.len == 0) {
            bad_value.* = "";
            return error.MissingValue;
        }
        lang = parseCode(value) orelse {
            bad_value.* = value;
            return error.UnknownLanguage;
        };
    }
    return .{ .args = try kept.toOwnedSlice(gpa), .lang = lang };
}

/// `en, ru, zh-Hans, ...`: the codes `--lang` accepts, for the unknown-language message.
pub const valid_codes = blk: {
    var text: []const u8 = "";
    for (codes, 0..) |code, i| text = text ++ (if (i == 0) "" else ", ") ++ code;
    break :blk text;
};

// ------------------------------------------------------------ operating system

/// The Windows language ids: the primary language is the low ten bits. Chinese is Simplified only
/// for the neutral id and the PRC and Singapore sublanguages; Traditional (Taiwan, Hong Kong,
/// Macau) has no table.
pub fn langIdCode(langid: u16) ?[]const u8 {
    return switch (langid & 0x3FF) {
        0x09 => "en",
        0x19 => "ru",
        0x04 => if (langid == 0x0004 or langid == 0x0804 or langid == 0x1004) "zh-Hans" else null,
        0x11 => "ja",
        0x39 => "hi",
        0x0A => "es",
        0x0C => "fr",
        0x07 => "de",
        0x16 => "pt-BR",
        0x21 => "id",
        else => null,
    };
}

const win = struct {
    extern "kernel32" fn GetUserDefaultUILanguage() callconv(.winapi) u16;
    extern "kernel32" fn GetStdHandle(which: u32) callconv(.winapi) ?*anyopaque;
    extern "kernel32" fn GetConsoleMode(handle: ?*anyopaque, mode: *u32) callconv(.winapi) i32;
    extern "kernel32" fn GetConsoleOutputCP() callconv(.winapi) u32;
    extern "kernel32" fn SetConsoleOutputCP(code_page: u32) callconv(.winapi) i32;
};

const cf = struct {
    extern "c" fn CFLocaleCopyPreferredLanguages() ?*anyopaque;
    extern "c" fn CFArrayGetCount(array: ?*anyopaque) isize;
    extern "c" fn CFArrayGetValueAtIndex(array: ?*anyopaque, index: isize) ?*anyopaque;
    extern "c" fn CFStringGetCString(string: ?*anyopaque, buffer: [*]u8, size: isize, encoding: u32) u8;
    extern "c" fn CFRelease(object: ?*anyopaque) void;
    const utf8_encoding: u32 = 0x08000100;
};

/// The operating system's preferred locales, most preferred first. Linux has none beyond the
/// environment variables (`resolve` reads those); macOS asks CoreFoundation, which a tool links
/// with `-framework CoreFoundation`; Windows names the user's UI language.
pub const OsLocales = struct {
    pub const max = 8;
    names: [max][64]u8 = undefined,
    lens: [max]u8 = @splat(0),
    count: usize = 0,

    fn add(self: *OsLocales, name: []const u8) void {
        if (self.count == max or name.len > 63) return;
        @memcpy(self.names[self.count][0..name.len], name);
        self.lens[self.count] = @intCast(name.len);
        self.count += 1;
    }

    pub fn query() OsLocales {
        var found: OsLocales = .{};
        switch (builtin.os.tag) {
            .macos => {
                const list = cf.CFLocaleCopyPreferredLanguages() orelse return found;
                defer cf.CFRelease(list);
                const total: usize = @intCast(@max(cf.CFArrayGetCount(list), 0));
                var buffer: [64]u8 = undefined;
                for (0..@min(total, max)) |i| {
                    const item = cf.CFArrayGetValueAtIndex(list, @intCast(i));
                    if (cf.CFStringGetCString(item, &buffer, buffer.len, cf.utf8_encoding) == 0) continue;
                    found.add(std.mem.sliceTo(&buffer, 0));
                }
            },
            .windows => if (langIdCode(win.GetUserDefaultUILanguage())) |code| found.add(code),
            else => {},
        }
        return found;
    }

    /// The locales as the slice `resolve` takes; `out` holds the slices and must outlive the result.
    pub fn slice(self: *const OsLocales, out: *[max][]const u8) []const []const u8 {
        for (0..self.count) |i| out[i] = self.names[i][0..self.lens[i]];
        return out[0..self.count];
    }
};

/// Makes a Windows console print UTF-8 and puts the old code page back at exit. Redirected output,
/// and every other platform, is left alone: the bytes are UTF-8 already.
pub const Console = struct {
    old_code_page: u32 = 0,
    active: bool = false,

    fn isConsole(which: i32) bool {
        const handle = win.GetStdHandle(@bitCast(which)) orelse return false;
        if (@intFromPtr(handle) == std.math.maxInt(usize)) return false;
        var mode: u32 = 0;
        return win.GetConsoleMode(handle, &mode) != 0;
    }

    pub fn enableUtf8() Console {
        if (builtin.os.tag != .windows) return .{};
        if (!isConsole(-11) and !isConsole(-12)) return .{};
        const old = win.GetConsoleOutputCP();
        if (win.SetConsoleOutputCP(65001) == 0) return .{};
        return .{ .old_code_page = old, .active = true };
    }

    pub fn restore(self: *Console) void {
        if (builtin.os.tag != .windows or !self.active) return;
        _ = win.SetConsoleOutputCP(self.old_code_page);
        self.active = false;
    }
};

// -------------------------------------------------------------------- tables

pub const Embedded = struct { code: []const u8, bytes: []const u8 };

/// Called with `(code, key)` the first time a key is missing from a language.
pub const MissingFn = *const fn (code: []const u8, key: []const u8) void;

fn logMissing(code: []const u8, key: []const u8) void {
    std.debug.print("l10n: missing {s} {s}\n", .{ code, key });
}

pub const Options = struct {
    /// `BK_L10N_DIR`: read `<dir>/<code>.lang` when it exists, else the embedded table.
    override_dir: ?[]const u8 = null,
    /// `BK_DEBUG_LOG=1`: report missing keys and unreadable tables on stderr.
    debug_log: bool = false,
    /// Replaces the stderr line, for tests.
    on_missing: MissingFn = logMissing,
};

/// The tables of one run. Texts returned by `get` live until `deinit`.
pub const Set = struct {
    const Table = struct {
        values: std.StringHashMapUnmanaged([]const u8) = .empty,
        announced: std.StringHashMapUnmanaged(void) = .empty,
    };

    arena: std.heap.ArenaAllocator,
    io: std.Io,
    embedded: []const Embedded,
    options: Options,
    tables: [codes.len]?Table = @splat(null),
    current: usize = 0,

    pub fn init(gpa: std.mem.Allocator, io: std.Io, embedded: []const Embedded, options: Options) Set {
        return .{ .arena = std.heap.ArenaAllocator.init(gpa), .io = io, .embedded = embedded, .options = options };
    }

    pub fn deinit(self: *Set) void {
        self.arena.deinit();
    }

    fn index(code: []const u8) ?usize {
        for (codes, 0..) |known, i| {
            if (std.mem.eql(u8, known, code)) return i;
        }
        return null;
    }

    fn embeddedBytes(self: *const Set, code: []const u8) []const u8 {
        for (self.embedded) |entry| {
            if (std.mem.eql(u8, entry.code, code)) return entry.bytes;
        }
        return "";
    }

    fn note(self: *const Set, comptime fmt: []const u8, args: anytype) void {
        if (self.options.debug_log) std.debug.print(fmt, args);
    }

    /// Parses a table, falling back from the override file to the embedded text and from that to an
    /// empty table (every key then shows the English text).
    fn load(self: *Set, i: usize) *Table {
        const arena = self.arena.allocator();
        const code = codes[i];
        var loaded: Table = .{};
        var from_file: ?[]const u8 = null;
        if (self.options.override_dir) |dir| {
            if (std.fmt.allocPrint(arena, "{s}{c}{s}.lang", .{ dir, std.fs.path.sep, code })) |path| {
                from_file = std.Io.Dir.cwd().readFileAlloc(self.io, path, arena, .limited(1 << 20)) catch null;
            } else |_| {}
        }
        var diag: Diagnostic = .{};
        const entries = blk: {
            if (from_file) |bytes| {
                if (parse(arena, bytes, &diag)) |ok| break :blk ok else |err| {
                    self.note("l10n: {s}.lang line {d}: {s}\n", .{ code, diag.line, @errorName(err) });
                }
            }
            break :blk parse(arena, self.embeddedBytes(code), &diag) catch |err| {
                self.note("l10n: {s}.lang line {d}: {s}\n", .{ code, diag.line, @errorName(err) });
                break :blk &[_]Entry{};
            };
        };
        for (entries) |entry| loaded.values.put(arena, entry.key, entry.value) catch {};
        self.tables[i] = loaded;
        return &self.tables[i].?;
    }

    fn table(self: *Set, i: usize) *Table {
        if (self.tables[i]) |*loaded| return loaded;
        return self.load(i);
    }

    /// Selects the language; a code that is not in `codes` selects English.
    pub fn setLanguage(self: *Set, code: []const u8) void {
        self.current = index(code) orelse 0;
        _ = self.table(self.current);
    }

    pub fn language(self: *const Set) []const u8 {
        return codes[self.current];
    }

    /// The text of `key` in the current language, else the English text, else the key itself, so the
    /// result is never empty for a non-empty key.
    pub fn get(self: *Set, key: []const u8) []const u8 {
        const own = self.table(self.current);
        if (own.values.get(key)) |text| return text;
        if (self.current != 0) {
            const gop = own.announced.getOrPut(self.arena.allocator(), key) catch return key;
            if (!gop.found_existing and self.options.debug_log) self.options.on_missing(codes[self.current], key);
            if (self.table(0).values.get(key)) |text| return text;
        }
        return key;
    }

    /// `get(key)` with `{0}`..`{9}` filled from `args` (strings or integers), into `buffer`.
    pub fn format(self: *Set, buffer: []u8, key: []const u8, args: anytype) error{NoSpaceLeft}![]u8 {
        var slots: [args.len][]const u8 = undefined;
        var scratch: [args.len][24]u8 = undefined;
        inline for (args, 0..) |arg, i| {
            switch (@typeInfo(@TypeOf(arg))) {
                .int, .comptime_int => slots[i] = std.fmt.bufPrint(&scratch[i], "{d}", .{arg}) catch return error.NoSpaceLeft,
                else => slots[i] = arg,
            }
        }
        return expand(buffer, self.get(key), &slots);
    }
};
