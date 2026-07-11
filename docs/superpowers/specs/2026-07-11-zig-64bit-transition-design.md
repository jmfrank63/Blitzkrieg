# Zig-assisted 64-bit transition design

## Goal

Move the playable Blitzkrieg runtime from Windows x86 to Windows x64 while preserving its existing game-data, save-game, network, and scripting formats. Establish a small Zig portability boundary that replaces architecture-dependent implementation code and becomes the foundation for later Linux and macOS work.

## Scope

This milestone includes the `Game` executable and every runtime library it loads: `StreamIO`, `GFX`, `Input`, `SFX`, and `Net`. It retains the current Windows renderer, windowing, input, and audio backends. Editors, conversion tools, and a portable graphics backend are explicitly out of scope.

The legacy C++ gameplay engine remains in place. Zig is introduced only behind a stable C ABI for narrowly bounded CPU- and ABI-sensitive services.

## Architecture

Add a `Blitz64` Zig static library, linked into all runtime modules that use its services. Its public C header exposes only fixed-width scalar types, opaque pointers, and `size_t` where a native buffer length is intended. It must not expose Zig data structures, C++ classes, STL types, or platform-specific types.

Initial services are:

- Bit conversion and integer helpers that currently rely on x86 inline assembly.
- Memory and numeric primitives whose behavior can be specified and tested independently.
- A stable 32-bit handle mechanism for legacy fields that currently carry object pointers in `int` or `DWORD` values.
- ABI assertions and conversion helpers for pointer-width boundaries.

The C++ layer calls exported Zig functions using C-compatible declarations. A handle is an opaque `uint32_t`; persistent data, messages, scripts, and network packets retain that width. A native pointer can be resolved only inside the owning process through the handle table. Pointer addresses must not be serialized or sent over module, network, or persistence boundaries.

## Platform and build contract

`build.zig` continues to support the current Windows x86 build while adding Windows x64 as a first-class target. MSVC and Windows SDK library paths are selected by target architecture rather than hardcoded to `x86`.

All game runtime DLLs and the executable are built for one architecture in a build invocation. The x64 runtime must never attempt to load a 32-bit DLL. The target selected by `-Dtarget=x86_64-windows-msvc` produces an x64 `Game.exe` and x64 runtime DLLs; the current x86 target remains available for equivalence testing during the transition.

The x64 build uses the Windows LLP64 model. Portable interfaces use `uint32_t`, `uint64_t`, `intptr_t`, `uintptr_t`, and `size_t` according to their meaning; they do not use `long`, `DWORD`, or `int` as pointer-sized substitutes.

## Assembly replacement rules

Each assembly replacement begins with a behavior test that runs against the intended public C ABI. It must cover normal inputs, boundary inputs, and defined bit-level behavior. The implementation is then added in Zig and linked into the C++ runtime.

Straightforward integer and memory operations are replaced with portable Zig first. SIMD is not introduced unless a post-migration benchmark identifies a real regression. Legacy x87 floating-point routines remain untouched until tests define acceptable rounding, precision, and determinism requirements. Old bundled libpng assembly is disabled or replaced through an upstream/portable implementation rather than translated line-for-line.

## ABI and compatibility rules

Every pointer-to-32-bit conversion is classified before change:

- Local transient value: use `uintptr_t` only within the native process where a pointer representation is actually required.
- Object identity crossing a legacy 32-bit field: use a `uint32_t` handle.
- Persistent or network data: use an explicit fixed-width identifier, never a pointer or native-size type.

Packed disk and wire structures retain their documented field widths and byte layout. Raw `Write(&value, sizeof(value))` operations are audited when `value` contains a pointer, native-size type, `long`, padding-sensitive layout, or C++ implementation state. Such cases are replaced with explicit serialization.

All Win32 callback and window-data uses are audited for x64 pointer widths, including `INT_PTR`, `LONG_PTR`, `WPARAM`, `LPARAM`, `SetWindowLongPtr`, and `GetWindowLongPtr`.

## Validation

`build.zig` gains a `test` step that runs Zig unit tests and the C++/Zig ABI integration test. Unit tests validate handle lifecycle, invalid-handle behavior, and every Zig replacement primitive. Integration tests compile and link a minimal C++ caller against the Zig library for both Windows x86 and Windows x64 targets.

For each migrated primitive, equivalence tests compare x86 and x64 outputs wherever the original behavior is deterministic. The playable runtime is validated as a complete x64 set: launch, dynamic module loading, mission startup, save/load, input, audio, video, and multiplayer smoke coverage. Existing content remains readable by both x86 and x64 builds during the transition.

## Delivery sequence

1. Add the Zig library, C ABI header, build targets, and test infrastructure.
2. Migrate deterministic assembly primitives using test-first replacement.
3. Add stable handles and eliminate confirmed pointer truncations in playable runtime paths.
4. Make Windows SDK/MSVC paths architecture-aware and fix x64 compiler errors.
5. Build and validate the complete Windows x64 runtime DLL set and executable.
6. Audit persistent/network layouts and run compatibility regression coverage.

## Non-goals

- Rewriting gameplay systems, AI, UI, or scene logic in Zig.
- Replacing Direct3D 9, DirectInput, Win32 windows, Winsock, or COM in this milestone.
- Shipping Linux or macOS binaries.
- Changing legacy content formats merely to make them native-size.
