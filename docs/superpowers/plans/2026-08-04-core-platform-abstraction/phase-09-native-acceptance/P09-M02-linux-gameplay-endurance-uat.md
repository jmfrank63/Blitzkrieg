# P09-M02 — Linux Mission, Save/Load, Endurance, and UAT

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove representative gameplay and sustained lifecycle behavior on Linux.

**Dependencies:** P09-M01.

**Allowed files:** `build.zig`, `tools/zig/linux_game_acceptance.zig`, `tools/zig/verify_gfxgpu_endurance.zig`, `docs/superpowers/evidence/platform-abstraction/linux-acceptance.md`.

- [ ] Start the agreed representative mission, issue keyboard/mouse commands, play audio, and exercise a network initialization path.
- [ ] Save, return to menu, load, and verify the same mission state from the writable Linux user root.
- [ ] Run resize/fullscreen/focus cycles, mission/menu cycles, and at least 30 minutes of scripted endurance.
- [ ] Compare renderer screenshots where deterministic and record human visual/audio/playability review separately.
- [ ] Require zero platform, renderer, audio, socket, worker, and module leaks at exit.
- [ ] Commit: `test: accept Linux gameplay and endurance`

**Evidence:** save path/hash, lifecycle samples, screenshots, logs, and human approval.
