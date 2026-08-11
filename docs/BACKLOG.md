# Backlog

Agreed-on ideas and deferred work, not scheduled yet. Add new items at the
bottom of their section; delete items when they ship (git history keeps them).

## Open

- **Mouse wheel adjusts slider rows.** Turning the wheel over a slider row in
  the options (gamma, particle density, volumes) should change the slider's
  value. The other half of this item — wheel scrolls lists, quarter turn
  travels the whole list — shipped 2026-08-12. (Requested 2026-08-11.)

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

## Ruled out

- **Hold-a-letter-key + mouse to scroll the map** (tried with G, 2026-08-12):
  macOS press-and-hold intercepts held letter keys for the accent picker, so
  letter+mouse chords fight the OS. Cursor keys and edge scrolling remain the
  way to scroll. Revisit only with a non-letter modifier or a mouse button.
