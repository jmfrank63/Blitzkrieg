# Blitzkrieg Vendor Record: dcimgui (Dear ImGui, docking branch)

Sources:

- `src-docking/`, `LICENSE`: https://github.com/floooh/dcimgui, tag `v1.92.9b`, commit `84984e98ad638a19a12313ca50bfe48ad00f3586`.
  Dear ImGui 1.92.9b (docking branch) with the dear_bindings C API (`cimgui.h`, `ig*` functions).
- `backends/`: https://github.com/ocornut/imgui, tag `v1.92.9b-docking`, commit `b48d1afbe8ee8b238e2961dc363a949dd7304e23`.
  `imgui_impl_sdl3` (platform) and `imgui_impl_sdlgpu3` (SDL GPU renderer, shaders embedded).

Local integration patches: none.

The backends must come from the same Dear ImGui version and branch as
`src-docking/`; a mismatch can compile and then fail at run time. Refreshing
means updating both sources to matching tags and repeating the version check
(`IMGUI_VERSION` in `src-docking/imgui.h`).

Used by the portable Map Editor (`docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`).
