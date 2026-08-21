# Phase 01 — Provider Catalogue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Take the provider list from rclone, store it in a schema that can hold any backend, and make it reachable from C++.

| Packet | Depends on | Owns |
|---|---|---|
| P01-M01 | P00-M03 | catalogue fetch, parse, cache and bootstrap |
| P01-M02 | M01 | generic schema with remote root, migration, secret classification |
| P01-M03 | M02 | catalogue and credentials exports |
| P01-M04 | M03 | provider selection and destination filtering |

Exit: the provider list comes from the catalogue, reaches C++, survives a cold start, and old credentials still sync.
