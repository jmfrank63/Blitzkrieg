const std = @import("std");
const formats = @import("formats.zig");

pub const Dirty = struct { pipeline: bool = true, bindings: bool = true, uniforms: bool = true };

pub const State = struct {
    effect: u32 = 0,
    topology: formats.Topology = .triangle_list,
    color_format: u8 = 0,
    depth_format: u8 = 0,
    sample_count: u8 = 1,
    cull: formats.Cull = .counter_clockwise,
    depth_test: bool = true,
    depth_write: bool = true,
    depth_compare: formats.Compare = .less_equal,
    blend_enable: bool = false,
    blend_source: formats.BlendFactor = .one,
    blend_destination: formats.BlendFactor = .zero,
    blend_op: formats.BlendOp = .add,
    blend_write_mask: u8 = 0x0f,
    stencil_enable: bool = false,
    stencil_read_mask: u8 = 0xff,
    stencil_write_mask: u8 = 0xff,
    alpha_test: formats.Compare = .always,
    attachment_count: u8 = 1,
    vertex_layout_digest: u64 = 0,
    dirty: Dirty = .{},

    pub fn setTopology(self: *State, topology: formats.Topology) void { if (self.topology != topology) { self.topology = topology; self.dirty.pipeline = true; } }
    pub fn setEffect(self: *State, effect: u32) void { if (self.effect != effect) { self.effect = effect; self.dirty.pipeline = true; } }
    pub fn setVertexLayout(self: *State, digest: u64) void { if (self.vertex_layout_digest != digest) { self.vertex_layout_digest = digest; self.dirty.pipeline = true; } }
    pub fn setViewport(self: *State) void { self.dirty.uniforms = true; }
    pub fn setTexture(self: *State) void { self.dirty.bindings = true; }
    pub fn setFog(self: *State) void { self.dirty.uniforms = true; }
    pub fn setBlendConstant(self: *State) void { self.dirty.uniforms = true; }
};

test "legacy default state is deterministic and dynamic values use specific dirty flags" {
    var state = State{};
    try std.testing.expect(state.depth_test and state.depth_write and !state.blend_enable);
    state.dirty = .{};
    state.setViewport();
    try std.testing.expect(state.dirty.uniforms and !state.dirty.pipeline and !state.dirty.bindings);
    state.dirty = .{};
    state.setTexture();
    try std.testing.expect(state.dirty.bindings and !state.dirty.pipeline);
    state.dirty = .{};
    state.setTopology(.line_list);
    try std.testing.expect(state.dirty.pipeline);
}
