# The fastest path (Remastered)

The maintainer (2026-10-28): "We can add the fix for finding the fastest route. BFS with weighted paths should be
doable, as the weights are already in place, right?"

## What 2003 does

`CStandartPathFinder::CalculatePath` (`Sources/src/AILogic/PathFinderInternal.cpp`) walks a Bresenham line from the
start to the goal; where it meets a blocked tile it follows the wall (`CalculateSimplePath`, then
`CalculateHandPath`), `EraseCycles` cuts the loops out and moves the finish to the tile nearest the goal it reached,
and `LineSmoothing` straightens the result in three passes. It never looks at how fast a unit crosses a tile, so a
lorry drives through a garden beside a faster road, and a wall is followed the long way round as often as the short.

The weights exist: a unit's top speed at a point is `GetMaxPossibleSpeed() * (mapPass + (1 - mapPass) * unitPass)`
(`CCommonUnit::GetMaxSpeedHere`), with `mapPass` the terrain class's `fPassability` (tileset terrtypes, roads3) and
`unitPass` the unit's own `fPassability` (1 for infantry and guns, 0.11 down to -0.21 for lorries and cars: off the
road they are slower than the terrain says).

## Decisions

- **Behind Remastered** (confirmed by the maintainer). Off is the 2003 finder, exactly.
- **The search is Zig, the snapshot is C++** (the maintainer's decision). `fastest_path.zig` is a static library
  (`FastestPath`, ReleaseSafe in every build) linked into AILogic, with the C ABI `bk_path_find` (`FastestPath.h`). It
  is a pure function over a per-order snapshot: `CStandartPathFinder::Search` fills a grid of the tiles round start
  and goal, a cell holding the weight of entering the tile (0 where `CanUnitGo` fails for the unit's bound radius and
  AI class) and the point checking's good-tile bit; the search returns the tile path with each tile's cost. No
  callback crosses the boundary. The work arrays are the library's static buffers, sized for the largest AI map
  (1024 x 1024), stamped per search so nothing is cleared; nothing is allocated, so nothing can leak.
- **A***, 8-connected. A step costs its length (70 straight, 99 diagonal) times the weight of the tile it enters;
  the weight is 256 at full speed, `256 / speed` otherwise, clamped to 1..4096 (speed below 1/16 counts as 1/16).
  The speed is taken in fixed point from the floats once per request (`InitWeights`, `floorf(x * 1024 + 0.5)`; times
  1024 is exact, so no machine rounds differently), and from there every cost and comparison is an integer.
  Heuristic: the octile distance times the smallest weight on the grid, admissible and consistent. A diagonal step
  needs both tiles beside it free (no unit cuts a corner between two blocked tiles; the 2003 walk did).
- **Deterministic.** The open list is an indexed binary heap ordered by (estimate, heuristic, cell index), a total
  order; the neighbours are taken in a fixed order; the costs are integers. Nothing depends on pointers or on an
  allocator. Paths are computed where 2003 computed them: inside the simulation, when the order is applied
  (`CreateBigStaticPath`) and at 2003's own re-path points (the small paths, `RecoverState`), so every machine of a
  lockstep game computes the same path from the same order on the same segment; nothing is computed on one machine
  and kept local.
- **The 2003 contract is kept.** A unit on a blocked tile first leaves it the 2003 way (`LookForFakePathBegin`, its
  prefix stays in `stopPoints`); `upperLimit` caps the path's steps (a node at the limit is not expanded); the point
  checking ends the search on the first good tile it takes, counted only nearer the goal than the start, as
  `AnalyzePoint` did; a goal the unit cannot enter ends the search beside it (2003: `finishPoint = curPoint`); an
  unreachable goal gives the path to the reachable tile nearest it, and `finishPoint` moves there.
- **Window.** The snapshot covers start and goal plus a margin of 16 tiles and half their distance. Only when that
  search ends on the nearest tile (the goal not reached) and the window may have cut the way off is the search run
  again over every tile the step limit reaches. A small path (`upperLimit` 4 x its distance) never needs more.
- **Node budget: a safety net.** 2^18 expansions; a search that spends it is answered by the 2003 finder. The
  measured maximum on the shipped maps is below (none came near).
- **Smoothing, cost-aware.** `FastestSmoothing` is `LineSmoothing`'s first pass over the A* path with one more
  condition: a straight line replaces the path only where it is walkable and its cost (`LineCost`, the same weights
  over its Bresenham tiles) is not above what the A* paid between its ends. The second and third passes straighten
  the 2003 walk's wall following, which an A* path does not have, and the points they work on are re-drawn lines whose
  cost the A* never knew, so they are left out.
- **The unit's own passability** reaches the finder through `IStaticPathFinder::SetUnitPassability`, called by the
  two big-path callers (`CPathUnit` and `CFormationCenter::CreateBigStaticPath`) after `SetPathParameters`, which puts
  it back to 1. The small paths (a few tiles toward the big path, round other units) weigh every terrain the same:
  the big path has chosen the way, and `CStandartPath` does not know the unit (YAGNI).
- **Trains and planes** keep their own finders, untouched.

## Game state: saves, replays, multiplayer

Which finder plays is game state, decided once as a mission starts (`CAILogic::Init`,
`CStandartPathFinder::SetFastestPath( NRemastered::Simulation() )`):

- `NRemastered::Simulation()` (`Main/Remastered.h`): the option in single player, a replay's recorded mode while a
  replay plays (`History.Remastered`, as `Enabled()`), and in multiplayer the host's option. This changes the
  2026-10-25 decision "multiplayer always plays the original" for the simulation's own rules only: `Enabled()` stays
  off in multiplayer for everything else.
- The host's option travels with the game info (`SGameInfo::nRemastered`, packed after the 2003 fields, read only
  where the packet holds it: a host without it plays the 2003 finder) and every player sets it as
  `Multiplayer.Remastered` (`SetGlobalVars`). A multiplayer replay brings it back with the other `Multiplayer.*`
  vars; one recorded before has none, so it is removed before a history's vars are read.
