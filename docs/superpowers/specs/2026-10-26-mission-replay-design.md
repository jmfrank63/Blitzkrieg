# Mission replays: record, quicksave, quickload, play back

## Goal (2026-10-26, Johannes)

"Recording and playing back missions. This will not be easy, since we have to deal with saving and
loading. What we could allow is quicksave and quickload. But regular loading should be disabled, you
have to end the mission to stop the recording to load again."

So: every single-player mission is recorded from its start to its end, and the recording can be
played back as the same battle. Quicksave (F5) is allowed during a recorded mission and leaves a
marker; quickload (F8) rewinds the recording to that marker. Any other load is refused while a
mission is being recorded. Ending or leaving the mission stops the recording and keeps the replay.

This spec supersedes the backlog note `.planning/phases/999.6-single-player-replays/999.6-NOTES.md`
(it agrees with it and adds the save/load rules and the determinism work).

## What exists

The 2003 game already has replays. A replay (`.rpl`) is not a video: it is the list of player
commands per AI segment, and playback runs the mission again and feeds them in.

### Recording

- `CCommandsHistory` (`Sources/src/Main/CommandsHistory.h:39`, singleton `ICommandsHistory`,
  `Sources/src/Main/CommandsHistoryInterface.h:5`) keeps `savingHistory`, a map from segment number
  to the commands issued in it (`CommandsHistory.h:44-45`).
- Every player order goes through the transceiver. The single-player one executes it at once and
  adds it to the history under the current segment `nCommonSegment`
  (`Sources/src/Main/SinglePlayerTransceiver.cpp:69-126`): register/unregister group, group command,
  unit command, show areas. `AddCommand` stores the ones whose `NeedToBeStored()` is true
  (`CommandsHistory.cpp:237-241`; all but `CControlSumCheckCommand`,
  `Sources/src/Main/AILogicCommandInternal.h:107-129`). Multiplayer adds the commands of all
  players at the segment they are executed (`Sources/src/Main/MultiPlayerTransceiver.cpp:640-656`).
- A segment is a fixed 50 ms of game time (`AI_SEGMENT_DURATION`, `Sources/src/AILogic/aiconsts.h:12`);
  `DoSegments` runs `ExecuteSegmentCommands(n)` then `IAILogic::Segment()` and increments the segment
  (`SinglePlayerTransceiver.cpp:47-68`). Game speed and pause only change how many segments a frame
  runs, not what a segment does.
- At mission start `PrepareToStartMission` clears the history, keeps the random seed in
  `pStartRandomSeed` and a copy of the scenario tracker (`CommandsHistory.cpp:17-35`), called from
  `CICMission::PostCreate` (`Sources/src/GameTT/iMissionInternal.cpp:229-231`).
- After loading the map the AI computes a CRC of every unit's position, direction, HP and morale
  (`CAILogic::UpdateCheckSum`, `Sources/src/AILogic/AILogicInternal.cpp:986-1015`) and hands it to
  `CheckStartMapCheckSum`, which prints "Invalid history: maps are different" when a played-back
  history started elsewhere (`CommandsHistory.cpp:258-268`, `AILogicInternal.cpp:955-956`).
  Multiplayer repeats that CRC every `checksumperiod` ms to detect out-of-sync clients
  (`AILogicInternal.cpp:819-823`, `1085-1089`). This is the state hash the verification below builds on.

### Storing

- `CCommandsHistory::Save` writes a data tree (`CreateDataTreeSaver`, the XML tree) with
  `StartMapCheckSum`, `History`, `MissionName`, `MapName`, `ChapterName`, the start `ScenarioTracker`,
  `Players` (logic id and side), `Multiplayer` and every `Multiplayer.*` global var (game speed among
  them), `MODName`, `MODVersion` and, since the port's Remastered option, `Remastered`
  (`CommandsHistory.cpp:162-236`).
- Not stored: the start random seed (`RandomSeed01`, commented out at `CommandsHistory.cpp:177-181`),
  the random-map header and seed of a template mission (commented out at `195-207`), the difficulty
  level, the game version.
