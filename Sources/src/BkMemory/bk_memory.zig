//! BkMemory: the one allocator instance of the whole process.
//!
//! Every module (the game, its DLLs, the editors) reaches this library through
//! the C ABI below, so a block allocated in one module can be freed in another
//! and there is exactly one set of leak records to report. The instance is a
//! comptime-initialised global, which means it exists before any static
//! constructor of any module runs.
//!
//! Every block carries a 16-byte header in front of the user pointer. The
//! header makes `free`/`realloc` independent of the caller knowing size or
//! alignment, and its magic turns a foreign pointer into a loud panic instead
//! of heap corruption.

const std = @import("std");
const builtin = @import("builtin");
const build_options = @import("bk_memory_options");

const Alignment = std.mem.Alignment;

/// `safe` puts SafeAllocator (leak records, canaries) over the smp allocator,
/// `safe_page` and `safe_c` put it over the page and the C allocator for the
/// backend measurement. `smp` and `crt` are the release/baseline backends
/// without leak records. Same order as the build option's enum.
pub const Mode = enum { safe, smp, crt, safe_page, safe_c };
pub const mode: Mode = @enumFromInt(build_options.allocator_mode);

const use_safe = mode == .safe or mode == .safe_page or mode == .safe_c;

/// A canary that differs from the default one of start.zig's per-exe
/// SafeAllocator, so a block that crosses between the two panics.
const canary: u32 = 0x426b4d31;

const backing: std.mem.Allocator = switch (mode) {
    .safe, .smp => std.heap.smp_allocator,
    .safe_page => std.heap.page_allocator,
    .safe_c, .crt => std.heap.c_allocator,
};

var instance: if (use_safe) std.heap.SafeAllocator else void =
    if (use_safe) std.heap.SafeAllocator.init(backing, .{ .canary = canary }) else {};

const Header = extern struct {
    size: u64,
    align_log2: u8,
    _reserved: [3]u8,
    magic: u32,
};

comptime {
    std.debug.assert(@sizeOf(Header) == 16);
}

const header_size = @sizeOf(Header);
const live_magic: u32 = 0x4d4d4b42;
const freed_magic: u32 = 0x46464b42;
const min_align: usize = 16;

var live_count: usize = 0;
var live_bytes: usize = 0;
/// Set once `bk_mem_report` has run: the backing memory is gone, so a free
/// after that point is counted and ignored and an alloc is a bug.
var closed: bool = false;
var closed_free_count: usize = 0;
var report_leaks: usize = 0;

pub const std_options: std.Options = .{ .logFn = logToStderr };

/// Plain text on stderr, no colour escapes, so a test can match on it.
fn logToStderr(
    comptime level: std.log.Level,
    comptime scope: @EnumLiteral(),
    comptime format: []const u8,
    args: anytype,
) void {
    const io = std.Options.debug_io;
    const prev = io.swapCancelProtection(.blocked);
    defer _ = io.swapCancelProtection(prev);
    var buffer: [64]u8 = undefined;
    const t = std.debug.lockStderr(&buffer).terminal();
    defer std.debug.unlockStderr();
    t.writer.print("{s}({t}): " ++ format ++ "\n", .{ level.asText(), scope } ++ args) catch {};
}

fn backend() std.mem.Allocator {
    return if (use_safe) instance.allocator() else backing;
}

/// Offset of the user pointer from the backing block, for an alignment.
fn padFor(alignment: usize) usize {
    return @max(header_size, alignment);
}

fn normalizeAlign(alignment: usize) ?Alignment {
    const a = @max(alignment, min_align);
    if (!std.math.isPowerOfTwo(a)) return null;
    return Alignment.fromByteUnits(a);
}

fn headerOf(user: [*]u8) *Header {
    return @ptrCast(@alignCast(user - header_size));
}

