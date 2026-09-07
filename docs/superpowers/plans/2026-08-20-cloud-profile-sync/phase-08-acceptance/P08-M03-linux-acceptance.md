# P08-M03 — Linux acceptance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Accept on Linux.

**Dependencies:** P08-M02.

**Allowed files:** `docs/superpowers/evidence/cloud-sync/p08-m03-linux.md`.

- [ ] Build and run for `x86_64-linux-gnu` natively; cross-compilation cannot close this gate.
- [ ] Confirm `~/.cache/blitzkrieg/` link creation, sync, and daemon reaping.
- [ ] Check behaviour on a case-sensitive filesystem: profile names are compared case-insensitively by `NProfile::NameEquals` on the assumption of APFS and NTFS, and ext4 does not share it. A profile pair differing only in case is the test.
- [ ] Human playability approval is required.
- [ ] Commit checkpoint: `cloudsync: Linux acceptance evidence`.

**Evidence:** Evidence records the case-sensitivity finding explicitly, whether or not it caused a defect.
