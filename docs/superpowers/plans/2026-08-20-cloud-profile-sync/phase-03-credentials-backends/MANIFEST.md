# Phase 03 — Credentials and Backends

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Store connection details where the option system cannot corrupt them, and make the two credential-based backends work end to end.

| Packet | Depends on | Owns |
|---|---|---|
| P03-M01 | P02-M05 | credentials file, remote parameters, and their exports |
| P03-M02 | M01 | S3-compatible backend |
| P03-M03 | M02 | WebDAV backend |
| P03-M04 | M03 | connection test with classified failures |

Exit: the phase-02 cycle passes against a real S3-compatible remote and a real WebDAV server, with credentials reachable from C++ and absent from every log.
