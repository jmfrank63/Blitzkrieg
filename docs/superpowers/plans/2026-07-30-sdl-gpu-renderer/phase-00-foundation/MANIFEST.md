# Phase 00 — Build and ABI Foundation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Produce a buildable Zig renderer library, current SDL3 dependency, portable public GFX declarations, stable C ABI, and a real C++ ABI proof.

**Architecture:** SDL is vendored and built as a shared runtime dependency. The legacy and new renderer coexist behind `-Drenderer=legacy|sdl_gpu`; `legacy` remains the default until Phase 09.

**Tech Stack:** Zig 0.16 build graph, SDL3 C API, C++17, Windows x64.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P00-M01 | none | SDL dependency and renderer option | `zig build sdl3` |
| P00-M02 | M01 | portable public GFX types | legacy `zig build gfx` |
| P00-M03 | M01 | Zig library root/build test | `test-gfxgpu-core` |
| P00-M04 | M03 | C ABI header and exports | Zig ABI layout tests |
| P00-M05 | M02, M04 | C++ caller and smoke executable | ABI and smoke startup |

Phase exit:

```powershell
zig build test-gfxgpu -Dtarget=x86_64-windows-msvc -Doptimize=Debug
zig build gfx -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Drenderer=legacy
```

Expected: all tests pass and the legacy GFX DLL still builds unchanged in behavior.
