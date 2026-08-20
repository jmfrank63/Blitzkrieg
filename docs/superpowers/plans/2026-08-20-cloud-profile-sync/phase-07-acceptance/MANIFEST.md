# Phase 07 — Native Acceptance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Prove the feature on real machines, including the two-machine case the whole design exists for.

| Packet | Depends on | Owns |
|---|---|---|
| P07-M01 | P06-M03 | macOS acceptance |
| P07-M02 | M01 | Windows acceptance, junction included |
| P07-M03 | M02 | Linux acceptance |
| P07-M04 | M03 | two-machine convergence |

Exit: two machines converge through a delete and a conflict, with no save lost, on Windows and macOS.
