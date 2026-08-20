# Phase 05 — Config Backup and Restore

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Keep config.cfg off the machine without letting it onto the sync set, and make restoring it safe.

| Packet | Depends on | Owns |
|---|---|---|
| P05-M01 | P02-M04 | per-host config snapshot upload |
| P05-M02 | M01 | backup listing and retention |
| P05-M03 | M02 | restore with a GFX-preserving merge |
| P05-M04 | M03 | pre-restore local backup and undo |

Exit: a snapshot uploads per host, prunes to retention, and restores without disturbing local display settings.
