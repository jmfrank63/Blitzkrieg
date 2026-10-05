// test-resource-bridge: Project+Tree C ABI tier.
//
// T01 scaffolded this file as a smoke that stops after BkResNew(wpn) +
// BkResClose. T02 extends it: open each of the 21 fixtures through
// BkResOpen, save it back through BkResSave (safe-save read-back), and
// byte-compare the saved file against the fixture. For a representative
// subset (wpn, msh, pcp - stats-only, keyframe, image fronts) it also
// exercises delete -> restore -> save and asserts byte-identity.
//
// On a host without a GPU the start reports BK_EDITOR_NO_DEVICE and the
// test exits 0 with "skipped: no GPU device", mirroring editor_bridge_test.cpp.
// CI runners that have a device set BK_REQUIRE_ENGINE=1; a skip is then a
// failure, so a regression on those runners cannot hide as a skip.
//
// argv:
//   [0] self
//   [1] staged install root (contains Data/consts.xml); defaults to the
//       executable's own directory, like the editor_bridge test.
//   [2] fixture source root: tools/zig/fixtures/resource_editor
//   [3] scratch output root: zig-out/local-test/resource_editor/t02
#include "StdAfx.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
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

static bool ReadBytes( const std::string &szPath, std::string &out )
{
	std::ifstream f( szPath, std::ios::binary );
	if ( !f ) return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

// The 21 fixture extensions, in EXTENSIONS.md / kind-table order. The index
// here must match the BkResKind ordinal: a mismatch between the test's table
// and the bridge's would silently align with the wrong root.
struct Fixture { const char *pszExt; int nKindOrdinal; };
static const Fixture kFixtures[] = {
	{ "wpn", 0  }, { "mcp", 1  }, { "trc", 2  }, { "scp", 3  },
	{ "spt", 4  }, { "unt", 5  }, { "msh", 6  }, { "obt", 7  },
	{ "fnc", 8  }, { "bld", 9  }, { "bdg", 10 }, { "pcp", 11 },
	{ "eff", 12 }, { "til", 13 }, { "3rd", 14 }, { "3rv", 15 },
	{ "mip", 16 }, { "chc", 17 }, { "cgc", 18 }, { "mdc", 19 },
	{ "gui", 20 },
};
static const int kFixtureCount = int( sizeof(kFixtures) / sizeof(kFixtures[0]) );

static bool RoundTripOne( BkResSession *pSession, const std::string &szFixtureRoot,
                          const std::string &szScratchRoot, const Fixture &fx )
{
	const std::string szIn = szFixtureRoot + "/" + fx.pszExt + "/project." + fx.pszExt;
	const std::string szOutDir = szScratchRoot + "/" + fx.pszExt;
	const std::string szOut = szOutDir + "/project." + fx.pszExt;
	std::error_code ec;
	std::filesystem::create_directories( szOutDir, ec );
	// Make sure stale state from a prior run cannot mask a regression.
	std::filesystem::remove( szOut, ec );
	std::filesystem::remove( szOut + ".bak", ec );
	std::filesystem::remove( szOut + ".tmp", ec );

	std::string szWhat;
	bool ok = true;

	szWhat = std::string( fx.pszExt ) + ": BkResOpen";
	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return false;
	}

	BkResKind kind = -2;
	szWhat = std::string( fx.pszExt ) + ": BkResKindOf";
	ok = Check( BkResKindOf( pSession, &kind ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	szWhat = std::string( fx.pszExt ) + ": kind ordinal matches";
	ok = Check( kind == fx.nKindOrdinal, szWhat.c_str() ) && ok;

	// Count nodes so the two-pass contract exercises both branches.
	int nCount = -1;
	szWhat = std::string( fx.pszExt ) + ": BkResNodes count (null buffer)";
	ok = Check( BkResNodes( pSession, 0, 0, &nCount ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	szWhat = std::string( fx.pszExt ) + ": at least a root node";
	ok = Check( nCount >= 1, szWhat.c_str() ) && ok;
	std::vector<BkResNodeRecord> nodes( nCount );
	szWhat = std::string( fx.pszExt ) + ": BkResNodes fill";
	ok = Check( BkResNodes( pSession, nodes.data(), nCount, &nCount ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;

	szWhat = std::string( fx.pszExt ) + ": BkResSave";
	if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		BkResClose( pSession );
		return false;
	}

	std::string szBefore, szAfter;
	if ( !Check( ReadBytes( szIn, szBefore ), "fixture readable" ) ) { BkResClose( pSession ); return false; }
	if ( !Check( ReadBytes( szOut, szAfter ), "saved file readable" ) ) { BkResClose( pSession ); return false; }
	szWhat = std::string( fx.pszExt ) + ": byte-identical round-trip";
	if ( !Check( szBefore == szAfter, szWhat.c_str() ) )
	{
		std::printf( "   in=%zu bytes, out=%zu bytes\n", szBefore.size(), szAfter.size() );
		ok = false;
	}

	// A re-open of the saved copy must round-trip too - "the game reads it unchanged"
	// invariant extended to the editor's own reader.
	szWhat = std::string( fx.pszExt ) + ": re-open saved copy";
	ok = Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	BkResClose( pSession );
	return ok;
}

static bool DeleteRestoreOne( BkResSession *pSession, const std::string &szFixtureRoot,
                              const std::string &szScratchRoot, const Fixture &fx )
{
	const std::string szIn = szFixtureRoot + "/" + fx.pszExt + "/project." + fx.pszExt;
	const std::string szOutDir = szScratchRoot + "/" + fx.pszExt;
	const std::string szOut = szOutDir + "/project.deleterestore." + fx.pszExt;
	std::error_code ec;
	std::filesystem::create_directories( szOutDir, ec );
	std::filesystem::remove( szOut, ec );
	std::filesystem::remove( szOut + ".bak", ec );
	std::filesystem::remove( szOut + ".tmp", ec );

	std::string szWhat;
	bool ok = true;

	szWhat = std::string( fx.pszExt ) + " [dr]: BkResOpen";
	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return false;
	}

	int nCount = 0;
	BkResNodes( pSession, 0, 0, &nCount );
	if ( !Check( nCount >= 2, "has at least one non-root node to delete" ) ) { BkResClose( pSession ); return false; }
	std::vector<BkResNodeRecord> nodes( nCount );
	BkResNodes( pSession, nodes.data(), nCount, &nCount );
	// Find the first direct child of root. The root has id 1.
	int nVictim = 0, nParent = 0, nIndex = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		if ( nodes[i].parent == 1 )
		{
			nVictim = nodes[i].id;
			nParent = nodes[i].parent;
			break;
		}
	}
	if ( !Check( nVictim != 0, "found a victim node under the root" ) ) { BkResClose( pSession ); return false; }

	// Two-pass size then write.
	int nBlobSize = 0;
	szWhat = std::string( fx.pszExt ) + " [dr]: BkResDeleteNode (size)";
	ok = Check( BkResDeleteNode( pSession, nVictim, 0, 0, &nBlobSize ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	ok = Check( nBlobSize > 0, "blob size is positive" ) && ok;
	// At this point the node is already removed (second phase of DeleteNode).
	// Re-open to get a clean copy and then exercise the "size + fill in one call"
	// shape callers actually use.
	BkResClose( pSession );
	BkResOpen( pSession, szIn.c_str() );
	BkResNodes( pSession, nodes.data(), nCount, &nCount );
	for ( int i = 0; i < nCount; ++i )
		if ( nodes[i].parent == 1 ) { nVictim = nodes[i].id; nParent = nodes[i].parent; nIndex = 0; break; }
	std::vector<unsigned char> blob( nBlobSize );
	int nWrittenSize = 0;
	szWhat = std::string( fx.pszExt ) + " [dr]: BkResDeleteNode (fill)";
	ok = Check( BkResDeleteNode( pSession, nVictim, blob.data(), (int)blob.size(), &nWrittenSize ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	ok = Check( nWrittenSize == nBlobSize, "written size matches sized pass" ) && ok;

	int nRestoredId = 0;
	szWhat = std::string( fx.pszExt ) + " [dr]: BkResRestoreNode";
	ok = Check( BkResRestoreNode( pSession, blob.data(), nWrittenSize, nParent, nIndex, &nRestoredId ) == BK_EDITOR_OK, szWhat.c_str() ) && ok;
	ok = Check( nRestoredId != 0, "restored id is non-zero" ) && ok;

	szWhat = std::string( fx.pszExt ) + " [dr]: BkResSave";
	if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, szWhat.c_str() ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		BkResClose( pSession );
		return false;
	}
	std::string szBefore, szAfter;
	ReadBytes( szIn, szBefore );
	ReadBytes( szOut, szAfter );
	szWhat = std::string( fx.pszExt ) + " [dr]: byte-identical after delete+restore+save";
	if ( !Check( szBefore == szAfter, szWhat.c_str() ) )
	{
		std::printf( "   in=%zu bytes, out=%zu bytes\n", szBefore.size(), szAfter.size() );
		ok = false;
	}
	BkResClose( pSession );
	return ok;
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

	const std::string szSelfDir = DirectoryOf( argv[0] != 0 ? argv[0] : "." );
	const char *pszRoot = argc > 1 ? argv[1] : szSelfDir.c_str();
	const std::string szFixtureRoot = argc > 2 ? argv[2] : ( szSelfDir + "/fixtures/resource_editor" );
	const std::string szScratchRoot = argc > 3 ? argv[3] : ( szSelfDir + "/local-test/resource_editor/t02" );

	FILE *pProbe = std::fopen( ( std::string( pszRoot ) + "/Data/consts.xml" ).c_str(), "rb" );
	if ( pProbe == 0 )
	{
		const int nSkipped = SkipOrFail( std::string( "no staged game at " ) + pszRoot + " (run: zig build install-game)" );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	std::fclose( pProbe );
	// Also need the fixtures. Without them the Project+Tree sub-step cannot run;
	// still a skip (not a fail) because this tier runs on hosts that only have
	// the staged install laid down.
	FILE *pFixProbe = std::fopen( ( szFixtureRoot + "/EXTENSIONS.md" ).c_str(), "rb" );
	if ( pFixProbe == 0 )
	{
		const int nSkipped = SkipOrFail( std::string( "no resource fixtures at " ) + szFixtureRoot );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	std::fclose( pFixProbe );

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

	// Smoke: BkResNew works for a fresh wpn project, kind round-trips, close.
	Check( BkResNew( pSession, 0 ) == BK_EDITOR_OK, "BkResNew(wpn) answers OK" );
	BkResKind newKind = -1;
	Check( BkResKindOf( pSession, &newKind ) == BK_EDITOR_OK, "BkResKindOf after New answers OK" );
	Check( newKind == 0, "KindOf after BkResNew(wpn) is 0" );
	int nFreshCount = -1;
	Check( BkResNodes( pSession, 0, 0, &nFreshCount ) == BK_EDITOR_OK, "BkResNodes answers OK with a null buffer" );
	Check( nFreshCount == 1, "a fresh project exposes just the root node" );
	Check( BkResClose( pSession ) == BK_EDITOR_OK, "BkResClose answers OK" );

	// Round-trip every fixture.
	for ( int i = 0; i < kFixtureCount; ++i )
		RoundTripOne( pSession, szFixtureRoot, szScratchRoot, kFixtures[i] );

	// Delete->restore->save on the three representative kinds.
	const char *pszRep[] = { "wpn", "msh", "pcp" };
	for ( int r = 0; r < 3; ++r )
	{
		Fixture fx = {};
		for ( int i = 0; i < kFixtureCount; ++i )
			if ( std::strcmp( kFixtures[i].pszExt, pszRep[r] ) == 0 ) { fx = kFixtures[i]; break; }
		DeleteRestoreOne( pSession, szFixtureRoot, szScratchRoot, fx );
	}

	// Lock/unlock on a saved copy.
	{
		Fixture fx = kFixtures[0]; // wpn
		const std::string szOut = szScratchRoot + "/" + fx.pszExt + "/project." + fx.pszExt;
		BkResOpen( pSession, szOut.c_str() );
		Check( BkResLock( pSession ) == BK_EDITOR_OK, "BkResLock on a saved project" );
		char owner[256] = {};
		Check( BkResLockOwner( pSession, owner, (int)sizeof( owner ) ) == BK_EDITOR_OK, "BkResLockOwner reads" );
		Check( owner[0] != 0, "owner string is non-empty" );
		Check( BkResClose( pSession ) == BK_EDITOR_OK, "BkResClose releases lock" );
	}

	// T05: cells family round-trip against bld. Writes passability, locked
	// tiles, and transparency lines on the root node of the bld fixture;
	// reads back; saves; reopens the saved copy; reads again; asserts the
	// values persisted byte-identically through NResourceModel::Load/Save.
	{
		const std::string szIn = szFixtureRoot + "/bld/project.bld";
		const std::string szOutDir = szScratchRoot + "/bld";
		const std::string szOut = szOutDir + "/project.cells.bld";
		std::error_code ec;
		std::filesystem::create_directories( szOutDir, ec );
		std::filesystem::remove( szOut, ec );
		std::filesystem::remove( szOut + ".bak", ec );
		std::filesystem::remove( szOut + ".tmp", ec );

		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "cells: BkResOpen bld" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		else
		{
			const int nRoot = 1; // RebuildIds hands the root id 1.
			// A 3x2 passability grid with a recognisable byte pattern.
			const int nW = 3, nH = 2;
			unsigned char cells[6] = { 1, 2, 3, 4, 5, 6 };
			Check( BkResSetPassabilityCells( pSession, nRoot, cells, nW, nH ) == BK_EDITOR_OK,
			       "cells: BkResSetPassabilityCells" );

			int rw = 0, rh = 0;
			unsigned char read_cells[16] = {};
			Check( BkResGetPassabilityCells( pSession, nRoot, 0, 0, &rw, &rh ) == BK_EDITOR_OK,
			       "cells: BkResGetPassabilityCells size" );
			Check( rw == nW && rh == nH, "cells: passability w/h round-trip" );
			Check( BkResGetPassabilityCells( pSession, nRoot, read_cells, (int)sizeof( read_cells ), &rw, &rh ) == BK_EDITOR_OK,
			       "cells: BkResGetPassabilityCells read" );
			Check( std::memcmp( read_cells, cells, 6 ) == 0, "cells: passability bytes round-trip" );

			// A 2x2 locked-tiles grid.
			unsigned char locked[4] = { 0, 1, 1, 0 };
			Check( BkResSetLockedTiles( pSession, nRoot, locked, 2, 2 ) == BK_EDITOR_OK,
			       "cells: BkResSetLockedTiles" );
			unsigned char read_locked[4] = {};
			Check( BkResGetLockedTiles( pSession, nRoot, read_locked, 4, &rw, &rh ) == BK_EDITOR_OK,
			       "cells: BkResGetLockedTiles" );
			Check( rw == 2 && rh == 2, "cells: locked w/h round-trip" );
			Check( std::memcmp( read_locked, locked, 4 ) == 0, "cells: locked bytes round-trip" );

			// A short transparency-lines list.
			BkResPoint2 lines[3] = { { 0.5f, 1.5f }, { 2.0f, 3.0f }, { 4.25f, 5.75f } };
			Check( BkResSetTransparencyLines( pSession, nRoot, lines, 3 ) == BK_EDITOR_OK,
			       "cells: BkResSetTransparencyLines" );
			int nLineCount = -1;
			Check( BkResGetTransparencyLines( pSession, nRoot, 0, 0, &nLineCount ) == BK_EDITOR_OK,
			       "cells: BkResGetTransparencyLines size" );
			Check( nLineCount == 3, "cells: transparency line count" );
			BkResPoint2 read_lines[3] = {};
			Check( BkResGetTransparencyLines( pSession, nRoot, read_lines, 3, &nLineCount ) == BK_EDITOR_OK,
			       "cells: BkResGetTransparencyLines fill" );
			bool bLinesOk = true;
			for ( int i = 0; i < 3; ++i )
				if ( read_lines[i].x != lines[i].x || read_lines[i].y != lines[i].y ) bLinesOk = false;
			Check( bLinesOk, "cells: transparency bytes round-trip" );

			// Save, reopen, re-read. The values must persist through XML.
			if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, "cells: BkResSave" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
			Check( BkResClose( pSession ) == BK_EDITOR_OK, "cells: BkResClose after save" );
			if ( !Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, "cells: re-open saved copy" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );

			std::memset( read_cells, 0, sizeof( read_cells ) );
			rw = 0; rh = 0;
			Check( BkResGetPassabilityCells( pSession, nRoot, read_cells, (int)sizeof( read_cells ), &rw, &rh ) == BK_EDITOR_OK,
			       "cells: re-read passability" );
			Check( rw == nW && rh == nH && std::memcmp( read_cells, cells, 6 ) == 0,
			       "cells: passability survives save+reopen" );

			std::memset( read_locked, 0, sizeof( read_locked ) );
			rw = 0; rh = 0;
			Check( BkResGetLockedTiles( pSession, nRoot, read_locked, 4, &rw, &rh ) == BK_EDITOR_OK,
			       "cells: re-read locked" );
			Check( rw == 2 && rh == 2 && std::memcmp( read_locked, locked, 4 ) == 0,
			       "cells: locked tiles survive save+reopen" );

			std::memset( read_lines, 0, sizeof( read_lines ) );
			nLineCount = 0;
			Check( BkResGetTransparencyLines( pSession, nRoot, read_lines, 3, &nLineCount ) == BK_EDITOR_OK,
			       "cells: re-read transparency" );
			bLinesOk = ( nLineCount == 3 );
			for ( int i = 0; i < 3 && bLinesOk; ++i )
				if ( read_lines[i].x != lines[i].x || read_lines[i].y != lines[i].y ) bLinesOk = false;
			Check( bLinesOk, "cells: transparency lines survive save+reopen" );

			BkResClose( pSession );
		}
	}

	Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "the bridge stops" );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();

	if ( g_nFailures == 0 )
		std::printf( "resource-bridge: Project+Tree OK (%d fixtures)\n", kFixtureCount );
	else
		std::printf( "resource-bridge: %d failures\n", g_nFailures );
	return g_nFailures == 0 ? 0 : 1;
}
