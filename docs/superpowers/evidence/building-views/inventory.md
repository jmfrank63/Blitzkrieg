# Building inventory and side convention

Written by `bk-views inventory Data/Buildings <out>` (`tools/zig/bk_views.zig`), the same data as
`tools/zig/fixtures/bk-views/inventory.json`; `test-bk-views` fails when either differs from what the
tool writes. 115 families, 194 variant folders.

**Sides.** Side k + 1 is side k turned by the quarter turn that takes world (x, y) to (-y, x) about the
zero cross: the footprint's front corner goes to the right corner, the left wall becomes the right
wall and every 1.xml Direction grows by 90 (`turnPoint`, `turnGrid`, `turnXml`). A suffix `_1` .. `_4`
names that side where the variants of a family are turns of one building; `_1_2` is colourway 2 of
side 1; a folder without a suffix, or a numbered subfolder (`dot/01`), starts at side 1. Each
variant's side is measured against the family's first variant (`how`): `same view` when both visible
walls correlate at 0.5 or more and the outlines overlap 0.75 or more (it repeats that view with other
textures or details); otherwise the footprints say whether the turn is even or odd (`outline` when
they fit both ways and the outlines decide), an even turn is a half turn, and an odd one goes to the
footprint that fits better (`footprint`), else to the wall the two views share (`walls`);
`unresolved` when nothing tells.

