# Phase 2 plan: Modern debugging and developer workflow

## Objective

Validate and stabilize the VS Code debugging workflow for Blitzkrieg while documenting the required developer toolchain and launch configuration.

## Success criteria

- Native `cppvsdbg` debug launch configurations for `Game.exe` and `ELK.exe` are verified.
- WinDbg launch configurations for `Game.exe` are confirmed.
- Developer tooling notes clearly describe the required shell, MSVC version, and extensions.
- The current debug workflow is captured in planning docs so future contributors can reproduce it.

## Tasks

1. Validate VS Code launch configurations
   - Confirm `.vscode/launch.json` contains native debug entries for `Game.exe`, `Game.exe (No Build)`, `Debug Game.exe (FastDebug)`, `Debug Game.exe (Release)`, and `Debug ELK.exe (Level Editor)`.
   - Confirm `.vscode/launch.json` contains WinDbg entries for `Game.exe`.
   - Verify that `program`, `cwd`, and `preLaunchTask` values are correct for the current build layout.

2. Verify developer tooling setup
   - Inspect `.vscode/tasks.json` to confirm the VS Dev Shell initialization command is correct for `Visual Studio 2026 Insiders` and `Win32` builds.
   - Confirm the MSVC toolchain version and required dependencies are described in planning docs or README.
   - Note any missing guidance for required VS Code extensions such as C++ and WinDbg/WinDbgX.

3. Test debug launch scenarios
   - If possible, launch the native debug configuration for `Game.exe` and confirm the debugger attaches.
   - If possible, launch the WinDbg configuration for `Game.exe` and confirm the target starts.
   - Validate that `Game.exe` can be started from the debug target without asset path failures.

4. Document the developer workflow
   - Add or update a developer note in `.planning/PROJECT.md` or `README.md` covering the VS Code debug workflow and WinDbg requirements.
   - Record any runtime caveats in `.planning/phase-2/VERIFICATION.md`.

5. Capture the handoff
   - Summarize the debug workflow and required tools in `.planning/phase-2/VERIFICATION.md`.
   - Identify the next phase entry point: dependency inventory and SDK replacement planning.

## Deliverables

- `.planning/phase-2/RESEARCH.md`
- `.planning/phase-2/PLAN.md`
- `.planning/phase-2/VERIFICATION.md`
- Updated planning or README notes as needed
