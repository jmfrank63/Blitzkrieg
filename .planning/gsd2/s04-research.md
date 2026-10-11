# S04 Research: what the leak reports hold

**Source:** one `resource-editor-host-check` run (21 fixtures' worth of engine use is one project open
plus the docks' operations; the tier leaks 26,115 blocks at exit in count mode). The stacks came from a
direct staged `ResourceEditor --check wpn` run with `BK_MEM_REPORT=log` on WSL Ubuntu 24.04 (dzn ICD),
interrupted at 540 s with 7,448 blocks and their stacks parsed of the full 26k — the families repeat, so
the sample is representative. Log: `~/s04-host-check-stacks.log` (host-local), analyzed copy and the
clustering script's numbers below taken 2026-10-11.

## Families (by share of the parsed sample)

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