# SDL_GPU Renderer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement exactly one packet at a time. Do not redesign the architecture or combine packets.

**Goal:** Replace the D3D8-style/D3D9-backed GFX implementation with one Zig-owned SDL_GPU renderer and a thin C++ `IGFX` adapter, ending with the Windows 11 x64 game running through SDL_GPU's Direct3D 12 backend.

**Architecture:** Enigma continues to call the existing C++ `IGFX` interfaces. New C++ adapter objects translate those calls into a versioned C ABI. Zig owns renderer state, SDL_GPU objects, resource registries, frame recording, pipelines, shader loading, and destruction. The renderer contains no direct D3D12, Vulkan, Metal, Win32, Cocoa, or X11 graphics calls.

**Tech Stack:** Zig 0.16, C++17 adapter code, `zig-sdl3` v0.2.2 with its SDL 3.4.0 dependency, SDL_GPU, SDL_shadercross commit `e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba`, DirectX Shader Compiler `v1.9.2607`, canonical HLSL, Windows 11 x64 acceptance through SDL_GPU's `direct3d12` driver.

---

## Authoritative documents

Read these before executing any packet:

1. `docs/superpowers/specs/2026-07-30-sdl-gpu-luna-design.md`
2. `docs/superpowers/plans/2026-07-30-sdl-gpu-renderer/EXECUTION.md`
3. The selected phase's `MANIFEST.md`
4. The selected packet

If a packet conflicts with the approved design, stop and report the exact conflict. Do not resolve architectural conflicts locally.

## Milestone boundary

This corpus proves the renderer migration on Windows 11 x64. It does not port the complete game platform layer. Linux x64/Vulkan and Apple Silicon/Metal are architectural constraints and later acceptance targets. A packet may add compile-only checks for those targets, but it must not introduce a second renderer or expose backend-native objects.

The final Windows gate is:

```powershell
zig build test-gfxgpu -Dtarget=x86_64-windows-msvc -Doptimize=Debug
zig build game-all -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Drenderer=sdl_gpu
zig build verify-x64-runtime -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Drenderer=sdl_gpu
zig build install-game -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Drenderer=sdl_gpu
```

The staged `Game.exe` must reach the main menu, start a representative mission, render UI and world geometry correctly, survive resize/minimize/restore, return to the menu, and shut down without validation errors or leaked renderer resources.

## Global invariants

- Zig exclusively owns every `SDL_GPUDevice`, buffer, texture, sampler, shader, pipeline, transfer buffer, command buffer, and fence.
- C++ stores only opaque numeric handles and borrowed pointers passed for the duration of a call.
- No SDL opaque pointer crosses `gfxgpu_c.h`.
- No C++ exception, Zig error union, allocator object, standard-library container, or platform handle crosses the ABI.
- Every ABI struct starts with `struct_size`; the API table starts with `abi_version` and `struct_size`.
- Every resource handle is a 64-bit index-plus-generation value. Zero is invalid.
- Stale and wrong-kind handles fail deterministically and never access freed storage.
- Public ABI functions return `GfxGpuResult`; detailed diagnostics are copied into a caller-owned byte buffer.
- All CPU-to-GPU writes use SDL_GPU transfer buffers and copy passes.
- A frame has exactly one active command buffer and at most one active render pass.
- Pipelines are immutable and cached from normalized render state, shader effect, render-target formats, vertex layout, and primitive topology.
- Render-target changes end the current pass. Pipeline changes do not create a second pass unless attachment compatibility changes.
- Deferred destruction waits until the associated submission is safe.
- Canonical shaders are HLSL. Runtime code loads offline-produced backend blobs; it never invokes a shader compiler.
- Windows acceptance requires DXIL and the `direct3d12` SDL_GPU driver.
- The SDL dependency is vendored from `https://codeberg.org/7Games/zig-sdl3` tag `v0.2.2`; its unused FreeType/HarfBuzz dependencies are lazy so renderer builds do not require text-extension archives.
- The legacy D3D renderer remains selectable until Phase 09. It is a comparison oracle, not a code source for new ownership.
- Do not add gameplay features or visual improvements during parity work.

## Stable file map

### Public boundary

- `Sources/src/GFX/GFXPlatform.h` — portable definitions used by public GFX declarations.
- `Sources/src/GFXGPU/gfxgpu_c.h` — complete versioned C ABI.
- `Sources/src/GFXGPU/root.zig` — Zig library root and test import root.
- `Sources/src/GFXGPU/abi.zig` — exported C entry point and API-table wrappers.

### Zig renderer

