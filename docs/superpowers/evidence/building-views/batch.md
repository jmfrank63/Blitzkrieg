# Building views batch

Every variant of `Data/Buildings` turned by one, two and three quarter turns with `bk-views side`, by
`zig build building-views-batch-<group>` into `zig-out/remastered/Buildings/<region>/summer/<variant>_r<k>/`
(a numbered subfolder `dot/01` gives `dot/01_r1`); each pack passed `bk-views-check` and decomposed back, or
the step failed. Per family: `<family>.md`. Written by `bk-views report`.

- Variants: 194; generated folders: 582 of 582; not generated: 0.
- Gate: the shape of every turn drawn by the blocks (its intact sprite's height 0.7..1.3 times the 2003
  one's, the blocks' ground 0.5..2.0 times the footprint's cells, their outline IoU 0.85 or more against
  the variant's 2003 picture, its protrusion at most 1.5 times the 2003 one's plus 0.03), the style
  of every generated picture, and the hold-out where the family has another side of the colourway, against
  the floors of `tools/zig/fixtures/bk-views/gate.json`: 194 of 194 variants pass; hold-out 43 of 43; 0
  fail on the shape; 0 more pass the measures but look wrong on the contact sheet (`bk-views contact`),
  listed in gate.json's `by_eye`. A failing variant is listed with its first reason, to be fixed by hand.
- 2003 sprite: 40 variants have turns drawn as their 2003 sprites, moved onto the turned footprint: thin
  objects (a 2003 protrusion of 0.15 or more) and small ones (4 footprint cells or fewer) whose blocks fail
  the shape, where the turn keeps the footprint (cells IoU 0.9 or more), or mirrored onto it on an odd
  turn of a rectangular footprint.
- Hand shapes: 56 variants are drawn on blocks authored by hand (`tools/zig/fixtures/bk-views/shapes`).

