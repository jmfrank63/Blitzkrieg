# P00-M03 — Define the Supported Target Model

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Centralize target classification and prevent Windows-only build configuration from leaking into Linux/macOS.

**Dependencies:** P00-M01.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`.

**Required model:**

```zig
pub const PlatformTarget = enum { windows_x64, linux_x64, macos_arm64 };
pub const TestMode = enum { compile, run };
```

- [ ] Write table tests for supported triples, executable/shared-library names, package roots, shader formats, GPU drivers, native-run eligibility, and unsupported target diagnostics.
- [ ] Move target suffix, Windows SDK/MSVC setup, system library, subsystem, entry point, resource, and `.def` decisions into named helpers keyed by `PlatformTarget`.
- [ ] Construct only the selected renderer and playable runtime graph for default/`game-all`; keep the legacy renderer, `BuildVersion`, `BetaKeyGen`, and `FontGen` behind explicit Windows-only steps so Linux/macOS builds never compile or link them.
- [ ] Add `-Dtest-mode=compile|run`; default to `run` only for a matching native target and reject explicit `run` for a non-native target.
- [ ] Keep `-Dtarget` authoritative; do not probe the host with a process or environment-specific script.
- [ ] Run `zig build test-build-support` and compile it for all three supported triples.
- [ ] Commit: `build: define cross platform target policy`

**Evidence:** target table test output including rejected `x86-windows-msvc` and `aarch64-linux-gnu` cases.
