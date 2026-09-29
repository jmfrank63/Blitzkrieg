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
#include <map>
#include "../../Sources/src/EditorBridge/bridge.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
#include "../../Sources/src/MapFile/MapOverlay.h"
#include "../../Sources/src/Formats/fmtTerrain.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"
#include "../../Sources/src/GFX/GFX.H"
#include "../../Sources/src/Scene/Scene.h"
#include "../../Sources/src/Image/Image.h"
#include "../../Sources/src/Platform/Paths.h"
#include "../../Sources/src/StreamIO/GeneratedData.h"
#include "../../Sources/src/StreamIO/ProfilePaths.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>

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
#include <crtdbg.h>
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

// A visual sample of a decoded picture (D-29): RGBA8, top row first - the
// same layout BkEditorObjectPicture writes - as an uncompressed 32-bit TGA,
// swapped to the BGRA row order the format wants (the bridge's own WriteFrame
// does the same swap for a captured frame). For a human to look at, not for
// the pass/fail checks above it.
static bool WriteRgbaTga( const char *pszPath, const unsigned char *pRgba, int nWidth, int nHeight )
{
	FILE *pFile = fopen( pszPath, "wb" );
	if ( pFile == 0 )
		return false;
	const unsigned char header[18] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                                   (unsigned char)( nWidth & 0xff ), (unsigned char)( nWidth >> 8 ),
	                                   (unsigned char)( nHeight & 0xff ), (unsigned char)( nHeight >> 8 ), 32, 0x28 };
	bool bWritten = fwrite( header, 1, sizeof header, pFile ) == sizeof header;
	std::vector<unsigned char> row( size_t( nWidth ) * 4 );
	for ( int y = 0; y < nHeight && bWritten; ++y )
	{
		for ( int x = 0; x < nWidth; ++x )
		{
			const unsigned char *pPixel = pRgba + ( size_t( y ) * nWidth + x ) * 4;
			row[x * 4 + 0] = pPixel[2];
			row[x * 4 + 1] = pPixel[1];
			row[x * 4 + 2] = pPixel[0];
			row[x * 4 + 3] = pPixel[3];
		}
		bWritten = fwrite( &row[0], 1, row.size(), pFile ) == row.size();
	}
	return fclose( pFile ) == 0 && bWritten;
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