| Family | Region | Variants (folder under Data/Buildings: side, how) | Missing sides | Colourways | Winter |
|---|---|---|---|---|---|
| a_cisterns | africa | `africa/summer/a_cisterns/01`: 1; `africa/summer/a_cisterns/02`: 1 (same view) | 2, 3, 4 | - | no |
| a_h01 | africa | `africa/summer/a_h01_1`: 1; `africa/summer/a_h01_2`: 2 (footprint) | 3, 4 | - | no |
| a_h02 | africa | `africa/summer/a_h02_2`: 2; `africa/summer/a_h02_4`: 4 (footprint) | 1, 3 | - | no |
| a_h03 | africa | `africa/summer/a_h03_2`: 2; `africa/summer/a_h03_3`: 3 (footprint) | 1, 4 | - | no |
| a_h04 | africa | `africa/summer/a_h04_1`: 1 | 2, 3, 4 | - | no |
| a_h07 | africa | `africa/summer/a_h07_1`: 1; `africa/summer/a_h07_2`: 2 (footprint); `africa/summer/a_h07_3`: 3 (footprint) | 4 | - | no |
| a_h08 | africa | `africa/summer/a_h08_1`: 1; `africa/summer/a_h08_4`: 4 (walls) | 2, 3 | - | no |
| a_h09 | africa | `africa/summer/a_h09_2`: 2 | 1, 3, 4 | - | no |
| a_mosque_01 | africa | `africa/summer/a_mosque_01_4`: 4 | 1, 2, 3 | - | no |
| a_mosque_02 | africa | `africa/summer/a_mosque_02_1`: 1 | 2, 3, 4 | - | no |
| a_wb | africa | `africa/summer/a_wb/01`: 1; `africa/summer/a_wb/02`: ? (unresolved) | 2, 3, 4 | - | no |
| a_wt | africa | `africa/summer/a_wt/01`: 1; `africa/summer/a_wt/02`: 4 (footprint) | 2, 3 | - | no |
| conduitbarrel | common | `common/summer/conduitbarrel`: 1 | 2, 3, 4 | - | yes |
| dot | common | `common/summer/dot/01`: 1; `common/summer/dot/02`: 3 (footprint); `common/summer/dot/03`: 4 (walls); `common/summer/dot/04`: 4 (walls) | 2 | - | yes |
| dot_h | common | `common/summer/dot_h/01`: 1; `common/summer/dot_h/02`: 2 (footprint); `common/summer/dot_h/03`: 3 (footprint); `common/summer/dot_h/04`: 4 (walls) |  | - | yes |
| dzot | common | `common/summer/dzot/01`: 1; `common/summer/dzot/02`: ? (unresolved); `common/summer/dzot/03`: ? (same view); `common/summer/dzot/04`: 3 (outline) | 2, 4 | - | yes |
| tent | common | `common/summer/tent/01`: 1; `common/summer/tent/02`: 2 (walls) | 3, 4 | - | yes |
| watchtower | common | `common/summer/watchtower`: 1 | 2, 3, 4 | - | yes |
| watertankbig | common | `common/summer/watertankbig`: 1 | 2, 3, 4 | - | yes |
| watertanklittle | common | `common/summer/watertanklittle`: 1 | 2, 3, 4 | - | yes |
| e_chapel | europe | `europe/summer/e_chapel_1`: 1 | 2, 3, 4 | - | no |
| e_cityhall | europe | `europe/summer/e_cityhall_1`: 1 | 2, 3, 4 | - | no |
| e_ctownhouse01 | europe | `europe/summer/e_ctownhouse01_1`: 1; `europe/summer/e_ctownhouse01_2`: 2 (footprint); `europe/summer/e_ctownhouse01_4`: 4 (footprint) | 3 | - | no |
| e_ctownhouse02 | europe | `europe/summer/e_ctownhouse02_1`: 1; `europe/summer/e_ctownhouse02_1_2`: 1 (same view); `europe/summer/e_ctownhouse02_4`: 4 (footprint) | 2, 3 | `e_ctownhouse02_1_2` | no |
| e_ctownhouse03 | europe | `europe/summer/e_ctownhouse03_2`: 2; `europe/summer/e_ctownhouse03_4`: 4 (footprint) | 1, 3 | - | no |
| e_ctownhouse04 | europe | `europe/summer/e_ctownhouse04_1`: 1; `europe/summer/e_ctownhouse04_1_2`: 1 (same view) | 2, 3, 4 | `e_ctownhouse04_1_2` | no |
| e_frenchshed01 | europe | `europe/summer/e_frenchshed01`: 1 | 2, 3, 4 | - | no |
| e_frenchshed02 | europe | `europe/summer/e_frenchshed02`: 1 | 2, 3, 4 | - | no |
| e_frenchshed03 | europe | `europe/summer/e_frenchshed03`: 1 | 2, 3, 4 | - | no |
| e_frenchshed04 | europe | `europe/summer/e_frenchshed04`: 1 | 2, 3, 4 | - | no |
| e_frenchshed05 | europe | `europe/summer/e_frenchshed05`: 1 | 2, 3, 4 | - | no |
| e_frenchshed06 | europe | `europe/summer/e_frenchshed06`: 1 | 2, 3, 4 | - | no |
| e_house01 | europe | `europe/summer/e_house01_1`: 1; `europe/summer/e_house01_2`: 3 (outline); `europe/summer/e_house01_3`: 1 (same view) | 2, 4 | - | no |
| e_house02 | europe | `europe/summer/e_house02_1`: 1; `europe/summer/e_house02_2`: 1 (same view); `europe/summer/e_house02_4`: 4 (walls) | 2, 3 | - | no |
| e_house03 | europe | `europe/summer/e_house03_2`: 2; `europe/summer/e_house03_3`: 4 (footprint); `europe/summer/e_house03_4`: 4 (same view) | 1, 3 | - | no |
| e_house04 | europe | `europe/summer/e_house04_4`: 4 | 1, 2, 3 | - | no |
| e_house05 | europe | `europe/summer/e_house05_3`: 3 | 1, 2, 4 | - | no |
| e_house06 | europe | `europe/summer/e_house06_4`: 4 | 1, 2, 3 | - | no |
| e_house07 | europe | `europe/summer/e_house07_1`: 1 | 2, 3, 4 | - | no |
| e_house08 | europe | `europe/summer/e_house08_1`: 1 | 2, 3, 4 | - | no |
| e_house09 | europe | `europe/summer/e_house09_1`: 1 | 2, 3, 4 | - | no |
| e_house10 | europe | `europe/summer/e_house10_2`: 2 | 1, 3, 4 | - | no |
| e_house11 | europe | `europe/summer/e_house11_1`: 1; `europe/summer/e_house11_3`: 3 (footprint); `europe/summer/e_house11_4`: 4 (footprint) | 2 | - | no |
| e_house12 | europe | `europe/summer/e_house12_3`: 3 | 1, 2, 4 | - | no |
| e_jettybuilding | europe | `europe/summer/e_jettybuilding_1`: 1; `europe/summer/e_jettybuilding_4`: 2 (footprint) | 3, 4 | - | no |
| e_monument | europe | `europe/summer/e_monument`: 1 | 2, 3, 4 | - | no |
| e_railwaystation | europe | `europe/summer/e_railwaystation_1`: 1; `europe/summer/e_railwaystation_2`: ? (unresolved) | 2, 3, 4 | - | no |
| e_shed01 | europe | `europe/summer/e_shed01_2`: 2; `europe/summer/e_shed01_3`: 3 (footprint) | 1, 4 | - | no |
| e_shed02 | europe | `europe/summer/e_shed02_2`: 2; `europe/summer/e_shed02_3`: ? (unresolved); `europe/summer/e_shed02_4`: 4 (footprint); `europe/summer/e_shed02_5`: 2 (same view) | 1, 3 | - | no |
| e_statue | europe | `europe/summer/e_statue/01`: 1; `europe/summer/e_statue/02`: 2 (walls); `europe/summer/e_statue/04`: 2 (walls) | 3, 4 | - | no |
| e_stella | europe | `europe/summer/e_stella/01`: 1; `europe/summer/e_stella/02`: 1 (same view) | 2, 3, 4 | - | no |
| e_stubtrack | europe | `europe/summer/e_stubtrack/01`: 1; `europe/summer/e_stubtrack/02`: ? (unresolved); `europe/summer/e_stubtrack/03`: ? (unresolved); `europe/summer/e_stubtrack/04`: ? (unresolved) | 2, 3, 4 | - | no |
| e_townhouse01 | europe | `europe/summer/e_townhouse01_3`: 3; `europe/summer/e_townhouse01_4`: 4 (walls) | 1, 2 | - | no |
| e_townhouse02 | europe | `europe/summer/e_townhouse02_2`: 2; `europe/summer/e_townhouse02_3`: 1 (walls) | 3, 4 | - | no |
| e_townhouse03 | europe | `europe/summer/e_townhouse03_1`: 1; `europe/summer/e_townhouse03_4`: ? (unresolved) | 2, 3, 4 | - | no |
| e_townhouse04 | europe | `europe/summer/e_townhouse04_3`: 3; `europe/summer/e_townhouse04_4`: 4 (walls) | 1, 2 | - | no |
| e_townrailwaystation | europe | `europe/summer/e_townrailwaystation_4`: 4 | 1, 2, 3 | - | no |
| e_waterset | europe | `europe/summer/e_waterset/01`: 1; `europe/summer/e_waterset/02`: ? (unresolved); `europe/summer/e_waterset/03`: ? (unresolved); `europe/summer/e_waterset/04`: ? (unresolved) | 2, 3, 4 | - | no |
| e_wb | europe | `europe/summer/e_wb/01`: 1; `europe/summer/e_wb/02`: 4 (walls) | 2, 3 | - | no |
| e_wellbig | europe | `europe/summer/e_wellbig/01`: 1; `europe/summer/e_wellbig/03`: 3 (outline) | 2, 4 | - | no |
| e_welllittle | europe | `europe/summer/e_welllittle/01`: 1; `europe/summer/e_welllittle/02`: 1 (same view) | 2, 3, 4 | - | no |
| e_wt | europe | `europe/summer/e_wt/01`: 1; `europe/summer/e_wt/02`: 3 (footprint) | 2, 4 | - | no |
| admhouse | ussr | `ussr/summer/admhouse`: 1 | 2, 3, 4 | - | yes |
| barracks | ussr | `ussr/summer/barracks`: 1; `ussr/summer/barracks_2`: ? (unresolved) | 2, 3, 4 | - | yes |
| chimneystalkbig | ussr | `ussr/summer/chimneystalkbig`: 1 | 2, 3, 4 | - | yes |
| chimneystalklittle | ussr | `ussr/summer/chimneystalklittle`: 1 | 2, 3, 4 | - | yes |
| church | ussr | `ussr/summer/church`: 1 | 2, 3, 4 | - | yes |
| cistern | ussr | `ussr/summer/cistern/01`: 1; `ussr/summer/cistern/02`: ? (unresolved) | 2, 3, 4 | - | yes |
| dustbin | ussr | `ussr/summer/dustbin/01`: 1; `ussr/summer/dustbin/02`: 4 (walls); `ussr/summer/dustbin/03`: 3 (footprint) | 2 | - | yes |
| electricsubstation | ussr | `ussr/summer/electricsubstation`: 1 | 2, 3, 4 | - | yes |
| factoryboiler | ussr | `ussr/summer/factoryboiler`: 1 | 2, 3, 4 | - | yes |
| factoryfloor01 | ussr | `ussr/summer/factoryfloor01`: 1 | 2, 3, 4 | - | yes |
| factoryfloor02 | ussr | `ussr/summer/factoryfloor02`: 1 | 2, 3, 4 | - | yes |
| factoryfloor03 | ussr | `ussr/summer/factoryfloor03`: 1 | 2, 3, 4 | - | yes |
| factoryoffice | ussr | `ussr/summer/factoryoffice`: 1 | 2, 3, 4 | - | yes |
| house01 | ussr | `ussr/summer/house01`: 1 | 2, 3, 4 | - | yes |
| house02 | ussr | `ussr/summer/house02`: 1 | 2, 3, 4 | - | yes |
| house03 | ussr | `ussr/summer/house03`: 1 | 2, 3, 4 | - | yes |
| house04 | ussr | `ussr/summer/house04`: 1 | 2, 3, 4 | - | yes |
| house05 | ussr | `ussr/summer/house05`: 1 | 2, 3, 4 | - | yes |
| house06 | ussr | `ussr/summer/house06`: 1 | 2, 3, 4 | - | yes |
| house07 | ussr | `ussr/summer/house07`: 1 | 2, 3, 4 | - | yes |
| house08 | ussr | `ussr/summer/house08`: 1 | 2, 3, 4 | - | yes |
| house09 | ussr | `ussr/summer/house09`: 1 | 2, 3, 4 | - | yes |
| house10 | ussr | `ussr/summer/house10`: 1 | 2, 3, 4 | - | yes |
| house11 | ussr | `ussr/summer/house11`: 1 | 2, 3, 4 | - | yes |
| house12 | ussr | `ussr/summer/house12`: 1 | 2, 3, 4 | - | yes |
| housebig | ussr | `ussr/summer/housebig`: 1 | 2, 3, 4 | - | yes |
| kiosk | ussr | `ussr/summer/kiosk/01`: 1; `ussr/summer/kiosk/02`: ? (unresolved); `ussr/summer/kiosk/03`: 1 (same view); `ussr/summer/kiosk/04`: ? (same view) | 2, 3, 4 | - | yes |
| leaders | ussr | `ussr/summer/leaders/01`: 1; `ussr/summer/leaders/02`: 2 (walls) | 3, 4 | - | yes |
| lenin | ussr | `ussr/summer/lenin`: 1 | 2, 3, 4 | - | yes |
| pigeonloft | ussr | `ussr/summer/pigeonloft/01`: 1; `ussr/summer/pigeonloft/02`: 4 (walls) | 2, 3 | - | yes |
| railroadbighouse | ussr | `ussr/summer/railroadbighouse`: 1 | 2, 3, 4 | - | yes |
| railroadlittlehouse | ussr | `ussr/summer/railroadlittlehouse`: 1 | 2, 3, 4 | - | yes |
| railwaystation | ussr | `ussr/summer/railwaystation`: 1 | 2, 3, 4 | - | yes |
| shed01 | ussr | `ussr/summer/shed01`: 1 | 2, 3, 4 | - | yes |
| shed02 | ussr | `ussr/summer/shed02`: 1 | 2, 3, 4 | - | yes |
| storehouse | ussr | `ussr/summer/storehouse_1`: 1; `ussr/summer/storehouse_2`: 2 (walls) | 3, 4 | - | yes |
| stubtrack | ussr | `ussr/summer/stubtrack/01`: 1; `ussr/summer/stubtrack/02`: ? (unresolved); `ussr/summer/stubtrack/03`: 3 (footprint); `ussr/summer/stubtrack/04`: ? (unresolved) | 2, 4 | - | yes |
| towncornhouse01 | ussr | `ussr/summer/towncornhouse01`: 1 | 2, 3, 4 | - | yes |
| towncornhouse02 | ussr | `ussr/summer/towncornhouse02`: 1 | 2, 3, 4 | - | yes |
| towncornhouse03 | ussr | `ussr/summer/towncornhouse03`: 1 | 2, 3, 4 | - | yes |
| towncornhouse04 | ussr | `ussr/summer/towncornhouse04`: 1 | 2, 3, 4 | - | yes |
| townhouse01 | ussr | `ussr/summer/townhouse01_1`: 1; `ussr/summer/townhouse01_2`: ? (unresolved) | 2, 3, 4 | - | yes |
| townhouse02 | ussr | `ussr/summer/townhouse02_1`: 1; `ussr/summer/townhouse02_2`: 2 (footprint) | 3, 4 | - | yes |
| townhouse03 | ussr | `ussr/summer/townhouse03_1`: 1; `ussr/summer/townhouse03_2`: 2 (walls) | 3, 4 | - | yes |
| townhouse04 | ussr | `ussr/summer/townhouse04_1`: 1; `ussr/summer/townhouse04_2`: ? (unresolved) | 2, 3, 4 | - | yes |
| townhouse05 | ussr | `ussr/summer/townhouse05_1`: 1; `ussr/summer/townhouse05_2`: 2 (walls); `ussr/summer/townhouse05_3`: 2 (same view) | 3, 4 | - | yes |
| ukrainehouse01 | ussr | `ussr/summer/ukrainehouse01_1`: 1; `ussr/summer/ukrainehouse01_2`: 2 (walls) | 3, 4 | - | yes |
| ukrainehouse02 | ussr | `ussr/summer/ukrainehouse02`: 1 | 2, 3, 4 | - | yes |
| ukrainehouse03 | ussr | `ussr/summer/ukrainehouse03_1`: 1; `ussr/summer/ukrainehouse03_2`: 2 (footprint) | 3, 4 | - | yes |
| warehouse | ussr | `ussr/summer/warehouse`: 1 | 2, 3, 4 | - | yes |
| watertower | ussr | `ussr/summer/watertower`: 1 | 2, 3, 4 | - | yes |
| wb | ussr | `ussr/summer/wb/01`: 1; `ussr/summer/wb/02`: 4 (walls) | 2, 3 | - | yes |
| wt | ussr | `ussr/summer/wt/01`: 1; `ussr/summer/wt/02`: 2 (footprint) | 3, 4 | - | yes |

