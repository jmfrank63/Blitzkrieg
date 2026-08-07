# P06-M06 — Close the Portable Game Bootstrap Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Build the real Game plus Input, Net, SFX, SDL_GPU, and minimal runtime modules through first clean frame and shutdown.

**Dependencies:** P06-M05, P04-M05, P05-M05.

**Allowed files:** `tools/zig/game_bootstrap_smoke.cpp`, `tools/zig/game_bootstrap_gate.zig`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Stage a minimal deterministic data fixture and launch the real Game graph in smoke mode.
- [ ] Assert PlatformRuntime, window, Input, Net, SFX, renderer, and Main initialize in documented order.
- [ ] Render one frame, process injected input/quit, and shut down with zero live platform/renderer/audio/network handles.
- [ ] Run natively on Windows/Linux and compile macOS arm64.
- [ ] Verify Game link commands contain no Windows libraries on Linux/macOS.
- [x] Commit: `test: close portable game bootstrap gate`

**Evidence:** the bootstrap smoke now links the real `PlatformRuntime` ABI and records `P`/`p` runtime create/destroy around each `A`/`a` SDL application cycle, with GfxGpu consumer API and renderer `R`/`r` nested inside. Windows compile verification remains the active gate; native run is still coupled to the headless SDL window limitation, and Input/Net/SFX/Main integration remains open.
