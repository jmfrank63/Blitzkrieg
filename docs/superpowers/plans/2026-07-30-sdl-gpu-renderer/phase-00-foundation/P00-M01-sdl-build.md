# P00-M01 — Vendor SDL3 and Add Renderer Selection

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Add the pinned `zig-sdl3` v0.2.2 package and build options without changing the active renderer.

**Dependencies:** None.

**Allowed files:** `build.zig`, new `build.zig.zon`, new `tools/zig/verify_sdl3.zig`, new `vendor/zig-sdl3/**`, `.gitignore` only if the SDL build creates an unignored deterministic output directory.

**Interface:** `-Drenderer=legacy|sdl_gpu`, default `legacy`; `-Dsdl-debug=true|false`; build step `sdl3`.

- [ ] Add a failing build-option test in `tools/zig/verify_sdl3.zig` that accepts only the two renderer strings and reports the rejected value.
- [ ] Vendor the upstream `zig-sdl3` v0.2.2 source at commit `83c694024f23cbacfa36fcd8fca1c57d4957203e` under `vendor/zig-sdl3` and mark only its unused `freetype` and `harfbuzz` dependencies lazy. Keep its SDL source dependency and GPU bindings intact.
- [ ] Patch the vendored build wrapper so its “full” module preparation does not enable disabled SDL extensions, and define `SIZE_MAX` as an `ULL` value for Windows Zig 0.16 `translate-c` compatibility with MSVC `limits.h`.
- [ ] Point the root `build.zig.zon` dependency at `vendor/zig-sdl3`; use `b.dependency("sdl3", .{ .target = target, .optimize = optimize, .c_sdl_preferred_linkage = .dynamic })` and the package's public `module("sdl3")` surface.
- [ ] Add `sdl3` verification using `tools/zig/verify_sdl3.zig`, importing the package module and requiring linked SDL version 3.4.0. The package's own SDL dependency is pinned to `castholm/SDL` v0.4.0+3.4.0.
- [ ] Make the `sdl3` build step compile and run the verifier. Defer copying the generated SDL3 runtime beside game staging to Phase 09, because the upstream package exposes the linked library through its module rather than as a parent-visible artifact.
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
