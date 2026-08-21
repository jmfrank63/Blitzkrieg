# P00-M01 — bundled rclone dependency

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Fetch the official rclone archive as a hashed build dependency and stage it into the game layout.

**Dependencies:** None.

**Allowed files:** `build.zig.zon`, `build.zig`, `tools/zig/stage.zig`, `tools/zig/build_support.zig`.

- [x] Add one lazy dependency per target triple to `build.zig.zon`, following the existing `dxc_binary` entry exactly — official URL from `downloads.rclone.org`, pinned version, content hash. The binary never enters the repository.
- [x] **`build.zig.zon` is a static literal — it cannot interpolate a version into several URLs.** Write the version out in each entry and put the bump procedure in a comment beside them, or generate the file from a single source in a build step. Do not attempt a shared constant; there is no mechanism for one.
- [x] **Install the extracted executable into `zig-out/bin` first, then add its name to the runtime list.** The mechanism is not what an earlier draft assumed: `stage_runtime_files` is a list of *names* built in `build.zig` (around line 1500), and `stage.zig` copies those names out of `zig-out/bin` or `zig-out/lib`. Adding a name that was never installed stages nothing.
- [x] Use the platform-correct name (`rclone.exe` on Windows) and confirm the staged copy lands beside `libCloudSync.dylib` in the layout.
- [x] Mark the executable bit on POSIX. An archive member staged without it is found by discovery and then rejected as `.not_executable`, which is a confusing way to fail.
- [x] **Do not touch `daemon.zig`.** Discovery already searches the executable directory before `PATH`; if bundling appears to need a discovery change, the staging path is wrong. That is a stop condition.
- [x] Keep the dependency `lazy` so a build that never stages the game does not fetch 31 MB.
- [x] Record the fetched and installed sizes per platform in the commit message.
- [x] Commit checkpoint: `cloudsync: bundle the rclone binary`.

**Evidence:** `zig build install-game -Dtarget=aarch64-macos --release=fast` produces a layout containing an executable rclone, with its size recorded.
