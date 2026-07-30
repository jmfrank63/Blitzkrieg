# SDL_GPU Renderer and Luna Execution Design

**Date:** 2026-07-30
**Status:** Approved for implementation planning
**Branch:** `sdl3-gpu-renderer`

## Purpose

Replace Blitzkrieg's D3D8-style renderer, currently implemented through a
Direct3D 9 compatibility shim, with one native renderer implementation for:

- Windows x64 through SDL_GPU's Direct3D 12 backend;
- Linux x64 through SDL_GPU's Vulkan backend;
- macOS on Apple Silicon through SDL_GPU's Metal backend.

The renderer core will be written in Zig. A thin C++ adapter will continue to
implement the Enigma engine's existing `IGFX` object interfaces while
delegating rendering and GPU-resource ownership to Zig through a stable C ABI.

The implementation plan will be written for execution by a lower-tier coding
model, referred to as Luna. Architectural decisions, interfaces, dependencies,
and verification requirements must therefore be decided in advance. Luna will
receive module-sized, self-contained task packets and will not be responsible
for decomposing phases or inventing architecture.

## Goals

1. Maintain one renderer source architecture across all three operating
   systems.
2. Use native graphics backends without Proton, Wine, CrossOver, or another
   Direct3D translation layer.
3. Preserve the engine-facing `IGFX` behavior while replacing D3D resource
   ownership and draw submission.
4. Teach modern rendering concepts—pipelines, command buffers, render passes,
   transfers, immutable state, and explicit resource binding—without requiring
   three raw graphics API implementations.
5. Keep GPU resource ownership and most mutable renderer state in Zig.
6. Make implementation tasks small enough for Luna to complete and verify in
   isolated sessions.
7. Preserve the working game throughout migration and retain the old renderer
   only as a temporary visual reference until final cutover.

## Non-goals

This renderer plan will not:

- port the complete game platform layer to SDL3;
- replace all Win32 input, filesystem, process, networking, or dialog code;
- introduce Proton, Wine, DXVK, MoltenVK, bgfx, or OpenGL;
- expose Vulkan, Metal, or Direct3D 12 objects to engine code;
- redesign gameplay, scene management, asset formats, or object factories
  outside the changes required by the renderer boundary;
- introduce multithreaded command recording during the initial migration;
- add new visual features before legacy rendering parity is established;
- remove the reference D3D renderer before the cross-platform renderer passes
  the defined parity gates.

The complete native platform port is a successor project. This plan includes
only the SDL window/bootstrap work required to create and validate the unified
renderer.

## Existing Constraints

### Legacy graphics behavior

`Sources/src/GFX/GFX.H` exposes a large stateful `IGFX` interface. It includes
device lifecycle, transforms, lights, materials, textures, viewport state,
temporary and persistent geometry, text, render targets, screenshots, gamma,
and effect selection.

`Sources/src/GFX/Specific.h` maps D3D8-era names to Direct3D 9 types and
functions. The current renderer therefore has D3D8 fixed-function semantics
even though it links to `d3d9`.

`Sources/src/GFX/GraphicsEngine.cpp` defines the legacy effect catalog and
performs immediate state mutation followed by draw submission. The new
renderer must preserve visible behavior without reproducing D3D's mutable
driver state internally.

### Windows coupling

The current `IGFX` boundary exposes `HWND`, `RECT`, `DWORD`, `WORD`, and
`STDCALL`. The existing build graph also links D3D9 and Windows system
libraries directly. Portable engine-facing types and target-aware build rules
must be established before the C++ adapter can compile natively on Linux and
macOS.

### SDL_GPU constraints

SDL_GPU provides native Direct3D 12, Vulkan, and Metal backends. The application
creates one `SDL_GPUDevice`, claims an `SDL_Window`, records work into command
buffers and render/copy passes, and acquires a swapchain texture for
presentation.

