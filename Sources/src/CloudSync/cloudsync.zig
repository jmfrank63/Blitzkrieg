//! Root module and C ABI for cloud profile sync.
//!
//! Everything C++ ever sees of this module is below: five `callconv(.c)`
//! functions, no zig error union, no slice, no allocator, no pointer into any
//! structure this module owns. That is deliberate. The game's C++ half is
//! built by a different compiler with a different runtime, and a zig type
//! crossing that boundary is an ABI it cannot describe.
//!
//! **Conventions every later export follows** (they exist so the surface stays
//! predictable as packets add to it):
//!
//! - A handle is a small non-negative index into a fixed-size table, never a
//!   pointer. `-1` is the only failure value any function returns; a caller
//!   distinguishes failures by calling `bk_cloudsync_last_error`, never by
//!   reading a second error code.
//! - A returned string is owned by this module and valid until the next call
//!   on the same handle (for the module-wide strings, until the next call at
//!   all). The caller copies it if it wants to keep it.
//! - Anything returning bytes writes them into a caller-supplied buffer and
//!   returns the length, so that ownership never crosses.
//! - No export blocks on a socket. `bk_cloudsync_discovery_status` and
//!   `bk_cloudsync_refresh_discovery` can spawn `rclone version` and so may
//!   take a moment; from P02 onward the sync itself lives on a worker thread
//!   and the main loop only polls.
//!
//! Exports arrive with the packet that implements them, never before — see the
//! amendment rule in the plan's `EXECUTION.md`. There are no stubs here.
//!
//! ## The discovery cache
//!
//! `bk_cloudsync_available` and `bk_cloudsync_discovery_status` answer two
//! questions about one fact, so they read one cached `daemon.Availability`
//! rather than each running their own search: two searches can disagree — a
//! binary deleted between them is enough — and a settings screen that says
//! "unavailable: not found" next to an enabled sync button is worse than
//! either answer alone. Discovery runs on the first call and then only when
//! `bk_cloudsync_refresh_discovery` asks for it again.
//!
//! **Until `P03-M01` exists there is no `rclone_path` to consult**, so the
//! search covers the game directory and `PATH` only. That packet adds the
//! explicit path from `cloud.credentials` as the first source, and owns
//! invalidating this cache when the player changes it — the refresh entry
//! point below is what it calls. The gap is recorded on both sides so neither
//! packet assumes the other closed it.
//!
//! ## Thread safety
//!
//! The refresh above is triggered from the UI thread while the sync worker
//! (P02-M02) may be reading the discovered path in order to spawn a daemon. A
//! refresh that frees the old strings underneath that reader is a
//! use-after-free whose crash lands nowhere near its cause, so:
//!
//! - the cache is guarded by a mutex;
//! - **no caller ever holds a pointer into it.** `available` returns a copied
//!   bool and `discovery_status` serialises into the caller's buffer, both
//!   under the lock. Anything that needs the path itself copies it into its
//!   own allocation before releasing. With no borrows outstanding, a refresh
//!   can free the previous value immediately instead of reference-counting it;
//! - the probe runs **outside** the lock. `daemon.probeVersion` spawns a
//!   subprocess, and holding a mutex across a process launch would stall every
//!   reader for its duration;
//! - and because the probe runs outside the lock, each refresh carries a
//!   generation. Two refreshes can overlap — a credentials save while a retry
//!   is already in flight — and can finish in either order. The mutex makes
//!   that safe; it says nothing about ordering. A refresh publishes only if no
//!   refresh has started since it did, so the one that started last is the one
//!   whose answer survives, whenever it happens to finish. See `refresh`.

const std = @import("std");
const builtin = @import("builtin");
const daemon = @import("daemon.zig");

const Allocator = std.mem.Allocator;