- Where: `<profile>/replays/<name>.rpl`, or `<profile>/mods/<mod>/replays/` with a mod active
  (`Sources/src/GameTT/SaveReplay.cpp:89-103`, `Sources/src/GameTT/ReplayList.cpp:84-101`).
  `NProfile::Prefix` puts it under the player profile, or under `BK_PROFILES_ROOT`
  (`Sources/src/StreamIO/ProfilePaths.h:30-46`).
- When: only the mission statistics screen offers "Save Replay" (`Data/UI/Stats.xml`, element 10003,
  hidden), and `Stats.cpp:489-493` shows it only for a multiplayer game that is not itself a replay.
  It opens `ui\Popup\SaveReplay` for a name (`SaveReplay.cpp:85-113`). The developer flag `-sh<file>`
  writes the history at exit (`Sources/src/Game/GameMain.cpp:2021`, `2270-2278`) and before every save
  (`Sources/src/Main/MainLoopCommands.cpp:159-160`).

### Playing back

- Main menu > Load > "Load replay" (`Sources/src/GameTT/UIState.cpp:323-324`) lists `*.rpl`
  (`ReplayList.cpp:84-101`). `OnOk` loads the file, switches to the replay's mod if needed and
  re-enters through `SetFileName` (`ReplayList.cpp:113-136`, `27-57`), which creates a single- or
  multiplayer transceiver as the replay says and starts the mission paused.
- `Load` reads the tree back, swaps in the recorded scenario tracker (the player's own is stored and
  restored afterwards, `CommandsHistory.cpp:84-86`, `57-64`), sets `History.Playing`, `History.Remastered`
  and the `Multiplayer.*` vars, and sets a zero random seed (`CommandsHistory.cpp:103-105`). In a final
  build it refuses a replay whose map or resource checksums differ from the installed ones
  (`CommandsHistory.cpp:146-160`, the errors shown by `ReplayList.cpp:63-83`).
- During playback the transceiver ignores the viewer's orders (`bHistoryPlaying`,
  `SinglePlayerTransceiver.cpp:71-126`) and `ExecuteSegmentCommands` feeds the loaded commands in
  before the segment they were recorded in (`CommandsHistory.cpp:242-252`).
- When the mission ends, the win/lose/quit paths restore the scenario tracker and go back to the
  replay list (`Sources/src/GameTT/PlayersInterface.cpp:115-125`, `Stats.cpp:583-594`,
  `Sources/src/GameTT/CustomMessageReaction.cpp:89-99`).
- `-lh<file>` / `-lhclient<file>` play a history from the command line (`GameMain.cpp:2279-2295`,
  `CommandsHistory.cpp:36-56`).

### What the port changed in replay code

`git log` on `CommandsHistory.*`, `ReplayList.cpp`, `SaveReplay.cpp`, `SinglePlayerTransceiver.cpp`:
path and include normalisation, UTF-16 text fixes (`ec69d5173`), profile paths (`b0d803b85`,
`62a5ebaff`), the map script path expansion when the replay computes map checksums (`52613f14a`) and
the `Remastered` key (`dd25a28fa`). Outside those files, three changes matter:

- `686fe5b84` (2026-09-25) seeds the random generator from OS entropy at every launch, as the original's
  ISAAC did; before it every launch drew the same sequence. Single-player recordings therefore start
  from an unpredictable seed, and playback starts from zero.
- The port replaced StreamIO's ISAAC generator with a 32-bit LCG in Zig (`bk_random_get`,
  `Sources/src/StreamIOZig/streamio.zig`), wrapped by `RandomGen` in
  `Sources/src/StreamIOZig/legacy_bridge.cpp:1752-1776`. Its `Store`/`Restore` are empty and it has no
  `operator&(IStructureSaver&)`, so `saver.Add( 14, g_pGlobalRandomGen )` in the save
  (`Sources/src/Main/iMainInternal.cpp:312-313`) writes and reads nothing. The original saved the whole
  ISAAC state (`Sources/src/StreamIO/RandomGenInternal.cpp:94-105`). In the port a loaded game goes on
  with whatever random state the process happens to have.
- 64-bit SSE floats instead of the original's x87 at 24-bit precision
  (`AILogicInternal.cpp:1017-1022`): replays from the 2003 game cannot play identically in the port
  (they are also refused by the checksums once resources differ). This spec does not try to play them.

