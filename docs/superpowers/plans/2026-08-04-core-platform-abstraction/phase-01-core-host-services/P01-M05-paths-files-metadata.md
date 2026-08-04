# P01-M05 — Paths, Files, and Metadata

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Expose executable/data/user roots, normalization, metadata, directory creation, and enumeration with UTF-8 semantics.

**Dependencies:** P01-M04.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/Paths.cpp`, `Sources/src/Platform/Windows/Paths.cpp`, `Sources/src/Platform/Linux/Paths.cpp`, `Sources/src/Platform/MacOS/Paths.cpp`, `Sources/src/Platform/Paths.h`, `Sources/src/Platform/Paths.cpp`, `Sources/src/Misc/FileUtils.h`, `Sources/src/Misc/FileUtils.cpp`, `tools/zig/platform_paths_test.cpp`, `tools/zig/platform_file_utils_test.cpp`, `build.zig`.

- [ ] Test mixed separators, dot segments, non-ASCII names, spaces, missing files, timestamps, sorted enumeration, read-only data root, and writable user root.
- [ ] Implement platform roots and caller-owned enumeration records without changing save-relative names.
- [ ] Convert `FileUtils` metadata/time comparisons and remove native file handles.
- [ ] Run tests in a temporary directory outside the source tree on Windows and Linux.
- [ ] Verify no installed-data write is possible through the writable-root API.
- [ ] Commit: `platform: expose portable paths and metadata`

**Evidence:** root table and deterministic enumeration fixture.
