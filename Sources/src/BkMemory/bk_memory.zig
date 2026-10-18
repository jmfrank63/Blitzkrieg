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

/// The depth the platform's capture cost allows without `-Dbk-mem-stack-frames`.
/// Windows keeps 24 because every allocation there already spends operator new
/// plus the MSVC STL allocator inlines on plumbing, which left no captured
/// frame for the engine call site the leak reports name, and its
/// RtlCaptureStackBackTrace makes a deeper stack a fixed small cost. macOS
/// arm64 keeps 24 because its captures walk frame pointers (debug.SelfInfo
/// below). 0 on Linux and on macOS x64: there every captured frame is unwound
/// from the images' DWARF tables through std.debug's shared unwind state, and
/// the engine's crash handler caught that machinery SEGV under the CI runners
/// (38307115498: the counts still worked while a driver's own thread - the
/// runner image's gfxstream ICD assigning through the hooked operator new -
/// unwound the same shared state concurrently; both threads met in
/// SelfInfo's module reader). The counts never unwind, so the CI gates
/// (BK_MEM_REPORT=count) are unaffected; a Linux or macOS leak investigation
/// passes -Dbk-mem-stack-frames=24 explicitly for that run, alone in the
/// process, on a foreign-ICD-free loader (VK_DRIVER_FILES=lvp).
const default_stack_frames: usize = if (builtin.os.tag == .windows or
    (builtin.os.tag.isDarwin() and builtin.cpu.arch == .aarch64))
    24
else
    0;

/// Frames recorded per allocation, from -Dbk-mem-stack-frames. 0 keeps the
/// platform default; a Linux or macOS leak investigation passes 24 explicitly
/// for that run to get the full stacks back.
const stack_frames: usize = if (build_options.stack_frames != 0)
    build_options.stack_frames
else
    default_stack_frames;

var instance: if (use_safe) std.heap.SafeAllocator else void =
    if (use_safe) std.heap.SafeAllocator.init(backing, .{ .canary = canary, .stack_trace_frames = stack_frames }) else {};

/// The record path's serialization. std.debug's SelfInfo - which every stack
/// capture walks - is a process-global built lazily on first use and guarded
/// nowhere on the unwind path itself; two threads capturing at once met
/// inside its ELF module reader on the CI runner and SEGV'd (the
/// random-missions crash backtrace: Io.Reader.peekByte under getUnwindSections;
/// on Windows the same race bailed 37310 of 43197 allocation stacks out at
/// their first frame instead). While captures are armed (a non-zero
/// -Dbk-mem-stack-frames) the capture-holding backend calls hold this mutex,
/// so an unwind never runs beside another one. With captures off - the CI
/// gates' default on the counted tiers - the mutex is skipped entirely: the
/// backend calls are already free of shared state without it.
var record_mutex: std.atomic.Mutex = .unlocked;

/// tryLock with a yield loop: std.atomic.Mutex has no blocking lock on its
/// own (the engine's threads must not fail an allocation because the record
/// path was busy, so a try is repeated rather than abandoned; the record
/// itself is short). Not re-entrant: alloc/free/realloc hold no caller lock.
inline fn lockRecordSpin() void {
    while (!record_mutex.tryLock()) {
        std.atomic.spinLoopHint();
        std.Thread.yield() catch {};
    }
}

/// Every allocation of a debug build records a stack, and std.debug unwinds
/// it from the images' unwind tables before it tries frame pointers. On macOS
/// arm64 that table walk cost more than the rest of the allocation many times
/// over: the debug editor-bridge tier ran about 15 times longer than before
/// BkMemory and CI killed the macOS jobs. Apple's arm64 ABI keeps a frame
/// pointer in every frame, so there the frame chain is walked instead; the
/// leak reports still get their file and line from the default lookup.
pub const debug = struct {
    pub const SelfInfo = if (builtin.os.tag.isDarwin() and builtin.cpu.arch == .aarch64)
        FramePointerSelfInfo
    else
        std.debug.TargetInfo(builtin.os.tag, builtin.cpu.arch);
};

