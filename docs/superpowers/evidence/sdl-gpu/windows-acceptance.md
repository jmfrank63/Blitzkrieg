# P09-M03 Windows 11 x64 acceptance

Status: accepted for the Windows SDL_GPU renderer gate.

## Build identity

```text
branch: sdl3-gpu-renderer
commit: 6e4a53c6a
OS: Microsoft Windows 11 Pro N, build 26100
CPU: Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz
GPU: NVIDIA GeForce RTX 3060 Ti
driver: 32.0.15.9597
Zig: 0.16.0
SDL3: 3.2.20 (SDL3-3.2.20-release-3.2.20)
shader format: DXIL
shader manifest: GFXS schema 2, 50 DXIL blobs plus manifest
```

The requested repository-local cache/output cleanup was attempted after
verifying the paths were inside the repository, but the execution environment
blocked recursive deletion. The acceptance commands were then run against the
existing local build outputs.

## Automated gate

```text
zig build test-gfxgpu -Dtarget=x86_64-windows-msvc -Doptimize=Debug => PASS
  GfxGpu ABI v1, table 240 bytes, live resources 0
zig build game-all -Dtarget=x86_64-windows-msvc -Doptimize=Debug => PASS
zig build verify-x64-runtime -Dtarget=x86_64-windows-msvc -Doptimize=Debug => PASS
zig build install-game -Dtarget=x86_64-windows-msvc -Doptimize=Debug => PASS
SDL_GPU_DRIVER=direct3d12 Game.exe -startup-smoke -windowed => exit 0
```

The staged install contains `GFXGPU.dll`, `SDL3.dll`,
`Shaders/GfxGpu/gfxgpu-shaders.manifest`, and all referenced DXIL blobs.
The package listing contains the same runtime and shader tree and no
`shadercross`, `dxcompiler.dll`, `dxil.dll`, or source HLSL files.

## Human acceptance

The project-owner review in `menu-mission-parity.md` accepted the supplied
representative menu/mission pair as pixel-identical, with no unexplained
material differences. The SDL GPU renderer is accepted on Windows 11 x64 with
D3D12 forced through SDL_GPU.

```text
accepted: true
reviewer: project owner
classified differences: none; representative-mission pair pixel-identical
```
