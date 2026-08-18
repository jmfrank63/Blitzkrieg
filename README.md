[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



This repository is a personal pet project by Johannes Maria Frank. I use it for fun and learning, especially to practice agentic coding in brownfield application work. The original README is preserved in `readme_original`.

Warning: this project is a work in progress.



# What this repository can do today

- Contains the full Blitzkrieg single-player source code and game data.

- Builds the whole game with `zig build` (Zig + clang) for six targets — Windows (MSVC and MinGW), macOS (Intel and Apple Silicon) and Linux (x86_64 and arm64) — producing a runnable install layout under `zig-out/game/<os>/<arch>/<config>`. Only the MSVC target needs Visual Studio; Zig brings its own toolchain for the rest.

- Builds and runs as a native **64-bit** Game.exe (`zig build install-game -Dtarget=x86_64-windows-msvc`); the 32-bit targets have been retired. Campaign missions and tutorials are playable in x64, including save/load, video, and sound.

- Ships with StreamIO.dll fully replaced by a Zig implementation (`Sources/src/StreamIOZig`) — the engine's entire persistence layer: binary save/load object graphs, XML data trees, the object factory, global variables, console, and options.

- Compiles the legacy C++ with clang and UBSan enabled, which has surfaced and fixed dozens of latent bugs the 2003 MSVC build silently tolerated — including a 22-year-old pure-virtual-call crash in the tank/turret teardown.

- Validates every supported target in CI, running the platform test suites natively on its own runner — Linux x86_64 and arm64, Windows MSVC and MinGW, macOS Intel and Apple Silicon — rather than only cross-compiling them.

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



# Supported platforms

`zig build` accepts six targets. Pass one with `-Dtarget=`; the staged tree is laid out as `zig-out/game/<os>/<arch>/<config>`, so several targets and both optimisation levels sit side by side instead of overwriting each other.

| Platform | Target triple | Status |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Playable |
| macOS (Apple Silicon) | `aarch64-macos` | Playable |
| macOS (Intel) | `x86_64-macos` | Builds; not play-tested |
| Linux (x86_64) | `x86_64-linux-gnu` | Work in progress |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Platform layer only |
| Windows (MinGW) | `x86_64-windows-gnu` | Platform layer only |

"Platform layer only" means the target builds and passes the platform test suites, but the full game build has not been brought up on it yet.

Only `x86_64-windows-msvc` needs a Visual Studio installation. Zig supplies its own toolchain, headers and import libraries for every other target, MinGW included.

Each target is exercised by the `Cross-platform validation` workflow on a runner of its own architecture, so the suites execute rather than merely link.



# Running the game with Zig (primary)

1. Clone the repository with submodules, or run `git submodule update --init --recursive` in an existing checkout.

2. Install Zig (0.16 or later). For `x86_64-windows-msvc` you also need the MSVC toolchain and a Windows SDK (paths are configurable via `-Dmsvc-include`/`-Dwindows-sdk-include` and their `lib` counterparts if yours differ from the defaults in `build.zig`). No other target requires them.

3. Run `zig build install-game -Dtarget=<triple> --release=fast` to play, or drop `--release=fast` for a debug build. Omit `-Dtarget` to build for the host.

4. Start the staged binary — `zig-out/game/windows/x86_64/release/Game.exe` on Windows, `zig-out/game/macos/arm64/release/Game` on Apple Silicon (`.../debug/...` without `--release=fast`). The staged tree is named for the optimisation it holds, so the two never overwrite each other.

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

- Moved off Win32 onto a portable platform layer: SDL3 for windowing, input and audio, an SDL GPU renderer in place of the DirectX path, and a small native adapter per operating system.

- Brought the game up on macOS, where it is playable on Apple Silicon, with Linux following.

- Added Linux arm64 (Raspberry Pi class) and the Windows GNU ABI (MinGW) as targets, which meant separating "is this Windows?" from "is this Visual Studio?" throughout the build. Doing so uncovered that Intel macOS had been compiling with none of its Cocoa adapters, that the Linux job had been red for want of a C runtime, and that the Linux C++ paths were pinned to the x86_64 multiarch triple.

- Put all six targets under CI, each on a runner of its own architecture.



# Roadmap

1. Finish stabilizing the 64-bit build through the remaining tutorials and campaigns.

2. Bring the full game build up on Linux arm64 and MinGW — only their platform layers are validated today — and play-test Intel macOS.

3. Build a utility to convert 32-bit save games to the 64-bit format.

4. Replace FMOD, Stingray, and Bink with open source alternatives.

5. Continue replacing C++ projects one by one with Zig code, using the StreamIO port as the template.
