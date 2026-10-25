# The Remastered gameplay option

One switch separates the 2003 game from the features the port adds that the
original did not have. Options > Settings > Gameplay > **Remastered**, off by
default: off plays the original game's rules, on plays Johannes's extended
version. Every non-original feature lives behind it (the who-is-inside strip
first, camera angles and others later).

## Decision (2026-10-25, Johannes)

- Off is the original game, exactly: a player who never touches the option
  plays Blitzkrieg as it was.
- On is "features the original game did not have". Fixes of the port's own
  defects are not features: they stay on in both modes.
- One global var, one helper. The option's action (`SetRemastered`,
  `Sources/src/StreamIOZig/legacy_bridge.cpp`) writes
  `Options.GamePlay.Remastered` (1 or 0) whenever the option changes and once
  at startup; every feature asks `NRemastered::Enabled()`
  (`Sources/src/Main/Remastered.h`) and nothing else.
- Multiplayer always plays the original: `Enabled()` is false while
  `MultiplayerGame` is set. Every player's AI runs the same commands in
  lockstep, so a rule that changes what an order does must be the same on
  every machine; the original rules are the ones every client has, so no
  setting has to travel with the game settings and an older client stays
  compatible.
- A replay (`CCommandsHistory`) records the mode as `Remastered` and plays
  back in it (`History.Remastered` while `History.Playing`); a replay written
  before the option has none and plays as the original.
- The option is on the main options screen only (Flags 1, like Language), so
  a mission runs in one mode from start to end. A save carries no mode: it
  loads in whatever mode the player has chosen (below).

## The first feature: the who-is-inside strip

Commits 9ca19c2f5, 18c8ae216, 0a778e9b4, e38ac8f24, a8c18d671 and f3f89112c
mixed port fixes with new behaviour. The original is the code before them
(`git show 9ca19c2f5^:<path>`, the same as the 2003 import 7f0f4be5c).

Kept in both modes (port fixes, save fixes, test seams):

| Change | Where |
| --- | --- |
| The passenger-change message matches its container whatever the pointer width (the strip refreshes in real time) | `CSelector::UpdateSelection`, `MOBuilding.cpp` |
| Action masks above INT_MAX wrap the way they did through int; `IsActiveBit` reads full width (the exit button is back) | `LuaLib/Script.h`, `UI/UIBasic.cpp` |
| A strip element's width is what it drew, not its x-origin plus that (clicks reach the cell under the cursor) | `CUISquadElement::Visit` |
| After a load every visual re-applies its selection state to its icons (icons save their colour) | `CWorldClient::ReconcileSelection` |
| `CFormationLeaveTransportState` stays registered and serializes, so a save that holds one loads in either mode | `AILogicObjectFactory.cpp`, `FormationStatesSerialize.cpp` |
| The BK_AI_TRACE lines, the harness's held cursor for `click=`/`rclick=`, `world=` and the TraceSelection / TraceContainers dumps | `GameMain.cpp`, `WorldClient.cpp` |

Behind the switch:

| Original (off) | Remastered (on) |
| --- | --- |
| The strip shows for one selected container only | It stays up while the selection is passengers of one container (`StripContainer`) |
| A cell click toggles its element: selects that squad (through its first soldier, the squad selected as a whole) or deselects all its soldiers; the strip closes with the new selection | A plain click selects every occupant; shift-click toggles the clicked soldier's squad; deselecting the last squad hands the selection back to the container |
| Squadless soldiers share one element | Each squadless soldier has a cell of his own, and a squad dissolving inside a container rebuilds the strip |
| Every cell draws the same | Selected cells are bright, the others a little dimmer |
| A passenger's HP bar on a container follows the container's selection (icon ids below 10100) | It follows the passenger's own selection (ids below 10000 follow the container) |
| An occupant offers LEAVE only when his own actions have it | Occupants always offer LEAVE, and it sends the selected occupants to the clicked spot |
| A transport's load is unselectable | The player's own load is selectable |
| A squad aboard a transport ignores MOVE and LEAVE (MOVE frees its flag while it stays aboard) | MOVE and LEAVE land it through `CFormationLeaveTransportState` |
| An order into a building is refused by a squad aboard a transport | It gets out at the transport's entrance and goes in, in one order |

A change of building from a building's occupants is original behaviour and
works in both modes: `CFormationIdleBuildingState` puts a LEAVE before an
order into another building (measured: the squad picked in the strip, or all
twenty occupants with Remastered, end up in the other house). What did
nothing was the same order from a truck's passengers, which is now behind the
switch. A queued order (shift held on the order click) is refused by a squad
in a building in both modes, as in the original.

## Saves and the AI state the switch decides

The one piece of AI state the mode decides is a transport's load's
selectable flag, which a save keeps. `CAILogic::SyncTransportLoadSelectable`
applies the loading mode's rule when a save is read (it replaces the
unconditional fixup 0a778e9b4 added) and on the first segment after the mode
changes: off flags every soldier aboard unselectable, on makes the player's
own selectable. Everything else the switch decides is read at the moment an
order or a click happens, so a save written in one mode plays in the other.

## Tests

- `zig build test-remastered`: the policy (off by default, off in
  multiplayer, a replay keeps its mode) through the global vars.
- `zig build remastered-game-reads-it`: the real Game on a map written at test
  time from `Data/Maps/river3d.xml` (two houses, two squads in one, a third in
  a truck), off and on, measuring the selection and who sits where through the
  TraceSelection and TraceContainers dumps; and saves written in one mode and
  loaded in the other.

## Notes

- The Game looks for `defconf.cfg` relative to its working folder
  (`ResolveConfigFileName`); a Game started from a scratch folder has no
  option actions unless the file is beside it, so the tier copies it there.
