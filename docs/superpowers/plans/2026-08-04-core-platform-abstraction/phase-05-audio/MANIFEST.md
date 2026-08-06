# Phase 05 — Portable Audio Support

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Remove Windows heap, atomic, timer, and diagnostic dependencies from the miniaudio-backed SFX runtime.

| Packet | Depends on | Owns |
|---|---|---|
| P05-M01 | P01-M06 | allocator and diagnostic callbacks |
| P05-M02 | M01 | miniaudio context/device lifecycle |
| P05-M03 | M02 | atomics, timing, completion workers |
| P05-M04 | M03 | stream/callback lifetime |
| P05-M05 | M04 | SFX module and native audio gate |

Exit: SFX contains no direct Windows heap/debug/atomic/timer calls and survives repeated native init/play/stop/shutdown.

P05-M01 Windows checkpoint: `test-audio-lifecycle`, `test-platform-audio`, and `test-input-audio-gate` pass natively; SFX allocation and diagnostics are portable, while the shared ABI allocator handoff remains open for the independently loaded module.
P05-M02 Windows checkpoint: miniaudio backend selection, null fallback, explicit teardown, and lifecycle fixture coverage pass; production module double-init and real-device probing remain part of M05.
P05-M03 Windows checkpoint: SFX diagnostics, timing, completion exchange, and fade worker use portable facilities; `test-audio-worker` passes 1000 handoffs and restart, with TSan left open for a supported toolchain.