- A save keeps it as `CAILogic` chunk 20, written only when on: a 2003 save, or one written with it off, plays on with
  the 2003 finder whatever the option says now; one written on plays on with the fastest path.

## Tests

- `zig build test-fastest-path`: the A* on fixture grids (`fastest_path.zig`): the road beside a field, a swamp, a
  wall, no corner cutting, an unreachable and a blocked goal, the bound radius (a unit of radius 1 takes the wide gap,
  radius 0 the narrow one), the step limit, the good tile, the budget, determinism.
- `zig build test-remastered`: `Simulation()`'s policy next to `Enabled()`'s.
- `zig build fastest-path-game-reads-it`, `fastest-path-passages-game-reads-it` and
  `fastest-path-maps-game-reads-it`: below.

## A replay from the command line

A replay played with `-lh` loads its history and then takes `History.Playing` off again
(`CCommandsHistory::LoadCommandLineHistory`): it plays as a live game, so `Enabled()` and the first
`Simulation()` read the viewer's option, and a replay recorded on was played with the 2003 finder (measured: the
march diverged after 4 s). It now sets `History.CommandLine`, and `Simulation()` takes a command-line replay for
a replay: its recorded mode. `Enabled()` is left as it was (outside this change).

## Passages: one vehicle at a time through a narrow place

The maintainer's decision (2026-10-28), a step after the A*: where a unit stands in another's way, it may move aside,
but only one passage is opened and every other unit takes the same; a narrow place (a bridge, a gap in a wall or a
wood, a street between houses) is an area with an entry and an exit; units wanting through wait at the entry in the
order they came (FIFO), and only one at a time is inside, going from entry to exit, so none is ever stuck. Found from
the static map, not marked by hand; deterministic; the queue is simulation state, saved and the same on every machine.

Built (`Passages.h`, `Passages.cpp`, simulation code in C++: it is game state):

- **Found on the path.** When a vehicle (not infantry, not aircraft) gets a new big path with the fastest path on
  (`CPathUnit::SendAlongPath`, through `FindPassages`), `CPassages::FindOnPath` walks its tiles and measures the way's
  width across each step on the static map (free tiles for the unit's AI class, units left out: the 2003 collisions
  move them). A tile is narrow where fewer than 2 (2r + 1) tiles are free across it, r the vehicle's bound radius:
  two such vehicles do not fit side by side. A run of narrow tiles is one passage; one where the path begins is the
  one the vehicle stands in and is skipped.
- **One passage per place.** A run whose tiles touch an existing passage's box is that passage (the box grows);
  otherwise a new one. All in `thePassages`, saved with the other global AI objects (chunk 29, written only once
  there is a passage), cleared with them.
- **The queue.** In `CPathUnit::SecondSegment`, before a vehicle moves, `MustWait`: within two tiles of its next
  passage (its body clear of it) it joins the passage's queue, ordered by the game time it arrived and then by unit id;
  it goes in when the passage is free and it heads the queue, and gives the passage back once its body has been in it
  and is out again. A vehicle that waits does what a stopped 2003 unit does (no speed, its tiles locked), so the
  others' small paths go round it. A new order leaves every queue (`Leave`).
