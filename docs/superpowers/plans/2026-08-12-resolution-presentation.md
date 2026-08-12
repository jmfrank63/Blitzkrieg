# Resolution & Presentation Rules Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement docs/superpowers/specs/2026-08-12-resolution-presentation-design.md — menus/videos never upscale (1:1 or shrink-to-fit in a black frame), missions render at the drawable with the world projection-zoomed to the configured resolution's view region and the HUD scaled by the configured resolution but anchored to the real screen edges.

**Architecture:** Everything rides existing mechanisms: the Zig present blit gains a scale cap, `UpdatePresentOffsets` publishes drawable-size globals, `ChangeResolution` computes per-screen-type scene sizes, `NSceneScreenScale::GetGameplayScale` gains a configured-base input, and `CUIScreen::Reposition` derives the mission HUD scale from that base. One new risk area: fractional world scales resurrect the terrain tile-seam artefact, so terrain atlas sampling gets a half-texel inset.

**Tech Stack:** C++ (legacy engine), Zig (GFXGPU renderer + build), SDL3 GPU. Build: `zig build install-game -Dtarget=aarch64-macos --release=fast` (also runs the Zig tests). Runtime verification: `BK_GFX_TRACE=1` from `zig-out/game/macos/arm64/release`.

## Global Constraints

- Menus/videos: present scale `min(1, fit_scale)` — never upscale; black frame around.
- Missions: scene = drawable; world region = configured resolution's view; HUD scale from `cfg_eff`, anchors on drawable edges; never cropped, never off screen.
- `cfg_eff = min(cfg, drawable)` per axis; `Auto` ⇒ `cfg = drawable`.
- Fullscreen never changes the display mode (no `SDL_SetWindowFullscreenMode` anywhere).
- Windowed window size clamps to `SDL_GetDisplayUsableBounds`.
- `Auto` resolution must look exactly like today (world at whole-step legacy scale, borderless menus).
- Legacy D3D9 path (`Sources/src/GFX`) and ELK editor are out of scope; all changed code must fall back to today's behavior when the new globals are unset.
- The game process must be launched via the Bash tool's `run_in_background` (it dies with the shell otherwise); the agent cannot press keys or screenshot the OS — use `BK_GFX_TRACE` output and ask Johannes for F9 captures.

---

### Task 1: Shrink-only present fit

**Files:**
- Create: `Sources/src/GFXGPU/present_fit.zig`
- Modify: `Sources/src/GFXGPU/sdl.zig:150-166` (`blitTextureFit`)
- Modify: `Sources/src/GFXGPU/GraphicsEngineGpu.cpp:812-820` (`UpdatePresentOffsets` fit branch)
- Modify: `build.zig` (test wiring)

**Interfaces:**
- Produces: `present_fit.fitRect(source_w, source_h, dest_w, dest_h) -> struct { x: u32, y: u32, w: u32, h: u32 }` — the letterboxed destination rect with scale capped at 1.0. Consumed by `sdl.zig` and mirrored (not imported) by the C++ offsets math.

- [ ] **Step 1: Write the pure fit function with failing-first tests**

Create `Sources/src/GFXGPU/present_fit.zig`:

