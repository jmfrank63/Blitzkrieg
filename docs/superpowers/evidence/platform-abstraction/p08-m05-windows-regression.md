# P08-M05 Windows regression evidence

Status: partial; the playable build, module/renderer gates, install layout, and
game archive pass. The Zig staged verifier and the full clean regression matrix
remain open.

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
- The staged executable was launched directly from
  `zig-out/game/windows-x64` with `Game.exe -startup-smoke -windowed` and
  exited with code 0.
- The same source/build checkpoint fixed the Windows C++17 namespace issue in
  `Paths.h` and kept MSVC `_variant_t` calls on their `const char *` overload.

## Remaining

- `verify-x64-runtime` is not accepted: its Zig child-process smoke path does
  now resolves the absolute staged `Game.exe` and reaches the Game process,
  but the headless Game loop does not terminate at the startup checkpoint in
  this environment. The verifier retry was stopped after its bounded timeout;
  no debug or Game processes were left running.
- Clean-cache, resource/exports comparison, CI matrix, and macOS/Linux package
  evidence remain open for the cross-platform closure.
