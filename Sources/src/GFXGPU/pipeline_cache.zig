const std = @import("std");
const sdl = @import("sdl.zig");
const formats = @import("formats.zig");
const key_mod = @import("pipeline_key.zig");
const layout_mod = @import("vertex_layout.zig");

pub const PipelineKey = key_mod.PipelineKey;

pub const Error = error{
    ShaderMissing,
    RequiredAttributeMissing,
    UnsupportedFormat,
    UnsupportedState,
    CreationFailed,
} || std.mem.Allocator.Error;

pub const CreateInfo = struct {
    key: PipelineKey,
    vertex_shader: ?*anyopaque,
    fragment_shader: ?*anyopaque,
    layout: layout_mod.VertexLayout,
    required_vertex_mask: u32,
};

pub const Api = struct {
    create: *const fn (*anyopaque, CreateInfo) ?*anyopaque,
    release: *const fn (*anyopaque, *anyopaque) void,
};

fn primitive(value: formats.Topology) ?sdl.c.SDL_GPUPrimitiveType {
    return switch (value) {
        .point_list => sdl.c.SDL_GPU_PRIMITIVETYPE_POINTLIST,
        .line_list => sdl.c.SDL_GPU_PRIMITIVETYPE_LINELIST,
        .line_strip => sdl.c.SDL_GPU_PRIMITIVETYPE_LINESTRIP,
        .triangle_list => sdl.c.SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .triangle_strip => sdl.c.SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
    };
}

fn compare(value: formats.Compare) sdl.c.SDL_GPUCompareOp {
    return switch (value) {
        .never => sdl.c.SDL_GPU_COMPAREOP_NEVER,
        .less => sdl.c.SDL_GPU_COMPAREOP_LESS,
        .equal => sdl.c.SDL_GPU_COMPAREOP_EQUAL,
        .less_equal => sdl.c.SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
        .greater => sdl.c.SDL_GPU_COMPAREOP_GREATER,
        .not_equal => sdl.c.SDL_GPU_COMPAREOP_NOT_EQUAL,
        .greater_equal => sdl.c.SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,
        .always => sdl.c.SDL_GPU_COMPAREOP_ALWAYS,
    };
}

fn blendFactor(value: formats.BlendFactor) ?sdl.c.SDL_GPUBlendFactor {
    return switch (value) {
        .zero => sdl.c.SDL_GPU_BLENDFACTOR_ZERO,
        .one => sdl.c.SDL_GPU_BLENDFACTOR_ONE,
        .source_color => sdl.c.SDL_GPU_BLENDFACTOR_SRC_COLOR,
        .inverse_source_color => sdl.c.SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
        .source_alpha => sdl.c.SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .inverse_source_alpha => sdl.c.SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .destination_alpha => sdl.c.SDL_GPU_BLENDFACTOR_DST_ALPHA,
        .inverse_destination_alpha => sdl.c.SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
        .destination_color => sdl.c.SDL_GPU_BLENDFACTOR_DST_COLOR,
        .inverse_destination_color => sdl.c.SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR,
    };
}

fn blendOp(value: formats.BlendOp) sdl.c.SDL_GPUBlendOp {
    return switch (value) {
        .add => sdl.c.SDL_GPU_BLENDOP_ADD,
        .subtract => sdl.c.SDL_GPU_BLENDOP_SUBTRACT,
        .reverse_subtract => sdl.c.SDL_GPU_BLENDOP_REVERSE_SUBTRACT,
        .minimum => sdl.c.SDL_GPU_BLENDOP_MIN,
        .maximum => sdl.c.SDL_GPU_BLENDOP_MAX,
    };
}

fn vertexFormat(value: layout_mod.AttributeFormat) sdl.c.SDL_GPUVertexElementFormat {
    return switch (value) {
        .float1 => sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
        .float2 => sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        .float3 => sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .float4 => sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        .normalized_u8x4 => sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
    };
}

fn textureFormat(value: u8, depth: bool) ?sdl.c.SDL_GPUTextureFormat {
    if (depth) return switch (value) { 80 => sdl.c.SDL_GPU_TEXTUREFORMAT_D16_UNORM, 75 => sdl.c.SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT, 71 => sdl.c.SDL_GPU_TEXTUREFORMAT_D32_FLOAT, else => null };
    return switch (value) { 6 => sdl.c.SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 32 => sdl.c.SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, else => null };
}

fn requiredBit(location: u8) u32 {
    return switch (location) { 0 => 1, 7 => 2, 1 => 4, 2 => 8, 3 => 16, 5 => 32, 6 => 64, else => 0 };
}

fn hasRequired(layout: layout_mod.VertexLayout, mask: u32) bool {
    var present: u32 = 0;
    for (layout.attributes[0..layout.attribute_count]) |attribute| present |= requiredBit(attribute.location);
    return present & mask == mask;
}

