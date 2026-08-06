# P07-M01 — Remove Clock, Sleep, and Atomic Residue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Convert remaining playable timing, delay, seed, and atomic call sites to PlatformClient.

**Dependencies:** P06-M06.

**Allowed files:** `Sources/src/Common/InterfaceScreenBase.cpp`, `Sources/src/Scene/Transition.cpp`, `Sources/src/Scene/OpenVideoPlayer.cpp`, `Sources/src/GameTT/Chapter.cpp`, `Sources/src/GameTT/iMissionInternal.cpp`, `Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp`, `Sources/src/Main/iMainInternal.cpp`, `Sources/src/AILogic/AIUnit.cpp`, `Sources/src/AILogic/DamageToEnemyUpdater.cpp`, `tools/zig/runtime_clock_residue_test.cpp`, `tools/zig/runtime_platform_allowlist.txt`.

- [x] Add simulated-time fixtures for transitions, video position, mission pacing, random seed, diagnostics timestamps, and delayed UI processing.
- [x] Replace `timeGetTime`, `GetTickCount`, `Sleep`, and owned `Interlocked*` calls with platform services.
- [x] Preserve 32-bit wrap behavior where serialized or compared by legacy code.
- [x] Compare fixture outputs against Windows oracle values.
- [x] Remove converted token entries from the allowlist and run the audit.
- [x] Commit: `runtime: remove native timing and atomic residue`

**Evidence:** Windows direct Zig/MSVC fixture run passed with
`clock residue fixtures: wrap=32 transition=16/127/255 video=0/6 mission=64 seed=0x12345678 diagnostics=4 delayed-ui=1`.
The converted modules contain no `timeGetTime`, `GetTickCount`, `Sleep(`,
`Interlocked`, or `mmsystem.h` residue. `zig test
tools/zig/runtime_platform_audit_test.zig` passed all 9 tests after removing
the 10 converted GetTickCount allowlist entries (inventory 39, ownership 38).
`zig build test-platform-clock -Dtarget=x86_64-windows-msvc -Dtest-mode=run`
and `zig build game -Dtarget=x86_64-windows-msvc` also passed.
