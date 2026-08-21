# Next Packet

Start at `phase-00-bundled-rclone/P00-M01-bundled-dependency.md`.

Nothing in this plan is implemented. The design at
`docs/superpowers/specs/2026-08-21-cloud-provider-coverage-design.md` was
measured against rclone v1.75.0: `config/providers` returns 69 backends with
fully self-describing options, S3 alone carries 53 vendor examples, and
`config/oauthstatus`/`config/oauthstop` exist alongside `config/create`'s
interactive state machine.

## What this plan changes in the shipped code

Two things, both from the cloud-profile-sync plan:

- `Sources/src/CloudSync/creds.zig` — `Protocol = enum { s3, webdav }` and its
  `Payload` union are replaced by a generic `{ backend, options }` map. This is
  a **revision of working, committed code**, so the migration path for already
  saved credentials is part of the packet, not an afterthought.
- `Data/Configs/defconf.cfg` — `Cloud.Provider` stops being a static droplist
  and is filled from the catalogue through `szActionFill`.

Everything else in that plan stands.

## Sizing

31.0 MB fetched per platform, 84.3 MB installed. `strip` saves nothing —
30.4 MB of the binary is `__gopclntab`, which the Go runtime requires. A
trimmed s3+webdav build would be smaller but is the opposite of this plan's
goal, and costs a Go toolchain per platform.