/// Zig 0.16 moved the mutex out of `std.Thread` and into `std.Io`, where
/// locking takes an `Io` because a contended wait is a cancellation point.
/// A C ABI entry point has no `Io` to pass and should not have to build one:
/// the only thing a lock needs here is a futex, and `Io.Threaded`'s futex
/// vtable entries ignore the instance they are given entirely. A statically
/// initialised instance is therefore sufficient, costs nothing at startup, and
/// needs no teardown — unlike a real `Io.Threaded`, which would have to be
/// created before the first lock and so would need a lock of its own.
var lock_io_impl: std.Io.Threaded = .init_single_threaded;

fn lockIo() std.Io {
    return lock_io_impl.io();
}

/// How a refresh obtains an `Availability`. A function pointer only so that
/// the ordering tests can substitute a probe whose duration they control;
/// production always uses `resolveWithOwnIo`.
pub const Resolver = *const fn (gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability;

/// `daemon.resolveIn` wants an `Io` and this module has no long-lived one yet
/// — the daemon supervisor that will own one arrives with P02. Building a
/// short-lived `Io.Threaded` per refresh is what `daemon.resolve` itself does,
/// and a refresh is rare enough that the setup cost is invisible next to the
/// process launch inside it.
fn resolveWithOwnIo(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    return daemon.resolveIn(gpa, threaded.io(), search);
}

pub const StatusError = error{
    /// The caller's buffer cannot hold the document and its terminator.
    NoSpaceLeft,
    /// Discovery has not produced a value, which can only mean it failed.
    NotDiscovered,
};

pub const Discovery = struct {
    gpa: Allocator,
    resolver: Resolver = resolveWithOwnIo,
    search: daemon.Search = .{},

    mutex: std.Io.Mutex = .init,
    /// The ticket counter. Every refresh takes the next value under the lock,
    /// and publishes only if the counter has not moved since — see `refresh`.
    next_gen: u64 = 0,
    /// Null before the first successful discovery, and after `clear`.
    value: ?daemon.Availability = null,

    fn lock(self: *Discovery) void {
        self.mutex.lockUncancelable(lockIo());
    }

    fn unlock(self: *Discovery) void {
        self.mutex.unlock(lockIo());
    }

    /// Run discovery and replace the cache with the result.
    ///
    /// The publish predicate is `my_gen == self.next_gen`: publish only if no
    /// refresh has started since this one did. The weaker `my_gen >
    /// published_gen` is not enough and fails in exactly the case that
    /// matters — with generations 1 and 2 both in flight and nothing published
    /// yet, generation 1 satisfies `1 > 0` and installs its result while 2 is
    /// still probing. Generation 2 is typically the credentials save that just
    /// changed the path, so the cache would then serve the superseded path to
    /// the worker for as long as the newer probe takes.
    ///
    /// A superseded refresh still succeeds from its caller's point of view; it
    /// simply does not publish. And a refresh that fails leaves the cache
    /// alone rather than reverting it to an older probe's answer:
    /// stale-but-known beats confidently wrong.
    pub fn refresh(self: *Discovery) Allocator.Error!void {
        self.lock();
        self.next_gen += 1;
        const my_gen = self.next_gen;
        self.unlock();

        // Outside the lock: this launches a process and waits for it.
        var probed = try self.resolver(self.gpa, self.search);

        self.lock();
        defer self.unlock();
        if (my_gen != self.next_gen) {
            probed.deinit(self.gpa);
            return;
        }
        if (self.value) |*previous| previous.deinit(self.gpa);
        self.value = probed;
    }

    /// Discovery runs once. Concurrent first callers may each start one; the
    /// generation rule then discards all but the newest, which is the same
    /// outcome as an explicit refresh race and needs no separate handling.
    fn ensure(self: *Discovery) Allocator.Error!void {
        self.lock();
        const discovered = self.value != null;
        self.unlock();
        if (discovered) return;
        return self.refresh();
    }

    /// A copy of the verdict, never a borrow of it.
    pub fn available(self: *Discovery) bool {
        self.lock();
        defer self.unlock();
        const current = self.value orelse return false;
        return current == .ready;
    }

    /// Serialise the cached verdict into `out` as `{ found, path, version,
    /// reason }`, under the lock, and return its length excluding the NUL.
    ///
    /// `reason` is the typed `daemon.Reason` rather than free text: the
    /// settings screen has to tell a player *why* the feature is unavailable
    /// and *which* binary was chosen, and neither a boolean nor a generic
    /// error string can answer either question.
    pub fn writeStatus(self: *Discovery, out: []u8) StatusError!usize {
        if (out.len == 0) return error.NoSpaceLeft;
        self.lock();
        defer self.unlock();
        const current = self.value orelse return error.NotDiscovered;

        // One byte held back for the terminator, which is not part of the
        // returned length but lets a C caller treat the buffer as a string.
        var writer: std.Io.Writer = .fixed(out[0 .. out.len - 1]);
        var json: std.json.Stringify = .{ .writer = &writer };
        writeAvailability(&json, current) catch return error.NoSpaceLeft;
        const length = writer.buffered().len;
        out[length] = 0;
        return length;
    }

    /// Drop the cache and make sure no probe already in flight can install
    /// itself afterwards — bumping the counter supersedes every outstanding
    /// ticket at once.
    pub fn clear(self: *Discovery) void {
        self.lock();
        defer self.unlock();
        self.next_gen += 1;
        if (self.value) |*previous| previous.deinit(self.gpa);
        self.value = null;
    }
};

fn writeAvailability(json: *std.json.Stringify, current: daemon.Availability) std.json.Stringify.Error!void {
    const found = current == .ready;
    const chosen: ?[]const u8 = switch (current) {
        .ready => |ready| ready.path,
        .unavailable => |rejected| rejected.path,
    };
    const version: ?daemon.Version = switch (current) {
        .ready => |ready| ready.version,
        .unavailable => |rejected| rejected.version,
    };

    try json.beginObject();
    try json.objectField("found");
    try json.write(found);
    try json.objectField("path");
    if (chosen) |value| try json.write(value) else try json.write(null);
    try json.objectField("version");
    if (version) |value| {
        var buffer: [48]u8 = undefined;
        // A three-part version never approaches 48 bytes.
        const text = std.fmt.bufPrint(&buffer, "{d}.{d}.{d}", .{ value.major, value.minor, value.patch }) catch unreachable;
        try json.write(text);
    } else try json.write(null);
    try json.objectField("reason");
    if (current.reason()) |reason| try json.write(@tagName(reason)) else try json.write(null);
    try json.endObject();
}

// ---------------------------------------------------------------------------
// Module state behind the C ABI
// ---------------------------------------------------------------------------

/// `smp_allocator` rather than a page allocator because every export can be
/// called from any thread, and rather than a debug allocator because the
/// shipping module must not pay for leak tracking. The tests below build their
/// own `Discovery` values over `std.testing.allocator`, so this instance is
/// only ever exercised through the C++ smoke consumer.
var module: Discovery = .{ .gpa = std.heap.smp_allocator };

var error_mutex: std.Io.Mutex = .init;
/// Module-owned, valid until the next call, as the conventions above promise.
/// Fixed storage so the pointer itself never dangles; only the contents change.
var error_text: [512]u8 = @splat(0);

fn setError(comptime message: []const u8) void {
    comptime std.debug.assert(message.len < error_text.len);
    error_mutex.lockUncancelable(lockIo());
    defer error_mutex.unlock(lockIo());
    @memcpy(error_text[0..message.len], message);
    error_text[message.len] = 0;
}

fn clearError() void {
    error_mutex.lockUncancelable(lockIo());
    defer error_mutex.unlock(lockIo());
    error_text[0] = 0;
}

/// 1 when a usable rclone was found, 0 otherwise. Reads the same cached
/// discovery `bk_cloudsync_discovery_status` reports, so the two can never
/// disagree.
pub export fn bk_cloudsync_available() callconv(.c) u32 {
    module.ensure() catch {
        setError("cloud sync: out of memory while searching for rclone");
        return 0;
    };
    clearError();
    return if (module.available()) 1 else 0;
}

/// Write `{ found, path, version, reason }` into `json_out` as NUL-terminated
/// JSON and return its length excluding the NUL, or -1 on failure. On failure
/// the contents of `json_out` are unspecified.
pub export fn bk_cloudsync_discovery_status(json_out: [*]u8, cap: u32) callconv(.c) i32 {
    module.ensure() catch {
        setError("cloud sync: out of memory while searching for rclone");
        return -1;
    };
    const length = module.writeStatus(json_out[0..cap]) catch |err| {
        switch (err) {
            error.NoSpaceLeft => setError("cloud sync: buffer too small for the discovery status document"),
            error.NotDiscovered => setError("cloud sync: rclone discovery has not run"),
        }
        return -1;
    };
    clearError();
    return @intCast(length);
}

/// Re-run discovery and replace the cache. 0 on success, -1 on failure; a
/// refresh that a newer one superseded reports success without publishing, and
/// a refresh that fails leaves the previous value in place.
pub export fn bk_cloudsync_refresh_discovery() callconv(.c) i32 {
    module.refresh() catch {
        setError("cloud sync: out of memory while searching for rclone");
        return -1;
    };
    clearError();
    return 0;
}

/// Release everything this module owns. Idempotent, and safe to call without
/// ever having called anything else. A later call re-runs discovery rather
/// than answering from a cache that was thrown away.
///
/// Packets that add owned state — the daemon child above all — stop it here.
pub export fn bk_cloudsync_shutdown() callconv(.c) void {
    module.clear();
    clearError();
}

/// The most recent failure, or an empty string when the most recent call
/// succeeded. Module-owned; copy it if you need to keep it.
pub export fn bk_cloudsync_last_error() callconv(.c) [*:0]const u8 {
    return @ptrCast(&error_text);
}

// ---------------------------------------------------------------------------
// Tests
//
// Nothing here touches `module` or the real resolver: a test that shelled out
// to whatever rclone the machine happens to have would pass or fail on the
// machine rather than on the code. The C++ smoke consumer covers the real
// path, because proving the exports work from C++ is the one thing these
// cannot do.
//
// Nothing here prints, either. The Zig 0.16 build runner fails a test step
// whose binary writes anything at all to stderr, even with every test passing.
// ---------------------------------------------------------------------------

const testing = std.testing;

/// A probe the test opens and closes by hand. `entered` rises when the probe
/// starts, which is also the proof that its refresh has taken its ticket;
/// `release` lets it finish. No sleeps and no timing assumptions: every
/// ordering below is imposed, not hoped for.
const Gate = struct {
    entered: std.atomic.Value(bool) = .init(false),
    release: std.atomic.Value(bool) = .init(false),
    /// What this probe answers with, duplicated into the caller's allocator.
    path: []const u8 = "",
    /// When set, the probe fails instead of answering.
    fails: bool = false,

    fn open(self: *Gate) void {
        self.release.store(true, .release);
    }

    fn awaitEntry(self: *Gate) void {
        while (!self.entered.load(.acquire)) std.Thread.yield() catch {};
    }
};

var gates: [8]Gate = @splat(.{});
var gate_cursor: std.atomic.Value(u32) = .init(0);
var probe_count: std.atomic.Value(u32) = .init(0);

fn resetGates() void {
    gates = @splat(.{});
    gate_cursor.store(0, .release);
    probe_count.store(0, .release);
}

/// Each call takes the next gate in order, so a test that starts one refresh,
/// waits for it to enter, and only then starts the next knows exactly which
/// gate belongs to which generation.
fn gatedResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = search;
    const index = gate_cursor.fetchAdd(1, .monotonic);
    const gate = &gates[index];
    _ = probe_count.fetchAdd(1, .monotonic);
    gate.entered.store(true, .release);
    while (!gate.release.load(.acquire)) std.Thread.yield() catch {};
    if (gate.fails) return error.OutOfMemory;
    return .{ .ready = .{
        .path = try gpa.dupe(u8, gate.path),
        .version = .{ .major = 1, .minor = 75, .patch = 0 },
    } };
}

