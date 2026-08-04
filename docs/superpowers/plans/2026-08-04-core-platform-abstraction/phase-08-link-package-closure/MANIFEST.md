# Phase 08 — Target Link and Package Closure

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Build, link, stage, and verify the complete playable graph for Windows, Linux, and macOS.

| Packet | Depends on | Owns |
|---|---|---|
| P08-M01 | P07-M06 | target source/link policy |
| P08-M02 | M01 | PlatformRuntime names, rpaths, install names |
| P08-M03 | M02 | Linux link closure |
| P08-M04 | M03 | macOS arm64 link and bundle closure |
| P08-M05 | M04 | Windows x64 regression closure |
| P08-M06 | M05 | staging/package verifier matrix |

Exit: `game-all`, `install-game`, and runtime verification pass for all triples with target-correct package contents.
