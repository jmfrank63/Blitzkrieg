# Phase 2 research: Modern debugging and developer workflow

## Current environment

- VS Code tasks exist for `Build Game (Debug)`, `Build Game (Release)`, `Build ELK Editor (Debug)`, `Build All (Debug)`, `Build All (Release)`, and `Build Game (FastDebug)`.
- Launch configurations in `.vscode/launch.json` include native `cppvsdbg` targets for `Game.exe`, `Game.exe (No Build)`, `FastDebug`, `Release`, and `ELK.exe`.
- WinDbg launch configurations are also present for `Game.exe`.
- The repository uses `powershell.exe` with `Launch-VsDevShell.ps1` to initialize the Visual Studio developer environment for tasks.

## Focus for this phase

- Verify that the debug workflow is reliable for both native and WinDbg-based sessions.
- Confirm that VS Code project tooling is correctly configured for the legacy `Win32` build target.
- Ensure developer-facing guidance is clear about required toolchain, extension, and shell setup.
- Identify any gaps in the current VS Code integration that would slow future modernization work.

## Findings

- The current `.vscode/tasks.json` is already wired for the modern Visual Studio developer shell and provides a complete build matrix.
- `.vscode/launch.json` is appropriately configured to attach to the current build outputs and uses the proper working directories.
- There is no explicit developer note in `.planning` or `README.md` that calls out `WinDbgX` requirements or the need for the VS Dev Shell setup beyond the task shell config.
- The existing setup is a strong baseline, but the next step is to validate that the runtime entries work as expected in a real debug session.

## Recommendation

- Use Phase 2 to document the debug launch workflows and capture any developer notes needed for VS Code + MSVC + WinDbg.
- Treat the launch config as validated when the following are confirmed:
  - native `cppvsdbg` launch targets are present and correct,
  - WinDbg launch targets exist,
  - build tasks are present and use the correct shell initialization,
  - `Sources/src/Game/Debug/Game.exe` is produced by the build.
- Add an explicit note to `.planning/PROJECT.md` or `README.md` if any build/run instructions are missing or unclear.