/// A gate that is open before anyone reaches it, for the cases that care about
/// the result rather than the ordering.
fn openGate(index: usize, path: []const u8) void {
    gates[index].path = path;
    gates[index].open();
}

fn cachedPath(discovery: *Discovery) ?[]const u8 {
    discovery.lock();
    defer discovery.unlock();
    const current = discovery.value orelse return null;
    return switch (current) {
        .ready => |ready| ready.path,
        .unavailable => |rejected| rejected.path,
    };
}

fn expectCachedPath(discovery: *Discovery, expected: []const u8) !void {
    const actual = cachedPath(discovery) orelse return error.TestExpectedCachedValue;
    try testing.expectEqualStrings(expected, actual);
}

fn refreshThread(discovery: *Discovery, result: *anyerror!void) void {
    result.* = discovery.refresh();
}

fn fixedResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = search;
    _ = probe_count.fetchAdd(1, .monotonic);
    return .{ .ready = .{
        .path = try gpa.dupe(u8, "/fixed/rclone"),
        .version = .{ .major = 1, .minor = 75, .patch = 0 },
    } };
}

fn rejectingResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = search;
    return .{ .unavailable = .{
        .reason = .too_old,
        .path = try gpa.dupe(u8, "/old/rclone"),
        .version = .{ .major = 1, .minor = 60, .patch = 2 },
    } };
}

