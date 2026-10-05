// The resource bridge smoke tier (S04 T01): starts the engine the same way
// test-editor-bridge does, calls BkResNew on a wpn project, BkResClose,
// BkEditorStop. On a host without a GPU (CI's Linux runner, three of the six)
// the start reports BK_EDITOR_NO_DEVICE and this exits 0 with a one-line
// "skipped: no GPU device" message - the same gate editor_bridge_test.cpp uses.
// CI runners that do have a device set BK_REQUIRE_ENGINE=1, where a skip is
// then a failure, so a regression on the real runners cannot hide as a skip.
#include "StdAfx.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <SDL3/SDL.h>
#include "resource_bridge.h"

#if defined(_WIN32) || defined(_WIN64)
#include <crtdbg.h>
#endif

static int g_nFailures = 0;

static bool Check( bool bCondition, const char *pszWhat )
{
	if ( !bCondition )
	{
		std::printf( "FAIL: %s\n", pszWhat );
		++g_nFailures;
	}
	return bCondition;
}

static std::string DirectoryOf( const char *pszPath )
{
	const std::string szPath( pszPath );
	const std::string::size_type nCut = szPath.find_last_of( "/\\" );
	return nCut == std::string::npos ? std::string( "." ) : szPath.substr( 0, nCut );
}

static int SkipOrFail( const std::string &szWhy )
{
	const char *pszRequire = std::getenv( "BK_REQUIRE_ENGINE" );
	if ( pszRequire != 0 && *pszRequire != 0 && std::strcmp( pszRequire, "0" ) != 0 )
	{
		std::printf( "FAIL: resource-bridge: %s, and BK_REQUIRE_ENGINE is set\n", szWhy.c_str() );
		return 1;
	}
	std::printf( "resource-bridge: skipped: %s\n", szWhy.c_str() );
	return 0;
}

int main( int argc, char **argv )
{
#if defined(_WIN32) || defined(_WIN64)
	_set_error_mode( _OUT_TO_STDERR );
	_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
#endif

	// A real hidden window, never a null handle. SDL_WINDOW_NOT_FOCUSABLE
	// keeps a Linux compositor from stealing focus to a window the user
	// never asked for; SDL_WINDOW_HIDDEN keeps it off the taskbar. Both are
	// what the Map Editor's hidden tiers use (AGENTS.md: Linux pitfalls).
	if ( !SDL_Init( SDL_INIT_VIDEO ) )
	{
		const char *pszError = SDL_GetError();
		if ( std::strstr( pszError, "video driver" ) != 0 || std::strstr( pszError, "No available" ) != 0 )
			return SkipOrFail( std::string( "no video driver (" ) + pszError + ")" );
		std::printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	SDL_Window *pWindow = SDL_CreateWindow( "resource-bridge-test", 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE );
	if ( pWindow == 0 )
	{
		std::printf( "FAIL: SDL_CreateWindow: %s\n", SDL_GetError() );
		SDL_Quit();
		return 1;
	}

	// The installation to edit is this executable's own directory, like
	// editor-bridge-test: every engine module derives its roots from the
	// running executable's location, so running from anywhere else gives
	// each module a different root. The run step passes "." as that root
	// (the executable's cwd is set to the install directory).
	const std::string szSelfDir = DirectoryOf( argv[0] != 0 ? argv[0] : "." );
	const char *pszRoot = argc > 1 ? argv[1] : szSelfDir.c_str();
	// A probe for the staged game: without a staged install this cannot run
	// and the tier skips rather than failing (same shape as editor-bridge).
	FILE *pProbe = std::fopen( ( std::string( pszRoot ) + "/Data/consts.xml" ).c_str(), "rb" );
	if ( pProbe == 0 )
	{
		const int nSkipped = SkipOrFail( std::string( "no staged game at " ) + pszRoot + " (run: zig build install-game)" );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	std::fclose( pProbe );

	BkEditorSession *pSession = 0;
	const BkEditorStatus start = BkEditorStart( pWindow, pszRoot, &pSession );
	if ( start == BK_EDITOR_NO_DEVICE )
	{
		const int nSkipped = SkipOrFail( std::string( "no GPU device (" ) + BkEditorLastMessage( pSession ) + ")" );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	if ( !Check( start == BK_EDITOR_OK, "the bridge starts" ) )
	{
		std::printf( "resource-bridge: %s\n", BkEditorLastMessage( pSession ) );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 1;
	}

	// The smoke sequence: BkResNew on wpn (kind code 0 - the first entry of
	// EResourceKind), read the kind back, close it. Stubs today (T01), but
	// the host must compile and link against them, and a later task will
	// replace them without breaking this sequence.
	Check( BkResNew( pSession, 0 ) == BK_EDITOR_OK, "BkResNew(wpn) answers OK" );
	BkResKind kind = -1;
	Check( BkResKindOf( pSession, &kind ) == BK_EDITOR_OK, "BkResKindOf answers OK" );
	// Two-pass count read with no project open semantics: an empty tree answers 0.
	int nNodes = -1;
	Check( BkResNodes( pSession, 0, 0, &nNodes ) == BK_EDITOR_OK, "BkResNodes answers OK with a null buffer" );
	Check( nNodes == 0, "BkResNodes answers a count of 0 for the stub" );
	Check( BkResClose( pSession ) == BK_EDITOR_OK, "BkResClose answers OK" );

	Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "the bridge stops" );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();

	if ( g_nFailures == 0 )
		std::printf( "resource-bridge: smoke OK\n" );
	else
		std::printf( "resource-bridge: %d failures\n", g_nFailures );
	return g_nFailures == 0 ? 0 : 1;
}
