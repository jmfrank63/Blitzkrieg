# Zig StreamIO replacement design

## Goal

Replace the legacy x86-only `StreamIO.dll` with a pure-Zig `StreamIO.dll` that can be loaded by the existing C++ engine as a Windows x64 module and can later target Linux and macOS. Existing game content, options, saves, and serialized data remain readable without conversion.

## Compatibility contract

The replacement retains the current DLL name and exports:

- `GetModuleDescriptor`
- `GetTempRawBuffer_Hook`
- `GetSLS_Hook`
- `GetSingletonGlobal_Hook`

It must present C++-ABI-compatible implementations of the interfaces consumed outside StreamIO, including `ISaveLoadSystem`, `ISingleton`, `IDataStorage`, `IDataStream`, the option system, and global variables. C++ owns the legacy interface definitions; Zig exports opaque allocations whose vtables and calling conventions match the selected target ABI.

Persistent and network-visible formats retain their current fixed-width fields, byte order, record order, and compression behavior. Native pointers, `usize`, and Zig container layouts never enter a file, save, or packet. A compatibility fixture suite covers representative configuration XML, PAK/ZIP reads, and existing saves before each corresponding subsystem becomes the default implementation.

## Scope and sequence

The work is split into independently shippable phases. Each phase retains a fallback to the old StreamIO implementation until its compatibility tests and in-game smoke checks pass.

### Phase 1: bootstrap and C++ ABI bridge

Create a Zig `StreamIO.dll` exporting the four stable hooks. Implement singleton registration/lookup and temporary raw buffers. Build a narrow C++ bridge that owns the C++ virtual interface/vtable definitions and forwards into Zig using a fixed C ABI. Validate that an x64 `Game.exe` can load the DLL, resolve hooks, and initialize global singletons.

### Phase 2: streams and storage

Implement file, memory, directory, ZIP/PAK, and mod-overlay storage in Zig. The C++ bridge exposes existing `IDataStream` and `IDataStorage` methods. Preserve seek behavior, access flags, file enumeration order where observable, and storage precedence. Add fixtures from the repository Data tree and compare stream contents, lengths, and checksums with the legacy module.

### Phase 3: save/load serialization

Implement `ISaveLoadSystem` and structure serialization incrementally. Decode existing saved games and write byte-compatible output where the old format is deterministic. Keep all record sizes and integer widths explicit. Introduce differential tests that serialize with the legacy x86 module and deserialize with Zig, then reverse the direction.

### Phase 4: options, global variables, and XML

Replace MSXML/COM with a Zig XML parser and writer implementing only the XML operations used by StreamIO: element traversal, named attributes, text values, array/item structure, and UTF-8/UTF-16 conversion required by legacy content. Replace `_bstr_t` and COM option handling with explicit string conversion. Preserve option defaults, repair behavior, and configuration file output.

### Phase 5: cutover and staging guard

`build.zig` builds and stages the Zig StreamIO runtime artifact for x64. Staging verifies every required runtime DLL has the selected machine architecture before `run` starts `Game.exe`; it reports the name and architecture of any mismatch. The existing x86 StreamIO remains available only for the x86 target while the x64 target never stages it.

## Implementation boundaries

Zig owns algorithms, memory, filesystem access, ZIP/PAK decoding, XML parsing, and platform-specific implementations. The C++ bridge is intentionally small and contains only ABI declarations, vtable shims, and conversions that cannot be expressed safely in Zig alone. Gameplay, UI, graphics, and module-loading policy remain outside this replacement.

The first phase does not attempt to port `Scene`, `AILogic`, or `GameTT`. It produces a validated x64 StreamIO foundation that those modules can consume during their own migrations.

## Validation

Every phase is test-first. Zig unit tests cover memory safety and format codecs. C++ integration tests load the produced DLL through `LoadLibrary` and invoke each exported hook. x86 and x64 compatibility tests use identical fixtures; the x64 test process must not load an x86 DLL.

The final acceptance command for this project is:

```powershell
zig build run '-Dtarget=x86_64-windows-msvc' '-Doptimize=ReleaseFast'
```

It must stage architecture-consistent runtime binaries, launch the game, load StreamIO successfully, enter the main menu, and exit normally.

## Non-goals

- Translating the old StreamIO C++ source line-for-line.
- Keeping MSXML, COM XML smart pointers, or `_bstr_t`.
- Changing legacy game data formats to Zig-native layouts.
- Making the remaining x86 runtime modules load inside an x64 process.