fn missingResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = gpa;
    _ = search;
    return .{ .unavailable = .{ .reason = .not_found, .path = null, .version = null } };
}

fn unreadableResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = search;
    return .{ .unavailable = .{
        .reason = .not_executable,
        .path = try gpa.dupe(u8, "/bin/rclone"),
        .version = null,
    } };
}

fn statusOf(discovery: *Discovery, buffer: []u8) ![]const u8 {
    const length = try discovery.writeStatus(buffer);
    try testing.expectEqual(@as(u8, 0), buffer[length]);
    return buffer[0..length];
}

test "a ready discovery serialises every field the settings screen needs" {
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = fixedResolve };
    defer discovery.clear();
    try testing.expect(discovery.available() == false); // nothing cached yet
    try discovery.ensure();

    var buffer: [512]u8 = undefined;
    try testing.expectEqualStrings(
        \\{"found":true,"path":"/fixed/rclone","version":"1.75.0","reason":null}
    , try statusOf(&discovery, &buffer));
    try testing.expect(discovery.available());
}

test "each rejection carries its typed reason rather than free text" {
    var buffer: [512]u8 = undefined;

    var too_old: Discovery = .{ .gpa = testing.allocator, .resolver = rejectingResolve };
    defer too_old.clear();
    try too_old.ensure();
    try testing.expectEqualStrings(
        \\{"found":false,"path":"/old/rclone","version":"1.60.2","reason":"too_old"}
    , try statusOf(&too_old, &buffer));
    try testing.expect(!too_old.available());

    var missing: Discovery = .{ .gpa = testing.allocator, .resolver = missingResolve };
    defer missing.clear();
    try missing.ensure();
    try testing.expectEqualStrings(
        \\{"found":false,"path":null,"version":null,"reason":"not_found"}
    , try statusOf(&missing, &buffer));

    var unreadable: Discovery = .{ .gpa = testing.allocator, .resolver = unreadableResolve };
    defer unreadable.clear();
    try unreadable.ensure();
    try testing.expectEqualStrings(
        \\{"found":false,"path":"/bin/rclone","version":null,"reason":"not_executable"}
    , try statusOf(&unreadable, &buffer));
}