Graphics pipelines are immutable combinations of shader, vertex layout,
primitive topology, rasterizer, multisample, depth/stencil, blend, and target
format state. Legacy state changes must therefore be converted into a
canonical pipeline key and cached pipeline object.

Shaders are packaged in backend-compatible formats:

- DXIL for Direct3D 12;
- SPIR-V for Vulkan;
- Metal source or metallib for Metal.

SDL_shadercross will be used offline so runtime behavior does not depend on a
shader compiler being installed.

References:

- <https://wiki.libsdl.org/SDL3/CategoryGPU>
- <https://wiki.libsdl.org/SDL3/SDL_CreateGPUDevice>
- <https://wiki.libsdl.org/SDL3/SDL_ClaimWindowForGPUDevice>
- <https://github.com/libsdl-org/SDL_shadercross>

## Architecture

```text
Enigma game and managers
          |
          | existing IGFX virtual calls
          v
Portable C++ GFX adapter
          |
          | stable C ABI: plain values, structs, slices, opaque handles
          v
Zig renderer core
          |
          | SDL3 C API
          v
SDL_GPU
    |            |            |
    v            v            v
  D3D12       Vulkan        Metal
 Windows       Linux     macOS ARM64
```

### Ownership boundary

The C++ adapter owns:

- legacy C++ object identity;
- `IGFX` virtual dispatch;
- engine reference counting;
- conversion from engine structs to C ABI structs;
- calls into the Zig renderer API;
- compatibility behavior that is purely about engine object semantics.

The Zig renderer owns:

- the SDL GPU device and claimed window;
- frame and render-pass state;
- texture, sampler, buffer, render-target, shader, and pipeline registries;
- generational resource handles;
- transfer and temporary-geometry arenas;
- logical render state;
- pipeline-key construction and pipeline caching;
- draw submission;
- diagnostics and renderer statistics.

The C++ adapter must not retain SDL GPU object pointers. The Zig renderer must
not retain pointers to movable or ref-counted C++ objects.

### C ABI rules

The ABI will use:

- fixed-width integer types;
- `extern` structs with explicitly documented layout;
- pointer-plus-length slices;
- opaque 64-bit generational handles;
- integer result codes;
- a renderer-owned last-error string copied into caller-provided storage;
- no C++ classes, STL containers, exceptions, Zig error unions, or allocators
  across the boundary.

Every public ABI function will define:

- valid nullability;
- ownership of every pointer;
- whether data is copied or borrowed;
- thread requirements;
- handle validity requirements;
- success and failure behavior.

### Thread model

Initial rendering is single-threaded:

- SDL window creation, window claiming, frame begin, draw recording, and frame
  submission occur on the main/render thread;
- resource creation requests are executed on that same thread;
- screenshot completion may be polled, but no background renderer thread is
  introduced;
- the C ABI rejects calls made in an invalid frame or pass state.

This reduces migration risk and matches the existing engine's immediate
rendering behavior. Multithreaded rendering is deferred until after parity.

## Renderer State Model

The new renderer will not issue an SDL call for every legacy state setter.
Setters update a CPU-side `RenderState` value. A draw operation resolves that
state into:

1. a vertex-layout description;
2. a shader/effect family;
3. a `PipelineKey`;
4. resource bindings;
5. per-draw uniform data.

`PipelineKey` contains only state that changes immutable SDL graphics pipeline
creation:

- effect/shader variant;
- vertex layout;
- primitive topology;
- cull and fill mode;
- depth test/write/function;
- stencil mode;
- blend mode and color-write mask;
- render-target and depth formats;
- sample count.

Texture handles, samplers, transforms, material values, lights, and alpha
reference are bound separately and do not create distinct pipelines unless the
shader variant requires it.

The pipeline cache is deterministic: identical keys produce the same cached
pipeline, and unsupported combinations return a named renderer error rather
than silently choosing arbitrary state.

## Resource Model

### Handles

