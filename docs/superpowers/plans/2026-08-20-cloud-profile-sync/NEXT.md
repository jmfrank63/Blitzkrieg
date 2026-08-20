# Next Packet

Start at `phase-00-rc-transport/P00-M01-rc-json-client.md`.

Nothing in this plan is implemented yet. The design is committed at
`docs/superpowers/specs/2026-08-20-cloud-profile-sync-design.md` and its
behavioural claims were measured against rclone v1.75.0 on macOS arm64:
bisync conflict handling, `_async` plus `job/status`, the `NAME_MAX` session
abort and its symlink workaround, the `all files were changed` guard and its
sentinel workaround, the rc `maxDelete` default of 0, and `backupDir1`
capturing a propagated delete.

Nothing is verified on Windows. Two items are Windows-first risks and should
not be discovered late:

- the short link is a junction (`mklink /J`) rather than a symlink, and
  whether rclone leaves a junction root unresolved in the session name is
  unproven (P01-M01);
- deep install paths are the realistic way a player hits the session-name
  budget, and the measured 212-byte Steam-style path leaves 29 bytes of
  headroom without the link (P01-M02).

## Important working-tree files

The branch is `feature/cloud-profile-sync`. Only documentation exists so far.
The pinned rclone binary used for design verification is not in the
repository and must be fetched per machine.
