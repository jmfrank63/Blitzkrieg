# P08-M05 Windows regression evidence

Status: partial; the playable build and direct startup smoke pass. The Zig
staged verifier and the full clean regression matrix remain open.

## Passed

- `zig build game-all -Dtarget=x86_64-windows-msvc -Dtest-mode=run` completed
  with the platform audit reporting zero inventory hits and zero allowlist
  ownership entries.
- The staged executable was launched directly from
  `zig-out/game/windows-x64` with `Game.exe -startup-smoke -windowed` and
  exited with code 0.
- The same source/build checkpoint fixed the Windows C++17 namespace issue in
  `Paths.h` and kept MSVC `_variant_t` calls on their `const char *` overload.

## Remaining

- `verify-x64-runtime` is not accepted: its Zig child-process smoke path does
  not reproduce the direct PowerShell launch result in this environment and
  must be fixed or replaced before the packet can close.
- Platform, renderer, resource/exports, clean-build, and CI matrix evidence
  has not yet been rerun for this checkpoint.
