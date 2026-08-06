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
	controllerAdded,
	controllerRemoved,
	controllerButtonDown,
	controllerButtonUp,
	controllerAxis,
};

// SDL-compatible key values used by the platform event boundary. SDL headers
// stay private to SDLApplication.cpp and are not required by game code.
enum class PlatformKey : int
{
	returnKey = 0x0000000d,
	escape = 0x0000001b,
	space = 0x00000020,
	plus = 0x0000002b,
	minus = 0x0000002d,
};

enum PlatformModifier : int
{
	modifierNone = 0x0000,
	modifierControl = 0x00c0,
	modifierAlt = 0x0300,
	modifierGui = 0x0c00,
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
	int scancode = 0;
	int modifiers = 0;
	int button = 0;
	int deviceId = 0;
	int control = 0;
	int value = 0;
	bool repeat = false;
	char text[64] = {};
};
}

#endif