fn checkHeader(user: [*]u8, what: []const u8) *Header {
    const h = headerOf(user);
    if (h.magic != live_magic) {
        if (h.magic == freed_magic) std.debug.panic("{s}: double free of {*}", .{ what, user });
        std.debug.panic("{s}: {*} was not allocated by BkMemory (bad header magic)", .{ what, user });
    }
    return h;
}

fn allocImpl(size: usize, alignment: usize, ra: usize) ?[*]u8 {
    if (@atomicLoad(bool, &closed, .acquire)) std.debug.panic("bk_mem_alloc after bk_mem_report", .{});
    const a = normalizeAlign(alignment) orelse return null;
    const pad = padFor(a.toByteUnits());
    const total = std.math.add(usize, pad, size) catch return null;
    const base = backend().rawAlloc(total, a, ra) orelse return null;
    const user = base + pad;
    headerOf(user).* = .{
        .size = size,
        .align_log2 = @intCast(@intFromEnum(a)),
        ._reserved = .{ 0, 0, 0 },
        .magic = live_magic,
    };
    _ = @atomicRmw(usize, &live_count, .Add, 1, .monotonic);
    _ = @atomicRmw(usize, &live_bytes, .Add, size, .monotonic);
    return user;
}

fn freeImpl(user: [*]u8, ra: usize) void {
    if (@atomicLoad(bool, &closed, .acquire)) {
        _ = @atomicRmw(usize, &closed_free_count, .Add, 1, .monotonic);
        return;
    }
    const h = checkHeader(user, "bk_mem_free");
    const size: usize = @intCast(h.size);
    const alignment: Alignment = @enumFromInt(h.align_log2);
    const pad = padFor(alignment.toByteUnits());
    const base = user - pad;
    h.magic = freed_magic;
    _ = @atomicRmw(usize, &live_count, .Sub, 1, .monotonic);
    _ = @atomicRmw(usize, &live_bytes, .Sub, size, .monotonic);
    backend().rawFree(base[0 .. pad + size], alignment, ra);
}

fn reallocImpl(user: ?[*]u8, new_size: usize, want_align: usize, ra: usize) ?[*]u8 {
    const p = user orelse return allocImpl(new_size, want_align, ra);
    if (new_size == 0) {
        freeImpl(p, ra);
        return null;
    }
    if (@atomicLoad(bool, &closed, .acquire)) std.debug.panic("bk_mem_realloc after bk_mem_report", .{});
    const h = checkHeader(p, "bk_mem_realloc");
    const old_size: usize = @intCast(h.size);
    const old_align: Alignment = @enumFromInt(h.align_log2);
    // A realloc never lowers the alignment; a stricter request forces a move.
    const wanted = normalizeAlign(want_align) orelse return null;
    if (wanted.compare(.gt, old_align)) return moveBlock(p, old_size, new_size, wanted.toByteUnits(), ra);

    const pad = padFor(old_align.toByteUnits());
    const base = p - pad;
    const total = std.math.add(usize, pad, new_size) catch return null;
    const mem = base[0 .. pad + old_size];
    if (backend().rawRemap(mem, old_align, total, ra)) |new_base| {
        const new_user = new_base + pad;
        headerOf(new_user).size = new_size;
        if (new_size >= old_size) {
            _ = @atomicRmw(usize, &live_bytes, .Add, new_size - old_size, .monotonic);
        } else {
            _ = @atomicRmw(usize, &live_bytes, .Sub, old_size - new_size, .monotonic);
        }
        return new_user;
    }
    return moveBlock(p, old_size, new_size, old_align.toByteUnits(), ra);
}

fn moveBlock(old: [*]u8, old_size: usize, new_size: usize, alignment: usize, ra: usize) ?[*]u8 {
    const fresh = allocImpl(new_size, alignment, ra) orelse return null;
    @memcpy(fresh[0..@min(old_size, new_size)], old[0..@min(old_size, new_size)]);
    freeImpl(old, ra);
    return fresh;
}

export fn bk_mem_alloc(size: usize) callconv(.c) ?*anyopaque {
    return allocImpl(size, min_align, @returnAddress());
}

