# Phase 01 — Sync Planning Primitives

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Turn a profile directory into safe bisync parameters. Every packet here is pure logic, testable with no network.

| Packet | Depends on | Owns |
|---|---|---|
| P01-M01 | P00-M04 | short link creation and repointing |
| P01-M02 | M01 | session-name budget validation |
| P01-M03 | M02 | filter rules, machine-local state paths, and the sentinel |
| P01-M04 | M03 | bisync parameter construction |

Exit: a profile directory at a pathological depth yields a short, budget-checked, correctly defaulted parameter set offline.
