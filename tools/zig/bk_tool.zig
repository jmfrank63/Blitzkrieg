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
//! `bk_tool_conventions_test.zig` runs every built tool against the exit-code
//! and stream rules; the path helpers are tested here.

const std = @import("std");

pub const exit_ok: u8 = 0;
pub const exit_failed: u8 = 1;
pub const exit_usage: u8 = 2;

/// Returned by a tool's `run` to mean "print the usage and exit 2".
/// `error.Failed` means the reason is already on stderr (see `Context.fail`).
pub const Error = error{ Usage, Failed };

pub const Context = struct {
    io: std.Io,
    gpa: std.mem.Allocator,
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

    fn report(ctx: *Context, comptime fmt: []const u8, args: anytype) void {
        ctx.err.print("{s}: ", .{ctx.tool_name}) catch {};
        ctx.err.print(fmt ++ "\n", args) catch {};
    }
};

/// The whole `main` of a tool: collect the arguments, apply the help and exit
/// code rules, and turn any error `run` did not explain into a one-line reason
/// rather than a Zig stack trace.
pub fn main(
    init: std.process.Init,
    comptime tool_name: []const u8,
    comptime usage: []const u8,
    comptime run: fn (*Context) anyerror!void,
) u8 {
    const gpa = init.gpa;
    const io = init.io;

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
    const tool_args = arena.alloc([]const u8, raw_args.len -| 1) catch {
        stderr.interface.writeAll(tool_name ++ ": out of memory reading the arguments\n") catch {};
        return exit_failed;
    };
    for (tool_args, 1..) |*arg, i| arg.* = raw_args[i];

    for (tool_args) |arg| {
        if (isHelp(arg)) {
            stdout.interface.writeAll(usage) catch {};
            return exit_ok;
        }
    }

    var ctx: Context = .{
        .io = io,
        .gpa = gpa,
        .args = tool_args,
        .out = &stdout.interface,
        .err = &stderr.interface,
        .tool_name = tool_name,
    };
    run(&ctx) catch |err| switch (err) {
        error.Usage => {
            stderr.interface.writeAll(usage) catch {};
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
