# P08-M03 — Close the Linux x64 Link

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Compile and link every playable Linux module and Game with no unresolved native/platform symbols.

**Dependencies:** P08-M02.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `Sources/src/Platform/Posix/Clock.cpp`, `Sources/src/Platform/Posix/Sync.cpp`, `Sources/src/Platform/Posix/Debug.cpp`, `Sources/src/Platform/Posix/DynamicLibrary.cpp`, `Sources/src/Platform/Posix/Socket.cpp`, `Sources/src/Platform/Linux/Paths.cpp`, `Sources/src/Platform/Linux/System.cpp`, `tools/zig/linux_link_closure_test.zig`.

- [x] Run `zig build game-all -Dtarget=x86_64-linux-gnu.2.39 -Dtest-mode=run --summary all` in the native ext4 WSL worktree. The post-fix run completed with `112/112` steps succeeded and `11/11` tests passed.
- [x] Convert each unresolved symbol at its owning abstraction boundary; do not add dummy libraries or broad stubs.
- [x] Reject Windows libraries, undefined platform ABI symbols, absolute cache rpaths, and duplicate PlatformRuntime copies. The Linux audit reports zero inventory/allowlist hits, and the ELF link now carries PlatformRuntime explicitly instead of embedding its `.so` in `libMisc.a`.
- [x] Run all platform, Input, Net, SFX, renderer, module, and audit gates after closure; `game-all` reports 11/11 tests passed.
- [ ] Repeat from a clean local cache and record wall time separately from correctness.
- [x] Commit: `build: close native Linux game link` (`d5438e4d2`)

**Evidence:** successful clean `game-all` summary and ELF dependency audit.
