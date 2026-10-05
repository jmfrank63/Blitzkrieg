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
#include "bridge_session.h"

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

// Plays a user for the lock tests (BK_RESOURCE_EDITOR_USER); null clears it.
static void SetLockUser( const char *pszUser )
{
#if defined(_WIN32) || defined(_WIN64)
	_putenv_s( "BK_RESOURCE_EDITOR_USER", pszUser != 0 ? pszUser : "" );
#else
	if ( pszUser != 0 )
		setenv( "BK_RESOURCE_EDITOR_USER", pszUser, 1 );
	else
		unsetenv( "BK_RESOURCE_EDITOR_USER" );
#endif
}

static std::vector<BkResNodeRecord> AllNodes( BkResSession *pSession )
{
	int nCount = 0;
	BkResNodes( pSession, 0, 0, &nCount );
	std::vector<BkResNodeRecord> nodes( nCount > 0 ? nCount : 0 );
	if ( nCount > 0 )
		BkResNodes( pSession, nodes.data(), nCount, &nCount );
	return nodes;
}

static bool SameZero( BkResSession *pSession, int nNode, const BkResPoint2 &want )
{
	BkResPoint2 got = { -1.0f, -1.0f };
	return BkResGetZeroPoint( pSession, nNode, &got ) == BK_EDITOR_OK && got.x == want.x && got.y == want.y;
}

static bool SameCells( BkResSession *pSession, int nNode, const unsigned char *pWant, int nW, int nH )
{
	unsigned char got[64] = {};
	int w = 0, h = 0;
	return BkResGetPassabilityCells( pSession, nNode, got, (int)sizeof( got ), &w, &h ) == BK_EDITOR_OK
		&& w == nW && h == nH && std::memcmp( got, pWant, size_t( nW * nH ) ) == 0;
}

static bool SameShoot( BkResSession *pSession, int nNode, const BkResAimedPoint &want )
{
	BkResAimedPoint got[2] = {};
	int n = 0;
	return BkResGetShootPoints( pSession, nNode, got, 2, &n ) == BK_EDITOR_OK && n == 1
		&& got[0].at.x == want.at.x && got[0].at.y == want.at.y && got[0].angle == want.angle && got[0].cone == want.cone;
}

