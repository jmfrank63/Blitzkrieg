# P06-M01 — Port Shared Runtime Headers and PCH Inputs

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove unconditional Windows/COM compiler assumptions from headers included by playable runtime modules.

**Dependencies:** P05-M05.

**Allowed files:** `Sources/src/Misc/StdAfx.h`, `Sources/src/Main/StdAfx.h`, `Sources/src/Game/StdAfx.h`, `Sources/src/Input/StdAfx.h`, `Sources/src/SFX/StdAfx.h`, `Sources/src/Net/StdAfx.h`, `Sources/src/Image/StdAfx.h`, `Sources/src/Anim/StdAfx.h`, `Sources/src/Common/StdAfx.h`, `Sources/src/UI/StdAfx.h`, `Sources/src/Formats/StdAfx.h`, `Sources/src/RandomMapGen/StdAfx.h`, `Sources/src/Scene/StdAfx.h`, `Sources/src/AILogic/StdAfx.h`, `Sources/src/GameTT/StdAfx.h`, `Sources/src/Misc/MemorySystem.h`, `Sources/src/Misc/MemorySystem.cpp`, `Sources/src/Main/assert.cpp`, `Sources/src/Main/GameDB.h`, `tools/zig/runtime_headers_test.cpp`, `build.zig`.

- [ ] Build one translation unit that includes every runtime `StdAfx.h` separately under each supported target to detect include-order leakage.
- [ ] Include `Platform/Compiler.h`, `LegacyTypes.h`, and `LegacyVariant.h` before shared engine headers; guard Windows/COM headers by `_WIN32`.
- [ ] Replace unconditional MSVC pragmas, `__int64`, `__stdcall`, `for` macro, and `comutil.h` assumptions with Phase 00/02 contracts.
- [ ] Replace raw `__cdecl` and `__stdcall` declarations in the memory system, assertion entry point, and game database callbacks with `BK_CDECL` and `BK_STDCALL`.
- [ ] Keep editor/MFC headers out of the test and runtime source sets.
- [ ] Run header compile for all targets and Windows `game-all`.
- [ ] Commit: `refactor: make runtime headers platform portable`

**Evidence:** per-header/per-target compile table.
