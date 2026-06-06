# Phase 1 plan: Stabilize build and runtime environment

## Objective

Ensure the current Blitzkrieg repo has a stable modern build and runtime workflow for `Debug | Win32`, and document the verified developer path.

## Success criteria

- `Game.exe` builds cleanly in `Debug | Win32`.
- VS Code build tasks and launch configurations are present and correct.
- Runtime asset path/junction handling is verified.
- The current build/run workflow is documented in planning artifacts and README guidance.

## Tasks

1. Validate current build support
   - Run `Build Game (Debug)` and confirm the build completes successfully.
   - Confirm `Build Game (Release)` and `Build All (Debug)` tasks exist in `.vscode/tasks.json`.
   - Verify `Sources/src/Game/Debug/Game.exe` is produced successfully.

2. Verify VS Code debugging integration
   - Confirm native `cppvsdbg` launch configurations for `Game.exe`, `Game.exe (No Build)`, `FastDebug`, `Release`, and `ELK.exe` exist in `.vscode/launch.json`.
   - Confirm WinDbg launch configurations for `Game.exe` are present.
   - Test at least one native debug launch path if practical.

3. Confirm runtime environment
   - Verify the data junction from `Sources/src/Game/Debug/data` to `Sources/src/data` is present.
   - Confirm the repository can locate game assets through the current build layout.

4. Document the verified workflow
   - Update `.planning/PROJECT.md`, `.planning/REQUIREMENTS.md`, and `.planning/ROADMAP.md` with any new findings from the current workspace.
   - Add or update README guidance if the current VS Code build/debug instructions need clarification.

5. Capture handoff notes
   - Record the validated build/debug workflow in `.planning/phase-1/VERIFICATION.md`.
   - Identify the next entry point for Phase 2: improving native debugger reliability and formalizing dependency inventory.

## Deliverables

- `.planning/phase-1/RESEARCH.md`
- `.planning/phase-1/PLAN.md`
- `.planning/phase-1/VERIFICATION.md`
- Updated project planning docs and README as needed

## Notes

- This phase does not change source code or replace dependencies; it is focused on confirming the current modernization baseline.