fn realCreate(device: *anyopaque, input: CreateInfo) ?*anyopaque {
    const color_format = textureFormat(input.key.color_format, false) orelse return null;
    const depth_format = textureFormat(input.key.depth_format, true);
    var buffers = [_]sdl.c.SDL_GPUVertexBufferDescription{.{ .slot = 0, .pitch = input.layout.stride, .input_rate = sdl.c.SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 }};
    var attributes: [16]sdl.c.SDL_GPUVertexAttribute = undefined;
    for (input.layout.attributes[0..input.layout.attribute_count], 0..) |attribute, index| {
        attributes[index] = .{ .location = attribute.location, .buffer_slot = 0, .format = vertexFormat(attribute.format), .offset = attribute.offset };
    }
    const blend = sdl.c.SDL_GPUColorTargetBlendState{ .src_color_blendfactor = blendFactor(input.key.blend_source).?, .dst_color_blendfactor = blendFactor(input.key.blend_destination).?, .color_blend_op = blendOp(input.key.blend_op), .src_alpha_blendfactor = blendFactor(input.key.blend_source).?, .dst_alpha_blendfactor = blendFactor(input.key.blend_destination).?, .alpha_blend_op = blendOp(input.key.blend_op), .color_write_mask = input.key.blend_write_mask, .enable_blend = input.key.blend_enable, .enable_color_write_mask = true, .padding1 = 0, .padding2 = 0 };
    const targets = [_]sdl.c.SDL_GPUColorTargetDescription{.{ .format = color_format, .blend_state = blend }};
    const info = sdl.c.SDL_GPUGraphicsPipelineCreateInfo{
        .vertex_shader = @ptrCast(@alignCast(input.vertex_shader.?)),
        .fragment_shader = @ptrCast(@alignCast(input.fragment_shader.?)),
        .vertex_input_state = .{ .vertex_buffer_descriptions = &buffers, .num_vertex_buffers = 1, .vertex_attributes = &attributes, .num_vertex_attributes = input.layout.attribute_count },
        .primitive_type = primitive(input.key.topology).?,
        .rasterizer_state = .{ .fill_mode = sdl.c.SDL_GPU_FILLMODE_FILL, .cull_mode = switch (input.key.cull) { .none => sdl.c.SDL_GPU_CULLMODE_NONE, else => sdl.c.SDL_GPU_CULLMODE_BACK }, .front_face = sdl.c.SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE, .depth_bias_constant_factor = 0, .depth_bias_clamp = 0, .depth_bias_slope_factor = 0, .enable_depth_bias = false, .enable_depth_clip = true, .padding1 = 0, .padding2 = 0 },
        .multisample_state = .{ .sample_count = @enumFromInt(input.key.sample_count), .sample_mask = 0, .enable_mask = false, .padding1 = 0, .padding2 = 0, .padding3 = 0 },
        .depth_stencil_state = .{ .compare_op = compare(input.key.depth_compare), .back_stencil_state = .{}, .front_stencil_state = .{}, .compare_mask = input.key.stencil_read_mask, .write_mask = input.key.stencil_write_mask, .enable_depth_test = input.key.depth_test, .enable_depth_write = input.key.depth_write, .enable_stencil_test = input.key.stencil_enable, .padding1 = 0, .padding2 = 0, .padding3 = 0 },
        .target_info = .{ .color_target_descriptions = &targets, .num_color_targets = input.key.attachment_count, .depth_stencil_format = depth_format orelse 0, .has_depth_stencil_target = depth_format != null, .padding1 = 0, .padding2 = 0, .padding3 = 0 },
        .props = 0,
    };
    return @ptrCast(sdl.c.SDL_CreateGPUGraphicsPipeline(@ptrCast(@alignCast(device)), &info));
}

fn realRelease(device: *anyopaque, pipeline: *anyopaque) void { sdl.c.SDL_ReleaseGPUGraphicsPipeline(@ptrCast(@alignCast(device)), @ptrCast(@alignCast(pipeline))); }
pub const real_api = Api{ .create = realCreate, .release = realRelease };

const Entry = struct { key: PipelineKey, handle: *anyopaque, last_use_serial: u64 };
const Context = struct { pub fn hash(_: @This(), key: PipelineKey) u64 { return key.hash(); } pub fn eql(_: @This(), a: PipelineKey, b: PipelineKey) bool { return std.meta.eql(a, b); } };

pub const Stats = struct { hits: u64 = 0, misses: u64 = 0, live: u32 = 0 };

