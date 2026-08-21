# P00-M03 — packaging, size and signing

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the bundled binary survive packaging on each platform.

**Dependencies:** P00-M02.

**Allowed files:** `build.zig`, `tools/zig/stage.zig`, `Data/THIRD-PARTY-NOTICES.txt`, `docs/superpowers/evidence/cloud-sync/bundled-rclone-size.md`, `tools/zig/verify_runtime.zig`.

- [ ] Record measured fetched and installed size per platform. The macOS arm64 reference is 31.0 MB fetched, 84.3 MB installed.
- [ ] **This packet does not sign anything, and its gate must be closable on an unsigned development build.** Signing and notarization are excluded by `2026-08-02-linux-macos-platform-port/README.md` ("installers, signing/notarization"), and an earlier draft both deferred them to a human gate *and* demanded `codesign --verify` in its evidence — which an unsigned build can never satisfy. That contradiction is removed: the signed-release gate belongs to `P00-M04`.
- [ ] What this packet does own is the **ordering constraint that makes signing possible later**: the nested executable is staged as a normal file so it can be signed before the archive is built. Signing after archiving signs nothing, so a packaging step that zipped first would foreclose the release path.
- [ ] Confirm `package-game` includes it and that the packaged layout still verifies, since the runtime verifier enumerates staged files.
- [ ] Confirm the deterministic-package property the existing plan established is unaffected — the same input must still produce the same package hash.
- [ ] **Ship rclone's licence text, not just a note that it exists.** MIT requires the copyright and permission notice to accompany copies; recording "rclone is MIT" in an evidence file does not discharge that.
- [ ] **There is no `COPYING` to stage.** The official v1.75.0 macOS archive contains exactly `rclone`, `rclone.1`, `README.html`, `README.txt` and `git-log.txt` — verified. The MIT text is inside `README.txt`, so the licence must come from a file we own: create `Data/THIRD-PARTY-NOTICES.txt` carrying rclone's copyright line and permission notice verbatim, and stage that. Do not extract it from `README.txt` at build time; a text scrape that silently stops matching is worse than a file under review.
- [ ] Assert the notices file is present in the packaged layout, in the same check that verifies the binary.
- [ ] Note the licence in the evidence file as well, with the path the notice ships at.
- [ ] Commit checkpoint: `cloudsync: package the bundled rclone`.

**Evidence:** Packaging passes per platform with sizes recorded, the notices file present in the packaged layout, and the staged layout leaving the nested executable signable before archiving. No signature is asserted — see P00-M04.