```zig
const std = @import("std");

pub const Rect = struct { x: u32, y: u32, w: u32, h: u32 };

/// Letterbox rect for presenting a scene into a drawable: aspect-fit,
/// but the scale is capped at 1.0 - a scene smaller than the drawable is
/// shown 1:1 in a black frame, never upscaled (the spec's shrink-only rule).
pub fn fitRect(source_w: u32, source_h: u32, dest_w: u32, dest_h: u32) Rect {
    if (source_w == 0 or source_h == 0 or dest_w == 0 or dest_h == 0)
        return .{ .x = 0, .y = 0, .w = dest_w, .h = dest_h };
    const scale_w = @as(f64, @floatFromInt(dest_w)) / @as(f64, @floatFromInt(source_w));
    const scale_h = @as(f64, @floatFromInt(dest_h)) / @as(f64, @floatFromInt(source_h));
    const scale = @min(1.0, @min(scale_w, scale_h));
    const fit_w: u32 = @max(1, @min(dest_w, @as(u32, @intFromFloat(@round(@as(f64, @floatFromInt(source_w)) * scale)))));
    const fit_h: u32 = @max(1, @min(dest_h, @as(u32, @intFromFloat(@round(@as(f64, @floatFromInt(source_h)) * scale)))));
    return .{ .x = (dest_w - fit_w) / 2, .y = (dest_h - fit_h) / 2, .w = fit_w, .h = fit_h };
}

test "small scene is shown 1:1 in a black frame, not upscaled" {
    const r = fitRect(800, 600, 1440, 900);
    try std.testing.expectEqual(@as(u32, 800), r.w);
    try std.testing.expectEqual(@as(u32, 600), r.h);
    try std.testing.expectEqual(@as(u32, 320), r.x);
    try std.testing.expectEqual(@as(u32, 150), r.y);
}

test "large scene shrinks to fit with aspect kept" {
    const r = fitRect(1920, 1080, 1440, 900);
    try std.testing.expectEqual(@as(u32, 1440), r.w);
    try std.testing.expectEqual(@as(u32, 810), r.h);
    try std.testing.expectEqual(@as(u32, 0), r.x);
    try std.testing.expectEqual(@as(u32, 45), r.y);
}

test "exact fit is identity" {
    const r = fitRect(1440, 900, 1440, 900);
    try std.testing.expectEqual(Rect{ .x = 0, .y = 0, .w = 1440, .h = 900 }, r);
}
```

- [ ] **Step 2: Wire the tests into the build and verify they run**

Find the existing test wiring pattern: `grep -n "addTest" build.zig`. Mirror the
`stage_tests` wiring (build.zig around lines 1154-1163): create a module for
`Sources/src/GFXGPU/present_fit.zig`, `b.addTest` it, `addRunArtifact`, and make
the same step that depends on `stage_tests_run` depend on this run too.

Run: `zig build install-game -Dtarget=aarch64-macos --release=fast --summary all 2>&1 | grep "tests passed"`
Expected: the test count increases from 11 to 14 and all pass.

- [ ] **Step 3: Use fitRect in the blit**

In `Sources/src/GFXGPU/sdl.zig`, add `const present_fit = @import("present_fit.zig");`
and replace the body of `blitTextureFit` (keep the zero-size guard):

```zig
pub fn blitTextureFit(command_buffer: *GpuCommandBuffer, source: *GpuTexture, source_width: u32, source_height: u32, destination: *GpuTexture, destination_width: u32, destination_height: u32) void {
    if (source_width == 0 or source_height == 0 or destination_width == 0 or destination_height == 0) return;
    const rect = present_fit.fitRect(source_width, source_height, destination_width, destination_height);
    const info = c.SDL_GPUBlitInfo{
        .source = .{ .texture = source, .w = source_width, .h = source_height },
        .destination = .{ .texture = destination, .x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h },
        .load_op = c.SDL_GPU_LOADOP_CLEAR,
        .clear_color = .{ .r = 0, .g = 0, .b = 0, .a = 1 },
        .filter = if (rect.w == source_width and rect.h == source_height) c.SDL_GPU_FILTER_NEAREST else c.SDL_GPU_FILTER_LINEAR,
    };
    c.SDL_BlitGPUTexture(command_buffer, &info);
}
```

(1:1 uses NEAREST so an unscaled menu is pixel-exact; only real shrinks filter.)

- [ ] **Step 4: Cap the input-mapping scale the same way**

In `GraphicsEngineGpu.cpp` `UpdatePresentOffsets`, the fit branch (line ~814):

```cpp
const double fScale = Min( 1.0, Min( double( pixel_width ) / width_, double( pixel_height ) / height_ ) );
```

(only the added outer `Min(1.0, ...)` changes; the offset/scale lines below stay).

- [ ] **Step 5: Build, run, verify with trace**

Build: `zig build install-game -Dtarget=aarch64-macos --release=fast` — 0 errors, all tests pass.
Run (run_in_background): `cd zig-out/game/macos/arm64/release && BK_GFX_TRACE=1 ./Game -windowed`
after setting `GFX.Mode` to `800x600x32` in `profiles/Player/config.cfg` (the
`<item ...><Var>800x600x32</Var>...<KeyName>GFX.Mode</KeyName>` value).
Expected trace: `present fit=1 ... scale 1.0000,1.0000` (no upscale) once the menu is up. Kill the game afterwards (`pkill -f "release/Game"`).

- [ ] **Step 6: Commit**