// Run first, right after the start, before any map is open (Task 1 carried:
// the BAD_ARGUMENT/REFUSED-before-start paths had no tests at all). Three
// things, on the entry points that take arguments to exercise them with:
//  1. A null session answers BK_EDITOR_NO_SESSION for every entry point that
//     takes one - BkEditorStart (which creates the session) and BkEditorStop
//     (documented safe on null) excluded.
//  2. Every map-needing entry point answers BK_EDITOR_REFUSED, naming
//     "no map is open", on the real started-but-mapless session -
//     BkEditorOpenMap itself excluded, since a map is exactly what it is
//     about to open.
//  3. A representative set of null-output arguments answer BK_EDITOR_BAD_ARGUMENT.
static void TestEntryPointsBeforeAMap( BkEditorSession *pSession )
{
	struct Call
	{
		const char *name;
		std::function<BkEditorStatus()> fn;
	};

	int nInt = 0, nInt2 = 0;
	float fFloat = 0.0f, fFloat2 = 0.0f;
	unsigned char cChar = 0;
	char cBuf[8] = { 0 };
	unsigned char rgba[64] = { 0 };
	BkEditorMapSummary summary; memset( &summary, 0, sizeof summary );
	BkEditorObjectState objState; memset( &objState, 0, sizeof objState );
	BkEditorObjectRecord objRecords[1]; memset( objRecords, 0, sizeof objRecords );
	BkEditorCatalogueEntry catEntries[1]; memset( catEntries, 0, sizeof catEntries );
	BkEditorMod modEntries[1]; memset( modEntries, 0, sizeof modEntries );
	BkEditorSoundRecord soundRecord; memset( &soundRecord, 0, sizeof soundRecord );
	soundRecord.name[0] = 'x'; // SoundRecordWellFormed: non-empty, finite x/y/z (0 is finite)
	BkEditorPaintCell cell = { 0, 0, 0 };
	BkEditorView view; memset( &view, 0, sizeof view );
	BkEditorPathSet paths; memset( &paths, 0, sizeof paths );
	void *pDevice = 0; unsigned int nFormat = 0;

	const std::vector<Call> noSession = {
		{ "BkEditorOpenMap", [&] { return BkEditorOpenMap( 0, SHIPPED_MAP, &summary ); } },
		{ "BkEditorSaveMap", [&] { return BkEditorSaveMap( 0, "zig-out/local-test/should-not-exist.bzm" ); } },
		{ "BkEditorAddObject", [&] { return BkEditorAddObject( 0, "x", 0, 0, 0, 0, &nInt ); } },
		{ "BkEditorPlaceObject", [&] { return BkEditorPlaceObject( 0, 0, 0, 0, 0, 0 ); } },
		{ "BkEditorMoveObject", [&] { return BkEditorMoveObject( 0, 0, 0, 0 ); } },
		{ "BkEditorTurnObject", [&] { return BkEditorTurnObject( 0, 0, 0 ); } },
		{ "BkEditorSetObjectPlayer", [&] { return BkEditorSetObjectPlayer( 0, 0, 0 ); } },
		{ "BkEditorDeleteObject", [&] { return BkEditorDeleteObject( 0, 0 ); } },
		{ "BkEditorRestoreObject", [&] { return BkEditorRestoreObject( 0, 0 ); } },
		{ "BkEditorSetDiplomacy", [&] { return BkEditorSetDiplomacy( 0, 0, 0 ); } },
		{ "BkEditorEngineObjectState", [&] { return BkEditorEngineObjectState( 0, 0, &objState ); } },
		{ "BkEditorObjects", [&] { return BkEditorObjects( 0, objRecords, 1, &nInt ); } },
		{ "BkEditorDiplomacy", [&] { return BkEditorDiplomacy( 0, 0, &nInt ); } },
		{ "BkEditorPaint", [&] { return BkEditorPaint( 0, &cell, 1, &nInt ); } },
		{ "BkEditorUndoPaint", [&] { return BkEditorUndoPaint( 0, 0 ); } },
		{ "BkEditorRedoPaint", [&] { return BkEditorRedoPaint( 0, 0 ); } },
		{ "BkEditorEngineTile", [&] { return BkEditorEngineTile( 0, 0, 0, &cChar ); } },
		{ "BkEditorTilesetTiles", [&] { return BkEditorTilesetTiles( 0, &cChar, 1, &nInt ); } },
		{ "BkEditorWorldToTile", [&] { return BkEditorWorldToTile( 0, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorTerrainMatchesEngine", [&] { return BkEditorTerrainMatchesEngine( 0 ); } },
		{ "BkEditorWorldMatchesMap", [&] { return BkEditorWorldMatchesMap( 0 ); } },
		{ "BkEditorCatalogue", [&] { return BkEditorCatalogue( 0, catEntries, 1, &nInt ); } },
		{ "BkEditorObjectPicture", [&] { return BkEditorObjectPicture( 0, "x", rgba, sizeof rgba, 16, &nInt, &nInt2 ); } },
		{ "BkEditorMods", [&] { return BkEditorMods( 0, modEntries, 1, &nInt ); } },
		{ "BkEditorSetMod", [&] { return BkEditorSetMod( 0, 0 ); } },
		{ "BkEditorActiveMod", [&] { return BkEditorActiveMod( 0, &modEntries[0] ); } },
		{ "BkEditorPaths", [&] { return BkEditorPaths( 0, &paths ); } },
		{ "BkEditorTestMapPath", [&] { return BkEditorTestMapPath( 0, "p", 0, "f", cBuf, sizeof cBuf ); } },
		{ "BkEditorSetCamera", [&] { return BkEditorSetCamera( 0, 0, 0 ); } },
		{ "BkEditorFrame", [&] { return BkEditorFrame( 0 ); } },
		{ "BkEditorViewState", [&] { return BkEditorViewState( 0, &view ); } },
		{ "BkEditorZoomAt", [&] { return BkEditorZoomAt( 0, 0, 0, 0 ); } },
		{ "BkEditorSetZoom", [&] { return BkEditorSetZoom( 0, 0 ); } },
		{ "BkEditorSetYaw", [&] { return BkEditorSetYaw( 0, 0 ); } },
		{ "BkEditorSetOverlay", [&] { return BkEditorSetOverlay( 0, 0, 0 ); } },
		{ "BkEditorGpuDevice", [&] { return BkEditorGpuDevice( 0, &pDevice, &nFormat ); } },
		{ "BkEditorResize", [&] { return BkEditorResize( 0, 640, 480 ); } },
		{ "BkEditorScreenSize", [&] { return BkEditorScreenSize( 0, &nInt, &nInt2 ); } },
		{ "BkEditorCaptureFrame", [&] { return BkEditorCaptureFrame( 0, "zig-out/local-test/should-not-exist.tga" ); } },
		{ "BkEditorObjectAt", [&] { return BkEditorObjectAt( 0, 0, 0, &nInt ); } },
		{ "BkEditorScreenToWorld", [&] { return BkEditorScreenToWorld( 0, 0, 0, &fFloat, &fFloat2 ); } },
		{ "BkEditorWorldToScreen", [&] { return BkEditorWorldToScreen( 0, 0, 0, &fFloat, &fFloat2 ); } },
		{ "BkEditorWorldToMap", [&] { return BkEditorWorldToMap( 0, 0, 0, &fFloat, &fFloat2 ); } },
		{ "BkEditorSetMapType", [&] { return BkEditorSetMapType( 0, 0 ); } },
		{ "BkEditorSetAttackingSide", [&] { return BkEditorSetAttackingSide( 0, 0 ); } },
		{ "BkEditorSounds", [&] { return BkEditorSounds( 0, &soundRecord, 1, &nInt ); } },
		{ "BkEditorAddSound", [&] { return BkEditorAddSound( 0, 0, &soundRecord ); } },
		{ "BkEditorSetSound", [&] { return BkEditorSetSound( 0, 0, &soundRecord ); } },
		{ "BkEditorDeleteSound", [&] { return BkEditorDeleteSound( 0, 0 ); } },
	};
	int nNoSessionFailures = 0;
	for ( const Call &c : noSession )
		if ( !Check( c.fn() == BK_EDITOR_NO_SESSION, ( std::string( c.name ) + " with a null session is BK_EDITOR_NO_SESSION" ).c_str() ) )
			++nNoSessionFailures;
	printf( "editor-bridge: %d/%zu entry points answered NO_SESSION for a null session\n",
	        int( noSession.size() ) - nNoSessionFailures, noSession.size() );

	// Every map-needing entry point, on the real (started) session before any
	// map has been opened. Arguments are chosen to be otherwise well-formed,
	// so each call actually reaches its own "no map is open" check rather
	// than stopping earlier at an argument check.
	const std::vector<Call> noMap = {
		{ "BkEditorAddObject", [&] { return BkEditorAddObject( pSession, "x", 0, 0, 0, 0, &nInt ); } },
		{ "BkEditorPlaceObject", [&] { return BkEditorPlaceObject( pSession, 0, 0, 0, 0, 0 ); } },
		{ "BkEditorMoveObject", [&] { return BkEditorMoveObject( pSession, 0, 0, 0 ); } },
		{ "BkEditorTurnObject", [&] { return BkEditorTurnObject( pSession, 0, 0 ); } },
		{ "BkEditorSetObjectPlayer", [&] { return BkEditorSetObjectPlayer( pSession, 0, 0 ); } },
		{ "BkEditorDeleteObject", [&] { return BkEditorDeleteObject( pSession, 0 ); } },
		{ "BkEditorRestoreObject", [&] { return BkEditorRestoreObject( pSession, 0 ); } },
		{ "BkEditorSetDiplomacy", [&] { return BkEditorSetDiplomacy( pSession, 0, 0 ); } },
		{ "BkEditorObjects", [&] { return BkEditorObjects( pSession, objRecords, 1, &nInt ); } },
		{ "BkEditorDiplomacy", [&] { return BkEditorDiplomacy( pSession, 0, &nInt ); } },
		{ "BkEditorPaint", [&] { return BkEditorPaint( pSession, &cell, 1, &nInt ); } },
		{ "BkEditorUndoPaint", [&] { return BkEditorUndoPaint( pSession, 0 ); } },
		{ "BkEditorRedoPaint", [&] { return BkEditorRedoPaint( pSession, 0 ); } },
		{ "BkEditorEngineTile", [&] { return BkEditorEngineTile( pSession, 0, 0, &cChar ); } },
		{ "BkEditorTilesetTiles", [&] { return BkEditorTilesetTiles( pSession, &cChar, 1, &nInt ); } },
		{ "BkEditorWorldToTile", [&] { return BkEditorWorldToTile( pSession, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorTerrainMatchesEngine", [&] { return BkEditorTerrainMatchesEngine( pSession ); } },
		{ "BkEditorWorldMatchesMap", [&] { return BkEditorWorldMatchesMap( pSession ); } },
		{ "BkEditorZoomAt", [&] { return BkEditorZoomAt( pSession, 0, 0, 0 ); } },
		{ "BkEditorSetZoom", [&] { return BkEditorSetZoom( pSession, 0 ); } },
		{ "BkEditorWorldToScreen", [&] { return BkEditorWorldToScreen( pSession, 0, 0, &fFloat, &fFloat2 ); } },
		{ "BkEditorObjectAt", [&] { return BkEditorObjectAt( pSession, 0, 0, &nInt ); } },
		{ "BkEditorSetMapType", [&] { return BkEditorSetMapType( pSession, 0 ); } },
		{ "BkEditorSetAttackingSide", [&] { return BkEditorSetAttackingSide( pSession, 0 ); } },
		{ "BkEditorSounds", [&] { return BkEditorSounds( pSession, &soundRecord, 1, &nInt ); } },
		{ "BkEditorAddSound", [&] { return BkEditorAddSound( pSession, 0, &soundRecord ); } },
		{ "BkEditorSetSound", [&] { return BkEditorSetSound( pSession, 0, &soundRecord ); } },
		{ "BkEditorDeleteSound", [&] { return BkEditorDeleteSound( pSession, 0 ); } },
		{ "BkEditorSaveMap", [&] { return BkEditorSaveMap( pSession, "zig-out/local-test/should-not-exist.bzm" ); } },
	};
	int nNoMapFailures = 0;
	for ( const Call &c : noMap )
	{
		const BkEditorStatus status = c.fn();
		const bool bOk = Check( status == BK_EDITOR_REFUSED, ( std::string( c.name ) + " before a map is open is BK_EDITOR_REFUSED" ).c_str() ) &&
		                 Check( std::string( BkEditorLastMessage( pSession ) ) == "no map is open", ( std::string( c.name ) + " names \"no map is open\"" ).c_str() );
		if ( !bOk )
			++nNoMapFailures;
	}
	printf( "editor-bridge: %d/%zu map-needing entry points refused \"no map is open\"\n",
	        int( noMap.size() ) - nNoMapFailures, noMap.size() );

	// A representative set of null-output arguments, on the real session -
	// map open or not does not matter, since the argument check runs first
	// in every one of these.
	Check( BkEditorScreenSize( pSession, 0, &nInt ) == BK_EDITOR_BAD_ARGUMENT, "BkEditorScreenSize with a null out_width is BAD_ARGUMENT" );
	Check( BkEditorObjects( pSession, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "BkEditorObjects with a null out_count is BAD_ARGUMENT" );
	Check( BkEditorEngineTile( pSession, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "BkEditorEngineTile with a null out_tile is BAD_ARGUMENT" );
	Check( BkEditorWorldToTile( pSession, 0, 0, 0, &nInt ) == BK_EDITOR_BAD_ARGUMENT, "BkEditorWorldToTile with a null out_x is BAD_ARGUMENT" );
	Check( BkEditorDiplomacy( pSession, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "BkEditorDiplomacy with a null out_value is BAD_ARGUMENT" );
	Check( BkEditorScreenToWorld( pSession, 0, 0, 0, &fFloat ) == BK_EDITOR_BAD_ARGUMENT, "BkEditorScreenToWorld with a null wx is BAD_ARGUMENT" );
	Check( BkEditorWorldToMap( pSession, 0, 0, 0, &fFloat ) == BK_EDITOR_BAD_ARGUMENT, "BkEditorWorldToMap with a null mx is BAD_ARGUMENT" );
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

// A BkEditorTestMapPath answer, always backslash-separated (bridge.cpp), as
// a real host path for std::filesystem, whose separator is '/' on the
// runners this test targets.
static std::string HostPath( const std::string &szEnginePath )
{
	std::string szHost = szEnginePath;
#if !defined(_WIN32)
	for ( std::string::size_type i = 0; i < szHost.size(); ++i )
		if ( szHost[i] == '\\' ) szHost[i] = '/';
#endif
	return szHost;
}

// BkEditorPaths and BkEditorTestMapPath (plan 03-02, D-01/D-02/D-08/D-09):
// the two host roots, the generated-data path a test-launch copy goes to -
// base and named mods alike - argument validation, and the stale-sibling
// cleanup the direct map launch depends on (a same-stem .xml must never
// outrank the .bzm this call is about to write).
static void TestPathsAndTestMapPath( BkEditorSession *pSession )
{
	BkEditorPathSet paths;
	memset( &paths, 0, sizeof paths );
	if ( Check( BkEditorPaths( pSession, &paths ) == BK_EDITOR_OK, "BkEditorPaths reads the roots" ) )
	{
		Check( paths.base_root[0] != 0, "the base root is not empty" );
		Check( paths.user_root[0] != 0, "the user root is not empty" );
		const size_t nBaseLen = strlen( paths.base_root );
		const size_t nUserLen = strlen( paths.user_root );
		Check( nBaseLen > 0 && ( paths.base_root[nBaseLen - 1] == '/' || paths.base_root[nBaseLen - 1] == '\\' ),
		       "the base root ends in a separator" );
		Check( nUserLen > 0 && ( paths.user_root[nUserLen - 1] == '/' || paths.user_root[nUserLen - 1] == '\\' ),
		       "the user root ends in a separator" );
	}

	char buffer[1024];
	memset( buffer, 0x7f, sizeof buffer );
	if ( Check( BkEditorTestMapPath( pSession, "MapEditorTest", "", "mapeditor_test.bzm", buffer, sizeof buffer ) == BK_EDITOR_OK,
	            "BkEditorTestMapPath with no mod is OK" ) )
	{
		const std::string szPath( buffer );
		const char *pszTail = "cache\\generated\\MapEditorTest\\base\\maps\\mapeditor_test.bzm";
		const size_t nTailLen = strlen( pszTail );
		Check( szPath.size() >= nTailLen && szPath.compare( szPath.size() - nTailLen, nTailLen, pszTail ) == 0,
		       ( "the base-mod path ends in cache\\generated\\MapEditorTest\\base\\maps\\mapeditor_test.bzm: \"" + szPath + "\"" ).c_str() );
		std::error_code error;
		Check( std::filesystem::exists( std::filesystem::path( HostPath( szPath ) ).parent_path(), error ),
		       "and its directory exists" );

		// A stale sibling with the other extension, sitting where the next
		// BkEditorTestMapPath call for the same name will write: it must be
		// gone afterwards, or the game could load it instead of the fresh copy.
		const std::string szSiblingHost = HostPath( szPath.substr( 0, szPath.size() - 4 ) + ".xml" );
		{
			FILE *pStale = fopen( szSiblingHost.c_str(), "wb" );
			if ( pStale != 0 )
			{
				fputs( "stale", pStale );
				fclose( pStale );
			}
		}
		Check( std::filesystem::exists( szSiblingHost, error ), "the stale sibling was created for this test" );
		Check( BkEditorTestMapPath( pSession, "MapEditorTest", "", "mapeditor_test.bzm", buffer, sizeof buffer ) == BK_EDITOR_OK,
		       "BkEditorTestMapPath runs again over the stale sibling" );
		Check( !std::filesystem::exists( szSiblingHost, error ), "and the stale sibling is gone" );
	}

	if ( Check( BkEditorTestMapPath( pSession, "MapEditorTest", "Some Mod", "mapeditor_test.bzm", buffer, sizeof buffer ) == BK_EDITOR_OK,
	            "BkEditorTestMapPath with a mod is OK" ) )
	{
		const std::string szPath( buffer );
		Check( szPath.find( "\\some_mod\\maps\\" ) != std::string::npos,
		       ( "\"Some Mod\" gives the key some_mod: \"" + szPath + "\"" ).c_str() );
	}

	Check( BkEditorTestMapPath( pSession, "MapEditorTest", "", "../x.bzm", buffer, sizeof buffer ) == BK_EDITOR_BAD_ARGUMENT,
	       "\"../x.bzm\" is BK_EDITOR_BAD_ARGUMENT" );
	Check( BkEditorTestMapPath( pSession, "MapEditorTest", "", "a\\b.bzm", buffer, sizeof buffer ) == BK_EDITOR_BAD_ARGUMENT,
	       "\"a\\\\b.bzm\" is BK_EDITOR_BAD_ARGUMENT" );
	Check( BkEditorTestMapPath( pSession, "MapEditorTest", "", "x.txt", buffer, sizeof buffer ) == BK_EDITOR_BAD_ARGUMENT,
	       "\"x.txt\" is BK_EDITOR_BAD_ARGUMENT" );
	Check( BkEditorTestMapPath( pSession, "MapEditorTest", "", "", buffer, sizeof buffer ) == BK_EDITOR_BAD_ARGUMENT,
	       "an empty file name is BK_EDITOR_BAD_ARGUMENT" );
	Check( BkEditorTestMapPath( pSession, "", "", "mapeditor_test.bzm", buffer, sizeof buffer ) == BK_EDITOR_BAD_ARGUMENT,
	       "an empty profile is BK_EDITOR_BAD_ARGUMENT" );

	// A buffer one byte short of the full path: refused, with nothing written.
	std::string szFull;
	if ( BkEditorTestMapPath( pSession, "MapEditorTest", "", "mapeditor_test.bzm", buffer, sizeof buffer ) == BK_EDITOR_OK )
		szFull = buffer;
	if ( Check( !szFull.empty(), "the full path is known for the short-buffer check" ) )
	{
		std::vector<char> tight( szFull.size() ); // one byte short of size()+1
		tight[0] = 0x7f;
		Check( BkEditorTestMapPath( pSession, "MapEditorTest", "", "mapeditor_test.bzm", &tight[0], (int)tight.size() ) == BK_EDITOR_REFUSED,
		       "a buffer one byte short is BK_EDITOR_REFUSED" );
		Check( tight[0] == 0, "and nothing is written past out[0] == 0" );
	}
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
// A tank, on purpose. It is the one kind of object that can be moved, turned
// and owned, so it exercises all three read-backs; and PackFrameIndex does
// nothing for a unit, so the bridge's extra packing step has to be invisible.
static void TestObjectEdits( BkEditorSession *pSession, const std::string &szScratch )
{
	const char *const pszName = "JS_2";
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
	Check( BkEditorSetObjectPlayer( pSession, nLinkID, 1 ) == BK_EDITOR_OK, "and changes hands" );
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
	add.nPlayer = 1;
	int nExpectedLinkID = -1;
	NMapOverlay::AddObject( &expected, add, &nExpectedLinkID );
	Check( nExpectedLinkID == nLinkID, "the two paths agree on the link ID" );
	if ( !Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
	       szWhere.empty() ? "and the saved map is the expected one" : ( "edited save differs at " + szWhere ).c_str() );
	remove( szSaved.c_str() );
}

// D-19's read-back check, exercised straight through the bridge:
// BkEditorSaveMap only answers OK once the file it just wrote has been read
// back and found equivalent to what was meant (SaveSessionMap). The editor
// itself always passes a temporary path here and swaps it over the user's map
// on success - this test proves the bridge half of that contract on its own,
// for both shipped formats, and that a write that cannot even start (a
// missing directory) is refused with the path named in the message.
static void TestSaveVerifiesWhatItWrote( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo expected;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
		return;
	if ( !Check( !expected.objects.empty(), "the map has an object to move" ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, "the map to verify-save opens" ) )
		return;

	const int nLinkID = expected.objects[0].link.nLinkID;
	if ( !Check( BkEditorMoveObject( pSession, nLinkID, expected.objects[0].vPos.x + 32.0f, expected.objects[0].vPos.y ) == BK_EDITOR_OK,
	             "an object moves through the engine" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	int nTileCount = -1;
	if ( !Check( BkEditorTilesetTiles( pSession, 0, 0, &nTileCount ) == BK_EDITOR_REFUSED && nTileCount > 0,
	             "the tileset's tile count reads" ) )
		return;
	std::vector<unsigned char> tiles( size_t( nTileCount ), 0 );
	int nTilesRead = -1;
	if ( !Check( BkEditorTilesetTiles( pSession, &tiles[0], nTileCount, &nTilesRead ) == BK_EDITOR_OK, "the tileset's tiles read" ) )
		return;
	const BkEditorPaintCell cell = { 30, 30, tiles[0] };
	int nToken = -1;
	if ( !Check( BkEditorPaint( pSession, &cell, 1, &nToken ) == BK_EDITOR_OK, "a cell paints for the verify-save test" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}

	const std::string szEditedBzm = szScratch + "\\verify-edited.bzm";
	remove( szEditedBzm.c_str() );
	if ( !Check( BkEditorSaveMap( pSession, szEditedBzm.c_str() ) == BK_EDITOR_OK, "the edited map saves and reads back as itself (.bzm)" ) )
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );

	const std::string szEditedXml = szScratch + "\\verify-edited.xml";
	remove( szEditedXml.c_str() );
	if ( BkEditorSaveMap( pSession, szEditedXml.c_str() ) != BK_EDITOR_OK )
	{
		// The spec's fallback for .xml only, in case the XML writer does not
		// round-trip a float exactly: read back what NMapFile::Write itself
		// produced, write it again to a second file, and byte-compare the
		// two - idempotent rather than equal-to-the-original.
		printf( "editor-bridge: %s (falling back to the idempotent .xml check)\n", BkEditorLastMessage( pSession ) );
		CMapInfo firstWrite;
		std::string szFirstError;
		if ( Check( NMapFile::Read( szEditedXml.c_str(), &firstWrite, &szFirstError ), szFirstError.c_str() ) )
		{
			const std::string szEditedXmlAgain = szScratch + "\\verify-edited-again.xml";
			remove( szEditedXmlAgain.c_str() );
			std::string szWriteError;
			if ( Check( NMapFile::Write( szEditedXmlAgain.c_str(), firstWrite, &szWriteError ), szWriteError.c_str() ) )
			{
				std::ifstream first( szEditedXml, std::ios::binary );
				std::ifstream again( szEditedXmlAgain, std::ios::binary );
				const std::string szFirstBytes( ( std::istreambuf_iterator<char>( first ) ), std::istreambuf_iterator<char>() );
				const std::string szAgainBytes( ( std::istreambuf_iterator<char>( again ) ), std::istreambuf_iterator<char>() );
				Check( szFirstBytes == szAgainBytes, "and the .xml write is at least idempotent" );
			}
			remove( szEditedXmlAgain.c_str() );
		}
	}

	const std::string szBadPath = szScratch + "\\no-such-dir\\x.bzm";
	if ( Check( BkEditorSaveMap( pSession, szBadPath.c_str() ) != BK_EDITOR_OK, "a save into a missing directory is refused" ) )
		Check( strstr( BkEditorLastMessage( pSession ), szBadPath.c_str() ) != 0,
		       NStr::Format( "and the path is named in the message: %s", BkEditorLastMessage( pSession ) ) );

	Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, "coldwinter reopens afterward" );
	remove( szEditedBzm.c_str() );
	remove( szEditedXml.c_str() );
}

// An edit the engine will not take must not reach the file either, and the
// engine refuses in silence: CAIEditor's MoveObject, TurnObject and SetPlayer
// all return nothing useful and simply leave the object as it was. A poplar is
// the case that makes this visible - the engine's object for one is a
// CGivenPassabilityStObject, which has no direction at all and no owner, so
// both edits are asked for, both are ignored, and both have to come back
// refused rather than be written into the map.
static void TestRefusedEditsReachNeither( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szSaved = szScratch + "\\bridge-refused-edit.bzm";
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, "the map opens" ) )
		return;
	int nLinkID = -1;
	if ( !Check( BkEditorAddObject( pSession, "W_BigPoplar", 100.0f, 100.0f, 0, 0, &nLinkID ) == BK_EDITOR_OK,
	             "a tree is added" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	Check( BkEditorTurnObject( pSession, nLinkID, 1024 ) == BK_EDITOR_REFUSED,
	       "turning something the engine cannot turn is refused" );
	Check( BkEditorSetObjectPlayer( pSession, nLinkID, 1 ) == BK_EDITOR_REFUSED,
	       "and so is giving it to a player" );
	printf( "editor-bridge: refused: %s\n", BkEditorLastMessage( pSession ) );
	// Moving it is fine, so the refusals above are about those two fields and
	// not about the object being untouchable.
	Check( BkEditorMoveObject( pSession, nLinkID, 132.0f, 100.0f ) == BK_EDITOR_OK, "but moving it is not" );
	if ( !Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "and it saves" ) )
		return;

	CMapInfo expected, saved;
	std::string szError, szWhere;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
		return;
	NMapOverlay::SAddObject add;
	add.szName = "W_BigPoplar";
	add.vPos = CVec3( 132.0f, 100.0f, 0.0f );
	add.nDir = 0;      // the refused turn left this alone
	add.nPlayer = 0;   // and so did the refused change of hands
	NMapOverlay::AddObject( &expected, add, 0 );
	if ( !Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
	       szWhere.empty() ? "with neither refused edit in it"
	                       : ( "a refused edit reached the file at " + szWhere ).c_str() );
	remove( szSaved.c_str() );
}

// The case the single-field calls cannot reach: one call changing several
// fields, where the first is accepted and the second is not. The engine applies
// them one after another, so the move lands before the turn is refused, and
// putting only the map back would leave the engine showing the object at a
// position the file never records - the same disagreement the refusal exists to
// prevent, the other way round.
//
// A tree again: its engine object takes a move and cannot take a turn, so the
// two happen in one call by construction rather than by timing.
static void TestPartlyRefusedEditRollsTheEngineBack( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szSaved = szScratch + "\\bridge-partial.bzm";
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, "the map opens" ) )
		return;
	int nLinkID = -1;
	if ( !Check( BkEditorAddObject( pSession, "W_BigPoplar", 100.0f, 100.0f, 0, 0, &nLinkID ) == BK_EDITOR_OK,
	             "a tree is added" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	BkEditorObjectState engineBefore;
	if ( !Check( BkEditorEngineObjectState( pSession, nLinkID, &engineBefore ) == BK_EDITOR_OK,
	             "the engine holds it" ) )
		return;

	// Move and turn together. The move is fine, the turn is not.
	Check( BkEditorPlaceObject( pSession, nLinkID, 132.0f, 100.0f, 1024, 0 ) == BK_EDITOR_REFUSED,
	       "a move the engine takes with a turn it does not is refused whole" );

	BkEditorObjectState engineAfter;
	if ( !Check( BkEditorEngineObjectState( pSession, nLinkID, &engineAfter ) == BK_EDITOR_OK,
	             "the engine still holds it" ) )
		return;
	Check( engineAfter.x == engineBefore.x && engineAfter.y == engineBefore.y,
	       "and the engine did not keep the half of it that worked" );
	Check( engineAfter.dir == engineBefore.dir && engineAfter.player == engineBefore.player,
	       "nor anything else" );

	// And the map agrees: the object is still where it was added.
	if ( !Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "it saves" ) )
		return;
	CMapInfo expected, saved;
	std::string szError, szWhere;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
		return;
	NMapOverlay::SAddObject add;
	add.szName = "W_BigPoplar";
	add.vPos = CVec3( 100.0f, 100.0f, 0.0f );
	NMapOverlay::AddObject( &expected, add, 0 );
	if ( !Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
	       szWhere.empty() ? "with the object where it was before the refused edit"
	                       : ( "a partly refused edit reached the file at " + szWhere ).c_str() );
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
	// Out of range is a caller bug, and reaches neither the map nor the file.
	Check( BkEditorSetAttackingSide( pSession, 2 ) == BK_EDITOR_BAD_ARGUMENT, "an attacking side past 1 is a bad argument" );
	Check( BkEditorSetDiplomacy( pSession, 0, 3 ) == BK_EDITOR_BAD_ARGUMENT, "a diplomacy past 2 is a bad argument" );
	Check( BkEditorSetDiplomacy( pSession, 0, 256 ) == BK_EDITOR_BAD_ARGUMENT, "and so is one a BYTE would wrap to 0" );
	Check( BkEditorSetDiplomacy( pSession, 0, -1 ) == BK_EDITOR_BAD_ARGUMENT, "and one below 0" );
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

// The document is built from this, so it has to be the map as read: every
// object in file order, objects before scenario objects, with the map's own
// owner - not the engine's, which is 0 for anything but a building.
static void TestObjectsReadBack( BkEditorSession *pSession )
{
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, &summary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;

	int nCount = -1;
	Check( BkEditorObjects( pSession, 0, 0, &nCount ) == BK_EDITOR_REFUSED, "a short buffer is refused" );
	Check( nCount == int( map.objects.size() + map.scenarioObjects.size() ), "and still reports the full count" );

	std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
	if ( !Check( BkEditorObjects( pSession, &records[0], nCount, &nCount ) == BK_EDITOR_OK, "the list fits" ) )
		return;
	for ( int i = 0; i < nCount; ++i )
	{
		const bool bScenario = i >= int( map.objects.size() );
		const SMapObjectInfo &rObject = bScenario ? map.scenarioObjects[i - map.objects.size()] : map.objects[i];
		if ( !Check( records[i].link_id == rObject.link.nLinkID &&
		             strcmp( records[i].name, rObject.szName.c_str() ) == 0 &&
		             records[i].x == rObject.vPos.x && records[i].y == rObject.vPos.y &&
		             records[i].dir == rObject.nDir && records[i].player == rObject.nPlayer &&
		             records[i].scenario == ( bScenario ? 1 : 0 ) && records[i].known == 1,
		             NStr::Format( "object %d reads back as the file has it", i ) ) )
			return;
	}

	Check( summary.map_type == map.nType && summary.attacking_side == map.nAttackingSide, "the summary carries the map's own fields" );
	for ( int nPlayer = 0; nPlayer < int( map.diplomacies.size() ); ++nPlayer )
	{
		int nValue = -1;
		Check( BkEditorDiplomacy( pSession, nPlayer, &nValue ) == BK_EDITOR_OK && nValue == map.diplomacies[nPlayer],
		       NStr::Format( "player %d's side reads back", nPlayer ) );
	}
	int nIgnored = 0;
	Check( BkEditorDiplomacy( pSession, int( map.diplomacies.size() ), &nIgnored ) == BK_EDITOR_BAD_ARGUMENT, "a player past the table is a caller bug" );
}

// An object whose type the database does not know is kept as it is: the
// preservation invariant writes it back unchanged, so no edit may reach it.
static void TestUnknownObjectIsReadOnly( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szCopy = szScratch + "\\coldwinter-unknown-read-only.bzm";
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	map.objects[0].szName = "No_Such_Object_In_Any_Database";
	const int nLinkID = map.objects[0].link.nLinkID;
	if ( !Check( NMapFile::Write( szCopy.c_str(), map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, szCopy.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	BkEditorObjectRecord record;
	int nCount = 0;
	BkEditorObjects( pSession, &record, 1, &nCount );
	Check( record.link_id == nLinkID && record.known == 0, "the list marks it unknown" );
	Check( BkEditorDeleteObject( pSession, nLinkID ) == BK_EDITOR_REFUSED, "its delete is refused" );
	Check( BkEditorMoveObject( pSession, nLinkID, record.x + 32, record.y ) == BK_EDITOR_REFUSED, "and so is its move" );

	const std::string szSaved = szScratch + "\\coldwinter-unknown-read-only-saved.bzm";
	CMapInfo saved;
	if ( Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
	{
		std::string szWhere;
		Check( NMapFile::AreEquivalent( map, saved, &szWhere ), ( "and the saved map is the one read: " + szWhere ).c_str() );
	}
	remove( szCopy.c_str() );
	remove( szSaved.c_str() );
}

// Two terrains, everything but which picture was drawn for each cross.
//
// STileTypeDesc::GetMapsIndex picks the artwork for a cross with rand() against
// the tileset's probability ranges (Formats/fmtTerrain.h:84-92), so painting the
// same cell twice gives the same terrain with different pictures on it - two
// independently painted maps can never be equal field for field, and asking for
// that would be asking the engine to be something it is not. Everything that
// decides the shape of the terrain is compared; only the roll is not.
static bool SameTerrainButForTheRoll( const STerrainInfo &rLeft, const STerrainInfo &rRight, std::string *pWhere )
{
	pWhere->clear();
	if ( rLeft.tiles.GetSizeX() != rRight.tiles.GetSizeX() || rLeft.tiles.GetSizeY() != rRight.tiles.GetSizeY() )
	{
		*pWhere = "terrain.tiles size";
		return false;
	}
	for ( int y = 0; y < rLeft.tiles.GetSizeY(); ++y )
		for ( int x = 0; x < rLeft.tiles.GetSizeX(); ++x )
			if ( rLeft.tiles[y][x].tile != rRight.tiles[y][x].tile || rLeft.tiles[y][x].noise != rRight.tiles[y][x].noise )
			{
				*pWhere = NStr::Format( "terrain.tiles[%d][%d]", y, x );
				return false;
			}
	if ( rLeft.patches.GetSizeX() != rRight.patches.GetSizeX() || rLeft.patches.GetSizeY() != rRight.patches.GetSizeY() )
	{
		*pWhere = "terrain.patches size";
		return false;
	}
	for ( int y = 0; y < rLeft.patches.GetSizeY(); ++y )
		for ( int x = 0; x < rLeft.patches.GetSizeX(); ++x )
		{
			const STerrainPatchInfo &rL = rLeft.patches[y][x];
			const STerrainPatchInfo &rR = rRight.patches[y][x];
			if ( rL.basecrosses.size() != rR.basecrosses.size() )
			{
				*pWhere = NStr::Format( "terrain.patches[%d][%d].basecrosses size", y, x );
				return false;
			}
			for ( size_t i = 0; i < rL.basecrosses.size(); ++i )
				if ( rL.basecrosses[i].tile != rR.basecrosses[i].tile || rL.basecrosses[i].x != rR.basecrosses[i].x ||
				     rL.basecrosses[i].y != rR.basecrosses[i].y || rL.basecrosses[i].flags != rR.basecrosses[i].flags )
				{
					*pWhere = NStr::Format( "terrain.patches[%d][%d].basecrosses[%d]", y, x, int( i ) );
					return false;
				}
		}
	return true;
}

// The spec's engine-tier terrain check: after a paint, the engine's terrain
// equals the bridge's copy in tiles and patch crosses, and the saved file
// equals what the overlay produces on its own. The bridge's copy is what gets
// saved, so a disagreement means the editor draws one thing and writes another.
static void TestPaintReachesEngineAndFile( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szSaved = szScratch + "\\bridge-painted.bzm";
	CMapInfo expected;
	std::string szError, szWhere;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
		return;
	if ( !Check( expected.terrain.tiles.GetSizeX() > 64, "the map is big enough to paint in" ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, "the map to paint opens" ) )
		return;

	// A tile the map records as noisy, painted into a cell beside one that
	// already carries it. Noise belongs to the tile in the tileset and the
	// bridge takes the engine's answer for it rather than a caller's, so a tile
	// whose noise is 0 would leave that indistinguishable from not asking at
	// all - and a tile dropped somewhere its neighbours do not match is written
	// straight back out by the preprocessing pass, which is why this looks for
	// a neighbour rather than any quiet cell.
	unsigned char nNoisyTile = 0;
	int nCellX = -1, nCellY = -1;
	for ( int y = 1; y + 1 < expected.terrain.tiles.GetSizeY() && nCellX < 0; ++y )
		for ( int x = 1; x + 1 < expected.terrain.tiles.GetSizeX() && nCellX < 0; ++x )
			if ( expected.terrain.tiles[y][x].noise != 0 &&
			     expected.terrain.tiles[y][x + 1].noise == 0 &&
			     expected.terrain.tiles[y][x + 1].tile != expected.terrain.tiles[y][x].tile )
			{
				nNoisyTile = expected.terrain.tiles[y][x].tile;
				nCellX = x + 1;
				nCellY = y;
			}
	if ( !Check( nCellX >= 0, "the map has a noisy tile with a quiet neighbour to paint over" ) )
		return;

	BkEditorPaintCell cell;
	cell.x = nCellX;
	cell.y = nCellY;
	cell.tile = nNoisyTile;
	int nToken = -1;
	if ( !Check( BkEditorPaint( pSession, &cell, 1, &nToken ) == BK_EDITOR_OK, "a cell paints through the bridge" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	if ( !Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK,
	             "and the engine's terrain matches the copy that will be saved" ) )
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );

	// The brush's other half: a world point becomes the cell it falls in. An
	// object's own position, so it is inside the map by construction.
	if ( Check( !expected.objects.empty(), "the map has an object to take a position from" ) )
	{
		// A map's positions are in AI coordinates and the terrain converts world
		// ones, which is the difference the MFC editor spells AI2Vis before every
		// such call (TemplateEditorFrame1.cpp:1747).
		CVec3 vWorld;
		AI2Vis( &vWorld, expected.objects[0].vPos );
		int nTileX = -1, nTileY = -1;
		Check( BkEditorWorldToTile( pSession, vWorld.x, vWorld.y, &nTileX, &nTileY ) == BK_EDITOR_OK,
		       "a world point becomes a tile index" );
		Check( nTileX >= 0 && nTileX < expected.terrain.tiles.GetSizeX() &&
		       nTileY >= 0 && nTileY < expected.terrain.tiles.GetSizeY(), "inside the map" );
	}

	// And what was saved is what the overlay produces with no engine in sight -
	// the same deterministic function the map file tier tests.
	if ( !Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, "the painted map saves" ) )
		return;
	CMapInfo saved;
	if ( !Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	std::vector<NMapOverlay::SPaintCell> cells;
	NMapOverlay::SPaintCell overlayCell;
	overlayCell.nX = cell.x;
	overlayCell.nY = cell.y;
	overlayCell.tile = cell.tile;
	// The noise flag belongs to the tile, and the bridge takes the engine's
	// answer rather than a caller's; the file is read for it here so the two
	// paints have the same input. That the value is right is not this
	// comparison's job - BkEditorTerrainMatchesEngine above compares noise over
	// the whole map, and that is what would catch a wrong one.
	overlayCell.noise = saved.terrain.tiles[cell.y][cell.x].noise;
	cells.push_back( overlayCell );
	NMapOverlay::SPaintUndo undo;
	if ( !Check( NMapOverlay::Paint( &expected, cells, &undo ), "the overlay paints the same cell" ) )
		return;
	Check( saved.terrain.tiles[cell.y][cell.x].tile == cell.tile, "the painted tile reached the file" );
	Check( saved.terrain.tiles[cell.y][cell.x].noise != 0,
	       "and the noise the tile carries came with it, not the cell's old one" );
	Check( SameTerrainButForTheRoll( expected.terrain, saved.terrain, &szWhere ),
	       szWhere.empty() ? "and the saved terrain is the expected one"
	                       : ( "painted save differs at " + szWhere ).c_str() );
	Check( NMapFile::CompareAltitudeArrays( expected.terrain, saved.terrain ),
	       "and painting left the altitudes alone" );
	remove( szSaved.c_str() );
}

// The catalogue, the camera, a frame, and the two conversions that turn a
// click into a cell. The conversions are checked by composing them: a camera
// put on a known object and asked what is under the middle of the screen has
// to answer with that object's own cell, give or take the tile the anchor
// falls in. Checking only that they return OK would pass on any two numbers.
static void TestCatalogueCameraAndFrame( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	BkEditorMapSummary summary;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, &summary ) == BK_EDITOR_OK, "the map opens" ) )
		return;
	// The open places the camera on the map's middle, the way the game places
	// its mission camera, so a frame drawn before any BkEditorSetCamera already
	// looks at the map and a screen point already has ground under it.
	{
		const int nCentreX = summary.width_tiles / 2, nCentreY = summary.height_tiles / 2;
		float wx = 0.0f, wy = 0.0f;
		int tx = -1, ty = -1;
		Check( BkEditorFrame( pSession ) == BK_EDITOR_OK, "a frame draws before the camera is set" );
		Check( BkEditorScreenToWorld( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f, &wx, &wy ) == BK_EDITOR_OK &&
		       BkEditorWorldToTile( pSession, wx, wy, &tx, &ty ) == BK_EDITOR_OK &&
		       abs( tx - nCentreX ) <= 2 && abs( ty - nCentreY ) <= 2,
		       NStr::Format( "an open map is under the middle of the screen before the camera is set (cell %d,%d against the map's middle %d,%d)", tx, ty, nCentreX, nCentreY ) );
		printf( "editor-bridge: on open the middle of the screen is cell %d,%d, the map's middle %d,%d\n", tx, ty, nCentreX, nCentreY );
	}

	int nCount = 0;
	Check( BkEditorCatalogue( pSession, 0, 0, &nCount ) == BK_EDITOR_REFUSED,
	       "asking with no room is refused" );
	if ( !Check( nCount > 0, "and still says how many there are" ) )
		return;
	std::vector<BkEditorCatalogueEntry> entries( nCount );
	int nRead = 0;
	if ( !Check( BkEditorCatalogue( pSession, &( entries[0] ), int( entries.size() ), &nRead ) == BK_EDITOR_OK,
	             "the object catalogue reads" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}
	Check( nRead == nCount, "and reads all of them" );
	bool bFoundPoplar = false;
	for ( int i = 0; i < nRead && !bFoundPoplar; ++i )
		bFoundPoplar = strcmp( entries[i].name, "W_BigPoplar" ) == 0;
	Check( bFoundPoplar, "and holds a name the map uses" );
	printf( "editor-bridge: the catalogue has %d objects\n", nCount );

	if ( !Check( !map.objects.empty(), "the map has an object to look at" ) )
		return;
	CVec3 vAnchor;
	AI2Vis( &vAnchor, map.objects[0].vPos );
	Check( BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y ) == BK_EDITOR_OK, "the camera moves" );
	if ( !Check( BkEditorFrame( pSession ) == BK_EDITOR_OK, "a frame draws" ) )
	{
		printf( "editor-bridge: %s\n", BkEditorLastMessage( pSession ) );
		return;
	}

	int nAnchorX = -1, nAnchorY = -1;
	if ( !Check( BkEditorWorldToTile( pSession, vAnchor.x, vAnchor.y, &nAnchorX, &nAnchorY ) == BK_EDITOR_OK,
	             "the anchor is on the map" ) )
		return;
	float wx = 0.0f, wy = 0.0f;
	if ( !Check( BkEditorScreenToWorld( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f, &wx, &wy ) == BK_EDITOR_OK,
	             "a screen point becomes a world point" ) )
		return;
	int nMiddleX = -1, nMiddleY = -1;
	if ( !Check( BkEditorWorldToTile( pSession, wx, wy, &nMiddleX, &nMiddleY ) == BK_EDITOR_OK,
	             "and that becomes a cell" ) )
		return;
	printf( "editor-bridge: the camera is on cell %d,%d and the middle of the screen is %d,%d\n",
	        nAnchorX, nAnchorY, nMiddleX, nMiddleY );
	// The anchor is what the camera looks at, so the middle of the screen is
	// within a tile or two of it - not exact, because the camera looks along a
	// slope and the point lands where the ray meets the ground.
	Check( abs( nMiddleX - nAnchorX ) <= 2 && abs( nMiddleY - nAnchorY ) <= 2,
	       "and it is the cell the camera was put on" );
}

// The overlay runs inside the engine's own frame: a callback set through the
// bridge is called once per BkEditorFrame, with a command buffer and a target,
// and not at all once it is removed.
static int g_nOverlayCalls = 0;
static bool g_bOverlayHadTarget = true;
static void CountOverlay( void *pUser, void *pCommandBuffer, void *pTarget, unsigned int nWidth, unsigned int nHeight )
{
	++g_nOverlayCalls;
	if ( pCommandBuffer == 0 || pTarget == 0 || nWidth == 0 || nHeight == 0 || pUser != &g_nOverlayCalls )
		g_bOverlayHadTarget = false;
}

static void TestOverlayDeviceAndSize( BkEditorSession *pSession, SDL_Window *pWindow )
{
	void *pDevice = 0;
	unsigned int nFormat = 0;
	Check( BkEditorGpuDevice( pSession, &pDevice, &nFormat ) == BK_EDITOR_OK && pDevice != 0 && nFormat != 0,
	       "the engine's GPU device and colour format are handed out" );

	g_nOverlayCalls = 0;
	g_bOverlayHadTarget = true;
	Check( BkEditorSetOverlay( pSession, CountOverlay, &g_nOverlayCalls ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	for ( int i = 0; i < 3; ++i )
		BkEditorFrame( pSession );
	// Exactly once per present, not "at least once": a present that skipped
	// the overlay would still pass a >= 1 check on the third frame alone.
	Check( g_nOverlayCalls == 3 && g_bOverlayHadTarget, NStr::Format( "the overlay ran exactly once per present (%d calls for 3 frames)", g_nOverlayCalls ) );
	BkEditorSetOverlay( pSession, 0, 0 );
	const int nCalls = g_nOverlayCalls;
	BkEditorFrame( pSession );
	Check( g_nOverlayCalls == nCalls, "a removed overlay is not called again" );

	// The screen is the window: no desktop-size mode, no scale between a mouse
	// position and a screen position.
	int nWindowW = 0, nWindowH = 0, nScreenW = 0, nScreenH = 0;
	SDL_GetWindowSize( pWindow, &nWindowW, &nWindowH );
	Check( BkEditorScreenSize( pSession, &nScreenW, &nScreenH ) == BK_EDITOR_OK && nScreenW == nWindowW && nScreenH == nWindowH,
	       NStr::Format( "the screen is the window's size (%dx%d against %dx%d)", nScreenW, nScreenH, nWindowW, nWindowH ) );

	// A resize follows the window and never touches it: not moved back to the
	// profile's display or re-centred, not sized, and the screen is the
	// window's size afterwards. The window is put somewhere that is not the
	// origin first, so a re-centre would show.
	SDL_SetWindowPosition( pWindow, 100, 80 );
	SDL_SyncWindow( pWindow );
	SDL_SetWindowSize( pWindow, 800, 500 );
	SDL_SyncWindow( pWindow );
	int nBeforeX = 0, nBeforeY = 0, nBeforeW = 0, nBeforeH = 0;
	SDL_GetWindowPosition( pWindow, &nBeforeX, &nBeforeY );
	SDL_GetWindowSize( pWindow, &nBeforeW, &nBeforeH );
	printf( "editor-bridge: before the resize the window is %dx%d at %d,%d\n", nBeforeW, nBeforeH, nBeforeX, nBeforeY );
	Check( BkEditorResize( pSession, nBeforeW + 1, nBeforeH ) == BK_EDITOR_BAD_ARGUMENT,
	       "a resize to a size that is not the window's is refused" );
	Check( BkEditorResize( pSession, nBeforeW, nBeforeH ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	int nAfterX = 0, nAfterY = 0;
	SDL_GetWindowPosition( pWindow, &nAfterX, &nAfterY );
	SDL_GetWindowSize( pWindow, &nWindowW, &nWindowH );
	Check( nAfterX == nBeforeX && nAfterY == nBeforeY,
	       NStr::Format( "the resize leaves the window where it was (%d,%d, was %d,%d)", nAfterX, nAfterY, nBeforeX, nBeforeY ) );
	Check( nWindowW == nBeforeW && nWindowH == nBeforeH,
	       NStr::Format( "and at the size it was (%dx%d, was %dx%d)", nWindowW, nWindowH, nBeforeW, nBeforeH ) );
	Check( nWindowW == 800 && nWindowH == 500, NStr::Format( "which is the size it was given (%dx%d)", nWindowW, nWindowH ) );
	Check( BkEditorScreenSize( pSession, &nScreenW, &nScreenH ) == BK_EDITOR_OK && nScreenW == nWindowW && nScreenH == nWindowH,
	       NStr::Format( "and the screen is the window (%dx%d against %dx%d)", nScreenW, nScreenH, nWindowW, nWindowH ) );
	printf( "editor-bridge: after a resize the window is %dx%d at %d,%d and the screen %dx%d\n", nWindowW, nWindowH, nAfterX, nAfterY, nScreenW, nScreenH );

	// A window larger than its display's usable area: SetMode clamped the
	// window and kept the larger size for the scene, so the screen stopped
	// being the window. A resize that follows the window keeps the two equal.
	SDL_Rect usable = { 0, 0, 0, 0 };
	if ( Check( SDL_GetDisplayUsableBounds( SDL_GetDisplayForWindow( pWindow ), &usable ) && usable.w > 0 && usable.h > 0, "the display's usable area reads" ) )
	{
		SDL_SetWindowSize( pWindow, usable.w + 200, usable.h + 200 );
		SDL_SyncWindow( pWindow );
		SDL_GetWindowPosition( pWindow, &nBeforeX, &nBeforeY );
		SDL_GetWindowSize( pWindow, &nBeforeW, &nBeforeH );
		Check( BkEditorResize( pSession, nBeforeW, nBeforeH ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		SDL_GetWindowPosition( pWindow, &nAfterX, &nAfterY );
		SDL_GetWindowSize( pWindow, &nWindowW, &nWindowH );
		BkEditorScreenSize( pSession, &nScreenW, &nScreenH );
		printf( "editor-bridge: a window larger than the usable %dx%d: %dx%d at %d,%d before, %dx%d at %d,%d after, screen %dx%d\n",
		        usable.w, usable.h, nBeforeW, nBeforeH, nBeforeX, nBeforeY, nWindowW, nWindowH, nAfterX, nAfterY, nScreenW, nScreenH );
		Check( nWindowW == nBeforeW && nWindowH == nBeforeH && nAfterX == nBeforeX && nAfterY == nBeforeY,
		       "a resize leaves a window larger than its display as it is" );
		Check( nScreenW == nWindowW && nScreenH == nWindowH, "and the screen is still the window" );
		SDL_SetWindowSize( pWindow, 800, 500 );
		SDL_SyncWindow( pWindow );
		Check( BkEditorResize( pSession, 800, 500 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	}
	// Screen-to-world still composes after a resize: the camera test's check,
	// on the camera test's anchor - the shipped map's first object.
	CMapInfo map;
	std::string szError;
	if ( Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) && Check( !map.objects.empty(), "the map has an object to look at" ) )
	{
		CVec3 vAnchor;
		AI2Vis( &vAnchor, map.objects[0].vPos );
		int nAnchorX = -1, nAnchorY = -1;
		BkEditorWorldToTile( pSession, vAnchor.x, vAnchor.y, &nAnchorX, &nAnchorY );
		BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );
		BkEditorFrame( pSession );
		float wx = 0, wy = 0;
		int tx = -1, ty = -1;
		Check( BkEditorScreenToWorld( pSession, 400.0f, 250.0f, &wx, &wy ) == BK_EDITOR_OK &&
		       BkEditorWorldToTile( pSession, wx, wy, &tx, &ty ) == BK_EDITOR_OK &&
		       abs( tx - nAnchorX ) <= 2 && abs( ty - nAnchorY ) <= 2,
		       NStr::Format( "after a resize the middle of the screen is still the camera's cell (%d,%d against %d,%d)", tx, ty, nAnchorX, nAnchorY ) );
		printf( "editor-bridge: after a resize the camera is on cell %d,%d and the middle of the screen is %d,%d\n", nAnchorX, nAnchorY, tx, ty );

		// A resize with the camera already placed and no BkEditorSetCamera
		// after it: the resize places the camera again at its anchor, as the
		// game does on a resolution change, since the placement's distance
		// depends on the screen's height.
		SDL_SetWindowSize( pWindow, 700, 420 );
		SDL_SyncWindow( pWindow );
		Check( BkEditorResize( pSession, 700, 420 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		BkEditorFrame( pSession );
		tx = ty = -1;
		Check( BkEditorScreenToWorld( pSession, 350.0f, 210.0f, &wx, &wy ) == BK_EDITOR_OK &&
		       BkEditorWorldToTile( pSession, wx, wy, &tx, &ty ) == BK_EDITOR_OK &&
		       abs( tx - nAnchorX ) <= 2 && abs( ty - nAnchorY ) <= 2,
		       NStr::Format( "a resize keeps the camera on its anchor without a new BkEditorSetCamera (%d,%d against %d,%d)", tx, ty, nAnchorX, nAnchorY ) );
		printf( "editor-bridge: a resize with no new camera leaves the middle of the screen on %d,%d, the anchor %d,%d\n", tx, ty, nAnchorX, nAnchorY );
	}
	// Back to the size the later tests were given, window first: the resize
	// follows the window.
	SDL_SetWindowSize( pWindow, 640, 480 );
	SDL_SyncWindow( pWindow );
	Check( BkEditorResize( pSession, 640, 480 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorScreenSize( pSession, &nScreenW, &nScreenH ) == BK_EDITOR_OK && nScreenW == 640 && nScreenH == 480,
	       NStr::Format( "and back to 640x480 (%dx%d)", nScreenW, nScreenH ) );
	// The overlay still runs in the frame right after a resize: nothing in
	// BkEditorResize's own path (SetScreenProjection, PublishWorldBase, the
	// re-placed camera) touches the overlay callback, but that was untested.
	g_nOverlayCalls = 0;
	Check( BkEditorSetOverlay( pSession, CountOverlay, &g_nOverlayCalls ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	BkEditorFrame( pSession );
	Check( g_nOverlayCalls == 1, NStr::Format( "the overlay ran in the frame after a resize (%d calls)", g_nOverlayCalls ) );
	BkEditorSetOverlay( pSession, 0, 0 );
}

// The renderer outlives the session, and the overlay is the caller's: a stop
// has to take it out, or the next present calls into whatever the caller freed.
// A present straight through IGFX, with no session left to draw one.
static int PresentThroughTheEngine()
{
	IGFX *pGFX = GetSingleton<IGFX>();
	if ( pGFX == 0 || !pGFX->BeginScene() )
		return 0;
	pGFX->Clear( 0, 0, GFXCLEAR_ALL, 0xff000000 );
	pGFX->EndScene();
	pGFX->Flip();
	return 1;
}

// The frame at the camera, as an uncompressed 32-bit TGA, so a person can look
// at what the pick ratio only counts. Written by BkEditorCaptureFrame, which the
// app's own check reads as well, so the file is checked here to be the
// screen's size and top row first: a capture of the wrong size would pass
// the app's check at a pixel that happened to fall inside.
static bool SaveFrame( BkEditorSession *pSession, const std::string &szPath )
{
	if ( !Check( BkEditorCaptureFrame( pSession, szPath.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return false;
	int nScreenW = 0, nScreenH = 0;
	BkEditorScreenSize( pSession, &nScreenW, &nScreenH );
	unsigned char header[18] = { 0 };
	FILE *pFile = fopen( szPath.c_str(), "rb" );
	const bool bRead = pFile != 0 && fread( header, 1, sizeof header, pFile ) == sizeof header;
	long nLength = 0;
	if ( pFile != 0 )
	{
		fseek( pFile, 0, SEEK_END );
		nLength = ftell( pFile );
		fclose( pFile );
	}
	const int nWidth = header[12] | ( header[13] << 8 ), nHeight = header[14] | ( header[15] << 8 );
	return Check( bRead && header[2] == 2 && header[16] == 32 && ( header[17] & 0x20 ) != 0 &&
	              nWidth == nScreenW && nHeight == nScreenH && nLength == 18 + long( nWidth ) * nHeight * 4,
	              NStr::Format( "the captured frame is a %dx%d top-first 32-bit TGA of %ld bytes, the screen %dx%d", nWidth, nHeight, nLength, nScreenW, nScreenH ) );
}

// Defined below (after TestObjectUnderTheCursor); forward-declared here so
// TestTerrainUnderTheCamera's second anchor can reuse it instead of the
// hand-rolled read its first anchor used to do alone (Task 6 carried).
static std::vector<unsigned char> ReadFramePixels( const std::string &szPath, int *pnWidth, int *pnHeight );

// The ground is drawn wherever the camera looks. The terrain is laid out in
// screen space for the game's own camera - yaw 45, pitch 30, the rod of
// iMissionInternal.cpp's SetMissionCameraPlacement - while objects go through
// the view matrix, so a camera placed any other way draws the objects and
// leaves the ground black under them. Measured on the anchor below (the
// shipped map's first object, W_BigPoplar, the camera test's anchor) at
// 640x480: the game's own frame there has 1.7% black pixels in the lower half
// (its HUD), the bridge's frame 80.4% before its camera was placed like the
// game's and 0.5% after. The lower half is below the haze, which fades to
// black at the top.
static const float TERRAIN_BLACK_BAR = 0.10f;

// The lower half's black fraction of a captured, top-first 32-bit BGRA frame
// (SaveFrame/ReadFramePixels's own layout) - factored out of
// TestTerrainUnderTheCamera so the same check runs at more than one anchor.
static float LowerHalfBlackFraction( const std::vector<unsigned char> &pixels, int nWidth, int nHeight )
{
	int nBlack = 0, nCounted = 0;
	for ( int y = nHeight / 2; y < nHeight; ++y )
	{
		for ( int x = 0; x < nWidth; ++x, ++nCounted )
		{
			const unsigned char *p = &pixels[( size_t( y ) * nWidth + x ) * 4];
			if ( p[0] < 16 && p[1] < 16 && p[2] < 16 )
				++nBlack;
		}
	}
	return nCounted > 0 ? float( nBlack ) / nCounted : 1.0f;
}

static void TestTerrainUnderTheCamera( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	// Two anchors from the pick set: TestObjectUnderTheCursor's own scan of
	// the first up-to-20 map objects the engine actually holds (its own
	// 16/20 picking bar), not an arbitrary map index - the map's last object
	// by file order sits at the map's edge (measured: 79.2% black, since the
	// camera runs out of ground before its full view), which an unfiltered
	// "first and last" pick would have hit. The first of the pick set and
	// one partway through it are two different, engine-known-good objects,
	// so one anchor's coincidentally-clear ground cannot say "the ground is
	// always drawn under the camera" on its own (Task 6 carried).
	std::vector<size_t> pickSet;
	for ( size_t i = 0; i < map.objects.size() && pickSet.size() < 20; ++i )
	{
		BkEditorObjectState state;
		if ( BkEditorEngineObjectState( pSession, map.objects[i].link.nLinkID, &state ) == BK_EDITOR_OK )
			pickSet.push_back( i );
	}
	if ( !Check( pickSet.size() >= 2, "the pick set has two objects to look at" ) )
		return;
	const size_t anchorIndices[] = { pickSet[0], pickSet[pickSet.size() / 2] };
	for ( size_t n = 0; n < 2; ++n )
	{
		const auto &object = map.objects[anchorIndices[n]];
		CVec3 vAnchor;
		AI2Vis( &vAnchor, object.vPos );
		BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );
		BkEditorFrame( pSession );
		const std::string szFrame = szScratch + ( n == 0 ? "/editor-bridge-terrain.tga" : "/editor-bridge-terrain-2.tga" );
		if ( !SaveFrame( pSession, szFrame ) )
			continue;
		int nWidth = 0, nHeight = 0;
		const std::vector<unsigned char> pixels = ReadFramePixels( szFrame, &nWidth, &nHeight );
		if ( !Check( !pixels.empty(), "the terrain frame reads back" ) )
			continue;
		const float fBlack = LowerHalfBlackFraction( pixels, nWidth, nHeight );
		printf( "editor-bridge: the lower half of the frame at %.0f,%.0f is %.1f%% black (%s, bar %.0f%%)\n",
		        vAnchor.x, vAnchor.y, fBlack * 100.0f, object.szName.c_str(), TERRAIN_BLACK_BAR * 100.0f );
		Check( fBlack < TERRAIN_BLACK_BAR, NStr::Format( "the ground is drawn under the camera (%.1f%% of the lower half black)", fBlack * 100.0f ) );
	}
}

// D-10, D-11, D-14, D-16: the zoom is bounded by NSceneScreenScale's own
// limit for the window's size, and a zoom stays anchored on the world point
// under the screen point it was asked at, in either direction, past the
// bound and back.
static void TestZoomStepsBoundedAndAnchored( BkEditorSession *pSession, SDL_Window *pWindow, const std::string &szScratch )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) || !Check( !map.objects.empty(), "the map has an object to look at" ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CVec3 vAnchor;
	AI2Vis( &vAnchor, map.objects[0].vPos );
	BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );

	// Larger than the other tests' 640x480, as TestOverlayDeviceAndSize
	// resizes to: more room before the D-09 zoom-in bound (the view may
	// shrink to a 640x480-effective viewport) is reached.
	SDL_SetWindowSize( pWindow, 1280, 960 );
	SDL_SyncWindow( pWindow );
	int nWidth = 0, nHeight = 0;
	SDL_GetWindowSize( pWindow, &nWidth, &nHeight );
	if ( !Check( BkEditorResize( pSession, nWidth, nHeight ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorFrame( pSession );

	BkEditorView view;
	if ( !Check( BkEditorViewState( pSession, &view ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	printf( "editor-bridge: at %dx%d the max zoom is %d steps\n", nWidth, nHeight, view.max_zoom_steps );
	Check( view.max_zoom_steps >= 1, NStr::Format( "the window is wide enough to zoom at least once (%d steps)", view.max_zoom_steps ) );

	// Left and above the centre: the world point there should stay under it
	// while zooming in as far as the window allows.
	const float fPointX = float( nWidth ) / 2.0f - 120.0f, fPointY = float( nHeight ) / 2.0f - 90.0f;
	float wxBefore = 0.0f, wyBefore = 0.0f;
	if ( !Check( BkEditorScreenToWorld( pSession, fPointX, fPointY, &wxBefore, &wyBefore ) == BK_EDITOR_OK, "the point is on the map before zooming" ) )
		return;
	Check( BkEditorZoomAt( pSession, 20, fPointX, fPointY ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorViewState( pSession, &view ) == BK_EDITOR_OK && view.zoom_steps == view.max_zoom_steps,
	       NStr::Format( "a large zoom-in clamps at the maximum (%d against %d)", view.zoom_steps, view.max_zoom_steps ) );
	Check( view.scale > 1.0f, NStr::Format( "the scale grew with the zoom (%.2f)", view.scale ) );
	BkEditorFrame( pSession );
	float wxAfter = 0.0f, wyAfter = 0.0f;
	Check( BkEditorScreenToWorld( pSession, fPointX, fPointY, &wxAfter, &wyAfter ) == BK_EDITOR_OK &&
	       fabsf( wxAfter - wxBefore ) <= 2.0f && fabsf( wyAfter - wyBefore ) <= 2.0f,
	       NStr::Format( "the point stayed anchored while zooming in (%.1f,%.1f against %.1f,%.1f)", wxAfter, wyAfter, wxBefore, wyBefore ) );

	const std::string szFrame = szScratch + "/editor-bridge-zoom.tga";
	if ( SaveFrame( pSession, szFrame ) )
	{
		std::vector<unsigned char> file;
		if ( FILE *pFile = fopen( szFrame.c_str(), "rb" ) )
		{
			fseek( pFile, 0, SEEK_END );
			file.resize( size_t( ftell( pFile ) ) );
			fseek( pFile, 0, SEEK_SET );
			if ( fread( file.data(), 1, file.size(), pFile ) != file.size() )
				file.clear();
			fclose( pFile );
		}
		if ( Check( file.size() > 18, "the zoomed frame reads back" ) )
		{
			const int nFrameWidth = file[12] | ( file[13] << 8 ), nFrameHeight = file[14] | ( file[15] << 8 );
			const unsigned char *pPixels = &file[18];
			int nBlack = 0, nCounted = 0;
			for ( int y = nFrameHeight / 2; y < nFrameHeight; ++y )
				for ( int x = 0; x < nFrameWidth; ++x, ++nCounted )
				{
					const unsigned char *p = pPixels + ( size_t( y ) * nFrameWidth + x ) * 4;
					if ( p[0] < 16 && p[1] < 16 && p[2] < 16 )
						++nBlack;
				}
			const float fBlack = nCounted > 0 ? float( nBlack ) / nCounted : 1.0f;
			printf( "editor-bridge: at max zoom the lower half of the frame is %.1f%% black\n", fBlack * 100.0f );
			Check( fBlack < TERRAIN_BLACK_BAR, NStr::Format( "the terrain rebuilt after the zoom (%.1f%% black)", fBlack * 100.0f ) );
		}
	}

	Check( BkEditorZoomAt( pSession, -20, fPointX, fPointY ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorViewState( pSession, &view ) == BK_EDITOR_OK && view.zoom_steps == 0,
	       NStr::Format( "a large zoom-out clamps at 0 (%d)", view.zoom_steps ) );

	Check( BkEditorSetZoom( pSession, view.max_zoom_steps + 5 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorViewState( pSession, &view ) == BK_EDITOR_OK && view.zoom_steps == view.max_zoom_steps,
	       NStr::Format( "SetZoom past the maximum clamps (%d against %d)", view.zoom_steps, view.max_zoom_steps ) );

	// Back to 0 and the size the later tests expect.
	BkEditorSetZoom( pSession, 0 );
	SDL_SetWindowSize( pWindow, 640, 480 );
	SDL_SyncWindow( pWindow );
	SDL_GetWindowSize( pWindow, &nWidth, &nHeight );
	Check( BkEditorResize( pSession, nWidth, nHeight ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
}

// One round trip at whatever zoom the camera is at now: the centre and four
// points 150-200 px off it should come back within 2 px, and screen
// right/up should be the world directions the camera was placed for (the
// direction check carried from plan 5 Task 6).
static void CheckWorldToScreenRoundTrip( BkEditorSession *pSession, float fCentreX, float fCentreY, const char *pszWhen )
{
	float wx0 = 0.0f, wy0 = 0.0f;
	if ( !Check( BkEditorScreenToWorld( pSession, fCentreX, fCentreY, &wx0, &wy0 ) == BK_EDITOR_OK, "the centre is on the map" ) )
		return;
	float sx0 = 0.0f, sy0 = 0.0f;
	Check( BkEditorWorldToScreen( pSession, wx0, wy0, &sx0, &sy0 ) == BK_EDITOR_OK &&
	       fabsf( sx0 - fCentreX ) <= 2.0f && fabsf( sy0 - fCentreY ) <= 2.0f,
	       NStr::Format( "%s: the centre round-trips (%.1f,%.1f against %.1f,%.1f)", pszWhen, sx0, sy0, fCentreX, fCentreY ) );

	static const float offsets[4][2] = { { -180, -150 }, { 180, -150 }, { -180, 150 }, { 180, 150 } };
	for ( int i = 0; i < 4; ++i )
	{
		const float sx = fCentreX + offsets[i][0], sy = fCentreY + offsets[i][1];
		float wx = 0.0f, wy = 0.0f;
		if ( BkEditorScreenToWorld( pSession, sx, sy, &wx, &wy ) != BK_EDITOR_OK )
			continue;	// off the terrain at this zoom/window size - nothing to round-trip
		float sx2 = 0.0f, sy2 = 0.0f;
		Check( BkEditorWorldToScreen( pSession, wx, wy, &sx2, &sy2 ) == BK_EDITOR_OK &&
		       fabsf( sx2 - sx ) <= 2.0f && fabsf( sy2 - sy ) <= 2.0f,
		       NStr::Format( "%s: point %d round-trips (%.1f,%.1f against %.1f,%.1f)", pszWhen, i, sx2, sy2, sx, sy ) );
	}

	float wxRight = 0.0f, wyRight = 0.0f, wxUp = 0.0f, wyUp = 0.0f;
	if ( Check( BkEditorScreenToWorld( pSession, fCentreX + 100.0f, fCentreY, &wxRight, &wyRight ) == BK_EDITOR_OK &&
	            BkEditorScreenToWorld( pSession, fCentreX, fCentreY - 100.0f, &wxUp, &wyUp ) == BK_EDITOR_OK,
	            "right and up of the centre are on the map" ) )
	{
		Check( wxRight > wx0 && wyRight > wy0,
		       NStr::Format( "%s: screen right is world (+x,+y) (%.1f,%.1f against %.1f,%.1f)", pszWhen, wxRight, wyRight, wx0, wy0 ) );
		Check( wxUp < wx0 && wyUp > wy0,
		       NStr::Format( "%s: screen up is world (-x,+y) (%.1f,%.1f against %.1f,%.1f)", pszWhen, wxUp, wyUp, wx0, wy0 ) );
	}
}

// BkEditorWorldToScreen: composes with BkEditorScreenToWorld at any zoom,
// the direction check carries from plan 5 Task 6, and BkEditorWorldToTile's
// world-corner convention is what view.zig's brush outline relies on.
static void TestWorldToScreenRoundTrip( BkEditorSession *pSession, int nWidth, int nHeight )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) || !Check( !map.objects.empty(), "the map has an object to look at" ) )
		return;
	BkEditorMapSummary summary;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, &summary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CVec3 vAnchor;
	AI2Vis( &vAnchor, map.objects[0].vPos );
	BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );
	BkEditorFrame( pSession );

	const float fCentreX = float( nWidth ) / 2.0f, fCentreY = float( nHeight ) / 2.0f;
	CheckWorldToScreenRoundTrip( pSession, fCentreX, fCentreY, "at zoom 0" );

	BkEditorView view;
	if ( Check( BkEditorViewState( pSession, &view ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) && view.max_zoom_steps > 0 )
	{
		Check( BkEditorSetZoom( pSession, view.max_zoom_steps ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		BkEditorFrame( pSession );
		CheckWorldToScreenRoundTrip( pSession, fCentreX, fCentreY, "at max zoom" );
		BkEditorSetZoom( pSession, 0 );
	}

	// CTerrain's own GetTileIndex (Scene/TerrainEditor.cpp) rounds to the
	// NEAREST tile rather than flooring into a bucket (WorldToTile's default
	// isExact=false), so a tile's centre - not a corner - is a plain
	// index * fWorldCellSize for X; Y is measured from the terrain's far
	// edge, not from world_y 0, so its centre is (height_tiles - row) *
	// fWorldCellSize instead. A cell's corner therefore sits half a cell
	// off its centre - minus in X, plus in Y (the flip) - which is exactly
	// what view.zig's drawOverlay needs to walk a brush's boundary: this
	// confirms the relationship rather than assuming it.
	int nTileX = -1, nTileY = -1;
	const float fCentreWX = 10.0f * fWorldCellSize;
	const float fCentreWY = float( summary.height_tiles - 8 ) * fWorldCellSize;
	const float fCornerX = fCentreWX - fWorldCellSize / 2.0f;
	const float fCornerY = fCentreWY + fWorldCellSize / 2.0f;
	Check( BkEditorWorldToTile( pSession, fCornerX + fWorldCellSize / 2.0f, fCornerY - fWorldCellSize / 2.0f, &nTileX, &nTileY ) == BK_EDITOR_OK &&
	       nTileX == 10 && nTileY == 8,
	       NStr::Format( "a tile's world corner plus half a cell gives that tile (%d,%d against 10,8)", nTileX, nTileY ) );
}

// A camera put on an object answers, at the middle of the screen, with that
// object - the picking half of "the camera is on cell 83,36 and the middle of
// the screen is 83,36".
// How far above the middle of the screen the pick is made. The camera's anchor
// is at height 0 and an object stands on the ground, which on this map is up to
// about 12 pixels above or below that (measured: an object at height 11.7 lands
// 9 pixels higher on the screen, one at -17 11 pixels lower). A sprite's hit box
// rises from its foot and never reaches below it, so a point exactly in the
// middle misses every object standing a little higher than height 0. The
// "4 of 20 without a rise, 11 of 20 with this rise" this comment used to cite
// was measured before the camera was placed like the game's own (see
// TestObjectUnderTheCursor's own note); today's count with this rise and the
// game's placement is that note's 16 of 20, not restated here to avoid a
// second copy drifting stale again.
static const float PICK_RISE = 12.0f;

static void TestObjectUnderTheCursor( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	// Game types by name, so one frame can be kept over a unit as well as the
	// first one over anything.
	int nCatalogue = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
	std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
	int nRead = 0;
	BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nRead );
	int nPicked = 0, nTried = 0;
	bool bSaved = false, bSavedUnit = false;
	for ( size_t i = 0; i < map.objects.size() && nTried < 20; ++i )
	{
		BkEditorObjectState state;
		if ( BkEditorEngineObjectState( pSession, map.objects[i].link.nLinkID, &state ) != BK_EDITOR_OK )
			continue;
		++nTried;
		// The engine answers in AI units and the camera takes world units, as
		// TestCatalogueCameraAndFrame converts them.
		CVec3 vAnchor;
		AI2Vis( &vAnchor, state.x, state.y, 0.0f );
		BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );
		BkEditorFrame( pSession );
		int nLinkID = -1;
		if ( BkEditorObjectAt( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f - PICK_RISE, &nLinkID ) == BK_EDITOR_OK &&
		     nLinkID == map.objects[i].link.nLinkID )
		{
			++nPicked;
			// Frames to look at: over the first object picked, and over the first
			// unit (game type 1, SGVOGT_UNIT) picked.
			bool bUnit = false;
			for ( int j = 0; j < nRead && !bUnit; ++j )
				bUnit = catalogue[j].game_type == 1 && map.objects[i].szName == catalogue[j].name;
			if ( !bSaved || ( bUnit && !bSavedUnit ) )
			{
				const std::string szFrame = szScratch + ( bSaved ? "/editor-bridge-unit.tga" : "/editor-bridge-objects.tga" );
				const bool bWritten = SaveFrame( pSession, szFrame );
				printf( "editor-bridge: %s %s (%s)\n", bWritten ? "saved" : "could not save", szFrame.c_str(), map.objects[i].szName.c_str() );
				if ( bSaved )
					bSavedUnit = true;
				bSaved = true;
				if ( bUnit )
					bSavedUnit = true;
			}
		}
	}
	printf( "editor-bridge: %d of %d objects picked at the middle of the screen\n", nPicked, nTried );
	// Not every one: a small object can sit behind a big neighbour, and that
	// neighbour is a right answer too. Measured on macOS arm64 at 1440x900:
	// 11 of 20 with CCamera's default placement, and 16 of 20 at 640x480 once
	// the camera was placed like the game's (pitch 30, not 45). The misses are
	// a neighbour the scene listed first (the MFC editor, copied here, takes
	// the first) and one object at the map's edge where the camera stops
	// short of it. Half is the bar, just under that.
	Check( nTried > 0 && nPicked * 2 >= nTried, "the object under the camera is the one picked, for most objects" );

	int nNothing = -1;
	BkEditorSetCamera( pSession, 16.0f, 16.0f );
	BkEditorFrame( pSession );
	Check( BkEditorObjectAt( pSession, 0.0f, 0.0f, &nNothing ) != BK_EDITOR_FAILED, "a point over nothing is an answer, not a failure" );
	Check( BkEditorObjectAt( pSession, 0.0f, 0.0f, 0 ) == BK_EDITOR_BAD_ARGUMENT, "and nowhere to put the answer is a bad argument" );
}

// A captured frame's pixels, BGRA, top row first (SaveFrame checked the
// header). Empty when the file does not read, or when its type, bit depth
// or descriptor do not match SaveFrame's own layout - assumed without a
// check until now, so a differently-shaped TGA (a truncated capture, or the
// wrong file handed in by mistake) silently read as noise instead of
// failing with a reason (Task 7.1 carried).
static std::vector<unsigned char> ReadFramePixels( const std::string &szPath, int *pnWidth, int *pnHeight )
{
	std::vector<unsigned char> file;
	if ( FILE *pFile = fopen( szPath.c_str(), "rb" ) )
	{
		fseek( pFile, 0, SEEK_END );
		file.resize( size_t( ftell( pFile ) ) );
		fseek( pFile, 0, SEEK_SET );
		if ( fread( file.data(), 1, file.size(), pFile ) != file.size() )
			file.clear();
		fclose( pFile );
	}
	if ( file.size() <= 18 )
		return std::vector<unsigned char>();
	// Type 2 (uncompressed truecolour), 32 bits, descriptor bit 5 set (top
	// row first) - exactly what SaveFrame writes and what every caller here
	// assumes.
	if ( file[2] != 2 || file[16] != 32 || ( file[17] & 0x20 ) == 0 )
	{
		printf( "editor-bridge: %s is not a top-first 32-bit uncompressed TGA (type %d, %d bits, descriptor 0x%02x)\n",
		        szPath.c_str(), int( file[2] ), int( file[16] ), int( file[17] ) );
		return std::vector<unsigned char>();
	}
	*pnWidth = file[12] | ( file[13] << 8 );
	*pnHeight = file[14] | ( file[15] << 8 );
	const size_t nExpected = 18 + size_t( *pnWidth ) * size_t( *pnHeight ) * 4;
	if ( file.size() < nExpected )
	{
		printf( "editor-bridge: %s is %zu bytes, a %dx%d 32-bit TGA needs %zu\n", szPath.c_str(), file.size(), *pnWidth, *pnHeight, nExpected );
		return std::vector<unsigned char>();
	}
	return std::vector<unsigned char>( file.begin() + 18, file.end() );
}

// How many pixels of a box differ between two frames by more than a little -
// the "something is drawn there now" half of a placed object.
static int ChangedPixels( const std::vector<unsigned char> &rBefore, const std::vector<unsigned char> &rAfter,
                          int nWidth, int nHeight, int nLeft, int nTop, int nRight, int nBottom )
{
	if ( rBefore.size() != rAfter.size() || rBefore.size() < size_t( nWidth ) * nHeight * 4 )
		return -1;
	int nChanged = 0;
	for ( int y = Max( 0, nTop ); y < Min( nHeight, nBottom ); ++y )
		for ( int x = Max( 0, nLeft ); x < Min( nWidth, nRight ); ++x )
		{
			const size_t n = ( size_t( y ) * nWidth + x ) * 4;
			if ( abs( int( rBefore[n] ) - rAfter[n] ) + abs( int( rBefore[n + 1] ) - rAfter[n + 1] ) +
			     abs( int( rBefore[n + 2] ) - rAfter[n + 2] ) > 48 )
				++nChanged;
		}
	return nChanged;
}

// An object the editor places is drawn where it was put and answers a click
// there, as the map's own objects do. The editor's placer goes from the click
// to the object the way this check does - the screen point to the world point
// under it (BkEditorScreenToWorld), that to the map position
// (BkEditorWorldToMap), then BkEditorAddObject - for a static object and for a
// unit. Found by the app's smoke (plan 5, Task 7): the bridge held the object
// and saved it, but no frame showed it and no click found it. The placer handed
// the world point to BkEditorAddObject, which takes map units, so the object
// went in at 0.7 of the clicked point's distance from the map's corner - drawn
// and pickable, but off the screen. Measured with the world point passed
// straight in: nothing picked and 0 pixels changed for either object; with the
// map position, both picked and about 1000 pixels changed.
//
// The box compared is above the click, where an object standing on the ground
// there is drawn: a sprite rises from its foot. Measured at 640x480: the poplar
// changes 1209 of its 1664 pixels, the T-34 1015.
static const int PLACED_BOX_HALF_WIDTH = 16, PLACED_BOX_HEIGHT = 48, PLACED_MIN_CHANGED = 40;

static void TestPlacedObjectDrawsAndPicks( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	static const char *const names[] = { "W_BigPoplar", "T-34" };
	for ( int nName = 0; nName < 2; ++nName )
	{
		const char *pszName = names[nName];
		if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			return;
		CMapInfo map;
		std::string szError;
		if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) || !Check( !map.objects.empty(), "the map has an object to look at" ) )
			return;
		// Bare ground on screen to place on: the first point, on a ring round
		// the middle of the screen with the camera on the map's first object,
		// that no object answers at.
		CVec3 vAnchor;
		AI2Vis( &vAnchor, map.objects[0].vPos );
		BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );
		BkEditorFrame( pSession );
		BkEditorFrame( pSession );
		float sx = -1.0f, sy = -1.0f;
		for ( int nTry = 0; nTry < 16 && sx < 0.0f; ++nTry )
		{
			const float fX = nScreenWidth / 2.0f + ( ( nTry % 4 ) - 1.5f ) * 100.0f;
			const float fY = nScreenHeight / 2.0f + ( ( nTry / 4 ) - 1.5f ) * 60.0f + PLACED_BOX_HEIGHT / 2;
			int nIgnored = -1;
			bool bClear = true;
			for ( int dy = 0; dy <= PLACED_BOX_HEIGHT && bClear; dy += 8 )
				for ( int dx = -PLACED_BOX_HALF_WIDTH; dx <= PLACED_BOX_HALF_WIDTH && bClear; dx += 8 )
					bClear = BkEditorObjectAt( pSession, fX + dx, fY - dy, &nIgnored ) != BK_EDITOR_OK;
			if ( bClear )
			{
				sx = fX;
				sy = fY;
			}
		}
		if ( !Check( sx >= 0.0f, "there is bare ground on screen to place on" ) )
			return;
		const std::string szBefore = szScratch + "/editor-bridge-placed-before.tga";
		const std::string szAfter = szScratch + NStr::Format( "/editor-bridge-placed-%d.tga", nName );
		if ( !SaveFrame( pSession, szBefore ) )
			return;
		float wx = 0.0f, wy = 0.0f;
		if ( !Check( BkEditorScreenToWorld( pSession, sx, sy, &wx, &wy ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			return;
		float mx = 0.0f, my = 0.0f;
		if ( !Check( BkEditorWorldToMap( pSession, wx, wy, &mx, &my ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			return;
		Check( fabs( mx - wx * FP_SQRT_2 ) < 0.01f && fabs( my - wy * FP_SQRT_2 ) < 0.01f,
		       NStr::Format( "a map unit is sqrt 2 world units' worth smaller (world %.1f,%.1f is map %.1f,%.1f)", wx, wy, mx, my ) );
		int nLinkID = -1;
		if ( !Check( BkEditorAddObject( pSession, pszName, mx, my, 0, 0, &nLinkID ) == BK_EDITOR_OK,
		             NStr::Format( "%s is placed at the map position under %.0f,%.0f: %s", pszName, sx, sy, BkEditorLastMessage( pSession ) ) ) )
			return;
		BkEditorObjectState state;
		BkEditorEngineObjectState( pSession, nLinkID, &state );
		for ( int i = 0; i < 4; ++i )
			BkEditorFrame( pSession );
		int nPicked = -1;
		bool bPicked = false;
		for ( int dy = 0; dy <= PLACED_BOX_HEIGHT && !bPicked; dy += 4 )
			bPicked = BkEditorObjectAt( pSession, sx, sy - dy, &nPicked ) == BK_EDITOR_OK && nPicked == nLinkID;
		const bool bSaved = SaveFrame( pSession, szAfter );
		int nWidth = 0, nHeight = 0;
		const std::vector<unsigned char> before = ReadFramePixels( szBefore, &nWidth, &nHeight );
		const std::vector<unsigned char> after = ReadFramePixels( szAfter, &nWidth, &nHeight );
		const int nChanged = bSaved ? ChangedPixels( before, after, nWidth, nHeight, int( sx ) - PLACED_BOX_HALF_WIDTH, int( sy ) - PLACED_BOX_HEIGHT,
		                                             int( sx ) + PLACED_BOX_HALF_WIDTH, int( sy ) + 4 ) : -1;
		printf( "editor-bridge: %s placed at screen %.0f,%.0f = world %.1f,%.1f = map %.1f,%.1f; the engine holds it at %.1f,%.1f; "
		        "picked there: %s; %d pixels changed above it\n",
		        pszName, sx, sy, wx, wy, mx, my, state.x, state.y, bPicked ? "yes" : "no", nChanged );
		Check( bPicked, NStr::Format( "a click where %s was placed answers with it (link ID %d, answered %d)", pszName, nLinkID, nPicked ) );
		Check( nChanged >= PLACED_MIN_CHANGED, NStr::Format( "%s is drawn where it was placed (%d pixels changed, bar %d)", pszName, nChanged, PLACED_MIN_CHANGED ) );
	}
}

// The texture each unit on screen is drawn with, read the way the renderer
// reads it: a visit hands over the mesh's or the sprite's texture, and the
// texture manager names it by the key it was loaded under - the model path the
// map object chose plus its season's letter ("...\1w" in winter).
class CTextureNameVisitor : public ISceneVisitor
{
public:
	std::vector<std::string> names;
	virtual void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff ) {  }
	virtual void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff ) {  }
	virtual bool STDCALL IsValid() const { return true; }
	void Add( IGFXTexture *pTexture )
	{
		ITextureManager *pTM = GetSingleton<ITextureManager>();
		std::string szName = pTexture == 0 || pTM == 0 ? "<none>" : pTM->GetTextureName( pTexture );
		NStr::ToLower( szName );
		names.push_back( szName );
	}
	virtual void STDCALL VisitSprite( const SBasicSpriteInfo *pObj, int nType, int nPriority ) { Add( pObj->pTexture ); }
	virtual void STDCALL VisitMeshObject( IMeshVisObj *pObj, int nType, int nPriority ) { Add( pObj->GetTexture() ); }
	virtual void STDCALL VisitParticles( IParticleSource *pObj ) {  }
	virtual void STDCALL VisitSceneObject( ISceneObject *pObj ) {  }
	virtual void STDCALL VisitText( const CVec3 &vPos, const char *pszText, IGFXFont *pFont, DWORD color ) {  }
	virtual void STDCALL VisitBoldLine( CVec3 *corners, float fWidth, DWORD color ) {  }
	virtual void STDCALL VisitMechTrace( const SMechTrace &trace ) {  }
	virtual void STDCALL VisitGunTrace( const SGunTrace &trace ) {  }
	virtual void STDCALL VisitUIRects( IGFXTexture *pTexture, const int nShadingEffect, SGFXRect2 *rects, const int nNumRects ) {  }
	virtual void STDCALL VisitUIText( IGFXText *pText, const CTRect<float> &rcRect, const int nY, const DWORD dwColor, const DWORD dwFlags ) {  }
	virtual void STDCALL VisitUICustom( IUIElement *pElement ) {  }
};

// The last part of a unit texture's key without its digits: "" or "b" (blood)
// in summer, "w" or "bw" in winter. Anything else - "default" for a texture
// that did not load, a path outside units\ - answers with "?".
static std::string UnitTextureSeasonLetters( const std::string &szName )
{
	if ( szName.compare( 0, 6, "units\\" ) != 0 )
		return "?";
	const size_t nSlash = szName.find_last_of( "\\/" );
	const std::string szLast = szName.substr( nSlash + 1 );
	size_t nDigits = 0;
	while ( nDigits < szLast.size() && szLast[nDigits] >= '0' && szLast[nDigits] <= '9' )
		++nDigits;
	if ( nDigits == 0 )
		return "?";
	return szLast.substr( nDigits );
}

// The textures of every unit the scene has on screen now.
static std::vector<std::string> UnitTexturesOnScreen( int nScreenWidth, int nScreenHeight )
{
	CTextureNameVisitor visitor;
	IScene *pScene = GetSingleton<IScene>();
	if ( pScene == 0 )
		return visitor.names;
	std::pair<IVisObj*, CVec2> *pObjects = 0;
	int nCount = 0;
	pScene->Pick( CTRect<float>( 0.0f, 0.0f, float( nScreenWidth ), float( nScreenHeight ) ), &pObjects, &nCount, SGVOGT_UNIT );
	for ( int i = 0; i < nCount; ++i )
		if ( pObjects[i].first != 0 )
			pObjects[i].first->Visit( &visitor );
	return visitor.names;
}

// How many of a set of texture keys are unit textures, how many of those are in
// the season's paint, and the first few that are not.
struct SSeasonTally
{
	int nUnits, nRight;
	std::string szWrong;
	SSeasonTally() : nUnits( 0 ), nRight( 0 ) {  }
	void Add( const std::string &szName, int nSeason )
	{
		const std::string szLetters = UnitTextureSeasonLetters( szName );
		if ( szLetters == "?" )
			return;
		++nUnits;
		if ( nSeason == 1 ? ( szLetters == "w" || szLetters == "bw" ) : ( szLetters == "" || szLetters == "b" ) )
			++nRight;
		else if ( szWrong.size() < 300 )
			szWrong += " " + szName;
	}
};

// Gap fix (M1 hand try): a winter map drew every unit in its summer paint -
// the 10.5-cm Flak38 tan instead of its 1w grey, the infantry in summer
// uniforms. CWorldBase::CreateMapObject hands the world's season to every map
// object it builds, and the world only learns the map's season from SetSeason,
// which the game calls before it builds a mission's objects
// (iMissionInternal.cpp:1495) and the MFC editor on every load
// (TemplateEditorFrame1.cpp:1683). The bridge never called it, so every world
// stayed CWorldBase's SEASON_SUMMER. Checked here on the winter map and on a
// summer one - opened in turn, so the second open has to undo the first's
// season - for the map's own units, a placed Flak38 and a placed squad, by the
// texture each of them is actually drawn with.
static void TestSeasonPicksTheVisuals( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	struct SSeasonCase { const char *pszMap; int nSeason; const char *pszSeasonName; const char *pszTag; };
	static const SSeasonCase cases[] = { { SHIPPED_MAP, 1, "Winter", "winter" }, { BRIDGE_MAP, 0, "Summer", "summer" } };
	static const char *const placed[] = { "10.5-cm_Flak38", "German_rifle_39" };
	static const char *const placedPaths[] = { "units\\technics\\german\\artillery\\10_5_cm_flak38\\", "units\\humans\\german\\" };
	int nCatalogue = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
	std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
	int nRead = 0;
	BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nRead );
	std::map<std::string, int> gameTypes;
	for ( int i = 0; i < nRead; ++i )
		gameTypes[catalogue[i].name] = catalogue[i].game_type;

	for ( int nCase = 0; nCase < 2; ++nCase )
	{
		const SSeasonCase &rCase = cases[nCase];
		BkEditorMapSummary summary;
		if ( !Check( BkEditorOpenMap( pSession, rCase.pszMap, &summary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			return;
		Check( summary.season == rCase.nSeason, NStr::Format( "%s is a %s map (season %d)", rCase.pszMap, rCase.pszTag, summary.season ) );
		const std::string szWorldSeason = GetGlobalVar( "World.Season", "<unset>" );
		Check( szWorldSeason == rCase.pszSeasonName,
		       NStr::Format( "the world is in %s's season: World.Season is \"%s\", want \"%s\"", rCase.pszMap, szWorldSeason.c_str(), rCase.pszSeasonName ) );

		CMapInfo map;
		std::string szError;
		if ( !Check( NMapFile::Read( rCase.pszMap, &map, &szError ), szError.c_str() ) || !Check( !map.objects.empty(), "the map has an object to look at" ) )
			return;

		// The map's own units and squads (game types 1 and 15), before anything
		// is placed: the camera on each of the first few in turn.
		SSeasonTally own;
		int nLooked = 0;
		const std::vector<SMapObjectInfo> *lists[2] = { &map.objects, &map.scenarioObjects };
		for ( int nList = 0; nList < 2 && nLooked < 6; ++nList )
			for ( size_t i = 0; i < lists[nList]->size() && nLooked < 6; ++i )
			{
				const SMapObjectInfo &rObject = ( *lists[nList] )[i];
				std::map<std::string, int>::const_iterator it = gameTypes.find( rObject.szName );
				BkEditorObjectState state;
				if ( it == gameTypes.end() || ( it->second != 1 && it->second != 15 ) ||
				     BkEditorEngineObjectState( pSession, rObject.link.nLinkID, &state ) != BK_EDITOR_OK )
					continue;
				++nLooked;
				CVec3 vAt;
				AI2Vis( &vAt, state.x, state.y, 0.0f );
				BkEditorSetCamera( pSession, vAt.x, vAt.y );
				BkEditorFrame( pSession );
				const std::vector<std::string> names = UnitTexturesOnScreen( nScreenWidth, nScreenHeight );
				for ( size_t j = 0; j < names.size(); ++j )
					own.Add( names[j], rCase.nSeason );
			}
		printf( "editor-bridge: %s (%s): the map's own units, %d looked at: %d unit pictures, %d in the season's textures%s%s\n",
		        rCase.pszMap, rCase.pszTag, nLooked, own.nUnits, own.nRight, own.szWrong.empty() ? "" : "; wrong:", own.szWrong.c_str() );
		Check( own.nUnits > 0 && own.nRight == own.nUnits,
		       NStr::Format( "%s's own units are drawn in %s textures (%d of %d)", rCase.pszMap, rCase.pszTag, own.nRight, own.nUnits ) );

		// Then a Flak38 and a squad placed on bare ground, found on a wider grid
		// than TestPlacedObjectDrawsAndPicks's: coldwinter's anchor stands in a
		// wood, with one bare patch in that test's 4x4 ring. A patch the engine
		// will not take an object on (arnheim has water on screen) is passed over.
		CVec3 vAnchor;
		AI2Vis( &vAnchor, map.objects[0].vPos );
		BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );
		BkEditorFrame( pSession );
		BkEditorFrame( pSession );
		std::vector<CVec2> spots;
		for ( int nTry = 0; nTry < 42; ++nTry )
		{
			const float fX = nScreenWidth / 2.0f + ( ( nTry % 7 ) - 3.0f ) * 80.0f;
			const float fY = nScreenHeight / 2.0f + ( ( nTry / 7 ) - 2.5f ) * 60.0f + PLACED_BOX_HEIGHT / 2;
			int nIgnored = -1;
			bool bClear = true;
			for ( int dy = 0; dy <= PLACED_BOX_HEIGHT && bClear; dy += 8 )
				for ( int dx = -PLACED_BOX_HALF_WIDTH; dx <= PLACED_BOX_HALF_WIDTH && bClear; dx += 8 )
					bClear = BkEditorObjectAt( pSession, fX + dx, fY - dy, &nIgnored ) != BK_EDITOR_OK;
			if ( bClear )
				spots.push_back( CVec2( fX, fY ) );
		}
		const std::string szBefore = szScratch + NStr::Format( "/editor-bridge-season-%s-before.tga", rCase.pszTag );
		if ( !SaveFrame( pSession, szBefore ) )
			return;
		CVec2 vPlacedAt[2];
		size_t nSpot = 0;
		for ( int i = 0; i < 2; ++i )
		{
			bool bPlaced = false;
			for ( ; nSpot < spots.size() && !bPlaced; ++nSpot )
			{
				if ( i == 1 && fabs( spots[nSpot].x - vPlacedAt[0].x ) + fabs( spots[nSpot].y - vPlacedAt[0].y ) < 90.0f )
					continue;
				float wx = 0.0f, wy = 0.0f, mx = 0.0f, my = 0.0f;
				int nLinkID = -1;
				bPlaced = BkEditorScreenToWorld( pSession, spots[nSpot].x, spots[nSpot].y, &wx, &wy ) == BK_EDITOR_OK &&
				          BkEditorWorldToMap( pSession, wx, wy, &mx, &my ) == BK_EDITOR_OK &&
				          BkEditorAddObject( pSession, placed[i], mx, my, 0, 0, &nLinkID ) == BK_EDITOR_OK;
				if ( bPlaced )
					vPlacedAt[i] = spots[nSpot];
			}
			if ( !Check( bPlaced, NStr::Format( "%s is placed on bare ground on %s (%d patches found)", placed[i], rCase.pszMap, int( spots.size() ) ) ) )
				return;
		}
		for ( int i = 0; i < 4; ++i )
			BkEditorFrame( pSession );
		const std::string szAfter = szScratch + NStr::Format( "/editor-bridge-season-%s.tga", rCase.pszTag );

		// The Flak38's colour as drawn, for a person reading the log: the mean of
		// the pixels its placing changed. Its textures average 55,48,33 (tan,
		// 1_c.dds) and 94,95,90 (grey, 1w_c.dds).
		if ( SaveFrame( pSession, szAfter ) )
		{
			int nWidth = 0, nHeight = 0;
			const std::vector<unsigned char> before = ReadFramePixels( szBefore, &nWidth, &nHeight );
			const std::vector<unsigned char> after = ReadFramePixels( szAfter, &nWidth, &nHeight );
			double fSum[3] = { 0, 0, 0 };
			int nChanged = 0;
			if ( before.size() == after.size() && after.size() >= size_t( nWidth ) * nHeight * 4 )
				for ( int y = Max( 0, int( vPlacedAt[0].y ) - 80 ); y < Min( nHeight, int( vPlacedAt[0].y ) + 16 ); ++y )
					for ( int x = Max( 0, int( vPlacedAt[0].x ) - 40 ); x < Min( nWidth, int( vPlacedAt[0].x ) + 40 ); ++x )
					{
						const size_t n = ( size_t( y ) * nWidth + x ) * 4;
						if ( abs( int( before[n] ) - after[n] ) + abs( int( before[n + 1] ) - after[n + 1] ) + abs( int( before[n + 2] ) - after[n + 2] ) <= 48 )
							continue;
						// The TGA holds BGRA.
						fSum[0] += after[n + 2];
						fSum[1] += after[n + 1];
						fSum[2] += after[n];
						++nChanged;
					}
			if ( nChanged > 0 )
				printf( "editor-bridge: on %s the placed Flak38 at %.0f,%.0f is drawn at mean RGB %.0f,%.0f,%.0f (%d pixels, %s)\n", rCase.pszMap,
				        vPlacedAt[0].x, vPlacedAt[0].y, fSum[0] / nChanged, fSum[1] / nChanged, fSum[2] / nChanged, nChanged, szAfter.c_str() );
		}

		const std::vector<std::string> names = UnitTexturesOnScreen( nScreenWidth, nScreenHeight );
		SSeasonTally placedTally[2];
		for ( size_t i = 0; i < names.size(); ++i )
			for ( int j = 0; j < 2; ++j )
				if ( names[i].compare( 0, strlen( placedPaths[j] ), placedPaths[j] ) == 0 )
					placedTally[j].Add( names[i], rCase.nSeason );
		printf( "editor-bridge: %s (%s): placed Flak38 %d/%d, placed infantry %d/%d in the season's textures%s%s%s\n",
		        rCase.pszMap, rCase.pszTag, placedTally[0].nRight, placedTally[0].nUnits, placedTally[1].nRight, placedTally[1].nUnits,
		        placedTally[0].szWrong.empty() && placedTally[1].szWrong.empty() ? "" : "; wrong:", placedTally[0].szWrong.c_str(), placedTally[1].szWrong.c_str() );
		Check( placedTally[0].nUnits > 0 && placedTally[0].nRight == placedTally[0].nUnits,
		       NStr::Format( "the placed Flak38 on %s is drawn in %s textures (%d of %d)", rCase.pszMap, rCase.pszTag, placedTally[0].nRight, placedTally[0].nUnits ) );
		Check( placedTally[1].nUnits > 0 && placedTally[1].nRight == placedTally[1].nUnits,
		       NStr::Format( "the placed infantry on %s is drawn in %s textures (%d of %d)", rCase.pszMap, rCase.pszTag, placedTally[1].nRight, placedTally[1].nUnits ) );
	}
}

// Only the meshes' textures: a unit's icons are sprites, and some of them
// (an icon with nothing to show) are visited without a texture.
class CMeshTextureNameVisitor : public CTextureNameVisitor
{
public:
	virtual void STDCALL VisitSprite( const SBasicSpriteInfo *pObj, int nType, int nPriority ) {  }
};

// The mesh textures of the units a pick of a screen box answers with.
static std::vector<std::string> UnitMeshTexturesIn( const CTRect<float> &rcBox )
{
	CMeshTextureNameVisitor visitor;
	IScene *pScene = GetSingleton<IScene>();
	if ( pScene == 0 )
		return visitor.names;
	std::pair<IVisObj*, CVec2> *pObjects = 0;
	int nCount = 0;
	pScene->Pick( rcBox, &pObjects, &nCount, SGVOGT_UNIT );
	for ( int i = 0; i < nCount; ++i )
		if ( pObjects[i].first != 0 )
			pObjects[i].first->Visit( &visitor );
	return visitor.names;
}

// Gap fix (M1 hand try): on a winter map a unit whose folder has no winter
// texture was drawn pure white, in the editor and in the game. 83 of the 242
// unit mesh folders have no 1w (the 105-mm M2A1 has only 1_c/_h/_l.dds), and
// the unit asks for "<path>\1w" (MOUnitMechanical.cpp). The original DX8 GFX
// drew a checker for a missing file; GFXGPU's texture manager answers null,
// and a mesh with no texture is drawn white. CVisObjBuilder now falls back to
// the season-less name. Checked on coldwinter by the texture each placed gun
// is drawn with: the M2A1 in its summer "1", the Flak38 still in its "1w" -
// and by the M2A1's colour as drawn, which must not be near white.
static void TestMissingSeasonTextureFallsBack( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	static const char *const placed[] = { "105mm_M2A1_USA", "10.5-cm_Flak38" };
	static const char *const wanted[] = { "units\\technics\\allies\\artillery\\105mm_m2a1_usa\\1",
	                                      "units\\technics\\german\\artillery\\10_5_cm_flak38\\1w" };
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) || !Check( !map.objects.empty(), "the map has an object to look at" ) )
		return;
	CVec3 vAnchor;
	AI2Vis( &vAnchor, map.objects[0].vPos );
	BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );
	BkEditorFrame( pSession );
	BkEditorFrame( pSession );
	// Bare ground, found as TestSeasonPicksTheVisuals finds it.
	std::vector<CVec2> spots;
	for ( int nTry = 0; nTry < 42; ++nTry )
	{
		const float fX = nScreenWidth / 2.0f + ( ( nTry % 7 ) - 3.0f ) * 80.0f;
		const float fY = nScreenHeight / 2.0f + ( ( nTry / 7 ) - 2.5f ) * 60.0f + PLACED_BOX_HEIGHT / 2;
		int nIgnored = -1;
		bool bClear = true;
		for ( int dy = 0; dy <= PLACED_BOX_HEIGHT && bClear; dy += 8 )
			for ( int dx = -PLACED_BOX_HALF_WIDTH; dx <= PLACED_BOX_HALF_WIDTH && bClear; dx += 8 )
				bClear = BkEditorObjectAt( pSession, fX + dx, fY - dy, &nIgnored ) != BK_EDITOR_OK;
		if ( bClear )
			spots.push_back( CVec2( fX, fY ) );
	}
	const std::string szBefore = szScratch + "/editor-bridge-season-fallback-before.tga";
	if ( !SaveFrame( pSession, szBefore ) )
		return;
	CVec2 vPlacedAt[2];
	size_t nSpot = 0;
	for ( int i = 0; i < 2; ++i )
	{
		bool bPlaced = false;
		for ( ; nSpot < spots.size() && !bPlaced; ++nSpot )
		{
			if ( i == 1 && fabs( spots[nSpot].x - vPlacedAt[0].x ) + fabs( spots[nSpot].y - vPlacedAt[0].y ) < 160.0f )
				continue;
			float wx = 0.0f, wy = 0.0f, mx = 0.0f, my = 0.0f;
			int nLinkID = -1;
			bPlaced = BkEditorScreenToWorld( pSession, spots[nSpot].x, spots[nSpot].y, &wx, &wy ) == BK_EDITOR_OK &&
			          BkEditorWorldToMap( pSession, wx, wy, &mx, &my ) == BK_EDITOR_OK &&
			          BkEditorAddObject( pSession, placed[i], mx, my, 0, 0, &nLinkID ) == BK_EDITOR_OK;
			if ( bPlaced )
				vPlacedAt[i] = spots[nSpot];
		}
		if ( !Check( bPlaced, NStr::Format( "%s is placed on bare ground on %s (%d patches found)", placed[i], SHIPPED_MAP, int( spots.size() ) ) ) )
			return;
	}
	for ( int i = 0; i < 4; ++i )
		BkEditorFrame( pSession );
	const std::string szAfter = szScratch + "/editor-bridge-season-fallback.tga";

	for ( int i = 0; i < 2; ++i )
	{
		const CTRect<float> rcBox( vPlacedAt[i].x - 40.0f, vPlacedAt[i].y - 80.0f, vPlacedAt[i].x + 40.0f, vPlacedAt[i].y + 16.0f );
		const std::vector<std::string> names = UnitMeshTexturesIn( rcBox );
		bool bAllWanted = !names.empty();
		std::string szNames;
		for ( size_t j = 0; j < names.size(); ++j )
		{
			bAllWanted = bAllWanted && names[j] == wanted[i];
			if ( szNames.size() < 300 )
				szNames += " " + names[j];
		}
		printf( "editor-bridge: on %s the placed %s at %.0f,%.0f is drawn with:%s\n", SHIPPED_MAP, placed[i], vPlacedAt[i].x, vPlacedAt[i].y, szNames.c_str() );
		Check( bAllWanted, NStr::Format( "the placed %s on %s is drawn with %s (drawn with:%s)", placed[i], SHIPPED_MAP, wanted[i], szNames.c_str() ) );
	}

	// The M2A1's colour as drawn: the mean of the pixels its placing changed.
	// Without a texture it is drawn white.
	if ( SaveFrame( pSession, szAfter ) )
	{
		int nWidth = 0, nHeight = 0;
		const std::vector<unsigned char> before = ReadFramePixels( szBefore, &nWidth, &nHeight );
		const std::vector<unsigned char> after = ReadFramePixels( szAfter, &nWidth, &nHeight );
		double fSum[3] = { 0, 0, 0 };
		int nChanged = 0;
		if ( before.size() == after.size() && after.size() >= size_t( nWidth ) * nHeight * 4 )
			for ( int y = Max( 0, int( vPlacedAt[0].y ) - 80 ); y < Min( nHeight, int( vPlacedAt[0].y ) + 16 ); ++y )
				for ( int x = Max( 0, int( vPlacedAt[0].x ) - 40 ); x < Min( nWidth, int( vPlacedAt[0].x ) + 40 ); ++x )
				{
					const size_t n = ( size_t( y ) * nWidth + x ) * 4;
					if ( abs( int( before[n] ) - after[n] ) + abs( int( before[n + 1] ) - after[n + 1] ) + abs( int( before[n + 2] ) - after[n + 2] ) <= 48 )
						continue;
					// The TGA holds BGRA.
					fSum[0] += after[n + 2];
					fSum[1] += after[n + 1];
					fSum[2] += after[n];
					++nChanged;
				}
		const double fR = nChanged > 0 ? fSum[0] / nChanged : 0.0, fG = nChanged > 0 ? fSum[1] / nChanged : 0.0, fB = nChanged > 0 ? fSum[2] / nChanged : 0.0;
		printf( "editor-bridge: on %s the placed M2A1 at %.0f,%.0f is drawn at mean RGB %.0f,%.0f,%.0f (%d pixels, %s)\n", SHIPPED_MAP,
		        vPlacedAt[0].x, vPlacedAt[0].y, fR, fG, fB, nChanged, szAfter.c_str() );
		Check( nChanged >= PLACED_MIN_CHANGED && Min( fR, Min( fG, fB ) ) < 180.0,
		       NStr::Format( "the placed M2A1 is drawn, and not white (mean RGB %.0f,%.0f,%.0f over %d pixels)", fR, fG, fB, nChanged ) );
	}
}

// D-12: what the renderer actually draws when the camera is placed at yaw
// offsets other than the game's own 45, measured rather than guessed. The
// terrain is laid out on a fixed isometric screen grid
// (Scene/TerrainInternal.cpp, CTerrain::MovePatches) and buildings/infantry
// are single-direction sprites (Main/GameDB.h), so only offset 0 is known
// good against today's numbers (TestTerrainUnderTheCamera's bar and
// TestObjectUnderTheCursor's half-picked bar); the rest is printed and left
// to the plan's checkpoint decision. The camera's anchor is set once, before
// the loop, so only the yaw changes between the five captures.
static const int YAW_MEASURE_OFFSETS[] = { 0, 30, 90, 180, 270 };

static void TestYawMeasurement( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	Check( BkEditorSetYaw( pSession, std::numeric_limits<float>::quiet_NaN() ) == BK_EDITOR_BAD_ARGUMENT,
	       "a non-finite yaw is a bad argument" );

	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) || !Check( !map.objects.empty(), "the map has an object to look at" ) )
		return;

	CVec3 vAnchor;
	AI2Vis( &vAnchor, map.objects[0].vPos );
	BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );

	// Up to 30 known objects, by the engine's own position for each (the same
	// source TestObjectUnderTheCursor reads), gathered once at yaw 0 so the
	// same set is checked at every offset.
	struct SKnownObject { int nLinkID; float wx, wy; };
	std::vector<SKnownObject> known;
	for ( size_t i = 0; i < map.objects.size() && known.size() < 30; ++i )
	{
		BkEditorObjectState state;
		if ( BkEditorEngineObjectState( pSession, map.objects[i].link.nLinkID, &state ) != BK_EDITOR_OK )
			continue;
		CVec3 vWorld;
		AI2Vis( &vWorld, state.x, state.y, 0.0f );
		SKnownObject known_object;
		known_object.nLinkID = map.objects[i].link.nLinkID;
		known_object.wx = vWorld.x;
		known_object.wy = vWorld.y;
		known.push_back( known_object );
	}
	Check( !known.empty(), "there are known objects to measure against" );

	for ( size_t nOffsetIdx = 0; nOffsetIdx < sizeof( YAW_MEASURE_OFFSETS ) / sizeof( YAW_MEASURE_OFFSETS[0] ); ++nOffsetIdx )
	{
		const int nOffset = YAW_MEASURE_OFFSETS[nOffsetIdx];
		if ( !Check( BkEditorSetYaw( pSession, float( nOffset ) ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			continue;
		BkEditorView view;
		if ( Check( BkEditorViewState( pSession, &view ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( fabsf( view.yaw_degrees - ( 45.0f + float( nOffset ) ) ) <= 0.01f,
			       NStr::Format( "yaw +%d: the view reports 45+offset (%.1f)", nOffset, view.yaw_degrees ) );

		BkEditorFrame( pSession );
		BkEditorFrame( pSession );
		const std::string szFrame = szScratch + NStr::Format( "/editor-bridge-yaw-%d.tga", nOffset );
		SaveFrame( pSession, szFrame );

		int nFrameWidth = 0, nFrameHeight = 0;
		const std::vector<unsigned char> pixels = ReadFramePixels( szFrame, &nFrameWidth, &nFrameHeight );
		float fBlack = 1.0f;
		if ( !pixels.empty() )
		{
			int nBlack = 0, nCounted = 0;
			for ( int y = nFrameHeight / 2; y < nFrameHeight; ++y )
				for ( int x = 0; x < nFrameWidth; ++x, ++nCounted )
				{
					const unsigned char *p = &pixels[( size_t( y ) * nFrameWidth + x ) * 4];
					if ( p[0] < 16 && p[1] < 16 && p[2] < 16 )
						++nBlack;
				}
			fBlack = nCounted > 0 ? float( nBlack ) / nCounted : 1.0f;
		}

		// (b): how many of the known objects still land on screen at this
		// yaw. (a): of those, how many BkEditorObjectAt still finds at the
		// same screen point. (c): of those, whether the terrain the pick
		// solves against (BkEditorScreenToWorld -> BkEditorWorldToTile)
		// agrees with the object's own tile (BkEditorWorldToTile of its own
		// world position) - the ground and the object staying together.
		int nOnScreen = 0, nPicked = 0, nTerrainAgrees = 0;
		for ( size_t i = 0; i < known.size(); ++i )
		{
			float sx = 0.0f, sy = 0.0f;
			if ( BkEditorWorldToScreen( pSession, known[i].wx, known[i].wy, &sx, &sy ) != BK_EDITOR_OK )
				continue;
			if ( sx < 0.0f || sy < 0.0f || sx >= float( nScreenWidth ) || sy >= float( nScreenHeight ) )
				continue;
			++nOnScreen;
			int nLinkID = -1;
			if ( BkEditorObjectAt( pSession, sx, sy - PICK_RISE, &nLinkID ) == BK_EDITOR_OK && nLinkID == known[i].nLinkID )
				++nPicked;
			int nOwnTileX = -1, nOwnTileY = -1;
			float wx = 0.0f, wy = 0.0f;
			int nPickedTileX = -1, nPickedTileY = -1;
			if ( BkEditorWorldToTile( pSession, known[i].wx, known[i].wy, &nOwnTileX, &nOwnTileY ) == BK_EDITOR_OK &&
			     BkEditorScreenToWorld( pSession, sx, sy, &wx, &wy ) == BK_EDITOR_OK &&
			     BkEditorWorldToTile( pSession, wx, wy, &nPickedTileX, &nPickedTileY ) == BK_EDITOR_OK &&
			     nOwnTileX == nPickedTileX && nOwnTileY == nPickedTileY )
				++nTerrainAgrees;
		}

		printf( "editor-bridge: yaw +%d: black %.1f%%, picked %d/%d, terrain agrees %d/%d\n",
		        nOffset, fBlack * 100.0f, nPicked, nOnScreen, nTerrainAgrees, nOnScreen );

		// Only offset 0 is a pass/fail gate: today's known-good behaviour. The
		// rest is measurement for the checkpoint, not an assertion - the
		// renderer is not expected to be correct at an untested yaw.
		if ( nOffset == 0 )
		{
			Check( fBlack < TERRAIN_BLACK_BAR, NStr::Format( "yaw +0: the ground is drawn under the camera (%.1f%% black, bar %.0f%%)", fBlack * 100.0f, TERRAIN_BLACK_BAR * 100.0f ) );
			Check( nOnScreen > 0 && nPicked * 2 >= nOnScreen,
			       NStr::Format( "yaw +0: at least half the on-screen objects are picked (%d/%d)", nPicked, nOnScreen ) );
			Check( nOnScreen > 0 && nTerrainAgrees * 10 >= nOnScreen * 9,
			       NStr::Format( "yaw +0: at least 90%% of the terrain picks agree (%d/%d)", nTerrainAgrees, nOnScreen ) );
		}
	}

	// Reset to 0 so the tests that run after this one see the game's own yaw.
	Check( BkEditorSetYaw( pSession, 0.0f ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
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
// Data is read-only for every tier. Nothing is needed beside the map -
// CTerrain::LoadLocal keeps the path only as a name and takes the tileset,
// crosset and roadset from storage (TerrainInternal.cpp:88-110).
//
// Left in place on purpose, not removed at the end: the app tier's own host
// check (03-11's panelSmoke, main.zig) opens this exact file afterward to
// prove its unknown-objects warning against a real Open, found via
// dirname(output) (main.zig's own comment). A `remove()` here used to run
// unconditionally, and its success is platform-dependent for a path built
// with a literal backslash: POSIX `remove()` (macOS) does not treat '\' as a
// separator, so the delete silently failed there and the file stayed,
// while on Windows it succeeded and removed the very fixture the host check
// needed - the check ran (PASS) on one platform and printed "skipped" on the
// other for a reason that had nothing to do with whether the warning itself
// worked. Keeping the file unconditionally makes the two tiers agree on
// every platform instead of one depending on the other's cleanup rules.
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
}

// A tile the open map's tileset actually offers (BkEditorTilesetTiles),
// different from current. The older paint tests used to guess
// (current+1)%4, which depends on the map: tile 1 is in none of the shipped
// tilesets (TestPaintRefusesTileOutsideTileset's own comment), so a current
// tile of 0 wrapped a guess straight into a tile the paint would refuse
// instead of the good, different tile the test wanted (Task 3 carried).
// Falls back to current itself if the tileset somehow offers nothing else,
// so a caller's own Check still names a sensible failure rather than an
// empty read silently picking tile 0.
static unsigned char OtherTilesetTile( BkEditorSession *pSession, unsigned char current )
{
	int nCount = 0;
	BkEditorTilesetTiles( pSession, 0, 0, &nCount );
	std::vector<unsigned char> tiles( size_t( nCount > 0 ? nCount : 1 ) );
	int nRead = 0;
	BkEditorTilesetTiles( pSession, &tiles[0], nCount, &nRead );
	for ( int i = 0; i < nRead; ++i )
		if ( tiles[i] != current )
			return tiles[i];
	return current;
}

// A paint undone is the map as it was, in the file and in the engine; redone,
// it is the paint again. Out of order is refused.
static void TestPaintUndoIsExact( BkEditorSession *pSession, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	const unsigned char tile = OtherTilesetTile( pSession, original.terrain.tiles[20][20].tile );
	BkEditorPaintCell first[] = { { 20, 20, tile }, { 21, 20, tile } };
	BkEditorPaintCell second[] = { { 22, 20, tile } };
	int nFirst = -1, nSecond = -1;
	Check( BkEditorPaint( pSession, first, 2, &nFirst ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorPaint( pSession, second, 1, &nSecond ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( nFirst >= 0 && nSecond >= 0 && nFirst != nSecond, "each paint has its own token" );
	const std::string szPainted = szScratch + "\\undo-painted.bzm";
	Check( BkEditorSaveMap( pSession, szPainted.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	Check( BkEditorUndoPaint( pSession, nFirst ) == BK_EDITOR_REFUSED, "the older paint is not undone first" );
	Check( BkEditorUndoPaint( pSession, nSecond ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorUndoPaint( pSession, nFirst ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	const std::string szUndone = szScratch + "\\undo-undone.bzm";
	CMapInfo undone;
	std::string szWhere;
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szUndone.c_str(), &undone, &szError ), szError.c_str() ) )
		Check( NMapFile::AreEquivalent( original, undone, &szWhere ), ( "two paints undone are the original: " + szWhere ).c_str() );

	Check( BkEditorRedoPaint( pSession, nSecond ) == BK_EDITOR_REFUSED, "redo takes the most recently undone first" );
	Check( BkEditorRedoPaint( pSession, nFirst ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorRedoPaint( pSession, nSecond ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	CMapInfo painted, redone;
	const std::string szRedone = szScratch + "\\undo-redone.bzm";
	szWhere.clear();
	if ( Check( BkEditorSaveMap( pSession, szRedone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szPainted.c_str(), &painted, &szError ), szError.c_str() ) &&
	     Check( NMapFile::Read( szRedone.c_str(), &redone, &szError ), szError.c_str() ) )
		Check( NMapFile::AreEquivalent( painted, redone, &szWhere ), ( "redone is the paint, crosses and all: " + szWhere ).c_str() );

	// A new paint ends the redo branch.
	Check( BkEditorUndoPaint( pSession, nSecond ) == BK_EDITOR_OK, "undo once more" );
	int nThird = -1;
	Check( BkEditorPaint( pSession, second, 1, &nThird ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorRedoPaint( pSession, nSecond ) == BK_EDITOR_REFUSED, "a paint after an undo drops what could be redone" );
	remove( szPainted.c_str() );
	remove( szUndone.c_str() );
	remove( szRedone.c_str() );
}

// The far corner and a refusal. A cell in the last patch row and column is the
// one InclusivePatches and RegionTiles exist for: handed the half-open region,
// CTerrain::Update would run one patch past the end of the map. A paint with
// one cell off the map is refused before the engine is touched, so the cell
// that was on it is not painted there either.
static void TestPaintAtTheEdgeAndRefused( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int nSizeX = original.terrain.tiles.GetSizeX(), nSizeY = original.terrain.tiles.GetSizeY();
	const BkEditorPaintCell corner = { nSizeX - 1, nSizeY - 1,
	                                   OtherTilesetTile( pSession, original.terrain.tiles[nSizeY - 1][nSizeX - 1].tile ) };
	int nToken = -1;
	Check( BkEditorPaint( pSession, &corner, 1, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK,
	       ( std::string( "a paint in the last patch row and column: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorUndoPaint( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK,
	       ( std::string( "and its undo: " ) + BkEditorLastMessage( pSession ) ).c_str() );

	const BkEditorPaintCell partly[] = {
		{ 5, 5, OtherTilesetTile( pSession, original.terrain.tiles[5][5].tile ) },
		{ nSizeX, 5, 0 },
	};
	nToken = 0;
	Check( BkEditorPaint( pSession, partly, 2, &nToken ) == BK_EDITOR_REFUSED, "a paint with a cell off the map is refused" );
	Check( nToken == -1, "and has no token" );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK,
	       ( std::string( "and the engine did not keep the cell that was on the map: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	const std::string szSaved = szScratch + "\\edge-saved.bzm";
	CMapInfo saved;
	std::string szWhere;
	if ( Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		Check( NMapFile::AreEquivalent( original, saved, &szWhere ), ( "an undone paint and a refused one leave the map as read: " + szWhere ).c_str() );
	remove( szSaved.c_str() );
}

// A tile the map's tileset has no terrain type for is the caller's mistake:
// BK_EDITOR_BAD_ARGUMENT, naming the tile, and nothing painted - not even the
// cells beside it that name a good tile. Tile 1 is in none of the shipped
// tilesets; 255 is what a caller's -1 becomes in the cell's unsigned char.
static void TestPaintRefusesTileOutsideTileset( BkEditorSession *pSession )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	unsigned char before[2] = { 0, 0 };
	if ( !Check( BkEditorEngineTile( pSession, 30, 30, &before[0] ) == BK_EDITOR_OK &&
	             BkEditorEngineTile( pSession, 31, 30, &before[1] ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const unsigned char badTiles[] = { 1, (unsigned char)-1 };
	for ( int i = 0; i < 2; ++i )
	{
		// The good cell first, so a check that stopped at it would paint it.
		const BkEditorPaintCell cells[] = { { 30, 30, (unsigned char)( before[0] == 0 ? 2 : 0 ) }, { 31, 30, badTiles[i] } };
		int nToken = 0;
		const std::string szWhat = NStr::Format( "tile %d", int( badTiles[i] ) );
		Check( BkEditorPaint( pSession, cells, 2, &nToken ) == BK_EDITOR_BAD_ARGUMENT, ( "a paint naming " + szWhat + " outside the tileset is a bad argument" ).c_str() );
		Check( nToken == -1, "and has no token" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( szWhat ) != std::string::npos,
		       ( std::string( "and the message names the tile: " ) + BkEditorLastMessage( pSession ) ).c_str() );
		unsigned char after[2] = { 0, 0 };
		BkEditorEngineTile( pSession, 30, 30, &after[0] );
		BkEditorEngineTile( pSession, 31, 30, &after[1] );
		Check( after[0] == before[0] && after[1] == before[1], "and the engine's tiles are as they were" );
		Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK,
		       ( std::string( "and the map's terrain still matches the engine's: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	}
}

// The brush's palette: every tile BkEditorTilesetTiles hands out paints, and
// tile 1, which no shipped tileset has, is not among them. The count comes
// first with no buffer, and a buffer one short is refused with nothing
// written past it.
static void TestTilesetTilesAllPaint( BkEditorSession *pSession )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nCount = -1;
	if ( !Check( BkEditorTilesetTiles( pSession, 0, 0, &nCount ) == BK_EDITOR_REFUSED && nCount > 1,
	             NStr::Format( "the tileset's tile count comes back with no buffer (%d): %s", nCount, BkEditorLastMessage( pSession ) ) ) )
		return;
	std::vector<unsigned char> tiles( size_t( nCount ) + 1, 0xAB );
	int nShort = -1;
	Check( BkEditorTilesetTiles( pSession, &( tiles[0] ), nCount - 1, &nShort ) == BK_EDITOR_REFUSED && nShort == nCount,
	       "a buffer one short is refused and still told the total" );
	Check( tiles[size_t( nCount ) - 1] == 0xAB && tiles[size_t( nCount )] == 0xAB, "and nothing is written past its capacity" );
	int nRead = -1;
	if ( !Check( BkEditorTilesetTiles( pSession, &( tiles[0] ), nCount, &nRead ) == BK_EDITOR_OK && nRead == nCount,
	             BkEditorLastMessage( pSession ) ) )
		return;
	bool bAscending = true;
	for ( int i = 1; i < nCount; ++i )
		bAscending = bAscending && tiles[i - 1] < tiles[i];
	Check( bAscending, "the tiles come once each, ascending" );
	bool bHasOne = false;
	for ( int i = 0; i < nCount; ++i )
		bHasOne = bHasOne || tiles[i] == 1;
	Check( !bHasOne, "tile 1, in no shipped tileset, is not offered" );
	int nPainted = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		const BkEditorPaintCell cell = { 30, 30, tiles[i] };
		int nToken = -1;
		const BkEditorStatus painted = BkEditorPaint( pSession, &cell, 1, &nToken );
		if ( Check( painted == BK_EDITOR_OK, NStr::Format( "tileset tile %d paints: %s", int( tiles[i] ), BkEditorLastMessage( pSession ) ) ) )
		{
			++nPainted;
			BkEditorUndoPaint( pSession, nToken );
		}
	}
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK,
	       ( std::string( "and after painting each and undoing it the terrain still matches: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	printf( "editor-bridge: the tileset of %s offers %d tiles, %d painted\n", SHIPPED_MAP, nCount, nPainted );
}

// Delete then restore is the original object, in the map and in the engine,
// and add - delete - restore keeps the added object's link ID.
static void TestDeleteRestoreKeepsTheObject( BkEditorSession *pSession, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;

	int nLinkID = -1;
	BkEditorObjectState before;
	for ( size_t i = 0; i < original.objects.size() && nLinkID < 0; ++i )
	{
		const int nCandidate = original.objects[i].link.nLinkID;
		if ( BkEditorEngineObjectState( pSession, nCandidate, &before ) != BK_EDITOR_OK )
			continue;		// not placed
		if ( BkEditorDeleteObject( pSession, nCandidate ) == BK_EDITOR_OK )
			nLinkID = nCandidate;
	}
	if ( !Check( nLinkID >= 0, "some placed object can be deleted" ) )
		return;
	// The engine's link table too, which only a Windows debug build's assert
	// ("Repeated link" in CLinkObject::SetLink) caught before: a deleted
	// object's ID must name nothing, so that the restore can register it again,
	// and after the restore it must name the restored object.
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "after a delete: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorRestoreObject( pSession, nLinkID ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "after its restore: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	BkEditorObjectState after;
	Check( BkEditorEngineObjectState( pSession, nLinkID, &after ) == BK_EDITOR_OK &&
	       after.x == before.x && after.y == before.y && after.dir == before.dir && after.player == before.player,
	       "the engine holds it again, where it was" );
	const std::string szSaved = szScratch + "\\restore-saved.bzm";
	CMapInfo saved;
	std::string szWhere;
	if ( Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		Check( NMapFile::AreEquivalent( original, saved, &szWhere ), ( "delete then restore is the original map: " + szWhere ).c_str() );
	Check( BkEditorRestoreObject( pSession, nLinkID ) == BK_EDITOR_REFUSED, "nothing to restore twice" );

	// The link-ID hazard: delete the highest ID, add, then undo both. The add
	// must not have been handed the deleted object's ID.
	int nHighest = -1;
	for ( size_t i = 0; i < original.objects.size(); ++i )
		nHighest = Max( nHighest, original.objects[i].link.nLinkID );
	for ( size_t i = 0; i < original.scenarioObjects.size(); ++i )
		nHighest = Max( nHighest, original.scenarioObjects[i].link.nLinkID );
	if ( Check( BkEditorDeleteObject( pSession, nHighest ) == BK_EDITOR_OK, "the object with the highest link ID deletes" ) )
	{
		BkEditorObjectState anywhere;
		BkEditorEngineObjectState( pSession, nLinkID, &anywhere );
		int nAdded = -1;
		if ( Check( BkEditorAddObject( pSession, original.objects[0].szName.c_str(), anywhere.x + 64, anywhere.y, 0, 0, &nAdded ) == BK_EDITOR_OK,
		            BkEditorLastMessage( pSession ) ) )
		{
			Check( nAdded != nHighest, "an add never reuses a deleted object's link ID" );
			Check( BkEditorDeleteObject( pSession, nAdded ) == BK_EDITOR_OK, "undo the add" );
			Check( BkEditorRestoreObject( pSession, nHighest ) == BK_EDITOR_OK, "undo the delete" );
			Check( BkEditorRestoreObject( pSession, nAdded ) == BK_EDITOR_OK && BkEditorDeleteObject( pSession, nAdded ) == BK_EDITOR_OK,
			       "and the added object's own restore still finds it" );
			Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "after the add and the deletes undone: " ) + BkEditorLastMessage( pSession ) ).c_str() );
		}
	}
	remove( szSaved.c_str() );
}

// A squad deletes and comes back. Its engine object is a formation, which
// IAIEditor::DeleteObject does not know (it asserts "Unknown object"), so the
// bridge deletes it soldier by soldier as the MFC editor does. Picking now
// answers a soldier with his squad, so this is the delete a click leads to.
static void TestSquadDeletesAndRestores( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nCatalogue = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
	std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
	int nRead = 0;
	BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nRead );

	// The first placed squad (game type 15, SGVOGT_SQUAD) the map lets go of.
	const std::vector<SMapObjectInfo> *lists[2] = { &original.objects, &original.scenarioObjects };
	int nSquad = -1, nSquads = 0;
	BkEditorObjectState before;
	bool bPickedBefore = false;
	for ( int nList = 0; nList < 2 && nSquad < 0; ++nList )
		for ( size_t i = 0; i < lists[nList]->size() && nSquad < 0; ++i )
		{
			const SMapObjectInfo &rObject = ( *lists[nList] )[i];
			bool bSquad = false;
			for ( int j = 0; j < nRead && !bSquad; ++j )
				bSquad = catalogue[j].game_type == 15 && rObject.szName == catalogue[j].name;
			if ( !bSquad || BkEditorEngineObjectState( pSession, rObject.link.nLinkID, &before ) != BK_EDITOR_OK )
				continue;
			++nSquads;
			// Whether a click on the squad answers with it, for the picture half
			// of the check below.
			CVec3 vAnchor;
			AI2Vis( &vAnchor, before.x, before.y, 0.0f );
			BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );
			BkEditorFrame( pSession );
			int nPicked = -1;
			bPickedBefore = BkEditorObjectAt( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f - PICK_RISE, &nPicked ) == BK_EDITOR_OK &&
			                nPicked == rObject.link.nLinkID;
			if ( BkEditorDeleteObject( pSession, rObject.link.nLinkID ) == BK_EDITOR_OK )
				nSquad = rObject.link.nLinkID;
		}
	printf( "editor-bridge: squad %d deleted (%d squads tried), picked by a click before: %s\n", nSquad, nSquads, bPickedBefore ? "yes" : "no" );
	if ( !Check( nSquad >= 0, "a placed squad can be deleted" ) )
		return;
	BkEditorObjectState gone;
	Check( BkEditorEngineObjectState( pSession, nSquad, &gone ) == BK_EDITOR_REFUSED, "the engine no longer holds the squad" );
	// The picture half: no soldier of the deleted squad is still drawn.
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "after the squad's delete: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	BkEditorFrame( pSession );
	int nPicked = -1;
	Check( !( BkEditorObjectAt( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f - PICK_RISE, &nPicked ) == BK_EDITOR_OK && nPicked == nSquad ),
	       "and a click where it stood no longer answers with it" );

	if ( !Check( BkEditorRestoreObject( pSession, nSquad ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorObjectState after;
	Check( BkEditorEngineObjectState( pSession, nSquad, &after ) == BK_EDITOR_OK &&
	       after.x == before.x && after.y == before.y && after.dir == before.dir,
	       "the engine holds the squad again, where it was" );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "after the squad's restore: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	if ( bPickedBefore )
	{
		BkEditorFrame( pSession );
		nPicked = -1;
		Check( BkEditorObjectAt( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f - PICK_RISE, &nPicked ) == BK_EDITOR_OK && nPicked == nSquad,
		       "and a click on it answers with it again" );
	}
	const std::string szSaved = szScratch + "\\squad-restore-saved.bzm";
	CMapInfo saved;
	std::string szWhere;
	if ( Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		Check( NMapFile::AreEquivalent( original, saved, &szWhere ), ( "a squad's delete then restore is the original map: " + szWhere ).c_str() );
	remove( szSaved.c_str() );
}

// A broken map is rejected as a whole and the map that was open stays open:
// it still lists, edits and saves as itself.
static void TestBrokenMapKeepsTheOpenOne( BkEditorSession *pSession, const std::string &szScratch )
{
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, &summary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	// Written by hand, so with the OS's separator; the bridge is handed the
	// engine's, as every other scratch map here is.
	const std::string szBroken = szScratch + "\\broken.bzm";
	std::string szBrokenNative = szBroken;
	for ( size_t i = 0; i < szBrokenNative.size(); ++i )
		if ( szBrokenNative[i] == '\\' )
			szBrokenNative[i] = '/';
	if ( FILE *pFile = fopen( szBrokenNative.c_str(), "wb" ) )
	{
		const char garbage[] = "this is not a map, and the reader has to say so";
		fwrite( garbage, 1, sizeof garbage, pFile );
		fclose( pFile );
	}
	const std::string szMissing = szScratch + "\\no-such-map.bzm";
	BkEditorMapSummary ignored;
	Check( BkEditorOpenMap( pSession, szBroken.c_str(), &ignored ) == BK_EDITOR_DATA_MISSING, "a broken map is not opened" );
	Check( *BkEditorLastMessage( pSession ) != 0, "and says why" );
	printf( "editor-bridge: a broken map: %s\n", BkEditorLastMessage( pSession ) );
	Check( BkEditorOpenMap( pSession, szMissing.c_str(), &ignored ) == BK_EDITOR_DATA_MISSING, "nor is a missing one" );
	int nCount = 0;
	Check( BkEditorObjects( pSession, 0, 0, &nCount ) == BK_EDITOR_REFUSED && nCount == summary.object_count,
	       "the map that was open still lists its objects" );
	Check( BkEditorSetMapType( pSession, 1 ) == BK_EDITOR_OK, "and takes an edit" );
	const std::string szSaved = szScratch + "\\kept-saved.bzm";
	CMapInfo expected, saved;
	std::string szError, szWhere;
	if ( Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) &&
	     Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
	{
		expected.nType = 1;
		Check( NMapFile::AreEquivalent( expected, saved, &szWhere ), ( "and saves as itself: " + szWhere ).c_str() );
	}
	remove( szBrokenNative.c_str() );
	remove( szSaved.c_str() );
}

// arnheim carries 361 terrain objects under link ID 0, "no link ID". The
// engine holds and draws every one of them, but a link ID is the only name the
// ABI has, so an edit of a shared one cannot say which object it means: it is
// refused, and the map saves as it was read. An object with an ID of its own
// still deletes and comes back.
static void TestSharedLinkIDIsReadOnly( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &map, &szError ), szError.c_str() ) )
		return;
	std::map<int, int> counts;
	const std::vector<SMapObjectInfo> *lists[2] = { &map.objects, &map.scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
			++counts[( *lists[nList] )[i].link.nLinkID];
	int nShared = -1;
	for ( std::map<int, int>::const_iterator it = counts.begin(); it != counts.end() && nShared < 0; ++it )
		if ( it->second > 1 )
			nShared = it->first;
	if ( !Check( nShared >= 0, "arnheim has objects that share a link ID" ) )
		return;
	printf( "editor-bridge: %d objects of arnheim share link ID %d\n", counts[nShared], nShared );
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorObjectState before;
	Check( BkEditorEngineObjectState( pSession, nShared, &before ) == BK_EDITOR_OK, "the engine holds an object under the shared ID" );
	Check( BkEditorDeleteObject( pSession, nShared ) == BK_EDITOR_REFUSED, "deleting a shared link ID is refused" );
	Check( strstr( BkEditorLastMessage( pSession ), "share link ID" ) != 0, "and says why" );
	Check( BkEditorMoveObject( pSession, nShared, before.x + 32, before.y ) == BK_EDITOR_REFUSED, "and so is moving it" );
	Check( BkEditorPlaceObject( pSession, nShared, before.x, before.y, 0, 0 ) == BK_EDITOR_REFUSED, "and placing it" );
	BkEditorObjectState after;
	Check( BkEditorEngineObjectState( pSession, nShared, &after ) == BK_EDITOR_OK && after.x == before.x && after.y == before.y,
	       "the engine object has not moved" );
	Check( BkEditorRestoreObject( pSession, nShared ) == BK_EDITOR_REFUSED, "and there is nothing to restore" );

	// An object with a link ID of its own is unaffected.
	int nOwn = -1;
	for ( size_t i = 0; i < map.objects.size() && nOwn < 0; ++i )
	{
		const int nCandidate = map.objects[i].link.nLinkID;
		BkEditorObjectState state;
		if ( counts[nCandidate] == 1 && BkEditorEngineObjectState( pSession, nCandidate, &state ) == BK_EDITOR_OK &&
		     BkEditorDeleteObject( pSession, nCandidate ) == BK_EDITOR_OK )
			nOwn = nCandidate;
	}
	if ( Check( nOwn >= 0, "an object with its own link ID deletes" ) )
		Check( BkEditorRestoreObject( pSession, nOwn ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	const std::string szSaved = szScratch + "\\shared-link-saved.bzm";
	CMapInfo saved;
	std::string szWhere;
	if ( Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		Check( NMapFile::AreEquivalent( map, saved, &szWhere ), ( "refused edits of a shared link ID leave the map as read: " + szWhere ).c_str() );
	remove( szSaved.c_str() );
}

// Every kind of object the palette offers can be asked for, and each answer is
// a status, never a crash. Johannes placed a sound from the palette and the
// release editor died in CheckStaticObject: a sound's stats are an
// SSoundRPGStats, not an SObjectBaseRPGStats, and the static_cast there read
// a passability array out of something that has none. So one entry of every
// game type the catalogue holds is added at the map's middle - what the
// middle of the screen shows after an open - and each must come back OK, or
// refused with a message that names the object. What was added is deleted
// again, and the engine has to agree with the map afterwards.
static void TestEveryGameTypeAnswers( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nCatalogue = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
	if ( !Check( nCatalogue > 0, "the catalogue has entries" ) )
		return;
	std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue );
	int nRead = 0;
	if ( !Check( BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nRead ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	std::map<int, std::string> firstOfType;
	std::map<int, int> countOfType;
	for ( int i = 0; i < nRead; ++i )
	{
		++countOfType[catalogue[i].game_type];
		if ( firstOfType.find( catalogue[i].game_type ) == firstOfType.end() )
			firstOfType[catalogue[i].game_type] = catalogue[i].name;
	}
	Check( firstOfType.find( 100 ) != firstOfType.end(), "the catalogue offers a sound (game type 100), the case that crashed" );

	float wx = 0.0f, wy = 0.0f, mx = 0.0f, my = 0.0f;
	BkEditorFrame( pSession );
	if ( !Check( BkEditorScreenToWorld( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f, &wx, &wy ) == BK_EDITOR_OK &&
	             BkEditorWorldToMap( pSession, wx, wy, &mx, &my ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	for ( std::map<int, std::string>::const_iterator it = firstOfType.begin(); it != firstOfType.end(); ++it )
	{
		const char *pszName = it->second.c_str();
		// Said before the call, so a crash leaves the object that caused it as
		// the last line of the log.
		printf( "editor-bridge: adding game type %d (%d in the catalogue) as %s at map %.0f,%.0f\n",
		        it->first, countOfType[it->first], pszName, mx, my );
		fflush( stdout );
		int nLinkID = -1;
		const BkEditorStatus status = BkEditorAddObject( pSession, pszName, mx, my, 0, 0, &nLinkID );
		const std::string szMessage = BkEditorLastMessage( pSession );
		printf( "editor-bridge:   -> status %d%s%s\n", int( status ), status == BK_EDITOR_OK ? "" : ": ", status == BK_EDITOR_OK ? "" : szMessage.c_str() );
		if ( status == BK_EDITOR_OK )
		{
			Check( nLinkID > 0, NStr::Format( "%s comes back with a link ID", pszName ) );
			Check( BkEditorDeleteObject( pSession, nLinkID ) == BK_EDITOR_OK,
			       NStr::Format( "the added %s is undone: %s", pszName, BkEditorLastMessage( pSession ) ) );
		}
		else
		{
			Check( status == BK_EDITOR_REFUSED || status == BK_EDITOR_BAD_ARGUMENT,
			       NStr::Format( "adding %s (game type %d) is OK, refused or a bad argument, not status %d", pszName, it->first, int( status ) ) );
			Check( szMessage.find( pszName ) != std::string::npos,
			       NStr::Format( "the refusal of %s names it: \"%s\"", pszName, szMessage.c_str() ) );
			Check( nLinkID == -1, NStr::Format( "a refused %s has no link ID", pszName ) );
		}
	}
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK,
	       ( std::string( "after every game type was asked for: " ) + BkEditorLastMessage( pSession ) ).c_str() );
}

// D-29: BkEditorObjectPicture over every placeable catalogue entry (every
// game type but the sound list, 100, and the tank pit, 5 - the same
// placeable rule TestEveryGameTypeAnswers' own catalogue loop follows, and
// panels_logic.isPlaceable's Zig-side equivalent) - the coverage measurement
// the plan's checkpoint decision is made from.
static void TestObjectPictures( BkEditorSession *pSession, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nCatalogue = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
	if ( !Check( nCatalogue > 0, "the catalogue has entries" ) )
		return;
	std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue );
	int nRead = 0;
	if ( !Check( BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nRead ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	std::vector<unsigned char> buffer( 256 * 256 * 4 );
	int nPlaceable = 0, nWithPicture = 0;
	std::map<int, int> missingByType;
	std::string szFirstWithPicture;
	bool bCheckedFirstUnit = false;
	double fTotalSeconds = 0.0;

	for ( int i = 0; i < nRead; ++i )
	{
		// 5: tank pit, 100: sound - neither is a map object (WhyNotAMapObject,
		// session.cpp), so neither is ever offered a picture in the palette.
		if ( catalogue[i].game_type == 5 || catalogue[i].game_type == 100 )
			continue;
		++nPlaceable;
		int nWidth = 0, nHeight = 0;
		const Uint64 nStart = SDL_GetPerformanceCounter();
		const BkEditorStatus status = BkEditorObjectPicture( pSession, catalogue[i].name, &buffer[0], int( buffer.size() ), 64, &nWidth, &nHeight );
		const Uint64 nEnd = SDL_GetPerformanceCounter();
		fTotalSeconds += double( nEnd - nStart ) / double( SDL_GetPerformanceFrequency() );
		if ( status == BK_EDITOR_OK )
		{
			++nWithPicture;
			if ( szFirstWithPicture.empty() )
			{
				szFirstWithPicture = catalogue[i].name;
				// One sample, for a human to look at rather than trust the
				// checks below alone (03-09's checkpoint asks for it).
				const std::string szSample = szScratch + "/03-09-sample-picture.tga";
				const bool bSampleWritten = WriteRgbaTga( szSample.c_str(), &buffer[0], nWidth, nHeight );
				printf( "editor-bridge: %s %s (%s, %dx%d)\n", bSampleWritten ? "saved" : "could not save", szSample.c_str(), catalogue[i].name, nWidth, nHeight );
			}
			// 1: SGVOGT_UNIT (Main/GameDB.h) - the first unit with a picture,
			// checked for a sane decode rather than trusting the status alone.
			if ( !bCheckedFirstUnit && catalogue[i].game_type == 1 )
			{
				bCheckedFirstUnit = true;
				Check( nWidth >= 1 && nWidth <= 256 && nHeight >= 1 && nHeight <= 256,
				       NStr::Format( "%s's picture is %dx%d, expected 1..256 on each side", catalogue[i].name, nWidth, nHeight ) );
				bool bNonBlack = false;
				for ( int p = 0; p < nWidth * nHeight && !bNonBlack; ++p )
					if ( buffer[p * 4 + 0] != 0 || buffer[p * 4 + 1] != 0 || buffer[p * 4 + 2] != 0 )
						bNonBlack = true;
				Check( bNonBlack, NStr::Format( "%s's picture has at least one non-black pixel", catalogue[i].name ) );
			}
		}
		else
		{
			Check( status == BK_EDITOR_REFUSED,
			       NStr::Format( "%s with no picture is refused, got status %d: %s", catalogue[i].name, int( status ), BkEditorLastMessage( pSession ) ) );
			++missingByType[catalogue[i].game_type];
		}
	}
	printf( "editor-bridge: pictures: %d of %d placeable objects have one\n", nWithPicture, nPlaceable );
	printf( "editor-bridge: pictures: %.3f ms per decode (%d decodes, %.3f s total)\n",
	        nPlaceable != 0 ? fTotalSeconds * 1000.0 / nPlaceable : 0.0, nPlaceable, fTotalSeconds );
	for ( std::map<int, int>::const_iterator it = missingByType.begin(); it != missingByType.end(); ++it )
		printf( "editor-bridge: pictures: game type %d has %d without one\n", it->first, it->second );

	int nUnknownWidth = 0, nUnknownHeight = 0;
	Check( BkEditorObjectPicture( pSession, "NoSuchObjectAtAll", &buffer[0], int( buffer.size() ), 64, &nUnknownWidth, &nUnknownHeight ) == BK_EDITOR_BAD_ARGUMENT,
	       "an unknown name is a bad argument" );

	if ( !szFirstWithPicture.empty() )
	{
		unsigned char smallBuffer[16];
		int nShortWidth = 0, nShortHeight = 0;
		const BkEditorStatus shortStatus = BkEditorObjectPicture( pSession, szFirstWithPicture.c_str(), smallBuffer, sizeof smallBuffer, 64, &nShortWidth, &nShortHeight );
		Check( shortStatus == BK_EDITOR_REFUSED,
		       NStr::Format( "a 16-byte buffer is refused for %s, got status %d", szFirstWithPicture.c_str(), int( shortStatus ) ) );
		Check( nShortWidth > 0 && nShortHeight > 0, "a short buffer still reports the real sizes" );
	}
	else
		printf( "editor-bridge: pictures: skipped the short-buffer check, no placeable object has a picture\n" );

	// User-requested addition (03-09 Task 4): Allies_Bren has no icon.tga of
	// its own (Data/Units/Humans/Allies/Bren has only .san/.dds files) but is
	// a member of the gb_bren_43 squad (Data/Squads/gb_bren_43/1.xml's own
	// <Members>), which does - BkEditorObjectPicture should borrow it rather
	// than refuse.
	{
		int nBrenWidth = 0, nBrenHeight = 0;
		const BkEditorStatus brenStatus = BkEditorObjectPicture( pSession, "Allies_Bren", &buffer[0], int( buffer.size() ), 64, &nBrenWidth, &nBrenHeight );
		Check( brenStatus == BK_EDITOR_OK,
		       NStr::Format( "Allies_Bren (no icon.tga of its own) should borrow its squad's, got status %d: %s", int( brenStatus ), BkEditorLastMessage( pSession ) ) );
		if ( brenStatus == BK_EDITOR_OK )
		{
			Check( nBrenWidth >= 1 && nBrenWidth <= 256 && nBrenHeight >= 1 && nBrenHeight <= 256,
			       NStr::Format( "Allies_Bren's borrowed picture is %dx%d, expected 1..256 on each side", nBrenWidth, nBrenHeight ) );
			bool bNonBlack = false;
			for ( int p = 0; p < nBrenWidth * nBrenHeight && !bNonBlack; ++p )
				if ( buffer[p * 4 + 0] != 0 || buffer[p * 4 + 1] != 0 || buffer[p * 4 + 2] != 0 )
					bNonBlack = true;
			Check( bNonBlack, "Allies_Bren's borrowed picture has at least one non-black pixel" );
		}
	}
}

// D-26: BkEditorMods lists the fixture mod (tools/zig/fixtures/editor_mod,
// staged at <install>/mods/EditorTestMod for this tier only - never the
// unlicensed AchtungPanzer2); BkEditorSetMod switches to it and back,
// closing the open map each time (its object database is about to change
// under it); a bad or unknown folder is refused and changes nothing.
static void TestModsListSetAndClear( BkEditorSession *pSession, const std::string &szScratch )
{
	(void)szScratch;
	int nBaseCatalogue = 0;
	BkEditorCatalogue( pSession, 0, 0, &nBaseCatalogue );

	// The sizing call with capacity 0 is REFUSED whenever there is at least
	// one mod - the same two-call convention BkEditorCatalogue's own sizing
	// call follows (TestEveryGameTypeAnswers, above); only the count matters.
	int nModCount = 0;
	BkEditorMods( pSession, 0, 0, &nModCount );
	if ( !Check( nModCount > 0, "at least the fixture mod is installed" ) )
		return;
	std::vector<BkEditorMod> mods( nModCount );
	int nRead = 0;
	if ( !Check( BkEditorMods( pSession, &mods[0], nModCount, &nRead ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nFixture = -1;
	for ( int i = 0; i < nRead; ++i )
		if ( strcmp( mods[i].folder, "EditorTestMod" ) == 0 )
			nFixture = i;
	if ( !Check( nFixture >= 0, "BkEditorMods lists EditorTestMod" ) )
		return;
	Check( strcmp( mods[nFixture].name, "Editor Test Mod" ) == 0,
	       NStr::Format( "EditorTestMod's name is \"Editor Test Mod\", got \"%s\"", mods[nFixture].name ) );
	Check( strcmp( mods[nFixture].version, "1.0" ) == 0,
	       NStr::Format( "EditorTestMod's version is \"1.0\", got \"%s\"", mods[nFixture].version ) );

	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	if ( !Check( BkEditorSetMod( pSession, "EditorTestMod" ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorMod active;
	memset( &active, 0, sizeof active );
	if ( Check( BkEditorActiveMod( pSession, &active ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		Check( strcmp( active.folder, "EditorTestMod" ) == 0,
		       NStr::Format( "ActiveMod's folder is EditorTestMod, got \"%s\"", active.folder ) );
		Check( strcmp( active.name, "Editor Test Mod" ) == 0, "ActiveMod's name follows the mod" );
	}

	// The switch closes the open map: its object database just changed.
	int nLinkID = -1;
	Check( BkEditorObjectAt( pSession, 0.0f, 0.0f, &nLinkID ) == BK_EDITOR_REFUSED,
	       "no map is open right after the switch" );

	// D-26, revised 2026-09-29: the editor now closes the map on every
	// switch, so a switch with no map open is the common case - to None and
	// back again, each one OK, and still no map open after either.
	Check( BkEditorSetMod( pSession, 0 ) == BK_EDITOR_OK,
	       NStr::Format( "SetMod(null) with no map open: %s", BkEditorLastMessage( pSession ) ) );
	if ( !Check( BkEditorSetMod( pSession, "EditorTestMod" ) == BK_EDITOR_OK,
	             NStr::Format( "SetMod(EditorTestMod) with no map open: %s", BkEditorLastMessage( pSession ) ) ) )
		return;
	Check( BkEditorObjectAt( pSession, 0.0f, 0.0f, &nLinkID ) == BK_EDITOR_REFUSED,
	       "still no map open after two switches with none open" );
	BkEditorMod again;
	memset( &again, 0, sizeof again );
	if ( Check( BkEditorActiveMod( pSession, &again ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( strcmp( again.folder, "EditorTestMod" ) == 0,
		       NStr::Format( "ActiveMod's folder is EditorTestMod after switching with no map open, got \"%s\"", again.folder ) );

	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nCatalogueWithMod = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCatalogueWithMod );
	Check( nCatalogueWithMod >= nBaseCatalogue,
	       NStr::Format( "the catalogue with EditorTestMod active has %d entries, at least the base %d", nCatalogueWithMod, nBaseCatalogue ) );

	Check( BkEditorSetMod( pSession, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	BkEditorMod cleared;
	memset( &cleared, 0, sizeof cleared );
	if ( Check( BkEditorActiveMod( pSession, &cleared ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( cleared.folder[0] == 0, "ActiveMod reports none after SetMod(null)" );

	Check( BkEditorSetMod( pSession, "../x" ) == BK_EDITOR_BAD_ARGUMENT, "\"../x\" is a bad argument" );
	Check( BkEditorSetMod( pSession, "a/b" ) == BK_EDITOR_BAD_ARGUMENT, "\"a/b\" is a bad argument" );
	const BkEditorStatus noSuchStatus = BkEditorSetMod( pSession, "NoSuchMod" );
	const std::string szNoSuchMessage = BkEditorLastMessage( pSession );
	Check( noSuchStatus == BK_EDITOR_REFUSED,
	       NStr::Format( "\"NoSuchMod\" is refused, got status %d", int( noSuchStatus ) ) );
	Check( szNoSuchMessage.find( "NoSuchMod" ) != std::string::npos,
	       NStr::Format( "the refusal names the folder: \"%s\"", szNoSuchMessage.c_str() ) );

	// Leave the session on no mod with coldwinter open, for whatever runs next.
	Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
}

// D-28: BkEditorSaveMap stamps szMODName/szMODVersion from the active mod -
// never from a game profile the editor has none of - and leaves the two
// fields exactly as read when no mod is active (the preservation invariant
// bridge.h's own BkEditorSaveMap comment describes).
static void TestSaveRecordsTheMod( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::string szStamped = szScratch + "\\mod-stamped.bzm";
	const std::string szFree = szScratch + "\\mod-free.bzm";

	if ( !Check( BkEditorSetMod( pSession, "EditorTestMod" ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	if ( !Check( BkEditorSaveMap( pSession, szStamped.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo stamped;
	std::string szError;
	if ( !Check( NMapFile::Read( szStamped.c_str(), &stamped, &szError ), szError.c_str() ) )
		return;
	Check( stamped.szMODName == "Editor Test Mod",
	       NStr::Format( "the mod-stamped map's szMODName is \"Editor Test Mod\", got \"%s\"", stamped.szMODName.c_str() ) );
	Check( stamped.szMODVersion == "1.0",
	       NStr::Format( "the mod-stamped map's szMODVersion is \"1.0\", got \"%s\"", stamped.szMODVersion.c_str() ) );

	if ( !Check( BkEditorSetMod( pSession, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	if ( !Check( BkEditorSaveMap( pSession, szFree.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo shipped, free;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &shipped, &szError ), szError.c_str() ) )
		return;
	if ( !Check( NMapFile::Read( szFree.c_str(), &free, &szError ), szError.c_str() ) )
		return;
	Check( free.szMODName == shipped.szMODName,
	       NStr::Format( "with no mod active szMODName is untouched: \"%s\" vs shipped's \"%s\"", free.szMODName.c_str(), shipped.szMODName.c_str() ) );
	Check( free.szMODVersion == shipped.szMODVersion,
	       NStr::Format( "with no mod active szMODVersion is untouched: \"%s\" vs shipped's \"%s\"", free.szMODVersion.c_str(), shipped.szMODVersion.c_str() ) );
}

// BkEditorSounds/AddSound/SetSound/DeleteSound against CMapInfo::sounds.sounds
// (see bridge.h's own comment on BkEditorSounds for why this is not
// CMapInfo::soundsList, a sibling field this bridge never touches because
// nothing serialises it). Finds a shipped map with a non-empty sound list by
// walking Data/Maps, at most 60 .bzm files deep; falls back to coldwinter
// (which has none) so the rest of the test still runs against a real map.
static void TestSoundList( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	std::string szMapPath = SHIPPED_MAP;
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	{
		int nExamined = 0;
		std::error_code error;
		std::filesystem::recursive_directory_iterator it( "Data/Maps", error ), end;
		for ( ; !error && it != end && nExamined < 60; it.increment( error ) )
		{
			if ( it->is_directory() )
				continue;
			if ( it->path().extension() != ".bzm" )
				continue;
			++nExamined;
			std::string szCandidateEngine = it->path().string();
			for ( std::string::size_type i = 0; i < szCandidateEngine.size(); ++i )
				if ( szCandidateEngine[i] == '/' ) szCandidateEngine[i] = '\\';
			CMapInfo candidate;
			std::string szCandidateError;
			if ( !NMapFile::Read( szCandidateEngine.c_str(), &candidate, &szCandidateError ) )
				continue;
			if ( !candidate.sounds.sounds.empty() )
			{
				szMapPath = szCandidateEngine;
				original = candidate;
				break;
			}
		}
		printf( "editor-bridge: TestSoundList uses %s (%d sound(s), %d shipped maps examined)\n",
		        szMapPath.c_str(), int( original.sounds.sounds.size() ), nExamined );
	}

	if ( !Check( BkEditorOpenMap( pSession, szMapPath.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// BkEditorSounds equals the file's list.
	int nCount = -1;
	BkEditorSounds( pSession, 0, 0, &nCount );
	if ( !Check( nCount == int( original.sounds.sounds.size() ), "BkEditorSounds reports the full count with a short buffer" ) )
		return;
	std::vector<BkEditorSoundRecord> records( nCount > 0 ? nCount : 1 );
	int nRead = 0;
	if ( !Check( BkEditorSounds( pSession, &records[0], nCount, &nRead ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( nRead == nCount, "and reads all of them" );
	for ( int i = 0; i < nCount; ++i )
	{
		const SMapSoundInfo &rSound = original.sounds.sounds[i];
		Check( strcmp( records[i].name, rSound.szName.c_str() ) == 0 &&
		       records[i].x == rSound.vPos.x && records[i].y == rSound.vPos.y && records[i].z == rSound.vPos.z &&
		       records[i].repeat_ms == int( rSound.timeRepeat ) && records[i].repeat_random_ms == int( rSound.timeRepeatRandom ) &&
		       records[i].mute_in_combat == ( rSound.bMuteDuringCombat ? 1 : 0 ) &&
		       records[i].min_radius == rSound.nMinRadius && records[i].max_radius == rSound.nMaxRadius,
		       NStr::Format( "sound %d reads back as the file has it", i ) );
	}

	// A known sound (the first catalogue entry of game type 100) at the map's
	// middle: count + 1.
	int nCatalogue = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
	if ( !Check( nCatalogue > 0, "the catalogue has entries" ) )
		return;
	std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue );
	int nCatalogueRead = 0;
	if ( !Check( BkEditorCatalogue( pSession, &catalogue[0], nCatalogue, &nCatalogueRead ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	std::string szSoundName;
	for ( int i = 0; i < nCatalogueRead; ++i )
		if ( catalogue[i].game_type == 100 ) { szSoundName = catalogue[i].name; break; }
	if ( !Check( !szSoundName.empty(), "the catalogue offers a sound (game type 100)" ) )
		return;

	BkEditorFrame( pSession );
	float wx = 0.0f, wy = 0.0f;
	if ( !Check( BkEditorScreenToWorld( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f, &wx, &wy ) == BK_EDITOR_OK,
	             "the map's middle is on the map" ) )
		return;

	BkEditorSoundRecord add;
	memset( &add, 0, sizeof add );
	strncpy( add.name, szSoundName.c_str(), sizeof add.name - 1 );
	add.x = wx;
	add.y = wy;
	add.z = 0.0f;
	add.repeat_ms = 1000;
	add.repeat_random_ms = 500;
	add.mute_in_combat = 1;
	add.min_radius = 1;
	add.max_radius = 5;
	if ( !Check( BkEditorAddSound( pSession, -1, &add ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nAfterAdd = -1;
	BkEditorSounds( pSession, 0, 0, &nAfterAdd );
	Check( nAfterAdd == nCount + 1, "adding a sound grows the list by one" );

	// Set its radii.
	std::vector<BkEditorSoundRecord> afterAdd( nAfterAdd );
	int nAfterAddRead = 0;
	if ( !Check( BkEditorSounds( pSession, &afterAdd[0], nAfterAdd, &nAfterAddRead ) == BK_EDITOR_OK, "the grown list reads" ) )
		return;
	BkEditorSoundRecord edited = afterAdd[nAfterAdd - 1];
	edited.min_radius = 2;
	edited.max_radius = 9;
	if ( !Check( BkEditorSetSound( pSession, nAfterAdd - 1, &edited ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	std::vector<BkEditorSoundRecord> afterSet( nAfterAdd );
	int nAfterSetRead = 0;
	if ( !Check( BkEditorSounds( pSession, &afterSet[0], nAfterAdd, &nAfterSetRead ) == BK_EDITOR_OK, "the edited list reads" ) )
		return;
	Check( afterSet[nAfterAdd - 1].min_radius == 2 && afterSet[nAfterAdd - 1].max_radius == 9, "the set radii stuck" );

	// A kept (not deleted) edit survives a save and reload with its exact
	// fields - the add+delete round trip below only proves the list's shape
	// comes back; this proves a real edit's own values do too.
	{
		const std::string szKept = szScratch + "\\sounds-kept.bzm";
		if ( Check( BkEditorSaveMap( pSession, szKept.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			CMapInfo kept;
			std::string szKeptError;
			if ( Check( NMapFile::Read( szKept.c_str(), &kept, &szKeptError ), szKeptError.c_str() ) )
			{
				if ( Check( kept.sounds.sounds.size() == size_t( nAfterAdd ), "the kept map has the grown sound count" ) )
				{
					const SMapSoundInfo &rKept = kept.sounds.sounds[nAfterAdd - 1];
					const BkEditorSoundRecord &rExpected = afterSet[nAfterAdd - 1];
					Check( rKept.szName == rExpected.name && rKept.vPos.x == rExpected.x && rKept.vPos.y == rExpected.y && rKept.vPos.z == rExpected.z &&
					           rKept.timeRepeat == NTimer::STime( rExpected.repeat_ms ) && rKept.timeRepeatRandom == NTimer::STime( rExpected.repeat_random_ms ) &&
					           rKept.bMuteDuringCombat == ( rExpected.mute_in_combat != 0 ) && rKept.nMinRadius == rExpected.min_radius && rKept.nMaxRadius == rExpected.max_radius,
					       "the kept sound's own fields (name, position, repeat, mute, radii) survived the save and reload" );
				}
			}
		}
		remove( szKept.c_str() );
	}

	// Delete it: the list equals the file's again.
	if ( !Check( BkEditorDeleteSound( pSession, nAfterAdd - 1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nAfterDelete = -1;
	BkEditorSounds( pSession, 0, 0, &nAfterDelete );
	Check( nAfterDelete == nCount, "deleting the added sound shrinks the list back" );

	// Every refusal above leaves the list unchanged.
	BkEditorSoundRecord bad = add;
	strncpy( bad.name, "NoSuchSoundAtAll", sizeof bad.name - 1 );
	Check( BkEditorAddSound( pSession, -1, &bad ) == BK_EDITOR_REFUSED, "an unknown name is refused" );
	bad = add;
	bad.x = -1000000.0f;
	bad.y = -1000000.0f;
	Check( BkEditorAddSound( pSession, -1, &bad ) == BK_EDITOR_REFUSED, "an off-map position is refused" );
	bad = add;
	bad.repeat_ms = -1;
	Check( BkEditorAddSound( pSession, -1, &bad ) == BK_EDITOR_REFUSED, "a negative repeat is refused" );
	bad = add;
	bad.min_radius = 9;
	bad.max_radius = 1;
	Check( BkEditorAddSound( pSession, -1, &bad ) == BK_EDITOR_REFUSED, "min_radius above max_radius is refused" );
	Check( BkEditorAddSound( pSession, -1, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null record is a bad argument" );
	Check( BkEditorAddSound( pSession, nCount + 5, &add ) == BK_EDITOR_BAD_ARGUMENT, "an index past the end is a bad argument" );
	Check( BkEditorSetSound( pSession, nCount, &add ) == BK_EDITOR_BAD_ARGUMENT, "set past the end is a bad argument" );
	Check( BkEditorDeleteSound( pSession, nCount ) == BK_EDITOR_BAD_ARGUMENT, "delete past the end is a bad argument" );
	int nAfterRefusals = -1;
	BkEditorSounds( pSession, 0, 0, &nAfterRefusals );
	Check( nAfterRefusals == nCount, "none of the refusals changed the list" );

	// Save to <scratch>/sounds.bzm and read back: sounds.sounds equals the
	// original (preservation, T-03-10-02).
	const std::string szSaved = szScratch + "\\sounds.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo saved;
	if ( !Check( NMapFile::Read( szSaved.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	std::string szWhere;
	Check( NMapFile::AreEquivalent( original, saved, &szWhere ),
	       szWhere.empty() ? "the saved map equals the original after add+delete"
	                       : ( "add+delete left a difference at " + szWhere ).c_str() );
	remove( szSaved.c_str() );
}

int main( int argc, char **argv )
{
	// A failed assert in a Windows debug build prints to stderr and then calls
	// abort(), which the debug CRT reports as a "Debug Error!" message box. On a
	// CI runner nobody clicks it: run 35683754678 sat behind one for three and a
	// half hours. Report both to stderr and let abort() just end the process.
#if defined(_WIN32) || defined(_WIN64)
	_set_error_mode( _OUT_TO_STDERR );
	_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
#endif

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
		// First, before any map is open (Task 1 carried).
		TestEntryPointsBeforeAMap( pSession );
		TestShippedMapOpens( pSession );
		TestBridgeSpansAreBuilt( pSession );
		TestUneditedSaveIsEquivalent( pSession, szScratch );
		TestPathsAndTestMapPath( pSession );
		TestObjectEdits( pSession, szScratch );
		TestSaveVerifiesWhatItWrote( pSession, szScratch );
		TestRefusedEditsReachNeither( pSession, szScratch );
		TestPartlyRefusedEditRollsTheEngineBack( pSession, szScratch );
		TestMapsOwnFields( pSession, szScratch );
		TestObjectsReadBack( pSession );
		TestUnknownObjectIsReadOnly( pSession, szScratch );
		TestPaintReachesEngineAndFile( pSession, szScratch );
		TestPaintUndoIsExact( pSession, szScratch );
		TestPaintAtTheEdgeAndRefused( pSession, szScratch );
		TestPaintRefusesTileOutsideTileset( pSession );
		TestTilesetTilesAllPaint( pSession );
		TestDeleteRestoreKeepsTheObject( pSession, szScratch );
		// The 640x480 the window was created at: BkEditorStart sets the mode to
		// the window's own size. The middle of the screen is the middle of what
		// the engine draws, so it is read from the bridge rather than assumed.
		int nScreenWidth = 0, nScreenHeight = 0;
		Check( BkEditorScreenSize( pSession, &nScreenWidth, &nScreenHeight ) == BK_EDITOR_OK, "the screen size reads" );
		printf( "editor-bridge: the screen is %dx%d\n", nScreenWidth, nScreenHeight );
		TestCatalogueCameraAndFrame( pSession, nScreenWidth, nScreenHeight );
		TestTerrainUnderTheCamera( pSession, szScratch );
		TestZoomStepsBoundedAndAnchored( pSession, pWindow, szScratch );
		TestWorldToScreenRoundTrip( pSession, nScreenWidth, nScreenHeight );
		TestOverlayDeviceAndSize( pSession, pWindow );
		// Read again rather than trusting the resize test to have put the
		// screen back at the size it was.
		Check( BkEditorScreenSize( pSession, &nScreenWidth, &nScreenHeight ) == BK_EDITOR_OK, "the screen size reads after the resize test" );
		printf( "editor-bridge: after the resize test the screen is %dx%d\n", nScreenWidth, nScreenHeight );
		TestObjectUnderTheCursor( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestPlacedObjectDrawsAndPicks( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestSeasonPicksTheVisuals( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestMissingSeasonTextureFallsBack( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestYawMeasurement( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestEveryGameTypeAnswers( pSession, nScreenWidth, nScreenHeight );
		TestObjectPictures( pSession, szScratch );
		TestSquadDeletesAndRestores( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestDeleteIsRefusedWhileReferred( pSession, szScratch );
		TestSharedLinkIDIsReadOnly( pSession, szScratch );
		TestBrokenMapKeepsTheOpenOne( pSession, szScratch );
		TestMissingStatsDoNotStopTheOpen( pSession );
		TestUnknownObjectDoesNotStopTheOpen( pSession, szScratch );
		TestModsListSetAndClear( pSession, szScratch );
		TestSaveRecordsTheMod( pSession, szScratch );
		TestSoundList( pSession, nScreenWidth, nScreenHeight, szScratch );
		// The overlay reaches a present made straight through the engine, so the
		// check after the stop below can tell a removed overlay from a present
		// that never happened.
		g_nOverlayCalls = 0;
		Check( BkEditorSetOverlay( pSession, CountOverlay, &g_nOverlayCalls ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( PresentThroughTheEngine() == 1 && g_nOverlayCalls >= 1,
		       NStr::Format( "an overlay runs in a present made through the engine (%d calls)", g_nOverlayCalls ) );
		Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "and stops" );
		const int nCallsAtStop = g_nOverlayCalls;
		PresentThroughTheEngine();
		Check( g_nOverlayCalls == nCallsAtStop,
		       NStr::Format( "a stop removes the overlay (%d calls after it)", g_nOverlayCalls - nCallsAtStop ) );
	}

	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	if ( g_nFailures == 0 )
		printf( "editor-bridge: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
