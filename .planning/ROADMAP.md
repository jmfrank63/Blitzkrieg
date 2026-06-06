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
