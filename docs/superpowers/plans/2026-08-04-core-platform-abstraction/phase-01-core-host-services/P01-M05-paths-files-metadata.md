# P01-M05 — Paths, Files, and Metadata

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Expose executable/data/user roots, normalization, metadata, directory creation, and enumeration with UTF-8 semantics.

**Dependencies:** P01-M04.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/Paths.cpp`, `Sources/src/Platform/Windows/Paths.cpp`, `Sources/src/Platform/Linux/Paths.cpp`, `Sources/src/Platform/MacOS/Paths.cpp`, `Sources/src/Platform/Paths.h`, `Sources/src/Platform/Paths.cpp`, `Sources/src/Misc/FileUtils.h`, `Sources/src/Misc/FileUtils.cpp`, `tools/zig/platform_paths_test.cpp`, `tools/zig/platform_file_utils_test.cpp`, `build.zig`.

- [x] Test the existing mixed-separator, missing-file, metadata, and writable-root contracts on Windows.
- [x] Preserve platform roots and save-relative names in the existing paths facade.
- [ ] Convert `FileUtils` metadata/time comparisons and remove native file handles.
- [x] Run `test-platform-paths` and `test-platform-files` natively on Windows; Linux remains in CI coverage.
- [ ] Verify no installed-data write is possible through the writable-root API.
- [x] Commit checkpoint: `platform: expose portable paths and metadata`.

**Evidence:** Windows `zig build test-platform-paths -Dtarget=x86_64-windows-msvc -Dtest-mode=run` and `zig build test-platform-files -Dtarget=x86_64-windows-msvc -Dtest-mode=run` both pass.
