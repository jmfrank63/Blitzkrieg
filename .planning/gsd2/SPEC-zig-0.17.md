# Milestone: move the build to Zig 0.17 (2026-10-09)

Branch `chore/zig-0.17` (from `main` `4d7645fc5`). Zig 0.17.0 is installed by scoop on win-home and is the default
`zig` there (`~/scoop/apps/zig/0.17.0`); Zig 0.16.0 stays at `~/bin/zig-x86_64-windows-0.16.0` for comparison.
Release notes: https://ziglang.org/download/0.17.0/release-notes.html (read the Language Changes, Standard Library
and Build System sections before planning; 0.17 is newer than the models' training data, so check every API in
`~/scoop/apps/zig/0.17.0/lib/std` rather than from memory).

## Goal

The game, the Map Editor and the Resource Editor build and pass their tiers with Zig 0.17 on Windows x64 MSVC (this
machine), and CI builds every job with Zig 0.17 and is green. Zig 0.16 support is not kept.

## Done already

- `0e817f44a`: array multiplication (`a ** n`, removed in 0.17) replaced by `@splat` in 51 files.

## What is known to break

`zig build --help` under 0.17 fails while configuring:

- Our `build.zig` (10,770 lines) uses configure-time APIs that 0.17 removed, because the build now runs in a
  separate configurer process and the maker resolves paths: `LazyPath.getPath` (33 uses), `Build.pathFromRoot`
  (40), `Run.addPathDir` (18), `b.sysroot` (31 in `build.zig`, 1 in `tools/zig/build_support.zig`),
  `b.build_root` (1, now `b.root`, a `Cache.Path`), `b.args` (4, now `Run.addPassthruArgs`), the
  `OptimizeMode` tags `.ReleaseFast/.ReleaseSmall/.ReleaseSafe` (28, now `std.lang.Optimize` `.fast/.small/.safe`),
  `b.graph.*` (54, check each), `setEnvironmentVariable` (35, check), `addTranslateC` (2, deprecated, still works).
  Many run steps pass absolute paths under `zig-out/local-test` and `zig-out/bin`; keep what the tests receive
  identical (same directories, same arguments) and prefer the 0.17 LazyPath argument APIs
  (`addFileArg2`/`addDirectoryArg2` with `PathArgOptions`) over strings where the path is an input.
- `vendor/zig-sdl3/build.zig` (tracked, ours to patch): `.ReleaseSmall` at line 219, maybe more.
- Fetched packages (under `zig-pkg/`, gitignored, pinned in `build.zig.zon`) that do not compile on 0.17:
  - `sdl` = castholm/SDL `v0.4.0+3.4.0`: `b.sysroot` at `build.zig:80`. Upstream's newest tag `v0.5.4+3.4.16`
    still targets 0.16.
  - `dxc` = Gota7/dxc-build branch `zig-sdl3` `a60f9fae5`: `@typeInfo(...).@"struct".decls` at `build.zig:207`
    (struct-of-arrays type info).
  - Others may follow once these compile (`spirv_cross`, `sdl_linux_deps`, the SDL satellite libraries of
    `vendor/zig-sdl3`).
- Zig sources (`Sources/editor`, `Sources/src/*Zig*`, `Sources/src/CloudSync`, `tools/zig`) have not been compiled
  yet; expect std changes from the notes: `std.builtin` -> `std.lang`, `@typeInfo` struct-of-arrays
  (`.fields` -> `field_names`/`field_types`, 125 hits to check), `@hasDecl` only sees `pub` declarations (60 uses),
  `@bitCast` on extern structs is a compile error (94 uses to audit; arrays and vectors change meaning silently),
  `DebugAllocator` deprecated, `ArrayList.getLast` deprecated, `std.zon.parse` reworked (`gpu.zig`),
  `@import("builtin").os` deprecated (134, still compiles).

## Slices

1. **S01 Fetched packages compile on 0.17.** Patch each failing package in its `zig-pkg/<hash>` copy until
   `zig build --help` gets past it; keep each package's change minimal and save it as a patch file
   `.planning/gsd2/zig-0.17-patches/<package>.patch` (git diff inside the package, CRLF kept as the package has
   it). Do not push or fork: the maintainer's agent forks each package on GitHub, pushes the patch, opens the
   upstream PR and repins `build.zig.zon` (`zig fetch --save=<name> <fork url>`). Record each package, its upstream
   URL and commit, and the patch file in the slice summary.
2. **S02 Build scripts on the 0.17 build API.** `build.zig`, `tools/zig/build_support.zig`,
   `vendor/zig-sdl3/build.zig`: `zig build --help` and `zig build -l` succeed; the step list is unchanged
   (compare with the 0.16 list: `~/bin/zig-x86_64-windows-0.16.0/zig.exe build -l` at `4d7645fc5`).
3. **S03 Game and editors compile.** `zig build install-game install-map-editor install-resource-editor` (check
   the exact Resource Editor install step name with `zig build -l`) with `--release=fast` and in debug.
4. **S04 Tiers green on Windows.** The Map Editor and Resource Editor tiers listed in `AGENTS.md`, plus
   `test-editor-core`, `test-platform-foundation`, `test-random-missions` with `-Drandom-missions-sweep=` a small
   `only=` subset, and the leak gate (`Random missions leak gate` step in CI). Same results as on 0.16.
5. **S05 CI and docs.** `.github/workflows/cross-platform.yml`: every "Install Zig 0.16.0" step and every cache key
   to 0.17.0 (Windows, macOS arm64, macOS x64, Linux x64, Linux arm64, MinGW); `build.zig.zon`
   `minimum_zig_version = "0.17.0"`; `AGENTS.md`, `README.md` and the comments in `build.zig` that name 0.16.
   The maintainer pushes and watches CI; macOS and Linux results count only from CI or the maintainer's machines.

## Rules

- `AGENTS.md` applies (CRLF, conventional commits with a scope, never write into shipped `Data/`, tests write only
  under `zig-out/local-test`, Verify lines are plain commands joined by `&&`).
- Do not push. Do not edit fetched packages anywhere except their `zig-pkg/` copies in S01.
- Migrate to the 0.17 API; do not keep 0.16 compatibility shims. A deprecated API that still compiles may stay
  unless the slice touches the line anyway.
- No behaviour change: same steps, same arguments to the tests, same staged files. A difference found by a test is
  a bug to fix, not a golden to update.
- `PREFERENCES.md` verification commands call bare `zig`, which is 0.17 on this machine now.
- Reviews: after each slice the maintainer's agent has the slice's commits reviewed by GPT-6-Luna (medium effort)
  outside GSD and steers real findings back as tasks. Models: Opus 5.5 main, Sonnet 5.5 / Haiku 5.5 for completion.
