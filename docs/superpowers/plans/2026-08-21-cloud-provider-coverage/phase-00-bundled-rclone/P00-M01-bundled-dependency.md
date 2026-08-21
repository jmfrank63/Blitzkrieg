# P00-M01 — bundled rclone dependency

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Fetch the official rclone archive as a hashed build dependency and stage it into the game layout.

**Dependencies:** None.

**Allowed files:** `build.zig.zon`, `build.zig`, `tools/zig/stage.zig`, `tools/zig/build_support.zig`.

- [ ] Add one lazy dependency per target triple to `build.zig.zon`, following the existing `dxc_binary` entry exactly — official URL from `downloads.rclone.org`, pinned version, content hash. The binary never enters the repository.
- [ ] Pin the version in one place and reference it from each URL, so a bump is a single edit rather than six.
- [ ] Extract and stage the binary into the game layout beside `libCloudSync.dylib` by adding it to `runtime_files` in `tools/zig/stage.zig`, with the platform-correct name (`rclone.exe` on Windows).
- [ ] Mark the executable bit on POSIX. An archive member staged without it is found by discovery and then rejected as `.not_executable`, which is a confusing way to fail.
- [ ] **Do not touch `daemon.zig`.** Discovery already searches the executable directory before `PATH`; if bundling appears to need a discovery change, the staging path is wrong. That is a stop condition.
- [ ] Keep the dependency `lazy` so a build that never stages the game does not fetch 31 MB.
- [ ] Record the fetched and installed sizes per platform in the commit message.
- [ ] Commit checkpoint: `cloudsync: bundle the rclone binary`.

**Evidence:** `zig build install-game -Dtarget=aarch64-macos --release=fast` produces a layout containing an executable rclone, with its size recorded.
