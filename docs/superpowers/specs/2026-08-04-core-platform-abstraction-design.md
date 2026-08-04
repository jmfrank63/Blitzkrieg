# Core Platform Abstraction Design

**Date:** 2026-08-04

**Status:** Approved architecture, ready for implementation planning

## Goal

Remove Win32, DirectInput, WinSock, Windows heap, and Windows UI dependencies from playable game modules by routing host services through one versioned Blitzkrieg platform ABI. Preserve the accepted Windows x64 behavior while enabling native Linux x64 and Apple-Silicon macOS builds and runtime validation.

## Context

The current port already provides useful platform wrappers for clocks, synchronization, diagnostics, dynamic libraries, paths, sockets, SDL application ownership, and event translation. The full game build now exposes the remaining architectural problem: playable modules still include Windows headers, name Windows types in private state, call Win32 functions directly, and link Windows system libraries unconditionally.

Extending `PortableCrt.h` with increasingly broad Win32 emulation would allow individual files to compile but would leave ownership and behavior ambiguous. The permanent boundary must be explicit, process-wide, testable, and independent of C++ ABI details.

## Chosen architecture

One shared `PlatformRuntime` library owns process-wide host state and exports a versioned C ABI from `Sources/src/PlatformABI/platform_c.h`. `Game` links it directly, initializes it before loading gameplay modules, and shuts it down after unloading them. Every runtime module links the same shared library and uses a thin C++ wrapper from `Sources/src/PlatformABI/PlatformClient.h`; no gameplay module compiles a private copy of platform state.

```text
Game, Input, Net, SFX, UI, Scene, Main, GameTT
                         |
                         v
              PlatformClient C++ wrapper
                         |
                         v
              versioned platform_c.h ABI
                         |
                         v
                  PlatformRuntime
             /            |            \
      Windows backend   POSIX core    SDL services
                          /  \
                      Linux  macOS
```

The ABI uses fixed-width integers, explicit byte counts, caller-owned buffers, opaque 64-bit handles, `struct_size`, and an append-only API table. It does not expose STL objects, exceptions, C++ classes, Zig error unions, native window handles, `HANDLE`, `SOCKET`, Objective-C objects, or SDL opaque pointers to gameplay modules.

## Ownership model

- `Game` calls `bk_platform_get_api`, validates the ABI, and creates exactly one runtime instance.
- `PlatformRuntime` owns SDL initialization, the application window, the host event queue, cursor capture state, platform workers, and shared diagnostics.
- The SDL_GPU renderer borrows the application window through the existing renderer-neutral bridge; the platform ABI never transfers ownership.
- Input consumes normalized platform events. DirectInput is retained only as an optional Windows implementation during migration and is removed from the final playable graph.
- Networking owns opaque socket handles through the ABI. Packet formats remain in `Net` and do not move into the platform runtime.
- miniaudio remains the audio implementation. PlatformRuntime supplies allocation, clocks, atomics, and diagnostics; it does not become an audio engine.
- Dynamic module handles belong to PlatformRuntime. A successful load has one matching unload after module shutdown.

## Backend selection

`build.zig` selects source files from the resolved target, never from the host:

- Windows: `Platform/Windows/*.cpp` plus shared SDL services.
- Linux: `Platform/Posix/*.cpp`, `Platform/Linux/*.cpp`, and shared SDL services.
- macOS: `Platform/Posix/*.cpp`, `Platform/MacOS/*.cpp`, and shared SDL services.

Windows system libraries, resources, subsystem flags, `.def` files, MSVC CRT linkage, and SDK paths are constructed only for Windows targets. POSIX libraries are target guarded. Cross-compilation may prove compilation and package shape; runtime evidence requires a matching native host.

## Service groups

### Core runtime

Monotonic time, sleep, atomics, mutexes, events, worker threads, debugger detection, bounded diagnostics, environment lookup, executable path, and dynamic library loading.

### Storage and system

UTF-8 path normalization, file metadata, directory enumeration, writable user roots, dialogs, URL/file launch, and child process execution. Installed data remains read-only; saves, profiles, logs, screenshots, and caches use the writable user root.

### Application and events

SDL lifetime, application window, display modes, resize/minimize/focus state, keyboard, text, mouse, cursor capture, wheel, clipboard, controller, and quit events. Event records are fixed-layout and SDL-free at the ABI boundary.

### Networking

Socket creation, bind, broadcast, nonblocking mode, address conversion, send/receive, readiness polling, close, and portable error mapping. WinSock startup is private to the Windows backend; Linux and macOS use POSIX sockets.

### Audio support

Portable allocation callbacks, atomic completion flags, diagnostics, monotonic scheduling, and worker lifecycle for the existing miniaudio backend. Windows private heaps are an optional backend optimization and cannot affect public behavior.

## Migration strategy

Migration is vertical and test-driven. Each service first receives a failing ABI contract test, then a backend implementation, then one or more converted consumers. Direct OS calls remain temporarily allowed only in an explicit audit allowlist. Every module removes entries from that allowlist; no module adds broad compatibility shims.

`PortableCrt.h` remains a temporary bridge for legacy scalar types and harmless standard-library spelling differences. It must not grow implementations for windowing, input, sockets, threading, dynamic loading, files, processes, audio allocation, or diagnostics. The final audit permits native APIs only beneath `Sources/src/Platform` and narrowly approved Windows adapters.

## Error policy

Every ABI operation returns a stable `BkPlatformResult`. Detailed UTF-8 diagnostics are copied into a caller-owned buffer. Expected host conditions such as missing files, unavailable clipboard contents, nonblocking socket progress, minimized windows, and absent controllers have explicit result values. Programmer errors, stale opaque handles, ownership violations, and calls before initialization fail deterministically in Debug tests.

No platform exception crosses the ABI. Platform callbacks do not unwind into SDL, miniaudio, or gameplay code.

## Testing strategy

Each service has three levels of evidence:

1. Pure contract tests validate layout, version negotiation, handles, errors, and deterministic state without native UI or network access.
2. Native platform tests exercise the selected backend on Windows, Linux, and macOS.
3. Module integration tests load the actual runtime DLL/shared library, build the real consumer module, and verify startup/shutdown and ownership.

The completion matrix requires:

- Windows x64 native compile and runtime regression;
- Linux x64 native compile, package, launch, menu, representative mission, save/load, and shutdown;
- macOS arm64 compile/package in GitHub Actions and native human gameplay acceptance on an Apple-Silicon Mac;
- zero forbidden platform-token hits in playable source lists;
- no Windows system library in Linux/macOS link commands or staged packages;
- clean repeated startup/shutdown, resize, focus, controller, audio, and network lifecycle tests.

## Scope

Included: playable `Game`, runtime modules, SDL application/event services, Input, Net, SFX/miniaudio support, runtime paths/files, dynamic loading, dialogs/process services, module exports, packaging, and three-platform acceptance.

Excluded: MFC editors, developer utilities on non-Windows targets, the legacy D3D renderer on Linux/macOS, protocol redesign, a new audio engine, direct Cocoa/X11/Wayland code, installers, signing/notarization, Intel macOS, and gameplay changes.

## Completion definition

The abstraction is complete when playable modules include no OS SDK headers, contain no direct native calls, expose no native types, and link only against engine libraries, SDL-dependent renderer/runtime libraries, and `PlatformRuntime`. Windows, Linux, and macOS then pass their native gates with the same save data, control identifiers, network protocol, and gameplay behavior.
