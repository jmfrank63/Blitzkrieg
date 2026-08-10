# Zig Build Transition

This branch starts the move from the Visual Studio solution to `build.zig` using Zig 0.16.

The migration is intentionally incremental:

1. Keep `Sources/src/A7.sln` as the reference build while each project is mirrored in Zig.
2. Start with leaf/static libraries whose inputs are simple and easy to compare.
3. Move upward through dependent libraries, then DLLs, then `Game.exe`.
4. Only remove MSBuild project files once the matching Zig target is verified.

Default behavior:

- `zig build` now runs the `game-all` default step, which builds and installs the playable runtime set (`Game.exe` plus required game DLLs) into `zig-out/bin`.
- `zig build install-game` now creates a runnable layout in `zig-out/game` by copying `zig-out/bin` artifacts and creating a `Data` junction to the repository `Data` folder.
- The staged game root now also includes `config.cfg` and `defconf.cfg` copied from `Data/Configs`.
- `zig build run` now builds and stages the runnable layout, then launches `Game.exe` from that install directory.
- Use `-Dinstall-dir=...` to change the staging path and `-Dcopy-data=true` to copy `Data` instead of creating a junction.
- `zig build package` now creates two zip installers in `zig-out/packages`: `Blitzkrieg-game.zip` and `Blitzkrieg-game-with-editors.zip`.
- You can also build each variant separately with `zig build package-game` and `zig build package-game-editors`.
- Use `-Dpackage-dir=...` to change where zip installers are written.
- Use `-Doptimize=Debug -Dbuild-variant=debug` and `-Doptimize=ReleaseFast -Dbuild-variant=release` to keep independently testable Windows outputs in `zig-out/game/windows/x86_64/debug` and `zig-out/game/windows/x86_64/release`. The matching packages are written below `zig-out/packages/windows/x86_64/debug` and `zig-out/packages/windows/x86_64/release`.

For a native Windows debug/release smoke test:

```powershell
zig build install-game -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Dbuild-variant=debug
zig build install-game -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast -Dbuild-variant=release
zig build verify-x64-runtime -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Dbuild-variant=debug
zig build verify-x64-runtime -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast -Dbuild-variant=release
```

Run either staged executable directly with `zig-out\game\windows\x86_64\debug\Game.exe` or `zig-out\game\windows\x86_64\release\Game.exe`.

Current Zig targets:

```powershell
zig build zlib
zig build libpng
zig build misc
zig build image
zig build lualib
zig build net
zig build buildversion
zig build betakeygen
zig build input
zig build formats
zig build anim
zig build common
zig build ui
zig build fontgen
zig build sfx
zig build gfx
zig build randommapgen
zig build main
zig build game
```

The build targets x64/MSVC (`x86_64-windows-msvc`); `x86_64-linux-gnu` and `aarch64-macos` are the other supported targets. 32-bit Windows was dropped once the port moved to 64-bit — play the original 32-bit game from a GOG install instead. Targets are selected with Zig's normal `-Dtarget=...` option.

The first target omits `Sources/src/zlib/minigzip.c` because it is a sample executable with its own `main`, not part of the zlib library surface used by the game.

`libpng` is the first dependency test: it compiles against the local zlib headers and links to the Zig-built `zlib` target. Its source list mirrors `Sources/src/libpng/libpng.vcxproj`, including `pngtest.c`, so this slice changes the build graph rather than library contents.

`Misc` is the first C++ static library. It uses Visual Studio/MSVC headers for Win32/MSVC parity; the default include paths match the local VS 18 Insiders install and can be overridden with:

```powershell
zig build misc -Dmsvc-include="C:\Path\To\VC\Tools\MSVC\<version>\include" -Dwindows-sdk-include="C:\Program Files (x86)\Windows Kits\10\Include\<version>"
```

`Image` is the first Zig-built DLL. It uses `Sources/src/Image/Image.def` for exports and links the Zig-built `Misc`, `libpng`, and `zlib` artifacts. The resource script is not mirrored yet; the current slice proves code compilation, export definition handling, and DLL linking first.

`LuaLib` mirrors the legacy Lua C sources plus the game's `Script.cpp` wrapper as a static library. It currently has no dependencies on the other Zig-built project libraries.

`Net` is mirrored as a DLL using `Sources/src/Net/net.def`. It links the Zig-built `Misc` artifact plus the same Winsock and ODBC system libraries as the Visual Studio project. The OpenSpy placeholder source files are not included yet because they are not part of the current `.vcxproj` source list.

`BuildVersion` is the first Zig-built executable target. It mirrors the legacy console utility and uses an explicit `mainCRTStartup` entry point so the MSVC CRT calls the existing `main` function.

`BetaKeyGen` mirrors the legacy beta key console utility. It links the Zig-built `Misc` and `zlib` artifacts and enables the `_DO_BETA_CHECK` define used by the Visual Studio project.

`Input` is mirrored as a DLL using `Sources/src/Input/Input.def`. It links the Zig-built `Misc` artifact plus the DirectInput, WinMM, COM support, and ODBC system libraries used by the Visual Studio project.

`Formats` is mirrored as a static library. This slice also tightens a few shared headers for Clang compatibility while preserving MSVC builds: dependent iterator types now use `typename`, `CSaverAccessor` accepts `CPtr<IStructureSaver>` directly, and an old unsigned fill cast is now standard C++.

`Anim` is mirrored as a DLL using `Sources/src/Anim/Animation.def`. It links the Zig-built `Misc` and `Formats` artifacts plus the same ODBC and COM support libraries as the Visual Studio project.

`Common` is mirrored as a static library. This target keeps the broad legacy include surface used by the gameplay/shared headers while preserving the Visual Studio source list.

`UI` is mirrored as a DLL using `Sources/src/UI/UI.def`. This slice adds narrowly scoped Clang/MSVC compatibility fixes in shared UI/GFX helper code and links the Zig-built `Misc`, `Common`, and `LuaLib` artifacts.

`FontGen` is mirrored as a console utility. It links the Zig-built `Image`, `Common`, `Formats`, and `Misc` artifacts plus the Win32 GDI/User libraries used by the font atlas generator.

`SFX` is mirrored as a DLL using `Sources/src/SFX/Sound.def`. It builds the open-audio backend and the embedded Xiph Ogg/Vorbis C sources with `SFX_USE_OPEN_AUDIO_BACKEND`.

`GFX` is mirrored as a DLL using `Sources/src/GFX/GFX.def`. It links the Zig-built `Misc` and `Formats` artifacts plus the Direct3D, User/GDI, and COM libraries used by the DirectX 9 renderer.

`RandomMapGen` is mirrored as a static library. This slice includes small compatibility updates in shared accessor/helper headers so legacy smart-pointer copy-initialization patterns build under Zig/Clang while preserving MSVC behavior.

`Main` is mirrored as a static library. This target required narrowly scoped compatibility updates in multiplayer command plumbing, script string conversions, and RPG stats templates to match modern C++ parsing rules without changing runtime flow.

`Game` is now mirrored as a Win32 executable target. It links the Zig-built `Main`, `Misc`, `LuaLib`, `zlib`, `RandomMapGen`, and `Formats` artifacts, uses `WinMainCRTStartup`, and links the same core Win32/user/system libraries as the Visual Studio build path.
