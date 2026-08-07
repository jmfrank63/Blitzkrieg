# Windows staged runtime smoke

Revision: `03c554df4` (canonical package metadata staging).

Target: `x86_64-windows-msvc`, staged install `zig-out/game/windows-x64`.

The Windows graph compiled successfully with:

```text
zig build game-all -Dtarget=x86_64-windows-msvc -Dtest-mode=compile
```

The staged game was launched twice through the native Zig verifier:

```text
zig run tools/zig/verify_gfxgpu_endurance.zig -- zig-out/game/windows-x64
```

Both launches loaded PlatformRuntime, SDL_GPU, Input, SFX, Net, Main, UI,
and the game modules, reached `BK_STARTUP: C6 main menu smoke checkpoint
passed`, and exited successfully. The verifier reported:

```text
P08-M04 native Zig endurance smoke passed: restarts=2
```

The supporting Windows gates also passed:

```text
zig build test-platform-foundation -Dtarget=x86_64-windows-msvc -Dtest-mode=run
zig build test-gfxgpu -Dtarget=x86_64-windows-msvc -Dtest-mode=run
```

The GfxGpu gate reported the native `direct3d12`/`dxil` driver, three
identical frame hashes, textured/depth/pixel-transform coverage, and zero
live resources.

The aggregate Windows test gate also passed with exit status 0:

```text
zig build test -Dtarget=x86_64-windows-msvc -Dtest-mode=run
```

That gate included the staged x64 runtime verifier and the full shader/test
graph.

This closes the automated staged-launch portion of P09-M04. Mission,
save/load, input/audio interaction, resize/focus scenarios, deterministic
screenshot comparison, leak instrumentation, and human regression approval
remain open.
