# Roadmap

## Phase 1 – Stabilize build and runtime environment

Goals:

- Confirm current MSVC build and runtime path.
- Add explicit VS Code build/debug tasks and documentation.
- Capture the project scope and success criteria in planning docs.

Key outcomes:

- Clean `Debug | Win32` build of `Game`.
- Working Game runtime from `Sources/src/Game/Debug/Game.exe`.
- Documented build/run/debug workflow in `.planning/PROJECT.md` and `README.md`.

## Phase 2 – Modern debugging and developer workflow

Goals:

- Ensure native VS Code debugging is reliable.
- Verify WinDbg support for low-level runtime inspection.
- Improve tooling for working with legacy code.

Key outcomes:

- Configured VS Code tasks and launch settings for `Game` and `ELK`.
- Confirmed debugger attach/launch workflows.
- Added developer notes for VS Code and MSVC toolchain requirements.

## Phase 3 – Dependency replacement planning

Goals:

- Audit proprietary SDK usage in the codebase.
- Evaluate open-source replacements for FMOD, BINK, and Stingray.
- Start isolating legacy libraries behind migration boundaries.

Key outcomes:

- Inventory of proprietary dependencies.
- Replacement strategy for audio, video codec, and UI.
- Prototype or stubbed integration points for alternatives.

## Phase 4 – Runtime stability and compatibility

Goals:

- Reduce remaining runtime exceptions and crashes.
- Harden the game runtime for the legacy tutorial path.
- Preserve compatibility with data and asset loading.

Key outcomes:

- Stable tutorial and mission startup in `Debug` build.
- Clear regression tests or manual validation checklist.
- Runtime stability improvements documented in `.planning/STATE.md`.

## Phase 5 – Zig migration pilot preparation

Goals:

- Create a small pilot plan for migrating a targeted subsystem to Zig.
- Preserve the C++ branch while preparing for hybrid evolution.
- Keep the overall game runnable as the migration proceeds.

Key outcomes:

- Pilot scope and success criteria for Zig porting.
- Clear boundary between legacy C++ and new Zig code.
- A follow-up roadmap item for `/gsd-plan-phase 2` or later.

## Phase 6 – 64-bit transition (branch: 64transition)

Goals:

- Move the whole game from x86 to x86_64.
- Build infrastructure is already done: every module compiles and links for
  `x86_64-windows-msvc` (`zig build -Dtarget=x86_64-windows-msvc`); the only
  linker blocker (StreamIO stdcall-decorated exports) is fixed via
  `StreamIO.x64.def`.
- The real work is runtime pointer hygiene: `DWORD(pointer)` truncations
  (e.g. `CTextureLock` in GFXHelper.h), 4-byte pointer IDs in the save
  format, struct layout changes in anything serialized or memcpy'd.

## Feature backlog

