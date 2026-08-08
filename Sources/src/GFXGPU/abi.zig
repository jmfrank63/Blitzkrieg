const std = @import("std");
const libc = @cImport({
    @cInclude("stdlib.h");
});
const errors = @import("error.zig");
const renderer_mod = @import("renderer.zig");
const device_mod = @import("device.zig");
const sdl = @import("sdl.zig");

pub const abi_version: u32 = 1;
pub const RendererHandle = opaque {};
pub const Result = errors.Result;

pub const CreateInfo = extern struct {
    struct_size: u32,
    flags: u32,
    sdl_window: ?*anyopaque,
    width: u32,
    height: u32,
    shader_directory_utf8: ?[*:0]const u8,
    preferred_driver_utf8: ?[*:0]const u8,
};

pub const LiveCounts = extern struct {
    struct_size: u32,
    textures: u32,
    buffers: u32,
    samplers: u32,
    render_targets: u32,
};
pub const ClearInfo = extern struct { struct_size: u32, mask: u32, color_rgba8: u32, depth: f32, stencil: u32 };
pub const ViewportInfo = extern struct { struct_size: u32, x: f32, y: f32, width: f32, height: f32, min_depth: f32, max_depth: f32 };
pub const MatrixInfo = extern struct { struct_size: u32, values: [16]f32 };
pub const TemporaryGeometryInfo = extern struct { struct_size: u32, data: ?*const anyopaque, byte_length: u32, stride: u32 };
pub const StateInfo = extern struct { struct_size: u32, kind: u32, index: u32, value: u32, values: [16]f32 };
pub const TextureCreateInfo = extern struct { struct_size: u32, width: u32, height: u32, mip_count: u32, format: u32, usage: u32 };
pub const TextureUploadInfo = extern struct { struct_size: u32, data: ?*const anyopaque, byte_length: u32, row_pitch: u32, mip_level: u32 };
pub const RenderTargetCreateInfo = extern struct { struct_size: u32, width: u32, height: u32, format: u32 };
pub const BufferCreateInfo = extern struct { struct_size: u32, element_count: u32, format: u32, stride: u32, usage: u32 };
pub const BufferUploadInfo = extern struct { struct_size: u32, data: ?*const anyopaque, byte_length: u32, byte_offset: u32 };
pub const ReadbackInfo = extern struct { struct_size: u32, width: u32, height: u32, byte_length: u32, row_pitch: u32, data: ?*anyopaque };

pub const Api = extern struct {
    abi_version: u32,
    struct_size: u32,
    create: *const fn (?*const CreateInfo, *?*RendererHandle) callconv(.c) Result,
    destroy: *const fn (?*RendererHandle) callconv(.c) void,
    get_last_error: *const fn (?*RendererHandle, ?[*]u8, u32, ?*u32) callconv(.c) Result,
    get_live_counts: *const fn (?*RendererHandle, ?*LiveCounts) callconv(.c) Result,
    begin_frame: *const fn (?*RendererHandle) callconv(.c) Result,
    end_frame: *const fn (?*RendererHandle) callconv(.c) Result,
    present: *const fn (?*RendererHandle) callconv(.c) Result,
    cancel_frame: *const fn (?*RendererHandle) callconv(.c) void,
    clear: *const fn (?*RendererHandle, ?*const ClearInfo) callconv(.c) Result,
    resize: *const fn (?*RendererHandle, u32, u32) callconv(.c) Result,
    set_viewport: *const fn (?*RendererHandle, ?*const ViewportInfo) callconv(.c) Result,
    set_transform: *const fn (?*RendererHandle, ?*const MatrixInfo, ?*const MatrixInfo) callconv(.c) Result,
    set_color: *const fn (?*RendererHandle, u32) callconv(.c) Result,
    set_fog: *const fn (?*RendererHandle, u32) callconv(.c) Result,
    set_state: *const fn (?*RendererHandle, ?*const StateInfo) callconv(.c) Result,
    create_texture: *const fn (?*RendererHandle, ?*const TextureCreateInfo, ?*u64) callconv(.c) Result,
    upload_texture: *const fn (?*RendererHandle, u64, ?*const TextureUploadInfo) callconv(.c) Result,
    destroy_texture: *const fn (?*RendererHandle, u64) callconv(.c) Result,
    create_render_target: *const fn (?*RendererHandle, ?*const RenderTargetCreateInfo, ?*u64) callconv(.c) Result,
    bind_render_target: *const fn (?*RendererHandle, u64) callconv(.c) Result,
    create_buffer: *const fn (?*RendererHandle, ?*const BufferCreateInfo, ?*u64) callconv(.c) Result,
    upload_buffer: *const fn (?*RendererHandle, u64, ?*const BufferUploadInfo) callconv(.c) Result,
    destroy_buffer: *const fn (?*RendererHandle, u64) callconv(.c) Result,
    set_texture: *const fn (?*RendererHandle, u64) callconv(.c) Result,
    set_sampler: *const fn (?*RendererHandle, u64) callconv(.c) Result,
    draw: *const fn (?*RendererHandle, u32, u32) callconv(.c) Result,
    draw_indexed: *const fn (?*RendererHandle, u64, u32, u32, u32, i32) callconv(.c) Result,
    draw_temporary: *const fn (?*RendererHandle, ?*const TemporaryGeometryInfo, u32) callconv(.c) Result,
    bind_vertex_buffer: *const fn (?*RendererHandle, u64) callconv(.c) Result,
};

