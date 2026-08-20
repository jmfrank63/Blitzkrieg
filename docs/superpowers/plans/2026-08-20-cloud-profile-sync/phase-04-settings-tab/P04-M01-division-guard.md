# P04-M01 — division constants and bounds guard

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Repair the stale tab constants and stop a future division from indexing a missing UI element.

**Dependencies:** P03-M04.

**Allowed files:** `Sources/src/GameTT/InterfaceOptionsSettings.cpp`, `Sources/src/GameTT/InterfaceOptionsSettings.h`.

- [ ] Write the failing case first: force a synthetic sixth and seventh division and observe the current behaviour before changing anything.
- [ ] Correct `_E_BUTTON_CHANGE_DIVISION_END` from `10009` to `10012` and `_E_LIST_END` from `1002` to `1005`, matching the six button and six list elements actually present in `Data/UI/OptionsSettings.xml`. Both constants are currently unused, so they are documentation that is simply wrong.
- [ ] Add a bounds check in `CInterfaceOptionsSettings::Create()`: the loop indexes `_E_BUTTON_CHANGE_DIVISION_BEGIN + nMaxDivision` and `_E_LIST_BEGIN + nMaxDivision` with no guard, so a seventh division reaches `checked_cast` on a null child.
- [ ] On overflow, skip the extra division and emit a diagnostic naming it, rather than asserting — a mod adding options must not make the settings screen unopenable.
- [ ] This file carries CP1251 comments. After editing, restore any clobbered comment lines from `git show HEAD:Sources/src/GameTT/InterfaceOptionsSettings.cpp`, matching by ASCII skeleton and keeping CRLF.
- [ ] Commit checkpoint: `settings: correct division bounds and guard the tab index`.

**Evidence:** The synthetic seventh division is skipped with a diagnostic and the screen still opens; verified headlessly with `cmd=0x100e00d8`.
