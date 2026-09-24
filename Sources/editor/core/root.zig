//! The Map Editor core: no UI, no engine, no C. See
//! docs/superpowers/specs/2026-09-19-portable-map-editor-design.md, "Editor core".
pub const bridge = @import("bridge.zig");
pub const fake_bridge = @import("fake_bridge.zig");
pub const document = @import("document.zig");
pub const history = @import("history.zig");
pub const editor = @import("editor.zig");

test {
    @import("std").testing.refAllDecls(@This());
}
