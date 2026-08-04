# Phase 07 — Playable-Module Decontamination

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Remove remaining native calls, native headers, Windows scalar helpers, and case-insensitive path assumptions from playable modules.

| Packet | Depends on | Owns |
|---|---|---|
| P07-M01 | P06-M06 | clock/sleep/atomic residue |
| P07-M02 | M01 | files, paths, strings, metadata |
| P07-M03 | M02 | UI/Image geometry and case correctness |
| P07-M04 | M03 | Scene cursor, video, transitions |
| P07-M05 | M04 | exports, COM residue, module lifetimes |
| P07-M06 | M05 | zero-hit playable-source audit |

Exit: the audit reports zero forbidden native tokens outside approved platform backends and Windows adapters.
