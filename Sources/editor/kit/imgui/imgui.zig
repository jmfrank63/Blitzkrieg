//! Dear ImGui for the Zig editors: the dcimgui C API (`ig*` functions,
//! docking branch) and the SDL3 / SDL GPU backend shim.
pub const c = @cImport({
    @cInclude("cimgui.h");
    @cInclude("imgui_backend.h");
});

/// GFXGPU overlay callback (`Api.set_overlay`): draws the ImGui frame that
/// the last `igRender` finished onto the renderer's colour target.
pub fn overlayCallback(user: ?*anyopaque, command_buffer: *anyopaque, target: *anyopaque, width: u32, height: u32) callconv(.c) void {
    _ = user;
    _ = width;
    _ = height;
    c.bk_imgui_backend_render(command_buffer, target);
}
