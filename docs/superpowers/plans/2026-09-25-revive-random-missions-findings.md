# Random missions: diagnosis

Sweep: `all`, 624 cases (20 chapters, 88 templates, 3 difficulties), on an
Apple M1 (macOS, `--release=fast`), HEAD 7f530496f.

- As committed: the sweep **crashes** (SIGSEGV, RC1) at case 179,
  `german\kharkov43\1 spring_ukraine\hunt00 d1`, after 75 s wall time for
  `zig build test-random-missions --release=fast -Dtest-mode=run
  -Drandom-missions-sweep=all` (build and staging up to date). 60 of the 178
  finished cases had failed, all "the seed gives the same map".
- With RC1 worked around (the staged `logs` objects copied back in by hand), the
  whole sweep runs: **624 cases, 88 failed, 206 s** (executable run directly,
  machine otherwise idle). The 88 failures are every template's regenerate case,
  all "the seed gives the same map" (RC3 + RC4).
- With RC1 worked around and the RC3/RC4 fixes below applied as temporary
  patches: **624 cases, 0 failed** (205 s with the two generators pinned from the
  harness; 240 s with the proposed generator fix, while a debug sweep ran
  alongside). `cover`: 208 cases, 0 failed, 82 s (measured alone).
- The debug build aborts at its first case (RC2). With RC2 patched, a debug
  `cover` sweep passes: 208 cases, 0 failed, 3807 s.
- Task 5.4 landed the RC3 fix for real (seeding `NWin32Random` and `rand()`
  from the stored seed in `MapInfo_StaticMethods_RMGeneration.cpp`, RC1-RC4 all
  now fixed in tree): `only=spring_ukraine` gives `60 cases, 0 failed`; the
  full `all` sweep gives **624 cases, 0 failed, 201 s** (3:31 wall,
  `--release=fast`); `cover` gives **208 cases, 0 failed, 77 s** (1:28 wall) —
  above the ≥150 floor a later CI decision needs.

For Task 9: a release `all` takes about 3.5 min of test time on an M1, and
`cover` about 1.4 min. A debug `cover` takes about 63 min. No other check (generates, reads, anchor, briefing map,
engine opens, regenerates) failed in any release case once RC1 was out of the
way.

## Failing cases

"First case" means the first case of each template in sweep order. That case
is always `d0` in `all`, and it is the only one that regenerates. "Any" means
any case, depending on which patches the generator draws.

| Template | Chapters | Difficulties | Check | Root cause |
|---|---|---|---|---|
| every `spring_ukraine` template (defend00, defend01, escort00, hunt00, securearea00-05) | german\kharkov43, ussr\ukraine (every chapter of the setting) | any: seen at d1 of defend00 and of hunt00 | generates (process dies with SIGSEGV) | RC1 |
| every template (the first one of the sweep in debug) | every chapter | any | the debug build aborts under UBSan before generating | RC2 |
| africa securearea00-03 | german\africa | first case (d0) | the seed gives the same map | RC3, RC4 |
| spring_france defend00, defend01, escort00, hunt00, securearea00-05 | german\ardennes | first case (d0) | the seed gives the same map | RC3, RC4 |
| spring_ukraine defend00, defend01, escort00, hunt00, securearea00-05 | german\kharkov43 | first case (d0) | the seed gives the same map | RC3, RC4 |
| summer_france defend00, defend01, escort00, hunt00, securearea00-09 | german\france | first case (d0) | the seed gives the same map | RC3, RC4 |
| summer_germany defend00, defend01, escort00, hunt00 | ussr\german | first case (d0) | the seed gives the same map | RC3, RC4 |
| summer_germany securearea00-09 | german\barbarossa | first case (d0) | the seed gives the same map | RC3, RC4 |
| summer_russia defend00, defend01, escort00, hunt00, securearea00-09 | german\typhoon | first case (d0) | the seed gives the same map | RC3, RC4 |
| summer_ukraine defend00, defend01, escort00, hunt00 | german\kharkov42 | first case (d0) | the seed gives the same map | RC3, RC4 |
| summer_ukraine securearea00-09 | german\typhoon | first case (d0) | the seed gives the same map | RC3, RC4 |
| winter_russia defend00, defend01, escort00, hunt00, securearea03-06 | ussr\moscow | first case (d0) | the seed gives the same map | RC3, RC4 |

