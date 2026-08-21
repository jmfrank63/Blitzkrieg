# P00-M03 — packaging, size and signing

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the bundled binary survive packaging on each platform.

**Dependencies:** P00-M02.

**Allowed files:** `build.zig`, `tools/zig/stage.zig`, `docs/superpowers/evidence/cloud-sync/bundled-rclone-size.md`, `tools/zig/verify_runtime.zig`.

- [ ] Record measured fetched and installed size per platform. The macOS arm64 reference is 31.0 MB fetched, 84.3 MB installed.
- [ ] On macOS the bundled executable must be signed and notarized with the app; an unsigned nested binary makes the whole bundle fail Gatekeeper. Verify with `codesign --verify --deep` on the packaged result.
- [ ] Confirm `package-game` includes it and that the packaged layout still verifies, since the runtime verifier enumerates staged files.
- [ ] Confirm the deterministic-package property the existing plan established is unaffected — the same input must still produce the same package hash.
- [ ] **Ship rclone's licence text, not just a note that it exists.** MIT requires the copyright and permission notice to accompany copies, so recording "rclone is MIT" in an evidence file does not discharge it. Stage rclone's `COPYING` alongside the binary, or fold it into a third-party notices file in the package, and assert its presence in the packaging check.
- [ ] Note the licence in the evidence file as well, with the path the notice ships at.
- [ ] Commit checkpoint: `cloudsync: package the bundled rclone`.

**Evidence:** Packaging passes per platform with sizes recorded, and on macOS the nested binary passes `codesign --verify`.
