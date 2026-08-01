const std = @import("std");

pub const TransferError = error{ ZeroBytes, MapFailed, Overflow };
pub const Allocation = struct { offset: usize, length: usize, submission_serial: u64, mapped: bool = true };

pub const Pool = struct {
    allocator: std.mem.Allocator,
    storage: []u8,
    cursor: usize = 0,
    high_water: usize = 0,
    map_allowed: bool = true,

    pub fn init(allocator: std.mem.Allocator, initial_size: usize) !Pool {
        if (initial_size == 0) return TransferError.ZeroBytes;
        return .{ .allocator = allocator, .storage = try allocator.alloc(u8, initial_size) };
    }
    pub fn deinit(self: *Pool) void { self.allocator.free(self.storage); self.* = undefined; }
    pub fn alloc(self: *Pool, length: usize, alignment: usize, serial: u64) !Allocation {
        if (length == 0) return TransferError.ZeroBytes;
        if (!self.map_allowed) return TransferError.MapFailed;
        const aligned = std.mem.alignForward(usize, self.cursor, alignment);
        const end = std.math.add(usize, aligned, length) catch return TransferError.Overflow;
        if (end > self.storage.len) {
            var next = self.storage.len * 2;
            while (next < end) next *= 2;
            const grown = try self.allocator.alloc(u8, next);
            @memcpy(grown[0..self.cursor], self.storage[0..self.cursor]);
            self.allocator.free(self.storage); self.storage = grown;
        }
        self.cursor = end; self.high_water = @max(self.high_water, end);
        return .{ .offset = aligned, .length = length, .submission_serial = serial };
    }
    pub fn reset(self: *Pool) void { self.cursor = 0; }
};

test "transfer pool aligns, grows, and rejects zero/map failures" {
    var pool = try Pool.init(std.testing.allocator, 8); defer pool.deinit();
    const first = try pool.alloc(3, 4, 1); try std.testing.expectEqual(@as(usize, 0), first.offset);
    const second = try pool.alloc(9, 8, 2); try std.testing.expectEqual(@as(usize, 8), second.offset);
    try std.testing.expect(pool.storage.len >= 17);
    try std.testing.expectError(TransferError.ZeroBytes, pool.alloc(0, 1, 0));
    pool.map_allowed = false; try std.testing.expectError(TransferError.MapFailed, pool.alloc(1, 1, 3));
}
