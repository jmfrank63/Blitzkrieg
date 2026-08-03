#include "Event.h"
#include <cstdio>

int main()
{
	NPlatform::PlatformEvent event;
	event.type = NPlatform::EventType::controllerAxis;
	event.deviceId = 7;
	event.control = 1;
	event.value = -32768;
	if (event.deviceId != 7 || event.value != -32768) return 1;
	std::puts("platform controller and clipboard contract passed");
	return 0;
}
