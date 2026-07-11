const std = @import("std");

var buffers: [10]std.ArrayListUnmanaged(u8) = [_]std.ArrayListUnmanaged(u8){.empty} ** 10;

pub export fn bk_streamio_temp_buffer(size: c_int, index: c_int) callconv(.c) ?*anyopaque {
    if (size <= 0 or index < 0 or index >= buffers.len) return null;

    const buffer = &buffers[@intCast(index)];
    buffer.ensureTotalCapacity(std.heap.page_allocator, @intCast(size)) catch return null;
    buffer.items.len = @intCast(size);
    return @ptrCast(buffer.items.ptr);
}
