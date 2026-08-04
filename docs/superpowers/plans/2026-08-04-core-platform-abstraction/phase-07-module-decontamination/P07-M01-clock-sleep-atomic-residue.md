# P07-M01 — Remove Clock, Sleep, and Atomic Residue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Convert remaining playable timing, delay, seed, and atomic call sites to PlatformClient.

**Dependencies:** P06-M06.

**Allowed files:** `Sources/src/Common/InterfaceScreenBase.cpp`, `Sources/src/Scene/Transition.cpp`, `Sources/src/Scene/OpenVideoPlayer.cpp`, `Sources/src/GameTT/Chapter.cpp`, `Sources/src/GameTT/iMissionInternal.cpp`, `Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp`, `Sources/src/Main/iMainInternal.cpp`, `Sources/src/AILogic/AIUnit.cpp`, `Sources/src/AILogic/DamageToEnemyUpdater.cpp`, `tools/zig/runtime_clock_residue_test.cpp`, `tools/zig/runtime_platform_allowlist.txt`.

- [ ] Add simulated-time fixtures for transitions, video position, mission pacing, random seed, diagnostics timestamps, and delayed UI processing.
- [ ] Replace `timeGetTime`, `GetTickCount`, `Sleep`, and owned `Interlocked*` calls with platform services.
- [ ] Preserve 32-bit wrap behavior where serialized or compared by legacy code.
- [ ] Compare fixture outputs against Windows oracle values.
- [ ] Remove converted token entries from the allowlist and run the audit.
- [ ] Commit: `runtime: remove native timing and atomic residue`

**Evidence:** timing fixtures and reduced allowlist count.
