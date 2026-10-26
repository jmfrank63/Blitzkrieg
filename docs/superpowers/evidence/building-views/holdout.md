# Hold-out of the building views generator

Written for M005 S03 (2026-10-26). Each variant below is drawn again by `bk-views holdout <out-dir> <variant-dir>...`
(`tools/zig/bk_views.zig`) from its family without it and without any variant that shows the same side, from the
first other side of its colourway (`From`), and every damage state is scored against the 2003 picture:

- **IoU**: outline overlap (alpha 128 or more) at the offset with the best overlap, searched from the footprints laid
  on each other;
- **SSIM**: mean structural similarity of the brightness at a quarter of the size (4x4 pixels averaged, 5x5 windows
  opaque in both): the layout of walls, roofs and windows. At full size every drawn side scores near 0;
- **Sharpness**: gradient energy of the drawn picture over the 2003 one (below 1 smeared, above 1 more edges);
- **Colour**: distance of the mean colours (0..255 a channel); **Hue**: L1 distance of the 12-band hue histograms.

The 23 variants are every variant of a family with two or more measured sides that has another side of its
colourway (hue histograms of the intact pictures within 0.1, the rule of `side`): a_h01, a_h02, a_h03, a_h07, a_h08,
e_ctownhouse01 (`_2`, `_4`), e_ctownhouse03, e_house11 (`_1`, `_3`), e_shed01, e_townhouse01 and ukrainehouse03. The
other 70 variants of the 39 multi-side families have no other side of their colourway by that rule (it is strict:
e_house11_4 against `_1` measures 0.287 though it is plainly the same red tiles and stone). `test-bk-views` draws the
23 again and fails below the floors in `tools/zig/fixtures/bk-views/gate.json`, which come from the "after" numbers.

"Before" is the S02 generator (voxel visual hull, Blender ray casting, global mirrors, patch fill; commit `82a2cd4b8`)
scored with the same metrics; "after" is the S03 generator (fitted blocks with roofs, Zig ray casting, faces
borrowed from matching faces, patch fill for the last gaps; commit `3d4485dc4`). Means over the 23 variants; a
winter state counts as its summer state (25 pictures a state).

| State | IoU | SSIM | Sharpness | Colour | Hue |
|---|---|---|---|---|---|
| 1 | 0.672 -> 0.860 | 0.154 -> 0.170 | 0.90 -> 1.30 | 33.8 -> 18.6 | 0.242 -> 0.123 |
| 2 | 0.650 -> 0.799 | 0.065 -> 0.049 | 0.82 -> 1.21 | 35.7 -> 14.2 | 0.197 -> 0.071 |
| 3 | 0.592 -> 0.743 | 0.033 -> 0.023 | 0.92 -> 1.03 | 20.2 -> 11.6 | 0.089 -> 0.050 |

The three-side families among them (state 1, 2, 3 per row; `before -> after`):

| Family | State | IoU | SSIM | Sharpness | Colour | Hue |
|---|---|---|---|---|---|---|
| a_h07 | 1 | 0.772 -> 0.902 | 0.268 -> 0.304 | 1.02 -> 1.19 | 11.2 -> 7.4 | 0.050 -> 0.143 |
| a_h07 | 2 | 0.734 -> 0.796 | 0.052 -> 0.013 | 0.95 -> 1.17 | 19.4 -> 18.7 | 0.056 -> 0.051 |
| a_h07 | 3 | 0.709 -> 0.745 | 0.004 -> -0.001 | 1.00 -> 1.01 | 8.7 -> 4.4 | 0.047 -> 0.030 |
| e_ctownhouse01 | 1 | 0.812 -> 0.896 | -0.003 -> 0.011 | 1.10 -> 1.12 | 34.8 -> 7.5 | 0.091 -> 0.063 |
| e_ctownhouse01 | 2 | 0.789 -> 0.828 | 0.081 -> -0.001 | 1.12 -> 1.13 | 16.9 -> 1.4 | 0.126 -> 0.072 |
| e_ctownhouse01 | 3 | 0.674 -> 0.675 | 0.052 -> 0.088 | 0.73 -> 1.17 | 26.2 -> 15.6 | 0.109 -> 0.079 |
| e_house11 | 1 | 0.763 -> 0.817 | 0.022 -> 0.059 | 0.98 -> 1.11 | 40.6 -> 20.0 | 0.197 -> 0.072 |
| e_house11 | 2 | 0.740 -> 0.777 | 0.027 -> 0.046 | 1.25 -> 1.07 | 51.6 -> 11.2 | 0.059 -> 0.064 |
| e_house11 | 3 | 0.686 -> 0.703 | 0.035 -> 0.022 | 0.95 -> 0.94 | 21.5 -> 22.5 | 0.053 -> 0.055 |

