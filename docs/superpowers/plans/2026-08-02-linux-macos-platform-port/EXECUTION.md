# Cross-Platform Execution Contract

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`. Execute one assigned packet; do not select the next packet yourself.

**Goal:** Produce one small, reviewable, tested portability change without weakening Windows or introducing host-shell dependencies.

**Architecture:** The coordinator owns decomposition and architecture. The implementer owns test-first changes inside the assigned packet's file boundary.

**Tech Stack:** Git, Zig 0.16.0, C++17, SDL 3.4.0/SDL_GPU, SDL_shadercross, miniaudio, native Windows/Linux/macOS runners.

---

## Start protocol

```text
git status --short
git branch --show-current
git log -1 --oneline
zig version
```

Required state:

- branch is `linux-macos-platform` unless the coordinator names another task branch;
- Zig reports `0.16.0`;
- no unexplained change overlaps allowed files;
- dependency commits named in the manifest are present.

Read the assigned packet in full. Extract objective, dependencies, allowed files, commands, execution class, and evidence. If an allowed file has user changes, stop and report the path and diff summary.

## Implementation loop

1. Add the smallest focused test.
2. Run the narrow command and record the expected failure.
3. Implement only enough to pass.
4. Run the narrow command again.
5. Run the phase regression command.
6. Run `zig build audit-build-hermeticity` after build-path changes.
7. Run `git diff --check`, inspect `git diff --stat`, and confirm generated outputs are ignored.
8. Commit only packet files with the exact subject stated by the packet.

## Change boundaries

Allowed: packet files, direct test/evidence files, and dependency hashes explicitly owned by the packet.

Prohibited: broad formatting, gameplay/data/visual changes, editor portability, second graphics/audio implementations, shell-launched build operations, CMake/Ninja/Make/package-manager dependencies, backend graphics calls, native handles in portable headers, network/save/config format changes, weakened tests, and generated artifacts.

If a required file is outside scope, report `PACKET BLOCKED` with packet, file, reason, and smallest boundary change. Do not edit it.

## Hermetic build rule

The audit scans `build.zig`, build support, and every reachable Zig build tool. These executable/process tokens are forbidden:

```text
pwsh powershell powershell.exe bash sh cmd cmd.exe
cmake ninja make ln xcopy robocopy
```

`zig`, Zig-built artifacts via `addRunArtifact`, and the produced game are allowed. Dependencies arrive only through pinned `build.zig.zon` entries; tools do not clone, fetch, or install packages.

## Target execution classes

- `pure`: host-independent test;
- `compile`: target build without execution;
- `native`: target executes only when OS and CPU match host;
- `human`: named reviewer on matching host.

`tools/zig/build_support.zig` rejects non-native execution. A cross-build is never reported as a runtime pass.

## Testing tiers

### Tier A — hermetic and pure

```text
zig build audit-build-hermeticity
zig build test-build-support
zig build test-platform-core
zig build test-platform-files
```

### Tier B — compile matrix

```text
zig build game-all -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Dtest-mode=compile
zig build game-all -Dtarget=x86_64-linux-gnu -Doptimize=Debug -Dtest-mode=compile
zig build game-all -Dtarget=aarch64-macos -Doptimize=Debug -Dtest-mode=compile
```

macOS needs a configured SDK/sysroot and normally compiles on a licensed macOS runner.

### Tier C — native services

```text
Windows x86_64:
zig build test-platform-window -Dtarget=x86_64-windows-msvc -Dtest-mode=run
zig build test-platform-input -Dtarget=x86_64-windows-msvc -Dtest-mode=run
zig build test-platform-audio -Dtarget=x86_64-windows-msvc -Dtest-mode=run
zig build test-platform-network -Dtarget=x86_64-windows-msvc -Dtest-mode=run

