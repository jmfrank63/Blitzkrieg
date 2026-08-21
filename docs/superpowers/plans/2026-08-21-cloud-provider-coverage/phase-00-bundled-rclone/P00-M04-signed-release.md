# P00-M04 — signed release gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Establish, as an explicit human release gate, that a shipped macOS build with a bundled rclone passes Gatekeeper.

**Dependencies:** P00-M03.

**Allowed files:** `docs/superpowers/evidence/cloud-sync/signed-release.md`.

This packet is a gate, not an implementation. It requires an Apple Developer
identity and cannot be completed by an automated run or on an unsigned
development build. Nothing in the plan below it depends on it.

- [ ] Sign the nested rclone executable **before** the archive is created, then sign the app, then notarize, then staple the ticket. Signing after archiving signs nothing.
- [ ] Verify the signature with `codesign --verify --deep --strict` on the packaged app.
- [ ] **Verify notarization separately.** `codesign --verify` says nothing about notarization; use `xcrun stapler validate` and an `spctl --assess` evaluation. A build reported as notarized on the strength of `codesign` alone was not tested for it.
- [ ] Confirm the third-party notices file from P00-M03 is present in the signed artifact, since signing and packaging can reshape the layout.
- [ ] Record which steps were automated and which required the credentialed operator, so the boundary is documented rather than folklore.
- [ ] Human release approval is required and is never inferred from a passing command.
- [ ] Commit checkpoint: `cloudsync: signed release evidence`.

**Evidence:** Signature verified, notarization verified by stapler and spctl rather than by codesign, notices present in the signed artifact, and named human approval.
