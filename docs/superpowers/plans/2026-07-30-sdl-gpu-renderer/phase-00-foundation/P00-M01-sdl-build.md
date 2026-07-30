# P00-M01 — Vendor SDL3 and Add Renderer Selection

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Add the pinned official SDL3 Windows development package and build options without changing the active renderer.

**Dependencies:** None.

**Allowed files:** `build.zig`, new `build.zig.zon`, new `tools/zig/verify_sdl3.zig`, `.gitignore` only if the SDL build creates an unignored deterministic output directory.

**Interface:** `-Drenderer=legacy|sdl_gpu`, default `legacy`; `-Dsdl-debug=true|false`; build step `sdl3`.

- [ ] Add a failing build-option test in `tools/zig/verify_sdl3.zig` that accepts only the two renderer strings and reports the rejected value.
- [ ] Create `build.zig.zon` dependency `sdl3_windows` with URL `https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-devel-3.4.12-VC.zip` and Zig package hash `N-V-__8AAE4JlAMyAncBlb_JYlu-9ZZHJlu9H-XNIGZspCR-`. Record and verify upstream SHA-256 `8793a153c7eba93b1eb8022fd2356383ec446b2584e43724a72ef68d682813ab`.
- [ ] On `x86_64-windows-msvc`, add the package's SDL3 headers, `lib/x64/SDL3.lib`, and `lib/x64/SDL3.dll` through one `SdlDependency` helper. Other OS targets return a precise “provider belongs to native platform successor” build error only when an SDL-linked artifact is requested; pure core tests remain cross-compilable.
- [ ] Add `sdl3` verification to compile a C probe including `SDL3/SDL.h` and `SDL3/SDL_gpu.h`, link the import library, run it against the packaged DLL, and require `SDL_GetVersion()` to report 3.4.12.
- [ ] Install `SDL3.dll` beside staged Windows executables. Keep all consumers behind `SdlDependency` so a source/system provider can be added without renderer changes.
- [ ] Add `renderer` and `sdl-debug` options. Pass no new macro to legacy targets when `renderer=legacy`.
- [ ] Run:

```powershell
zig build sdl3 -Dtarget=x86_64-windows-msvc -Doptimize=Debug
zig build gfx -Dtarget=x86_64-windows-msvc -Doptimize=Debug -Drenderer=legacy
zig build sdl3 -Drenderer=invalid
```

Expected: first two succeed; third fails with the accepted values in the message.

- [ ] Commit: `build: add pinned SDL3 renderer dependency`

**Evidence:** dependency URL/version/hash, installed DLL path, and three command results.
