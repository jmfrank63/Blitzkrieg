//! BkMemory micro-benchmark: one pass of mixed allocations through the C ABI of
//! whichever BkMemory build this exe is linked to, printing one table row.
//!
//! One pass per process, so the peak private bytes of a row belong to that pass
//! alone. `zig build bk-memory-bench` runs every backend and pass in turn.
//!
//! Usage: bk_memory_bench <label> <small|large> <threads> <total-allocs>

const std = @import("std");
const builtin = @import("builtin");

extern fn bk_mem_alloc(size: usize) ?*anyopaque;
extern fn bk_mem_free(p: ?*anyopaque) void;
extern fn bk_mem_live_count() usize;

const kernel32 = struct {
    const Counters = extern struct {
        cb: u32,
        page_fault_count: u32,
        peak_working_set: usize,
        working_set: usize,
        quota_peak_paged_pool: usize,
        quota_paged_pool: usize,
        quota_peak_non_paged_pool: usize,
        quota_non_paged_pool: usize,
        pagefile_usage: usize,
        peak_pagefile_usage: usize,
        private_usage: usize,
    };
    extern "kernel32" fn QueryPerformanceCounter(count: *i64) callconv(.winapi) i32;
    extern "kernel32" fn QueryPerformanceFrequency(freq: *i64) callconv(.winapi) i32;
    extern "kernel32" fn GetCurrentProcess() callconv(.winapi) *anyopaque;
    extern "kernel32" fn K32GetProcessMemoryInfo(process: *anyopaque, counters: *Counters, cb: u32) callconv(.winapi) i32;
};

fn nowNs() u64 {
    if (builtin.os.tag == .windows) {
        var freq: i64 = 0;
        var count: i64 = 0;
        _ = kernel32.QueryPerformanceFrequency(&freq);
        _ = kernel32.QueryPerformanceCounter(&count);
        const c: u128 = @intCast(count);
        return @intCast(c * std.time.ns_per_s / @as(u128, @intCast(freq)));
    }
    var ts: std.c.timespec = undefined;
    _ = std.c.clock_gettime(.MONOTONIC, &ts);
    return @as(u64, @intCast(ts.sec)) * std.time.ns_per_s + @as(u64, @intCast(ts.nsec));
}

/// Peak private commit of the process: PeakPagefileUsage on Windows, the peak
/// resident set elsewhere (the only peak the platform offers cheaply).
fn peakPrivateBytes() usize {
    if (builtin.os.tag == .windows) {
        var c: kernel32.Counters = undefined;
        c.cb = @sizeOf(kernel32.Counters);
        _ = kernel32.K32GetProcessMemoryInfo(kernel32.GetCurrentProcess(), &c, c.cb);
        return c.peak_pagefile_usage;
    }
    var ru: std.c.rusage = undefined;
    _ = std.c.getrusage(0, &ru);
    const kib: usize = @intCast(ru.maxrss);
    return if (builtin.os.tag == .macos) kib else kib * 1024;
}

const Pass = enum { small, large };

const Job = struct {
    pass: Pass,
    allocs: usize,
    seed: u64,
    checksum: u8 = 0,
};

/// A sliding window of live blocks: every step frees the oldest slot's block
/// and allocates a new one in its place, so the live set stays constant and the
/// allocator sees both a steady state and the churn of a game frame.
fn run(job: *Job) void {
    const window: usize = switch (job.pass) {
        .small => 4096,
        .large => 64,
    };
    const min_size: usize = if (job.pass == .small) 16 else 8 * 1024;
    const max_size: usize = if (job.pass == .small) 512 else 64 * 1024;
    var slots: [4096]?*anyopaque = @splat(null);
    var prng = std.Random.DefaultPrng.init(job.seed);
    const random = prng.random();
    var sum: u8 = 0;
    for (0..job.allocs) |i| {
        const slot = &slots[i % window];
        if (slot.*) |old| bk_mem_free(old);
        const size = min_size + random.uintAtMost(usize, max_size - min_size);
        const p: [*]u8 = @ptrCast(bk_mem_alloc(size) orelse std.process.exit(2));
        // Touch the first and last byte so pages are really committed.
        p[0] = @truncate(i);
        p[size - 1] = @truncate(size);
        sum +%= p[0] ^ p[size - 1];
        slot.* = p;
    }
    for (slots[0..window]) |*slot| {
        if (slot.*) |old| bk_mem_free(old);
        slot.* = null;
    }
    job.checksum = sum;
}

pub fn main(init: std.process.Init) !void {
    var arena_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    _ = args.skip();
    const label = args.next() orelse usage();
    const pass = std.meta.stringToEnum(Pass, args.next() orelse usage()) orelse usage();
    const threads = std.fmt.parseInt(usize, args.next() orelse usage(), 10) catch usage();
    const total = std.fmt.parseInt(usize, args.next() orelse usage(), 10) catch usage();
    if (threads == 0 or threads > 64) usage();

    const jobs = try arena.alloc(Job, threads);
    for (jobs, 0..) |*job, t| job.* = .{ .pass = pass, .allocs = total / threads, .seed = 0x9e3779b97f4a7c15 +% t };
    const handles = try arena.alloc(std.Thread, threads);

    const start = nowNs();
    if (threads == 1) {
        run(&jobs[0]);
    } else {
        // The default stack: on Linux glibc carves static TLS out of it.
        for (handles, jobs) |*h, *job| h.* = try std.Thread.spawn(.{}, run, .{job});
        for (handles) |h| h.join();
    }
    const elapsed = nowNs() - start;

    var done: usize = 0;
    var check: u8 = 0;
    for (jobs) |job| {
        done += job.allocs;
        check ^= job.checksum;
    }
    // Each step is one free and one alloc once the window is full; the
    // per-op figure counts both.
    const ops = done * 2;
    const ns_per_op = @as(f64, @floatFromInt(elapsed)) / @as(f64, @floatFromInt(ops));
    const peak_mib = @as(f64, @floatFromInt(peakPrivateBytes())) / (1024.0 * 1024.0);
    std.debug.print("{s: <16} {s: <6} {d: >2} thr {d: >9} allocs {d: >8.1} ns/op {d: >8.1} MiB peak (live {d}, sum {d})\n", .{
        label, @tagName(pass), threads, done, ns_per_op, peak_mib, bk_mem_live_count(), check,
    });
}

fn usage() noreturn {
    std.debug.print("usage: bk_memory_bench <label> <small|large> <threads> <total-allocs>\n", .{});
    std.process.exit(1);
}
