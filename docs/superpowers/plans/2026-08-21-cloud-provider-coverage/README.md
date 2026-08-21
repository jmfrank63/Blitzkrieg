# Cloud Provider Coverage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement exactly one packet at a time. Do not combine packets. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship rclone with the game, and offer every backend rclone can configure as a destination, discovered at runtime so a newer rclone brings new providers with no game change. Wrappers and non-destinations are filtered; writability is confirmed per configuration rather than promised by a list.

**Architecture:** The rclone binary is a hashed `build.zig.zon` dependency staged into the game layout, where `daemon.zig` discovery already looks first. Every provider screen is built from `config/providers` — rclone's own machine-readable catalogue of 69 backends and their options — rendered by one generic form, with the eleven wrapper backends filtered out. No provider name, field name or vendor list is written into our source.

**Tech Stack:** Zig 0.16.0, C++17 game modules, rclone v1.75.0+ as a bundled binary, Windows x64, Linux x64, macOS arm64/x64.

---

## Authoritative documents

1. `docs/superpowers/specs/2026-08-21-cloud-provider-coverage-design.md`
2. `docs/superpowers/specs/2026-08-20-cloud-profile-sync-design.md` — everything except its credentials section still stands
3. This README, then `EXECUTION.md`, then the phase `MANIFEST.md`, then the packet

## Relationship to the cloud-profile-sync plan

That plan is complete through `P07-M03`, with `P08-M02` (Windows acceptance)
done out of order. Its rc transport, daemon supervisor, bisync semantics,
trash and restore protocols are **not** revisited here. This plan replaces one
thing: the two-arm credentials schema in `P03-M01` and the static
`Cloud.Provider` droplist in `P05-M02`.

Read `phase-00-rc-transport/MANIFEST.md` of that plan before writing Zig. It
records four APIs the packet texts assumed which do not exist in Zig 0.16
(`std.crypto.random`, `std.Thread.Mutex`, `std.net.Server`,
`std.posix.symlink`) and two runtime traps: socket-level timeouts panic under
`Io.Threaded`, and a build test step fails if its binary writes anything at
all to stderr.

## Global invariants

- **Nothing about a provider is hardcoded**, with three declared exceptions.
  No backend name, field name, vendor list or default appears in our source or
  data; anything hardcoded is a defect the next rclone release exposes, and a
  packet that adds a provider special case is wrong even if its tests pass.
  The exceptions, each of which must stay in one named place with a comment
  giving its criterion:
  1. the **legacy migration** in P01-M02, which necessarily names `s3` and
     `webdav` to read files written by the two-arm build;
  2. the **candidate filter** in P01-M04, the eleven backends that wrap
     another remote or are not cloud destinations;
  3. the **test fixture**, a snapshot of one rclone version's catalogue, which
     is evidence about that version and never the truth about rclone.
- **Offered is not the same as verified, and neither has a number.** rclone
  has 69 backends; eleven wrap another remote or are not cloud destinations,
  and the rest are *candidates*. Nothing in the catalogue says whether a
  candidate supports the writable, deletable semantics bisync needs — some
  backends are read-only or restrict deletion — so a writable connection test,
  not a count, decides. Never state a figure for "supported providers", and
  never let an acceptance run of three or four imply the rest.
- **A fresh install must be able to acquire a catalogue.** Startup cannot do
  it: `GameMain.cpp` reaches `Available()` only when cloud sync is already
  enabled, so a first-run player would never trigger it. The catalogue is
  fetched on first need — opening the credentials dialog — and refreshed
  opportunistically after a successful sync.
- **Store only what the player set.** Never persist a copy of rclone's
  defaults; a default that changes upstream must follow upstream.
- Secrets are never returned by the load path — a `has_secret` flag only, as
  the existing credentials contract already requires.
- The catalogue is cached to disk. No cache yet is an empty list, never an
  error, and never a reason to block the settings screen.
- Credentials saved under the old two-arm union keep working.
- The bundled binary is found by existing discovery; no discovery change is
  needed to make bundling work.
- Everything the previous plan established about the daemon, deadlines,
  worker thread and ABI amendment rule continues to apply.

## Phase graph

```text
00 bundled rclone
        |
        v
01 provider catalogue  ──►  02 generic credentials form
                                      |
                                      v
                              03 OAuth backends
                                      |
                                      v
                              04 native acceptance
```

## Phase completion contracts

| Phase | Required gate |
|---|---|
| 00 | A fresh install reports cloud sync available with no rclone on `PATH` |
| 01 | The provider list comes from the catalogue, reaches C++, survives a cold start, and old credentials still sync to the same bucket |
| 02 | An arbitrary static-credential backend can be configured and connection-tested without a line of provider-specific code |
| 03 | One OAuth backend authorises and syncs |
| 04 | Three static backends and one OAuth backend pass end to end, and a newer rclone exposes a new provider with no game change |

## Packet index

- `phase-00-bundled-rclone`: P00-M01 through P00-M04
- `phase-01-provider-catalogue`: P01-M01 through P01-M04
- `phase-02-generic-form`: P02-M01 through P02-M04
- `phase-03-oauth`: P03-M01 through P03-M03
- `phase-04-acceptance`: P04-M01 through P04-M02

Seventeen packets, one of which (P00-M04) is a credentialed human release gate that does not block the rest. Each has an explicit allowlist, a failing test, an
implementation boundary, commands, evidence, and a commit.
