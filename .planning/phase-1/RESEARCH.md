# Phase 1 research: Stabilize build and runtime environment

## Current state

- The repository already contains a functional VS Code task set in `.vscode/tasks.json` for building `Game`, `ELK`, and full solution configurations.
- `.vscode/launch.json` includes native `cppvsdbg` debug launch configurations for `Game.exe`, `ELK.exe`, and WinDbg support for `Game.exe`.
- A successful `Debug | Win32` build was verified using the `Build Game (Debug)` task.
- The game runtime path uses a junction from `Sources/src/Game/Debug/data` to `Sources/src/data`, preserving the legacy asset layout.

## What this phase must solve

- Confirm that the existing modern MSVC build path is stable and documented.
- Verify that VS Code launch and task integration is working for the current repository state.
- Capture the current build/runtime workflow as actionable documentation.
- Identify early risks that could block later dependency modernization work.

## Risks and constraints

- Proprietary SDKs are still present in `Sources/sdk` and the current build may depend on them.
- The legacy `Win32` target and junction-based asset layout mean runtime behavior is sensitive to path configuration.
- Existing tooling is modernized, but the codebase remains largely legacy C++ with assumptions that may break under future refactoring.

## Research conclusions

- Phase 1 should prioritize existing build/runtime stability over dependency migration.
- The current workspace already has the required VS Code tasks and debug launch configurations, so this phase can focus on validation and documentation.
- A clean build and a confirmed debug workflow are sufficient to move to Phase 2, where tooling and debugging are further hardened.
