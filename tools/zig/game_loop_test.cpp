#include "../../Sources/src/Game/GameFrame.h"

#include <cstdio>
#include <string>

#define CHECK(condition) \
	do { \
		if ( !(condition) ) { \
			std::fprintf( stderr, "game loop check failed: %s\n", #condition ); \
			return 1; \
		} \
	} while ( false )

static NPlatform::PlatformEvent Event( NPlatform::EventType type )
{
	NPlatform::PlatformEvent event{};
	event.type = type;
	return event;
}

int main()
{
	NGame::FramePacingPolicy policy;
	CHECK( policy.Next().render && policy.Next().sleepMilliseconds == 0 );
	policy.Apply( Event( NPlatform::EventType::focusLost ) );
	CHECK( !policy.Next().render && policy.Next().sleepMilliseconds == 40 );
	policy.Apply( Event( NPlatform::EventType::focusGained ) );
	CHECK( policy.Next().render );
	policy.Apply( Event( NPlatform::EventType::windowMinimized ) );
	CHECK( policy.IsMinimized() && !policy.Next().render );
	policy.Apply( Event( NPlatform::EventType::windowResized ) );
	CHECK( policy.IsMinimized() && !policy.Next().render );
	policy.Apply( Event( NPlatform::EventType::windowRestored ) );
	CHECK( !policy.IsMinimized() && policy.Next().render );
	policy.Apply( Event( NPlatform::EventType::quit ) );
	CHECK( policy.IsQuitRequested() && !policy.Next().render );
	policy.Reset();
	CHECK( !policy.IsQuitRequested() && policy.Next().render );
	std::string trace;
	for ( int cycle = 0; cycle != 3; ++cycle )
	{
		policy.Reset();
		trace += policy.Next().render ? 'R' : 's';
		policy.Apply( Event( NPlatform::EventType::focusLost ) );
		trace += policy.Next().render ? 'R' : 's';
		policy.Apply( Event( NPlatform::EventType::focusGained ) );
		trace += policy.Next().render ? 'R' : 's';
		policy.Apply( Event( NPlatform::EventType::quit ) );
		trace += policy.Next().render ? 'R' : 's';
	}
	CHECK( trace == "RsRsRsRsRsRs" );
	std::puts( "game loop policy passed: focus/minimize/resize/quit pacing and 3 restart traces" );
	return 0;
}
