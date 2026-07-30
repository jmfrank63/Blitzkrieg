const std = @import("std");
const handles = @import("handles.zig");

pub const Usage = enum { static, dynamic, temporary };
pub const BufferError = error{ ZeroSize, Overflow, StaticUpdate, OutOfBounds, MisalignedIndex };
pub const Buffer = struct { size: u64, stride: u32, usage: Usage, index_size: u8 = 0, last_use_serial: u64 = 0 };
pub const Store = struct {
    registry: handles.Registry(Buffer),
    pub fn init(allocator: std.mem.Allocator) Store { return .{ .registry = handles.Registry(Buffer).init(allocator) }; }
    pub fn deinit(self: *Store) void { self.registry.deinit(false) catch unreachable; }
    pub fn create(self: *Store, size: u64, stride: u32, usage: Usage, index_size: u8) !handles.Handle {
        if (size == 0) return BufferError.ZeroSize;
        if (index_size != 0 and index_size != 2 and index_size != 4) return BufferError.MisalignedIndex;
        return self.registry.insert(.{ .size = size, .stride = stride, .usage = usage, .index_size = index_size });
    }
    pub fn update(self: *Store, handle: handles.Handle, offset: u64, length: u64) !void {
        const buffer = try self.registry.getMut(handle);
        if (buffer.usage == .static) return BufferError.StaticUpdate;
        if (offset > buffer.size or length > buffer.size - offset) return BufferError.OutOfBounds;
    }
};
test "buffer usage, bounds, and stale handles" {
    var store = Store.init(std.testing.allocator); defer store.deinit();
    const static = try store.create(16, 4, .static, 0);
    try std.testing.expectError(BufferError.StaticUpdate, store.update(static, 0, 1));
    const dynamic = try store.create(16, 4, .dynamic, 0);
    try store.update(dynamic, 4, 12);
    try std.testing.expectError(BufferError.OutOfBounds, store.update(dynamic, 8, 9));
    _ = try store.registry.remove(dynamic);
    try std.testing.expectError(error.InvalidHandle, store.registry.get(dynamic));
}
