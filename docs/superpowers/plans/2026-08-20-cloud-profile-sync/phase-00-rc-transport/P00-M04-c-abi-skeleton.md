# P00-M04 — C ABI skeleton and build wiring

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Create the export surface and put the module in the build graph, with availability genuinely working.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `build.zig`, `tools/zig/build_support.zig`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the C++ smoke consumer first, modelled on the Blitz64 ABI test, linking the library and calling every exported symbol.
- [ ] Define the surface with `callconv(.c)` and no Zig error union, slice, or allocator crossing it: `bk_cloudsync_available() u32`, `bk_cloudsync_shutdown() void`, and `bk_cloudsync_last_error() [*:0]const u8`.
- [ ] **Implement `bk_cloudsync_available` for real** against P00-M02 discovery — it returns whether a usable rclone was found. This packet ships one working export rather than a set of placeholders.
- [ ] Do **not** declare `begin`, `poll`, or any other export here. Each arrives with the packet that implements it, under the ABI amendment rule in `EXECUTION.md`; an export declared before its behaviour is how a feature ships unwired.
- [ ] Establish the conventions the later exports follow: handles are small positive indices into a fixed-size table, `-1` is the only failure value, and returned strings are module-owned and valid until the next call on that handle.
- [ ] Add `addCloudSync` to `build.zig` mirroring `addStreamIOZig`, including `.def` selection between the 32-bit and x64 export lists.
- [ ] Wire the module into all six supported triples and confirm each compiles. MSVC targets compile on Windows only; do not claim them from macOS.
- [ ] Commit checkpoint: `cloudsync: C ABI skeleton and build graph`.

**Evidence:** `zig build test-cloudsync-abi -Dtarget=aarch64-macos -Dtest-mode=run` passes with `available` reflecting real discovery, and `-Dtest-mode=compile` succeeds for `x86_64-linux-gnu` and `x86_64-windows-gnu`.
