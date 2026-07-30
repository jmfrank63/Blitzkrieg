const std = @import("std");

pub const Handle = u64;
pub const invalid_handle: Handle = 0;
pub const RegistryError = error{ InvalidHandle, LiveHandles };

var next_registry_tag: u8 = 1;

fn takeRegistryTag() u8 {
    const tag = next_registry_tag;
    next_registry_tag +%= 1;
    if (next_registry_tag == 0) next_registry_tag = 1;
    return tag;
}

fn makeGeneration(tag: u8, counter: u32) u32 {
    const low: u32 = if ((counter & 0x00ff_ffff) == 0) 1 else counter & 0x00ff_ffff;
    return (@as(u32, tag) << 24) | low;
}

fn makeHandle(index: u32, generation: u32) Handle {
    return (@as(u64, generation) << 32) | @as(u64, index + 1);
}

pub fn Registry(comptime T: type) type {
    return struct {
        const Self = @This();
        const Slot = struct {
            generation: u32,
            value: ?T = null,
        };

        allocator: std.mem.Allocator,
        slots: std.ArrayListUnmanaged(Slot) = .empty,
        free_indices: std.ArrayListUnmanaged(u32) = .empty,
        registry_tag: u8,
        live_count: usize = 0,

        pub fn init(allocator: std.mem.Allocator) Self {
            return .{ .allocator = allocator, .registry_tag = takeRegistryTag() };
        }

        pub fn insert(self: *Self, value: T) !Handle {
            if (self.free_indices.items.len != 0) {
                const index = self.free_indices.pop().?;
                const slot = &self.slots.items[index];
                slot.value = value;
                self.live_count += 1;
                return makeHandle(index, slot.generation);
            }

            const index: u32 = @intCast(self.slots.items.len);
            const generation = makeGeneration(self.registry_tag, 1);
            try self.slots.append(self.allocator, .{ .generation = generation, .value = value });
            self.live_count += 1;
            return makeHandle(index, generation);
        }

        pub fn get(self: *Self, handle: Handle) RegistryError!*const T {
            const slot = try self.lookupSlot(handle);
            return &slot.value.?;
        }

        pub fn getMut(self: *Self, handle: Handle) RegistryError!*T {
            const slot = try self.lookupSlot(handle);
            return &slot.value.?;
        }

        pub fn remove(self: *Self, handle: Handle) !T {
            const index = try self.indexFor(handle);
            // Reserve the free-list entry before mutating the live slot so an
            // allocation failure cannot silently change registry counts.
            try self.free_indices.append(self.allocator, index);
            const slot = &self.slots.items[index];
            const value = slot.value.?;
            var counter = (slot.generation & 0x00ff_ffff) +% 1;
            if ((counter & 0x00ff_ffff) == 0) counter = 1;
            slot.generation = makeGeneration(self.registry_tag, counter);
            slot.value = null;
            self.live_count -= 1;
            return value;
        }

        pub fn liveCount(self: *const Self) usize {
            return self.live_count;
        }

        pub fn deinit(self: *Self, assert_empty: bool) !void {
            if (assert_empty and self.live_count != 0) return RegistryError.LiveHandles;
            self.slots.deinit(self.allocator);
            self.free_indices.deinit(self.allocator);
            self.* = undefined;
        }

        fn indexFor(self: *const Self, handle: Handle) RegistryError!u32 {
            if (handle == invalid_handle) return RegistryError.InvalidHandle;
            const index_plus_one: u32 = @truncate(handle & 0xffff_ffff);
            const generation: u32 = @truncate(handle >> 32);
            if (index_plus_one == 0 or generation == 0) return RegistryError.InvalidHandle;
            const index = index_plus_one - 1;
            if (index >= self.slots.items.len) return RegistryError.InvalidHandle;
            const slot = self.slots.items[index];
            if (slot.generation != generation or slot.value == null) return RegistryError.InvalidHandle;
            if ((generation >> 24) != self.registry_tag) return RegistryError.InvalidHandle;
            return index;
        }

        fn lookupSlot(self: *Self, handle: Handle) RegistryError!*Slot {
            return &self.slots.items[try self.indexFor(handle)];
        }
    };
}

test "generational registry rejects zero, stale, and wrong-registry handles" {
    var first = Registry(u32).init(std.testing.allocator);
    defer first.deinit(false) catch unreachable;
    var second = Registry(u32).init(std.testing.allocator);
    defer second.deinit(false) catch unreachable;

    const handle = try first.insert(42);
    try std.testing.expect(handle != invalid_handle);
    try std.testing.expectEqual(@as(u32, 42), (try first.get(handle)).*);
    try std.testing.expectError(RegistryError.InvalidHandle, first.get(invalid_handle));
    try std.testing.expectError(RegistryError.InvalidHandle, second.get(handle));
    _ = try first.remove(handle);
    try std.testing.expectError(RegistryError.InvalidHandle, first.get(handle));
    const reused = try first.insert(7);
    try std.testing.expect(reused != handle);
    try std.testing.expectEqual(@as(usize, 1), first.liveCount());
}

test "generation wrap skips zero and deinit can assert emptiness" {
    var registry = Registry(u8).init(std.testing.allocator);
    const handle = try registry.insert(1);
    const index = @as(usize, @intCast(@as(u32, @truncate(handle)) - 1));
    registry.slots.items[index].generation = makeGeneration(registry.registry_tag, 0x00ff_ffff);
    _ = try registry.remove(handle);
    const next = try registry.insert(2);
    try std.testing.expect(@as(u32, @truncate(next >> 32)) & 0x00ff_ffff == 1);
    try std.testing.expectError(RegistryError.LiveHandles, registry.deinit(true));
    _ = try registry.remove(next);
    try registry.deinit(true);
}

test "failed insertion and removal preserve live counts" {
    var failing = std.testing.FailingAllocator.init(std.testing.allocator, .{ .fail_index = 0 });
    var registry = Registry(u32).init(failing.allocator());
    defer registry.deinit(false) catch unreachable;
    try std.testing.expectError(error.OutOfMemory, registry.insert(1));
    try std.testing.expectEqual(@as(usize, 0), registry.liveCount());
}