### What happens today when the player saves or loads during a mission

The save holds the whole history: `CMainLoop::Serialize` adds the `ICommandsHistory` singleton as
chunk 23 (`iMainInternal.cpp:325`), which writes `savingHistory`, `loadedHistory`, the start checksum,
start seed, start tracker and players (`CommandsHistory.cpp:279-294`), and the transceiver with its
`nCommonSegment` as chunk 12 (`iMainInternal.cpp:307`, `SinglePlayerTransceiver.cpp:9-22`).

- Save, quicksave (F5 writes `quick.sav`, `iMainInternal.cpp:534-545`; `Data/Configs/defconf.cfg:959-971`),
  autosave: the recording goes on unchanged.
- Load, quickload (F8, `iMainInternal.cpp:547-556`), loading from the escape menu
  (`Data/UI/escapemenu/EscapeMenuReactions.xml:857-888`) or the main menu: the history and the segment
  counter come back as they were when that save was written. The recording is silently rewound to the
  save's moment, whatever save it is: an older one of the same mission, or one of another mission,
  whose history then replaces the current one. Nothing is discarded or marked. A replay saved later
  holds "start to the save's moment, then what happened after the load".
- That replay then desyncs on playback for three reasons: the start seed is not in the file, the
  random state after the load is not the state at the save's moment (the save keeps none in the port),
  and nothing proves that a loaded world simulates exactly like the world that was saved.
- Multiplayer refuses save and load (`MainLoopCommands.cpp:162-163`, `212-216`).

So the save/load rules the goal asks for are close to what the engine already does: a quickload
already rewinds the history to the quicksave. What is missing is the random state, the proof that a
load is transparent, and the rule that other loads are refused.

## Determinism

Playback is exact only if the AI, given the same start state, random seed and commands at the same
segments, computes the same world. The original multiplayer relied on the same property (lockstep
with periodic CRC checks), so the AI was written for it. What holds and what does not:

| Concern | State | Evidence |
| --- | --- | --- |
| Fixed time step | Holds: the AI advances in 50 ms segments, independent of frame rate and game speed | `aiconsts.h:12`, `SinglePlayerTransceiver.cpp:56-67`; `AILogic` reads no clock (only `Updater.cpp:306`, for visual interpolation) |
| AI threads | Holds: the AI runs on the main thread; no thread creation in `AILogic`, `Main`, `GameTT` | grep |
| One random stream for the AI | Holds: `Random()` (`Sources/src/StreamIO/RandomGen.h:23-35`) is the global generator; visual randomness uses the separate `NWin32Random` (`Sources/src/Misc/Win32Random.h`) in `Scene` and `Common`, and `rand()` in a few visual spots (`Common/MOUnitMechanical.cpp:897`, `978`, `Common/MOUnitInfantry.cpp:244`) | grep |
| Start seed | Broken for single player: entropy at launch (`686fe5b84`), not stored, zero on playback. Fixed in slice 1: `StartRandomState` | above |
| Random state across a load | Broken in the port: not saved. Fixed in slice 1: chunk 14 holds it again | `legacy_bridge.cpp:1752-1776`, `iMainInternal.cpp:312-313` |
| Load transparency | Unproved: the replay plays the world through the save's moment without loading; the recording continued from a loaded world | - |
| AI state changed outside the transceiver | Gaps: "Call HQ" runs a script function directly (`Sources/src/GameTT/WorldClientActions.cpp:873-877`), the difficulty is set by the options (`Sources/src/StreamIO/OptionSystemInternal.cpp:113-135`) and is not in the replay | grep |
| Pointer-ordered containers in the AI | Risk: `std::set< CObj<CFormation> >` (`Sources/src/AILogic/Units.h:25`), pointer hashes (`AILogicInternal.h:44`, `RailroadGraph.h:197`, `Graveyard.h:64`). Iterating them follows heap addresses, which differ between a recorded run and its playback | grep |
| Same build, same platform | Holds in the experiment below (two playbacks identical); the pointer-ordered containers can still break it on other maps | experiment |
| Across platforms | Does not hold today: three standard libraries (MSVC STL, libstdc++, libc++) differ in `unordered_map` bucket order (117 uses in `AILogic`) and in `std::sort` order for equal keys (`Commander.cpp:45`, `69`, `GeneralIntendant.cpp:642` sort by ratings); libm `sin`/`cos`/`atan2` differ in the last bit between glibc, Apple and UCRT; clang contracts `a*b+c` into FMA by default on arm64 (the build sets no `-ffp-contract`, `build.zig:21-41`), and not on baseline x86_64 | grep, `build.zig` |
| GPU renderer, text engine, Linux/macOS windowing | No effect: they do not call `Random()` and do not change AI state; the frame rate they give changes only how many segments run per frame | grep |
| Remastered | Holds: recorded and replayed (`CommandsHistory.cpp:140-144`, `233-234`); the option can only change on the main options screen, so a mission runs in one mode | `docs/superpowers/specs/2026-10-25-remastered-option-design.md` |

