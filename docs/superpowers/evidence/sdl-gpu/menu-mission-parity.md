# P08-M03 main-menu and representative-mission parity

Status: awaiting human acceptance.

The preparation harness is `tools/zig/capture_gfx_game.ps1`. It builds the
legacy and SDL GPU renderers into separate temporary staging directories with
the same copied `Data` tree. It does not touch the user's normal game
settings, and it stops stale `Game.exe` processes before each staged launch.

Example preparation command:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/zig/capture_gfx_game.ps1 `
  -OutputRoot C:/temp/bk-p08-m03
```

The generated `manifest.json` records the commit, renderer executable paths,
identical scenario list, capture directory, and log directory. Build logs are
written below the selected temporary output root. Optional `-Launch` starts
each renderer in sequence with `-windowed`; additional arguments can be
provided with `-GameArguments`.

Preparation verification completed on 2026-08-02 at commit
`201e82d9f`: both renderer installs completed successfully under
`C:\Users\jmfrank\AppData\Local\Temp\bk-p08-m03-91375bec1f8e40be86dfe00bb0622de7`.
The temporary root contained separate `legacy` and `sdl_gpu` executables and
63,715 files in each staged tree, including copied `Data`. This is preparation
evidence only; it is not human visual acceptance.

## Human acceptance checklist

Review the legacy and SDL GPU screenshots side by side for each scenario.
Screenshots remain outside the repository and must not be committed.

| Scenario | Geometry | Textures | Fonts | Color/alpha | Depth | Fog/lighting | Shadows | Water | Clipping/flicker | Missing draws | Result |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Main menu | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | n/a | n/a | [ ] | [ ] | pending |
| Loading screen | [ ] | [ ] | [ ] | [ ] | [ ] | n/a | n/a | n/a | [ ] | [ ] | pending |
| Initial mission view | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | pending |
| Terrain close-up | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | pending |
| Units and selection markers | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | pending |
| Particles/explosions | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | pending |
| Shadows and water, if present | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | pending |
| Pause UI | [ ] | [ ] | [ ] | [ ] | [ ] | n/a | n/a | n/a | [ ] | [ ] | pending |
| Return to menu | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | n/a | n/a | [ ] | [ ] | pending |

## Renderer logs

Record the startup line and shutdown line from each staged run here:

```text
legacy startup: pending human run
legacy shutdown: pending human run
sdl_gpu startup: pending human run
sdl_gpu shutdown: pending human run
```

## Review sign-off

```text
accepted: pending
date: pending
hardware/driver: pending
reviewer: pending
classified non-material differences: pending
unexplained material differences: pending
```

Luna must continue to report `awaiting human acceptance` until this section
is completed by the human reviewer.
