#ifndef BLITZKRIEG_INPUT_CODES_H
#define BLITZKRIEG_INPUT_CODES_H

#include <cstddef>
#include <cstdint>

namespace NInput
{
struct KeyCodeEntry
{
	const char *name;
	std::uint32_t code;
};

const KeyCodeEntry *KeyboardCodes(std::size_t *count);
std::uint32_t CodeForName(const char *name);
const char *NameForCode(std::uint32_t code);
std::uint32_t SDLScancodeToLegacy(std::uint32_t scancode);
std::uint32_t SDLScancodeToVirtualKey(std::uint32_t scancode);

// Windows built the character from the scancode itself, in Convert2Text, rather
// than waiting for a separate OS text message. SDL only reports text through
// SDL_EVENT_TEXT_INPUT, which needs SDL_StartTextInput and which an IME can
// suppress, and an edit box handed a key with no character does nothing at all.
// Returns 0 when the key types nothing, or when shift makes the result depend
// on a keyboard layout this cannot know.
uint16_t CharacterFromKeycode( int keycode, int modifiers );
std::size_t DecodeUtf8(const char *text, std::uint16_t *output, std::size_t capacity);
}

#endif