```bash
git add Sources/src/GFXGPU/present_fit.zig Sources/src/GFXGPU/sdl.zig Sources/src/GFXGPU/GraphicsEngineGpu.cpp build.zig
git commit -m "feat: cap the present fit at 1:1 - menus and videos never upscale"
```

---

### Task 2: Publish drawable size and change flag

**Files:**
- Modify: `Sources/src/GFXGPU/GraphicsEngineGpu.cpp:797-828` (`UpdatePresentOffsets`)
- Modify: `Sources/src/GFXGPU/GraphicsEngineGpu.h:124-126` (members)

**Interfaces:**
- Produces globals: `GFX.Drawable.SizeX`, `GFX.Drawable.SizeY` (window pixel size, ints) and `GFX.DrawableChanged` (set to 1 on every change after the first publish). Consumed by Task 4's `ChangeResolution` and its per-frame reaction.

- [ ] **Step 1: Add members**

In `GraphicsEngineGpu.h` next to `pending_fullscreen_frames_`:

```cpp
    int published_drawable_w_ = 0;
    int published_drawable_h_ = 0;
```

- [ ] **Step 2: Publish in UpdatePresentOffsets**

In `UpdatePresentOffsets`, inside the `if ( sdl_window_ != nullptr && ... )` block
after `pixel_width/pixel_height` are known valid (after line ~810):

```cpp
        if ( pixel_width != published_drawable_w_ || pixel_height != published_drawable_h_ )
        {
            const bool bFirstPublish = published_drawable_w_ == 0 && published_drawable_h_ == 0;
            published_drawable_w_ = pixel_width;
            published_drawable_h_ = pixel_height;
            SetGlobalVar( "GFX.Drawable.SizeX", pixel_width );
            SetGlobalVar( "GFX.Drawable.SizeY", pixel_height );
            // The startup publish is not a change: nothing consumed the old
            // value, and flagging it would make the first screen re-run its
            // mode change for no reason.
            if ( !bFirstPublish )
                SetGlobalVar( "GFX.DrawableChanged", 1 );
        }
```

- [ ] **Step 3: Build and verify with trace**

Build. Run the game (run_in_background, `BK_GFX_TRACE=1`, windowed), resize the
window by launching with different `GFX.Mode` values across two runs, and check
via a debug print or by adding a temporary
`fprintf(stderr, "drawable %dx%d\n", pixel_width, pixel_height)` (remove before
commit) that publishes fire. Minimal acceptable check: game still boots to the
menu and `BK_GFX_TRACE` present lines are unchanged.

- [ ] **Step 4: Commit**

```bash
git add Sources/src/GFXGPU/GraphicsEngineGpu.cpp Sources/src/GFXGPU/GraphicsEngineGpu.h
git commit -m "feat: publish the drawable size and a change flag from the present path"
```

---

### Task 3: Clamp windowed window size to usable display bounds

**Files:**
- Modify: `Sources/src/GFXGPU/GraphicsEngineGpu.cpp:436` area (`SetMode`, windowed `SDL_SetWindowSize` call)

**Interfaces:**
- Consumes: `SelectedDisplay()` (`target`, already in scope in `SetMode`).
- Produces: no new interface; the windowed window can no longer extend off screen, so `drawable <= usable bounds` holds for every later task.

- [ ] **Step 1: Clamp before sizing**

In `SetMode`, immediately before the windowed `SDL_SetWindowSize` call at line ~436
(`if ( fullscreen != GFXFS_FULLSCREEN && !SDL_SetWindowSize( window, nSizeX, nSizeY ) )`):

```cpp
        // A window larger than the display's usable area hangs off screen and
        // takes the HUD with it; the WM only clamps user resizes, not
        // programmatic ones. Clamp to the usable bounds of the display the
        // window is (about to be) on.
        if ( fullscreen != GFXFS_FULLSCREEN )
        {
            SDL_Rect usable{};
            const SDL_DisplayID clamp_display = target != 0 ? target : SDL_GetDisplayForWindow( window );
            if ( clamp_display != 0 && SDL_GetDisplayUsableBounds( clamp_display, &usable ) && usable.w > 0 && usable.h > 0 )
            {
                nSizeX = Min( nSizeX, usable.w );
                nSizeY = Min( nSizeY, usable.h );
            }
        }
```

- [ ] **Step 2: Build and verify**

