const std = @import("std");

pub const Renderer = struct {
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) Renderer {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *Renderer) void {
        self.* = undefined;
    }
};
