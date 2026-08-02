#include "../../Sources/src/Platform/LegacyVariant.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static char FoldAscii(char value) { return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value; }
static bool EqualAsciiIgnoreCase(const char *left, const char *right) {
    while (*left && *right && FoldAscii(*left) == FoldAscii(*right)) { ++left; ++right; }
    return *left == *right;
}

static void AddUniqueMode(std::vector<std::string> *modes, int width, int height, int bpp) {
    char text[64]; std::snprintf(text, sizeof(text), "%dx%dx%d", width, height, bpp);
    for (const std::string &mode : *modes) if (mode == text) return;
    modes->emplace_back(text);
}

int main() {
    variant_t text("1920x1080x32");
    assert(text.vt == VT_BSTR);
    variant_t number(32);
    assert(number.vt == VT_I4 && static_cast<int>(number) == 32);
    variant_t copy = text;
    assert(copy == text);
    assert(EqualAsciiIgnoreCase("GFX.Monitor", "gfx.monitor"));

    std::vector<std::string> modes;
    AddUniqueMode(&modes, 1920, 1080, 32);
    AddUniqueMode(&modes, 1920, 1080, 32);
    AddUniqueMode(&modes, 1280, 720, 32);
    assert(modes.size() == 2);
    assert(modes[0] == "1920x1080x32");
    assert(modes[1] == "1280x720x32");
    std::puts("options bridge contract tests passed");
    return 0;
}
