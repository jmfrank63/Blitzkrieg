const std = @import("std");
const errors = @import("error.zig");
const renderer_mod = @import("renderer.zig");
const device_mod = @import("device.zig");

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
};

fn create(info: ?*const CreateInfo, out_renderer: ?*?*RendererHandle) callconv(.c) Result {
    if (info == null or out_renderer == null) return errors.invalid_argument;
    const create_info = info.?.*;
    if (create_info.struct_size < @sizeOf(CreateInfo) or create_info.width == 0 or create_info.height == 0)
        return errors.invalid_argument;
    const state = std.heap.c_allocator.create(renderer_mod.Renderer) catch return errors.out_of_memory;
    state.* = renderer_mod.Renderer.init(std.heap.c_allocator);
    if ((create_info.flags & 2) == 0) {
        state.device = device_mod.Device.init(std.heap.c_allocator, device_mod.real_api, @intCast(@import("sdl.zig").shaderformat_dxil), (create_info.flags & 1) != 0, create_info.preferred_driver_utf8) catch {
            state.deinit();
            std.heap.c_allocator.destroy(state);
            return errors.sdl_error;
        };
    }
    out_renderer.?.* = @ptrCast(state);
    return errors.ok;
}

fn destroy(handle: ?*RendererHandle) callconv(.c) void {
    if (handle) |value| {
        const state: *renderer_mod.Renderer = @ptrCast(@alignCast(value));
        state.deinit();
        std.heap.c_allocator.destroy(state);
    }
}

fn getLastError(handle: ?*RendererHandle, destination: ?[*]u8, capacity: u32, written: ?*u32) callconv(.c) Result {
    if (handle == null) return errors.invalid_handle;
    return errors.copyBounded("", destination, capacity, written);
}