export fn bk_mem_alloc_aligned(size: usize, alignment: usize) callconv(.c) ?*anyopaque {
    return allocImpl(size, alignment, @returnAddress());
}

export fn bk_mem_calloc(n: usize, size: usize) callconv(.c) ?*anyopaque {
    const total = std.math.mul(usize, n, size) catch return null;
    const p = allocImpl(total, min_align, @returnAddress()) orelse return null;
    @memset(p[0..total], 0);
    return p;
}

export fn bk_mem_realloc(p: ?*anyopaque, size: usize) callconv(.c) ?*anyopaque {
    return reallocImpl(@ptrCast(p), size, min_align, @returnAddress());
}

export fn bk_mem_realloc_aligned(p: ?*anyopaque, size: usize, alignment: usize) callconv(.c) ?*anyopaque {
    return reallocImpl(@ptrCast(p), size, alignment, @returnAddress());
}

export fn bk_mem_free(p: ?*anyopaque) callconv(.c) void {
    if (p) |ptr| freeImpl(@ptrCast(ptr), @returnAddress());
}

/// `size`/`alignment` come from a C++ sized or aligned delete; the header is
/// authoritative, they are only cross-checked in Debug. An alignment of 0
/// means "not given".
export fn bk_mem_free_sized(p: ?*anyopaque, size: usize, alignment: usize) callconv(.c) void {
    const ptr: [*]u8 = @ptrCast(p orelse return);
    if (builtin.mode == .debug and !@atomicLoad(bool, &closed, .acquire)) {
        const h = checkHeader(ptr, "bk_mem_free_sized");
        if (size != 0 and h.size != size) {
            std.debug.panic("bk_mem_free_sized: {*} has size {d}, delete passed {d}", .{ ptr, h.size, size });
        }
        if (alignment != 0) {
            const stored = @as(usize, 1) << @intCast(h.align_log2);
            if (stored != @max(alignment, min_align)) {
                std.debug.panic("bk_mem_free_sized: {*} has alignment {d}, delete passed {d}", .{ ptr, stored, alignment });
            }
        }
    }
    freeImpl(ptr, @returnAddress());
}

export fn bk_mem_size(p: ?*anyopaque) callconv(.c) usize {
    const ptr: [*]u8 = @ptrCast(p orelse return 0);
    return @intCast(checkHeader(ptr, "bk_mem_size").size);
}

export fn bk_mem_live_count() callconv(.c) usize {
    return @atomicLoad(usize, &live_count, .monotonic);
}

export fn bk_mem_live_bytes() callconv(.c) usize {
    return @atomicLoad(usize, &live_bytes, .monotonic);
}

/// Runs the final leak check now and closes the allocator. Returns the leak
/// count (the live-block count when the backend keeps no leak records). Later
/// calls return the same count.
export fn bk_mem_report() callconv(.c) usize {
    if (@cmpxchgStrong(bool, &closed, false, true, .acq_rel, .acquire) != null) {
        return @atomicLoad(usize, &report_leaks, .acquire);
    }
    const leaks: usize = if (use_safe) instance.deinit() else @atomicLoad(usize, &live_count, .monotonic);
    @atomicStore(usize, &report_leaks, leaks, .release);
    return leaks;
}

/// Frees that arrived after `bk_mem_report` and were ignored.
export fn bk_mem_closed_free_count() callconv(.c) usize {
    return @atomicLoad(usize, &closed_free_count, .monotonic);
}

const kernel32 = struct {
    extern "kernel32" fn GetEnvironmentVariableA(name: [*:0]const u8, buffer: [*]u8, size: u32) callconv(.winapi) u32;
    extern "kernel32" fn GetCurrentProcess() callconv(.winapi) *anyopaque;
    extern "kernel32" fn TerminateProcess(process: *anyopaque, code: u32) callconv(.winapi) i32;
};

const exit_leaks: u32 = 3;
const exit_in_use: u32 = 4;

