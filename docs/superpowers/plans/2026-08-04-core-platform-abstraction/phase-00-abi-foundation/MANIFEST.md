# Phase 00 — ABI Foundation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Establish the checked platform boundary, versioned ABI, single shared runtime, client wrapper, and three-target foundation gate.

| Packet | Depends on | Owns |
|---|---|---|
| P00-M01 | current `main` | playable-source inventory and audit baseline |
| P00-M02 | M01 | public C ABI types and layout tests |
| P00-M03 | M02 | shared runtime lifecycle and state |
| P00-M04 | M03 | C++ client and real shared-library consumer |
| P00-M05 | M04 | target graph and foundation matrix |

Exit: `test-platform-abi` and `test-platform-foundation` pass for native Windows/Linux and compile for macOS arm64, with a versioned shared runtime used by a real C++ consumer.
