# P00-M01 runtime platform inventory

The audit reads checked-in fixtures, derives playable source paths from the
source-array manifest in `build.zig`, audits recognized `linkSystemLibrary`
calls, resolves quoted relative includes against each source directory, and
rejects unknown or stale allowlist entries.

## Raw fixture output

Windows and Linux emitted the same fixture records:

```text
fixture output: token=windows.h file=tools/zig/fixtures/runtime_platform/windows_header.cpp line=1
fixture output: token=dinput.h file=tools/zig/fixtures/runtime_platform/direct_input.h line=1
fixture output: token=winsock2.h file=tools/zig/fixtures/runtime_platform/socket.cpp line=1
fixture output: token=HANDLE file=tools/zig/fixtures/runtime_platform/handle.cpp line=1
fixture output: token=SOCKET file=tools/zig/fixtures/runtime_platform/socket_type.cpp line=1
fixture output: token=GetTickCount file=tools/zig/fixtures/runtime_platform/clock.cpp line=1
fixture output: token=HeapAlloc file=tools/zig/fixtures/runtime_platform/heap.cpp line=1
fixture output: token=OutputDebugString file=tools/zig/fixtures/runtime_platform/debug.cpp line=1
fixture output: token=wrong-case relative include file=tools/zig/fixtures/runtime_platform/case_relative.cpp line=1
```

## Windows native run

- Command: `zig build test-runtime-platform-audit -Dtest-mode=run`
- Result: exit code 0; the audit emitted 5 passing tests.
- Inventory: 70 total hits, 69 unique allowlist ownership entries. The
  duplicate `SOCKET` key is a real two-token occurrence on one source line.

## Linux native run

- Environment: WSL2 Ubuntu, x86_64; Zig 0.16.0.
- Command:
  `zig build --cache-dir /tmp/bk-zig-cache-p00m01-final --global-cache-dir /tmp/bk-zig-global-p00m01-final test-runtime-platform-audit -Dtarget=x86_64-linux-gnu -Dtest-mode=run`
- Result: exit code 0; the same 70 sorted hits, 69 unique ownership entries,
  and raw fixture output were emitted.
- The Windows and Linux sorted inventory blocks and fixture blocks are byte
  identical in the captured command output below.

## Sorted inventory output

```text
audio|P05|winmm|build.zig|1922
audio|P05|winmm|build.zig|2015
audio|P05|winmm|build.zig|2358
audio|P05|winmm|build.zig|2612
core|P01|GetTickCount|Sources/src/Common/InterfaceScreenBase.cpp|207
core|P01|GetTickCount|Sources/src/GFX/GraphicsEngine.cpp|1298
core|P01|GetTickCount|Sources/src/GFX/GraphicsEngine.cpp|1360
core|P01|GetTickCount|Sources/src/Game/WinFrame.cpp|327
core|P01|GetTickCount|Sources/src/Game/WinFrame.cpp|343
core|P01|GetTickCount|Sources/src/Main/iMainInternal.cpp|200
core|P01|GetTickCount|Sources/src/Main/iMainInternal.cpp|949
core|P01|GetTickCount|Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp|31
core|P01|GetTickCount|Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp|36
core|P01|GetTickCount|Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp|41
core|P01|GetTickCount|Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp|43
core|P01|GetTickCount|Sources/src/SFX/SoundEngine.cpp|199
core|P01|HANDLE|Sources/src/SFX/AudioBackendOpen.cpp|24
core|P01|HANDLE|Sources/src/libpng/pngrio.c|51
core|P01|HANDLE|Sources/src/libpng/pngrio.c|83
core|P01|HANDLE|Sources/src/libpng/pngrio.c|99
core|P01|HANDLE|Sources/src/libpng/pngtest.c|55
core|P01|HANDLE|Sources/src/libpng/pngwio.c|48
core|P01|HeapAlloc|Sources/src/SFX/AudioBackendOpen.cpp|29
core|P01|HeapAlloc|Sources/src/SFX/AudioBackendOpen.cpp|36
core|P01|OutputDebugString|Sources/src/GFX/VideoCheck.cpp|110
core|P01|OutputDebugString|Sources/src/GFX/VideoCheck.cpp|120
core|P01|OutputDebugString|Sources/src/GFX/VideoCheck.cpp|153
core|P01|OutputDebugString|Sources/src/GFX/VideoCheck.cpp|45
core|P01|OutputDebugString|Sources/src/GFX/VideoCheck.cpp|54
core|P01|OutputDebugString|Sources/src/GFX/VideoCheck.cpp|65
core|P01|OutputDebugString|Sources/src/GFX/VideoCheck.cpp|77
core|P01|OutputDebugString|Sources/src/GFX/VideoCheck.cpp|88
core|P01|OutputDebugString|Sources/src/SFX/AudioBackendOpen.cpp|1191
core|P01|OutputDebugString|Sources/src/SFX/AudioBackendOpen.cpp|264
core|P01|OutputDebugString|Sources/src/SFX/AudioBackendOpen.cpp|272
core|P01|OutputDebugString|Sources/src/SFX/AudioBackendOpen.cpp|280
core|P01|OutputDebugString|Sources/src/SFX/AudioBackendOpen.cpp|843
core|P01|OutputDebugString|Sources/src/SFX/AudioBackendOpen.cpp|963
core|P01|OutputDebugString|Sources/src/SFX/AudioBackendOpen.cpp|997
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|132
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|138
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|139
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|140
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|142
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|144
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|146
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|148
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|150
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|152
core|P01|OutputDebugString|Sources/src/SFX/SoundEngine.cpp|99
core|P01|windows.h|Sources/src/Game/main.cpp|9
core|P01|windows.h|Sources/src/LuaLib/Script.cpp|3
core|P01|windows.h|Sources/src/Platform/System.cpp|10
core|P01|windows.h|Sources/src/libpng/pngtest.c|35
graphics|P08|d3d9|build.zig|2020
graphics|P08|d3d9|build.zig|2654
graphics|P08|dxguid|build.zig|2360
graphics|P08|dxguid|build.zig|2655
input|P03|dinput.h|Sources/src/GFX/VideoCheck.cpp|6
input|P03|dinput.h|Sources/src/Input/InputAPI.cpp|6
input|P03|dinput8|build.zig|2359
net|P04|SOCKET|Sources/src/Platform/SocketWin32.cpp|11
net|P04|SOCKET|Sources/src/Platform/SocketWin32.cpp|11
net|P04|SOCKET|Sources/src/Platform/SocketWin32.cpp|12
net|P04|SOCKET|Sources/src/Platform/SocketWin32.cpp|47
net|P04|winsock2.h|Sources/src/Net/NetLowest.cpp|2
net|P04|winsock2.h|Sources/src/Platform/SocketWin32.cpp|4
net|P04|ws2_32|build.zig|2248
net|P04|ws2_32|build.zig|3206
net|P04|ws2_32|build.zig|3240
```

The test also emitted `platform inventory count: 70` and
`platform allowlist ownership count: 69` on both hosts.
