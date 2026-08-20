# Phase 04 — Config Backup and Restore

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Keep config.cfg off the machine without letting it into the sync set, and make restoring it safe and reversible.

| Packet | Depends on | Owns |
|---|---|---|
| P04-M01 | P02-M05 | per-host config snapshot upload |
| P04-M02 | M01 | backup listing, retention, and listing export |
| P04-M03 | M02 | restore with a GFX-preserving merge |
| P04-M04 | M03 | pre-restore backup and undo |

Exit: a snapshot uploads per host, prunes to retention, restores without disturbing local display settings, and can be undone — all reachable from C++.
