# Phase 00 — Bundled rclone

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Ship the binary so the feature works out of the box.

| Packet | Depends on | Owns |
|---|---|---|
| P00-M01 | — | hashed build dependency and staging |
| P00-M02 | M01 | availability out of the box, no PATH |
| P00-M03 | M02 | packaging, size and third-party notices |

Exit: a fresh install on a machine with no rclone reports cloud sync available.

P00-M01 macOS checkpoint: `zig build install-game -Dtarget=aarch64-macos
--release=fast` stages an executable `rclone` reporting v1.75.0, and it runs
with `PATH` emptied. All five CloudSync suites and `test-streamio` still pass.
Commit `319602e62`.

Sizes fetched / installed: macos-arm64 32.5/88.4 MB, macos-x64 34.9/95.5,
linux-x64 31.5/85.3, linux-arm64 28.6/78.3, windows-x64 31.5/85.2.

Two findings for later packets:

- **Zig's zip extractor ignores the archive's external file attributes**, so the
  extracted `rclone` is `-rw-rw-r--` in the package cache, and both
  `Step.installFile` and `stage.zig`'s copy preserve their *source's*
  permissions. The executable bit would therefore never have appeared and
  discovery would have rejected the staged file as `.not_executable`. It is set
  once on the way into `zig-out/bin` by a cached `Step.Run` of
  `install -m 0755`, and staging carries it from there. That step is a new
  host-tool dependency on POSIX; it is skipped on Windows hosts.
- **`lazy` is conditional on the target, not on the step.** Only the archive for
  `-Dtarget` is ever requested, which is what the packet asked for. But the Zig
  0.16 build runner resolves lazy dependencies during *configure*, before it
  selects steps, so on a cold cache even `zig build test-streamio` pulls that
  target's archive. There is no supported way for `build()` to see which step
  was requested. The packet's "a build that never stages the game does not
  fetch 31 MB" is therefore only partly true in practice.

Not a defect in this packet: `install-game -Dtarget=x86_64-linux-gnu` fails
compiling the engine's C++ (`'stdio.h' file not found`) — cross-compiling to
Linux from this macOS host has no libc headers. Reproduced on the unchanged
tree by stashing. The Linux rclone itself fetched and installed correctly
before that failure.

