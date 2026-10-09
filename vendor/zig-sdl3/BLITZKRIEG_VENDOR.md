# Blitzkrieg Vendor Record: zig-sdl3

Source: https://codeberg.org/7Games/zig-sdl3

Version: `v0.2.2`

Source commit: `83c694024f23cbacfa36fcd8fca1c57d4957203e`

Local integration patches:

- FreeType and HarfBuzz dependencies are removed from the vendored manifest because this renderer does not enable SDL_ttf.
- Disabled SDL extensions remain disabled during the package's second module preparation.
- Windows Zig 0.16 `translate-c` receives an `ULL` `SIZE_MAX` definition to avoid MSVC's `ui64` suffix in `limits.h`.
- Zig 0.17 removed `@cImport`: the discarded `@cImport` of `SDL3/SDL_main.h` in `src/main_callbacks.zig` is removed. Its result was never used, so no symbol or type changes.

The SDL dependency and GPU bindings remain upstream package code. Refreshing this vendor requires repeating the provenance and patch audit before changing the package commit.