fn create(info: ?*const CreateInfo, out_renderer: ?*?*RendererHandle) callconv(.c) Result {
    if (info == null or out_renderer == null) return errors.invalid_argument;
    const create_info = info.?.*;
    if (create_info.struct_size < @sizeOf(CreateInfo) or create_info.width == 0 or create_info.height == 0)
        return errors.invalid_argument;
    const raw_state = libc.malloc(@sizeOf(renderer_mod.Renderer)) orelse return errors.out_of_memory;
    const state: *renderer_mod.Renderer = @ptrCast(@alignCast(raw_state));
    state.* = renderer_mod.Renderer.init(std.heap.c_allocator);
    if ((create_info.flags & 2) == 0) {
        const shader_formats: u32 = @intCast(sdl.shaderformat_dxil | sdl.shaderformat_spirv | sdl.shaderformat_msl);
        state.device = device_mod.Device.init(std.heap.c_allocator, device_mod.real_api, shader_formats, (create_info.flags & 1) != 0, create_info.preferred_driver_utf8) catch {
            state.deinit();
            libc.free(state);
            return errors.sdl_error;
        };
        const shader_directory = create_info.shader_directory_utf8 orelse "zig-out/shaders";
        state.setShaderDirectory(shader_directory) catch {
            state.deinit();
            libc.free(state);
            return errors.out_of_memory;
        };
        state.attachWindow(create_info.sdl_window, create_info.width, create_info.height) catch {
            state.deinit();
            libc.free(state);
            return errors.sdl_error;
        };
    }
    out_renderer.?.* = @ptrCast(state);
    return errors.ok;
}

fn destroy(handle: ?*RendererHandle) callconv(.c) void {
    if (handle) |value| {
        const state: *renderer_mod.Renderer = @ptrCast(@alignCast(value));
        if (state.device == null) {
            libc.free(state);
            return;
        }
        state.deinit();
        libc.free(state);
    }
}

fn getLastError(handle: ?*RendererHandle, destination: ?[*]u8, capacity: u32, written: ?*u32) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    const message = if (renderer.last_error.len > 0) renderer.last_error else sdl.getError();
    return errors.copyBounded(message, destination, capacity, written);
}

fn getLiveCounts(handle: ?*RendererHandle, counts: ?*LiveCounts) callconv(.c) Result {
    if (handle == null or counts == null) return errors.invalid_argument;
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (counts.?.struct_size < @sizeOf(LiveCounts)) return errors.invalid_argument;
    counts.?.textures = @intCast(renderer.textures.count());
    counts.?.buffers = @intCast(renderer.buffers.count());
    counts.?.samplers = 0;
    counts.?.render_targets = 0;
    return errors.ok;
}

