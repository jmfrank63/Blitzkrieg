# Linux and macOS Platform Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement exactly one packet at a time. Do not redesign the architecture or combine packets. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the complete SDL_GPU game build, package, launch, play a representative mission, save, and exit natively on x86_64 Linux and Apple-Silicon macOS while preserving the accepted Windows 11 x64 runtime.

**Architecture:** A small shared platform layer replaces Win32 process, timing, synchronization, dynamic-module, path, and window services. SDL3 owns the application window and translates events; the existing engine interfaces remain the gameplay boundary. The one Zig SDL_GPU renderer borrows the SDL window and selects DXIL, SPIR-V, or MSL assets without backend-native graphics calls.

**Tech Stack:** Zig 0.16.0 build graph and tools, C++17 legacy modules, vendored `zig-sdl3` and SDL 3.4.0, SDL_GPU, SDL_shadercross commit `e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba`, canonical HLSL, miniaudio, WinSock/POSIX sockets, Windows x64, Linux x64, and macOS arm64 native runners.

---

## Authoritative documents

Read these before executing any packet:

1. `docs/superpowers/plans/2026-07-30-sdl-gpu-renderer/NEXT.md`
2. `docs/superpowers/evidence/sdl-gpu/portability-audit.md`
3. `docs/superpowers/plans/2026-08-02-linux-macos-platform-port/README.md`
4. `docs/superpowers/plans/2026-08-02-linux-macos-platform-port/EXECUTION.md`
5. The selected phase's `MANIFEST.md`
6. The selected packet

If a packet conflicts with this README or the accepted SDL_GPU handoff, stop and report the exact conflict. Do not resolve architectural conflicts inside a packet.

## Milestone boundary

This corpus ports the playable game runtime. It includes:

- `Game` startup, SDL window ownership, event loop, display modes, and shutdown;
- the runtime shared modules loaded by `Game`;
- input, miniaudio playback, file/storage/config/save paths, networking, dynamic module loading, dialogs, process/URL launch, and diagnostics;
- DXIL, SPIR-V, and MSL shader artifacts selected by SDL_GPU;
- portable Linux directory packaging and a macOS `.app` bundle;
- native automated and human acceptance on Windows x64, Linux x64, and macOS arm64.

It excludes editors, MFC authoring tools, the Windows-only legacy renderer on non-Windows targets, the `BuildVersion`, `BetaKeyGen`, and `FontGen` developer utilities on non-Windows targets, a second renderer, direct backend graphics code, Intel macOS, Linux arm64, installers, signing/notarization, auto-update, dedicated-server support, and gameplay changes.

The milestone is complete only after native Linux and native Apple-Silicon human gates are approved and the final Windows regression is green.

## Global invariants

- `build.zig` and every tool it invokes use Zig build steps, Zig executables, and repository inputs only. They must not launch PowerShell, Bash, `cmd`, CMake, Ninja, `ln`, package managers, or platform shell builtins.
- Generated files live only under `.zig-cache*`, `zig-out`, or an explicitly ignored evidence-capture directory.
- SDL3 owns `SDL_Init`, the `SDL_Window`, the event pump, and `SDL_Quit` on the main thread.
- `GraphicsEngineGpu` borrows `SDL_Window *`; it does not create, adopt, show, hide, destroy the application window, or initialise/quit SDL video.
- Zig exclusively owns SDL_GPU devices, swapchains, command buffers, shaders, pipelines, and GPU resources.
- No direct D3D12, Vulkan, Metal, DXGI, Cocoa, X11, Wayland, or Win32 rendering call is added.
- Canonical shaders remain HLSL. Runtime code loads offline artifacts and never compiles shaders.
- Windows uses DXIL/`direct3d12`, Linux uses SPIR-V/`vulkan`, and macOS uses MSL/`metal` for acceptance.
- Cross-compilation proves compilation and package shape only. Runtime acceptance is recorded only on a matching native host.
- Public engine/module interfaces use fixed-width legacy-compatible types. Native handles do not cross new portable boundaries.
- Windows resources, subsystem settings, `.def` handling, SDK paths, CRT libraries, and system libraries are target-guarded.
- Default and `game-all` builds instantiate only the selected renderer and playable runtime graph. Windows-only legacy-renderer and developer-utility steps remain explicit and are never constructed for Linux/macOS game builds.
- Runtime module discovery uses platform filename policy and the existing `GetModuleDescriptor` contract; gameplay modules are not statically fused.
- Data paths are UTF-8 at the platform boundary. Legacy backslashes are normalized before touching the host filesystem.
- Installed game data is read-only. Config, logs, profiles, saves, and caches use a writable per-user root outside the package.
- Keybind names and legacy numeric control IDs remain stable so existing configuration files continue to load.
- miniaudio remains the audio backend. No second audio library is introduced.
- Network packet formats and multiplayer protocol values remain unchanged.
- Existing Windows renderer and gameplay acceptance remains a regression gate throughout the port.
- Human visual or playability approval is never inferred from automated output.

