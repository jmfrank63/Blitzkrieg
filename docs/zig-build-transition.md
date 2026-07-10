# Zig Build Transition

This branch starts the move from the Visual Studio solution to `build.zig` using Zig 0.16.

The migration is intentionally incremental:

1. Keep `Sources/src/A7.sln` as the reference build while each project is mirrored in Zig.
2. Start with leaf/static libraries whose inputs are simple and easy to compare.
3. Move upward through dependent libraries, then DLLs, then `Game.exe`.
4. Only remove MSBuild project files once the matching Zig target is verified.

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
```

By default the build targets Win32/MSVC (`x86-windows-msvc`) because that matches the current game build. Other targets can be explored explicitly with Zig's normal `-Dtarget=...` option once the graph is less dependent on Win32 APIs.

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
