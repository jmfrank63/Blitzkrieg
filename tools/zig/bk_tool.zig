//! The conventions every `bk-*` modding tool keeps (Phase 8, D-04), written
//! once so that seven tools do not drift into seven behaviours:
//!
//! - `--help` (or `-h`) anywhere prints the usage on stdout and exits 0.
//! - Bad usage prints the usage on stderr and exits 2.
//! - A failed operation exits 1 with `<tool>: <reason>` on stderr and nothing
//!   on stdout, so a script can tell "you called me wrong" from "it failed".
//! - Success prints a one-line summary (a listing tool prints its listing
//!   first and the summary last).
//! - Paths on the command line may use `/` or `\` on every host; paths a tool
//!   prints use the host's separator. Paths are UTF-8 (Zig's own argument and
//!   file APIs are WTF-8 on Windows, so nothing here re-encodes them).
//! - A tool writes only its named output (or an explicit `--out`) and its
//!   output is deterministic: sorted iteration, no timestamps.
//!
//! - Messages and help come from string tables (`lang/<code>.lang`, `bk_l10n.zig`, D062): the
//!   language is `--lang`, then `BK_LANG`, then the operating system, then English. What stays
//!   English in every language, because scripts and tests parse it: the `usage: <tool> ` prefix
//!   and the synopsis lines, the `<tool>: ` prefix of a failure, the exit codes, and the summary
//!   lines a tool prints on success.
//!
//! `bk_tool_conventions_test.zig` runs every built tool against the exit-code
//! and stream rules; the path helpers are tested here.

const std = @import("std");
const bk_l10n = @import("bk_l10n.zig");

pub const exit_ok: u8 = 0;
pub const exit_failed: u8 = 1;
pub const exit_usage: u8 = 2;

/// Returned by a tool's `run` to mean "print the usage and exit 2".
/// `error.Failed` means the reason is already on stderr (see `Context.fail`).
pub const Error = error{ Usage, Failed };

/// Every table the tools share, embedded so an installed tool needs no files beside it. One
/// `en.lang` holds the keys of all four Zig tools; a tool simply never asks for another's keys.
pub const embedded = blk: {
    var out: [bk_l10n.codes.len]bk_l10n.Embedded = undefined;
    for (bk_l10n.codes, 0..) |code, i| out[i] = .{ .code = code, .bytes = @embedFile("lang/" ++ code ++ ".lang") };
    break :blk out;
};

/// Fails the build when a message key is not in `en.lang`, so a typo cannot reach a modder as the
/// bare key.
pub fn requireKey(comptime key: []const u8) void {
    @setEvalBranchQuota(2_000_000);
    const en = embedded[0].bytes;
    const present = comptime std.mem.startsWith(u8, en, key ++ "=") or std.mem.indexOf(u8, en, "\n" ++ key ++ "=") != null;
    if (!present)
        @compileError("string table key '" ++ key ++ "' is not in tools/zig/lang/en.lang");
}

/// What a tool says for `--help` and for bad usage. The synopsis (`usage: <tool> ...` and its
/// continuation lines, ending in a newline) is English code; each body entry is the key of one
/// translated paragraph, and paragraphs are separated by a blank line. The `--lang` paragraph
/// follows them for every tool.
pub const Usage = struct {
    synopsis: []const u8,
    body: []const []const u8,
};

