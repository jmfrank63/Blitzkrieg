# Style of the generated sides of e_ctownhouse01

Written for M005 S03 (2026-10-26) by `bk-views style <out-dir> Data/Buildings/europe/summer/e_ctownhouse01_2
<pictures-dir>...` on the pictures `zig build bk-views-side` draws (`zig-out/local-test/bk-views/side/
e_ctownhouse01_2.work/e_ctownhouse01_2_r<k>/`): each damage state against the 2003 pictures of that state of the
family in its colourway (`e_ctownhouse01_2`, `_4`). `_r1` is side 3, which no 2003 picture shows; `_r2` is side 4
(exists as `_4`), `_r3` side 1 (exists as `_1` in another colourway). Sharpness is the gradient energy ratio, colour
the distance of the mean colours, hue the L1 distance of the hue histograms. `test-bk-views` checks the style of the
hold-out pictures against `tools/zig/fixtures/bk-views/gate.json`; these are within those floors.

| Generated | State | Sharpness | Colour | Hue | Before (S02): sharpness, colour, hue |
|---|---|---|---|---|---|
| e_ctownhouse01_2_r1 | 1 | 0.90 | 5.1 | 0.040 | 0.92, 14.6, 0.047 |
| e_ctownhouse01_2_r1 | 2 | 1.00 | 6.9 | 0.021 | 0.96, 10.8, 0.011 |
| e_ctownhouse01_2_r1 | 3 | 1.14 | 7.2 | 0.026 | 1.18, 1.6, 0.035 |
| e_ctownhouse01_2_r2 | 1 | 1.08 | 2.7 | 0.009 | 1.07, 2.2, 0.006 |
| e_ctownhouse01_2_r2 | 2 | 1.03 | 2.2 | 0.022 | 1.01, 1.4, 0.018 |
| e_ctownhouse01_2_r2 | 3 | 1.07 | 9.3 | 0.034 | 1.03, 8.8, 0.035 |
| e_ctownhouse01_2_r3 | 1 | 0.96 | 13.5 | 0.058 | 0.97, 20.6, 0.109 |
| e_ctownhouse01_2_r3 | 2 | 1.13 | 6.8 | 0.011 | 0.91, 12.5, 0.022 |
| e_ctownhouse01_2_r3 | 3 | 0.97 | 11.6 | 0.030 | 0.90, 12.8, 0.058 |

Whole-picture statistics do not see the S02 sides' lumpy roofs (their numbers are alike); the hold-out's outline
IoU and colour distance do (`holdout.md`).
