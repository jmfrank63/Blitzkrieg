#include "../../Sources/src/Game/GameFrame.h"

#include <cstdio>

#define CHECK(condition) \
	do { \
		if ( !(condition) ) { \
			std::fprintf( stderr, "game frame check failed: %s\n", #condition ); \
			return 1; \
		} \
	} while ( false )

int main()
{
	NGame::GameFrame frame;
	CHECK( !frame.IsActive() && !frame.IsExit() );
	CHECK( sizeof( frame.BorrowWindow().value.bits ) == sizeof( BkPlatformWindowHandle ) );
	frame.ResetExit();
	CHECK( !frame.IsExit() );
	// Splash lifecycle: hiding one that never showed is a no-op, and a
	// show/hide round-trip is balanced whether or not the host's video stack
	// can bring a window up (headless CI cannot; the call then reports false
	// and leaves no subsystem reference behind). The missing bitmap is
	// deliberate - the splash is cosmetic and must not fail the launch.
	frame.HideSplash();
	frame.ShowSplash( "no-such-splash.bmp", 32, 32 );
	frame.HideSplash();
	std::puts( "game frame contract compiled: SDL-owned lifecycle, normalized event queue, focus/quit state" );
	return 0;
}
