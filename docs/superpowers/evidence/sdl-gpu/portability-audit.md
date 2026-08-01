# P09-M04 portability audit

Status: Windows renderer acceptance complete; native-platform work handed off.

## Renderer source classification

Search scope: `Sources/src/GFXGPU` and the active SDL GPU build inputs in
`build.zig`.

| Finding | Classification | Result |
|---|---|---|
| SDL GPU API calls through `sdl.zig`/SDL headers | Required renderer abstraction | Allowed |
| `DXIL`, HLSL filenames, shader-format constants | Windows shader asset format | Allowed; successor must add SPIR-V/MSL |
| `direct3d12` test/driver strings | Test/runtime selection | Allowed; no direct D3D12 API call |
| `Win32Helper.h`, `GFXNativeWindow`, Win32 window properties | Platform window/bootstrap coupling in the C++ adapter | Successor work; not renderer-core backend graphics |
| `D3D9`, Vulkan, Metal, DXGI, Cocoa, X11 API calls | Direct backend graphics API | None found in renderer implementation |
| `Specific.h` imported by new renderer sources | Legacy renderer header leakage | None found |

The remaining D3D9 references are in legacy comparison code, comments, shader
convention notes, or build-time legacy selection; they are not calls from the
SDL GPU renderer core. `gfxgpu_c.h` exposes only the deliberately opaque
borrowed `void *sdl_window` for the native window plus ordinary data buffers;
it contains no SDL type or SDL object definition.

## Compile-only portability probes

```text
zig build GfxGpuZig -Dtarget=x86_64-linux-gnu -Doptimize=Debug
=> BLOCKED after SDL compilation: unable to symlink libSDL3.so.0 -> libSDL3.so.0.4.0 (PermissionDenied)

zig build GfxGpuZig -Dtarget=aarch64-macos -Doptimize=Debug
=> BLOCKED before compilation: --sysroot is required for non-native macOS targets
```

These are environment/toolchain blockers, not renderer-source portability
failures. Windows acceptance remains unchanged and passed in P09-M03.
