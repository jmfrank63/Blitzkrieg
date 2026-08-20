# P00-M04 — C ABI skeleton and build wiring

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Create the export surface and put the module in the build graph, with availability genuinely working.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `build.zig`, `tools/zig/build_support.zig`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the C++ smoke consumer first, modelled on the Blitz64 ABI test, linking the library and calling every exported symbol.
- [ ] Define the surface with `callconv(.c)` and no Zig error union, slice, or allocator crossing it: `bk_cloudsync_available() u32`, `bk_cloudsync_discovery_status(json_out: [*]u8, cap: u32) i32`, `bk_cloudsync_shutdown() void`, and `bk_cloudsync_last_error() [*:0]const u8`.
- [ ] `discovery_status` returns `{ found, path, version, reason }`, where `reason` is the typed P00-M02 rejection (`not_found`, `too_old`, `not_executable`) rather than free text. The settings dialog has to tell a player *why* the feature is unavailable and *which* binary was chosen; a boolean plus a generic last-error string cannot answer either, and "cloud sync unavailable" with no reason is the least actionable message the screen could show.
- [ ] **Define the caching contract now, since the explicit-path source arrives later.** Discovery runs once and both `available` and `discovery_status` read the same cached result, so they can never disagree. Add `bk_cloudsync_refresh_discovery() i32` to re-run it and replace the cache.
- [ ] **Give the cache a thread-safety contract in the same packet that creates it.** Refresh is triggered from the UI thread while the sync worker (P02-M02) may be reading the path to spawn a daemon; a refresh that frees the old strings underneath a reader is a use-after-free, and the crash lands nowhere near the cause. Guard the cache with a `std.Thread.Mutex`.
- [ ] **No caller ever holds a pointer into the cache.** `available` returns a copied bool and `discovery_status` serialises into the caller's buffer, both under the lock; anything needing the path — the daemon spawn above all — copies it into its own allocation before releasing. With no borrows outstanding, refresh can free the previous value immediately instead of reference-counting it.
- [ ] Run the probe *outside* the lock and swap the result in under it. `probeVersion` spawns a subprocess, and holding a mutex across that would stall every reader for the duration of a process launch.
- [ ] Add a concurrent test: several threads calling `available` and `discovery_status` in a loop while another calls `refresh_discovery`, run under the debug allocator so a freed-while-read shows up as a failure rather than as a flake.
- [ ] Note in the header that until `P03-M01` exists there is no `rclone_path` to consult, so discovery searches the game directory and `PATH` only; that packet adds the explicit path as the first source **and** owns invalidating this cache. Record the dependency here so the gap is visible from both sides rather than assumed.
- [ ] **Implement `bk_cloudsync_available` for real** against P00-M02 discovery — it returns whether a usable rclone was found. This packet ships one working export rather than a set of placeholders.
- [ ] Do **not** declare `begin`, `poll`, or any other export here. Each arrives with the packet that implements it, under the ABI amendment rule in `EXECUTION.md`; an export declared before its behaviour is how a feature ships unwired.
- [ ] Establish the conventions the later exports follow: handles are small positive indices into a fixed-size table, `-1` is the only failure value, and returned strings are module-owned and valid until the next call on that handle.
- [ ] Add `addCloudSync` to `build.zig` mirroring `addStreamIOZig`, including `.def` selection between the 32-bit and x64 export lists.
- [ ] Wire the module into all six supported triples and confirm each compiles. MSVC targets compile on Windows only; do not claim them from macOS.
- [ ] Commit checkpoint: `cloudsync: C ABI skeleton and build graph`.

**Evidence:** `zig build test-cloudsync-abi -Dtarget=aarch64-macos -Dtest-mode=run` passes with `available` reflecting real discovery, and `-Dtest-mode=compile` succeeds for `x86_64-linux-gnu` and `x86_64-windows-gnu`.