Reading: the outlines and the colours moved closest to the 2003 sides. The sharpness ratio moved from below 1 (the
hull smeared texture) to above 1: the drawn sides hold more edges than the 2003 ones, from seams where a face changes
its source and chimneys painted onto roof planes (the blocks hold no chimneys). SSIM stays near the level of a
different picture of the same building, as a side drawn from the opposite side must; damaged and ruined states lose
a little SSIM. The ruins (state 3) are drawn from blocks too: their IoU rose from 0.592 to 0.743.

## S06 generator (M005 S06, 2026-10-26)

The same 23 variants drawn by the S06 generator: blocks cost 0.03 of IoU each (a block the outlines barely need is
dropped and the rest refit), at least 12 world units each way, roofs no steeper than 1.75 and, but for ruins, on walls
a third of their height; chimneys, towers and spires cut from the variant's own outline; a face a picture sees at a
grazing angle (cosine under 0.25) taken from one it sees squarely. Means, S03 -> S06:

| State | IoU | Worst IoU | SSIM | Sharpness | Colour | Hue |
|---|---|---|---|---|---|---|
| 1 | 0.860 -> 0.868 | 0.693 -> 0.715 | 0.170 -> 0.190 | 1.30 -> 1.29 | 18.6 -> 16.4 | 0.123 -> 0.101 |
| 2 | 0.799 -> 0.813 | 0.640 -> 0.649 | 0.049 -> 0.050 | 1.21 -> 1.25 | 14.2 -> 13.1 | 0.071 -> 0.064 |
| 3 | 0.743 -> 0.761 | 0.658 -> 0.613 | 0.023 -> 0.050 | 1.03 -> 1.06 | 11.6 -> 12.6 | 0.050 -> 0.057 |

