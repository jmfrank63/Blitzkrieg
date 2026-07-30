const std = @import("std");
const manifest = @import("shader_manifest.zig");

pub const max_uniform_slots = 3;
pub const max_samplers = 2;
pub const max_storage_textures = 0;
pub const max_storage_buffers = 0;
pub const max_lights = 8;

pub const FrameUniforms = extern struct {
    view_proj: [16]f32 align(16),
    fog: [4]f32 align(16),
};

pub const DrawUniforms = extern struct {
    world: [16]f32 align(16),
    color: [4]f32 align(16),
};

pub const LightUniforms = extern struct {
    light_data: [32][4]f32 align(16),
};

pub const AttributeLocation = enum(u8) {
    position = 0,
    weights = 1,
    indices = 2,
    normal = 3,
    point_size = 4,
    diffuse = 5,
    specular = 6,
    uv0 = 7,
    uv1 = 8,
    uv2 = 9,
    uv3 = 10,
    uv4 = 11,
    uv5 = 12,
    uv6 = 13,
    uv7 = 14,
};

pub const UniformStage = enum { vertex, fragment };
pub const UniformError = error{ InvalidSlot, PayloadTooLarge, ResourceCountExceeded };
pub const PushUniformFn = *const fn (UniformStage, u8, []const u8) void;

pub fn pushUniform(stage: UniformStage, slot: u8, bytes: []const u8, push: PushUniformFn) UniformError!void {
    if (slot >= max_uniform_slots) return UniformError.InvalidSlot;
    if (bytes.len > @sizeOf(LightUniforms)) return UniformError.PayloadTooLarge;
    push(stage, slot, bytes);
}

pub fn validateResourceCounts(record: *const manifest.Record) UniformError!void {
    if (record.sampler_count > max_samplers or record.storage_texture_count > max_storage_textures or record.storage_buffer_count > max_storage_buffers or record.uniform_buffer_count > max_uniform_slots) return UniformError.ResourceCountExceeded;
}

fn writeFloat(bytes: []u8, offset: *usize, value: f32) void {
    var bits: [4]u8 = undefined;
    std.mem.writeInt(u32, &bits, @bitCast(value), .little);
    @memcpy(bytes[offset.* .. offset.* + bits.len], &bits);
    offset.* += bits.len;
}

fn writeFloats(comptime count: usize, bytes: []u8, offset: *usize, values: [count]f32) void {
    for (values) |value| writeFloat(bytes, offset, value);
}

pub fn serializeFrame(value: FrameUniforms) [@sizeOf(FrameUniforms)]u8 {
    var bytes: [@sizeOf(FrameUniforms)]u8 = undefined;
    var offset: usize = 0;
    writeFloats(16, &bytes, &offset, value.view_proj);
    writeFloats(4, &bytes, &offset, value.fog);
    return bytes;
}

pub fn serializeDraw(value: DrawUniforms) [@sizeOf(DrawUniforms)]u8 {
    var bytes: [@sizeOf(DrawUniforms)]u8 = undefined;
    var offset: usize = 0;
    writeFloats(16, &bytes, &offset, value.world);
    writeFloats(4, &bytes, &offset, value.color);
    return bytes;
}

pub fn serializeLights(value: LightUniforms) [@sizeOf(LightUniforms)]u8 {
    var bytes: [@sizeOf(LightUniforms)]u8 = undefined;
    var offset: usize = 0;
    for (value.light_data) |light| writeFloats(4, &bytes, &offset, light);
    return bytes;
}

comptime {
    std.debug.assert(@sizeOf(FrameUniforms) == 80);
    std.debug.assert(@alignOf(FrameUniforms) == 16);
    std.debug.assert(@offsetOf(FrameUniforms, "fog") == 64);
    std.debug.assert(@sizeOf(DrawUniforms) == 80);
    std.debug.assert(@alignOf(DrawUniforms) == 16);
    std.debug.assert(@offsetOf(DrawUniforms, "color") == 64);
    std.debug.assert(@sizeOf(LightUniforms) == 512);
    std.debug.assert(@alignOf(LightUniforms) == 16);
}

test "uniform serialization golden values" {
    const frame = serializeFrame(.{ .view_proj = [_]f32{1} ** 16, .fog = .{ 0.25, 0.5, 0.75, 0 } });
    try std.testing.expectEqual(@as(u8, 0), frame[0]);
    try std.testing.expectEqual(@as(u8, 0x3f), frame[67]);
    const draw = serializeDraw(.{ .world = [_]f32{0} ** 16, .color = .{ 1, 0.5, 0, 1 } });
    try std.testing.expectEqual(@as(u8, 0x3f), draw[66]);
    const lights = serializeLights(.{ .light_data = [_][4]f32{.{ 1, 2, 3, 4 }} ** 32 });
    try std.testing.expectEqual(@as(u8, 0x80), lights[3]);
}

test "uniform pushes are bounded to three slots" {
    const Fake = struct { fn push(_: UniformStage, _: u8, _: []const u8) void {} };
    try pushUniform(.vertex, 0, &[_]u8{1}, Fake.push);
    try pushUniform(.fragment, 2, &[_]u8{1}, Fake.push);
    try std.testing.expectError(UniformError.InvalidSlot, pushUniform(.vertex, 3, &[_]u8{1}, Fake.push));
    try std.testing.expectError(UniformError.PayloadTooLarge, pushUniform(.fragment, 0, &([@sizeOf(LightUniforms) + 1]u8{0} ** (@sizeOf(LightUniforms) + 1)), Fake.push));
}
