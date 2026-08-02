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

## Interpretation

Compile-only success for Linux and macOS validates the Phase 00 contracts: target classification, portable compiler/value headers, Zig build-tool compilation, and hermetic build-path setup. It is not a claim that the game runtime currently links or runs on those platforms.
