//! D-14's worker: one long operation at a time (`Session.perform`) on its
//! own thread, so the window keeps repainting and the progress modal can
//! show the position and take Cancel. The job owns a copy of its input
//! (paths, the mod's name, the filter, the searched text), so nothing it
//! reads changes under it; the UI thread starts it after `Session.begin`
//! and hands its result to `Session.finish` once `done` says so. The
//! thread gets std's default stack, 16 MiB, which Linux needs for glibc's
//! static TLS (AGENTS.md).
const std = @import("std");
const elk = @import("elk_core");
const session_mod = @import("session.zig");

const Session = session_mod.Session;
const Work = session_mod.Work;
const Outcome = session_mod.Outcome;

pub const Job = struct {
    gpa: std.mem.Allocator,
    session: *Session,
    /// The input, its slices in `arena`.
    work: Work,
    arena: std.heap.ArenaAllocator,
    progress: session_mod.Progress = .{},
    /// What the modal says, as `CProgressDialog`'s message did.
    message: [768:0]u8 = @splat(0),
    /// When it started, in the UI's milliseconds.
    started_ms: u64,
    result: anyerror!Outcome = error.NotFinished,
    finished: std.atomic.Value(bool) = .init(false),
    thread: std.Thread = undefined,

    /// Copies `work` and starts it; `hold` is the scripted runs' seam
    /// (`Progress.hold`). The caller destroys the job after `finish`.
    pub fn start(gpa: std.mem.Allocator, session: *Session, work: Work, message: []const u8, now_ms: u64, hold: ?usize) !*Job {
        const job = try gpa.create(Job);
        errdefer gpa.destroy(job);
        job.* = .{ .gpa = gpa, .session = session, .work = undefined, .arena = .init(gpa), .started_ms = now_ms };
        errdefer job.arena.deinit();
        job.work = try owned(job.arena.allocator(), work);
        const len = @min(message.len, job.message.len);
        @memcpy(job.message[0..len], message[0..len]);
        if (hold) |unit| job.progress.hold.store(unit, .release);
        job.thread = try std.Thread.spawn(.{}, run, .{job});
        return job;
    }

    fn run(self: *Job) void {
        self.result = self.session.perform(self.work, &self.progress);
        self.finished.store(true, .release);
    }

    pub fn done(self: *const Job) bool {
        return self.finished.load(.acquire);
    }

    /// Waits for the thread (at once when `done`) and returns the work's
    /// result.
    pub fn join(self: *Job) anyerror!Outcome {
        self.thread.join();
        return self.result;
    }

    pub fn destroy(self: *Job) void {
        self.arena.deinit();
        self.gpa.destroy(self);
    }

    /// The position and range the modal shows.
    pub fn position(self: *const Job) usize {
        return self.progress.position.load(.acquire);
    }

    pub fn range(self: *const Job) usize {
        return self.progress.range.load(.acquire);
    }
};

/// `work` with every slice copied into `arena`.
fn owned(arena: std.mem.Allocator, work: Work) !Work {
    return switch (work) {
        .export_pak => |pak| .{ .export_pak = .{
            .game_folder = try arena.dupe(u8, pak.game_folder),
            .destination = switch (pak.destination) {
                .mod => |name| .{ .mod = try arena.dupe(u8, name) },
                .game => .game,
            },
            .filter = if (pak.filter) |filter| .{
                .name = try arena.dupe(u8, filter.name),
                .states = filter.states,
                .changed = filter.changed,
                .conditions = try arena.dupe(u8, filter.conditions),
            } else null,
            .element = pak.element,
        } },
        .export_sheet => |path| .{ .export_sheet = try arena.dupe(u8, path) },
        .import_sheet => |path| .{ .import_sheet = try arena.dupe(u8, path) },
        .find => |find| .{ .find = .{
            .options = .{
                .text = try arena.dupe(u8, find.options.text),
                .down = find.options.down,
                .match_case = find.options.match_case,
                .whole_word = find.options.whole_word,
            },
            .from = find.from,
        } },
    };
}

const testing = std.testing;

test "a job runs the work on its own thread, held, released and cancelled" {
    // Data/ELK read only: an export to a scratch game folder.
    const io = testing.io;
    const scratch = "zig-out/local-test/elk/app/jobs";
    std.Io.Dir.cwd().deleteTree(io, scratch) catch {};
    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open("Data/ELK/elk.xml");
    const texts = session.textCount();
    for ([_]bool{ false, true }) |cancel| {
        const begun = try session.begin(.{ .export_pak = .{ .game_folder = scratch, .destination = .{ .mod = "Jobs" } } });
        var job = try Job.start(testing.allocator, &session, begun, "Exporting", 0, 100);
        defer job.destroy();
        while (!job.progress.held.load(.acquire)) try io.sleep(.fromMilliseconds(1), .awake);
        try testing.expectEqual(@as(usize, 100), job.position());
        try testing.expectEqual(texts, job.range());
        try testing.expect(!job.done());
        if (cancel) job.progress.cancel() else job.progress.release();
        const result = job.join();
        if (cancel) {
            try testing.expectError(error.Cancelled, session.finish(job.work, result));
        } else {
            const outcome = try session.finish(job.work, result);
            defer testing.allocator.free(outcome.exported.path);
            try testing.expectEqual(texts, outcome.exported.entries);
            // The cancelled run below must not have touched it.
            std.Io.Dir.cwd().deleteFile(io, outcome.exported.path) catch {};
        }
    }
    try testing.expectError(error.FileNotFound, std.Io.Dir.cwd().access(io, scratch ++ "/mods/Jobs/data/texts.pak", .{}));
}