/// What the detach report does. `fail` is the strict form (leaks exit 3),
/// `log` reports the same way but never fails on leaks, so a tier stays green
/// while the leaks it names are still being fixed.
const ReportPolicy = enum { off, fail, log };

/// BK_MEM_REPORT=0|1|log overrides the build default for diagnosis. Any value
/// needs a backend with leak records; without one the policy is always `off`.
fn reportPolicy() ReportPolicy {
    if (!use_safe) return .off;
    const default: ReportPolicy = if (builtin.mode == .debug) .fail else .off;
    var buf: [8]u8 = undefined;
    var value: []const u8 = undefined;
    if (builtin.os.tag == .windows) {
        const n = kernel32.GetEnvironmentVariableA("BK_MEM_REPORT", &buf, buf.len);
        // 0 is unset, and a result above the buffer is a value too long to be one of ours.
        if (n == 0 or n >= buf.len) return default;
        value = buf[0..n];
    } else {
        const raw = std.c.getenv("BK_MEM_REPORT") orelse return default;
        value = std.mem.span(raw);
    }
    if (std.mem.eql(u8, value, "1")) return .fail;
    if (std.mem.eql(u8, value, "log")) return .log;
    if (std.mem.eql(u8, value, "0")) return .off;
    return default;
}

fn exitNow(code: u32) noreturn {
    if (builtin.os.tag == .windows) {
        _ = kernel32.TerminateProcess(kernel32.GetCurrentProcess(), code);
        unreachable;
    }
    std.c._exit(@intCast(code));
}

/// The final leak check at library detach, so a program that never calls
/// `bk_mem_report` still gets its report. A held shard mutex means a thread
/// was killed mid-allocation (ExitProcess), and the records are not trusted.
fn detachHook() void {
    if (@atomicLoad(bool, &closed, .acquire)) return;
    const policy = reportPolicy();
    if (policy == .off) return;
    const log = std.log.scoped(.BkMemory);
    if (use_safe) {
        for (&instance.threads) |*t| {
            if (t.mutex != .unlocked) {
                log.err("allocator in use at exit", .{});
                exitNow(exit_in_use);
            }
        }
    }
    const leaks = bk_mem_report();
    if (policy == .log) {
        // The full report with stacks came from bk_mem_report; this is the
        // one line a script can count, printed for a clean run too.
        log.warn("bk_mem: {d} leaked block(s)", .{leaks});
        return;
    }
    if (leaks > 0) {
        log.err("{d} leaked block(s) at exit", .{leaks});
        exitNow(exit_leaks);
    }
}

/// The DLL entry for the safe/smp builds, which link no libc: Zig's own
/// `_DllMainCRTStartup` calls this. A libc (crt mode) build keeps the CRT's
/// default entry and no report is wanted there.
pub fn DllMain(
    hinst: std.os.windows.HINSTANCE,
    reason: std.os.windows.DWORD,
    reserved: std.os.windows.LPVOID,
) callconv(.winapi) std.os.windows.BOOL {
    _ = hinst;
    _ = reserved;
    const dll_process_detach = 0;
    if (reason == dll_process_detach) detachHook();
    return .TRUE;
}

fn finiHook() callconv(.c) void {
    detachHook();
}

const fini_entry: *const fn () callconv(.c) void = &finiHook;

comptime {
    if (builtin.output_mode == .Lib and builtin.os.tag == .linux) {
        @export(&fini_entry, .{ .name = "bk_mem_fini_entry", .section = ".fini_array" });
    }
}

// Tests run in the test binary, where the exports above are plain functions
// over the same global instance. None of them closes the global instance; the
// leak-report test uses a local SafeAllocator, so no test depends on order.

const testing = std.testing;

test "header round-trip and size" {
    const p = bk_mem_alloc(100).?;
    defer bk_mem_free(p);
    try testing.expectEqual(@as(usize, 100), bk_mem_size(p));
    const bytes: [*]u8 = @ptrCast(p);
    @memset(bytes[0..100], 0xAB);
    try testing.expectEqual(@as(usize, 0), @intFromPtr(p) % 16);
    try testing.expectEqual(live_magic, headerOf(bytes).magic);
}

