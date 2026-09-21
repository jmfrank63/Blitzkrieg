// The engine tier. Needs a hidden SDL window and a GPU device.
//
// Where there is no video driver at all it prints a skip and exits 0, because
// three of the six CI runners have none (see the spec's tier table). Everything
// else - SDL failing for another reason, the window failing after SDL started,
// or the bridge failing on a machine that does have a device - is a failure
// with exit 1. A tier that cannot tell a skip from a failure is worse than no
// tier: the map file tier once swept zero maps and reported success.
#include "StdAfx.h"
#include <SDL3/SDL.h>
#include "../../Sources/src/EditorBridge/bridge.h"

static int g_nFailures = 0;

static bool Check( bool bCondition, const char *pszWhat )
{
	if ( !bCondition )
	{
		printf( "FAIL: %s\n", pszWhat );
		++g_nFailures;
	}
	return bCondition;
}

int main( int argc, char **argv )
{
	// A real hidden window, never a null handle: passing 0 would take the
	// no-device path on every machine and the tier would skip itself into
	// always-green.
	if ( !SDL_Init( SDL_INIT_VIDEO ) )
	{
		const char *pszError = SDL_GetError();
		if ( strstr( pszError, "video driver" ) != 0 || strstr( pszError, "No available" ) != 0 )
		{
			printf( "editor-bridge: skipped: no video driver (%s)\n", pszError );
			return 0;
		}
		printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	SDL_Window *pWindow = SDL_CreateWindow( "editor-bridge-test", 640, 480, SDL_WINDOW_HIDDEN );
	if ( pWindow == 0 )
	{
		printf( "FAIL: SDL_CreateWindow: %s\n", SDL_GetError() );
		SDL_Quit();
		return 1;
	}

	// The engine tier needs an engine: an installation with the modules and
	// Data beside each other, which is what install-game stages. The repository
	// tree is not one - its dylibs are in zig-out/lib and its Data at ./Data -
	// so point at the staged layout and say plainly when it is not there rather
	// than failing in the middle of startup.
	const char *pszRoot = argc > 1 ? argv[1] : "zig-out/game/macos/arm64/release";
	FILE *pProbe = fopen( ( std::string( pszRoot ) + "/Data/consts.xml" ).c_str(), "rb" );
	if ( pProbe == 0 )
	{
		printf( "editor-bridge: skipped: no staged game at %s (run: zig build install-game)\n", pszRoot );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
	}
	fclose( pProbe );

	// And the executable has to live inside that installation.
	//
	// Every engine module derives its own roots from the running executable's
	// location - each dylib links its own copy of NPlatform::Paths, and
	// BaseRoot() comes from executableRoot() - so telling the bridge where the
	// game is only moves the bridge's copy. Run this from the build cache and
	// libAILogic computes a base of .zig-cache/..., fails to find StreamIO
	// there, leaves its g_pGlobalSingleton null and dereferences it: an
	// EXC_BAD_ACCESS inside GetSingleton<IGlobalVars> with no message.
	//
	// Until the staging step installs this executable beside Game, say so and
	// skip. A crash with no explanation is the one outcome worse than a skip.
	{
		const std::string szSelf = argv[0] != 0 ? argv[0] : "";
		const std::string szRoot( pszRoot );
		if ( szSelf.find( szRoot ) == std::string::npos )
		{
			printf( "editor-bridge: skipped: this executable is at %s, outside the installation at %s;\n"
			        "               every engine module derives its roots from the executable's location,\n"
			        "               so it has to be staged beside Game before it can start the engine\n",
			        szSelf.c_str(), szRoot.c_str() );
			SDL_DestroyWindow( pWindow );
			SDL_Quit();
			return 0;
		}
	}

	BkEditorSession *pSession = 0;
	const BkEditorStatus status = BkEditorStart( pWindow, pszRoot, &pSession );
	if ( status == BK_EDITOR_NO_DEVICE )
	{
		printf( "editor-bridge: skipped: no GPU device (%s)\n", BkEditorLastMessage( pSession ) );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
	}
	// pSession may be null here: the start can fail before it allocates one,
	// which is why BkEditorLastMessage is defined for a null session.
	if ( !Check( status == BK_EDITOR_OK, "the bridge starts" ) )
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
	else
		Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "and stops" );

	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	if ( g_nFailures == 0 )
		printf( "editor-bridge: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