Every row stays inside the floors of `tools/zig/fixtures/bk-views/gate.json` (they were not changed). Tried and not
kept: one source picture per face (the face's best source first; state 1 SSIM 0.195 -> 0.173), a volume cost of
0.005 instead of 0.02 (worst state-3 IoU 0.586, under the floor), and the wall rule for ruins (e_shed01_2's ruin hue
0.27, over the floor; 0.217 without it).

| Family | Held out | From | State | IoU | SSIM | Sharpness | Colour | Hue |
|---|---|---|---|---|---|---|---|---|
| a_h01 | a_h01_1 | a_h01_2 | 1 | 0.928 | 0.367 | 1.28 | 13.5 | 0.043 |
| a_h01 | a_h01_1 | a_h01_2 | 2 | 0.887 | 0.017 | 1.11 | 2.7 | 0.074 |
| a_h01 | a_h01_1 | a_h01_2 | 3 | 0.747 | 0.064 | 1.08 | 8.9 | 0.013 |
| a_h01 | a_h01_2 | a_h01_1 | 1 | 0.920 | 0.358 | 1.19 | 9.8 | 0.190 |
| a_h01 | a_h01_2 | a_h01_1 | 2 | 0.880 | 0.040 | 1.23 | 12.9 | 0.027 |
| a_h01 | a_h01_2 | a_h01_1 | 3 | 0.800 | 0.058 | 0.98 | 15.3 | 0.010 |
| a_h02 | a_h02_2 | a_h02_4 | 1 | 0.875 | 0.267 | 1.14 | 12.9 | 0.044 |
| a_h02 | a_h02_2 | a_h02_4 | 2 | 0.802 | 0.086 | 1.16 | 3.7 | 0.017 |
| a_h02 | a_h02_2 | a_h02_4 | 3 | 0.760 | 0.066 | 0.84 | 18.5 | 0.016 |
| a_h02 | a_h02_4 | a_h02_2 | 1 | 0.860 | 0.202 | 1.61 | 6.2 | 0.058 |
| a_h02 | a_h02_4 | a_h02_2 | 2 | 0.777 | 0.029 | 1.11 | 13.5 | 0.008 |
| a_h02 | a_h02_4 | a_h02_2 | 3 | 0.758 | 0.040 | 1.34 | 14.2 | 0.016 |
| a_h03 | a_h03_2 | a_h03_3 | 1 | 0.817 | 0.150 | 1.48 | 39.5 | 0.097 |
| a_h03 | a_h03_2 | a_h03_3 | 2 | 0.796 | 0.044 | 0.94 | 39.9 | 0.099 |
| a_h03 | a_h03_2 | a_h03_3 | 3 | 0.733 | 0.094 | 0.97 | 6.5 | 0.005 |
| a_h03 | a_h03_3 | a_h03_2 | 1 | 0.845 | 0.135 | 1.03 | 26.8 | 0.161 |
| a_h03 | a_h03_3 | a_h03_2 | 2 | 0.820 | 0.012 | 1.44 | 35.1 | 0.112 |
| a_h03 | a_h03_3 | a_h03_2 | 3 | 0.674 | 0.043 | 1.36 | 35.6 | 0.080 |
| a_h07 | a_h07_1 | a_h07_2 | 1 | 0.903 | 0.298 | 1.06 | 4.2 | 0.082 |
| a_h07 | a_h07_1 | a_h07_2 | 2 | 0.813 | 0.213 | 0.96 | 6.8 | 0.079 |
| a_h07 | a_h07_1 | a_h07_2 | 3 | 0.814 | -0.026 | 0.98 | 5.4 | 0.015 |
| a_h07 | a_h07_2 | a_h07_1 | 1 | 0.913 | 0.434 | 0.91 | 4.3 | 0.162 |
| a_h07 | a_h07_2 | a_h07_1 | 2 | 0.774 | -0.022 | 1.61 | 8.1 | 0.055 |
| a_h07 | a_h07_2 | a_h07_1 | 3 | 0.738 | -0.022 | 1.03 | 9.2 | 0.039 |
| a_h07 | a_h07_3 | a_h07_1 | 1 | 0.940 | 0.310 | 1.24 | 7.8 | 0.048 |
| a_h07 | a_h07_3 | a_h07_1 | 2 | 0.838 | -0.015 | 0.89 | 6.8 | 0.046 |
| a_h07 | a_h07_3 | a_h07_1 | 3 | 0.776 | 0.108 | 1.10 | 8.0 | 0.040 |
| a_h08 | a_h08_1 | a_h08_4 | 1 | 0.792 | 0.320 | 1.15 | 9.1 | 0.040 |
| a_h08 | a_h08_1 | a_h08_4 | 2 | 0.651 | -0.006 | 1.28 | 1.0 | 0.002 |
| a_h08 | a_h08_1 | a_h08_4 | 3 | 0.807 | 0.032 | 0.97 | 11.1 | 0.020 |
| a_h08 | a_h08_4 | a_h08_1 | 1 | 0.795 | 0.331 | 1.46 | 11.7 | 0.038 |
| a_h08 | a_h08_4 | a_h08_1 | 2 | 0.713 | 0.025 | 1.15 | 17.3 | 0.005 |
| a_h08 | a_h08_4 | a_h08_1 | 3 | 0.761 | 0.074 | 1.08 | 3.0 | 0.012 |
| e_ctownhouse01 | e_ctownhouse01_2 | e_ctownhouse01_4 | 1 | 0.908 | 0.057 | 1.20 | 7.7 | 0.028 |
| e_ctownhouse01 | e_ctownhouse01_2 | e_ctownhouse01_4 | 2 | 0.843 | 0.030 | 1.11 | 6.0 | 0.043 |
| e_ctownhouse01 | e_ctownhouse01_2 | e_ctownhouse01_4 | 3 | 0.690 | 0.090 | 0.96 | 3.2 | 0.053 |
| e_ctownhouse01 | e_ctownhouse01_4 | e_ctownhouse01_2 | 1 | 0.861 | -0.001 | 1.12 | 4.7 | 0.060 |
| e_ctownhouse01 | e_ctownhouse01_4 | e_ctownhouse01_2 | 2 | 0.818 | 0.053 | 1.41 | 4.5 | 0.095 |
| e_ctownhouse01 | e_ctownhouse01_4 | e_ctownhouse01_2 | 3 | 0.613 | 0.061 | 1.06 | 8.7 | 0.066 |
| e_ctownhouse03 | e_ctownhouse03_2 | e_ctownhouse03_4 | 1 | 0.715 | 0.098 | 1.64 | 24.3 | 0.106 |
| e_ctownhouse03 | e_ctownhouse03_2 | e_ctownhouse03_4 | 2 | 0.649 | -0.008 | 1.16 | 7.7 | 0.036 |
| e_ctownhouse03 | e_ctownhouse03_2 | e_ctownhouse03_4 | 3 | 0.758 | 0.085 | 0.85 | 35.0 | 0.123 |
| e_ctownhouse03 | e_ctownhouse03_4 | e_ctownhouse03_2 | 1 | 0.744 | 0.093 | 0.87 | 16.2 | 0.179 |
| e_ctownhouse03 | e_ctownhouse03_4 | e_ctownhouse03_2 | 2 | 0.722 | 0.019 | 1.05 | 9.3 | 0.188 |
| e_ctownhouse03 | e_ctownhouse03_4 | e_ctownhouse03_2 | 3 | 0.676 | 0.028 | 1.23 | 34.9 | 0.106 |
| e_house11 | e_house11_1 | e_house11_3 | 1 | 0.882 | 0.069 | 0.83 | 7.7 | 0.045 |
| e_house11 | e_house11_1 | e_house11_3 | 2 | 0.835 | 0.033 | 0.80 | 11.1 | 0.040 |
| e_house11 | e_house11_1 | e_house11_3 | 3 | 0.744 | 0.087 | 1.00 | 24.1 | 0.050 |
| e_house11 | e_house11_3 | e_house11_1 | 1 | 0.888 | 0.106 | 1.53 | 14.5 | 0.040 |
| e_house11 | e_house11_3 | e_house11_1 | 2 | 0.852 | 0.070 | 1.33 | 9.6 | 0.050 |
| e_house11 | e_house11_3 | e_house11_1 | 3 | 0.724 | 0.017 | 1.02 | 25.5 | 0.062 |
| e_shed01 | e_shed01_2 | e_shed01_3 | 1 | 0.923 | 0.115 | 0.99 | 35.3 | 0.060 |
| e_shed01 | e_shed01_2 | e_shed01_3 | 2 | 0.878 | 0.101 | 1.36 | 17.0 | 0.031 |
| e_shed01 | e_shed01_2 | e_shed01_3 | 3 | 0.850 | 0.017 | 1.06 | 11.8 | 0.217 |
| e_shed01 | e_shed01_3 | e_shed01_2 | 1 | 0.889 | 0.102 | 0.72 | 48.4 | 0.049 |
| e_shed01 | e_shed01_3 | e_shed01_2 | 2 | 0.860 | 0.063 | 0.61 | 23.3 | 0.038 |
| e_shed01 | e_shed01_3 | e_shed01_2 | 3 | 0.826 | 0.031 | 0.95 | 4.7 | 0.082 |
| e_townhouse01 | e_townhouse01_3 | e_townhouse01_4 | 1 | 0.895 | 0.111 | 1.01 | 7.9 | 0.088 |
| e_townhouse01 | e_townhouse01_3 | e_townhouse01_4 | 2 | 0.828 | 0.069 | 1.51 | 12.0 | 0.031 |
| e_townhouse01 | e_townhouse01_3 | e_townhouse01_4 | 3 | 0.800 | 0.035 | 1.14 | 5.2 | 0.009 |
| e_townhouse01 | e_townhouse01_4 | e_townhouse01_3 | 1 | 0.945 | 0.168 | 1.85 | 8.4 | 0.226 |
| e_townhouse01 | e_townhouse01_4 | e_townhouse01_3 | 2 | 0.919 | 0.124 | 1.79 | 5.0 | 0.060 |
| e_townhouse01 | e_townhouse01_4 | e_townhouse01_3 | 3 | 0.845 | 0.031 | 1.06 | 3.1 | 0.040 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 1 | 0.875 | 0.157 | 2.34 | 34.3 | 0.304 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 1w | 0.871 | 0.184 | 2.06 | 23.4 | 0.232 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 2 | 0.860 | -0.003 | 1.47 | 20.2 | 0.120 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 2w | 0.860 | -0.003 | 1.47 | 20.2 | 0.120 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 3 | 0.743 | 0.058 | 1.02 | 10.2 | 0.059 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 3w | 0.743 | 0.058 | 1.02 | 10.2 | 0.059 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 1 | 0.863 | 0.162 | 1.32 | 18.4 | 0.103 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 1w | 0.853 | 0.150 | 1.16 | 14.1 | 0.039 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 2 | 0.823 | 0.143 | 1.62 | 17.5 | 0.114 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 2w | 0.823 | 0.143 | 1.62 | 17.5 | 0.114 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 3 | 0.822 | 0.061 | 1.17 | 1.5 | 0.113 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 3w | 0.822 | 0.061 | 1.17 | 1.5 | 0.113 |

## After (S03 generator)

| Family | Held out | From | State | IoU | SSIM | Sharpness | Colour | Hue |
|---|---|---|---|---|---|---|---|---|
| a_h01 | a_h01_1 | a_h01_2 | 1 | 0.923 | 0.339 | 1.39 | 30.7 | 0.039 |
| a_h01 | a_h01_1 | a_h01_2 | 2 | 0.864 | 0.065 | 1.08 | 15.1 | 0.056 |
| a_h01 | a_h01_1 | a_h01_2 | 3 | 0.803 | 0.003 | 1.04 | 5.4 | 0.005 |
| a_h01 | a_h01_2 | a_h01_1 | 1 | 0.875 | 0.304 | 1.19 | 18.7 | 0.016 |
| a_h01 | a_h01_2 | a_h01_1 | 2 | 0.811 | 0.042 | 1.20 | 3.0 | 0.027 |
| a_h01 | a_h01_2 | a_h01_1 | 3 | 0.798 | 0.051 | 0.98 | 14.8 | 0.002 |
| a_h02 | a_h02_2 | a_h02_4 | 1 | 0.925 | 0.141 | 1.23 | 8.4 | 0.048 |
| a_h02 | a_h02_2 | a_h02_4 | 2 | 0.892 | 0.001 | 1.14 | 8.4 | 0.034 |
| a_h02 | a_h02_2 | a_h02_4 | 3 | 0.778 | 0.024 | 0.82 | 18.0 | 0.013 |
| a_h02 | a_h02_4 | a_h02_2 | 1 | 0.868 | 0.277 | 1.49 | 5.3 | 0.077 |
| a_h02 | a_h02_4 | a_h02_2 | 2 | 0.816 | 0.098 | 1.16 | 6.5 | 0.022 |
| a_h02 | a_h02_4 | a_h02_2 | 3 | 0.779 | 0.042 | 1.29 | 7.8 | 0.031 |
| a_h03 | a_h03_2 | a_h03_3 | 1 | 0.806 | 0.100 | 1.72 | 47.1 | 0.081 |
| a_h03 | a_h03_2 | a_h03_3 | 2 | 0.776 | 0.104 | 0.89 | 37.3 | 0.099 |
| a_h03 | a_h03_2 | a_h03_3 | 3 | 0.696 | 0.044 | 0.96 | 5.8 | 0.006 |
| a_h03 | a_h03_3 | a_h03_2 | 1 | 0.838 | 0.124 | 1.00 | 36.8 | 0.043 |
| a_h03 | a_h03_3 | a_h03_2 | 2 | 0.773 | 0.059 | 1.34 | 49.4 | 0.104 |
| a_h03 | a_h03_3 | a_h03_2 | 3 | 0.668 | 0.012 | 1.04 | 4.0 | 0.008 |
| a_h07 | a_h07_1 | a_h07_2 | 1 | 0.887 | 0.223 | 1.26 | 9.4 | 0.197 |
| a_h07 | a_h07_1 | a_h07_2 | 2 | 0.815 | -0.010 | 1.09 | 22.7 | 0.046 |
| a_h07 | a_h07_1 | a_h07_2 | 3 | 0.770 | -0.014 | 0.98 | 0.7 | 0.004 |
| a_h07 | a_h07_2 | a_h07_1 | 1 | 0.917 | 0.374 | 1.00 | 2.3 | 0.131 |
| a_h07 | a_h07_2 | a_h07_1 | 2 | 0.748 | 0.065 | 1.50 | 29.8 | 0.063 |
| a_h07 | a_h07_2 | a_h07_1 | 3 | 0.734 | -0.035 | 1.00 | 5.4 | 0.036 |
| a_h07 | a_h07_3 | a_h07_1 | 1 | 0.901 | 0.314 | 1.30 | 10.5 | 0.102 |
| a_h07 | a_h07_3 | a_h07_1 | 2 | 0.824 | -0.016 | 0.93 | 3.6 | 0.044 |
| a_h07 | a_h07_3 | a_h07_1 | 3 | 0.730 | 0.047 | 1.06 | 7.0 | 0.050 |
| a_h08 | a_h08_1 | a_h08_4 | 1 | 0.768 | 0.300 | 1.19 | 12.5 | 0.059 |
| a_h08 | a_h08_1 | a_h08_4 | 2 | 0.640 | 0.013 | 1.31 | 1.1 | 0.009 |
| a_h08 | a_h08_1 | a_h08_4 | 3 | 0.698 | -0.034 | 0.91 | 5.7 | 0.004 |
| a_h08 | a_h08_4 | a_h08_1 | 1 | 0.800 | 0.201 | 1.55 | 13.1 | 0.029 |
| a_h08 | a_h08_4 | a_h08_1 | 2 | 0.725 | -0.037 | 1.21 | 11.9 | 0.004 |
| a_h08 | a_h08_4 | a_h08_1 | 3 | 0.747 | 0.153 | 1.08 | 6.0 | 0.010 |
| e_ctownhouse01 | e_ctownhouse01_2 | e_ctownhouse01_4 | 1 | 0.903 | 0.014 | 1.18 | 5.3 | 0.025 |
| e_ctownhouse01 | e_ctownhouse01_2 | e_ctownhouse01_4 | 2 | 0.838 | 0.010 | 1.02 | 1.3 | 0.030 |
| e_ctownhouse01 | e_ctownhouse01_2 | e_ctownhouse01_4 | 3 | 0.666 | 0.109 | 1.25 | 25.0 | 0.060 |
| e_ctownhouse01 | e_ctownhouse01_4 | e_ctownhouse01_2 | 1 | 0.889 | 0.009 | 1.06 | 9.7 | 0.102 |
| e_ctownhouse01 | e_ctownhouse01_4 | e_ctownhouse01_2 | 2 | 0.818 | -0.013 | 1.24 | 1.5 | 0.114 |
| e_ctownhouse01 | e_ctownhouse01_4 | e_ctownhouse01_2 | 3 | 0.683 | 0.067 | 1.08 | 6.2 | 0.098 |
| e_ctownhouse03 | e_ctownhouse03_2 | e_ctownhouse03_4 | 1 | 0.693 | 0.079 | 1.52 | 18.5 | 0.082 |
| e_ctownhouse03 | e_ctownhouse03_2 | e_ctownhouse03_4 | 2 | 0.680 | 0.008 | 1.22 | 18.5 | 0.015 |
| e_ctownhouse03 | e_ctownhouse03_2 | e_ctownhouse03_4 | 3 | 0.724 | -0.022 | 0.82 | 38.1 | 0.134 |
| e_ctownhouse03 | e_ctownhouse03_4 | e_ctownhouse03_2 | 1 | 0.821 | 0.074 | 0.86 | 12.3 | 0.068 |
| e_ctownhouse03 | e_ctownhouse03_4 | e_ctownhouse03_2 | 2 | 0.700 | 0.101 | 1.18 | 8.7 | 0.181 |
| e_ctownhouse03 | e_ctownhouse03_4 | e_ctownhouse03_2 | 3 | 0.712 | 0.102 | 1.18 | 32.7 | 0.120 |
| e_house11 | e_house11_1 | e_house11_3 | 1 | 0.862 | 0.075 | 0.60 | 20.0 | 0.051 |
| e_house11 | e_house11_1 | e_house11_3 | 2 | 0.832 | 0.017 | 0.74 | 12.4 | 0.060 |
| e_house11 | e_house11_1 | e_house11_3 | 3 | 0.748 | 0.092 | 0.88 | 14.6 | 0.063 |
| e_house11 | e_house11_3 | e_house11_1 | 1 | 0.772 | 0.044 | 1.63 | 20.0 | 0.093 |
| e_house11 | e_house11_3 | e_house11_1 | 2 | 0.722 | 0.075 | 1.39 | 10.0 | 0.069 |
| e_house11 | e_house11_3 | e_house11_1 | 3 | 0.658 | -0.047 | 1.01 | 30.3 | 0.047 |
| e_shed01 | e_shed01_2 | e_shed01_3 | 1 | 0.949 | 0.098 | 1.03 | 36.5 | 0.052 |
| e_shed01 | e_shed01_2 | e_shed01_3 | 2 | 0.855 | 0.071 | 1.32 | 14.0 | 0.036 |
| e_shed01 | e_shed01_2 | e_shed01_3 | 3 | 0.834 | 0.005 | 0.99 | 14.5 | 0.153 |
| e_shed01 | e_shed01_3 | e_shed01_2 | 1 | 0.892 | 0.123 | 0.65 | 48.4 | 0.067 |
| e_shed01 | e_shed01_3 | e_shed01_2 | 2 | 0.866 | 0.076 | 0.60 | 24.2 | 0.032 |
| e_shed01 | e_shed01_3 | e_shed01_2 | 3 | 0.841 | 0.006 | 0.96 | 4.4 | 0.075 |
| e_townhouse01 | e_townhouse01_3 | e_townhouse01_4 | 1 | 0.912 | 0.102 | 0.97 | 7.6 | 0.064 |
| e_townhouse01 | e_townhouse01_3 | e_townhouse01_4 | 2 | 0.874 | 0.074 | 1.26 | 6.3 | 0.030 |
| e_townhouse01 | e_townhouse01_3 | e_townhouse01_4 | 3 | 0.741 | 0.059 | 1.08 | 2.4 | 0.046 |
| e_townhouse01 | e_townhouse01_4 | e_townhouse01_3 | 1 | 0.915 | 0.129 | 1.82 | 9.0 | 0.656 |
| e_townhouse01 | e_townhouse01_4 | e_townhouse01_3 | 2 | 0.887 | 0.226 | 1.51 | 8.2 | 0.106 |
| e_townhouse01 | e_townhouse01_4 | e_townhouse01_3 | 3 | 0.835 | 0.001 | 1.00 | 1.2 | 0.039 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 1 | 0.841 | 0.149 | 2.54 | 38.6 | 0.370 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 1w | 0.838 | 0.162 | 2.15 | 24.0 | 0.299 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 2 | 0.815 | 0.014 | 1.55 | 23.9 | 0.120 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 2w | 0.815 | 0.014 | 1.55 | 23.9 | 0.120 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 3 | 0.746 | -0.084 | 0.98 | 9.4 | 0.052 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 3w | 0.746 | -0.084 | 0.98 | 9.4 | 0.052 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 1 | 0.867 | 0.264 | 1.19 | 7.8 | 0.251 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 1w | 0.849 | 0.238 | 1.05 | 12.0 | 0.071 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 2 | 0.799 | 0.089 | 1.46 | 6.2 | 0.180 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 2w | 0.799 | 0.089 | 1.46 | 6.2 | 0.180 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 3 | 0.718 | 0.033 | 1.19 | 10.7 | 0.068 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 3w | 0.718 | 0.033 | 1.19 | 10.7 | 0.068 |

## Before (S02 generator, same metrics)

| Family | Held out | From | State | IoU | SSIM | Sharpness | Colour | Hue |
|---|---|---|---|---|---|---|---|---|
| a_h01 | a_h01_1 | a_h01_2 | 1 | 0.778 | 0.269 | 0.47 | 89.6 | 0.183 |
| a_h01 | a_h01_1 | a_h01_2 | 2 | 0.735 | 0.067 | 0.43 | 91.8 | 0.185 |
| a_h01 | a_h01_1 | a_h01_2 | 3 | 0.654 | 0.053 | 0.85 | 13.0 | 0.034 |
| a_h01 | a_h01_2 | a_h01_1 | 1 | 0.592 | 0.176 | 1.12 | 13.6 | 0.118 |
| a_h01 | a_h01_2 | a_h01_1 | 2 | 0.561 | 0.045 | 1.34 | 41.6 | 0.094 |
| a_h01 | a_h01_2 | a_h01_1 | 3 | 0.482 | 0.019 | 0.84 | 18.8 | 0.028 |
| a_h02 | a_h02_2 | a_h02_4 | 1 | 0.556 | 0.246 | 0.31 | 78.6 | 0.386 |
| a_h02 | a_h02_2 | a_h02_4 | 2 | 0.539 | 0.085 | 0.15 | 148.8 | 0.347 |
| a_h02 | a_h02_2 | a_h02_4 | 3 | 0.530 | 0.006 | 0.83 | 14.6 | 0.058 |
| a_h02 | a_h02_4 | a_h02_2 | 1 | 0.571 | 0.268 | 0.30 | 82.2 | 0.154 |
| a_h02 | a_h02_4 | a_h02_2 | 2 | 0.557 | 0.075 | 1.14 | 91.0 | 0.050 |
| a_h02 | a_h02_4 | a_h02_2 | 3 | 0.494 | 0.057 | 1.68 | 39.7 | 0.162 |
| a_h03 | a_h03_2 | a_h03_3 | 1 | 0.536 | 0.103 | 1.01 | 105.8 | 0.523 |
| a_h03 | a_h03_2 | a_h03_3 | 2 | 0.641 | 0.065 | 0.60 | 35.6 | 0.115 |
| a_h03 | a_h03_2 | a_h03_3 | 3 | 0.528 | 0.029 | 0.76 | 6.0 | 0.019 |
| a_h03 | a_h03_3 | a_h03_2 | 1 | 0.601 | 0.008 | 1.09 | 37.1 | 0.097 |
| a_h03 | a_h03_3 | a_h03_2 | 2 | 0.480 | 0.066 | 0.49 | 64.3 | 0.733 |
| a_h03 | a_h03_3 | a_h03_2 | 3 | 0.402 | 0.061 | 0.62 | 51.2 | 0.058 |
| a_h07 | a_h07_1 | a_h07_2 | 1 | 0.733 | 0.174 | 1.04 | 8.3 | 0.066 |
| a_h07 | a_h07_1 | a_h07_2 | 2 | 0.719 | 0.051 | 0.74 | 9.8 | 0.066 |
| a_h07 | a_h07_1 | a_h07_2 | 3 | 0.680 | 0.005 | 0.93 | 11.3 | 0.055 |
| a_h07 | a_h07_2 | a_h07_1 | 1 | 0.886 | 0.318 | 0.97 | 2.9 | 0.049 |
| a_h07 | a_h07_2 | a_h07_1 | 2 | 0.744 | 0.106 | 1.39 | 25.9 | 0.062 |
| a_h07 | a_h07_2 | a_h07_1 | 3 | 0.766 | 0.044 | 1.18 | 7.3 | 0.031 |
| a_h07 | a_h07_3 | a_h07_1 | 1 | 0.698 | 0.311 | 1.04 | 22.4 | 0.036 |
| a_h07 | a_h07_3 | a_h07_1 | 2 | 0.739 | -0.002 | 0.72 | 22.5 | 0.039 |
| a_h07 | a_h07_3 | a_h07_1 | 3 | 0.682 | -0.038 | 0.89 | 7.5 | 0.053 |
| a_h08 | a_h08_1 | a_h08_4 | 1 | 0.707 | 0.275 | 0.84 | 21.5 | 0.094 |
| a_h08 | a_h08_1 | a_h08_4 | 2 | 0.596 | -0.070 | 0.87 | 10.8 | 0.069 |
| a_h08 | a_h08_1 | a_h08_4 | 3 | 0.603 | 0.041 | 0.92 | 36.7 | 0.020 |
| a_h08 | a_h08_4 | a_h08_1 | 1 | 0.670 | 0.245 | 1.15 | 22.1 | 0.156 |
| a_h08 | a_h08_4 | a_h08_1 | 2 | 0.625 | -0.004 | 0.93 | 19.9 | 0.066 |
| a_h08 | a_h08_4 | a_h08_1 | 3 | 0.626 | 0.033 | 1.44 | 18.5 | 0.089 |
| e_ctownhouse01 | e_ctownhouse01_2 | e_ctownhouse01_4 | 1 | 0.793 | -0.015 | 1.57 | 7.5 | 0.075 |
| e_ctownhouse01 | e_ctownhouse01_2 | e_ctownhouse01_4 | 2 | 0.752 | 0.093 | 0.89 | 29.8 | 0.085 |
| e_ctownhouse01 | e_ctownhouse01_2 | e_ctownhouse01_4 | 3 | 0.664 | 0.070 | 0.84 | 18.1 | 0.108 |
| e_ctownhouse01 | e_ctownhouse01_4 | e_ctownhouse01_2 | 1 | 0.831 | 0.010 | 0.62 | 62.0 | 0.106 |
| e_ctownhouse01 | e_ctownhouse01_4 | e_ctownhouse01_2 | 2 | 0.827 | 0.069 | 1.36 | 4.0 | 0.166 |
| e_ctownhouse01 | e_ctownhouse01_4 | e_ctownhouse01_2 | 3 | 0.683 | 0.034 | 0.62 | 34.4 | 0.109 |
| e_ctownhouse03 | e_ctownhouse03_2 | e_ctownhouse03_4 | 1 | 0.648 | -0.012 | 1.45 | 24.9 | 0.222 |
| e_ctownhouse03 | e_ctownhouse03_2 | e_ctownhouse03_4 | 2 | 0.610 | 0.094 | 0.71 | 38.8 | 0.265 |
| e_ctownhouse03 | e_ctownhouse03_2 | e_ctownhouse03_4 | 3 | 0.544 | 0.038 | 1.00 | 22.4 | 0.044 |
| e_ctownhouse03 | e_ctownhouse03_4 | e_ctownhouse03_2 | 1 | 0.568 | 0.052 | 0.89 | 8.2 | 0.307 |
| e_ctownhouse03 | e_ctownhouse03_4 | e_ctownhouse03_2 | 2 | 0.555 | 0.111 | 1.52 | 14.1 | 0.348 |
| e_ctownhouse03 | e_ctownhouse03_4 | e_ctownhouse03_2 | 3 | 0.450 | 0.055 | 0.89 | 7.1 | 0.209 |
| e_house11 | e_house11_1 | e_house11_3 | 1 | 0.863 | 0.031 | 0.58 | 11.3 | 0.153 |
| e_house11 | e_house11_1 | e_house11_3 | 2 | 0.840 | 0.033 | 1.11 | 36.9 | 0.085 |
| e_house11 | e_house11_1 | e_house11_3 | 3 | 0.774 | 0.048 | 0.91 | 38.4 | 0.051 |
| e_house11 | e_house11_3 | e_house11_1 | 1 | 0.662 | 0.013 | 1.38 | 69.9 | 0.242 |
| e_house11 | e_house11_3 | e_house11_1 | 2 | 0.640 | 0.020 | 1.39 | 66.4 | 0.033 |
| e_house11 | e_house11_3 | e_house11_1 | 3 | 0.598 | 0.023 | 0.98 | 4.6 | 0.054 |
| e_shed01 | e_shed01_2 | e_shed01_3 | 1 | 0.586 | 0.211 | 0.32 | 21.2 | 0.161 |
| e_shed01 | e_shed01_2 | e_shed01_3 | 2 | 0.584 | 0.136 | 0.45 | 18.6 | 0.151 |
| e_shed01 | e_shed01_2 | e_shed01_3 | 3 | 0.566 | 0.028 | 0.75 | 30.8 | 0.019 |
| e_shed01 | e_shed01_3 | e_shed01_2 | 1 | 0.608 | 0.112 | 0.24 | 49.9 | 0.164 |
| e_shed01 | e_shed01_3 | e_shed01_2 | 2 | 0.567 | 0.091 | 0.50 | 8.2 | 0.153 |
| e_shed01 | e_shed01_3 | e_shed01_2 | 3 | 0.582 | 0.120 | 0.75 | 47.5 | 0.188 |
| e_townhouse01 | e_townhouse01_3 | e_townhouse01_4 | 1 | 0.652 | 0.148 | 0.75 | 6.0 | 0.436 |
| e_townhouse01 | e_townhouse01_3 | e_townhouse01_4 | 2 | 0.621 | 0.090 | 0.67 | 32.1 | 0.286 |
| e_townhouse01 | e_townhouse01_3 | e_townhouse01_4 | 3 | 0.516 | 0.083 | 0.51 | 15.5 | 0.219 |
| e_townhouse01 | e_townhouse01_4 | e_townhouse01_3 | 1 | 0.645 | 0.172 | 0.20 | 35.2 | 1.219 |
| e_townhouse01 | e_townhouse01_4 | e_townhouse01_3 | 2 | 0.646 | 0.055 | 0.27 | 19.2 | 0.105 |
| e_townhouse01 | e_townhouse01_4 | e_townhouse01_3 | 3 | 0.588 | 0.101 | 0.66 | 37.6 | 0.279 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 1 | 0.629 | 0.270 | 1.94 | 9.5 | 0.471 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 1w | 0.638 | 0.151 | 1.92 | 35.1 | 0.134 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 2 | 0.627 | 0.066 | 0.56 | 26.9 | 0.578 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 2w | 0.627 | 0.066 | 0.56 | 26.9 | 0.578 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 3 | 0.557 | 0.010 | 0.97 | 4.3 | 0.086 |
| ukrainehouse03 | ukrainehouse03_1 | ukrainehouse03_2 | 3w | 0.557 | 0.010 | 0.97 | 4.3 | 0.086 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 1 | 0.680 | 0.201 | 0.72 | 4.9 | 0.339 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 1w | 0.676 | 0.116 | 0.63 | 15.2 | 0.146 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 2 | 0.710 | 0.107 | 0.86 | 4.9 | 0.132 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 2w | 0.710 | 0.107 | 0.86 | 4.9 | 0.132 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 3 | 0.634 | -0.054 | 1.07 | 7.5 | 0.086 |
| ukrainehouse03 | ukrainehouse03_2 | ukrainehouse03_1 | 3w | 0.634 | -0.054 | 1.07 | 7.5 | 0.086 |
