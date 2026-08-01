# P08-M03 main-menu and representative-mission parity

Status: accepted by human reviewer.

The preparation harness is `tools/zig/capture_gfx_game.ps1`. It builds the
legacy and SDL GPU renderers into separate temporary staging directories with
the same copied `Data` tree. It does not touch the user's normal game
settings, creates each install's `screenshots` directory, and stops stale
`Game.exe` processes before each staged launch.

Example preparation command:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/zig/capture_gfx_game.ps1 `
  -OutputRoot C:/temp/bk-p08-m03
```

The generated `manifest.json` records the commit, renderer executable paths,
identical scenario list, capture directory, and log directory. Build logs are
written below the selected temporary output root. Use `-LaunchRenderer legacy`
or `-LaunchRenderer sdl_gpu` with `-Launch` to review one renderer at a time;
additional arguments can be provided with `-GameArguments`.

Preparation verification completed on 2026-08-02 at commit
`201e82d9f`: both renderer installs completed successfully under
`C:\Users\jmfrank\AppData\Local\Temp\bk-p08-m03-91375bec1f8e40be86dfe00bb0622de7`.
The temporary root contained separate `legacy` and `sdl_gpu` executables and
63,715 files in each staged tree, including copied `Data`. This is preparation
evidence only; it is not human visual acceptance.

## Exact screenshot comparison

The supplied representative-mission screenshots were compared on 2026-08-02.
The harness names the files `shot0000legacy.tga` and `shot0000sdl_gpu.tga`:

```text
legacy:  C:\Users\jmfrank\AppData\Local\Temp\bk-p08-m03-91375bec1f8e40be86dfe00bb0622de7\legacy\screenshots\shot0000legacy.tga
sdl_gpu: C:\Users\jmfrank\AppData\Local\Temp\bk-p08-m03-91375bec1f8e40be86dfe00bb0622de7\sdl_gpu\screenshots\shot0000sdl_gpu.tga
ImageMagick: magick compare -metric AE => 0 (0)
TGA header: 1804x1353, 32-bit, identical in both files
```

The two screenshots are therefore 100% pixel-identical. This records the
supplied representative-mission pair; the broader scenario checklist below
remains pending until each scenario has been reviewed or compared.

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
accepted: true
date: 2026-08-02
hardware/driver: Windows 11 x64, D3D12 forced through SDL_GPU
reviewer: project owner
classified non-material differences: none; supplied representative-mission pair is pixel-identical
unexplained material differences: none
```

Human acceptance is complete for the supplied representative-mission pair.