pub const Context = struct {
    io: std.Io,
    gpa: std.mem.Allocator,
    /// Lives until the tool exits; for formatted messages.
    arena: std.mem.Allocator,
    texts: *bk_l10n.Set,
    /// The tool's arguments without argv[0], `--help` already handled.
    args: []const []const u8,
    out: *std.Io.Writer,
    err: *std.Io.Writer,
    tool_name: []const u8,

    /// Write `<tool>: <reason>` to stderr and return `error.Failed`, so the
    /// call site reads `return ctx.fail(...)`.
    pub fn fail(ctx: *Context, comptime fmt: []const u8, args: anytype) error{Failed} {
        ctx.report(fmt, args);
        return error.Failed;
    }

    /// Like `fail`, with the reason from the string table: `key` names a text whose `{0}`..`{9}`
    /// are filled from `args` (strings or integers). Library error names stay English as arguments.
    pub fn failKey(ctx: *Context, comptime key: []const u8, args: anytype) error{Failed} {
        ctx.noteKey(key, args);
        return error.Failed;
    }

    /// `failKey` without the error: a warning line for a batch that goes on, `<tool>: <reason>`.
    pub fn noteKey(ctx: *Context, comptime key: []const u8, args: anytype) void {
        const reason = ctx.formatKey(key, args);
        ctx.err.print("{s}: ", .{ctx.tool_name}) catch {};
        ctx.err.writeAll(reason) catch {};
        ctx.err.writeAll("\n") catch {};
    }

    /// The text of `key` in the run's language, for use as a `failKey` argument.
    pub fn text(ctx: *Context, comptime key: []const u8) []const u8 {
        requireKey(key);
        return ctx.texts.get(key);
    }

    /// `key` with its slots filled; on running out of memory the unfilled text is better than none.
    pub fn formatKey(ctx: *Context, comptime key: []const u8, args: anytype) []const u8 {
        return formatText(ctx.arena, ctx.texts, key, args);
    }

    fn report(ctx: *Context, comptime fmt: []const u8, args: anytype) void {
        ctx.err.print("{s}: ", .{ctx.tool_name}) catch {};
        ctx.err.print(fmt ++ "\n", args) catch {};
    }
};

/// `key` of `texts` with its slots filled, into `arena`; on running out of memory the unfilled text
/// is better than none.
pub fn formatText(arena: std.mem.Allocator, texts: *bk_l10n.Set, comptime key: []const u8, args: anytype) []const u8 {
    requireKey(key);
    const template = texts.get(key);
    var size: usize = template.len;
    inline for (args) |arg| {
        size += switch (@typeInfo(@TypeOf(arg))) {
            .int, .comptime_int => 24,
            else => arg.len,
        };
    }
    const buffer = arena.alloc(u8, size) catch return template;
    return texts.format(buffer, key, args) catch template;
}

/// The help text: the English synopsis, then the translated paragraphs and the `--lang` paragraph.
fn buildUsage(arena: std.mem.Allocator, texts: *bk_l10n.Set, comptime usage: Usage) std.mem.Allocator.Error![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(arena, usage.synopsis);
    inline for (usage.body) |key| {
        requireKey(key);
        try out.appendSlice(arena, "\n");
        try out.appendSlice(arena, texts.get(key));
        try out.appendSlice(arena, "\n");
    }
    requireKey("tool.usage.lang");
    var scratch: [1024]u8 = undefined;
    try out.appendSlice(arena, "\n");
    try out.appendSlice(arena, texts.format(&scratch, "tool.usage.lang", .{bk_l10n.valid_codes}) catch texts.get("tool.usage.lang"));
    try out.appendSlice(arena, "\n");
    return out.items;
}

