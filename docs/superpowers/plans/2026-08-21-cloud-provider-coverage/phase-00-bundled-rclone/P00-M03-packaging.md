# P00-M03 — packaging, size and signing

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the bundled binary survive packaging on each platform.

**Dependencies:** P00-M02.

**Allowed files:** `build.zig`, `tools/zig/stage.zig`, `Data/THIRD-PARTY-NOTICES.txt`, `docs/superpowers/evidence/cloud-sync/bundled-rclone-size.md`, `tools/zig/verify_runtime.zig`.

- [ ] Record measured fetched and installed size per platform. The macOS arm64 reference is 31.0 MB fetched, 84.3 MB installed.
- [ ] **Signing is out of scope, and bundling rclone does not change that.** Measured on v1.75.0: rclone's official macOS binary is `adhoc, linker-signed` and `spctl --assess` rejects it — exactly like the game's own `Game` binary. Both are ad-hoc signed, which is what lets an arm64 binary execute at all, and neither is notarized. The game already ships in that state, so the bundled binary adds no new Gatekeeper condition. `2026-08-02-linux-macos-platform-port/README.md` excludes "installers, signing/notarization" and that still holds.
- [ ] Record the constraint for whoever adopts Developer ID signing later, because it is easy to get wrong once and hard to notice: a signed app requires **every nested Mach-O** to be signed too, so the bundled rclone must be signed *before* the archive is built. Signing after archiving signs nothing. Staging it as an ordinary file keeps that path open; nothing in this packet forecloses it.
- [ ] Do not assert a signature in the evidence. There is no identity to sign with, and a gate an unsigned development build cannot close is a gate that stops the plan.
- [ ] Confirm `package-game` includes it and that the packaged layout still verifies, since the runtime verifier enumerates staged files.
- [ ] Confirm the deterministic-package property the existing plan established is unaffected — the same input must still produce the same package hash.
- [ ] **Ship rclone's licence text, not just a note that it exists.** MIT requires the copyright and permission notice to accompany copies; recording "rclone is MIT" in an evidence file does not discharge that.
- [ ] **There is no `COPYING` to stage.** The official v1.75.0 macOS archive contains exactly `rclone`, `rclone.1`, `README.html`, `README.txt` and `git-log.txt` — verified. The MIT text is inside `README.txt`, so the licence must come from a file we own: create `Data/THIRD-PARTY-NOTICES.txt` carrying rclone's copyright line and permission notice verbatim, and stage that. Do not extract it from `README.txt` at build time; a text scrape that silently stops matching is worse than a file under review.
- [ ] Assert the notices file is present in the packaged layout, in the same check that verifies the binary.
- [ ] Note the licence in the evidence file as well, with the path the notice ships at.
- [ ] Commit checkpoint: `cloudsync: package the bundled rclone`.

**Evidence:** Packaging passes per platform with sizes recorded, the notices file present in the packaged layout, and the staged layout leaving the nested executable signable before archiving. No signature is asserted — see P00-M04.
