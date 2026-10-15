# S04/T05 — the known residue, with reasons

Every number below is a BkMemory detach report from a graceful close, parsed by
the owner-cluster rule: a block belongs to the first stack frame that is not
allocator/STL plumbing (`bk-mem`, `new_delete`, `/std/`, `xmemory`, `xstring`,
`xtree`, `vcruntime`, unresolved `???` modules). Windows captures carry full
stacks (the record mutex in `BkMemory` serializes SelfInfo); Linux defaults to
0-frame counts (D086).

## Verdict

`LeakObjectsOnExit` is retired: no live call site remains
(`rg -n LeakObjectsOnExit Sources/ build.zig` matches only comments). The
registries release in owner order everywhere the engine tears down:

- the C++ `CSingleton` (`StreamIO/Singleton.cpp`) releases in reverse
  registration order from `Done()`;
- the zig bridge's `Singleton` (`StreamIOZig/legacy_bridge.cpp`) releases its
  entry array in reverse index order and clears the Zig global-variable map
  (`bk_global_clear`) last;
- the editors call that `Done()` at the last `BkEditorStop`
  (`EditorBridge/bridge.cpp`, session counter);
- the Game calls it at `NMain::Finalize` → `UnloadAllModules`
  (`Main/LoadDLLs.cpp:124`), and the loader then runs every module DLL's
  static destructors in reverse load order — dependents before providers.

The counts that the shutdown order actually moved:

| tier | armed-leak era | after the safe shutdown order |
| --- | --- | --- |
| resource-editor-host-check | 45,395 | 1,692 |
| map-editor-host-check | 45,395 | 2,125 |

(The Linux CI run on `f3082a2` printed 6,364 for the Resource Editor because
the bridge's reverse-order `Done` was written but missed in the T04 commit;
`c697a5199` carries it. CI at or after `139307fca` measures the fixed tree.)

## The Game's 15,574 blocks: process-lifetime state, held by design

The Game (60 s at the menu, graceful close, exit=0) reports 15,574 blocks /
1.9 MB. A/B evidence: with and without the bridge's reverse-order `Done`, the
report is block-for-block, byte-for-byte identical (15,574 = 15,574; every
family identical). The Game's singletons live in the module DLLs' own C++
registries and in cross-DLL `CPtr` caches, so the bridge release — which is
what the editors exercise — touches nothing of theirs; and what the loader's
reverse-order static teardown does release still leaves these families, whose
refcounts do not reach zero (static caches release in neighbor DLLs, and
`CLinkObject` graphs and interface screens hold cycles).

The families, first engine frame per block:

| blocks | bytes | owner | what it is |
| --- | --- | --- | --- |
| 9,522 | 562,279 | `Main/UserProfile.cpp:142` | the config read path: binds, sections, double-clicks, powers loaded into `CInputBinder::config` at startup (`Input/InputBinder.cpp:576`); engine-lifetime input state |
| 2,675 | 763,157 | (no resolved frame) | same mass, inner frames unresolved (STL/allocator only) |
| 1,042 | 190,824 | `Main/iMainInternal.cpp:402` | `IScene()->Clear()` at `ResetStack`: scene/effect/link-object registries |
| 874 | 69,958 | `Main/Initialization.cpp:94` | `pInput->Init()`: the input system's startup allocations |
| 568 | 46,830 | `Game/GameMain.cpp:948` | `IFontManager()->GetFont("fonts\\medium")`: glyph/texture storage |
| 297 | 49,687 | `StreamIO/StructureSaver.h:82` | the structure-saver type registry |
| 232 | 206,961 | (SDL3.dll) | SDL's own storage; `SDL_Quit` frees what it owns, the rest is SDL-internal |
| 360 | ~47,000 | tail | cursor, SFX, option-system strings (`iMainInternal.cpp:873/943/968`, `Initialization.cpp:150/156/188`, `InputHelper.h:19`, `GameMain.cpp:859/925`) |