Build. Set `GFX.Mode` to `1920x1080x32` in `profiles/Player/config.cfg`, run
windowed with `BK_GFX_TRACE=1` (run_in_background). Expected trace: the SetMode
line reports a window no larger than the usable bounds (1440x~868 on the
MacBook), not 1920x1080.

- [ ] **Step 3: Commit**

```bash
git add Sources/src/GFXGPU/GraphicsEngineGpu.cpp
git commit -m "feat: clamp windowed mode sizes to the display's usable bounds"
```

---

### Task 4: Per-screen-type scene sizing in ChangeResolution

**Files:**
- Modify: `Sources/src/Common/InterfaceScreenBase.cpp:421-470` (`ChangeResolution`) and the per-frame block at lines ~204-222.

**Interfaces:**
- Consumes: `GFX.Drawable.SizeX/Y`, `GFX.DrawableChanged` (Task 2).
- Produces globals: `GFX.World.BaseSizeX`, `GFX.World.BaseSizeY` — `cfg_eff` per axis, published whenever a Mission mode is applied, cleared (set to 0) when a non-Mission mode is applied. Consumed by Task 5 (`GetGameplayScale`) and Task 6 (`CUIScreen::Reposition`).

- [ ] **Step 1: Compute the desired scene size per screen type**

In `ChangeResolution`, after reading `nDesiredSizeX/Y` (line ~423-424), insert:

```cpp
	// The configured resolution is not a literal render size any more
	// (docs/superpowers/specs/2026-08-12-resolution-presentation-design.md):
	// missions render at the drawable and use the configuration as the world
	// view / HUD base; menus and videos render at the configuration and are
	// presented shrink-only. cfg_eff clamps the configuration to the
	// drawable so an oversized setting behaves as the drawable size.
	const int nDrawableX = GetGlobalVar( "GFX.Drawable.SizeX", 0 );
	const int nDrawableY = GetGlobalVar( "GFX.Drawable.SizeY", 0 );
	int nWorldBaseX = 0, nWorldBaseY = 0;
	if ( szInterfaceType == "Mission" && nDrawableX > 0 && nDrawableY > 0 )
	{
		nWorldBaseX = nDesiredSizeX > 0 ? Min( nDesiredSizeX, nDrawableX ) : nDrawableX;
		nWorldBaseY = nDesiredSizeY > 0 ? Min( nDesiredSizeY, nDrawableY ) : nDrawableY;
		nDesiredSizeX = nDrawableX;
		nDesiredSizeY = nDrawableY;
	}
```

- [ ] **Step 2: Publish the world base with the other mode globals**

In the same function, inside the `if ( ... != ... )` mode-change block, next to the
`SetGlobalVar( "GFX.Mode.Current.*" ... )` writes (line ~459-465), add:

```cpp
		SetGlobalVar( "GFX.World.BaseSizeX", nWorldBaseX );
		SetGlobalVar( "GFX.World.BaseSizeY", nWorldBaseY );
```

(`nWorldBaseX/Y` are 0 for non-Mission screens, which is the documented
"unset" state every consumer falls back on.)

- [ ] **Step 3: React to drawable changes like display changes**

In the per-frame block of `CInterfaceScreenBase::StepLocal`'s caller (same
function that handles `GFX.DisplayChanged` at lines ~204-214), add directly
after that block:

```cpp
	// A live window resize changes the drawable; the mission scene follows it
	// (its scene is the drawable) and cfg_eff re-clamps. Menus keep their
	// configured scene and only the letterbox changes, which the present path
	// handles by itself.
	if ( GetGlobalVar( "GFX.DrawableChanged", 0 ) != 0 )
	{
		SetGlobalVar( "GFX.DrawableChanged", 0 );
		if ( szInterfaceType == "Mission" )
		{
			if ( ChangeResolution() )
				GetSingleton<IScene>()->Reposition();
		}
	}
```

Note: `ChangeResolution`'s existing diff check compares desired against
`GFX.Mode.Current.*`; because Mission's desired size now tracks the drawable,
a resize produces a real diff and re-runs SetMode. Verify this while testing.

- [ ] **Step 4: Build and verify with trace**

Build; run windowed at `GFX.Mode` `1024x768x32` (run_in_background,
`BK_GFX_TRACE=1`). Expected: menu SetMode traces still request 1024x768
(scene = cfg), and after loading into a mission (ask Johannes, or use a save in
`saves/`) the mission SetMode requests the drawable size. Menus: present
`fit=1 scale 1,1` (1:1 in frame). Mission: `fit=0 offset 0,0`.

