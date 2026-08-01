# Successor platform migration

The Windows 11 x64 SDL_GPU/D3D12 renderer is the accepted baseline. The next
milestone ports the game platform around that renderer without introducing a
second renderer implementation.

## First successor milestone

Build an SDL3 platform bootstrap that opens the existing game window and runs
the existing renderer smoke test on native Linux. After that gate passes,
repeat the bootstrap and smoke gate on Apple Silicon/macOS.

## Scope

- Window creation, event loop, resize, minimize/restore, fullscreen, and native
  handle ownership through the existing opaque adapter boundary.
- Input, cursor, clipboard, text input, and controller mapping.
- Audio replacement and device lifecycle independent of Windows audio APIs.
- Filesystem paths, config/save locations, writable-user-data policy, and data
  packaging.
- Dialogs, process launch, crash reporting, and debugger/telemetry hooks.
- Networking and platform socket behavior.
- Package layout and install/update behavior on Linux and macOS.
- Shader format expansion from DXIL to SPIR-V and MSL while retaining one
  backend-neutral shader manifest and hash/length validation.
- Native Linux/macOS acceptance: startup, menu, representative mission,
  resize/minimize/restore, clean shutdown, and renderer live counts.

## Constraints

SDL_GPU remains the graphics abstraction. Platform-native code belongs in the
SDL3 bootstrap and adapter boundary; the Zig renderer core must not gain direct
D3D, Vulkan, Metal, X11, Cocoa, or Win32 graphics calls.
