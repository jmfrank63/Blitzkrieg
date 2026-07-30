const std = @import("std");
const errors = @import("error.zig");
const renderer_mod = @import("renderer.zig");

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

pub const Api = extern struct {
    abi_version: u32,
    struct_size: u32,
    create: *const fn (?*const CreateInfo, *?*RendererHandle) callconv(.c) Result,
    destroy: *const fn (?*RendererHandle) callconv(.c) void,
    get_last_error: *const fn (?*RendererHandle, ?[*]u8, u32, ?*u32) callconv(.c) Result,
    get_live_counts: *const fn (?*RendererHandle, ?*LiveCounts) callconv(.c) Result,
};

fn create(info: ?*const CreateInfo, out_renderer: ?*?*RendererHandle) callconv(.c) Result {
    if (info == null or out_renderer == null) return errors.invalid_argument;
    const create_info = info.?.*;
    if (create_info.struct_size < @sizeOf(CreateInfo) or create_info.width == 0 or create_info.height == 0)
        return errors.invalid_argument;
    const state = std.heap.c_allocator.create(renderer_mod.Renderer) catch return errors.out_of_memory;
    state.* = renderer_mod.Renderer.init(std.heap.c_allocator);
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

const api = Api{
    .abi_version = abi_version,
    .struct_size = @sizeOf(Api),
    .create = create,
    .destroy = destroy,
    .get_last_error = getLastError,
    .get_live_counts = getLiveCounts,
};

pub export fn gfxgpu_get_api(requested_version: u32, out_api: ?*Api) callconv(.c) Result {
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
    var api_short = Api{ .abi_version = 0, .struct_size = 1, .create = create, .destroy = destroy, .get_last_error = getLastError, .get_live_counts = getLiveCounts };
    try std.testing.expectEqual(errors.invalid_argument, gfxgpu_get_api(abi_version, &api_short));
    try std.testing.expectEqual(errors.unsupported, gfxgpu_get_api(abi_version + 1, &api_short));
}

test "ABI create and destroy balance and rejects null arguments" {
    var output: ?*RendererHandle = null;
    try std.testing.expectEqual(errors.invalid_argument, create(null, &output));
    var info = CreateInfo{
        .struct_size = @sizeOf(CreateInfo),
        .flags = 0,
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