By type: securearea 60, defend 14, escort 7 and hunt 7 fail "same map", which
covers all 88 templates. On every one of them the first difference the check
reports is `terrain.patches[0][0].basecrosses.size`, except
`summer_russia\defend01`, where it is `basecrosses[0].x`.

## Root causes

### RC1: staging drops `Data/Objects/SimpleObjects/common/summer/logs`, so the `Logs01`-`Logs08` objects have no stats, and the generator dereferences null

Evidence:
- Backtrace (release, `only=spring_ukraine`, run from
  `zig-out/game/macos/arm64/release`): `EXC_BAD_ACCESS address=0x0` in
  `ApplyTilesInObjectsPassability<ModifyTilesFunctional<CArray2D<unsigned char>, unsigned char>>`
  at `Sources/src/RandomMapGen/LA_Types.h:267`, called from
  `CMapInfo::CreateRandomMap` (`MapInfo_StaticMethods_RMGeneration.cpp`, the
  `ApplyTilesInObjectsPassability` calls at 652/659, inlined).
  `nObjectIndex = 117`, `pMapObjectInfo[117].szName = "Logs02"`.
- `GetDesc("Logs02")` succeeds: `Data/objects.xml:29550` has it, with the path
  `objects\simpleobjects\common\Summer\Logs\02`. `GetRPGStats` returns null
  because `ReadRPGStats` (`Main/GameDB.cpp:278`) cannot open `...\02\1.xml`.
- The file is in the repository (tracked since 5e48dedd8, 144 files under
  `logs/01`-`08`) and in GOG `data.pak`
  (`Objects/SimpleObjects/common/summer/logs/02/1.xml`), but it is not in
  either staged game: `zig-out/game/macos/arm64/{debug,release}/Data/Objects/SimpleObjects/common/summer/`
  lists every sibling except `logs`. The same is true of the main checkout's
  release game.
- The cause is `tools/zig/stage.zig:379-386` (`isForbiddenStagedPath`). It
  rejects a path if *any* component is a user-write name, and
  `isUserWriteName` (`stage.zig:402-407`) includes `"logs"`. It was added in
  e892d195c (2026-08-07, "build: exclude forbidden artifacts during staging").
  `tools/zig/verify_runtime.zig:286-295` (`isForbiddenArtifactPath`) has the
  same rule and would reject a staged tree that contained the directory.
  Across all tracked `Data`, this directory is the only one the rule catches.
- Which data places the object: three spring_Ukraine patches contain `Logs0x`
  (`Data/Scenarios/Patches/spring_Ukraine/p_wh_base_gr_we_6.bzm`,
  `p_wh_base_gr_we_8.bzm`, `p_wh_base_as_ns_6.bzm`). So only spring_ukraine
  templates can crash, and only when the draw picks one of these patches. This
  is not a case-sensitivity problem, and it is not from the GOG update-1 patch.
- Proof: with `logs` copied into the staged release `Data`, `only=spring_ukraine`
  goes from a crash at case 2 to `60 cases, 10 failed` (all "same map"), and
  the whole sweep runs through (624 cases).

Fix: in `tools/zig/stage.zig:379-386`, apply the user-write, temp and cache
names only to the first component of the `Data`-relative path (`Data/logs`,
`Data/saves`, `Data/cache` stay excluded, while `Objects/.../summer/logs` is
staged). In `tools/zig/verify_runtime.zig:286-295`, apply them only to the
first two components of the stage-relative path (the root and `Data/<x>`).
The stage test fixture `Data/logs/stage.log` stays excluded. Add a fixture
`Data/Objects/SimpleObjects/common/summer/logs/01/1.xml` to
`tools/zig/stage_test.zig` that must be staged. (Rejected alternative: an
allowlist entry for this one path. It would break again on the next nested
asset directory called `logs`, `temp` or `cache`.) Optionally, as defence in
depth, `LA_Types.h:266` could skip an object whose `GetRPGStats` is null, with
a `DebugTrace`. The GOG code does not check either.
Test: `zig build test-random-missions --release=fast -Dtest-mode=run -Drandom-missions-sweep=only=spring_ukraine`
fails before (the process dies with SIGSEGV, 2 `start` lines). After, it runs
all 60 cases (still with 10 "same map" failures until RC3/RC4 are fixed).
Also, `ls zig-out/game/macos/arm64/release/Data/Objects/SimpleObjects/common/summer/logs/02/1.xml`
exists after staging.

