# P00-M04 — C ABI and build wiring

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Export the module to C++ the way StreamIOZig does, and put it in the build graph for every target.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `build.zig`, `tools/zig/build_support.zig`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the C++ smoke consumer first, modelled on the Blitz64 ABI test, linking the library and calling every exported symbol.
- [ ] Define the ABI in `cloudsync.zig` with `callconv(.c)`, and let no Zig error union, slice, or allocator cross it: `bk_cloudsync_available() u32`, `bk_cloudsync_begin(profile: [*:0]const u8) i32`, `bk_cloudsync_poll(handle: i32) u32`, `bk_cloudsync_error(handle: i32) [*:0]const u8`, `bk_cloudsync_shutdown() void`.
- [ ] Handles are small positive indices into a fixed-size table; `-1` is the only failure value. Returned error strings are owned by the module and stay valid until the next call on that handle.
- [ ] Stub every body to a fixed `.unavailable` result. Behaviour arrives in phases 02 and 03 — this packet is the boundary, not the feature.
- [ ] Add `addCloudSync` to `build.zig` mirroring `addStreamIOZig`, including the `.def` selection between the 32-bit and x64 export lists.
- [ ] Wire the module into all six supported triples and confirm each compiles. MSVC targets compile on Windows only; do not claim them from macOS.
- [ ] Commit checkpoint: `cloudsync: export the C ABI and join the build graph`.

**Evidence:** `zig build test-cloudsync-abi -Dtarget=aarch64-macos -Dtest-mode=run` passes, and `-Dtest-mode=compile` succeeds for `x86_64-linux-gnu` and `x86_64-windows-gnu`.
