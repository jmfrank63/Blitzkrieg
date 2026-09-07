# Phase 08 — Native Acceptance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Prove the feature on real machines, including the two-machine case the whole design exists for.

| Packet | Depends on | Owns |
|---|---|---|
| P08-M01 | P07-M03 | macOS acceptance |
| P08-M02 | M01 | Windows acceptance, junction included |
| P08-M03 | M02 | Linux acceptance |
| P08-M04 | M03 | two-machine convergence |

Exit: two machines converge through a delete and a conflict, with no save lost, on Windows and macOS.

P08-M02 Windows checkpoint (run OUT OF ORDER, before M01 — its content is
Windows-local, this session sat on the Windows machine, and M01 needs the
Mac; recorded as a sequencing deviation, not a dependency violation): every
machine-verifiable item measured and green — junction creation by the
shipped build with Developer Mode off, unelevated; the junction root
unresolved in a 76-byte session name with real saves carried through; a
2.9 GB copy at a 196-char Steam-style deep path pairing with the identical
session name; job-object reaping after a forced kill; a decoy pid with the
wrong start time declined. Evidence in
`evidence/cloud-sync/p08-m02-windows.md`. **Human playability approval is
still pending** — the packet is not closed until a person plays the build
and the evidence file records it. Commit `e6a2ce102`.

Carried forward from P08-M02:

- **The deep-path item found the biggest latent gap of the project:**
  `plan.ensureShortLink` had no production caller, and `bisyncParams`
  measured the budget against the relative `path1` while rclone mangles
  the absolute one. The worker now engages the short link for every
  transfer-shaped job before the budget check (`Worker.Options.link_roots`
  for tests). Wiring it changes the session name, so every EXISTING
  pairing re-pairs once — macOS and Linux acceptance will see one
  `needs_resync`-shaped re-pair on first contact with state from before
  this commit; wipe `cloudsync/state` and let it pair.
- Multi-install coexistence works: slots `p0`/`p1` held the shallow and
  deep installs of the same profile side by side.
- A forced kill leaves the stale `daemon.json` behind BY DESIGN; the next
  launch's reaper deletes it (`stale_pid`) or declines a recycled pid on
  the start-time check.
