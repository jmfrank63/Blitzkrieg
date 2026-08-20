# Phase 03 — C++ Integration and Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Connect the Zig engine to the game: a facade, the three sync points, and a visible state.

| Packet | Depends on | Owns |
|---|---|---|
| P03-M01 | P02-M04 | C++ facade over the C ABI |
| P03-M02 | M01 | startup pull before the profile config is read |
| P03-M03 | M02 | post-save and exit push |
| P03-M04 | M03 | syncing indicator and skip-to-offline |

Exit: the game starts, syncs, and exits on every target without the main loop ever blocking on a socket.