## Experiment (2026-10-26, Linux x64, debug build of b38b2e356)

A scratch mod held `Data/Maps/river3d.xml` with two German rifle squads (player 0) and two Soviet
ones (player 1), 140 map units apart, so they fight at once and the random hits decide who falls. The
Game ran under Xvfb with HOME, XDG_*, BK_USER_ROOT, BK_PROFILES_ROOT and BK_MODS_ROOT under
`zig-out/local-test/replay-exp`, driven by `BK_AUTO_UI`; `units=0x0x1000000` printed each player's
living units at 1 s and then every 5 s of game time. `-sh<file>` recorded, `-lh<file>` played back.

| Run | 1 s | 6 s | 11 s | 16 s | 21 s | 26 s | 31 s |
| --- | --- | --- | --- | --- | --- | --- | --- |
| record 1 | 20 / 20 | 19 / 11 | 15 / 3 | 14 / 0 | 14 / 0 | 14 / 0 | 14 / 1 |
| record 2 | 20 / 20 | 18 / 10 | 15 / 4 | 13 / 1 | 13 / 0 | 13 / 0 | 13 / 1 |
| playback 1 of record 1 | 20 / 20 | 19 / 10 | 13 / 3 | 11 / 0 | 11 / 0 | 11 / 0 | 11 / 1 |
| playback 2 of record 1 | 20 / 20 | 19 / 10 | 13 / 3 | 11 / 0 | 11 / 0 | 11 / 0 | 11 / 1 |

- Two recordings of the same mission differ: the seed comes from entropy at launch.
- Two playbacks of the same file are identical at every point: with the same seed and the same
  (here: no) commands the AI is deterministic run to run on one machine, through the mission start,
  its autosave and different frame timing.
- A playback does not reproduce its recording (11 survivors against 14): the recording's seed is lost.

Save and load: a run saved the game at 6 s (`MAIN_COMMAND_SAVE`) and went on; three fresh processes
loaded that save, unpaused and counted at the same game times after the load.

| Run | +0 s | +3 s | +6 s | +9 s |
| --- | --- | --- | --- | --- |
| the saving run, going on | 19 / 10 | 14 / 6 | 12 / 2 | 11 / 0 |
| load 1 | 19 / 10 | 14 / 7 | 12 / 2 | 11 / 0 |
| load 2 | 19 / 10 | 14 / 6 | 11 / 4 | 10 / 2 |
| load 3 | 19 / 10 | 14 / 6 | 11 / 3 | 10 / 1 |

The same save goes on differently in every process, while the playbacks above, timed the same way,
matched exactly: the random state is not in the save. The `.rpl` the recordings wrote is 347 bytes:
the header, `<History/>` and no seed. An attempt to also record player orders through `click=` /
`rclick=` selected nothing on this map (TraceSelection reported no units), so command playback itself
was not exercised; the replay list UI was not driven either. The scripts are scratch files under
`zig-out/local-test/replay-exp` and are not part of the repository.

## The rules

### Recording

1. Every single-player mission is recorded: campaign, chapter, custom and random (template) missions,
   tutorials included. Recording begins in `PrepareToStartMission` and covers the mission until it
   ends. Playback itself is never recorded. Multiplayer keeps recording as it does (below).