Linux x86_64:
zig build test-platform-window -Dtarget=x86_64-linux-gnu -Dtest-mode=run
zig build test-platform-input -Dtarget=x86_64-linux-gnu -Dtest-mode=run
zig build test-platform-audio -Dtarget=x86_64-linux-gnu -Dtest-mode=run
zig build test-platform-network -Dtarget=x86_64-linux-gnu -Dtest-mode=run

macOS arm64:
zig build test-platform-window -Dtarget=aarch64-macos -Dtest-mode=run
zig build test-platform-input -Dtarget=aarch64-macos -Dtest-mode=run
zig build test-platform-audio -Dtarget=aarch64-macos -Dtest-mode=run
zig build test-platform-network -Dtarget=aarch64-macos -Dtest-mode=run
```

### Tier D — SDL_GPU smoke

```text
zig build gfxgpu-smoke -Dtarget=x86_64-windows-msvc -Dtest-mode=run -Dgpu-driver=direct3d12
zig build gfxgpu-smoke -Dtarget=x86_64-linux-gnu -Dtest-mode=run -Dgpu-driver=vulkan
zig build gfxgpu-smoke -Dtarget=aarch64-macos -Dtest-mode=run -Dgpu-driver=metal
```

Required startup: `GFXGPU abi=1 driver=<name> shader_format=<dxil|spirv|msl> debug=<0|1>`. Required shutdown has all live counts zero.

### Tier E — full game smoke

```text
zig build install-game -Dtarget=x86_64-windows-msvc -Doptimize=Debug
zig build test-game-smoke -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Dtest-mode=run
zig build install-game -Dtarget=x86_64-linux-gnu -Doptimize=Debug
zig build test-game-smoke -Dtarget=x86_64-linux-gnu -Doptimize=Debug -Dtest-mode=run
zig build install-game -Dtarget=aarch64-macos -Doptimize=Debug
zig build test-game-smoke -Dtarget=aarch64-macos -Doptimize=Debug -Dtest-mode=run
```

The Zig runner starts the staged game with existing smoke/reference flags, captures logs, enforces a timeout, requests normal shutdown, and fails on missing checkpoints.

### Tier F — human acceptance

Only P08-M04/P08-M05 and P09-M04/P09-M05 claim native visual/playability acceptance. The implementer prepares the package, command, log, deterministic scene, and checklist; a human records accepted/rejected.

## Evidence record

```text
PACKET COMPLETE
packet: <ID>
commit: <hash> <subject>
changed files:
- <path>
tests:
- <command> => PASS (<key output>)
cross-target checks:
- <target> => COMPILE PASS or NOT RUN (<reason>)
evidence:
- <artifact or diagnostic fact>
remaining concerns:
- none
```

Use a concrete concern when one exists. Never claim native or human success from compilation.

## Error and logging policy

- Platform failures expose bounded UTF-8 diagnostics.
- SDL failures include operation and `SDL_GetError()`.
- Socket failures include normalized operation and native numeric error.
- Module failures include logical name, resolved path, and loader diagnostic.
- Startup logs target, paths, SDL version, GPU driver/format, and audio backend/device.
- Shutdown logs module unload, audio stop, renderer live counts, window destruction, and SDL quit.
- No normal-mode per-frame, per-packet, or per-draw logging.

## Escalation triggers

Stop when pinned APIs change ownership, a build operation cannot be Zig-native, a native handle must escape, module registration cannot be preserved, variant use exceeds the fixed subset, input IDs cannot remain compatible, case-insensitive lookup cannot be normalized, miniaudio needs a native window, network bytes would change, SDL_GPU lacks the required format, validation reports lifetime/deadlock/device errors, a test fails three immediate runs, or human acceptance finds an unexplained regression.

## Review protocol

Review packet scope, test-first evidence, target classification, shell audit, ownership/destruction, type/calling convention, serialization/network compatibility, paths, module load order, SDL main-thread ownership, shader pairing, native logs, ignored artifacts, and commit scope. Rejected packets are amended before advancing.
