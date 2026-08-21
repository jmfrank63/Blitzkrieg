# P01-M01 — catalogue fetch, cache and bootstrap

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Read rclone's provider catalogue, keep it available without the daemon, and make sure a fresh install ever gets one.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/CloudSync/catalogue.zig`, `Sources/src/CloudSync/catalogue_test.zig`, `tools/zig/fixtures/config_providers.json`, `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/worker.zig`, `Sources/src/CloudSync/worker_test.zig`, `build.zig`.

- [ ] Capture a real `config/providers` reply, trim it to at least `s3`, `webdav`, `sftp`, `drive`, `dropbox`, and commit it as a fixture. Record the rclone version that produced it — the fixture is a snapshot, and a packet that treats it as the truth about rclone has misunderstood the plan.
- [ ] Write the failing parser test against the fixture before writing the parser.
- [ ] Model every field the catalogue provides: `Name`, `Type`, `Help`, `Required`, `Advanced`, `Examples`, `IsPassword`, `Sensitive`, `Hide`, `Default`, `DefaultStr`, `Exclusive`, `FieldName`, `NoPrefix`, `ShortOpt`, and **`Provider`**. Take the union across all backends, not the fields the first option of one backend happens to carry — `Provider` is absent from s3's first option and present on 35 options overall, which is exactly how it gets missed.
- [ ] Model `Provider` on **examples** too. It appears on 664 of them: s3's `region` examples are AWS-only, and rendering them for Wasabi would offer regions that do not exist there.
- [ ] **Tolerate unknown fields and unknown types.** A newer rclone will add both. An unrecognised type falls back to a text field and must never fail the parse.
- [ ] Implement `fetch`, and `cache`/`loadCached` over `<gamedir>/cloudsync/providers.json` through temp-file-then-rename. Stamp the cache with the rclone version, and refresh when the running binary reports a different one.
- [ ] **Fetch on first need, not at startup.** A fresh install must be able to acquire a catalogue, but startup is the wrong place to do it: `GameMain.cpp` only reaches `NCloudSync::Available()` inside `CloudOptionIsOn(Cloud.Enabled) && CloudOptionIsOn(Cloud.Sync.OnStartup)`, and a fresh profile has both off. Anything hung off startup availability would therefore never run for exactly the players who need it — the deadlock this bullet exists to break.
- [ ] Implement `ensureCatalogue()` as a **worker job**, not a function call. Fetching means starting a daemon and making an rc call, so a synchronous version would block its caller on socket work — the invariant the worker exists to uphold. Add the job kind and its transitions here; `worker.zig` is in the allowlist for exactly this.
- [ ] Return the cache immediately when it is present and version-matched; only a miss or a version change enqueues a job.
- [ ] The job is cancellable and reports through the same snapshot mechanism as a sync, so a player who opens and closes the dialog does not strand a fetch.
- [ ] Callers are a deliberate player action (opening the credentials dialog, owned by P02-M03) and an opportunistic refresh after a successful sync, where the daemon is already up and the fetch is free.
- [ ] Own the post-sync opportunistic refresh here, since `engine.zig` is in this allowlist. The dialog trigger belongs to P02-M03 and the export to P01-M03; **this packet must not reach into `GameMain.cpp` or the facade.**
- [ ] Starting a daemon on dialog open is acceptable — it is a deliberate action, not startup latency. Starting one at launch for a disabled feature is not.
- [ ] A failed fetch is not an error state: the cache stays absent, the dialog says so and offers retry, and nothing blocks.
- [ ] Commit checkpoint: `cloudsync: fetch, cache and bootstrap the provider catalogue`.

**Evidence:** Parser tests pass over the fixture including an injected unknown type and unknown top-level field; `ensureCatalogue` fetches once when the cache is absent, reuses it when present and version-matched, refetches on a version change, and reports rather than throwing when the daemon is unavailable.