2. At the start the history stores, besides what it stores today: the random generator state at the
   moment the AI starts (taken where `PrepareToStartMission` now takes `pStartRandomSeed` and set again
   at the same point on playback, not in `Load`, so nothing that runs between the replay list and the
   mission start can shift it), the difficulty level, the random-map header and seed for a template
   mission (the commented-out code at `CommandsHistory.cpp:195-207` and `88-98`), and a recording id
   (a random 128-bit value, stored in the save with the history).
3. Orders that change the AI outside the transceiver become recorded commands: "Call HQ" (a
   `CScriptCallCommand` that carries the script function name), and anything else the audit tier
   (slice 2) finds. A replay has no other input.

### Quicksave

4. F5 during a recorded mission writes `quick.sav` as today. The history adds a marker before the
   save is written: `{ index, segment, state hash, real-time timestamp }`. The marker is in the save
   (it is part of the history) and in the replay.
5. A regular save (escape menu > Save, or an autosave) is allowed and is a marker too, but such a save
   cannot be loaded until the mission ends (rule 8; decision 3). Ironman (`Options.MissionSave.Disabled`) keeps
   refusing manual saves.

### Quickload: rewinding the recording

6. F8 during a recorded mission loads `quick.sav` only if its history carries the current recording
   id. The load restores, from the save:
   - `savingHistory`, so every command after the quicksave's segment is dropped, and the markers up
     to and including the quicksave's;
   - `nCommonSegment` (the transceiver, chunk 12) and the AI's own time, so recording continues at the
     quicksave's segment;
   - the random generator state at the quicksave's moment. This needs the generator saved again:
     `RandomGen` gets `operator&(IStructureSaver&)` writing `bk_random_get_state()` as chunk 1, so
     chunk 14 of the save holds it (a save without it loads as today). This is a port fix and applies
     to every load, not only to replays: it restores the original game's behaviour that a loaded save
     goes on with the random state it was saved with.
   The history then also records the rewind itself: a `rewinds` list, `{ marker index, segment, how many
   commands were dropped }`, kept outside the part a load restores (copied over the load), for the
   replay's statistics and for diagnosis. Playback ignores it.
7. The result: the recording is the timeline the player ended up keeping. Playback runs from the start
   through every segment up to the quicksave's (the same commands, the same random draws, so the same
   world), then the commands recorded after the quickload. For this to equal what the player saw, a
   loaded world must simulate exactly like the world that was saved: slice 3 proves it, with the
   marker's state hash checked during playback (rule 13). If a mission's world cannot be made
   load-transparent, the fallback is to put the quicksave into the replay and load it at the marker on
   playback (open question 1).
8. Every other load is refused while a mission is recorded: the escape menu's Load button is greyed
   (`UI_DISABLE_WINDOW_FORCE`, as `EscapeMenuReactions.xml:738-773` already does for Save under
   Ironman), F8 with a `quick.sav` from another recording does nothing, and the in-mission load list is
   not reachable. A tooltip on the greyed button, and a chat line for a refused F8, read "End the mission
   to load a saved game" (text key `textes\strings\replay-load-disabled`, and
   `textes\strings\replay-quickload-other` "This quicksave belongs to another mission"). The main menu's
   Load is reachable only once the mission is left, so it needs no rule.
9. The texts go through the localization pipeline the Remastered option used (commit `35dbf1f98`):
   the English text in `Data/Textes/...` (UTF-16LE), the ELK entry in `Data/ELK/game_data_base/textes/...`,
   an overlay per language in `Localization/game/<code>/game_data_base/textes/...` for all nine
   (`ru`, `zh-Hans`, `ja`, `hi`, `es`, `fr`, `de`, `pt-BR`, `id`), `Localization/game/base-hashes.txt`;
   the `language-mods` completeness check fails a language that misses a key.

### Ending

