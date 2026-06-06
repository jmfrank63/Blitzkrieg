# Requirements

## Project goal

Port the legacy Blitzkrieg single-player source code to a modern Windows development workflow while preserving the playable game experience.

## Functional requirements

1. Build support
   - The repository must compile successfully using `Sources/src/A7.sln` in `Debug | Win32`.
   - The `Game` project must build cleanly and produce a working `Game.exe` runtime.
   - Build tasks for `Debug` and `Release` should be available in VS Code.

2. Runtime and debugging
   - The game must launch from the build output directory with current assets.
   - The tutorial and selection/movement gameplay path must be runnable.
   - Native debugging in VS Code Insiders must be supported.
   - WinDbg debugging should be available as a secondary runtime path.

3. Dependency modernization
   - Proprietary libraries used by the project must be identified and isolated.
   - FMOD, BINK, and Stingray dependencies must have a replacement strategy.
   - Open-source alternatives should be evaluated for audio, video codec, and UI systems.

4. Preservation and compatibility
   - Existing `Data/` asset layout must remain compatible with the game runtime.
   - The repository must continue using CRLF line endings and existing formatting conventions.
   - The port must avoid introducing regressions in the core single-player experience.

## Non-functional requirements

- Documentation must clearly explain the modernization plan and next steps.
- The project must remain maintainable by a small team or solo maintainer.
- Build and run instructions should be accurate for the current environment.

## Out of scope for this project initialization

- Full rewrite of the game engine in a new language.
- Replacing all game content or adding new campaigns.
- Multiplayer networking work.
