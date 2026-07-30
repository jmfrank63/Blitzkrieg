const std = @import("std");
const device_mod = @import("device.zig");
const frame_mod = @import("frame.zig");

pub const Renderer = struct {
    allocator: std.mem.Allocator,
    device: ?device_mod.Device = null,
    frame: frame_mod.Frame = .{},

    pub const LiveCounts = struct {
        textures: u32 = 0,
        buffers: u32 = 0,
        samplers: u32 = 0,
        render_targets: u32 = 0,
        shaders: u32 = 0,
        pipelines: u32 = 0,
    };

    pub fn init(allocator: std.mem.Allocator) Renderer {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *Renderer) void {
        if (self.device) |*device| device.deinit();
        self.* = undefined;
    }
};
