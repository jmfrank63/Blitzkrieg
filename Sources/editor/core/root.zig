//! The Map Editor core: no UI, no engine, no C. See
//! docs/superpowers/specs/2026-09-19-portable-map-editor-design.md, "Editor core".
//! The kit (editor_kit) is importable here so later S02 tasks can re-point
//! core submodules at the kit's version without a second build.zig edit.
pub const kit = @import("editor_kit");
pub const records = @import("records.zig");
pub const bridge = @import("bridge.zig");
pub const fake_bridge = @import("fake_bridge.zig");
pub const document = @import("document.zig");
pub const history = @import("history.zig");
pub const editor = @import("editor.zig");
pub const tools = @import("tools.zig");
pub const tools_vso = @import("tools_vso.zig");
pub const tools_groups = @import("tools_groups.zig");
pub const tools_ai = @import("tools_ai.zig");
pub const tools_heights = @import("tools_heights.zig");
pub const tools_damage = @import("tools_damage.zig");
pub const filters = @import("filters.zig");
pub const layers = @import("layers.zig");
pub const tools_fields = @import("tools_fields.zig");
pub const files = @import("files.zig");
pub const settings = @import("settings.zig");
pub const autosave = @import("autosave.zig");
pub const shipped = @import("shipped.zig");
pub const script_file = @import("script_file.zig");
pub const checks = @import("checks.zig");
pub const rmg = @import("rmg.zig");
pub const composers = @import("composers.zig");

test {
    @import("std").testing.refAllDecls(@This());
}
