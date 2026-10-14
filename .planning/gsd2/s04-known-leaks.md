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

