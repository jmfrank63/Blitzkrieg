# P02-M05 — sync exports through the C ABI

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the engine reachable from C++, so nothing built so far is stranded behind a stub.

**Dependencies:** P02-M04.

**Allowed files:** `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`, `build.zig`.

- [ ] Extend the C++ ABI smoke test first to drive a whole sync through the exports against two local directories, and watch it fail.
- [ ] Add the exports, each backed by the engine and none of them stubbed: `bk_cloudsync_begin(profile: [*:0]const u8) i32`, `bk_cloudsync_poll(handle: i32) u32`, `bk_cloudsync_outcome(handle: i32) u32`, `bk_cloudsync_error(handle: i32) [*:0]const u8`, `bk_cloudsync_cancel(handle: i32) void`, and `bk_cloudsync_release(handle: i32) void`.
- [ ] Own the whole export path in this commit — Zig root, both `.def` files, and the ABI test — per the ABI amendment rule in `EXECUTION.md`.
- [ ] Keep the process-wide singleton lifecycle honest: `bk_cloudsync_shutdown` must stop the worker, shut the daemon down, and be safe to call twice or with syncs in flight.
- [ ] Map `Outcome` to stable numeric values in a header comment; C++ switches on them and reordering the enum later would silently change behaviour.
- [ ] Confirm the exported error string stays valid until the next call on its handle, and that `release` invalidates the handle rather than freeing shared state.
- [ ] Commit checkpoint: `cloudsync: expose the sync engine through the C ABI`.

**Evidence:** `zig build test-cloudsync-abi -Dtarget=aarch64-macos -Dtest-mode=run` drives a pair/diverge/converge cycle entirely through the exports, with the conflict file and both trash entries asserted from C++.