| Region | Family | Variant | Side | New sides | Gate | Hold-out | 2003 sprite |
|---|---|---|---|---|---|---|---|
| africa | a_cisterns | africa/summer/a_cisterns/01 | 1 | 2, 3, 4 | pass | - | - |
| africa | a_cisterns | africa/summer/a_cisterns/02 | 1 | 2, 3, 4 | pass | - | - |
| africa | a_h01 | africa/summer/a_h01_1 | 1 | 2, 3, 4 | pass | pass | - |
| africa | a_h01 | africa/summer/a_h01_2 | 2 | 3, 4, 1 | pass | pass | - |
| africa | a_h02 | africa/summer/a_h02_2 | 2 | 3, 4, 1 | pass | pass | - |
| africa | a_h02 | africa/summer/a_h02_4 | 4 | 1, 2, 3 | pass | pass | - |
| africa | a_h03 | africa/summer/a_h03_2 | 2 | 3, 4, 1 | pass | pass | - |
| africa | a_h03 | africa/summer/a_h03_3 | 3 | 4, 1, 2 | pass | pass | - |
| africa | a_h04 | africa/summer/a_h04_1 | 1 | 2, 3, 4 | pass | - | - |
| africa | a_h07 | africa/summer/a_h07_1 | 1 | 2, 3, 4 | pass | pass | - |
| africa | a_h07 | africa/summer/a_h07_2 | 2 | 3, 4, 1 | pass | pass | - |
| africa | a_h07 | africa/summer/a_h07_3 | 3 | 4, 1, 2 | pass | pass | - |
| africa | a_h08 | africa/summer/a_h08_1 | 1 | 2, 3, 4 | pass | pass | - |
| africa | a_h08 | africa/summer/a_h08_4 | 4 | 1, 2, 3 | pass | pass | - |
| africa | a_h09 | africa/summer/a_h09_2 | 2 | 3, 4, 1 | pass | - | - |
| africa | a_mosque_01 | africa/summer/a_mosque_01_4 | 4 | 1, 2, 3 | pass | - | - |
| africa | a_mosque_02 | africa/summer/a_mosque_02_1 | 1 | 2, 3, 4 | pass | - | - |
| africa | a_wb | africa/summer/a_wb/01 | 1 | 2, 3, 4 | pass | - | - |
| africa | a_wb | africa/summer/a_wb/02 | ? | ? (turns 1, 2, 3) | pass | - | - |
| africa | a_wt | africa/summer/a_wt/01 | 1 | 2, 3, 4 | pass | - | - |
| africa | a_wt | africa/summer/a_wt/02 | 4 | 1, 2, 3 | pass | - | - |
| common | conduitbarrel | common/summer/conduitbarrel | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| common | dot | common/summer/dot/01 | 1 | 2, 3, 4 | pass | pass | - |
| common | dot | common/summer/dot/02 | 3 | 4, 1, 2 | pass | pass | - |
| common | dot | common/summer/dot/03 | 4 | 1, 2, 3 | pass | pass | - |
| common | dot | common/summer/dot/04 | 4 | 1, 2, 3 | pass | pass | - |
| common | dot_h | common/summer/dot_h/01 | 1 | 2, 3, 4 | pass | pass | - |
| common | dot_h | common/summer/dot_h/02 | 2 | 3, 4, 1 | pass | pass | - |
| common | dot_h | common/summer/dot_h/03 | 3 | 4, 1, 2 | pass | pass | - |
| common | dot_h | common/summer/dot_h/04 | 4 | 1, 2, 3 | pass | pass | - |
| common | dzot | common/summer/dzot/01 | 1 | 2, 3, 4 | pass | pass | - |
| common | dzot | common/summer/dzot/02 | ? | ? (turns 1, 2, 3) | pass | - | - |
| common | dzot | common/summer/dzot/03 | ? | ? (turns 1, 2, 3) | pass | - | - |
| common | dzot | common/summer/dzot/04 | 3 | 4, 1, 2 | pass | pass | - |
| common | tent | common/summer/tent/01 | 1 | 2, 3, 4 | pass | - | r1 mirrored, r2, r3 mirrored |
| common | tent | common/summer/tent/02 | 2 | 3, 4, 1 | pass | - | - |
| common | watchtower | common/summer/watchtower | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| common | watertankbig | common/summer/watertankbig | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| common | watertanklittle | common/summer/watertanklittle | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| europe | e_chapel | europe/summer/e_chapel_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_cityhall | europe/summer/e_cityhall_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_ctownhouse01 | europe/summer/e_ctownhouse01_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_ctownhouse01 | europe/summer/e_ctownhouse01_2 | 2 | 3, 4, 1 | pass | pass | - |
| europe | e_ctownhouse01 | europe/summer/e_ctownhouse01_4 | 4 | 1, 2, 3 | pass | pass | - |
| europe | e_ctownhouse02 | europe/summer/e_ctownhouse02_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_ctownhouse02 | europe/summer/e_ctownhouse02_1_2 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_ctownhouse02 | europe/summer/e_ctownhouse02_4 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_ctownhouse03 | europe/summer/e_ctownhouse03_2 | 2 | 3, 4, 1 | pass | pass | - |
| europe | e_ctownhouse03 | europe/summer/e_ctownhouse03_4 | 4 | 1, 2, 3 | pass | pass | - |
| europe | e_ctownhouse04 | europe/summer/e_ctownhouse04_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_ctownhouse04 | europe/summer/e_ctownhouse04_1_2 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_frenchshed01 | europe/summer/e_frenchshed01 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_frenchshed02 | europe/summer/e_frenchshed02 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_frenchshed03 | europe/summer/e_frenchshed03 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_frenchshed04 | europe/summer/e_frenchshed04 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_frenchshed05 | europe/summer/e_frenchshed05 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_frenchshed06 | europe/summer/e_frenchshed06 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_house01 | europe/summer/e_house01_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_house01 | europe/summer/e_house01_2 | 3 | 4, 1, 2 | pass | - | - |
| europe | e_house01 | europe/summer/e_house01_3 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_house02 | europe/summer/e_house02_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_house02 | europe/summer/e_house02_2 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_house02 | europe/summer/e_house02_4 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_house03 | europe/summer/e_house03_2 | 2 | 3, 4, 1 | pass | - | - |
| europe | e_house03 | europe/summer/e_house03_3 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_house03 | europe/summer/e_house03_4 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_house04 | europe/summer/e_house04_4 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_house05 | europe/summer/e_house05_3 | 3 | 4, 1, 2 | pass | - | - |
| europe | e_house06 | europe/summer/e_house06_4 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_house07 | europe/summer/e_house07_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_house08 | europe/summer/e_house08_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_house09 | europe/summer/e_house09_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_house10 | europe/summer/e_house10_2 | 2 | 3, 4, 1 | pass | - | - |
| europe | e_house11 | europe/summer/e_house11_1 | 1 | 2, 3, 4 | pass | pass | - |
| europe | e_house11 | europe/summer/e_house11_3 | 3 | 4, 1, 2 | pass | pass | - |
| europe | e_house11 | europe/summer/e_house11_4 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_house12 | europe/summer/e_house12_3 | 3 | 4, 1, 2 | pass | - | - |
| europe | e_jettybuilding | europe/summer/e_jettybuilding_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_jettybuilding | europe/summer/e_jettybuilding_4 | 2 | 3, 4, 1 | pass | - | - |
| europe | e_monument | europe/summer/e_monument | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| europe | e_railwaystation | europe/summer/e_railwaystation_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_railwaystation | europe/summer/e_railwaystation_2 | ? | ? (turns 1, 2, 3) | pass | - | - |
| europe | e_shed01 | europe/summer/e_shed01_2 | 2 | 3, 4, 1 | pass | pass | - |
| europe | e_shed01 | europe/summer/e_shed01_3 | 3 | 4, 1, 2 | pass | pass | - |
| europe | e_shed02 | europe/summer/e_shed02_2 | 2 | 3, 4, 1 | pass | - | - |
| europe | e_shed02 | europe/summer/e_shed02_3 | ? | ? (turns 1, 2, 3) | pass | - | - |
| europe | e_shed02 | europe/summer/e_shed02_4 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_shed02 | europe/summer/e_shed02_5 | 2 | 3, 4, 1 | pass | - | - |
| europe | e_statue | europe/summer/e_statue/01 | 1 | 2, 3, 4 | pass | pass | r1 mirrored, r2, r3 mirrored |
| europe | e_statue | europe/summer/e_statue/02 | 2 | 3, 4, 1 | pass | pass | r1, r2, r3 |
| europe | e_statue | europe/summer/e_statue/04 | 2 | 3, 4, 1 | pass | - | r1 mirrored, r2, r3 mirrored |
| europe | e_stella | europe/summer/e_stella/01 | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| europe | e_stella | europe/summer/e_stella/02 | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| europe | e_stubtrack | europe/summer/e_stubtrack/01 | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| europe | e_stubtrack | europe/summer/e_stubtrack/02 | ? | ? (turns 1, 2, 3) | pass | - | r1, r2, r3 |
| europe | e_stubtrack | europe/summer/e_stubtrack/03 | ? | ? (turns 1, 2, 3) | pass | - | r1, r2, r3 |
| europe | e_stubtrack | europe/summer/e_stubtrack/04 | ? | ? (turns 1, 2, 3) | pass | - | r1, r2, r3 |
| europe | e_townhouse01 | europe/summer/e_townhouse01_3 | 3 | 4, 1, 2 | pass | pass | - |
| europe | e_townhouse01 | europe/summer/e_townhouse01_4 | 4 | 1, 2, 3 | pass | pass | - |
| europe | e_townhouse02 | europe/summer/e_townhouse02_2 | 2 | 3, 4, 1 | pass | - | - |
| europe | e_townhouse02 | europe/summer/e_townhouse02_3 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_townhouse03 | europe/summer/e_townhouse03_1 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_townhouse03 | europe/summer/e_townhouse03_4 | ? | ? (turns 1, 2, 3) | pass | - | - |
| europe | e_townhouse04 | europe/summer/e_townhouse04_3 | 3 | 4, 1, 2 | pass | - | - |
| europe | e_townhouse04 | europe/summer/e_townhouse04_4 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_townrailwaystation | europe/summer/e_townrailwaystation_4 | 4 | 1, 2, 3 | pass | - | - |
| europe | e_waterset | europe/summer/e_waterset/01 | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| europe | e_waterset | europe/summer/e_waterset/02 | ? | ? (turns 1, 2, 3) | pass | - | r1, r2, r3 |
| europe | e_waterset | europe/summer/e_waterset/03 | ? | ? (turns 1, 2, 3) | pass | - | r1, r2, r3 |
| europe | e_waterset | europe/summer/e_waterset/04 | ? | ? (turns 1, 2, 3) | pass | - | r1, r2, r3 |
| europe | e_wb | europe/summer/e_wb/01 | 1 | 2, 3, 4 | pass | pass | - |
| europe | e_wb | europe/summer/e_wb/02 | 4 | 1, 2, 3 | pass | pass | - |
| europe | e_wellbig | europe/summer/e_wellbig/01 | 1 | 2, 3, 4 | pass | pass | r1, r2, r3 |
| europe | e_wellbig | europe/summer/e_wellbig/03 | 3 | 4, 1, 2 | pass | pass | r1, r2, r3 |
| europe | e_welllittle | europe/summer/e_welllittle/01 | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| europe | e_welllittle | europe/summer/e_welllittle/02 | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| europe | e_wt | europe/summer/e_wt/01 | 1 | 2, 3, 4 | pass | - | - |
| europe | e_wt | europe/summer/e_wt/02 | 3 | 4, 1, 2 | pass | - | - |
| ussr | admhouse | ussr/summer/admhouse | 1 | 2, 3, 4 | pass | - | - |
| ussr | barracks | ussr/summer/barracks | 1 | 2, 3, 4 | pass | - | - |
| ussr | barracks | ussr/summer/barracks_2 | ? | ? (turns 1, 2, 3) | pass | - | - |
| ussr | chimneystalkbig | ussr/summer/chimneystalkbig | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| ussr | chimneystalklittle | ussr/summer/chimneystalklittle | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| ussr | church | ussr/summer/church | 1 | 2, 3, 4 | pass | - | - |
| ussr | cistern | ussr/summer/cistern/01 | 1 | 2, 3, 4 | pass | - | r1 mirrored, r2, r3 mirrored |
| ussr | cistern | ussr/summer/cistern/02 | ? | ? (turns 1, 2, 3) | pass | - | r1 mirrored, r2, r3 mirrored |
| ussr | dustbin | ussr/summer/dustbin/01 | 1 | 2, 3, 4 | pass | pass | - |
| ussr | dustbin | ussr/summer/dustbin/02 | 4 | 1, 2, 3 | pass | pass | r1 mirrored, r2, r3 mirrored |
| ussr | dustbin | ussr/summer/dustbin/03 | 3 | 4, 1, 2 | pass | - | r1 mirrored, r2, r3 mirrored |
| ussr | electricsubstation | ussr/summer/electricsubstation | 1 | 2, 3, 4 | pass | - | - |
| ussr | factoryboiler | ussr/summer/factoryboiler | 1 | 2, 3, 4 | pass | - | - |
| ussr | factoryfloor01 | ussr/summer/factoryfloor01 | 1 | 2, 3, 4 | pass | - | - |
| ussr | factoryfloor02 | ussr/summer/factoryfloor02 | 1 | 2, 3, 4 | pass | - | - |
| ussr | factoryfloor03 | ussr/summer/factoryfloor03 | 1 | 2, 3, 4 | pass | - | - |
| ussr | factoryoffice | ussr/summer/factoryoffice | 1 | 2, 3, 4 | pass | - | - |
| ussr | house01 | ussr/summer/house01 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house02 | ussr/summer/house02 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house03 | ussr/summer/house03 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house04 | ussr/summer/house04 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house05 | ussr/summer/house05 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house06 | ussr/summer/house06 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house07 | ussr/summer/house07 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house08 | ussr/summer/house08 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house09 | ussr/summer/house09 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house10 | ussr/summer/house10 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house11 | ussr/summer/house11 | 1 | 2, 3, 4 | pass | - | - |
| ussr | house12 | ussr/summer/house12 | 1 | 2, 3, 4 | pass | - | - |
| ussr | housebig | ussr/summer/housebig | 1 | 2, 3, 4 | pass | - | - |
| ussr | kiosk | ussr/summer/kiosk/01 | 1 | 2, 3, 4 | pass | - | - |
| ussr | kiosk | ussr/summer/kiosk/02 | ? | ? (turns 1, 2, 3) | pass | - | r1 mirrored, r2, r3 mirrored |
| ussr | kiosk | ussr/summer/kiosk/03 | 1 | 2, 3, 4 | pass | - | - |
| ussr | kiosk | ussr/summer/kiosk/04 | ? | ? (turns 1, 2, 3) | pass | - | r1 mirrored, r2, r3 mirrored |
| ussr | leaders | ussr/summer/leaders/01 | 1 | 2, 3, 4 | pass | pass | r1 mirrored, r2, r3 mirrored |
| ussr | leaders | ussr/summer/leaders/02 | 2 | 3, 4, 1 | pass | pass | r1 mirrored, r2, r3 mirrored |
| ussr | lenin | ussr/summer/lenin | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| ussr | pigeonloft | ussr/summer/pigeonloft/01 | 1 | 2, 3, 4 | pass | - | - |
| ussr | pigeonloft | ussr/summer/pigeonloft/02 | 4 | 1, 2, 3 | pass | - | r1, r2, r3 |
| ussr | railroadbighouse | ussr/summer/railroadbighouse | 1 | 2, 3, 4 | pass | - | - |
| ussr | railroadlittlehouse | ussr/summer/railroadlittlehouse | 1 | 2, 3, 4 | pass | - | - |
| ussr | railwaystation | ussr/summer/railwaystation | 1 | 2, 3, 4 | pass | - | - |
| ussr | shed01 | ussr/summer/shed01 | 1 | 2, 3, 4 | pass | - | - |
| ussr | shed02 | ussr/summer/shed02 | 1 | 2, 3, 4 | pass | - | - |
| ussr | storehouse | ussr/summer/storehouse_1 | 1 | 2, 3, 4 | pass | - | - |
| ussr | storehouse | ussr/summer/storehouse_2 | 2 | 3, 4, 1 | pass | - | - |
| ussr | stubtrack | ussr/summer/stubtrack/01 | 1 | 2, 3, 4 | pass | - | r1 mirrored, r2, r3 mirrored |
| ussr | stubtrack | ussr/summer/stubtrack/02 | ? | ? (turns 1, 2, 3) | pass | - | r1 mirrored, r2, r3 mirrored |
| ussr | stubtrack | ussr/summer/stubtrack/03 | 3 | 4, 1, 2 | pass | - | r1 mirrored, r2, r3 mirrored |
| ussr | stubtrack | ussr/summer/stubtrack/04 | ? | ? (turns 1, 2, 3) | pass | - | r1 mirrored, r2, r3 mirrored |
| ussr | towncornhouse01 | ussr/summer/towncornhouse01 | 1 | 2, 3, 4 | pass | - | - |
| ussr | towncornhouse02 | ussr/summer/towncornhouse02 | 1 | 2, 3, 4 | pass | - | - |
| ussr | towncornhouse03 | ussr/summer/towncornhouse03 | 1 | 2, 3, 4 | pass | - | - |
| ussr | towncornhouse04 | ussr/summer/towncornhouse04 | 1 | 2, 3, 4 | pass | - | - |
| ussr | townhouse01 | ussr/summer/townhouse01_1 | 1 | 2, 3, 4 | pass | - | - |
| ussr | townhouse01 | ussr/summer/townhouse01_2 | ? | ? (turns 1, 2, 3) | pass | - | - |
| ussr | townhouse02 | ussr/summer/townhouse02_1 | 1 | 2, 3, 4 | pass | - | - |
| ussr | townhouse02 | ussr/summer/townhouse02_2 | 2 | 3, 4, 1 | pass | - | - |
| ussr | townhouse03 | ussr/summer/townhouse03_1 | 1 | 2, 3, 4 | pass | - | - |
| ussr | townhouse03 | ussr/summer/townhouse03_2 | 2 | 3, 4, 1 | pass | - | - |
| ussr | townhouse04 | ussr/summer/townhouse04_1 | 1 | 2, 3, 4 | pass | - | - |
| ussr | townhouse04 | ussr/summer/townhouse04_2 | ? | ? (turns 1, 2, 3) | pass | - | - |
| ussr | townhouse05 | ussr/summer/townhouse05_1 | 1 | 2, 3, 4 | pass | - | - |
| ussr | townhouse05 | ussr/summer/townhouse05_2 | 2 | 3, 4, 1 | pass | - | - |
| ussr | townhouse05 | ussr/summer/townhouse05_3 | 2 | 3, 4, 1 | pass | - | - |
| ussr | ukrainehouse01 | ussr/summer/ukrainehouse01_1 | 1 | 2, 3, 4 | pass | - | - |
| ussr | ukrainehouse01 | ussr/summer/ukrainehouse01_2 | 2 | 3, 4, 1 | pass | - | - |
| ussr | ukrainehouse02 | ussr/summer/ukrainehouse02 | 1 | 2, 3, 4 | pass | - | - |
| ussr | ukrainehouse03 | ussr/summer/ukrainehouse03_1 | 1 | 2, 3, 4 | pass | pass | - |
| ussr | ukrainehouse03 | ussr/summer/ukrainehouse03_2 | 2 | 3, 4, 1 | pass | pass | - |
| ussr | warehouse | ussr/summer/warehouse | 1 | 2, 3, 4 | pass | - | - |
| ussr | watertower | ussr/summer/watertower | 1 | 2, 3, 4 | pass | - | r1, r2, r3 |
| ussr | wb | ussr/summer/wb/01 | 1 | 2, 3, 4 | pass | - | - |
| ussr | wb | ussr/summer/wb/02 | 4 | 1, 2, 3 | pass | - | - |
| ussr | wt | ussr/summer/wt/01 | 1 | 2, 3, 4 | pass | - | - |
| ussr | wt | ussr/summer/wt/02 | 2 | 3, 4, 1 | pass | - | - |
