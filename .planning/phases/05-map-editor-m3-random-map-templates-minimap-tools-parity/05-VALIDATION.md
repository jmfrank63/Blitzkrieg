---
phase: "05"
slug: "map-editor-m3-random-map-templates-minimap-tools-parity"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-09-30"
---

# Phase 05 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Zig `zig build` step matrix + C++ test executables (random-missions-test pattern); app tier via `BK_EDITOR_AUTO`; game tier via `BK_AUTO_UI` / `BK_MAP_TRACE` |
| **Config file** | `build.zig` (never `zig fmt build.zig`) |
| **Quick run command** | `zig build test -Dcopy-data=false -Dtest-mode=run` |
| **Full suite command** | `zig build test map-editor-smoke map-editor-auto map-editor-auto-m2 test-map-editor-engine test-editor-bridge -Dcopy-data=false -Dtest-mode=run` (+ `map-editor-m3-auto`, composer round-trip, determinism steps as plans add them; sweeps `test-map-files-all` at the phase gate) |
| **Estimated runtime** | Quick ~2–5 min; full local matrix ~20–40 min; `test-map-files-all` sweep longer |

---

## Sampling Rate

- **After every task commit:** Run the quick command plus the tier the task touched (core/map-file/bridge/engine/auto per plan verify blocks)
- **After every plan wave:** Run the full suite command
- **Before `/gsd-verify-work`:** Full suite green, incl. `test-map-files-all` (1,755 maps, 0 FAIL)
- **Max feedback latency:** ~10 min (quick tier + touched tier)
- **Windows:** every plan's GUI-adjacent tests also run on win-home (`ssh win-home`)

---

## Per-Task Verification Map

Task IDs are filled as plans land (D-39's 11 plans). The binding contract is the D-40 exit-criteria → signal map below; each plan's tasks must reference the row(s) they advance.

| D-40 # | Criterion | Threat Ref | Test Type | Automated Signal | Status |
|--------|-----------|------------|-----------|------------------|--------|
| 1 | Every `05-PARITY.md` row closed with evidence | — | source/test evidence | Evidence column filled per row; "not a feature" rows cite file:line | ⬜ pending |
| 2 | Core+map-file tiers green on 6 CI targets | — | CI | Green CI run (run id in SUMMARY) | ⬜ pending |
| 3 | Engine tier green (macOS arm64 + Windows-MSVC) incl. new round trips | — | engine test | `test-map-editor-engine` + expected-value builder comparisons | ⬜ pending |
| 4 | Composer round trip (43+102+404+27 files) load/save/reload equal, 5 engine-C++ targets | — | data-only test | New composer round-trip step | ⬜ pending |
| 5 | Authored RMG set + fixed seed → byte-identical `.bzm`; game loads under `BK_AUTO_UI`, clean exit | — | determinism + game tier | Determinism step + game-reads-it step | ⬜ pending |
| 6 | `CreateMiniMapImage` writes 4 images beside a saved user map; minimap click moves camera | — | app auto + shot compare | `map-editor-m3-auto` shot comparison | ⬜ pending |
| 7 | `zig build map-editor-m3-auto` passes locally (macOS arm64 + win-home) | — | app auto | Local run logs recorded | ⬜ pending |
| 8 | `test-map-files-all` 1,755 maps, 0 FAIL | — | sweep | Sweep output 0 FAIL | ⬜ pending |
| 9 | MFC editor deleted; `git grep` clean; everything builds | — | grep + CI | `git grep -n 'src/MapEditor\|MapEditor.vcxproj\|Editors/MapEditor'` → only `.planning`/`docs` history | ⬜ pending |
| 10 | Hand try on the release build approves M3 | — | human | — | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements — the tiers exist from phases 3–4. New steps (`map-editor-m3-auto`, composer round-trip, determinism, game-reads-it-M3) are created by the plans that need them (per `05-RESEARCH.md` Validation Architecture).

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Review PARITY evidence | D-40.1 | Judgement call on evidence quality | Johannes reviews each closed row of `05-PARITY.md` |
| Minimap shot sanity | D-40.6 | Shot compare proves change, not visual quality | Eyeball the `map-editor-m3-auto` minimap shots |
| M3 hand try | D-40.10 | Release-build UX approval | Johannes's scripted M3 walk-through on the release build (macOS + Windows) |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < ~10 min
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
