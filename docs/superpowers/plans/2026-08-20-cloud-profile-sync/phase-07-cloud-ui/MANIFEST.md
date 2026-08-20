# Phase 07 — Cloud UI

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Give the player the two screens the feature cannot be used without: where credentials are entered, and where backups are restored.

| Packet | Depends on | Owns |
|---|---|---|
| P07-M01 | P06-M04, P03-M04 | credentials dialog and connection test |
| P07-M02 | M01, P04-M02 | backup browser |
| P07-M03 | M02, P04-M04 | restore confirmation and undo |

Exit: a player can enter credentials, test the connection, browse backups, restore one, and undo it, without editing a file by hand.
