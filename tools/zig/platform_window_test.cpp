#include "../../Sources/src/Platform/SDLApplication.h"

#include <cstdio>

#define CHECK(condition) \
	do { \
		if ( !(condition) ) { \
			std::fprintf( stderr, "platform window check failed: %s\\n", #condition ); \
			return 1; \
		} \
	} while ( false )

int main()
{
	NPlatform::SDLApplication::SetInitializationFailureForTests( true );
	NPlatform::SDLApplication injected;
	CHECK( !injected.Initialize( "Blitzkrieg test", 320, 200 ) );
	CHECK( !injected.LastError().empty() );
	injected.Shutdown();
	NPlatform::SDLApplication::SetInitializationFailureForTests( false );

	NPlatform::SDLApplication app;
	CHECK( app.Initialize( "Blitzkrieg test", 320, 200 ) );
	CHECK( app.BorrowWindow().value != nullptr );
	CHECK( app.LogicalSize().width == 320 && app.LogicalSize().height == 200 );
	CHECK( app.PixelSize().width > 0 && app.PixelSize().height > 0 );
	CHECK( !app.IsVisible() );
	app.Show();
	CHECK( app.IsVisible() );
	app.Hide();
	CHECK( !app.IsVisible() );
	app.Shutdown();
	app.Shutdown();
	CHECK( app.BorrowWindow().value == nullptr );
	return 0;
}
