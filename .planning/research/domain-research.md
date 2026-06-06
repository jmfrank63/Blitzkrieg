# Domain research: modernizing a legacy Windows RTS game

## Context

The Blitzkrieg repository contains a legacy Windows real-time strategy game with a large C++ codebase and several closed-source dependencies. The current branch name indicates a focus on runtime execution and modern build support.

## Key research findings

1. Modern Windows toolchain support
   - Building the project with a current MSVC toolchain is viable if include paths and project settings are updated carefully.
   - VS Code Insiders can host MSVC builds and native debugging through `msbuild` tasks and the C++ debug adapter.
   - Maintaining the existing `Win32` target preserves compatibility with the legacy binary interfaces.

2. Proprietary dependency replacement
   - FMOD: A common open-source alternative for audio is OpenAL, `miniaudio`, or SDL2 audio. For minimal integration, `miniaudio` is attractive because it is header-only and supports legacy Windows audio output.
   - BINK: Video codec replacement can be handled with FFmpeg or a lightweight codec library. Since the project likely uses BINK for cutscene playback, a stubbed fallback or external conversion pipeline is a useful intermediate step.
   - Stingray: Legacy UI and runtime systems can be migrated gradually. Immediate steps include isolating Stingray calls and identifying the smallest runtime surface area.

3. Runtime and debugging
   - Native debugging in VS Code is straightforward once MSVC build output paths are stable.
   - WinDbg is useful for investigating low-level crashes in a legacy 32-bit executable.
   - Existing runtime debug support should be documented and preserved for future contributors.

4. Project constraints
   - The repository uses CRLF and Windows-native build scripts.
   - `Data/` and `Sources/` asset paths are already part of the build layout.
   - Legacy code may assume old compiler behavior and platform APIs, so changes should be incremental.

## Recommendation

- Start by stabilizing the existing build and runtime path.
- Create a dependency inventory for FMOD, BINK, and Stingray usage.
- Prototype an isolated replacement path for one proprietary dependency before doing a full migration.
- Keep the game runnable while making changes; avoid broad refactors until the build/runtime baseline is secure.

## Notes for next phase

- The next phase should generate an implementation plan for `/gsd-plan-phase 1` based on the current branch state.
- Use the research findings to validate the roadmap: prioritize build stability, debugging support, and dependency isolation.
