# P08-M05 Windows regression evidence

Status: partial; the playable build, module/renderer gates, install layout,
archive, and x64 staged verifier pass. The full clean regression matrix remains
open.

## Passed

- `zig build game-all -Dtarget=x86_64-windows-msvc -Dtest-mode=run` completed
  with the platform audit reporting zero inventory hits and zero allowlist
  ownership entries.
- `zig build test-platform-foundation -Dtarget=x86_64-windows-msvc
  -Dtest-mode=run` passed the shared-runtime lifecycle and platform audit
  checks, with zero inventory hits and zero allowlist ownership entries.
- `zig build test-gfxgpu -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passed
  the ABI, shader, SDL window, renderer, texture, depth, native-driver, and
  Zig resource-lifecycle smokes.
- `zig build test-net-module -Dtarget=x86_64-windows-msvc -Dtest-mode=run`
  and `zig build test-sfx-module -Dtarget=x86_64-windows-msvc
  -Dtest-mode=run` passed their module factory/lifecycle checks with the
  target-correct `PlatformRuntime.dll` staged by the build graph.
- `zig build test-input-module -Dtarget=x86_64-windows-msvc
  -Dtest-mode=run` passed the Input factory lifecycle against the absolute
  staged module path.
- `zig build install-game -Dtarget=x86_64-windows-msvc -Dtest-mode=compile`
  completed the runnable Windows staging layout with one target-correct
  `PlatformRuntime.dll` and the expected Game/module/SDL/GFXGPU artifacts.
- `zig build package-game -Dtarget=x86_64-windows-msvc -Dtest-mode=compile`
  completed and emitted `zig-out/packages/windows-x64/Blitzkrieg-game.zip`.
- `zig build test-stage`, `zig build test-platform-linkage`, and
  `zig build test-platform-storage` passed for the Windows target, covering
  target manifest rules, shared-runtime linkage policy, and package/config
  storage behavior.
- `zig build test-game-command-line -Dtarget=x86_64-windows-msvc
  -Dtest-mode=run` passes the documented `-startup-smoke` compatibility mode.
- `zig build game-all -Dtarget=x86_64-windows-msvc -Dtest-mode=run` completed
  successfully after the module-loader fix; the embedded platform audit still
  reports zero inventory hits and zero allowlist ownership entries.
- `tools/zig/verify_x64_runtime.ps1 -InstallDir zig-out/Game/windows-x64`
  passed under x64 CDB, including the `BK_STARTUP: C6 main menu smoke
  checkpoint passed` marker. The native Zig verifier also passed and reported
  `native Zig x64 runtime verification passed`.
- The staged executable was launched directly from
  `zig-out/game/windows-x64` with `Game.exe -startup-smoke -windowed` and
  exited with code 0.
- The same source/build checkpoint fixed the Windows C++17 namespace issue in
  `Paths.h` and kept MSVC `_variant_t` calls on their `const char *` overload.

## Remaining

- The verifier timeout was traced to `Misc::CFileIterator` constructing each
  module path twice (`...\\AILogic.dll\\AILogic.dll`). The loader therefore
  rejected all modules before entering the game loop. The iterator now keeps
  the normalized absolute file path without appending the filename a second
  time; `LoadAllModules` also uses the target-native separator for its glob.
- Clean-cache, resource/exports comparison, CI matrix, and macOS/Linux package
  evidence remain open for the cross-platform closure.