- **Multiple player profiles** — each with its own config and savegames,
  with optional password protection per profile. Never implemented in the
  original (verified against upstream: the "PlayerProfile" dialog is just a
  name edit; one global `config.cfg`, one global `saves\` dir; the only
  existing separation is per-MOD save dirs).
  Design sketch: `profiles\<name>\config.cfg` + `profiles\<name>\saves\`,
  profile-selection list at startup, "last profile" pointer in a root
  config, migration of existing config/saves into a default profile. All
  persistence already funnels through `ResolveConfigFileName` and the
  `saves\` path construction in `CICLoad`/`CICSave`, so the change is
  localized.
  Password protection (decision 2026-07-27): the whole per-profile folder
  is encrypted with a key derived from the profile password — real
  protection of the content, not just a UI gate. Natural hook point: all
  profile file I/O (config + saves) already flows through the zig StreamIO
  file streams (`bk_stream_*` in streamio.zig), so a transparent
  encrypt/decrypt layer keyed per-profile can live there without touching
  the C++ callers; key derivation from the password prompt at profile
  selection (needs a masked-input mode in `CUIEditBox`). Forgotten password
  = unrecoverable profile — needs a clear warning at creation.
  Per-profile cutscene unlocks fold in naturally since the cutscenes menu
  now derives them from the profile's own `saves\` dir (2026-07-27
  save-derived unlock logic; the scan must run after the profile is
  unlocked so headers are decryptable).
- **Use x64 address space: preload/cache aggressively** — the 32-bit
  build's 2GB ceiling shaped every eviction policy; x64 removes it. Ideas:
  preload the texture/mesh/sound pool during game startup (or campaign
  select) so mission loads only deserialize state; keep shared managers
  warm across missions instead of purging (the SDSM_MERGE + deferred-purge
  machinery from 2026-07-27 already supports reuse — a "never purge, evict
  only on pressure" mode is the natural extension); cache parsed XML/GDB
  and decoded map data. Measure win via the [share] trace lines.
- **MCP server to control the game** — expose the running game to an AI
  agent (and to automated testing) as MCP tools. Building blocks already
  proven in the debug workflow: direct mission launch (unquoted
  `-<mission>.xml` arg), direct save launch (`-<name>.sav` arg),
  `RedirectStandardError` panic capture (exit 3 = zig panic, 0xDEAD =
  second instance — check `Get-Process Game` first), PrintWindow-based
  screenshot capture of the occluded/fullscreen window, `load_trace.log` /
  `bk_stderr.log` telemetry. Command injection candidates: the console
  command stream (`IConsoleBuffer` world-command channel that LUA tutorials
  already use) and `IMainLoop::Command`; input injection via the
  `EmulateInput` bind path if real clicks are needed. Natural tool set:
  launch/attach, screenshot, read-state (units/selection via
  `ReturnScriptIDs`-style queries), issue-command, save/load, quit.
- **Load-time optimization** — `CMainLoop::Serialize` manager[0] block is
  nearly the whole cost of savegame loads (instrumented via
  `load_trace.log` per-manager timings; see docs/scaling.md session
  notes). Data points (x64 Debug build): tutorial save ~13s, mid-campaign
  ~26s, "USSR Leningrad1" 45s (user-reported 2026-07-27) — grows with
  mission size, so the map/terrain/texture load inside manager[0]
  dominates. Before optimizing the Debug numbers, measure a ReleaseFast
  build: Debug is clang -O0 + UBSan and known ~2-3x slower (the theora
  lesson); the fix may be partly "play on Release".
  KEY MECHANISM (analyzed 2026-07-27): the shared-resource managers
  (texture/mesh/anim/sound/particle shares, BasicShare.h) already default
  to `SDSM_MERGE` serialization — same-name resources still resident are
  reused via `SwapData` with NO disk I/O. But `CICLoad` pops all
  interfaces first, and every `PopInterface` calls
  `ClearResources(false)` → `Clear(CLEAL_UNREFERENCED)` — the dying world
  releases its refs, the purge empties the shares, and the merge finds
  nothing to reuse. FIX SHAPE: in the load path, defer the unreferenced
  purge until AFTER `Serialize` (pop without clearing, deserialize with
  merge, then purge what the new world doesn't reference). A same-mission
  load (death retry — the dominant case) then reuses nearly everything →
  seconds instead of 45s; cross-mission loads still correct, briefly
  holding two missions' resources (fine on x64). Also: each PopInterface
  in the pop-all loop runs the 7-manager purge — O(stack depth) wasted
  work even outside loads. Secondary wins: compile zlib/pak-inflate and
  image decode ReleaseFast inside Debug builds (proven xiph pattern in
  build.zig); parallel file-read+decode with main-thread-only D3D upload.
- **Fullscreen without distortion** — render at the monitor's native
  aspect ratio instead of stretching the 4:3-era projection. NOT easy
  (user's assessment, shared): the engine assumes one global screen rect —
  `NSceneScreenScale` gameplay projection, UI layout scaling
  (`ShouldScaleLegacyLayout`), minimap pow2-vs-viewport assumptions (the
  2026-07-26 minimap bug class), `SetDstRect` video letterboxing, cursor
  and pick coordinate transforms all bake it in. Likely shape:
  aspect-correct ortho + pillarbox/expanded FOV decision per subsystem,
  and an audit of every `GetScreenRect()` consumer. Prerequisite notes in
  the minimap memory: the pow2-texture-vs-size assumption may lurk in
  other viewport-derived code.
- **Vulkan renderer** — replace the D3D8 backend to unlock cross-platform
  compilation (the zig build already cross-compiles everything except the
  Win32/D3D8 layer). All device access already funnels through
  `IGFX`/`CGraphicsEngine` (GFX.dll), so the port surface is one module
  plus the D3D8-isms leaked through it (FVF vertex formats, `IGFXVertices`
  buffer semantics, `SetShadingEffect` fixed-function states, RTT via
  `IGFXRTexture`, `IsSafeToPresent` scene bracketing). Suggested path:
  first wrap D3D8 usage behind a narrower internal RHI inside GFX.dll,
  then add the Vulkan implementation; windowing/input (WinFrame) and SFX
  (DirectSound-era) need their own cross-platform stories — consider SDL
  for both when the time comes.
- **Chapter-title layout** — our `UI\common\Chapter.xml` deliberately
  diverges from GOG (centered title vs. original left-aligned); revisit if
  further resolutions change the bar/`?`-button geometry.
- **x86→x64 save converter (decision 2026-07-26)** — if x86-era saves turn
  out not to load in the x64 build, do NOT add compatibility shims to the
  engine; write a standalone converter utility instead (or accept fresh
  saves). The x64 engine reads/writes only its native format.

### Phase 2: Variable zoom and minimap scaling

**Goal:** Add player-controlled variable map zoom bounded between the configured
settings resolution (max zoom-out limit) and 640x480 effective viewport (max
zoom-in limit), decouple the minimap from map zoom by giving it a fixed size
relative to screen width (minimap dialog + diamond hold their authored
baselines at every resolution; the layout fixup closes the legacy
rail/status-bar gap on wide drawables — amended 2026-09-10, the original
"half the available width" rule was unreachable with the dialog's
fixed-size multi-tile art), and add zoom controls: Shift+mouse wheel, J
(zoom in), K (zoom out), L (reset zoom).

**Requirements**: TBD
**Depends on:** Phase 1
**Plans:** 3 plans

Plans:

- [x] 01 — zoom state, bounds, input binds, GPU mirror, persistence resets (complete)
- [x] 02 — cursor-anchored zoom application and terrain rebuild trigger (complete)
- [x] 03 — minimap cluster sizing, texture recreation (complete)

**Complete 2026-10-05** — executed, verified (source level), and the in-game sign-off
approved by Johannes (variable zoom and minimap layout); see
`.planning/phases/02-variable-zoom-and-minimap-scaling/02-VERIFICATION.md` (status passed).

### Phase 3: Map editor plan 6: finish M1

**Goal:** Meet the M1 exit criteria of `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`: test-launch the edited map in the game (the game plays the saved map), load a mod's data like the game, camera rotate and zoom, safe save (temporary file and swap) with the unsaved-changes prompt, editor settings and recent files, `BK_EDITOR_AUTO` automation with shot comparison, the full open/save sweep of every shipped map, and packaging `MapEditor` with the game — plus the "Carried to plan 6" list of `docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md` (object icons, brush outline via world-to-screen, panels following a resize, the map's sound list, the unknown-objects warning, the Windows console subsystem, and the deferred minors). Plans 1–5 of the map editor are merged (main 6657668a6).
**Requirements**: CONTEXT D-01..D-29, the spec's M1 exit criteria, plan 5's "Carried to plan 6" list
**Depends on:** Map editor plans 1–5 (merged); independent of Phase 2
**Plans:** 16/16 plans complete (15 planned + gap closure 03-16). **Complete 2026-09-29** — verified (03-VERIFICATION.md passed, 03-UAT.md 2/2), Johannes's M1 hand try approved on macOS and Windows, CI run 36588755990 green.

Plans:
**Wave 1**

- [x] 03-01-PLAN.md — the game's `-editor-test` switch: session-only MapEditorTest profile, no cloud sync, windowed, no first-visit help; BK_AUTO_UI `units=`

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 03-02-PLAN.md — Test in game from the editor (F5, restart prompt, failure report) and the "game reads it" tier

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 03-03-PLAN.md — safe save: temporary file, bridge read-back, one .bak per session, atomic swap

**Wave 4** *(blocked on Wave 3 completion)*

- [x] 03-04-PLAN.md — unsaved-changes prompt, shipped maps read-only, user maps folder

**Wave 5** *(blocked on Wave 4 completion)*

- [x] 03-05-PLAN.md — camera zoom like the game (Shift+wheel, pinch, Home), per-map view memory, BkEditorWorldToScreen and the brush outline

**Wave 6** *(blocked on Wave 5 completion)*

- [x] 03-06-PLAN.md — camera rotation: measure the renderer at other yaws, Johannes decides, build if it draws correctly (checkpoint)

**Wave 7** *(blocked on Wave 6 completion)*

- [x] 03-07-PLAN.md — mapeditor.cfg and the Settings window, Open Recent, autosave and recovery copies

**Wave 8** *(blocked on Wave 7 completion)*

- [x] 03-08-PLAN.md — mods: `-mod=`, File > Mod, mod passed to the test game, mod maps in user data recording their mod

**Wave 9** *(blocked on Wave 8 completion)*

- [x] 03-09-PLAN.md — object pictures in the palette (shipped icon.tga through the engine; checkpoint on the rest)

**Wave 10** *(blocked on Wave 9 completion)*

- [x] 03-10-PLAN.md — the map's sound list: listed, edited with undo, marked on the map

**Wave 11** *(blocked on Wave 10 completion)*

- [x] 03-11-PLAN.md — unknown-objects warning, panels follow a resize, gesture guard, app-side carried minors

**Wave 12** *(blocked on Wave 11 completion)*

- [x] 03-12-PLAN.md — BK_EDITOR_AUTO with shot comparison; the spec's editor-app scenario

**Wave 13** *(blocked on Wave 12 completion)*

- [x] 03-13-PLAN.md — bridge and engine-tier carried minors; host check honours test mode

**Wave 14** *(blocked on Wave 13 completion)*

- [x] 03-14-PLAN.md — packaging MapEditor beside Game; Windows GUI subsystem with console attach

**Wave 15** *(blocked on Wave 14 completion)*

- [x] 03-15-PLAN.md — full open/save sweep, whole suite and CI, spec updated, Johannes's M1 hand try

**Gap closure**

- [x] 03-16-PLAN.md — plan-5 leftovers (status line, view.zig tests, literal scroll test), Restart exit popup, game-reads-it baseline, CI package job, release package ordering, editor independent of the working directory

### Phase 4: Map editor M2: roads, rivers, bridges, AI groups, scripts

**Goal:** Map Editor M2 of `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` ("The editor set"): edit what M1 only preserves — roads and rivers, bridges (including rotating them, deferred from M1) and entrenchments, AI and unit groups (reinforcements, start commands, reserve positions, AI general data), scripts and script areas — with the same undo, save-preservation, test-in-game and CI standards as M1. Revisit free camera rotation (D-12, deferred with evidence in 03-06). Scope to be settled in discuss-phase; may split into several phases.
**Requirements**: CONTEXT D-01..D-25 (04-CONTEXT.md) and the M2 rows of 04-PARITY.md
**Depends on:** Phase 3
**Plans:** 13/13 plans executed (D-24's eight split for one agent context each; executed one after another in the shared worktree)

Plans:
**Wave 1**

- [x] 04-01-PLAN.md — record foundation: NMapRecords for every M2 collection, generic record_edit command traced by camera anchors, palette filter, spec and decision-log amendments

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 04-02-PLAN.md — cascade delete and fixed FindReferences (start commands, reserve positions, script-ID notes)

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 04-03-PLAN.md — app foundations: named commands and predicates, camera-anchor UI, marker layer, right button/double click/keys, tool registry, automation verbs, map-editor-auto-m2

**Wave 4** *(blocked on Wave 3 completion)*

- [x] 04-04-PLAN.md — BK_MAP_TRACE game seam and Lua Trace mirror, map-editor-game-reads-it-m2 (camera anchor), GFXGPU road/river render proof

**Wave 5** *(blocked on Wave 4 completion)*

- [x] 04-05-PLAN.md — roads and rivers: edit log, CVSOBuilder derive, river passability, MFC gestures, panel and markers

**Wave 6** *(blocked on Wave 5 completion)*

- [x] 04-06-PLAN.md — bridges: PlanBridge, draw/select/delete as wholes, rotate (_01/_02), built during play with its mark

**Wave 7** *(blocked on Wave 6 completion)*

- [x] 04-07-PLAN.md — fences: PlanFences, AI tile mapping, Fence tool

**Wave 8** *(blocked on Wave 7 completion)*

- [x] 04-08-PLAN.md — entrenchments: PlanEntrenchment port with property tests, draw/select/delete as wholes; mid-phase CI gate

**Wave 9** *(blocked on Wave 8 completion)*

- [x] 04-09-PLAN.md — script IDs and reinforcement groups: Script ID field, Group Manager, Hide checked, Select objects

**Wave 10** *(blocked on Wave 9 completion)*

- [x] 04-10-PLAN.md — script file (dialog, copies beside map/test map/Save As) and script areas (tool, panel, handles); the game runs the script

**Wave 11** *(blocked on Wave 10 completion)*

- [x] 04-11-PLAN.md — start commands and artillery reserve positions

**Wave 12** *(blocked on Wave 11 completion)*

- [x] 04-12-PLAN.md — AI general: sides, mobile script IDs, parcels and reinforce points

**Wave 13** *(blocked on Wave 12 completion)*

- [x] 04-13-PLAN.md — integration and exit: full game-reads-it and editor-app scenarios, preservation sweeps, CI and win-home, release walk-through, parity evidence

### Phase 5: Map editor M3: random map templates, minimap tools, full parity

**Goal:** Map Editor M3 of `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`: random map templates and generation from the editor, minimap tools, and every remaining feature of the MFC map editor (`Sources/src/MapEditor`) so the portable editor reaches full parity. When parity is shown by a feature-by-feature checklist against the MFC editor, delete the MFC map editor from the tree.
**Requirements**: none mapped — CONTEXT D-01..D-40 (05-CONTEXT.md) are the requirements of record; 05-PARITY.md is the checklist of record (D-36) and 05-VALIDATION.md's D-40 exit criteria are the coverage contract
**Depends on:** Phase 4
**Plans:** 11/11 plans complete. **Complete 2026-10-05** — verified (05-VERIFICATION.md passed, 05-UAT.md 14/14), Johannes's M3 hand try approved on macOS and Windows, MFC map editor deleted, CI run 37219830755 green at main 1d7264fd6.

Plans (per D-39; waves serialized — plans 2-8 all touch bridge.h/editor.zig/fake_bridge.zig/c_bridge.zig/commands.zig/build.zig, so the shared-worktree execution order is plan order; depends_on keeps D-39's logical structure):

- [x] 05-01-PLAN.md — foundations: altitude region primitive (D-19), New Map (D-23), Save as XML/BZM (D-24), brush 1-16, status bar/title, map-editor-m3-auto step, PARITY anchor verification, spec D-19
- [x] 05-02-PLAN.md — heights tool (D-18), Update Map / Instant Update / Fit To Grid (D-20), Fill Entire Map (D-22), tile properties (D-35)
- [x] 05-03-PLAN.md — object filters + Filters Composer (D-31), Fields tool (D-21), the shared RMG storage scan
- [x] 05-04-PLAN.md — multi-selection (D-25), Properties panel (D-26), links (D-27), direction wheel (D-28), Damage tool (D-29)
- [x] 05-05-PLAN.md — players add/delete + Unit Creation Info (D-30), Check Map (D-33), the railroad <2-control-points game-loader guard (carried bug)
- [x] 05-06-PLAN.md — Layers menu and fire ranges (D-32), Wireframe/Depth-Complexity measured
- [x] 05-07-PLAN.md — Minimap panel (D-14..D-16), Create Minimap Images (D-17), the D-40.6 shot proof
- [x] 05-08-PLAN.md — Create Random Map (D-01..D-04, the D-05 szMODName fix, determinism) and Export lists (D-13)
- [x] 05-09-PLAN.md — Containers + Graphs composers (D-06..D-12), user RMG storage root (D-09, costly), round-trip half 1
- [x] 05-10-PLAN.md — Fields + Templates composers (R7-R13, the implemented Check!), round-trip complete (D-40.4), authored end-to-end (D-40.5)
- [x] 05-11-PLAN.md — app shell (D-34: Options, View, drag-drop, single instance, Help), full parity verification + gates, hand-try prep, MFC editor deletion behind a one-way checkpoint (D-38)

### Phase 6: Resource Editor: portable port of editor.exe

**Goal:** Port the MFC resource editor (`Sources/src/editor`, `editor.exe`, about 64,000 lines, more than 20 sub-editors: units, weapons, buildings, objects, effects, bridges, fences, entrenchments, particles, animations and the rest) to the portable editor stack (Zig app + Dear ImGui + C bridge to the engine) on macOS and Windows, with its own design spec, undo, safe save, output the game reads unchanged, and CI tiers like the map editor. Every sub-editor is ported; the MFC resource editor is deleted when parity is shown.
**Requirements**: TBD
**Depends on:** Phase 5
**Plans:** 0 plans

Plans:

- [ ] TBD (run /gsd-plan-phase 6 to break down)

### Phase 7: ELK: portable localisation kit

**Goal:** Port ELK, the Blitzkrieg localisation kit (`Sources/src/ELK`, about 12,000 lines: text database, import from game/PAK/XLS, export, translation editing with filters, statistics, spell check, fonts), to the portable stack on macOS and Windows with its own design spec; its output must be read by the game unchanged. Delete the MFC ELK when parity is shown.
**Requirements**: TBD
**Depends on:** Phase 6
**Plans:** 0 plans

Plans:

- [ ] TBD (run /gsd-plan-phase 7 to break down)

### Phase 8: Small tools: portable converters and checkers

**Goal:** Port or replace the remaining Windows-only tools (`ExcelExporter`, `FontGen`, `bzmconvertor`, `imagedefrag`, `WhereIS`, `spcomp`, `OffsetRomb`, `betakeygen` and the rest found by an inventory) with portable command-line tools built by `zig build` on macOS and Windows, each with tests; decide per tool (port, replace with an existing path, or drop with a written reason). Remove the shipped Windows binaries from `Sources/Tools` that are replaced.
**Requirements**: TBD
**Depends on:** Phase 7
**Plans:** 0 plans

Plans:

- [ ] TBD (run /gsd-plan-phase 8 to break down)

## Backlog

### Phase 999.1: Random map generation: fast polygon fill (BACKLOG)

**Goal:** Cut "CreateRandomMap. Fill polygons." (2.5–15 s per map in the Windows debug CI tier, median 10 s; "Find Polygons." is 0–1 ms) without changing a single generated map: the same seed must still produce byte-identical maps. Take it up after map editor plan 6 (Phase 3). Analysis and the proposed order (measure, determinism check, cheap fixes, then an edge grid) in `.planning/phases/999.1-random-map-generation-fast-polygon-fill/999.1-NOTES.md`.
**Requirements:** TBD
**Plans:** 0 plans

Plans:

- [ ] TBD (promote with /gsd-review-backlog when ready)

### Phase 999.2: Smaller installer: derive textures instead of shipping them, modern compression (BACKLOG)

**Goal:** Make the **download** as small as possible; installed size does not matter (decision 2026-09-29: a few GB on disk is fine). Ship only what cannot be derived — drop the `_c` (DXT) and `_l` (16-bit) copies of every texture (about 910 MB, a third of `Data`) and the generated season textures (about 67 MB) from the download and recreate them at install time (or have the renderer use `_h` directly), never touching Nival's hand-painted season textures. Compress the download as hard as possible (xz or zstd at maximum settings, long window), then unpack fully on install; `.pak` stays supported (mods, GeneratedData), and a `.pak` can travel inside the compressed download and be written back out as `.pak` at install. Take it up after map editor plan 6 (Phase 3). Details in `.planning/phases/999.2-smaller-installer-derived-textures-modern-compression/999.2-NOTES.md`.
**Requirements:** TBD
**Plans:** 0 plans

Plans:

- [ ] TBD (promote with /gsd-review-backlog when ready)