/// The whole `main` of a tool: collect the arguments, choose the language, apply the help and exit
/// code rules, and turn any error `run` did not explain into a one-line reason rather than a Zig
/// stack trace.
pub fn main(
    init: std.process.Init,
    comptime tool_name: []const u8,
    comptime usage: Usage,
    comptime run: fn (*Context) anyerror!void,
) u8 {
    const gpa = init.gpa;
    const io = init.io;

    var console = bk_l10n.Console.enableUtf8();
    defer console.restore();
    var stdout_buffer: [4096]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(io, &stdout_buffer);
    var stderr_buffer: [1024]u8 = undefined;
    var stderr = std.Io.File.stderr().writer(io, &stderr_buffer);
    defer stdout.interface.flush() catch {};
    defer stderr.interface.flush() catch {};

    const arena = init.arena.allocator();
    const raw_args = std.process.Args.toSlice(init.minimal.args, arena) catch {
        stderr.interface.writeAll(tool_name ++ ": out of memory reading the arguments\n") catch {};
        return exit_failed;
    };
    const given_args = arena.alloc([]const u8, raw_args.len -| 1) catch {
        stderr.interface.writeAll(tool_name ++ ": out of memory reading the arguments\n") catch {};
        return exit_failed;
    };
    for (given_args, 1..) |*arg, i| arg.* = raw_args[i];

    const env = bk_l10n.EnvView.fromMap(init.environ_map);
    var texts = bk_l10n.Set.init(gpa, io, &embedded, .{ .override_dir = env.bk_l10n_dir, .debug_log = env.debugLog() });
    defer texts.deinit();

    // `--lang` is not one of the tool's options, so it is taken out before anything else looks.
    var bad_value: []const u8 = "";
    const stripped = bk_l10n.stripLangArg(arena, given_args, &bad_value) catch |err| {
        // The language is not known yet: English message listing the codes, then the English usage.
        texts.setLanguage("en");
        const shown = buildUsage(arena, &texts, usage) catch "";
        switch (err) {
            error.UnknownLanguage => stderr.interface.print(tool_name ++ ": unknown language '{s}'; use one of: {s}\n", .{ bad_value, bk_l10n.valid_codes }) catch {},
            else => stderr.interface.print(tool_name ++ ": --lang needs a language code; use one of: {s}\n", .{bk_l10n.valid_codes}) catch {},
        }
        stderr.interface.writeAll(shown) catch {};
        return if (err == error.OutOfMemory) exit_failed else exit_usage;
    };
    const tool_args = stripped.args;

    var os_locales = bk_l10n.OsLocales.forEnv(env);
    var os_slots: [bk_l10n.OsLocales.max][]const u8 = undefined;
    texts.setLanguage(bk_l10n.resolve(stripped.lang, env, os_locales.slice(&os_slots)).code);
    const shown_usage = buildUsage(arena, &texts, usage) catch {
        stderr.interface.writeAll(tool_name ++ ": out of memory\n") catch {};
        return exit_failed;
    };

    for (tool_args) |arg| {
        if (isHelp(arg)) {
            stdout.interface.writeAll(shown_usage) catch {};
            return exit_ok;
        }
    }

    var ctx: Context = .{
        .io = io,
        .gpa = gpa,
        .arena = arena,
        .texts = &texts,
        .args = tool_args,
        .out = &stdout.interface,
        .err = &stderr.interface,
        .tool_name = tool_name,
    };
    run(&ctx) catch |err| switch (err) {
        error.Usage => {
            stderr.interface.writeAll(shown_usage) catch {};
            return exit_usage;
        },
        error.Failed => return exit_failed,
        else => {
            ctx.report("{s}", .{@errorName(err)});
            return exit_failed;
        },
    };
    return exit_ok;
}

pub fn isHelp(arg: []const u8) bool {
    return std.mem.eql(u8, arg, "--help") or std.mem.eql(u8, arg, "-h");
}

/// A command-line path in the host's form: both `/` and `\` are separators on
/// input, whatever the host. Caller owns the result.
pub fn inputPath(gpa: std.mem.Allocator, arg: []const u8) ![]u8 {
    const path = try gpa.dupe(u8, arg);
    toHostSeparators(path);
    return path;
}

/// Rewrite every `/` and `\` in place to the host separator; used for both
/// input paths and paths a tool prints.
pub fn toHostSeparators(path: []u8) void {
    for (path) |*c| {
        if (c.* == '/' or c.* == '\\') c.* = std.fs.path.sep;
    }
}

test "both separators are accepted and the host's is written" {
    const gpa = std.testing.allocator;
    const path = try inputPath(gpa, "mods/my mod\\Data/units");
    defer gpa.free(path);
    const sep = std.fs.path.sep_str;
    try std.testing.expectEqualStrings("mods" ++ sep ++ "my mod" ++ sep ++ "Data" ++ sep ++ "units", path);
}

test "help is recognised in both spellings only" {
    try std.testing.expect(isHelp("--help"));
    try std.testing.expect(isHelp("-h"));
    try std.testing.expect(!isHelp("help"));
    try std.testing.expect(!isHelp("--helpme"));
}
