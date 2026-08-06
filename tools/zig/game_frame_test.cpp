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
	std::puts( "game frame contract compiled: SDL-owned lifecycle, normalized event queue, focus/quit state" );
	return 0;
}
