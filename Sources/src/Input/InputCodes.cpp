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
// Every entry in `codes` above is a control the device registers, so a scancode
// this table does not name is a key the game can never see. It used to cover
// only the letters plus nine odds and ends, which silently disabled everything
// bound to a digit (the Ctrl+1..9 unit groups), to `-`/`=` (game speed), to Tab,
// and to the function keys.
std::uint32_t SDLScancodeToLegacy(std::uint32_t scancode)
{
	static const std::uint32_t letters[] = { 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c };
	if ( scancode >= 4 && scancode <= 29 ) return letters[scancode - 4];		// A..Z
	if ( scancode >= 30 && scancode <= 38 ) return 0x02 + ( scancode - 30 );	// 1..9
	if ( scancode >= 58 && scancode <= 67 ) return 0x3b + ( scancode - 58 );	// F1..F10
	// The keypad digits are laid out 7-8-9 / 4-5-6 / 1-2-3 in DIK order, so they
	// cannot be derived arithmetically from the SDL run KP_1..KP_9.
	static const std::uint32_t keypadDigits[] = { 0x4f, 0x50, 0x51, 0x4b, 0x4c, 0x4d, 0x47, 0x48, 0x49 };
	if ( scancode >= 89 && scancode <= 97 ) return keypadDigits[scancode - 89];
	switch ( scancode )
	{
		case 39: return 0x0b;		// 0
		case 40: return 0x1c;		// ENTER
		case 41: return 0x01;		// ESC
		case 42: return 0x0e;		// BACKSPACE
		case 43: return 0x0f;		// TAB
		case 44: return 0x39;		// SPACE
		case 45: return 0x0c;		// -
		case 46: return 0x0d;		// =
		case 47: return 0x1a;		// [
		case 48: return 0x1b;		// ]
		case 49: return 0x2b;		// backslash
		case 50: return 0x2b;		// non-US hash, same physical key as backslash
		case 51: return 0x27;		// ;
		case 52: return 0x28;		// '
		case 53: return 0x29;		// `
		case 54: return 0x33;		// ,
		case 55: return 0x34;		// .
		case 56: return 0x35;		// /
		case 57: return 0x3a;		// CAPITAL
		case 68: return 0x57;		// F11
		case 69: return 0x58;		// F12
		case 71: return 0x46;		// SCROLL
		case 73: return 0xd2;		// INSERT
		case 74: return 0xc7;		// HOME
		case 75: return 0xc9;		// PG_UP
		case 76: return 0xd3;		// DELETE
		case 77: return 0xcf;		// END
		case 78: return 0xd1;		// PG_DOWN
		case 79: return 0xcd;		// RIGHT
		case 80: return 0xcb;		// LEFT
		case 81: return 0xd0;		// DOWN
		case 82: return 0xc8;		// UP
		case 83: return 0x45;		// NUM
		case 84: return 0xb5;		// NUM_DIVIDE
		case 85: return 0x37;		// NUM_MULTIPLY
		case 86: return 0x4a;		// NUM_MINUS
		case 87: return 0x4e;		// NUM_PLUS
		case 88: return 0x9c;		// NUM_ENTER
		case 98: return 0x52;		// NUM_0
		case 99: return 0x53;		// NUM_PERIOD
		case 101: return 0xdd;		// APP_MENU
		case 224: return 0x1d;		// LCTRL
		case 225: return 0x2a;		// LSHIFT
		case 226: return 0x38;		// LALT
		case 227: return 0xdb;		// LWIN
		case 228: return 0x9d;		// RCTRL
		case 229: return 0x36;		// RSHIFT
		case 230: return 0xb8;		// RALT
		case 231: return 0xdc;		// RWIN
		default: return 0;
	}
}

// The UI reads STextMessage::nVirtualKey, which on Windows came from
// MapVirtualKeyEx over the DirectInput scancode. There is no such call here, so
// the mapping is spelled out. Values are Windows VK_* codes; the few the
// portable headers define are repeated numerically to keep this file a leaf.
std::uint32_t SDLScancodeToVirtualKey(std::uint32_t scancode)
{
	if ( scancode >= 4 && scancode <= 29 ) return 0x41 + ( scancode - 4 );		// 'A'..'Z'
	if ( scancode >= 30 && scancode <= 38 ) return 0x31 + ( scancode - 30 );	// '1'..'9'
	if ( scancode >= 58 && scancode <= 69 ) return 0x70 + ( scancode - 58 );	// VK_F1..VK_F12
	if ( scancode >= 89 && scancode <= 97 ) return 0x61 + ( scancode - 89 );	// VK_NUMPAD1..9
	switch ( scancode )
	{
		case 39: return 0x30;		// '0'
		case 40: return 0x0d;		// VK_RETURN
		case 41: return 0x1b;		// VK_ESCAPE
		case 42: return 0x08;		// VK_BACK
		case 43: return 0x09;		// VK_TAB
		case 44: return 0x20;		// VK_SPACE
		case 45: return 0xbd;		// VK_OEM_MINUS
		case 46: return 0xbb;		// VK_OEM_PLUS
		case 47: return 0xdb;		// VK_OEM_4
		case 48: return 0xdd;		// VK_OEM_6
		case 49: return 0xdc;		// VK_OEM_5
		case 50: return 0xdc;		// non-US hash, same physical key as backslash
		case 51: return 0xba;		// VK_OEM_1
		case 52: return 0xde;		// VK_OEM_7
		// CUIScreen::OnChar tests for 192 by value to toggle the console.
		case 53: return 0xc0;		// VK_OEM_3
		case 54: return 0xbc;		// VK_OEM_COMMA
		case 55: return 0xbe;		// VK_OEM_PERIOD
		case 56: return 0xbf;		// VK_OEM_2
		case 57: return 0x14;		// VK_CAPITAL
		case 70: return 0x2c;		// VK_SNAPSHOT
		case 71: return 0x91;		// VK_SCROLL
		case 72: return 0x13;		// VK_PAUSE
		case 73: return 0x2d;		// VK_INSERT
		case 74: return 0x24;		// VK_HOME
		case 75: return 0x21;		// VK_PRIOR
		case 76: return 0x2e;		// VK_DELETE
		case 77: return 0x23;		// VK_END
		case 78: return 0x22;		// VK_NEXT
		case 79: return 0x27;		// VK_RIGHT
		case 80: return 0x25;		// VK_LEFT
		case 81: return 0x28;		// VK_DOWN
		case 82: return 0x26;		// VK_UP
		case 83: return 0x90;		// VK_NUMLOCK
		case 84: return 0x6f;		// VK_DIVIDE
		case 85: return 0x6a;		// VK_MULTIPLY
		case 86: return 0x6d;		// VK_SUBTRACT
		case 87: return 0x6b;		// VK_ADD
		case 88: return 0x0d;		// VK_RETURN, from the keypad
		case 98: return 0x60;		// VK_NUMPAD0
		case 99: return 0x6e;		// VK_DECIMAL
		case 101: return 0x5d;		// VK_APPS
		// The screens test the side-independent modifiers, as Windows reported
		// them for an unextended MapVirtualKeyEx.
		case 224: case 228: return 0x11;	// VK_CONTROL
		case 225: case 229: return 0x10;	// VK_SHIFT
		case 226: case 230: return 0x12;	// VK_MENU
		case 227: return 0x5b;		// VK_LWIN
		case 231: return 0x5c;		// VK_RWIN
		default: return 0;
	}
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
