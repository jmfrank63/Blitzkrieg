# Phase 00 target matrix evidence

Date: 2026-08-02

| Target triple | Policy | Foundation status | Test mode | Runtime claim | First remaining frontier |
|---|---|---|---|---|---|
| `x86_64-windows-msvc` | `windows_x64` | Native header, build-support, stage, shader-parser, matrix, and hermeticity tests pass | `run` | Native foundation only | Full Windows runtime remains dependent on the configured MSVC and Windows SDK toolchain |
| `x86_64-linux-gnu` | `linux_x64` | Portable headers and build-support compile successfully; foundation compile step passes | `compile` | None | Playable runtime graph is intentionally not exposed yet; existing legacy modules still require the later Linux portability packets and a Linux C++/system-library setup |
| `aarch64-macos` | `macos_arm64` | Portable headers and build-support compile successfully; foundation compile step passes | `compile` | None | Playable runtime graph is intentionally not exposed yet; macOS SDK/sysroot and Metal runtime integration remain later-phase work |

## Negative policy evidence

The target policy rejects:

- `x86-windows-msvc` with `unsupported target`
- `aarch64-linux-gnu` with `unsupported target`
- `-Dtest-mode=run` for `x86_64-linux-gnu` on the current Windows host with `-Dtest-mode=run requires a matching native target`

## Phase 05 network/system evidence

Date: 2026-08-03

| Target triple | Socket contract | Network/system gate | Evidence |
|---|---|---|---|
| `x86_64-windows-msvc` | Native compile and run | Native run passed | TCP loopback, nonblocking read, UI callback injection, child-process exit, fixture hash, and idempotent shutdown passed |
| `x86_64-linux-gnu` | Native run in WSL | Native run passed | Run with Linux-local Zig caches; gate reported `fixture=c70d495e tcp=4 child=23` |
| `aarch64-macos` | Cross compile in CI | Compile-only on Windows | Local cross compile requires an Apple SDK sysroot; GitHub Actions supplies the macOS SDK and remains the runtime validation environment |

The Phase 05 gate preserves the protocol fixture hash `c70d495e` while exercising the portable socket and system-service boundaries on the native Windows and Linux paths.

## Phase 06 runtime-header evidence

Date: 2026-08-03

| Target triple | Runtime-header matrix | Result |
|---|---|---|
| `x86_64-windows-msvc` | `zig build test-runtime-headers -Dtarget=x86_64-windows-msvc -Dtest-mode=compile` | Passed |
| `x86_64-linux-gnu.2.32` | Native WSL compile of all playable `StdAfx.h` headers | Passed |

The Linux matrix uses glibc 2.32 as the portable baseline because the installed Zig/libc headers reject newer libstdc++ headers when targeting an older baseline. This is compile evidence only; runtime link closure is covered by the remaining Phase 06 packets.

## Interpretation

Compile-only success for Linux and macOS validates the Phase 00 contracts: target classification, portable compiler/value headers, Zig build-tool compilation, and hermetic build-path setup. It is not a claim that the game runtime currently links or runs on those platforms.
