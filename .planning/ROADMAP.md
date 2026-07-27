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
