# Linux and macOS Platform Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Run the SDL_GPU game natively on x86_64 Linux and Apple-Silicon macOS, while preserving the accepted Windows 11 build and retaining one renderer implementation.

**Architecture:** Introduce a small SDL3-based platform layer owning the window, event pump, paths, and process-facing services. The legacy game continues to use opaque handles and existing interfaces; platform-specific code is isolated behind the new layer. The renderer receives an `SDL_Window *` through the existing opaque `GFXNativeWindow` boundary, so SDL_GPU selects D3D12, Vulkan, or Metal without game-side graphics API branches.

**Tech Stack:** Zig build system and cross-compilation, C++ legacy game sources, vendored SDL3/SDL_GPU, current open audio backend, POSIX sockets on Linux/macOS, native Linux and macOS test hosts.

---

## Locked decisions and acceptance boundary

- Linux is the first runnable deliverable; macOS follows the same platform abstraction after the Linux smoke gate is green.
- SDL3 is the only new cross-platform runtime surface. Do not add PowerShell, Bash, CMake, package-manager, or platform shell calls to `build.zig`.
- The SDL_GPU core remains graphics-API neutral. It must not gain direct D3D, Vulkan, Metal, X11, Wayland, Cocoa, or Win32 graphics calls.
- Windows remains a supported target and keeps its current splash/resource behavior behind a Windows-only implementation.
- Cross-compilation is a compile gate, not a runtime claim. Linux and macOS acceptance requires a native host or CI runner for the executable, mission, and lifecycle checks.
- Editors, legacy tools, and Windows-only resource compilers are out of the initial game-port critical path. The deliverable is `Game`, its runtime data, and its supported launcher/package layout.

## Phase 0 — establish a reproducible target matrix

**Files:**
- Modify: `build.zig`
- Create: `docs/superpowers/evidence/platform-port/target-matrix.md`
- Create: `tools/zig/platform_build_matrix_test.zig`

1. Add explicit `-Dtarget` validation for the supported game triples: `x86_64-windows-msvc`, `x86_64-linux-gnu`, and `aarch64-macos`.
2. Keep the host-default target behavior, but fail early with an actionable error for a target outside the supported matrix. Do not inspect the host with shell commands.
3. Add a Zig test that validates the target-selection helper and rejects Windows-only link/resource configuration for Linux/macOS.
4. Document the compiler/sysroot/runtime prerequisites per target in the matrix, including which checks are cross-compile-only and which need native runners.
5. Verify:

   ```powershell
   zig build test-platform-build-matrix
   zig build -Dtarget=x86_64-windows-msvc -Doptimize=Debug
   zig build -Dtarget=x86_64-linux-gnu -Doptimize=Debug
   zig build -Dtarget=aarch64-macos -Doptimize=Debug
   ```

   Expected: the matrix test passes; Windows builds as before; the two non-Windows builds either compile or report the next missing source/sysroot dependency without invoking a shell.
6. Commit: `build: define supported game target matrix`.

## Phase 1 — move executable startup and window ownership behind SDL3

**Files:**
- Create: `Sources/src/Platform/Platform.h`
- Create: `Sources/src/Platform/Platform.cpp`
- Create: `Sources/src/Platform/PlatformWin32.cpp`
- Create: `Sources/src/Platform/PlatformSDL.cpp`
- Create: `Sources/src/Game/GameMain.cpp`
- Modify: `Sources/src/Game/main.cpp`
- Modify: `Sources/src/Game/WinFrame.h`
- Modify: `Sources/src/Game/WinFrame.cpp`
- Modify: `Sources/src/GFX/GFXPlatform.h`
- Modify: `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`
- Modify: `build.zig`
- Create: `tools/zig/platform_window_test.cpp`

