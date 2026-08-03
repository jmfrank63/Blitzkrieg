#ifndef BLITZKRIEG_PLATFORM_EVENT_H
#define BLITZKRIEG_PLATFORM_EVENT_H

namespace NPlatform
{
enum class EventType
{
	quit,
	focusGained,
	focusLost,
	windowMoved,
	windowResized,
	windowMinimized,
	windowRestored,
};
}

#endif
