# P00-M02 — Isolate Portable Public GFX Types

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove `HWND` and D3D declarations from the public `IGFX` boundary while preserving the legacy renderer build.

**Dependencies:** P00-M01.

**Allowed files:** `Sources/src/GFX/GFXPlatform.h`, `Sources/src/GFX/GFX.H`, `Sources/src/GFX/GFXTypes.h`, `Sources/src/GFX/GraphicsEngine.h`, `Sources/src/GFX/GraphicsEngine.cpp`.

**Required declaration:**

```cpp
struct GFXNativeWindow {
    void* value;
};
```

- [ ] Add a C++ compile test in `GFXPlatform.h` using `static_assert(sizeof(GFXNativeWindow) == sizeof(void*))`.
- [ ] Change only public renderer initialization parameters from `HWND` to `GFXNativeWindow`.
- [ ] Convert `GFXNativeWindow.value` back to `HWND` inside the legacy `.cpp`; keep `windows.h` and D3D headers private to legacy implementation files.
- [ ] Ensure `GFXTypes.h` exposes fixed-width values and engine structs without importing `Specific.h`.
- [ ] Search and classify every changed call site:

```powershell
rg -n "\bHWND\b|Specific\.h|d3d9\.h" Sources/src/GFX/GFX.H Sources/src/GFX/GFXTypes.h Sources/src/GFX/GFXPlatform.h
```

Expected: no public D3D header and no `HWND` in `IGFX`; any remaining result has an explanatory private-implementation role.

- [ ] Run legacy `gfx` and `game` builds.
- [ ] Commit: `refactor: make GFX window boundary portable`

**Evidence:** search output and successful legacy build commands.
