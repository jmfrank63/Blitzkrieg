# Phase 5 deferred items

Out-of-scope discoveries logged by the executors (not fixed in the plan that found them).

## From 05-04

- **Older engine tests leave scratch files behind on macOS.** The pre-05-04 tests of
  `tools/zig/editor_bridge_test.cpp` call `remove()` on scratch paths joined with `"\\"`
  (the engine's own separator), which POSIX `remove()` does not split, so the files stay in
  `zig-out/local-test`. 05-04 routed its own three tests (`TestM3MultiSelect`,
  `TestM3PropertiesAndLinks`, `TestM3Damage`) through `OsPath`, as the M2 tests already do;
  the rest are untouched (harmless: the folder is the tests' own scratch space).
- **Ctrl+click and Alt+click are not scriptable in BK_EDITOR_AUTO.** `click=` carries no
  modifiers, so the m3-auto scenario reaches the Ctrl rubber band through `band_select` and
  the Damage tool's repair through `damage:<object>:repair`; the gestures themselves are
  proven by the core Selector tests and the view test
  `the Damage tool - left damages, right heals, Alt+left and the middle button repair to full`.
- **The direction wheel's hot area is the filter row's height.** The dial is drawn 40 px
  tall beside the palette's filter input but the invisible button is one frame high, so
  only its upper half takes a drag (kept so the M1 reference frames' row heights hold).
- **The flag swap's engine re-placement.** `BkEditorSetObjectFields` renames a re-owned
  flag's record (Flag_<party>) but the engine object keeps its old type until the map is
  reopened; the record (what is saved) is right.
- **TestM3Fields' touched-cell proof flakes.** One run of `test-editor-bridge` during 05-04
  printed `the fields' tiles change exactly the cells the engine's own fill touches (63
  touched, 1 disagree)` - the final-scanline replay gap already open in `.planning/WINDOWS.md`
  (entry 4, from 05-03); the next runs passed.
