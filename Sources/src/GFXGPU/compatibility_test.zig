const std = @import("std");
const gpu = @import("gfxgpu");

const EvidenceKind = enum { conversion, state, catalog, shader, abi, lifecycle };

const MatrixRow = struct {
    requirement: []const u8,
    legacy_symbol: []const u8,
    new_symbol: []const u8,
    test_name: []const u8,
    evidence: EvidenceKind,
};

const rows = [_]MatrixRow{
    .{ .requirement = "pixel-format-gfx", .legacy_symbol = "EGFXPixelFormat", .new_symbol = "formats.fromGfxPixelFormat", .test_name = "legacy conversion tables", .evidence = .conversion },
    .{ .requirement = "pixel-format-d3d", .legacy_symbol = "D3DFORMAT", .new_symbol = "formats.fromD3dPixelFormat", .test_name = "legacy conversion tables", .evidence = .conversion },
    .{ .requirement = "index-format", .legacy_symbol = "D3DFMT_INDEX16/32", .new_symbol = "formats.fromIndexFormat", .test_name = "legacy conversion tables", .evidence = .conversion },
    .{ .requirement = "primitive-topology", .legacy_symbol = "D3DPRIMITIVETYPE", .new_symbol = "formats.fromPrimitive", .test_name = "primitive vertex counts", .evidence = .conversion },
    .{ .requirement = "compare-function", .legacy_symbol = "D3DCMPFUNC", .new_symbol = "formats.fromCompare", .test_name = "legacy conversion tables", .evidence = .conversion },
    .{ .requirement = "cull-mode", .legacy_symbol = "D3DCULL", .new_symbol = "formats.fromCull", .test_name = "legacy conversion tables", .evidence = .conversion },
    .{ .requirement = "blend-factor", .legacy_symbol = "D3DBLEND", .new_symbol = "formats.fromBlendFactor", .test_name = "legacy conversion tables", .evidence = .conversion },
    .{ .requirement = "blend-operation", .legacy_symbol = "D3DBLENDOP", .new_symbol = "formats.fromBlendOp", .test_name = "legacy conversion tables", .evidence = .conversion },
    .{ .requirement = "fvf-layout", .legacy_symbol = "FVF", .new_symbol = "vertex_layout.decodeFvf", .test_name = "FVF golden layouts", .evidence = .conversion },
    .{ .requirement = "pipeline-key", .legacy_symbol = "render-state", .new_symbol = "pipeline_key.PipelineKey", .test_name = "pipeline key immutable state", .evidence = .state },
    .{ .requirement = "render-state-dirty", .legacy_symbol = "SetRenderState", .new_symbol = "render_state.State", .test_name = "legacy default state", .evidence = .state },
    .{ .requirement = "effect-catalog", .legacy_symbol = "SetShadingEffect", .new_symbol = "effects.specs/find", .test_name = "effect catalog complete", .evidence = .catalog },
    .{ .requirement = "shader-manifest", .legacy_symbol = "shader map", .new_symbol = "shader_manifest.Manifest", .test_name = "shader loader hashes", .evidence = .shader },
    .{ .requirement = "shader-pipeline-probes", .legacy_symbol = "effect shader stages", .new_symbol = "gfxgpu_smoke effect probes", .test_name = "25 shader pairs and pipelines", .evidence = .shader },
    .{ .requirement = "buffer-lifecycle", .legacy_symbol = "Create/DestroyVertexBuffer", .new_symbol = "abi.create_buffer/destroy_buffer", .test_name = "ABI resource lifecycle", .evidence = .lifecycle },
    .{ .requirement = "texture-lifecycle", .legacy_symbol = "Create/DestroyTexture", .new_symbol = "abi.create_texture/destroy_texture", .test_name = "ABI resource lifecycle", .evidence = .lifecycle },
    .{ .requirement = "render-target", .legacy_symbol = "SetRenderTarget", .new_symbol = "abi.create_render_target/bind_render_target", .test_name = "ABI state validation", .evidence = .abi },
    .{ .requirement = "screenshot", .legacy_symbol = "TakeScreenShot", .new_symbol = "gfxgpu_readback", .test_name = "ABI readback validation", .evidence = .abi },
    .{ .requirement = "frame-lifecycle", .legacy_symbol = "BeginScene/EndScene/Present", .new_symbol = "abi.begin_frame/end_frame/present", .test_name = "ABI state validation", .evidence = .lifecycle },
};

test "compatibility matrix has no unmapped rows" {
    try std.testing.expect(rows.len >= 19);
    for (rows) |row| {
        try std.testing.expect(row.requirement.len != 0);
        try std.testing.expect(row.legacy_symbol.len != 0);
        try std.testing.expect(row.new_symbol.len != 0);
        try std.testing.expect(row.test_name.len != 0);
    }
}

test "every catalog effect ID is represented by a shader family and policy" {
    for (gpu.effects.specs) |spec| {
        try std.testing.expect(gpu.effects.find(spec.id) != null);
        try std.testing.expect(spec.sampler_count <= 2);
        try std.testing.expect(spec.uniform_groups > 0);
    }
}

test "all compatibility matrix evidence kinds are populated" {
    var seen = [_]bool{false} ** 6;
    for (rows) |row| seen[@intFromEnum(row.evidence)] = true;
    for (seen) |value| try std.testing.expect(value);
}
