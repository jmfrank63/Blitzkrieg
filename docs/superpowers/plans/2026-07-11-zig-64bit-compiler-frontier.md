# Windows x64 compiler frontier

Target: `x86_64-windows-msvc` with `ReleaseFast`.

| Build command | Exit | First diagnostic | Source | Classification |
|---|---:|---|---|---|
| `zig build game-all -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast` | 0 | none | — | — |
| `zig build gfx -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build input -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build sfx -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build net -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build image -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build anim -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build ui -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build game -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast` | 0 | none | — | — |

## Resolved executable blockers

The initial x64 build exposed three `WinFrame.cpp` diagnostics. The fixes were verified by a successful fresh `game-all` build:

1. `TranslateCharsetInfo((DWORD*)dwCharSet, ...)` at line 247 converted a 32-bit integer into a pointer. It now passes `&dwCharSet`.
2. `cs.lpszClass = LPCSTR(atomWndClassName)` at line 381 converted a 16-bit window-class atom into a pointer. It now uses `MAKEINTATOM`.
3. `SplashScreenDialogProc` at line 520 returned `BOOL`; it now uses the pointer-sized `INT_PTR` required by `DLGPROC`.

The subsequent link failure was unresolved Registry APIs from `RandomMapGen`. `Game` now links `advapi32`, which resolves `RegCreateKeyExA`, `RegCloseKey`, `RegQueryValueExA`, and `RegSetValueExA` for both x86 and x64 builds.

No x86 assembly or DLL ABI diagnostics were emitted by the playable runtime DLL targets. The next plan can begin the pointer/handle audit.

## Separate x86 observation

The x86 `ReleaseFast` `Game` link initially failed on the same unresolved Registry functions from `RandomMapGen`. The `advapi32` link added to `Game` resolves this dependency for both Windows architectures.