/// The default SelfInfo with `can_unwind` off, so std.debug takes the frame
/// pointer path for every capture.
const FramePointerSelfInfo = struct {
    const Inner = std.debug.TargetInfo(builtin.os.tag, builtin.cpu.arch);

    inner: Inner,

    pub const init: FramePointerSelfInfo = .{ .inner = .init };
    pub const can_unwind = false;

    pub fn deinit(si: *FramePointerSelfInfo, io: std.Io) void {
        si.inner.deinit(io);
    }

    pub fn getSymbols(
        si: *FramePointerSelfInfo,
        io: std.Io,
        symbol_allocator: std.mem.Allocator,
        text_arena: std.mem.Allocator,
        address: usize,
        resolve_inline_callers: bool,
        symbols: *std.ArrayList(std.debug.Symbol),
    ) std.debug.SelfInfoError!void {
        return si.inner.getSymbols(io, symbol_allocator, text_arena, address, resolve_inline_callers, symbols);
    }

    pub fn getModuleName(si: *FramePointerSelfInfo, io: std.Io, address: usize) std.debug.SelfInfoError![]const u8 {
        return si.inner.getModuleName(io, address);
    }

    pub fn getModuleSlide(si: *FramePointerSelfInfo, io: std.Io, address: usize) std.debug.SelfInfoError!usize {
        return si.inner.getModuleSlide(io, address);
    }
};