- **Never stuck.** An occupant that dies, or holds a passage longer than 30 s of game time, gives it back
  (`CPassages::Segment`); a vehicle found inside a passage it never waited for (it stood there when ordered) is not
  held.
- **The squad in the way** is left to the 2003 rule that a vehicle pushes infantry aside (`CCollision::FindCandidates`,
  `bNeedPush`): the vehicles that follow wait in the queue, so they take the lane the first one opened. A lane cleared
  through units in open ground is not made a passage of its own (YAGNI: there the 2003 collisions let vehicles pass
  side by side).
- **Determinism.** Only integer tiles and game time decide; the queue's order is total (time, then unique id);
  passages are kept in a vector in the order found.

## Measured (win-home, Windows x64, debug install, 2026-10-29)

The search runs ReleaseSafe; the snapshot and everything round it are the debug C++ build, so the times below are a
debug Game's (the C++ snapshot dominates them).

**March** (`fastest-path-game-reads-it`, road3d rewritten: a garden of speed 0.3 across the straight way, an asphalt
ring round it; ms of game time until the last unit of a kind is within 6 tiles of its goal):

| Kind (unitPass) | 2003 finder | fastest path | way |
|---|---|---|---|
| lorry Opel_Blitz_Cargo (-0.18) | 33 750 | 18 500 | by the ring (11 tiles off its row) |
| car Opel_Kadett (-0.21) | 32 500 | 19 000 | by the ring (17 tiles off) |
| tank Pz_Kpfw_IV_Ausf_D | 22 200 | 21 850 | straight |
| squad German_rifle_41 (1) | 32 650 | 32 850 | straight (infantry: one speed everywhere; held within 2 %) |

Every unit arrived in both. Requests: 668 with the 2003 finder (mean 4 us), 543 with the fastest path (mean 270 us,
max 5.9 ms, at most 1 167 nodes expanded, no budget hit; 2 answered by the 2003 finder: start and goal on one tile).
The march recorded with Remastered on and played (`-lh`) by a player whose option is off: the fastest path, 31 200 of
31 200 unit samples (every unit's place every segment) the recording's; recorded off and played by a player whose
option is on: the 2003 finder, 31 200 of 31 200 the same. A save written on and loaded after the player turned the
option off plays on with the fastest path (`loaded finder fastest=1`).

**Passages** (`fastest-path-passages-game-reads-it`, road3d with two swamp walls, each with one gap of 4 AI tiles,
three lorries and two tanks ordered through both, a rifle squad standing in the first gap):

| Vehicle | 2003 finder | fastest path with passages |
|---|---|---|
| lorry 1 | 51 450 | 51 950 |
| lorry 2 | 53 250 | 49 350 |
| lorry 3 | 37 200 | 40 550 |
| tank 1 | 34 100 | 59 100 (waited its turn twice) |
| tank 2 | 43 300 | 37 750 |

All arrived; both gaps were found as passages; 0 entries while a passage was taken, 0 timeouts, entered in the order
of arrival (passage 0: units 6, 1, 43, 41, 39, 1; unit 1 came back to it once on a new path and queued again).
Waiting costs the vehicle that waits (tank 1); nobody is stuck. With the 2003 finder the same column also arrived on
this map: the measure here is the one-at-a-time rule, not a rescue.

**Shipped maps** (the missions' own AI orders, 60 s of game time, Remastered on, BK_PATH_TRACE):

| Map | requests | mean | max | nodes expanded, max | budget hits |
|---|---|---|---|---|---|
| kursk | 4 539 (3 426 big paths) | 460 us | 36.3 ms | 92 600 | 0 |
| dessau | 63 (41 big) | 643 us | 15.0 ms | 454 | 0 |
| vitebsk (by hand, see below) | 1 774 (1 285 big) | 580 us | 26.4 ms | 56 090 | 0 |

The budget (262 144 nodes) was never reached; the most a search took was 35 % of it (an unreachable goal searched
over every tile the step limit reaches). Requests the 2003 finder answered: start and goal on one tile (kursk 87).

## Left open

- **vitebsk leaks** AILogic blocks at exit with either finder (3 with Remastered off, 12 on, in one run each): the
  tier plays kursk and dessau only; the leak needs its own hunt.
- **A bridge.** The passages tier uses two gaps in walls; no tier places a working bridge over a river (a bridge's
  spans and their link list in a written map are not yet known to work); a bridge is a narrow place by the same
  measure.
- The small paths (a few tiles toward the big path) weigh every terrain the same.
- macOS and Linux are left to CI.
