# Linux staged launch smoke

Revision: `62dcc8401` (`docs: record bootstrap fullscreen blocker`)

Environment: WSL2 Ubuntu with WSLg (`DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-0`),
target `x86_64-linux-gnu.2.39`.

The full Linux graph was rebuilt with:

```text
wsl.exe -d Ubuntu -- bash -lc 'cd /home/jmfrank/blitzkrieg-wsl && zig build game-all -Dtarget=x86_64-linux-gnu.2.39 -Dtest-mode=compile'
```

That completed successfully. The staged install was then launched from
`zig-out/game/linux-x64` with only that directory on `LD_LIBRARY_PATH`:

```text
env -u SDL_VIDEODRIVER SDL_AUDIODRIVER=dummy \
  LD_LIBRARY_PATH=zig-out/game/linux-x64 \
  timeout 45s zig-out/game/linux-x64/Game -startup-smoke -windowed
```

Three consecutive launches completed successfully:

```text
SMOKE_1_OK
SMOKE_2_OK
SMOKE_3_OK
```

After the Windows metadata/staging graph changes, the pushed head was fetched
into the same WSL checkout and `zig build game-all -Dtarget=x86_64-linux-gnu.2.39
-Dtest-mode=compile` completed successfully again.

The same WSL checkout also passed the native Linux platform and renderer
gates:

```text
zig build test-platform-foundation -Dtarget=x86_64-linux-gnu.2.39 -Dtest-mode=run
zig build test-gfxgpu -Dtarget=x86_64-linux-gnu.2.39 -Dtest-mode=run
```

The foundation gate passed its lifecycle/client/audit checks. The GFXGPU gate
reported the Vulkan/SPIR-V native matrix, three identical reference frame
hashes, visible textured/pixel-transform/depth probes, and zero live resources.

This closes the staged Linux launch-repeat portion of P09-M01. Renderer
identity, input/focus/minimize interaction, live-handle diagnostics, and the
mission/save/load/endurance UAT remain separate acceptance work.
