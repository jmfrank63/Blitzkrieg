[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

This repository is a personal pet project by Johannes Maria Frank. I use it for fun and learning, especially to practice agentic coding in brownfield application work. The original README is preserved in `readme_original`.

Warning: this project is a work in progress.


# Where the project stands (November 2026)

The port itself is largely done: the game runs natively on Windows, macOS and Linux, it speaks ten languages, and all the editors are ported. The project is currently stuck on artwork, on the turned views of buildings and props. See "The wall: artwork" below.


# What has been done

**The game**

- The whole single-player game and its data are here, built with `zig build` (Zig + clang) as a native 64-bit program. Campaigns, tutorials and random missions are playable, including save/load, video and sound.

- The engine moved off Win32 and DirectX onto a portable layer: SDL3 for windows, input and audio, and an SDL GPU renderer.

- The closed libraries are gone: FMOD, Bink and Stingray have been removed. Sound and movies now use Ogg Vorbis and Ogg Theora.

- StreamIO.dll, the engine's persistence layer, is replaced by a Zig implementation (`Sources/src/StreamIOZig`).

- The legacy C++ compiles with clang and UBSan, which has surfaced and fixed dozens of bugs the 2003 build silently tolerated.

- The original random missions are revived, with each campaign chapter unlocking its historical mission as its script intends.

- Player profiles can follow a player between machines through cloud sync.

- LAN multiplayer works. The GameSpy code has been removed, and internet play is not connected yet.

**Ten languages**

- The game, all the editors, the command-line tools and their guides, and this README are available in English, Russian, Chinese (Simplified), Hindi, Spanish, French, German, Portuguese (Brazil), Japanese and Indonesian.

- The nine translations install as language mods, with a new TrueType text engine that draws every one of these scripts.

- The translations are machine-made and still need review by native speakers. Corrections are welcome.

**All the editors**

- The Map Editor, the Resource Editor with all its sub-editors, and ELK, the localisation kit, are rewritten as Zig apps with Dear ImGui (`Sources/editor`). They talk to the C++ engine through a C ABI (`Sources/src/EditorBridge`).

- The old Windows-only MFC editors have been deleted. The game reads what the new editors write, and tests check this with the game's own readers.

- The old command-line tools are replaced by seven portable ones for making mods: `bk-pak`, `bk-table`, `bk-image`, `bk-video`, `bk-font`, `bk-map` and `bk-sprite`. See `tools/guide/README.md`.

**The Remastered option**

- Options > Settings > Gameplay > Remastered is off by default, and off plays the original rules. On adds features the 2003 game did not have, such as turning the camera in four steps, a fastest-route pathfinder, and moving infantry straight from one container to another.

- Mission replays are being built; their groundwork is merged.


# The wall: artwork

Turning the camera means every building and prop must be shown from four sides, but the 2003 art is a set of pictures of each object from one side only. The other sides were never drawn.

So far I have tried two approaches for all 412 objects:

- Building each turned picture from 2003 pixels (given up on 2026-10-29: it created a new problem for each one it solved).

- Rendering simple 3D models painted from those same pictures. This is what the project does today. The side the 2003 art shows looks right, but every part it never saw has to be guessed. Details also fail: a dormer window painted onto a flat roof cannot look right from another side.

Neither is good enough. The correct way is a real 3D model of each object, with details such as dormers as real geometry and a texture on each flat face, so every side follows from the shape.

The question is where those models and textures come from:

- Modelling them by hand or with agents, in the 2003 designs.

- Blitzkrieg 2's models, which Nival also published. A test render shows they are built this way and look clean at all four turns, but many are different buildings from the 2003 ones of the same name, and the license does not clearly cover their art.

- Image-to-3D generators, which I have not tried on this art.

No decision has been made. If you are a 3D artist or know this kind of work, I would like to hear from you.


# Blender asset conversion and validation

The Maya40 replacement pipeline is documented in `docs/blender-replacement.md`.

Quick commands:

- Convert: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Validate only: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Supported platforms

Pass a target with `-Dtarget=`. The staged tree is laid out as `zig-out/game/<os>/<arch>/<config>`, so several targets and both optimisation levels sit side by side.

| Platform | Target triple | Status |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Game and editors run; tested in CI |
| macOS (Apple Silicon) | `aarch64-macos` | Game and editors run; tested in CI |
| Linux (x86_64) | `x86_64-linux-gnu` | Game and editors run; tested in CI on a software renderer |
| macOS (Intel) | `x86_64-macos` | Builds; the CI runner has no GPU for the game, so the game is checked on a real Intel Mac |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Platform tests only; game and editors not tested yet |
| Windows (MinGW) | `x86_64-windows-gnu` | Platform tests only; the engine does not compile here yet |

CI runs each target on a runner of its own architecture, so the suites execute rather than merely link. Some jobs on Linux and macOS are currently failing.

On Linux the window icon is set in the program, which X11 and KDE honour. GNOME on Wayland ignores it and takes the dock and alt-tab icon from a desktop entry named after the program's app id (`org.blitzkrieg.game`, and `org.blitzkrieg.mapeditor`, `org.blitzkrieg.resourceeditor`, `org.blitzkrieg.elk` for the editors). The install ships an opt-in script that writes those entries and the icon into `~/.local/share`; nothing runs it for you. From the install folder run `Tools/install_desktop_entry.sh` (from a checkout: `tools/install_desktop_entry.sh [install-dir]`), and `Tools/install_desktop_entry.sh --remove` undoes it.

Only `x86_64-windows-msvc` needs a Visual Studio installation. Zig supplies its own toolchain, headers and import libraries for every other target, MinGW included.


# Building and running

1. Clone the repository with submodules, or run `git submodule update --init --recursive` in an existing checkout.

2. Install Zig 0.17. For `x86_64-windows-msvc` you also need the MSVC toolchain and a Windows SDK (paths are configurable via `-Dmsvc-include`/`-Dwindows-sdk-include` and their `lib` counterparts if yours differ from the defaults in `build.zig`).

3. Run `zig build install-all -Dtarget=<triple> --release=fast` for the game, the nine language mods and all the editors, or `zig build install-game` for the game alone. Omit `-Dtarget` to build for the host, and drop `--release=fast` for a debug build.

4. Start the staged binary, for example `zig-out/game/windows/x86_64/release/Game.exe` on Windows or `zig-out/game/macos/arm64/release/Game` on Apple Silicon (`.../debug/...` without `--release=fast`). The game runs in a window; `-fullscreen` is available as a command-line option.

5. `zig build package` creates distributable zip packages, and `zig build --help` lists every step and test.


# History so far

- Found the missing libraries and made the 2003 solution compile from a clean state under modern Visual Studio, then played the first tutorial.

- Moved the build to Zig: `build.zig` compiles every project with clang and stages a runnable game layout.

- Replaced StreamIO.dll with Zig, and made the game playable end to end under the Zig build.

- Ported the game to 64-bit, and play-tested it through the tutorials crash by crash with UBSan.

- Moved off Win32 onto SDL3 and an SDL GPU renderer, and brought the game up on macOS and Linux.

- Ported the Map Editor, the Resource Editor and ELK, and deleted the MFC versions. Replaced the old command-line tools.

- Removed FMOD, Bink and Stingray.

- Added the text engine and the ten languages across the game, the editors and the tools.

- Added the Remastered option and the camera turn, which led to the wall described above.


# Roadmap

1. Get past the artwork wall: decide where the 3D models and textures come from, and get permission for anything that needs it.

2. Finish mission replays.

3. Get every CI job passing again.

4. Bring the game and editors up on Linux arm64 and MinGW, and add Windows arm64.

5. Connect internet multiplayer through an OpenSpy replacement for GameSpy.

6. Build a utility to convert 32-bit save games to the 64-bit format.

7. Continue replacing C++ projects with Zig code, using the StreamIO port as the template.
