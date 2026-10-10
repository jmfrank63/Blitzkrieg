//! A std.mem.Allocator over the BkMemory C ABI.
//!
//! Zig code that hands memory to C++ (or receives memory from it) uses this
//! instead of its own allocator, so `bk_mem_free` / `operator delete` can
//! release the block and the process keeps one set of leak records.

const std = @import("std");
const Alignment = std.mem.Alignment;

extern fn bk_mem_alloc_aligned(size: usize, alignment: usize) callconv(.c) ?*anyopaque;
extern fn bk_mem_realloc_aligned(p: ?*anyopaque, size: usize, alignment: usize) callconv(.c) ?*anyopaque;
extern fn bk_mem_free(p: ?*anyopaque) callconv(.c) void;

pub const allocator: std.mem.Allocator = .{ .ptr = undefined, .vtable = &vtable };

const vtable: std.mem.Allocator.VTable = .{
    .alloc = alloc,
    .resize = resize,
    .remap = remap,
    .free = free,
};

fn alloc(_: *anyopaque, len: usize, alignment: Alignment, _: usize) ?[*]u8 {
    return @ptrCast(bk_mem_alloc_aligned(len, alignment.toByteUnits()));
}

/// BkMemory cannot grow a block in place for the caller, so only a no-op
/// resize succeeds; `remap` does the real work.
fn resize(_: *anyopaque, memory: []u8, _: Alignment, new_len: usize, _: usize) bool {
    return new_len == memory.len;
}

fn remap(_: *anyopaque, memory: []u8, alignment: Alignment, new_len: usize, _: usize) ?[*]u8 {
    return @ptrCast(bk_mem_realloc_aligned(memory.ptr, new_len, alignment.toByteUnits()));
}

fn free(_: *anyopaque, memory: []u8, _: Alignment, _: usize) void {
    bk_mem_free(memory.ptr);
}
