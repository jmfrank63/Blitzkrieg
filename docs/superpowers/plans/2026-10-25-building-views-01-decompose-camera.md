# Building views 01: decompose and camera

Slice 1 of [the building views spec](../specs/2026-10-25-building-views-design.md). Later slices get their
own plan when this one is done.

## What the engine already says

- A building `.san` is `SSpritesPack` in an engine structure file. Top level: chunk 1 (the data), chunks 0 and
  2 empty (no objects). A chunk is `id: u8`, a length `n << 1` in one byte, or `(n << 1) | 1` in four bytes
  when `n << 1 >= 256`, then `n` bytes. Inside chunk 1: chunk 127 the signature `u32 0x314e4153`, chunk 1 the
  pack, whose chunk 1 is the sprite vector: one chunk 1 per sprite. A sprite: chunk 1 squares (chunk 1 the
  count `i32`, chunk 2 the raw 36-byte squares: `x, y, size, u1, v1, u2, v2, depth_left, depth_right` as
  `f32`), chunk 2 edge (chunk 1 `bool`, chunk 2 the `CVarArray2D<short, BYTE>`: chunks 1 size, 2 line count,
  3 raw `i16` data, 4 raw `u8` line sizes; chunk 3 bound box, 4 `f32`), chunk 3 center (2 `i32`).
- `CSpritesPackBuilder::Unpack` is the engine's own decompose: a picture of the squares' bounding box, each
  square copied from the atlas rectangle `int(uv * atlas size)`.
- Square places are multiples of their size from the sprite's center, so they do not depend on where the
  picture starts: a picture made by `Unpack`, packed again with the same center, the same passability and
  the same locked-tiles center, gives the same squares, depths, edge and atlas.
- The 2003 camera is in every `1.xml`: `PicturePosition` (relative to the zero cross) beside `WorldPosition`
  for fire slots, fire, smoke and explosion points. The MFC editors rendered at one world unit per pixel,
  yaw 45 degrees, pitch -120 degrees (`MfcEditorCamera`); an AI tile is `fWorldCellSize / 2` world units and
  32 x 16 px on screen.

## Tasks

1. **Round-trip tier first** (`zig build bk-views-roundtrip`, failing until 2-4 exist): copies tracked
   buildings (`e_ctownhouse01_1`, `_2`, `_4`, `e_house01_1`, `_2`, `_3`) to `zig-out/local-test/bk-views`,
   runs `bk-views decompose`, `bk-views-check` (the engine reader), `bk-sprite compose-pack` per damage
   state, decomposes the result and compares.
2. **`bk-views decompose <variant-dir> <out-dir>`** (`tools/zig/bk_views.zig`, std-only): every `*.san` of the
   variant to `<name>.json` (all pack fields) and `<name>_<i>.png` per sprite, all sprites of the variant on
   one canvas (the union of their squares, the zero cross at the same pixel), plus `building.json` (canvas,
   anchor, passability and origin from `1.xml`, the locked-tiles center under the MFC camera). Reads
   `<name>_h.dds` (ARGB8888). Writes PNG with stored deflate blocks (short, deterministic).
3. **`bk-views-check <variant-dir> <out-dir>`** (C++, engine libraries): loads each `.san` as
   `SSpritesPack::Load` does and the `_h.dds` with the image processor, unpacks every sprite with
   `CSpritesPackBuilder::Unpack`, and checks every field and every pixel against what decompose wrote. The
   JSON is written by one formatter on each side with exact decimal floats, so the check compares text.
4. **`bk-sprite compose-pack <sprite.png> <shadow.png> <out-name> --zero x,y [--noise <png>]
   [--pass <1.xml> --locked x,y]`** (C++): `NCompose::ComposeSingleObjectPack` (and `ComposeNoisePack` with
   `--noise`), the Resource Editor's building composer, with the passability read from `1.xml`.
5. **Compare**: the recomposed variant's `.san` and `_h.dds` must equal the originals byte for byte, and
   decompose of the result must equal the first decompose; any difference fails with the file and offset.
6. **`bk-views camera <variant-dir>...`**: least squares of the `1.xml` point pairs gives the 2 x 3 linear
   camera; reported as pixels per world unit, yaw and pitch next to `MfcEditorCamera`. With that camera a
   box over the locked footprint, its height fitted, is drawn and compared with the sprite's alpha (IoU).
   The 8 three-side families' result is `tools/zig/fixtures/bk-views/camera-2003.json`; the camera test
   fails when the fit leaves it or an IoU falls below the recorded floor.
7. **Wiring**: `bk-views`, `test-bk-views` (unit tests, the convention test row, the camera test),
   `bk-views-roundtrip`; message keys in all ten `tools/zig/lang/*.lang`.
8. **Spec**: the confirmed layout, the camera answer and the round-trip result.

## Verify

`zig build test-bk-views bk-views-roundtrip test-bk-sprite test-bk-image`