test "a buffer that cannot hold the document fails instead of truncating" {
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = fixedResolve };
    defer discovery.clear();
    try discovery.ensure();

    var tiny: [16]u8 = undefined;
    try testing.expectError(error.NoSpaceLeft, discovery.writeStatus(&tiny));
    var empty: [0]u8 = undefined;
    try testing.expectError(error.NoSpaceLeft, discovery.writeStatus(&empty));

    // The document plus its terminator is the exact requirement, not one byte
    // more: a buffer of that size must succeed.
    var measured: [512]u8 = undefined;
    const length = try discovery.writeStatus(&measured);
    const exact = try testing.allocator.alloc(u8, length + 1);
    defer testing.allocator.free(exact);
    try testing.expectEqual(length, try discovery.writeStatus(exact));
    const one_short = try testing.allocator.alloc(u8, length);
    defer testing.allocator.free(one_short);
    try testing.expectError(error.NoSpaceLeft, discovery.writeStatus(one_short));
}

test "discovery runs once and both readers see the same result" {
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = fixedResolve };
    defer discovery.clear();
    probe_count.store(0, .release);

    var buffer: [512]u8 = undefined;
    try testing.expect(bkAvailableOf(&discovery));
    _ = try statusOf(&discovery, &buffer);
    try testing.expect(bkAvailableOf(&discovery));
    try testing.expectEqual(@as(u32, 1), probe_count.load(.acquire));

    try discovery.refresh();
    try testing.expectEqual(@as(u32, 2), probe_count.load(.acquire));
}

