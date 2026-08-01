const builtin = @import("builtin");
const errors = @import("errors.zig");
const std = @import("std");

const Init = @This();

const Allocated = struct {
    init: Init,
    theaded_io: std.Io.Threaded,
    safe_allocator: ?std.heap.DebugAllocator(.{}),
    arena_allocator: std.heap.ArenaAllocator,
};

/// Permanent storage for the entire process, cleaned automatically on exit.
/// Threadsafe.
arena: *std.heap.ArenaAllocator,

/// A default-selected general purpose allocator for temporary heap allocations.
/// Debug mode will set up leak checking if possible. Threadsafe.
gpa: std.mem.Allocator,

/// An appropriate default Io implementation based on the target configuration.
/// Debug mode will set up leak checking if possible.
io: std.Io,

/// A list of arguments.
args: [][*:0]u8,

/// Deinitialize the init arguments.
///
/// ## Function Parameters
/// * `self`: The init structure.
/// * `allocator`: The allocator used to allocated these args.
pub fn deinit(
    self: *@This(),
    allocator: std.mem.Allocator,
) void {
    const allocated: *Allocated = @fieldParentPtr("init", self);
    allocated.theaded_io.deinit();
    if (allocated.safe_allocator) |*safe_allocator|
        _ = safe_allocator.deinit();
    allocated.arena_allocator.deinit();
    allocator.destroy(allocated);
}

/// Initialize the init arguments.
///
/// ## Function Parameters
/// * `allocator`: The allocator used to allocated these args.
/// * `args`: Arguments to initialize with.
///
/// ## Return Value
/// Return a pointer to the initialized arguments, this must be deinitialized later.
pub fn init(
    allocator: std.mem.Allocator,
    args: [][*:0]u8,
) !*@This() {
    const allocated = try allocator.create(Allocated);
    errdefer allocator.destroy(allocated);

    const is_wasm = builtin.cpu.arch.isWasm();
    const use_safe_allocator = !is_wasm and switch (builtin.mode) {
        .Debug, .ReleaseSafe => true,
        .ReleaseFast, .ReleaseSmall => !builtin.link_libc and builtin.single_threaded, // Also not ideal.
    };
    allocated.safe_allocator = if (use_safe_allocator) std.heap.DebugAllocator(.{}).init else null;
    errdefer if (allocated.safe_allocator) |*safe_allocator| safe_allocator.deinit();

    const gpa = if (use_safe_allocator)
        allocated.safe_allocator.?.allocator()
    else if (builtin.link_libc)
        std.heap.c_allocator
    else if (is_wasm)
        std.heap.wasm_allocator
    else if (!builtin.single_threaded)
        std.heap.smp_allocator
    else
        comptime unreachable;

    const arena_backing_allocator = if (is_wasm) gpa else std.heap.page_allocator;

    allocated.arena_allocator = .init(arena_backing_allocator);
    errdefer allocated.arena_allocator.deinit();

    allocated.theaded_io = .init(gpa, .{});
    errdefer allocated.theaded_io.deinit();

    allocated.init = .{
        .arena = &allocated.arena_allocator,
        .gpa = gpa,
        .io = allocated.theaded_io.io(),
        .args = args,
    };
    return &allocated.init;
}

test "Init" {
    errors.refAllDeclsRecursive(@This());
}