Each externally visible resource uses a typed 64-bit handle containing a slot
index and generation. Releasing a resource invalidates its generation. Stale,
wrong-type, zero, and out-of-range handles are rejected.

Handle-table behavior is tested without SDL or a GPU before any device module
depends on it.

### Buffers

Static geometry is uploaded once and retained in GPU buffers. Dynamic and
temporary geometry uses reusable transfer storage and cycled GPU buffers.
Legacy lock/unlock calls map to CPU staging memory; unlock schedules or records
the upload.

Vertex and index buffers remain separate because SDL_GPU does not allow every
usage combination on one buffer.

### Textures

The format layer maps legacy pixel formats to SDL GPU texture formats or to a
defined conversion path. Compressed formats are selected only after device
support is checked. Unsupported legacy formats fail with a diagnostic until a
specific conversion module is implemented and tested.

Texture lock/unlock uses CPU-owned staging memory. Uploads are encoded in copy
passes. Mipmap behavior is explicit: supplied levels are uploaded; generated
levels use SDL GPU mipmap generation where valid.

### Deferred lifetime

The renderer records release requests centrally. A resource is removed from
the public handle table immediately and its SDL resource is released through
the renderer's lifetime manager after any required in-flight use is complete.
Shutdown drains submitted work before releasing the device.

## Shader Model

One canonical HLSL source tree will be maintained. Offline shader tasks use
SDL_shadercross to produce target artifacts and a generated manifest.

The manifest records:

- logical shader identifier;
- stage;
- entry point;
- artifact format;
- resource counts;
- expected binding layout;
- content hash.

The runtime loader chooses an artifact supported by the active SDL GPU device.
It verifies the manifest and fails with the shader identifier and backend name
when an artifact is absent or incompatible.

Legacy fixed-function effects are grouped into behavior families. Numeric
effect IDs map to a family plus feature flags. This prevents dozens of copied
shader programs while retaining a complete, reviewable mapping table.

## C++ Adapter Design

The replacement adapter will be split by responsibility:

- module factory and registration;
- graphics-engine lifecycle and display modes;
- state, transforms, lights, and materials;
- texture and render-target objects;
- vertex and index objects;
- temporary geometry;
- draw and mesh submission;
- fonts, text, and screen-space helpers;
- screenshot and gamma compatibility.

Adapter objects store Zig handles rather than D3D interfaces. Their destructors
release those handles through the C ABI. `SwapData`, `ClearInternalContainer`,
and device-reset compatibility are expressed in handle terms.

The manager layer is reused when it is renderer-independent. A manager is
changed only when a concrete D3D type or D3D lifetime assumption is proven to
cross its boundary.

## Migration Phases

### Phase 0 — Build and ABI foundation

Establish SDL3 dependencies, portable GFX-facing types, target-aware build
helpers, the Zig renderer library, the C ABI, and a C++ ABI smoke test.

### Phase 1 — Pure renderer foundations

Implement diagnostics, generational handles, format conversion, topology
conversion, FVF decoding, logical render state, pipeline keys, viewport
conversion, color conversion, and matrix conversion as GPU-independent Zig
modules.

### Phase 2 — Device and frame lifecycle

Create the SDL GPU device, claim the window, configure the swapchain, acquire
command buffers and swapchain textures, execute clear-only render passes,
submit/present, handle resize/minimize, and shut down cleanly.

### Phase 3 — GPU resources

Implement transfer storage, static and dynamic buffers, textures, samplers,
depth textures, render targets, screenshot readback, and resource lifetime.

### Phase 4 — Shader toolchain

Add canonical shader sources, offline SDL_shadercross compilation, generated
manifests, runtime shader loading, binding validation, and baseline shader
families.

### Phase 5 — Draw submission

Implement render-pass control, pipeline caching, resource binding, uniforms,
indexed and non-indexed drawing, temporary geometry, static geometry, viewport
stacks, clear behavior, and render-target switching.

### Phase 6 — C++ `IGFX` adapter