/// `available` with the lazy discovery the export performs, so the test above
/// measures the same thing `bk_cloudsync_available` does.
fn bkAvailableOf(discovery: *Discovery) bool {
    discovery.ensure() catch return false;
    return discovery.available();
}

test "shutdown drops the cache and the next call rediscovers" {
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = fixedResolve };
    defer discovery.clear();
    try discovery.ensure();
    try expectCachedPath(&discovery, "/fixed/rclone");

    discovery.clear();
    try testing.expect(cachedPath(&discovery) == null);
    var buffer: [512]u8 = undefined;
    try testing.expectError(error.NotDiscovered, discovery.writeStatus(&buffer));
    try testing.expect(!discovery.available());

    try testing.expect(bkAvailableOf(&discovery));
    try expectCachedPath(&discovery, "/fixed/rclone");
}

test "a failing refresh keeps the previous value rather than clearing it" {
    resetGates();
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = gatedResolve };
    defer discovery.clear();

    openGate(0, "/first/rclone");
    try discovery.ensure();
    try expectCachedPath(&discovery, "/first/rclone");

    gates[1].fails = true;
    gates[1].open();
    try testing.expectError(error.OutOfMemory, discovery.refresh());
    try expectCachedPath(&discovery, "/first/rclone");
}

test "an older refresh that finishes last never reaches the cache" {
    resetGates();
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = gatedResolve };
    defer discovery.clear();

    openGate(0, "/base/rclone");
    try discovery.ensure();
    try expectCachedPath(&discovery, "/base/rclone");

    // Generation 2 starts first and is still probing.
    gates[1].path = "/older/rclone";
    var older_result: anyerror!void = {};
    const older = try std.Thread.spawn(.{}, refreshThread, .{ &discovery, &older_result });
    gates[1].awaitEntry();

    // Generation 3 starts second, finishes first, and publishes.
    gates[2].path = "/newer/rclone";
    var newer_result: anyerror!void = {};
    const newer = try std.Thread.spawn(.{}, refreshThread, .{ &discovery, &newer_result });
    gates[2].awaitEntry();
    gates[2].open();
    newer.join();
    try newer_result;

    // Inspected while the older probe is still in flight — it is blocked on
    // its gate and cannot have finished — the cache must already hold the
    // newer answer and must never hold the older one.
    try testing.expect(!gates[1].release.load(.acquire));
    try expectCachedPath(&discovery, "/newer/rclone");

    gates[1].open();
    older.join();
    // Superseded, so it reports success to its own caller without publishing.
    try older_result;
    try expectCachedPath(&discovery, "/newer/rclone");
}

