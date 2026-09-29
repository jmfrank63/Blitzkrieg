//! editor_imgui for `zig build test-map-editor-view`: view.zig imports the
//! ImGui module, but nothing its tests reach draws or reads ImGui (the
//! capture flags come from the tests' own input fake), so no ImGui is
//! compiled or linked into that test. A test that did reach ImGui fails to
//! compile here, naming the missing declaration.
pub const c = struct {};