10. Recording stops when the mission ends: won, lost, aborted (escape menu > End mission / Exit to main
    menu / Exit to Windows), restarted (a restart is a new mission start and a new recording), or the
    game quits. The replay is then written automatically to
    `<profile>/[mods/<mod>/]replays/auto/<chapter>-<mission>.rpl`: one replay per mission, the latest
    attempt, overwriting the one before (decision 2). There are no manual replay saves: the statistics
    screen's "Save Replay" (`Stats.cpp:489`) goes, in single player and multiplayer alike, and a
    multiplayer game is saved the same automatic way. The lose screen's "Return to game" and a quickload
    from it continue the recording.
11. After the mission ends, loading is allowed again.

### Multiplayer

12. Multiplayer has no saves or loads, so none of the save/load rules apply. It keeps its zero start
    seed (`Sources/src/Main/GamePlaying.cpp:36-38`, `111-113`), its Save Replay button and its replay
    list; it gets the new header (version, mods, hashes) and the automatic save of rule 10. Remastered
    stays off in multiplayer and in its replays.

## Playback

13. Playback is a mission run with the viewer's orders ignored, as today. Each N segments (N = 20, one
    second of game time) it compares the AI's state hash with the one recorded at that segment; the
    first mismatch is reported once in the chat ("Replay out of sync at 03:12") and in the debug log,
    and playback continues. The markers' hashes are checked the same way.
14. Controls, a strip at the top of the screen in playback only: pause, speed (the game's own speed
    steps, `CMD_GAME_SPEED_INC/DEC`, up to the fastest the timer allows, plus a "fast forward" that runs
    segments without drawing until a target time), a time line with the recorded duration and the
    markers, and "jump to marker", which fast-forwards to the marker's segment (seeking back restarts the
    mission and fast-forwards). The camera, selection and minimap stay free: they do not change the AI.
15. Replays are listed with mission, date, duration, game version and mod. A replay whose game version,
    mod set or map/resource hashes differ is shown greyed with the reason (the existing
    `message_box_error_badmap/badresources` texts, plus a new `replay-version-differs`).

## File format and versioning

The `.rpl` stays the engine's data tree, so the existing reader and the replay list keep working. New
keys, all optional for a reader of an older file:

| Key | Meaning |
| --- | --- |
| `ReplayVersion` | 2 for this format (absent = 1, the original) |
| `GameVersion` | `build.zig` `game_version`, e.g. `2.0.0`, plus the git commit |
| `Platform` | `linux-x86_64`, `macos-aarch64`, `windows-x86_64` |
| `Mods` | the active mod and its version (today `MODName`, `MODVersion`), plus every language mod and its pak hash; a language mod changes texts only, so a replay plays under any language and the field is informational |
| `MapHash`, `ResourceHash` | the existing `CheckSumMap` / `CheckSumRes` of the map, in single player too |
| `Remastered` | as today |
| `Difficulty` | the AI difficulty level at the start |
| `StartRandomState` | the generator state at the AI start (`RandomSeed01`, revived; absent = zero seed) |
| `RandomMap` | header and seed for a template mission |
| `RecordingId` | rule 2 |
| `Markers`, `Rewinds` | rules 4 and 6 |
| `StateHashes` | segment and CRC every N segments, delta-encoded |
| `DurationSegments`, `Outcome` | for the list and the playback time line |

The replay is language-independent: it stores commands, ids and hashes, no text. A replay plays only on
the same `GameVersion` and `Platform` until cross-platform determinism is proved (slice 7, decision 5:
replays must play on every platform); a later build
may decide a version range is compatible when the determinism tiers say so.

## Verification

- `test-replay-core` (Zig over the C ABI, or C++ beside the engine tests): history markers, rewind
  truncation, the format round trip, old-format reading.
- `replay-game-reads-it`: the real Game on a test map written at test time (like
  `remastered-game-reads-it`: two sides that fight, so random draws decide the outcome), driven by a
  `BK_AUTO_UI` schedule with orders, records a replay, plays it back twice, and compares the per-second
  state hashes of the recording and both playbacks; then the same with a quicksave, more orders, a
  quickload and different orders; then checks that the escape menu's Load is greyed and F8 with a
  foreign `quick.sav` does nothing. It writes nothing outside `zig-out/local-test/replay`.
- `replay-load-transparency`: runs a mission N segments, saves, loads, runs M more and compares the
  state hashes with a straight N+M run, on several test maps and one campaign mission.
