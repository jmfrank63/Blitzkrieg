[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



This repository is a personal pet project by Johannes Maria Frank. I use it for fun and learning, especially to practice agentic coding in brownfield application work. The original README is preserved in `readme_original`.

Warning: this project is a work in progress.



# What this repository can do today

- Contains the full Blitzkrieg single-player source code and game data.

- Builds cleanly from a fresh `A7.sln` solution under modern MSVC tooling.

- Includes native exception reporting and modern debugging support.

- Uses Git submodules for missing libraries.

- Supports both native C++ and WinDbg debugging in VS Code Insiders.

- Has removed legacy BugSlay crash handling and replaced it with standard C++ asserts.



# History so far

- Found the missing libraries and added them as Git submodules.

- Built a Windows XP SP3 VM with Visual Studio 6 to study the original environment.

- VS 6 was unstable and crashed often, so the effort shifted to modern tooling.

- Installed Visual Studio 2010 and converted the old `.dwr` project to `.sln`.

- Loaded the solution into VS 2026 Insiders and updated all dependencies.

- Completed two weeks of agentic coding to make the solution compile from a clean state with zero errors and warnings.

- That did not mean the game was fully running yet, but it did reach tutorial load once and the video plus initial menu were already loading.

- Moved development to VS Code Insiders for better tools.

- Configured two debug paths: native C++ and WinDbg.

- Removed BugSlay because it made debugging harder and in one case crashed after a crash.

- Replaced BugSlay with simple standard C++ asserts.

- With BugSlay removed, the project is now ready for focused runtime debugging toward a full run.



# Roadmap

1. Make the game run without exceptions.

2. Move compilation to Zig.

3. Replace FMOD, Stingray, and Bink with open source alternatives.

4. Start replacing C++ projects one by one with Zig code.
