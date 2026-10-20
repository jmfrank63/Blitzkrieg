//! The C ABI of `bk_l10n.zig` (M002 S07, D062), for the C++ tools bk-font, bk-map and bk-sprite:
//! the same language order, `--lang` option and tables as the Zig tools, behind `bk_l10n.h`. It is
//! built as a static library that a tool links; the tables are embedded here.
//!
//! The tools are single-threaded at the point they ask for texts, so the state is one set of
//! globals. Its memory comes from the page allocator and is left to the process exit, the way a
//! tool's arguments are.
const std = @import("std");
const bk_l10n = @import("bk_l10n.zig");

const embedded = blk: {
    var out: [bk_l10n.codes.len]bk_l10n.Embedded = undefined;
    for (bk_l10n.codes, 0..) |code, i| out[i] = .{ .code = code, .bytes = @embedFile("lang/" ++ code ++ ".lang") };
    break :blk out;
};

/// What `bk_l10n_init` could not accept: the values of the header's `BK_L10N_*` constants.
const status_ok = 0;
const status_missing_value = 1;
const status_unknown_language = 2;

const State = struct {
    set: bk_l10n.Set,
    kept: [][*c]u8,
    kept_count: c_int,
    bad_value: [:0]const u8,
};

var state: ?State = null;

const EnvMap = struct {
    pub fn get(_: EnvMap, name: []const u8) ?[]const u8 {
        var zname: [64:0]u8 = undefined;
        if (name.len >= zname.len) return null;
        @memcpy(zname[0..name.len], name);
        zname[name.len] = 0;
        const value = std.c.getenv(&zname) orelse return null;
        return std.mem.span(value);
    }
};

fn io() std.Io {
    return std.Io.Threaded.global_single_threaded.io();
}

fn chooseLanguage(set: *bk_l10n.Set, lang: ?[]const u8, env: bk_l10n.EnvView) void {
    var os_locales = bk_l10n.OsLocales.query();
    var os_slots: [bk_l10n.OsLocales.max][]const u8 = undefined;
    set.setLanguage(bk_l10n.resolve(lang, env, os_locales.slice(&os_slots)).code);
}

/// `text` and a terminator, from the page allocator.
fn dupeTerminated(text: []const u8) ?[:0]const u8 {
    const buffer = std.heap.page_allocator.alloc(u8, text.len + 1) catch return null;
    @memcpy(buffer[0..text.len], text);
    buffer[text.len] = 0;
    return buffer[0..text.len :0];
}

/// The state of a run that never called `bk_l10n_init` (a test of a helper): the environment only.
fn ensure() *State {
    if (state == null) {
        const env = bk_l10n.EnvView.fromMap(EnvMap{});
        var set = bk_l10n.Set.init(std.heap.page_allocator, io(), &embedded, .{ .override_dir = env.bk_l10n_dir, .debug_log = env.debugLog() });
        chooseLanguage(&set, null, env);
        state = .{ .set = set, .kept = &.{}, .kept_count = 0, .bad_value = "" };
    }
    return &state.?;
}

/// Chooses the language from `--lang`, `BK_LANG`, the environment and the operating system, and
/// takes `--lang` out of the arguments (read them with `bk_l10n_args`). A bad `--lang` is the return
/// code and `bk_l10n_bad_value`; the language is then English, so the caller can print its English
/// message and usage.
export fn bk_l10n_init(argc: c_int, argv: [*c][*c]u8) c_int {
    const page = std.heap.page_allocator;
    const env = bk_l10n.EnvView.fromMap(EnvMap{});
    var set = bk_l10n.Set.init(page, io(), &embedded, .{ .override_dir = env.bk_l10n_dir, .debug_log = env.debugLog() });

    const total: usize = if (argc > 0) @intCast(argc) else 0;
    const given = page.alloc([]const u8, total) catch return status_ok;
    for (given, 0..) |*slot, i| slot.* = std.mem.span(@as([*:0]const u8, @ptrCast(argv[i])));

    var bad_value: []const u8 = "";
    var status: c_int = status_ok;
    var lang: ?[]const u8 = null;
    // The program name is not an option, so scanning starts after it.
    const tail = if (given.len > 0) given[1..] else given;
    var kept_slices: []const []const u8 = tail;
    if (bk_l10n.stripLangArg(page, tail, &bad_value)) |stripped| {
        lang = stripped.lang;
        kept_slices = stripped.args;
    } else |err| {
        status = if (err == error.UnknownLanguage) status_unknown_language else status_missing_value;
    }

    const kept = page.alloc([*c]u8, kept_slices.len + 2) catch return status_ok;
    var count: usize = 0;
    if (total > 0) {
        kept[0] = argv[0];
        count = 1;
    }
    for (kept_slices) |arg| {
        kept[count] = @ptrCast(@constCast(arg.ptr));
        count += 1;
    }
    kept[count] = null;

    chooseLanguage(&set, if (status == status_ok) lang else "en", env);
    const copy = dupeTerminated(bad_value) orelse "";
    state = .{ .set = set, .kept = kept, .kept_count = @intCast(count), .bad_value = copy };
    return status;
}

