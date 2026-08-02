# P00-M01 — Replace Shell Build Steps with Zig Artifacts

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Eliminate PowerShell/CMake/Ninja shader setup and PowerShell determinism comparison from the reachable build graph.

**Dependencies:** none.

**Allowed files:** `build.zig`, `build.zig.zon`, `tools/zig/compile_gfxgpu_shaders.zig`, `tools/zig/verify_shadercross.zig`, `tools/zig/compare_trees.zig`, `tools/zig/build_hermeticity_test.zig`, `tools/shadercross/build_shadercross.ps1` (delete).

- [ ] Add a failing hermeticity test that parses `build.zig` and recursively named Zig build tools and rejects the executable tokens listed in `EXECUTION.md` when used by `addSystemCommand`, `std.process.run`, or equivalent process creation.
- [ ] Import `vendor/zig-sdl3/build.zig` and create the host shadercross CLI with `shadercross.cli`; pass it to shader compilation with `addArtifactArg`/`addRunArtifact`, never a `.exe` path.
- [ ] Replace the inline PowerShell hash comparison with `tools/zig/compare_trees.zig`, which sorts relative file names and compares SHA-256 plus bytes.
- [ ] Keep the pinned SDL_shadercross commit and DXC inputs in `build.zig.zon`; remove runtime clone/download/install logic and delete the obsolete PowerShell builder.
- [ ] Run `zig build verify-shadercross`, `zig build test-gfxgpu-shaders`, `zig build test-gfxgpu-shader-determinism`, and `zig build audit-build-hermeticity`.
- [ ] Verify a repository search returns no reachable build invocation of `pwsh`, `powershell`, `cmake`, or `ninja`.
- [ ] Commit: `build: replace shell shader tools with Zig artifacts`

**Evidence:** hermeticity audit output, shadercross version/help result, and identical tree hash result.