fn getLiveCounts(handle: ?*RendererHandle, counts: ?*LiveCounts) callconv(.c) Result {
    if (handle == null or counts == null) return errors.invalid_argument;
    if (counts.?.struct_size < @sizeOf(LiveCounts)) return errors.invalid_argument;
    counts.?.textures = 0;
    counts.?.buffers = 0;
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
    renderer.frame.begin(true) catch return errors.invalid_state;
    return errors.ok;
}
fn endFrame(handle: ?*RendererHandle) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    renderer.frame.end() catch return errors.invalid_state;
    return errors.ok;
}
fn present(handle: ?*RendererHandle) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    renderer.frame.present() catch return errors.invalid_state;
    return errors.ok;
}
fn cancelFrame(handle: ?*RendererHandle) callconv(.c) void {
    if (withRenderer(handle)) |renderer| renderer.frame.cancel();
}
fn clear(handle: ?*RendererHandle, info: ?*const ClearInfo) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or info.?.struct_size < @sizeOf(ClearInfo)) return errors.invalid_argument;
    if (renderer.frame.state != .recording) return errors.invalid_state;
    return errors.ok;
}
fn resize(handle: ?*RendererHandle, width: u32, height: u32) callconv(.c) Result {
    _ = withRenderer(handle) orelse return errors.invalid_handle;
    if (width == 0 or height == 0) return errors.invalid_argument;
    return errors.ok;
}
fn setViewport(handle: ?*RendererHandle, viewport: ?*const ViewportInfo) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (viewport == null or viewport.?.struct_size < @sizeOf(ViewportInfo)) return errors.invalid_argument; if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }
fn setTransform(handle: ?*RendererHandle, world: ?*const MatrixInfo, view_proj: ?*const MatrixInfo) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (world == null or view_proj == null or world.?.struct_size < @sizeOf(MatrixInfo) or view_proj.?.struct_size < @sizeOf(MatrixInfo)) return errors.invalid_argument; if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }
fn setColor(handle: ?*RendererHandle, _: u32) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }
fn setFog(handle: ?*RendererHandle, _: u32) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }
fn setState(handle: ?*RendererHandle, info: ?*const StateInfo) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (info == null or info.?.struct_size < @sizeOf(StateInfo)) return errors.invalid_argument; if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }
fn createTexture(handle: ?*RendererHandle, info: ?*const TextureCreateInfo, out_handle: ?*u64) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (info == null or out_handle == null or info.?.struct_size < @sizeOf(TextureCreateInfo) or info.?.width == 0 or info.?.height == 0 or info.?.mip_count == 0) return errors.invalid_argument; const id = renderer.next_resource_handle; renderer.next_resource_handle += 1; renderer.resources.put(renderer.allocator, id, {}) catch return errors.out_of_memory; out_handle.?.* = id; return errors.ok; }
fn uploadTexture(handle: ?*RendererHandle, texture: u64, info: ?*const TextureUploadInfo) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (info == null or info.?.struct_size < @sizeOf(TextureUploadInfo) or info.?.data == null or info.?.byte_length == 0 or !renderer.resources.contains(texture)) return errors.invalid_argument; return errors.ok; }
fn destroyTexture(handle: ?*RendererHandle, texture: u64) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (texture == 0 or !renderer.resources.remove(texture)) return errors.invalid_handle; return errors.ok; }
fn createRenderTarget(handle: ?*RendererHandle, info: ?*const RenderTargetCreateInfo, out_handle: ?*u64) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (info == null or out_handle == null or info.?.struct_size < @sizeOf(RenderTargetCreateInfo) or info.?.width == 0 or info.?.height == 0) return errors.invalid_argument; const id = renderer.next_resource_handle; renderer.next_resource_handle += 1; renderer.resources.put(renderer.allocator, id, {}) catch return errors.out_of_memory; out_handle.?.* = id; return errors.ok; }
fn bindRenderTarget(handle: ?*RendererHandle, target: u64) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (target != 0 and !renderer.resources.contains(target)) return errors.invalid_handle; if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }
fn createBuffer(handle: ?*RendererHandle, info: ?*const BufferCreateInfo, out_handle: ?*u64) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (info == null or out_handle == null or info.?.struct_size < @sizeOf(BufferCreateInfo) or info.?.element_count == 0 or info.?.stride == 0) return errors.invalid_argument; const id = renderer.next_resource_handle; renderer.next_resource_handle += 1; renderer.resources.put(renderer.allocator, id, {}) catch return errors.out_of_memory; out_handle.?.* = id; return errors.ok; }
fn uploadBuffer(handle: ?*RendererHandle, buffer: u64, info: ?*const BufferUploadInfo) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (info == null or info.?.struct_size < @sizeOf(BufferUploadInfo) or info.?.data == null or info.?.byte_length == 0 or !renderer.resources.contains(buffer)) return errors.invalid_argument; return errors.ok; }
fn destroyBuffer(handle: ?*RendererHandle, buffer: u64) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (buffer == 0 or !renderer.resources.remove(buffer)) return errors.invalid_handle; return errors.ok; }
fn setTexture(handle: ?*RendererHandle, texture: u64) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (texture == 0) return errors.invalid_argument; if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }
fn setSampler(handle: ?*RendererHandle, sampler: u64) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (sampler == 0) return errors.invalid_argument; if (renderer.frame.state != .recording and renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }
fn draw(handle: ?*RendererHandle, _: u32, primitive_count: u32) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (primitive_count == 0) return errors.invalid_argument; if (renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }
fn drawIndexed(handle: ?*RendererHandle, index_buffer: u64, index_size: u32, first_index: u32, index_count: u32, _: i32) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (index_buffer == 0 or (index_size != 2 and index_size != 4) or index_count == 0) return errors.invalid_argument; if (renderer.frame.state != .pass_active) return errors.invalid_state; _ = first_index; return errors.ok; }
fn drawTemporary(handle: ?*RendererHandle, info: ?*const TemporaryGeometryInfo, primitive_count: u32) callconv(.c) Result { const renderer = withRenderer(handle) orelse return errors.invalid_handle; if (info == null or info.?.struct_size < @sizeOf(TemporaryGeometryInfo) or info.?.data == null or info.?.byte_length == 0 or info.?.stride == 0 or primitive_count == 0) return errors.invalid_argument; if (renderer.frame.state != .pass_active) return errors.invalid_state; return errors.ok; }

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