1. Define the smallest platform contract in `Platform.h`: startup arguments, process working/data directory discovery, one owned `SDL_Window *`, event pumping, window visibility/resize/fullscreen requests, and a quit signal. Keep native handles private to the Windows implementation.
2. Extract the common body of `WinMain` from `Game/main.cpp` into `GameMain(argc, argv, Platform&)`. Add a conventional `main(int, char**)` for Linux/macOS and retain a thin Windows `WinMain` adapter that translates its command line then calls the same body.
3. Make `PlatformWin32.cpp` preserve the existing splash-screen and Windows resource behavior. Make `PlatformSDL.cpp` create and own the SDL window directly; it must not request `SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER`.
4. Change `GFXNativeWindow` documentation and construction so it carries an `SDL_Window *` for the SDL_GPU renderer. Update `GraphicsEngineGpu::Init` to consume that window without reinitialising video, wrapping an HWND, or destroying a window it does not own.
5. Route the existing `NWinFrame` calls through the platform contract during the transition, keeping old game code compiling until later input/window work replaces each use.
6. Add a headless-safe window lifecycle test: initialise SDL video, create a hidden window, resize it, poll events, destroy it, and assert no SDL error. Register it as `zig build test-platform-window`.
7. Verify:

   ```powershell
   zig build test-platform-window -Dtarget=x86_64-windows-msvc
   zig build test-gfxgpu -Dtarget=x86_64-windows-msvc -Doptimize=Debug
   zig build -Dtarget=x86_64-linux-gnu -Doptimize=Debug
   ```

   Expected: Windows GPU smoke remains green; Linux no longer depends on `WinMainCRTStartup`, `addWin32ResourceFile`, or an adopted HWND.
8. Commit: `platform: make SDL own the game window`.

## Phase 2 — make the build graph platform-aware without external scripts

**Files:**
- Modify: `build.zig`
- Modify: `Sources/src/Game/GameVersion.rc`
- Modify: `Sources/src/Game/SplashResources.rc`
- Create: `tools/zig/package_layout_test.zig`
- Create: `docs/superpowers/evidence/platform-port/package-layout.md`

1. Split Windows-only C/C++ defines, MSVC include discovery, library paths, Windows system libraries, executable subsystem/entry settings, and `.rc` resource steps from common compile configuration.
2. Retain compatibility flags only where the legacy C++ source demonstrably needs them; do not leak `_WIN32`, `_WINDOWS`, MSVC SDK include paths, `winmm`, `user32`, or `advapi32` into non-Windows targets.
3. Define Zig install/package steps for each target that copy `Game`, DLL/dylib dependencies, `data/`, and the SDL GPU shader directory into a deterministic runtime root. Use `std.Build` install/copy/write APIs only.
4. Add a Zig layout test that asserts required files exist and rejects Windows `.exe`, `.dll`, and resource artifacts in Linux/macOS staging roots.
5. Document expected layouts: Linux portable directory first; macOS `.app` bundle as the macOS delivery format after the native executable works.
6. Verify:

   ```powershell
   zig build test-package-layout
   zig build package-game -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseSafe
   zig build package-game -Dtarget=x86_64-linux-gnu -Doptimize=ReleaseSafe
   zig build package-game -Dtarget=aarch64-macos -Doptimize=ReleaseSafe
   ```

   Expected: each target stages only its intended assets and loader dependencies; generated staging/cache directories stay ignored.
7. Commit: `build: package game for supported platforms`.

## Phase 3 — replace platform services at their actual call sites

**Files:**
- Modify: `Sources/src/Input/InputAPI.cpp`
- Modify: `Sources/src/Input/InputAPI.h`
- Modify: `Sources/src/Net/NetLowest.cpp`
- Modify: `Sources/src/Net/NetLowest.h`
- Modify: `Sources/src/StreamIO/FileSystem.cpp`
- Modify: `Sources/src/StreamIO/FileSystem.h`
- Modify: `Sources/src/Misc/FileUtils.cpp`
- Modify: `Sources/src/Main/Initialization.cpp`
- Modify: `Sources/src/Game/main.cpp`
- Modify: `Sources/src/Game/SysKeys.cpp`
- Create: `Sources/src/Platform/PlatformInput.cpp`
- Create: `Sources/src/Platform/PlatformPaths.cpp`
- Create: `Sources/src/Platform/PlatformSockets.cpp`
- Create: `tools/zig/platform_services_test.cpp`