## Where the measured side is not the suffix

- `europe/summer/e_house01_2`: suffix 2, measured side 3 (outline)
- `europe/summer/e_house01_3`: suffix 3, measured side 1 (same view as `europe/summer/e_house01_1`)
- `europe/summer/e_house02_2`: suffix 2, measured side 1 (same view as `europe/summer/e_house02_1`)
- `europe/summer/e_house03_3`: suffix 3, measured side 4 (footprint)
- `europe/summer/e_jettybuilding_4`: suffix 4, measured side 2 (footprint)
- `europe/summer/e_railwaystation_2`: suffix 2, measured nothing (unresolved)
- `europe/summer/e_shed02_3`: suffix 3, measured nothing (unresolved)
- `europe/summer/e_shed02_5`: suffix 5, measured side 2 (same view as `europe/summer/e_shed02_2`)
- `europe/summer/e_townhouse02_3`: suffix 3, measured side 1 (walls)
- `europe/summer/e_townhouse03_4`: suffix 4, measured nothing (unresolved)
- `ussr/summer/barracks_2`: suffix 2, measured nothing (unresolved)
- `ussr/summer/townhouse01_2`: suffix 2, measured nothing (unresolved)
- `ussr/summer/townhouse04_2`: suffix 2, measured nothing (unresolved)
- `ussr/summer/townhouse05_3`: suffix 3, measured side 2 (same view as `ussr/summer/townhouse05_2`)
