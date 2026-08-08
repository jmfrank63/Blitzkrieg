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
    const blend_count: u32 = switch (position) { 0x02, 0x04 => 0, 0x06 => 1, 0x08 => 2, 0x0a => 3, 0x0c => 4, 0x0e => 5, else => return LayoutError.UnsupportedPosition };
    if (blend_count > 4) return LayoutError.UnsupportedBlendWeights;
    // GFXFVF_XYZRHW is a pre-transformed position of four floats. The vertex
    // shaders declare `float3 position` and get w from the direct-transform
    // matrix, so the attribute stays float3 while the layout strides past 16
    // bytes. Reading only 12 here is what put every later semantic four bytes
    // early and made the terrain's colour and texcoords garbage.
    const position_bytes: u32 = if (position == 0x04) 16 else 12;
    var offset: u32 = 0;
    try layout.add(.position, 0, offset, .float3);
    offset += position_bytes;
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
        const dimension_code: u2 = @truncate(fvf >> @intCast(16 + index * 2));
        const components: u32 = switch (dimension_code) { 0 => 2, 1 => 3, 2 => 4, 3 => 1 };
        try layout.add(.texcoord, @intCast(index), offset, switch (components) { 1 => .float1, 2 => .float2, 3 => .float3, else => .float4 });
        offset += components * 4;
    }
    if (offset > std.math.maxInt(u16)) return LayoutError.StrideOverflow;
    layout.stride = @intCast(offset);
    return layout;
}

// The pipeline only declares the semantics its shader actually consumes, so it
// looks them up by name rather than by index. `location` is the legacy semantic
// number, which is not the shader's input location.
pub fn find(layout: VertexLayout, semantic: Semantic, location: u8) ?Attribute {
    for (layout.attributes[0..layout.attribute_count]) |attribute| {
        if (attribute.semantic == semantic and attribute.location == location) return attribute;
    }
    return null;
}

test "FVF golden layouts preserve legacy semantic order and stride" {
    // XYZ | NORMAL | DIFFUSE | TEX1 is 12 + 12 + 4 + 8. This asserted 32 while
    // the file did not compile, which is SGFXVertex's stride without the
    // diffuse this mask also sets.
    const layout = try decodeFvf(0x102 | 0x10 | 0x40 | 0x100);
    try std.testing.expectEqual(@as(u16, 36), layout.stride);
    try std.testing.expectEqual(@as(u8, 4), layout.attribute_count);
    try std.testing.expectEqual(Semantic.position, layout.attributes[0].semantic);
    try std.testing.expectEqual(@as(u16, 12), layout.attributes[1].offset);
    try std.testing.expectEqual(Semantic.normal, layout.attributes[1].semantic);
    try std.testing.expectEqual(Semantic.diffuse, layout.attributes[2].semantic);
    try std.testing.expectEqual(Semantic.texcoord, layout.attributes[3].semantic);
}

test "unsupported FVF position and dimensions are rejected" {
    try std.testing.expectError(LayoutError.UnsupportedBlendWeights, decodeFvf(0x0e));
}

// Every vertex format the engine actually draws with. The renderer used to
// hardcode a 32-byte pitch with the colour at 12 or 16, which is correct for
// exactly two of these; the rest were strided and swizzled as garbage. Terrain
// tiles (STerrainTLVertex) are the worst case at 40 bytes, so the GPU walked
// 32-byte steps through 40-byte records and rasterised nothing.
test "engine vertex formats decode to their true C++ strides" {
    const xyz = 0x002;
    const xyzrhw = 0x004;
    const normal = 0x010;
    const diffuse = 0x040;
    const specular = 0x080;
    const tex1 = 0x100;
    const tex2 = 0x200;

    // SGFXVertex: XYZ | NORMAL | TEX1, no diffuse at all.
    const mesh = try decodeFvf(xyz | normal | tex1);
    try std.testing.expectEqual(@as(u16, 32), mesh.stride);
    try std.testing.expectEqual(@as(u8, 3), mesh.attribute_count);
    try std.testing.expectEqual(Semantic.normal, mesh.attributes[1].semantic);
    try std.testing.expectEqual(Semantic.texcoord, mesh.attributes[2].semantic);
    try std.testing.expectEqual(@as(u16, 24), mesh.attributes[2].offset);

    // SGFXLVertex: XYZ | DIFFUSE | SPECULAR | TEX1.
    const lit = try decodeFvf(xyz | diffuse | specular | tex1);
    try std.testing.expectEqual(@as(u16, 28), lit.stride);
    try std.testing.expectEqual(@as(u16, 12), lit.attributes[1].offset);
    try std.testing.expectEqual(@as(u16, 20), lit.attributes[3].offset);

    // SGFXTLVertex: XYZRHW | DIFFUSE | SPECULAR | TEX1. Pre-transformed
    // position is four floats, so the colour sits at 16 and the texcoord at 24.
    const screen = try decodeFvf(xyzrhw | diffuse | specular | tex1);
    try std.testing.expectEqual(@as(u16, 32), screen.stride);
    try std.testing.expectEqual(Semantic.position, screen.attributes[0].semantic);
    try std.testing.expectEqual(@as(u16, 16), screen.attributes[1].offset);
    try std.testing.expectEqual(@as(u16, 24), screen.attributes[3].offset);

    // STerrainTLVertex: XYZRHW | DIFFUSE | TEX2, and no specular. 36 bytes
    // forced into a 32-byte stride is what left the terrain black.
    const terrain = try decodeFvf(xyzrhw | diffuse | tex2);
    try std.testing.expectEqual(@as(u16, 36), terrain.stride);
    try std.testing.expectEqual(@as(u8, 4), terrain.attribute_count);
    try std.testing.expectEqual(Semantic.diffuse, terrain.attributes[1].semantic);
    try std.testing.expectEqual(@as(u16, 16), terrain.attributes[1].offset);
    try std.testing.expectEqual(@as(u16, 20), terrain.attributes[2].offset);
    try std.testing.expectEqual(@as(u16, 28), terrain.attributes[3].offset);

    // SGFXLineVertex: XYZ | DIFFUSE, the smallest at 16 bytes.
    const line = try decodeFvf(xyz | diffuse);
    try std.testing.expectEqual(@as(u16, 16), line.stride);
    try std.testing.expectEqual(@as(u8, 2), line.attribute_count);

    // SGFXTLVertex2 and SGFXLVertex2 carry a second texcoord set.
    try std.testing.expectEqual(@as(u16, 40), (try decodeFvf(xyzrhw | diffuse | specular | tex2)).stride);
    try std.testing.expectEqual(@as(u16, 36), (try decodeFvf(xyz | diffuse | specular | tex2)).stride);
}

// The shaders declare `float3 position`, so a pre-transformed vertex must be
// described to the pipeline as three floats even though it occupies sixteen
// bytes. The rhw component is consumed by the direct-transform matrix instead.
test "pre-transformed position is declared as three floats but strides four" {
    const layout = try decodeFvf(0x004 | 0x040);
    try std.testing.expectEqual(AttributeFormat.float3, layout.attributes[0].format);
    try std.testing.expectEqual(@as(u16, 16), layout.attributes[1].offset);
    try std.testing.expectEqual(@as(u16, 20), layout.stride);
}

comptime { std.debug.assert(@sizeOf(u32) == 4); std.debug.assert(@sizeOf(AttributeFormat) == 1); }
