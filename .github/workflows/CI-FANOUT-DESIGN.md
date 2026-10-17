# CI fan-out design: build once per platform, run tiers in parallel

Status: ready for planning (2026-10-17). Not implemented. Options 3 (per-area
path filters) and 4 (Windows and macOS only on dispatch, main and pull
requests) are live in `cross-platform.yml`; this note covers options 1 and 2,
which attack the wall time of the runs that still execute everything.

## Problem

- The longest job, `windows-resource-editor`, takes 1h32m to 2.5h. It is one
  runner doing about 27 serial steps: the resource core, bridge and mod
  round-trip tiers, the install, host check, smoke, batch, 19
  `resource-editor-auto-<editor>` steps, game-reads-it and the resource model
  tier.
- Every product job (`*-game`, `*-resource-editor`, `*-map-editor`) builds the
  full engine from its own `.zig-cache`. Per platform the engine is built three
  times per run, and a cold cache costs 20 to 40 minutes each time (see the
  timeout comments in the workflow).
- Billing multiplies macOS minutes by 10 and Windows by 2, so the duplicated
  builds are the expensive part, not the tiers.

## Design

Per platform (Windows MSVC, macOS arm64, Linux x64):

1. `<platform>-build` job: checkout, Zig, cache restore, then build and stage
   everything the tier jobs run: `install-game`, `install-map-editor`,
   `install-resource-editor` and the test executables behind the tier steps.
   Upload the staged install (`zig-out/game/<os>/<arch>/...`) plus the test
   binaries as one artifact per platform. Save the caches here only.
2. Tier jobs, `needs: [<platform>-build, changes]`: checkout (sparse, Data
   only as the tier needs it), download the artifact, run their tiers. For
   Windows the Resource Editor splits into three jobs of about 9 steps each:
   - RE-a: core, bridge, mod round-trip, install, host, smoke, batch,
     auto core, auto wpn
   - RE-b: auto unt, spt, msh, obt, fnc, bld, bdg, pcp, eff
   - RE-c: auto til, 3rd, 3rv, mip, chc, cgc, mdc, gui, game-reads-it,
     resource model
   The Game and Map Editor jobs become tier jobs of the same build.
3. The `changes` gates stay on the tier jobs; the build job runs when any of
   its platform's tier jobs would run (the OR of their conditions).

## Open mechanics

- Does `zig build <tier-step>` in a tier job rebuild? A tier step depends on
  its compile steps; with only the downloaded `zig-out` and no `.zig-cache`,
  Zig rebuilds them. Two ways out, to measure before choosing:
  - restore the build job's `.zig-cache` read-only in each tier job
    (`actions/cache/restore`, same key). Cheap if the cache is a few hundred
    MB; Windows cache restore has measured minutes, not seconds;
  - add `-Dprebuilt=<dir>` style run-only steps to `build.zig` that run the
    staged executables without a compile dependency. More work, no cache
    transfer, and the tiers then test exactly the uploaded bits.
- How much of `.zig-cache` a tier needs: measure the size of `.zig-cache/o`
  entries a single `resource-editor-auto-gui` touches after a warm build
  (`--summary all` lists the cached steps) versus the full cache.
- Artifact size and transfer time: the Windows install upload is measured at
  about 5.5 minutes (run 36271881570). Per tier job a download of the same
  size plus extraction is the overhead to beat; compress the test binaries,
  leave Data out of the artifact (the checkout already has it).
- Cache key races: only the build job saves caches; tier jobs restore only.
- Engine-module loading on Linux needs `rdynamic` and the `$ORIGIN` rpath
  preserved through upload and download; artifacts drop the executable bit,
  so tier jobs `chmod +x` (or tar the tree before upload).
- macOS: one build at 10x versus three; the tiers are short there, so a
  single tier job after the build may beat a three-way split once runner
  start-up and download are counted.

## Expected win

- Windows Resource Editor wall time: today build (about 25 minutes warm,
  40+ cold) plus about 27 serial tier steps. With the build shared and the
  tiers in three parallel jobs: build once (about 25 minutes), then the
  longest third of the tiers plus download (estimate 25 to 35 minutes), so
  roughly 50 to 60 minutes instead of 92 to 150.
- Runner minutes: the engine is built once per platform instead of three
  times. On macOS (10x) that saves two engine builds of 20 to 38 minutes each
  per run, the largest single saving in billed minutes.
- These are estimates from the step timings cited in the workflow comments;
  the first plan task should time one warm and one cold run of the split.

## Current gates (options 3 and 4, for reference)

| Job | Runs on a feature push when | Runs on a PR when |
|---|---|---|
| linux-game | core | core |
| linux-resource-editor, linux-map-editor | core or editors-shared | same |
| linux-arm-platform | code (anything not docs) | same |
| windows-game | never | core, editors-shared or elk |
| windows-resource-editor, -map-editor, -mingw-platform | never | core or editors-shared |
| macos-game | never | core |
| macos-resource-editor, -map-editor, macos-intel-platform | never | core or editors-shared |

Dispatch and pushes to main run every job. `core` is any changed path that is
not `.planning/**`, `docs/**`, a root `*.md`, `Sources/editor/**`,
`Sources/elk/**` or `Data/ELK/**`, so build files, tools, vendor, Data and
this workflow run everything.

Docs-only push (for example `docs/x.md` on `feat/y`): the `changes` job runs
(ubuntu, under a minute) and reports every area false; all twelve product
jobs show as skipped and the run concludes success. A skipped job is not a
failure, and no job `needs` a product job, so nothing cascades. If `changes`
itself fails (for example a force push whose base commit is gone), the
product jobs skip because their `if:` has an implicit `success()`, and the run
is red on `changes`, which is visible rather than silent.

## Out of scope here

- Splitting the Linux and macOS Resource Editor jobs further than the
  Windows one; decide after measuring Windows.
- Self-hosted runners or a remote Zig cache.
