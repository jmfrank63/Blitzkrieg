# P07-M03 — Port UI/Image Geometry and Case-Sensitive Includes

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove Win32 rectangle helpers and Windows filesystem case assumptions from UI, Image, and dependent headers.

**Dependencies:** P07-M02.

**Allowed files:** `Sources/src/Image/ImageReal.cpp`, `Sources/src/Image/ImageProcessor.cpp`, `Sources/src/UI/UIInternal.h`, `Sources/src/UI/UIObjMap.cpp`, `Sources/src/UI/UIObjectiveScreen.cpp`, `Sources/src/UI/UIScreen.cpp`, `Sources/src/SFX/SFX.h`, `Sources/src/GFX/GFXHelper.h`, `Sources/src/Main/GameStats.h`, `Sources/src/Main/GameDB.h`, `tools/zig/ui_image_portability_test.cpp`, `tools/zig/runtime_platform_allowlist.txt`.

- [x] Add compile/runtime fixtures for rectangle construction, null image returns, UI resource paths, and exact-case include resolution.
- [x] Replace `SetRect` with engine rectangle constructors and correct pointer-return error values.
- [x] Correct include spelling to repository case without renaming serialized/resource identifiers.
- [x] Replace `_wcsicmp` only at owned call sites with the approved portable comparator.
- [ ] Build Image/UI on Linux and macOS compile targets and run fixtures on Windows/Linux.
- [x] Commit: `runtime: port UI and Image host assumptions`

**Evidence:**

- Windows `ui-image-portability-test` passed: `rectangle=1 null-image=1 resource-path=1 exact-case=1`.
- `zig build game -Dtarget=x86_64-windows-msvc` passed after replacing all owned `SetRect`/`_wcsicmp` residue. The source scan reports zero M03 hits in Image/UI and dependent headers.
- Include spellings remain repository-case correct; serialized UI/resource identifiers were not renamed. The duplicate `Basic.h` include declaration was removed as a portability cleanup.
- Linux/macOS compile and Linux fixture execution remain unavailable in this Windows checkout; the configured Linux Zig C++ sysroot limitation is recorded from M02.