const Header = extern struct {
    size: u64,
    align_log2: u8,
    /// The `modules` entry of the code that called operator new, 0 for the
    /// C ABI callers, which are all ours.
    module: u8,
    _reserved: [2]u8,
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

/// The modules that called operator new, so the exit report can tell our
/// blocks from those of a foreign library. The replaced operator new is the
/// whole process's on Linux and macOS, so libLLVM (loaded by Mesa's drivers)
/// and Apple's CoreML (loaded by SDL's Metal stack) allocate through it too.
/// Both build registries in their static initializers and keep them until the
/// process ends: about 2,650 blocks in the Game on Linux, 4 per tool run on
/// macOS. That memory is not ours to free, and counted as a leak it hid every
/// leak of our own behind a count that never reaches 0. Each of our modules
/// registers itself from new_delete.cpp; a module that never did is foreign,
/// and its blocks are reported per library instead of as leaks. Ownership is
/// decided at report time, so a module whose first allocation came before its
/// registration still counts as ours.
const ModuleEntry = struct {
    /// The module's code range, [start, end). Both are 0 once our module has
    /// unloaded, so a library loaded later at the same address is not taken for it.
    start: usize,
    end: usize,
    own: bool,
    /// The C++ runtime itself (libc++, libstdc++), whose own calls of
    /// operator new are attributed to the code that called into the runtime.
    runtime: bool,
    live: usize,
    name_len: u8,
    name: [55]u8,
};

/// Entry 0 is the C ABI callers. 255 entries cover every library a process
/// loads with room to spare; a module past them is entry 0, counted as ours.
var modules: [256]ModuleEntry = @splat(.{ .start = 0, .end = 0, .own = true, .runtime = false, .live = 0, .name_len = 0, .name = @splat(0) });
var module_count: usize = 1;
var module_mutex: std.atomic.Mutex = .unlocked;

inline fn lockModuleSpin() void {
    while (!module_mutex.tryLock()) {
        std.atomic.spinLoopHint();
        std.Thread.yield() catch {};
    }
}

/// A module's code range and file name, found from one address in it.
const ModuleInfo = struct { start: usize, end: usize, path: []const u8 };

fn findModule(address: usize) ?ModuleInfo {
    switch (builtin.os.tag) {
        .linux => {
            const Search = struct {
                address: usize,
                found: ?ModuleInfo = null,

                fn visit(info: *std.c.dl_phdr_info, _: usize, data: ?*anyopaque) callconv(.c) c_int {
                    const search: *@This() = @ptrCast(@alignCast(data.?));
                    var low: usize = std.math.maxInt(usize);
                    var high: usize = 0;
                    for (info.phdr[0..info.phnum]) |ph| {
                        if (@intFromEnum(ph.type) != 1) continue; // PT_LOAD
                        low = @min(low, info.addr + ph.vaddr);
                        high = @max(high, info.addr + ph.vaddr + ph.memsz);
                    }
                    if (search.address < low or search.address >= high) return 0;
                    const name = if (info.name) |n| std.mem.span(n) else "";
                    search.found = .{ .start = low, .end = high, .path = if (name.len == 0) "main program" else name };
                    return 1;
                }
            };
            var search: Search = .{ .address = address };
            _ = std.c.dl_iterate_phdr(Search.visit, &search);
            return search.found;
        },
        .macos => {
            var info: std.c.dl_info = undefined;
            if (std.c.dladdr(@ptrFromInt(address), &info) == 0) return null;
            // A return address is always in __TEXT, which starts at the header.
            // The shared cache's images keep their other segments apart from
            // __TEXT, so the whole image is not one range; __TEXT is.
            const header: *const std.macho.mach_header_64 = @ptrCast(@alignCast(info.fbase));
            var command: [*]const u8 = @ptrCast(info.fbase);
            command += @sizeOf(std.macho.mach_header_64);
            for (0..header.ncmds) |_| {
                const lc: *const std.macho.load_command = @ptrCast(@alignCast(command));
                if (lc.cmd == .SEGMENT_64) {
                    const seg: *const std.macho.segment_command_64 = @ptrCast(@alignCast(command));
                    if (std.mem.eql(u8, std.mem.sliceTo(&seg.segname, 0), "__TEXT")) {
                        const start = @intFromPtr(info.fbase);
                        return .{ .start = start, .end = start + seg.vmsize, .path = std.mem.span(info.fname) };
                    }
                }
                command += lc.cmdsize;
            }
            return null;
        },
        .windows => {
            const from_address = 0x4;
            const unchanged_refcount = 0x2;
            var handle: ?*anyopaque = null;
            if (kernel32.GetModuleHandleExW(from_address | unchanged_refcount, @ptrFromInt(address), &handle) == 0) return null;
            const base: [*]const u8 = @ptrCast(handle.?);
            const nt = std.mem.readInt(u32, base[0x3c..][0..4], .little);
            // SizeOfImage: the optional header starts 24 bytes into the NT
            // headers and holds it at 56 in both PE32 and PE32+.
            const size = std.mem.readInt(u32, base[nt + 24 + 56 ..][0..4], .little);
            const S = struct {
                var path: [260]u8 = undefined;
            };
            var wide: [260]u16 = undefined;
            const n = kernel32.GetModuleFileNameW(handle, &wide, wide.len);
            // The report prints the name only; non-ASCII characters become '?'.
            for (wide[0..n], 0..) |ch, i| S.path[i] = if (ch < 0x80) @intCast(ch) else '?';
            return .{ .start = @intFromPtr(base), .end = @intFromPtr(base) + size, .path = S.path[0..n] };
        },
        else => return null,
    }
}

fn baseName(path: []const u8) []const u8 {
    const slash = std.mem.lastIndexOfAny(u8, path, "/\\") orelse return path;
    return path[slash + 1 ..];
}

fn entryFor(address: usize) u8 {
    const n = @atomicLoad(usize, &module_count, .acquire);
    for (modules[1..n], 1..) |*m, i| {
        if (address >= @atomicLoad(usize, &m.start, .monotonic) and address < @atomicLoad(usize, &m.end, .monotonic)) return @intCast(i);
    }
    return addEntry(address, false);
}

/// `fresh` makes a new entry even when one covers the address: a module that
/// registers is new to the process, and an entry still covering its range is
/// a library unloaded before it.
fn addEntry(address: usize, fresh: bool) u8 {
    lockModuleSpin();
    defer module_mutex.unlock();
    const n = module_count;
    for (modules[1..n], 1..) |*m, i| {
        if (address < m.start or address >= m.end) continue;
        if (!fresh) return @intCast(i);
        @atomicStore(usize, &m.start, 0, .monotonic);
        @atomicStore(usize, &m.end, 0, .monotonic);
    }
    if (n == modules.len) return 0;
    const info = findModule(address) orelse return 0;
    const name = baseName(info.path);
    const m = &modules[n];
    m.* = .{ .start = info.start, .end = info.end, .own = false, .runtime = false, .live = 0, .name_len = 0, .name = @splat(0) };
    m.name_len = @intCast(@min(name.len, m.name.len));
    @memcpy(m.name[0..m.name_len], name[0..m.name_len]);
    m.runtime = std.mem.startsWith(u8, name, "libc++") or std.mem.startsWith(u8, name, "libstdc++");
    @atomicStore(usize, &module_count, n + 1, .release);
    return @intCast(n);
}

/// The entry of the code that called operator new. A call that the C++
/// runtime made on someone's behalf (basic_string's out-of-line growth in
/// libc++.1.dylib) belongs to the code that called the runtime, found by
/// walking the frame chain from operator new's own frame. Only on macOS,
/// whose ABI keeps a frame record in every frame of the system libraries;
/// elsewhere our modules carry their C++ runtime inside them, and a foreign
/// library's call through the system runtime is foreign either way.
fn callerEntry(return_address: usize, frame: usize) u8 {
    var ra = return_address;
    var fp = frame;
    var entry = entryFor(ra);
    if (comptime !builtin.os.tag.isDarwin()) return entry;
    var depth: usize = 0;
    while (modules[entry].runtime and depth < 8) : (depth += 1) {
        if (fp == 0 or fp % @sizeOf(usize) != 0) break;
        const next: usize = @as(*const usize, @ptrFromInt(fp)).*;
        // A frame record further up the same stack, never a wild pointer.
        if (next <= fp or next - fp > 1 << 20 or next % @sizeOf(usize) != 0) break;
        // arm64e system code may sign its saved return addresses; the
        // address bits are the low 47.
        ra = @as(*const usize, @ptrFromInt(next + @sizeOf(usize))).* & ((1 << 47) - 1);
        fp = next;
        entry = entryFor(ra);
    }
    return entry;
}

/// Blocks still live from modules that never registered.
fn foreignLive() usize {
    var total: usize = 0;
    const n = @atomicLoad(usize, &module_count, .acquire);
    for (modules[1..n]) |*m| {
        if (!@atomicLoad(bool, &m.own, .monotonic)) total += @atomicLoad(usize, &m.live, .monotonic);
    }
    return total;
}

/// One line per foreign library with live blocks and, when `leaks` of our
/// own are left, where they came from: the module whose operator new made
/// them, or the C ABI (Zig code and the C libraries) for the rest. That
/// names the module to look in without the per-block stacks, which on Linux
/// cost the editors more than the run itself.
fn logModules(leaks: usize) void {
    const log = std.log.scoped(.BkMemory);
    const n = @atomicLoad(usize, &module_count, .acquire);
    if (leaks != 0) {
        var by_operator_new: usize = 0;
        for (modules[1..n]) |*m| {
            const live = @atomicLoad(usize, &m.live, .monotonic);
            if (!m.own or live == 0) continue;
            by_operator_new += live;
            log.warn("bk_mem: of those, {d} from {s}'s operator new", .{ live, m.name[0..m.name_len] });
        }
        if (leaks > by_operator_new) log.warn("bk_mem: of those, {d} from the C ABI", .{leaks - by_operator_new});
    }
    for (modules[1..n]) |*m| {
        const live = @atomicLoad(usize, &m.live, .monotonic);
        if (m.own or live == 0) continue;
        log.warn("bk_mem: {d} foreign block(s) from {s}", .{ live, m.name[0..m.name_len] });
    }
}

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

fn allocImpl(size: usize, alignment: usize, ra: usize, module: u8) ?[*]u8 {
    if (@atomicLoad(bool, &closed, .acquire)) std.debug.panic("bk_mem_alloc after bk_mem_report", .{});
    const a = normalizeAlign(alignment) orelse return null;
    const pad = padFor(a.toByteUnits());
    const total = std.math.add(usize, pad, size) catch return null;
    if (comptime stack_frames != 0) lockRecordSpin();
    const base = backend().rawAlloc(total, a, ra) orelse {
        if (comptime stack_frames != 0) record_mutex.unlock();
        return null;
    };
    if (comptime stack_frames != 0) record_mutex.unlock();
    const user = base + pad;
    headerOf(user).* = .{
        .size = size,
        .align_log2 = @intCast(@intFromEnum(a)),
        .module = module,
        ._reserved = .{ 0, 0 },
        .magic = live_magic,
    };
    _ = @atomicRmw(usize, &live_count, .Add, 1, .monotonic);
    _ = @atomicRmw(usize, &live_bytes, .Add, size, .monotonic);
    if (module != 0) _ = @atomicRmw(usize, &modules[module].live, .Add, 1, .monotonic);
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
    if (h.module != 0) _ = @atomicRmw(usize, &modules[h.module].live, .Sub, 1, .monotonic);
    _ = @atomicRmw(usize, &live_count, .Sub, 1, .monotonic);
    _ = @atomicRmw(usize, &live_bytes, .Sub, size, .monotonic);
    if (comptime stack_frames != 0) lockRecordSpin();
    defer if (comptime stack_frames != 0) record_mutex.unlock();
    backend().rawFree(base[0 .. pad + size], alignment, ra);
}

fn reallocImpl(user: ?[*]u8, new_size: usize, want_align: usize, ra: usize) ?[*]u8 {
    const p = user orelse return allocImpl(new_size, want_align, ra, 0);
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
    if (comptime stack_frames != 0) lockRecordSpin();
    const remapped = backend().rawRemap(mem, old_align, total, ra);
    if (comptime stack_frames != 0) record_mutex.unlock();
    if (remapped) |new_base| {
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
    const fresh = allocImpl(new_size, alignment, ra, headerOf(old).module) orelse return null;
    @memcpy(fresh[0..@min(old_size, new_size)], old[0..@min(old_size, new_size)]);
    freeImpl(old, ra);
    return fresh;
}

export fn bk_mem_alloc(size: usize) callconv(.c) ?*anyopaque {
    return allocImpl(size, min_align, @returnAddress(), 0);
}

export fn bk_mem_alloc_aligned(size: usize, alignment: usize) callconv(.c) ?*anyopaque {
    return allocImpl(size, alignment, @returnAddress(), 0);
}

export fn bk_mem_calloc(n: usize, size: usize) callconv(.c) ?*anyopaque {
    const total = std.math.mul(usize, n, size) catch return null;
    const p = allocImpl(total, min_align, @returnAddress(), 0) orelse return null;
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

/// operator new's entry: `caller` is its return address and `frame` its own
/// frame record (0 when there is none), which attribute the block to the
/// module that asked for it. An alignment of 0 is the default one.
export fn bk_mem_alloc_from(size: usize, alignment: usize, caller: ?*const anyopaque, frame: ?*const anyopaque) callconv(.c) ?*anyopaque {
    const module = callerEntry(@intFromPtr(caller), @intFromPtr(frame));
    return allocImpl(size, if (alignment == 0) min_align else alignment, @returnAddress(), module);
}

/// Marks the module holding `address` as ours (see ModuleEntry).
export fn bk_mem_register_module(address: ?*const anyopaque) callconv(.c) void {
    const entry = addEntry(@intFromPtr(address), true);
    if (entry != 0) @atomicStore(bool, &modules[entry].own, true, .monotonic);
}

/// Our module holding `address` is unloading: its range is forgotten, and
/// its blocks still count as ours.
export fn bk_mem_unregister_module(address: ?*const anyopaque) callconv(.c) void {
    const a = @intFromPtr(address);
    lockModuleSpin();
    defer module_mutex.unlock();
    for (modules[1..module_count]) |*m| {
        if (a < m.start or a >= m.end) continue;
        @atomicStore(usize, &m.start, 0, .monotonic);
        @atomicStore(usize, &m.end, 0, .monotonic);
    }
}

/// Live blocks of foreign libraries, which the leak counts leave out.
export fn bk_mem_foreign_count() callconv(.c) usize {
    return foreignLive();
}

export fn bk_mem_live_count() callconv(.c) usize {
    return @atomicLoad(usize, &live_count, .monotonic);
}

export fn bk_mem_live_bytes() callconv(.c) usize {
    return @atomicLoad(usize, &live_bytes, .monotonic);
}

/// Runs the final leak check now and closes the allocator. Returns the leak
/// count (the live-block count when the backend keeps no leak records),
/// without the blocks of foreign libraries. Later calls return the same count.
export fn bk_mem_report() callconv(.c) usize {
    return reportImpl(true);
}

/// `print` false counts the leaks without logging them: a tier that leaks
/// hundreds of thousands of blocks spends minutes symbolizing their stacks.
fn reportImpl(print: bool) usize {
    if (@cmpxchgStrong(bool, &closed, false, true, .acq_rel, .acquire) != null) {
        return @atomicLoad(usize, &report_leaks, .acquire);
    }
    const all: usize = if (use_safe) instance.deinitLog(print) else @atomicLoad(usize, &live_count, .monotonic);
    const leaks = all -| foreignLive();
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
    extern "kernel32" fn GetModuleHandleExW(flags: u32, address: ?*const anyopaque, module: *?*anyopaque) callconv(.winapi) i32;
    extern "kernel32" fn GetModuleFileNameW(module: ?*anyopaque, name: [*]u16, size: u32) callconv(.winapi) u32;
};

const exit_leaks: u32 = 3;
const exit_in_use: u32 = 4;

/// What the detach report does. `fail` is the strict form (leaks exit 3),
/// `log` reports the same way but never fails on leaks, so a tier stays green
/// while the leaks it names are still being fixed. `count` is `log` without the
/// per-block stacks, for tiers whose leak report would run to megabytes.
const ReportPolicy = enum { off, fail, log, count };

/// BK_MEM_REPORT=0|1|log|count overrides the build default for diagnosis. Any value
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
    if (std.mem.eql(u8, value, "count")) return .count;
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
/// was mid-allocation: Windows' ExitProcess kills the other threads without
/// unwinding, so there the hold is permanent and the records are not trusted;
/// POSIX exit() runs this destructor while the other threads still run (the
/// single-instance helper's detached accept and connection threads (WR-A02)
/// outlive main), so there a hold is one allocation in flight and the walk
/// waits it out before declaring the records untrusted.
fn detachHook() void {
    if (@atomicLoad(bool, &closed, .acquire)) return;
    const policy = reportPolicy();
    if (policy == .off) return;
    const log = std.log.scoped(.BkMemory);
    if (use_safe) {
        const hold_is_permanent = builtin.os.tag == .windows;
        for (&instance.threads) |*t| {
            if (t.mutex == .unlocked) continue;
            if (!hold_is_permanent) {
                // The holder unlocks within the allocation it is inside -
                // microseconds, not the 100 ms this waits - so a hold that
                // survives the wait is a genuinely stuck thread, and only
                // that reaches the exit below.
                var waits: usize = 0;
                const nap: std.c.timespec = .{ .sec = 0, .nsec = 1_000_000 };
                while (t.mutex != .unlocked and waits < 100) : (waits += 1) {
                    _ = std.c.nanosleep(&nap, null);
                }
            }
            if (t.mutex != .unlocked) {
                log.err("allocator in use at exit", .{});
                exitNow(exit_in_use);
            }
        }
    }
    // The bucket walk races a thread that is still allocating at exit - the
    // single-instance helper's detached accept and connection threads
    // (WR-A02) can outlive main - and trips SafeAllocator's own leak-count
    // assertion mid-walk. The atomic live count is the same number without
    // touching the buckets; only the explicit BK_MEM_REPORT=log walks them,
    // and whoever set it asked for the per-block stacks.
    const leaks = if (policy == .log)
        reportImpl(true)
    else blk: {
        const n = @atomicLoad(usize, &live_count, .monotonic) -| foreignLive();
        @atomicStore(usize, &report_leaks, n, .release);
        @atomicStore(bool, &closed, true, .release);
        break :blk n;
    };
    if (policy == .log or policy == .count) {
        // For `log` the full report with stacks came from reportImpl; this is the
        // one line a script can count, printed for a clean run too.
        log.warn("bk_mem: {d} leaked block(s)", .{leaks});
        logModules(leaks);
        return;
    }
    if (leaks > 0) log.err("{d} leaked block(s) at exit", .{leaks});
    logModules(leaks);
    if (leaks > 0) exitNow(exit_leaks);
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

/// The exit-time report on ELF and Mach-O. Zig's debug backend does not
/// implement `ExportOptions.section`, so there is no `.fini_array` entry from
/// here; `bk_memory_fini.c` in the same library is that entry
/// (`__attribute__((destructor))`) and calls this. Exported only from the
/// shared library: the test binary has no destructor and reports nothing.
fn finiEntry() callconv(.c) void {
    detachHook();
}

comptime {
    if (builtin.output_mode == .Lib and builtin.os.tag != .windows) {
        @export(&finiEntry, .{ .name = "bk_mem_fini_entry" });
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

test "default stack depth follows the platform" {
    // An explicit -Dbk-mem-stack-frames is the caller's call, not the rule.
    if (build_options.stack_frames != 0) return;
    const expected: usize = if (builtin.os.tag == .windows or
        (builtin.os.tag.isDarwin() and builtin.cpu.arch == .aarch64))
        24
    else
        // The DWARF-unwind platforms capture nothing by default: std.debug's
        // shared unwind state raced two threads' captures on the CI runner
        // (the SEGV inside SelfInfo's module reader, run 38307115498) and the
        // counts the CI gates read never unwind anyway.
        0;
    try testing.expectEqual(expected, stack_frames);
}

/// An address in a library the test binary did not build: libc's malloc, or
/// kernel32's GetCurrentProcess on Windows.
fn foreignCodeAddress() ?*const anyopaque {
    if (builtin.os.tag == .windows) {
        const k32 = struct {
            extern "kernel32" fn GetModuleHandleA(name: [*:0]const u8) callconv(.winapi) ?*anyopaque;
            extern "kernel32" fn GetProcAddress(module: ?*anyopaque, name: [*:0]const u8) callconv(.winapi) ?*const anyopaque;
        };
        return k32.GetProcAddress(k32.GetModuleHandleA("kernel32.dll"), "GetCurrentProcess");
    }
    // RTLD_DEFAULT: 0 on glibc, -2 on Darwin.
    const default_handle: ?*anyopaque = if (builtin.os.tag.isDarwin()) @ptrFromInt(std.math.maxInt(usize) - 1) else null;
    return std.c.dlsym(default_handle, "malloc");
}

test "a foreign library's blocks are counted apart from ours" {
    bk_mem_register_module(@ptrCast(&foreignCodeAddress));
    const foreign = foreignCodeAddress() orelse return error.SkipZigTest;
    const f0 = bk_mem_foreign_count();
    const c0 = bk_mem_live_count();

    const own = bk_mem_alloc_from(24, 0, @ptrFromInt(@returnAddress()), null).?;
    try testing.expectEqual(f0, bk_mem_foreign_count());
    const theirs = bk_mem_alloc_from(24, 64, foreign, null).?;
    try testing.expectEqual(f0 + 1, bk_mem_foreign_count());
    try testing.expectEqual(c0 + 2, bk_mem_live_count());
    try testing.expectEqual(@as(usize, 0), @intFromPtr(theirs) % 64);

    const entry = modules[headerOf(@ptrCast(theirs)).module];
    try testing.expect(!entry.own);
    try testing.expect(entry.name_len != 0);
    try testing.expect(modules[headerOf(@ptrCast(own)).module].own);

    // A block that moves stays the library's.
    const moved = bk_mem_realloc_aligned(theirs, 32, 4096).?;
    try testing.expectEqual(f0 + 1, bk_mem_foreign_count());
    bk_mem_free(moved);
    bk_mem_free(own);
    try testing.expectEqual(f0, bk_mem_foreign_count());
    try testing.expectEqual(c0, bk_mem_live_count());
}

test "an unloaded module's range is forgotten, its blocks stay ours" {
    const here: *const anyopaque = @ptrCast(&foreignCodeAddress);
    bk_mem_register_module(here);
    const p = bk_mem_alloc_from(8, 0, here, null).?;
    const index = headerOf(@ptrCast(p)).module;
    bk_mem_unregister_module(here);
    try testing.expectEqual(@as(usize, 0), modules[index].end);
    try testing.expect(modules[index].own);
    bk_mem_free(p);
    bk_mem_register_module(here);
}
