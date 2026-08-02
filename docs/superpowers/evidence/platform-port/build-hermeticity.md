# Phase 00 build hermeticity evidence

Date: 2026-08-02

## Tool and dependency inputs

- Zig: `0.16.0`
- SDL C dependency: `castholm/SDL` `v0.4.0+3.4.0`, commit `ae5deb068787bd71d9aadbc054ff1af54f5d058c`
- SDL3 Zig dependency: vendored at `vendor/zig-sdl3`
- SDL_shadercross: commit `e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba`
- DirectX Shader Compiler: release `v1.9.2607`, archive `dxc_2026_07_29.zip`
- SPIRV-Cross Zig package: commit `e5dd9ed763941025bdcd795edec2f3439c6dc10c`
- SPIR-V Headers: commit `1bfd27101e4578d0284061bdf8f09fb4755c7c2d`

## Phase exit commands

The following exact command set passed twice in succession:

```text
zig build audit-build-hermeticity
zig build test-build-support
zig build test-platform-headers -Dtarget=x86_64-windows-msvc
zig build test-platform-headers -Dtarget=x86_64-linux-gnu
zig build test-platform-headers -Dtarget=aarch64-macos -Dtest-mode=compile
```

The combined `zig build platform-foundation` and `zig build test-platform-foundation` steps also passed natively and in compile-only Linux/macOS modes.

The build-path scan found no `pwsh`, `powershell`, `bash`, `cmd`, `cmake`, `ninja`, `ln`, or `std.process.run` in the shader, staging, packaging, and foundation paths. Runtime launching remains a separate Windows-oriented `run` step and is not part of the foundation shell-free contract.

Generated `zig-out`, `.zig-cache`, and `zig-pkg` roots are ignored. Targeted checks confirmed those roots are ignored, with zero source/evidence files under cache roots.
