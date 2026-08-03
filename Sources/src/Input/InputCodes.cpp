#include "InputCodes.h"

#include <cstring>

namespace NInput
{
// DIK-compatible values are part of the config ABI. They are intentionally
// kept here so SDL and legacy backends share the same stable identifiers.
static const KeyCodeEntry codes[] = {
	{ "ESC", 0x01 }, { "1", 0x02 }, { "2", 0x03 }, { "3", 0x04 }, { "4", 0x05 }, { "5", 0x06 }, { "6", 0x07 }, { "7", 0x08 }, { "8", 0x09 }, { "9", 0x0a }, { "0", 0x0b },
	{ "-", 0x0c }, { "=", 0x0d }, { "BACKSPACE", 0x0e }, { "TAB", 0x0f }, { "Q", 0x10 }, { "W", 0x11 }, { "E", 0x12 }, { "R", 0x13 }, { "T", 0x14 }, { "Y", 0x15 }, { "U", 0x16 }, { "I", 0x17 }, { "O", 0x18 }, { "P", 0x19 },
	{ "[", 0x1a }, { "]", 0x1b }, { "ENTER", 0x1c }, { "LCTRL", 0x1d }, { "A", 0x1e }, { "S", 0x1f }, { "D", 0x20 }, { "F", 0x21 }, { "G", 0x22 }, { "H", 0x23 }, { "J", 0x24 }, { "K", 0x25 }, { "L", 0x26 }, { ";", 0x27 }, { "'", 0x28 }, { "`", 0x29 },
	{ "LSHIFT", 0x2a }, { "\\", 0x2b }, { "Z", 0x2c }, { "X", 0x2d }, { "C", 0x2e }, { "V", 0x2f }, { "B", 0x30 }, { "N", 0x31 }, { "M", 0x32 }, { ",", 0x33 }, { ".", 0x34 }, { "/", 0x35 }, { "RSHIFT", 0x36 }, { "NUM_MULTIPLY", 0x37 }, { "LALT", 0x38 }, { "SPACE", 0x39 }, { "CAPITAL", 0x3a },
	{ "F1", 0x3b }, { "F2", 0x3c }, { "F3", 0x3d }, { "F4", 0x3e }, { "F5", 0x3f }, { "F6", 0x40 }, { "F7", 0x41 }, { "F8", 0x42 }, { "F9", 0x43 }, { "F10", 0x44 }, { "NUM", 0x45 }, { "SCROLL", 0x46 }, { "NUM_7", 0x47 }, { "NUM_8", 0x48 }, { "NUM_9", 0x49 }, { "NUM_MINUS", 0x4a }, { "NUM_4", 0x4b }, { "NUM_5", 0x4c }, { "NUM_6", 0x4d }, { "NUM_PLUS", 0x4e }, { "NUM_1", 0x4f }, { "NUM_2", 0x50 }, { "NUM_3", 0x51 }, { "NUM_0", 0x52 }, { "NUM_PERIOD", 0x53 },
	{ "F11", 0x57 }, { "F12", 0x58 }, { "NUM_ENTER", 0x9c }, { "RCTRL", 0x9d }, { "NUM_DIVIDE", 0xb5 }, { "RALT", 0xb8 }, { "HOME", 0xc7 }, { "UP", 0xc8 }, { "PG_UP", 0xc9 }, { "LEFT", 0xcb }, { "RIGHT", 0xcd }, { "END", 0xcf }, { "DOWN", 0xd0 }, { "PG_DOWN", 0xd1 }, { "INSERT", 0xd2 }, { "DELETE", 0xd3 }, { "LWIN", 0xdb }, { "RWIN", 0xdc }, { "APP_MENU", 0xdd },
};

const KeyCodeEntry *KeyboardCodes(std::size_t *count) { if ( count ) *count = sizeof(codes) / sizeof(codes[0]); return codes; }
std::uint32_t CodeForName(const char *name) { for (const auto &entry : codes) if (name && std::strcmp(entry.name, name) == 0) return entry.code; return 0; }
const char *NameForCode(std::uint32_t code) { for (const auto &entry : codes) if (entry.code == code) return entry.name; return "UNKNOWN_KEY"; }
std::uint32_t SDLScancodeToLegacy(std::uint32_t scancode)
{
	static const std::uint32_t letters[] = { 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c };
	if ( scancode >= 4 && scancode <= 29 ) return letters[scancode - 4];
	switch ( scancode ) { case 40: return 0x1c; case 41: return 0x01; case 44: return 0x39; case 224: return 0x1d; case 225: return 0x2a; case 226: return 0x38; case 228: return 0x9d; case 229: return 0x36; case 230: return 0xb8; default: return 0; }
}

std::size_t DecodeUtf8(const char *text, std::uint16_t *output, std::size_t capacity)
{
	if ( !text || !output || capacity == 0 ) return 0;
	const unsigned char *p = reinterpret_cast<const unsigned char *>( text );
	std::size_t count = 0;
	while ( *p && count < capacity )
	{
		std::uint32_t codepoint = 0;
		std::size_t length = 1;
		if ( (*p & 0x80) == 0 ) codepoint = *p;
		else if ( (*p & 0xe0) == 0xc0 ) { codepoint = *p & 0x1f; length = 2; }
		else if ( (*p & 0xf0) == 0xe0 ) { codepoint = *p & 0x0f; length = 3; }
		else if ( (*p & 0xf8) == 0xf0 ) { codepoint = *p & 0x07; length = 4; }
		bool valid = true;
		for ( std::size_t i = 1; i < length; ++i )
		{
			if ( (p[i] & 0xc0) != 0x80 ) { valid = false; break; }
			codepoint = (codepoint << 6) | (p[i] & 0x3f);
		}
		if ( !valid ) { ++p; continue; }
		if ( codepoint <= 0xffff ) output[count++] = static_cast<std::uint16_t>( codepoint );
		else if ( codepoint <= 0x10ffff && count + 1 < capacity )
		{
			codepoint -= 0x10000;
			output[count++] = static_cast<std::uint16_t>( 0xd800 + (codepoint >> 10) );
			output[count++] = static_cast<std::uint16_t>( 0xdc00 + (codepoint & 0x3ff) );
		}
		p += length;
	}
	return count;
}
}
