# P03-M02 — Create the SDL Application and Owned Window

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Initialize SDL once and create/destroy one hidden application window on the main thread.

**Dependencies:** P03-M01.

**Allowed files:** `Sources/src/Platform/Event.h`, `Sources/src/Platform/SDLApplication.h`, `Sources/src/Platform/SDLApplication.cpp`, `tools/zig/platform_window_test.cpp`, `build.zig`.

- [ ] Test initialization failure injection, hidden window creation, title/size query, show/hide, double shutdown, and destruction order.
- [ ] Set app metadata, initialize required SDL subsystems, create the window hidden with high-pixel-density/resizable flags, and store no native platform handle.
- [ ] Expose only an opaque borrowed window value, dimensions, visibility, and status/error methods from the public header; include SDL headers only in `.cpp`.
- [ ] Ensure all SDL application/window calls assert main-thread execution and `SDL_Quit` occurs after window destruction.
- [ ] Run the hidden window test natively on Windows and Linux; compile macOS.
- [ ] Commit: `platform: let SDL own the game window`

**Evidence:** lifecycle trace proving init, create, destroy, quit order.
