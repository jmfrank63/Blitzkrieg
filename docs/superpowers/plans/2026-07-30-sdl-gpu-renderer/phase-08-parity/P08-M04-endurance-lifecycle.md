# P08-M04 — Validate Resize, Fullscreen, Restart, and Endurance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove lifecycle correctness during real-game stress.

**Dependencies:** P08-M03 with human acceptance recorded.

**Allowed files:** `tools/zig/verify_gfxgpu_endurance.ps1`, `build.zig`, `docs/superpowers/evidence/sdl-gpu/endurance.md`.

- [ ] Automate or precisely prompt: 20 resizes, 10 minimize/restore cycles, 10 supported fullscreen/windowed toggles, five mission load/return cycles, and two complete game restarts.
- [ ] Sample renderer live counts, cache counts, transfer high-water marks, process memory, and validation diagnostics after each cycle.
- [ ] Fail on monotonic live-resource growth, stale-handle diagnostic, command-state error, device loss, missing present, or non-zero final count.
- [ ] Allow pipeline/sampler caches to plateau; document first-cycle warmup and prove later cycles remain bounded.
- [ ] Run on Windows 11 x64 with SDL debug mode and D3D12 forced.
- [ ] Commit: `test: validate GPU renderer endurance`

**Evidence:** cycle table, peak/ending counts, memory range, zero validation failures.
