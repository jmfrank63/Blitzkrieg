# P07-M02 — Remove File, Path, and CRT Residue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Convert native file metadata, executable paths, access checks, separators, and Windows CRT spellings.

**Dependencies:** P07-M01.

**Allowed files:** `Sources/src/GameTT/Common.cpp`, `Sources/src/GameTT/MainMenu.cpp`, `Sources/src/GameTT/OptionEntryWrapper.cpp`, `Sources/src/Main/GameDB.cpp`, `Sources/src/Main/iMainInternal.cpp`, `Sources/src/RandomMapGen/Resource_Functions.cpp`, `Sources/src/RandomMapGen/Resource_Types.h`, `Sources/src/Misc/StrProc.cpp`, `tools/zig/runtime_storage_residue_test.cpp`, `tools/zig/runtime_platform_allowlist.txt`.

- [ ] Test checksum reads, timestamps, executable path, writable save/screenshot roots, mixed separators, non-ASCII names, `_stricmp` behavior, and missing files.
- [ ] Route metadata/path operations through PlatformClient and streams through existing StreamIO.
- [ ] Replace `_access`, `_itoa`, `_finite`, `_stricmp`, `_strnicmp`, and `MAX_PATH` with focused portable helpers.
- [ ] Preserve ASCII case-insensitive resource comparison and on-disk bytes.
- [ ] Run Windows/Linux fixtures and the source audit.
- [ ] Commit: `runtime: remove native storage and CRT residue`

**Evidence:** file/checksum/path fixtures and reduced allowlist.
