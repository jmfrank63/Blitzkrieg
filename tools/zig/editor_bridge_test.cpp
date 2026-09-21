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
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
#include "../../Sources/src/MapFile/MapOverlay.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"

static std::string DirectoryOf( const char *pszPath )
{
	const std::string szPath( pszPath );
	const std::string::size_type nCut = szPath.find_last_of( "/\\" );
	return nCut == std::string::npos ? std::string( "." ) : szPath.substr( 0, nCut );
}

// Both sides go through the platform's canonical form first: argv[0] arrives
// relative on one runner and absolute on the next, and a textual compare would
// call those two different directories.
static bool SamePath( const char *pszLeft, const char *pszRight )
{
#if defined(_WIN32) || defined(_WIN64)
	char left[_MAX_PATH], right[_MAX_PATH];
	if ( _fullpath( left, pszLeft, _MAX_PATH ) == 0 || _fullpath( right, pszRight, _MAX_PATH ) == 0 )
		return false;
	return _stricmp( left, right ) == 0;
#else
	char left[PATH_MAX], right[PATH_MAX];
	if ( realpath( pszLeft, left ) == 0 || realpath( pszRight, right ) == 0 )
		return false;
	return strcmp( left, right ) == 0;
#endif
}

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static void MakeDirectory( const char *pszPath )
{
#if defined(_WIN32) || defined(_WIN64)
	_mkdir( pszPath );
#else
	mkdir( pszPath, 0755 );
#endif
}

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

// The same map the map-file tier uses, as a file dialog would hand the path
// over: an OS path written with the engine's separator, because OpenFileStream
// splits on backslash only.
// arnheim has eleven bridge spans; dessau names "Logs08", an object the
// database describes but whose RPG stats file is not in shipped Data.
static const char *const BRIDGE_MAP = "Data\\Maps\\Multiplayer\\arnheim.bzm";
static const char *const MISSING_STATS_MAP = "Data\\Maps\\dessau.bzm";
static const char *const SHIPPED_MAP = "Data\\Maps\\Multiplayer\\coldwinter.bzm";

static void Describe( const char *pszMap, const BkEditorMapSummary &rSummary )
{
	printf( "editor-bridge: %s is %dx%d tiles, season %d, %d players, %d objects "
	        "(%d placed, %d unknown), %d bridge spans (%d placed)\n",
	        pszMap, rSummary.width_tiles, rSummary.height_tiles, rSummary.season,
	        rSummary.player_count, rSummary.object_count, rSummary.placed_object_count,
	        rSummary.unknown_object_count, rSummary.bridge_span_count, rSummary.bridge_span_placed );
}

static void TestShippedMapOpens( BkEditorSession *pSession )
{
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, &summary ) == BK_EDITOR_OK, "a shipped map opens" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	Describe( SHIPPED_MAP, summary );
	Check( summary.width_tiles > 0 && summary.height_tiles > 0, "and reports its size" );
	Check( summary.object_count > 0, "and its objects" );
	Check( summary.unknown_object_count == 0, "and knows every type in it" );
	Check( summary.placed_object_count > 0, "and the engine holds them" );
}

// The spec's first engine-tier check: open a shipped map, save it with no
// edits, and get back what was read. The bridge writes the snapshot rather
// than the engine's copy - the engine's has UnpackFrameIndices applied, which
// picks a random visual variant per type - so a difference here means building
// the engine state wrote through to the snapshot, which it must not.
static void TestUneditedSaveIsEquivalent( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szSaved = szScratch + "\\bridge-roundtrip.bzm";
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, "the map to save opens" ) )
		return;
	if ( !Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "a map saves" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	CMapInfo original, saved;
	std::string szError, szWhere;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	Check( NMapFile::AreEquivalent( original, saved, &szWhere ),
	       szWhere.empty() ? "and an unedited save is equivalent" : ( "save differs at " + szWhere ).c_str() );
}