Reason on record: this is the class the engine's own authors solved with
`LeakObjectsOnExit` — the original `Basic.h` guard comment said the choice was
deliberate. Freeing it means either breaking the cross-DLL `CPtr` caches into
explicit release phases (an engine-wide refactor out of S04's scope) or
walking the `CLinkObject` cycles by hand (the registries have `Clear()`, but
clearing them at a safe point is menu-lifecycle work, M006-adjacent). The
report now names real owners; the mass is engine state, not lost memory.

## The editors' remaining residue

Resource Editor 1,692 / Map Editor 3,541 blocks (win-home, T04-era captures):
the same shape at smaller scale — registry entries whose refcounts are held by
the editor-host's own cached `CPtr`s past the last `BkEditorStop`. The
host-check tiers pass with these counts printed as warnings; folding them
means the same cross-DLL cache work as above.

## Method notes

- `parse-t05.py` (workspace temp) clusters by first non-plumbing frame; its
  file:line regex accepts drive-letter paths (`C:\...` broke the first draft —
  every local Windows capture parsed as `<no frame>` until fixed).
- The A/B Game capture pair: `zig-out/local-test/tier-logs/s04-t04-game.err.log`
  (pre-`c697a5199` build) and `s04-t05-game.err.log` (post). Identical reports.

## T08: the residue eliminated at the root

The BK_REF_AUDIT census (BK_REF_AUDIT=1 at registry Done) and the
BK_REF_TRACE transition trace named the second owners; fixing them collapsed
the Game's detach report from **15,574 blocks / 1.92 MB to 227 blocks /
~210 KB** (98.5% blocks):

| fix | what it released |
| --- | --- |
| the two emergency commands are no longer constructed into the no-op NBugSlayer::AddEmergencyCommand sink (iMainInternal.cpp) | the dropped commands pinned CMainLoop, whose CPtr<IInput> members held the input binder and its 9.5k bind-string blocks; the audit's 15 surviving singletons all fell to zero |
| the bridge's core services die at registry Done (legacy_bridge.cpp, BridgeCoreServicesTeardown) | console_buffer/option_system bridge-owned references, and the unregistered save_load_system - its factory database was the largest single block at 532 KB |
| GlobalVars, ConsoleBuffer, RandomGen get real refcounting | the no-op AddRef/Release pairs made the 532,632-byte wide-variable table (and the other two) survive every release |
| NMain::Finalize releases the engine's current font, clears the shared managers, and shuts SDL down through the NMain::SetApplicationShutdown slot | the 564-block glyph family (the engine's current_font_ and the managers' maps pinned the font past the report) and SDL's own allocations (206 KB; SDL_Quit used to run at static destruction, after the report) |

The remaining 227 blocks: ~220 SDL-internal allocations that survive
SDL_Quit by SDL's own design (the shutdown provably runs), plus seven
named transients on the config read path (Initialization.cpp:102,
GameMain.cpp:859, iMainInternal.cpp:965) and six plumbing-only
unresolved-frame blocks. The editors keep their own floors (1,206 /
1,614 on the refreshed build).

Method notes for the next session: header-only edits do not invalidate
every module's compile - run game-all and then install-game, in that
order, and bump the input module's BK_INPUT_BUILD_INCREMENT if a
header edit fails to reach Input.dll. The masked refcount planes
(REF_ADD_OBJ = 0x1000, Basic.h:462) mean raw refcount numbers above
the plain plane are internal claims, not holders.


## T08c blocker: the Linux stack capture records nothing

The Linux Resource Editor's ~3,472-block floor (CI band 3,472-4,184; the WSL
clone reproduces 3,101 with host check PASS) has no family table: every
attempt to capture stacks on Linux printed (empty stack trace) for every
block - with the platform default and with -Dbk-mem-stack-frames=24 
explicitly (the option's plumbing in bk_memory.zig:55-70 is correct; the
SafeAllocator receives 24). D086 set the Linux default to 0 for CI cost
after the unwind burned ~70% CPU; the capture path itself appears dead on
this zig version. Naming the Linux families (are they the same engine-side
families as Windows, or Linux platform layers?) needs this fixed first -
it is the gate for T08c on Linux. Provisional answer until then: the
editors' residue is the same engine-side families measured on Windows
(the BkEditorStop path lacks the Game's Finalize ordering - the managers'
Clear and engine-font release), plus a Linux platform delta (SDL's Linux
backends allocate more than Windows') that the Windows captures cannot
see. The direct-run capture recipe that works otherwise: run the staged
ResourceEditor directly with --check wpn <tga> <fixture> (the argument
order matters), BK_MEM_REPORT=log; the tier step overrides the report
mode to count, so the direct run is required for stacks.

## T08c on Linux, resolved in two steps (2026-10-14)

1. **The stack capture needs the explicit option.** The blocker above stands:
   D086 sets 0 frames on Linux (the shared unwind state SEGV'd under CI,
   38307115498 - a foreign-ICD driver thread unwinding beside our captures),
   so the counted tiers record no stacks and the default report prints
   "(empty stack trace)" for every block. A Linux investigation takes the
   recorded recipe: rebuild with `-Dbk-mem-stack-frames=24`, run alone in the
   process with `VK_DRIVER_FILES=lvp`. That produces full per-block frames -
   BkMemory's own resolve to file:line, engine `.so` frames by module and
   address (resolvable offline). One capture race remains under contention
   (a single block printed empty on a unit-tier run); it hides one stack,
   not the count.
2. **The Game's Finalize ordering moved to `BkEditorStop`.** The last
   session's stop now runs `SetFont(0)` and the font, texture and mesh
   managers' `Clear` before the registry `Done()`, as `NMain::Finalize` does
   for the Game. Measured on Linux (MapEditor --check, real GPU, one run):
   **35,507 blocks / 76.7 MB before, 8,549 after** (-76%); the tier prints
   8,549 per host-check run and stays green. The Linux families before the
   fix (owner-cluster rule, first non-plumbing frame):

   | blocks | bytes | owner |
   | --- | --- | --- |
   | ~5,034 | ~766 KB | SDL's own storage (`SDL_malloc.c`) |
   | 1,326 | 34 KB | the engine's `2Darray.h` (map/terrain tiles) |
   | ~680 | ~48 KB | `RPGStats.h` (the objects DB read) |
   | ~608 | ~57 KB | `Initialization.cpp` (the config/input read path) |
   | 477 | 31 KB | `Globals.h` (global variables) |
   | 204 | 45 KB | `EditorBridge/session.cpp:88` |
   | ~148 | ~53 KB | `fmtSprite.h` (sprite formats) |
   | ~78 | 45 KB | `GameDB.cpp` (the object DB) |

   The managers' Clear released most of the first four engine-side families
   with the font's; what remains is SDL plus the registries whose release is
   the cross-DLL cache work out of S04's original scope.

   Post-fix partial table (Linux, one run, 24-frame capture per the recipe
   above; bucket-ordered report, 3,088 of 8,553 blocks symbolized before the
   run's leash - the shares are directional, the counts above are exact):

   | blocks | bytes | owner |
   | --- | --- | --- |
   | 1,494 | ~259 KB | SDL's own storage (`SDL_malloc.c`, three sites) |
   | 149 | ~137 KB | thread descriptors (`pthread_create.c:454`), spawned-and-unreaped worker threads - the per-cycle suspect |
   | 4 | ~7 KB | `EditorBridge/session.cpp` |

   The engine registry families of the pre-fix table (RPGStats, Globals,
   GameDB, `2Darray.h`) do not appear in the symbolized set: the Finalize
   ordering dissolved them.

## The unit-tier single-block leak, found and fixed (2026-10-14)

CI's Linux "Map editor unit tier" failed with `1 leaked block(s) at exit`
(149/149 tests passed, exit 3): the hung-owner test's hand-off times out and
abandons its `HandoffJob`, whose worker destroys the job only when the peer's
listener closes - a scheduling race with BkMemory's report, so about one run
in twenty exited 3 (reproduced locally at that rate). The job now counts
itself in `HandoffJob.outstanding`, and the test drains it after its own
cleanup, asserting zero. The panels tier itself is green on Linux with 0
leaked blocks.


## The per-cycle residue is zero (Linux, 2026-10-15)

`BK_EDITOR_CYCLES=N MapEditor --check <map>` runs N times File > Close then
File > Open through the panels' own `act` in one session, a frame on each
side, and prints BkMemory's live counters after every close. Debug build,
default stack frames, real GPU, coldwinter:

```
cycle 1: live 37645 blocks 86757455 bytes (delta 0 blocks 0 bytes)
cycle 2..8: live 37645 blocks 86757455 bytes (delta 0 blocks 0 bytes)
```

Verdict: 0 blocks and 0 bytes per cycle, flat from the first close, not a
trend that settles. A 40-cycle run (a day at 40 tests) stays flat the same way
and exits with 8,547 blocks, as 8 cycles do (8,547) and no cycles do (8,553,
the usual jitter). The pthread_create family (149 blocks) does not grow with
cycles: it belongs to the engine start, not to a map. What the cycle does not
cover: a test launch's Game runs in its own process, so its memory is never
the editor's; a second `Host.start` after `Host.stop` is unsupported by
contract (`kit/host.zig`), so the 8,549 at exit is paid once per process.


## The editors' cache residue, T08's method on Linux (2026-10-15)

Map Editor host check, Linux debug build, RADV (AMD Renoir), coldwinter:
**8,549 blocks / ~2.65 MB -> 2,651 blocks / 168 KB**. What is left is
2,640 blocks of Mesa's libLLVM static initializers plus 11 small
process-lifetime blocks. SDL's own floor turned out to be zero here: the
"~5k SDL" of the earlier tables was a GPU device nothing destroyed.

### Census

`BK_REF_AUDIT=1 MapEditor --check <coldwinter>` on the starting tree:

```
ref-audit: tid=31 refs=2 type=17CScenarioTracker2
ref-audit: tid=8 refs=2 type=10CGameTimer
ref-audit: tid=5 refs=2 type=10CObjectsDB
```

`BK_REF_TRACE` covers only `CTRefCount` classes. `CGameTimer` and
`CObjectsDB` use `OBJECT_NORMAL_METHODS`, whose own AddRef/Release don't
trace, and on Linux the tracer prints raw caller addresses. So the
transitions came from gdb instead: `rbreak` on the three classes'
AddRef/Release, `bt 10` per hit, 4,718 hits, paired per `CPtrBase this=`
holder. Every holder nets to zero. The second owners are the AI module's
globals: `updater.pGameTimer`, `theUnitCreation.pIDB` and
`theStatistics.pScenarioTracker`. They release from
`__run_exit_handlers`, which runs **before** BkMemory's report, so the
three survivors were never in the leak count. A registry survivor means
"held past Done", not "leaked"; measure before you fix.

The family table needs the stack recipe (`-Dbk-mem-stack-frames=24`,
`BK_MEM_REPORT=log`). On RADV it ran without a capture fault: 7,549
stacks in 4.5 minutes.

### Fixes, one per commit, host-check count before -> after

| commit | fix | blocks |
| --- | --- | --- |
| `2e4134eed` | the last `BkEditorStop` runs `IAIEditor::Clear` before the managers' Clear: no stop path cleared the AI's per-map state (global objects, link-object and command registries), so it outlived the deleted world | 8,549 -> 7,549 (the `-mod` run 7,271 -> 7,271) |
| `d5c3573d2` | the last stop runs `IGFX::Done` after the managers' Clear, before the registry Done. Nothing destroyed the renderer (`~GraphicsEngineGpu` does not call `Done`), so the SDL GPU device's command buffers, buffer containers, memory regions, fences and descriptor caches (4,829 blocks in `SDL_gpu_vulkan.c`) reached the report. Textures and buffers the registry releases afterwards find `renderer_` null and do nothing | 7,549 -> 2,652 (the `-mod` run 7,271 -> 2,652) |
| `9d158451a` | the bridge registry's Done frees StreamIO's temp buffers (`bk_streamio_temp_buffers_free`). They only grow, and the AI's new-objects list (`CUpdater::GetNewStaticObjects`) held one 1,586,448-byte block | 2,652 -> 2,651; 1,754,912 -> 168,464 bytes |

Tried and dropped: a `CStatistics::Clear` called from
`NGlobalObjects::Clear` cleans the audit to zero survivors but measured
7,549 -> 7,549, because the static destructor already released the tracker
before the report. Dropped as a no-op. (Its first draft defined `Clear`
inline in `Statistics.h`, where `IScenarioTracker` is only declared. That
picks the `void*` `CastToRefCountImpl` overload, which nothing exports,
and libAILogic.so stopped loading. **The host check still passed, with no
AI**, at a false 3,783. A count drop needs the log checked for
`Failed to load module`.)

### Where this stops, and why

Final census (24-frame capture after the three fixes, 2,651 blocks):

| blocks | bytes | owner | why it stays |
| --- | --- | --- | --- |
| 2,640 | ~153 KB | `libLLVM.so.21` static initializers (`dl-init.c call_init`) | Mesa's Vulkan driver loads LLVM. Its constructors allocate through our interposed `operator new`, and its own static destructors free them after the report, if at all. Foreign driver state |
| 6 | ~260 B + 6.7 KB | AILogic globals, constructed empty by the AI's `Clear` (`session.cpp:358` and the stop's own Clear) | the module's static storage, freed by its static destructors |
| 1 | 8,216 | `EnsureCoreServices` (`legacy_bridge.cpp:1796`) | the bridge's core-service storage, process lifetime |
| 2 | 186 | the options bridge's core-module handle and path (`options_bridge.cpp:105`, `DynamicLibrary.cpp:42`) | process-lifetime module handle |
| 1 | 160 | `main.zig:179`, the argument list | `main` leaves through `process.exit` |

No registry entry outlives Done any more: BK_REF_AUDIT prints nothing
after the fixes, beyond the tracker that dies before the report. The next
owners are process-lifetime statics or a foreign library, not cross-DLL
second owners, so the method has nothing left to break.

Reopen contract: `BK_EDITOR_CYCLES=1` and `=8` on the final build print
`delta 0 blocks 0 bytes` on every cycle and exit with 2,651. Like the
managers, IGFX is not re-registered by `BkEditorStart`. A stop followed by
a second start was already unsupported (`kit/host.zig`), and no host does
it.

Resource Editor (shares the bridge): `BK_REQUIRE_ENGINE=1 zig build
test-resource-bridge -Dtarget=x86_64-linux-gnu -Dtest-mode=run` passes on
the final tree with 2,648 leaked blocks and no module-load failure. The
full resource sweep is left to the maintainer.

Open observations, not chased:

- Under lavapipe (`VK_DRIVER_FILES=lvp`) the same run reports 723 blocks
  both before and after the GPU-device and temp-buffer fixes.
- The Game has the same two gaps: `NMain::Finalize` never calls
  `IGFX::Done`, and the temp buffers are freed only by the shared registry
  Done that now does so. The Game's 227 were measured at the menu on
  Windows/D3D12, which never grew the AI temp buffer. Its device
  destruction is not in this slice.
- The host check passes when an engine module fails to load (see the
  dropped fix above), and nothing in the tier asserts the AI is present.
