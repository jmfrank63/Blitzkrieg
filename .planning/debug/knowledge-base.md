# GSD Debug Knowledge Base

Resolved debug sessions. Used by `gsd-debugger` to surface known-pattern hypotheses at the start of new investigations.

---

## rmg-windows-nondeterminism - same RMG seed gives a different .bzm on windows-msvc only
- **Date:** 2026-10-02
- **Error patterns:** byte-identical, regenerates, same seed, identical? same size first difference, windows-msvc only, determinism, bzm differs, padding, raw bytes
- **Root cause(s):** SVectorStripeObjectPoint's 3 implicit pad bytes after bool bKeyPoint are saved raw (CSaverAccessor::DoDataVector, no IStructureSaver operator&); CVSOBuilder::SliceSpline builds points on its stack and the ctor sets members only, so stack leftovers went into the file; run-varying on windows-msvc, repeatable garbage on macOS
- **Fix:** named zero-initialised member BYTE cReserved[3] + static_assert(sizeof == 40); layout/format unchanged, read-back keeps file bytes
- **Files changed:** Sources/src/Formats/fmtVSO.h, tools/zig/map_file_test.cpp
- **Why not caught:** no gate existed for this class - the round-trip tests compare read-then-write (bytes come from the file), and the only byte-level generate-twice check ran first on windows-msvc
- **Recurrence guard:** tools/zig/map_file_test.cpp:TestVsoPointBytes (test-map-files, every CI platform): a point built over 0x00 vs 0xff memory and copied into 0xa5 memory is the same bytes; the same road built after two different stack fills saves the same file. Diagnostic: a same-size BZM diff whose runs sit at a fixed offset of a fixed-size record is padding - decode with the chunk format in Sources/src/StreamIOZig/legacy_bridge.cpp ZigStructureWriter. Sibling raw-saved padded types: SMapObjectInfo::SLinkInfo, SVertexAltitude
---

