# P03-M01 — credentials file and exports

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Store endpoint, bucket, key, and secret where neither the option system nor the backup path can reach them.

**Dependencies:** P02-M05.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/creds_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing test round-tripping a 40-character secret and a 20-character access key — the exact lengths the option system would have destroyed.
- [ ] Define `Credentials = struct { provider, endpoint, bucket, region, access_key, secret, rclone_path }` and implement `load(allocator, path) !?Credentials` and `save(allocator, path, creds) !void` over `profiles/cloud.credentials`.
- [ ] Write with owner-only permissions (`0o600` on POSIX) via temp-file-then-rename, so a crash mid-write cannot leave a truncated credential file.
- [ ] Implement `remoteParams(self, allocator) !std.json.Value` producing the inline rc remote object, and `remoteName(self)` returning a short stable name — Path2 must contribute a short `name:root` to the session name, since `bilib.FsPath` charges full length only to the `local` branch.
- [ ] Add the exports the dialog in phase 07 will need, wired now rather than stubbed: `bk_cloudsync_creds_load(json_out, cap) i32`, `bk_cloudsync_creds_save(json: [*:0]const u8) i32`, and `bk_cloudsync_creds_present() u32`. Own the whole export path — Zig root, both `.def` files, ABI test.
- [ ] **Never return the secret through `creds_load`.** Return every other field plus a `has_secret` flag; the dialog shows a masked placeholder and only sends a secret when the player types a new one. An export that hands the secret back is one memory scrape from a support log.
- [ ] Confirm the file is excluded from both the sync filter set (P01-M03) and the backup set (phase 04). `config.cfg` leaves the machine, so **a credential inside it would be uploaded to the service it unlocks**.
- [ ] Add `redacted()` and use it on every path that can reach a log, a daemon argument, or the ABI error string.
- [ ] Commit checkpoint: `cloudsync: credentials outside the option system`.

**Evidence:** Unit tests show a 40-character secret surviving a round trip, permissions asserted, `creds_load` withholding the secret, and `redacted()` covering every secret-bearing field.
