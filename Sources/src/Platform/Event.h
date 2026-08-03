#ifndef BLITZKRIEG_PLATFORM_EVENT_H
#define BLITZKRIEG_PLATFORM_EVENT_H

#include <cstdint>

namespace NPlatform
{
enum class EventType
{
	unknown,
	quit,
	focusGained,
	focusLost,
	windowMoved,
	windowResized,
	windowMinimized,
	windowRestored,
	keyDown,
	keyUp,
	textInput,
	mouseMotion,
	mouseButtonDown,
	mouseButtonUp,
	mouseWheel,
};

struct PlatformEvent
{
	EventType type = EventType::unknown;
	std::uint64_t timestamp = 0;
	std::uint64_t windowId = 0;
	int x = 0;
	int y = 0;
	int data1 = 0;
	int data2 = 0;
	int key = 0;
	int modifiers = 0;
	int button = 0;
	bool repeat = false;
	char text[64] = {};
};
}

#endif
