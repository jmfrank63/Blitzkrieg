# P08-M01 — Stage and Verify the Linux Runtime

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce a deterministic portable Linux game directory with correct loader-relative module discovery.

**Dependencies:** P07-M05.

**Allowed files:** `build.zig`, `tools/zig/stage.zig`, `tools/zig/package.zig`, `tools/zig/verify_runtime.zig`, `.gitignore`, `.vscode/settings.json`.

- [ ] Define the Linux layout under `zig-out/game/x86_64-linux-gnu`: `Game`, `lib/*.so`, SDL runtime as required by chosen linkage, `Data`, and `Shaders/GfxGpu` SPIR-V/manifest.
- [ ] Add verifier checks for executable bit, ELF target architecture, required module names/exports, relative load paths, missing Windows files, writable-data absence, and complete shader records.
- [ ] Make package ZIP/TAR choice deterministic through the existing Zig archiver; do not call `tar` or shell tools.
- [ ] Test staging into a path containing spaces and package extraction with the Zig verifier.
- [ ] Run `install-game`, `verify-runtime`, `package-game`, and hermeticity audit for Linux.
- [ ] Commit: `build: package portable Linux game runtime`

**Evidence:** sorted package manifest and verifier output.
