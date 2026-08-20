# Phase 02 — Sync Engine

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Drive a full sync to completion without blocking, and turn every rclone failure mode into something a player can act on.

| Packet | Depends on | Owns |
|---|---|---|
| P02-M01 | P01-M04 | pairing and resync bootstrap |
| P02-M02 | M01 | async job polling and the sync state machine |
| P02-M03 | M02 | error classification and recovery prompts |
| P02-M04 | M03 | trash retention and pruning |

Exit: a pair/diverge/converge cycle over two local directories passes, with a conflict preserved and a propagated delete recoverable from the trash.
