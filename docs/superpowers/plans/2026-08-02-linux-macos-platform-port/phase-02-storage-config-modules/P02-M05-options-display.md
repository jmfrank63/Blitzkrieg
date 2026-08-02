# P02-M05 — Port Options and Display Enumeration

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove OLE automation and Win32 display enumeration from the active StreamIO options bridge.

**Dependencies:** P02-M01, P02-M04.

**Allowed files:** `Sources/src/StreamIOZig/options_bridge.cpp`, `Sources/src/StreamIOZig/legacy_bridge.cpp`, `Sources/src/StreamIO/OptionSystem.h`, `Sources/src/StreamIO/OptionsConvert.h`, `tools/zig/options_bridge_test.cpp`, `build.zig`.

- [ ] Add option round-trip tests for each supported variant tag, metadata/defaults, case-insensitive key lookup, monitor list, unique display modes, action callbacks, and config serialization.
- [ ] Replace raw `VARIANT`/BSTR helpers with `LegacyVariant` operations and remove `oleauto.h`, `oleaut32`, and `comsupp` from non-Windows linkage.
- [ ] Enumerate displays and modes through SDL display APIs; preserve existing option strings `Primary`, `MonitorN`, and `<width>x<height>x<bpp>`.
- [ ] Resolve StreamIO callback symbols through `DynamicLibrary` or direct linked registration, never `GetModuleHandleA`.
- [ ] Route `legacy_bridge.cpp` timing and diagnostics through `Clock`/`Debug`, and replace `_stricmp`/`_strnicmp` with the shared ASCII case-fold helper without changing option-key matching.
- [ ] Run option bridge and StreamIO tests on Windows/Linux; compile macOS.
- [ ] Commit: `platform: port options and display discovery`

**Evidence:** serialized config comparison and display option list.
