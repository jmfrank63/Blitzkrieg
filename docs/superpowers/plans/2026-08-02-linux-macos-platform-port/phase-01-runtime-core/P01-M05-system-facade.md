# P01-M05 — Define Portable System Services

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Provide the executable/environment/dialog/URL/process operations needed by later game call-site packets.

**Dependencies:** P01-M04.

**Allowed files:** `Sources/src/Platform/System.h`, `Sources/src/Platform/System.cpp`, `tools/zig/platform_system_test.cpp`, `build.zig`.

- [ ] Define and test `ExecutablePath`, `GetEnvironment`, `SetEnvironment`, `ShowError`, `OpenUrl`, `OpenFile`, and synchronous `RunProcess(argv, working_directory, exit_code)`.
- [ ] Use SDL APIs for executable base path, simple message boxes, and URL opening; use explicit argument arrays rather than command strings.
- [ ] Implement child process only in private target branches: Win32 process APIs on Windows and `posix_spawn`/`waitpid` on Linux/macOS. Do not invoke a shell.
- [ ] Make UI-opening tests injectable so CI validates requested title/text/URL without showing a modal dialog.
- [ ] Run `test-platform-core` and the hermeticity audit.
- [ ] Commit: `platform: add portable system services`

**Evidence:** executable/environment/process test output and injected dialog/URL records.
