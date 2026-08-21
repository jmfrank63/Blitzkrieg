# Phase 02 — Generic Credentials Form

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** One renderer for every backend, driven entirely by the catalogue.

| Packet | Depends on | Owns |
|---|---|---|
| P02-M01 | P01-M04 | form model derived from the catalogue |
| P02-M02 | M01 | form exports |
| P02-M03 | M02 | the dialog renders the model |
| P02-M04 | M03 | validation and connection test for any backend |

Exit: an arbitrary destination backend is configurable and testable with no provider-specific code.
