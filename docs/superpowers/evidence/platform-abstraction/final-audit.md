# P07-M06 Final Playable-Source Audit

The playable-source audit now treats native implementation as an explicit
adapter boundary. Native residue is permitted only under `Sources/src/Platform/`,
`Sources/src/libpng/`, `Sources/src/Input/`, the DirectDraw/DirectInput probe
`Sources/src/GFX/VideoCheck.cpp`, and the Windows resource entry
`Sources/src/Game/WindowsMain.cpp`. Portable gameplay sources are scanned in
full.

## Windows

Command:

```text
zig test tools/zig/runtime_platform_audit_test.zig
```

Result: 11/11 tests passed, `platform inventory count: 0`, and
`platform allowlist ownership count: 0`.

The same audit is a dependency of `zig build game-all` and `zig build package`.

## Linux

Command run through Ubuntu WSL with Linux-local Zig caches:

```text
ZIG_LOCAL_CACHE_DIR=/tmp/blitzkrieg-zig-cache \
ZIG_GLOBAL_CACHE_DIR=/tmp/blitzkrieg-zig-global \
zig test tools/zig/runtime_platform_audit_test.zig
```

Result: 11/11 tests passed, `platform inventory count: 0`, and
`platform allowlist ownership count: 0`.

The identical host audit is also run by the Linux, Windows, and macOS jobs in
`.github/workflows/cross-platform.yml`.

## Regression policy

The allowlist contains only its explanatory comment. A new forbidden token,
wrong-case relative include, or unguarded playable native library link makes
the audit fail. Windows-native system libraries remain accepted only inside an
explicit `target.result.os.tag == .windows` block.
