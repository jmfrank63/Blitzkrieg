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
- `zig build fastest-path-game-reads-it` and `fastest-path-maps-game-reads-it`: below.