fn withRenderer(handle: ?*RendererHandle) ?*renderer_mod.Renderer {
    const value = handle orelse return null;
    return @ptrCast(@alignCast(value));
}
fn beginFrame(handle: ?*RendererHandle) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    const acquired = renderer.beginFrame() catch |err| {
        renderer.last_error = @errorName(err);
        return errors.invalid_state;
    };
    renderer.last_error = @tagName(renderer.frame.state);
    return if (acquired) errors.ok else errors.invalid_state;
}
fn endFrame(handle: ?*RendererHandle) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    renderer.endFrame() catch return errors.invalid_state;
    return errors.ok;
}
fn present(handle: ?*RendererHandle) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    renderer.present() catch return errors.invalid_state;
    return errors.ok;
}
fn cancelFrame(handle: ?*RendererHandle) callconv(.c) void {
    if (withRenderer(handle)) |renderer| renderer.cancelFrame();
}
fn clear(handle: ?*RendererHandle, info: ?*const ClearInfo) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or info.?.struct_size < @sizeOf(ClearInfo)) return errors.invalid_argument;
    if (renderer.frame.state != .recording) {
        renderer.last_error = @tagName(renderer.frame.state);
        return errors.invalid_state;
    }
    // The engine passes its D3DCOLOR (0xAARRGGBB) through unchanged.
    const color = info.?.color_rgba8;
    renderer.clear(.{
        @as(f32, @floatFromInt((color >> 16) & 0xff)) / 255.0,
        @as(f32, @floatFromInt((color >> 8) & 0xff)) / 255.0,
        @as(f32, @floatFromInt((color >> 0) & 0xff)) / 255.0,
        @as(f32, @floatFromInt((color >> 24) & 0xff)) / 255.0,
    }) catch |err| {
        renderer.last_error = @errorName(err);
        return errors.invalid_state;
    };
    return errors.ok;
}
fn resize(handle: ?*RendererHandle, width: u32, height: u32) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (width == 0 or height == 0) return errors.invalid_argument;
    renderer.resize(width, height) catch |err| return switch (err) {
        error.InvalidViewport, error.InvalidState => errors.invalid_state,
        error.NoDevice, error.SceneTextureCreateFailed, error.DepthTextureCreateFailed => errors.sdl_error,
    };
    return errors.ok;
}
fn setViewport(handle: ?*RendererHandle, viewport: ?*const ViewportInfo) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (viewport == null or viewport.?.struct_size < @sizeOf(ViewportInfo)) return errors.invalid_argument;
    if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state;
    renderer.setViewport(.{ .x = viewport.?.x, .y = viewport.?.y, .width = viewport.?.width, .height = viewport.?.height, .min_depth = viewport.?.min_depth, .max_depth = viewport.?.max_depth }) catch |err| return switch (err) {
        error.InvalidState, error.InvalidViewport => errors.invalid_state,
    };
    return errors.ok;
}
fn setTransform(handle: ?*RendererHandle, world: ?*const MatrixInfo, view_proj: ?*const MatrixInfo) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (world == null or view_proj == null or world.?.struct_size < @sizeOf(MatrixInfo) or view_proj.?.struct_size < @sizeOf(MatrixInfo)) return errors.invalid_argument;
    if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state;
    renderer.world_matrix = world.?.values;
    renderer.view_proj_matrix = view_proj.?.values;
    return errors.ok;
}
fn setColor(handle: ?*RendererHandle, color: u32) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state;
    // D3DCOLOR is 0xAARRGGBB: red lives in bits 16-23, blue in bits 0-7.
    renderer.draw_color = .{ @as(f32, @floatFromInt((color >> 16) & 0xff)) / 255.0, @as(f32, @floatFromInt((color >> 8) & 0xff)) / 255.0, @as(f32, @floatFromInt(color & 0xff)) / 255.0, @as(f32, @floatFromInt((color >> 24) & 0xff)) / 255.0 };
    return errors.ok;
}
fn setFog(handle: ?*RendererHandle, _: u32) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state;
    return errors.ok;
}
const state_shade_effect: u32 = 8; // GFXGPU_STATE_SHADE_EFFECT
fn setState(handle: ?*RendererHandle, info: ?*const StateInfo) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or info.?.struct_size < @sizeOf(StateInfo)) return errors.invalid_argument;
    if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state;
    // This validated every state and then discarded it, so SHADE_EFFECT,
    // DEPTH_MODE, LIGHTING and the rest were silently dropped. Record the
    // shading effect at least: it selects the blend mode, without which
    // alpha-blended draws such as the war fog paint over the whole screen.
    if (info.?.kind == state_shade_effect) renderer.shade_effect = info.?.value;
    return errors.ok;
}
fn createTexture(handle: ?*RendererHandle, info: ?*const TextureCreateInfo, out_handle: ?*u64) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or out_handle == null or info.?.struct_size < @sizeOf(TextureCreateInfo) or info.?.width == 0 or info.?.height == 0 or info.?.mip_count == 0) return errors.invalid_argument;
    const id = renderer.createTexture(info.?.width, info.?.height, info.?.format) catch |err| return switch (err) {
        error.NoDevice, error.UnsupportedTextureFormat, error.TextureCreateFailed => errors.sdl_error,
        error.OutOfMemory => errors.out_of_memory,
    };
    out_handle.?.* = id;
    return errors.ok;
}
fn uploadTexture(handle: ?*RendererHandle, texture: u64, info: ?*const TextureUploadInfo) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or info.?.struct_size < @sizeOf(TextureUploadInfo) or info.?.data == null or info.?.byte_length == 0) return errors.invalid_argument;
    renderer.uploadTexture(texture, info.?.data.?, info.?.byte_length, info.?.row_pitch) catch |err| return switch (err) {
        error.InvalidTexture, error.TextureUploadOutOfBounds => errors.invalid_argument,
        error.NoDevice, error.TransferBufferCreateFailed, error.TransferBufferMapFailed, error.CommandBufferFailed, error.CopyPassFailed, error.SubmitFailed, error.WaitForIdleFailed => errors.sdl_error,
    };
    return errors.ok;
}
fn destroyTexture(handle: ?*RendererHandle, texture: u64) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (texture == 0) return errors.invalid_handle;
    renderer.destroyTexture(texture) catch return errors.invalid_handle;
    return errors.ok;
}
fn createRenderTarget(handle: ?*RendererHandle, info: ?*const RenderTargetCreateInfo, out_handle: ?*u64) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or out_handle == null or info.?.struct_size < @sizeOf(RenderTargetCreateInfo) or info.?.width == 0 or info.?.height == 0) return errors.invalid_argument;
    const id = renderer.next_resource_handle;
    renderer.next_resource_handle += 1;
    renderer.resources.put(renderer.allocator, id, {}) catch return errors.out_of_memory;
    out_handle.?.* = id;
    return errors.ok;
}
fn bindRenderTarget(handle: ?*RendererHandle, target: u64) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (target != 0 and !renderer.resources.contains(target)) return errors.invalid_handle;
    if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state;
    return errors.ok;
}
fn createBuffer(handle: ?*RendererHandle, info: ?*const BufferCreateInfo, out_handle: ?*u64) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or out_handle == null or info.?.struct_size < @sizeOf(BufferCreateInfo) or info.?.element_count == 0 or info.?.stride == 0) return errors.invalid_argument;
    const id = renderer.createBuffer(info.?.element_count, info.?.format, info.?.stride, info.?.usage) catch |err| return switch (err) {
        error.BufferTooLarge => errors.invalid_argument,
        error.NoDevice, error.BufferCreateFailed => errors.sdl_error,
        error.OutOfMemory => errors.out_of_memory,
    };
    out_handle.?.* = id;
    return errors.ok;
}
fn uploadBuffer(handle: ?*RendererHandle, buffer: u64, info: ?*const BufferUploadInfo) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or info.?.struct_size < @sizeOf(BufferUploadInfo) or info.?.data == null or info.?.byte_length == 0) return errors.invalid_argument;
    renderer.uploadBuffer(buffer, info.?.data.?, info.?.byte_length, info.?.byte_offset) catch |err| return switch (err) {
        error.InvalidBuffer, error.BufferUploadOutOfBounds => errors.invalid_argument,
        error.NoDevice, error.TransferBufferCreateFailed, error.TransferBufferMapFailed, error.CommandBufferFailed, error.CopyPassFailed, error.SubmitFailed, error.WaitForIdleFailed => errors.sdl_error,
    };
    return errors.ok;
}
fn destroyBuffer(handle: ?*RendererHandle, buffer: u64) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (buffer == 0) return errors.invalid_handle;
    renderer.destroyBuffer(buffer) catch |err| return switch (err) {
        error.InvalidBuffer => errors.invalid_handle,
        error.NoDevice => errors.sdl_error,
    };
    return errors.ok;
}
fn setTexture(handle: ?*RendererHandle, texture: u64) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state;
    if (texture == 0) {
        renderer.bound_texture = null;
        return errors.ok;
    }
    renderer.bindTexture(texture) catch return errors.invalid_handle;
    return errors.ok;
}
fn bindVertexBuffer(handle: ?*RendererHandle, buffer: u64) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (buffer == 0) return errors.invalid_argument;
    if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state;
    renderer.bindVertexBuffer(buffer) catch return errors.invalid_handle;
    return errors.ok;
}
fn setSampler(handle: ?*RendererHandle, sampler: u64) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (sampler == 0) return errors.invalid_argument;
    if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state;
    if (sampler != 1 and sampler != 2) return errors.invalid_argument;
    renderer.use_linear_sampler = sampler == 2;
    return errors.ok;
}
fn draw(handle: ?*RendererHandle, vertex_buffer: u32, primitive_count: u32) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (primitive_count == 0) return errors.invalid_argument;
    if (renderer.frame.state != .pass_active) {
        renderer.last_error = "draw requires active render pass";
        return errors.invalid_state;
    }
    renderer.draw(vertex_buffer, primitive_count) catch |err| {
        renderer.last_error = @errorName(err);
        return switch (err) {
            error.InvalidDraw, error.InvalidBuffer, error.UnsupportedVertexFormat => errors.invalid_argument,
            error.InvalidState => errors.invalid_state,
            error.NoDevice, error.ShaderDirectoryMissing, error.ShaderFileMissing, error.ShaderFileReadFailed, error.ShaderCreationFailed, error.PipelineCreateFailed => errors.sdl_error,
            else => errors.internal_error,
        };
    };
    return errors.ok;
}
fn drawIndexed(handle: ?*RendererHandle, index_buffer: u64, index_size: u32, first_index: u32, index_count: u32, vertex_offset: i32) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (index_buffer == 0 or (index_size != 2 and index_size != 4) or index_count == 0) return errors.invalid_argument;
    if (renderer.frame.state != .pass_active) return errors.invalid_state;
    renderer.drawIndexed(index_buffer, index_size, first_index, index_count, vertex_offset) catch |err| {
        renderer.last_error = @errorName(err);
        return switch (err) {
            error.InvalidDraw, error.InvalidBuffer, error.VertexBufferMissing, error.UnsupportedVertexFormat => errors.invalid_argument,
            error.InvalidState => errors.invalid_state,
            error.NoDevice, error.ShaderDirectoryMissing, error.ShaderFileMissing, error.ShaderFileReadFailed, error.ShaderCreationFailed, error.PipelineCreateFailed => errors.sdl_error,
            else => errors.internal_error,
        };
    };
    return errors.ok;
}
fn drawTemporary(handle: ?*RendererHandle, info: ?*const TemporaryGeometryInfo, primitive_count: u32) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or info.?.struct_size < @sizeOf(TemporaryGeometryInfo) or info.?.data == null or info.?.byte_length == 0 or info.?.stride == 0 or primitive_count == 0) return errors.invalid_argument;
    if (renderer.frame.state != .pass_active) {
        renderer.last_error = "draw_temporary requires active render pass";
        return errors.invalid_state;
    }
    renderer.drawTemporary(info.?.data.?, info.?.byte_length, info.?.stride, primitive_count) catch |err| {
        renderer.last_error = @errorName(err);
        return switch (err) {
            error.InvalidDraw, error.InvalidBuffer, error.InvalidState, error.UnsupportedVertexFormat => errors.invalid_argument,
            error.NoDevice, error.CreateFailed, error.BufferCreateFailed, error.BufferTooLarge, error.BufferUploadOutOfBounds, error.TransferBufferCreateFailed, error.TransferBufferMapFailed, error.CommandBufferFailed, error.CopyPassFailed, error.SubmitFailed, error.WaitForIdleFailed, error.ShaderDirectoryMissing, error.ShaderFileMissing, error.ShaderFileReadFailed, error.ShaderCreationFailed, error.PipelineCreateFailed, error.InvalidTexture, error.SamplerCreateFailed, error.SamplerMissing, error.UnsupportedDriver, error.UnsupportedShaderFormat => errors.sdl_error,
            error.OutOfMemory => errors.out_of_memory,
        };
    };
    return errors.ok;
}
pub fn gfxgpu_readback(handle: ?*RendererHandle, info: ?*ReadbackInfo) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or info.?.struct_size < @sizeOf(ReadbackInfo) or info.?.width == 0 or info.?.height == 0 or info.?.data == null) return errors.invalid_argument;
    if (info.?.row_pitch < info.?.width * 4 or info.?.byte_length < info.?.row_pitch * info.?.height) return errors.invalid_argument;
    renderer.readback(@as([*]u8, @ptrCast(info.?.data.?))[0..info.?.byte_length], info.?.width, info.?.height, info.?.row_pitch) catch |err| {
        renderer.last_error = @errorName(err);
        return switch (err) {
            error.ReadbackUnavailable => errors.unsupported,
            error.ReadbackInvalid => errors.invalid_state,
            error.NoDevice, error.TransferBufferCreateFailed, error.CommandBufferFailed, error.CopyPassFailed, error.SubmitFailed, error.WaitForIdleFailed, error.TransferBufferMapFailed => errors.sdl_error,
        };
    };
    return errors.ok;
}

