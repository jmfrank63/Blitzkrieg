# S04 Research: what the leak reports hold

**Source:** one `resource-editor-host-check` run (21 fixtures' worth of engine use is one project open
plus the docks' operations; the tier leaks 26,115 blocks at exit in count mode). The stacks came from a
direct staged `ResourceEditor --check wpn` run with `BK_MEM_REPORT=log` on WSL Ubuntu 24.04 (dzn ICD),
interrupted at 540 s with 7,448 blocks and their stacks parsed of the full 26k — the families repeat, so
the sample is representative. Log: `~/s04-host-check-stacks.log` (host-local), analyzed copy and the
clustering script's numbers below taken 2026-10-11.

## Families (by share of the parsed sample)

**Windows capture (2026-10-13, T02's final tables: direct staged runs with BK_MEM_REPORT=log on
win-home, debug build; logs zig-out/local-test/tier-logs/s04-t02-{resource-editor,map-editor,game}.err.log):**

| Process | Blocks | Bytes | The mass |
|---|---|---|---|
| ResourceEditor --check wpn | 44,232 | 74.6 MB | 21.9k×32 B + 10.5k×48 B + 6.4k×80 B + 5.1k×64 B: **the string/map-node family (DataTree AddStringData + the std containers)**; the bk backing FixedBufferAllocator 487 blocks = 70.2 MB (the engine's big buffers, 95% of the byte mass); SDL3 third-party 213 blocks; WorldBase.cpp:203 1,042 blocks (190 KB); Initialization.cpp 2,459 blocks (startup singletons); StructureSaver.h:82 346 blocks |
| MapEditor --check coldwinter | 56,059 | 77.9 MB | the same shape: the strings/nodes mass + the backing buffers 493×70.2 MB + SDL3 1,399 blocks + Lua lmem.c:100 31 blocks |
| Game (60 s at its menu, graceful close) | 1,224 | 127 KB | 1,221× the same small-alloc family + 3 backing blocks |

The count-mode baselines (S02 rows of the tier-results: 45,395 host-check / 52,655 smoke / 136k-171k
the game-bearing tiers) agree with these numbers - the direct runs replicate the tiers' counts, so
the log mode parses the same population.

**Walk caveat (recorded, not blocking):** the Windows stacks come out one-frame for the large
majority of blocks (38307115498's SEGV cousin: std.debug's shared SelfInfo unwind state bails via
switch_to_fp, gated by allow_unsafe_unwind=false; the record mutex in bk_memory prevents the worst
(shared-state SEGV) but the walk depth stays short). The family attribution stands on: the WSL
capture's DTHelper.h:91 AddStringData name (3,758 blocks) + its no-frame sibling (3,442), the same
37-72 B sizes; and the size histograms here. T02's owners: the DataTree's character data + the
engine's std container nodes (T03's fix target), the World's per-scene state, the startup
singletons (Initialization.cpp), the saver's records, SDL3's own caches.


| Family | Allocs | Bytes | Shape |
|---|---|---|---|
| `StreamIO/DTHelper.h:91 AddStringData<char>` | 3,758 | 199,168 B | Small strings, 37-72 B each; the DataTree's character data from project load and export |
| no frame resolved | 3,442 | 193,286 B | Same sizes; stacks end inside the allocator plumbing — C/Zig callers whose frames do not symbolize in this report |
| `SDL_malloc.c:6459/6478/6496` | 214 | 47,9xx B | SDL's audio/stream/cache allocations made through the hooked allocator |
| `Common/WorldBase.cpp:203` | 16 | 13,120 B | One 820-B block each: the World object's per-scene state |
| other engine sites (SSHelper, fmtAnimation, resource_bridge, Globals) | 8 | ~0.5 KB | One-off residues |

The counts are stable across runs (S02's record: 43-44k per Resource Editor tier every run) — this is
*process residue*, not growth: allocations the engine intentionally keeps alive at exit (LeakObjectsOnExit,
CLinkObject registries) plus data freed by neither.

## What the design doc already says

- safe-allocator-design.md, "the Game's third-party leaks" section: the detach report will name engine
  objects freed by LeakObjectsOnExit and the CLinkObject registries the moment the engine routes through
  BkMemory; S04 must free them in a safe shutdown order instead.
- The report's `<no frame>` blocks need a Windows debug run with stacks to attribute (the Linux report's
  frames end in the allocator; Windows' CaptureStackBackTrace walks more reliably) — a planning task, not
  an unknown.

## Implications for the plan

- The two string families (7,200 allocs, 392 KB) are one fix if the `<no frame>` shape is confirmed as
  the same call path: the DataTree strings need an owner that drops them at shutdown, not per-leak fixes.
- The World/SDL/engine one-offs are small in bytes but the counts are what the report prints; a safe
  shutdown order (S04's "LeakObjectsOnExit is gone; CLinkObject registries are freed") addresses them
  together.
- An empty report needs the report itself to stop tripping on the detached threads' residue: the WR-A02
  note in single_instance.zig (hand-off jobs from the process allocator can outlive the caller) is one
  known block that a late exit cannot attribute deterministically; its fix belongs with the sampling
  (accept or re-home) decision in the design doc.

## The string family's owner mechanism (T03/T04 decoded, 2026-10-13)

- DTHelper.h:91 AddStringData is the tree's read path: `pData->resize(n)` in the objects' own
  std::string/std::map-node allocations. The tree itself (CDataTreeXML, MSXML DOM) stores nothing -
  **the owner of the string mass is what was loaded from the tree**: the object DB and the interfaces'
  loaded state, held by process-lifetime singletons and the CLinkObject registries.
- The old hack: NRefCount::LeakObjectsOnExit (Misc/Basic.h:31). CTRefCount::Release stops deleting
  (if LeakObjectsOnExit: return) once the flag is set at teardown (set in Main/MainLoopCommands.cpp;
  the shutdown paths in SceneInternal/UIInternal/GameMain/iMissionInternal/AILogicInternal consult it) -
  from that point objects are deliberately leaked because the registries outlive the destructors.
- The fix shape (per the design doc's safe shutdown order): free the CLinkObject registries and the
  interface singletons in owner order before the allocator detaches, then remove
  LeakObjectsOnExit from Release. That also drops the string family and the World/UI/DB residues
  together, which is why T03 folds into T04.