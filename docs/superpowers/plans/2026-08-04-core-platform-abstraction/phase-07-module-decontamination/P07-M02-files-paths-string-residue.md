# P07-M02 — Remove File, Path, and CRT Residue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Convert native file metadata, executable paths, access checks, separators, and Windows CRT spellings.

**Dependencies:** P07-M01.

**Allowed files:** `Sources/src/GameTT/Common.cpp`, `Sources/src/GameTT/MainMenu.cpp`, `Sources/src/GameTT/OptionEntryWrapper.cpp`, `Sources/src/Main/GameDB.cpp`, `Sources/src/Main/iMainInternal.cpp`, `Sources/src/RandomMapGen/Resource_Functions.cpp`, `Sources/src/RandomMapGen/Resource_Types.h`, `Sources/src/Misc/StrProc.cpp`, `tools/zig/runtime_storage_residue_test.cpp`, `tools/zig/runtime_platform_allowlist.txt`.

- [x] Test checksum reads, timestamps, executable path, writable save/screenshot roots, mixed separators, non-ASCII names, `_stricmp` behavior, and missing files.
- [x] Route metadata/path operations through PlatformClient and streams through existing StreamIO.
- [x] Replace `_access`, `_itoa`, `_finite`, `_stricmp`, `_strnicmp`, and `MAX_PATH` with focused portable helpers.
- [x] Preserve ASCII case-insensitive resource comparison and on-disk bytes.
- [ ] Run Windows/Linux fixtures and the source audit.
- [x] Commit: `runtime: remove native storage and CRT residue`

**Evidence:**

- Windows `runtime-storage-residue-test` passed with `bytes=16 checksum=2e7df7ee utf8=1 mixed-separators=1 ascii-case=1 timestamp=1 missing=1`.
- Windows `zig build game -Dtarget=x86_64-windows-msvc`, `zig build test-file-utils -Dtarget=x86_64-windows-msvc -Dtest-mode=run`, `zig build test-platform-paths -Dtarget=x86_64-windows-msvc -Dtest-mode=run`, and the existing `test-platform-storage` gate passed.
- The source fixture reports zero owned M02 hits for `_access`, `_itoa`, `_finite`, `_stricmp`, `_strnicmp`, `MAX_PATH`, direct executable-directory queries, and direct file-handle metadata calls. `zig test tools/zig/runtime_platform_audit_test.zig` remains green (9/9; inventory 39, ownership 38).
- Linux fixture compilation remains unavailable in this Windows checkout because the configured Zig C++ Linux sysroot cannot locate `<cctype>`; Windows gates are authoritative per the execution handoff.
