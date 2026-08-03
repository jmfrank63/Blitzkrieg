#ifndef BLITZKRIEG_INPUT_CODES_H
#define BLITZKRIEG_INPUT_CODES_H




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
std::size_t DecodeUtf8(const char *text, std::uint16_t *output, std::size_t capacity);
}

#endif