1. Translate SDL keyboard, mouse, text-input, clipboard, cursor, focus, controller, close, resize, minimise, and restore events into the existing input/window interfaces. Preserve Windows message handling only in `PlatformWin32.cpp` until every consumer is migrated.
2. Replace `DirectInput8Create` in `InputAPI.cpp` with the SDL-backed input implementation and cover mouse coordinates, button edges, wheel, text, and focus loss in a deterministic event-injection test.
3. Introduce a platform-path service with read-only data root, writable config/save/cache root, UTF-8 paths, separator normalization at the boundary, and an explicit current-directory policy. Route `FileSystem.cpp`, `FileUtils.cpp`, startup logging, and save/config locations through it instead of `CreateFile`, `GetCurrentDirectory`, and backslash-built paths.
4. Replace Winsock startup/cleanup and socket aliases in `NetLowest.cpp` with a small socket adapter. Map error handling, nonblocking mode, close, and address conversion for WinSock and POSIX; test loopback connect/send/receive/close on each native target.
5. Move system dialogs, URL/process launch, crash-report handoff, and debugger tracing behind named platform functions. Preserve the in-game UI message boxes; only replace OS-dialog calls.
6. Add service tests for writable-path creation, data-root lookup, separator conversion, synthetic input events, and loopback networking. Register `zig build test-platform-services`.
7. Verify:

   ```powershell
   zig build test-platform-services -Dtarget=x86_64-windows-msvc
   zig build -Dtarget=x86_64-linux-gnu -Doptimize=Debug
   zig build -Dtarget=aarch64-macos -Doptimize=Debug
   ```

   Expected: the game target has no unconditional DirectInput, WinSock, `CreateFile`, or current-directory dependency outside the Windows platform implementation.
8. Commit: `platform: abstract input paths and sockets`.

## Phase 4 — make renderer assets backend-neutral and prove Linux graphics

**Files:**
- Modify: `Sources/src/GFXGPU/GfxGpu.zig`
- Modify: `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`
- Modify: `Sources/src/GFXGPU/GfxGpuAbi.h`
- Modify: `Sources/src/GFXGPU/Shaders/manifest.json`
- Create: `tools/zig/gfxgpu_shader_manifest_test.zig`
- Modify: `build.zig`
- Create: `docs/superpowers/evidence/platform-port/linux-smoke.md`

1. Audit the present DXIL-only shader entries and define one manifest record per logical shader: source/identity, vertex layout, expected byte length/hash, and backend artifact paths. Do not fork draw code by backend.
2. Generate or check in the required SPIR-V artifacts from the canonical shader sources, then teach the existing shader loader to select artifacts from SDL_GPU's chosen driver while retaining hash/length validation.
3. Keep Windows/D3D12's accepted assets untouched. Fail clearly if a required driver artifact is absent rather than silently falling back to a different renderer.
4. Add manifest validation for duplicate logical shaders, missing backend artifacts, incorrect hash/length, and unsupported driver selection. Register `zig build test-gfxgpu-shaders`.
5. On a native x86_64 Linux host, run the existing GPU factory test and the game startup/reference-scene smoke with Vulkan software or hardware support. Capture driver name, SDL error stream, shader selected, frame count, and live-object count as evidence.
6. Verify:

   ```powershell
   zig build test-gfxgpu-shaders
   zig build test-gfxgpu -Dtarget=x86_64-linux-gnu -Doptimize=Debug
   zig build package-game -Dtarget=x86_64-linux-gnu -Doptimize=Debug
   ```

   Expected: compilation passes everywhere; native Linux reaches the existing renderer smoke without Windows handles or DXIL-only asset failures.
7. Commit: `gfx: add Linux SDL GPU shader assets`.

## Phase 5 — Linux full-game acceptance

**Files:**
- Create: `tools/zig/linux_game_acceptance.zig`
- Create: `docs/superpowers/evidence/platform-port/linux-uat.md`
- Modify: `build.zig`
- Modify only the platform/service files identified by failures from the acceptance run.

1. Add an opt-in `zig build test-game-linux` orchestration step that launches the packaged game with the existing startup-smoke/reference-scene flags, bounded timeout, captured logs, and explicit shutdown request. Implement the launcher in Zig; do not use Bash.
2. Run the native manual matrix: startup/menu, representative mission, save/load, audio device selection/loss where available, keyboard/mouse/text input, controller mapping, resize, minimise/restore, windowed/fullscreen, network loopback/lobby path, and clean shutdown.
3. Record target hardware/driver/session type (X11 or Wayland), commands, results, and failures in `linux-uat.md`; attach only stable textual diagnostics to the repository, not captures or caches.
4. Fix only confirmed Linux portability failures, extending a focused automated test before each fix when a non-interactive reproduction exists.
5. Verify:

   ```powershell
   zig build test-platform-window -Dtarget=x86_64-linux-gnu
   zig build test-platform-services -Dtarget=x86_64-linux-gnu
   zig build test-game-linux -Dtarget=x86_64-linux-gnu -Doptimize=Debug
   ```

   Expected: Linux passes the automatic smoke and the human matrix with zero renderer live objects after shutdown.
