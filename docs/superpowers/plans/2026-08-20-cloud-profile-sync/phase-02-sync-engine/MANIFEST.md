# Phase 02 — Sync Engine

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Drive a full sync to completion off the main thread, turn every rclone failure into something a player can act on, and expose it through the ABI.

| Packet | Depends on | Owns |
|---|---|---|
| P02-M01 | P01-M04 | pairing and resync bootstrap |
| P02-M02 | M01 | worker thread and the sync state machine |
| P02-M03 | M02 | error classification and recovery outcomes |
| P02-M04 | M03 | two-sided trash retention and pruning |
| P02-M05 | M04 | sync exports wired through the C ABI |

Exit: a pair/diverge/converge cycle over two local directories passes end to end through the C ABI, with a conflict preserved, a delete recoverable, and no socket call on the calling thread.