Replace D3D-owning C++ classes with handle-based adapter modules and connect the
full engine-facing graphics interface to the Zig C ABI.

### Phase 7 — Legacy effect families

Port UI, text, opaque, alpha-tested, alpha-blended, multitexture, lightmap,
lighting, stencil, shadow, water, and special-effect families. Each family has
explicit legacy-effect mappings and visual fixtures.

### Phase 8 — Compatibility and parity

Validate formats, layouts, pipeline coverage, reference scenes, menus,
missions, resize/fullscreen behavior, resource lifetime, restart behavior, and
visual parity against the reference renderer.

### Phase 9 — Native target validation and cutover

Validate Direct3D 12 on Windows, Vulkan on Linux, and Metal on Apple Silicon;
verify packaged shaders and runtime files; switch the Windows game to the new
`GFX` backend; prove that the portable adapter and renderer are the only GFX
implementation exposed to future Linux and macOS game targets; and remove D3D
linkage from the active renderer target. Complete Linux and macOS game startup
remains gated by the successor platform-port plan.

## Luna Plan Corpus

The implementation plan will be stored as:

```text
docs/superpowers/plans/2026-07-30-sdl-gpu-renderer/
├── README.md
├── EXECUTION.md
├── phase-00-foundation/
│   ├── MANIFEST.md
│   └── P00-Mxx-*.md
├── phase-01-core/
├── phase-02-device/
├── phase-03-resources/
├── phase-04-shaders/
├── phase-05-drawing/
├── phase-06-cpp-adapter/
├── phase-07-effects/
├── phase-08-parity/
└── phase-09-platform-validation/
```

`README.md` defines the architecture summary, global invariants, dependency
graph, milestones, and completion contract.

`EXECUTION.md` defines Luna's operating rules and evidence format.

Each phase `MANIFEST.md` lists:

- module task identifiers;
- dependency identifiers;
- files owned by each task;
- tasks that may run independently;
- the phase integration gate;
- the exact evidence required before the next phase begins.

Each `Pxx-Myy` file is one Luna session.

## Luna Module Task Contract

Every task packet must include the following sections.

### Identity

- stable task identifier;
- concise module name;
- phase;
- dependencies;
- expected commit message.

### Objective

One observable outcome stated without optional extras.

### Context

Only the architectural facts needed for this module. The packet links to the
design specification but does not require Luna to rediscover interfaces.

### Allowed changes

- exact files to create;
- exact files to modify;
- exact test files;
- explicit statement that all other files are read-only.

The normal task changes two to five files. More files require a written reason
in the packet.

### Required interfaces

- exact Zig types;
- exact function signatures;
- exact C ABI declarations where applicable;
- required struct layout and ownership;
- dependencies that already exist.

### Implementation checklist

A function-by-function checklist in dependency order. Functions are steps
inside the module task, not separate agent tasks.

### Invariants and forbidden behavior

Examples include:

- do not store C++ pointers in Zig;
- do not expose SDL pointers through the C ABI;
- do not add backend-specific branches outside the SDL integration module;
- do not weaken, skip, or rewrite acceptance tests;
- do not add placeholder success returns;
- do not modify unrelated legacy code;
- do not commit generated build output.

### Test-first sequence

The packet defines:

1. the test or smoke check to add;
2. the command that must fail before implementation;
3. the expected failure reason;
4. the minimum implementation;
5. the command that must pass afterward;
6. regression commands for dependencies.

### Completion evidence

Luna reports:

- files changed;
- tests added;
- commands run with exit status;
- first failing command if incomplete;
- any deviation from the packet;
- the expected commit message.

After verification, the coordinator records the resulting commit hash in the
phase manifest.

### Escalation rules

Luna stops without broadening scope when:

- a required interface is missing or contradicts the packet;
- a dependency test fails before the task's edits;
- the SDL API required by the packet is unavailable;
- completion requires changing a file outside the allowed set;
- the legacy behavior is ambiguous;
- a platform-specific result cannot be verified in the current environment;
- the same failure persists after two focused correction attempts.

