# P01-M02 — generic credentials schema

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace the two-arm union with a backend name and an option map, without losing saved credentials.

**Dependencies:** P01-M01.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/creds_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing migration test first, using a real `cloud.credentials` file written by the current two-arm code.
- [ ] Replace `Protocol` and `Payload` with `{ backend: []const u8, options: StringHashMap([]const u8), rclone_path: ?[]const u8 }`. This deletes code; resist the urge to keep the enum "for convenience", since it is exactly the hardcoding this plan exists to remove.
- [ ] **Migrate in place.** An existing file with `protocol: "s3"` becomes `backend: "s3"` with its fields as options; the same for `webdav`. Both names are rclone backend names already, so the mapping is identity. Migrate on load and rewrite on next save.
- [ ] Preserve the secret contract exactly: values whose catalogue entry sets `IsPassword` or `Sensitive` are never returned by the load path, only a per-field `has_value` flag.
- [ ] **Store only what the player set.** Never write a value equal to the catalogue default — a default that changes upstream must follow upstream.
- [ ] Update `remoteParams` to emit `{"type": backend, ...options}` generically, and keep `remoteName` short so the bisync session-name budget is unaffected.
- [ ] Own the whole export path for any signature change, per the ABI amendment rule.
- [ ] Commit checkpoint: `cloudsync: generic credentials schema`.

**Evidence:** A credentials file written by the previous build loads, migrates, syncs, and round-trips; secrets stay withheld; no stored value duplicates a catalogue default.
