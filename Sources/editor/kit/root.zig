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

test {
    @import("std").testing.refAllDecls(@This());
}