## Stable file map

### Build and host tools

- `build.zig` — target-aware build graph, platform source selection, test and package steps.
- `build.zig.zon` — pinned dependencies only; no host bootstrap scripts.
- `tools/zig/build_support.zig` — target classification, suffixes, source/link policy, native-run eligibility.
- `tools/zig/build_hermeticity_test.zig` — rejects shell/process dependencies from the build path.
- `tools/zig/stage.zig` — shell-free target-specific runtime staging.
- `tools/zig/package.zig` — deterministic archives and package metadata.
- `tools/zig/compile_gfxgpu_shaders.zig` — deterministic multi-format manifest writer driven by a Zig-built shadercross artifact.
- `tools/zig/platform_*.cpp` and `tools/zig/platform_*.zig` — focused platform tests.

### Portable compiler and runtime boundary

- `Sources/src/Platform/Compiler.h` — calling convention, export visibility, compiler feature, and old spelling compatibility.
- `Sources/src/Platform/LegacyTypes.h` — fixed-width legacy value types.
- `Sources/src/Platform/LegacyVariant.h/.cpp` — non-Windows `variant_t` subset matching used engine semantics.
- `Sources/src/Platform/Clock.h/.cpp` — monotonic milliseconds/nanoseconds and sleep.
- `Sources/src/Platform/Sync.h/.cpp` — event, mutex, lock, and worker-thread primitives.
- `Sources/src/Platform/Debug.h/.cpp` — bounded diagnostic output and debugger query.
- `Sources/src/Platform/DynamicLibrary.h/.cpp` — SDL-backed load/symbol/unload wrapper.
- `Sources/src/Platform/Paths.h/.cpp` — base/data and writable user roots plus separator normalization.
- `Sources/src/Platform/System.h/.cpp` — dialogs, URL/file launch, environment, executable path, and child-process execution.
- `Sources/src/Platform/Socket.h`, `SocketWin32.cpp`, and `SocketPosix.cpp` — fixed-width socket boundary and implementations.

### SDL application and input

- `Sources/src/Platform/Event.h` — SDL-free fixed-layout event records.
- `Sources/src/Platform/SDLApplication.h/.cpp` — SDL lifetime, owned window, event translation, and window operations.
- `Sources/src/Game/GameMain.h/.cpp` — common game startup and shutdown body.
- `Sources/src/Game/main.cpp` — portable entry point and thin Windows adapter.
- `Sources/src/Game/WinFrame.cpp` — Windows-only splash/resource adapter after window migration.
- `Sources/src/Input/InputCodes.h/.cpp` — stable legacy control IDs and SDL mapping.
- `Sources/src/Input/InputAPI.h/.cpp` — event-fed keyboard, mouse, text, clipboard, and controller state.

### Runtime modules with planned edits

