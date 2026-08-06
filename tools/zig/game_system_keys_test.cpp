#include "../../Sources/src/Game/SysKeys.h"

#include <cstdio>

#define CHECK(condition) \
	do { \
		if ( !(condition) ) { \
			std::fprintf( stderr, "system key check failed: %s\n", #condition ); \
			return 1; \
		} \
	} while ( false )

static NPlatform::PlatformEvent Key( NPlatform::EventType type, int key = 0, int modifiers = NPlatform::modifierNone )
{
	NPlatform::PlatformEvent event{};
	event.type = type;
	event.key = key;
	event.modifiers = modifiers;
	return event;
}

int main()
{
	NSysKeys::EnableSystemKeys( true );
	CHECK( NSysKeys::Process( Key( NPlatform::EventType::keyDown, static_cast<int>( NPlatform::PlatformKey::returnKey ), NPlatform::modifierAlt ) ) == NSysKeys::Action::toggleFullscreen );
	CHECK( NSysKeys::Process( Key( NPlatform::EventType::keyUp, static_cast<int>( NPlatform::PlatformKey::returnKey ), NPlatform::modifierAlt ) ) == NSysKeys::Action::pass );
	CHECK( NSysKeys::Process( Key( NPlatform::EventType::keyDown, static_cast<int>( NPlatform::PlatformKey::plus ) ) ) == NSysKeys::Action::pass );
	CHECK( NSysKeys::Process( Key( NPlatform::EventType::keyDown, static_cast<int>( NPlatform::PlatformKey::minus ) ) ) == NSysKeys::Action::pass );
	CHECK( NSysKeys::Process( Key( NPlatform::EventType::keyDown, static_cast<int>( NPlatform::PlatformKey::escape ), NPlatform::modifierControl ) ) == NSysKeys::Action::releaseMouse );
	CHECK( NSysKeys::Process( Key( NPlatform::EventType::keyDown, static_cast<int>( NPlatform::PlatformKey::space ), NPlatform::modifierGui ) ) == NSysKeys::Action::consume );
	CHECK( NSysKeys::Process( Key( NPlatform::EventType::focusLost ) ) == NSysKeys::Action::pass );
	CHECK( NSysKeys::Process( Key( NPlatform::EventType::quit ) ) == NSysKeys::Action::pass );
	NSysKeys::Reset();
	std::puts( "system key policy passed: toggle, release, reserved suppression, focus cleanup" );
	return 0;
}
