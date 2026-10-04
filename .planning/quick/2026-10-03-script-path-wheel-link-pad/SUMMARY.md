# Quick task 2026-10-03: script path, wheel delta, link padding

Base c814ff6cb. Commits:
- f54732c46 map script paths relative to the map's folder; loaders expand them
- 5c50d8f27 direction wheel turns the selection by the delta
- 1ead27a0d SLinkInfo pad bytes zeroed

## Part 1 - script paths (user ruling 2026-10-03: cloud-synced saves)
- Base is the map's own folder. The game has only ever loaded `<map folder>/<last component of szScriptFile>.lua` (iMissionInternal, GameCreation), so a script beside its map is stored as its bare name.
- `NMapScriptPath` (Sources/src/Formats/fmtMapScriptPath.h): `ToStored` cuts an absolute value (drive, root or share) to its last component and leaves anything else untouched. `BesideMap`/`ExpandOnLoad` expand on load, splitting on either separator.
- Writers: CreateRandomMap stores the map's name; the bridge's save turns an absolute value into its name. Readers: the bridge reads an absolute value as its name; iMissionInternal and GameCreation accept '/'. The check-sum reads (GameCreation, CommandsHistory) expand a relative value.
- Shipped maps: 66/66 still round-trip through NMapFile. intro_allies and intro_ussr hold `C:\a7\...` and become relative when saved through the bridge.
- Accepted risk: 11 shipped maps store a bare script name and their check sum now includes the script. Peers on one build agree. A replay from an older build on those maps could report a bad map in a final-release build.
- Proofs:
  - one seed generated into two folders gives byte-identical files;
  - a moved map plus its .lua runs its script in the real Game;
  - a Windows drive path and a macOS root path save to the same bytes.
- WINDOWS.md entry 5 is fixed.

## Part 2 - wheel by delta (user ruling: overrides MFC set-to-angle)
- `Editor.rotateSelection`: each member turns from its direction at the start of the drag by the whole turn since. One drag is one undo step; pressing only grabs the dial. Pure maths: `wheelDeltaDegrees` and `wheelDegreesOfDirection` in panels_logic.
- Tests:
  - a core test;
  - TestM3PropertiesAndLinks: two units each turned a quarter, undone byte for byte;
  - m3-auto frames 291-317. No reference shots were re-seeded.
- Docs: PARITY O6 and the 05-04 SUMMARY note were updated.

## Part 3 - SLinkInfo padding
- `BYTE cReserved[3]` is zeroed in the constructor, with a static_assert of 12 bytes; format unchanged. New test TestLinkInfoBytes, which fails without the fix. WINDOWS.md entry 9 is fixed.

## Gates (macOS arm64)
- These pass: zig build test, test-map-files, test-editor-bridge, map-editor-game-reads-it-m3, test-rmg-determinism, and hermeticity.
- map-editor-m3-auto: the M3 scenario passes (590 actions), but only with the M1/M2 chain dropped. The chain's first step, the visible-window M1 smoke, needs the real pointer over the window. That flake is known and unrelated.
- CI run 37083716241.
