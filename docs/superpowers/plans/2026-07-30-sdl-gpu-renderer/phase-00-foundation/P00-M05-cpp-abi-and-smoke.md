# P00-M05 — Prove the C++ ABI and SDL Bootstrap

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Link a real C++17 caller to Zig and prove SDL can initialize without creating a GPU device yet.

**Dependencies:** P00-M02, P00-M04.

**Allowed files:** `build.zig`, `tools/zig/gfxgpu_abi_test.cpp`, `tools/zig/gfxgpu_smoke.cpp`.

- [ ] Write `gfxgpu_abi_test.cpp` to assert API version/size, reject version 2, create with a null window only when a no-device test flag is set, read an intentionally generated diagnostic, destroy, and verify zero live counts.
- [ ] Add `gfxgpu-abi-test`; run it before linking the Zig library and record the expected failure.
- [ ] Link `GfxGpuZig`, SDL3, and the matching MSVC runtime. Run the test.
- [ ] Write `gfxgpu_smoke.cpp` to initialize SDL video, create a hidden 320×200 resizable window, print the SDL revision, destroy the window, and quit. The program returns non-zero on every SDL failure.
- [ ] Add `gfxgpu-smoke` and make it depend on the staged SDL runtime.
- [ ] Run:

```powershell
zig build gfxgpu-abi-test -Dtarget=x86_64-windows-msvc -Doptimize=Debug
zig build gfxgpu-smoke -Dtarget=x86_64-windows-msvc -Doptimize=Debug
```

- [ ] Add aggregate `test-gfxgpu` depending on current renderer tests.
- [ ] Commit: `test: prove GfxGpu C++ ABI and SDL bootstrap`

**Evidence:** ABI output, SDL revision, and clean process exit.
