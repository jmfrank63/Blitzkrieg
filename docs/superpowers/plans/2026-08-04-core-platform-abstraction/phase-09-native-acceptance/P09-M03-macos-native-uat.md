# P09-M03 — macOS Native Smoke and Human UAT

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Validate the CI-produced arm64 bundle on an Apple-Silicon Mac.

**Dependencies:** P08-M06.

**Allowed files:** `build.zig`, `tools/zig/macos_game_acceptance.zig`, `tools/zig/verify_gfxgpu_endurance.zig`, `docs/superpowers/evidence/platform-abstraction/macos-acceptance.md`.

- [ ] Download the exact verified CI artifact and run it from a normal user directory without developer environment variables.
- [ ] Verify PlatformRuntime, Metal renderer/MSL, Input, controller/clipboard, SFX, Net initialization, menu, and representative mission.
- [ ] Exercise resize/fullscreen/focus, save/load, menu/mission return, and clean shutdown.
- [ ] Record macOS version, machine model, artifact hash, backend identity, logs, screenshots, and human visual/audio/playability approval.
- [ ] Require no validation errors, crashes, or leaked runtime resources.
- [ ] Commit: `test: accept Apple Silicon native runtime`

**Evidence:** artifact hash, native logs/screenshots, save hash, and human approval.
