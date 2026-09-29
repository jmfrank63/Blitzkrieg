---
status: testing
phase: 03-map-editor-plan-6-finish-m1
source: [03-VERIFICATION.md]
started: 2026-09-29T09:25:27Z
updated: 2026-09-29T09:25:27Z
---

## Current Test

number: 1
name: A reopened map comes back with its camera and zoom (D-15)
expected: |
  Open map A, scroll and zoom in 2-3 steps, open map B, then reopen A (File > Open Recent).
  A comes back at the scroll position and zoom it was left at; a map opened for the first time shows its middle unzoomed.
awaiting: user response

## Tests

### 1. A reopened map comes back with its camera and zoom (D-15)
expected: A comes back at the scroll position and zoom it was left at; a first-time map shows its middle unzoomed
result: [pending]

### 2. Crash recovery is offered at the next start
expected: With autosave at 1 minute, edit a map, wait for the recovery copy, kill the editor (kill -9), start it again: a dialog names the map and offers Open / Discard / Later; Open restores the edits, Discard deletes the copy, Later asks again next start
result: [pending]

## Summary

total: 2
passed: 0
issues: 0
pending: 2
skipped: 0
blocked: 0

## Gaps