- [ ] **Step 5: Commit**

```bash
git add Sources/src/Common/InterfaceScreenBase.cpp
git commit -m "feat: mission scenes render at the drawable; menus keep the configured size"
```

---

### Task 5: World zoom from the configured base

**Files:**
- Modify: `Sources/src/Scene/SceneScreenScale.h:12-27` (`GetGameplayScale`)
- Modify: `docs/superpowers/specs/2026-08-12-resolution-presentation-design.md` (s_world definition)

**Interfaces:**
- Consumes: `GFX.World.BaseSizeX/Y` (Task 4). All existing call sites
  (`SceneInternal.cpp`, `SceneDraw.cpp`, `DrawVisitor.cpp`, `TerrainInternal.cpp`,
  `FrameSelection.cpp`) pick the new scale up automatically because every
  helper in the header derives from `GetGameplayScale`.
- Produces: `GetGameplayScale` returns `legacy_step(cfg_eff) * fill(drawable, cfg_eff)` — fractional when cfg < drawable.

- [ ] **Step 1: Find the global-var include used by Scene code**

Run: `grep -n '#include' Sources/src/Scene/SceneInternal.cpp | head -20` and note
the header that declares `GetGlobalVar` (follow it from any Scene file that
calls `GetGlobalVar`). Add that include to `SceneScreenScale.h`.

- [ ] **Step 2: New scale source with legacy fallback**

Replace `GetGameplayScale` in `SceneScreenScale.h`:

```cpp
	inline float GetGameplayScale( const CTRect<float> &rcScreen )
	{
		const float fWidth = Max( rcScreen.Width(), 1.0f );
		const float fHeight = Max( rcScreen.Height(), 1.0f );
		// The whole-step rule (see the seam analysis below) applies to the
		// legacy base factor; the fill factor on top of it may be fractional
		// because Task "fractional-safe terrain sampling" removed the seam
		// mechanism. When the world base globals are unset (menus, the ELK
		// editor, the legacy path) this reduces to the old whole-step rule.
		const float fBaseW = float( GetGlobalVar( "GFX.World.BaseSizeX", 0 ) );
		const float fBaseH = float( GetGlobalVar( "GFX.World.BaseSizeY", 0 ) );
		if ( fBaseW < 1.0f || fBaseH < 1.0f )
			return Max( 1.0f, floorf( Min( fWidth / LEGACY_GAMEPLAY_WIDTH, fHeight / LEGACY_GAMEPLAY_HEIGHT ) ) );
		const float fLegacyStep = Max( 1.0f, floorf( Min( fBaseW / LEGACY_GAMEPLAY_WIDTH, fBaseH / LEGACY_GAMEPLAY_HEIGHT ) ) );
		const float fFill = Max( 1.0f, Min( fWidth / fBaseW, fHeight / fBaseH ) );
		return fLegacyStep * fFill;
	}
```

Keep the existing seam comment block in place above the fallback line — it
documents why the base factor stays whole-stepped.

Performance note: `GetGameplayScale` runs in per-sprite loops
(`SceneDraw.cpp:912/965/1018`), and this adds two `GetGlobalVar` hash lookups
per call. After building, run a mission with `BK_PERF=1` and compare
`sceneDraw` ms/frame against a pre-change run; if it regressed measurably,
hoist the two lookups into `CScene` once per frame and pass them through —
do not cache inside the header (statics there would go stale on mode changes).

- [ ] **Step 3: Sync the spec**

In the spec's Missions section, replace the `s_world = s_hud * f` sentence with:

```
  region a `cfg_eff` screen shows today, rendered into the drawable — net
  ortho scale `s_world = legacy_step(cfg_eff) * f`, where `legacy_step` is
  the existing whole-step base factor (preserves Auto exactly) and `f` is
  the uniform fill. `f` is uniform, so no distortion; the non-limiting axis
  shows slightly more world.
```

- [ ] **Step 4: Build and verify**

Build. Run at `Auto` (run_in_background): mission looks exactly as today
(fLegacyStep=1, fFill=1). Run at `1024x768x32`: mission world visibly zooms
~1.17x (compare a unit's on-screen size against an Auto run via F9 captures
from Johannes, or defer the visual check to Task 7's seam verification which
uses the same capture).

