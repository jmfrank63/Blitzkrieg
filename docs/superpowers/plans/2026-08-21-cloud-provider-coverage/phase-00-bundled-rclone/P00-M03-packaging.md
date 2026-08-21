# P00-M03 — packaging, size and signing

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the bundled binary survive packaging on each platform.

**Dependencies:** P00-M02.

**Allowed files:** `build.zig`, `tools/zig/stage.zig`, `Data/THIRD-PARTY-NOTICES.txt`, `docs/superpowers/evidence/cloud-sync/bundled-rclone-size.md`, `tools/zig/verify_runtime.zig`.

- [ ] Record measured fetched and installed size per platform. The macOS arm64 reference is 31.0 MB fetched, 84.3 MB installed.
- [ ] **Signing and notarization are outside the platform plan's scope** — `2026-08-02-linux-macos-platform-port/README.md` excludes "installers, signing/notarization" explicitly — so this packet must not silently adopt them. Define the boundary instead of assuming it.
- [ ] Record what is required for a signed release without implementing the credentialed parts: the nested executable must be signed before the archive is created (signing after zipping signs nothing), then the app notarized and the ticket stapled.
- [ ] Automate only what needs no credentials — the ordering, and a `codesign --verify` check when an identity is available. Everything requiring an Apple identity is a human release gate, recorded as such.
- [ ] State plainly that **`codesign --verify` does not establish notarization**; that needs `xcrun stapler validate` or an `spctl` assessment. A packet claiming a notarized build on the strength of `codesign` alone is claiming something it did not test.
- [ ] Confirm `package-game` includes it and that the packaged layout still verifies, since the runtime verifier enumerates staged files.
- [ ] Confirm the deterministic-package property the existing plan established is unaffected — the same input must still produce the same package hash.
- [ ] **Ship rclone's licence text, not just a note that it exists.** MIT requires the copyright and permission notice to accompany copies; recording "rclone is MIT" in an evidence file does not discharge that.
- [ ] **There is no `COPYING` to stage.** The official v1.75.0 macOS archive contains exactly `rclone`, `rclone.1`, `README.html`, `README.txt` and `git-log.txt` — verified. The MIT text is inside `README.txt`, so the licence must come from a file we own: create `Data/THIRD-PARTY-NOTICES.txt` carrying rclone's copyright line and permission notice verbatim, and stage that. Do not extract it from `README.txt` at build time; a text scrape that silently stops matching is worse than a file under review.
- [ ] Assert the notices file is present in the packaged layout, in the same check that verifies the binary.
- [ ] Note the licence in the evidence file as well, with the path the notice ships at.
- [ ] Commit checkpoint: `cloudsync: package the bundled rclone`.

**Evidence:** Packaging passes per platform with sizes recorded, and on macOS the nested binary passes `codesign --verify`.
