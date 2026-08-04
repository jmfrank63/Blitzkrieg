# P07-M03 — Port UI/Image Geometry and Case-Sensitive Includes

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove Win32 rectangle helpers and Windows filesystem case assumptions from UI, Image, and dependent headers.

**Dependencies:** P07-M02.

**Allowed files:** `Sources/src/Image/ImageReal.cpp`, `Sources/src/Image/ImageProcessor.cpp`, `Sources/src/UI/UIInternal.h`, `Sources/src/UI/UIObjMap.cpp`, `Sources/src/UI/UIObjectiveScreen.cpp`, `Sources/src/UI/UIScreen.cpp`, `Sources/src/SFX/SFX.h`, `Sources/src/GFX/GFXHelper.h`, `Sources/src/Main/GameStats.h`, `Sources/src/Main/GameDB.h`, `tools/zig/ui_image_portability_test.cpp`, `tools/zig/runtime_platform_allowlist.txt`.

- [ ] Add compile/runtime fixtures for rectangle construction, null image returns, UI resource paths, and exact-case include resolution.
- [ ] Replace `SetRect` with engine rectangle constructors and correct pointer-return error values.
- [ ] Correct include spelling to repository case without renaming serialized/resource identifiers.
- [ ] Replace `_wcsicmp` only at owned call sites with the approved portable comparator.
- [ ] Build Image/UI on Linux and macOS compile targets and run fixtures on Windows/Linux.
- [ ] Commit: `runtime: port UI and Image host assumptions`

**Evidence:** rectangle fixture, module compile output, zero wrong-case includes.
