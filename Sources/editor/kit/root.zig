//! The Map Editor kit: reusable editor plumbing (files, shipped, script_file,
//! autosave, history stack primitive, settings primitive, host, crt, imgui
//! wrapper, BK_EDITOR_AUTO schedule driver, pictures-cache, testlaunch) with
//! no engine-bridge dependency. See the slice S02 plan for the extraction
//! schedule. T01 only scaffolds this module: later tasks move submodules in
//! and add them as `pub const` re-exports here.
//!
//! The kit must not import `editor_core`; the dependency direction is
//! `editor_core -> editor_kit`.

test {
    @import("std").testing.refAllDecls(@This());
}
