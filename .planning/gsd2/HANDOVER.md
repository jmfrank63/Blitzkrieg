# Handover: GSD auto run of the remaining migration (2026-10-07)

Written when the work moved to another machine during a system upgrade. Branch `feat/resource-editor` is pushed at
`d44d957a4` (plus this file); the tree is clean. Local `main` on the old machine was 4 commits behind `origin/main`:
pull before merging.

## Goal and order

Babysit the remaining migration through GSD-2 in auto mode until done, in this order:

1. Resource Editor (milestone M001, `feat/resource-editor`) - in progress, see below.
2. Small tools (phase 8).
3. ELK (phase 7).
4. Localization.
5. Editor cloud sync.
6. Installer.

Each milestone ends with `git merge --no-ff` into `main`, then a push. GSD never pushes (`auto_push: false`); the
maintainer (or the babysitting agent) pushes.

## Where M001 stands

- All 18 slices complete. GSD was in `validating-milestone` when stopped. Hand tests are deferred by the user:
  "get it working with CI and then see".
- Full Linux sweep (`tools/zig/run-resource-sweep.sh`, about 27 minutes) passed at `c6b4329c6` (VERDICT=PASS, 41
  tiers). The old MFC editor was deleted in `c6b4329c6` (decision D037, approved).
- MFC goldens: all 20 kinds under `tools/zig/fixtures/resource_editor/*/golden/`, comparator pass=10 accepted=10.
  The road SoilParams difference is the upstream MFC bug `nVal & ESP_DUST != 0x0` (precedence) in
  `3dRoadTreeItem.h`; fixed in the port (`aea2ebc4c`), accepted in the golden. Policy: port bugs are fixed, never
  adopted.
- AchtungPanzer2 mod round trip (`zig build test-resource-mod-roundtrip`, mod root via `BK_MOD_ROOT`, on the old
  machine `~/Downloads/achtung_panzer_2_ostfront/AchtungPanzer2/data`) replaces the INTEX2 rows (B-09.14, B-14.5).
  INTEX2 = Total Challenge II (INtex); it ships only with the Russian edition. The Steam Anthology on win-home has no
  Mods folder. Optional follow-up only if the user switches Steam to Russian.

## CI: next job

Run 37531953359 on `d44d957a4` (`.github/workflows/cross-platform.yml`, runs on push to `main` and `feat/**`):

| Job | Result |
| --- | --- |
| linux-platform | success |
| linux-arm-platform | success |
| windows-mingw-platform | success |
| windows-platform (MSVC) | failure at step "Resource bridge tier" |
| macos-platform (arm64) | failure at step "Resource editor auto" |
| macos-intel-platform | cancelled: exceeded the 45 min job limit |

Next steps:

1. Get the failing step logs (needs `gh auth login`; the public API only gives "exit code 1"). The old machine's `gh`
   token was invalid; job status works without auth via
   `curl -s https://api.github.com/repos/jmfrank63/Blitzkrieg/actions/runs/<id>/jobs`.
2. MSVC "Resource bridge tier": reproduce natively on win-home (`ssh win-home`, user `jmfrank`, Tailscale
   100.103.20.48) in the worktree `D:\bk-ci` with Zig 0.16 at
   `C:\Users\jmfrank\scoop\apps\zig\0.16.0\zig.exe` (the default `zig` there is 0.17). Likely related to the
   effect-position range check or the MSVC min/max fix (`6a0ee67a9`, `d44d957a4`).
3. macOS arm "Resource editor auto": CI runs the aggregate `resource-editor-auto` and also the per-editor steps;
   the aggregate is redundant and long. Probably drop it in CI in favour of the per-editor steps, which also helps
   macos-intel's 45 minute limit (or raise `timeout-minutes` for that job).
4. Hand each failure to GSD as a separate task with reproduction steps, push its fix, repeat until all six jobs are
   green. Then let GSD finish M001 validation and completion, merge `feat/resource-editor` into `main` with `--no-ff`,
   push, and start the next milestone.

## GSD setup

- `gsd-pi` 3.0.0 under node v23.6.1. Headless: `gsd headless auto`, `gsd headless steer "<text>"`,
  `gsd headless query`. Run steers with `nohup` and a long timeout in the background.