- [ ] **Step 5: Commit**

```bash
git add Sources/src/Scene/SceneScreenScale.h docs/superpowers/specs/2026-08-12-resolution-presentation-design.md
git commit -m "feat: world zoom derives from the configured base times the drawable fill"
```

---

### Task 6: Mission HUD scale from the configured base

**Files:**
- Modify: `Sources/src/UI/UIScreen.cpp` (`CUIScreen::Reposition`, the `fScale` line)

**Interfaces:**
- Consumes: `GFX.World.BaseSizeX/Y` (Task 4).
- Produces: edge-anchored (mission HUD) screens scale by
  `min(base_w/1024, base_h/768)` while repositioning against the full
  drawable rect; canvas screens (menus) unchanged.

- [ ] **Step 1: Derive the scale from the base for edge-anchored screens**

In `CUIScreen::Reposition`, replace the `fScale` computation
(`const float fScale = Min( rcScreen.Width() / LEGACY_UI_WIDTH, rcScreen.Height() / LEGACY_UI_HEIGHT );`):

```cpp
		// Edge-anchored screens are the mission HUD: their size comes from
		// the configured resolution (GFX.World.BaseSize, cfg_eff) while their
		// anchors stay on the real drawable edges, so the HUD scales down at
		// low configured resolutions but can never leave the visible area.
		// Canvas screens (menus) keep scaling with their own scene, which is
		// the configured resolution already.
		float fScaleW = rcScreen.Width(), fScaleH = rcScreen.Height();
		if ( bAnchorLayoutToScreenEdges )
		{
			const int nBaseW = GetGlobalVar( "GFX.World.BaseSizeX", 0 );
			const int nBaseH = GetGlobalVar( "GFX.World.BaseSizeY", 0 );
			if ( nBaseW > 0 && nBaseH > 0 )
			{
				fScaleW = float( nBaseW );
				fScaleH = float( nBaseH );
			}
		}
		const float fScale = Min( fScaleW / LEGACY_UI_WIDTH, fScaleH / LEGACY_UI_HEIGHT );
```

If `UIScreen.cpp` does not already include the global-var header, add the same
include found in Task 5 Step 1.

- [ ] **Step 2: Build and verify**

