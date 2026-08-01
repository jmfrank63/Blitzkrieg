const std = @import("std");

pub const ConversionError = error{ UnsupportedValue, Overflow };

pub const PixelFormat = enum(u8) { dxt1, dxt3, dxt5, a8, r8, rg8, rgba8, bgra8, depth16, depth24_stencil8, depth32 };
pub const IndexFormat = enum(u8) { u16, u32 };
pub const Topology = enum(u8) { point_list, line_list, line_strip, triangle_list, triangle_strip };
pub const Compare = enum(u8) { never, less, equal, less_equal, greater, not_equal, greater_equal, always };
pub const Cull = enum(u8) { none, clockwise, counter_clockwise };
pub const BlendFactor = enum(u8) { zero, one, source_color, inverse_source_color, source_alpha, inverse_source_alpha, destination_alpha, inverse_destination_alpha, destination_color, inverse_destination_color };
pub const BlendOp = enum(u8) { add, subtract, reverse_subtract, minimum, maximum };

pub fn formatUnsupported(category: []const u8, value: u32, buffer: []u8) []const u8 {
    const written = std.fmt.bufPrint(buffer, "{s}: unsupported value {}", .{ category, value }) catch buffer;
    return written;
}

pub fn fromGfxPixelFormat(value: u32) ConversionError!PixelFormat {
    return switch (value) {
        1 => .dxt1,
        3 => .dxt3,
        5 => .dxt5,
        6 => .rgba8,
        else => ConversionError.UnsupportedValue,
    };
}

pub fn fromD3dPixelFormat(value: u32) ConversionError!PixelFormat {
    return switch (value) {
        28 => .a8,
        21 => .rgba8,
        32 => .bgra8,
        75 => .depth24_stencil8,
        80 => .depth16,
        71 => .depth32,
        else => ConversionError.UnsupportedValue,
    };
}

pub fn fromIndexFormat(value: u32) ConversionError!IndexFormat {
    return switch (value) { 101 => .u16, 102 => .u32, else => ConversionError.UnsupportedValue };
}

pub fn fromPrimitive(value: u32) ConversionError!Topology {
    return switch (value) { 1 => .point_list, 2 => .line_list, 3 => .line_strip, 4 => .triangle_list, 5 => .triangle_strip, else => ConversionError.UnsupportedValue };
}

pub fn fromCompare(value: u32) ConversionError!Compare {
    return switch (value) { 1 => .never, 2 => .less, 3 => .equal, 4 => .less_equal, 5 => .greater, 6 => .not_equal, 7 => .greater_equal, 8 => .always, else => ConversionError.UnsupportedValue };
}

pub fn fromCull(value: u32) ConversionError!Cull {
    return switch (value) { 1 => .none, 2 => .clockwise, 3 => .counter_clockwise, else => ConversionError.UnsupportedValue };
}

pub fn fromBlendFactor(value: u32) ConversionError!BlendFactor {
    return switch (value) { 1 => .zero, 2 => .one, 3 => .source_color, 4 => .inverse_source_color, 5 => .source_alpha, 6 => .inverse_source_alpha, 7 => .destination_alpha, 8 => .inverse_destination_alpha, 9 => .destination_color, 10 => .inverse_destination_color, else => ConversionError.UnsupportedValue };
}

pub fn fromBlendOp(value: u32) ConversionError!BlendOp {
    return switch (value) { 1 => .add, 2 => .subtract, 3 => .reverse_subtract, 4 => .minimum, 5 => .maximum, else => ConversionError.UnsupportedValue };
}

pub fn primitiveVertexCount(topology: Topology, primitive_count: u32) ConversionError!u32 {
    const extra: u32 = switch (topology) { .point_list => 0, .line_list => 0, .line_strip => 1, .triangle_list => 0, .triangle_strip => 2 };
    const multiplier: u32 = switch (topology) { .point_list => 1, .line_list => 2, .line_strip => 1, .triangle_list => 3, .triangle_strip => 1 };
    const multiplied = std.math.mul(u32, primitive_count, multiplier) catch return ConversionError.Overflow;
    return std.math.add(u32, multiplied, extra) catch return ConversionError.Overflow;
}

test "legacy conversion tables cover supported values and reject unknowns" {
    const pixel_values = [_]u32{ 1, 3, 5, 6 };
    for (pixel_values) |value| _ = try fromGfxPixelFormat(value);
    for ([_]u32{ 28, 21, 32, 75, 80, 71 }) |value| _ = try fromD3dPixelFormat(value);
    for ([_]u32{ 101, 102 }) |value| _ = try fromIndexFormat(value);
    for ([_]u32{ 1, 2, 3, 4, 5 }) |value| _ = try fromPrimitive(value);
    for ([_]u32{ 1, 2, 3, 4, 5, 6, 7, 8 }) |value| _ = try fromCompare(value);
    try std.testing.expectError(ConversionError.UnsupportedValue, fromPrimitive(99));
    try std.testing.expectError(ConversionError.UnsupportedValue, fromD3dPixelFormat(999));
}

test "primitive vertex counts preserve list and strip rules with overflow checks" {
    try std.testing.expectEqual(@as(u32, 6), try primitiveVertexCount(.triangle_list, 2));
    try std.testing.expectEqual(@as(u32, 5), try primitiveVertexCount(.triangle_strip, 3));
    try std.testing.expectEqual(@as(u32, 4), try primitiveVertexCount(.line_strip, 3));
    try std.testing.expectError(ConversionError.Overflow, primitiveVertexCount(.triangle_list, std.math.maxInt(u32)));
}

test "unsupported conversion diagnostics include category and value" {
    var buffer: [64]u8 = undefined;
    const message = formatUnsupported("pixel", 999, &buffer);
    try std.testing.expectEqualStrings("pixel: unsupported value 999", message);
}
