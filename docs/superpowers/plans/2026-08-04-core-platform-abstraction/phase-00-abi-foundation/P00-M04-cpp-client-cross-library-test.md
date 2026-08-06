# P00-M04 — Add the C++ Client and Real Cross-Library Test

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Give legacy C++ modules checked ABI access and prove they share one runtime instance.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/PlatformABI/PlatformClient.h`, `Sources/src/PlatformABI/PlatformClient.cpp`, `tools/zig/platform_test_consumer_a.cpp`, `tools/zig/platform_test_consumer_b.cpp`, `tools/zig/platform_client_test.cpp`, `build.zig`.

- [x] Build two dynamic test consumers that independently call `PlatformClient` and report the observed runtime generation.
- [x] Verify consumer and client artifacts link the shared runtime.
- [x] Implement checked table acquisition, result helpers, bounded diagnostics, and no-throw C++ wrappers.
- [x] Load both consumers dynamically; assert identical runtime generation `1`.
- [x] Destroy the runtime before unloading consumers; detached client calls fail deterministically.
- [x] Commit: `platform: add checked C++ ABI client`

**Evidence:** real DLL/SO consumer output showing one shared instance.
