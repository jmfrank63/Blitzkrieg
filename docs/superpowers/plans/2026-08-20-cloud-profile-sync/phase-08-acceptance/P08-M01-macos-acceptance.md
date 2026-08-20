# P08-M01 — macOS acceptance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Accept the feature on the development platform first.

**Dependencies:** P07-M03.

**Allowed files:** `docs/superpowers/evidence/cloud-sync/p08-m01-macos.md`.

- [ ] Build and install with `zig build install-game -Dtarget=aarch64-macos --release=fast` and run `./Game` from `zig-out/game/macos/arm64/release`.
- [ ] Walk the whole path by hand: enable sync in the Cloud tab, enter credentials, test the connection, pair, play, save, exit, and confirm the remote holds the save.
- [ ] Confirm the daemon dies with the game, and that a killed game leaves an orphan the next launch reaps — and that a *foreign* rclone running at the same time is left alone.
- [ ] Measure the frame-time impact of a sync running behind the main menu and record it. A sync that costs frames is a defect, not a tradeoff.
- [ ] Restore a config backup in merge mode, **exit the game normally, relaunch**, and confirm the restored values are present with local display settings intact; then undo it and relaunch again. The exit path rewrites `config.cfg` from memory, so a restore verified without a restart proves nothing.
- [ ] Human playability approval is required and is never inferred from a passing test.
- [ ] Commit checkpoint: `cloudsync: macOS acceptance evidence`.

**Evidence:** Evidence records commands, frame timings, screenshots, the foreign-rclone check, and explicit human approval.