The blocked report must identify the command, diagnostic, relevant files, and
smallest decision needed from the planner.

## Execution Protocol

1. The coordinator selects only a task whose dependencies are committed and
   whose tests pass.
2. Luna starts from a clean worktree and reads `EXECUTION.md`, the task packet,
   and only the directly referenced code.
3. Luna verifies dependency health before editing.
4. Luna follows the packet's test-first sequence.
5. Luna stays within the allowed-file set.
6. Luna runs focused tests and the packet's regression commands.
7. Luna records completion evidence.
8. A separate verification pass checks the diff, commands, and invariants.
9. The task is committed only after verification.
10. Phase integration runs only when every task in the manifest is complete.

Luna does not recursively create subagents or rewrite the task hierarchy. The
coordinator may run independent module packets in parallel only when their
manifests declare disjoint ownership and no dependency relation.

## Error Handling

Errors are divided into:

- programmer errors, detected by assertions and unit tests;
- invalid legacy calls, returned as stable ABI result codes;
- unavailable SDL/backend features, returned with an SDL diagnostic;
- recoverable frame conditions such as minimize or unavailable swapchain,
  represented as named frame outcomes;
- fatal device initialization or device-loss conditions, reported to the
  adapter with enough context for orderly shutdown.

No adapter method may convert a renderer failure into an unconditional success.
Where the legacy interface can return only `bool`, the adapter logs the full
renderer error and returns `false`.

## Verification Strategy

### Pure Zig tests

Run without SDL or a GPU:

- handles and stale-handle rejection;
- format and topology mapping;
- FVF decoding;
- render-state canonicalization;
- pipeline-key equality and hashing;
- matrix, color, viewport, and rectangle conversion;
- effect-family mapping;
- shader-manifest parsing.

### ABI tests

A real C++ executable links the Zig renderer library and verifies:

- struct sizes and alignment;
- exported symbol names;
- calling convention;
- handle round trips;
- error propagation;
- slice and string transfer;
- create/use/release lifecycle.

### SDL integration tests

Focused smoke executables verify:

- device creation;
- claimed window and clear-only frame;
- buffer and texture upload;
- baseline triangle;
- textured quad;
- offscreen target;
- screenshot readback;
- resize/minimize recovery;
- repeated startup/shutdown.

### Visual parity

Stable scenes are captured by the old and new renderers. Comparison records:

- expected image dimensions;
- tolerance metric;
- masks for intentionally variable regions;
- renderer/backend metadata;
- artifact paths.

Manual approval is required for the first accepted reference of each effect
family. Later regressions use automated comparison against that reference.

### Cross-platform gates

Windows is the primary implementation host, but a phase cannot claim native
completion until its platform gate passes on:

- Windows x64 with D3D12;
- Linux x64 with Vulkan;
- macOS ARM64 with Metal.

Platform-unavailable tests are reported as unverified rather than treated as
passing.

## Completion Contract

The renderer migration is complete only when:

1. one Zig renderer implementation runs through SDL_GPU on all three target
   backends;
2. the C++ adapter implements the engine-facing graphics contract without D3D
   resource ownership;
3. menus and representative missions meet the approved visual parity
   thresholds;
4. all legacy effect IDs used by game content have an explicit supported
   mapping;
5. texture, geometry, render-target, screenshot, resize, fullscreen, and
   restart tests pass;
6. renderer resource registries are empty after clean shutdown;
7. packaged builds contain the correct SDL library and shader artifacts;
8. the Windows game uses the new renderer by default, while renderer/adapter
   smoke applications use that same implementation on Linux and macOS ARM64;
9. D3D9 and DXGUID are no longer linked by the active renderer target;
10. remaining native-platform work is documented as a separate successor plan
    rather than hidden inside renderer tasks.
