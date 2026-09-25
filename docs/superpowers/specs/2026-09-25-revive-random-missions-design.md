# Revive the original random missions

Every random (template) mission of the original Blitzkrieg 1.2 campaigns is
playable again, each chapter unlocks its historical mission the way the
original chapter scripts intend, and the chapter screen's fallback that
listed every historical mission is removed.

## Why

From the second chapter of each campaign on, the chapter script enables the
historical mission only after the player wins a random mission there
(`Data/Scenarios/Chapters/*/*/script.lua`, `MissionFinished`: `EnableMission`
when `Mission.Current.Random == 1`). Until then the original chapter map
offers only random missions. The port breaks this in two places:

1. **Data.** Commit `c1532955b` ("restrict campaign random templates to
   secure areas") cut the defend, escort and hunt templates from the three
   campaign files, 63 → 43 entries each for USSR and Allies, 76 → 56 for
   German. In the shipped data the `summer_ukraine` secure-area templates
   carry the `summer_russia` setting, so chapters set in `summer_ukraine`
   (USSR Kursk, USSR Rumania, German Kharkov42) were left with no template at
   all, and Moscow, Stalingrad, Ukraine and both Ardennes with 4–6 instead
   of 8–10.
2. **Code.** Commit `230a5ab6c` added a fallback to
   `CInterfaceChapter::InitWindow` (`Sources/src/GameTT/Chapter.cpp`): when
   no historical mission is enabled, list every non-template mission of the
   chapter, finished ones included, and preselect one. That is the normal
   state of every chapter before its first random win, so the player is
   carried from historical mission to historical mission. The fallback kept
   the three template-less chapters from being dead ends; it also bypasses
   the design everywhere else.

## The reference: GOG Blitzkrieg 1.2

The original game is installed from GOG on `desktop-016loe1` (`ssh win-home`,
`D:\GOG\Blitzkrieg`, version 1.2). It is **read only**: nothing on that
machine is changed. Its archives (`data.pak`, `patch-1.pak`, `patch-2.pak`,
`update-1.pak`, `patch_galaxy.pak`) are copied to
`zig-out/local-test/gog-original` for comparison; they are never committed.
Only hashes of GOG files enter the repository.

Compared with GOG, applied in load order, our `Data` holds every historical
mission, map and script unchanged. The mission-related differences are:

| Files | Difference | Action |
|---|---|---|
| `Scenarios/Campaigns/{German,Allies,USSR}/*.xml` | 20 templates missing from each | restore GOG lists |
| `Scenarios/Patches/{spring_ukraine,summer_russia,winter_russia}/p_hunted_{E,N,S,W}_1.bzm` (12) | ours are `data.pak`'s; GOG's `update-1.pak` replaces them | take `update-1.pak`'s |
| `Scenarios/Chapters/German/France/context.xml` | ours is `data.pak`'s; `update-1.pak` replaces it | take `update-1.pak`'s |
| `Scenarios/Chapters/German/France/1.xml`, `USSR/Leningrad/1.xml` | three generated random missions baked in, from when the game wrote into `Data` | restore GOG |
| `Scenarios/TemplateMissions/All/summer_france/securearea0{1,5}/1.xml` | objective positions from one generator run written back | restore GOG |
| `Scenarios/Chapters/German/Barbarossa/context.xml` | `BM_13` where GOG has `T34_Calliope_USA` (commit `8662b2ac6`) | **keep**, deliberate |
| `Scenarios/Chapters/USSR/Finland/1.xml` | float formatting only | restore GOG |

Out of scope: the ~30 UI, options and text files that differ from GOG as
part of the port's own changes, and GOG's `terrain/sets/terrain/...` and
`editor/resizedialogstyles` leftovers.

## Design

### 1. Restore the mission data

Copy the files in the table above from the GOG layers into `Data`, in
`update-1.pak`-over-`patch-*`-over-`data.pak` order, except Barbarossa's
`context.xml`. A manifest, `Data/Scenarios/gog-1.2-scenarios.sha256`, lists
the SHA-256 of every GOG file under `scenarios/` as GOG resolves it (the
highest layer wins), keyed by lower-case path, with the deliberate
deviations listed separately and each given a reason. A data check (below)
compares `Data/Scenarios` against it, so the files cannot drift again. The
manifest is generated once by a small script in `tools/data/` that reads the
local GOG copy; the script is committed, the GOG files are not.

### 2. Find and fix why defend, escort and hunt fail

The reason for `c1532955b` is not recorded. The work starts with a
diagnosis: generate every template through the headless generator test
(section 4) and run the four mission types in the game, and record each
failure with its root cause. Candidates already visible: the unpatched
`p_hunted_*` patches (fixed by section 1), the random map generator's port
to 64-bit and to case-sensitive file systems, and the template missions'
scripts. Each root cause is fixed in the engine or data with a failing test
first. The plan contains the diagnosis as its own task; the fix tasks are
written from its findings.

### 3. Remove the fallback

`CInterfaceChapter::InitWindow` returns to the original's behaviour: only
enabled, unfinished historical missions and the generated random missions
are listed, and the preselected mission is a historical one when one is
enabled, a random one otherwise. The block that lists every historical
mission when none is enabled is deleted. The guards the port added for a
stale `Mission.Current.Index` and for a chapter with no missions at all stay,
as error handling for broken mod data; they log and return to the campaign
screen, and they never list a mission the script has not enabled.
`IncrementChapterVisited`'s restoring of the previous mission set when
generation produced nothing stays for the same reason.

A data check guarantees the situation the fallback covered cannot arise
with the shipped data: every chapter whose script enables its historical
mission only after a random win (all but the first chapter of each
campaign) has at least three templates whose setting equals the chapter's
setting and at least three map placeholders, so the chapter screen always
generates one random mission per difficulty. The first chapters (Poland,
Norway, Finland) have no placeholders in the original either; their
`EnterChapter` enables the historical missions directly. With GOG's lists
the gated chapters have 4 to 24 templates and 7 to 10 placeholders each.

### 4. Tests

- **Data check** (`zig build test-mission-data`, CI on all targets): the
  manifest comparison of section 1; every campaign template resolves to a
  mission file whose `TemplateMap`, description texts and setting exist;
  the per-chapter template and placeholder counts of section 3, for the
  chapters whose script gates the historical mission on
  `Mission.Current.Random`.
- **Generator test** (`zig build test-random-missions`, CI on macOS and
  Windows): a headless C++ test executable, built the way
  `editor-bridge-test` is, that for each chapter of the three campaigns and
  each template of that chapter's setting, at each of the three
  difficulties (the first chapters have none), runs the random map generator with a fixed seed into a
  scratch generated-data root under `zig-out/local-test`, loads the
  resulting map, and checks that every `AnchorScriptID` the template's
  objectives name exists on it. A failure names the chapter, template and
  difficulty. Windows CRT asserts and aborts are routed to stderr.
- **Game run** (local, needs a GPU; `BK_AUTO_UI`): for one defend, one
  escort, one hunt and one secure-area mission, in a throw-away profile, the
  harness enters a chapter from a prepared save, starts the random mission,
  and wins it through the mission script (a new harness verb that runs a Lua
  call such as `Win(0)` in the mission's script engine). It then checks that
  the chapter screen lists the chapter's historical mission and nothing it
  should not. Artifacts go to `zig-out/local-test`.
- **By hand:** Johannes plays one defend, one escort and one hunt mission.

## Success criteria

- The three campaign files list GOG's 63/76/63 templates, and
  `Data/Scenarios` matches the GOG manifest except the recorded deviations.
- `test-mission-data` and `test-random-missions` pass in CI; the latter
  generates every template of every chapter at every difficulty.
- The game run wins each of the four mission types and the chapter then
  offers its historical mission.
- `Chapter.cpp` no longer lists historical missions the chapter script has
  not enabled.

## Constraints

- Work on branch `fix/revive-random-missions` in `.worktrees/random-missions`.
- GOG's machine is read only; GOG files are never committed.
- Test artifacts and scratch generated data live in `zig-out/local-test`.
- Never touch the live profile or cloud state; the game run uses a
  throw-away profile.
- Never `zig fmt` `build.zig`; run `zig test tools/zig/build_hermeticity_test.zig`
  after editing it.
