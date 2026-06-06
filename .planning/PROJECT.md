# Blitzkrieg Reloaded

## Project summary

This project is a focused modernization effort for the legacy Blitzkrieg single-player source code. The main goal is to port the game to modern toolchains, preserve the playable tutorial/runtime experience, and remove or replace proprietary dependencies so the project can continue as an open, maintainable engine.

## Why this project exists

- The repository already builds successfully in a modern MSVC environment and runs a working tutorial path.
- The original game depends on legacy SDKs and proprietary libraries that make long-term maintenance and portability difficult.
- Modernizing the build and runtime stack will preserve the game and enable future migration work.

## Primary objective

Port Blitzkrieg to a modern Windows development workflow by:

- maintaining a clean MSVC build on `Sources/src/A7.sln`
- supporting launch and runtime debugging in VS Code Insiders
- removing or replacing proprietary SDKs (FMOD, BINK, Stingray)
- keeping the original gameplay behavior intact

## Success criteria

- `Sources/src/A7.sln` builds cleanly in `Debug | Win32` with no compiler errors and a working `Game.exe` runtime.
- The game can be started from VS Code with a reliable native and/or WinDbg debugging path.
- Proprietary or closed-source dependencies are isolated and have a migration plan.
- The repository contains a clear roadmap and a phase plan for future implementation.

## Scope

In scope:

- modern Windows toolchain support for the existing C++ source tree
- debugging configuration for VS Code Insiders
- documentation of project requirements and roadmap
- dependency replacement planning for FMOD, BINK, and Stingray
- preparation for a future Zig migration pilot

Out of scope for this initialization phase:

- full Zig rewrite of the game engine
- replacing the complete game asset pipeline in this first project
- multiplayer or new game content beyond runtime stability

## Current constraints

- Target platform remains `Win32` for compatibility with the legacy codebase.
- The repository uses CRLF line endings and must preserve existing formatting conventions.
- The project relies on `Data/` game assets; runtime paths must remain compatible with the existing build layout.

## Stakeholders

- Johannes Maria Frank (project owner)
- Maintainers of the Blitzkrieg repository
- Future contributors interested in modernizing legacy game code

## Next step

Run `/gsd-plan-phase 1` to turn this plan into the first execution phase.
