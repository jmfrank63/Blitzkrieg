# P01-M03 — filters and sentinel

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Decide what is in the sync set, and guarantee bisync always sees one unchanged file.

**Dependencies:** P01-M02.

**Allowed files:** `Sources/src/CloudSync/plan.zig`, `Sources/src/CloudSync/plan_test.zig`.

- [ ] Write the failing test asserting the filter set excludes exactly the intended paths and that the sentinel is created once and never rewritten.
- [ ] Define `SENTINEL_NAME = ".bkprofile"` and implement `ensureSentinel(profile_dir, profile_id) !void` that writes only when the file is absent. **Rewriting it defeats its entire purpose.**
- [ ] Implement `writeFiltersFile(allocator, path) !void` emitting rclone filter lines that exclude `config.cfg`, `screenshots/**`, `*.tmp-rename`, `cloud.credentials`, and the trash directory; pass it to bisync as `filtersFile`.
- [ ] State in the file header, not only in the spec, that the sentinel backs two separate guards: it keeps `foundSame` true so the `all files were changed` abort cannot fire on a small profile, and it counts toward `oldCount` so a single delete stays at or under the 50% `maxDelete` ratio.
- [ ] Add a test named for the invariant — `single_delete_passes_with_sentinel` — so the behaviour fails loudly if the sentinel is ever made conditional.
- [ ] Commit checkpoint: `cloudsync: sync filters and the profile sentinel`.

**Evidence:** Unit tests show the exact filter set and prove a second `ensureSentinel` call does not rewrite the file.
