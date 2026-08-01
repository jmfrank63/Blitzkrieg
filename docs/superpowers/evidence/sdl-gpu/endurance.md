# P08-M04 endurance and lifecycle validation

Status: in progress.

The runner is `tools/zig/verify_gfxgpu_endurance.ps1`. It records process
working/private memory after each automated resize and minimize/restore cycle,
copies staged game logs, and performs two startup-smoke restarts. With
`-Interactive`, it prompts the reviewer for fullscreen/windowed toggles and
mission load/return cycles.

Example:

```powershell
zig build verify-gfxgpu-endurance `
  -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Drenderer=sdl_gpu `
  -Dinstall-dir=zig-out/Game/x64/Debug
```

The current game process does not expose live GPU resource counts or transfer
high-water marks to PowerShell. Those remain an explicit instrumentation gap;
the packet cannot be marked complete until the run records those counts or the
coordinator approves a bounded replacement metric.

## Run record

```text
Windows: pending
driver: pending (required: direct3d12)
SDL debug validation: pending
resize cycles: pending / 20
minimize/restore cycles: pending / 10
fullscreen/windowed toggles: pending / 10
mission load/return cycles: pending / 5
complete restarts: pending / 2
peak/ending live counts: pending
memory range: pending
validation failures: pending
```

Acceptance: pending.
