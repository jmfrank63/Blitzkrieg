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

- **Multiple player profiles** — each with its own config and savegames.
  Never implemented in the original (verified against upstream: the
  "PlayerProfile" dialog is just a name edit; one global `config.cfg`, one
  global `saves\` dir; the only existing separation is per-MOD save dirs).
  Design sketch: `profiles\<name>\config.cfg` + `profiles\<name>\saves\`,
  profile-selection list at startup, "last profile" pointer in a root
  config, migration of existing config/saves into a default profile. All
  persistence already funnels through `ResolveConfigFileName` and the
  `saves\` path construction in `CICLoad`/`CICSave`, so the change is
  localized.
- **Load-time optimization, remaining 6s** — `CMainLoop::Serialize`
  manager[0] block is the whole remaining cost of savegame loads
  (instrumented via `load_trace.log` per-manager timings; see
  docs/scaling.md session notes).
- **Chapter-title layout** — our `UI\common\Chapter.xml` deliberately
  diverges from GOG (centered title vs. original left-aligned); revisit if
  further resolutions change the bar/`?`-button geometry.
- **x86→x64 save converter (decision 2026-07-26)** — if x86-era saves turn
  out not to load in the x64 build, do NOT add compatibility shims to the
  engine; write a standalone converter utility instead (or accept fresh
  saves). The x64 engine reads/writes only its native format.
