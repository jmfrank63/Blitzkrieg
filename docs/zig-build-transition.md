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
