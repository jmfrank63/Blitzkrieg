const std = @import("std");
const formats = @import("formats.zig");
const handles = @import("handles.zig");

pub const Error = error{ FrameNotRecording, PassMissing, PipelineMissing, BufferMissing, VertexRange, CountOverflow, RenderTargetFeedback, EncodeFailed };
pub const Dirty = struct { pipeline: bool = true, viewport: bool = true, vertex_buffers: bool = true, samplers: bool = true, uniforms: bool = true };

pub const DrawPlan = struct {
    topology: formats.Topology,
    first_vertex: u32,
    vertex_count: u32,
    primitive_count: u32,
    pipeline: handles.Handle,
    vertex_buffer: handles.Handle,
    vertex_offset: u64 = 0,
    serial: u64,
};

pub fn makePlan(frame_recording: bool, pass_active: bool, topology: formats.Topology, primitive_count: u32, first_vertex: u32, vertex_capacity: u32, pipeline: handles.Handle, vertex_buffer: handles.Handle, vertex_offset: u64, serial: u64, render_target: handles.Handle, sampled_texture: ?handles.Handle) Error!DrawPlan {
    if (!frame_recording) return Error.FrameNotRecording;
    if (!pass_active) return Error.PassMissing;
    if (pipeline == handles.invalid_handle) return Error.PipelineMissing;
    if (vertex_buffer == handles.invalid_handle) return Error.BufferMissing;
    const vertex_count = formats.primitiveVertexCount(topology, primitive_count) catch return Error.CountOverflow;
    if (first_vertex > vertex_capacity or vertex_count > vertex_capacity - first_vertex) return Error.VertexRange;
    if (sampled_texture) |texture| if (texture == render_target) return Error.RenderTargetFeedback;
    return .{ .topology = topology, .first_vertex = first_vertex, .vertex_count = vertex_count, .primitive_count = primitive_count, .pipeline = pipeline, .vertex_buffer = vertex_buffer, .vertex_offset = vertex_offset, .serial = serial };
}

pub const Api = struct {
    bind_pipeline: *const fn (*anyopaque, handles.Handle) bool,
    set_viewport: *const fn (*anyopaque) bool,
    bind_vertex_buffer: *const fn (*anyopaque, handles.Handle, u64) bool,
    bind_samplers: *const fn (*anyopaque) bool,
    push_uniforms: *const fn (*anyopaque) bool,
    draw: *const fn (*anyopaque, u32, u32) bool,
};

pub fn encode(context: *anyopaque, api: Api, plan: DrawPlan, dirty: *Dirty) Error!void {
    if (dirty.pipeline and !api.bind_pipeline(context, plan.pipeline)) return error.EncodeFailed;
    if (dirty.pipeline) dirty.pipeline = false;
    if (dirty.viewport and !api.set_viewport(context)) return error.EncodeFailed;
    if (dirty.viewport) dirty.viewport = false;
    if (dirty.vertex_buffers and !api.bind_vertex_buffer(context, plan.vertex_buffer, plan.vertex_offset)) return error.EncodeFailed;
    if (dirty.vertex_buffers) dirty.vertex_buffers = false;
    if (dirty.samplers and !api.bind_samplers(context)) return error.EncodeFailed;
    if (dirty.samplers) dirty.samplers = false;
    if (dirty.uniforms and !api.push_uniforms(context)) return error.EncodeFailed;
    if (dirty.uniforms) dirty.uniforms = false;
    if (!api.draw(context, plan.vertex_count, 1)) return error.EncodeFailed;
}

test "DrawPlan validates state, ranges, overflow, and feedback before encoding" {
    const pipeline: handles.Handle = 1;
    const buffer: handles.Handle = 2;
    try std.testing.expectError(Error.FrameNotRecording, makePlan(false, true, .triangle_list, 1, 0, 3, pipeline, buffer, 0, 1, 3, null));
    try std.testing.expectError(Error.PassMissing, makePlan(true, false, .triangle_list, 1, 0, 3, pipeline, buffer, 0, 1, 3, null));
    try std.testing.expectError(Error.VertexRange, makePlan(true, true, .triangle_list, 1, 2, 3, pipeline, buffer, 0, 1, 3, null));
    try std.testing.expectError(Error.RenderTargetFeedback, makePlan(true, true, .triangle_list, 1, 0, 3, pipeline, buffer, 0, 1, 3, 3));
    try std.testing.expectError(Error.CountOverflow, makePlan(true, true, .triangle_list, std.math.maxInt(u32), 0, std.math.maxInt(u32), pipeline, buffer, 0, 1, 3, null));
}

test "draw encoding follows call order and preserves dirty groups after failure" {
    const Context = struct { trace: [8]u8 = undefined, count: usize = 0, fail_at: u8 = 0 };
    var context = Context{};
    const Fake = struct {
        var ctx: *Context = undefined;
        fn mark(tag: u8) bool { ctx.trace[ctx.count] = tag; ctx.count += 1; return ctx.fail_at != tag; }
        fn pipeline(_: *anyopaque, _: handles.Handle) bool { return mark(1); }
        fn viewport(_: *anyopaque) bool { return mark(2); }
        fn buffer(_: *anyopaque, _: handles.Handle, _: u64) bool { return mark(3); }
        fn samplers(_: *anyopaque) bool { return mark(4); }
        fn uniforms(_: *anyopaque) bool { return mark(5); }
        fn draw(_: *anyopaque, _: u32, _: u32) bool { return mark(6); }
    };
    Fake.ctx = &context;
    const plan_value = try makePlan(true, true, .triangle_list, 1, 0, 3, 1, 2, 0, 7, 3, null);
    var dirty = Dirty{};
    try encode(@ptrCast(&context), .{ .bind_pipeline = Fake.pipeline, .set_viewport = Fake.viewport, .bind_vertex_buffer = Fake.buffer, .bind_samplers = Fake.samplers, .push_uniforms = Fake.uniforms, .draw = Fake.draw }, plan_value, &dirty);
    try std.testing.expectEqualSlices(u8, &[_]u8{ 1, 2, 3, 4, 5, 6 }, context.trace[0..6]);
    try std.testing.expect(!dirty.pipeline and !dirty.viewport and !dirty.vertex_buffers and !dirty.samplers and !dirty.uniforms);
    context = .{ .fail_at = 4 };
    dirty = .{};
    try std.testing.expectError(error.EncodeFailed, encode(@ptrCast(&context), .{ .bind_pipeline = Fake.pipeline, .set_viewport = Fake.viewport, .bind_vertex_buffer = Fake.buffer, .bind_samplers = Fake.samplers, .push_uniforms = Fake.uniforms, .draw = Fake.draw }, plan_value, &dirty));
    try std.testing.expect(dirty.samplers and dirty.uniforms);
}
