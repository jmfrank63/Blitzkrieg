# P01-M01 — catalogue fetch, cache and bootstrap

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Read rclone's provider catalogue, keep it available without the daemon, and make sure a fresh install ever gets one.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/CloudSync/catalogue.zig`, `Sources/src/CloudSync/catalogue_test.zig`, `tools/zig/fixtures/config_providers.json`, `Sources/src/CloudSync/engine.zig`, `build.zig`.

- [ ] Capture a real `config/providers` reply, trim it to at least `s3`, `webdav`, `sftp`, `drive`, `dropbox`, and commit it as a fixture. Record the rclone version that produced it — the fixture is a snapshot, and a packet that treats it as the truth about rclone has misunderstood the plan.
- [ ] Write the failing parser test against the fixture before writing the parser.
- [ ] Model every field the catalogue provides: `Name`, `Type`, `Help`, `Required`, `Advanced`, `Examples`, `IsPassword`, `Sensitive`, `Hide`, `Default`, `DefaultStr`, `Exclusive`, `FieldName`, `NoPrefix`.
- [ ] **Tolerate unknown fields and unknown types.** A newer rclone will add both. An unrecognised type falls back to a text field and must never fail the parse.
- [ ] Implement `fetch`, and `cache`/`loadCached` over `<gamedir>/cloudsync/providers.json` through temp-file-then-rename. Stamp the cache with the rclone version, and refresh when the running binary reports a different one.
- [ ] **Bootstrap the first fetch, or a fresh install never gets a catalogue.** The settings list may not start a daemon, and an empty cache yields nothing — so nothing would ever populate it. Fetch once in the background at startup, immediately after availability detection succeeds and before or alongside the first sync, and write the cache on success.
- [ ] A failed bootstrap is not an error state: it leaves the cache absent, is retried next launch, and never blocks startup or the settings screen.
- [ ] Commit checkpoint: `cloudsync: fetch, cache and bootstrap the provider catalogue`.

**Evidence:** Parser tests pass over the fixture including an injected unknown type and unknown top-level field; a cold start with no cache performs exactly one background fetch and writes it; a failed fetch leaves startup unaffected.