// Every edit has to change the engine and the map together, and the saved file
// has to equal the map the overlay builds on its own with no engine in sight.
// That equality is the point of the tier: it is what proves the two paths agree
// rather than merely both running.
//
// A poplar, on purpose: PackFrameIndex only does anything for a fence, an
// entrenchment or a bridge span, so an ordinary object is the case where the
// bridge's extra packing step must be invisible.
static void TestObjectEdits( BkEditorSession *pSession, const std::string &szScratch )
{
	const char *const pszName = "W_BigPoplar";
	const std::string szSaved = szScratch + "\\bridge-edited.bzm";
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, "the map to edit opens" ) )
		return;

	int nLinkID = -1;
	if ( !Check( BkEditorAddObject( pSession, pszName, 100.0f, 100.0f, 0, 0, &nLinkID ) == BK_EDITOR_OK,
	             "an object is added through the engine" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	Check( nLinkID > 0, "and comes back with a link ID" );
	Check( BkEditorMoveObject( pSession, nLinkID, 132.0f, 100.0f ) == BK_EDITOR_OK, "and moves" );
	Check( BkEditorTurnObject( pSession, nLinkID, 1024 ) == BK_EDITOR_OK, "and turns" );
	if ( !Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "and saves" ) )
		return;

	// The expected value, built by the overlay alone - no engine involved.
	CMapInfo expected, saved;
	std::string szError, szWhere;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
		return;
	NMapOverlay::SAddObject add;
	add.szName = pszName;
	add.vPos = CVec3( 132.0f, 100.0f, 0.0f );
	add.nDir = 1024;
	add.nPlayer = 0;
	int nExpectedLinkID = -1;
	NMapOverlay::AddObject( &expected, add, &nExpectedLinkID );
	Check( nExpectedLinkID == nLinkID, "the two paths agree on the link ID" );
	if ( !Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
	       szWhere.empty() ? "and the saved map is the expected one" : ( "edited save differs at " + szWhere ).c_str() );
	remove( szSaved.c_str() );
}

// nType and nAttackingSide are the map's own, so the only thing to prove is
// that they reach the file and that nothing travels with them.
static void TestMapsOwnFields( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szSaved = szScratch + "\\bridge-fields.bzm";
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, "the map opens" ) )
		return;
	Check( BkEditorSetMapType( pSession, 1 ) == BK_EDITOR_OK, "the map type is set" );
	Check( BkEditorSetAttackingSide( pSession, 1 ) == BK_EDITOR_OK, "and the attacking side" );
	if ( !Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "and it saves" ) )
		return;
	CMapInfo fields, expected;
	std::string szError, szWhere;
	if ( !Check( NMapFile::Read( szSaved.c_str(), &fields, &szError ), szError.c_str() ) )
		return;
	Check( fields.nType == 1 && fields.nAttackingSide == 1, "both reached the file" );
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
		return;
	expected.nType = 1;
	expected.nAttackingSide = 1;
	Check( NMapFile::AreEquivalent( expected, fields, &szWhere ),
	       szWhere.empty() ? "and nothing else moved" : ( "fields save differs at " + szWhere ).c_str() );
	remove( szSaved.c_str() );
}

// A bridge names its spans by link ID, so deleting one has to be refused with
// a reason, and the map has to be exactly as it was afterwards. A refusal that
// left half an edit behind would save a map the editor never showed.
static void TestDeleteIsRefusedWhileReferred( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szSaved = szScratch + "\\bridge-refused.bzm";
	CMapInfo map;
	std::string szError, szWhere;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( !map.bridges.empty() && !map.bridges[0].empty(), "the map has a bridge to refer to a span" ) )
		return;
	const int nSpanLinkID = map.bridges[0][0];

	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, 0 ) == BK_EDITOR_OK, "the map with bridges opens" ) )
		return;
	Check( BkEditorDeleteObject( pSession, nSpanLinkID ) == BK_EDITOR_REFUSED, "deleting a bridge's span is refused" );
	Check( *BkEditorLastMessage( pSession ) != 0, "and says why" );
	printf( "editor-bridge: refused: %s\n", BkEditorLastMessage( pSession ) );
	if ( !Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "and the map still saves" ) )
		return;
	CMapInfo saved;
	if ( !Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	Check( NMapFile::AreEquivalent( map, saved, &szWhere ),
	       szWhere.empty() ? "unchanged by the refusal" : ( "refused delete left a change at " + szWhere ).c_str() );
	remove( szSaved.c_str() );
}

// A bridge's spans are not placed where they are found: they are set aside and
// built afterwards, through the bridge that lists them, so one bridge's spans
// end up in the file's order. Getting that wrong is silent - the spans are
// simply missing from the engine and from the link map, and a summary that
// counted only what the file said would still look right - so the count the
// engine ended up holding is what is checked.
static void TestBridgeSpansAreBuilt( BkEditorSession *pSession )
{
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, &summary ) == BK_EDITOR_OK, "a map with bridges opens" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	Describe( BRIDGE_MAP, summary );
	if ( !Check( summary.bridge_span_count > 0, "the map really has bridges" ) )
		return;
	Check( summary.bridge_span_placed == summary.bridge_span_count, "and every span reached the engine" );
}

