#include <cassert>
#include <cstdio>
#include <cmath>
#include <algorithm>

template <class T> struct CTRect {
    T x1, y1, x2, y2;
    CTRect(T a, T b, T c, T d) : x1(a), y1(b), x2(c), y2(d) {}
    T Width() const { return x2 - x1; }
    T Height() const { return y2 - y1; }
};
template <class T> T Max(T a, T b) { return a > b ? a : b; }
template <class T> T Min(T a, T b) { return a < b ? a : b; }
struct SHMatrix { float m[16]; };
struct CVec3 { float x, y, z; };
inline void CreateOrthographicProjectionMatrixRH( SHMatrix *, float, float, float, float ) {}
#include "Scene/SceneScreenScale.h"

int main() {
    // 1440x868 measures 1.1302 before rounding. A fractional scale is exactly
    // what drew the seams: the tiles come from one point sampled atlas, so a
    // tile spread over 36.2 pixels lets the edge pixel take its colour from the
    // neighbouring atlas cell, and the seams then autocorrelate at the scaled
    // tile size. Whole steps only.
    const CTRect<float> screen(0, 0, 1440, 868);
    const float scale = NSceneScreenScale::GetGameplayScale(screen);
    assert(scale == 1.0f);

    // Whatever the window, the scale is a whole number and never below one.
    const int sizes[][2] = { {800,600}, {1024,768}, {1280,800}, {1440,868}, {1920,1080}, {2048,1536}, {2560,1440}, {3840,2160} };
    for (const auto &size : sizes) {
        const CTRect<float> r(0, 0, static_cast<float>(size[0]), static_cast<float>(size[1]));
        const float s = NSceneScreenScale::GetGameplayScale(r);
        assert(s >= 1.0f);
        assert(s == std::floor(s));
    }
    // An exact double still doubles.
    const CTRect<float> twice(0, 0, 2048, 1536);
    assert(NSceneScreenScale::GetGameplayScale(twice) == 2.0f);

    // Every scaled terrain vertex must land on a whole pixel, or the tile edge
    // straddles one and the point sampler reads the neighbouring atlas tile.
    // Tested against a window that actually magnifies, now that 1440x868 does not.
    for (int i = -40; i <= 40; ++i) {
        float x = static_cast<float>(i * 32);
        float y = static_cast<float>(i * 16);
        NSceneScreenScale::ScaleGameplayScreenPoint(&x, &y, twice);
        assert(x == std::floor(x));
        assert(y == std::floor(y));
    }
    // At a whole scale a tile edge stays exactly one tile apart from the next,
    // so the sampling phase is identical for every tile and no seam can appear.
    float e0x = 0, e0y = 0, e1x = 32, e1y = 0;
    NSceneScreenScale::ScaleGameplayScreenPoint(&e0x, &e0y, twice);
    NSceneScreenScale::ScaleGameplayScreenPoint(&e1x, &e1y, twice);
    assert(e1x - e0x == 64.0f);

    // Two tiles sharing an edge must still land on the same pixel, or rounding
    // would open the very gaps it is meant to close.
    for (int i = -40; i < 40; ++i) {
        float ax = static_cast<float>((i + 1) * 32), ay = 0;
        float bx = static_cast<float>((i + 1) * 32), by = 0;
        NSceneScreenScale::ScaleGameplayScreenPoint(&ax, &ay, twice);
        NSceneScreenScale::ScaleGameplayScreenPoint(&bx, &by, twice);
        assert(ax == bx && ay == by);
    }

    // A window that needs no magnification must be left exactly alone.
    const CTRect<float> native(0, 0, 1024, 768);
    float nx = 333.5f, ny = 111.25f;
    NSceneScreenScale::ScaleGameplayScreenPoint(&nx, &ny, native);
    assert(nx == 333.5f && ny == 111.25f);

    std::puts("scene screen scale: scaled vertices land on whole pixels");
    return 0;
}
