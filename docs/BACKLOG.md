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
  Collects everything the macOS rounds changed that Windows still needs:
  - Build verification of all changes on a Windows machine (cross-compile
    from macOS is not possible — MSVC paths).
  - `windowDisplayChanged` platform events are not emitted on the Windows
    path, so OS display-move following will not work there yet.
  - The window→game mouse transform (GFX.Present.*) is applied in the
    non-Windows event pump only; the Windows WndProc path needs the same.
  - App icon: Game.rc is not wired into the zig build.
  - Frame pacing: `NPlatform::SleepPreciseNanoseconds` reports failure on
    Windows, so the limiter there still sleeps to a millisecond short of the
    deadline and spins out the rest. Windows 10 1803 and later have a
    high-resolution waitable timer
    (`CreateWaitableTimerExW` + `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`).
    Measure the limiter on Windows first — on macOS the precise wait took it
    from 1.0-1.4% of a core to 0.11-0.23% with no more jitter, but an
    unmeasured change to frame pacing is not an improvement.

- **Verify the particle-bucket sweep in a mission.** `CDrawVisitor::Clear`
  now drops the per-texture particle buckets that were still empty on
  arrival, once every 300 frames, so the map stops accumulating an entry per
  texture the process has ever drawn. Only the empty-map path has been
  exercised: `-m<name>.sav` answered "CICLoad::Exec open failed" in the
  headless setup even with the save in `profiles/<active>/saves/`, so no
  scene with live particles was ever driven. Get a mission running headless
  again, then confirm particles still draw and that the map does not grow
  across missions. (Found in review 2026-08-19.)

- **BK_DATA_TRACE cannot see a silent misparse.** `bk_tree_int` traces a
  value that fails to parse, which catches text with a hex letter in it
  ("0E000000"). The acknowledgement table broke on the other half of the
  class: "01000000" parses perfectly and wrongly as decimal 1000000, so
  nothing was reported until a UBSan enum check aborted startup. Catching
  the rest means flagging an 8-digit integer with a leading zero in an int
  field, which needs an allowlist for the legitimate ones. (Found
  2026-08-19.)

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
