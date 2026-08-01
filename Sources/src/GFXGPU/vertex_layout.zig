const std = @import("std");

pub const LayoutError = error{ UnsupportedPosition, UnsupportedBlendWeights, AttributeLimit, InvalidTextureDimensions, StrideOverflow };
pub const Semantic = enum(u8) { position, blend_weight, blend_index, normal, point_size, diffuse, specular, texcoord };
pub const AttributeFormat = enum(u8) { float1, float2, float3, float4, normalized_u8x4 };
pub const Attribute = struct { semantic: Semantic, location: u8, offset: u16, format: AttributeFormat };
pub const VertexLayout = struct {
    attributes: [16]Attribute = undefined,
    attribute_count: u8 = 0,
    stride: u16 = 0,

    fn add(self: *VertexLayout, semantic: Semantic, location: u8, offset: u32, format: AttributeFormat) LayoutError!void {
        if (self.attribute_count >= self.attributes.len) return LayoutError.AttributeLimit;
        if (offset > std.math.maxInt(u16)) return LayoutError.StrideOverflow;
        self.attributes[self.attribute_count] = .{ .semantic = semantic, .location = location, .offset = @intCast(offset), .format = format };
        self.attribute_count += 1;
    }
};

pub fn decodeFvf(fvf: u32) LayoutError!VertexLayout {
    var layout = VertexLayout{};
    const position = fvf & 0x0f;
    if (position == 0x04) return LayoutError.UnsupportedPosition;
    const blend_count: u32 = switch (position) { 0x02 => 0, 0x06 => 1, 0x08 => 2, 0x0a => 3, 0x0c => 4, 0x0e => 5, else => return LayoutError.UnsupportedPosition };
    if (blend_count > 4) return LayoutError.UnsupportedBlendWeights;
    var offset: u32 = 0;
    try layout.add(.position, 0, offset, .float3);
    offset += 12;
    if (blend_count != 0) {
        try layout.add(.blend_weight, 1, offset, switch (blend_count) { 1 => .float1, 2 => .float2, 3 => .float3, else => .float4 });
        offset += blend_count * 4;
        if ((fvf & 0x1000) != 0) { try layout.add(.blend_index, 2, offset, .normalized_u8x4); offset += 4; }
    }
    if ((fvf & 0x10) != 0) { try layout.add(.normal, 3, offset, .float3); offset += 12; }
    if ((fvf & 0x20) != 0) { try layout.add(.point_size, 4, offset, .float1); offset += 4; }
    if ((fvf & 0x40) != 0) { try layout.add(.diffuse, 5, offset, .normalized_u8x4); offset += 4; }
    if ((fvf & 0x80) != 0) { try layout.add(.specular, 6, offset, .normalized_u8x4); offset += 4; }
    const tex_count = (fvf >> 8) & 0x0f;
    if (tex_count > 8) return LayoutError.AttributeLimit;
    for (0..tex_count) |index| {
        const dimension_code = (fvf >> @intCast(16 + index * 2)) & 3;
        const components: u32 = switch (dimension_code) { 0 => 2, 1 => 3, 2 => 4, 3 => 1 };
        try layout.add(.texcoord, @intCast(index), offset, switch (components) { 1 => .float1, 2 => .float2, 3 => .float3, else => .float4 });
        offset += components * 4;
    }
    if (offset > std.math.maxInt(u16)) return LayoutError.StrideOverflow;
    layout.stride = @intCast(offset);
    return layout;
}

test "FVF golden layouts preserve legacy semantic order and stride" {
    const layout = try decodeFvf(0x102 | 0x10 | 0x40 | 0x100);
    try std.testing.expectEqual(@as(u16, 32), layout.stride);
    try std.testing.expectEqual(@as(u8, 4), layout.attribute_count);
    try std.testing.expectEqual(Semantic.position, layout.attributes[0].semantic);
    try std.testing.expectEqual(@as(u16, 12), layout.attributes[1].offset);
    try std.testing.expectEqual(Semantic.normal, layout.attributes[1].semantic);
    try std.testing.expectEqual(Semantic.diffuse, layout.attributes[2].semantic);
    try std.testing.expectEqual(Semantic.texcoord, layout.attributes[3].semantic);
}

test "unsupported FVF position and dimensions are rejected" {
    try std.testing.expectError(LayoutError.UnsupportedPosition, decodeFvf(0x04));
    try std.testing.expectError(LayoutError.UnsupportedBlendWeights, decodeFvf(0x0e));
}

comptime { std.debug.assert(@sizeOf(u32) == 4); std.debug.assert(@sizeOf(AttributeFormat) == 1); }