6. Commit: `test: accept Linux game platform`.

## Phase 6 — Apple-Silicon/macOS implementation and acceptance

**Files:**
- Modify: `Sources/src/Platform/PlatformSDL.cpp`
- Create: `Sources/src/Platform/PlatformMacOS.mm` only if an SDL API cannot supply a required macOS integration
- Modify: `build.zig`
- Modify: `Sources/src/GFXGPU/Shaders/manifest.json`
- Create: `tools/zig/macos_bundle_test.zig`
- Create: `docs/superpowers/evidence/platform-port/macos-uat.md`

1. First compile `aarch64-macos` using an explicit licensed macOS SDK/sysroot on a macOS runner. Keep all toolchain discovery as user/CI configuration, not build-script shell probing.
2. Run the Phase 1 SDL platform implementation unchanged where SDL supplies the behavior. Add Objective-C++ only for a documented macOS-specific requirement such as bundle resource lookup, application lifecycle integration, or OS dialog behavior; keep it behind `Platform.h`.
3. Produce Metal-compatible shader artifacts from the same logical manifest and validate their hashes/lengths. Do not introduce Metal calls in the Zig renderer core.
4. Extend the package step to assemble `Blitzkrieg.app/Contents/MacOS`, `Resources`, shader assets, and required dylibs with correct relative loader paths. Add `test-macos-bundle` to validate the bundle contents without launching it.
5. On a native Apple-Silicon runner, execute the same automatic smoke and human acceptance matrix as Linux, including Retina/high-DPI resize and application focus/quit behavior.
6. Verify:

   ```powershell
   zig build test-platform-window -Dtarget=aarch64-macos
   zig build test-platform-services -Dtarget=aarch64-macos
   zig build test-macos-bundle -Dtarget=aarch64-macos
   zig build package-game -Dtarget=aarch64-macos -Doptimize=Debug
   ```

   Expected: the `.app` bundle is self-contained for the declared runtime dependencies; native macOS reaches the renderer smoke using SDL_GPU/Metal and exits cleanly.
7. Commit: `platform: support Apple Silicon game runtime`.

## Phase 7 — regression gates and release handoff

**Files:**
- Modify: `build.zig`
- Modify: `.github/workflows/ci.yml` (or the repository's existing CI configuration)
- Modify: `README.md`
- Modify: `docs/superpowers/evidence/platform-port/target-matrix.md`
- Create: `docs/superpowers/evidence/platform-port/release-checklist.md`

1. Add CI compile/package jobs for Windows x64, Linux x64, and macOS arm64. Run native smoke jobs only on matching runners; label cross-build jobs as compile-only.
2. Keep the accepted Windows renderer tests, and require the common platform unit tests for every target.
3. Document supported OS/architecture/driver expectations, data placement, launch command, known exclusions, and debugging/log location for each platform.
4. Add a release checklist requiring clean working tree, ignored-cache verification, package-layout validation, Windows regression evidence, native Linux evidence, native macOS evidence, and human approval.
5. Verify the full target matrix using the exact commands in the evidence document and ensure all generated `zig-out`, `.zig-cache*`, package staging, and captures remain ignored and hidden by the workspace settings.
6. Commit: `ci: verify supported game platforms`.

## Completion criteria

The port is complete only when:

- `Game` builds/packages through Zig for Windows x64, Linux x64, and macOS arm64 without build-time PowerShell/Bash dependencies.
- The production renderer remains the one SDL_GPU implementation and receives an SDL-owned window, not a platform graphics handle.
- Native Linux and Apple-Silicon macOS both pass startup, menu, representative mission, input, audio, resize/minimise/restore, fullscreen, save path, clean-shutdown, and renderer live-count acceptance.
- Windows renderer acceptance remains green, and the platform-specific code is confined to the platform layer/build conditionals.
- Repository and VS Code status remain free of generated caches and staging artifacts.
