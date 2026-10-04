//! The Map Editor kit: reusable editor plumbing (files, shipped, script_file,
//! autosave, history stack primitive, settings primitive, host, crt, imgui
//! wrapper, BK_EDITOR_AUTO schedule driver, pictures-cache, testlaunch) with
//! no engine-bridge dependency. See the slice S02 plan for the extraction
//! schedule. T02 brings the four leaf primitives in; later tasks add the
//! coupled modules as `pub const` re-exports.
//!
//! The kit must not import `editor_core`; the dependency direction is
//! `editor_core -> editor_kit`.
pub const files = @import("files.zig");
pub const shipped = @import("shipped.zig");
pub const script_file = @import("script_file.zig");
pub const autosave = @import("autosave.zig");
pub const history = @import("history.zig");
pub const settings = @import("settings.zig");
pub const crt = @import("crt.zig");
pub const testlaunch = @import("testlaunch.zig");
pub const auto_schedule = @import("auto_schedule.zig");
pub const pictures_cache = @import("pictures_cache.zig");
pub const host = @import("host.zig");
// The ImGui wrapper lives under `kit/imgui/` so the kit is self-contained,
// but it is reached through its own `editor_imgui` module (build.zig's
// `editor_imgui_module`), not through this re-export, because a file can
// belong to only one Zig module.

test {
    @import("std").testing.refAllDecls(@This());
}
