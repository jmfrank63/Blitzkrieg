---
phase: "3"
slug: "map-editor-plan-6-finish-m1"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-09-28"
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Zig `std.testing` (core tier) + C++ test executables via `zig build` (map-file and engine tiers) + scripted-SDL smoke `smoke.zig` / `BK_EDITOR_AUTO` (app tier) |
| **Config file** | none — steps defined in `build.zig` |
| **Quick run command** | `zig build test -Dtarget=aarch64-macos` |
| **Full suite command** | `zig build test-editor-bridge -Dtest-mode=run && zig build test-map-editor-engine && zig build map-editor-host-check && zig build map-editor-smoke && zig build test-map-files-all` (macOS arm64; CI runs the GPU tiers on macOS and Windows) |
| **Estimated runtime** | ~120 seconds quick; full sweep several minutes |

---

## Sampling Rate

- **After every task commit:** Run `zig build test -Dtarget=aarch64-macos` plus the tier the task touches
- **After every plan wave:** Run the full suite command locally and one CI run on both GPU runners
- **Before `/gsd-verify-work`:** Full suite must be green, plus Johannes's hand try of the M1 exit criteria
- **Max feedback latency:** 300 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| (filled by the planner per task) | | | D-01..D-29 | | | | | | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Test-launch harness test (spawn `Game` or a stand-in and assert start/exit, profile, mod, windowed)
- [ ] `tools/zig/editor_bridge_test.cpp` — zoom-step bounds, pointer anchoring, yaw rotation and reset
- [ ] `Sources/editor/core/editor.zig` — safe save temp+rename, `.bak` once per session, autosave interval, recovery copy
- [ ] Settings file round-trip and recent-files tests
- [ ] `smoke.zig` → `BK_EDITOR_AUTO` generalisation and shot comparison tests

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Trackpad pinch and rotate gestures feel right | D-10, D-12 | Real multi-touch input cannot be produced from the agent shell | Johannes pinches and rotates on the MacBook trackpad in MapEditor |
| Test in game end to end on a real display | D-01..D-09 | Needs a visible game window and play | Johannes presses Test in game, plays, quits back to the editor |
| Native file dialogs and the unsaved-changes prompt | D-23 | Native dialogs cannot be driven headlessly | Johannes opens, edits, quits, and answers the prompt |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 300s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
