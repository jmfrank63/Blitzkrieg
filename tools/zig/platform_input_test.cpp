#include "InputCodes.h"
#include <cstdio>

static bool Check(bool condition, const char *message)
{
	if (!condition) std::fprintf(stderr, "platform input test failed: %s\n", message);
	return condition;
}

int main()
{
	std::uint16_t utf16[8] = {};
	const std::size_t count = NInput::DecodeUtf8("A\xC3\xA9\xF0\x9F\x92\xA5", utf16, 8);
	if (!Check(count == 4, "UTF-8 expands to UTF-16 code units")) return 1;
	if (!Check(utf16[0] == 'A' && utf16[1] == 0x00e9, "Unicode BMP text")) return 1;
	if (!Check(utf16[2] == 0xd83d && utf16[3] == 0xdca5, "Unicode supplementary text")) return 1;
	if (!Check(NInput::SDLScancodeToLegacy(4) == 0x1e, "SDL A scancode")) return 1;
	if (!Check(NInput::SDLScancodeToLegacy(41) == 0x01, "SDL escape scancode")) return 1;
	std::puts("platform input event contract passed");
	return 0;
}