- `error.zig` — result mapping and thread-local diagnostic text.
- `handles.zig` — typed generational registries.
- `formats.zig` — legacy pixel/index/topology/clear conversions.
- `vertex_layout.zig` — D3D FVF decoding and SDL_GPU vertex descriptions.
- `render_state.zig` — normalized mutable legacy state.
- `pipeline_key.zig` — deterministic immutable pipeline key.
- `math_convert.zig` — matrix, viewport, color, and clip-space conversion.
- `sdl.zig` — the only Zig module that imports SDL headers.
- `device.zig` — GPU device creation, backend selection, and capability checks.
- `surface.zig` — SDL window claim, swapchain format, resize/minimize state.
- `frame.zig` — command-buffer and frame state machine.
- `transfer.zig` — reusable upload/readback transfer allocation.
- `buffers.zig` — static, dynamic, and temporary GPU buffers.
- `textures.zig` — texture registry, uploads, mip levels, and format checks.
- `samplers.zig` — immutable sampler cache.
- `targets.zig` — depth and color render-target ownership.
- `readback.zig` — screenshot copy and row-pitch normalization.
- `lifetime.zig` — deferred release and live-resource metrics.
- `shader_manifest.zig` — generated shader-record reader.
- `shaders.zig` — shader blob validation and `SDL_GPUShader` ownership.
- `pipeline_cache.zig` — graphics pipeline creation and cache.
- `passes.zig` — render-pass attachment and load/store decisions.
- `bindings.zig` — texture/sampler/storage/uniform binding.
- `draw.zig` — draw validation and command encoding.
- `renderer.zig` — top-level orchestration called by the ABI.

### C++ adapter

- `Sources/src/GFXGPU/GraphicsEngineGpu.h/.cpp` — `IGFX` implementation and engine-facing state translation.
- `TextureGpu.h/.cpp` — `IGFXTexture`/surface adapters.
- `GeometryBufferGpu.h/.cpp` — vertex/index/temporary geometry adapters.
- `GfxGpuObjectFactory.cpp` — module factory exports.
- `GFXGPU.def` — exported factory names.

### Shaders and tools

- `Sources/src/GFXGPU/shaders/bindings.hlsl` — fixed register convention.
- `Sources/src/GFXGPU/shaders/common.hlsl` — shared math, fog, lighting, texture helpers.
- `Sources/src/GFXGPU/shaders/*.hlsl` — one source per effect family.
- `Sources/src/GFXGPU/shaders/manifest.json` — explicit effect/stage/entry-point records.
- `tools/zig/compile_gfxgpu_shaders.zig` — deterministic SDL_shadercross driver and generated manifest writer.
- `tools/zig/gfxgpu_abi_test.cpp` — real C++ ABI consumer.
- `tools/zig/gfxgpu_smoke.cpp` — SDL window/device/frame smoke program.
- `tools/zig/verify_gfxgpu_runtime.ps1` — staged-runtime and renderer-log checks.

## C ABI contract

`gfxgpu_c.h` must use these fixed-width primitives and no compiler-dependent enums:

```c
#define GFXGPU_ABI_VERSION 1u

typedef uint64_t GfxGpuHandle;
typedef struct GfxGpuRenderer GfxGpuRenderer;

typedef uint32_t GfxGpuResult;
#define GFXGPU_OK               UINT32_C(0)
#define GFXGPU_INVALID_ARGUMENT UINT32_C(1)
#define GFXGPU_INVALID_STATE    UINT32_C(2)
#define GFXGPU_INVALID_HANDLE   UINT32_C(3)
#define GFXGPU_UNSUPPORTED      UINT32_C(4)
#define GFXGPU_OUT_OF_MEMORY    UINT32_C(5)
#define GFXGPU_SDL_ERROR        UINT32_C(6)
#define GFXGPU_IO_ERROR         UINT32_C(7)
#define GFXGPU_SHADER_ERROR     UINT32_C(8)
#define GFXGPU_INTERNAL_ERROR   UINT32_C(9)

typedef struct GfxGpuExtent {
    uint32_t width;
    uint32_t height;
} GfxGpuExtent;

typedef struct GfxGpuViewport {
    float x;
    float y;
    float width;
    float height;
    float min_depth;
    float max_depth;
} GfxGpuViewport;

typedef struct GfxGpuCreateInfo {
    uint32_t struct_size;
    uint32_t flags;
    void *sdl_window;
    uint32_t width;
    uint32_t height;
    const char *shader_directory_utf8;
    const char *preferred_driver_utf8;
} GfxGpuCreateInfo;
```

The complete `GfxGpuApi` table is introduced in `P00-M04`. Later packets append operations only at the end and increment neither the version nor reorder fields during this milestone. The final operation groups are:

- lifecycle and diagnostics;
- begin/end frame, clear, present, resize, viewport;
- transform, material, light, fog, blend, depth, stencil, cull, and effect state;
- texture, sampler, render-target, vertex-buffer, and index-buffer ownership;
- texture and buffer upload/readback;
- non-indexed and indexed draw submission;
- renderer statistics and live-resource counts.

## Handle layout

All handles use:

```text
63                              32 31                              0
+--------------------------------+--------------------------------+
| generation (non-zero uint32)   | slot index + 1 (non-zero u32) |
+--------------------------------+--------------------------------+
```

The resource kind is enforced by the registry type, not encoded in the bits. A texture handle passed to a buffer operation returns `GFXGPU_INVALID_HANDLE`.

## Coordinate and state policy

- Preserve row-major legacy matrix input exactly at the ABI.
- Convert once in Zig before upload; do not transpose independently in shaders.
- SDL_GPU viewport depth remains `[0, 1]`.
- Apply the selected backend clip-space correction in `math_convert.zig`, represented as backend-neutral policy data.
- Normalize all legacy booleans and enum values before constructing `PipelineKey`.
- Unsupported legacy state values return `GFXGPU_UNSUPPORTED` with the numeric value in the diagnostic.
- Alpha-test is implemented in shaders with `clip`.
- Fixed-function lighting is implemented in shader effect families from normalized material/light uniforms.
- DXT1/3/5 support is capability-checked. No silent texture-format substitution is allowed.

## Shader binding convention

The register map is fixed for all shader families:

```hlsl
cbuffer FrameUniforms  : register(b0, space1) { float4x4 g_view_proj; float4 g_fog; };
cbuffer DrawUniforms   : register(b1, space1) { float4x4 g_world; float4 g_color; };
cbuffer LightUniforms  : register(b2, space1) { float4 g_light_data[32]; };
Texture2D g_texture0   : register(t0, space2);
Texture2D g_texture1   : register(t1, space2);
SamplerState g_sampler0: register(s0, space2);
SamplerState g_sampler1: register(s1, space2);
```

Every shader manifest record declares effect ID, vertex entry point, fragment entry point, required vertex attributes, sampler count, uniform-buffer count, and produced blob paths. The compiler rejects duplicate IDs and missing blobs.

## Phase graph

```text
00 foundation
   |
   v
01 pure core -------> 04 shader toolchain
   |                         |
   v                         v
02 device/frame ---> 03 resources
   \____________________  ___/
                        \/
                    05 drawing
                        |
                        v
                  06 C++ adapter
                        |
                        v
                    07 effects
                        |
                        v
                    08 parity
                        |
                        v
              09 Windows validation
```

Complete a phase before starting any packet in the next phase. Within a phase, obey the dependency list in its manifest. Parallel work is allowed only when packets have no dependency path and no overlapping allowed files.

## Phase completion contracts

| Phase | Required gate |
|---|---|
| 00 | Zig library, SDL dependency, C header, and real C++ ABI test build and pass |
| 01 | Pure Zig state/format/layout/handle tests pass without a GPU |
| 02 | Smoke window creates `direct3d12`, clears, presents, resizes, and exits cleanly |
| 03 | Buffer/texture/target upload and destruction tests pass with zero live resources |
| 04 | Offline compiler deterministically emits valid DXIL manifest records |
| 05 | Reference executable draws static, dynamic, indexed, textured, depth-tested geometry |
| 06 | `GFXGPU.dll` implements the engine-facing interfaces and game links with either renderer |
| 07 | Every legacy effect ID maps to a tested shader/state family |
| 08 | Menu and representative mission parity evidence is accepted |
| 09 | SDL_GPU is default, D3D renderer linkage is removed, Windows gate passes |

## Packet index

- `phase-00-foundation`: P00-M01 through P00-M05
- `phase-01-core`: P01-M01 through P01-M05
- `phase-02-device`: P02-M01 through P02-M04
- `phase-03-resources`: P03-M01 through P03-M06
- `phase-04-shaders`: P04-M01 through P04-M05
- `phase-05-drawing`: P05-M01 through P05-M04
- `phase-06-cpp-adapter`: P06-M01 through P06-M06
- `phase-07-effects`: P07-M01 through P07-M05
- `phase-08-parity`: P08-M01 through P08-M04
- `phase-09-windows-validation`: P09-M01 through P09-M04

This is 48 independently reviewable implementation packets. A coordinator gives Luna exactly one packet plus the authoritative documents. The next packet starts only after verification and review of the current packet.
