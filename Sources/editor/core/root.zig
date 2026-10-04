//! The Map Editor core: no UI, no engine, no C. See
//! docs/superpowers/specs/2026-09-19-portable-map-editor-design.md, "Editor core".
//! The kit (editor_kit) is importable here and the leaf primitives moved into
//! it in S02/T02 (`files`, `shipped`, `script_file`, `autosave`) are reached
//! directly through `editor_kit`; the core itself only re-exposes the kit as
//! `core.kit` for convenience and keeps the map-specific composed modules
//! (`settings`, `history`) that it built over the kit's generic primitives.
const kit_mod = @import("editor_kit");

pub const kit = kit_mod;
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
pub const settings = @import("settings.zig");
pub const checks = @import("checks.zig");
pub const rmg = @import("rmg.zig");
pub const composers = @import("composers.zig");

test {
    @import("std").testing.refAllDecls(@This());
}