// A described object whose stats file is missing has no footprint, so
// CAIEditor::IsObjectInsideOfMap has nothing to test it against and used to
// dereference the null it got back. Shipped Data contains one, so this is a
// map the editor has to survive rather than a case that had to be constructed.
static void TestMissingStatsDoNotStopTheOpen( BkEditorSession *pSession )
{
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( !Check( BkEditorOpenMap( pSession, MISSING_STATS_MAP, &summary ) == BK_EDITOR_OK,
	             "a map naming an object whose stats are missing opens" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	Describe( MISSING_STATS_MAP, summary );
	Check( summary.placed_object_count > 0, "and the rest of its objects are placed" );
}

// The case that crashes the MFC editor: it writes GetDesc( name )->eGameType
// with no null check, and GetDesc returns 0 for a name the database does not
// know. A map that names an object a mod no longer ships has to open, report
// the object, and leave it alone.
//
// The copy goes in the scratch directory, never in the installation: shipped
// Data is read-only for every tier, and a run that is killed between writing
// and removing would otherwise leave a map behind in it. Nothing is needed
// beside the map - CTerrain::LoadLocal keeps the path only as a name and takes
// the tileset, crosset and roadset from storage (TerrainInternal.cpp:88-110).
static void TestUnknownObjectDoesNotStopTheOpen( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szCopy = szScratch + "\\coldwinter-unknown-object.bzm";
	const char *const pszCopy = szCopy.c_str();
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( !map.objects.empty(), "the map has an object to rename" ) )
		return;
	map.objects[0].szName = "No_Such_Object_In_Any_Database";
	if ( !Check( NMapFile::Write( pszCopy, map, &szError ), szError.c_str() ) )
		return;

	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	const BkEditorStatus status = BkEditorOpenMap( pSession, pszCopy, &summary );
	if ( Check( status == BK_EDITOR_OK, "a map naming an object the database does not know still opens" ) )
		Check( summary.unknown_object_count == 1, "and reports exactly the one unknown object" );
	else
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
	remove( szCopy.c_str() );
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
	// Data beside each other, which is what install-game stages, and this
	// executable is staged into it. So the installation to edit is this
	// executable's own directory unless a caller names another one.
	const std::string szSelfDir = DirectoryOf( argv[0] != 0 ? argv[0] : "." );
	const char *pszRoot = argc > 1 ? argv[1] : szSelfDir.c_str();
	// Everything this test writes goes here. Defaults beside the executable so
	// a hand run needs no argument; the build passes zig-out/local-test.
	const std::string szScratch = argc > 2 ? argv[2] : szSelfDir;
	MakeDirectory( szScratch.c_str() );
	FILE *pProbe = fopen( ( std::string( pszRoot ) + "/Data/consts.xml" ).c_str(), "rb" );
	if ( pProbe == 0 )
	{
		printf( "editor-bridge: skipped: no staged game at %s (run: zig build install-game)\n", pszRoot );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
	}
	fclose( pProbe );

	// And the installation has to be the one this executable lives in.
	//
	// Every engine module derives its own roots from the running executable's
	// location - each dylib links its own copy of NPlatform::Paths, and
	// BaseRoot() comes from executableRoot() - so telling the bridge where the
	// game is only moves the bridge's copy. Run this from the build cache and
	// libAILogic computes a base of .zig-cache/..., finds no StreamIO there,
	// and its static initializers dereference a null singleton: an
	// EXC_BAD_ACCESS inside GetSingleton<IGlobalVars> with no message.
	//
	// The build stages this executable beside Game, so a mismatch is a build
	// or invocation mistake and fails loudly. Skipping here would be the
	// always-green outcome this tier exists to avoid.
	if ( !Check( SamePath( szSelfDir.c_str(), pszRoot ), "the executable lives in the installation it edits" ) )
	{
		printf( "editor-bridge: this executable is at %s but was told to edit %s;\n"
		        "               every engine module derives its roots from the executable's\n"
		        "               location, so it has to run from inside the installation\n",
		        szSelfDir.c_str(), pszRoot );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 1;
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
	{
		TestShippedMapOpens( pSession );
		TestBridgeSpansAreBuilt( pSession );
		TestUneditedSaveIsEquivalent( pSession, szScratch );
		TestObjectEdits( pSession, szScratch );
		TestMapsOwnFields( pSession, szScratch );
		TestDeleteIsRefusedWhileReferred( pSession, szScratch );
		TestMissingStatsDoNotStopTheOpen( pSession );
		TestUnknownObjectDoesNotStopTheOpen( pSession, szScratch );
		Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "and stops" );
	}

	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	if ( g_nFailures == 0 )
		printf( "editor-bridge: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
