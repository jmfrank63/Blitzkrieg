# Backlog

Agreed-on ideas and deferred work, not scheduled yet. Add new items at the
bottom of their section; delete items when they ship (git history keeps them).

## Open

- **Mouse wheel adjusts slider rows.** Turning the wheel over a slider row in
  the options (gamma, particle density, volumes) should change the slider's
  value. The other half of this item — wheel scrolls lists, quarter turn
  travels the whole list — shipped 2026-08-12. (Requested 2026-08-11.)

- **Zooming in and out of the game.** Zoom the mission view in and out
  (mouse wheel is the natural control; the camera already has a
  `camera_zoom` slider bound to the wheel, but the distance is effectively
  fixed today). Needs sensible min/max limits and should compose with the
  fixed-resolution presentation. (Requested 2026-08-12.)

- **Windows parity pass.** Deferred while macOS is brought up first
  ("we will worry about windows once we have mac correct", 2026-08-11).
  Collects everything the macOS rounds changed that Windows still needs.
  Build verification and the frame-pacing measurement shipped 2026-08-19
  (release build compiles, test suite passes, startup smoke and an
  80-second save-loaded mission run exit clean; the limiter held exactly
  60 with the millisecond sleep + spin at ~5% of a core, and the
  high-resolution waitable timer took that spin away — 29.9% → 24.4% of a
  core on the menu — with pacing still exact and game/wall at 1.0000).
  Still open:
  - `windowDisplayChanged` platform events are not emitted on the Windows
    path, so OS display-move following will not work there yet.
  - The window→game mouse transform (GFX.Present.*) is applied in the
    non-Windows event pump only; the Windows WndProc path needs the same.
  - App icon: Game.rc is not wired into the zig build.

- **Observe the particle-bucket map does not grow across missions.**
  `CDrawVisitor::Clear` drops the per-texture particle buckets that were
  still empty on arrival, once every 300 frames. That particles still draw
  through the sweep is confirmed (2026-08-20, Windows: a quicksave with a
  burning plane held particles=13 steady with bursts to 160 across ~14
  sweep cycles). What remains is the growth half — that the map really
  stops accumulating an entry per texture across mission loads — which has
  no external observable today; needs a bucket-count line in `BK_PERF` to
  check. (Found in review 2026-08-19.)

- **BK_DATA_TRACE cannot see a silent misparse.** `bk_tree_int` traces a
  value that fails to parse, which catches text with a hex letter in it
  ("0E000000"). The acknowledgement table broke on the other half of the
  class: "01000000" parses perfectly and wrongly as decimal 1000000, so
  nothing was reported until a UBSan enum check aborted startup. Catching
  the rest means flagging an 8-digit integer with a leading zero in an int
  field, which needs an allowlist for the legitimate ones. (Found
  2026-08-19.)

- **Save names with spaces cannot be passed on the command line.** RunGame
  re-joins argv with spaces before ProcessCommandLine, losing the original
  quoting, so `-Bug Not Present on Windows.sav` splits into five arguments
  and main.cpp's parser exits with the usage text. Space-free names work
  (`-bugsave.sav`). Either re-quote arguments containing spaces at the
  join, or parse argv directly. (Found 2026-08-20.)

- **Verify the launch splash on macOS and Linux.** The Blitzkrieg logo
  between launch and the first video was Windows-only (a Win32 dialog in
  WinFrame.cpp; the SDL path had an empty stub). SDLApplication now shows
  the same picture - a borderless, centered, always-on-top 600x352 window
  blitting Data/splash.bmp (the IDB_SPLASH resource bitmap, staged like
  icon.bmp) - from before SDL_Init until the game window is up, with its
  own refcounted video-subsystem reference. Compile-verified for
  x86_64-linux-gnu and inert on Windows (startup smoke passes); needs one
  launch on each platform to confirm the window actually appears and goes
  away. (Found 2026-08-20.)

- **test-game-frame and test-game-loop do not build on Windows.** Both
  compile Platform/Debug.cpp, whose PlatformClient.h include of
  "PlatformABI/platform_c.h" is not on those targets' include paths
  (Sources/src is missing from the list), so the steps fail before running
  - on clean main, unrelated to any recent change. The zig build test
  suite does not include them, which is why it passes. Add the include
  path or drop Debug.cpp from the lists. (Found 2026-08-20.)

- **Verify garrison bars in live play on Intel macOS.** The green enemies
  returned in every fresh mission because the load-time re-lock only heals
  saves: the live boarding path never locked on macOS at all.
  CMOBuilding::Load reached the bar through the tree's only two
  dynamic_cast<ISceneIconBar*> - a cast on an object created in Scene,
  across the module boundary. The MSVC ABI compares RTTI name strings, so
  Windows always locked; the Itanium ABI compares typeinfo identity, the
  copies in the two modules do not unify, the cast returned null, and the
  lock was silently skipped - which is also why Mac-written saves carried
  locked=false. Both sites now use static_cast like every other
  ICON_HP_BAR access (Windows regression identical). Verify on the Mac:
  garrisoned enemies red in a fresh mission, red after save/load of a
  Mac-written save. Standing hazard to remember: any dynamic_cast on an
  object created in another module is a silent null on macOS/Linux.
  (Found 2026-08-20.)

## Ruled out

- **Hold-a-letter-key + mouse to scroll the map** (tried with G, 2026-08-12):
  macOS press-and-hold intercepts held letter keys for the accent picker, so
  letter+mouse chords fight the OS. Cursor keys and edge scrolling remain the
  way to scroll. Revisit only with a non-letter modifier or a mouse button.

- **Yielding inside the frame limiter's spin** (measured 2026-08-19):
  `std::this_thread::yield` and the ARM `yield` hint do not block, so the
  core stays busy for the whole remaining interval and only the syscall
  count grows. Measured 0.6-1.2% of a core against 1.0-1.4% for the tight
  spin — inside the run-to-run noise — and frame-to-frame jitter got worse.
  Blocking on a high-resolution timer is what actually helps.

- **An adaptive spin margin for the frame limiter** (measured 2026-08-19):
  sleeping to a measured overshoot short of the deadline instead of a fixed
  millisecond was 5-10x worse than the code it replaced (4.7-8.7% of a core
  against 1.0-1.4%). The overshoot is variable, so every early wake-up has
  to spin out the variance. Also worth knowing: Darwin coalesces `nanosleep`
  to a millisecond or more whatever unit `sleep_for` is handed, so no amount
  of tuning around `sleep_for` reaches the deadline accurately.
