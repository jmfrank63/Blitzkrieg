# P01-M03 — filters, state paths, and sentinel

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Decide what is in the sync set, keep machine-local state out of it, and guarantee bisync always sees one unchanged file.

**Dependencies:** P01-M02.

**Allowed files:** `Sources/src/CloudSync/plan.zig`, `Sources/src/CloudSync/plan_test.zig`.

- [ ] Write the failing test asserting the filter set excludes exactly the intended paths, that no machine-local state file sits inside Path1, and that the sentinel is created once and never rewritten.
- [ ] **Put machine-local state outside Path1.** Pairing state, the workdir, the pid/daemon record, and the filters file all describe one machine and must not travel to another. Implement `stateRoot(allocator) ![]u8` returning `<gamedir>/cloudsync/`, with pairing state at `<stateRoot>/state/<profile>.json`. Nothing under `profiles/<name>/` may hold machine-local state except the trash and the sentinel.
- [ ] Implement `writeFiltersFile(allocator, path) !void` excluding `config.cfg`, `screenshots/**`, `*.tmp-rename`, `cloud.credentials`, and the local trash directory. Pass it to bisync as `filtersFile`.
- [ ] Add a belt-and-braces exclusion for `.cloudsync-*` so a future machine-local file dropped in the profile directory by mistake is still not synced.
- [ ] Define `SENTINEL_NAME = ".bkprofile"` and implement `ensureSentinel(profile_dir, profile_id, remote_has_sentinel: bool) !void`. Write only when the file is absent **and** the remote does not already carry one. **Never seed it on both sides**: two independently created copies differ in modification time, and bisync then aborts the resync with `Modtime not equal in listing ... .bkprofile` followed by `path1 and path2 are out of sync` (verified). A second machine lets the resync deliver it.
- [ ] State in the file header, not only in the spec, that the sentinel backs two guards: `foundSame`, so the `all files were changed` abort cannot fire on a small profile, and `oldCount`, so a single delete stays at or under the 50% `maxDelete` ratio.
- [ ] Add tests named for the invariants — `single_delete_passes_with_sentinel` and `sentinel_not_seeded_when_remote_has_one` — so both fail loudly if the behaviour is ever relaxed.
- [ ] Commit checkpoint: `cloudsync: filters, machine-local state paths, and the sentinel`.

**Evidence:** Unit tests show the exact filter set, no machine-local state inside Path1, and the sentinel skipped when the remote already has one.
