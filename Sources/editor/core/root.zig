//! The Map Editor core: no UI, no engine, no C. See
//! docs/superpowers/specs/2026-09-19-portable-map-editor-design.md, "Editor core".
//! The kit (editor_kit) is importable here and the leaf primitives moved into
//! it in S02/T02 are re-exported through `core.files`/`core.shipped`/
//! `core.script_file`/`core.autosave` so every existing caller keeps compiling
//! without a surface change.
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
pub const files = kit_mod.files;
pub const settings = @import("settings.zig");
pub const autosave = struct {
    // Everything the kit's autosave module exposes - re-exported one by one so
    // MapEditor callers keep seeing `core.autosave.Autosave`, `.target`, etc.
    pub const default_interval_minutes = kit_mod.autosave.default_interval_minutes;
    pub const Autosave = kit_mod.autosave.Autosave;
    pub const Target = kit_mod.autosave.Target;
    pub const target = kit_mod.autosave.target;

    /// MapEditor's thin shim threading `.bzm` through the kit's generic
    /// `recoveryName(buffer, doc_path, extension)` so panels.zig's caller
    /// shape (`core.autosave.recoveryName(buffer, doc_path)`) is preserved
    /// after T02's extraction. Other editors on the kit thread their own
    /// extension directly.
    pub fn recoveryName(buffer: []u8, doc_path: []const u8) ?[]const u8 {
        return kit_mod.autosave.recoveryName(buffer, doc_path, ".bzm");
    }
};
pub const shipped = kit_mod.shipped;
pub const script_file = kit_mod.script_file;
pub const checks = @import("checks.zig");
pub const rmg = @import("rmg.zig");
pub const composers = @import("composers.zig");

test {
    @import("std").testing.refAllDecls(@This());
}