Beyond random missions: historical maps also place `Logs0x` (`Data/Maps/kursk.bzm`,
`USSR/Kursk/Kursk.bzm`, `German/Kharkov42/Kharkov42.bzm`, `Tutorial/tutorial3.bzm`,
`Multiplayer/siege.bzm`, `normandie.bzm` and others). Every game staged since
e892d195c has lacked these objects' stats and art.

### RC2: `SRMMiniMapCreateParameter` has no constructor, and reading it loads an uninitialised `bool` (UB, debug build aborts)

Evidence: the debug build, `only=summer_france\securearea01`, aborts at the
first case: `panic: load of value 64, which is not valid for type 'bool'` at
`Sources/src/RandomMapGen/MiniMap_Methods.cpp:101`
(`saver.Add( "ShowAllBuildingsPassability", &bAllBuildingPassability )`), from
`CMapInfo::CreateMiniMapImage` (`MapInfo_StaticMethods_MiniMapCreation.cpp:72`,
`SRMMiniMapCreateParameter createParameter;` then `LoadDataResource`).
`CTreeAccessor::AddIntData<bool>` (`Sources/src/StreamIO/DTHelper.h:66`) copies
`*pData` into an `int` *before* it reads the chunk, so an uninitialised member
is loaded as a `bool`. All four `Data/Terrain/sets/*/minimap.xml` carry every
field, so a release build ends up with the right values. The defect is the load
of the uninitialised value, which UBSan rejects in debug.
Proof: with a temporary constructor, the same debug run passes (12 cases, 0 failed).

Fix: `Sources/src/RandomMapGen/MiniMap_Types.h:117`, after `DWORD dwBridgeWidth;`, add
`SRMMiniMapCreateParameter() : nWoodRadius( 0 ), fTerrainShadeRatio( 0.0f ), bAllBuildingPassability( false ), bTerrainShades( false ), dwMinAlpha( 0 ), dwBridgeWidth( 0 ) {}`.
Test: `zig build test-random-missions -Dtest-mode=run -Drandom-missions-sweep=only=summer_france\securearea01`
(debug) aborts with the panic before and runs to its result line after. See
"Debug build" below for what a debug `cover` sweep found once this was patched.

### RC3: the generator draws from two process-global generators that the stored seed does not restore: `NWin32Random` and the C runtime's `rand()`

The stored `.seed` (`MapInfo_StaticMethods_RMGeneration.cpp:901-909`) only holds
the `IRandomGen` state. Correction: the port's `IRandomGen` is not ISAAC but a
32-bit LCG (`Sources/src/StreamIOZig/legacy_bridge.cpp` `RandomGen`,
`streamio.zig` `bk_random_*`), whose `Init()` starts from a constant (Task 5.5
fixes that, seeding it from entropy each launch instead). The generator also
draws from:
- `NWin32Random` (a static LCG in `Sources/src/Misc/Win32Random.cpp`, never
  seeded anywhere in the tree). It drives the polygon edge jitter in
  `RandomizeEdges`/`GetRandomBetweenPoint` (`Sources/src/RandomMapGen/Polygons_Types.h:1058,1061,1112,1152`),
  called by `CreateRandomMap` (`MapInfo_StaticMethods_RMGeneration.cpp:1705,1712`)
  and by `VSO_StaticMethods.cpp:446` (rivers and roads).
