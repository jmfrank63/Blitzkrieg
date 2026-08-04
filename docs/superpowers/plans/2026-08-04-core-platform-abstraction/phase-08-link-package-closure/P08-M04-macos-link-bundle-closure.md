# P08-M04 — Close the macOS arm64 Link and Bundle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Compile/link the playable arm64 graph and produce a structurally valid `.app` bundle.

**Dependencies:** P08-M03.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `Sources/src/Platform/Posix/Clock.cpp`, `Sources/src/Platform/Posix/Sync.cpp`, `Sources/src/Platform/Posix/Debug.cpp`, `Sources/src/Platform/Posix/DynamicLibrary.cpp`, `Sources/src/Platform/Posix/Socket.cpp`, `Sources/src/Platform/MacOS/Paths.mm`, `Sources/src/Platform/MacOS/System.mm`, `tools/zig/stage.zig`, `tools/zig/verify_runtime.zig`, `.github/workflows/cross-platform.yml`.

- [ ] Run macOS arm64 compile/link in GitHub Actions with the selected Xcode sysroot and C++ headers.
- [ ] Fix only target-owned source, visibility, install-name, framework, and bundle issues.
- [ ] Stage `Blitzkrieg.app/Contents/MacOS`, `Frameworks`, `Resources/Data`, shaders, and `Info.plist` through Zig tools.
- [ ] Verify arm64 Mach-O architecture, `@rpath`/`@loader_path`, exports, no quarantine assumptions, and no Windows files.
- [ ] Upload the verified bundle artifact for native testing.
- [ ] Commit: `build: close macOS arm64 game bundle`

**Evidence:** green macOS job, install-name report, and bundle manifest.