/// The arguments without `--lang`, null-terminated like `argv`; `*argc` is set to their count. Before
/// `bk_l10n_init` ran, or when it rejected `--lang`, this is not meaningful.
export fn bk_l10n_args(argc: *c_int) [*c][*c]u8 {
    const current = ensure();
    argc.* = current.kept_count;
    return current.kept.ptr;
}

/// The value the last rejected `--lang` carried ("" when it had none).
export fn bk_l10n_bad_value() [*:0]const u8 {
    return ensure().bad_value.ptr;
}

/// `en, ru, zh-Hans, ...`: the codes `--lang` accepts.
export fn bk_l10n_valid_codes() [*:0]const u8 {
    const terminated_codes = bk_l10n.valid_codes ++ "\x00";
    return @ptrCast(terminated_codes.ptr);
}

/// The code of the language in use.
export fn bk_l10n_language() [*:0]const u8 {
    const code = ensure().set.language();
    inline for (bk_l10n.codes) |known| {
        if (std.mem.eql(u8, known, code)) {
            const terminated_code = known ++ "\x00";
            return @ptrCast(terminated_code.ptr);
        }
    }
    return "en";
}

/// The text of `key` in the language in use, else the English text, else the key: never null. The
/// text lives to the end of the process.
export fn bk_l10n_get(key: [*:0]const u8) [*:0]const u8 {
    const current = ensure();
    const text = current.set.get(std.mem.span(key));
    // Table values are slices of the arena without a terminator; terminate a copy once per key.
    return terminated(text);
}

var terminated_cache: std.StringHashMapUnmanaged([:0]const u8) = .empty;

fn terminated(text: []const u8) [*:0]const u8 {
    const page = std.heap.page_allocator;
    if (terminated_cache.get(text)) |found| return found.ptr;
    const copy = dupeTerminated(text) orelse return "";
    terminated_cache.put(page, text, copy) catch {};
    return copy.ptr;
}

/// `bk_l10n_get(key)` with `{0}`..`{9}` filled from `args[0..count]`, written into `buffer` and
/// terminated. Returns the length the text needs without the terminator (as `snprintf` does), so a
/// result of `capacity` or more was cut short.
export fn bk_l10n_format(buffer: [*c]u8, capacity: usize, key: [*:0]const u8, args: [*c]const [*c]const u8, count: usize) usize {
    const current = ensure();
    const template = current.set.get(std.mem.span(key));
    const page = std.heap.page_allocator;
    const slots = page.alloc([]const u8, @min(count, 10)) catch return 0;
    defer page.free(slots);
    for (slots, 0..) |*slot, i| slot.* = if (args[i] == null) "" else std.mem.span(@as([*:0]const u8, @ptrCast(args[i])));

    // Expand into a buffer that is large enough, then copy what fits.
    var need: usize = template.len;
    for (slots) |slot| need += slot.len;
    const scratch = page.alloc(u8, need + 1) catch return 0;
    defer page.free(scratch);
    const text = bk_l10n.expand(scratch, template, slots) catch template;
    if (capacity > 0 and buffer != null) {
        const n = @min(text.len, capacity - 1);
        @memcpy(buffer[0..n], text[0..n]);
        buffer[n] = 0;
    }
    return text.len;
}

/// Makes a Windows console print UTF-8 (a no-op elsewhere and when output is redirected). The old
/// code page is put back by `bk_l10n_console_restore`.
var console: bk_l10n.Console = .{};

export fn bk_l10n_console_utf8() void {
    console = bk_l10n.Console.enableUtf8();
}

export fn bk_l10n_console_restore() void {
    console.restore();
}