- the C runtime `rand()` in `STileTypeDesc::GetMapsIndex`
  (`Sources/src/Formats/fmtTerrain.h:87`). It picks each tile's variant,
  called from `MapInfo_StaticMethods_RMGeneration.cpp:443` (default field),
  `:531` (`FillTileSet`) and `Sources/src/RandomMapGen/TerrainBuilder.cpp:141`
  (`CTerrainBuilder::GetNeighboursMask`, the cross tiles). The game seeds it
  from the clock (`GameTT/Chapter.cpp:187,375`).

Evidence (release, `only=spring_ukraine`, 10 regenerate cases, harness patched
to pin a generator to a fixed value right before each `CreateRandomMap`, and
with RC4 normalised):

| pinned | result |
|---|---|
| neither | 10 × differs at `basecrosses.size` |
| `NWin32Random` only | 9 × `basecrosses[0].tile`, 1 × `basecrosses[0].cross` (the shapes now match, the tile variants do not) |
| `rand()` only | 10 × `basecrosses.size` |
| both | 0 failures; whole `all` sweep: 624 cases, 0 failed |

With both pinned, per-stage hashes of the tiles and objects printed at every
`timeKeeper.Trace` checkpoint are identical between the first run and the
regeneration. An lldb breakpoint on `rand` over one hunt00 case found no
`rand()` caller in the generation other than `GetMapsIndex` (36,729 calls from
`FillTileSet`, 12,072 from `GetNeighboursMask`). The `rand()` sites in
`MapInfo_StaticMethods.cpp:1944-2000` and `MapInfo_StaticMethods_SoundsCreation.cpp`
are never reached by `CreateRandomMap`.
The GOG original has the same code (`NWin32Random` unseeded, `rand()` seeded
from `timeGetTime`), so the defect is inherited, not introduced by the port.
It matters when `Main/RandomMapHelper.cpp:43` regenerates, for example when
another machine or a cleared cache has no map, or when a later mission of the
same template has overwritten `maps\<finalmap>`.

Fix (options):
1. **Recommended.** Derive both from the stored seed, right after it is written.
   In `MapInfo_StaticMethods_RMGeneration.cpp`, after line 909
   (`pRandomGenSeed->Store( pRandomSeedStream );`), add:
   ```cpp
   // The tile variants (rand() in STileTypeDesc::GetMapsIndex) and the polygon
   // jitter (NWin32Random in RandomizeEdges) draw from generators the seed
   // does not hold: seed both from it, so a save regenerates the same map.
   const unsigned int nLegacySeed = Random();
   NWin32Random::Seed( int( nLegacySeed ) );
   srand( nLegacySeed );
   ```
   Also add `#include "../Misc/Win32Random.h"`. Verified as a temporary patch:
   624 cases, 0 failed (together with the RC4 fix) and 208 `cover` cases, 0
   failed. This is two lines and changes no call site. The costs: it reseeds
   the process-wide `rand()` (harmless, the game already seeds it from the
   clock), and it assumes no other thread calls `rand()` during generation. The
   sweep shows none in the test, but that was not checked in the running game.
   `NWin32Random` is also a
   visual/effects generator elsewhere (`Scene`, `MOUnit*`), and it drives
   `ScenarioTracker2Internal.cpp` (reincarnation), `PlayerSkill.cpp` and
   `iMainInternal.cpp`. Reseeding it at generation time does not change the
   distribution any of those draw from, only where in the sequence a later
   draw lands; generation runs in the briefing, before those systems have
   drawn anything for the mission, so this changes nothing a player sees.
2. Route every generator draw through `IRandomGen`: give `GetMapsIndex` an
   overload that takes the draw (`GetMapsIndex( Random( 10000 ) )` at the three
   generator call sites), and make `RandomizeEdges`/`GetRandomBetweenPoint`
   use `Random()` instead of `NWin32Random`. This is cleaner (no global
   state, thread safe), but it touches headers the map editor and `Scene`
   share (`fmtTerrain.h`, `Polygons_Types.h`) and changes more lines.
3. Store the `NWin32Random` and `rand()` state in the `.seed` file: rejected,
   because the C runtime's state is opaque and the format is the original's.