test "alignments 1 to 4096" {
    for ([_]usize{ 1, 2, 4, 8, 16, 32, 64, 4096 }) |a| {
        const p = bk_mem_alloc_aligned(33, a).?;
        try testing.expectEqual(@as(usize, 0), @intFromPtr(p) % a);
        const bytes: [*]u8 = @ptrCast(p);
        @memset(bytes[0..33], 0x5A);
        try testing.expectEqual(@as(usize, 33), bk_mem_size(p));
        bk_mem_free_sized(p, 33, a);
    }
}

test "non power of two alignment fails" {
    try testing.expect(bk_mem_alloc_aligned(8, 24) == null);
}

test "realloc grow shrink null and zero" {
    const first = bk_mem_realloc(null, 16).?;
    var bytes: [*]u8 = @ptrCast(first);
    for (0..16) |i| bytes[i] = @intCast(i);

    const grown = bk_mem_realloc(first, 20000).?;
    bytes = @ptrCast(grown);
    for (0..16) |i| try testing.expectEqual(@as(u8, @intCast(i)), bytes[i]);
    try testing.expectEqual(@as(usize, 20000), bk_mem_size(grown));

    const shrunk = bk_mem_realloc(grown, 8).?;
    bytes = @ptrCast(shrunk);
    for (0..8) |i| try testing.expectEqual(@as(u8, @intCast(i)), bytes[i]);
    try testing.expectEqual(@as(usize, 8), bk_mem_size(shrunk));

    try testing.expect(bk_mem_realloc(shrunk, 0) == null);
}

test "realloc aligned keeps content and honours a stricter alignment" {
    const first = bk_mem_alloc(32).?;
    var bytes: [*]u8 = @ptrCast(first);
    for (0..32) |i| bytes[i] = @intCast(i + 1);
    const moved = bk_mem_realloc_aligned(first, 64, 256).?;
    try testing.expectEqual(@as(usize, 0), @intFromPtr(moved) % 256);
    bytes = @ptrCast(moved);
    for (0..32) |i| try testing.expectEqual(@as(u8, @intCast(i + 1)), bytes[i]);
    bk_mem_free(moved);
}

test "calloc zeroes and overflow returns null" {
    const p = bk_mem_calloc(10, 7).?;
    defer bk_mem_free(p);
    const bytes: [*]u8 = @ptrCast(p);
    for (bytes[0..70]) |b| try testing.expectEqual(@as(u8, 0), b);
    try testing.expect(bk_mem_calloc(std.math.maxInt(usize) / 2 + 1, 2) == null);
    try testing.expect(bk_mem_alloc(std.math.maxInt(usize) - 8) == null);
}

test "live count and bytes track deltas" {
    const c0 = bk_mem_live_count();
    const b0 = bk_mem_live_bytes();
    const a = bk_mem_alloc(40).?;
    const b = bk_mem_alloc(60).?;
    try testing.expectEqual(c0 + 2, bk_mem_live_count());
    try testing.expectEqual(b0 + 100, bk_mem_live_bytes());
    const grown = bk_mem_realloc(a, 140).?;
    try testing.expectEqual(b0 + 200, bk_mem_live_bytes());
    bk_mem_free(grown);
    bk_mem_free(b);
    bk_mem_free(null);
    try testing.expectEqual(c0, bk_mem_live_count());
    try testing.expectEqual(b0, bk_mem_live_bytes());
}

test "leak count through deinitLog on a local instance" {
    var local: std.heap.SafeAllocator = .init(std.heap.smp_allocator, .{ .canary = 0x4c4f4341 });
    const a = local.allocator();
    const kept = try a.alloc(u8, 24);
    const freed = try a.alloc(u8, 24);
    a.free(freed);
    _ = kept;
    try testing.expectEqual(@as(usize, 1), local.deinitLog(false));
}
