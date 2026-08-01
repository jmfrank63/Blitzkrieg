const std = @import("std");
const formats = @import("formats.zig");
const state_mod = @import("render_state.zig");

pub const PipelineKey = struct {
    effect: u32,
    vertex_layout_digest: u64,
    topology: formats.Topology,
    color_format: u8,
    depth_format: u8,
    sample_count: u8,
    cull: formats.Cull,
    depth_test: bool,
    depth_write: bool,
    depth_compare: formats.Compare,
    stencil_enable: bool,
    stencil_read_mask: u8,
    stencil_write_mask: u8,
    blend_enable: bool,
    blend_source: formats.BlendFactor,
    blend_destination: formats.BlendFactor,
    blend_op: formats.BlendOp,
    blend_write_mask: u8,
    alpha_test: formats.Compare,
    attachment_count: u8,

    pub fn fromState(state: state_mod.State) PipelineKey {
        return .{
            .effect = state.effect,
            .vertex_layout_digest = state.vertex_layout_digest,
            .topology = state.topology,
            .color_format = state.color_format,
            .depth_format = state.depth_format,
            .sample_count = state.sample_count,
            .cull = state.cull,
            .depth_test = state.depth_test,
            .depth_write = state.depth_write,
            .depth_compare = if (state.depth_test) state.depth_compare else .always,
            .stencil_enable = state.stencil_enable,
            .stencil_read_mask = if (state.stencil_enable) state.stencil_read_mask else 0,
            .stencil_write_mask = if (state.stencil_enable) state.stencil_write_mask else 0,
            .blend_enable = state.blend_enable,
            .blend_source = if (state.blend_enable) state.blend_source else .one,
            .blend_destination = if (state.blend_enable) state.blend_destination else .zero,
            .blend_op = if (state.blend_enable) state.blend_op else .add,
            .blend_write_mask = if (state.blend_enable) state.blend_write_mask else 0x0f,
            .alpha_test = state.alpha_test,
            .attachment_count = state.attachment_count,
        };
    }

    pub fn hash(self: PipelineKey) u64 {
        var hasher = std.hash.Wyhash.init(0);
        hasher.update(std.mem.asBytes(&self.effect));
        hasher.update(std.mem.asBytes(&self.vertex_layout_digest));
        hasher.update(&.{ @intFromEnum(self.topology), self.color_format, self.depth_format, self.sample_count, @intFromEnum(self.cull), @intFromBool(self.depth_test), @intFromBool(self.depth_write), @intFromEnum(self.depth_compare), @intFromBool(self.stencil_enable), self.stencil_read_mask, self.stencil_write_mask, @intFromBool(self.blend_enable), @intFromEnum(self.blend_source), @intFromEnum(self.blend_destination), @intFromEnum(self.blend_op), self.blend_write_mask, @intFromEnum(self.alpha_test), self.attachment_count });
        return hasher.final();
    }
};

test "pipeline key ignores dynamic state and changes for immutable state" {
    var state = state_mod.State{};
    const base = PipelineKey.fromState(state);
    state.setViewport(); state.setTexture(); state.setFog(); state.setBlendConstant();
    try std.testing.expectEqual(base, PipelineKey.fromState(state));
    state.setTopology(.line_strip);
    try std.testing.expect(base != PipelineKey.fromState(state));
    try std.testing.expect(base.hash() != PipelineKey.fromState(state).hash());
    state = state_mod.State{};
    state.blend_enable = true;
    try std.testing.expect(base != PipelineKey.fromState(state));
}
