---
status: complete
phase: 03-map-editor-plan-6-finish-m1
source: [03-VERIFICATION.md]
started: 2026-09-29T09:25:27Z
updated: 2026-09-29T16:11:33Z
---

## Current Test

[testing complete]

## Tests

### 1. A reopened map comes back with its camera and zoom (D-15)
expected: A comes back at the scroll position and zoom it was left at; a first-time map shows its middle unzoomed
result: pass (Johannes 2026-09-29: "The reopening starts exactly at the old position")

### 2. Crash recovery is offered at the next start
expected: With autosave at 1 minute, edit a map, wait for the recovery copy, kill the editor (kill -9), start it again: a dialog names the map and offers Open / Discard / Later; Open restores the edits, Discard deletes the copy, Later asks again next start
result: pass (Johannes 2026-09-29: "Crash recovery worked perfectly")

## Summary

total: 2
passed: 2
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

Also confirmed by Johannes on real Windows (win-home, RDP): the release MapEditor.exe edits, test-launches, restarts without the exit popup, and starts from outside its installation directory (after 296118b26/8ee15cb2e).
