# Windows SDL fullscreen lifecycle

The focused Windows SDL application test initially appeared to hang during
fullscreen testing. Checkpoint tracing showed that both
`SDL_SetWindowFullscreen(true)` and `SDL_SetWindowFullscreen(false)` returned;
the process stalled only during termination.

Root cause: `test-platform-window` overrode the Windows executable entry point
with raw `main` while using the CRT libraries. Its C++/CRT termination path was
therefore invalid. The target now uses `mainCRTStartup`, links
`PlatformRuntime` and `PlatformClient`, and uses the same non-implicit libc
configuration as the other Windows platform tests.

Validation:

```text
zig build test-platform-window -Dtarget=x86_64-windows-msvc -Dtest-mode=run
exit code 0
```

The test covers initialization-failure injection, create/show/hide/resize,
fullscreen enter/leave, minimized-state inspection, idempotent shutdown, and
opaque borrowed-window cleanup. `zig build game-all
-Dtarget=x86_64-windows-msvc -Dtest-mode=compile` and the Windows platform
foundation gate also pass after adding `PlatformClient.cpp` to the canonical
`Misc`, `StreamIOOptionsAbi`, and `StreamIO` source graphs.
