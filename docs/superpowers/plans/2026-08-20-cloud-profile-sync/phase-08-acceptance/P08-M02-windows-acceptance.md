# P08-M02 — Windows acceptance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Accept on Windows, where the riskiest unknowns live.

**Dependencies:** P08-M01.

**Allowed files:** `docs/superpowers/evidence/cloud-sync/p08-m02-windows.md`.

- [ ] Build on the Windows machine — MSVC C++ cannot be cross-built from macOS, so this cannot be claimed remotely.
- [ ] Re-confirm junction creation from a normal account with Developer Mode off, in the shipped build. The capability is already settled — `New-Item -ItemType Junction` succeeded unelevated on the target machine — so this is a regression check on our own code path, not an open question.
- [ ] Re-confirm in the shipped build that rclone leaves a junction root unresolved in the session name. Already settled by probe — `C__bk_p0..C__bk_remote` from a junction pointing at a deep target, evidence in `docs/superpowers/evidence/cloud-sync/junction-session-name.md` — so this is a regression check. What the probe did **not** cover is data moving through the junction, both sides having been empty; carry real saves across.
- [ ] Test from a deliberately deep install path, the realistic way a player meets the budget: a Steam-style path measured 212 bytes of the 241 available without the link.
- [ ] Confirm the job object reaps the daemon after a forced process kill, and that the identity check declines a recycled pid.
- [ ] Human playability approval is required.
- [ ] Commit checkpoint: `cloudsync: Windows acceptance evidence`.

**Evidence:** Evidence records the junction creation method, the measured session-name length with and without the link, the reaping behaviour, and human approval.