- `Sources/src/Misc/Win32Helper.h`, `Thread.h/.cpp`, `HPTimer.cpp`, `FileUtils.h/.cpp`, `StrProc.cpp`, and `ModernAssert.h` — platform primitives.
- `Sources/src/StreamIOZig/streamio.zig`, `legacy_bridge.cpp`, and `options_bridge.cpp` — portable file metadata/enumeration and variant/display options.
- `Sources/src/Main/LoadDLLs.cpp`, `Initialization.cpp`, `iMainInternal.cpp`, and `MainLoopCommands.cpp` — modules, paths, clocks, dialogs, shutdown.
- `Sources/src/Net/NetLowest.h/.cpp`, `NetA4.cpp`, `NetAcks.h/.cpp`, and `NetLogin.cpp` — sockets and workers.
- `Sources/src/SFX/AudioBackendOpen.cpp`, `StreamFadeOff.h/.cpp`, and `SoundEngine.cpp` — miniaudio and worker lifecycle.
- `Sources/src/Common/InterfaceScreenBase.cpp`, `Sources/src/Scene/OpenVideoPlayer.cpp`, `Sources/src/Scene/Transition.cpp`, selected `Sources/src/GameTT` files, and selected `Sources/src/RandomMapGen` files — direct platform-call residue.

### Renderer and shaders

- `Sources/src/GFX/GFXPlatform.h` — opaque borrowed application-window boundary.
- `Sources/src/GFXGPU/GraphicsEngineGpu.h/.cpp` — SDL window borrower and engine adapter.
- `Sources/src/GFXGPU/shader_manifest.zig` — backend-format manifest records.
- `Sources/src/GFXGPU/shaders.zig` — artifact validation and selected-format shader creation.
- `Sources/src/GFXGPU/device.zig` — requested/selected SDL_GPU driver and shader format.
- `Sources/src/GFXGPU/shaders/manifest.json` and `*.hlsl` — logical records and canonical sources.

### Evidence

- `docs/superpowers/evidence/platform-port/build-hermeticity.md`
- `docs/superpowers/evidence/platform-port/target-matrix.md`
- `docs/superpowers/evidence/platform-port/linux-acceptance.md`
- `docs/superpowers/evidence/platform-port/macos-acceptance.md`
- `docs/superpowers/evidence/platform-port/windows-regression.md`

## Core platform contracts

`Compiler.h` owns these spellings. Existing interfaces continue to use `STDCALL`, which aliases `BK_STDCALL`:

```cpp
#if defined(_WIN32)
#define BK_CDECL __cdecl
#define BK_STDCALL __stdcall
#define BK_EXPORT __declspec(dllexport)
#define BK_IMPORT __declspec(dllimport)
#define BK_NORETURN __declspec(noreturn)
#else
#define BK_CDECL
#define BK_STDCALL
#define BK_EXPORT __attribute__((visibility("default")))
#define BK_IMPORT
#define BK_NORETURN [[noreturn]]
#endif
#define STDCALL BK_STDCALL
```

`Event.h` is SDL-free and fixed-layout:

```cpp
namespace NPlatform {
enum class EventType : uint32_t {
    None, Quit, WindowShown, WindowHidden, WindowMoved, WindowResized,
    WindowMinimized, WindowRestored, FocusGained, FocusLost,
    KeyDown, KeyUp, TextInput, MouseMove, MouseButtonDown,
    MouseButtonUp, MouseWheel, ControllerAdded, ControllerRemoved,
    ControllerButtonDown, ControllerButtonUp, ControllerAxis
};
struct Event {
    EventType type;
    uint64_t timestamp_ns;
    int32_t a, b, c, d;
    uint32_t flags;
    char text_utf8[32];
};
}
```

The SDL window is borrowed through the established opaque renderer value:

```cpp
struct GFXNativeWindow {
    void *value; // borrowed SDL_Window* for SDL_GPU
};
```

Renderer shutdown releases the GPU window claim/device before `SDLApplication` destroys the window. No renderer path calls `SDL_DestroyWindow` or `SDL_QuitSubSystem`.

## Runtime path policy

