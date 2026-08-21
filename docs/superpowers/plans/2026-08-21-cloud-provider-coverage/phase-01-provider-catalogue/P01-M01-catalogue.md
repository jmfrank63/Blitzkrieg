# P01-M01 — catalogue fetch, parse and cache

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Read rclone's provider catalogue and keep it available without the daemon.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/CloudSync/catalogue.zig`, `Sources/src/CloudSync/catalogue_test.zig`, `tools/zig/fixtures/config_providers.json`, `build.zig`.

- [ ] Capture a real `config/providers` reply, trim it to a representative subset — at minimum `s3`, `webdav`, `sftp`, `drive`, `dropbox` — and commit it as a fixture. Record which rclone version produced it.
- [ ] Write the failing parser test against that fixture before writing the parser.
- [ ] Model an option with every field the catalogue provides: `Name`, `Type`, `Help`, `Required`, `Advanced`, `Examples`, `IsPassword`, `Sensitive`, `Hide`, `Default`, `DefaultStr`, `Exclusive`, `FieldName`, `NoPrefix`.
- [ ] **Tolerate unknown fields and unknown types.** A newer rclone will add both, and the whole point of this plan is that a newer rclone needs no game change. An unrecognised type falls back to a text field; it must never fail the parse.
- [ ] Implement `fetch(client)` over `config/providers` on the worker, and `cache(path)` / `loadCached(path)` writing to `<gamedir>/cloudsync/providers.json` through temp-file-then-rename.
- [ ] A missing cache is an empty list, not an error. Nothing in the settings path may block on the catalogue being present.
- [ ] Stamp the cache with the rclone version it came from, and refresh when the running binary reports a different one.
- [ ] Commit checkpoint: `cloudsync: fetch and cache the provider catalogue`.

**Evidence:** Parser tests pass over the fixture, including an injected unknown option type and an unknown top-level field; cache round-trips and a missing cache yields an empty list.