pub const Cache = struct {
    allocator: std.mem.Allocator,
    device: *anyopaque,
    api: Api,
    entries: std.ArrayListUnmanaged(Entry) = .empty,
    index: std.HashMap(PipelineKey, u32, Context, 80) = .empty,
    stats: Stats = .{},

    pub fn init(allocator: std.mem.Allocator, device: *anyopaque, api: Api) Cache { return .{ .allocator = allocator, .device = device, .api = api }; }
    pub fn deinit(self: *Cache) void { for (self.entries.items) |entry| self.api.release(self.device, entry.handle); self.entries.deinit(self.allocator); self.index.deinit(self.allocator); self.* = undefined; }
    pub fn getOrCreate(self: *Cache, input: CreateInfo, serial: u64) Error!*anyopaque {
        if (input.vertex_shader == null or input.fragment_shader == null) return Error.ShaderMissing;
        if (!hasRequired(input.layout, input.required_vertex_mask)) return Error.RequiredAttributeMissing;
        if (textureFormat(input.key.color_format, false) == null or input.key.attachment_count != 1) return Error.UnsupportedFormat;
        if (input.key.sample_count != 1 and input.key.sample_count != 2 and input.key.sample_count != 4 and input.key.sample_count != 8) return Error.UnsupportedState;
        if (self.index.get(input.key)) |index| { self.stats.hits += 1; self.entries.items[index].last_use_serial = serial; return self.entries.items[index].handle; }
        self.stats.misses += 1;
        const handle = self.api.create(self.device, input) orelse return Error.CreationFailed;
        errdefer self.api.release(self.device, handle);
        const index: u32 = @intCast(self.entries.items.len);
        try self.entries.append(self.allocator, .{ .key = input.key, .handle = handle, .last_use_serial = serial });
        errdefer _ = self.entries.pop();
        try self.index.put(self.allocator, input.key, index);
        self.stats.live += 1;
        return handle;
    }
};

test "pipeline cache hits, misses on key changes, and tracks serials" {
    const ContextState = struct { creates: u32 = 0, releases: u32 = 0 };
    var context = ContextState{};
    const Fake = struct { var ctx: *ContextState = undefined; fn create(_: *anyopaque, _: CreateInfo) ?*anyopaque { ctx.creates += 1; return @ptrFromInt(ctx.creates); } fn release(_: *anyopaque, _: *anyopaque) void { ctx.releases += 1; } };
    Fake.ctx = &context;
    var cache = Cache.init(std.testing.allocator, @ptrFromInt(1), .{ .create = Fake.create, .release = Fake.release });
    defer cache.deinit();
    const state = @import("render_state.zig").State{ .color_format = 6 };
    var key = PipelineKey.fromState(state);
    const layout = try layout_mod.decodeFvf(0x02);
    const input = CreateInfo{ .key = key, .vertex_shader = @ptrFromInt(2), .fragment_shader = @ptrFromInt(3), .layout = layout, .required_vertex_mask = 1 };
    _ = try cache.getOrCreate(input, 4);
    _ = try cache.getOrCreate(input, 9);
    try std.testing.expectEqual(@as(u32, 1), context.creates);
    try std.testing.expectEqual(@as(u64, 1), cache.stats.hits);
    key.topology = .line_list;
    _ = try cache.getOrCreate(.{ .key = key, .vertex_shader = input.vertex_shader, .fragment_shader = input.fragment_shader, .layout = layout, .required_vertex_mask = 1 }, 10);
    try std.testing.expectEqual(@as(u32, 2), context.creates);
    try std.testing.expectEqual(@as(u32, 2), cache.stats.live);
}

test "pipeline cache rejects missing required attributes and shader handles" {
    var cache = Cache.init(std.testing.allocator, @ptrFromInt(1), .{ .create = struct { fn c(_: *anyopaque, _: CreateInfo) ?*anyopaque { return @ptrFromInt(1); } }.c, .release = struct { fn r(_: *anyopaque, _: *anyopaque) void {} }.r });
    defer cache.deinit();
    const input = CreateInfo{ .key = PipelineKey.fromState(.{ .color_format = 6 }), .vertex_shader = @ptrFromInt(2), .fragment_shader = @ptrFromInt(3), .layout = .{}, .required_vertex_mask = 1 };
    try std.testing.expectError(Error.RequiredAttributeMissing, cache.getOrCreate(input, 1));
    try std.testing.expectError(Error.ShaderMissing, cache.getOrCreate(.{ .key = input.key, .vertex_shader = null, .fragment_shader = input.fragment_shader, .layout = input.layout, .required_vertex_mask = 0 }, 1));
}

test "pipeline creation failure does not leak or increment live entries" {
    const ContextState = struct { releases: u32 = 0 };
    var context = ContextState{};
    const Fake = struct { var ctx: *ContextState = undefined; fn create(_: *anyopaque, _: CreateInfo) ?*anyopaque { return null; } fn release(_: *anyopaque, _: *anyopaque) void { ctx.releases += 1; } };
    Fake.ctx = &context;
    var cache = Cache.init(std.testing.allocator, @ptrFromInt(1), .{ .create = Fake.create, .release = Fake.release });
    defer cache.deinit();
    const layout = try layout_mod.decodeFvf(0x02);
    const input = CreateInfo{ .key = PipelineKey.fromState(.{ .color_format = 6 }), .vertex_shader = @ptrFromInt(2), .fragment_shader = @ptrFromInt(3), .layout = layout, .required_vertex_mask = 1 };
    try std.testing.expectError(Error.CreationFailed, cache.getOrCreate(input, 1));
    try std.testing.expectEqual(@as(u64, 1), cache.stats.misses);
    try std.testing.expectEqual(@as(u32, 0), cache.stats.live);
    try std.testing.expectEqual(@as(u32, 0), context.releases);
}
