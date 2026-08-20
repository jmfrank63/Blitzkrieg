# Phase 06 — Backends

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Make the two credential-based backends work end to end against real remotes.

| Packet | Depends on | Owns |
|---|---|---|
| P06-M01 | P04-M04, P05-M04 | S3-compatible backend |
| P06-M02 | M01 | WebDAV backend |
| P06-M03 | M02 | connection test and failure reporting |

Exit: the phase-02 cycle passes against a real S3-compatible remote and a real WebDAV server.
