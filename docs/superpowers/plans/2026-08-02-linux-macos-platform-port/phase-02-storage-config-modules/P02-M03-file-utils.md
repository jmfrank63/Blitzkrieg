# P02-M03 — Port the C++ File Utility Layer

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Keep `NFile::CFile` and `CFileIterator` interfaces while removing Win32 file handles and current-directory traversal.

**Dependencies:** P02-M02.

**Allowed files:** `Sources/src/Misc/FileUtils.h`, `Sources/src/Misc/FileUtils.cpp`, `Sources/src/StreamIO/FileAttribs.h`, `tools/zig/platform_file_utils_test.cpp`, `build.zig`.

- [ ] Test every open/share/create flag combination used by the game, duplicate stream position, read/write/seek/truncate/flush, rename/remove, attributes, wildcard iteration, recursive enumeration, full-name resolution, and free-space query.
- [ ] Store a portable opaque implementation pointer or C file descriptor instead of `HANDLE`; use C++17 filesystem and C file operations with explicit errors.
- [ ] Replace `WIN32_FIND_DATAA` storage with an engine-owned entry record and normalize returned paths to legacy separators only when returning to legacy callers.
- [ ] Remove process-wide `SetCurrentDirectory` recursion and perform all traversal relative to opened paths.
- [ ] Run the file utility test on Windows and Linux and compile on macOS.
- [ ] Commit: `platform: port legacy file utilities`

**Evidence:** flag/operation matrix and recursive enumeration output.
