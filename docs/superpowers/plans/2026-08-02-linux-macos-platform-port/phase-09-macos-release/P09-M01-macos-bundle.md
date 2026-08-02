# P09-M01 — Build and Verify the macOS Application Bundle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Assemble an unsigned, self-contained Apple-Silicon `.app` without shell or Apple command-line tool invocation from the build.

**Dependencies:** P08-M05.

**Allowed files:** `build.zig`, `tools/zig/stage.zig`, `tools/zig/package.zig`, `tools/zig/verify_runtime.zig`, `tools/zig/macos_info_plist.zig`.

- [ ] Stage `Blitzkrieg.app/Contents/MacOS/Game`, `Frameworks/*.dylib`, `Resources/Data`, `Resources/Shaders/GfxGpu`, and generated `Info.plist` with stable bundle ID/version/executable keys.
- [ ] Set target-native install names/rpaths during linking through Zig link options; do not run `install_name_tool`.
- [ ] Verify Mach-O arm64 architecture, dylib references confined to system or bundle-relative paths, required module exports, MSL manifest completeness, executable permissions, and absence of Windows/Linux artifacts.
- [ ] Produce a deterministic ZIP with normalized paths/timestamps using Zig packaging.
- [ ] Run bundle verifier and hermeticity audit on macOS.
- [ ] Commit: `build: package Apple Silicon app bundle`

**Evidence:** sorted bundle manifest, dylib dependency report, package hash.