const api = Api{
    .abi_version = abi_version,
    .struct_size = @sizeOf(Api),
    .create = create,
    .destroy = destroy,
    .get_last_error = getLastError,
    .get_live_counts = getLiveCounts,
    .begin_frame = beginFrame,
    .end_frame = endFrame,
    .present = present,
    .cancel_frame = cancelFrame,
    .clear = clear,
    .resize = resize,
    .set_viewport = setViewport,
    .set_transform = setTransform,
    .set_color = setColor,
    .set_fog = setFog,
    .set_state = setState,
    .create_texture = createTexture,
    .upload_texture = uploadTexture,
    .destroy_texture = destroyTexture,
    .create_render_target = createRenderTarget,
    .bind_render_target = bindRenderTarget,
    .create_buffer = createBuffer,
    .upload_buffer = uploadBuffer,
    .destroy_buffer = destroyBuffer,
    .set_texture = setTexture,
    .set_sampler = setSampler,
    .draw = draw,
    .draw_indexed = drawIndexed,
    .draw_temporary = drawTemporary,
    .bind_vertex_buffer = bindVertexBuffer,
};

pub fn gfxgpu_get_api(requested_version: u32, out_api: ?*Api) callconv(.c) Result {
    if (out_api == null) return errors.invalid_argument;
    if (requested_version != abi_version) return errors.unsupported;
    if (out_api.?.struct_size < @sizeOf(Api)) return errors.invalid_argument;
    out_api.?.* = api;
    return errors.ok;
}

