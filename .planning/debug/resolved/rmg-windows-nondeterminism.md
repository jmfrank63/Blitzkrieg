---
status: resolved
trigger: "CI run 37029035801, windows-platform (MSVC/UCRT x86_64), test-editor-bridge: Create Random Map determinism checks fail (editor_bridge_test.cpp ~13451 'the same seed regenerates a byte-identical map', ~13476 'the seed a blank draw reported regenerates that map'). Pass on macOS arm64/intel, Linux x86/arm, windows-mingw."
created: 2026-10-02T18:10:00Z
updated: 2026-10-02T19:25:00Z
---

## Current Focus

bug_class: Bohrbug in the code (uninitialised bytes persisted), Heisenbug in its symptom (the garbage's value depends on platform stack/heap state)
reasoning_checkpoint:
  hypothesis: "The regenerated map differs because SVectorStripeObjectPoint's 3 implicit pad bytes after bKeyPoint are never written (the ctor initialises members only), CVSOBuilder::SliceSpline builds each point on its stack, trivially-copyable copies carry the pad bytes into the vector, and CSaverAccessor::DoDataVector writes the vector as raw bytes - so whatever the stack held there lands in the BZM; on windows-msvc those leftovers change per run."
  confirming_evidence:
    - "win-home full diff: 1487/1487 (same seed) and 894/894 (blank draw) differing bytes sit at offset 33..35 of 40-byte records in the roads3 points raw chunk; nothing else differs"
    - "macOS maps carry non-zero pad garbage too (cf4701, ff1701, 1c4801), merely repeatable there"
    - "new map-file test on win-home without the fix: a point placement-built over 0x00 vs 0xff memory differs; the same road built after two stack fills differs in bytes and in the saved file"
  falsification_test: "with cReserved[3] zero-initialised, the win-home editor-bridge run must show gen1 == gen2 and drawn1 == drawn2 byte for byte, and the map-file test must pass on win-home and macOS; any remaining difference falsifies 'only cause'"
  fix_rationale: "naming the pad bytes as a zero-initialised member makes every byte of the record defined at construction and copied member-wise by the language - the raw save then writes only defined bytes. Layout (40 B, same offsets) and file format are unchanged, and a point read from a map keeps the bytes it had, so shipped-map round trips stay byte-identical."
  blind_spots: "other raw-saved padded types (SMapObjectInfo::SLinkInfo, SVertexAltitude) show no difference in this run - SetZero() covers altitudes; SLinkInfo locals in FillObjectSet are still uninitialised padding and could leak in another build configuration (release); not fixed here, listed under follow_ups"
  candidate_causes:
    - "code: uninitialised struct padding written raw (CONFIRMED)"
    - "data/config: absolute script path stored in the map (present, identical between runs - eliminated as cause)"
    - "environment: UCRT per-thread rand() state (eliminated: geometry identical)"
  and_gate: "yes, two conditions together: (1) a raw byte save of a padded POD and (2) pad bytes left uninitialised by construction on the stack. Either alone is harmless; windows-msvc only made the garbage vary."
next_action: none - fixed (cabb77da8), verified on macOS and win-home; CI dispatch on fix/rmg-windows-determinism for the runner itself

## Symptoms

expected: same seed/template/context/setting/level/graph/angle -> byte-identical .bzm (D-04)
actual: windows-msvc only - regenerated map differs. CI log: "(identical? size 459328 vs 459328, first difference at 118138: 62 vs 72)" for the same-seed check; "(identical? size 407650 vs 407650, first difference at 102540: 70 vs 63)" for the blank-draw-seed check. (The "459328 vs 444165, first difference at 3" dump belongs to the seed-778 'another seed makes another map' check, which is expected to differ.)
errors: FAIL: the same seed regenerates a byte-identical map; FAIL: the seed a blank draw reported regenerates that map
reproduction: zig build test-editor-bridge on x86_64-windows-msvc (CI job windows-platform)
started: first run of the 05-08 determinism harness on windows-msvc (run 37029035801, commit bbb7c26c6)

## Eliminated

- hypothesis: Windows path syntax (absolute 'D:\...' script path with backslashes) stored in the map makes the regeneration differ (coordinator hint)
  evidence: the map does store szScriptFile = <output root>maps\<name> absolute with backslashes (already WINDOWS.md #5), but both generations of the same-seed check write the same root and name: the szScriptFile chunk is byte-identical in gen1/gen2 on win-home, the files are the same size, and the full diff has zero differing bytes outside the VSO point padding. The 15 KB gap (459328 vs 444165) is the seed-778 check, which asserts the maps DIFFER.
  timestamp: 2026-10-02T18:55:00Z

- hypothesis: RMG calls rand()/random helpers on a path or thread where UCRT's per-thread rand state was never seeded
  evidence: an unseeded draw would change geometry, object placement or tile picks (sizes and many chunks); the full diff shows only padding bytes inside otherwise identical point records - every float, flag and object is the same
  timestamp: 2026-10-02T18:55:00Z

- hypothesis: a pointer-keyed hash map is iterated (output order differs)
  evidence: reordering would move whole records; no record moved - only 1-3 bytes at offset 33 of 40-byte point records differ
  timestamp: 2026-10-02T18:55:00Z

- hypothesis: time is read into the map
  evidence: no differing byte outside the point padding; the padding values are constant within a spline segment and differ between segments - not a clock
  timestamp: 2026-10-02T18:55:00Z

## Evidence

- timestamp: 2026-10-02T18:10:00Z
  checked: CI log of run 37029035801 (saved to zig-out/local-test/run-37029035801-failed.log), stdout ordering of the 'identical?' dumps vs FAIL lines
  found: the same-seed failure is size 459328 vs 459328 with differences at 118138-118139 (62 5e vs 72 bf) and again at 118178-118179, 40 bytes later. Blank-draw failure: 407650 vs 407650, differences at 102540 and 102599 (70 vs 63), 59 bytes apart... (see dump). Same size both times - not a structural divergence, only a few bytes.
  implication: generation itself is deterministic in structure; a few bytes per record carry garbage

- timestamp: 2026-10-02T18:10:00Z
  checked: decoded the dump around 118105..118185 as floats
  found: records of [3 floats pos][3 floats unit normal][float radius][float 67.88 width][byte 01/00][3 differing bytes][float 0.0/1.0] = 40 bytes, matching SVectorStripeObjectPoint (Formats/fmtVSO.h): vPos, vNorm, fRadius, fWidth, bool bKeyPoint, (3 pad bytes), fOpacity
  implication: the differing bytes are the compiler-inserted padding after bKeyPoint

- timestamp: 2026-10-02T18:12:00Z
  checked: Sources/src/Formats/fmtVSO.cpp + Sources/src/StreamIO/SSHelper.h
  found: SVectorStripeObject::operator&(IStructureSaver&) does saver.Add(2, &points); SVectorStripeObjectPoint has no IStructureSaver operator&, so AddInternal(vector) takes DoDataVector -> AddRawData(2, &data[0], sizeof(T1)*nSize): the whole 40-byte objects, padding included, go to the file
  implication: whatever bytes sit in the padding are persisted

- timestamp: 2026-10-02T18:13:00Z
  checked: Sources/src/RandomMapGen/VSO_StaticMethods.cpp CVSOBuilder::SliceSpline
  found: `SVectorStripeObjectPoint point;` is a stack local; the ctor initialises every member but not the 3 pad bytes; it is copied into a std::list (push_back) and then into the vector (SampleCurve) - a trivially-copyable copy carries the pad bytes along
  implication: the pad bytes in the map are stack garbage from SliceSpline's frame

- timestamp: 2026-10-02T18:20:00Z
  checked: coordinator hint (paths) - session_rmg.cpp + MapInfo_StaticMethods_RMGeneration.cpp:874-960
  found: the map does store a path: mapInfo.szScriptFile = szOutputRoot + "maps\\" + name (absolute, backslashes) - already ledgered as WINDOWS.md #5. But in the failing same-seed check both generations write to the same root and name, so the string is identical in both files; the CI dumps show equal sizes (459328 vs 459328; 407650 vs 407650). The 15 KB gap (459328 vs 444165) belongs to the seed-778 check, which asserts the files DIFFER (different seed, different map) - it is not a failure.
  implication: a stored path cannot explain a same-path regeneration differing; confirm with a full diff on Windows

- timestamp: 2026-10-02T18:30:00Z
  checked: audit of the other raw-saved (DataChunk/AddRawData) types in CMapInfo's BZM save path for implicit padding
  found: SVectorStripeObjectPoint (40 B, 3 pad after bKeyPoint) - raw via DoDataVector; SVertexAltitude {float; BYTE} (8 B, 3 pad) - raw via Do2DArrayData, but RMG and new maps SetZero() the array first and write field-wise; SMapObjectInfo::SLinkInfo {int; bool; int} (12 B, 3 pad) - raw via saver.Add(8,&link) (no IStructureSaver operator&). SMainTileInfo (2 B), SCrossTileInfo (5 B), CVec2/CVec3, SColor: no padding. Everything else in CMapInfo serialises field by field.
  implication: three padded raw-saved types; only the VSO point is implicated by the CI dump so far - the full diff decides whether SLinkInfo/SVertexAltitude also leak garbage

- timestamp: 2026-10-02T18:35:00Z
  checked: win-home toolchain
  found: MSVC 14.52's STL static_asserts "expected Clang 22" (Zig 0.16 = Clang 21); pinned MSVC 14.51.36231 + SDK 10.0.26100.0, the exact pair CI run 37029035801 logged
  implication: local Windows build now mirrors CI

- timestamp: 2026-10-02T18:45:00Z
  checked: macOS arm64 gate with a local-only patch keeping the regenerated maps (zig-out/local-test/rmg-diag-gen1.bzm, -gen2.bzm, -drawn2.bzm), decoded with zig-out/local-test/bzmdiff.py (chunk parser for the ZigStructureWriter format)
  found: gate PASS, gen1 == gen2 byte for byte. szScriptFile chunk = "\Volumes\Storage\...\zig-out\local-test\rmg-user\maps\m3_rmg_a" (absolute, backslashes - same string in both). The VSO point pad bytes are NOT zero on macOS either: roads3 pad triples 000000 x554, cf4701 x69, ff1701 x19, 1c4801 x18 - garbage, but the same garbage in both generations. Shipped arnheim.bzm also carries a garbage triple (a68103) from the original editor.
  implication: the uninitialised padding exists on every platform; on macOS the stack bytes under it happen to repeat run to run, on windows-msvc they do not. The bug is the raw save of uninitialised bytes, not anything Windows-specific in the generator.

- timestamp: 2026-10-02T18:55:00Z
  checked: win-home (x86_64-windows-msvc, MSVC 14.51.36231, SDK 10.0.26100.0), test-editor-bridge run in the interactive session (over plain ssh the tier skips: "no GPU device"), diag patch keeping the maps; full diff with bzmdiff.py
  found: REPRODUCED - "FAIL: the same seed regenerates a byte-identical map" (459311 vs 459311, first difference at 118138 - the same offset as CI) and "FAIL: the seed a blank draw reported regenerates that map" (403254 vs 403254). Same seed gen1 vs gen2: 1487 differing bytes in 554 runs, ALL in chunk 1/1/1/8/<road>/2/2 (map > terrain > roads3 > road > points > raw bytes), every run at offset 33 mod 40 = bytes 33..35 of SVectorStripeObjectPoint = the padding after bKeyPoint. Blank draw pair: 894 bytes in 526 runs, same chunk, same offset 33. No byte differs anywhere else - not in szScriptFile ("D:\bk-rmg\zig-out\local-test\rmg-user\maps\m3_rmg_a" in both), not in objects (SLinkInfo padding), not in altitudes (SVertexAltitude padding).
  implication: ROOT CAUSE CONFIRMED (uninitialised memory - one of the four starting hypotheses) - the only nondeterminism is the uninitialised pad bytes of the VSO points. The pad triples on Windows (5fbb4a, b29d4a, af3277, 804d4a, f8f37c ...) change between generations in one process; one constant triple (cf4701, x69) appears identically on macOS and Windows (it is shipped patch data, see below).

- timestamp: 2026-10-02T19:00:00Z
  checked: new test TestVsoPointBytes (tools/zig/map_file_test.cpp, test-map-files tier) WITHOUT the fix
  found: macOS: "a new road point keeps no byte of the memory it was built in" and "a road point copied into other memory is every byte of the original" FAIL; the stack-fill end-to-end checks pass there (the SliceSpline slot is 000000 on arm64). win-home msvc: all four FAIL - including "the same road built over two different stack fills is the same bytes" and "the two maps the two builds are saved into are the same file" ((identical? size 158018, first difference at 59383: 19 vs 22)). Every other check of the tier passes.
  implication: red phase holds on the platform with the symptom; the test guards the defect in CI's windows-platform "Map file tier"

- timestamp: 2026-10-02T19:12:00Z
  checked: WITH the fix (cReserved[3] zero-initialised): macOS test-map-files, macOS test-editor-bridge gate, win-home test-map-files
  found: macOS map-file PASS (EXIT 0); macOS editor-bridge PASS, gen1 == gen2; win-home map-file PASS incl. "66 of 66 maps round-tripped" (shipped maps keep their original pad bytes, e.g. arnheim's a68103). Remaining non-zero pad triples in a generated map (cf4701 x69, ff1701 x19, 1c4801 x18) are the shipped patch files' own bytes copied verbatim (found in Data/Scenarios/Patches/summer_France/p_lg_village_a_5.bzm, p_start_*_S_2.bzm, p_sm_village_*.bzm) - deterministic data, not runtime garbage.
  implication: fix holds on both platforms; file format compatibility intact

- timestamp: 2026-10-02T19:10:00Z
  checked: win-home test-editor-bridge WITH the fix, interactive session, diag patch keeping the maps
  found: editor-bridge PASS (EXIT 0); no FAIL line; the only "identical?" dump is the seed-778 check (expected to differ). Same-seed gen1 == gen2 and blank-draw drawn1 == drawn2 byte for byte (cmp). The fixed gen1 vs the unfixed gen1 differ in 1662 bytes, 554 runs, all at offset 33 of the roads3 point records - the fix changes nothing but the pad bytes.
  implication: falsification test passed - the pad bytes were the only source of nondeterminism

- timestamp: 2026-10-02T19:14:00Z
  checked: an instrumented win-home build (fprintf of the 8 bytes at &point+32 in SliceSpline, plus a malloc sample and &list.back(), fix reverted) to see what the leftover word is
  found: with the instrumentation in place the leftover was 3f9a7b in every traced call (float-like) - the extra calls change SliceSpline's frame, so the probe disturbs what it measures
  implication: which earlier store leaves the run-varying bytes on windows-msvc is not pinned; it does not matter for the fix (every byte is now defined) and is recorded as unknown rather than guessed

## Resolution

root_cause: "SVectorStripeObjectPoint (Formats/fmtVSO.h) has 3 implicit pad bytes after `bool bKeyPoint`. Maps save the points vector as raw bytes (CSaverAccessor::DoDataVector - the struct has no IStructureSaver operator&), and the RMG builds each point as a stack local in CVSOBuilder::SliceSpline whose constructor sets the members only, so the pad bytes carried the stack's leftovers into the BZM. On windows-msvc those leftovers differ between generations (macOS: garbage too, but repeatable), so a seed did not regenerate a byte-identical map. AND-gate: raw byte save + uninitialised padding."
fix: "named the pad bytes as a zero-initialised member `BYTE cReserved[3]` (ctor `cReserved()`), plus static_assert(sizeof == 40) - layout and file format unchanged; points read from a map keep their bytes. Regression test TestVsoPointBytes in tools/zig/map_file_test.cpp (test-map-files tier, on every CI platform)."
verification:
  - "signal 1 (repro before fix): win-home msvc reproduced both FAILs at CI's offset 118138; full diff = pad bytes only"
  - "signal 2 (test red before fix): TestVsoPointBytes 4/4 FAIL on win-home msvc, 2/4 on macOS (the stack-fill checks only bite on msvc)"
  - "signal 3 (green after fix): win-home test-map-files PASS (66/66 maps round-trip), win-home test-editor-bridge PASS with gen1==gen2 and drawn1==drawn2; macOS test-map-files PASS; macOS gate test-editor-bridge -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run PASS"
  - "signal 4 (fix changes only the cause): fixed vs unfixed generated map differ only at offset 33..35 of the point records"
  - "signal 5 (no collateral): every other check of test-map-files and test-editor-bridge passes on both hosts"
oracle_type: specified (byte-identity of two saves is the D-04 contract itself); boundary neighbours: both fills 0x00/0xff, a copy into a third fill 0xa5, and the saved-file level as well as the in-memory level
guardrail_verdict: accepted
files_changed: [Sources/src/Formats/fmtVSO.h, tools/zig/map_file_test.cpp]
commit: cabb77da8
follow_ups:
  - "SMapObjectInfo::SLinkInfo {int; bool; int} is also saved raw (saver.Add(8, &link)) with 3 uninitialised pad bytes; RMG's FillObjectSet builds objects as stack locals. It did not vary in any run seen here, but it is the same class - candidate for the same treatment"
  - "SVertexAltitude {float; BYTE} is saved raw too; every producer SetZero()s the array first, so it is defined today"
  - "WINDOWS.md #5 (absolute script path with backslashes in generated maps) stays open: real, user-ruled (store relative with '/', expand on load), but not this failure's cause"
