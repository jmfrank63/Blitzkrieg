#include "../../Sources/src/Platform/SDLApplication.h"

#include <cstdio>
#include <cstring>

int main()
{
	NPlatform::SDLApplication application;
	if (!application.AddVirtualControllerForTests(7, "Virtual Test Pad") || application.AddVirtualControllerForTests(7, "duplicate"))
		return 1;
	char name[32] = {};
	if (!application.GetControllerName(7, name, sizeof(name)) || std::strcmp(name, "Virtual Test Pad") != 0)
		return 2;
	char shortName[8] = {};
	if (application.GetControllerName(7, shortName, sizeof(shortName)) || std::strcmp(shortName, "Virtual") != 0)
		return 3;
	if (!application.RemoveVirtualControllerForTests(7) || application.GetControllerName(7, name, sizeof(name)))
		return 4;
	std::puts("platform controller passed: virtual name copy and removal");
	return 0;
}
