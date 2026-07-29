const std = @import("std");
const vk = @import("vulkan");
const ctx_mod = @import("context.zig");

pub const VkContext = ctx_mod.VkContext;
const Context = ctx_mod.Context;

// C ABI exports — thin wrappers around Context methods.
// zigcc exports use .Stdcall calling convention to match the
// C++ bridge declaration in gfxvk_c.h.

export fn gfxvk_create_context(hwnd: ?*anyopaque) callconv(.c) ?*VkContext {
    const h: std.os.windows.HWND = @ptrCast(hwnd);
    const result = ctx_mod.init(std.heap.page_allocator, h) catch return null;
    const ctx = std.heap.page_allocator.create(Context) catch return null;
    ctx.* = result;
    return @ptrCast(ctx);
}

export fn gfxvk_destroy_context(ctx: ?*VkContext) callconv(.c) void {
    if (ctx) |c| {
        const typed: *Context = @ptrCast(@alignCast(c));
        ctx_mod.deinit(typed);
        std.heap.page_allocator.destroy(typed);
    }
}

export fn gfxvk_begin_scene(ctx: ?*VkContext) callconv(.c) bool {
    if (ctx) |c| return ctx_mod.begin_scene(@ptrCast(@alignCast(c))) catch return false;
    return false;
}

export fn gfxvk_end_scene(ctx: ?*VkContext) callconv(.c) void {
    if (ctx) |c| ctx_mod.end_scene(@ptrCast(@alignCast(c)));
}

export fn gfxvk_clear(
    ctx: ?*VkContext,
    nNumRects: c_int,
    pRects: ?*anyopaque,
    dwFlags: u32,
    dwColor: u32,
    fDepth: f32,
    dwStencil: u32,
) callconv(.c) void {
    _ = nNumRects; _ = pRects; _ = dwFlags; _ = fDepth; _ = dwStencil;
    if (ctx) |c| {
        const r = @as(f32, @floatFromInt((dwColor >> 16) & 0xFF)) / 255.0;
        const g = @as(f32, @floatFromInt((dwColor >> 8) & 0xFF)) / 255.0;
        const b = @as(f32, @floatFromInt(dwColor & 0xFF)) / 255.0;
        const a = @as(f32, @floatFromInt((dwColor >> 24) & 0xFF)) / 255.0;
        ctx_mod.set_clear_color(@ptrCast(@alignCast(c)), .{ .float_32 = .{ r, g, b, a } });
    }
}

export fn gfxvk_flip(ctx: ?*VkContext) callconv(.c) bool {
    if (ctx) |c| return ctx_mod.flip(@ptrCast(@alignCast(c))) catch return false;
    return false;
}

// ── Phase 3 ABI ─────────────────────────────────────────────────

export fn gfxvk_set_effect(ctx: ?*VkContext, effect_id: u32) callconv(.c) void {
    if (ctx) |c| ctx_mod.set_effect(@ptrCast(@alignCast(c)), effect_id);
}

export fn gfxvk_set_texture(ctx: ?*VkContext, tex_ptr: ?*anyopaque) callconv(.c) void {
    if (ctx) |c| ctx_mod.set_texture(@ptrCast(@alignCast(c)), tex_ptr);
}

export fn gfxvk_create_texture(
    ctx: ?*VkContext,
    tex_ptr: ?*anyopaque,
    width: u32,
    height: u32,
    format: u32,
    mips: u32,
    data: ?*const u8,
    data_size: usize,
) callconv(.c) bool {
    if (ctx) |c| return ctx_mod.create_texture(@ptrCast(@alignCast(c)), tex_ptr, width, height, format, mips, @ptrCast(data), data_size);
    return false;
}

export fn gfxvk_lock_vb(ctx: ?*VkContext, num_vertices: usize) callconv(.c) ?[*]u8 {
    if (ctx) |c| return ctx_mod.lock_vb(@ptrCast(@alignCast(c)), num_vertices);
    return null;
}
export fn gfxvk_unlock_vb(ctx: ?*VkContext) callconv(.c) void {
    if (ctx) |c| ctx_mod.unlock_vb(@ptrCast(@alignCast(c)));
}

export fn gfxvk_lock_ib(ctx: ?*VkContext, num_indices: usize) callconv(.c) ?[*]u8 {
    if (ctx) |c| return ctx_mod.lock_ib(@ptrCast(@alignCast(c)), num_indices);
    return null;
}
export fn gfxvk_unlock_ib(ctx: ?*VkContext) callconv(.c) void {
    if (ctx) |c| ctx_mod.unlock_ib(@ptrCast(@alignCast(c)));
}

export fn gfxvk_draw_indexed(
    ctx: ?*VkContext,
    index_count: usize,
    vertex_base: usize,
    index_base: usize,
) callconv(.c) void {
    if (ctx) |c| ctx_mod.draw_indexed(@ptrCast(@alignCast(c)), index_count, vertex_base, index_base);
}

export fn gfxvk_set_viewport(
    ctx: ?*VkContext,
    x: c_int,
    y: c_int,
    width: c_int,
    height: c_int,
) callconv(.c) void {
    if (ctx) |c| ctx_mod.set_viewport(@ptrCast(@alignCast(c)), x, y, width, height);
}