test "an older refresh that finishes first is discarded, not published" {
    // This is the case `my_gen > published_gen` gets wrong: with generations 2
    // and 3 both in flight and nothing published since, generation 2 would
    // satisfy that test and install its result while 3 is still probing.
    resetGates();
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = gatedResolve };
    defer discovery.clear();

    openGate(0, "/base/rclone");
    try discovery.ensure();

    gates[1].path = "/older/rclone";
    var older_result: anyerror!void = {};
    const older = try std.Thread.spawn(.{}, refreshThread, .{ &discovery, &older_result });
    gates[1].awaitEntry();

    gates[2].path = "/newer/rclone";
    var newer_result: anyerror!void = {};
    const newer = try std.Thread.spawn(.{}, refreshThread, .{ &discovery, &newer_result });
    gates[2].awaitEntry();

    gates[1].open();
    older.join();
    try older_result;

    // The newer probe is demonstrably still running — its gate is shut — and
    // the older one has completed. The cache must still hold the baseline: an
    // answer that has been superseded is discarded, not written.
    try testing.expect(!gates[2].release.load(.acquire));
    try expectCachedPath(&discovery, "/base/rclone");

    gates[2].open();
    newer.join();
    try newer_result;
    try expectCachedPath(&discovery, "/newer/rclone");
}

const StressState = struct {
    discovery: *Discovery,
    stop: std.atomic.Value(bool) = .init(false),
    failures: std.atomic.Value(u32) = .init(0),
};

fn stressReader(state: *StressState) void {
    var buffer: [512]u8 = undefined;
    while (!state.stop.load(.acquire)) {
        if (!state.discovery.available()) {
            _ = state.failures.fetchAdd(1, .monotonic);
            continue;
        }
        const length = state.discovery.writeStatus(&buffer) catch {
            _ = state.failures.fetchAdd(1, .monotonic);
            continue;
        };
        // The bytes were copied out under the lock, so they stay readable no
        // matter how many times the cache has been replaced since. Every
        // refresh here answers identically, so anything but the expected
        // document means a read saw memory a refresh had already freed.
        const document = buffer[0..length];
        if (std.mem.indexOf(u8, document, "\"path\":\"/fixed/rclone\"") == null) {
            _ = state.failures.fetchAdd(1, .monotonic);
        }
    }
}

fn stressRefresher(state: *StressState) void {
    var remaining: u32 = 200;
    while (remaining > 0) : (remaining -= 1) {
        state.discovery.refresh() catch {
            _ = state.failures.fetchAdd(1, .monotonic);
            return;
        };
    }
    state.stop.store(true, .release);
}

test "concurrent readers survive a refresher freeing the value underneath them" {
    if (builtin.single_threaded) return;

    // The debug allocator turns a freed-while-read into a failure rather than
    // into a flake: it poisons freed memory and reports a leak or a double
    // free outright, where a general-purpose allocator would usually hand the
    // same bytes back and let the bug pass.
    var debug: std.heap.DebugAllocator(.{ .safety = true, .thread_safe = true }) = .init;
    defer testing.expect(debug.deinit() == .ok) catch @panic("cloudsync discovery cache leaked");

    var discovery: Discovery = .{ .gpa = debug.allocator(), .resolver = fixedResolve };
    defer discovery.clear();
    try discovery.ensure();

    var state: StressState = .{ .discovery = &discovery };
    var readers: [4]std.Thread = undefined;
    for (&readers) |*reader| reader.* = try std.Thread.spawn(.{}, stressReader, .{&state});
    const refresher = try std.Thread.spawn(.{}, stressRefresher, .{&state});

    refresher.join();
    state.stop.store(true, .release);
    for (readers) |reader| reader.join();
    try testing.expectEqual(@as(u32, 0), state.failures.load(.acquire));
}
