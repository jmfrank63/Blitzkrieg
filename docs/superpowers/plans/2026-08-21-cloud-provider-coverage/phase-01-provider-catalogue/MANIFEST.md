# Phase 01 — Provider Catalogue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Take the provider list from rclone instead of from our source.

| Packet | Depends on | Owns |
|---|---|---|
| P01-M01 | P00-M03 | catalogue fetch, parse and cache |
| P01-M02 | M01 | generic credentials schema and migration |
| P01-M03 | M02 | dynamic provider list in the option system |

Exit: the provider list comes from the catalogue, survives a cold start, and old credentials still load.
