[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



This repository is a personal pet project by me (Johannes Maria Frank). I use it for fun and learning, especially to practice agentic coding in brownfield application work. The original README is preserved in `readme_original`.

Warning: this project is a work in progress.



# What this repository can do today

- Contains the full Blitzkrieg single-player source code and game data.

- Builds cleanly from a fresh `A7.sln` solution under modern MSVC tooling.

- Runs in a real window by default, with `-fullscreen` still available as a command-line option.

- The tutorial is now working in the `Debug | Win32` build; units can be selected and moved while runtime debugging continues.

- Includes native exception reporting and modern debugging support.

- Uses Git submodules for missing libraries.

- Supports both native C++ and WinDbg debugging in VS Code Insiders.

- Has removed legacy BugSlay crash handling and replaced it with standard C++ asserts.



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



# Roadmap

1. Continue removing remaining runtime exceptions beyond the now-working tutorial path.

2. Move compilation to Zig.

3. Replace FMOD, Stingray, and Bink with open source alternatives.

4. Start replacing C++ projects one by one with Zig code.
