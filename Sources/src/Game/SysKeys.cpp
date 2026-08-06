#include "SysKeys.h"

#include <set>

namespace
{
bool system_keys_enabled = true;
std::set<int> pressed_keys;
}

namespace NSysKeys
{
void EnableSystemKeys( const bool enabled )
{
	system_keys_enabled = enabled;
	if ( !enabled ) pressed_keys.clear();
}

void Reset() { pressed_keys.clear(); }

Action Process( const NPlatform::PlatformEvent &event )
{
	if ( event.type == NPlatform::EventType::focusLost )
	{
		pressed_keys.clear();
		return Action::pass;
	}
	if ( event.type == NPlatform::EventType::keyUp )
	{
		pressed_keys.erase( event.key );
		return Action::pass;
	}
	if ( event.type != NPlatform::EventType::keyDown ) return Action::pass;
	pressed_keys.insert( event.key );
	if ( !system_keys_enabled ) return Action::pass;
	if ( event.modifiers == ( event.modifiers | NPlatform::modifierGui ) ) return Action::consume;
	if ( event.key == static_cast<int>( NPlatform::PlatformKey::returnKey ) &&
		( event.modifiers & NPlatform::modifierAlt ) != 0 ) return Action::toggleFullscreen;
	if ( event.key == static_cast<int>( NPlatform::PlatformKey::escape ) &&
		( event.modifiers & NPlatform::modifierControl ) != 0 ) return Action::releaseMouse;
	return Action::pass;
}
}
