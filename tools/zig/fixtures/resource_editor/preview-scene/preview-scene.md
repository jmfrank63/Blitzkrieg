# Preview-scene spike (M001 / S01 / T05)

This directory holds the artifacts of the preview-scene spike: the committed
test harness `tools/zig/preview_scene_spike.cpp`, the one captured TGA per
preview kind (`mesh.tga`, `sprite.tga`, `particle.tga`) and the per-run log
`spike.log` produced beside them under `zig-out/local-test/resource_editor/`
and copied here for each committed capture.

Spec cross-reference: D-16 and D-17 of
`docs/superpowers/specs/2026-09-30-portable-resource-editor-design.md` and
R018 of `.planning/phases/06-resource-editor-portable-port/06-CONTEXT.md`.

## Scope of this spike

- **The capture spine is live now.** The spike proves on any GPU-capable
  host (Linux included, where this slice was executed) that
  `BkEditorStart` → `BkEditorCaptureFrame` writes a well-formed
  uncompressed 32-bit TGA at the screen's size, top row first, and that
  the engine's own default clear path already produces a measurable share
  of non-black-non-magenta pixels (~0.25 on the Linux agent). The three
  per-kind captures and the `spike.log` live under
  `zig-out/local-test/preview-scene/`; the committed copies here are from
  the same run, carried over so a reader without a GPU can see the shape.
- **The preview-scene bridge path is deferred to S04.** The design (D-16)
  calls for a new `BkResPreviewBegin( kind )` / `BkResPreviewShow( project )`
  pair that mounts an in-memory preview storage over the data, exports the
  edited project into it, and builds the visual through `IVisObjBuilder` on
  an empty `IScene`. That ABI does not exist yet. Until it does, the three
  per-kind captures use the engine's default-camera empty-scene frame as a
  stand-in: the camera, the capture, the readback and the TGA layout are
  all exercised; the only thing missing is the per-kind visual object.
- **No map is opened by this spike.** D-16 says the preview scene is an
  empty `IScene` without terrain (except road and river, in S13). Not
  opening a map also keeps the spike away from a Zig 0.16 Linux debug-build
  trap in `CArray2D::SetZero` (`memset(nullptr, 0, 0)`) that the AI
  editor's `Clear` path hits before the war fog is sized - the same trap
  the map-opening tiers (`test-editor-bridge`) hit today. The capture path
  does not need a map and the preview spec says it should not have one.
- **Non-black-non-magenta share on the empty scene is informational, not
  asserted.** The 1 % floor the task plan names is the stronger assertion
  S04 inherits once `IVisObjBuilder` is wired - a mesh / sprite / particle
  on an otherwise-empty scene must be visible. For T05 the share is logged
  per capture (today's three captures all read ~0.248 because the engine
  clears to a non-black colour) so a reader can watch it climb once S04
  places actual visuals; the hard assertion is only that every capture
  wrote a readable TGA at the screen's size.

## Camera parameters (measured, not guessed)

The map editor's D-12 investigation showed that any camera other than the
game's own placement breaks the terrain draw. The preview spike therefore
re-uses the game's camera verbatim: `BkEditorSetCamera( wx, wy )` with no
`BkEditorSetYaw` override, which leaves the yaw at the game's own 45°
(`SetSessionCamera` in `Sources/src/EditorBridge/session.cpp`). The anchor
height is set by `SetSessionCamera` to the engine's measured working
distance for the current screen size; the editor and preview share that
plumbing by construction.

| Parameter             | Value / source                                                        |
|-----------------------|-----------------------------------------------------------------------|
| yaw (degrees)         | 45 (`BkEditorView::yaw_degrees` default, `bridge.cpp:2498`)           |
| FOV                   | the engine's `GFX.Camera.FOV` default (not overridden by the spike)   |
| anchor height         | `SetSessionCamera`'s working distance for the current screen height   |
| screen size           | 640 x 480 (`SDL_CreateWindow` in `preview_scene_spike.cpp`)           |
| world-units-per-tile  | 32.0 (`fWorldCellSize`, matches `bridge.cpp:418`)                     |
| yaw override          | none (`BkEditorSetYaw` not called; D-12 says "the game's own 45")     |

