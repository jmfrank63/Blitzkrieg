# Phase 00 — Bundled rclone

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Ship the binary so the feature works out of the box.

| Packet | Depends on | Owns |
|---|---|---|
| P00-M01 | — | hashed build dependency and staging |
| P00-M02 | M01 | availability out of the box, no PATH |
| P00-M03 | M02 | packaging, size and third-party notices |
| P00-M04 | M03 | signed release gate (human, credentialed) |

Exit: a fresh install on a machine with no rclone reports cloud sync available. P00-M04 is a release gate and does not block later phases.
