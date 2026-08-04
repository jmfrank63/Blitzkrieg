# Core Platform Abstraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement exactly one packet at a time. Do not redesign the ABI or combine packets. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace direct Win32 dependencies in the playable runtime with one versioned, process-wide platform ABI implemented for Windows, Linux, and macOS.

**Architecture:** `Game` and every runtime module call a thin C++ client over the fixed-layout `bk_platform_get_api` C table exported by one shared `PlatformRuntime` library. Target-selected Windows, POSIX, Linux, macOS, and SDL implementation files remain private to that library; native types and native SDK headers never enter gameplay modules.

**Tech Stack:** Zig 0.16.0 build graph and tests, C ABI with fixed-width types, C++17 clients/backends, SDL 3.4.0 application/input services, miniaudio, WinSock/POSIX sockets, Windows x64, Linux x64, and macOS arm64.

---

## Authoritative documents

Read these before executing a packet:

1. `docs/superpowers/specs/2026-08-04-core-platform-abstraction-design.md`
2. `docs/superpowers/plans/2026-08-02-linux-macos-platform-port/README.md`
3. `docs/superpowers/plans/2026-08-04-core-platform-abstraction/README.md`
4. `docs/superpowers/plans/2026-08-04-core-platform-abstraction/EXECUTION.md`
5. The selected phase `MANIFEST.md`
6. The selected packet

The accepted SDL_GPU renderer and completed platform tests are constraints, not replacement targets. If a packet conflicts with either, stop and report the exact conflict.

## Current frontier

The renderer and its native shader formats are validated on Windows, Linux, and macOS. The full Linux `install-game` build now reaches legacy runtime modules and fails where they still include or call DirectInput, WinSock, Win32 window/system APIs, Windows heap APIs, and case-insensitive Windows paths. This plan starts at that frontier.

It supersedes unfinished implementation details in the old port plan's P06/P08 frontier while preserving all accepted work from its earlier phases.

## Milestone boundary

Included:

- one shared `PlatformRuntime` and versioned C ABI;
- core timing, synchronization, diagnostics, dynamic library, paths, storage, environment, dialog, and process services;
- SDL application window, events, cursor, clipboard, and controller services;
- event-fed Input without DirectInput in the portable graph;
- WinSock/POSIX Net backends without protocol changes;
- portable miniaudio allocation, atomics, diagnostics, and lifecycle;
- portable `Game` startup/window/system-key flow;
- removal of native calls and native headers from playable modules;
- target-correct linking, staging, packaging, and Windows/Linux/macOS acceptance.

Excluded:

- editors and MFC tools;
- `BuildVersion`, `BetaKeyGen`, and `FontGen` on Linux/macOS;
- legacy D3D rendering outside Windows;
- protocol, save-format, gameplay, renderer, or audio-engine redesign;
- direct Cocoa, X11, Wayland, Vulkan, Metal, or D3D12 code;
- installers, signing/notarization, Intel macOS, and Linux arm64.

## Global invariants

- `PlatformRuntime` is instantiated once per process; gameplay DLLs/shared objects never compile private platform state.
- The ABI is C-compatible and append-only. Every aggregate begins with `struct_size`; the table begins with `abi_version` and `struct_size`.
- ABI values use fixed-width integers, UTF-8 plus explicit lengths, caller-owned output buffers, callbacks with user-data pointers, and opaque 64-bit handles.
- No STL type, exception, Zig error union, native handle, SDL pointer, Objective-C object, `HANDLE`, or `SOCKET` crosses the ABI.
- SDL initialization, application window, event pump, cursor capture, and SDL shutdown remain on the main thread.
- Input control IDs, binding names, text behavior, and saved input configuration remain stable.
- Network packet bytes and protocol constants remain unchanged.
- miniaudio remains the audio backend; Windows private-heap use is optional and backend-private.
- `PortableCrt.h` may carry scalar compatibility only. It must not gain window, input, socket, thread, process, dynamic-library, file, heap, audio, or diagnostic implementations.
- Native APIs are permitted only below `Sources/src/Platform` and in explicitly audited temporary Windows adapters.
- Build selection uses the resolved target, not the host. Windows SDK libraries/resources are never constructed for Linux/macOS.
- Cross targets use `-Dtest-mode=compile`; matching native targets use `-Dtest-mode=run`.
- Build tools remain Zig executables and Zig build steps. No shell, PowerShell, CMake, Ninja, or package manager is invoked by `build.zig`.
- Human playability approval is never inferred from compile or smoke output.

## Stable file map

### Public ABI and client

- `Sources/src/PlatformABI/platform_c.h` — result codes, handles, records, callbacks, and API table.
- `Sources/src/PlatformABI/PlatformClient.h/.cpp` — checked C++ accessors with no native types.
- `Sources/src/PlatformABI/PlatformRuntime.cpp` — exported table and lifecycle dispatcher.
- `Sources/src/PlatformABI/PlatformState.h` — private process-wide ownership state.

### Backends

