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
    // The window that shows the artefact: 1440x868 gives 1.1302, not a whole number.
    const CTRect<float> screen(0, 0, 1440, 868);
    const float scale = NSceneScreenScale::GetGameplayScale(screen);
    assert(scale > 1.13f && scale < 1.131f);

    // Every scaled terrain vertex must land on a whole pixel, or the tile edge
    // straddles one and the point sampler reads the neighbouring atlas tile.
    for (int i = -40; i <= 40; ++i) {
        float x = static_cast<float>(i * 32);
        float y = static_cast<float>(i * 16);
        NSceneScreenScale::ScaleGameplayScreenPoint(&x, &y, screen);
        assert(x == std::floor(x));
        assert(y == std::floor(y));
    }

    // Two tiles sharing an edge must still land on the same pixel, or rounding
    // would open the very gaps it is meant to close.
    for (int i = -40; i < 40; ++i) {
        float ax = static_cast<float>((i + 1) * 32), ay = 0;
        float bx = static_cast<float>((i + 1) * 32), by = 0;
        NSceneScreenScale::ScaleGameplayScreenPoint(&ax, &ay, screen);
        NSceneScreenScale::ScaleGameplayScreenPoint(&bx, &by, screen);
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
