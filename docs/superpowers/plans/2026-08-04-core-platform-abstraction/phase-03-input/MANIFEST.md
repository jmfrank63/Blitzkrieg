# Phase 03 — Portable Input Module

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Replace DirectInput state and polling with normalized platform events while preserving legacy control IDs and bindings.

| Packet | Depends on | Owns |
|---|---|---|
| P03-M01 | P02-M05 | public Input type decontamination |
| P03-M02 | M01 | keyboard and mouse state |
| P03-M03 | M02 | text, repeat, focus transitions |
| P03-M04 | M03 | controller discovery and axes |
| P03-M05 | M04 | binding, combo, emulation compatibility |
| P03-M06 | M05 | Input shared-module gate |

Exit: Input contains no DirectInput header/type/call in the portable graph and fixture behavior matches Windows control semantics.

P03-M01 Windows checkpoint: input contract compilation and portable code mapping execution pass; DirectInput type decontamination remains open.
