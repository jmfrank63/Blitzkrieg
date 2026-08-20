# P08-M02 — Windows acceptance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Accept on Windows, where the riskiest unknowns live.

**Dependencies:** P08-M01.

**Allowed files:** `docs/superpowers/evidence/cloud-sync/p08-m02-windows.md`.

- [ ] Build on the Windows machine — MSVC C++ cannot be cross-built from macOS, so this cannot be claimed remotely.
- [ ] **Prove the junction works without administrator rights**, from a normal account with Developer Mode off. This is the single largest unproven assumption in the plan.
- [ ] **Prove rclone leaves a junction root unresolved** in the bisync session name, as it does a POSIX symlink. If it resolves the junction, the session-name budget returns as a live risk and P01-M02's check becomes the only defence — stop and report.
- [ ] Test from a deliberately deep install path, the realistic way a player meets the budget: a Steam-style path measured 212 bytes of the 241 available without the link.
- [ ] Confirm the job object reaps the daemon after a forced process kill, and that the identity check declines a recycled pid.
- [ ] Human playability approval is required.
- [ ] Commit checkpoint: `cloudsync: Windows acceptance evidence`.

**Evidence:** Evidence records the junction creation method, the measured session-name length with and without the link, the reaping behaviour, and human approval.
