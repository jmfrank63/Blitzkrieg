const std = @import("std");
const handles = @import("handles.zig");
pub const Filter = enum { nearest, linear, anisotropic };
pub const Address = enum { clamp, wrap, mirror };
pub const Key = struct { min: Filter = .linear, mag: Filter = .linear, mip: Filter = .linear, u: Address = .wrap, v: Address = .wrap, w: Address = .wrap, anisotropy: u8 = 1, compare: bool = false, compare_function: u8 = 0, min_lod: i16 = 0, max_lod: i16 = 16 };
pub const Cache = struct { keys: std.AutoHashMapUnmanaged(Key, handles.Handle) = .empty, allocator: std.mem.Allocator, next: handles.Handle = 1, hits: u64 = 0, misses: u64 = 0,
    pub fn init(allocator: std.mem.Allocator) Cache { return .{ .allocator = allocator }; }
    pub fn deinit(self: *Cache) void { self.keys.deinit(self.allocator); self.* = undefined; }
    pub fn getOrCreate(self: *Cache, key: Key) !handles.Handle { if (self.keys.get(key)) |value| { self.hits += 1; return value; } const value = self.next; self.next += 1; try self.keys.put(self.allocator, key, value); self.misses += 1; return value; }
};
test "sampler cache reuses immutable keys" { var cache = Cache.init(std.testing.allocator); defer cache.deinit(); const key = Key{}; const first = try cache.getOrCreate(key); const second = try cache.getOrCreate(key); try std.testing.expectEqual(first, second); try std.testing.expectEqual(@as(u64, 1), cache.hits); }
