const std = @import("std");
const formats = @import("formats.zig");
const handles = @import("handles.zig");
const transfer = @import("transfer.zig");

pub const Error = error{ FrameNotRecording, PassMissing, PipelineMissing, BufferMissing, VertexRange, CountOverflow, RenderTargetFeedback, EncodeFailed };
pub const IndexError = error{ IndexBufferMissing, InvalidIndexSize, IndexRange, InstanceCount, CountOverflow };
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

pub const IndexedPlan = struct { index_buffer: handles.Handle, index_size: u8, first_index: u32, index_count: u32, base_vertex: i32, instance_count: u32 = 1, serial: u64 };

pub fn makeIndexedPlan(index_buffer: handles.Handle, index_size: u8, first_index: u32, index_count: u32, index_capacity_bytes: u64, base_vertex: i32, instance_count: u32, serial: u64) IndexError!IndexedPlan {
    if (index_buffer == handles.invalid_handle) return IndexError.IndexBufferMissing;
    if (index_size != 2 and index_size != 4) return IndexError.InvalidIndexSize;
    if (instance_count != 1) return IndexError.InstanceCount;
    const byte_offset = std.math.mul(u64, first_index, index_size) catch return IndexError.CountOverflow;
    const byte_count = std.math.mul(u64, index_count, index_size) catch return IndexError.CountOverflow;
    if (byte_offset > index_capacity_bytes or byte_count > index_capacity_bytes - byte_offset) return IndexError.IndexRange;
    return .{ .index_buffer = index_buffer, .index_size = index_size, .first_index = first_index, .index_count = index_count, .base_vertex = base_vertex, .serial = serial };
}

pub const IndexedApi = struct {
    bind_index_buffer: *const fn (*anyopaque, handles.Handle, u8, u64) bool,
    draw_indexed: *const fn (*anyopaque, u32, u32, u32, i32, u32) bool,
};

pub fn encodeIndexed(context: *anyopaque, api: IndexedApi, plan: IndexedPlan) IndexError!void {
    const offset = std.math.mul(u64, plan.first_index, plan.index_size) catch return IndexError.CountOverflow;
    if (!api.bind_index_buffer(context, plan.index_buffer, plan.index_size, offset)) return error.IndexRange;
    if (!api.draw_indexed(context, plan.index_count, plan.instance_count, plan.first_index, plan.base_vertex, 0)) return error.IndexRange;
}

pub const TemporaryRing = struct {
    pool: transfer.Pool,
    pub fn init(allocator: std.mem.Allocator, size: usize) !TemporaryRing { return .{ .pool = try transfer.Pool.init(allocator, size) }; }
    pub fn deinit(self: *TemporaryRing) void { self.pool.deinit(); }
    pub fn beginFrame(self: *TemporaryRing) void { self.pool.reset(); }
    pub fn alloc(self: *TemporaryRing, bytes: usize, alignment: usize, serial: u64) !transfer.Allocation { return self.pool.alloc(bytes, alignment, serial); }
};

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

test "indexed plans validate 16/32-bit byte bounds and fixed instance count" {
    const plan16 = try makeIndexedPlan(1, 2, 2, 3, 12, -4, 1, 8);
    try std.testing.expectEqual(@as(u32, 2), plan16.first_index);
    const plan32 = try makeIndexedPlan(2, 4, 1, 2, 12, 3, 1, 9);
    try std.testing.expectEqual(@as(u8, 4), plan32.index_size);
    try std.testing.expectError(IndexError.InvalidIndexSize, makeIndexedPlan(1, 3, 0, 1, 4, 0, 1, 1));
    try std.testing.expectError(IndexError.IndexRange, makeIndexedPlan(1, 2, 4, 1, 8, 0, 1, 1));
    try std.testing.expectError(IndexError.InstanceCount, makeIndexedPlan(1, 2, 0, 1, 2, 0, 2, 1));
}

test "temporary ring grows and resets per frame" {
    var ring = try TemporaryRing.init(std.testing.allocator, 8);
    defer ring.deinit();
    _ = try ring.alloc(7, 1, 1);
    _ = try ring.alloc(9, 4, 1);
    try std.testing.expect(ring.pool.storage.len >= 20);
    ring.beginFrame();
    const allocation = try ring.alloc(4, 4, 2);
    try std.testing.expectEqual(@as(usize, 0), allocation.offset);
}