- `.gsd/` is gitignored: the database (`.gsd/gsd.db`), runtime and preferences do not travel with git. Copy `.gsd/`
  from the old machine to resume M001 with its history; otherwise recreate the preferences below.
- Models: Opus 5.5 is the main (planning, heavy); Sonnet 5.5 for simpler work. Reviews: GPT-6-Luna medium. Cost is
  the most important factor.
- `.gsd/PREFERENCES.md` as used:

```yaml
version: 1
models:
  research: claude-code/claude-sonnet-5-5
  planning: claude-code/claude-opus-5-5
  completion: claude-code/claude-sonnet-5-5
git:
  isolation: none
  auto_push: false
  push_branches: false
  manage_gitignore: false
verification_commands:
  - zig build install-game install-map-editor
  - zig build test-editor-core
verification_auto_fix: true
verification_max_retries: 2
auto_supervisor:
  soft_timeout_minutes: 40
  idle_timeout_minutes: 20
  hard_timeout_minutes: 90
auto_report: true
dynamic_routing:
  enabled: true
  allow_flat_rate_providers: true
  cross_provider: false
  escalate_on_failure: true
  tier_models:
    light: claude-code/claude-sonnet-5-5
    standard: claude-code/claude-sonnet-5-5
    heavy: claude-code/claude-opus-5-5
reactive_execution:
  enabled: false
  max_parallel: 2
```

- Do not add `execution:` under `models:`: an explicit phase model disables dynamic routing. GSD's `metrics.json`
  records the wrong model; do not trust it for routing checks.
- Local patches to GSD (redo them on a fresh install): `model-router.js` knows the 5.5 tiers (both in the package
  `dist` and in `~/.gsd/agent/extensions/gsd/`); the `reactive-execute.md` prompt uses foreground Agent calls
  (background subagents were killed). Reactive execution is off anyway.
- Supervisor: a loop that reruns `gsd headless --timeout 604800000 auto` until the milestone is complete, a blocker
  appears, or five runs change neither HEAD nor the state line, and writes one status line per run
  (`rc= commits= dirty= milestone= slices= tasks= task= phase= blockers= next=`). `dirty=` above 0 after a task means
  GSD's task commit left files out: commit them (as `affe58ffd`, `aacfdc40a`).

## Known GSD quirks and workarounds

- "Unhandled phase" hang on resume: kill gsd, clear stale rows (`workers`, `unit_dispatches` with status `running`,
  `milestone_leases`) and `.gsd/runtime/units/*.json`, restart.
- The pre-execution check rejects Verify lines with notes; AGENTS.md now forbids them. Fix in the DB and the plan.
- The planner drops steered tasks: re-steer as "separate task NOW with its own task plan"; order tasks with the
  `tasks.sequence` column (all 0 means id order).
- The stuck detector fires right after a restart; just restart again.
- complete-slice can stall; a steer session completing it works.
- A task was once marked complete without doing its work (T07, the MFC deletion): reopen via SQL (status `pending`,
  `one_liner=''`) and move its runtime JSON aside.
- Agents cannot run the full sweep (10 minute foreground cap); the maintainer runs it at the end of each slice in a
  separate worktree (old machine: `/tmp/bk-sweep`).
- In shell checks, use bracketed `pgrep`/`pkill` patterns so they do not match the checking shell.

## win-home (Windows machine)

- MFC needs the user's desktop session; over ssh the editor crashes in session 0. The shipped
  `D:\GOG\Blitzkrieg\reseditor.exe` works.
- Golden scripts: `tools/zig/win-home/export-goldens.ps1` (an `-os` open-and-save pass first; batch export needs
  `<own_data><export_file_name>`, `gamma.cfg` and a doubled trailing backslash on the destination). MFC batch export
  of 3rd/3rv crashes (null tree dock bar); the port must export every kind in batch mode.
- Clean up at the very end: `D:\bk-goldens*`, `D:\bk-ci`; ask the user before deleting files their manual exports
  left in the GOG install (`D:\GOG\Blitzkrieg\1_c.dds`, `data\scenarios\map_*.dds`, `context.xml`).

## Standing decisions

- File names stay case sensitive (MFC lowercased them).
- Never write into shipped `Data/`; never touch the user's profile, saves, settings or cloud sync.
- CRLF for all text files. Conventional commit messages with a scope.
