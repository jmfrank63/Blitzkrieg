# P03-M01 — credentials file and exports

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Store endpoint, bucket, key, and secret where neither the option system nor the backup path can reach them.

**Dependencies:** P02-M05.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/creds_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing test round-tripping a 40-character secret and a 20-character access key — the exact lengths the option system would have destroyed.
- [ ] **Use a tagged schema, not one flat S3-shaped struct.** The two backends share almost no fields, and a single struct either cannot express WebDAV or fills half its fields with nulls:

  ```zig
  const Protocol = enum { s3, webdav };
  const S3 = struct { s3_provider, endpoint, bucket, region, access_key, secret };
  const WebDav = struct { url, vendor, user, pass };
  const Credentials = struct {
      protocol: Protocol,
      payload: union(Protocol) { s3: S3, webdav: WebDav },
      rclone_path: ?[]const u8,
  };
  ```
- [ ] **Keep rclone's `provider` and the game's protocol choice apart.** `Cloud.Provider` selects the protocol (S3 or WebDAV); rclone's S3 `provider` names the vendor behind it (AWS, Cloudflare, Minio, Wasabi). They are different questions with the same word, so the field is `s3_provider` and never `provider`.
- [ ] Implement `load(allocator, path) !?Credentials` and `save(allocator, path, creds) !void` over `profiles/cloud.credentials`, tolerating an unknown `protocol` by returning null rather than erroring — a file from a newer build must not make the game unstartable.
- [ ] Write with owner-only permissions (`0o600` on POSIX) via temp-file-then-rename, so a crash mid-write cannot leave a truncated credential file.
- [ ] Implement `remoteParams(self, allocator) !std.json.Value` producing the inline rc remote object, and `remoteName(self)` returning a short stable name — Path2 must contribute a short `name:root` to the session name, since `bilib.FsPath` charges full length only to the `local` branch.
- [ ] Add the exports the dialog in phase 07 will need, wired now rather than stubbed: `bk_cloudsync_creds_load(json_out, cap) i32`, `bk_cloudsync_creds_save(json: [*:0]const u8) i32`, `bk_cloudsync_creds_clear_secret() i32`, and `bk_cloudsync_creds_present() u32`. Own the whole export path — Zig root, both `.def` files, ABI test.
- [ ] **Never return the secret through `creds_load`.** Return every other field plus a `has_secret` flag; the dialog shows a masked placeholder and only sends a secret when the player types a new one. An export that hands the secret back is one memory scrape from a support log.
- [ ] **Define omission as preservation.** Since `creds_load` withholds the secret, the dialog cannot send it back, so `creds_save` receiving no secret field must merge the stored one rather than write an empty string. Without that rule, editing only the endpoint silently destroys the credential and the next sync fails on authentication. Clearing is a separate deliberate act through `creds_clear_secret`, never a side effect of saving.
- [ ] Add `edit_endpoint_only_preserves_secret` to the test: load, change one non-secret field, save, reload, and assert the secret still authenticates. Add `clear_secret_removes_it` for the explicit path.
- [ ] Confirm the file is excluded from both the sync filter set (P01-M03) and the backup set (phase 04). `config.cfg` leaves the machine, so **a credential inside it would be uploaded to the service it unlocks**.
- [ ] Add `redacted()` and use it on every path that can reach a log, a daemon argument, or the ABI error string.
- [ ] Commit checkpoint: `cloudsync: credentials outside the option system`.

**Evidence:** Unit tests show a 40-character secret surviving a round trip, permissions asserted, `creds_load` withholding the secret, and `redacted()` covering every secret-bearing field.
