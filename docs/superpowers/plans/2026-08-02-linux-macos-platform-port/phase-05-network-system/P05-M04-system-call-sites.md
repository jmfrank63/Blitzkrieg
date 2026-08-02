# P05-M04 — Route Game System Calls Through the Facade

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove direct Windows dialogs, file opening, child process, debugger, and crash-hook calls from playable runtime paths.

**Dependencies:** P01-M05.

**Allowed files:** `Sources/src/Game/GameMain.cpp`, `Sources/src/Game/main.cpp`, `Sources/src/Main/Initialization.cpp`, `Sources/src/Main/EmergencySave.h`, `Sources/src/Main/MainLoopCommands.cpp`, `Sources/src/RandomMapGen/Resource_Functions.cpp`, `tools/zig/platform_system_calls_test.cpp`, `build.zig`.

- [ ] Add injected tests for startup errors, initialization errors, emergency save errors, open-log action, external map-generation command argv/working directory/exit code, and debugger-only paths.
- [ ] Replace `MessageBox`, `ShellExecute`, `CreateProcess`, `WaitForSingleObject`, module-debug probing, and raw debug output with Phase 01 services.
- [ ] Keep Windows SEH/crash filter private to `_WIN32`; non-Windows writes the same fatal context to stderr/log and exits normally through common teardown when safe.
- [ ] Never construct a shell command string; preserve external tool arguments as an argv vector.
- [ ] Run system-call tests and hermeticity audit.
- [ ] Commit: `platform: route game system integration through facade`

**Evidence:** captured service requests and child exit-code test.
