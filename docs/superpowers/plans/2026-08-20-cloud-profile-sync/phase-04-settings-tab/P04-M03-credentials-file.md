# P04-M03 — credentials file

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Store endpoint, bucket, key, and secret where neither the option system nor the backup path can reach them.

**Dependencies:** P04-M02.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/creds_test.zig`.

- [ ] Write the failing test round-tripping a 40-character secret and a 20-character access key — the exact lengths the option system would have destroyed.
- [ ] Define `Credentials = struct { provider, endpoint, bucket, region, access_key, secret, rclone_path }` and implement `load(allocator, path) !?Credentials` and `save(allocator, path, creds) !void` over `profiles/cloud.credentials`.
- [ ] Write with owner-only permissions (`0o600` on POSIX) and via a temp-file-then-rename so a crash mid-write cannot leave a truncated credential file.
- [ ] Implement `remoteParams(self, allocator) !std.json.Value` producing the inline rc remote object — `{"type": "s3", "provider": ..., "access_key_id": ..., ...}` — so no `rclone.conf` entry is ever needed.
- [ ] Confirm the file is excluded from both the sync filter set (P01-M03) and the backup set (phase 05). `config.cfg` leaves the machine; **a credential inside it would be uploaded to the service it unlocks.**
- [ ] Add a `redacted()` helper for logging, and use it — a secret must never reach the daemon log or the error string exposed through the C ABI.
- [ ] Commit checkpoint: `cloudsync: credentials file outside the option system`.

**Evidence:** Unit tests show a 40-character secret surviving a round trip, file permissions asserted, and `redacted()` covering every secret-bearing field.
