# Luna Execution Contract

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`. Execute one assigned packet; do not select the next packet yourself.

**Goal:** Make a small, reviewable, tested change that satisfies one renderer packet without changing architecture or unrelated code.

**Architecture:** The coordinator owns decomposition and architectural decisions. Luna owns test-first implementation inside the assigned packet's allowed files.

**Tech Stack:** Git, PowerShell 7, Zig 0.16, C++17, SDL3/SDL_GPU, SDL_shadercross.

---

## Start protocol

Run:

```powershell
git status --short
git branch --show-current
git log -1 --oneline
```

Required state:

- branch is `sdl3-gpu-renderer`;
- there are no unexplained changes in files allowed by the packet;
- every dependency commit named in the phase manifest is present.

Read the assigned packet in full. Extract:

- objective;
- allowed files;
- prohibited files;
- dependencies;
- exact tests;
- acceptance evidence.

If an allowed file already has uncommitted user changes, stop and report the path and diff summary. Never discard or overwrite user work.

## Implementation loop

For every behavior in the packet:

1. Add the smallest automated test that expresses the behavior.
2. Run the packet's narrow command and capture the expected failure.
3. Implement only enough production code to pass.
4. Run the narrow command again.
5. Run the phase regression command.
6. Inspect `git diff --check` and `git diff --stat`.
7. Commit only the packet's files with the exact commit prefix stated by the packet.

Keep functions cohesive and explicit. Prefer value types and bounded slices. Any unsafe pointer conversion must be confined to `abi.zig` or `sdl.zig`, include a precondition comment, and have a boundary test.

## Change boundaries

Allowed:

- files listed by the packet;
- one direct test file listed by the packet;
- `build.zig` only when explicitly listed;
- a generated lock or dependency hash file when the packet adds the dependency that creates it.

Not allowed:

- broad formatting;
- renaming unrelated interfaces;
- changing gameplay or asset semantics;
- changing public ABI field order;
- importing SDL outside `sdl.zig` and C++ smoke/bootstrap programs;
- calling D3D12, Vulkan, Metal, DXGI, Win32 rendering, Cocoa rendering, or X11 rendering APIs directly;
- implementing future packets early;
- weakening or deleting an existing test to make a packet pass;
- committing generated shader binaries, build caches, logs, screenshots, or staged game files.

If a required change falls outside the allowed list, report:

```text
PACKET BLOCKED
packet: <ID>
required file: <path>
reason: <one concrete paragraph>
smallest proposed boundary change: <paths and symbols>
```

Do not make the extra edit.

## Error and logging policy

Zig internal functions use errors. ABI wrappers translate them to `GfxGpuResult` and record a bounded UTF-8 diagnostic. SDL failure diagnostics include the SDL operation and `SDL_GetError()` text. Do not log per draw call in normal mode.

Renderer startup must produce one concise line containing:

```text
GFXGPU abi=1 driver=<name> shader_format=<format> debug=<0|1>
```

Renderer shutdown must produce:

```text
GFXGPU shutdown buffers=<n> textures=<n> samplers=<n> shaders=<n> pipelines=<n> targets=<n>
```

The accepted shutdown line has all counts equal to zero.

## Testing tiers

### Tier A: pure unit tests

No SDL initialization or GPU:

```powershell
zig build test-gfxgpu-core -Dtarget=x86_64-windows-msvc -Doptimize=Debug
```

### Tier B: ABI tests

Real C++ caller linked to the Zig renderer library:

```powershell
zig build gfxgpu-abi-test -Dtarget=x86_64-windows-msvc -Doptimize=Debug
```

### Tier C: GPU smoke

Creates an SDL window and forces SDL_GPU D3D12:

```powershell
$env:SDL_GPU_DRIVER='direct3d12'
zig build gfxgpu-smoke -Dtarget=x86_64-windows-msvc -Doptimize=Debug
Remove-Item Env:SDL_GPU_DRIVER
```

The smoke program renders a deterministic sequence and exits itself; it must not wait for interactive input.

### Tier D: adapter and game build

```powershell
zig build gfx -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Drenderer=sdl_gpu
zig build game-all -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Drenderer=sdl_gpu
```

### Tier E: human visual acceptance

Interactive evidence is allowed only where a Phase 08 or Phase 09 packet explicitly requires it. Luna prepares the build, log path, deterministic scene, and checklist. The coordinator or user records acceptance.

## Evidence record

At packet completion, report:

```text
PACKET COMPLETE
packet: <ID>
commit: <hash> <subject>
changed files:
- <path>
tests:
- <command> => PASS (<key output>)
regression:
- <command> => PASS
evidence:
- <artifact or log fact>
remaining concerns:
- none
```

Use a concrete concern instead of `none` when one exists. Do not claim a visual gate passed unless a human recorded it.

## Escalation triggers

Stop and report when:

- current SDL3 or Zig APIs differ from the packet and the difference changes ownership or architecture;
- an `IGFX` behavior cannot be represented by the existing ABI;
- a legacy effect cannot be mapped without changing visible behavior;
- an asset format is not supported by SDL_GPU on the forced D3D12 driver;
- validation reports a use-after-free, double release, command-buffer state error, or attachment mismatch;
- a test is nondeterministic across three immediate runs;
- completing the packet requires direct backend API access;
- the current game does not reproduce the expected legacy behavior used as the parity oracle.

## Review protocol

The coordinator reviews:

- packet diff against allowed files;
- tests before implementation quality;
- ownership and error paths;
- ABI size/type consistency;
- stale-handle behavior;
- destruction order;
- validation output;
- absence of generated artifacts;
- commit scope.

A rejected packet is amended in the same task branch/commit sequence. Luna does not advance to another packet while review findings remain.
