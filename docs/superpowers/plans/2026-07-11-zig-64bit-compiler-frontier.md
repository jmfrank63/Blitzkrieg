# Windows x64 compiler frontier

Target: `x86_64-windows-msvc` with `ReleaseFast`.

| Build command | Exit | First diagnostic | Source | Classification |
|---|---:|---|---|---|
| `zig build game-all -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 1 | cast to `unsigned long *` from `DWORD` | `Sources/src/Game/WinFrame.cpp:247` | Win32 width |
| `zig build gfx -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build input -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build sfx -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build net -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build image -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build anim -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build ui -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 0 | none | — | — |
| `zig build game -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all` | 1 | cast to `unsigned long *` from `DWORD` | `Sources/src/Game/WinFrame.cpp:247` | Win32 width |

## Executable blockers

`Game` and `game-all` report the same three diagnostics in `WinFrame.cpp`:

1. `TranslateCharsetInfo((DWORD*)dwCharSet, ...)` at line 247 converts a 32-bit integer into a pointer. The correct call must pass a pointer-sized representation of the character set identifier, not an address derived from a `DWORD` cast.
2. `cs.lpszClass = LPCSTR(atomWndClassName)` at line 381 converts a 16-bit window-class atom into a pointer. Use the documented integer-resource conversion (`MAKEINTATOM`) instead.
3. `SplashScreenDialogProc` at line 520 returns `BOOL`; the x64 `DLGPROC` callback type returns pointer-sized `INT_PTR`.

No x86 assembly or DLL ABI diagnostics were emitted by the playable runtime DLL targets. The next plan should repair these three launcher-width issues with regression tests before widening the pointer/handle audit.

## Separate x86 observation

The x86 `ReleaseFast` `Game` link currently fails on unresolved Registry functions from `RandomMapGen` (`RegCreateKeyExA`, `RegCloseKey`, `RegQueryValueExA`, and `RegSetValueExA`). This requires `advapi32`; it is unrelated to the x64 target-library path change and is not modified by the foundation plan.