Build. Mission at `Auto`: HUD identical to today (base == drawable). Mission at
`1024x768x32` on the 1440x900 display: HUD elements draw at 1.0x design scale
(smaller than Auto's) and sit on the real screen edges — verify via an F9
capture from Johannes: measure a HUD panel's pixel width in the TGA and check
it equals the panel's width at 1024x768 design scale, and that its outer edge
touches the capture edge.

- [ ] **Step 3: Commit**

```bash
git add Sources/src/UI/UIScreen.cpp
git commit -m "feat: mission HUD scales by the configured base, anchored to the drawable"
```

---

### Task 7: Fractional-safe terrain sampling

**Files:**
- Modify: the terrain tile mesh builder (locate in Step 1 — expected in
  `Sources/src/Scene/TerrainInternal.cpp` or the mesh code feeding
  `STerrainCurrMeshData::Draw` in `Sources/src/Scene/TerraDraw.cpp:28-70`)

**Interfaces:**
- Consumes: nothing new. Removes the constraint that world scales must be
  whole numbers (the seam mechanism), which Task 5's fractional fill relies on.

- [ ] **Step 1: Locate the tileset UV assignment**

The tileset atlas is bound in `STerrainCurrMeshData::Draw`
(`Sources/src/Scene/TerraDraw.cpp:31`, `pGFX->SetTexture( 0, pTileset )`).
Find where the vertices it draws get their UVs:

```bash
grep -rn "Tileset\|tileset" Sources/src/Scene/*.cpp Sources/src/Scene/*.h | grep -iv settexture
grep -rn "STerrainCurrMeshData" Sources/src/Scene/*.cpp Sources/src/Scene/*.h
```

Read the mesh-building code these hits point to and identify the per-tile UV
rect computation (a tile index mapped to an atlas cell, e.g.
`u0 = (tile % columns) * tileWidth / atlasWidth`).

- [ ] **Step 2: Apply a half-texel inset**

At the identified UV computation, inset each tile's UV rect by half a texel so
a point sample taken at a fractionally-scaled tile edge cannot cross into the
neighbouring atlas cell:

```cpp
	// Half-texel inset: at a fractional world scale a tile edge lands inside
	// a pixel, and the point sample for that pixel could fall past the tile's
	// atlas cell into its neighbour - the one-pixel seam lattice measured at
	// scale 1.13 (36px across, 72px down). Pulling the UV rect in by half a
	// texel keeps every sample inside the cell at any scale; the half texel
	// is invisible at tile magnifications >= 1.
	const float fInsetU = 0.5f / fAtlasWidth;
	const float fInsetV = 0.5f / fAtlasHeight;
	u0 += fInsetU; u1 -= fInsetU;
	v0 += fInsetV; v1 -= fInsetV;
```

(Adapt variable names to the code found in Step 1; if UVs are computed
per-vertex rather than as a rect, apply the inset to the min/max UV of each
tile quad. If adjacent tiles share vertices including UVs — no per-tile UV
split — stop and report: the fix then needs a different mechanism and a
design decision.)

Check the other samplers bound in `STerrainCurrMeshData::Draw` (crosset,
noise): apply the same treatment only if they are atlases of tiles; a plain
repeating noise texture needs none.

- [ ] **Step 3: Build and verify numerically (needs Johannes)**

Build. Ask Johannes for two F9 captures from a mission at `GFX.Mode`
`1024x768x32` (fractional fill 1.17): one before this task's change (stash it),
one after. Run the seam metric on both (from
`memory/blitzkrieg-measure-graphics-artefacts.md`) — parse the TGA raw
(uncompressed 32-bit), compute the vertical-line response
`|2*L(x,y) - L(x-1,y) - L(x+1,y)|` summed per column over a terrain-only
region, autocorrelate the per-column sums, and compare the peak at the scaled
tile period (`round(64 * 1.17) = 75` px) against the mean. Expected: the
before-capture shows a clear periodic peak, the after-capture shows none
(ratio to mean < 1.5x, same as a UI-region control).

Write the metric as a throwaway script in the session scratchpad, not the repo.

- [ ] **Step 4: Commit**

```bash
git add Sources/src/Scene/<files found in step 1>
git commit -m "fix: inset terrain atlas UVs half a texel so fractional world scales cannot seam"
```

---

### Task 8: Documentation and end-to-end verification

**Files:**
- Modify: `docs/scaling.md` (mechanisms section)
- Test: full trace matrix + Johannes sign-off

**Interfaces:**
- Consumes: everything above.

- [ ] **Step 1: Update docs/scaling.md**

In the "The scaling mechanisms" section, update the `NSceneScreenScale`
paragraph: the scale is now `legacy_step(cfg_eff) * fill(drawable/cfg_eff)`
with the whole-step rule retired for the fill factor thanks to the half-texel
atlas inset; the UI layout scale for edge-anchored screens comes from
`GFX.World.BaseSize`. Reference the spec
(`docs/superpowers/specs/2026-08-12-resolution-presentation-design.md`). Note
that the note "fullscreen / monitor viewport is planned" is now implemented.

- [ ] **Step 2: Trace matrix**

Build; run each configuration (run_in_background, `BK_GFX_TRACE=1`, kill after
checking; edit `GFX.Mode` in `profiles/Player/config.cfg` between runs):

| Config | Screen | Expected trace |
| --- | --- | --- |
| Auto, fullscreen | menu | `present fit=1 ... scale 1.0000,1.0000 offset 0.0,~16` or exact-fit |
| 800x600, fullscreen | menu | scene 800x600, `scale 1.0000` (1:1, frame), never > 1 |
| 1920x1080, windowed | menu | window clamped to usable bounds; fit scale < 1 |
| Auto, fullscreen | mission (needs a save) | scene == drawable, `fit=0 offset 0,0` |
| 1024x768, fullscreen | mission | scene == drawable, `fit=0 offset 0,0` |

- [ ] **Step 3: Johannes sign-off**

Ask Johannes to verify visually: menu black frame at 800x600 (no stretch), HUD
size and edge-anchoring at 1024x768 in a mission, no terrain seams while
scrolling, live window resizing in a mission.

- [ ] **Step 4: Commit**

```bash
git add docs/scaling.md
git commit -m "docs: record the resolution-derived world zoom and HUD base in scaling.md"
```
