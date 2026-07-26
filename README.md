[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



This repository is a personal pet project by Johannes Maria Frank. I use it for fun and learning, especially to practice agentic coding in brownfield application work. The original README is preserved in `readme_original`.

Warning: this project is a work in progress.



# What this repository can do today

- Contains the full Blitzkrieg single-player source code and game data.

- Builds the whole game with `zig build` (Zig + clang), producing a runnable install layout under `zig-out/Game/<arch>/<config>` — no Visual Studio required for the build itself.

- Builds and runs as a native **64-bit** Game.exe (`zig build install-game -Dtarget=x86_64-windows-msvc`) alongside the 32-bit build. Campaign missions and tutorials are playable in x64, including save/load, video, and sound.

- Ships with StreamIO.dll fully replaced by a Zig implementation (`Sources/src/StreamIOZig`) — the engine's entire persistence layer: binary save/load object graphs, XML data trees, the object factory, global variables, console, and options.

- Compiles the legacy C++ with clang and UBSan enabled, which has surfaced and fixed dozens of latent bugs the 2003 MSVC build silently tolerated — including a 22-year-old pure-virtual-call crash in the tank/turret teardown.

- Still builds cleanly from the original `A7.sln` solution under modern MSVC tooling (`Debug | Win32`) as a behavioral reference.

- Runs in a real window by default, with `-fullscreen` still available as a command-line option.

- Uses Git submodules for missing libraries.

- Supports both native C++ and WinDbg debugging in VS Code Insiders, plus headless crash capture scripts under `tools/zig/`.

- Provides a helper workflow for compressed movie asset packaging under `VideoAssets/`.

- Has removed legacy BugSlay crash handling and replaced it with standard C++ asserts.



# Blender asset conversion and validation

The Maya40 replacement pipeline is documented in `docs/blender-replacement.md`.

Quick commands:

- Convert: `BZMConvertor.exe -obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Validate only: `BZMConvertor.exe -validateobj2mod <input.obj> [skeleton.txt] [animation.txt]`



# Running the game with Zig (primary)

1. Clone the repository with submodules, or run `git submodule update --init --recursive` in an existing checkout.

2. Install Zig (0.16 or later) and the MSVC toolchain + Windows SDK (paths are configurable via `-Dmsvc-include`/`-Dwindows-sdk-include` and their `lib` counterparts if yours differ from the defaults in `build.zig`).

3. Run `zig build install-game` for 32-bit, or `zig build install-game -Dtarget=x86_64-windows-msvc` for 64-bit.

4. Start `zig-out/Game/x86/Debug/Game.exe` (or `zig-out/Game/x64/Debug/Game.exe`).

5. `zig build package` creates distributable zip packages; `zig build test` runs the Zig unit tests and the C++ ABI smoke test.



# Running the game with Visual Studio 2026 Insiders

1. Clone the repository with submodules, or run `git submodule update --init --recursive` in an existing checkout.

2. Install Visual Studio 2026 Insiders / Visual Studio 18 with the Desktop development with C++ workload, the VS 2026 MSVC toolchain, and a Windows 10 or Windows 11 SDK.

3. Open `Sources/src/A7.sln`.

4. Select `Debug | Win32`.

5. Build the `Game` project, or build the full solution.

6. Start the `Game` project with F5, or run `Sources/src/Game/Debug/Game.exe` directly.

7. The game starts windowed by default. Add `-fullscreen` to the Game project command arguments if you want the old fullscreen behavior.

If a build cannot copy a DLL into `Sources/src/Game/Debug`, close any running `Game.exe` process and build again.



# History so far

- Found the missing libraries and added them as Git submodules.

- Built a Windows XP SP3 VM with Visual Studio 6 to study the original environment.

- VS 6 was unstable and crashed often, so the effort shifted to modern tooling.

- Installed Visual Studio 2010 and converted the old `.dwr` project to `.sln`.

- Loaded the solution into VS 2026 Insiders and updated all dependencies.

- Completed two weeks of agentic coding to make the solution compile from a clean state with zero errors and warnings.

- Runtime debugging has now progressed past the initial menu: the tutorial loads and is playable, including selecting and moving a tank.

- Moved development to VS Code Insiders for better tools.

- Configured two debug paths: native C++ and WinDbg.

- Removed BugSlay because it made debugging harder and in one case crashed after a crash.

- Replaced BugSlay with simple standard C++ asserts.

- With BugSlay removed, the project is now ready for focused runtime debugging toward a full run.

- Moved the build to Zig: `build.zig` compiles every project with clang, links against the MSVC toolchain, and stages a runnable game layout.

- Replaced StreamIO.dll — the engine's persistence and service layer — with a Zig implementation plus a C++ ABI bridge, fixing the save/load object-graph format, XML read/write, options, and per-frame performance regressions along the way.

- Made the game playable end to end under the Zig build: campaigns, tutorials, boarding, aviation, save/load round-trips, video in sync, minimap correct at modern resolutions.

- Ported the game to 64-bit: fixed pointer truncations in the renderer, x87-era `_control87` calls, `#pragma pack(1)` on STL types, x86 inline assembly (replaced with portable SSE intrinsics), the Lua unit-pointer channel, and the save-format object IDs.

- Play-tested the x64 build through the tutorials crash by crash, with UBSan turning each 20-year-old undefined behavior into an exact file and line: minimap coordinates, formation hashing, bombardment angle math, and more.



# Roadmap

1. Finish stabilizing the 64-bit build through the remaining tutorials and campaigns.

2. Build a utility to convert 32-bit save games to the 64-bit format.

3. Replace FMOD, Stingray, and Bink with open source alternatives.

4. Continue replacing C++ projects one by one with Zig code, using the StreamIO port as the template.