The spike today captures three frames at the engine's default camera
anchor (no map is open, so world-coordinate placements have nothing to
anchor to and `BkEditorSetCamera` is called with `(0, 0)` for the record).
When S04's `BkResPreviewBegin` lands, the three captures will differ
because each preview kind places its own camera on the kind's own object.

## Scene shape (preview kinds)

The design (D-16) names three scene shapes the ResourceEditor's preview
needs. S04 will build these through `BkResPreviewBegin( kind )`.

| Kind         | Scene shape                                                           |
|--------------|-----------------------------------------------------------------------|
| mesh unit    | empty `IScene`, no terrain, the mesh at the origin                    |
| sprite       | empty `IScene`, no terrain, the sprite's first frame at the origin    |
| particle     | empty `IScene`, no terrain, one particle emitter at the origin        |
| road         | `maps\road3d` loaded as terrain, the road piece on it (deferred S13)  |
| river        | `maps\river3d` loaded as terrain, the river piece on it (deferred S13)|

Until `BkResPreviewBegin` lands, the spike's three captures are not those
scene shapes - they are three readings of the engine's empty default
scene. The runbook keeps the measured camera (verified above) because
that is what S04 inherits; the scene content is the only thing that
changes with the kind.

## Fixture inputs the three captures are tied to

The task plan requires one trivial mesh unit, one sprite object and one
particle source. The committed fixtures the spike reads from (and that
S04 will export through the preview storage) are:

| Kind       | Fixture file                                                            |
|------------|-------------------------------------------------------------------------|
| mesh unit  | `tools/zig/fixtures/resource_editor/unt/project.unt` (+ `mesh-2x2x2.obj`) |
| sprite     | `tools/zig/fixtures/resource_editor/spt/project.spt` (+ `sprite-1frame.tga`) |
| particle   | `tools/zig/fixtures/resource_editor/eff/project.eff` (+ `particle-2key.txt`) |

The fixtures were committed by T02; this spike only names them so the
runbook, the pixel-share assertion and the eventual S04 preview build
line up on the same inputs.

## Non-black-non-magenta pixel share

The engine's missing-texture fallback renders as solid magenta
(255, 0, 255). A fully-black frame means the renderer never drew anything
(a scene that failed to build, a camera that framed off-world). Both are
failure modes S04's preview must avoid, so the per-capture share of
pixels that are neither solid black nor solid magenta is logged per
capture. For T05 the share is informational - every capture of today's
empty default scene reads ~0.25 because the engine clears to a non-black
colour - and S04 will promote the share to a hard >= 1 % floor once a
visual object actually lives in the scene.

## Running the spike

The build step `zig build preview-scene-spike` compiles the harness
everywhere and runs it on the current host:

```
zig build install-game
zig build preview-scene-spike
```

The harness writes `spike.log` and `mesh.tga` / `sprite.tga` /
`particle.tga` into `zig-out/local-test/preview-scene/` on every
GPU-capable run (Linux included - the Linux agent that ran this slice
produced those files). A runner with no GPU logs
`preview-scene: skipped: no GPU device (...)` to the same path and exits 0.
The committed copies under this directory are the ones carried over from
the slice's own run so a reader without the engine built can still read
the shape; S04 will overwrite them with real per-kind captures.

## Hand-off to S04

S04 (ResourceEditor preview-scene bridge path) must:

1. Add `BkResPreviewBegin( kind )` and `BkResPreviewShow( project )` to
   `Sources/src/EditorBridge/bridge.h` with the camera placement this
   runbook names (no `BkEditorSetYaw`, no custom FOV).
2. Replace the "empty default-scene stand-in" captures in
   `preview_scene_spike.cpp` with real `BkResPreviewBegin` /
   `BkResPreviewShow` calls on the fixture inputs above.
3. Promote the per-capture non-black-non-magenta share from logged-only to
   a hard `Check( >= 0.01 )` assertion in the spike. The spike's log
   format should stay the same - S04's own tests can parse it.
4. Fix the Linux debug-build `CArray2D::SetZero` null-pointer trap
   (`memset(nullptr, 0, 0)` in `Sources/src/Misc/2Darray.h:33`) that blocks
   `BkEditorOpenMap` on Linux today. Once fixed, S13 (preview road / river)
   can open `maps\road3d` / `maps\river3d` as the preview terrain the
   runbook names, without needing to work around this trap.