| Purpose | Windows | Linux | macOS |
|---|---|---|---|
| executable/base | staged game directory | staged game directory | `Blitzkrieg.app/Contents/Resources` |
| read-only data | `<base>/Data` | `<base>/Data` | `<Resources>/Data` |
| shaders | `<base>/Shaders/GfxGpu` | `<base>/Shaders/GfxGpu` | `<Resources>/Shaders/GfxGpu` |
| writable config/save/log | SDL preference path | SDL preference path | SDL preference path |
| dynamic modules | executable directory | `<base>/lib` | `<app>/Contents/Frameworks` |

The SDL preference-path identifiers are `jmfrank63` and `Blitzkrieg`. `-Dportable-user-data=true` may redirect writable files to `<base>/UserData`; it is off by default and never modifies packaged `Data`.

## Shared module naming policy

| Logical module | Windows | Linux | macOS |
|---|---|---|---|
| StreamIO | `StreamIO.dll` | `libStreamIO.so` | `libStreamIO.dylib` |
| GFXGPU | `GFXGPU.dll` | `libGFXGPU.so` | `libGFXGPU.dylib` |
| Other runtime module `X` | `X.dll` | `libX.so` | `libX.dylib` |

Enumeration accepts only the current platform suffix, sorts paths before loading, ignores the executable and SDL runtime, requires `GetModuleDescriptor`, rejects duplicate module type IDs, and unloads in reverse order.

## Shader artifact policy

One source record produces:

```text
<effect>.<stage>.dxil   -> Windows/direct3d12
<effect>.<stage>.spv    -> Linux/vulkan
<effect>.<stage>.msl    -> macOS/metal
```

The generated manifest stores effect, stage, entry point, format, relative path, byte length, SHA-256, vertex mask, and binding counts. Duplicate `(effect, stage, format)` records, missing pairs, traversal paths, and hash/length mismatches fail.

## Phase graph

```text
00 hermetic build and portable ABI
              |
              v
01 runtime core -----> 02 storage/config/modules
              \               /
               v             v
              03 SDL application/window
                         |
                         v
                  04 input and audio
                         |
                         v
                05 network/system services
                         |
                         v
                  06 portable game link
                         |
                         v
                  07 backend shaders
                         |
                         v
                  08 Linux validation
                         |
                         v
             09 macOS and release validation
```

Complete each phase before starting the next. Within a phase, obey its manifest. Parallel execution is allowed only for packets with no dependency path and no overlapping files.

## Phase completion contracts

| Phase | Required gate |
|---|---|
| 00 | Build shell audit passes; target classifier and portable header tests pass on all triples |
| 01 | clocks, sync, debug, dynamic libraries, and system facade pass native tests with Windows green |
| 02 | storage/config/save and module discovery pass with no COM dependency on Linux/macOS |
| 03 | SDL owns one window; event/resize/fullscreen and borrowed renderer-window smoke pass natively |
| 04 | stable input mapping and miniaudio lifecycle tests pass on native Linux |
| 05 | socket loopback and system-service tests pass natively with protocol values unchanged |
| 06 | every playable runtime module and `Game` links for all three targets |
| 07 | deterministic DXIL/SPIR-V/MSL manifests pass and SDL_GPU selects each native driver/format |
| 08 | native Linux automatic and human gates are accepted |
| 09 | native Apple-Silicon gates, Windows regression, packages, CI, and release checklist pass |

## Packet index

- `phase-00-hermetic-build`: P00-M01 through P00-M05
- `phase-01-runtime-core`: P01-M01 through P01-M05
- `phase-02-storage-config-modules`: P02-M01 through P02-M06
- `phase-03-sdl-application`: P03-M01 through P03-M05
- `phase-04-input-audio`: P04-M01 through P04-M06
- `phase-05-network-system`: P05-M01 through P05-M05
- `phase-06-portable-game-link`: P06-M01 through P06-M05
- `phase-07-backend-shaders`: P07-M01 through P07-M05
- `phase-08-linux-validation`: P08-M01 through P08-M05
- `phase-09-macos-release`: P09-M01 through P09-M06

This is 53 independently reviewable packets. A coordinator gives an implementer exactly one packet plus the authoritative documents. The next packet starts only after verification and review.
