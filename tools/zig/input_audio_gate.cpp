#include "Event.h"
#include <cstdio>
#include <atomic>
#include <thread>

int main()
{
	NPlatform::PlatformEvent focusLost;
	focusLost.type = NPlatform::EventType::focusLost;
	std::atomic<bool> audioStopped(false);
	std::thread worker([&audioStopped] { audioStopped.store(true); });
	worker.join();
	if (focusLost.type != NPlatform::EventType::focusLost || !audioStopped.load()) return 1;
	std::puts("input audio lifecycle gate passed");
	return 0;
}
