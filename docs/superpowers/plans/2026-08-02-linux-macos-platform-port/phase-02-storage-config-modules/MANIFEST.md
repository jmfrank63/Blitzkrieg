# Phase 02 — Storage, Configuration, and Runtime Modules

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Make value serialization, host files, data/user roots, display options, and dynamic gameplay module discovery portable.

**Architecture:** Existing game-facing storage interfaces remain stable. Platform paths and file metadata normalize legacy names at one boundary; runtime modules retain `GetModuleDescriptor` and load by target suffix.

**Tech Stack:** Zig StreamIO core, C/C++ file APIs, SDL preference/display APIs, portable dynamic library wrapper.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P02-M01 | P01-M05 | legacy variant subset | variant compatibility tests |
| P02-M02 | P02-M01 | Zig host file metadata/enumeration | StreamIO unit tests |
| P02-M03 | P02-M02 | C++ `NFile` implementation | file utility tests |
| P02-M04 | P02-M03 | data/user path policy | path integration tests |
| P02-M05 | P02-M01, M04 | options bridge/display enumeration | config round-trip |
| P02-M06 | P01-M04, M04 | module discovery/exports and phase gate | module load/save gate |

Phase exit: `zig build test-platform-files` and `zig build test-platform-modules` natively on Windows/Linux; macOS compile.
