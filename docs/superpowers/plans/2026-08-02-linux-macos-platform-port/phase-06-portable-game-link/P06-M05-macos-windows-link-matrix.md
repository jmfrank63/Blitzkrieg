# P06-M05 — Close macOS Linking and Regress Windows

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Link the complete Apple-Silicon runtime and prove the same graph still builds/runs on Windows x64.

**Dependencies:** P06-M04.

**Allowed files:** `build.zig`, `tools/zig/platform_build_matrix_test.zig`, `docs/superpowers/evidence/platform-port/target-matrix.md`, `docs/superpowers/evidence/platform-port/windows-regression.md`.

- [ ] Apply the common source/library graph to `aarch64-macos` with an explicit SDK/sysroot option and target-native dylib install names; do not probe `xcrun` or invoke Apple tools from `build.zig`.
- [ ] Build all runtime modules and `Game` on a macOS runner; record SDK and deployment target.
- [ ] On Windows run `test`, `test-gfxgpu`, `game-all`, `verify-x64-runtime`, and `install-game` for x64.
- [ ] Add a matrix test that rejects target-inappropriate libraries/resources/extensions for each artifact.
- [ ] Record compile/native status accurately and require a clean worktree outside evidence changes.
- [ ] Commit: `build: close portable game link matrix`

**Evidence:** macOS final link, Windows native regression transcript, target artifact audit.
