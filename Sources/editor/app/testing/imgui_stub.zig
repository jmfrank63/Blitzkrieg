//! editor_imgui for `zig build test-map-editor-view`: view.zig imports the
//! ImGui module, but nothing its tests reach draws or reads ImGui (the
//! capture flags come from the tests' own input fake), so no ImGui is
//! compiled or linked into that test. The Hindi encoder alone is a stub
//! (l10n.format reaches it); a test that did reach ImGui fails to compile
//! here, naming the missing declaration.
pub const c = struct {
    /// The Hindi shaper is not loaded here, so text is never shaped: the kit's
    /// `encodeHindi` takes its null path and `l10n.format` returns the text as is.
    pub fn bkFontsEncodeHindi(text: [*]const u8, buffer: [*]u8, capacity: usize) c_int {
        _ = text;
        _ = buffer;
        _ = capacity;
        return -1;
    }
};
