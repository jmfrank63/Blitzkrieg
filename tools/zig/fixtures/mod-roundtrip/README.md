# mod-roundtrip fixture

A tiny stand-in for a third-party mod's `data` folder, for `zig build test-resource-mod-roundtrip`.
Every file is a copy of a tracked file below `Data/`, lower-cased path as a mod would have it; no file of
a real third-party mod is ever copied into the repository. The tier runs over this folder first, then over
`BK_MOD_ROOT` (or `-Dmod-root`) when one is given.

Some files are hand-made, or edited copies, because they stand for what a real mod does and Data/ does not. Each
stands for a port bug S17/T03 fixed (D054), and the tier fails if its fix is taken out:

- `rivers3d/river_test.xml`: a river with minimap colours, a type and a priority (the river frame holds no item for them).
- `roads3d/road_aimask.xml`: the road of Data/ with an AI class mask of 0, which the four passability flags cannot say.
- `scenarios/campaigns/test/test.xml`: a campaign whose map picture sits under `scenarios\custom`, not beside the stats.
- `units/humans/german/stub_soldier/1.xml`: the gunner of Data/ with its KeyName emptied (a valid value).
- `objects/terraobjects/stub_stone/01/1.xml`: an object stub of the older form, with no Defence nodes or sounds (the
  reader's defaults, 40 to 90 armor, need more than the frame's one armor item).
- `fences/ussr/summer/townfence_vis/1.xml`: the town fence of Data/ whose second segment sees two of its three cells,
  a tile away from the corner the editor camera's lattice puts it at.
- `units/technics/german/artillery/mortar_stub/`: the 8 cm mortar of Data/ with its AABBCenter edited, so its stats are
  not what the models beside them say (1.mod and 2.mod are the mortar's own).

The medal, mission, chapter and campaign of Data/ carry no source picture here, so their picture rectangles come from
the stats the import kept.
