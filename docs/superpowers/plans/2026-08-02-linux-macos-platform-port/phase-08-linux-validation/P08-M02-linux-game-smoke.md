# P08-M02 — Automate Linux Startup, Reference Scene, and Save/Load

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Launch the packaged native Linux game and prove its main automatic checkpoints.

**Dependencies:** P08-M01.

**Allowed files:** `tools/zig/game_smoke.zig`, `Sources/src/Game/GameMain.cpp`, `build.zig`, `docs/superpowers/evidence/platform-port/linux-acceptance.md`.

- [ ] Add a Zig process controller with argv, working directory, stdout/stderr/log capture, 120-second timeout, and process-tree termination only on timeout.
- [ ] Run startup smoke to module load, SDL/vulkan/SPIR-V init, miniaudio init/no-device acceptance, main-menu checkpoint, reference-scene capture/hash, temporary-user-root save, reload, and normal quit.
- [ ] Require no fallback to Direct3D, missing module/data/shader, write inside package, crash, timeout, or non-zero renderer count.
- [ ] Run smoke three times on native Linux, once with software Vulkan and once with available hardware Vulkan when both are present.
- [ ] Commit: `test: automate Linux game startup and save smoke`

**Evidence:** commands, checkpoints, capture hash, save hash, and logs in acceptance markdown; generated artifacts remain uncommitted.
