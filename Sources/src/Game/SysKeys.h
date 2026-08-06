#ifndef BLITZKRIEG_SYSKEYS_H
#define BLITZKRIEG_SYSKEYS_H

#include "../Platform/Event.h"

namespace NSysKeys
{
enum class Action
{
	pass,
	consume,
	toggleFullscreen,
	releaseMouse,
};

void EnableSystemKeys( bool enabled );
void Reset();
Action Process( const NPlatform::PlatformEvent &event );
}

#endif