- `replay-rng-audit`: a debug-only trap that counts `Random()` calls outside `IAILogic::Segment()` and
  command execution during a scripted mission; it must stay at zero.
- Cross-platform: the CI fan-out (`.github/workflows/cross-platform.yml`) runs `replay-game-reads-it`
  on Linux x64, macOS arm64 and Windows x64, each on its own recording. A further job records on Linux
  and plays back on the other two, expected to fail until slice 7; it reports, it does not gate.

## Slices, in order

1. The random state in saves, and the start state in the replay. `RandomGen` serializes its state
   (chunk 14); the history stores and restores `StartRandomState` at `PrepareToStartMission`, and the
   difficulty; the state hash every N segments (`UpdateCheckSum` without sending, in single player too)
   goes into the history. Verify: `zig build test-replay-core`, `zig build test-streamio` (it holds the
   `bk_random_*` tests), `zig build replay-game-reads-it` (record, play back twice,
   hashes equal), `zig build remastered-game-reads-it`.
2. The input audit. "Call HQ" and any other direct AI call found become recorded commands; the
   `replay-rng-audit` trap. Verify: `zig build replay-rng-audit`, `zig build replay-game-reads-it`.
3. Load transparency. Fix what `replay-load-transparency` finds (likely pointer-ordered containers and
   state rebuilt on load). Verify: `zig build replay-load-transparency`, `zig build replay-game-reads-it`,
   `zig build remastered-game-reads-it`.
4. Quicksave markers, quickload rewind, other loads refused, the two texts in ten languages. Verify:
   `zig build replay-game-reads-it`, `zig build language-mods`, `zig build l10n-game-reads-it`,
   `zig build test-mods-paths`.
5. Ending: the automatic save, one per mission overwriting the last, Save Replay removed, the new
   header keys, the list's version check. Verify: `zig build test-replay-core`, `zig build replay-game-reads-it`.
6. Playback UI: speed, pause, fast forward, time line, markers, out-of-sync report; its texts in ten
   languages. Verify: `zig build replay-game-reads-it`, `zig build language-mods`,
   `zig build l10n-game-reads-it`, frames captured and measured by code.
7. Cross-platform determinism (required, decision 5; its own milestone): `-ffp-contract=off` for the
   engine; every floating-point function the AI and the simulation call (`sin`, `cos`, `atan2`, `exp`,
   `sqrt`, `pow`, `fmod` and whatever the audit finds) comes from one deterministic implementation in
   Zig, exported over the C ABI, bit-identical on x64 and arm64 and proved so by a test that compares
   result tables across the three platforms; `unordered_map` iteration and `std::sort` ties made
   order-independent where the AI depends on them; then the cross-platform job made gating. Verify:
   `zig build test-det-math` on all three platforms, `zig build replay-game-reads-it` on all three
   with a replay recorded on each of the others.
8. The 2003 replays (decision 4): a Zig tool `bk-replay convert` reads the original game's `.rpl` files
   and writes the new format, keeping the commands, map and mission; a replay recorded by the 32-bit
   2003 game is marked incompatible in its header (it cannot play identically: x87 versus SSE), and the
   replay list shows it marked so and does not play it. Verify: `zig build test-replay-convert` on
   replays from the Steam install on win-home copied to `tools/zig/fixtures/`.

Each slice runs the tiers above on Linux, then on win-home and mac-office before a push; the full sweep
is left to the maintainer.

### Slice 1 as built (2026-10-26, Linux x64)

Where the build differs from the text above, this wins:

- **Where the start state is taken and set.** `CICMission::PostCreate` calls `LoadCommandLineHistory`,
  which calls `PrepareToStartMission` itself; the second call right after it skips its block. So the
  start state is taken in that first call. A replay from the list (`Load` before the mission) sets its
  state in `PrepareToStartMission`; one from `-lh<file>` is read by `Load` *after* that call, so
  `LoadCommandLineHistory` sets it right after `Load`, at the same point of the mission start. `Load`
  itself still sets the zero seed, which a multiplayer replay and one without `StartRandomState` keep.
