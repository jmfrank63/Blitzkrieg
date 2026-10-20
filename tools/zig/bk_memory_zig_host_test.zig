//! Proof that a Zig host allocates through the one BkMemory instance.
//!
//! This is an executable rather than a `test` block on purpose: the exit code
//! of the clean run is decided by BkMemory's detach report, so a Zig-side leak
//! fails the process exactly like a C++ one. With BK_ZIG_PLANT_LEAK=1 one
//! block is leaked from `plantZigLeak` and the report must name it and exit 3.
//! With BK_ZIG_EXIT_CHURN=1 threads keep allocating as a foreign library
//! through the process's exit, as Apple's QuartzCore and CoreAudio threads do
//! on macOS, and the run must still end clean.

const std = @import("std");
const bk = @import("bk_memory_allocator");

extern fn bk_mem_alloc(size: usize) callconv(.c) ?*anyopaque;
extern fn bk_mem_alloc_from(size: usize, alignment: usize, caller: ?*const anyopaque, frame: ?*const anyopaque) callconv(.c) ?*anyopaque;
extern fn bk_mem_realloc(p: ?*anyopaque, size: usize) callconv(.c) ?*anyopaque;
extern fn bk_mem_free(p: ?*anyopaque) callconv(.c) void;
extern fn bk_mem_live_count() callconv(.c) usize;

var failures: usize = 0;

fn check(ok: bool, what: []const u8) void {
    if (ok) return;
    failures += 1;
    std.debug.print("FAIL: {s}\n", .{what});
}

/// Kept as its own noinline function so the leak report shows this frame.
noinline fn plantZigLeak() void {
    const block = bk.allocator.alloc(u8, 4096) catch @panic("OOM");
    // Touch it so the allocation cannot be optimized away.
    @memset(block, 0xA5);
    std.mem.doNotOptimizeAway(block.ptr);
}

/// The exe never registers with BkMemory, so a block attributed to its code
/// is a foreign library's: operator new's caller there is this address.
noinline fn foreignCaller() ?*const anyopaque {
    return @ptrFromInt(@returnAddress());
}

/// Allocates, grows and frees as a foreign library until the process is gone.
/// The exit report runs while this still runs, so it meets blocks mid-flight
/// and allocations after it has closed; neither is a leak of ours.
fn churnForeign() void {
    const caller = foreignCaller();
    while (true) {
        const p = bk_mem_alloc_from(64, 0, caller, null) orelse continue;
        @memset(@as([*]u8, @ptrCast(p))[0..64], 0x5A);
        const q = bk_mem_realloc(p, 4096) orelse p;
        bk_mem_free(q);
    }
}

fn fillAndVerify(buf: []u8, seed: u8) bool {
    for (buf, 0..) |*byte, i| byte.* = seed +% @as(u8, @truncate(i));
    for (buf, 0..) |byte, i| {
        if (byte != seed +% @as(u8, @truncate(i))) return false;
    }
    return true;
}

pub fn main(init: std.process.Init) !void {
    const a = bk.allocator;
    const start = bk_mem_live_count();

    // Sizes 1..64 KiB at alignments 1..64.
    const sizes = [_]usize{ 1, 7, 16, 100, 4096, 65536 };
    inline for ([_]u7{ 1, 2, 4, 8, 16, 32, 64 }) |align_bytes| {
        const alignment = comptime std.mem.Alignment.fromByteUnits(align_bytes);
        for (sizes) |size| {
            const block = try a.alignedAlloc(u8, alignment, size);
            check(@intFromPtr(block.ptr) % align_bytes == 0, "allocation honours its alignment");
            check(fillAndVerify(block, 3), "block holds what was written");
            a.free(block);
        }
    }

    // Grow and shrink through remap, contents must follow.
    var grown = try a.alloc(u8, 32);
    check(fillAndVerify(grown, 9), "pre-realloc fill");
    grown = try a.realloc(grown, 8192);
    for (grown[0..32], 0..) |byte, i| {
        if (byte != 9 +% @as(u8, @truncate(i))) {
            check(false, "realloc grow keeps the contents");
            break;
        }
    }
    grown = try a.realloc(grown, 16);
    for (grown, 0..) |byte, i| {
        if (byte != 9 +% @as(u8, @truncate(i))) {
            check(false, "realloc shrink keeps the contents");
            break;
        }
    }
    a.free(grown);

    // A block crosses the boundary in both directions.
    const zig_block = try a.alloc(u8, 256);
    bk_mem_free(zig_block.ptr);
    const c_ptr: [*]u8 = @ptrCast(bk_mem_alloc(300) orelse return error.OutOfMemory);
    a.free(c_ptr[0..300]);

    check(bk_mem_live_count() == start, "live count returns to its start");

    const plant = init.environ_map.get("BK_ZIG_PLANT_LEAK");
    if (plant != null and std.mem.eql(u8, plant.?, "1")) plantZigLeak();

    const churn = init.environ_map.get("BK_ZIG_EXIT_CHURN");
    if (churn != null and std.mem.eql(u8, churn.?, "1")) {
        for (0..4) |_| {
            const thread = try std.Thread.spawn(.{}, churnForeign, .{});
            thread.detach();
        }
    }

    if (failures != 0) std.process.exit(1);
}
