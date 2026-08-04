# P08-M03 — Close the Linux x64 Link

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Compile and link every playable Linux module and Game with no unresolved native/platform symbols.

**Dependencies:** P08-M02.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `Sources/src/Platform/Posix/Clock.cpp`, `Sources/src/Platform/Posix/Sync.cpp`, `Sources/src/Platform/Posix/Debug.cpp`, `Sources/src/Platform/Posix/DynamicLibrary.cpp`, `Sources/src/Platform/Posix/Socket.cpp`, `Sources/src/Platform/Linux/Paths.cpp`, `Sources/src/Platform/Linux/System.cpp`, `tools/zig/linux_link_closure_test.zig`.

- [ ] Run `zig build game-all -Dtarget=x86_64-linux-gnu -Dtest-mode=run --verbose` in the native WSL repository.
- [ ] Convert each unresolved symbol at its owning abstraction boundary; do not add dummy libraries or broad stubs.
- [ ] Reject Windows libraries, undefined platform ABI symbols, absolute cache rpaths, and duplicate PlatformRuntime copies.
- [ ] Run all platform, Input, Net, SFX, renderer, module, and audit gates after closure.
- [ ] Repeat from a clean local cache and record wall time separately from correctness.
- [ ] Commit: `build: close playable Linux link`

**Evidence:** successful clean `game-all` summary and ELF dependency audit.
