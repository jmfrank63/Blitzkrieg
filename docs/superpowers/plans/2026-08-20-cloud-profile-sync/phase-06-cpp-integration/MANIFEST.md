# Phase 06 — C++ Integration and Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Connect the engine to the game: a facade, the three sync points, and a visible state.

| Packet | Depends on | Owns |
|---|---|---|
| P06-M01 | P02-M05, P03-M04, P04-M04, P05-M02 | C++ facade over the C ABI, including the pending-restore apply and discovery-status wrappers |
| P06-M02 | M01 | startup pull before the profile config is read |
| P06-M03 | M02 | post-save and exit push |
| P06-M04 | M03 | sync indicator and skip-to-offline |

Exit: the game starts, syncs, and exits on every target with no socket call on the main thread.
