const std = @import("std");

pub const Release = struct { serial: u64, resource_id: u64 };
pub const Lifetime = struct {
    releases: std.ArrayListUnmanaged(Release) = .empty,
    allocator: std.mem.Allocator,
    next_submission: u64 = 1,
    completed: u64 = 0,

    pub fn init(allocator: std.mem.Allocator) Lifetime { return .{ .allocator = allocator }; }
    pub fn deinit(self: *Lifetime) void { self.releases.deinit(self.allocator); self.* = undefined; }
    pub fn submit(self: *Lifetime) u64 { const serial = self.next_submission; self.next_submission +%= 1; if (self.next_submission == 0) self.next_submission = 1; return serial; }
    pub fn deferRelease(self: *Lifetime, serial: u64, resource_id: u64) !void { try self.releases.append(self.allocator, .{ .serial = serial, .resource_id = resource_id }); }
    pub fn complete(self: *Lifetime, serial: u64) void { self.completed = @max(self.completed, serial); }
    pub fn drain(self: *Lifetime, callback: *const fn (u64) void) usize {
        var drained: usize = 0; var index: usize = self.releases.items.len;
        while (index != 0) { index -= 1; if (self.releases.items[index].serial <= self.completed) { callback(self.releases.items[index].resource_id); _ = self.releases.swapRemove(index); drained += 1; } }
        return drained;
    }
    pub fn shutdownDrain(self: *Lifetime, callback: *const fn (u64) void) void { var index = self.releases.items.len; while (index != 0) { index -= 1; callback(self.releases.items[index].resource_id); } self.releases.clearRetainingCapacity(); }
};

test "deferred releases respect completion serial boundaries" {
    var lifetime = Lifetime.init(std.testing.allocator); defer lifetime.deinit();
    const first = lifetime.submit(); const second = lifetime.submit();
    try lifetime.deferRelease(first, 10); try lifetime.deferRelease(second, 20);
    var released: u64 = 0;
    const Callback = struct { var target: *u64 = undefined; fn run(id: u64) void { target.* += id; } };
    Callback.target = &released;
    lifetime.complete(first); try std.testing.expectEqual(@as(usize, 1), lifetime.drain(Callback.run));
    try std.testing.expectEqual(@as(u64, 10), released);
    lifetime.complete(second); try std.testing.expectEqual(@as(usize, 1), lifetime.drain(Callback.run));
}