static void GeometryOnChildNodes( BkResSession *pSession, const std::string &szFixtureRoot, const std::string &szScratchRoot )
{
	const std::string szIn = szFixtureRoot + "/wpn/project.wpn";
	const std::string szDir = szScratchRoot + "/geometry";
	const std::string szSaved = szDir + "/project.wpn";
	const std::string szResaved = szDir + "/project.resaved.wpn";
	const std::string szBeforeDelete = szDir + "/project.before-delete.wpn";
	const std::string szAfterRestore = szDir + "/project.after-restore.wpn";
	std::error_code ec;
	std::filesystem::remove_all( szDir, ec );
	std::filesystem::create_directories( szDir, ec );

	if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "geometry: BkResOpen wpn" ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	// A child of the root, and the deepest node (last in pre-order) so the
	// test reaches below the first level whenever the tree has one.
	std::vector<BkResNodeRecord> nodes = AllNodes( pSession );
	int nChild = 0, nDeep = 0;
	for ( const BkResNodeRecord &n : nodes )
		if ( n.parent == 1 && nChild == 0 && n.child_count > 0 )
			nChild = n.id;
	if ( nChild == 0 )
		for ( const BkResNodeRecord &n : nodes )
			if ( n.parent == 1 ) { nChild = n.id; break; }
	if ( !nodes.empty() )
		nDeep = nodes.back().id;
	if ( !Check( nChild != 0 && nDeep != 0 && nDeep != 1, "geometry: wpn has nodes below the root" ) )
	{
		BkResClose( pSession );
		return;
	}
	const BkResPoint2 zeroChild = { 7.5f, -2.25f };
	const BkResPoint2 zeroDeep = { 1.0f, 2.0f };
	const BkResPoint2 zeroRoot = { 3.0f, 4.0f };
	const unsigned char cells[6] = { 9, 8, 7, 6, 5, 4 };
	const BkResAimedPoint shoot = { { 0.5f, 0.25f }, 90, 30 };
	Check( BkResSetZeroPoint( pSession, 1, &zeroRoot ) == BK_EDITOR_OK, "geometry: set the root's zero point" );
	Check( BkResSetZeroPoint( pSession, nChild, &zeroChild ) == BK_EDITOR_OK, "geometry: set a child's zero point" );
	Check( BkResSetPassabilityCells( pSession, nChild, cells, 3, 2 ) == BK_EDITOR_OK, "geometry: set a child's cells" );
	Check( BkResSetZeroPoint( pSession, nDeep, &zeroDeep ) == BK_EDITOR_OK, "geometry: set the deepest node's zero point" );
	Check( BkResSetShootPoints( pSession, nDeep, &shoot, 1 ) == BK_EDITOR_OK, "geometry: set the deepest node's shoot point" );
	if ( !Check( BkResSave( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "geometry: save" ) )
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
	BkResClose( pSession );

	if ( !Check( BkResOpen( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "geometry: reopen" ) )
	{
		std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	Check( AllNodes( pSession ).size() == nodes.size(), "geometry: reopen shows the same nodes, no geometry node" );
	Check( SameZero( pSession, 1, zeroRoot ), "geometry: the root's zero point survives save+reopen" );
	Check( SameZero( pSession, nChild, zeroChild ), "geometry: a child's zero point survives save+reopen" );
	Check( SameCells( pSession, nChild, cells, 3, 2 ), "geometry: a child's cells survive save+reopen" );
	Check( SameZero( pSession, nDeep, zeroDeep ), "geometry: the deepest node's zero point survives save+reopen" );
	Check( SameShoot( pSession, nDeep, shoot ), "geometry: the deepest node's shoot point survives save+reopen" );
	// Saving the reopened project again changes nothing.
	Check( BkResSave( pSession, szResaved.c_str() ) == BK_EDITOR_OK, "geometry: save the reopened project" );
	std::string szA, szB;
	ReadBytes( szSaved, szA );
	ReadBytes( szResaved, szB );
	Check( !szA.empty() && szA == szB, "geometry: open -> save of a project with geometry is byte-identical" );

	// Delete the child (its subtree holds the deepest node's geometry too),
	// then restore it where it was: the save matches the one before.
	Check( BkResSave( pSession, szBeforeDelete.c_str() ) == BK_EDITOR_OK, "geometry: save before delete" );
	nodes = AllNodes( pSession );
	int nIndex = 0;
	for ( const BkResNodeRecord &n : nodes )
	{
		if ( n.id == nChild )
			break;
		if ( n.parent == 1 )
			++nIndex;
	}
	int nSize = 0;
	BkResDeleteNode( pSession, nChild, 0, 0, &nSize );
	std::vector<unsigned char> blob( nSize > 0 ? nSize : 1 );
	Check( BkResDeleteNode( pSession, nChild, blob.data(), nSize, &nSize ) == BK_EDITOR_OK, "geometry: delete the child" );
	BkResPoint2 gone = { 0, 0 };
	Check( BkResGetZeroPoint( pSession, nChild, &gone ) != BK_EDITOR_OK || ( gone.x == 0 && gone.y == 0 ),
		"geometry: the deleted node's geometry is gone" );
	int nRestored = 0;
	Check( BkResRestoreNode( pSession, blob.data(), nSize, 1, nIndex, &nRestored ) == BK_EDITOR_OK && nRestored == nChild,
		"geometry: restore the child under its old id" );
	Check( SameZero( pSession, nChild, zeroChild ) && SameCells( pSession, nChild, cells, 3, 2 ),
		"geometry: restore brings the child's geometry back" );
	Check( SameZero( pSession, nDeep, zeroDeep ) && SameShoot( pSession, nDeep, shoot ),
		"geometry: restore brings the subtree's geometry back" );
	Check( BkResSave( pSession, szAfterRestore.c_str() ) == BK_EDITOR_OK, "geometry: save after restore" );
	ReadBytes( szBeforeDelete, szA );
	ReadBytes( szAfterRestore, szB );
	if ( !Check( !szA.empty() && szA == szB, "geometry: delete -> restore -> save is byte-identical to before the delete" ) )
		std::printf( "   before=%zu bytes, after=%zu bytes\n", szA.size(), szB.size() );
	BkResClose( pSession );
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

	// Delete->restore->save on three kinds whose fixture is an MFC item tree
	// (S03 T02); msh and pcp join when T03/T04 replace their stub fixtures.
	const char *pszRep[] = { "wpn", "trc", "unt" };
	for ( int r = 0; r < 3; ++r )
	{
		Fixture fx = {};
		for ( int i = 0; i < kFixtureCount; ++i )
			if ( std::strcmp( kFixtures[i].pszExt, pszRep[r] ) == 0 ) { fx = kFixtures[i]; break; }
		DeleteRestoreOne( pSession, szFixtureRoot, szScratchRoot, fx );
	}

	// MFC's lock (D-08): `locked_<user>` in the project's folder. Two
	// sessions play two users through the BK_RESOURCE_EDITOR_USER seam; the
	// second session is a bare BkEditorSession, which the data-only lock
	// entries accept as well as a started one.
	{
		const std::string szDir = szScratchRoot + "/lock";
		const std::string szProject = szDir + "/project.wpn";
		std::error_code ec;
		std::filesystem::remove_all( szDir, ec );
		std::filesystem::create_directories( szDir, ec );
		std::filesystem::copy_file( szFixtureRoot + "/wpn/project.wpn", szProject, ec );
		BkEditorSession *pOther = new BkEditorSession();

		SetLockUser( "alice" );
		Check( BkResOpen( pSession, szProject.c_str() ) == BK_EDITOR_OK, "lock: alice opens" );
		Check( BkResLock( pSession ) == BK_EDITOR_OK, "lock: alice locks" );
		Check( std::filesystem::exists( szDir + "/locked_alice", ec ), "lock: locked_alice is in the project's folder" );
		Check( !std::filesystem::exists( szProject + ".lock", ec ), "lock: no <path>.lock" );
		Check( BkResLock( pSession ) == BK_EDITOR_OK, "lock: alice's own lock is hers again, as in MFC" );
		char owner[256] = {};
		Check( BkResLockOwner( pSession, owner, (int)sizeof( owner ) ) == BK_EDITOR_OK && std::strcmp( owner, "alice" ) == 0,
			"lock: BkResLockOwner names alice" );

		SetLockUser( "bob" );
		Check( BkResOpen( pOther, szProject.c_str() ) == BK_EDITOR_OK, "lock: bob opens" );
		Check( BkResLock( pOther ) == BK_EDITOR_REFUSED, "lock: bob is refused while alice holds it" );
		Check( std::strstr( BkEditorLastMessage( pOther ), "alice" ) != 0, "lock: the refusal names alice" );
		Check( !std::filesystem::exists( szDir + "/locked_bob", ec ), "lock: a refused lock leaves no file" );
		owner[0] = 0;
		Check( BkResLockOwner( pOther, owner, (int)sizeof( owner ) ) == BK_EDITOR_OK && std::strcmp( owner, "alice" ) == 0,
			"lock: bob sees alice as the owner" );
		Check( BkResLockTakeOver( pOther ) == BK_EDITOR_OK, "lock: bob takes the lock over" );
		Check( !std::filesystem::exists( szDir + "/locked_alice", ec ), "lock: the take-over removes locked_alice" );
		Check( std::filesystem::exists( szDir + "/locked_bob", ec ), "lock: the take-over writes locked_bob" );
		Check( BkResClose( pOther ) == BK_EDITOR_OK, "lock: bob closes" );
		Check( !std::filesystem::exists( szDir + "/locked_bob", ec ), "lock: close removes locked_bob" );

		SetLockUser( "alice" );
		Check( BkResClose( pSession ) == BK_EDITOR_OK, "lock: alice closes" );
		bool bStray = false;
		for ( const auto &entry : std::filesystem::directory_iterator( szDir, ec ) )
		{
			const std::string szName = entry.path().filename().string();
			if ( szName.compare( 0, 7, "locked_" ) == 0 || szName.find( ".lock" ) != std::string::npos )
				bStray = true;
		}
		Check( !bStray, "lock: no lock file is left behind" );
		SetLockUser( 0 );
		delete pOther;
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

	// T06: points + aimed-points family round-trip against bld (zero point,
	// entrance, shoot points) and scp (shoot points). Set each channel on the
	// root node of the fixture, read back, save, reopen, read again. The
	// values must round-trip through the `_bk_geometry` persistence layer.
	{
		const std::string szIn = szFixtureRoot + "/bld/project.bld";
		const std::string szOutDir = szScratchRoot + "/bld";
		const std::string szOut = szOutDir + "/project.points.bld";
		std::error_code ec;
		std::filesystem::create_directories( szOutDir, ec );
		std::filesystem::remove( szOut, ec );
		std::filesystem::remove( szOut + ".bak", ec );
		std::filesystem::remove( szOut + ".tmp", ec );

		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "points: BkResOpen bld" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		else
		{
			const int nRoot = 1;

			// 1.2345678f needs nine significant digits: a six-digit %g would lose it.
			BkResPoint2 zero = { 1.2345678f, -2.5f };
			Check( BkResSetZeroPoint( pSession, nRoot, &zero ) == BK_EDITOR_OK, "points: BkResSetZeroPoint" );
			BkResPoint2 read_zero = { 0, 0 };
			Check( BkResGetZeroPoint( pSession, nRoot, &read_zero ) == BK_EDITOR_OK, "points: BkResGetZeroPoint" );
			Check( read_zero.x == zero.x && read_zero.y == zero.y, "points: zero point round-trip" );

			BkResPoint2 entrance = { 7.0f, 11.0f };
			Check( BkResSetEntrance( pSession, nRoot, &entrance ) == BK_EDITOR_OK, "points: BkResSetEntrance" );
			BkResPoint2 read_entrance = { 0, 0 };
			Check( BkResGetEntrance( pSession, nRoot, &read_entrance ) == BK_EDITOR_OK, "points: BkResGetEntrance" );
			Check( read_entrance.x == entrance.x && read_entrance.y == entrance.y, "points: entrance round-trip" );

			BkResAimedPoint shoots[2] = {
				{ { 0.5f, 1.5f }, 90, 15 },
				{ { 3.0f, 4.0f }, 180, 30 },
			};
			Check( BkResSetShootPoints( pSession, nRoot, shoots, 2 ) == BK_EDITOR_OK, "points: BkResSetShootPoints" );
			int nShootCount = -1;
			Check( BkResGetShootPoints( pSession, nRoot, 0, 0, &nShootCount ) == BK_EDITOR_OK, "points: BkResGetShootPoints size" );
			Check( nShootCount == 2, "points: shoot count" );
			BkResAimedPoint read_shoots[2] = {};
			Check( BkResGetShootPoints( pSession, nRoot, read_shoots, 2, &nShootCount ) == BK_EDITOR_OK, "points: BkResGetShootPoints fill" );
			bool bShootOk = true;
			for ( int i = 0; i < 2; ++i )
				if ( read_shoots[i].at.x != shoots[i].at.x || read_shoots[i].at.y != shoots[i].at.y
					|| read_shoots[i].angle != shoots[i].angle || read_shoots[i].cone != shoots[i].cone )
					bShootOk = false;
			Check( bShootOk, "points: shoot points round-trip" );

			if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, "points: BkResSave" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
			Check( BkResClose( pSession ) == BK_EDITOR_OK, "points: BkResClose after save" );
			if ( !Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, "points: re-open saved copy" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );

			read_zero = { 0, 0 };
			Check( BkResGetZeroPoint( pSession, nRoot, &read_zero ) == BK_EDITOR_OK, "points: re-read zero" );
			Check( read_zero.x == zero.x && read_zero.y == zero.y, "points: zero point survives save+reopen" );

			read_entrance = { 0, 0 };
			Check( BkResGetEntrance( pSession, nRoot, &read_entrance ) == BK_EDITOR_OK, "points: re-read entrance" );
			Check( read_entrance.x == entrance.x && read_entrance.y == entrance.y, "points: entrance survives save+reopen" );

			std::memset( read_shoots, 0, sizeof( read_shoots ) );
			nShootCount = -1;
			Check( BkResGetShootPoints( pSession, nRoot, read_shoots, 2, &nShootCount ) == BK_EDITOR_OK, "points: re-read shoot" );
			bShootOk = ( nShootCount == 2 );
			for ( int i = 0; i < 2 && bShootOk; ++i )
				if ( read_shoots[i].at.x != shoots[i].at.x || read_shoots[i].at.y != shoots[i].at.y
					|| read_shoots[i].angle != shoots[i].angle || read_shoots[i].cone != shoots[i].cone )
					bShootOk = false;
			Check( bShootOk, "points: shoot points survive save+reopen" );

			BkResClose( pSession );
		}
	}

	// T06: same aimed-points shape against scp (squad). The squad item class
	// doesn't carry zero/entrance in a general sense; shoot_points stands in
	// as a representative aimed channel so an scp fixture is covered too.
	{
		const std::string szIn = szFixtureRoot + "/scp/project.scp";
		const std::string szOutDir = szScratchRoot + "/scp";
		const std::string szOut = szOutDir + "/project.points.scp";
		std::error_code ec;
		std::filesystem::create_directories( szOutDir, ec );
		std::filesystem::remove( szOut, ec );
		std::filesystem::remove( szOut + ".bak", ec );
		std::filesystem::remove( szOut + ".tmp", ec );

		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "points-scp: BkResOpen scp" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		else
		{
			const int nRoot = 1;
			BkResAimedPoint fires[1] = { { { 2.0f, 6.0f }, 45, 10 } };
			Check( BkResSetFirePoints( pSession, nRoot, fires, 1 ) == BK_EDITOR_OK, "points-scp: BkResSetFirePoints" );
			int nFireCount = -1;
			BkResAimedPoint read_fires[1] = {};
			Check( BkResGetFirePoints( pSession, nRoot, read_fires, 1, &nFireCount ) == BK_EDITOR_OK, "points-scp: BkResGetFirePoints" );
			Check( nFireCount == 1 && read_fires[0].at.x == fires[0].at.x && read_fires[0].at.y == fires[0].at.y
				&& read_fires[0].angle == fires[0].angle && read_fires[0].cone == fires[0].cone,
				"points-scp: fire points round-trip" );

			if ( !Check( BkResSave( pSession, szOut.c_str() ) == BK_EDITOR_OK, "points-scp: BkResSave" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
			Check( BkResClose( pSession ) == BK_EDITOR_OK, "points-scp: BkResClose after save" );
			if ( !Check( BkResOpen( pSession, szOut.c_str() ) == BK_EDITOR_OK, "points-scp: re-open saved copy" ) )
				std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );

			std::memset( read_fires, 0, sizeof( read_fires ) );
			nFireCount = -1;
			Check( BkResGetFirePoints( pSession, nRoot, read_fires, 1, &nFireCount ) == BK_EDITOR_OK, "points-scp: re-read fire" );
			Check( nFireCount == 1 && read_fires[0].at.x == fires[0].at.x && read_fires[0].at.y == fires[0].at.y
				&& read_fires[0].angle == fires[0].angle && read_fires[0].cone == fires[0].cone,
				"points-scp: fire points survive save+reopen" );
			BkResClose( pSession );
		}
	}

	// Node ids are stable: a delete or restore elsewhere in the tree leaves
	// every other id - and the geometry keyed by it - where it was, and a
	// restored node gets its old id back, so the undo history stays valid.
	{
		const std::string szIn = szFixtureRoot + "/bld/project.bld";
		if ( !Check( BkResOpen( pSession, szIn.c_str() ) == BK_EDITOR_OK, "ids: BkResOpen bld" ) )
			std::printf( "   detail: %s\n", BkEditorLastMessage( pSession ) );
		else
		{
			const int nRoot = 1;
			BkResPoint2 zero = { 3.0f, 4.0f };
			Check( BkResSetZeroPoint( pSession, nRoot, &zero ) == BK_EDITOR_OK, "ids: BkResSetZeroPoint" );
			// The stub fixture's <fixture> element is frame data outside the
			// childs list, so the root has no child nodes; give it two to
			// delete around.
			const char szExtra[] = "<extra/>";
			int nExtra = 0;
			Check( BkResRestoreNode( pSession, reinterpret_cast<const unsigned char *>( szExtra ), int( sizeof( szExtra ) - 1 ), nRoot, 0, &nExtra ) == BK_EDITOR_OK,
				"ids: add a first child" );
			Check( BkResRestoreNode( pSession, reinterpret_cast<const unsigned char *>( szExtra ), int( sizeof( szExtra ) - 1 ), nRoot, 1, &nExtra ) == BK_EDITOR_OK,
				"ids: add a second child" );
			int nCount = 0;
			BkResNodes( pSession, 0, 0, &nCount );
			std::vector<BkResNodeRecord> nodes( nCount > 0 ? nCount : 1 );
			BkResNodes( pSession, nodes.data(), nCount, &nCount );
			std::vector<int> children;
			for ( int i = 0; i < nCount; ++i )
				if ( nodes[i].parent == nRoot )
					children.push_back( nodes[i].id );
			if ( Check( children.size() >= 2, "ids: the fixture root has two children" ) )
			{
				const int nFirst = children[0], nSecond = children[1];
				int nBlobSize = 0;
				BkResDeleteNode( pSession, nFirst, 0, 0, &nBlobSize );
				std::vector<unsigned char> blob( nBlobSize > 0 ? nBlobSize : 1 );
				Check( BkResDeleteNode( pSession, nFirst, blob.data(), nBlobSize, &nBlobSize ) == BK_EDITOR_OK, "ids: BkResDeleteNode" );
				int nAfter = 0;
				BkResNodes( pSession, 0, 0, &nAfter );
				Check( nAfter == nCount - 1, "ids: the delete removed exactly one node" );
				std::vector<BkResNodeRecord> after( nAfter > 0 ? nAfter : 1 );
				BkResNodes( pSession, after.data(), nAfter, &nAfter );
				bool bSecondKept = false, bFirstGone = true;
				for ( int i = 0; i < nAfter; ++i )
				{
					if ( after[i].id == nSecond && after[i].parent == nRoot ) bSecondKept = true;
					if ( after[i].id == nFirst ) bFirstGone = false;
				}
				Check( bSecondKept, "ids: the surviving sibling keeps its id" );
				Check( bFirstGone, "ids: the deleted id is gone" );
				BkResPoint2 read_zero = { 0, 0 };
				Check( BkResGetZeroPoint( pSession, nRoot, &read_zero ) == BK_EDITOR_OK && read_zero.x == zero.x && read_zero.y == zero.y,
					"ids: the root's geometry survives a child delete" );
				int nRestored = 0;
				Check( BkResRestoreNode( pSession, blob.data(), nBlobSize, nRoot, 0, &nRestored ) == BK_EDITOR_OK, "ids: BkResRestoreNode" );
				Check( nRestored == nFirst, "ids: a restored node gets its old id back" );
				BkResNodes( pSession, after.data(), 0, &nAfter );
				Check( nAfter == nCount, "ids: the restore brought the node count back" );
			}
			BkResClose( pSession );
		}
		int nClosedCount = -1;
		Check( BkResNodes( pSession, 0, 0, &nClosedCount ) == BK_EDITOR_REFUSED && nClosedCount == 0, "BkResNodes refuses when no project is open" );
	}

	// Geometry on a node below the root is saved in that node's own element,
	// comes back on reopen, and travels with the node through delete -> restore.
	GeometryOnChildNodes( pSession, szFixtureRoot, szScratchRoot );

	// An entry point this slice has not built yet fails loudly instead of
	// answering OK for work it did not do.
	{
		Check( BkResNew( pSession, 0 ) == BK_EDITOR_OK, "stubs: BkResNew" );
		BkResExportReport report = {};
		Check( BkResExport( pSession, 0, &report ) == BK_EDITOR_FAILED, "stubs: BkResExport is not a silent OK" );
		Check( std::strlen( BkEditorLastMessage( pSession ) ) > 0, "stubs: the failure says why" );
		BkResVec3 key = { 1.0f, 2.0f, 3.0f };
		Check( BkResSetParticleKeyframes( pSession, 1, &key, 1 ) == BK_EDITOR_FAILED, "stubs: an unbuilt geometry setter is not a silent OK" );
		BkResClose( pSession );
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