- `Sources/src/Platform/Core/*.cpp` — backend-neutral validation, handles, diagnostics, and service dispatch.
- `Sources/src/Platform/SDL/*.cpp` — SDL lifetime, window, event, cursor, clipboard, and controller services.
- `Sources/src/Platform/Windows/*.cpp` — Win32 clocks, system, dynamic library, and WinSock implementation.
- `Sources/src/Platform/Posix/*.cpp` — POSIX clocks, synchronization, dynamic library, files, processes, and sockets.
- `Sources/src/Platform/Linux/*.cpp` — Linux user roots and platform-specific system details.
- `Sources/src/Platform/MacOS/*.cpp` — macOS user roots and platform-specific system details.

### Converted consumers

- `Sources/src/Input/InputAPI.h/.cpp`, `InputBinder.cpp`, and `Specific.h` — normalized event-fed input.
- `Sources/src/Net/NetLowest.h/.cpp` and worker call sites — opaque portable sockets.
- `Sources/src/SFX/AudioBackendOpen.cpp`, `SoundEngine.cpp`, and `StreamFadeOff.cpp` — portable miniaudio support.
- `Sources/src/Game/main.cpp`, `GameMain.cpp`, `WinFrame.cpp`, and `SysKeys.cpp` — portable application shell plus a Windows-only thin adapter.
- `Sources/src/Main`, `Common`, `Scene`, `UI`, `Image`, `GameTT`, `AILogic`, and `RandomMapGen` selected files — direct native-call residue removal.

### Build and tests

- `build.zig` and `tools/zig/build_support.zig` — target source/link graph.
- `tools/zig/platform_abi_*.cpp` — ABI layout, lifecycle, and real shared-library consumer tests.
- `tools/zig/platform_*_test.cpp` — focused service and module tests.
- `tools/zig/runtime_platform_audit.zig` — playable-source native-token and native-header audit.
- `tools/zig/stage.zig` and `tools/zig/verify_runtime.zig` — target runtime staging and verification.

## Base ABI contract

P00-M02 introduces the exact table. Later packets append fields only:

```c
#define BK_PLATFORM_ABI_VERSION 1u
typedef uint64_t BkPlatformHandle;
typedef uint32_t BkPlatformResult;

typedef struct BkPlatformApi {
    uint32_t abi_version;
    uint32_t struct_size;
    BkPlatformResult (*runtime_create)(const void *create_info);
    void (*runtime_destroy)(void);
    BkPlatformResult (*get_last_error)(char *dst, uint32_t capacity, uint32_t *required);
} BkPlatformApi;

const BkPlatformApi *bk_platform_get_api(uint32_t requested_version);
```

## Phase graph

```text
00 ABI foundation
        |
        v
01 core host services ---> 02 SDL application boundary
        |                            |
        |                            v
        +----------------------> 03 Input
        |
        +----------------------> 04 Network
        |
        +----------------------> 05 Audio
                                      |
02 + 03 + 04 + 05 --------------------+
        |
        v
06 Game shell and runtime integration
        |
        v
07 playable-module decontamination
        |
        v
08 target link and package closure
        |
        v
09 native acceptance and cutover
```

## Phase completion contracts

| Phase | Required gate |
|---|---|
| 00 | ABI layout, shared-library consumer, lifecycle, audit baseline, and three-target compile pass |
| 01 | Core service contracts pass natively and no consumer calls old core Win32 APIs |
| 02 | SDL window/event/cursor/clipboard/controller contract passes on native hosts |
| 03 | Input builds without DirectInput outside its temporary Windows oracle and behavior fixtures pass |
| 04 | Net builds over opaque sockets and loopback/broadcast/protocol fixtures pass |
| 05 | SFX/miniaudio builds without public Windows heap/atomic/debug calls and lifecycle smoke passes |
| 06 | Portable Game shell reaches module initialization and clean shutdown on Windows and Linux |
| 07 | Playable source audit reports zero native headers/calls outside approved backend directories |
| 08 | `game-all`, staging, and package verification pass for all three target triples |
| 09 | Native Windows/Linux/macOS launch, mission, save/load, focus/resize, and shutdown gates are accepted |

## Packet index

- `phase-00-abi-foundation`: P00-M01 through P00-M05
- `phase-01-core-host-services`: P01-M01 through P01-M06
- `phase-02-sdl-application-boundary`: P02-M01 through P02-M05
- `phase-03-input`: P03-M01 through P03-M06
- `phase-04-network`: P04-M01 through P04-M05
- `phase-05-audio`: P05-M01 through P05-M05
- `phase-06-game-shell`: P06-M01 through P06-M06
- `phase-07-module-decontamination`: P07-M01 through P07-M06
- `phase-08-link-package-closure`: P08-M01 through P08-M06
- `phase-09-native-acceptance`: P09-M01 through P09-M05

This is 55 independently reviewable packets. Each packet has an explicit allowlist, failing test, implementation boundary, commands, evidence, and commit. The coordinator starts the next packet only after the current packet is committed, pushed, and its gate passes.