- **`StartRandomState`** is eight hex digits (a string, so a missing key is told apart from a zero
  state). `CCommandsHistory` reads the 32-bit state through `IRandomGen::Store`/`Restore`, which now
  write and read it as four bytes on a stream (they were empty). Written for single player only.
- **`Difficulty`** is the level the player chose (`CDifficultyLevel::GetChosenLevel`, without a script's
  cheat level). Single-player playback sets it and gives the viewer's level back on `Clear` or at the
  next mission that is not a playback.
- **The state hash** is `IAILogic::GetStateHash`, the data of `UpdateCheckSum` (units and shells) but a
  fresh CRC each call: `UpdateCheckSum` chains every CRC onto the one before, which would report every
  second after one difference. The single-player transceiver takes it after every 20th segment and hands
  it to `ICommandsHistory::AddStateHash`; multiplayer keeps its own check. Playback reports the first
  difference once ("Replay out of sync at mm:ss", chat and debug log).
- **`StateHashes`** in the `.rpl` is one string, `<segments since the previous>:<hash in hex>` separated
  by spaces (`20:1a2b3c4d 20:0 ...`). A hash at or after the segment a save is loaded at is dropped
  before the next one goes in (`NReplay::RecordStateHash`). The rules are `Sources/src/Main/ReplayCore.h`.
- **The save.** Chunk 14 holds chunk 1, the state (`RandomGen::operator&`); an empty chunk 14 (an older
  save) leaves the state as it is. The history's chunk 23 gained 11 (`stateHashes`), 12
  (`loadedStateHashes`), 13-15 (start state, difficulty, and whether they are known: a history from an
  older save writes no `StartRandomState`).
- **Found on the way:** reading a replay that holds a group command crashed a debug build:
  `CGroupCommand`, `CUnitCommand` and `CShowAreasCommand` left their members uninitialised and the data
  tree reader loads the old value of a bool before it overwrites it (UBSan: "load of value 85, which is
  not valid for type 'bool'"). The 2026-10-26 experiment gave no orders and never met it. They now start
  zeroed.
- **`-lh` is not the replay list.** `LoadCommandLineHistory` removes `History.Playing` after `Load`, so
  a command-line playback does not ignore the viewer's orders (the list's playback does). The tier gives
  none during playback.
- **Measured** (`replay-game-reads-it`, the test map above plus an order: a squad sent from one house to
  another): before slice 1 both playbacks matched 2 of the recording's 22 state hashes (the first
  difference at segment 60) and reported themselves out of sync, and two loads of one save matched 0 of
  10. After it: 22 of 22 in both playbacks, the 3 recorded commands played at their segments, no
  out-of-sync report, and the two loads 9 of 9. Both loads also matched the run that wrote the save going
  on (9 of 9): this small map is already load-transparent; slice 3 needs harder ones.

## Risks

- Load transparency may need many small fixes across the AI's serializers; the hash check per second
  finds where, not why. The fallback (open question 1) bounds the risk.
- A campaign mission depends on the scenario tracker (units and upgrades carried in). The replay keeps
  the start tracker and swaps it in; this has never been tested in single player.
- Template missions regenerate their map from the stored header and seed. If generation is not
  deterministic (it now seeds `rand()` and `NWin32Random` from `Random()`, `686fe5b84`), the fallback is
  storing the generated map hash and refusing a mismatch.
- Mods that change units or weapons change the resource hash; a replay is bound to its mod set.
- Fast forward runs many segments a frame; very long missions (an hour is 72,000 segments) need a
  "no drawing" mode to seek in seconds.
- File size: a typical mission has a few thousand commands and 3,600 hashes an hour; as XML that is
  some hundred KB, compressible.

## Decisions (Johannes, 2026-10-26)

1. Load transparency first: the random state goes back into saves so a loaded world continues exactly
   like the run that wrote it; the replay stores no quicksaves.
2. One automatic replay per mission, the latest attempt; no manual replay saves.
3. Regular saves during a recording are allowed; loading them is not, until the mission ends.
4. The 2003 game's replays get a conversion tool; those recorded by the 32-bit game are marked
   incompatible.
5. Replays are cross-platform: the simulation's floating-point maths comes from one deterministic Zig
   implementation shared by Linux, macOS and Windows (slice 7 is required).