test "C ABI uses fixed-width fields and rejects invalid API requests" {
    try std.testing.expectEqual(@as(usize, 4), @sizeOf(Result));
    try std.testing.expectEqual(@as(usize, 8), @sizeOf(u64));
    try std.testing.expectEqual(errors.invalid_argument, gfxgpu_get_api(abi_version, null));
    var api_short = api;
    api_short.struct_size = 1;
    try std.testing.expectEqual(errors.invalid_argument, gfxgpu_get_api(abi_version, &api_short));
    try std.testing.expectEqual(errors.unsupported, gfxgpu_get_api(abi_version + 1, &api_short));
}

test "ABI create and destroy balance and rejects null arguments" {
    var output: ?*RendererHandle = null;
    try std.testing.expectEqual(errors.invalid_argument, create(null, &output));
    var info = CreateInfo{
        .struct_size = @sizeOf(CreateInfo),
        .flags = 2,
        .sdl_window = null,
        .width = 640,
        .height = 480,
        .shader_directory_utf8 = null,
        .preferred_driver_utf8 = null,
    };
    try std.testing.expectEqual(errors.ok, create(&info, &output));
    try std.testing.expect(output != null);
    destroy(output);
    try std.testing.expectEqual(errors.invalid_handle, getLastError(null, null, 0, null));
    try std.testing.expectEqual(errors.invalid_argument, getLiveCounts(null, null));
}

test "standalone readback export validates its contract" {
    try std.testing.expectEqual(errors.invalid_handle, gfxgpu_readback(null, null));
    var output: ?*RendererHandle = null;
    var create_info = CreateInfo{
        .struct_size = @sizeOf(CreateInfo),
        .flags = 2,
        .sdl_window = null,
        .width = 2,
        .height = 2,
        .shader_directory_utf8 = null,
        .preferred_driver_utf8 = null,
    };
    try std.testing.expectEqual(errors.ok, create(&create_info, &output));
    defer destroy(output);
    var pixels: [16]u8 = undefined;
    var info = ReadbackInfo{ .struct_size = @sizeOf(ReadbackInfo), .width = 2, .height = 2, .byte_length = pixels.len, .row_pitch = 8, .data = &pixels };
    try std.testing.expectEqual(errors.unsupported, gfxgpu_readback(output, &info));
}
