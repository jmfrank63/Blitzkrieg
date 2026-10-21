//! `std.process.run` with a timeout that holds on Windows too.
//!
//! On a timeout, `std.process.run` (Zig 0.17) tears its pipe reader down
//! before it kills the child, because its defers unwind in reverse order. On
//! Windows that teardown cancels the pending pipe reads and waits for them, and
//! they only end when the child's end of the pipe closes: a child that hung
//! without output, such as a Game stuck in a modal error box no one can see,
//! hung the test with it, and the timeout never fired. This copy kills the
//! child first, so the reads end and the caller gets `error.Timeout`.

const std = @import("std");

pub fn run(gpa: std.mem.Allocator, io: std.Io, options: std.process.RunOptions) std.process.RunError!std.process.RunResult {
    var child = try std.process.spawn(io, .{
        .exe = options.exe,
        .argv = options.argv,
        .cwd = options.cwd,
        .environ_map = options.environ_map,
        .expand_arg0 = options.expand_arg0,
        .progress_node = options.progress_node,
        .create_no_window = options.create_no_window,

        .stdin = .ignore,
        .stdout = .pipe,
        .stderr = .pipe,
    });

    var multi_reader_buffer: std.Io.File.MultiReader.Buffer(2) = undefined;
    var multi_reader: std.Io.File.MultiReader = undefined;
    multi_reader.init(gpa, io, multi_reader_buffer.toStreams(), &.{ child.stdout.?, child.stderr.? });
    defer multi_reader.deinit();
    // Declared after the reader's deinit, so it runs before it.
    defer child.kill(io);

    const stdout_reader = multi_reader.reader(0);
    const stderr_reader = multi_reader.reader(1);

    while (multi_reader.fill(options.reserve_amount, options.timeout)) |_| {
        if (options.stdout_limit.toInt()) |limit| {
            if (stdout_reader.buffered().len > limit) return error.StreamTooLong;
        }
        if (options.stderr_limit.toInt()) |limit| {
            if (stderr_reader.buffered().len > limit) return error.StreamTooLong;
        }
    } else |err| switch (err) {
        error.EndOfStream => {},
        else => |e| return e,
    }

    try multi_reader.checkAnyError();

    const term = try child.wait(io);

    const stdout_slice = try multi_reader.toOwnedSlice(0);
    errdefer gpa.free(stdout_slice);
    const stderr_slice = try multi_reader.toOwnedSlice(1);
    errdefer gpa.free(stderr_slice);

    return .{ .stdout = stdout_slice, .stderr = stderr_slice, .term = term };
}

test "a child that outlives the timeout is killed and the run ends with Timeout" {
    const io = std.testing.io;
    const argv: []const []const u8 = if (@import("builtin").os.tag == .windows)
        &.{ "powershell", "-NoProfile", "-Command", "Start-Sleep -Seconds 60" }
    else
        &.{ "sleep", "60" };
    const started = std.Io.Clock.awake.now(io);
    const result = run(std.testing.allocator, io, .{
        .argv = argv,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(2), .clock = .awake } },
    });
    try std.testing.expectError(error.Timeout, result);
    const waited = started.durationTo(std.Io.Clock.awake.now(io));
    try std.testing.expect(waited.toSeconds() < 30);
}
