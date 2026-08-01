# P09-M03 — Run the Windows 11 x64 Acceptance Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Execute the complete automated and human Windows milestone from a clean checkout.

**Dependencies:** P09-M02.

**Allowed files:** `docs/superpowers/evidence/sdl-gpu/windows-acceptance.md`.

- [ ] Record branch, commit, Windows build, CPU, GPU, driver, Zig, SDL3, SDL_shadercross, and shader manifest versions.
- [ ] Remove only repository-local `zig-cache`/`.zig-cache`/`zig-out` after resolving and confirming each path is inside the repository; then run:

```powershell
zig build test-gfxgpu -Dtarget=x86_64-windows-msvc -Doptimize=Debug
zig build game-all -Dtarget=x86_64-windows-msvc -Doptimize=Debug
zig build verify-x64-runtime -Dtarget=x86_64-windows-msvc -Doptimize=Debug
zig build install-game -Dtarget=x86_64-windows-msvc -Doptimize=Debug
```

- [ ] Force `SDL_GPU_DRIVER=direct3d12`; launch the staged game and record startup driver line.
- [ ] Reach menu, start representative mission, exercise UI/world/particles/shadows/water where present, resize/minimize/restore, return to menu, and exit.
- [ ] Require no crash, device loss, validation error, missing draw, material parity defect, or non-zero shutdown live count.
- [ ] Human reviewer records final accepted/rejected state. Commit evidence only after accepted.
- [ ] Commit: `test: accept SDL GPU renderer on Windows 11`

**Evidence:** full command transcripts, human checklist, startup/shutdown lines.
