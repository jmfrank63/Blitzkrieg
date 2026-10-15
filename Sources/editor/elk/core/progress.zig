//! D-14's progress and Cancel: what a long operation on ELK's worker thread
//! shares with the progress modal (`CProgressDialog`'s range and position)
//! and how it learns that Cancel was pressed. The work calls `advance` once
//! before each unit (a text, a row); a cancelled run stops there with
//! `error.Cancelled`, between two units, so no unit is left half done and
//! every file it wrote went through `writeFileAtomic` whole. Every field is
//! atomic: the UI thread reads and cancels while the worker counts.
//!
//! `hold` is the scripted runs' seam: the work waits before the unit of
//! that number until it is released or cancelled, so a schedule can see the
//! modal mid-run and press its Cancel at a known place.
const std = @import("std");

pub const Error = error{Cancelled};

pub const Progress = struct {
    position: std.atomic.Value(usize) = .init(0),
    range: std.atomic.Value(usize) = .init(0),
    cancelled: std.atomic.Value(bool) = .init(false),
    hold: std.atomic.Value(usize) = .init(no_hold),
    /// Set while the work waits at `hold`.
    held: std.atomic.Value(bool) = .init(false),

    pub const no_hold = std.math.maxInt(usize);

    /// The units the work will go through, the position back at 0.
    pub fn start(self: ?*Progress, range: usize) void {
        const progress = self orelse return;
        progress.range.store(range, .release);
        progress.position.store(0, .release);
    }

    /// Before each unit: stops the work when Cancel was pressed, waits at
    /// `hold`, then counts the unit.
    pub fn advance(self: ?*Progress, io: std.Io) Error!void {
        const progress = self orelse return;
        const position = progress.position.load(.acquire);
        if (position == progress.hold.load(.acquire)) {
            progress.held.store(true, .release);
            defer progress.held.store(false, .release);
            while (progress.hold.load(.acquire) == position and !progress.cancelled.load(.acquire)) {
                io.sleep(.fromMilliseconds(2), .awake) catch break;
            }
        }
        if (progress.cancelled.load(.acquire)) return Error.Cancelled;
        progress.position.store(position + 1, .release);
    }

    /// Whether Cancel was pressed; for work that has one last step left
    /// after its units (writing the result), checked just before it.
    pub fn check(self: ?*Progress) Error!void {
        const progress = self orelse return;
        if (progress.cancelled.load(.acquire)) return Error.Cancelled;
    }

    pub fn cancel(self: *Progress) void {
        self.cancelled.store(true, .release);
    }

    pub fn release(self: *Progress) void {
        self.hold.store(no_hold, .release);
    }
};

const testing = std.testing;

test "advance counts each unit and stops between two once cancelled" {
    const io = testing.io;
    var progress: Progress = .{};
    Progress.start(&progress, 3);
    try Progress.advance(&progress, io);
    try Progress.advance(&progress, io);
    try testing.expectEqual(@as(usize, 2), progress.position.load(.acquire));
    progress.cancel();
    try testing.expectError(Error.Cancelled, Progress.advance(&progress, io));
    try testing.expectError(Error.Cancelled, Progress.check(&progress));
    try testing.expectEqual(@as(usize, 2), progress.position.load(.acquire));
    // No progress is the synchronous callers' case: nothing to count or stop.
    try Progress.advance(null, io);
    try Progress.check(null);
}

test "a held unit waits on another thread until released or cancelled" {
    const io = testing.io;
    const Worker = struct {
        fn run(progress: *Progress, done: *std.atomic.Value(usize), result: *?Error) void {
            for (0..4) |_| {
                Progress.advance(progress, testing.io) catch |err| {
                    result.* = err;
                    break;
                };
                _ = done.fetchAdd(1, .acq_rel);
            }
        }
    };
    for ([_]bool{ false, true }) |cancel| {
        var progress: Progress = .{};
        progress.hold.store(2, .release);
        Progress.start(&progress, 4);
        var done: std.atomic.Value(usize) = .init(0);
        var result: ?Error = null;
        const thread = try std.Thread.spawn(.{}, Worker.run, .{ &progress, &done, &result });
        while (!progress.held.load(.acquire)) try io.sleep(.fromMilliseconds(1), .awake);
        try testing.expectEqual(@as(usize, 2), done.load(.acquire));
        if (cancel) progress.cancel() else progress.release();
        thread.join();
        try testing.expectEqual(@as(usize, if (cancel) 2 else 4), done.load(.acquire));
        try testing.expectEqual(if (cancel) @as(?Error, Error.Cancelled) else null, result);
    }
}
