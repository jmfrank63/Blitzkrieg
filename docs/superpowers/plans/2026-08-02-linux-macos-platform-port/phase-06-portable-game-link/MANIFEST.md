# Phase 06 — Portable Runtime Module and Game Link

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Compile and link every module required by the playable game for Windows x64, Linux x64, and macOS arm64.

**Architecture:** Common precompiled-header content consumes portable compatibility headers; shared modules export the same factory descriptor with target-native filenames. Known residual platform calls move to established services before target link gates.

**Tech Stack:** Zig C/C++ build graph, target-native shared libraries, existing legacy module factories.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P06-M01 | P05-M05 | runtime shared headers/PCH | module header compile |
| P06-M02 | M01 | shared library exports/naming | fixture module load |
| P06-M03 | M01 | known platform-call residue | source audit |
| P06-M04 | M02, M03 | Linux full module/game link | Linux `game-all` |
| P06-M05 | M04 | macOS link plus Windows regression | three-target link matrix |

Phase exit: `game-all` compiles for all triples and Windows native automated tests remain green.
