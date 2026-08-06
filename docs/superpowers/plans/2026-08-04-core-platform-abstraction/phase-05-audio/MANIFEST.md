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

P05-M01 Windows checkpoint: audio initialization and input/audio lifecycle gates pass natively; ABI allocator/diagnostic conversion remains open.