Test: after the RC1 and RC4 fixes,
`zig build test-random-missions --release=fast -Dtest-mode=run -Drandom-missions-sweep=only=spring_ukraine`
fails before (`10 failed`, "the seed gives the same map (differs at
terrain.patches[0][0].basecrosses.size)") and passes after (`0 failed`). Landed
as the real fix (Task 5.4): `only=spring_ukraine` gives `60 cases, 0 failed`;
the full `all` sweep gives `624 cases, 0 failed, 201 s`; `cover` gives `208
cases, 0 failed, 77 s`.

### RC4 (test): the regeneration writes to another root, and the map records its own path, so "same map" can never pass

Evidence: `CreateRandomMap` stores the output path in the map
(`MapInfo_StaticMethods_RMGeneration.cpp:948`,
`mapInfo.szScriptFile = szRandomMapName`), and `MapFile/MapEquivalence.cpp:385`
compares it. The test regenerates into `<case>/b`
(`tools/zig/random_missions_test.cpp:240`) but generated into `<case>/a`. With
RC3 fixed (proposed patch) and nothing else changed, `only=spring_ukraine` gives
`10 × the seed gives the same map (differs at szScriptFile)`. With
`again.szScriptFile` set to `map.szScriptFile` before the comparison, it gives 0.
The game regenerates into the same root it generated into
(`Main/RandomMapHelper.cpp:74-75` and `GameTT/Mission.cpp:168-169` both use
`NGeneratedData::Root( MOD )`), so this is a defect of the test, not of the game.

Fix: `tools/zig/random_missions_test.cpp:240-245`. Regenerate into `szRoot`
itself, as the game does, and read the regenerated map from `szRoot`. The first
map is already in memory as `map`, and the `.seed` is rewritten with the same
state. Drop `szRootB`.
Test: the same `only=spring_ukraine` run gives `differs at szScriptFile` before
this fix (with RC3 fixed) and `0 failed` after.

## Debug build

With RC2 patched temporarily (and RC1 worked around, RC3 pinned, RC4
normalised), a debug `cover` sweep (UBSan live) ran in a scratch directory of
its own: **208 cases, 0 failed, 3807 s** (about 18 s per case). This found no
further UB and no other failure, so RC2 is the only debug-only defect this tier
reaches. (An earlier debug run that shared its scratch directory with
concurrent release runs reported "regenerates from its seed" for
summer_france securearea04 d2 and summer_germany securearea05 d2. The
isolated rerun passed both, with the same graph and angle. That run was an
artefact of the two processes deleting each other's case directories.) A debug
`all` would take about 3 h and was not run.

## The spec's other candidates

1. Case-sensitive paths: no failure traced to one. With RC1 worked around and
   RC3/RC4 patched, every template generates, reads, has its anchors on the map,
   lands on the briefing map, opens in the engine and regenerates identically
   (624/624). This includes the mixed-case patch directory `spring_Ukraine` and
   object paths like `Summer\Logs`, which the data tier resolves.
2. 64-bit: nothing surfaced in release (624/624), and nothing in the debug
   UBSan `cover` sweep beyond RC2 (208/208, see above). `SRandData` uses `unsigned _int32` and
   stores and restores symmetrically, and `IRandomGen` is restored correctly:
   with the other two generators pinned, the maps are identical.
3. Mission scripts: not exercised by this tier. `CreateRandomMap` copies
   `randomMapTemplate.szScriptFile + ".lua"` next to the map
   (`MapInfo_StaticMethods_RMGeneration.cpp`, after the save), and the scripts
   exist in `Data/Scenarios/Scripts` (`Defend_Area00`, `Defend_Area01`,
   `Escort00`, `Intercept00`, `SA_*`). Whether they run without a Lua error is
   a question for Task 8.
4. Windows debug asserts (`NI_ASSERT` live): **pending**. By the controller's
   ruling this runs in CI later (Task 9), not on `win-home`.

## Observations, not failures

- In `german\france` d2 cases the chapter units table has zero weights for
  `armor light` and `spg assault` for player 1. The generator logs
  "All table entries has zero weights ... Override unit to: Humber_MK1_GB" and
  places the override. This is not a failure of any check. It is worth a look
  in Task 8 if the enemy mix looks wrong in play.
