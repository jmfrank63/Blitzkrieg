# P08-M02 PlatformRuntime Link Evidence

`zig test tools/zig/platform_linkage_test.zig` passed 8/8 including target
runtime names, one shared dependency path, and rejection of absolute cache
paths. `zig test tools/zig/verify_runtime.zig` passed 2/2. The staged runtime
manifest now contains exactly one target-correct name:

| Target | Game | PlatformRuntime |
|---|---|---|
| Windows x64 | `Game.exe` | `PlatformRuntime.dll` |
| Linux x64 | `Game` | `libPlatformRuntime.so` |
| macOS arm64 | `Game` | `libPlatformRuntime.dylib` |

Clock and socket implementations were removed from the Misc static archive;
the shared Misc dependency carries PlatformRuntime, and the StreamIO and
options bridge dynamic modules link it explicitly. Windows `game-all`
compilation passed after this change.
