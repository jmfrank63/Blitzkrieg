// The noise texture is addressed in world tile units. Two tiles that are
// neighbours on the map must get adjacent noise coordinates, or the pattern
// jumps between them and draws a line along the seam.
#include <cassert>
#include <cstdio>

static const int nPatchSize = 8;      // STerrainPatchInfo::nSizeX/nSizeY
static const float fTileSize = 32;

struct Patch { int nStartX, nStartY; };

// After the fix.
static void NoiseFixed(const Patch &p, int i, int j, float *u, float *v) {
    const float fStartX = p.nStartX * nPatchSize;
    const float fStartY = p.nStartY * nPatchSize;
    *u = (fStartX + j) * fTileSize;
    *v = (fStartY + i) * fTileSize;
}
// As the original shipped: V built from the column.
static void NoiseOriginal(const Patch &p, int i, int j, float *u, float *v) {
    const float fStartX = p.nStartX * nPatchSize;
    *u = (fStartX + j) * fTileSize;
    *v = (fStartX + i) * fTileSize;
}

int main() {
    // Two patches stacked vertically: same column, next row.
    const Patch top = {3, 2}, below = {3, 3};
    float u0, v0, u1, v1;

    // Bottom row of the upper patch and top row of the lower one are adjacent
    // map tiles, so their noise coordinates must be one tile apart.
    NoiseFixed(top, nPatchSize - 1, 0, &u0, &v0);
    NoiseFixed(below, 0, 0, &u1, &v1);
    assert(u1 == u0);
    assert(v1 - v0 == fTileSize);

    NoiseOriginal(top, nPatchSize - 1, 0, &u0, &v0);
    NoiseOriginal(below, 0, 0, &u1, &v1);
    assert(v1 - v0 != fTileSize);   // the jump that drew the horizontal line

    // Two patches side by side: same row, next column. V must not move at all.
    const Patch left = {3, 2}, right = {4, 2};
    NoiseFixed(left, 0, nPatchSize - 1, &u0, &v0);
    NoiseFixed(right, 0, 0, &u1, &v1);
    assert(v1 == v0);
    assert(u1 - u0 == fTileSize);

    NoiseOriginal(left, 0, nPatchSize - 1, &u0, &v0);
    NoiseOriginal(right, 0, 0, &u1, &v1);
    assert(v1 != v0);               // the jump that drew the vertical line

    std::puts("terrain noise coordinates are continuous across patch seams");
    return 0;
}
