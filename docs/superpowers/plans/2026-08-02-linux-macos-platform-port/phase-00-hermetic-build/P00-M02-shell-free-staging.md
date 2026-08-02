# P00-M02 — Make Staging and Packaging Shell-Free

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Stage and package runtime files without PowerShell, `ln`, or platform shell fallbacks.

**Dependencies:** P00-M01.

**Allowed files:** `build.zig`, `tools/zig/stage.zig`, `tools/zig/package.zig`, `tools/zig/stage_test.zig`, `.gitignore`, `.vscode/settings.json`.

- [ ] Add failing tests for deterministic replacement, copy-data default, optional Zig filesystem symlink mode, target-specific runtime names, and rejection of stale `.stale` images.
- [ ] Remove `std.process.run` calls from `stage.zig`; use Zig directory/file/symlink APIs only. Make data copy the package default and emit an actionable permission error for an explicitly requested symlink.
- [ ] Replace hard-coded `Game.exe`/DLL/PDB lists with a target layout record supplied by `build.zig`; keep editors Windows-only and outside `game-all`.
- [ ] Ensure staging writes only under `zig-out/game/<triple>` and packaging only under `zig-out/packages/<triple>`.
- [ ] Add ignore and VS Code exclusion entries only for newly introduced staging roots.
- [ ] Run `zig build test-stage`, `zig build package-game -Dtarget=x86_64-windows-msvc`, and `zig build audit-build-hermeticity`.
- [ ] Commit: `build: make runtime staging shell free`

**Evidence:** stage unit test transcript, package listing, and hermeticity pass.
