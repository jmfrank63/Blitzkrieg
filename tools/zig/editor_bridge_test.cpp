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
#include <set>
#include "../../Sources/src/EditorBridge/bridge.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
#include "../../Sources/src/MapFile/MapOverlay.h"
#include "../../Sources/src/MapFile/MapRecords.h"
#include "../../Sources/src/MapFile/MapGeometry.h"
#include "../../Sources/src/Main/GameDB.h"
#include "../../Sources/src/Main/RPGStats.h"
#include "../../Sources/src/Formats/fmtTerrain.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"
#include "../../Sources/src/RandomMapGen/VA_Types.h"
#include "../../Sources/src/RandomMapGen/TerrainGenerator.h"
#include "../../Sources/src/RandomMapGen/PNoise.h"
#include "../../Sources/src/RandomMapGen/VSO_Types.h"
#include "../../Sources/src/StreamIO/RandomGen.h"
#include "../../Sources/src/StreamIO/StreamIOTypes.h"
#include "../../Sources/src/Misc/Win32Random.h"
#include "../../Sources/src/AILogic/AILogic.h"
#include "../../Sources/src/AILogic/aiconsts.h"
#include "../../Sources/src/Common/Actions.h"
#include "../../Sources/src/GFX/GFX.H"
#include "../../Sources/src/Scene/Scene.h"
#include "../../Sources/src/Scene/Terrain.h"
#include "../../Sources/src/Image/Image.h"
#include "../../Sources/src/Platform/Paths.h"
#include "../../Sources/src/StreamIO/GeneratedData.h"
#include "../../Sources/src/StreamIO/SeasonData.h"
#include "../../Sources/src/StreamIO/ProfilePaths.h"
#include <algorithm>
#include <cmath>
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

// The tile and object shells draw from the engine's random services: the
// bridge seeds its fills from a fixed state (session_fields.cpp), and the
// test replays them with the same seeds. The zero seed is the game's own
// fixed point (IRandomGenSeed::InitByZeroSeed, NWin32Random::Seed), and
// srand( 0 ) the bridge's for the C runtime's rand(), which picks every
// filled cell's tile variant (STileTypeDesc::GetMapsIndex).
static void ReseedRandom()
{
	NWin32Random::Seed( 0 );
	srand( 0 );
	if ( CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED ) )
	{
		pSeed->InitByZeroSeed();
		if ( g_pGlobalRandomGen != 0 )
			g_pGlobalRandomGen->SetSeed( pSeed );
		GetSingleton<IRandomGen>()->SetSeed( pSeed );
	}
}

// The opposite: every generator the fills draw from moved off the replay's
// fixed point, so an apply that still matches the replay proves the bridge
// seeds its own fills (session_fields.cpp SeedFieldFills) rather than riding
// on the seeds the test happened to leave behind.
static void ScrambleRandom()
{
	NWin32Random::Seed( 12345 );
	srand( 12345 );
	if ( IRandomGen *pGen = GetSingleton<IRandomGen>() )
		for ( int i = 0; i < 17; ++i )
			pGen->Get();
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
	BkEditorCameraAnchorRecord anchorRecord; memset( &anchorRecord, 0, sizeof anchorRecord );
	BkEditorVsoDescriptor vsoDescriptor; memset( &vsoDescriptor, 0, sizeof vsoDescriptor );
	BkEditorVsoInfo vsoInfo; memset( &vsoInfo, 0, sizeof vsoInfo );
	BkEditorVec3 vsoPoint = { 10.0f, 10.0f, 0.0f };
	BkEditorBridgeDescriptor bridgeDescriptor; memset( &bridgeDescriptor, 0, sizeof bridgeDescriptor );
	BkEditorPlannedPiece plannedPiece; memset( &plannedPiece, 0, sizeof plannedPiece );
	BkEditorFenceDescriptor fenceDescriptor; memset( &fenceDescriptor, 0, sizeof fenceDescriptor );
	BkEditorEntrenchmentInfo trenchInfo; memset( &trenchInfo, 0, sizeof trenchInfo );
	BkEditorVec3 trenchPoints[2] = { { 0.0f, 0.0f, 0.0f }, { 100.0f, 0.0f, 0.0f } };
	BkEditorBridgeInfo bridgeInfo; memset( &bridgeInfo, 0, sizeof bridgeInfo );
	BkEditorPaintCell cell = { 0, 0, 0 };
	BkEditorAltitudeRegion altRegion = { 0, 0, 1, 1 };
	BkEditorNewMapParams newMapParams; memset( &newMapParams, 0, sizeof newMapParams );
	newMapParams.size_x = 8;
	newMapParams.size_y = 8;
	BkEditorView view; memset( &view, 0, sizeof view );
	BkEditorPathSet paths; memset( &paths, 0, sizeof paths );
	BkEditorTile tileInfo; memset( &tileInfo, 0, sizeof tileInfo );
	BkEditorObjectFieldsEdit fieldsEdit; memset( &fieldsEdit, 0, sizeof fieldsEdit );
	fieldsEdit.mask = 1;
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
		{ "BkEditorAltitudes", [&] { return BkEditorAltitudes( 0, &altRegion, &fFloat, 1, &nInt ); } },
		{ "BkEditorSetAltitudes", [&] { return BkEditorSetAltitudes( 0, &altRegion, &fFloat, 1, &nInt ); } },
		{ "BkEditorNewMap", [&] { return BkEditorNewMap( 0, &newMapParams, &summary ); } },
		{ "BkEditorEngineTile", [&] { return BkEditorEngineTile( 0, 0, 0, &cChar ); } },
		{ "BkEditorTilesetTiles", [&] { return BkEditorTilesetTiles( 0, &cChar, 1, &nInt ); } },
		{ "BkEditorWorldToTile", [&] { return BkEditorWorldToTile( 0, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorTerrainMatchesEngine", [&] { return BkEditorTerrainMatchesEngine( 0 ); } },
		{ "BkEditorWorldMatchesMap", [&] { return BkEditorWorldMatchesMap( 0 ); } },
		{ "BkEditorCatalogue", [&] { return BkEditorCatalogue( 0, catEntries, 1, &nInt ); } },
		{ "BkEditorObjectPicture", [&] { return BkEditorObjectPicture( 0, "x", rgba, sizeof rgba, 16, &nInt, &nInt2 ); } },
		{ "BkEditorCloseMap", [&] { return BkEditorCloseMap( 0 ); } },
		{ "BkEditorDescribeTile", [&] { return BkEditorDescribeTile( 0, 0, &tileInfo ); } },
		{ "BkEditorTilePicture", [&] { return BkEditorTilePicture( 0, 0, rgba, sizeof rgba, 16, &nInt, &nInt2 ); } },
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
		{ "BkEditorPickObjects", [&] { return BkEditorPickObjects( 0, 0, 0, 0, 0, &nInt, 1, &nInt2 ); } },
		{ "BkEditorPickObjectsInTiles", [&] { return BkEditorPickObjectsInTiles( 0, 0, 0, 0, 0, &nInt, 1, &nInt2 ); } },
		{ "BkEditorMoveObjects", [&] { return BkEditorMoveObjects( 0, &nInt, 1, 0.0f, 0.0f, &nInt2 ); } },
		{ "BkEditorScreenToWorld", [&] { return BkEditorScreenToWorld( 0, 0, 0, &fFloat, &fFloat2 ); } },
		{ "BkEditorWorldToScreen", [&] { return BkEditorWorldToScreen( 0, 0, 0, &fFloat, &fFloat2 ); } },
		{ "BkEditorWorldToMap", [&] { return BkEditorWorldToMap( 0, 0, 0, &fFloat, &fFloat2 ); } },
		{ "BkEditorSetMapType", [&] { return BkEditorSetMapType( 0, 0 ); } },
		{ "BkEditorSetAttackingSide", [&] { return BkEditorSetAttackingSide( 0, 0 ); } },
		{ "BkEditorSounds", [&] { return BkEditorSounds( 0, &soundRecord, 1, &nInt ); } },
		{ "BkEditorAddSound", [&] { return BkEditorAddSound( 0, 0, &soundRecord ); } },
		{ "BkEditorSetSound", [&] { return BkEditorSetSound( 0, 0, &soundRecord ); } },
		{ "BkEditorDeleteSound", [&] { return BkEditorDeleteSound( 0, 0 ); } },
		{ "BkEditorCameraAnchors", [&] { return BkEditorCameraAnchors( 0, &anchorRecord ); } },
		{ "BkEditorSetCameraAnchors", [&] { return BkEditorSetCameraAnchors( 0, &anchorRecord ); } },
		{ "BkEditorGroundHeight", [&] { return BkEditorGroundHeight( 0, 0, 0, &fFloat ); } },
		{ "BkEditorSetObjectScriptID", [&] { return BkEditorSetObjectScriptID( 0, 0, 0 ); } },
		{ "BkEditorSetObjectFields", [&] { return BkEditorSetObjectFields( 0, 0, &fieldsEdit, &nInt ); } },
		{ "BkEditorCanLink", [&] { return BkEditorCanLink( 0, 0, 0, &nInt ); } },
		{ "BkEditorSetLink", [&] { return BkEditorSetLink( 0, 0, 0, &nInt ); } },
		{ "BkEditorUnlink", [&] { return BkEditorUnlink( 0, 0, &nInt ); } },
		{ "BkEditorDamageObject", [&] { return BkEditorDamageObject( 0, 0, 0.1f, 0, &nInt ); } },
		{ "BkEditorGroupIDs", [&] { return BkEditorGroupIDs( 0, &nInt, 1, &nInt2 ); } },
		{ "BkEditorGroup", [&] { return BkEditorGroup( 0, 0, &nInt, 1, &nInt2 ); } },
		{ "BkEditorSetGroup", [&] { return BkEditorSetGroup( 0, 0, &nInt, 1 ); } },
		{ "BkEditorDeleteGroup", [&] { return BkEditorDeleteGroup( 0, 0 ); } },
		{ "BkEditorFirstFreeGroupID", [&] { return BkEditorFirstFreeGroupID( 0, 0, &nInt ); } },
		{ "BkEditorSetHiddenScriptIDs", [&] { return BkEditorSetHiddenScriptIDs( 0, &nInt, 1 ); } },
		{ "BkEditorUndoEdit", [&] { return BkEditorUndoEdit( 0, 0 ); } },
		{ "BkEditorRedoEdit", [&] { return BkEditorRedoEdit( 0, 0 ); } },
		{ "BkEditorVsoDescriptors", [&] { return BkEditorVsoDescriptors( 0, 0, &vsoDescriptor, 1, &nInt ); } },
		{ "BkEditorVsoCount", [&] { return BkEditorVsoCount( 0, 0, &nInt ); } },
		{ "BkEditorVso", [&] { return BkEditorVso( 0, 0, 0, &vsoInfo, 0, 0, 0, 0 ); } },
		{ "BkEditorAddVso", [&] { return BkEditorAddVso( 0, 0, "x", &vsoPoint, 1, 3.0f, 1.0f, &nInt, &nInt2 ); } },
		{ "BkEditorVsoMatchesEngine", [&] { return BkEditorVsoMatchesEngine( 0 ); } },
		{ "BkEditorDeleteVso", [&] { return BkEditorDeleteVso( 0, 0, 0, &nInt ); } },
		{ "BkEditorMoveVsoPoints", [&] { return BkEditorMoveVsoPoints( 0, 0, 0, &vsoPoint, 1, &nInt ); } },
		{ "BkEditorSetVsoWidth", [&] { return BkEditorSetVsoWidth( 0, 0, 0, 0, 10.0f, 0, &nInt ); } },
		{ "BkEditorSetVsoOpacity", [&] { return BkEditorSetVsoOpacity( 0, 0, 0, 0, 0.5f, 0, &nInt ); } },
		{ "BkEditorInsertVsoPoint", [&] { return BkEditorInsertVsoPoint( 0, 0, 0, 0, &nInt ); } },
		{ "BkEditorDeleteVsoPoint", [&] { return BkEditorDeleteVsoPoint( 0, 0, 0, 0, &nInt ); } },
		{ "BkEditorPickVso", [&] { return BkEditorPickVso( 0, 0, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorBridgeDescriptors", [&] { return BkEditorBridgeDescriptors( 0, &bridgeDescriptor, 1, &nInt ); } },
		{ "BkEditorPlanBridge", [&] { return BkEditorPlanBridge( 0, "x", 0, 0, 0, 0, &plannedPiece, 1, &nInt ); } },
		{ "BkEditorDrawBridge", [&] { return BkEditorDrawBridge( 0, "x", 0, 0, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorBridges", [&] { return BkEditorBridges( 0, &bridgeInfo, 1, &nInt ); } },
		{ "BkEditorPickGroup", [&] { return BkEditorPickGroup( 0, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorDeleteBridge", [&] { return BkEditorDeleteBridge( 0, 0, &nInt ); } },
		{ "BkEditorRotateBridge", [&] { return BkEditorRotateBridge( 0, 0, &nInt ); } },
		{ "BkEditorToggleBridgeBuild", [&] { return BkEditorToggleBridgeBuild( 0, 0, &nInt ); } },
		{ "BkEditorWorldToAITile", [&] { return BkEditorWorldToAITile( 0, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorFenceDescriptors", [&] { return BkEditorFenceDescriptors( 0, &fenceDescriptor, 1, &nInt ); } },
		{ "BkEditorPlanFences", [&] { return BkEditorPlanFences( 0, "x", 0, 0, 0, 0, 0, &plannedPiece, 1, &nInt ); } },
		{ "BkEditorDrawFences", [&] { return BkEditorDrawFences( 0, "x", 0, 0, 0, 0, 0, &nInt ); } },
		{ "BkEditorPlanEntrenchment", [&] { return BkEditorPlanEntrenchment( 0, trenchPoints, 2, &plannedPiece, 1, &nInt ); } },
		{ "BkEditorDrawEntrenchment", [&] { return BkEditorDrawEntrenchment( 0, trenchPoints, 2, 0, &nInt, &nInt2 ); } },
		{ "BkEditorEntrenchments", [&] { return BkEditorEntrenchments( 0, &trenchInfo, 1, &nInt ); } },
		{ "BkEditorDeleteEntrenchment", [&] { return BkEditorDeleteEntrenchment( 0, 0, &nInt ); } },
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
		{ "BkEditorMoveObjects", [&] { return BkEditorMoveObjects( pSession, &nInt, 1, 0.0f, 0.0f, &nInt2 ); } },
		{ "BkEditorPickObjects", [&] { return BkEditorPickObjects( pSession, 0, 0, 0, 0, &nInt, 1, &nInt2 ); } },
		{ "BkEditorPickObjectsInTiles", [&] { return BkEditorPickObjectsInTiles( pSession, 0, 0, 0, 0, &nInt, 1, &nInt2 ); } },
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
		{ "BkEditorAltitudes", [&] { return BkEditorAltitudes( pSession, &altRegion, &fFloat, 1, &nInt ); } },
		{ "BkEditorSetAltitudes", [&] { return BkEditorSetAltitudes( pSession, &altRegion, &fFloat, 1, &nInt ); } },
		{ "BkEditorEngineTile", [&] { return BkEditorEngineTile( pSession, 0, 0, &cChar ); } },
		{ "BkEditorTilesetTiles", [&] { return BkEditorTilesetTiles( pSession, &cChar, 1, &nInt ); } },
		{ "BkEditorDescribeTile", [&] { return BkEditorDescribeTile( pSession, 0, &tileInfo ); } },
		{ "BkEditorTilePicture", [&] { return BkEditorTilePicture( pSession, 0, rgba, sizeof rgba, 16, &nInt, &nInt2 ); } },
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
		{ "BkEditorCameraAnchors", [&] { return BkEditorCameraAnchors( pSession, &anchorRecord ); } },
		{ "BkEditorSetCameraAnchors", [&] { return BkEditorSetCameraAnchors( pSession, &anchorRecord ); } },
		{ "BkEditorGroundHeight", [&] { return BkEditorGroundHeight( pSession, 0, 0, &fFloat ); } },
		{ "BkEditorSetObjectScriptID", [&] { return BkEditorSetObjectScriptID( pSession, 0, 0 ); } },
		{ "BkEditorGroupIDs", [&] { return BkEditorGroupIDs( pSession, &nInt, 1, &nInt2 ); } },
		{ "BkEditorGroup", [&] { return BkEditorGroup( pSession, 0, &nInt, 1, &nInt2 ); } },
		{ "BkEditorSetGroup", [&] { return BkEditorSetGroup( pSession, 0, &nInt, 1 ); } },
		{ "BkEditorDeleteGroup", [&] { return BkEditorDeleteGroup( pSession, 0 ); } },
		{ "BkEditorFirstFreeGroupID", [&] { return BkEditorFirstFreeGroupID( pSession, 0, &nInt ); } },
		{ "BkEditorSetHiddenScriptIDs", [&] { return BkEditorSetHiddenScriptIDs( pSession, &nInt, 1 ); } },
		{ "BkEditorUndoEdit", [&] { return BkEditorUndoEdit( pSession, 0 ); } },
		{ "BkEditorRedoEdit", [&] { return BkEditorRedoEdit( pSession, 0 ); } },
		{ "BkEditorVsoDescriptors", [&] { return BkEditorVsoDescriptors( pSession, 0, &vsoDescriptor, 1, &nInt ); } },
		{ "BkEditorVsoCount", [&] { return BkEditorVsoCount( pSession, 0, &nInt ); } },
		{ "BkEditorVso", [&] { return BkEditorVso( pSession, 0, 0, &vsoInfo, 0, 0, 0, 0 ); } },
		{ "BkEditorAddVso", [&] { return BkEditorAddVso( pSession, 0, "x", &vsoPoint, 1, 3.0f, 1.0f, &nInt, &nInt2 ); } },
		{ "BkEditorVsoMatchesEngine", [&] { return BkEditorVsoMatchesEngine( pSession ); } },
		{ "BkEditorDeleteVso", [&] { return BkEditorDeleteVso( pSession, 0, 0, &nInt ); } },
		{ "BkEditorMoveVsoPoints", [&] { return BkEditorMoveVsoPoints( pSession, 0, 0, &vsoPoint, 1, &nInt ); } },
		{ "BkEditorSetVsoWidth", [&] { return BkEditorSetVsoWidth( pSession, 0, 0, 0, 10.0f, 0, &nInt ); } },
		{ "BkEditorSetVsoOpacity", [&] { return BkEditorSetVsoOpacity( pSession, 0, 0, 0, 0.5f, 0, &nInt ); } },
		{ "BkEditorInsertVsoPoint", [&] { return BkEditorInsertVsoPoint( pSession, 0, 0, 0, &nInt ); } },
		{ "BkEditorDeleteVsoPoint", [&] { return BkEditorDeleteVsoPoint( pSession, 0, 0, 0, &nInt ); } },
		{ "BkEditorPickVso", [&] { return BkEditorPickVso( pSession, 0, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorBridgeDescriptors", [&] { return BkEditorBridgeDescriptors( pSession, &bridgeDescriptor, 1, &nInt ); } },
		{ "BkEditorPlanBridge", [&] { return BkEditorPlanBridge( pSession, "x", 0, 0, 0, 0, &plannedPiece, 1, &nInt ); } },
		{ "BkEditorDrawBridge", [&] { return BkEditorDrawBridge( pSession, "x", 0, 0, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorBridges", [&] { return BkEditorBridges( pSession, &bridgeInfo, 1, &nInt ); } },
		{ "BkEditorPickGroup", [&] { return BkEditorPickGroup( pSession, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorDeleteBridge", [&] { return BkEditorDeleteBridge( pSession, 0, &nInt ); } },
		{ "BkEditorRotateBridge", [&] { return BkEditorRotateBridge( pSession, 0, &nInt ); } },
		{ "BkEditorToggleBridgeBuild", [&] { return BkEditorToggleBridgeBuild( pSession, 0, &nInt ); } },
		{ "BkEditorWorldToAITile", [&] { return BkEditorWorldToAITile( pSession, 0, 0, &nInt, &nInt2 ); } },
		{ "BkEditorFenceDescriptors", [&] { return BkEditorFenceDescriptors( pSession, &fenceDescriptor, 1, &nInt ); } },
		{ "BkEditorPlanFences", [&] { return BkEditorPlanFences( pSession, "x", 0, 0, 0, 0, 0, &plannedPiece, 1, &nInt ); } },
		{ "BkEditorDrawFences", [&] { return BkEditorDrawFences( pSession, "x", 0, 0, 0, 0, 0, &nInt ); } },
		{ "BkEditorPlanEntrenchment", [&] { return BkEditorPlanEntrenchment( pSession, trenchPoints, 2, &plannedPiece, 1, &nInt ); } },
		{ "BkEditorDrawEntrenchment", [&] { return BkEditorDrawEntrenchment( pSession, trenchPoints, 2, 0, &nInt, &nInt2 ); } },
		{ "BkEditorEntrenchments", [&] { return BkEditorEntrenchments( pSession, &trenchInfo, 1, &nInt ); } },
		{ "BkEditorDeleteEntrenchment", [&] { return BkEditorDeleteEntrenchment( pSession, 0, &nInt ); } },
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
	// File > Close (03-15 gap fix) with nothing open is not a refusal.
	Check( BkEditorCloseMap( pSession ) == BK_EDITOR_OK, "BkEditorCloseMap with no map open is OK (nothing to close)" );

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
	add.nLinkWith = 0;		// a palette add is linked with nothing (see SAddObject::nLinkWith)
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
	add.nLinkWith = 0; // a palette add is linked with nothing (see SAddObject::nLinkWith)
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
	add.nLinkWith = 0;	// a palette add is linked with nothing (see SAddObject::nLinkWith)
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
// preservation invariant writes it back unchanged, so no edit may reach it -
// but its removal, which Check Map offers explicitly (05-05, D-33).
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
	Check( BkEditorMoveObject( pSession, nLinkID, record.x + 32, record.y ) == BK_EDITOR_REFUSED, "its move is refused" );
	// WR-A05: and its script ID, which the save would otherwise write back changed.
	Check( BkEditorSetObjectScriptID( pSession, nLinkID, record.script_id == 7 ? 8 : 7 ) == BK_EDITOR_REFUSED, "and so is a script ID for it" );

	// Its removal is the one edit allowed (05-05, D-33: Check Map's explicit fix,
	// PARITY F4): the record leaves the map and the restore brings it back as it
	// was - the save below is still the map that was read.
	{
		int nBefore = 0;
		BkEditorObjects( pSession, 0, 0, &nBefore );
		Check( BkEditorDeleteObject( pSession, nLinkID ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		int nAfter = 0;
		BkEditorObjects( pSession, 0, 0, &nAfter );
		Check( nAfter == nBefore - 1, "the unknown object's removal takes exactly it" );
		Check( BkEditorRestoreObject( pSession, nLinkID ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		BkEditorObjects( pSession, 0, 0, &nAfter );
		Check( nAfter == nBefore, "and its restore brings it back" );
		BkEditorObjectRecord again;
		int nOne = 0;
		BkEditorObjects( pSession, &again, 1, &nOne );
		Check( again.link_id == nLinkID && again.known == 0 && again.x == record.x && again.y == record.y, "in the place it held, still unknown" );
	}

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
//
// The build now generates the missing season textures (SeasonData, mounted
// over Data - StreamIO/SeasonData.h), so the M2A1 has a 1w in a staged game.
// The fallback is checked with that mount taken away, which leaves the M2A1
// with no winter texture whether or not the build generated one; then, with
// it mounted again, a second M2A1 must be drawn with the generated "1w".
static void TestMissingSeasonTextureFallsBack( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	static const char *const placed[] = { "105mm_M2A1_USA", "10.5-cm_Flak38" };
	static const char *const wanted[] = { "units\\technics\\allies\\artillery\\105mm_m2a1_usa\\1",
	                                      "units\\technics\\german\\artillery\\10_5_cm_flak38\\1w" };
	static const char *const pszM2A1Winter = "units\\technics\\allies\\artillery\\105mm_m2a1_usa\\1w_h.dds";
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( !Check( pStorage != 0, "the data storage is registered" ) )
		return;
	// Remounted on every way out, so the tests after this one see the
	// installation's storage as the game would.
	struct SRemount
	{
		IDataStorage *pStorage;
		bool bMounted;
		~SRemount() { if ( bMounted ) NSeasonData::Mount( pStorage ); }
	} remount = { pStorage, NSeasonData::Unmount( pStorage ) };
	printf( "editor-bridge: SeasonData %s for the fallback check\n", remount.bMounted ? "unmounted" : "is not mounted" );
	if ( !Check( !pStorage->IsStreamExist( pszM2A1Winter ),
	             "the M2A1 has no winter texture in Data itself (generated season textures belong in SeasonData, never in Data)" ) )
		return;
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

	// With SeasonData mounted again, a new M2A1 is drawn with the generated
	// winter texture. A texture is looked up when its unit is built, and the
	// failed lookup above is not cached, so no reload is needed.
	if ( !remount.bMounted )
	{
		printf( "editor-bridge: this installation has no SeasonData; the generated winter texture is not checked\n" );
		return;
	}
	remount.bMounted = false;
	if ( !Check( NSeasonData::Mount( pStorage ), "SeasonData mounts again" ) ||
	     !Check( pStorage->IsStreamExist( pszM2A1Winter ), "SeasonData has the M2A1's generated winter texture" ) )
		return;
	CVec2 vGenerated;
	bool bPlaced = false;
	for ( ; nSpot < spots.size() && !bPlaced; ++nSpot )
	{
		if ( fabs( spots[nSpot].x - vPlacedAt[0].x ) + fabs( spots[nSpot].y - vPlacedAt[0].y ) < 160.0f ||
		     fabs( spots[nSpot].x - vPlacedAt[1].x ) + fabs( spots[nSpot].y - vPlacedAt[1].y ) < 160.0f )
			continue;
		float wx = 0.0f, wy = 0.0f, mx = 0.0f, my = 0.0f;
		int nLinkID = -1;
		bPlaced = BkEditorScreenToWorld( pSession, spots[nSpot].x, spots[nSpot].y, &wx, &wy ) == BK_EDITOR_OK &&
		          BkEditorWorldToMap( pSession, wx, wy, &mx, &my ) == BK_EDITOR_OK &&
		          BkEditorAddObject( pSession, placed[0], mx, my, 0, 0, &nLinkID ) == BK_EDITOR_OK;
		if ( bPlaced )
			vGenerated = spots[nSpot];
	}
	if ( !Check( bPlaced, NStr::Format( "a second %s is placed on bare ground on %s", placed[0], SHIPPED_MAP ) ) )
		return;
	for ( int i = 0; i < 4; ++i )
		BkEditorFrame( pSession );
	const std::string szGeneratedWanted = std::string( wanted[0] ) + "w";
	const std::vector<std::string> names = UnitMeshTexturesIn( CTRect<float>( vGenerated.x - 40.0f, vGenerated.y - 80.0f, vGenerated.x + 40.0f, vGenerated.y + 16.0f ) );
	bool bAllGenerated = !names.empty();
	std::string szNames;
	for ( size_t j = 0; j < names.size(); ++j )
	{
		bAllGenerated = bAllGenerated && names[j] == szGeneratedWanted;
		if ( szNames.size() < 300 )
			szNames += " " + names[j];
	}
	printf( "editor-bridge: with SeasonData the placed %s at %.0f,%.0f is drawn with:%s\n", placed[0], vGenerated.x, vGenerated.y, szNames.c_str() );
	Check( bAllGenerated, NStr::Format( "with SeasonData the placed %s is drawn with %s (drawn with:%s)", placed[0], szGeneratedWanted.c_str(), szNames.c_str() ) );
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

// D-19 (M3): altitudes end to end through the bridge. A ramp goes in through
// BkEditorSetAltitudes, the saved map equals the map the same functions build
// (SetAltitudeRegion + UpdateTerrainShades over GrowForShades), undo puts the
// recorded region back so exactly that the next save is the unedited file byte
// for byte, and every caller bug - a non-finite height, an empty region, a
// count mismatch - is BAD_ARGUMENT while a region off the map is REFUSED, and
// none of them moves anything.
static bool FileBytes( const char *pszPath, std::vector<unsigned char> *pBytes )
{
	// The OS's own separator: the engine's streams split on '\\' and the
	// tests spell paths that way, but std::ifstream does not.
	std::string szPath( pszPath );
	std::replace( szPath.begin(), szPath.end(), '\\', '/' );
	std::ifstream file( szPath.c_str(), std::ios::binary );
	if ( !file )
		return false;
	file.seekg( 0, std::ios::end );
	const std::streamoff nSize = file.tellg();
	if ( nSize < 0 )
		return false;
	file.seekg( 0, std::ios::beg );
	pBytes->resize( size_t( nSize > 0 ? nSize : 0 ) );
	if ( nSize > 0 && !file.read( reinterpret_cast<char*>( &( *pBytes )[0] ), nSize ) )
		return false;
	return true;
}

static bool BridgeFilesAreIdentical( const char *pszLeft, const char *pszRight )
{
	std::vector<unsigned char> left, right;
	if ( !FileBytes( pszLeft, &left ) || !FileBytes( pszRight, &right ) )
		return false;
	if ( left == right )
		return true;
	// Kept for whoever has to look: where the two files first differ, with
	// the bytes around it (printable as text when they are text - a BZM's
	// sections name themselves, so the neighbourhood usually says which).
	size_t i = 0;
	while ( i < left.size() && i < right.size() && left[i] == right[i] )
		++i;
	printf( "editor-bridge: (identical? size %zu vs %zu, first difference at %zu: %02x vs %02x)\n",
	        left.size(), right.size(), i, i < left.size() ? left[i] : 0, i < right.size() ? right[i] : 0 );
	const size_t nFrom = i > 48 ? i - 48 : 0;
	const size_t nTo = Min( i + 48, Min( left.size(), right.size() ) );
	for ( size_t k = nFrom; k < nTo; ++k )
		printf( "editor-bridge:   %8zu %02x %02x %c%c\n", k, left[k], right[k],
	        ( left[k] >= 32 && left[k] < 127 ) ? left[k] : '.', ( right[k] >= 32 && right[k] < 127 ) ? right[k] : '.' );
	return false;
}

static void TestM3Altitudes( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int nSizeX = original.terrain.altitudes.GetSizeX(), nSizeY = original.terrain.altitudes.GetSizeY();
	if ( !Check( nSizeX > 20 && nSizeY > 20, "coldwinter has an altitude sheet big enough for an interior region" ) )
		return;
	const BkEditorAltitudeRegion region = { 8, 8, 16, 16 };
	const int nArea = ( region.x1 - region.x0 ) * ( region.y1 - region.y0 );
	std::vector<float> ramp( nArea );
	for ( int y = 0; y < region.y1 - region.y0; ++y )
		for ( int x = 0; x < region.x1 - region.x0; ++x )
			ramp[size_t( y * ( region.x1 - region.x0 ) + x )] = 32.0f * float( x + y );

	// The read agrees with the file the map came from.
	std::vector<float> read_back( nArea );
	int nCount = 0;
	Check( BkEditorAltitudes( pSession, &region, &( read_back[0] ), nArea, &nCount ) == BK_EDITOR_OK,
	       BkEditorLastMessage( pSession ) );
	Check( nCount == nArea, "the read counts the region's vertices" );
	bool bSameAsFile = true;
	for ( int i = 0; i < nArea && bSameAsFile; ++i )
	{
		const int nX = region.x0 + i % ( region.x1 - region.x0 );
		const int nY = region.y0 + i / ( region.x1 - region.x0 );
		bSameAsFile = read_back[size_t( i )] == original.terrain.altitudes[nY][nX].fHeight;
	}
	Check( bSameAsFile, "the heights read are the file's" );
	// The two-pass rule: a short buffer is refused with the total answered.
	nCount = 0;
	Check( BkEditorAltitudes( pSession, &region, 0, 0, &nCount ) == BK_EDITOR_REFUSED,
	       "a short buffer is refused" );
	Check( nCount == nArea, "and still answers the total" );

	// The edit, through the bridge. The unedited save is the SESSION's own,
	// taken before anything changes: the snapshot's altitude padding bytes
	// are its own heap's from the open's copy (the 04-01 raw-struct rule),
	// so a byte compare against another read's write would measure the
	// allocator, not the edit. Against this save the undo has to be exact -
	// nothing outside the record ever moves.
	const std::string szEdited = szScratch + "\\m3-altitudes-edited.bzm";
	const std::string szUndone = szScratch + "\\m3-altitudes-undone.bzm";
	const std::string szUnedited = szScratch + "\\m3-altitudes-unedited.bzm";
	Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	int nToken = -1;
	Check( BkEditorSetAltitudes( pSession, &region, &( ramp[0] ), nArea, &nToken ) == BK_EDITOR_OK,
	       BkEditorLastMessage( pSession ) );
	Check( nToken >= 0, "the edit has a token" );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// The expected map: the same functions on a fresh read.
	CMapInfo expected;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
		return;
	std::vector<SVertexAltitude> values( nArea );
	for ( int y = 0; y < region.y1 - region.y0; ++y )
		for ( int x = 0; x < region.x1 - region.x0; ++x )
		{
			const int nX = region.x0 + x, nY = region.y0 + y;
			const size_t nAt = size_t( y * ( region.x1 - region.x0 ) + x );
			// Bitwise, so the padding bytes are the map's own (the
			// raw-struct rule); only the height is the test's.
			memcpy( &values[nAt], &expected.terrain.altitudes[nY][nX], sizeof( SVertexAltitude ) );
			values[nAt].fHeight = 32.0f * float( x + y );
		}
	const CTRect<int> rEdit( region.x0, region.y0, region.x1, region.y1 );
	const CTRect<int> rGrown = NMapOverlay::GrowForShades( expected, rEdit );
	Check( NMapOverlay::SetAltitudeRegion( &expected, rEdit, values, 0 ), "the expected map takes the region" );
	Check( CMapInfo::UpdateTerrainShades( &expected.terrain, rGrown,
	       CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( expected.nSeason ) ) ),
	       "the expected map's shades update" );

	CMapInfo reread;
	std::string szWhere;
	if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( NMapFile::Read( szEdited.c_str(), &reread, &szError ), szError.c_str() ) )
	{
		Check( NMapFile::AreEquivalent( expected, reread, &szWhere ),
		       szWhere.empty() ? "the saved edit equals the same-function expected map"
		                       : ( "the altitude edit differs at " + szWhere ).c_str() );
		szWhere.clear();
		Check( !NMapFile::AreEquivalent( original, reread, &szWhere ), "and the comparator sees the altitude edit" );
	}

	// Undo puts the recorded region back raw; the file it writes is the
	// session's unedited save, byte for byte.
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		Check( BridgeFilesAreIdentical( szUnedited.c_str(), szUndone.c_str() ),
		       "an altitude edit undone writes the unedited save byte for byte" );
	}
	// Redo, and the ramp is back.
	Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	nCount = 0;
	Check( BkEditorAltitudes( pSession, &region, &( read_back[0] ), nArea, &nCount ) == BK_EDITOR_OK,
	       BkEditorLastMessage( pSession ) );
	bool bRampBack = true;
	for ( int i = 0; i < nArea && bRampBack; ++i )
		bRampBack = read_back[size_t( i )] == ramp[size_t( i )];
	Check( bRampBack, "redo restores the ramp" );
	// Leave the map as it was opened.
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// The refusals: caller bugs are BAD_ARGUMENT, a region off the map is
	// REFUSED, and each leaves the read exactly as it was. The off-map
	// region's count still has to match it - the ABI orders its checks that
	// way, count before bounds.
	std::vector<float> before( nArea );
	nCount = 0;
	Check( BkEditorAltitudes( pSession, &region, &( before[0] ), nArea, &nCount ) == BK_EDITOR_OK,
	       BkEditorLastMessage( pSession ) );
	std::vector<float> bad( ramp );
	bad[size_t( nArea / 2 )] = std::numeric_limits<float>::quiet_NaN();
	Check( BkEditorSetAltitudes( pSession, &region, &( bad[0] ), nArea, &nToken ) == BK_EDITOR_BAD_ARGUMENT,
	       "a non-finite height is BAD_ARGUMENT" );
	const BkEditorAltitudeRegion empty = { 8, 8, 8, 16 };
	Check( BkEditorSetAltitudes( pSession, &empty, &( ramp[0] ), nArea, &nToken ) == BK_EDITOR_BAD_ARGUMENT,
	       "an empty region is BAD_ARGUMENT" );
	Check( BkEditorSetAltitudes( pSession, &region, &( ramp[0] ), nArea + 1, &nToken ) == BK_EDITOR_BAD_ARGUMENT,
	       "a count mismatch is BAD_ARGUMENT" );
	std::vector<float> offMapRamp( 16, 64.0f );
	const BkEditorAltitudeRegion offMap = { nSizeX - 2, nSizeY - 2, nSizeX + 2, nSizeY + 2 };
	Check( BkEditorSetAltitudes( pSession, &offMap, &( offMapRamp[0] ), 16, &nToken ) == BK_EDITOR_REFUSED,
	       ( std::string( "an off-map region is REFUSED: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorAltitudes( pSession, &offMap, &( read_back[0] ), nArea, &nCount ) == BK_EDITOR_REFUSED,
	       "an off-map read is REFUSED too" );
	nCount = 0;
	Check( BkEditorAltitudes( pSession, &region, &( read_back[0] ), nArea, &nCount ) == BK_EDITOR_OK,
	       BkEditorLastMessage( pSession ) );
	Check( std::equal( before.begin(), before.end(), read_back.begin() ),
	       "and the refusals changed nothing the read can see" );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	remove( szEdited.c_str() );
	remove( szUndone.c_str() );
	remove( szUnedited.c_str() );
	printf( "editor-bridge: M3 altitudes ok\n" );
}

// File > New (M3, D-23) end to end: the engine builds the map (Create, every
// tile the season's most common tile, zero altitudes, the season's shades),
// it opens as a never-saved document, saves in either format, a caller bug is
// BAD_ARGUMENT and builds nothing, and the current mod's new map carries the
// mod's name and version like the M1 save (D-28). F5 (D-23) rides along: a
// crafted map whose file lacks altitudes opens with a zero sheet of the right
// vertex count, and a save keeps it without disturbing anything else.
static void TestM3NewMap( BkEditorSession *pSession, const std::string &szScratch )
{
	BkEditorNewMapParams params;
	memset( &params, 0, sizeof params );
	params.size_x = 8;
	params.size_y = 8;
	params.season = 0;		// Summer
	strcpy( params.szName, "m3_new_map" );
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( !Check( BkEditorNewMap( pSession, &params, &summary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( summary.width_tiles == 8 * 16 && summary.height_tiles == 8 * 16, "an 8x8-patch map is 128x128 tiles" );
	Check( summary.season == CMapInfo::REAL_SEASONS[0], "a Summer map's season is Summer's own real value" );
	Check( summary.player_count == 2, "a new map has the two default players" );
	Check( summary.object_count == 0 && summary.placed_object_count == 0, "and no objects" );

	// Every tile is the season's most common TERRAIN TYPE: what the MFC
	// editor's FillTerrain(MOST_COMMON_TILES[season]) wrote. GetMapsIndex
	// picks one of the type's tile variants by rand()
	// (fmtTerrain.h:STileTypeDesc, the same rand CreateRandomMap reseeds),
	// so the check is against the type's whole variant set, read back
	// through the engine's own terrain.
	STilesetDesc tilesetDesc;
	LoadDataResource( "terrain\\sets\\1\\tileset", "", false, 0, "tileset", tilesetDesc );
	if ( !Check( tilesetDesc.terrtypes.size() > size_t( CMapInfo::MOST_COMMON_TILES[0] ), "the Summer tileset lists the most common terrain type" ) )
		return;
	std::set<int> commonTiles;
	for ( size_t i = 0; i < tilesetDesc.terrtypes[CMapInfo::MOST_COMMON_TILES[0]].tiles.size(); ++i )
		commonTiles.insert( tilesetDesc.terrtypes[CMapInfo::MOST_COMMON_TILES[0]].tiles[i].nIndex );
	if ( !Check( !commonTiles.empty(), "the most common terrain type has tiles" ) )
		return;
	bool bAllCommon = true;
	for ( int y = 0; y < summary.height_tiles && bAllCommon; ++y )
		for ( int x = 0; x < summary.width_tiles; ++x )
		{
			unsigned char tile = 0;
			if ( BkEditorEngineTile( pSession, x, y, &tile ) != BK_EDITOR_OK || commonTiles.find( tile ) == commonTiles.end() )
			{
				bAllCommon = false;
				break;
			}
		}
	Check( bAllCommon, "every tile is one of Summer's most common terrain type's tiles" );

	// Zero altitudes, the whole vertex sheet.
	const BkEditorAltitudeRegion whole = { 0, 0, 8 * 16 + 1, 8 * 16 + 1 };
	const int nVertices = ( 8 * 16 + 1 ) * ( 8 * 16 + 1 );
	std::vector<float> heights( nVertices );
	int nCount = 0;
	if ( !Check( BkEditorAltitudes( pSession, &whole, &( heights[0] ), nVertices, &nCount ) == BK_EDITOR_OK,
	             BkEditorLastMessage( pSession ) ) )
		return;
	Check( nCount == nVertices, "the new map's vertex sheet is one more than the tiles per axis" );
	bool bAllZero = true;
	for ( int i = 0; i < nVertices; ++i )
		if ( heights[size_t( i )] != 0.0f )
		{
			bAllZero = false;
			break;
		}
	Check( bAllZero, "the new map's altitudes are zero" );

	// Either format: Save As .bzm and .xml read back equivalent to each other.
	const std::string szSavedBzm = szScratch + "\\m3-new-map.bzm";
	const std::string szSavedXml = szScratch + "\\m3-new-map.xml";
	Check( BkEditorSaveMap( pSession, szSavedBzm.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSaveMap( pSession, szSavedXml.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	CMapInfo fromBzm, fromXml;
	std::string szError, szWhere;
	if ( Check( NMapFile::Read( szSavedBzm.c_str(), &fromBzm, &szError ), szError.c_str() ) &&
	     Check( NMapFile::Read( szSavedXml.c_str(), &fromXml, &szError ), szError.c_str() ) )
		Check( NMapFile::AreEquivalent( fromBzm, fromXml, &szWhere ),
		       szWhere.empty() ? "the new map saves .bzm and .xml equivalent" : ( "the two formats differ at " + szWhere ).c_str() );

	// Caller bugs build nothing: the map that is open stays exactly as it is.
	const int nWidth = summary.width_tiles;
	BkEditorNewMapParams bad = params;
	bad.size_x = 0;
	Check( BkEditorNewMap( pSession, &bad, &summary ) == BK_EDITOR_BAD_ARGUMENT, "a size of 0 patches is BAD_ARGUMENT" );
	bad = params;
	bad.size_y = 33;
	Check( BkEditorNewMap( pSession, &bad, &summary ) == BK_EDITOR_BAD_ARGUMENT, "a size of 33 patches is BAD_ARGUMENT" );
	bad = params;
	bad.season = 4;
	Check( BkEditorNewMap( pSession, &bad, &summary ) == BK_EDITOR_BAD_ARGUMENT, "a season of 4 is BAD_ARGUMENT" );
	Check( BkEditorNewMap( pSession, 0, &summary ) == BK_EDITOR_BAD_ARGUMENT, "null params are BAD_ARGUMENT" );
	unsigned char tile = 0;
	Check( BkEditorEngineTile( pSession, 0, 0, &tile ) == BK_EDITOR_OK && BkEditorEngineTile( pSession, nWidth - 1, 0, &tile ) == BK_EDITOR_OK,
	       "and the refused builds left the map open" );

	// The current mod's new map carries the mod's name and version (D-28's
	// own stamp, at creation, like the MFC editor's new map).
	if ( BkEditorSetMod( pSession, "EditorTestMod" ) == BK_EDITOR_OK )
	{
		BkEditorMapSummary modSummary;
		memset( &modSummary, 0, sizeof modSummary );
		if ( Check( BkEditorNewMap( pSession, &params, &modSummary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			const std::string szModSaved = szScratch + "\\m3-new-map-mod.bzm";
			CMapInfo fromMod;
			szError.clear();
			if ( Check( BkEditorSaveMap( pSession, szModSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
			     Check( NMapFile::Read( szModSaved.c_str(), &fromMod, &szError ), szError.c_str() ) )
			{
				Check( fromMod.szMODName == "Editor Test Mod", "the current mod's new map carries the mod's name" );
				Check( fromMod.szMODVersion == "1.0", "and its version" );
			}
			remove( szModSaved.c_str() );
		}
		Check( BkEditorSetMod( pSession, 0 ) == BK_EDITOR_OK, "and the mod clears again" );
	}

	// F5: a map whose file lacks altitudes opens with a zero sheet of the
	// map's own vertex count, and saving keeps it without disturbing
	// anything else.
	CMapInfo crafted;
	szError.clear();
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &crafted, &szError ), szError.c_str() ) )
		return;
	crafted.terrain.altitudes.Clear();
	const std::string szNoAltitudes = szScratch + "\\m3-no-altitudes.bzm";
	if ( !Check( NMapFile::Write( szNoAltitudes.c_str(), crafted, &szError ), szError.c_str() ) )
		return;
	if ( Check( BkEditorOpenMap( pSession, szNoAltitudes.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		CMapInfo reopened;
		szError.clear();
		if ( Check( NMapFile::Read( szNoAltitudes.c_str(), &reopened, &szError ), szError.c_str() ) )
		{
			const int nMapVertices = ( reopened.terrain.tiles.GetSizeX() + 1 ) * ( reopened.terrain.tiles.GetSizeY() + 1 );
			std::vector<float> mapHeights( nMapVertices );
			const BkEditorAltitudeRegion mapWhole = { 0, 0, reopened.terrain.tiles.GetSizeX() + 1, reopened.terrain.tiles.GetSizeY() + 1 };
			nCount = 0;
			if ( Check( BkEditorAltitudes( pSession, &mapWhole, &( mapHeights[0] ), nMapVertices, &nCount ) == BK_EDITOR_OK,
			            BkEditorLastMessage( pSession ) ) )
			{
				Check( nCount == nMapVertices, "the crafted map's zero sheet has the map's own vertex count" );
				bool bZero = true;
				for ( int i = 0; i < nMapVertices; ++i )
					if ( mapHeights[size_t( i )] != 0.0f )
					{
						bZero = false;
						break;
					}
				Check( bZero, "the crafted map opens with zero altitudes" );
			}
			// And a save keeps the sheet without disturbing anything else.
			CMapInfo expected = reopened;
			expected.terrain.altitudes.SetSizes( reopened.terrain.patches.GetSizeX() * 16 + 1, reopened.terrain.patches.GetSizeY() * 16 + 1 );
			expected.terrain.altitudes.SetZero();
			const std::string szZeroSaved = szScratch + "\\m3-no-altitudes-saved.bzm";
			CMapInfo zeroSaved;
			szError.clear();
			szWhere.clear();
			if ( Check( BkEditorSaveMap( pSession, szZeroSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
			     Check( NMapFile::Read( szZeroSaved.c_str(), &zeroSaved, &szError ), szError.c_str() ) )
				Check( NMapFile::AreEquivalent( expected, zeroSaved, &szWhere ),
				       szWhere.empty() ? "a save keeps the zero sheet and nothing else moves" : ( "the saved sheet differs at " + szWhere ).c_str() );
			remove( szZeroSaved.c_str() );
		}
	}
 	remove( szNoAltitudes.c_str() );
 	remove( szSavedBzm.c_str() );
 	remove( szSavedXml.c_str() );
 	printf( "editor-bridge: M3 new map ok\n" );
 }

// The Heights machine (M3, D-18) end to end: raise, lower and level in all
// four modes against the same-function expected map (the engine's own
// gradient, pattern and shade functions on a fresh read - D-19's builder
// rule), the invalid-height rollback (nothing changes unless Ctrl is held),
// Generate for each of the MFC's three noise types, Set Zero, one token per
// step with byte-exact undo, and the ABI's own refusals.
//
// The builder mirrors session_terrain.cpp's arithmetic exactly - the same
// SVAGradient from editor\profile.tga, the same radial pattern, the same
// corner arithmetic and the same click-mode freeze - so a difference is a
// deviation, not a reimplementation.
struct SStrokeBuilder
{
	// The stroke-start cache, exactly the session's own (frozen on the step
	// that carries stroke_start).
	float fClickTileHeight;
	bool bClickTileValid;
	float fClickAverage;

	SStrokeBuilder() : fClickTileHeight( 0.0f ), bClickTileValid( false ), fClickAverage( 0.0f ) { }

	static float MaskAverageAt( const STerrainInfo &rTerrain, const SVAPattern &rMask, const CTPoint<int> &rCorner )
	{
		const CTRect<int> rBounds( 0, 0, rTerrain.altitudes.GetSizeX(), rTerrain.altitudes.GetSizeY() );
		CTRect<int> rRect( rCorner.x, rCorner.y, rCorner.x + rMask.heights.GetSizeX(), rCorner.y + rMask.heights.GetSizeY() );
		if ( ValidateIndices( rBounds, &rRect ) < 0 )
			return 0.0f;
		double fTotal = 0.0;
		int nCount = 0;
		for ( int nY = rRect.miny; nY < rRect.maxy; ++nY )
			for ( int nX = rRect.minx; nX < rRect.maxx; ++nX )
				if ( rMask.heights[nY - rCorner.y][nX - rCorner.x] != 0.0f )
				{
					fTotal += rTerrain.altitudes[nY][nX].fHeight;
					++nCount;
				}
		return ( nCount != 0 ) ? float( fTotal / nCount ) : 0.0f;
	}

	// One stroke step onto `pMap` (already read fresh); false with the reason
	// in rWhy when the pattern itself will not build (the image is missing).
	bool Step( CMapInfo *pMap, const BkEditorHeightsStrokeParams &rStroke, SVAPattern *pPattern, SVAPattern *pMask, std::string *pWhy )
	{
		STerrainInfo &rTerrain = pMap->terrain;
		CTPoint<int> tile;
		if ( !CMapInfo::GetTerrainTileIndices( rTerrain, CVec3( rStroke.pos_x, rStroke.pos_y, 0 ), &tile ) )
		{
			*pWhy = "the cursor is not over the map";
			return false;
		}
		const int nPatternSize = rStroke.brush * 2;
		const CTPoint<int> corner( tile.x - ( nPatternSize / 2 - 1 ), tile.y - ( nPatternSize / 2 - 1 ) );
		CTRect<int> rEdit( corner.x, corner.y, corner.x + nPatternSize, corner.y + nPatternSize );
		const CTRect<int> rBounds( 0, 0, rTerrain.altitudes.GetSizeX(), rTerrain.altitudes.GetSizeY() );
		if ( ValidateIndices( rBounds, &rEdit ) < 0 )
		{
			*pWhy = "the brush is not over the map";
			return false;
		}
		if ( rStroke.stroke_start != 0 )
		{
			CTPoint<int> refTile;
			if ( CMapInfo::GetTerrainTileIndices( rTerrain, CVec3( rStroke.click_x, rStroke.click_y, 0 ), &refTile ) )
			{
				bClickTileValid = true;
				fClickTileHeight = ( rTerrain.altitudes[refTile.y + 0][refTile.x + 0].fHeight +
				                      rTerrain.altitudes[refTile.y + 1][refTile.x + 0].fHeight +
				                      rTerrain.altitudes[refTile.y + 1][refTile.x + 1].fHeight +
				                      rTerrain.altitudes[refTile.y + 0][refTile.x + 1].fHeight ) / 4.0f;
				fClickAverage = MaskAverageAt( rTerrain, *pMask, CTPoint<int>( refTile.x - ( nPatternSize / 2 - 1 ), refTile.y - ( nPatternSize / 2 - 1 ) ) );
			}
			else
			{
				bClickTileValid = false;
				fClickTileHeight = 0.0f;
				fClickAverage = 0.0f;
			}
		}
		float fTarget = 0.0f;
		if ( rStroke.action == 2 )
		{
			switch ( rStroke.level_mode )
			{
				case 1: fTarget = fClickTileHeight; break;
				case 2: fTarget = MaskAverageAt( rTerrain, *pMask, corner ); break;
				case 3: fTarget = fClickAverage; break;
				default: fTarget = 0.0f; break;
			}
		}
		const float fRatio = rStroke.level_ratio_percent / 100.0f;
		const size_t nCount = size_t( rEdit.maxx - rEdit.minx ) * size_t( rEdit.maxy - rEdit.miny );
		std::vector<SVertexAltitude> values( nCount );
		size_t nValue = 0;
		for ( int nY = rEdit.miny; nY < rEdit.maxy; ++nY )
			for ( int nX = rEdit.minx; nX < rEdit.maxx; ++nX, ++nValue )
			{
				memcpy( &values[nValue], &rTerrain.altitudes[nY][nX], sizeof( SVertexAltitude ) );
				const float fAt = rTerrain.altitudes[nY][nX].fHeight;
				const float fPattern = pPattern->heights[nY - corner.y][nX - corner.x];
				float fHeight = fAt;
				if ( rStroke.action == 1 )
					fHeight = fAt - fPattern;
				else if ( rStroke.action == 2 )
				{
					if ( pMask->heights[nY - corner.y][nX - corner.x] != 0.0f )
						fHeight = fAt + ( fTarget - fAt ) * fRatio;
				}
				else
					fHeight = fAt + fPattern;
				values[nValue].fHeight = fHeight;
			}
		const CTRect<int> rGrown = NMapOverlay::GrowForShades( *pMap, rEdit );
		if ( !NMapOverlay::SetAltitudeRegion( pMap, rEdit, values, 0 ) ||
		     !CMapInfo::UpdateTerrainShades( &rTerrain, rGrown,
		       CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( pMap->nSeason ) ) ) )
		{
			*pWhy = "the expected map would not take the stroke";
			return false;
		}
		return true;
	}
};

static void TestM3Heights( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) ) return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) ) return;
	const int nTilesX = original.terrain.tiles.GetSizeX();
	const int nTilesY = original.terrain.tiles.GetSizeY();
	// The world point over an interior tile, and the round trip that says the
	// formula is the engine's own: a tile's centre in X from 0, in Y from the
	// far edge and reversed (GetTileIndicesInternal's isYReverse,
	// MapInfo_StaticMethods.cpp:63-66) - the view's own documented relationship.
	const int nCentreTileX = nTilesX / 2, nCentreTileY = nTilesY / 2;
	const float fCentreX = ( nCentreTileX + 0.5f ) * fWorldCellSize;
	const float fCentreY = ( nTilesY - nCentreTileY - 0.5f ) * fWorldCellSize;
	CTPoint<int> roundTrip;
	Check( CMapInfo::GetTerrainTileIndices( original.terrain, CVec3( fCentreX, fCentreY, 0 ), &roundTrip ) &&
	       roundTrip.x == nCentreTileX && roundTrip.y == nCentreTileY,
	       "the world point over the centre tile round-trips through the engine's own conversion" );

	const std::string szUnedited = szScratch + "\\m3-heights-unedited.bzm";
	const std::string szEdited = szScratch + "\\m3-heights-edited.bzm";
	const std::string szUndone = szScratch + "\\m3-heights-undone.bzm";
	Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// The engine's own pattern, the same objects the session builds.
	CPtr<IDataStream> pImageStream = GetSingleton<IDataStorage>()->OpenStream( "editor\\profile.tga", STREAM_ACCESS_READ );
	if ( !Check( pImageStream != 0, "editor\\profile.tga is in the data storage" ) ) return;
	CPtr<IImage> pImage = GetImageProcessor()->LoadImage( pImageStream );
	if ( !Check( pImage != 0, "editor\\profile.tga decodes" ) ) return;
	SVAGradient gradient;
	SVAPattern pattern, mask;
	gradient.CreateFromImage( pImage, CTPoint<float>( 0.0f, 1.0f ), CTPoint<float>( 0.0f, 2.0f ) );
	Check( pattern.CreateFromGradient( gradient, 6 ) && mask.CreateValue( 1.0f, 6 ), "the profile pattern and level mask build (brush 3)" );

	// One raise stroke of two steps: both tokens name edits of the log, the
	// engine holds what the map holds, and the saved file equals the
	// same-function expected map.
	BkEditorHeightsStrokeParams stroke;
	memset( &stroke, 0, sizeof stroke );
	stroke.action = 0;
	stroke.level_mode = 2;
	stroke.brush = 3;
	stroke.height_speed = 2.0f;
	stroke.level_ratio_percent = 3.0f;
	stroke.pos_x = fCentreX; stroke.pos_y = fCentreY;
	stroke.click_x = fCentreX; stroke.click_y = fCentreY;
	stroke.stroke_start = 1;
	int nToken1 = -1, nToken2 = -1;
	Check( BkEditorHeightsStroke( pSession, &stroke, &nToken1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( nToken1 >= 0, "the first step has a token" );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	stroke.stroke_start = 0;
	stroke.pos_x = fCentreX + fWorldCellSize; stroke.pos_y = fCentreY;
	Check( BkEditorHeightsStroke( pSession, &stroke, &nToken2 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( nToken2 == nToken1 + 1, "the second step is the next edit of the log" );

	{
		CMapInfo expected;
		szError.clear();
		if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) ) return;
		SStrokeBuilder builder;
		std::string szWhy;
		BkEditorHeightsStrokeParams step1 = stroke;
		step1.pos_x = fCentreX; step1.pos_y = fCentreY; step1.stroke_start = 1;
		if ( Check( builder.Step( &expected, step1, &pattern, &mask, &szWhy ) &&
	             builder.Step( &expected, stroke, &pattern, &mask, &szWhy ), szWhy.c_str() ) )
		{
			CMapInfo reread;
			std::string szWhere;
			if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
			     Check( NMapFile::Read( szEdited.c_str(), &reread, &szError ), szError.c_str() ) )
				Check( NMapFile::AreEquivalent( expected, reread, &szWhere ),
				       szWhere.empty() ? "a raise stroke equals the same-function expected map"
				                       : ( "the raise stroke differs at " + szWhere ).c_str() );
		}
	}
	// Undo, newest first: the file it writes is the unedited save, byte for
	// byte - the whole stroke, two tokens, leaves nothing behind.
	Check( BkEditorUndoEdit( pSession, nToken2 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorUndoEdit( pSession, nToken1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( BridgeFilesAreIdentical( szUnedited.c_str(), szUndone.c_str() ),
		       "a raise stroke undone writes the unedited save byte for byte" );

	// Lower, then level in each of the four modes, one stroke each, each
	// against its expected map and each undone byte-exact.
	for ( int nMode = -1; nMode < 4; ++nMode )
	{
		BkEditorHeightsStrokeParams one;
		memset( &one, 0, sizeof one );
		one.action = ( nMode == -1 ) ? 1 : 2;
		one.level_mode = ( nMode == -1 ) ? 2 : nMode;
		one.brush = 3;
		one.height_speed = 1.0f;
		one.level_ratio_percent = 50.0f;
		// The click modes' reference: the centre tile; the instant and zero
		// modes keep it anyway (the session takes the cache from whatever
		// the reference was).
		one.click_x = fCentreX; one.click_y = fCentreY;
		one.pos_x = fCentreX; one.pos_y = fCentreY;
		one.stroke_start = 1;
		int nToken = -1;
		const char *const szWhat = ( nMode == -1 ) ? "a lower stroke" :
			( nMode == 0 ? "a level-to-zero stroke" :
			  nMode == 1 ? "a level-to-click-tile stroke" :
			  nMode == 2 ? "a level-to-instant-average stroke" : "a level-to-click-average stroke" );
		Check( BkEditorHeightsStroke( pSession, &one, &nToken ) == BK_EDITOR_OK,
		       ( std::string( szWhat ) + " applies: " + BkEditorLastMessage( pSession ) ).c_str() );
		if ( nToken < 0 ) continue;
		Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		{
			CMapInfo expected;
			szError.clear();
			if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) ) return;
			// The pattern carries the stroke's own speed (the gradient's
			// ceiling is what the session keys its cache on).
			SVAGradient oneGradient;
			SVAPattern onePattern;
			oneGradient.CreateFromImage( pImage, CTPoint<float>( 0.0f, 1.0f ), CTPoint<float>( 0.0f, one.height_speed ) );
			Check( onePattern.CreateFromGradient( oneGradient, one.brush * 2 ), "the expected pattern builds" );
			SStrokeBuilder builder;
			std::string szWhy;
			if ( Check( builder.Step( &expected, one, &onePattern, &mask, &szWhy ), szWhy.c_str() ) )
			{
				CMapInfo reread;
				std::string szWhere;
				if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
				     Check( NMapFile::Read( szEdited.c_str(), &reread, &szError ), szError.c_str() ) )
					Check( NMapFile::AreEquivalent( expected, reread, &szWhere ),
					       ( std::string( szWhat ) + ( szWhere.empty() ? " equals the same-function expected map" : ( " differs at " + szWhere ).c_str() ) ).c_str() );
			}
		}
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( BridgeFilesAreIdentical( szUnedited.c_str(), szUndone.c_str() ),
			       ( std::string( szWhat ) + " undone writes the unedited save byte for byte" ).c_str() );
	}

	// The rollback: a speed that makes a cliff the IsValidHeight predicate
	// refuses is REFUSED with the MFC's own words, and nothing changes -
	// heights, engine, file. Ctrl keeps it (the MFC's MK_CONTROL override).
	std::vector<float> beforeCentre( 16, 0.0f );
	const BkEditorAltitudeRegion centreRegion = { nCentreTileX, nCentreTileY, nCentreTileX + 4, nCentreTileY + 4 };
	int nCount = 0;
	Check( BkEditorAltitudes( pSession, &centreRegion, &( beforeCentre[0] ), 16, &nCount ) == BK_EDITOR_OK,
	       BkEditorLastMessage( pSession ) );
	BkEditorHeightsStrokeParams cliff;
	memset( &cliff, 0, sizeof cliff );
	cliff.action = 0;
	cliff.brush = 3;
	cliff.height_speed = 4000.0f;
	cliff.pos_x = fCentreX; cliff.pos_y = fCentreY;
	cliff.click_x = fCentreX; cliff.click_y = fCentreY;
	cliff.stroke_start = 1;
	int nCliffToken = -1;
	Check( BkEditorHeightsStroke( pSession, &cliff, &nCliffToken ) == BK_EDITOR_REFUSED, "a cliff-making stroke is REFUSED" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "invalid height" ) != std::string::npos,
	       "and the refusal says the MFC's own words" );
	std::vector<float> afterRefused( 16, 0.0f );
	nCount = 0;
	Check( BkEditorAltitudes( pSession, &centreRegion, &( afterRefused[0] ), 16, &nCount ) == BK_EDITOR_OK,
	       BkEditorLastMessage( pSession ) );
	Check( std::equal( beforeCentre.begin(), beforeCentre.end(), afterRefused.begin() ),
	       "the refused stroke changed nothing the read can see" );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	cliff.ctrl_held = 1;
	Check( BkEditorHeightsStroke( pSession, &cliff, &nCliffToken ) == BK_EDITOR_OK,
	       ( std::string( "Ctrl keeps the cliff: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorUndoEdit( pSession, nCliffToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( BridgeFilesAreIdentical( szUnedited.c_str(), szUndone.c_str() ),
		       "the kept cliff undone writes the unedited save byte for byte" );

	// Generate: each of the MFC's three types over the whole sheet, undone
	// byte-exact. The types are the engine's own values (TG_FBM 0 Hills,
	// TG_HYBRID 3 Rocks, TG_RIDGED 4 Dunes). The field itself is the
	// engine's own seeded noise - NPerlinNoise::Init and the fractal
	// exponents draw the global generator, whose state no two calls see
	// alike - so the expected map here is STRUCTURAL, exactly the MFC
	// formula's own promises (TabTerrainAltitudesDialog.cpp:337-346): every
	// height lands in [min_z, max_z] * cell, the range's two ends are both
	// attained (the formula maps H's own min and max onto them), and the
	// shades are the engine's own recompute over the whole sheet (D-19's
	// deterministic part). The undo proofs carry the preservation weight.
	const int nGenTypes[3] = { 0, 3, 4 };
	const char *const nGenNames[3] = { "Hills", "Rocks", "Dunes" };
	for ( int nWhich = 0; nWhich < 3; ++nWhich )
	{
		int nToken = -1;
		Check( BkEditorGenerateHeights( pSession, nGenTypes[nWhich], 0.3f, -3.0f, 3.0f, &nToken ) == BK_EDITOR_OK,
		       ( std::string( nGenNames[nWhich] ) + " generates: " + BkEditorLastMessage( pSession ) ).c_str() );
		if ( nToken < 0 ) continue;
		Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		{
			const BkEditorAltitudeRegion whole = { 0, 0, original.terrain.altitudes.GetSizeX(), original.terrain.altitudes.GetSizeY() };
			const int nArea = ( whole.x1 - whole.x0 ) * ( whole.y1 - whole.y0 );
			std::vector<float> generated( nArea, 0.0f );
			int nRead = 0;
			if ( Check( BkEditorAltitudes( pSession, &whole, &( generated[0] ), nArea, &nRead ) == BK_EDITOR_OK &&
			     nRead == nArea, BkEditorLastMessage( pSession ) ) )
			{
				const float fLow = -3.0f * fWorldCellSize, fHigh = 3.0f * fWorldCellSize;
				bool bInRange = true, bLowAttained = false, bHighAttained = false;
				for ( int i = 0; i < nArea; ++i )
				{
					if ( generated[size_t( i )] < fLow || generated[size_t( i )] > fHigh )
					{
						bInRange = false;
						break;
					}
					if ( generated[size_t( i )] == fLow ) bLowAttained = true;
					if ( generated[size_t( i )] == fHigh ) bHighAttained = true;
				}
				Check( bInRange, ( std::string( nGenNames[nWhich] ) + " keeps every height inside [min_z, max_z] * cell" ).c_str() );
				Check( bLowAttained && bHighAttained, ( std::string( nGenNames[nWhich] ) + " attains both ends of the MFC formula's range" ).c_str() );
			}
			// The shades: the same recompute over a fresh read, compared
			// whole (AreEquivalent would fold a shade drift into the altitudes).
			CMapInfo expected;
			szError.clear();
			if ( Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
			{
				std::vector<SVertexAltitude> values( nArea );
				for ( int nY = whole.y0; nY < whole.y1; ++nY )
					for ( int nX = whole.x0; nX < whole.x1; ++nX )
					{
						const size_t nAt = size_t( nY - whole.y0 ) * size_t( whole.x1 - whole.x0 ) + size_t( nX - whole.x0 );
						memcpy( &values[nAt], &expected.terrain.altitudes[nY][nX], sizeof( SVertexAltitude ) );
						values[nAt].fHeight = generated[nAt];
					}
				Check( NMapOverlay::SetAltitudeRegion( &expected, CTRect<int>( whole.x0, whole.y0, whole.x1, whole.y1 ), values, 0 ) &&
				       CMapInfo::UpdateTerrainShades( &expected.terrain, CTRect<int>( whole.x0, whole.y0, whole.x1, whole.y1 ),
				         CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( expected.nSeason ) ) ),
				       ( std::string( nGenNames[nWhich] ) + ": the expected map builds" ).c_str() );
				CMapInfo reread;
				std::string szWhere;
				if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
				     Check( NMapFile::Read( szEdited.c_str(), &reread, &szError ), szError.c_str() ) )
					Check( NMapFile::AreEquivalent( expected, reread, &szWhere ),
					       ( std::string( nGenNames[nWhich] ) + ( szWhere.empty() ? " equals the same-shades expected map" : ( " differs at " + szWhere ).c_str() ) ).c_str() );
			}
		}
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( BridgeFilesAreIdentical( szUnedited.c_str(), szUndone.c_str() ),
			       ( std::string( nGenNames[nWhich] ) + " undone writes the unedited save byte for byte" ).c_str() );
	}

	// Set Zero: every height 0 with the shades recomputed, undone byte-exact.
	{
		int nToken = -1;
		Check( BkEditorSetZeroHeights( pSession, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		if ( nToken >= 0 )
		{
			Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			CMapInfo expected;
			szError.clear();
			if ( Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
			{
				const int nSizeX = expected.terrain.altitudes.GetSizeX(), nSizeY = expected.terrain.altitudes.GetSizeY();
				std::vector<SVertexAltitude> values( size_t( nSizeX ) * size_t( nSizeY ) );
				for ( int nY = 0; nY < nSizeY; ++nY )
					for ( int nX = 0; nX < nSizeX; ++nX )
					{
						const size_t nAt = size_t( nY ) * size_t( nSizeX ) + size_t( nX );
						memcpy( &values[nAt], &expected.terrain.altitudes[nY][nX], sizeof( SVertexAltitude ) );
						values[nAt].fHeight = 0.0f;
					}
				Check( NMapOverlay::SetAltitudeRegion( &expected, CTRect<int>( 0, 0, nSizeX, nSizeY ), values, 0 ) &&
				       CMapInfo::UpdateTerrainShades( &expected.terrain, CTRect<int>( 0, 0, nSizeX, nSizeY ),
				         CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( expected.nSeason ) ) ),
				       "the expected zero map builds" );
				CMapInfo reread;
				std::string szWhere;
				if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
				     Check( NMapFile::Read( szEdited.c_str(), &reread, &szError ), szError.c_str() ) )
					Check( NMapFile::AreEquivalent( expected, reread, &szWhere ),
					       szWhere.empty() ? "set zero equals the same-function expected map" : ( "set zero differs at " + szWhere ).c_str() );
			}
			Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
				Check( BridgeFilesAreIdentical( szUnedited.c_str(), szUndone.c_str() ),
				       "set zero undone writes the unedited save byte for byte" );
		}
	}

	// The ABI's own refusals: caller bugs are BAD_ARGUMENT (brush, a
	// non-finite float, a hidden generator type, a bad granularity), the
	// cursor off the map is REFUSED - each leaving the heights exactly as
	// they were (the session was undone back to the unedited state above).
	std::vector<float> beforeRefusals( 16, 0.0f );
	nCount = 0;
	Check( BkEditorAltitudes( pSession, &centreRegion, &( beforeRefusals[0] ), 16, &nCount ) == BK_EDITOR_OK,
	       BkEditorLastMessage( pSession ) );
	BkEditorHeightsStrokeParams bad = stroke;
	bad.brush = 1;
	Check( BkEditorHeightsStroke( pSession, &bad, 0 ) == BK_EDITOR_BAD_ARGUMENT, "brush 1 is BAD_ARGUMENT" );
	bad.brush = 17;
	Check( BkEditorHeightsStroke( pSession, &bad, 0 ) == BK_EDITOR_BAD_ARGUMENT, "brush 17 is BAD_ARGUMENT" );
	bad = stroke;
	bad.height_speed = std::numeric_limits<float>::quiet_NaN();
	Check( BkEditorHeightsStroke( pSession, &bad, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a non-finite speed is BAD_ARGUMENT" );
	bad = stroke;
	bad.pos_x = -100000.0f; bad.pos_y = -100000.0f;
	Check( BkEditorHeightsStroke( pSession, &bad, 0 ) == BK_EDITOR_REFUSED, "a cursor off the map is REFUSED" );
	int nGenToken = -1;
	Check( BkEditorGenerateHeights( pSession, 1, 0.3f, -3.0f, 3.0f, &nGenToken ) == BK_EDITOR_BAD_ARGUMENT,
	       "the hidden MULTI radio is not a generator" );
	Check( BkEditorGenerateHeights( pSession, 0, 0.0f, -3.0f, 3.0f, &nGenToken ) == BK_EDITOR_BAD_ARGUMENT,
	       "a zero granularity is BAD_ARGUMENT" );
	std::vector<float> afterRefusals( 16, 0.0f );
	nCount = 0;
	Check( BkEditorAltitudes( pSession, &centreRegion, &( afterRefusals[0] ), 16, &nCount ) == BK_EDITOR_OK,
	       BkEditorLastMessage( pSession ) );
	Check( std::equal( beforeRefusals.begin(), beforeRefusals.end(), afterRefusals.begin() ),
	       "and the refusals changed nothing the read can see" );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	remove( szEdited.c_str() );
	remove( szUndone.c_str() );
	remove( szUnedited.c_str() );
	printf( "editor-bridge: M3 heights ok\n" );
}

// The Update Map progress collector: a step counter and nothing else - no
// bridge re-entry, exactly the rule the header states.
struct M3UpdateProgress
{
	int nSteps;
	int nTotal;
	int nCalls;
	static void Report( int nStep, int nTotal, void *pUser )
	{
		M3UpdateProgress *pSelf = static_cast<M3UpdateProgress *>( pUser );
		++pSelf->nCalls;
		pSelf->nSteps = nStep;
		pSelf->nTotal = nTotal;
	}
};

static BkEditorObjectRecord M3FindObject( BkEditorSession *pSession, int nLinkID )
{
	BkEditorObjectRecord record;
	memset( &record, 0, sizeof record );
	record.link_id = -1;
	int nCount = 0;
	if ( BkEditorObjects( pSession, 0, 0, &nCount ) != BK_EDITOR_REFUSED || nCount <= 0 ) return record;
	std::vector<BkEditorObjectRecord> objects( nCount );
	if ( BkEditorObjects( pSession, &( objects[0] ), nCount, &nCount ) != BK_EDITOR_OK ) return record;
	for ( int i = 0; i < nCount; ++i )
		if ( objects[i].link_id == nLinkID ) return objects[i];
	return record;
}

// Update Map (M3, D-20) and Fill Entire Map (M3, D-22) at the engine tier.
// The composite runs on a shipped map whose fit candidate - a sprite object
// with passability, found by the test with the session's own predicate - has
// been moved off the AI grid: Update Map snaps it exactly as
// FitVisOrigin2AIGrid predicts, hears every step of the MFC's own progress
// count, leaves the altitude sheet it was given and undoes byte-exact; the
// fit toggle drives the move path by itself too. The fill makes every tile
// the chosen terrain type's own - equivalent to a whole-map paint of the
// same tile, which is the route it rides - and undoes byte-exact; a tile in
// no terrain type is refused, a paint's own rule, changing nothing.
static void TestM3UpdateMapAndFill( BkEditorSession *pSession, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) ) return;

	// The toggles: a null session is NO_SESSION, and the pair set to the
	// MFC's own defaults (Instant Update off, Fit on).
	Check( BkEditorSetTerrainModes( 0, 1, 1 ) == BK_EDITOR_NO_SESSION, "the modes without a session are NO_SESSION" );
	Check( BkEditorSetTerrainModes( pSession, 0, 1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// A fit candidate, by the session's own rule: a sprite (building, object
	// or terraobj) whose stats give it passability.
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	int nCount = 0;
	if ( !Check( BkEditorObjects( pSession, 0, 0, &nCount ) == BK_EDITOR_REFUSED && nCount > 0, "the object list sizes" ) ) return;
	std::vector<BkEditorObjectRecord> objects( nCount );
	if ( !Check( BkEditorObjects( pSession, &( objects[0] ), nCount, &nCount ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) ) return;
	int nCandidate = -1;
	float fOffX = 0.0f, fOffY = 0.0f;
	for ( int i = 0; i < nCount && nCandidate < 0; ++i )
	{
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( objects[i].name );
		if ( pDesc == 0 || pDesc->eVisType != SGVOT_SPRITE ) continue;
		// Buildings and generic objects only: their stats are the base
		// implementation, whose GetOrigin/GetPassability ignore the frame -
		// safe to ask without one. (A terraobj's frame lives in the record
		// the BkEditorObjects surface does not expose; the session's own fit
		// pass reads it there.)
		if ( pDesc->eGameType != SGVOGT_BUILDING && pDesc->eGameType != SGVOGT_OBJECT ) continue;
		if ( objects[i].link_id <= 0 ) continue;
		// The session's fit pass only looks at objects the engine holds
		// (byLinkID); the read surface cannot see that, so the scan asks the
		// same question the way a caller would.
		{
			BkEditorObjectState held;
			memset( &held, 0, sizeof held );
			if ( BkEditorEngineObjectState( pSession, objects[i].link_id, &held ) != BK_EDITOR_OK ) continue;
		}
		const SObjectBaseRPGStats *pRPG = static_cast<const SObjectBaseRPGStats *>( pObjectsDB->GetRPGStats( pDesc ) );
		if ( pRPG == 0 || pRPG->GetPassability( -1 ).IsEmpty() ) continue;
		nCandidate = objects[i].link_id;
		fOffX = objects[i].x + 7.3f;
		fOffY = objects[i].y + 3.9f;
	}
	if ( !Check( nCandidate > 0, "the shipped map holds a sprite object with passability to fit" ) ) return;

	// Fit is ON (step above), so the move itself would snap; the off-grid
	// start this test needs is made with it OFF.
	Check( BkEditorSetTerrainModes( pSession, 0, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorMoveObject( pSession, nCandidate, fOffX, fOffY ) == BK_EDITOR_OK, "fit off, the candidate moves off the AI grid" );
	{
		const BkEditorObjectRecord moved = M3FindObject( pSession, nCandidate );
		Check( moved.link_id == nCandidate && moved.x == fOffX && moved.y == fOffY,
		       "the off-grid move is on the map exactly as asked" );
	}
	Check( BkEditorSetTerrainModes( pSession, 0, 1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	const std::string szPreUpdate = szScratch + "\\m3-update-pre.bzm";
	const std::string szEdited = szScratch + "\\m3-update-edited.bzm";
	const std::string szUndone = szScratch + "\\m3-update-undone.bzm";
	Check( BkEditorSaveMap( pSession, szPreUpdate.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	M3UpdateProgress progress = { 0, 0, 0 };
	int nUpdateToken = -1;
	Check( BkEditorUpdateMap( pSession, &M3UpdateProgress::Report, &progress, &nUpdateToken ) == BK_EDITOR_OK,
	       ( std::string( "the composite updates: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( nUpdateToken >= 0, "the update has a token" );
	Check( progress.nTotal >= 8 && progress.nSteps == progress.nTotal && progress.nCalls == progress.nTotal,
	       ( std::string( "the progress heard every step of the MFC's own count (7 fixed + the snapped object): steps " )
	         + std::to_string( progress.nSteps ) + ", total " + std::to_string( progress.nTotal )
	         + ", calls " + std::to_string( progress.nCalls ) ).c_str() );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// The snap, exactly as the engine's own fit predicts: the map keeps the
	// raw fitted float, from the same FitVisOrigin2AIGrid the session ran.
	CVec3 vFitted( fOffX, fOffY, 0 );
	{
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( M3FindObject( pSession, nCandidate ).name );
		if ( Check( pDesc != 0, "the candidate's descriptor is still there" ) )
		{
			const SObjectBaseRPGStats *pRPG = static_cast<const SObjectBaseRPGStats *>( pObjectsDB->GetRPGStats( pDesc ) );
			FitVisOrigin2AIGrid( &vFitted, pRPG->GetOrigin( -1 ) );
			const BkEditorObjectRecord after = M3FindObject( pSession, nCandidate );
			Check( after.x == vFitted.x && after.y == vFitted.y,
			       "the update snaps the candidate exactly as FitVisOrigin2AIGrid predicts" );
		}
	}

	// The altitude sheet the composite was given is the altitude sheet it
	// leaves: UpdateAllHeights is AI-side state the file never sees, and the
	// shade recompute lands on the shades the shipped map already carried.
	Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	{
		CMapInfo before, after;
		std::string szError;
		if ( Check( NMapFile::Read( szPreUpdate.c_str(), &before, &szError ), szError.c_str() ) &&
		     Check( NMapFile::Read( szEdited.c_str(), &after, &szError ), szError.c_str() ) )
		{
			const int nSizeX = before.terrain.altitudes.GetSizeX(), nSizeY = before.terrain.altitudes.GetSizeY();
			Check( after.terrain.altitudes.GetSizeX() == nSizeX && after.terrain.altitudes.GetSizeY() == nSizeY,
			       "the altitude sheet keeps its size over the update" );
			bool bSame = true;
			for ( int nY = 0; nY < nSizeY && bSame; ++nY )
				for ( int nX = 0; nX < nSizeX && bSame; ++nX )
					bSame = memcmp( &before.terrain.altitudes[nY][nX], &after.terrain.altitudes[nY][nX],
						sizeof( SVertexAltitude ) ) == 0;
			Check( bSame, "the update touches no altitude vertex the file can see" );
		}
	}

	// Undo, newest first where there is one: the update's token puts the
	// whole composite back - the object off the grid included - byte for byte.
	Check( BkEditorUndoEdit( pSession, nUpdateToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( BridgeFilesAreIdentical( szPreUpdate.c_str(), szUndone.c_str() ),
		       "the update undone writes the pre-update save byte for byte" );

	// The toggle is the placer's own question (M3, D-20): the fit answered
	// through BkEditorSnapToGrid for the candidate's type lands exactly where
	// FitVisOrigin2AIGrid puts the point, a unit is left where it was, and
	// with the toggle off nothing moves either. The bridge's add and place
	// themselves stay raw - undo replays positions, it does not re-fit them.
	{
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( M3FindObject( pSession, nCandidate ).name );
		const SObjectBaseRPGStats *pRPG = static_cast<const SObjectBaseRPGStats *>( pObjectsDB->GetRPGStats( pDesc ) );
		CVec3 vAsk( fOffX + 0.5f, fOffY + 0.5f, 0 );
		FitVisOrigin2AIGrid( &vAsk, pRPG->GetOrigin( -1 ) );
		float fSnappedX = 0.0f, fSnappedY = 0.0f;
		Check( BkEditorSnapToGrid( pSession, M3FindObject( pSession, nCandidate ).name, fOffX + 0.5f, fOffY + 0.5f, &fSnappedX, &fSnappedY ) == BK_EDITOR_OK,
		       "the fit answers the placer's question" );
		Check( fSnappedX == vAsk.x && fSnappedY == vAsk.y, "and lands exactly as FitVisOrigin2AIGrid predicts" );
		Check( BkEditorSnapToGrid( pSession, "JS_2", fOffX + 0.5f, fOffY + 0.5f, &fSnappedX, &fSnappedY ) == BK_EDITOR_OK,
		       "the fit answers for a unit too" );
		Check( fSnappedX == fOffX + 0.5f && fSnappedY == fOffY + 0.5f, "and leaves the unit exactly as asked" );
		Check( BkEditorSetTerrainModes( pSession, 0, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BkEditorSnapToGrid( pSession, M3FindObject( pSession, nCandidate ).name, fOffX + 0.5f, fOffY + 0.5f, &fSnappedX, &fSnappedY ) == BK_EDITOR_OK,
		       "with fit off the fit answers again" );
		Check( fSnappedX == fOffX + 0.5f && fSnappedY == fOffY + 0.5f, "and leaves the point as asked" );
		Check( BkEditorSnapToGrid( pSession, "No_Such_Object_Type", fOffX, fOffY, &fSnappedX, &fSnappedY ) == BK_EDITOR_REFUSED,
		       "a type the database does not know is REFUSED" );
		Check( BkEditorSnapToGrid( pSession, 0, fOffX, fOffY, &fSnappedX, &fSnappedY ) == BK_EDITOR_BAD_ARGUMENT,
		       "a null name is BAD_ARGUMENT" );
		Check( BkEditorSetTerrainModes( pSession, 0, 1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	}

	// Fill Entire Map (D-22), from a fresh open so the candidate's moves are
	// out of the picture: every tile the terrain type's own, the crosses
	// recomputed, undone byte-exact - and the filled map equivalent to a
	// whole-map paint of the same tile, which is the route it rides.
	const std::string szFillPre = szScratch + "\\m3-fill-pre.bzm";
	const std::string szFilled = szScratch + "\\m3-fill-edited.bzm";
	const std::string szFillUndone = szScratch + "\\m3-fill-undone.bzm";
	const std::string szPainted = szScratch + "\\m3-fill-painted.bzm";
	Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	std::vector<unsigned char> tiles;
	{
		int nTiles = 0;
		Check( BkEditorTilesetTiles( pSession, 0, 0, &nTiles ) == BK_EDITOR_REFUSED && nTiles > 0, "the tile list sizes" );
		tiles.resize( nTiles );
		Check( BkEditorTilesetTiles( pSession, &( tiles[0] ), nTiles, &nTiles ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	}
	const unsigned char nFillTile = tiles[0];
	Check( BkEditorSaveMap( pSession, szFillPre.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	int nFillToken = -1;
	Check( BkEditorFillEntireMap( pSession, nFillTile, &nFillToken ) == BK_EDITOR_OK,
	       ( std::string( "the map fills: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( nFillToken >= 0, "the fill has a token" );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	{
		CMapInfo reread;
		std::string szError;
		if ( Check( BkEditorSaveMap( pSession, szFilled.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
		     Check( NMapFile::Read( szFilled.c_str(), &reread, &szError ), szError.c_str() ) )
		{
			bool bAll = true;
			const int nSizeX = reread.terrain.tiles.GetSizeX(), nSizeY = reread.terrain.tiles.GetSizeY();
			for ( int nY = 0; nY < nSizeY && bAll; ++nY )
				for ( int nX = 0; nX < nSizeX && bAll; ++nX )
					bAll = reread.terrain.tiles[nY][nX].tile == nFillTile;
			Check( bAll, "every tile of the map is the fill's terrain type" );
		}
	}
	// The whole-map paint of the same tile is the fill's expected value: a
	// fresh open, the full map's cells through BkEditorPaint, and the two
	// saves agree byte-level equivalence at the overlay's own compare.
	{
		Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		CMapInfo original;
		std::string szError;
		if ( Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		{
			const int nSizeX = original.terrain.tiles.GetSizeX(), nSizeY = original.terrain.tiles.GetSizeY();
			std::vector<BkEditorPaintCell> cells( size_t( nSizeX ) * size_t( nSizeY ) );
			int nAt = 0;
			for ( int nY = 0; nY < nSizeY; ++nY )
				for ( int nX = 0; nX < nSizeX; ++nX )
				{
					cells[nAt].x = nX;
					cells[nAt].y = nY;
					cells[nAt].tile = nFillTile;
					++nAt;
				}
			int nPaintToken = -1;
			Check( BkEditorPaint( pSession, &( cells[0] ), nAt, &nPaintToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			CMapInfo painted, filled;
			szError.clear();
			if ( Check( BkEditorSaveMap( pSession, szPainted.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
			     Check( NMapFile::Read( szPainted.c_str(), &painted, &szError ), szError.c_str() ) )
			{
				szError.clear();
				std::string szWhere;
				if ( Check( NMapFile::Read( szFilled.c_str(), &filled, &szError ), szError.c_str() ) )
					Check( NMapFile::AreEquivalent( painted, filled, &szWhere ),
					       szWhere.empty() ? "the fill is what a whole-map paint of the tile writes"
					                       : ( "the fill differs from the whole-map paint at " + szWhere ).c_str() );
			}
		}
	}

	// Undo, then the refusals: tile 1 is in no terrain type of the shipped
	// mapset (the paint-refusal test's own finding), and off-range indexes
	// are the caller's mistake. Each refusal changes nothing the save sees.
	Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	{
		int nToken = -1;
		Check( BkEditorFillEntireMap( pSession, nFillTile, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		// The fill's token is a PAINT of the log - undoPaint's route, the
		// same one a brush paint takes - not an edit token.
		Check( BkEditorUndoPaint( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		if ( Check( BkEditorSaveMap( pSession, szFillUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( BridgeFilesAreIdentical( szFillPre.c_str(), szFillUndone.c_str() ),
			       "the fill undone writes the pre-fill save byte for byte" );

		int nRefused = -1;
		Check( BkEditorFillEntireMap( pSession, 1, &nRefused ) == BK_EDITOR_REFUSED,
		       "a tile in no terrain type is REFUSED" );
		Check( BkEditorFillEntireMap( pSession, -1, &nRefused ) == BK_EDITOR_BAD_ARGUMENT, "tile -1 is BAD_ARGUMENT" );
		Check( BkEditorFillEntireMap( pSession, 256, &nRefused ) == BK_EDITOR_BAD_ARGUMENT, "tile 256 is BAD_ARGUMENT" );
		std::string szAfterRefusals = szScratch + "\\m3-fill-refused.bzm";
		if ( Check( BkEditorSaveMap( pSession, szAfterRefusals.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( BridgeFilesAreIdentical( szFillPre.c_str(), szAfterRefusals.c_str() ),
			       "and the refusals changed nothing the save sees" );
		remove( szAfterRefusals.c_str() );
	}
	Check( BkEditorCloseMap( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	int nNoMap = -1;
	M3UpdateProgress noMap = { 0, 0, 0 };
	Check( BkEditorUpdateMap( pSession, &M3UpdateProgress::Report, &noMap, &nNoMap ) == BK_EDITOR_REFUSED,
	       "an update with no map open is REFUSED" );
	Check( BkEditorFillEntireMap( pSession, nFillTile, &nNoMap ) == BK_EDITOR_REFUSED,
	       "a fill with no map open is REFUSED" );

	remove( szPreUpdate.c_str() );
	remove( szEdited.c_str() );
	remove( szUndone.c_str() );
	remove( szFillPre.c_str() );
	remove( szFilled.c_str() );
	remove( szFillUndone.c_str() );
	remove( szPainted.c_str() );
	printf( "editor-bridge: M3 update and fill ok\n" );
}

static bool SameBytes( const std::string &szLeft, const std::string &szRight );
static std::string DescribeDifference( const std::string &szLeft, const std::string &szRight );
static std::string OsPath( std::string szPath );
static bool ReadObjectRecord( BkEditorSession *pSession, int nLinkID, BkEditorObjectRecord *pOut );
static void TestM3Damage( BkEditorSession *pSession, const std::string &szScratch );
static void TestM3MinimapReads( BkEditorSession *pSession, const std::string &szScratch );
static void TestM3MinimapImages( BkEditorSession *pSession, const std::string &szScratch );
static void TestM3PlayersAndUnitCreation( BkEditorSession *pSession, const std::string &szScratch );
static void TestM3CheckMap( BkEditorSession *pSession, const std::string &szScratch );
static bool CraftFixture( BkEditorSession *pSession, const char *pszKind, const char *pszOut );
static void CheckSavedEquals( BkEditorSession *pSession, const std::string &szPath, const CMapInfo &rExpected, const char *pszWhat );

// M3 (D-26/D-27): the properties' fields and the links, on the engine. The
// fields edit (player, hp, angle, formation) saves as the builder's map; the
// garrison links a known infantry to a known building through
// BkEditorCanLink's answer, and a full building refuses the next passenger
// naming the rule; the unlink clears; deleting a host takes its passengers
// and the restore brings every one back byte for byte; and the flag swap
// renames the record the MFC properties' own way.
static void TestM3PropertiesAndLinks( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	// arnheim, not coldwinter: the multiplayer map holds no units a garrison
	// can name. arnheim has the units, the squads and the buildings.
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// One editable unit (or squad) and its record.
	int nTarget = -1;
	bool bTargetIsSquad = false;
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		int nCatalogue = 0;
		BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
		std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
		int nCatalogueRead = 0;
		BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nCatalogueRead );
		for ( int i = 0; i < nRead && nTarget < 0; ++i )
		{
			const BkEditorObjectRecord &rRecord = records[size_t( i )];
			if ( rRecord.link_id <= 0 || !rRecord.known )
				continue;
			for ( int j = 0; j < nCatalogueRead && nTarget < 0; ++j )
			{
				const int nGameType = catalogue[size_t( j )].game_type;
				if ( ( nGameType == 1 || nGameType == 15 ) && rRecord.name == std::string( catalogue[size_t( j )].name ) )
				{
					nTarget = rRecord.link_id;
					bTargetIsSquad = nGameType == 15;
				}
			}
		}
	}
	if ( !Check( nTarget > 0, ( "a unit to edit is on the map (squad: " + std::string( bTargetIsSquad ? "yes" : "no" ) + ")" ).c_str() ) )
		return;

	// The fields edit: one call, one token, the save the builder's map. The
	// formation bit rides only on a squad - on any other unit the record's
	// frame index is its segment, and the edit refuses naming that.
	BkEditorObjectFieldsEdit fields;
	memset( &fields, 0, sizeof fields );
	fields.mask = 1 | 2 | 4 | ( bTargetIsSquad ? 8 : 0 );
	fields.player = 1;
	fields.hp = 0.77f;
	fields.angle = 270.0f;
	fields.formation = 2;
	int nToken = -1;
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> probe( nCount > 0 ? nCount : 1 );
		int nProbeRead = 0;
		BkEditorObjects( pSession, &( probe[0] ), nCount, &nProbeRead );
		for ( int i = 0; i < nProbeRead; ++i )
			if ( probe[size_t( i )].link_id == nTarget )
				printf( "editor-bridge: M3 fields target %d: name %s, player %d, hp %f, dir %d, frame %d, mask %d\n",
				        nTarget, probe[size_t( i )].name, probe[size_t( i )].player, probe[size_t( i )].hp,
				        probe[size_t( i )].dir, probe[size_t( i )].frame_index, int( fields.mask ) );
	}
	// The unedited file, saved BEFORE the edit: the undo below is proven
	// against it byte for byte.
	const std::string szUnedited = szScratch + "\\m3-fields-unedited.bzm";
	const std::string szEdited = szScratch + "\\m3-fields-edited.bzm";
	const std::string szUndone = szScratch + "\\m3-fields-undone.bzm";
	Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	const BkEditorStatus nFieldsStatus = BkEditorSetObjectFields( pSession, nTarget, &fields, &nToken );
	printf( "editor-bridge: M3 fields edit: status %d, token %d, message '%s'\n", int( nFieldsStatus ), nToken, BkEditorLastMessage( pSession ) );
	if ( !Check( nFieldsStatus == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( nToken >= 0, "the fields edit has a token" );
	if ( !bTargetIsSquad )
	{
		BkEditorObjectFieldsEdit formationOnly;
		memset( &formationOnly, 0, sizeof formationOnly );
		formationOnly.mask = 8;
		formationOnly.formation = 1;
		int nFormationToken = -1;
		Check( BkEditorSetObjectFields( pSession, nTarget, &formationOnly, &nFormationToken ) == BK_EDITOR_REFUSED,
		       "a formation on a plain unit is refused" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "squad" ) != std::string::npos, "and names the rule" );
	}
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		const BkEditorObjectRecord *pNow = 0;
		for ( int i = 0; i < nRead; ++i )
			if ( records[size_t( i )].link_id == nTarget )
				pNow = &records[size_t( i )];
		Check( pNow != 0 && pNow->player == 1 && pNow->hp == 0.77f, "the player and the health are in the record" );
		Check( pNow->dir == int( ( 270.0f * 65536.0f ) / 360.0f + 0.5f ), "the angle is the MFC's own turn" );
		// And the engine turned with the record: the object is drawn facing
		// the new angle, not only saved that way.
		BkEditorObjectState engineNow;
		memset( &engineNow, 0, sizeof engineNow );
		if ( pNow != 0 && Check( BkEditorEngineObjectState( pSession, nTarget, &engineNow ) == BK_EDITOR_OK, "the engine holds the edited object" ) )
			Check( engineNow.dir == ( pNow->dir & 0xFFFF ),
			       ( "the engine faces the record's angle (" + std::to_string( engineNow.dir ) + " vs " + std::to_string( pNow->dir ) + ")" ).c_str() );
		if ( bTargetIsSquad )
			Check( pNow->frame_index == 2, "the formation is in the squad's record" );
	}

	// The expected value: the same setters on a fresh read, no engine.
	CMapInfo expected, saved;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &expected, &szError ), szError.c_str() ) )
		return;
	SMapObjectInfo *pExpected = 0;
	{
		std::vector<SMapObjectInfo> *lists[2] = { &expected.objects, &expected.scenarioObjects };
		for ( int nList = 0; nList < 2 && pExpected == 0; ++nList )
			for ( size_t i = 0; i < lists[nList]->size() && pExpected == 0; ++i )
				if ( ( *lists[nList] )[i].link.nLinkID == nTarget )
					pExpected = &( *lists[nList] )[i];
	}
	if ( !Check( pExpected != 0, "the builder finds the squad" ) )
		return;
	pExpected->nPlayer = 1;
	pExpected->fHP = 0.77f;
	pExpected->nDir = int( ( 270.0f * 65536.0f ) / 360.0f + 0.5f );
	if ( bTargetIsSquad )
		pExpected->nFrameIndex = 2;
	if ( !Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	std::string szWhere;
	if ( !Check( NMapFile::Read( szEdited.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
	       szWhere.empty() ? "the fields edit's save is the expected map" : ( "the fields edit differs at " + szWhere ).c_str() );

	// The undo, byte for byte.
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, "the fields edit undoes" );
	Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szUnedited, szUndone ), "the undone fields edit saves the unedited file byte for byte" );

	// The garrison: an infantry onto a building or a vehicle, through the
	// rules. Searched over the map's own objects, so the proof is the
	// engine's database, not a fixture.
	int nPassenger = -1, nHost = -1, nLinkType = -1;
	int nProbes = 0;
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		int nCatalogue = 0;
		BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
		std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
		int nCatalogueRead = 0;
		BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nCatalogueRead );
		auto gameTypeOf = [&]( const BkEditorObjectRecord &rRecord ) -> int {
			for ( int j = 0; j < nCatalogueRead; ++j )
				if ( rRecord.name == std::string( catalogue[size_t( j )].name ) )
					return catalogue[size_t( j )].game_type;
			return -1;
		};
		for ( int i = 0; i < nRead && nHost < 0; ++i )
		{
			const int nTargetType = gameTypeOf( records[size_t( i )] );
			if ( records[size_t( i )].link_id <= 0 || !records[size_t( i )].known )
				continue;
			if ( nTargetType != 2 && nTargetType != 1 ) // a building or a vehicle
				continue;
			for ( int j = 0; j < nRead && nHost < 0; ++j )
			{
				if ( j == i || records[size_t( j )].link_id <= 0 || !records[size_t( j )].known )
					continue;
				{
					const int nSourceType = gameTypeOf( records[size_t( j )] );
					if ( nSourceType != 1 && nSourceType != 15 ) // a unit or a squad
						continue;
				}
				int nType = -1;
				const BkEditorStatus nCan = BkEditorCanLink( pSession, records[size_t( j )].link_id, records[size_t( i )].link_id, &nType );
				if ( nCan == BK_EDITOR_OK && nType == 0 )
				{
					nPassenger = records[size_t( j )].link_id;
					nHost = records[size_t( i )].link_id;
					nLinkType = nType;
				}
				else if ( nProbes < 6 )
				{
					++nProbes;
					printf( "editor-bridge: M3 canlink probe: status %d, type %d, why '%s'\n", int( nCan ), nType, BkEditorLastMessage( pSession ) );
				}
			}
		}
	}
	if ( !Check( nPassenger > 0 && nHost > 0 && nLinkType == 0, ( "a garrison pair is on the map (" + std::to_string( nPassenger ) + " -> " + std::to_string( nHost ) + ")" ).c_str() ) )
		return;
	printf( "editor-bridge: M3 garrison pair: %d -> %d\n", nPassenger, nHost );
	Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// A full host refuses the next passenger, naming the rule. Proven on the
	// first host the fill actually fills up: each garrison-able host is
	// filled until the rules say no - the extra links ride the host's own
	// delete below, so the byte proof restores them all.
	std::vector<int> extraLinks;
	bool bSlotRefusal = false;
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		int nCatalogue = 0;
		BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
		std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
		int nCatalogueRead = 0;
		BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nCatalogueRead );
		auto gameTypeOf = [&]( const BkEditorObjectRecord &rRecord ) -> int {
			for ( int j = 0; j < nCatalogueRead; ++j )
				if ( rRecord.name == std::string( catalogue[size_t( j )].name ) )
					return catalogue[size_t( j )].game_type;
			return -1;
		};
		std::vector<int> hosts;
		for ( int i = 0; i < nRead; ++i )
		{
			if ( records[size_t( i )].link_id <= 0 || !records[size_t( i )].known )
				continue;
			const int nType = gameTypeOf( records[size_t( i )] );
			if ( nType == 2 || nType == 1 )
				hosts.push_back( records[size_t( i )].link_id );
		}
		for ( size_t h = 0; h < hosts.size() && !bSlotRefusal; ++h )
		{
			const int nThisHost = hosts[h];
			for ( int round = 0; round < 2 && !bSlotRefusal; ++round )
			{
				for ( int j = 0; j < nRead; ++j )
				{
					const int nWho = records[size_t( j )].link_id;
					if ( nWho <= 0 || !records[size_t( j )].known )
						continue;
					{
						const int nSourceType = gameTypeOf( records[size_t( j )] );
						if ( nSourceType != 1 && nSourceType != 15 )
							continue;
					}
					int nType = -1;
					const BkEditorStatus status = BkEditorCanLink( pSession, nWho, nThisHost, &nType );
					if ( status == BK_EDITOR_OK )
					{
						const bool bAlreadyIn = nWho == nPassenger || std::find( extraLinks.begin(), extraLinks.end(), nWho ) != extraLinks.end();
						if ( !bAlreadyIn && nThisHost == nHost )
						{
							int nExtraToken = -1;
							if ( BkEditorSetLink( pSession, nWho, nThisHost, &nExtraToken ) == BK_EDITOR_OK )
								extraLinks.push_back( nWho );
						}
						continue;
					}
					const std::string szWhy = BkEditorLastMessage( pSession );
					if ( szWhy.find( "slot" ) != std::string::npos || szWhy.find( "passengers" ) != std::string::npos || szWhy.find( "entrance" ) != std::string::npos )
					{
						bSlotRefusal = true;
						printf( "editor-bridge: M3 garrison refusal on host %d: %s\n", nThisHost, szWhy.c_str() );
						break;
					}
				}
			}
		}
	}
	Check( bSlotRefusal, "the garrison rules refuse when the host takes no more" );

	// The drop's link itself - undone byte for byte and redone - then the
	// unlink.
	const std::string szLinkBefore = szScratch + "\\m3-link-before.bzm";
	const std::string szLinkUndone = szScratch + "\\m3-link-undone.bzm";
	Check( BkEditorSaveMap( pSession, szLinkBefore.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	BkEditorObjectRecord passengerBefore;
	memset( &passengerBefore, 0, sizeof passengerBefore );
	Check( ReadObjectRecord( pSession, nPassenger, &passengerBefore ), "the passenger's record reads before the link" );
	nToken = -1;
	if ( !Check( BkEditorSetLink( pSession, nPassenger, nHost, &nToken ) == BK_EDITOR_OK && nToken >= 0, BkEditorLastMessage( pSession ) ) )
		return;
	{
		BkEditorObjectRecord passenger;
		memset( &passenger, 0, sizeof passenger );
		if ( Check( ReadObjectRecord( pSession, nPassenger, &passenger ), "the passenger's record reads" ) )
			Check( passenger.link_with == nHost, "the passenger's record names the host" );
	}
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, ( std::string( "the link undoes: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	{
		BkEditorObjectRecord passenger;
		memset( &passenger, 0, sizeof passenger );
		if ( Check( ReadObjectRecord( pSession, nPassenger, &passenger ), "the unlinked passenger's record reads" ) )
			Check( passenger.link_with == passengerBefore.link_with && passenger.x == passengerBefore.x && passenger.y == passengerBefore.y,
			       "the undone link puts the passenger's link and place back" );
	}
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "the engine agrees after the link's undo: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorSaveMap( pSession, szLinkUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szLinkBefore, szLinkUndone ), ( "the undone link saves the file before it byte for byte (" + DescribeDifference( szLinkBefore, szLinkUndone ) + ")" ).c_str() );
	Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, ( std::string( "the link redoes: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	{
		BkEditorObjectRecord passenger;
		memset( &passenger, 0, sizeof passenger );
		if ( Check( ReadObjectRecord( pSession, nPassenger, &passenger ), "the relinked passenger's record reads" ) )
			Check( passenger.link_with == nHost, "the redone link names the host again" );
	}
	remove( OsPath( szLinkBefore ).c_str() );
	remove( OsPath( szLinkUndone ).c_str() );
	Check( BkEditorUnlink( pSession, nPassenger, &nToken ) == BK_EDITOR_OK && nToken >= 0, "the unlink answers with a token" );
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		for ( int i = 0; i < nRead; ++i )
			if ( records[size_t( i )].link_id == nPassenger )
				Check( records[size_t( i )].link_with == 0, "and the record links with nothing" );
	}
	Check( BkEditorSetLink( pSession, nPassenger, nHost, &nToken ) == BK_EDITOR_OK, "the link goes back for the delete proof" );

	// Deleting the host takes the passengers; the restore brings every one
	// back, byte for byte.
	const std::string szHostGone = szScratch + "\\m3-host-gone.bzm";
	const std::string szHostBack = szScratch + "\\m3-host-back.bzm";
	Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorDeleteObject( pSession, nHost ) == BK_EDITOR_OK, ( std::string( "the host deletes: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		bool bHostGone = true, bPassengerGone = true;
		for ( int i = 0; i < nRead; ++i )
		{
			if ( records[size_t( i )].link_id == nHost ) bHostGone = false;
			if ( records[size_t( i )].link_id == nPassenger ) bPassengerGone = false;
		}
		Check( bHostGone, "the host is gone" );
		Check( bPassengerGone, "and the passenger went with it" );
	}
	Check( BkEditorSaveMap( pSession, szHostGone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorRestoreObject( pSession, nHost ) == BK_EDITOR_OK, ( std::string( "the host is restored: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		bool bPassengerBack = false;
		for ( int i = 0; i < nRead; ++i )
			if ( records[size_t( i )].link_id == nPassenger && records[size_t( i )].link_with == nHost )
				bPassengerBack = true;
		Check( bPassengerBack, "the passenger came back linked" );
	}
	Check( BkEditorSaveMap( pSession, szHostBack.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szUnedited, szHostBack ), "the host delete undone saves the unedited file byte for byte" );

	// The flag swap: a flag re-owned takes the party's general-side name,
	// exactly the MFC properties' swap. Found over the map's own flags; a map
	// with none the pair search above already proved the rest on.
	int nFlag = -1;
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		int nCatalogue = 0;
		BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
		std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
		int nCatalogueRead = 0;
		BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nCatalogueRead );
		for ( int i = 0; i < nRead && nFlag < 0; ++i )
			for ( int j = 0; j < nCatalogueRead && nFlag < 0; ++j )
				if ( catalogue[size_t( j )].game_type == 17 && records[size_t( i )].name == std::string( catalogue[size_t( j )].name ) )
					nFlag = records[size_t( i )].link_id;
	}
	if ( nFlag > 0 )
	{
		BkEditorObjectFieldsEdit swap;
		memset( &swap, 0, sizeof swap );
		swap.mask = 1;
		swap.player = 1;
		const std::string szNameBefore = [&] {
			int nCount = 0;
			BkEditorObjects( pSession, 0, 0, &nCount );
			std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
			int nRead = 0;
			BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
			for ( int i = 0; i < nRead; ++i )
				if ( records[size_t( i )].link_id == nFlag )
					return std::string( records[size_t( i )].name );
			return std::string();
		}();
		if ( Check( BkEditorSetObjectFields( pSession, nFlag, &swap, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			int nCount = 0;
			BkEditorObjects( pSession, 0, 0, &nCount );
			std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
			int nRead = 0;
			BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
			for ( int i = 0; i < nRead; ++i )
				if ( records[size_t( i )].link_id == nFlag )
				{
					const std::string szName( records[size_t( i )].name );
					Check( szName != szNameBefore && szName.rfind( "Flag_", 0 ) == 0,
					       ( "the flag swapped: " + szNameBefore + " -> " + szName ).c_str() );
					printf( "editor-bridge: M3 flag swap: %s -> %s\n", szNameBefore.c_str(), szName.c_str() );
				}
			Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, "the swap undoes" );
		}
	}
	else
		printf( "editor-bridge: M3 flag swap: no flag on the map, skipped\n" );

	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szHostGone ).c_str() );
	remove( OsPath( szHostBack ).c_str() );
	printf( "editor-bridge: M3 properties and links ok\n" );
}

// The bridge's record for one link ID, read fresh (M3 tests).
static bool ReadObjectRecord( BkEditorSession *pSession, int nLinkID, BkEditorObjectRecord *pOut )
{
	int nCount = 0;
	BkEditorObjects( pSession, 0, 0, &nCount );
	std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
	int nRead = 0;
	if ( BkEditorObjects( pSession, &( records[0] ), nCount, &nRead ) != BK_EDITOR_OK )
		return false;
	for ( int i = 0; i < nRead; ++i )
		if ( records[size_t( i )].link_id == nLinkID )
		{
			*pOut = records[size_t( i )];
			return true;
		}
	return false;
}

// The record a fresh read of the map file holds for one link ID (the
// expected-value builder's own lookup).
static SMapObjectInfo *FindMapRecord( CMapInfo *pMap, int nLinkID )
{
	std::vector<SMapObjectInfo> *lists[2] = { &pMap->objects, &pMap->scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
			if ( ( *lists[nList] )[i].link.nLinkID == nLinkID )
				return &( *lists[nList] )[i];
	return 0;
}

// M3 (D-29): the Damage tool's hit, on the engine. A hit moves the record's
// fHP by the percentage with the MFC's own clamps (1% floor for a unit), the
// engine still holds the object where the record says, the save is the
// builder's map (D-40.3), the undo is byte for byte, a squad floors at 1%
// too, a record no engine object carries stats for refuses without a crash
// (the MFC's unguarded FindByVis/pRPG dereference is not copied), and the
// repair mode sets full.
static void TestM3Damage( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The target: a whole UNIT of the catalogue (game type 1), whose 1%
	// floor the MFC's IsTechnics/IsHuman answers; and a SQUAD (game type
	// 15), whose soldiers IsHuman floors the same.
	int nTarget = -1, nSquad = -1;
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		int nCatalogue = 0;
		BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
		std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
		int nCatalogueRead = 0;
		BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nCatalogueRead );
		for ( int i = 0; i < nRead && ( nTarget < 0 || nSquad < 0 ); ++i )
		{
			const BkEditorObjectRecord &rRecord = records[size_t( i )];
			if ( rRecord.link_id <= 0 || !rRecord.known )
				continue;
			for ( int j = 0; j < nCatalogueRead; ++j )
			{
				if ( rRecord.name != std::string( catalogue[size_t( j )].name ) )
					continue;
				if ( nTarget < 0 && catalogue[size_t( j )].game_type == 1 && rRecord.hp == 1.0f )
					nTarget = rRecord.link_id;
				if ( nSquad < 0 && catalogue[size_t( j )].game_type == 15 )
					nSquad = rRecord.link_id;
				break;
			}
		}
	}
	if ( !Check( nTarget > 0, "a whole unit to damage is on the map" ) )
		return;
	Check( nSquad > 0, "a squad for the floor is on the map" );

	const std::string szUnedited = szScratch + "\\m3-damage-unedited.bzm";
	const std::string szDamaged = szScratch + "\\m3-damage-damaged.bzm";
	const std::string szUndone = szScratch + "\\m3-damage-undone.bzm";
	const std::string szRefused = szScratch + "\\m3-damage-refused.bzm";
	const std::string szPitCopy = szScratch + "\\m3-damage-pit.bzm";
	const std::string szPitUnedited = szScratch + "\\m3-damage-pit-unedited.bzm";
	Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	BkEditorObjectState engineBefore;
	memset( &engineBefore, 0, sizeof engineBefore );
	Check( BkEditorEngineObjectState( pSession, nTarget, &engineBefore ) == BK_EDITOR_OK, "the engine holds the unit" );

	// A 30% hit on a full unit: the 1% floor is not met, the record reads
	// 0.70 - the MFC's own float step (fHP - hpAdded).
	const float fExpected = 1.0f - 0.30f;
	int nToken = -1;
	if ( !Check( BkEditorDamageObject( pSession, nTarget, 0.30f, 0, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( nToken >= 0, "the damage has a token" );
	BkEditorObjectRecord record;
	memset( &record, 0, sizeof record );
	if ( Check( ReadObjectRecord( pSession, nTarget, &record ), "the damaged unit's record reads" ) )
		Check( record.hp == fExpected, "the record reads 70% after a 30% hit" );

	// The engine still holds the unit, where and as the record says: the
	// hit damages the live object, it does not re-place it.
	BkEditorObjectState engineState;
	memset( &engineState, 0, sizeof engineState );
	if ( Check( BkEditorEngineObjectState( pSession, nTarget, &engineState ) == BK_EDITOR_OK, "the engine still holds the object" ) )
	{
		Check( fabsf( engineState.x - record.x ) < 1.0f && fabsf( engineState.y - record.y ) < 1.0f,
		       ( "the engine's position is the record's (" + std::to_string( engineState.x ) + "," + std::to_string( engineState.y ) +
		         " vs " + std::to_string( record.x ) + "," + std::to_string( record.y ) + ")" ).c_str() );
		Check( engineState.player == record.player,
		       ( "the engine's player is the record's (" + std::to_string( engineState.player ) + " vs " + std::to_string( record.player ) + ")" ).c_str() );
		Check( engineState.x == engineBefore.x && engineState.y == engineBefore.y && engineState.dir == engineBefore.dir &&
		       engineState.player == engineBefore.player,
		       "and the hit moved, turned and re-owned nothing in the engine" );
	}
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "the engine agrees after the hit: " ) + BkEditorLastMessage( pSession ) ).c_str() );

	// D-40.3: the damaged save is the expected-value builder's map - the
	// same fHP on a fresh read of the file, no engine.
	{
		CMapInfo expected, saved;
		std::string szWhere;
		if ( Check( NMapFile::Read( BRIDGE_MAP, &expected, &szError ), szError.c_str() ) )
		{
			SMapObjectInfo *pExpected = FindMapRecord( &expected, nTarget );
			if ( Check( pExpected != 0, "the builder finds the unit" ) )
			{
				pExpected->fHP = fExpected;
				if ( Check( BkEditorSaveMap( pSession, szDamaged.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
				     Check( NMapFile::Read( szDamaged.c_str(), &saved, &szError ), szError.c_str() ) )
					Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
					       szWhere.empty() ? "the damaged save is the expected map" : ( "the damaged save differs at " + szWhere ).c_str() );
			}
		}
	}

	// The undo is byte for byte, and the engine agrees with the map again.
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, "the damage undoes" );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "the engine agrees after the undo: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szUnedited, szUndone ), ( "the undone damage saves the unedited file byte for byte (" + DescribeDifference( szUnedited, szUndone ) + ")" ).c_str() );

	// The 1% clamp: a 100% hit on the unit leaves 1%, not 0.
	nToken = -1;
	Check( BkEditorDamageObject( pSession, nTarget, 1.0f, 0, &nToken ) == BK_EDITOR_OK && nToken >= 0, BkEditorLastMessage( pSession ) );
	if ( Check( ReadObjectRecord( pSession, nTarget, &record ), "the clamped unit's record reads" ) )
		Check( record.hp == 0.01f, "a unit keeps 1% under a 100% hit (the MFC's clamp)" );
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, ( std::string( "and it undoes: " ) + BkEditorLastMessage( pSession ) ).c_str() );

	// The repair of a full unit changes nothing: OK with no token.
	nToken = -1;
	Check( BkEditorDamageObject( pSession, nTarget, 0.0f, 2, &nToken ) == BK_EDITOR_OK, "the repair answers" );
	Check( nToken == -1, "the repair of a full unit changes nothing" );
	if ( Check( ReadObjectRecord( pSession, nTarget, &record ), "the repaired unit's record reads" ) )
		Check( record.hp == 1.0f, "the repair leaves full" );
	// A real repair: damage first, then the mode-2 hit, then both undone.
	nToken = -1;
	Check( BkEditorDamageObject( pSession, nTarget, 0.4f, 0, &nToken ) == BK_EDITOR_OK && nToken >= 0, "the 40% hit for the repair answers with a token" );
	int nRepairToken = -1;
	Check( BkEditorDamageObject( pSession, nTarget, 0.0f, 2, &nRepairToken ) == BK_EDITOR_OK && nRepairToken >= 0, "the repair of a damaged unit answers with a token" );
	if ( Check( ReadObjectRecord( pSession, nTarget, &record ), "the repaired unit's record reads" ) )
		Check( record.hp == 1.0f, "the repair sets 100%" );
	Check( BkEditorUndoEdit( pSession, nRepairToken ) == BK_EDITOR_OK, ( std::string( "and the repair undoes: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	if ( Check( ReadObjectRecord( pSession, nTarget, &record ), "the unrepaired unit's record reads" ) )
		Check( record.hp == 1.0f - 0.4f, "back to the 40% hit" );
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, ( std::string( "and the hit undoes: " ) + BkEditorLastMessage( pSession ) ).c_str() );

	// A 0% hit changes nothing (the clamps leave nothing), and a percentage
	// out of range is the caller's bug.
	nToken = -1;
	Check( BkEditorDamageObject( pSession, nTarget, 0.0f, 0, &nToken ) == BK_EDITOR_OK && nToken == -1,
	       "a 0% hit changes nothing" );
	Check( BkEditorDamageObject( pSession, nTarget, 5.0f, 0, &nToken ) == BK_EDITOR_BAD_ARGUMENT,
	       "a percentage out of 0..1 is BAD_ARGUMENT" );

	// A squad floors at 1% like a unit (the MFC damaged its soldiers, which
	// IsHuman floors): a 100% hit leaves 1%, the engine agrees, and the undo
	// is the unedited file byte for byte.
	if ( nSquad > 0 )
	{
		nToken = -1;
		Check( BkEditorDamageObject( pSession, nSquad, 1.0f, 0, &nToken ) == BK_EDITOR_OK && nToken >= 0,
		       ( std::string( "a squad takes the hit: " ) + BkEditorLastMessage( pSession ) ).c_str() );
		if ( Check( ReadObjectRecord( pSession, nSquad, &record ), "the squad's record reads" ) )
			Check( record.hp == 0.01f, "a squad keeps 1% under a 100% hit" );
		Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "the engine agrees after the squad's hit: " ) + BkEditorLastMessage( pSession ) ).c_str() );
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, ( std::string( "and the squad's hit undoes: " ) + BkEditorLastMessage( pSession ) ).c_str() );
		Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( SameBytes( szUnedited, szUndone ), ( "the undone squad hit saves the unedited file byte for byte (" + DescribeDifference( szUnedited, szUndone ) + ")" ).c_str() );
	}

	// The stats refusal, no crash (the MFC bug is not copied - its
	// FindByVis answer and pRPG were dereferenced unguarded): a map copy
	// gains a record of a kind the engine never places (a tank pit, game
	// type 5 - engineers dig those during play), so the record is known,
	// editable, alone on its link ID and in the snapshot - every earlier
	// check passes - but no engine object carries stats for it. The
	// refusal names the stats and changes nothing: the save is still the
	// copy's unedited file, byte for byte.
	{
		std::string szPitName;
		{
			int nCatalogue = 0;
			BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
			std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
			int nCatalogueRead = 0;
			BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nCatalogueRead );
			for ( int j = 0; j < nCatalogueRead && szPitName.empty(); ++j )
				if ( catalogue[size_t( j )].game_type == SGVOGT_TANK_PIT )
					szPitName = catalogue[size_t( j )].name;
		}
		CMapInfo copy;
		if ( Check( !szPitName.empty(), "the database knows a tank pit" ) &&
		     Check( NMapFile::Read( BRIDGE_MAP, &copy, &szError ), szError.c_str() ) &&
		     Check( !copy.objects.empty(), "the copy has an object to stand the pit beside" ) )
		{
			int nPitLinkID = 0;
			std::vector<SMapObjectInfo> *lists[2] = { &copy.objects, &copy.scenarioObjects };
			for ( int nList = 0; nList < 2; ++nList )
				for ( size_t i = 0; i < lists[nList]->size(); ++i )
					nPitLinkID = Max( nPitLinkID, ( *lists[nList] )[i].link.nLinkID );
			++nPitLinkID;
			SMapObjectInfo pit = copy.objects[0];
			pit.szName = szPitName;
			pit.link.nLinkID = nPitLinkID;
			pit.link.nLinkWith = 0;
			pit.nScriptID = -1;
			pit.nFrameIndex = 0;
			pit.fHP = 1.0f;
			copy.objects.push_back( pit );
			if ( Check( NMapFile::Write( szPitCopy.c_str(), copy, &szError ), szError.c_str() ) &&
			     Check( BkEditorOpenMap( pSession, szPitCopy.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			{
				Check( BkEditorSaveMap( pSession, szPitUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
				BkEditorObjectRecord pitBefore;
				memset( &pitBefore, 0, sizeof pitBefore );
				Check( ReadObjectRecord( pSession, nPitLinkID, &pitBefore ) && pitBefore.known, "the pit's record reads, of a known type" );
				nToken = -1;
				Check( BkEditorDamageObject( pSession, nPitLinkID, 0.1f, 0, &nToken ) == BK_EDITOR_REFUSED && nToken == -1,
				       "a record no engine object carries stats for is refused" );
				Check( std::string( BkEditorLastMessage( pSession ) ).find( "stats" ) != std::string::npos,
				       ( "and the refusal names the stats, not the object: " + std::string( BkEditorLastMessage( pSession ) ) ).c_str() );
				BkEditorObjectRecord pitAfter;
				memset( &pitAfter, 0, sizeof pitAfter );
				if ( Check( ReadObjectRecord( pSession, nPitLinkID, &pitAfter ), "the pit's record still reads" ) )
					Check( pitAfter.hp == pitBefore.hp, "and its health is untouched" );
				Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
				Check( SameBytes( szPitUnedited, szRefused ), ( "the refused hit leaves the copy's file byte for byte (" + DescribeDifference( szPitUnedited, szRefused ) + ")" ).c_str() );
			}
		}
	}
	// And a link ID the map does not hold is refused earlier, as no object.
	Check( BkEditorDamageObject( pSession, 987654, 0.1f, 0, &nToken ) == BK_EDITOR_REFUSED, "a link ID off the map is refused" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "no object" ) != std::string::npos, "and names it" );

	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szDamaged ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szRefused ).c_str() );
	remove( OsPath( szPitCopy ).c_str() );
	remove( OsPath( szPitUnedited ).c_str() );
	printf( "editor-bridge: M3 damage ok\n" );
}

// M3 (D-25): the selection's batch move and delete-all, on the engine.
// Three movable objects - one a squad, whose record moves whole so its
// soldiers keep their offsets - move by one BkEditorMoveObjects call; the
// saved map is the builder's (the same moves applied to a fresh read through
// the overlay alone); the undo saves the unedited file byte for byte; a
// member that would leave the map refuses the whole move and changes
// nothing; and two deletes and their undo go through the cascade exactly.
static void TestM3MultiSelect( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// Three movable members: probed one link at a time with a small batch
	// move, exactly what a refusal means (a shared or unknown link ID, an
	// unknown type, a destination off the map). The third is a squad when the
	// map lets one move, so the squad-whole rule is in the batch.
	int members[3] = { -1, -1, -1 };
	int nFound = 0;
	bool bSquadAmongThem = false;
	int nSquadLink = -1;
	{
		int nCount = 0;
		BkEditorObjects( pSession, 0, 0, &nCount );
		std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
		int nRead = 0;
		BkEditorObjects( pSession, &( records[0] ), nCount, &nRead );
		int nCatalogue = 0;
		BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
		std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
		int nCatalogueRead = 0;
		BkEditorCatalogue( pSession, &( catalogue[0] ), nCatalogue, &nCatalogueRead );
		for ( int i = 0; i < nRead && nFound < 3; ++i )
		{
			const BkEditorObjectRecord &rRecord = records[size_t( i )];
			if ( rRecord.link_id <= 0 || !rRecord.known )
				continue;
			bool bSquad = false;
			for ( int j = 0; j < nCatalogueRead && !bSquad; ++j )
				bSquad = catalogue[size_t( j )].game_type == 15 && rRecord.name == std::string( catalogue[size_t( j )].name );
			int nToken = -1;
			const int nProbe[1] = { rRecord.link_id };
			if ( BkEditorMoveObjects( pSession, nProbe, 1, 32.0f, 0.0f, &nToken ) != BK_EDITOR_OK )
				continue;
			Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, "the probe move undoes" );
			if ( bSquad )
			{
				if ( nSquadLink >= 0 )
					continue;
				nSquadLink = rRecord.link_id;
				bSquadAmongThem = true;
			}
			members[nFound++] = rRecord.link_id;
		}
	}
	Check( nFound == 3, ( "three movable objects found (" + std::to_string( nFound ) + " tried)" ).c_str() );
	printf( "editor-bridge: M3 multi-select members %d, %d, %d (squad: %s)\n", members[0], members[1], members[2], bSquadAmongThem ? "yes" : "no" );

	// One batch move of the three: one token, one edit.
	const float fDx = 197.0f, fDy = 131.0f;
	const std::string szUnedited = szScratch + "\\m3-multi-unedited.bzm";
	const std::string szEdited = szScratch + "\\m3-multi-edited.bzm";
	const std::string szUndone = szScratch + "\\m3-multi-undone.bzm";
	const std::string szRefused = szScratch + "\\m3-multi-refused.bzm";
	Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	int nToken = -1;
	if ( !Check( BkEditorMoveObjects( pSession, members, 3, fDx, fDy, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( nToken >= 0, "the batch move has a token" );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "the engine agrees after the move: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	if ( !Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The expected value: the same moves on a fresh read, through the
	// overlay's own MoveObject - no engine involved.
	CMapInfo expected, saved;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() ) )
		return;
	NMapOverlay::SMoveObject move;
	move.vPos.z = 0.0f;
	for ( int i = 0; i < 3; ++i )
	{
		bool bFoundInExpected = false;
		const std::vector<SMapObjectInfo> *lists[2] = { &original.objects, &original.scenarioObjects };
		for ( int nList = 0; nList < 2 && !bFoundInExpected; ++nList )
			for ( size_t j = 0; j < lists[nList]->size() && !bFoundInExpected; ++j )
			{
				const SMapObjectInfo &rObject = ( *lists[nList] )[j];
				if ( rObject.link.nLinkID != members[i] )
					continue;
				bFoundInExpected = true;
				move.nLinkID = members[i];
				move.vPos.x = rObject.vPos.x + fDx;
				move.vPos.y = rObject.vPos.y + fDy;
				move.nDir = rObject.nDir;
				move.nPlayer = rObject.nPlayer;
			}
		Check( bFoundInExpected, ( "member " + std::to_string( members[i] ) + " is in the map" ).c_str() );
		Check( NMapOverlay::MoveObject( &expected, move ), "the builder moves the member" );
	}
	std::string szWhere;
	if ( !Check( NMapFile::Read( szEdited.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
	       szWhere.empty() ? "the batch move's save is the expected map" : ( "batch move differs at " + szWhere ).c_str() );

	// The band pick (M3, D-25): the screen rectangle over both members
	// answers at least the two links - the camera sits where the batch
	// move's placement put it, and the screen points come from
	// WorldToScreen of the members' moved positions, so the rectangle is
	// computed, not guessed. The two need not be on screen together (the
	// scene's rectangle pick selects its own patches from the rectangle).
	{
		CVec3 vA( 0.0f, 0.0f, 0.0f ), vB( 0.0f, 0.0f, 0.0f );
		SMapObjectInfo *pRecA = 0, *pRecB = 0;
		{
			std::vector<SMapObjectInfo> *lists[2] = { &original.objects, &original.scenarioObjects };
			for ( int nList = 0; nList < 2; ++nList )
				for ( size_t j = 0; j < lists[nList]->size(); ++j )
				{
					const int nID = ( *lists[nList] )[j].link.nLinkID;
					if ( nID == members[0] ) pRecA = &( *lists[nList] )[j];
					if ( nID == members[1] ) pRecB = &( *lists[nList] )[j];
				}
		}
		if ( Check( pRecA != 0 && pRecB != 0, "both members are in the original map" ) )
		{
			// Where the batch move put them - the expected map's positions,
			// which the engine was just checked to agree with.
			AI2Vis( &vA, pRecA->vPos.x + fDx, pRecA->vPos.y + fDy, 0.0f );
			AI2Vis( &vB, pRecB->vPos.x + fDx, pRecB->vPos.y + fDy, 0.0f );
			// The camera over member A, one frame drawn: WorldToScreen's
			// answers are the drawn ones (the squad-delete test's own recipe).
			Check( BkEditorSetCamera( pSession, vA.x, vA.y ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			Check( BkEditorFrame( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			float fAx = 0, fAy = 0, fBx = 0, fBy = 0;
			const bool bA = BkEditorWorldToScreen( pSession, vA.x, vA.y, &fAx, &fAy ) == BK_EDITOR_OK;
			const bool bB = BkEditorWorldToScreen( pSession, vB.x, vB.y, &fBx, &fBy ) == BK_EDITOR_OK;
			Check( bA && bB, "both members convert to the screen" );
			if ( bA && bB )
			{
				// The engine's band (CScene::Pick over a rectangle) takes a
				// sprite whose picture's centre is inside it
				// (Anim/SpriteAnimation.cpp), not one whose picture merely
				// meets it as BkEditorObjectAt's point does. WorldToScreen
				// answers the ground point a picture stands on, and a tree's
				// centre is well above that: so the band is normalized here,
				// whichever corner each member is in, and runs from a margin
				// below the ground points up by a screen's height, which no
				// picture outgrows.
				int nScreenW = 0, nScreenH = 0;
				Check( BkEditorScreenSize( pSession, &nScreenW, &nScreenH ) == BK_EDITOR_OK && nScreenH > 0, BkEditorLastMessage( pSession ) );
				const float fLeft = Min( fAx, fBx ) - 40.0f, fTop = Min( fAy, fBy ) - float( nScreenH );
				const float fRight = Max( fAx, fBx ) + 40.0f, fBottom = Max( fAy, fBy ) + 40.0f;
				// Two-pass: the sizing call answers the total; a buffer one
				// short is refused with the total and nothing written past its
				// capacity; the buffer the sizing pass asked for reads every
				// link. Only what an OK read wrote is looked at.
				int nTotal = 0;
				const BkEditorStatus nSizing = BkEditorPickObjects( pSession, fLeft, fTop, fRight, fBottom, 0, 0, &nTotal );
				Check( nSizing == BK_EDITOR_REFUSED && nTotal >= 2,
				       ( "the band over both members sizes at least the 2, got " + std::to_string( nTotal ) ).c_str() );
				if ( nTotal >= 2 )
				{
					std::vector<int> picked( size_t( nTotal ), -1 );
					int nShortCount = 0;
					Check( BkEditorPickObjects( pSession, fLeft, fTop, fRight, fBottom, &( picked[0] ), nTotal - 1, &nShortCount ) == BK_EDITOR_REFUSED && nShortCount == nTotal,
					       "a short band buffer is refused with the total" );
					Check( picked[size_t( nTotal - 1 )] == -1, "and nothing is written past its capacity" );
					int nPickedCount = 0;
					Check( BkEditorPickObjects( pSession, fLeft, fTop, fRight, fBottom, &( picked[0] ), nTotal, &nPickedCount ) == BK_EDITOR_OK && nPickedCount == nTotal,
					       "the band's full buffer reads every link" );
					bool bHasA = false, bHasB = false;
					for ( int i = 0; i < nPickedCount && i < nTotal; ++i )
					{
						bHasA = bHasA || picked[size_t( i )] == members[0];
						bHasB = bHasB || picked[size_t( i )] == members[1];
					}
					Check( bHasA && bHasB, "and the band's links include the two members" );
				}
			}
		}
	}

	// The undo is the unedited file byte for byte, and the redo moves again.
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, "the batch move undoes" );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "the engine agrees after the undo: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szUnedited, szUndone ), "the undone batch move saves the unedited file byte for byte" );
	Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, "and redoes" );
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, "and back once more" );

	// One member that would leave the map refuses the whole move, whatever
	// the other two asked for.
	const int nBad[3] = { members[0], members[1], members[2] };
	nToken = -1;
	Check( BkEditorMoveObjects( pSession, nBad, 3, -100000.0f, 0.0f, &nToken ) == BK_EDITOR_REFUSED,
	       "a member off the map refuses the whole move" );
	Check( nToken == -1, "and hands out no token" );
	Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szUnedited, szRefused ), "the refusal changed nothing: the map saves unedited byte for byte" );

	// Two deletes and their undo, through the cascade - the two non-squad
	// members, so the squad stays for the earlier tiers' expectations.
	int nPair[2] = { -1, -1 };
	{
		int nAt = 0;
		for ( int i = 0; i < 3 && nAt < 2; ++i )
			if ( members[i] != nSquadLink )
				nPair[nAt++] = members[i];
	}
	Check( nPair[0] > 0 && nPair[1] > 0, "two non-squad members to delete" );
	Check( BkEditorDeleteObject( pSession, nPair[0] ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorDeleteObject( pSession, nPair[1] ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorRestoreObject( pSession, nPair[0] ) == BK_EDITOR_OK, "the first restore puts its object back" );
	Check( BkEditorDeleteObject( pSession, nPair[0] ) == BK_EDITOR_OK, "and it deletes again (the two-delete walk)" );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "the engine agrees after the deletes: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorRestoreObject( pSession, nPair[1] ) == BK_EDITOR_OK, "the second restore puts its object back" );
	Check( BkEditorRestoreObject( pSession, nPair[0] ) == BK_EDITOR_OK, "and the first" );
	const std::string szRestored = szScratch + "\\m3-multi-restored.bzm";
	Check( BkEditorSaveMap( pSession, szRestored.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szUnedited, szRestored ), "the deletes undone restore every reference: byte for byte" );

	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szRefused ).c_str() );
	remove( OsPath( szRestored ).c_str() );
	printf( "editor-bridge: M3 multi-select ok\n" );
}

// A tile the map's tileset has no terrain type for is the caller's mistake:
// BK_EDITOR_BAD_ARGUMENT, naming the tile, and nothing painted - not even the
// cells beside it that name a good tile. Tile 1 is in none of the shipped
// tilesets; 255 is what a caller's -1 becomes in the cell's unsigned char.
static void TestPaintRefusesTileOutsideTileset( BkEditorSession *pSession ){
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
// Tile properties (05-02, D-35/TR2): the describe surface answers the
// terrain type's name and its variant count for EVERY tile the tileset
// lists, tile 0 included - the MFC's `> 0` guard
// (TabTileEditDialog.cpp:316) left its first tile without properties. A
// tile the tileset does not list is refused; a null out is BAD_ARGUMENT.
static void TestM3TileInfo( BkEditorSession *pSession )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) ) return;
	std::vector<unsigned char> tiles;
	{
		int nTiles = 0;
		Check( BkEditorTilesetTiles( pSession, 0, 0, &nTiles ) == BK_EDITOR_REFUSED && nTiles > 0, "the tile list sizes" );
		tiles.resize( nTiles );
		Check( BkEditorTilesetTiles( pSession, &( tiles[0] ), nTiles, &nTiles ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	}
	// Tile 0 itself: offered, and described - a name, a variant count of at
	// least one (the type lists the tile that names it), and the tileset's
	// own description name.
	{
		bool bZeroOffered = false;
		for ( size_t i = 0; i < tiles.size(); ++i ) bZeroOffered = bZeroOffered || tiles[i] == 0;
		Check( bZeroOffered, "tile 0 is one the tileset offers" );
		BkEditorTile info;
		memset( &info, 0, sizeof info );
		Check( BkEditorDescribeTile( pSession, 0, &info ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( info.terrain[0] != 0, "tile 0's terrain type has a name" );
		Check( info.variant_count >= 1, "tile 0's terrain type counts its variants" );
		Check( info.terrain_index >= 0, "tile 0 answers its terrain type's position" );
	}
	// Every offered tile answers; the variant count agrees with the type's
	// own list, and every count is at least one.
	{
		bool bAll = true, bCounts = true;
		for ( size_t i = 0; i < tiles.size() && bAll; ++i )
		{
			BkEditorTile info;
			memset( &info, 0, sizeof info );
			if ( BkEditorDescribeTile( pSession, tiles[i], &info ) != BK_EDITOR_OK || info.terrain[0] == 0 )
				bAll = false;
			else if ( info.variant_count < 1 )
				bCounts = false;
		}
		Check( bAll, "every offered tile describes" );
		Check( bCounts, "and every description counts at least one variant" );
	}
	// A tile no terrain type lists is REFUSED (tile 1 is in none of the
	// shipped tilesets - the paint-refusal test's own finding), and a null
	// out is BAD_ARGUMENT.
	{
		BkEditorTile info;
		memset( &info, 0, sizeof info );
		Check( BkEditorDescribeTile( pSession, 1, &info ) == BK_EDITOR_REFUSED, "tile 1, in no terrain type, is REFUSED" );
		Check( BkEditorDescribeTile( pSession, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null out is BAD_ARGUMENT" );
	}
	printf( "editor-bridge: M3 tile info ok\n" );
}

// The MFC editor's matcher (SSimpleFilter::Check with its caller's ToLower):
// every word of some one condition list must appear in the lowercased folder.
static bool FolderMatches( const BkEditorObjectFilter *pFilter, const char *pszFolder )
{
	if ( pFilter == 0 || pFilter->list_count == 0 )
		return false;
	std::string szFolder = pszFolder;
	NStr::ToLower( szFolder );
	for ( int i = 0; i < pFilter->list_count; ++i )
	{
		bool bAll = true;
		for ( int w = 0; w < pFilter->lists[i].word_count && bAll; ++w )
		{
			if ( szFolder.find( pFilter->lists[i].words[w] ) == std::string::npos )
				bAll = false;
		}
		if ( bAll )
			return true;
	}
	return false;
}

static const BkEditorObjectFilter *pBuildingsOf( const std::vector<BkEditorObjectFilter> &rFilters )
{
	for ( const BkEditorObjectFilter &rFilter : rFilters )
		if ( strcmp( rFilter.name, "Buildings" ) == 0 )
			return &rFilter;
	return 0;
}

// The object filters (M3, D-31): the shipped Data/Editor/filter.xml answers
// through the engine's own reader, a user file written under a scratch user
// root merges over it (user wins by name), and the catalogue's folder keys
// filter as the MFC editor's matcher predicts. Filters are installation
// data: the whole test runs with a map open or not, and the shipped file is
// never written.
static void TestM3Fields( BkEditorSession *pSession, const std::string &szScratch );
static int M3CountObjects( BkEditorSession *pSession );
static void TestM3Filters( BkEditorSession *pSession, const char *pszRoot, const std::string &szScratch )
{
	// Argument rules first: a null out_count and a negative capacity are
	// BAD_ARGUMENT; a null buffer with capacity 0 sizes.
	{
		int nCount = -1;
		Check( BkEditorObjectFilters( pSession, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "BkEditorObjectFilters with a null out_count is BAD_ARGUMENT" );
		Check( BkEditorObjectFilters( pSession, 0, -1, &nCount ) == BK_EDITOR_BAD_ARGUMENT, "a negative capacity is BAD_ARGUMENT" );
		Check( BkEditorObjectFilters( pSession, 0, 0, &nCount ) == BK_EDITOR_REFUSED && nCount > 0,
		       NStr::Format( "the sizing pass is REFUSED with the total there (%d)", nCount ) );
	}
	std::vector<BkEditorObjectFilter> shipped;
	{
		int nTotal = 0;
		if ( !Check( BkEditorObjectFilters( pSession, 0, 0, &nTotal ) == BK_EDITOR_REFUSED && nTotal > 0, "the filters size" ) )
			return;
		shipped.resize( size_t( nTotal ) );
		int nRead = 0;
		if ( !Check( BkEditorObjectFilters( pSession, &( shipped[0] ), nTotal, &nRead ) == BK_EDITOR_OK && nRead == nTotal,
		             BkEditorLastMessage( pSession ) ) )
			return;
		Check( nRead >= 9, NStr::Format( "the shipped file holds its filters (%d read)", nRead ) );
		// The MFC editor's own first entry: Buildings, one condition, the
		// folder word "buildings", from the shipped file (user 0).
		const BkEditorObjectFilter *pBuildings = 0;
		for ( const BkEditorObjectFilter &rFilter : shipped )
			if ( strcmp( rFilter.name, "Buildings" ) == 0 )
				pBuildings = &rFilter;
		if ( !Check( pBuildings != 0, "Buildings is among the shipped filters" ) )
			return;
		Check( pBuildings->user == 0, "a shipped filter answers user 0" );
		Check( pBuildings->list_count >= 1 && pBuildings->lists[0].word_count >= 1 &&
			       strcmp( pBuildings->lists[0].words[0], "buildings" ) == 0,
		       "Buildings' condition is the folder word buildings" );
		// Ordered by name: the read is stable across runs.
		bool bOrdered = true;
		for ( size_t i = 1; i < shipped.size(); ++i )
			bOrdered = bOrdered && strcmp( shipped[i - 1].name, shipped[i].name ) < 0;
		Check( bOrdered, "the read is ordered by name" );
	}

	// The user file: redirect the user root into the scratch, save a modified
	// Buildings plus a new filter, and read the merge back. The shipped file
	// is untouched; only what the caller marked user leaves the bridge.
	const std::string szOriginalUser = NPlatform::Paths::UserRoot();
	const std::string szScratchUser = ( std::filesystem::path( szScratch ) / "filters-user" ).string() + "/";
	NPlatform::Paths::SetInjectedRootsForTest( pszRoot, szScratchUser.c_str() );
	{
		std::vector<BkEditorObjectFilter> user;
		BkEditorObjectFilter editedBuildings = *pBuildingsOf( shipped );
		editedBuildings.user = 1;
		strcpy( editedBuildings.lists[0].words[1], "africa" );
		editedBuildings.lists[0].word_count = 2;
		user.push_back( editedBuildings );
		BkEditorObjectFilter added;
		memset( &added, 0, sizeof added );
		strcpy( added.name, "Editor Test Filter" );
		added.list_count = 1;
		added.lists[0].word_count = 2;
		strcpy( added.lists[0].words[0], "editor" );
		strcpy( added.lists[0].words[1], "test" );
		added.user = 1;
		user.push_back( added );
		Check( BkEditorSaveObjectFilters( pSession, &( user[0] ), int( user.size() ) ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		std::error_code error;
		Check( std::filesystem::exists( std::filesystem::path( szScratchUser ) / "mapeditor" / "filter.xml", error ),
		       "the user file exists under <user root>mapeditor" );

		// The merge: the user's Buildings replaces the shipped one in place
		// (user 1, the added word), the new filter is appended, and a shipped
		// name the user has not touched still answers with user 0.
		std::vector<BkEditorObjectFilter> merged;
		int nTotal = 0;
		Check( BkEditorObjectFilters( pSession, 0, 0, &nTotal ) == BK_EDITOR_REFUSED && nTotal == int( shipped.size() ) + 1,
		       NStr::Format( "the merge is the shipped set plus the user-only one (%d)", nTotal ) );
		merged.resize( size_t( nTotal ) );
		int nRead = 0;
		if ( !Check( BkEditorObjectFilters( pSession, &( merged[0] ), nTotal, &nRead ) == BK_EDITOR_OK && nRead == nTotal,
		             BkEditorLastMessage( pSession ) ) )
		{
			NPlatform::Paths::SetInjectedRootsForTest( pszRoot, szOriginalUser.c_str() );
			return;
		}
		const BkEditorObjectFilter *pMergedBuildings = 0, *pMergedNew = 0, *pShippedOnly = 0;
		for ( const BkEditorObjectFilter &rFilter : merged )
		{
			if ( strcmp( rFilter.name, "Buildings" ) == 0 ) pMergedBuildings = &rFilter;
			if ( strcmp( rFilter.name, "Editor Test Filter" ) == 0 ) pMergedNew = &rFilter;
			if ( strcmp( rFilter.name, "Obj T Russian" ) == 0 ) pShippedOnly = &rFilter;
		}
		Check( pMergedBuildings != 0 && pMergedBuildings->user == 1 && pMergedBuildings->lists[0].word_count == 2 &&
		       strcmp( pMergedBuildings->lists[0].words[1], "africa" ) == 0,
		       "the user's Buildings overrides the shipped one" );
		Check( pMergedNew != 0 && pMergedNew->user == 1 && pMergedNew->list_count == 1, "the user-only filter is appended" );
		Check( pShippedOnly != 0 && pShippedOnly->user == 0 && pShippedOnly->list_count >= 1,
		       "a shipped name the user has not touched keeps user 0" );

		// The catalogue filters as the MFC matcher predicts: a
		// buildings-folder key passes Buildings, a units-folder key does not,
		// and the added africa word admits the African building folders.
		{
			int nCatalogue = 0;
			Check( BkEditorCatalogue( pSession, 0, 0, &nCatalogue ) == BK_EDITOR_REFUSED && nCatalogue > 0, "the catalogue sizes" );
			std::vector<BkEditorCatalogueEntry> entries = std::vector<BkEditorCatalogueEntry>( size_t( nCatalogue ) );
			int nGot = 0;
			Check( BkEditorCatalogue( pSession, &( entries[0] ), nCatalogue, &nGot ) == BK_EDITOR_OK && nGot == nCatalogue,
			       BkEditorLastMessage( pSession ) );
			const BkEditorCatalogueEntry *pBuilding = 0, *pUnit = 0;
			for ( const BkEditorCatalogueEntry &rEntry : entries )
			{
				if ( pBuilding == 0 && _strnicmp( rEntry.path, "buildings", 9 ) == 0 && rEntry.placeable != 0 ) pBuilding = &rEntry;
				if ( pUnit == 0 && _strnicmp( rEntry.path, "units", 5 ) == 0 && rEntry.placeable != 0 ) pUnit = &rEntry;
			}
			if ( !Check( pBuilding != 0 && pUnit != 0, "the catalogue has a buildings-folder and a units-folder object" ) )
			{
				NPlatform::Paths::SetInjectedRootsForTest( pszRoot, szOriginalUser.c_str() );
				return;
			}
			Check( FolderMatches( pMergedBuildings, pBuilding->path ), "a buildings-folder object passes the merged Buildings" );
			Check( !FolderMatches( pMergedBuildings, pUnit->path ), "a units-folder object does not" );
		}

		// A word that is not NUL-terminated within its 32 bytes is
		// BAD_ARGUMENT, and the refusal wrote nothing (the file from the
		// successful save above still reads back the same).
		BkEditorObjectFilter bad = added;
		memset( bad.lists[0].words[0], 'x', BK_EDITOR_FILTER_WORD_LEN );
		Check( BkEditorSaveObjectFilters( pSession, &bad, 1 ) == BK_EDITOR_BAD_ARGUMENT, "an unterminated word is BAD_ARGUMENT" );
		Check( BkEditorSaveObjectFilters( pSession, 0, 1 ) == BK_EDITOR_BAD_ARGUMENT, "null filters with a count is BAD_ARGUMENT" );
		Check( BkEditorSaveObjectFilters( pSession, 0, 0 ) == BK_EDITOR_OK, "count 0 writes the empty set" );
		int nAfter = 0;
		Check( BkEditorObjectFilters( pSession, 0, 0, &nAfter ) == BK_EDITOR_REFUSED && nAfter == int( shipped.size() ),
		       NStr::Format( "the empty user file leaves the shipped set alone (%d)", nAfter ) );
	}
	NPlatform::Paths::SetInjectedRootsForTest( pszRoot, szOriginalUser.c_str() );
	printf( "editor-bridge: M3 filters ok\n" );
}

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

// 03-15 gap fix (Johannes's M1 hand try: "tile 0", "tile 1", ... could only
// be told apart by painting each): BkEditorDescribeTile names every tile the
// tileset offers by its terrain type, and BkEditorTilePicture cuts each one's
// diamond out of the tileset texture. On coldwinter: every offered tile
// decodes to a picture that is not empty, not one flat colour, transparent in
// its corners and opaque in its middle; tiles of different terrain types
// differ; the refusals follow the contract. All of them are written side by
// side to <scratch>/03-15-tile-pictures.tga for a person to look at. Then
// BkEditorCloseMap closes the map (File > Close) and the map-needing calls
// refuse again, and coldwinter is reopened for whatever runs next.
static void TestTilePicturesAndClose( BkEditorSession *pSession, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	unsigned char tiles[256];
	int nCount = 0;
	if ( !Check( BkEditorTilesetTiles( pSession, tiles, 256, &nCount ) == BK_EDITOR_OK && nCount > 1, BkEditorLastMessage( pSession ) ) )
		return;

	const int nSide = 64;
	std::vector<unsigned char> buffer( nSide * nSide * 4 );
	// Every picture, for the contact sheet: a cell per tile, 16 to a row.
	const int nColumns = 16, nCellWidth = nSide + 4, nCellHeight = nSide / 2 + 4;
	const int nRows = ( nCount + nColumns - 1 ) / nColumns;
	std::vector<unsigned char> sheet( size_t( nColumns * nCellWidth ) * size_t( nRows * nCellHeight ) * 4, 0 );
	for ( size_t i = 3; i < sheet.size(); i += 4 )
		sheet[i] = 255;

	std::string szTileset;
	std::map<int, std::string> terrainNames;
	std::map<int, std::vector<unsigned char> > firstOfTerrain;
	std::set<std::vector<unsigned char> > distinct;
	int nGood = 0;
	double fFirstSeconds = 0.0, fRestSeconds = 0.0;
	for ( int i = 0; i < nCount; ++i )
	{
		const int nTile = tiles[i];
		BkEditorTile info;
		if ( !Check( BkEditorDescribeTile( pSession, nTile, &info ) == BK_EDITOR_OK,
		             NStr::Format( "tile %d describes: %s", nTile, BkEditorLastMessage( pSession ) ) ) )
			continue;
		Check( info.terrain[0] != 0 && info.terrain_index >= 0, NStr::Format( "tile %d has a terrain type name (%d '%s')", nTile, info.terrain_index, info.terrain ) );
		if ( szTileset.empty() )
			szTileset = info.tileset;
		Check( szTileset == info.tileset && !szTileset.empty(), NStr::Format( "tile %d names the same tileset (%s, %s)", nTile, szTileset.c_str(), info.tileset ) );
		terrainNames[info.terrain_index] = info.terrain;

		int nWidth = 0, nHeight = 0;
		const Uint64 nStart = SDL_GetPerformanceCounter();
		const BkEditorStatus status = BkEditorTilePicture( pSession, nTile, &buffer[0], int( buffer.size() ), nSide, &nWidth, &nHeight );
		const double fSeconds = double( SDL_GetPerformanceCounter() - nStart ) / double( SDL_GetPerformanceFrequency() );
		( i == 0 ? fFirstSeconds : fRestSeconds ) += fSeconds;
		if ( !Check( status == BK_EDITOR_OK, NStr::Format( "tile %d has a picture: %s", nTile, BkEditorLastMessage( pSession ) ) ) )
			continue;
		if ( !Check( nWidth >= 8 && nWidth <= nSide && nHeight >= 4 && nHeight <= nSide,
		             NStr::Format( "tile %d's picture is %dx%d", nTile, nWidth, nHeight ) ) )
			continue;
		// The diamond: its middle opaque, its four corners transparent.
		const unsigned char *pMiddle = &buffer[( size_t( nHeight / 2 ) * nWidth + nWidth / 2 ) * 4];
		Check( pMiddle[3] == 255, NStr::Format( "tile %d's middle is opaque (alpha %d)", nTile, int( pMiddle[3] ) ) );
		Check( buffer[3] == 0 && buffer[( size_t( nWidth ) - 1 ) * 4 + 3] == 0 &&
		       buffer[( size_t( nHeight - 1 ) * nWidth ) * 4 + 3] == 0 && buffer[( size_t( nHeight ) * nWidth - 1 ) * 4 + 3] == 0,
		       NStr::Format( "tile %d's corners are transparent", nTile ) );
		// Not one flat colour: the opaque pixels vary.
		int nMinLuma = 256 * 3, nMaxLuma = -1, nOpaque = 0;
		for ( int p = 0; p < nWidth * nHeight; ++p )
		{
			if ( buffer[p * 4 + 3] != 255 )
				continue;
			++nOpaque;
			const int nLuma = buffer[p * 4 + 0] + buffer[p * 4 + 1] + buffer[p * 4 + 2];
			nMinLuma = Min( nMinLuma, nLuma );
			nMaxLuma = Max( nMaxLuma, nLuma );
		}
		const bool bOk = Check( nOpaque >= nWidth * nHeight / 3, NStr::Format( "tile %d's diamond covers a third of its box or more (%d of %d)", nTile, nOpaque, nWidth * nHeight ) ) &&
		                 Check( nMaxLuma - nMinLuma >= 12, NStr::Format( "tile %d's picture is not one flat colour (luma %d..%d)", nTile, nMinLuma, nMaxLuma ) ) &&
		                 Check( nMaxLuma > 0, NStr::Format( "tile %d's picture is not black", nTile ) );
		if ( !bOk )
			continue;
		++nGood;
		const std::vector<unsigned char> picture( buffer.begin(), buffer.begin() + size_t( nWidth ) * nHeight * 4 );
		distinct.insert( picture );
		if ( firstOfTerrain.find( info.terrain_index ) == firstOfTerrain.end() )
			firstOfTerrain[info.terrain_index] = picture;
		// Onto the contact sheet, over black.
		const int nCellX = ( i % nColumns ) * nCellWidth + 2, nCellY = ( i / nColumns ) * nCellHeight + 2;
		for ( int y = 0; y < nHeight && y < nCellHeight - 4; ++y )
			for ( int x = 0; x < nWidth && x < nCellWidth - 4; ++x )
			{
				const unsigned char *pPixel = &picture[( size_t( y ) * nWidth + x ) * 4];
				unsigned char *pTarget = &sheet[( size_t( nCellY + y ) * ( nColumns * nCellWidth ) + nCellX + x ) * 4];
				for ( int ch = 0; ch < 3; ++ch )
					pTarget[ch] = (unsigned char)( pPixel[ch] * pPixel[3] / 255 );
			}
	}
	Check( nGood == nCount, NStr::Format( "every offered tile has a good picture (%d of %d)", nGood, nCount ) );
	// Tiles of different terrain types show different ground.
	int nSamePairs = 0;
	for ( std::map<int, std::vector<unsigned char> >::const_iterator a = firstOfTerrain.begin(); a != firstOfTerrain.end(); ++a )
		for ( std::map<int, std::vector<unsigned char> >::const_iterator b = a; ++b != firstOfTerrain.end(); )
			if ( a->second == b->second )
				++nSamePairs;
	Check( firstOfTerrain.size() >= 2 && nSamePairs == 0,
	       NStr::Format( "the first tiles of the %d terrain types all differ (%d identical pairs)", int( firstOfTerrain.size() ), nSamePairs ) );
	Check( int( distinct.size() ) * 2 >= nCount, NStr::Format( "most tiles have a picture of their own (%d distinct of %d)", int( distinct.size() ), nCount ) );
	std::string szTerrains;
	for ( std::map<int, std::string>::const_iterator it = terrainNames.begin(); it != terrainNames.end(); ++it )
		szTerrains += ( szTerrains.empty() ? "" : ", " ) + it->second;
	printf( "editor-bridge: tile pictures: %s's tileset %s: %d tiles, %d distinct pictures, %d terrain types (%s)\n",
	        SHIPPED_MAP, szTileset.c_str(), nCount, int( distinct.size() ), int( terrainNames.size() ), szTerrains.c_str() );
	printf( "editor-bridge: tile pictures: first %.2f ms (decodes the texture), then %.3f ms each\n",
	        fFirstSeconds * 1000.0, nCount > 1 ? fRestSeconds * 1000.0 / ( nCount - 1 ) : 0.0 );
	const std::string szSheet = szScratch + "/03-15-tile-pictures.tga";
	const bool bSheetWritten = WriteRgbaTga( szSheet.c_str(), &sheet[0], nColumns * nCellWidth, nRows * nCellHeight );
	printf( "editor-bridge: tile pictures: %s %s\n", bSheetWritten ? "saved" : "could not save", szSheet.c_str() );

	// The contract's refusals.
	BkEditorTile info;
	Check( BkEditorDescribeTile( pSession, 1, &info ) == BK_EDITOR_REFUSED && info.terrain_index == -1 && info.terrain[0] == 0,
	       "tile 1, in no shipped tileset, is refused and zeroed" );
	Check( BkEditorDescribeTile( pSession, 256, &info ) == BK_EDITOR_BAD_ARGUMENT, "tile 256 is a bad argument" );
	Check( BkEditorDescribeTile( pSession, tiles[0], 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null out is a bad argument" );
	int nWidth = 0, nHeight = 0;
	Check( BkEditorTilePicture( pSession, 1, &buffer[0], int( buffer.size() ), nSide, &nWidth, &nHeight ) == BK_EDITOR_REFUSED, "tile 1 has no picture" );
	Check( BkEditorTilePicture( pSession, -1, &buffer[0], int( buffer.size() ), nSide, &nWidth, &nHeight ) == BK_EDITOR_BAD_ARGUMENT, "tile -1 is a bad argument" );
	Check( BkEditorTilePicture( pSession, tiles[0], &buffer[0], int( buffer.size() ), 4, &nWidth, &nHeight ) == BK_EDITOR_BAD_ARGUMENT, "max_side 4 is a bad argument" );
	// Not "small": the Windows SDK's rpcndr.h defines that as a macro for char.
	unsigned char tinyBuffer[16];
	Check( BkEditorTilePicture( pSession, tiles[0], tinyBuffer, sizeof tinyBuffer, nSide, &nWidth, &nHeight ) == BK_EDITOR_REFUSED && nWidth > 0 && nHeight > 0,
	       "a 16-byte buffer is refused and still told the real size" );
	// Scaled to fit a smaller side, keeping the shape.
	Check( BkEditorTilePicture( pSession, tiles[0], &buffer[0], int( buffer.size() ), 16, &nWidth, &nHeight ) == BK_EDITOR_OK && nWidth <= 16 && nHeight <= 16 && nWidth > nHeight,
	       NStr::Format( "max_side 16 scales the picture down to fit, still wider than tall (%dx%d)", nWidth, nHeight ) );

	// File > Close: the map is gone from the session and from the engine.
	Check( BkEditorCloseMap( pSession ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	int nAfter = -1;
	Check( BkEditorTilesetTiles( pSession, tiles, 256, &nAfter ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ) == "no map is open",
	       "after BkEditorCloseMap the tileset is refused: no map is open" );
	Check( BkEditorTilePicture( pSession, 0, &buffer[0], int( buffer.size() ), nSide, &nWidth, &nHeight ) == BK_EDITOR_REFUSED, "and so is a tile picture" );
	int nObjects = -1;
	Check( BkEditorObjects( pSession, 0, 0, &nObjects ) == BK_EDITOR_REFUSED, "and the object list" );
	Check( BkEditorCloseMap( pSession ) == BK_EDITOR_OK, "a second close is harmless" );
	Check( BkEditorFrame( pSession ) == BK_EDITOR_OK, NStr::Format( "a frame still draws with no map open: %s", BkEditorLastMessage( pSession ) ) );
	Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, NStr::Format( "coldwinter opens again after the close: %s", BkEditorLastMessage( pSession ) ) );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, NStr::Format( "and the world matches it: %s", BkEditorLastMessage( pSession ) ) );
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

// The engine takes a path with backslashes on every platform; the C library
// and the standard streams do not, so a path this test opens itself goes
// through here first (Windows takes the forward slashes as well).
static std::string OsPath( std::string szPath )
{
	for ( std::string::size_type i = 0; i < szPath.size(); ++i )
		if ( szPath[i] == '\\' ) szPath[i] = '/';
	return szPath;
}

// The bytes of two files are the same. Read whole; the largest shipped map is
// 1.6 MB.
static bool SameBytes( const std::string &szLeft, const std::string &szRight )
{
	std::ifstream left( OsPath( szLeft ).c_str(), std::ios::binary ), right( OsPath( szRight ).c_str(), std::ios::binary );
	if ( !left || !right )
		return false;
	const std::string szL( ( std::istreambuf_iterator<char>( left ) ), std::istreambuf_iterator<char>() );
	const std::string szR( ( std::istreambuf_iterator<char>( right ) ), std::istreambuf_iterator<char>() );
	return szL == szR;
}

// Where two files first differ, for a failed byte comparison to say: sizes and
// the first offset (or "same").
static std::string DescribeDifference( const std::string &szLeft, const std::string &szRight )
{
	std::ifstream left( OsPath( szLeft ).c_str(), std::ios::binary ), right( OsPath( szRight ).c_str(), std::ios::binary );
	if ( !left || !right )
		return "a file would not open";
	const std::string szL( ( std::istreambuf_iterator<char>( left ) ), std::istreambuf_iterator<char>() );
	const std::string szR( ( std::istreambuf_iterator<char>( right ) ), std::istreambuf_iterator<char>() );
	size_t nAt = 0;
	while ( nAt < szL.size() && nAt < szR.size() && szL[nAt] == szR[nAt] )
		++nAt;
	return NStr::Format( "sizes %d and %d, first difference at offset %d", int( szL.size() ), int( szR.size() ), int( nAt ) );
}

static std::vector<BkEditorObjectRecord> ReadObjectRecords( BkEditorSession *pSession )
{
	int nCount = 0;
	BkEditorObjects( pSession, 0, 0, &nCount );
	std::vector<BkEditorObjectRecord> records( nCount > 0 ? nCount : 1 );
	int nRead = 0;
	if ( BkEditorObjects( pSession, &records[0], nCount, &nRead ) != BK_EDITOR_OK )
		nRead = 0;
	records.resize( nRead );
	return records;
}

// D-05: a bridge span, a trench piece and a fence are drawn with their own
// tools in M2, so the catalogue reports them not placeable and BkEditorAddObject
// refuses them naming the tool - while a loaded map's spans, pieces and fences
// still load, draw and move (WhyNotAMapObject, which also guards
// PlaceOneObject, is untouched).
static void TestM2PaletteFilter( BkEditorSession *pSession, const std::string &szScratch )
{
	(void)szScratch;
	int nCatalogue = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
	if ( !Check( nCatalogue > 0, "the catalogue has entries" ) )
		return;
	std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue );
	int nCatalogueRead = 0;
	if ( !Check( BkEditorCatalogue( pSession, &catalogue[0], nCatalogue, &nCatalogueRead ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// Every entry of game type 4, 6 and 9 is not placeable.
	std::string szTrench, szSpan, szFence;
	std::set<std::string> fenceNames;
	int nFiltered = 0;
	for ( int i = 0; i < nCatalogueRead; ++i )
	{
		const int nType = catalogue[i].game_type;
		if ( nType != 4 && nType != 6 && nType != 9 )
			continue;
		++nFiltered;
		Check( catalogue[i].placeable == 0, NStr::Format( "%s (game type %d) is not placeable", catalogue[i].name, nType ) );
		if ( nType == 4 && szTrench.empty() ) szTrench = catalogue[i].name;
		if ( nType == 6 && szSpan.empty() ) szSpan = catalogue[i].name;
		if ( nType == 9 ) { if ( szFence.empty() ) szFence = catalogue[i].name; fenceNames.insert( catalogue[i].name ); }
	}
	printf( "editor-bridge: %d catalogue entries of game type 4, 6 and 9 are not placeable\n", nFiltered );
	Check( nFiltered > 0 && !szSpan.empty(), "the catalogue has bridge spans to filter" );

	// An add of each is refused naming its tool, and the map does not change.
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const size_t nObjectsBefore = ReadObjectRecords( pSession ).size();
	const struct { const std::string *pName; const char *pszTool; } refusals[] = { { &szSpan, "Bridge tool" }, { &szTrench, "Entrenchment tool" }, { &szFence, "Fence tool" } };
	for ( int i = 0; i < 3; ++i )
	{
		if ( refusals[i].pName->empty() )
			continue;
		int nLinkID = -1;
		Check( BkEditorAddObject( pSession, refusals[i].pName->c_str(), 200.0f, 200.0f, 0, 0, &nLinkID ) == BK_EDITOR_REFUSED,
		       ( "an add of " + *refusals[i].pName + " is refused" ).c_str() );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( refusals[i].pszTool ) != std::string::npos,
		       ( std::string( "and the message names the " ) + refusals[i].pszTool ).c_str() );
	}
	Check( ReadObjectRecords( pSession ).size() == nObjectsBefore, "the refused adds changed nothing in the map" );

	// A loaded map's spans still load: arnheim opens with every span placed.
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( Check( BkEditorOpenMap( pSession, BRIDGE_MAP, &summary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( summary.bridge_span_count > 0 && summary.bridge_span_placed == summary.bridge_span_count,
		       "arnheim still opens with every bridge span placed" );

	// A loaded fence still moves. Found by reading shipped maps (lightly, not
	// through the engine) for an object named like a catalogue fence with a
	// nonzero link ID nothing else shares.
	std::string szFenceMap;
	int nFenceLink = 0;
	{
		int nExamined = 0;
		std::error_code error;
		std::filesystem::recursive_directory_iterator it( "Data/Maps", error ), end;
		for ( ; !error && it != end && nExamined < 60 && nFenceLink == 0; it.increment( error ) )
		{
			if ( it->is_directory() || it->path().extension() != ".bzm" )
				continue;
			++nExamined;
			std::string szCandidate = it->path().string();
			for ( std::string::size_type i = 0; i < szCandidate.size(); ++i )
				if ( szCandidate[i] == '/' ) szCandidate[i] = '\\';
			CMapInfo candidate;
			std::string szCandidateError;
			if ( !NMapFile::Read( szCandidate.c_str(), &candidate, &szCandidateError ) )
				continue;
			std::map<int, int> counts;
			for ( size_t i = 0; i < candidate.objects.size(); ++i ) ++counts[candidate.objects[i].link.nLinkID];
			for ( size_t i = 0; i < candidate.scenarioObjects.size(); ++i ) ++counts[candidate.scenarioObjects[i].link.nLinkID];
			for ( size_t i = 0; i < candidate.objects.size() && nFenceLink == 0; ++i )
			{
				const SMapObjectInfo &rObject = candidate.objects[i];
				if ( rObject.link.nLinkID != 0 && counts[rObject.link.nLinkID] == 1 && fenceNames.count( rObject.szName ) != 0 )
				{
					szFenceMap = szCandidate;
					nFenceLink = rObject.link.nLinkID;
				}
			}
		}
		printf( "editor-bridge: TestM2PaletteFilter looks for a fence in %d shipped maps: %s\n", nExamined, szFenceMap.empty() ? "none found" : szFenceMap.c_str() );
	}
	if ( !Check( nFenceLink != 0, "a shipped map holds a fence to move" ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, szFenceMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorObjectRecord fence;
	memset( &fence, 0, sizeof fence );
	bool bFound = false;
	const std::vector<BkEditorObjectRecord> objects = ReadObjectRecords( pSession );
	for ( size_t i = 0; i < objects.size(); ++i )
		if ( objects[i].link_id == nFenceLink ) { fence = objects[i]; bFound = true; }
	if ( !Check( bFound && fence.known != 0, "the fence reads back as a known object" ) )
		return;
	// A step either way, in case one side is the map's edge or blocked.
	bool bMoved = false;
	const float steps[2] = { 16.0f, -16.0f };
	for ( int i = 0; i < 2 && !bMoved; ++i )
		if ( BkEditorMoveObject( pSession, nFenceLink, fence.x + steps[i], fence.y ) == BK_EDITOR_OK )
		{
			bMoved = true;
			const std::vector<BkEditorObjectRecord> after = ReadObjectRecords( pSession );
			bool bReadBack = false;
			for ( size_t j = 0; j < after.size(); ++j )
				if ( after[j].link_id == nFenceLink ) bReadBack = after[j].x == fence.x + steps[i] && after[j].y == fence.y;
			Check( bReadBack, "the moved fence reads back at its new place" );
			Check( BkEditorMoveObject( pSession, nFenceLink, fence.x, fence.y ) == BK_EDITOR_OK, "and moves back" );
		}
	Check( bMoved, ( "the loaded fence " + std::string( fence.name ) + " still moves: " + BkEditorLastMessage( pSession ) ).c_str() );
	printf( "editor-bridge: M2 palette filter ok\n" );
}

// Phase 4 (D-09): GFXGPU draws a shipped river and a shipped road, measured
// from pixels before the tools that edit them are built. The engine's own
// first river (arnheim) or road (coldwinter) is taken out with
// ITerrainEditor::RemoveRiver/RemoveRoad - the interface the M2 tools will
// use - and the pixels inside the stripe's own screen box must change: a
// stripe the GPU does not draw would leave them as they were.
//
// The box is the bounding box of the stripe's sample points near the middle of
// the screen, with the camera centred on the stripe's middle sample, widened
// by a margin for the stripe's own width. Rivers have an animated layer
// (SLayer::bAnimated, effect 303): two captures a second apart at the same
// camera are compared inside the same box, and a difference reads as "animated
// layer observed". Assumption A7: a static engine loop may not advance it; a
// zero there is printed, not failed.
static ITerrainEditor* EngineTerrainEditor()
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene != 0 ? pScene->GetTerrain() : 0;
	return pTerrain != 0 ? pTerrain->GetEditor() : 0;
}

static const float VSO_BOX_RADIUS = 140.0f;     // screen pixels round the middle sample that the box may reach
static const int VSO_BOX_MARGIN = 24;           // widening for the stripe's own width
static const float VSO_MIN_CHANGED_FRACTION = 0.02f;

static void MeasureVsoOnGpu( BkEditorSession *pSession, const char *pszMap, bool bRiver, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	const char *pszKind = bRiver ? "river" : "road";
	if ( !Check( BkEditorOpenMap( pSession, pszMap, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	ITerrainEditor *pEditor = EngineTerrainEditor();
	if ( !Check( pEditor != 0, "the engine has a terrain editor for the open map" ) )
		return;
	const TVSOList &rList = bRiver ? pEditor->GetTerrainInfo().rivers : pEditor->GetTerrainInfo().roads3;
	if ( !Check( !rList.empty(), NStr::Format( "%s has a %s in the engine's terrain", pszMap, pszKind ) ) )
		return;
	// Copied: removing it below erases the engine's own entry.
	const int nID = rList[0].nID;
	const std::vector<SVectorStripeObjectPoint> points = rList[0].points;
	if ( !Check( points.size() >= 2, NStr::Format( "the engine's first %s has sampled points (%d)", pszKind, int( points.size() ) ) ) )
		return;
	const SVectorStripeObjectPoint &rMiddle = points[points.size() / 2];
	BkEditorSetCamera( pSession, rMiddle.vPos.x, rMiddle.vPos.y );
	for ( int i = 0; i < 3; ++i )
		BkEditorFrame( pSession );

	float fCentreX = 0.0f, fCentreY = 0.0f;
	if ( !Check( BkEditorWorldToScreen( pSession, rMiddle.vPos.x, rMiddle.vPos.y, &fCentreX, &fCentreY ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nLeft = nScreenWidth, nTop = nScreenHeight, nRight = 0, nBottom = 0;
	int nNear = 0;
	for ( size_t i = 0; i < points.size(); ++i )
	{
		float sx = 0.0f, sy = 0.0f;
		if ( BkEditorWorldToScreen( pSession, points[i].vPos.x, points[i].vPos.y, &sx, &sy ) != BK_EDITOR_OK )
			continue;
		if ( hypotf( sx - fCentreX, sy - fCentreY ) > VSO_BOX_RADIUS )
			continue;
		++nNear;
		nLeft = Min( nLeft, int( sx ) );
		nRight = Max( nRight, int( sx ) );
		nTop = Min( nTop, int( sy ) );
		nBottom = Max( nBottom, int( sy ) );
	}
	if ( !Check( nNear > 0, NStr::Format( "some of the %s's points are within %.0f px of its middle sample", pszKind, VSO_BOX_RADIUS ) ) )
		return;
	nLeft = Max( 0, nLeft - VSO_BOX_MARGIN );
	nTop = Max( 0, nTop - VSO_BOX_MARGIN );
	nRight = Min( nScreenWidth, nRight + VSO_BOX_MARGIN );
	nBottom = Min( nScreenHeight, nBottom + VSO_BOX_MARGIN );
	const int nBoxArea = Max( 1, ( nRight - nLeft ) * ( nBottom - nTop ) );

	const std::string szA = szScratch + NStr::Format( "/04-04-%s-a.tga", pszKind );
	const std::string szA2 = szScratch + NStr::Format( "/04-04-%s-a2.tga", pszKind );
	const std::string szB = szScratch + NStr::Format( "/04-04-%s-b.tga", pszKind );
	if ( !SaveFrame( pSession, szA ) )
		return;
	// Rivers only: a second later at the same camera, for the animated layer.
	if ( bRiver )
	{
		for ( int i = 0; i < 60; ++i )
		{
			BkEditorFrame( pSession );
			SDL_Delay( 16 );
		}
		if ( !SaveFrame( pSession, szA2 ) )
			return;
	}
	const bool bRemoved = bRiver ? pEditor->RemoveRiver( nID ) : pEditor->RemoveRoad( nID );
	if ( !Check( bRemoved, NStr::Format( "the engine removes its first %s (id %d)", pszKind, nID ) ) )
		return;
	for ( int i = 0; i < 3; ++i )
		BkEditorFrame( pSession );
	const bool bSavedB = SaveFrame( pSession, szB );

	int nWidth = 0, nHeight = 0;
	const std::vector<unsigned char> before = ReadFramePixels( szA, &nWidth, &nHeight );
	const std::vector<unsigned char> after = ReadFramePixels( szB, &nWidth, &nHeight );
	const int nChanged = bSavedB ? ChangedPixels( before, after, nWidth, nHeight, nLeft, nTop, nRight, nBottom ) : -1;
	if ( bRiver )
	{
		const std::vector<unsigned char> later = ReadFramePixels( szA2, &nWidth, &nHeight );
		const int nAnimated = ChangedPixels( before, later, nWidth, nHeight, nLeft, nTop, nRight, nBottom );
		printf( "editor-bridge: M2 river drawn (%d px of %d) animated=%d%s\n", nChanged, nBoxArea, nAnimated,
		        nAnimated > 0 ? " (animated layer observed)" : " (animation unobserved (A7))" );
	}
	else
		printf( "editor-bridge: M2 road drawn (%d px of %d)\n", nChanged, nBoxArea );
	printf( "editor-bridge: M2 %s box %d,%d..%d,%d at screen %dx%d, %d of %d points near the middle; captures %s, %s\n", pszKind,
	        nLeft, nTop, nRight, nBottom, nScreenWidth, nScreenHeight, nNear, int( points.size() ), szA.c_str(), szB.c_str() );
	Check( nChanged > int( nBoxArea * VSO_MIN_CHANGED_FRACTION ),
	       NStr::Format( "GFXGPU draws the %s: taking it out changes %d of the %d pixels in its box (bar %.0f%%)", pszKind, nChanged, nBoxArea, VSO_MIN_CHANGED_FRACTION * 100.0f ) );

	// The engine is whole again: the map is reopened, and the bridge's copy of
	// the terrain still equals it.
	Check( BkEditorOpenMap( pSession, pszMap, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK, "the reopened map's terrain matches the engine's again" );
}

static void TestM2VsoRendersOnGpu( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	MeasureVsoOnGpu( pSession, BRIDGE_MAP, true, nScreenWidth, nScreenHeight, szScratch );
	MeasureVsoOnGpu( pSession, SHIPPED_MAP, false, nScreenWidth, nScreenHeight, szScratch );
}

static bool SameAnchors( const BkEditorCameraAnchorRecord &rLeft, const BkEditorCameraAnchorRecord &rRight )
{
	return memcmp( &rLeft, &rRight, sizeof rLeft ) == 0;
}

// D-22's data path, on the real engine: the camera anchors read as the file
// has them, a player's anchor set through the bridge pads the vector and saves
// as the NMapRecords-built expected map, the exact put of the old value brings
// back the unedited file byte for byte, and every refusal leaves the read
// unchanged. Also BkEditorGroundHeight, which the editor takes an anchor's z
// from.
// WR-B03 (04 review): a legacy map whose anchor lies off the map can have it
// edited onto the map, and the undo - the file's own value put back - goes
// through; a NEW off-map value is still refused.
static void TestM2OffMapAnchorUndo( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	NMapRecords::SCameraAnchors odd;
	odd.vNeutral = CVec3( 1.0e5f, -40.0f, 0.0f );
	odd.players.push_back( CVec3( -500.0f, 99999.0f, 0.0f ) );
	NMapRecords::PutCameraAnchors( &map, odd );
	const std::string szMap = szScratch + "\\anchors-offmap.bzm";
	const std::string szUnedited = szScratch + "\\anchors-offmap-unedited.bzm";
	const std::string szAfter = szScratch + "\\anchors-offmap-after.bzm";
	if ( !Check( NMapFile::Write( szMap.c_str(), map, &szError ), szError.c_str() ) ||
	     !Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) ||
	     !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorCameraAnchorRecord file;
	memset( &file, 0, sizeof file );
	if ( !Check( BkEditorCameraAnchors( pSession, &file ) == BK_EDITOR_OK && file.player_count == 1, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorCameraAnchorRecord onMap = file;
	const float fMiddleX = map.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
	const float fMiddleY = map.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
	onMap.neutral.x = fMiddleX; onMap.neutral.y = fMiddleY;
	onMap.players[0].x = fMiddleX; onMap.players[0].y = fMiddleY;
	Check( BkEditorSetCameraAnchors( pSession, &onMap ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSetCameraAnchors( pSession, &file ) == BK_EDITOR_OK,
	       NStr::Format( "the file's own off-map anchors go back, as an undo needs: %s", BkEditorLastMessage( pSession ) ) );
	BkEditorCameraAnchorRecord fresh = file;
	fresh.players[0].x = -123.0f;
	Check( BkEditorSetCameraAnchors( pSession, &fresh ) == BK_EDITOR_REFUSED, "a NEW off-map anchor is still refused" );
	if ( Check( BkEditorSaveMap( pSession, szAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szAfter ), "the off-map anchors put back save the unedited file byte for byte" );
	remove( OsPath( szMap ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szAfter ).c_str() );
	printf( "editor-bridge: M2 off-map camera anchor undo ok\n" );
}

static void TestM2CameraAnchors( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The read equals the file.
	BkEditorCameraAnchorRecord before;
	memset( &before, 0, sizeof before );
	if ( !Check( BkEditorCameraAnchors( pSession, &before ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( before.player_count == int( original.playersCameraAnchors.size() ), "the anchors read with the file's own vector size" );
	Check( before.neutral.x == original.vCameraAnchor.x && before.neutral.y == original.vCameraAnchor.y && before.neutral.z == original.vCameraAnchor.z,
	       "the neutral anchor reads as the file has it" );
	for ( int i = 0; i < before.player_count && i < int( original.playersCameraAnchors.size() ); ++i )
		Check( before.players[i].x == original.playersCameraAnchors[i].x && before.players[i].y == original.playersCameraAnchors[i].y &&
		       before.players[i].z == original.playersCameraAnchors[i].z, NStr::Format( "player %d's anchor reads as the file has it", i ) );
	Check( BkEditorCameraAnchors( pSession, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null out is a bad argument" );

	// An unedited save first, for the byte comparison at the end.
	const std::string szUnedited = szScratch + "\\anchors-unedited.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The ground height: the map's middle answers, a point off the map is
	// refused and leaves *z as the caller had it (zeroed by the entry point).
	const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
	const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
	float fZ = -1.0f;
	if ( !Check( BkEditorGroundHeight( pSession, fMiddleX, fMiddleY, &fZ ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( std::isfinite( fZ ), "the ground height at the map's middle is a number" );
	float fOffZ = 7.0f;
	Check( BkEditorGroundHeight( pSession, -500.0f, -500.0f, &fOffZ ) == BK_EDITOR_REFUSED, "a ground height off the map is refused" );
	Check( fOffZ == 0.0f, "and leaves z zeroed" );
	Check( BkEditorGroundHeight( pSession, fMiddleX, fMiddleY, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null z is a bad argument" );

	// Set player 2 through the padded value.
	BkEditorCameraAnchorRecord padded = before;
	padded.player_count = before.player_count > 3 ? before.player_count : 3;
	padded.players[2].x = fMiddleX;
	padded.players[2].y = fMiddleY;
	padded.players[2].z = fZ;
	if ( !Check( BkEditorSetCameraAnchors( pSession, &padded ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorCameraAnchorRecord after;
	memset( &after, 0, sizeof after );
	Check( BkEditorCameraAnchors( pSession, &after ) == BK_EDITOR_OK && SameAnchors( after, padded ), "the set anchors read back as they were put" );

	// Saved, it is the map the same NMapRecords calls build.
	CMapInfo expected = original;
	NMapRecords::SCameraAnchors anchors;
	NMapRecords::GetCameraAnchors( expected, &anchors );
	NMapRecords::SetPlayerCameraAnchor( &anchors, 2, CVec3( fMiddleX, fMiddleY, fZ ) );
	NMapRecords::PutCameraAnchors( &expected, anchors );
	const std::string szEdited = szScratch + "\\anchors-edited.bzm";
	if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		CMapInfo saved;
		if ( Check( NMapFile::Read( szEdited.c_str(), &saved, &szError ), szError.c_str() ) )
		{
			std::string szWhere;
			Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
			       szWhere.empty() ? "the saved map equals the expected map" : ( "the anchor save differs at " + szWhere ).c_str() );
			Check( saved.playersCameraAnchors.size() == size_t( padded.player_count ), "the saved vector is padded and never shrunk" );
		}
	}

	// Every refusal leaves the read unchanged.
	BkEditorCameraAnchorRecord bad = padded;
	bad.player_count = 33;
	Check( BkEditorSetCameraAnchors( pSession, &bad ) == BK_EDITOR_BAD_ARGUMENT, "33 players is a bad argument" );
	bad = padded;
	bad.players[0].x = std::numeric_limits<float>::quiet_NaN();
	Check( BkEditorSetCameraAnchors( pSession, &bad ) == BK_EDITOR_BAD_ARGUMENT, "a NaN anchor is a bad argument" );
	bad = padded;
	bad.players[1].x = -5000.0f;
	bad.players[1].y = -5000.0f;
	Check( BkEditorSetCameraAnchors( pSession, &bad ) == BK_EDITOR_REFUSED, "an anchor off the map is refused" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "player 1" ) != std::string::npos, "and names the slot" );
	bad = padded;
	bad.neutral.x = 1.0e9f;
	Check( BkEditorSetCameraAnchors( pSession, &bad ) == BK_EDITOR_REFUSED, "a neutral anchor off the map is refused" );
	Check( BkEditorSetCameraAnchors( pSession, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null record is a bad argument" );
	BkEditorCameraAnchorRecord unchanged;
	memset( &unchanged, 0, sizeof unchanged );
	Check( BkEditorCameraAnchors( pSession, &unchanged ) == BK_EDITOR_OK && SameAnchors( unchanged, padded ), "none of the refusals changed the anchors" );

	// The exact put of the before value brings back the unedited file.
	if ( !Check( BkEditorSetCameraAnchors( pSession, &before ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorCameraAnchorRecord restored;
	memset( &restored, 0, sizeof restored );
	Check( BkEditorCameraAnchors( pSession, &restored ) == BK_EDITOR_OK && SameAnchors( restored, before ), "the old value puts back exactly, size included" );
	const std::string szUndone = szScratch + "\\anchors-undone.bzm";
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "an anchor edit and its inverse save the unedited file byte for byte" );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	printf( "editor-bridge: M2 camera anchors ok\n" );
}

// D-15 / C7: an object's script ID, on the real engine. The first placed
// object the session may edit takes 4242: BkEditorObjects reports it, the save
// equals the map the same NMapRecords call builds, and a reopened save gives
// the value to the engine's own IAIEditor::GetObjectScriptID (the AI takes a
// script ID when an object is added, so this is the only way to read it, C7).
// Setting it back and saving gives the unedited file byte for byte; a value
// outside -1..32000, link ID 0 and an unknown link ID are refused and change
// nothing.
static void TestM2ScriptIDs( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\scriptid-unedited.bzm";
	const std::string szEdited = szScratch + "\\scriptid-edited.bzm";
	const std::string szUndone = szScratch + "\\scriptid-undone.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	printf( "editor-bridge: coldwinter holds %d reinforcement groups; the first free group ID from 0 is %d, from 900 is %d\n",
	        int( original.reinforcements.groups.size() ), NMapRecords::FirstFreeGroupID( original, 0 ), NMapRecords::FirstFreeGroupID( original, 900 ) );

	// The first object of the map's objects list that the session edits: a link
	// ID of its own, the engine holds it.
	const std::vector<BkEditorObjectRecord> before = ReadObjectRecords( pSession );
	int nTarget = -1, nOriginal = -1;
	for ( size_t i = 0; i < before.size() && nTarget < 0; ++i )
	{
		const BkEditorObjectRecord &rRecord = before[i];
		if ( rRecord.scenario != 0 || rRecord.link_id == 0 || rRecord.known == 0 )
			continue;
		int nSharing = 0;
		for ( size_t j = 0; j < before.size(); ++j )
			nSharing += before[j].link_id == rRecord.link_id ? 1 : 0;
		BkEditorObjectState engineState;
		if ( nSharing != 1 || BkEditorEngineObjectState( pSession, rRecord.link_id, &engineState ) != BK_EDITOR_OK )
			continue;
		nTarget = rRecord.link_id;
		nOriginal = rRecord.script_id;
	}
	if ( !Check( nTarget >= 0, "coldwinter has an object the session edits" ) )
		return;
	for ( size_t i = 0; i < before.size(); ++i )
		if ( before[i].link_id == nTarget )
			Check( before[i].script_id == original.objects[i].nScriptID, "the records read the file's script IDs" );

	// The set reads back and changes nothing else.
	if ( !Check( BkEditorSetObjectScriptID( pSession, nTarget, 4242 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::vector<BkEditorObjectRecord> after = ReadObjectRecords( pSession );
	bool bReadBack = after.size() == before.size();
	for ( size_t i = 0; i < after.size() && bReadBack; ++i )
		bReadBack = after[i].link_id == before[i].link_id &&
		            after[i].script_id == ( before[i].link_id == nTarget ? 4242 : before[i].script_id );
	Check( bReadBack, "BkEditorObjects reports the new script ID on that object and no other change" );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "after the script ID set: " ) + BkEditorLastMessage( pSession ) ).c_str() );

	// Saved, it is the map the same NMapRecords call builds.
	CMapInfo expected = original;
	Check( NMapRecords::SetObjectScriptID( &expected, nTarget, 4242 ), "the expected map takes the script ID" );
	if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		CMapInfo saved;
		if ( Check( NMapFile::Read( szEdited.c_str(), &saved, &szError ), szError.c_str() ) )
		{
			std::string szWhere;
			Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
			       szWhere.empty() ? "the saved map equals the expected map" : ( "the script ID save differs at " + szWhere ).c_str() );
		}
		// The engine's own reading: reopen the saved file and ask the AI.
		if ( Check( BkEditorOpenMap( pSession, szEdited.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
			IRefCount *pObject = pAIEditor != 0 ? pAIEditor->ObjectByLink( nTarget ) : 0;
			if ( Check( pObject != 0, "the reopened map has the object under its link ID" ) )
			{
				const int nEngineScriptID = pAIEditor->GetObjectScriptID( pObject );
				printf( "editor-bridge: the reopened object %d reports script ID %d to the AI (file said %d before)\n", nTarget, nEngineScriptID, nOriginal );
				Check( nEngineScriptID == 4242, "IAIEditor::GetObjectScriptID on the reopened object equals the script ID set" );
			}
		}
	}

	// Back to the shipped map: set, set back, and the bytes are the unedited ones.
	// The unedited bytes are this open's own: SVertexAltitude's padding goes to
	// the file raw and the session's snapshot is a copy, so two opens of one map
	// may save different padding (04-01, 04-09) - a save is only compared with
	// a save of the same open.
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( BkEditorSetObjectScriptID( pSession, nTarget, 4242 ) == BK_EDITOR_OK && BkEditorSetObjectScriptID( pSession, nTarget, nOriginal ) == BK_EDITOR_OK,
	       "the script ID goes to 4242 and back to what the file held" );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "a script ID edit and its inverse save the unedited file byte for byte" );

	// The refusals change nothing.
	Check( BkEditorSetObjectScriptID( pSession, nTarget, 32001 ) == BK_EDITOR_REFUSED, "32001 is refused" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "32000" ) != std::string::npos, "and the refusal names the range" );
	Check( BkEditorSetObjectScriptID( pSession, nTarget, -2 ) == BK_EDITOR_REFUSED, "-2 is refused" );
	Check( BkEditorSetObjectScriptID( pSession, 0, 5 ) == BK_EDITOR_REFUSED, "link ID 0 is refused" );
	Check( BkEditorSetObjectScriptID( pSession, 987654, 5 ) == BK_EDITOR_REFUSED, "an unknown link ID is refused" );
	Check( BkEditorSetObjectScriptID( pSession, nTarget, -1 ) == BK_EDITOR_OK && BkEditorSetObjectScriptID( pSession, nTarget, nOriginal ) == BK_EDITOR_OK,
	       "-1 (none) and the old value are both accepted" );
	const std::string szRefused = szScratch + "\\scriptid-refused.bzm";
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szRefused ), "the refusals changed nothing: the map saves unedited byte for byte" );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szRefused ).c_str() );
	printf( "editor-bridge: M2 script ids ok\n" );
}

// The sorted shipped map paths (engine form), the same on every platform.
static std::vector<std::string> SortedShippedMaps()
{
	std::vector<std::string> paths;
	std::error_code error;
	for ( std::filesystem::recursive_directory_iterator it( "Data/Maps", error ), itEnd; !error && it != itEnd; it.increment( error ) )
		if ( it->is_regular_file( error ) )
		{
			std::string szPath = it->path().generic_string();
			if ( szPath.size() > 4 && NStr::CompareAsciiNoCase( szPath.c_str() + szPath.size() - 4, ".bzm" ) == 0 )
			{
				std::replace( szPath.begin(), szPath.end(), '/', '\\' );
				paths.push_back( szPath );
			}
		}
	std::sort( paths.begin(), paths.end() );
	return paths;
}

// The group IDs of a map, ascending.
static std::vector<int> SortedGroupIDs( const CMapInfo &rMap )
{
	std::vector<int> ids;
	for ( std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator it = rMap.reinforcements.groups.begin();
	      it != rMap.reinforcements.groups.end(); ++it )
		ids.push_back( it->first );
	std::sort( ids.begin(), ids.end() );
	return ids;
}

// The bridge's view of one group: false when it is not there.
static bool ReadGroupOf( BkEditorSession *pSession, int nID, std::vector<int> *pIDs )
{
	int nCount = -2;
	BkEditorGroup( pSession, nID, 0, 0, &nCount );
	if ( nCount < 0 )
		return false;
	pIDs->assign( nCount > 0 ? nCount : 1, 0 );
	int nRead = -2;
	if ( BkEditorGroup( pSession, nID, &( *pIDs )[0], nCount, &nRead ) != BK_EDITOR_OK || nRead != nCount )
		return false;
	pIDs->resize( nCount );
	return true;
}

// D-16 on the real engine: the reinforcement groups of the first shipped map
// (in sorted path order) that has at least two, read as the file has them; a
// new group takes the first free ID from 0, gets two script IDs, loses one,
// another group gains one and a third is deleted; the save equals the map the
// same NMapRecords calls build. Put back the other way round, the edits save
// the unedited file byte for byte, and the same edits in another order save
// the same bytes as the first time (the groups go out in ID order). Refusals (-1, 32001, a duplicate, an unknown group, a buffer too
// short) change nothing, and an ID a group already holds is exempt from the
// rules so an undo can put a file's own odd data back.
static void TestM2Groups( BkEditorSession *pSession, const std::string &szScratch )
{
	std::string szMap = SHIPPED_MAP;
	int nScanned = 0;
	{
		const std::vector<std::string> paths = SortedShippedMaps();
		for ( size_t p = 0; p < paths.size(); ++p )
		{
			CMapInfo probe;
			std::string szProbeError;
			++nScanned;
			if ( NMapFile::Read( paths[p].c_str(), &probe, &szProbeError ) && probe.reinforcements.groups.size() >= 2 )
			{
				szMap = paths[p];
				break;
			}
		}
	}
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( szMap.c_str(), &original, &szError ), szError.c_str() ) )
		return;
	std::vector<int> existing = SortedGroupIDs( original );
	printf( "editor-bridge: groups on %s (%d maps scanned): %d groups\n", szMap.c_str(), nScanned, int( existing.size() ) );
	if ( !Check( existing.size() >= 2, "a shipped map with two reinforcement groups was found" ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\groups-unedited.bzm";
	const std::string szEdited = szScratch + "\\groups-edited.bzm";
	const std::string szUndone = szScratch + "\\groups-undone.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The reads equal the file: the IDs ascending, each group's script IDs in file order.
	int nTotal = -1;
	Check( BkEditorGroupIDs( pSession, 0, 0, &nTotal ) == ( existing.empty() ? BK_EDITOR_OK : BK_EDITOR_REFUSED ) && nTotal == int( existing.size() ),
	       "a sizing call answers the total (REFUSED for a zero capacity when there are groups)" );
	std::vector<int> listed( existing.size() + 1, -77 );
	int nListed = 0;
	Check( BkEditorGroupIDs( pSession, &listed[0], int( existing.size() ), &nListed ) == BK_EDITOR_OK && nListed == int( existing.size() ), "the group IDs list" );
	Check( std::equal( existing.begin(), existing.end(), listed.begin() ), "the group IDs read ascending, as the file has them" );
	Check( listed[existing.size()] == -77, "and nothing was written past the capacity" );
	for ( size_t i = 0; i < existing.size(); ++i )
	{
		std::vector<int> ids;
		Check( ReadGroupOf( pSession, existing[i], &ids ) && ids == original.reinforcements.groups.find( existing[i] )->second.ids,
		       NStr::Format( "group %d reads with the file's script IDs in the file's order", existing[i] ) );
	}

	// A buffer too short answers the total and writes nothing past it.
	{
		std::vector<int> ids;
		int nBigGroup = -1;
		for ( size_t i = 0; i < existing.size() && nBigGroup < 0; ++i )
			if ( original.reinforcements.groups.find( existing[i] )->second.ids.size() >= 2 )
				nBigGroup = existing[i];
		if ( nBigGroup >= 0 )
		{
			const int nHeld = int( original.reinforcements.groups.find( nBigGroup )->second.ids.size() );
			int canary[4] = { -77, -77, -77, -77 };
			int nCount = -1;
			Check( BkEditorGroup( pSession, nBigGroup, canary, 1, &nCount ) == BK_EDITOR_REFUSED && nCount == nHeld, "a group read with a buffer too short is REFUSED with the total" );
			Check( canary[1] == -77 && canary[2] == -77 && canary[3] == -77, "and wrote nothing past the capacity" );
		}
		int nCount = -1;
		Check( BkEditorGroup( pSession, 999999, 0, 0, &nCount ) == BK_EDITOR_REFUSED && nCount == -1, "an unknown group reads REFUSED with a count of -1" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "no reinforcement group" ) != std::string::npos, "and says there is no such group" );
		Check( BkEditorGroup( pSession, -1, 0, 0, &nCount ) == BK_EDITOR_BAD_ARGUMENT, "a negative group ID is a bad argument" );
		Check( BkEditorGroup( pSession, existing[0], 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null count is a bad argument" );
		Check( BkEditorGroupIDs( pSession, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "and so is a null count for the IDs" );
	}

	// The edits: New from 0, two script IDs in, one out; another group gains one; a third is deleted.
	int nFree = -1;
	Check( BkEditorFirstFreeGroupID( pSession, 0, &nFree ) == BK_EDITOR_OK && nFree == NMapRecords::FirstFreeGroupID( original, 0 ), "the first free group ID from 0 is the overlay's" );
	Check( BkEditorFirstFreeGroupID( pSession, -9, &nFree ) == BK_EDITOR_OK && nFree == NMapRecords::FirstFreeGroupID( original, 0 ), "a negative start counts as 0" );
	Check( BkEditorFirstFreeGroupID( pSession, existing[0], &nFree ) == BK_EDITOR_OK && nFree > existing[0] && original.reinforcements.groups.find( nFree ) == original.reinforcements.groups.end(),
	       "starting at a taken ID gives a higher free one (C9)" );
	Check( BkEditorFirstFreeGroupID( pSession, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null out is a bad argument" );
	const int nNew = NMapRecords::FirstFreeGroupID( original, 0 );
	int nFreeFrom0 = -1;
	BkEditorFirstFreeGroupID( pSession, 0, &nFreeFrom0 );
	if ( !Check( nFreeFrom0 == nNew, "the new group takes that ID" ) )
		return;
	const int nEdit = existing[0], nDelete = existing[1];
	const std::vector<int> editOriginal = original.reinforcements.groups.find( nEdit )->second.ids;
	const std::vector<int> deleteOriginal = original.reinforcements.groups.find( nDelete )->second.ids;
	const int nFirstAdded = 5000, nSecondAdded = 5001, nEditAdded = 5002;
	Check( original.reinforcements.GetGroupById( nFirstAdded ) == -1 && original.reinforcements.GetGroupById( nSecondAdded ) == -1 && original.reinforcements.GetGroupById( nEditAdded ) == -1,
	       "the script IDs the test adds are held by no group" );
	{
		const int none = 0;
		Check( BkEditorSetGroup( pSession, nNew, &none, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		std::vector<int> ids;
		Check( ReadGroupOf( pSession, nNew, &ids ) && ids.empty(), "a new group holds no script IDs" );
		const int two[2] = { nFirstAdded, nSecondAdded };
		Check( BkEditorSetGroup( pSession, nNew, two, 2 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( ReadGroupOf( pSession, nNew, &ids ) && ids.size() == 2 && ids[0] == nFirstAdded && ids[1] == nSecondAdded, "two script IDs in, in the order given" );
		const int one[1] = { nFirstAdded };
		Check( BkEditorSetGroup( pSession, nNew, one, 1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( ReadGroupOf( pSession, nNew, &ids ) && ids.size() == 1 && ids[0] == nFirstAdded, "one taken out" );
		std::vector<int> grown = editOriginal;
		grown.push_back( nEditAdded );
		Check( BkEditorSetGroup( pSession, nEdit, &grown[0], int( grown.size() ) ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( ReadGroupOf( pSession, nEdit, &ids ) && ids == grown, "an existing group gains a script ID at the end" );
		Check( BkEditorDeleteGroup( pSession, nDelete ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( !ReadGroupOf( pSession, nDelete, &ids ), "a deleted group is gone" );
	}

	// Saved, it is the map the same NMapRecords calls build.
	CMapInfo expected;
	if ( !Check( NMapFile::Read( szMap.c_str(), &expected, &szError ), szError.c_str() ) )
		return;
	{
		Check( NMapRecords::EraseReinforcementGroup( &expected, nDelete ), "the expected map loses the deleted group" );
		std::vector<int> grown = editOriginal;
		grown.push_back( nEditAdded );
		Check( NMapRecords::PutReinforcementGroup( &expected, nEdit, grown ), "and the edited group gains its script ID" );
		Check( NMapRecords::PutReinforcementGroup( &expected, nNew, std::vector<int>( 1, nFirstAdded ) ), "and holds the new group" );
	}
	if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		CMapInfo saved;
		if ( Check( NMapFile::Read( szEdited.c_str(), &saved, &szError ), szError.c_str() ) )
		{
			std::string szWhere;
			Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
			       szWhere.empty() ? "the saved map equals the expected map" : ( "the group save differs at " + szWhere ).c_str() );
			Check( saved.reinforcements.groups.size() == original.reinforcements.groups.size(), "one group in (new), one out (deleted)" );
		}
	}

	// The other way round: the edits put back exactly bring back the unedited file.
	const int nNoIDs = 0;
	const int *pEditOriginal = editOriginal.empty() ? &nNoIDs : &editOriginal[0];
	const int *pDeleteOriginal = deleteOriginal.empty() ? &nNoIDs : &deleteOriginal[0];
	Check( BkEditorDeleteGroup( pSession, nNew ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSetGroup( pSession, nEdit, pEditOriginal, int( editOriginal.size() ) ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSetGroup( pSession, nDelete, pDeleteOriginal, int( deleteOriginal.size() ) ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "the group edits and their inverses save the unedited file byte for byte" );

	// The same three edits in the reverse order save the same bytes: the file
	// lists the groups in ID order, whatever order the hash table was filled in.
	// (Compared with the first save, never with a map written from another read:
	// SVertexAltitude's padding differs between a copy and a read, 04-01.)
	{
		Check( BkEditorDeleteGroup( pSession, nDelete ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		std::vector<int> grown = editOriginal;
		grown.push_back( nEditAdded );
		Check( BkEditorSetGroup( pSession, nEdit, &grown[0], int( grown.size() ) ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BkEditorSetGroup( pSession, nNew, &nFirstAdded, 1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		const std::string szEditedAgain = szScratch + "\\groups-edited-again.bzm";
		if ( Check( BkEditorSaveMap( pSession, szEditedAgain.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szEdited, szEditedAgain ), "the same edits in another order save the same bytes: groups go out in ID order" );
		remove( OsPath( szEditedAgain ).c_str() );
		Check( BkEditorDeleteGroup( pSession, nNew ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BkEditorSetGroup( pSession, nEdit, pEditOriginal, int( editOriginal.size() ) ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BkEditorSetGroup( pSession, nDelete, pDeleteOriginal, int( deleteOriginal.size() ) ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	}

	// The refusals change nothing.
	{
		const int minusOne[1] = { -1 }, tooBig[1] = { 32001 }, twice[2] = { 6000, 6000 }, edge[2] = { 0, 32000 };
		Check( BkEditorSetGroup( pSession, nNew, minusOne, 1 ) == BK_EDITOR_REFUSED, "-1 is refused: it would match every object without a script ID" );
		Check( BkEditorSetGroup( pSession, nNew, tooBig, 1 ) == BK_EDITOR_REFUSED, "32001 is refused" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "32000" ) != std::string::npos, "and the refusal names the range" );
		Check( BkEditorSetGroup( pSession, nNew, twice, 2 ) == BK_EDITOR_REFUSED, "a duplicate in the list is refused" );
		Check( BkEditorSetGroup( pSession, -1, edge, 2 ) == BK_EDITOR_BAD_ARGUMENT, "a negative group ID is a bad argument" );
		Check( BkEditorSetGroup( pSession, nNew, edge, -1 ) == BK_EDITOR_BAD_ARGUMENT, "a negative count is a bad argument" );
		Check( BkEditorSetGroup( pSession, nNew, 0, 1 ) == BK_EDITOR_BAD_ARGUMENT, "a null list with a count is a bad argument" );
		Check( BkEditorSetGroup( pSession, nNew, 0, 1 << 20 ) == BK_EDITOR_BAD_ARGUMENT, "a count no group could hold is a bad argument" );
		Check( BkEditorDeleteGroup( pSession, 999999 ) == BK_EDITOR_REFUSED, "deleting an unknown group is refused" );
		Check( BkEditorDeleteGroup( pSession, -3 ) == BK_EDITOR_BAD_ARGUMENT, "and a negative ID is a bad argument" );
		std::vector<int> ids;
		Check( !ReadGroupOf( pSession, nNew, &ids ), "none of the refusals made the group" );
		// The limits are fine.
		Check( BkEditorSetGroup( pSession, nNew, edge, 2 ) == BK_EDITOR_OK && BkEditorDeleteGroup( pSession, nNew ) == BK_EDITOR_OK, "0 and 32000 are accepted" );
		const std::string szRefused = szScratch + "\\groups-refused.bzm";
		if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szUnedited, szRefused ), "the refusals changed nothing: the map saves unedited byte for byte" );
		remove( OsPath( szRefused ).c_str() );
	}

	// A file's own odd data: a group holding -1, a duplicate and a value past the
	// range opens, reads, lets a script ID go, and takes the odd group back
	// unchanged (an undo) - but adds no new odd value.
	{
		const std::string szOdd = szScratch + "\\groups-odd.bzm";
		CMapInfo odd;
		if ( Check( NMapFile::Read( szMap.c_str(), &odd, &szError ), szError.c_str() ) )
		{
			const int oddID = NMapRecords::FirstFreeGroupID( odd, 0 );
			const int oddIDs[4] = { 7, -1, 7, 40000 };
			NMapRecords::PutReinforcementGroup( &odd, oddID, std::vector<int>( oddIDs, oddIDs + 4 ) );
			if ( Check( NMapFile::Write( szOdd.c_str(), odd, &szError ), szError.c_str() ) &&
			     Check( BkEditorOpenMap( pSession, szOdd.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			{
				std::vector<int> ids;
				Check( ReadGroupOf( pSession, oddID, &ids ) && ids == std::vector<int>( oddIDs, oddIDs + 4 ), "an odd group reads as the file has it" );
				const int fewer[3] = { -1, 7, 40000 };
				Check( BkEditorSetGroup( pSession, oddID, fewer, 3 ) == BK_EDITOR_OK, "a script ID can be taken out of an odd group" );
				Check( BkEditorSetGroup( pSession, oddID, oddIDs, 4 ) == BK_EDITOR_OK, "and the odd group goes back exactly, as an undo needs" );
				const int worse[5] = { 7, -1, 7, 40000, -1 };
				Check( BkEditorSetGroup( pSession, oddID, worse, 5 ) == BK_EDITOR_REFUSED, "but a second -1 is a new odd value and is refused" );
				const int another[5] = { 7, -1, 7, 40000, 41000 };
				Check( BkEditorSetGroup( pSession, oddID, another, 5 ) == BK_EDITOR_REFUSED, "and so is a new value out of range" );
				Check( ReadGroupOf( pSession, oddID, &ids ) && ids == std::vector<int>( oddIDs, oddIDs + 4 ), "the refusals left the odd group as it was" );
			}
		}
		remove( OsPath( szOdd ).c_str() );
	}
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	printf( "editor-bridge: M2 groups ok\n" );
}

// The script file of the bridge's map, or false with the status the read gave.
static bool ReadScriptFileOf( BkEditorSession *pSession, std::string *pszName, BkEditorStatus *pStatus = 0 )
{
	BkEditorScriptFileRecord record;
	memset( &record, 0x7f, sizeof record );
	const BkEditorStatus status = BkEditorScriptFile( pSession, &record );
	if ( pStatus != 0 )
		*pStatus = status;
	if ( status != BK_EDITOR_OK )
		return false;
	*pszName = record.name;
	return true;
}

static BkEditorStatus PutScriptFileOf( BkEditorSession *pSession, const char *pszName )
{
	BkEditorScriptFileRecord record;
	memset( &record, 0, sizeof record );
	strncpy( record.name, pszName, sizeof record.name - 1 );
	return BkEditorSetScriptFile( pSession, &record );
}

// D-20 on the real engine: the script file of the first shipped map (in sorted
// path order) that names one reads as the file has it, verbatim (it may hold a
// folder); "m2_script" is set, saves as the map the same NMapRecords call
// builds and reads back; the file's own value - accepted although it is not a
// bare name - puts back, and the bytes are the unedited save's. A scratch map
// naming an odd script ("..\\odd name.lua") reads verbatim, takes a bare name
// and puts the odd one back, but a different odd name is a new value and is
// refused. "..\\x", "a/b", "x.lua" and an unterminated record are refused or
// bad arguments and change nothing.
static void TestM2ScriptFile( BkEditorSession *pSession, const std::string &szScratch )
{
	std::string szMap = SHIPPED_MAP;
	{
		const std::vector<std::string> paths = SortedShippedMaps();
		for ( size_t p = 0; p < paths.size(); ++p )
		{
			CMapInfo probe;
			std::string szProbeError;
			if ( NMapFile::Read( paths[p].c_str(), &probe, &szProbeError ) && !probe.szScriptFile.empty() && probe.szScriptFile.size() < 64 )
			{
				szMap = paths[p];
				break;
			}
		}
	}
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( szMap.c_str(), &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	printf( "editor-bridge: script file test on %s, which names \"%s\"\n", szMap.c_str(), original.szScriptFile.c_str() );
	const std::string szUnedited = szScratch + "\\scriptfile-unedited.bzm";
	const std::string szEdited = szScratch + "\\scriptfile-edited.bzm";
	const std::string szUndone = szScratch + "\\scriptfile-undone.bzm";
	const std::string szRefused = szScratch + "\\scriptfile-refused.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The read equals the file, verbatim.
	std::string szRead;
	Check( ReadScriptFileOf( pSession, &szRead ) && szRead == original.szScriptFile, "the script file reads as the file has it, verbatim" );
	Check( BkEditorScriptFile( pSession, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null out is a bad argument" );

	// m2_script is set, reads back and saves as the NMapRecords-built map.
	if ( !Check( PutScriptFileOf( pSession, "m2_script" ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( ReadScriptFileOf( pSession, &szRead ) && szRead == "m2_script", "the set name reads back" );
	CMapInfo expected = original;
	Check( NMapRecords::PutScriptFile( &expected, "m2_script" ), "the expected map takes the script file" );
	if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		CMapInfo saved;
		if ( Check( NMapFile::Read( szEdited.c_str(), &saved, &szError ), szError.c_str() ) )
		{
			std::string szWhere;
			Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
			       szWhere.empty() ? "the saved map equals the expected map" : ( "the script file save differs at " + szWhere ).c_str() );
			Check( saved.szScriptFile == "m2_script", "the saved file names m2_script" );
		}
	}
	Check( PutScriptFileOf( pSession, "" ) == BK_EDITOR_OK && ReadScriptFileOf( pSession, &szRead ) && szRead.empty(), "None (empty) is accepted" );

	// The file's own value goes back although it may not be a bare name, and the
	// map is then the unedited one.
	Check( PutScriptFileOf( pSession, original.szScriptFile.c_str() ) == BK_EDITOR_OK, "the value the file held puts back (an undo)" );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "a script file edit and its inverse save the unedited file byte for byte" );

	// New values that name a path or carry the extension are refused and change nothing.
	const char *const pszBad[] = { "..\\x", "a/b", "x.lua", "x.LUA", "..", ".hidden", "a b", "a:b", "dir\\name" };
	for ( size_t i = 0; i < sizeof pszBad / sizeof pszBad[0]; ++i )
	{
		Check( PutScriptFileOf( pSession, pszBad[i] ) == BK_EDITOR_REFUSED, NStr::Format( "\"%s\" is refused", pszBad[i] ) );
		if ( i == 0 )
			Check( std::string( BkEditorLastMessage( pSession ) ).find( "without folder or .lua" ) != std::string::npos, "and the refusal says how a script is named" );
	}
	BkEditorScriptFileRecord unterminated;
	memset( &unterminated, 'a', sizeof unterminated );
	Check( BkEditorSetScriptFile( pSession, &unterminated ) == BK_EDITOR_BAD_ARGUMENT, "an unterminated name is a bad argument" );
	Check( BkEditorSetScriptFile( pSession, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null record is a bad argument" );
	Check( ReadScriptFileOf( pSession, &szRead ) && szRead == original.szScriptFile, "none of the refusals changed the script file" );
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szRefused ), "the refusals changed nothing: the map saves unedited byte for byte" );

	// A file's own odd value: it opens, reads verbatim, takes a bare name and
	// puts the odd value back - but another odd name is a new value.
	{
		const std::string szOdd = szScratch + "\\scriptfile-odd.bzm";
		const char *const pszOddName = "..\\odd name.lua";
		CMapInfo odd = original;
		NMapRecords::PutScriptFile( &odd, pszOddName );
		if ( Check( NMapFile::Write( szOdd.c_str(), odd, &szError ), szError.c_str() ) &&
		     Check( BkEditorOpenMap( pSession, szOdd.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			Check( ReadScriptFileOf( pSession, &szRead ) && szRead == pszOddName, "an odd script file reads as the file has it" );
			Check( PutScriptFileOf( pSession, "m2_script" ) == BK_EDITOR_OK, "a bare name replaces an odd one" );
			Check( PutScriptFileOf( pSession, pszOddName ) == BK_EDITOR_OK, "and the odd value goes back exactly, as an undo needs" );
			Check( PutScriptFileOf( pSession, "..\\another odd.lua" ) == BK_EDITOR_REFUSED, "but another odd name is new and is refused" );
			Check( ReadScriptFileOf( pSession, &szRead ) && szRead == pszOddName, "the refusal left the odd value as it was" );
		}
		remove( OsPath( szOdd ).c_str() );
		// A value the record cannot hold reads as a refusal and stays editable
		// only away from it: the file keeps it byte-exact.
		const std::string szLong = szScratch + "\\scriptfile-long.bzm";
		CMapInfo longName = original;
		NMapRecords::PutScriptFile( &longName, std::string( 70, 'b' ) );
		if ( Check( NMapFile::Write( szLong.c_str(), longName, &szError ), szError.c_str() ) &&
		     Check( BkEditorOpenMap( pSession, szLong.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			BkEditorStatus status = BK_EDITOR_OK;
			Check( !ReadScriptFileOf( pSession, &szRead, &status ) && status == BK_EDITOR_REFUSED, "a script file name of 70 characters reads as a refusal" );
			const std::string szLongSaved = szScratch + "\\scriptfile-long-saved.bzm";
			CMapInfo savedLong;
			if ( Check( BkEditorSaveMap( pSession, szLongSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
			     Check( NMapFile::Read( szLongSaved.c_str(), &savedLong, &szError ), szError.c_str() ) )
				Check( savedLong.szScriptFile == std::string( 70, 'b' ), "and saves byte-exact while nobody edits it" );
			remove( OsPath( szLongSaved ).c_str() );
		}
		remove( OsPath( szLong ).c_str() );
	}
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szRefused ).c_str() );
	printf( "editor-bridge: M2 script file ok\n" );
}

// The bridge's script areas, two-pass.
static bool ReadAreasOf( BkEditorSession *pSession, std::vector<BkEditorScriptAreaRecord> *pAreas, BkEditorStatus *pStatus = 0 )
{
	int nCount = -2;
	// The sizing pass is REFUSED whenever there is anything to count: a capacity
	// below the total is (bridge.h), and *pnCount is the total all the same.
	BkEditorStatus status = BkEditorScriptAreas( pSession, 0, 0, &nCount );
	if ( pStatus != 0 )
		*pStatus = status;
	if ( ( status != BK_EDITOR_OK && status != BK_EDITOR_REFUSED ) || nCount < 0 )
		return false;
	pAreas->assign( nCount > 0 ? nCount : 1, BkEditorScriptAreaRecord() );
	int nRead = -2;
	status = BkEditorScriptAreas( pSession, &( *pAreas )[0], nCount, &nRead );
	if ( pStatus != 0 )
		*pStatus = status;
	if ( status != BK_EDITOR_OK || nRead != nCount )
		return false;
	pAreas->resize( nCount );
	return true;
}

static BkEditorScriptAreaRecord AreaRecordOf( const char *pszName, int nType, float fCx, float fCy, float fHx, float fHy, float fR )
{
	BkEditorScriptAreaRecord record;
	memset( &record, 0, sizeof record );
	strncpy( record.name, pszName, sizeof record.name - 1 );
	record.type = nType;
	record.cx = fCx; record.cy = fCy; record.hx = fHx; record.hy = fHy; record.r = fR;
	return record;
}

static bool SameAreaValue( const BkEditorScriptAreaRecord &rRecord, const SScriptArea &rArea )
{
	return std::string( rRecord.name ) == rArea.szName && rRecord.type == int( rArea.eType ) && rRecord.cx == rArea.center.x && rRecord.cy == rArea.center.y &&
	       rRecord.hx == rArea.vAABBHalfSize.x && rRecord.hy == rArea.vAABBHalfSize.y && rRecord.r == rArea.fR;
}

static bool SameAreaRecords( const BkEditorScriptAreaRecord &rLeft, const BkEditorScriptAreaRecord &rRight )
{
	return std::string( rLeft.name ) == rRight.name && rLeft.type == rRight.type && rLeft.cx == rRight.cx && rLeft.cy == rRight.cy &&
	       rLeft.hx == rRight.hx && rLeft.hy == rRight.hy && rLeft.r == rRight.r;
}

// D-21 on the real engine: the areas read as the file has them; a rectangle and a
// circle dragged in world units come back through BkEditorScriptAreaFromVis as the
// NMapGeometry values (the MFC truncation, once), add, rename and save as the map
// the same NMapRecords calls build, and deleted again leave the unedited bytes.
// Names are non-empty and unique, case-sensitive; an off-map centre, a negative size,
// a bad type or index and a non-finite number are refused and change nothing; the
// handle conversions equal MoveArea and ResizeArea. A file's own duplicate name and
// too-long name are kept: the pair can be put back beside its twin, the long name
// reads as a refusal and saves byte-exact.
static void TestM2ScriptAreas( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\areas-unedited.bzm";
	const std::string szEdited = szScratch + "\\areas-edited.bzm";
	const std::string szUndone = szScratch + "\\areas-undone.bzm";
	const std::string szRefused = szScratch + "\\areas-refused.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The read equals the file.
	std::vector<BkEditorScriptAreaRecord> before;
	if ( !Check( ReadAreasOf( pSession, &before ), BkEditorLastMessage( pSession ) ) )
		return;
	bool bSame = before.size() == original.scriptAreas.size();
	for ( size_t i = 0; i < before.size() && bSame; ++i )
		bSame = SameAreaValue( before[i], original.scriptAreas[i] );
	Check( bSame, NStr::Format( "the %d script areas read as the file has them", int( original.scriptAreas.size() ) ) );
	int nSizingCount = -2;
	const BkEditorStatus sizing = BkEditorScriptAreas( pSession, 0, 0, &nSizingCount );
	Check( sizing == ( original.scriptAreas.empty() ? BK_EDITOR_OK : BK_EDITOR_REFUSED ) && nSizingCount == int( original.scriptAreas.size() ),
	       "the sizing pass answers the total (REFUSED when there is something to count, as a buffer too short is)" );
	Check( BkEditorScriptAreas( pSession, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT && BkEditorScriptAreas( pSession, 0, -1, &nSizingCount ) == BK_EDITOR_BAD_ARGUMENT, "a null count or a negative capacity is a bad argument" );

	// The conversion of a drag, in world units at the map's middle.
	const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
	const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
	BkEditorScriptAreaRecord rect, ring;
	memset( &rect, 0x55, sizeof rect );
	if ( !Check( BkEditorScriptAreaFromVis( pSession, 0, fMiddleX - 100.0f, fMiddleY - 60.0f, fMiddleX + 100.0f, fMiddleY + 60.0f, "m2_area", &rect ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const SScriptArea expectedRect = NMapGeometry::AreaFromVis( SScriptArea::EAT_RECTANGLE, CVec2( fMiddleX - 100.0f, fMiddleY - 60.0f ), CVec2( fMiddleX + 100.0f, fMiddleY + 60.0f ), "m2_area" );
	Check( SameAreaValue( rect, expectedRect ), "BkEditorScriptAreaFromVis gives NMapGeometry::AreaFromVis for a rectangle" );
	Check( BkEditorScriptAreaFromVis( pSession, 1, fMiddleX + 400.0f, fMiddleY, fMiddleX + 480.0f, fMiddleY, "m2_ring", &ring ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	const SScriptArea expectedRing = NMapGeometry::AreaFromVis( SScriptArea::EAT_CIRCLE, CVec2( fMiddleX + 400.0f, fMiddleY ), CVec2( fMiddleX + 480.0f, fMiddleY ), "m2_ring" );
	Check( SameAreaValue( ring, expectedRing ), "and for a circle" );
	Check( rect.type == 0 && ring.type == 1 && ring.r > 0.0f && rect.hx > 0.0f, "the two shapes have their own fields" );
	// No map is touched by a conversion.
	std::vector<BkEditorScriptAreaRecord> stillBefore;
	Check( ReadAreasOf( pSession, &stillBefore ) && stillBefore.size() == before.size(), "the conversion added nothing" );
	// The handle conversions are MoveArea and ResizeArea.
	BkEditorScriptAreaRecord moved, resized;
	Check( BkEditorScriptAreaMoved( pSession, &rect, fMiddleX + 30.0f, fMiddleY - 20.0f, &moved ) == BK_EDITOR_OK &&
	       SameAreaValue( moved, NMapGeometry::MoveArea( expectedRect, CVec2( fMiddleX + 30.0f, fMiddleY - 20.0f ) ) ), "BkEditorScriptAreaMoved gives NMapGeometry::MoveArea" );
	Check( BkEditorScriptAreaResized( pSession, &ring, fMiddleX + 520.0f, fMiddleY + 10.0f, &resized ) == BK_EDITOR_OK &&
	       SameAreaValue( resized, NMapGeometry::ResizeArea( expectedRing, CVec2( fMiddleX + 520.0f, fMiddleY + 10.0f ) ) ), "BkEditorScriptAreaResized gives NMapGeometry::ResizeArea" );
	BkEditorScriptAreaRecord unusedOut;
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	Check( BkEditorScriptAreaFromVis( pSession, 2, 0, 0, 1, 1, "x", &unusedOut ) == BK_EDITOR_BAD_ARGUMENT, "a type other than 0 and 1 is a bad argument" );
	Check( BkEditorScriptAreaFromVis( pSession, 0, fNaN, 0, 1, 1, "x", &unusedOut ) == BK_EDITOR_BAD_ARGUMENT, "a NaN drag is a bad argument" );
	Check( BkEditorScriptAreaFromVis( pSession, 0, 0, 0, 1, 1, 0, &unusedOut ) == BK_EDITOR_BAD_ARGUMENT && BkEditorScriptAreaFromVis( pSession, 0, 0, 0, 1, 1, "x", 0 ) == BK_EDITOR_BAD_ARGUMENT,
	       "a null name or out is a bad argument" );
	Check( BkEditorScriptAreaMoved( pSession, 0, 0, 0, &unusedOut ) == BK_EDITOR_BAD_ARGUMENT && BkEditorScriptAreaMoved( pSession, &rect, fNaN, 0, &unusedOut ) == BK_EDITOR_BAD_ARGUMENT, "a null area or a NaN handle is a bad argument" );

	// Both are added, append order kept.
	if ( !Check( BkEditorAddScriptArea( pSession, -1, &rect ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	if ( !Check( BkEditorAddScriptArea( pSession, int( before.size() ) + 1, &ring ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	std::vector<BkEditorScriptAreaRecord> after;
	Check( ReadAreasOf( pSession, &after ) && after.size() == before.size() + 2 && SameAreaRecords( after[before.size()], rect ) && SameAreaRecords( after[before.size() + 1], ring ),
	       "the two areas read back in the order they were added, the old ones unchanged" );
	CMapInfo expected = original;
	Check( NMapRecords::InsertScriptArea( &expected, -1, expectedRect ) && NMapRecords::InsertScriptArea( &expected, -1, expectedRing ), "the expected map takes both areas" );
	if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		CMapInfo saved;
		if ( Check( NMapFile::Read( szEdited.c_str(), &saved, &szError ), szError.c_str() ) )
		{
			std::string szWhere;
			Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
			       szWhere.empty() ? "the saved map equals the expected map" : ( "the areas save differs at " + szWhere ).c_str() );
		}
	}

	// A rename is a set of the same index; the areas' names stay unique and case-sensitive.
	BkEditorScriptAreaRecord renamed = ring;
	strncpy( renamed.name, "m2_zone", sizeof renamed.name - 1 );
	Check( BkEditorSetScriptArea( pSession, int( before.size() ) + 1, &renamed ) == BK_EDITOR_OK, "an area is renamed" );
	BkEditorScriptAreaRecord upper = ring;
	strncpy( upper.name, "M2_AREA", sizeof upper.name - 1 );
	Check( BkEditorSetScriptArea( pSession, int( before.size() ) + 1, &upper ) == BK_EDITOR_OK, "names are case-sensitive: M2_AREA is not m2_area" );
	Check( BkEditorSetScriptArea( pSession, int( before.size() ) + 1, &ring ) == BK_EDITOR_OK, "the old name goes back" );
	Check( BkEditorSetScriptArea( pSession, int( before.size() ) + 1, &ring ) == BK_EDITOR_OK, "setting a record to its own value, name included, is accepted" );

	// Refusals change nothing.
	BkEditorScriptAreaRecord clash = ring;
	strncpy( clash.name, "m2_area", sizeof clash.name - 1 );
	Check( BkEditorSetScriptArea( pSession, int( before.size() ) + 1, &clash ) == BK_EDITOR_REFUSED, "a name another area holds is refused" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "m2_area" ) != std::string::npos && std::string( BkEditorLastMessage( pSession ) ).find( "exists" ) != std::string::npos, "and the refusal names it" );
	Check( BkEditorAddScriptArea( pSession, -1, &rect ) == BK_EDITOR_REFUSED, "adding the same name again is refused" );
	BkEditorScriptAreaRecord nameless = rect;
	nameless.name[0] = 0;
	Check( BkEditorAddScriptArea( pSession, -1, &nameless ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "needs a name" ) != std::string::npos, "an empty name is refused, saying why" );
	Check( BkEditorSetScriptArea( pSession, int( before.size() ) + 1, &nameless ) == BK_EDITOR_REFUSED, "so is renaming to nothing" );
	BkEditorScriptAreaRecord offMap = rect;
	strncpy( offMap.name, "off_map", sizeof offMap.name - 1 );
	offMap.cx = -5.0f;
	Check( BkEditorAddScriptArea( pSession, -1, &offMap ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "centre" ) != std::string::npos, "an area centred off the map is refused" );
	offMap.cx = 1.0e9f;
	Check( BkEditorAddScriptArea( pSession, -1, &offMap ) == BK_EDITOR_REFUSED, "so is one far beyond the far edge" );
	BkEditorScriptAreaRecord negative = rect;
	strncpy( negative.name, "negative", sizeof negative.name - 1 );
	negative.hx = -1.0f;
	Check( BkEditorAddScriptArea( pSession, -1, &negative ) == BK_EDITOR_REFUSED, "a negative size is refused" );
	BkEditorScriptAreaRecord badType = rect;
	strncpy( badType.name, "bad_type", sizeof badType.name - 1 );
	badType.type = 2;
	Check( BkEditorAddScriptArea( pSession, -1, &badType ) == BK_EDITOR_BAD_ARGUMENT, "a type other than 0 and 1 is a bad argument" );
	BkEditorScriptAreaRecord nonFinite = rect;
	strncpy( nonFinite.name, "non_finite", sizeof nonFinite.name - 1 );
	nonFinite.r = fNaN;
	Check( BkEditorAddScriptArea( pSession, -1, &nonFinite ) == BK_EDITOR_BAD_ARGUMENT, "a NaN radius is a bad argument" );
	BkEditorScriptAreaRecord unterminated;
	memset( &unterminated, 'a', sizeof unterminated );
	unterminated.type = 0;
	Check( BkEditorAddScriptArea( pSession, -1, &unterminated ) == BK_EDITOR_BAD_ARGUMENT && BkEditorAddScriptArea( pSession, -1, 0 ) == BK_EDITOR_BAD_ARGUMENT, "an unterminated name or a null record is a bad argument" );
	BkEditorScriptAreaRecord fresh = AreaRecordOf( "fresh", 0, fMiddleX, fMiddleY, 10.0f, 10.0f, 0.0f );
	Check( BkEditorAddScriptArea( pSession, -2, &fresh ) == BK_EDITOR_BAD_ARGUMENT && BkEditorAddScriptArea( pSession, int( after.size() ) + 1, &fresh ) == BK_EDITOR_BAD_ARGUMENT, "an insert index out of range is a bad argument" );
	Check( BkEditorSetScriptArea( pSession, int( after.size() ), &fresh ) == BK_EDITOR_BAD_ARGUMENT && BkEditorSetScriptArea( pSession, -1, &fresh ) == BK_EDITOR_BAD_ARGUMENT, "a set index out of range is a bad argument" );
	Check( BkEditorDeleteScriptArea( pSession, int( after.size() ) ) == BK_EDITOR_BAD_ARGUMENT && BkEditorDeleteScriptArea( pSession, -1 ) == BK_EDITOR_BAD_ARGUMENT, "a delete index out of range is a bad argument" );
	std::vector<BkEditorScriptAreaRecord> unchanged;
	bool bUnchanged = ReadAreasOf( pSession, &unchanged ) && unchanged.size() == after.size();
	for ( size_t i = 0; i < unchanged.size() && bUnchanged; ++i )
		bUnchanged = SameAreaRecords( unchanged[i], after[i] );
	Check( bUnchanged, "none of the refusals changed the areas" );
	// The edit, moved and resized, then put back: the same record twice.
	Check( BkEditorSetScriptArea( pSession, int( before.size() ) + 1, &resized ) == BK_EDITOR_OK && BkEditorSetScriptArea( pSession, int( before.size() ) + 1, &ring ) == BK_EDITOR_OK,
	       "a resized area and its old value both put" );

	// Deleted again, the areas are the file's and the bytes the unedited save's.
	Check( BkEditorDeleteScriptArea( pSession, int( before.size() ) + 1 ) == BK_EDITOR_OK && BkEditorDeleteScriptArea( pSession, int( before.size() ) ) == BK_EDITOR_OK, "both areas are deleted" );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "add, edit and delete of areas save the unedited file byte for byte" );
	// Put back at their own indexes (an undo of the deletes) the two are where they were.
	Check( BkEditorAddScriptArea( pSession, int( before.size() ), &rect ) == BK_EDITOR_OK && BkEditorAddScriptArea( pSession, int( before.size() ) + 1, &ring ) == BK_EDITOR_OK, "the deleted areas go back at their own indexes" );
	std::vector<BkEditorScriptAreaRecord> back;
	bool bBack = ReadAreasOf( pSession, &back ) && back.size() == after.size();
	for ( size_t i = 0; i < back.size() && bBack; ++i )
		bBack = SameAreaRecords( back[i], after[i] );
	Check( bBack, "and read as before the deletes" );
	Check( BkEditorDeleteScriptArea( pSession, int( before.size() ) + 1 ) == BK_EDITOR_OK && BkEditorDeleteScriptArea( pSession, int( before.size() ) ) == BK_EDITOR_OK, "and are deleted once more" );
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szRefused ), "with all of them deleted the map saves the unedited bytes again" );

	// A file's own odd data: two areas of one name, and a name too long for the record.
	{
		const std::string szOdd = szScratch + "\\areas-odd.bzm";
		CMapInfo odd = original;
		NMapRecords::InsertScriptArea( &odd, -1, NMapGeometry::AreaFromVis( SScriptArea::EAT_CIRCLE, CVec2( fMiddleX, fMiddleY ), CVec2( fMiddleX + 40.0f, fMiddleY ), "twin" ) );
		NMapRecords::InsertScriptArea( &odd, -1, NMapGeometry::AreaFromVis( SScriptArea::EAT_CIRCLE, CVec2( fMiddleX + 200.0f, fMiddleY ), CVec2( fMiddleX + 240.0f, fMiddleY ), "twin" ) );
		const int nTwin = int( odd.scriptAreas.size() ) - 2;
		if ( Check( NMapFile::Write( szOdd.c_str(), odd, &szError ), szError.c_str() ) &&
		     Check( BkEditorOpenMap( pSession, szOdd.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			const std::string szOddBefore = szScratch + "\\areas-odd-before.bzm", szOddAfter = szScratch + "\\areas-odd-after.bzm";
			Check( BkEditorSaveMap( pSession, szOddBefore.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			std::vector<BkEditorScriptAreaRecord> read;
			// Indexed only once the read is known to hold both twins: a failed read
			// leaves the vector short, and a hardened libc++ aborts on an index past it.
			const bool bTwins = ReadAreasOf( pSession, &read ) && nTwin >= 0 && int( read.size() ) == nTwin + 2 &&
			                    std::string( read[nTwin].name ) == "twin" && std::string( read[nTwin + 1].name ) == "twin";
			Check( bTwins, "the two areas of one name read as the file has them" );
			if ( bTwins )
			{
				const BkEditorScriptAreaRecord first = read[nTwin];
				Check( BkEditorDeleteScriptArea( pSession, nTwin ) == BK_EDITOR_OK && BkEditorAddScriptArea( pSession, nTwin, &first ) == BK_EDITOR_OK,
				       "one of the pair is deleted and put back beside its twin, as an undo needs" );
				BkEditorScriptAreaRecord third = first;
				Check( BkEditorAddScriptArea( pSession, -1, &third ) == BK_EDITOR_REFUSED, "but a third of that name is a new duplicate and is refused" );
				if ( Check( BkEditorSaveMap( pSession, szOddAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
					Check( SameBytes( szOddBefore, szOddAfter ), "the odd map saves the same bytes after the delete and the put back" );
			}
			remove( OsPath( szOddBefore ).c_str() );
			remove( OsPath( szOddAfter ).c_str() );
		}
		remove( OsPath( szOdd ).c_str() );
		// WR-A04: a file area off the map with a negative size can be deleted
		// and moved, and both undos (the exact record put back) go through.
		{
			const std::string szOffMap = szScratch + "\\areas-offmap.bzm";
			CMapInfo offMap = original;
			SScriptArea offArea;
			offArea.eType = SScriptArea::EAT_RECTANGLE;
			offArea.szName = "offmap";
			offArea.center = CVec2( -50.0f, 1.0e6f );
			offArea.vAABBHalfSize = CVec2( -3.0f, 4.0f );
			offArea.fR = 0.0f;
			NMapRecords::InsertScriptArea( &offMap, -1, offArea );
			const int nOff = int( offMap.scriptAreas.size() ) - 1;
			if ( Check( NMapFile::Write( szOffMap.c_str(), offMap, &szError ), szError.c_str() ) &&
			     Check( BkEditorOpenMap( pSession, szOffMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			{
				const std::string szOffBefore = szScratch + "\\areas-offmap-before.bzm", szOffAfter = szScratch + "\\areas-offmap-after.bzm";
				Check( BkEditorSaveMap( pSession, szOffBefore.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
				const BkEditorScriptAreaRecord fileOwn = AreaRecordOf( "offmap", 0, -50.0f, 1.0e6f, -3.0f, 4.0f, 0.0f );
				Check( BkEditorDeleteScriptArea( pSession, nOff ) == BK_EDITOR_OK && BkEditorAddScriptArea( pSession, nOff, &fileOwn ) == BK_EDITOR_OK,
				       NStr::Format( "the file's off-map area is deleted and put back, as an undo needs: %s", BkEditorLastMessage( pSession ) ) );
				const BkEditorScriptAreaRecord onMap = AreaRecordOf( "offmap", 0, fMiddleX, fMiddleY, 3.0f, 4.0f, 0.0f );
				Check( BkEditorSetScriptArea( pSession, nOff, &onMap ) == BK_EDITOR_OK && BkEditorSetScriptArea( pSession, nOff, &fileOwn ) == BK_EDITOR_OK,
				       NStr::Format( "moved onto the map and set back exactly: %s", BkEditorLastMessage( pSession ) ) );
				const BkEditorScriptAreaRecord newOdd = AreaRecordOf( "offmap2", 0, -50.0f, 1.0e6f, -3.0f, 4.0f, 0.0f );
				Check( BkEditorAddScriptArea( pSession, -1, &newOdd ) == BK_EDITOR_REFUSED, "a NEW off-map area is still refused" );
				if ( Check( BkEditorSaveMap( pSession, szOffAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
					Check( SameBytes( szOffBefore, szOffAfter ), "the off-map area map saves the same bytes after the edits and their put backs" );
				remove( OsPath( szOffBefore ).c_str() );
				remove( OsPath( szOffAfter ).c_str() );
			}
			remove( OsPath( szOffMap ).c_str() );
		}
		const std::string szLong = szScratch + "\\areas-long.bzm";
		CMapInfo longName = original;
		SScriptArea longArea = NMapGeometry::AreaFromVis( SScriptArea::EAT_CIRCLE, CVec2( fMiddleX, fMiddleY ), CVec2( fMiddleX + 40.0f, fMiddleY ), std::string( 70, 'n' ) );
		NMapRecords::InsertScriptArea( &longName, -1, longArea );
		if ( Check( NMapFile::Write( szLong.c_str(), longName, &szError ), szError.c_str() ) &&
		     Check( BkEditorOpenMap( pSession, szLong.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			std::vector<BkEditorScriptAreaRecord> read;
			BkEditorStatus status = BK_EDITOR_OK;
			Check( !ReadAreasOf( pSession, &read, &status ) && status == BK_EDITOR_REFUSED, "an area name of 70 characters reads as a refusal" );
			const std::string szLongSaved = szScratch + "\\areas-long-saved.bzm";
			CMapInfo savedLong;
			if ( Check( BkEditorSaveMap( pSession, szLongSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
			     Check( NMapFile::Read( szLongSaved.c_str(), &savedLong, &szError ), szError.c_str() ) )
				Check( !savedLong.scriptAreas.empty() && savedLong.scriptAreas.back().szName == std::string( 70, 'n' ), "and the map saves it byte-exact" );
			remove( OsPath( szLongSaved ).c_str() );
		}
		remove( OsPath( szLong ).c_str() );
	}
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szRefused ).c_str() );
	printf( "editor-bridge: M2 script areas ok\n" );
}

// D-16's Hide checked, on the real engine and the real pixels. One object a
// click picks with the camera on it, given script ID 4243 and hidden by that
// ID: the pixels of its screen box change (above 1 % of the box, against a
// control pair of unchanged captures), a click on it no longer answers it;
// unhidden it answers again and the box is drawn exactly as before (a shadow or
// a health bar left behind would show here). Hiding is a view setting: the map
// saves the same bytes hidden and shown, and a script ID that joins or leaves
// the hidden set hides or shows the object at once.
static bool FindPickableObject( BkEditorSession *pSession, int nGameType, const std::map<std::string, int> &rGameTypes, int nScreenWidth, int nScreenHeight,
                                int *pnLink, int *pnScriptID, float *pfCameraX, float *pfCameraY )
{
	const std::vector<BkEditorObjectRecord> objects = ReadObjectRecords( pSession );
	for ( size_t i = 0; i < objects.size(); ++i )
	{
		const BkEditorObjectRecord &rRecord = objects[i];
		if ( rRecord.scenario != 0 || rRecord.link_id == 0 || rRecord.known == 0 )
			continue;
		if ( nGameType >= 0 )
		{
			const std::map<std::string, int>::const_iterator itType = rGameTypes.find( rRecord.name );
			if ( itType == rGameTypes.end() || itType->second != nGameType )
				continue;
		}
		int nSharing = 0;
		for ( size_t j = 0; j < objects.size(); ++j )
			nSharing += objects[j].link_id == rRecord.link_id ? 1 : 0;
		BkEditorObjectState engineState;
		if ( nSharing != 1 || BkEditorEngineObjectState( pSession, rRecord.link_id, &engineState ) != BK_EDITOR_OK )
			continue;
		CVec3 vAnchor;
		AI2Vis( &vAnchor, engineState.x, engineState.y, 0.0f );
		BkEditorSetCamera( pSession, vAnchor.x, vAnchor.y );
		for ( int f = 0; f < 3; ++f )
			BkEditorFrame( pSession );
		int nPicked = -1;
		if ( BkEditorObjectAt( pSession, nScreenWidth / 2.0f, nScreenHeight / 2.0f - PICK_RISE, &nPicked ) == BK_EDITOR_OK && nPicked == rRecord.link_id )
		{
			*pnLink = rRecord.link_id;
			*pnScriptID = rRecord.script_id;
			*pfCameraX = vAnchor.x;
			*pfCameraY = vAnchor.y;
			return true;
		}
	}
	return false;
}

static void HideOneObject( BkEditorSession *pSession, const char *pszWhat, int nUnit, int nOriginal, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	const float fClickX = nScreenWidth / 2.0f, fClickY = nScreenHeight / 2.0f - PICK_RISE;
	// The box round the click point, and the two captures of the unchanged view.
	const int nHalf = 30;
	const int nLeft = int( fClickX ) - nHalf, nRight = int( fClickX ) + nHalf, nTop = int( fClickY ) - nHalf, nBottom = int( fClickY ) + nHalf;
	const int nBoxArea = ( nRight - nLeft ) * ( nBottom - nTop );
	const std::string szShown = szScratch + NStr::Format( "/04-09-hide-%s-shown.tga", pszWhat ), szControl = szScratch + NStr::Format( "/04-09-hide-%s-control.tga", pszWhat );
	const std::string szHidden = szScratch + NStr::Format( "/04-09-hide-%s-hidden.tga", pszWhat ), szBack = szScratch + NStr::Format( "/04-09-hide-%s-back.tga", pszWhat );
	if ( !SaveFrame( pSession, szShown ) )
		return;
	for ( int f = 0; f < 3; ++f )
		BkEditorFrame( pSession );
	if ( !SaveFrame( pSession, szControl ) )
		return;

	// Script ID 4243 on the object, then hidden by that ID.
	if ( !Check( BkEditorSetObjectScriptID( pSession, nUnit, 4243 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int nHide = 4243;
	if ( !Check( BkEditorSetHiddenScriptIDs( pSession, &nHide, 1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	for ( int f = 0; f < 3; ++f )
		BkEditorFrame( pSession );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "with the " ) + pszWhat + " hidden: " + BkEditorLastMessage( pSession ) ).c_str() );
	if ( !SaveFrame( pSession, szHidden ) )
		return;
	int nWidth = 0, nHeight = 0;
	const std::vector<unsigned char> shown = ReadFramePixels( szShown, &nWidth, &nHeight );
	const std::vector<unsigned char> control = ReadFramePixels( szControl, &nWidth, &nHeight );
	const std::vector<unsigned char> hidden = ReadFramePixels( szHidden, &nWidth, &nHeight );
	const int nNoise = ChangedPixels( shown, control, nWidth, nHeight, nLeft, nTop, nRight, nBottom );
	const int nChangedByHide = ChangedPixels( shown, hidden, nWidth, nHeight, nLeft, nTop, nRight, nBottom );
	printf( "editor-bridge: hiding the %s (object %d) changed %d of %d pixels of its box (%d between two unchanged captures)\n", pszWhat, nUnit, nChangedByHide, nBoxArea, nNoise );
	Check( nChangedByHide > nBoxArea / 100 && nChangedByHide > nNoise * 4,
	       NStr::Format( "hiding the %s changes %d of the %d pixels of its box, above 1%% and above the noise of %d", pszWhat, nChangedByHide, nBoxArea, nNoise ) );
	int nPicked = -1;
	const bool bAnswers = BkEditorObjectAt( pSession, fClickX, fClickY, &nPicked ) == BK_EDITOR_OK && nPicked == nUnit;
	Check( !bAnswers, NStr::Format( "a click on the hidden %s no longer answers it", pszWhat ) );

	// Hiding is not an edit: the map saves the same bytes hidden and shown.
	const std::string szSavedHidden = szScratch + NStr::Format( "\\hide-%s-saved-hidden.bzm", pszWhat ), szSavedShown = szScratch + NStr::Format( "\\hide-%s-saved-shown.bzm", pszWhat );
	const bool bSavedHidden = BkEditorSaveMap( pSession, szSavedHidden.c_str() ) == BK_EDITOR_OK;
	Check( bSavedHidden, BkEditorLastMessage( pSession ) );

	// Shown again: the object answers a click, and the box is drawn as before.
	if ( !Check( BkEditorSetHiddenScriptIDs( pSession, 0, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	for ( int f = 0; f < 3; ++f )
		BkEditorFrame( pSession );
	nPicked = -1;
	Check( BkEditorObjectAt( pSession, fClickX, fClickY, &nPicked ) == BK_EDITOR_OK && nPicked == nUnit, NStr::Format( "unhidden, a click on the %s answers it again", pszWhat ) );
	if ( SaveFrame( pSession, szBack ) )
	{
		const std::vector<unsigned char> back = ReadFramePixels( szBack, &nWidth, &nHeight );
		const int nChangedBack = ChangedPixels( hidden, back, nWidth, nHeight, nLeft, nTop, nRight, nBottom );
		const int nOffFromShown = ChangedPixels( shown, back, nWidth, nHeight, nLeft, nTop, nRight, nBottom );
		printf( "editor-bridge: unhiding the %s changed %d pixels of the box, %d from the first capture\n", pszWhat, nChangedBack, nOffFromShown );
		Check( nChangedBack > nBoxArea / 100, NStr::Format( "unhiding the %s changes the box again", pszWhat ) );
		Check( nOffFromShown <= Max( nBoxArea / 100, nNoise * 4 ), NStr::Format( "and the box is drawn as before (%d pixels differ from the first capture)", nOffFromShown ) );
	}
	if ( Check( BkEditorSaveMap( pSession, szSavedShown.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) && bSavedHidden )
		Check( SameBytes( szSavedHidden, szSavedShown ), "the map saves the same bytes hidden and shown" );
	remove( OsPath( szSavedHidden ).c_str() );
	remove( OsPath( szSavedShown ).c_str() );

	// A script ID that leaves the hidden set shows the object; one that joins it hides it.
	Check( BkEditorSetHiddenScriptIDs( pSession, &nHide, 1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSetObjectScriptID( pSession, nUnit, nOriginal ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	for ( int f = 0; f < 3; ++f )
		BkEditorFrame( pSession );
	nPicked = -1;
	Check( BkEditorObjectAt( pSession, fClickX, fClickY, &nPicked ) == BK_EDITOR_OK && nPicked == nUnit, NStr::Format( "a %s whose script ID left the hidden set is shown and picked", pszWhat ) );
	Check( BkEditorSetObjectScriptID( pSession, nUnit, 4243 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	nPicked = -1;
	Check( !( BkEditorObjectAt( pSession, fClickX, fClickY, &nPicked ) == BK_EDITOR_OK && nPicked == nUnit ), NStr::Format( "and one whose script ID joined it is hidden and not picked" ) );
	Check( BkEditorSetObjectScriptID( pSession, nUnit, nOriginal ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSetHiddenScriptIDs( pSession, 0, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	for ( int f = 0; f < 3; ++f )
		BkEditorFrame( pSession );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "with the " ) + pszWhat + " shown again: " + BkEditorLastMessage( pSession ) ).c_str() );
}

struct SHideKind
{
	int nGameType;          // -1: the first object that picks, whatever it is
	const char *pszWhat;
	bool bRequired;         // false: not finding one is a note, not a failure
};

// The kinds on one map: each measured by HideOneObject, then the map saves the
// unedited file byte for byte again.
static void HideKindsOnMap( BkEditorSession *pSession, const char *pszMap, const SHideKind *pKinds, int nKinds, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, pszMap, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\hide-unedited.bzm", szUndone = szScratch + "\\hide-undone.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The game type of every catalogue name: a static object, a mesh unit (a
	// tank, game type 1) and a squad (15) are hidden differently by the engine
	// (a sprite and its shadow, a mesh with a shadow pass and an icon, soldiers).
	int nCatalogue = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
	std::vector<BkEditorCatalogueEntry> catalogue( nCatalogue > 0 ? nCatalogue : 1 );
	int nRead = 0;
	BkEditorCatalogue( pSession, &catalogue[0], nCatalogue, &nRead );
	std::map<std::string, int> gameTypes;
	for ( int i = 0; i < nRead; ++i )
		gameTypes[catalogue[i].name] = catalogue[i].game_type;

	for ( int k = 0; k < nKinds; ++k )
	{
		int nUnit = -1, nOriginal = -1;
		float fCameraX = 0.0f, fCameraY = 0.0f;
		if ( !FindPickableObject( pSession, pKinds[k].nGameType, gameTypes, nScreenWidth, nScreenHeight, &nUnit, &nOriginal, &fCameraX, &fCameraY ) )
		{
			if ( pKinds[k].bRequired )
				Check( false, NStr::Format( "%s has a %s a click picks with the camera on it", pszMap, pKinds[k].pszWhat ) );
			else
				printf( "editor-bridge: no %s of %s is picked by a click with the camera on it; not measured\n", pKinds[k].pszWhat, pszMap );
			continue;
		}
		printf( "editor-bridge: hide test on the %s of %s, object %d (script ID %d), camera at %.0f,%.0f\n", pKinds[k].pszWhat, pszMap, nUnit, nOriginal, fCameraX, fCameraY );
		HideOneObject( pSession, pKinds[k].pszWhat, nUnit, nOriginal, nScreenWidth, nScreenHeight, szScratch );
	}

	// With every script ID put back the map is the unedited file.
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), NStr::Format( "with the script IDs put back %s saves the unedited file byte for byte", pszMap ) );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
}

static void TestM2HideGroups( BkEditorSession *pSession, int nScreenWidth, int nScreenHeight, const std::string &szScratch )
{
	const SHideKind coldwinter[] = { { -1, "object", true }, { 1, "unit", true } };
	HideKindsOnMap( pSession, SHIPPED_MAP, coldwinter, 2, nScreenWidth, nScreenHeight, szScratch );
	// Coldwinter has no squad a click picks; the bridge map does have squads.
	const SHideKind squads[] = { { 15, "squad", false } };
	HideKindsOnMap( pSession, BRIDGE_MAP, squads, 1, nScreenWidth, nScreenHeight, szScratch );

	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	// The argument checks.
	const int nHide = 4243;
	Check( BkEditorSetHiddenScriptIDs( pSession, &nHide, -1 ) == BK_EDITOR_BAD_ARGUMENT, "a negative count is a bad argument" );
	Check( BkEditorSetHiddenScriptIDs( pSession, 0, 1 ) == BK_EDITOR_BAD_ARGUMENT, "a null list with a count is a bad argument" );
	Check( BkEditorSetHiddenScriptIDs( pSession, &nHide, 1 << 20 ) == BK_EDITOR_BAD_ARGUMENT, "a count no map could need is a bad argument" );
	const int nNobody[3] = { 32000, 31999, 32000 };
	Check( BkEditorSetHiddenScriptIDs( pSession, nNobody, 3 ) == BK_EDITOR_OK, "IDs no object carries (one repeated) are fine" );
	Check( BkEditorSetHiddenScriptIDs( pSession, 0, 0 ) == BK_EDITOR_OK, "and an empty set shows everything" );
	printf( "editor-bridge: M2 hide checked ok\n" );
}

// D-04 (M2): deleting an object other records name is no longer refused; it
// cascades through the start commands (and, with bAllKinds, the reserve
// positions, the targets and the script-ID note), in one step that
// BkEditorRestoreObject undoes exactly. The scratch map is coldwinter with the
// records this test lays over it through NMapRecords, so the expected map is
// built by the same NMapOverlay::DeleteObject on a fresh read of that file.
static void TestM2CascadeDelete( BkEditorSession *pSession, const std::string &szScratch, bool bAllKinds )
{
	// Two units the session can delete: placed by the engine, a link ID of their
	// own, and nothing that refuses a delete (a span, a piece, a passenger).
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo base;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &base, &szError ), szError.c_str() ) )
		return;
	int nU = -1, nV = -1;
	for ( size_t i = 0; i < base.objects.size() && nV < 0; ++i )
	{
		const int nCandidate = base.objects[i].link.nLinkID;
		if ( nCandidate == 0 )
			continue;
		int nSharing = 0;
		for ( size_t j = 0; j < base.objects.size(); ++j )
			nSharing += base.objects[j].link.nLinkID == nCandidate ? 1 : 0;
		for ( size_t j = 0; j < base.scenarioObjects.size(); ++j )
			nSharing += base.scenarioObjects[j].link.nLinkID == nCandidate ? 1 : 0;
		if ( nSharing != 1 )
			continue;
		BkEditorObjectState engineState;
		if ( BkEditorEngineObjectState( pSession, nCandidate, &engineState ) != BK_EDITOR_OK )
			continue;
		CMapInfo copy = base;
		std::string szRefusal;
		if ( !NMapOverlay::DeleteObject( &copy, nCandidate, &szRefusal ) )
			continue;
		( nU < 0 ? nU : nV ) = nCandidate;
	}
	if ( !Check( nU >= 0 && nV >= 0, "coldwinter has two deletable placed objects for the cascade test" ) )
		return;

	// The scratch map: base plus the records that name U and V.
	const std::string szMap = szScratch + "\\cascade-scratch.bzm";
	const std::string szUnedited = szScratch + "\\cascade-unedited.bzm";
	const std::string szEdited = szScratch + "\\cascade-edited.bzm";
	const std::string szUndone = szScratch + "\\cascade-undone.bzm";
	CMapInfo scratch;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &scratch, &szError ), szError.c_str() ) )
		return;
	const size_t nCommandsBefore = scratch.startCommandsList.size();
	const size_t nReservesBefore = scratch.reservePositionsList.size();
	const int nScriptID = 77;
	{
		SAIStartCommand a, b;
		a.unitLinkIDs.push_back( nU );
		b.unitLinkIDs.push_back( nU );
		b.unitLinkIDs.push_back( nV );
		NMapRecords::InsertStartCommand( &scratch, -1, a );
		NMapRecords::InsertStartCommand( &scratch, -1, b );
	}
	if ( bAllKinds )
	{
		// A reserve position holding U as artillery and V as truck, a command of
		// V's with U as its target, and U's script ID held by a group.
		for ( size_t i = 0; i < scratch.objects.size(); ++i )
			if ( !Check( scratch.objects[i].nScriptID != nScriptID, "no object of coldwinter carries script ID 77 already" ) )
				return;
		SAIStartCommand c;
		c.unitLinkIDs.push_back( nV );
		c.linkID = nU;
		Check( NMapRecords::InsertStartCommand( &scratch, -1, c ), "the target command is laid over the map" );
		Check( NMapRecords::InsertReservePosition( &scratch, -1, SBattlePosition( nU, nV, CVec2( 100.0f, 100.0f ) ) ), "the reserve position is laid over the map" );
		Check( NMapRecords::SetObjectScriptID( &scratch, nU, nScriptID ), "U takes script ID 77" );
		Check( NMapRecords::PutReinforcementGroup( &scratch, NMapRecords::FirstFreeGroupID( scratch, 5 ), std::vector<int>( 1, nScriptID ) ), "a group holds it" );
	}
	if ( !Check( NMapFile::Write( szMap.c_str(), scratch, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The delete cascades and says so.
	if ( !Check( BkEditorDeleteObject( pSession, nU ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szMessage = BkEditorLastMessage( pSession );
	printf( "editor-bridge: cascade delete says: %s\n", szMessage.c_str() );
	Check( szMessage.find( "start command" ) != std::string::npos, "the delete says what it changed in the start commands" );
	if ( bAllKinds )
	{
		Check( szMessage.find( "reserve position" ) != std::string::npos, "and the reserve position it erased" );
		Check( szMessage.find( "script ID 77" ) != std::string::npos, "and that script ID 77 is still named by a group" );
	}
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "after the cascade delete: " ) + BkEditorLastMessage( pSession ) ).c_str() );

	// Saved, it is the map the same overlay call builds.
	CMapInfo expected;
	if ( !Check( NMapFile::Read( szMap.c_str(), &expected, &szError ), szError.c_str() ) )
		return;
	std::string szRefusal;
	Check( NMapOverlay::DeleteObject( &expected, nU, &szRefusal ), "the expected map takes the delete" );
	Check( expected.startCommandsList.size() == nCommandsBefore + ( bAllKinds ? 2 : 1 ), "the expected map lost the command that named only U" );
	Check( expected.reservePositionsList.size() == nReservesBefore, "and the reserve position" );
	if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		CMapInfo saved;
		if ( Check( NMapFile::Read( szEdited.c_str(), &saved, &szError ), szError.c_str() ) )
		{
			std::string szWhere;
			Check( NMapFile::AreEquivalent( expected, saved, &szWhere ),
			       szWhere.empty() ? "the saved map equals the expected map" : ( "the cascade save differs at " + szWhere ).c_str() );
			Check( saved.reservePositionsList.size() == nReservesBefore, "the saved map holds no reserve position naming U" );
			if ( bAllKinds )
			{
				Check( !saved.startCommandsList.empty() && saved.startCommandsList.back().linkID == 0, "the saved command's target is link ID 0" );
				Check( saved.reinforcements.groups.size() == expected.reinforcements.groups.size(), "and the group is still saved" );
			}
		}
	}

	// One restore undoes all of it: the unedited bytes.
	Check( BkEditorRestoreObject( pSession, nU ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, ( std::string( "after the restore: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "a cascade delete and its restore save the unedited file byte for byte" );
	const char *const files[] = { szMap.c_str(), szUnedited.c_str(), szEdited.c_str(), szUndone.c_str() };
	for ( size_t i = 0; i < sizeof files / sizeof files[0]; ++i )
		remove( OsPath( files[i] ).c_str() );
	printf( bAllKinds ? "editor-bridge: M2 cascade delete (all kinds) ok\n" : "editor-bridge: M2 cascade delete (start commands) ok\n" );
}

// ---------------------------------------------------------------------------
// 04-05: roads and rivers.
// ---------------------------------------------------------------------------

// The bare descriptor names BkEditorVsoDescriptors lists.
static std::vector<std::string> VsoDescriptorNames( BkEditorSession *pSession, int nKind )
{
	int nCount = 0;
	BkEditorVsoDescriptors( pSession, nKind, 0, 0, &nCount );
	std::vector<BkEditorVsoDescriptor> out( nCount > 0 ? nCount : 1 );
	int nRead = 0;
	std::vector<std::string> names;
	if ( BkEditorVsoDescriptors( pSession, nKind, &out[0], nCount, &nRead ) != BK_EDITOR_OK )
		return names;
	for ( int i = 0; i < nRead; ++i )
		names.push_back( out[i].name );
	return names;
}

static int VsoCountOf( BkEditorSession *pSession, int nKind )
{
	int nCount = -1;
	BkEditorVsoCount( pSession, nKind, &nCount );
	return nCount;
}

// The bridge's add as plain calls on a map read from the file: the expected
// record of an add (CreateVSO, Update( false ), UpdateZ, the road
// passability fix, NextVsoID), appended with InsertVso.
static bool AppendExpectedVso( CMapInfo *pMap, int nKind, const std::string &szName, const std::vector<CVec3> &rControls, float fWidthTiles, float fOpacity )
{
	SVectorStripeObject vso;
	const std::string szDesc = pMap->szSeasonFolder + ( nKind == 0 ? "Roads3D\\" : "Rivers\\" ) + szName;
	if ( !CVSOBuilder::CreateVSO( &vso, szDesc, rControls ) || vso.controlpoints.size() < 2 )
		return false;
	CVSOBuilder::Update( &vso, false, CVSOBuilder::DEFAULT_STEP, fWidthTiles * fWorldCellSize / 2.0f, fOpacity );
	if ( vso.points.size() < 2 )
		return false;
	CVSOBuilder::UpdateZ( pMap->terrain.altitudes, &vso );
	if ( nKind == 0 && vso.fPassability == 0 )
		vso.fPassability = 1;
	vso.nID = NMapRecords::NextVsoID( *pMap );
	return NMapRecords::InsertVso( pMap, nKind == 0 ? NMapRecords::VSO_ROAD : NMapRecords::VSO_RIVER, -1, vso );
}

static bool VsoAgreesWithEngine( BkEditorSession *pSession, const char *pszWhen )
{
	const bool bOk = BkEditorVsoMatchesEngine( pSession ) == BK_EDITOR_OK;
	Check( bOk, NStr::Format( "the engine's roads and rivers match the map %s: %s", pszWhen, BkEditorLastMessage( pSession ) ) );
	return bOk;
}

// The saved map at szPath, read back, equals rExpected.
static void CheckSavedEquals( BkEditorSession *pSession, const std::string &szPath, const CMapInfo &rExpected, const char *pszWhat )
{
	if ( !Check( BkEditorSaveMap( pSession, szPath.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo saved;
	std::string szError;
	if ( !Check( NMapFile::Read( szPath.c_str(), &saved, &szError ), szError.c_str() ) )
		return;
	std::string szWhere;
	Check( NMapFile::AreEquivalent( rExpected, saved, &szWhere ), NStr::Format( "%s: the saved map differs from the expected one at %s", pszWhat, szWhere.c_str() ) );
}

// A few points across the middle of the map, world units: the M2 tests draw
// there. z is left 0; the bridge fits every point to the ground.
static std::vector<CVec3> MiddleLine( const CMapInfo &rMap, float fDx, float fDy )
{
	const float fX = rMap.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f + fDx;
	const float fY = rMap.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f + fDy;
	std::vector<CVec3> line;
	line.push_back( CVec3( fX - 300.0f, fY - 100.0f, 0.0f ) );
	line.push_back( CVec3( fX, fY + 60.0f, 0.0f ) );
	line.push_back( CVec3( fX + 300.0f, fY - 40.0f, 0.0f ) );
	return line;
}

static std::vector<BkEditorVec3> ToCPoints( const std::vector<CVec3> &rPoints )
{
	std::vector<BkEditorVec3> out;
	for ( size_t i = 0; i < rPoints.size(); ++i )
	{
		BkEditorVec3 point = { rPoints[i].x, rPoints[i].y, rPoints[i].z };
		out.push_back( point );
	}
	return out;
}

// D-07/D-03 on the real engine: a road drawn through the bridge is the record
// the MFC tool's builder makes, saved as the expected map; undo gives back the
// unedited file byte for byte and redo the edited one; the engine agrees after
// every step; a road too short to load is refused and changes nothing.
static void TestM2Roads( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	VsoAgreesWithEngine( pSession, "at open" );
	Check( VsoCountOf( pSession, 0 ) == int( original.terrain.roads3.size() ), "the road count is the file's" );
	Check( VsoCountOf( pSession, 1 ) == int( original.terrain.rivers.size() ), "the river count is the file's" );

	const std::vector<std::string> roads = VsoDescriptorNames( pSession, 0 );
	const std::vector<std::string> rivers = VsoDescriptorNames( pSession, 1 );
	if ( !Check( !roads.empty() && !rivers.empty(), NStr::Format( "the season lists road and river types (%d, %d)", int( roads.size() ), int( rivers.size() ) ) ) )
		return;
	Check( std::is_sorted( roads.begin(), roads.end() ), "the road types are sorted" );
	printf( "editor-bridge: %d road types (first %s), %d river types (first %s)\n", int( roads.size() ), roads[0].c_str(), int( rivers.size() ), rivers[0].c_str() );
	{
		// A read of the first road equals the file's record.
		BkEditorVsoInfo info;
		std::vector<BkEditorVec3> controls( 64 );
		std::vector<BkEditorVsoKeyPoint> keys( 64 );
		if ( Check( BkEditorVso( pSession, 0, 0, &info, &controls[0], 64, &keys[0], 64 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			const SVectorStripeObject &rFirst = original.terrain.roads3[0];
			Check( info.saved_id == rFirst.nID && info.control_count == int( rFirst.controlpoints.size() ) && rFirst.szDescName == info.desc,
			       "the first road reads as the file has it" );
			Check( info.control_count > 0 && controls[0].x == rFirst.controlpoints[0].x && controls[0].y == rFirst.controlpoints[0].y, "and its first control point" );
		}
		BkEditorVsoInfo sized;
		Check( BkEditorVso( pSession, 0, 0, &sized, 0, 0, 0, 0 ) == BK_EDITOR_REFUSED && sized.control_count == info.control_count,
		       "a read with no buffers is refused with the counts filled" );
		Check( BkEditorVso( pSession, 0, 999, &sized, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a road past the end is a bad argument" );
		Check( BkEditorVso( pSession, 2, 0, &sized, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "kind 2 is a bad argument" );
	}

	const std::string szUnedited = szScratch + "\\roads-unedited.bzm";
	const std::string szEdited = szScratch + "\\roads-edited.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// Draw a road.
	const std::vector<CVec3> line = MiddleLine( original, 0.0f, 0.0f );
	const std::vector<BkEditorVec3> cLine = ToCPoints( line );
	int nToken = -1, nIndex = -1;
	if ( !Check( BkEditorAddVso( pSession, 0, roads[0].c_str(), &cLine[0], int( cLine.size() ), 3.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( nToken >= 0 && nIndex == int( original.terrain.roads3.size() ), NStr::Format( "the road lands at the end of the list (token %d, index %d)", nToken, nIndex ) );
	VsoAgreesWithEngine( pSession, "after a road was added" );
	CMapInfo expected;
	Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() );
	Check( AppendExpectedVso( &expected, 0, roads[0], line, 3.0f, 1.0f ), "the expected road builds" );
	{
		BkEditorVsoInfo info;
		std::vector<BkEditorVec3> controls( 8 );
		std::vector<BkEditorVsoKeyPoint> keys( 8 );
		if ( Check( BkEditorVso( pSession, 0, nIndex, &info, &controls[0], 8, &keys[0], 8 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			Check( info.saved_id == NMapRecords::NextVsoID( original ), NStr::Format( "the new road's nID is the bridge's own (%d)", info.saved_id ) );
			Check( info.control_count == 3 && info.key_count == 3, "three control points and three key points" );
			Check( std::fabs( keys[0].width - 3.0f * fWorldCellSize / 2.0f ) < 0.01f && keys[0].opacity == 1.0f, "width 3 and full opacity at the key points" );
		}
	}
	CheckSavedEquals( pSession, szEdited, expected, "a new road" );

	// Undo: the unedited file, byte for byte. Redo: the edited map again.
	Check( BkEditorUndoEdit( pSession, nToken + 1 ) == BK_EDITOR_REFUSED, "undo of a token that is not the newest is refused" );
	Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_REFUSED, "redo of an edit that was not undone is refused" );
	if ( Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		VsoAgreesWithEngine( pSession, "after the road's undo" );
		const std::string szUndone = szScratch + "\\roads-undone.bzm";
		if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szUnedited, szUndone ), "a road and its undo save the unedited file byte for byte" );
		remove( OsPath( szUndone ).c_str() );
	}
	if ( Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		VsoAgreesWithEngine( pSession, "after the road's redo" );
		CheckSavedEquals( pSession, szEdited, expected, "the road redone" );
	}
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// Refusals change nothing.
	const int nRoads = VsoCountOf( pSession, 0 );
	std::vector<BkEditorVec3> one( 1, cLine[0] );
	Check( BkEditorAddVso( pSession, 0, roads[0].c_str(), &one[0], 1, 3.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_REFUSED, "a one-point road is refused" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "too short" ) != std::string::npos, NStr::Format( "and says it is too short (%s)", BkEditorLastMessage( pSession ) ) );
	Check( nToken == -1 && nIndex == -1, "a refusal hands out no token" );
	std::vector<BkEditorVec3> close( 2, cLine[0] );
	close[1].x += 1.0f;
	Check( BkEditorAddVso( pSession, 0, roads[0].c_str(), &close[0], 2, 3.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_REFUSED, "two points 1 unit apart are refused" );
	Check( BkEditorAddVso( pSession, 0, "no_such_road", &cLine[0], 3, 3.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_REFUSED, "an unknown road type is refused" );
	Check( BkEditorAddVso( pSession, 0, "..\\roads3d\\x", &cLine[0], 3, 3.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_REFUSED, "a type name with a folder is refused" );
	std::vector<BkEditorVec3> off = cLine;
	off[2].x = -100.0f;
	Check( BkEditorAddVso( pSession, 0, roads[0].c_str(), &off[0], 3, 3.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_REFUSED, "a point off the map is refused" );
	Check( BkEditorAddVso( pSession, 0, roads[0].c_str(), &cLine[0], 3, 17.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "width 17 is a bad argument" );
	Check( BkEditorAddVso( pSession, 0, roads[0].c_str(), &cLine[0], 3, 3.0f, 1.5f, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "opacity 1.5 is a bad argument" );
	Check( BkEditorAddVso( pSession, 2, roads[0].c_str(), &cLine[0], 3, 3.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "kind 2 is a bad argument" );
	std::vector<BkEditorVec3> nan = cLine;
	nan[1].y = std::numeric_limits<float>::quiet_NaN();
	Check( BkEditorAddVso( pSession, 0, roads[0].c_str(), &nan[0], 3, 3.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "a NaN point is a bad argument" );
	Check( VsoCountOf( pSession, 0 ) == nRoads, "none of the refusals added a road" );
	VsoAgreesWithEngine( pSession, "after the refusals" );
	const std::string szRefused = szScratch + "\\roads-refused.bzm";
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szRefused ), "and the map saves unedited byte for byte" );
	remove( OsPath( szRefused ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	printf( "editor-bridge: M2 roads ok\n" );
}

// The first unit the palette places (game type 1, SGVOGT_UNIT), for the
// passability probe.
static std::string FirstPlaceableUnit( BkEditorSession *pSession )
{
	int nCount = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCount );
	std::vector<BkEditorCatalogueEntry> entries( nCount > 0 ? nCount : 1 );
	if ( BkEditorCatalogue( pSession, &entries[0], nCount, &nCount ) != BK_EDITOR_OK )
		return "";
	for ( int i = 0; i < nCount; ++i )
		if ( entries[i].game_type == 1 && entries[i].placeable != 0 )
			return entries[i].name;
	return "";
}

// Whether the AI would take the probe unit at a world point: false while the
// tiles under its rectangle are locked, which is what a river does to them
// (CAIEditor::CanAddObject tests IsRectOnLockedTiles for every class).
static bool ProbeFits( const std::string &szUnit, float fX, float fY )
{
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pAIEditor == 0 )
		return false;
	SMapObjectInfo probe;
	probe.szName = szUnit;
	CVec3 vAI;
	Vis2AI( &vAI, fX, fY, 0.0f );
	probe.vPos = CVec3( vAI.x, vAI.y, 0.0f );
	probe.nDir = 0;
	probe.nPlayer = 0;
	return pAIEditor->CanAddObject( probe );
}

// D-09 (river passability) on the real engine: a river the bridge adds locks
// the AI's tiles under it, its undo unlocks them, its redo locks them again,
// a whole-river delete unlocks them and its undo locks them; a road never
// touches the AI. Everything undone saves the unedited file byte for byte.
// On arnheim, deleting a shipped river unblocks a probe on it and undo blocks
// it again.
static void TestM2Rivers( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnit = FirstPlaceableUnit( pSession );
	const std::vector<std::string> roads = VsoDescriptorNames( pSession, 0 );
	const std::vector<std::string> rivers = VsoDescriptorNames( pSession, 1 );
	if ( !Check( !szUnit.empty() && !roads.empty() && !rivers.empty(), "a probe unit and road and river types" ) )
		return;
	const std::string szUnedited = szScratch + "\\rivers-unedited.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// A point the probe fits on now, with room for the line either side.
	const float fWidth = original.terrain.tiles.GetSizeX() * fWorldCellSize;
	const float fHeight = original.terrain.tiles.GetSizeY() * fWorldCellSize;
	float fProbeX = -1.0f, fProbeY = -1.0f;
	for ( int j = 2; j <= 8 && fProbeX < 0.0f; ++j )
		for ( int i = 2; i <= 8 && fProbeX < 0.0f; ++i )
		{
			const float fX = fWidth * i / 10.0f, fY = fHeight * j / 10.0f;
			if ( ProbeFits( szUnit, fX, fY ) )
			{
				fProbeX = fX;
				fProbeY = fY;
			}
		}
	if ( !Check( fProbeX >= 0.0f, ( "a point of coldwinter where " + szUnit + " fits" ).c_str() ) )
		return;
	printf( "editor-bridge: river probe %s at %.0f,%.0f\n", szUnit.c_str(), fProbeX, fProbeY );
	std::vector<BkEditorVec3> line( 3 );
	for ( int i = 0; i < 3; ++i )
	{
		line[i].x = fProbeX + ( i - 1 ) * 250.0f;
		line[i].y = fProbeY;
		line[i].z = 0.0f;
	}

	// A river: added, undone, redone, deleted, the delete undone, the add undone.
	int nToken = -1, nIndex = -1;
	if ( !Check( BkEditorAddVso( pSession, 1, rivers[0].c_str(), &line[0], 3, 4.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	VsoAgreesWithEngine( pSession, "after a river was added" );
	Check( !ProbeFits( szUnit, fProbeX, fProbeY ), "a new river locks the tiles under it" );
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	VsoAgreesWithEngine( pSession, "after the river's undo" );
	Check( ProbeFits( szUnit, fProbeX, fProbeY ), "its undo unlocks them" );
	Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	VsoAgreesWithEngine( pSession, "after the river's redo" );
	Check( !ProbeFits( szUnit, fProbeX, fProbeY ), "its redo locks them again" );
	int nDelete = -1;
	Check( BkEditorDeleteVso( pSession, 1, nIndex + 1, &nDelete ) == BK_EDITOR_BAD_ARGUMENT, "a river past the end is a bad argument" );
	if ( Check( BkEditorDeleteVso( pSession, 1, nIndex, &nDelete ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		VsoAgreesWithEngine( pSession, "after the river's delete" );
		Check( ProbeFits( szUnit, fProbeX, fProbeY ), "deleting the river unlocks its tiles" );
		Check( VsoCountOf( pSession, 1 ) == int( original.terrain.rivers.size() ), "and it is gone from the list" );
		Check( BkEditorUndoEdit( pSession, nDelete ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		VsoAgreesWithEngine( pSession, "after the delete's undo" );
		Check( !ProbeFits( szUnit, fProbeX, fProbeY ), "the delete's undo locks them again" );
	}
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	VsoAgreesWithEngine( pSession, "after everything was undone" );
	Check( ProbeFits( szUnit, fProbeX, fProbeY ), "and everything undone leaves the tiles free" );

	// A road through the same point never touches the AI.
	int nRoadToken = -1, nRoadIndex = -1;
	if ( Check( BkEditorAddVso( pSession, 0, roads[0].c_str(), &line[0], 3, 4.0f, 1.0f, &nRoadToken, &nRoadIndex ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		Check( ProbeFits( szUnit, fProbeX, fProbeY ), "a road leaves the probe's answer as it was" );
		int nRoadDelete = -1;
		Check( BkEditorDeleteVso( pSession, 0, nRoadIndex, &nRoadDelete ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( ProbeFits( szUnit, fProbeX, fProbeY ), "and so does its delete" );
		VsoAgreesWithEngine( pSession, "after a road's delete" );
		Check( BkEditorUndoEdit( pSession, nRoadDelete ) == BK_EDITOR_OK && BkEditorUndoEdit( pSession, nRoadToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		VsoAgreesWithEngine( pSession, "after the road was undone" );
	}
	const std::string szUndone = szScratch + "\\rivers-undone.bzm";
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "a river and a road, deleted and all undone, save the unedited file byte for byte" );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szUnedited ).c_str() );

	// arnheim: a shipped river.
	CMapInfo bridgeMap;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &bridgeMap, &szError ), szError.c_str() ) || !Check( !bridgeMap.terrain.rivers.empty(), "arnheim has a river" ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szArnheimUnit = FirstPlaceableUnit( pSession );
	VsoAgreesWithEngine( pSession, "on arnheim at open" );
	const std::vector<SVectorStripeObjectPoint> &rPoints = bridgeMap.terrain.rivers[0].points;
	std::vector<int> blocked;
	for ( size_t i = 0; i < rPoints.size(); i += Max( size_t( 1 ), rPoints.size() / 40 ) )
		if ( !ProbeFits( szArnheimUnit, rPoints[i].vPos.x, rPoints[i].vPos.y ) )
			blocked.push_back( int( i ) );
	if ( !Check( !blocked.empty(), "the probe is blocked somewhere on arnheim's first river" ) )
		return;
	int nArnheimToken = -1;
	if ( !Check( BkEditorDeleteVso( pSession, 1, 0, &nArnheimToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	VsoAgreesWithEngine( pSession, "after arnheim's river was deleted" );
	int nFreed = -1;
	for ( size_t k = 0; k < blocked.size() && nFreed < 0; ++k )
		if ( ProbeFits( szArnheimUnit, rPoints[blocked[k]].vPos.x, rPoints[blocked[k]].vPos.y ) )
			nFreed = blocked[k];
	if ( Check( nFreed >= 0, NStr::Format( "deleting the shipped river unblocks a probe on it (%d blocked points tried)", int( blocked.size() ) ) ) )
	{
		Check( BkEditorUndoEdit( pSession, nArnheimToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		VsoAgreesWithEngine( pSession, "after arnheim's river came back" );
		Check( !ProbeFits( szArnheimUnit, rPoints[nFreed].vPos.x, rPoints[nFreed].vPos.y ), "and its undo blocks it again" );
		printf( "editor-bridge: arnheim's river point %d unblocked by the delete, blocked again by the undo\n", nFreed );
	}
	printf( "editor-bridge: M2 rivers and passability ok\n" );
}

// The bridge's edits of an existing record as plain CVSOBuilder calls, for
// the expected maps: resample keeping the key points with the record's own
// first width and opacity, then fit to the ground; after an insert or a
// delete, the MFC order around the key-point backup.
static void ExpectedResample( const CMapInfo &rMap, SVectorStripeObject *pVso )
{
	CVSOBuilder::Update( pVso, true, CVSOBuilder::DEFAULT_STEP, pVso->points[0].fWidth, pVso->points[0].fOpacity );
	CVSOBuilder::UpdateZ( rMap.terrain.altitudes, pVso );
}

static void ExpectedAroundBackup( const CMapInfo &rMap, SVectorStripeObject *pVso, CVSOBuilder::SBackupKeyPoints *pBackup, float fWidth, float fOpacity )
{
	CVSOBuilder::Update( pVso, true, CVSOBuilder::DEFAULT_STEP, fWidth, fOpacity );
	pBackup->LoadKeyPoints( pVso );
	CVSOBuilder::Update( pVso, true, CVSOBuilder::DEFAULT_STEP, fWidth, fOpacity );
	CVSOBuilder::UpdateZ( rMap.terrain.altitudes, pVso );
}

static int ExpectedKeyIndex( const SVectorStripeObject &rVso, int nKey )
{
	int nSeen = 0;
	for ( size_t i = 0; i < rVso.points.size(); ++i )
		if ( rVso.points[i].bKeyPoint && nSeen++ == nKey )
			return int( i );
	return -1;
}

// D-08's edits of a selected road on the real engine: a move of one point and
// of all, a width, an opacity, an insert and point deletes, each the map the
// same CVSOBuilder calls build, the engine agreeing after each; undoing all of
// them saves the unedited file byte for byte.
static void TestM2RoadEdits( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::vector<std::string> roads = VsoDescriptorNames( pSession, 0 );
	if ( !Check( !roads.empty(), "road types for the edits" ) )
		return;
	const std::string szUnedited = szScratch + "\\road-edits-unedited.bzm";
	const std::string szEdited = szScratch + "\\road-edits-edited.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	std::vector<CVec3> line = MiddleLine( original, 0.0f, 300.0f );
	std::vector<BkEditorVec3> cLine = ToCPoints( line );
	std::vector<int> tokens;
	int nToken = -1, nIndex = -1;
	if ( !Check( BkEditorAddVso( pSession, 0, roads[0].c_str(), &cLine[0], 3, 3.0f, 1.0f, &nToken, &nIndex ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	tokens.push_back( nToken );
	CMapInfo expected;
	Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() );
	if ( !Check( AppendExpectedVso( &expected, 0, roads[0], line, 3.0f, 1.0f ), "the expected road builds" ) )
		return;
	SVectorStripeObject &rExpected = expected.terrain.roads3.back();

	// The pick finds it under its middle key point, cycling past any road it
	// overlaps; nothing is off the map.
	{
		const int nMiddle = ExpectedKeyIndex( rExpected, 1 );
		const CVec3 vMiddle = rExpected.points[nMiddle].vPos;
		bool bFound = false;
		for ( int nCycle = 0; nCycle < 8 && !bFound; ++nCycle )
		{
			int nKind = -1, nPicked = -1;
			if ( BkEditorPickVso( pSession, vMiddle.x, vMiddle.y, nCycle, &nKind, &nPicked ) == BK_EDITOR_OK )
				bFound = nKind == 0 && nPicked == nIndex;
		}
		Check( bFound, "the pick finds the new road under its middle" );
		int nKind = 0, nPicked = 0;
		Check( BkEditorPickVso( pSession, -500.0f, -500.0f, 0, &nKind, &nPicked ) == BK_EDITOR_REFUSED && nKind == -1 && nPicked == -1, "nothing is picked off the map" );
	}

	// A move of one point.
	std::vector<CVec3> moved = line;
	moved[1].y += 40.0f;
	std::vector<BkEditorVec3> cMoved = ToCPoints( moved );
	if ( Check( BkEditorMoveVsoPoints( pSession, 0, nIndex, &cMoved[0], 3, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		tokens.push_back( nToken );
		rExpected.controlpoints = moved;
		ExpectedResample( expected, &rExpected );
		VsoAgreesWithEngine( pSession, "after a point was moved" );
		CheckSavedEquals( pSession, szEdited, expected, "a moved point" );
	}
	// A move of all points.
	for ( size_t i = 0; i < moved.size(); ++i )
		moved[i].x += 30.0f;
	cMoved = ToCPoints( moved );
	if ( Check( BkEditorMoveVsoPoints( pSession, 0, nIndex, &cMoved[0], 3, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		tokens.push_back( nToken );
		rExpected.controlpoints = moved;
		ExpectedResample( expected, &rExpected );
		VsoAgreesWithEngine( pSession, "after the whole line was moved" );
		CheckSavedEquals( pSession, szEdited, expected, "a moved line" );
	}
	// A width at key point 1.
	if ( Check( BkEditorSetVsoWidth( pSession, 0, nIndex, 1, 90.0f, 0, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		tokens.push_back( nToken );
		rExpected.points[ExpectedKeyIndex( rExpected, 1 )].fWidth = 90.0f;
		ExpectedResample( expected, &rExpected );
		VsoAgreesWithEngine( pSession, "after a width" );
		CheckSavedEquals( pSession, szEdited, expected, "a width" );
	}
	// An opacity for every point.
	if ( Check( BkEditorSetVsoOpacity( pSession, 0, nIndex, 0, 0.5f, 2, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		tokens.push_back( nToken );
		for ( size_t i = 0; i < rExpected.points.size(); ++i )
			rExpected.points[i].fOpacity = 0.5f;
		VsoAgreesWithEngine( pSession, "after an opacity" );
		CheckSavedEquals( pSession, szEdited, expected, "an opacity" );
	}
	// An insert after control point 0.
	if ( Check( BkEditorInsertVsoPoint( pSession, 0, nIndex, 0, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		tokens.push_back( nToken );
		const int nKeyA = ExpectedKeyIndex( rExpected, 0 ), nKeyB = ExpectedKeyIndex( rExpected, 1 );
		const float fWidth = ( rExpected.points[nKeyA].fWidth + rExpected.points[nKeyB].fWidth ) / 2.0f;
		const float fOpacity = ( rExpected.points[nKeyA].fOpacity + rExpected.points[nKeyB].fOpacity ) / 2.0f;
		const float fRecordWidth = rExpected.points[0].fWidth, fRecordOpacity = rExpected.points[0].fOpacity;
		CVSOBuilder::SBackupKeyPoints backup;
		backup.SaveKeyPoints( rExpected );
		rExpected.controlpoints.insert( rExpected.controlpoints.begin() + 1, ( rExpected.controlpoints[0] + rExpected.controlpoints[1] ) / 2.0f );
		backup.AddKeyPoint( 1, fWidth, fOpacity );
		ExpectedAroundBackup( expected, &rExpected, &backup, fRecordWidth, fRecordOpacity );
		VsoAgreesWithEngine( pSession, "after an insert" );
		CheckSavedEquals( pSession, szEdited, expected, "an insert" );
		BkEditorVsoInfo info;
		Check( BkEditorVso( pSession, 0, nIndex, &info, 0, 0, 0, 0 ) == BK_EDITOR_REFUSED && info.control_count == 4 && info.key_count == 4, "the insert made four control and key points" );
	}
	// Point deletes, down to two, and the third refused.
	for ( int nDelete = 0; nDelete < 2; ++nDelete )
	{
		if ( !Check( BkEditorDeleteVsoPoint( pSession, 0, nIndex, 2 - nDelete, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			break;
		tokens.push_back( nToken );
		const float fRecordWidth = rExpected.points[0].fWidth, fRecordOpacity = rExpected.points[0].fOpacity;
		CVSOBuilder::SBackupKeyPoints backup;
		backup.SaveKeyPoints( rExpected );
		rExpected.controlpoints.erase( rExpected.controlpoints.begin() + ( 2 - nDelete ) );
		backup.RemoveKeyPoint( 2 - nDelete );
		ExpectedAroundBackup( expected, &rExpected, &backup, fRecordWidth, fRecordOpacity );
		VsoAgreesWithEngine( pSession, "after a point delete" );
		CheckSavedEquals( pSession, szEdited, expected, "a point delete" );
	}
	Check( BkEditorDeleteVsoPoint( pSession, 0, nIndex, 0, &nToken ) == BK_EDITOR_REFUSED, "a point delete is refused while only 2 remain" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "at least 2 points" ) != std::string::npos, "and says why" );
	// Bad arguments change nothing.
	Check( BkEditorSetVsoWidth( pSession, 0, nIndex, 0, 0.0f, 0, &nToken ) == BK_EDITOR_BAD_ARGUMENT, "a width of 0 is a bad argument" );
	Check( BkEditorSetVsoWidth( pSession, 0, nIndex, 0, 40.0f, 3, &nToken ) == BK_EDITOR_BAD_ARGUMENT, "mode 3 is a bad argument" );
	Check( BkEditorSetVsoWidth( pSession, 0, nIndex, 99, 40.0f, 0, &nToken ) == BK_EDITOR_BAD_ARGUMENT, "key point 99 is a bad argument" );
	Check( BkEditorSetVsoOpacity( pSession, 0, nIndex, 0, 1.5f, 0, &nToken ) == BK_EDITOR_BAD_ARGUMENT, "opacity 1.5 is a bad argument" );
	Check( BkEditorMoveVsoPoints( pSession, 0, nIndex, &cMoved[0], 3, &nToken ) == BK_EDITOR_BAD_ARGUMENT, "a move naming the wrong point count is a bad argument" );
	Check( BkEditorInsertVsoPoint( pSession, 0, nIndex, 5, &nToken ) == BK_EDITOR_BAD_ARGUMENT, "an insert at control point 5 is a bad argument" );
	std::vector<BkEditorVec3> tooClose = ToCPoints( std::vector<CVec3>( rExpected.controlpoints.begin(), rExpected.controlpoints.end() ) );
	tooClose[1] = tooClose[0];
	tooClose[1].x += 1.0f;
	Check( BkEditorMoveVsoPoints( pSession, 0, nIndex, &tooClose[0], 2, &nToken ) == BK_EDITOR_REFUSED, "a move putting two points 1 unit apart is refused" );
	VsoAgreesWithEngine( pSession, "after the refused edits" );
	CheckSavedEquals( pSession, szEdited, expected, "the refused edits" );

	// Every edit undone, newest first: the unedited file.
	for ( size_t i = tokens.size(); i-- > 0; )
		if ( !Check( BkEditorUndoEdit( pSession, tokens[i] ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			break;
	VsoAgreesWithEngine( pSession, "after every road edit was undone" );
	const std::string szUndone = szScratch + "\\road-edits-undone.bzm";
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "every road edit undone saves the unedited file byte for byte" );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	printf( "editor-bridge: M2 road edits ok (%d edits)\n", int( tokens.size() ) );
}

// CR-A01 (04 review): a file road with fewer than 2 control points loads - the
// game reads only its sampled points - but every resampling edit of it would
// read past the control points (SampleCurve's asserts are compiled out). The
// bridge keeps such a record as read: a move, a width, an opacity, an insert
// and a point delete are refused, the record is unchanged and the map saves as
// it was; deleting the whole record still works.
static void TestM2ShortVsoKeptAsRead( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::vector<std::string> roads = VsoDescriptorNames( pSession, 0 );
	// Not a railroad: the AI builds its railroad graph from the control points
	// at open, so a railroad needs its two points in the game as well.
	std::string szRoad;
	for ( size_t i = 0; i < roads.size() && szRoad.empty(); ++i )
		if ( roads[i].find( "rail" ) == std::string::npos )
			szRoad = roads[i];
	if ( !Check( !szRoad.empty(), "a road type that is not a railroad for the short-road test" ) )
		return;
	if ( !Check( AppendExpectedVso( &map, 0, szRoad, MiddleLine( map, 0.0f, -300.0f ), 3.0f, 1.0f ), "the short road builds" ) )
		return;
	const int nOne = int( map.terrain.roads3.size() ) - 1;
	map.terrain.roads3.back().controlpoints.resize( 1 );
	if ( !Check( AppendExpectedVso( &map, 0, szRoad, MiddleLine( map, 0.0f, 300.0f ), 3.0f, 1.0f ), "the empty road builds" ) )
		return;
	const int nNone = int( map.terrain.roads3.size() ) - 1;
	map.terrain.roads3.back().controlpoints.clear();
	const std::string szMap = szScratch + "\\short-roads.bzm";
	const std::string szSaved = szScratch + "\\short-roads-saved.bzm";
	const std::string szUnedited = szScratch + "\\short-roads-unedited.bzm";
	if ( !Check( NMapFile::Write( szMap.c_str(), map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int indices[2] = { nOne, nNone };
	for ( int n = 0; n < 2; ++n )
	{
		const int nIndex = indices[n];
		const int nControls = n == 0 ? 1 : 0;
		std::vector<BkEditorVec3> moved( 1 );
		moved[0].x = map.terrain.roads3[nIndex].points[0].vPos.x + 40.0f;
		moved[0].y = map.terrain.roads3[nIndex].points[0].vPos.y;
		moved[0].z = 0.0f;
		int nToken = -1;
		const BkEditorStatus move = BkEditorMoveVsoPoints( pSession, 0, nIndex, nControls > 0 ? &moved[0] : 0, nControls, &nToken );
		Check( move == BK_EDITOR_REFUSED && nToken == -1, NStr::Format( "a move of a road with %d control points is refused (%d): %s", nControls, int( move ), BkEditorLastMessage( pSession ) ) );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "kept as read" ) != std::string::npos, "and says it is kept as read" );
		const BkEditorStatus width = BkEditorSetVsoWidth( pSession, 0, nIndex, 0, 90.0f, 0, &nToken );
		Check( width == BK_EDITOR_REFUSED || width == BK_EDITOR_BAD_ARGUMENT, NStr::Format( "a width of a road with %d control points is refused (%d)", nControls, int( width ) ) );
		const BkEditorStatus opacity = BkEditorSetVsoOpacity( pSession, 0, nIndex, 0, 0.5f, 2, &nToken );
		Check( opacity == BK_EDITOR_REFUSED || opacity == BK_EDITOR_BAD_ARGUMENT, NStr::Format( "an opacity of a road with %d control points is refused (%d)", nControls, int( opacity ) ) );
		const BkEditorStatus insert = BkEditorInsertVsoPoint( pSession, 0, nIndex, 0, &nToken );
		Check( insert == BK_EDITOR_REFUSED || insert == BK_EDITOR_BAD_ARGUMENT, NStr::Format( "an insert into a road with %d control points is refused (%d)", nControls, int( insert ) ) );
		const BkEditorStatus erase = BkEditorDeleteVsoPoint( pSession, 0, nIndex, 0, &nToken );
		Check( erase == BK_EDITOR_REFUSED || erase == BK_EDITOR_BAD_ARGUMENT, NStr::Format( "a point delete of a road with %d control points is refused (%d)", nControls, int( erase ) ) );
		BkEditorVsoInfo info;
		Check( BkEditorVso( pSession, 0, nIndex, &info, 0, 0, 0, 0 ) != BK_EDITOR_FAILED && info.control_count == nControls,
		       NStr::Format( "the road still has %d control points", nControls ) );
	}
	if ( Check( BkEditorSaveMap( pSession, szSaved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szSaved ), "the refused edits of the short roads save the unedited file byte for byte" );
	int nToken = -1;
	if ( Check( BkEditorDeleteVso( pSession, 0, nNone, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	remove( OsPath( szMap ).c_str() );
	remove( OsPath( szSaved ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	printf( "editor-bridge: M2 short roads kept as read ok\n" );
}

// ---------------------------------------------------------------------------
// Bridges (04-06)
// ---------------------------------------------------------------------------

static const char *const WOODEN_BRIDGE = "W_WoodenBig_Heavy_01";
static const char *const WOODEN_BRIDGE_ROTATED = "W_WoodenBig_Heavy_02";

// The plan inputs of a bridge type from the object database's own stats, as
// the bridge takes them (session_groups.cpp BridgePlanInputFor): the first
// line span's length, the seeded (seed 0) begin span's origin.
static bool PlanInputFromStats( const char *pszType, NMapGeometry::SBridgePlanInput *pInput )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc *pDesc = pObjectsDB != 0 ? pObjectsDB->GetDesc( pszType ) : 0;
	const SBridgeRPGStats *pStats = pDesc != 0 ? NGDB::GetRPGStats<SBridgeRPGStats>( pObjectsDB, pDesc ) : 0;
	if ( pStats == 0 || pStats->states[0].begins.empty() || pStats->states[0].lines.empty() )
		return false;
	pInput->nDirection = pStats->direction == SBridgeRPGStats::HORIZONTAL ? NMapGeometry::BRIDGE_HORIZONTAL : NMapGeometry::BRIDGE_VERTICAL;
	pInput->fSpanLength = pStats->GetSpanStats( pStats->states[0].lines[0] ).fLength * fWorldCellSize / 2.0f;
	pInput->vBeginOrigin = pStats->GetOrigin( pStats->states[0].begins[0] );
	return true;
}

// A planned bridge laid over a map the way the bridge lays it: one object per
// span (the packed type, HP fHP, no script ID, player 0) with link IDs from
// NextLinkID up, then the entry at nEntryIndex (-1 appends). The link IDs go to
// pLinkIDs when given.
static bool LayBridge( CMapInfo *pMap, const char *pszType, const std::vector<NMapGeometry::SPlannedPiece> &rPlan, float fHP, int nEntryIndex,
                       std::vector<int> *pLinkIDs = 0 )
{
	std::vector<int> linkIDs;
	for ( size_t i = 0; i < rPlan.size(); ++i )
	{
		NMapOverlay::SAddObject add;
		add.szName = pszType;
		add.vPos = rPlan[i].vPos;
		add.nDir = rPlan[i].nDir;
		add.nPlayer = 0;
		add.nFrameIndex = rPlan[i].nPackedType;
		add.fHP = fHP;
		add.nScriptID = -1;
		int nLinkID = -1;
		if ( !NMapOverlay::AddObject( pMap, add, &nLinkID ) )
			return false;
		linkIDs.push_back( nLinkID );
	}
	if ( pLinkIDs != 0 )
		*pLinkIDs = linkIDs;
	return NMapRecords::InsertBridgeEntry( pMap, nEntryIndex, linkIDs );
}

static std::vector<BkEditorBridgeInfo> ReadBridges( BkEditorSession *pSession )
{
	int nCount = 0;
	BkEditorBridges( pSession, 0, 0, &nCount );
	std::vector<BkEditorBridgeInfo> out( nCount > 0 ? nCount : 1 );
	if ( BkEditorBridges( pSession, &out[0], int( out.size() ), &nCount ) != BK_EDITOR_OK && nCount > 0 )
		return std::vector<BkEditorBridgeInfo>();
	out.resize( nCount );
	return out;
}

// Every link of every bridges entry of the saved map at szPath is an object
// the engine holds (the game's LoadBridges dereferences each one).
static bool EveryBridgeLinkIsInTheEngine( BkEditorSession *pSession, const std::string &szPath, const char *pszWhen )
{
	CMapInfo saved;
	std::string szError;
	if ( !Check( BkEditorSaveMap( pSession, szPath.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) ||
	     !Check( NMapFile::Read( szPath.c_str(), &saved, &szError ), szError.c_str() ) )
		return false;
	int nMissing = 0, nLinks = 0;
	for ( size_t i = 0; i < saved.bridges.size(); ++i )
		for ( size_t j = 0; j < saved.bridges[i].size(); ++j, ++nLinks )
		{
			BkEditorObjectState state;
			if ( BkEditorEngineObjectState( pSession, saved.bridges[i][j], &state ) != BK_EDITOR_OK )
				++nMissing;
		}
	remove( OsPath( szPath ).c_str() );
	return Check( nMissing == 0, NStr::Format( "every bridge link names an engine object %s (%d of %d missing)", pszWhen, nMissing, nLinks ) );
}

static bool WorldAgrees( BkEditorSession *pSession, const char *pszWhen )
{
	return Check( BkEditorWorldMatchesMap( pSession ) == BK_EDITOR_OK, NStr::Format( "the world matches the map %s: %s", pszWhen, BkEditorLastMessage( pSession ) ) );
}

// D-10/D-03/C6 on the real engine: a W_WoodenBig_Heavy_01 bridge dragged
// across coldwinter is planned exactly as NMapGeometry::PlanBridge plans it
// with the stats' own inputs, saved as the map the map-file tier's builder
// makes, undone to the unedited bytes and redone; a drag along the wrong axis
// and one off the map are refused with nothing changed.
static void TestM2Bridges( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The types: every bridge of the object database, each _01 with its _02.
	int nTypes = 0;
	BkEditorBridgeDescriptors( pSession, 0, 0, &nTypes );
	std::vector<BkEditorBridgeDescriptor> types( nTypes > 0 ? nTypes : 1 );
	Check( BkEditorBridgeDescriptors( pSession, &types[0], int( types.size() ), &nTypes ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	bool bWooden = false;
	int nWithPartner = 0, nBuildable = 0;
	for ( int i = 0; i < nTypes; ++i )
	{
		nWithPartner += types[i].has_partner;
		nBuildable += types[i].build_during_play_allowed;
		if ( std::string( types[i].name ) == WOODEN_BRIDGE )
			bWooden = types[i].direction == 1 && types[i].has_partner == 1 && types[i].build_during_play_allowed == 1;
	}
	printf( "editor-bridge: %d bridge types, %d with a rotated variant, %d buildable during play\n", nTypes, nWithPartner, nBuildable );
	if ( !Check( bWooden, "W_WoodenBig_Heavy_01 is listed: horizontal, with a partner, buildable during play" ) )
		return;

	NMapGeometry::SBridgePlanInput input;
	if ( !Check( PlanInputFromStats( WOODEN_BRIDGE, &input ), "W_WoodenBig_Heavy_01's stats give the plan inputs" ) )
		return;
	// The literals the map-file tier plans with (map_file_test.cpp) are these.
	Check( input.nDirection == NMapGeometry::BRIDGE_HORIZONTAL && input.fSpanLength == 6.0f * fWorldCellSize / 2.0f &&
	       input.vBeginOrigin.x == 320.0f && input.vBeginOrigin.y == 160.0f,
	       NStr::Format( "the stats agree with the map-file tier's literals (L %g, origin %g, %g)", input.fSpanLength, input.vBeginOrigin.x, input.vBeginOrigin.y ) );

	const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
	const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
	const CVec2 vFirst( fMiddleX - 250.0f, fMiddleY + 250.0f ), vLast( fMiddleX + 250.0f, fMiddleY + 250.0f );
	std::vector<NMapGeometry::SPlannedPiece> plan;
	if ( !Check( NMapGeometry::PlanBridge( input, vFirst, vLast, &plan, &szError ), szError.c_str() ) )
		return;

	// The ghost's plan is the function's.
	{
		int nPlanned = 0;
		std::vector<BkEditorPlannedPiece> pieces( 64 );
		Check( BkEditorPlanBridge( pSession, WOODEN_BRIDGE, vFirst.x, vFirst.y, vLast.x, vLast.y, &pieces[0], 64, &nPlanned ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		bool bSame = nPlanned == int( plan.size() );
		for ( int i = 0; bSame && i < nPlanned; ++i )
			bSame = pieces[i].x == plan[i].vPos.x && pieces[i].y == plan[i].vPos.y && pieces[i].type == plan[i].nPackedType && pieces[i].dir == plan[i].nDir;
		Check( bSame, NStr::Format( "BkEditorPlanBridge plans what PlanBridge plans (%d spans)", nPlanned ) );
		Check( BkEditorPlanBridge( pSession, WOODEN_BRIDGE, vFirst.x, vFirst.y, vLast.x, vLast.y, 0, 0, &nPlanned ) == BK_EDITOR_REFUSED && nPlanned == int( plan.size() ),
		       "a plan with no buffer is refused with the count filled" );
	}

	const std::string szUnedited = szScratch + "\\bridges-unedited.bzm";
	const std::string szEdited = szScratch + "\\bridges-edited.bzm";
	const std::string szCheck = szScratch + "\\bridges-check.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int nObjectsBefore = int( ReadObjectRecords( pSession ).size() );

	int nToken = -1, nIndex = -1;
	if ( !Check( BkEditorDrawBridge( pSession, WOODEN_BRIDGE, vFirst.x, vFirst.y, vLast.x, vLast.y, &nToken, &nIndex ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( nToken >= 0 && nIndex == int( original.bridges.size() ), NStr::Format( "the bridge's entry is appended (token %d, index %d)", nToken, nIndex ) );
	Check( int( ReadObjectRecords( pSession ).size() ) == nObjectsBefore + int( plan.size() ), "one object per span" );
	WorldAgrees( pSession, "after a bridge was drawn" );
	EveryBridgeLinkIsInTheEngine( pSession, szCheck, "after a bridge was drawn" );
	{
		const std::vector<BkEditorBridgeInfo> infos = ReadBridges( pSession );
		if ( Check( int( infos.size() ) == nIndex + 1, "BkEditorBridges lists the new entry" ) )
		{
			const BkEditorBridgeInfo &rInfo = infos[nIndex];
			Check( std::string( rInfo.desc ) == WOODEN_BRIDGE && rInfo.span_count == int( plan.size() ) && rInfo.built_during_play == 0 &&
			       rInfo.min_x == plan.front().vPos.x && rInfo.max_x == plan.back().vPos.x && rInfo.min_y == plan.front().vPos.y,
			       "with its type, span count and box" );
		}
	}
	CMapInfo expected;
	Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() );
	Check( LayBridge( &expected, WOODEN_BRIDGE, plan, 1.0f, -1 ), "the expected bridge lays over the file's map" );
	CheckSavedEquals( pSession, szEdited, expected, "a new bridge" );

	// Undo: the unedited bytes; redo: the expected map again.
	if ( Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		WorldAgrees( pSession, "after the bridge's undo" );
		Check( int( ReadObjectRecords( pSession ).size() ) == nObjectsBefore, "the undo takes every span out" );
		const std::string szUndone = szScratch + "\\bridges-undone.bzm";
		if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szUnedited, szUndone ), "a bridge and its undo save the unedited file byte for byte" );
		remove( OsPath( szUndone ).c_str() );
	}
	if ( Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		WorldAgrees( pSession, "after the bridge's redo" );
		EveryBridgeLinkIsInTheEngine( pSession, szCheck, "after the bridge's redo" );
		CheckSavedEquals( pSession, szEdited, expected, "the bridge redone" );
	}
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// Refusals change nothing: the objects, the entries, the engine, the file.
	const size_t nBridges = ReadBridges( pSession ).size();
	nToken = nIndex = 7;
	Check( BkEditorDrawBridge( pSession, WOODEN_BRIDGE, fMiddleX, fMiddleY - 250.0f, fMiddleX + 10.0f, fMiddleY + 250.0f, &nToken, &nIndex ) == BK_EDITOR_REFUSED,
	       "a vertical drag of the horizontal _01 is refused" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "horizontally" ) != std::string::npos, NStr::Format( "and says so (%s)", BkEditorLastMessage( pSession ) ) );
	Check( nToken == -1 && nIndex == -1, "a refusal hands out no token" );
	const float fEdge = original.terrain.tiles.GetSizeX() * fWorldCellSize;
	Check( BkEditorDrawBridge( pSession, WOODEN_BRIDGE, fEdge - 200.0f, fMiddleY, fEdge + 600.0f, fMiddleY, &nToken, &nIndex ) == BK_EDITOR_REFUSED,
	       "a bridge running off the map's edge is refused" );
	printf( "editor-bridge: off the edge: %s\n", BkEditorLastMessage( pSession ) );
	Check( BkEditorDrawBridge( pSession, "no_such_bridge", vFirst.x, vFirst.y, vLast.x, vLast.y, &nToken, &nIndex ) == BK_EDITOR_REFUSED, "an unknown type is refused" );
	Check( BkEditorDrawBridge( pSession, "10.5-cm_Flak38", vFirst.x, vFirst.y, vLast.x, vLast.y, &nToken, &nIndex ) == BK_EDITOR_REFUSED, "a type that is not a bridge is refused" );
	Check( BkEditorDrawBridge( pSession, WOODEN_BRIDGE, std::numeric_limits<float>::quiet_NaN(), vFirst.y, vLast.x, vLast.y, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "a NaN drag is a bad argument" );
	Check( BkEditorDrawBridge( pSession, 0, vFirst.x, vFirst.y, vLast.x, vLast.y, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "a null type is a bad argument" );
	Check( ReadBridges( pSession ).size() == nBridges, "no refusal added an entry" );
	Check( int( ReadObjectRecords( pSession ).size() ) == nObjectsBefore, "nor an object" );
	WorldAgrees( pSession, "after the refusals" );
	EveryBridgeLinkIsInTheEngine( pSession, szCheck, "after the refusals" );
	const std::string szRefused = szScratch + "\\bridges-refused.bzm";
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szRefused ), "and the map saves unedited byte for byte" );
	remove( OsPath( szRefused ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	printf( "editor-bridge: M2 bridges draw ok\n" );
}

// ---------------------------------------------------------------------------
// Fences (04-07)
// ---------------------------------------------------------------------------

static const char *const FACTORY_FENCE = "W_FactoryFence";

// The plan inputs of a fence type as the object database's stats give them,
// read straight (dirs[d].centers[0]) where the bridge goes through the seeded
// GetCenterIndex: the two must agree (the seed 0 pick is the first).
static bool FenceInputFromStats( const char *pszType, const CMapInfo &rMap, NMapGeometry::SFencePlanInput *pInput )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc *pDesc = pObjectsDB != 0 ? pObjectsDB->GetDesc( pszType ) : 0;
	const SFenceRPGStats *pStats = pDesc != 0 ? NGDB::GetRPGStats<SFenceRPGStats>( pObjectsDB, pDesc ) : 0;
	if ( pStats == 0 || pStats->dirs.size() < 4 )
		return false;
	for ( int nDir = 0; nDir < 4; ++nDir )
	{
		if ( pStats->dirs[nDir].centers.empty() )
			return false;
		pInput->vOrigin[nDir] = pStats->GetOrigin( pStats->dirs[nDir].centers[0] );
	}
	pInput->nTilesX = rMap.terrain.patches.GetSizeX() * 32;
	pInput->nTilesY = rMap.terrain.patches.GetSizeY() * 32;
	return true;
}

// A planned fence run laid over a map the way the bridge lays it: one plain
// object per fence (the packed type, HP 1, no script ID, player 0), link IDs
// from NextLinkID up, no bridges entry. The link IDs go to pLinkIDs.
static bool LayFences( CMapInfo *pMap, const char *pszType, const std::vector<NMapGeometry::SPlannedPiece> &rPlan, std::vector<int> *pLinkIDs, int nFirstLinkID = -1 )
{
	pLinkIDs->clear();
	for ( size_t i = 0; i < rPlan.size(); ++i )
	{
		NMapOverlay::SAddObject add;
		add.szName = pszType;
		add.vPos = rPlan[i].vPos;
		add.nDir = rPlan[i].nDir;
		add.nPlayer = 0;
		add.nFrameIndex = rPlan[i].nPackedType;
		add.fHP = 1.0f;
		add.nScriptID = -1;
		// A later run's IDs start at the session's floor, not at the file's next:
		// an ID an undone edit held is never handed out again.
		add.nLinkID = nFirstLinkID < 0 ? -1 : nFirstLinkID + int( i );
		int nLinkID = -1;
		if ( !NMapOverlay::AddObject( pMap, add, &nLinkID ) )
			return false;
		pLinkIDs->push_back( nLinkID );
	}
	return true;
}

// D-14/D-03/C6 on the real engine: the tile mapping is the engine's own; a
// fence run dragged across coldwinter is planned exactly as
// NMapGeometry::PlanFences plans it with the stats' inputs, saved as the map
// the map-file tier's builder makes, and the placed fences are ordinary
// objects (one moved, one deleted, both put back); undone to the unedited
// bytes and redone; a run off the map and the wrong types are refused with
// nothing changed.
static void TestM2Fences( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The constants the geometry unit repeats are the engine's.
	Check( NMapGeometry::nAITileSize == int( SAIConsts::TILE_SIZE ), "MapGeometry's AI tile size is SAIConsts::TILE_SIZE" );
	Check( NMapGeometry::nFenceTypeNormal == int( SFenceRPGStats::FENCE_TYPE_NORMAL ), "MapGeometry's normal fence type is FENCE_TYPE_NORMAL" );

	// The tile mapping (research Q6): BkEditorWorldToAITile is the engine's
	// GetAITileIndex. It agrees with the static CMapInfo::GetAITileIndices at
	// the tile corners - the corners of the map and its centre, five points -
	// and NOT in between: the engine rounds half a cell, the static helper
	// truncates. The MFC tool uses the engine's, so the tool does.
	{
		const float fAITile = fWorldCellSize / 2.0f;
		const float fW = original.terrain.tiles.GetSizeX() * fWorldCellSize, fH = original.terrain.tiles.GetSizeY() * fWorldCellSize;
		const float points[5][2] = { { 0.0f, 0.0f }, { fW, 0.0f }, { 0.0f, fH }, { fW, fH }, { float( int( fW / fAITile ) / 2 ) * fAITile, float( int( fH / fAITile ) / 2 ) * fAITile } };
		int nAgree = 0;
		for ( int i = 0; i < 5; ++i )
		{
			int nX = -1, nY = -1;
			BkEditorWorldToAITile( pSession, points[i][0], points[i][1], &nX, &nY );
			CTPoint<int> viaMap;
			CMapInfo::GetAITileIndices( original.terrain, CVec3( points[i][0], points[i][1], 0.0f ), &viaMap );
			if ( nX == viaMap.x && nY == viaMap.y )
				++nAgree;
			else
				printf( "editor-bridge: AI tile at (%g, %g): engine %d,%d, CMapInfo %d,%d\n", points[i][0], points[i][1], nX, nY, viaMap.x, viaMap.y );
		}
		Check( nAgree == 5, NStr::Format( "BkEditorWorldToAITile equals CMapInfo::GetAITileIndices at five tile corners (%d of 5)", nAgree ) );
		int nX = -1, nY = -1;
		const float fInside = 5.0f * fAITile + 0.7f * fAITile;
		Check( BkEditorWorldToAITile( pSession, fInside, fInside, &nX, &nY ) == BK_EDITOR_OK && nX == 6 && nY == 6, NStr::Format( "the engine rounds 5.7 tiles to 6 (%d, %d)", nX, nY ) );
		CTPoint<int> viaMap;
		CMapInfo::GetAITileIndices( original.terrain, CVec3( fInside, fInside, 0.0f ), &viaMap );
		Check( viaMap.x == 5 && viaMap.y == 5, NStr::Format( "where CMapInfo truncates it to 5 (%d, %d): the two differ between corners", viaMap.x, viaMap.y ) );
		Check( BkEditorWorldToAITile( pSession, -50.0f, 10.0f, &nX, &nY ) == BK_EDITOR_REFUSED && nX < 0, "a point left of the map has a tile and is refused" );
		Check( BkEditorWorldToAITile( pSession, 0.0f, 0.0f, 0, &nY ) == BK_EDITOR_BAD_ARGUMENT, "a null out is a bad argument" );
	}

	// The types.
	int nTypes = 0;
	BkEditorFenceDescriptors( pSession, 0, 0, &nTypes );
	std::vector<BkEditorFenceDescriptor> types( nTypes > 0 ? nTypes : 1 );
	Check( BkEditorFenceDescriptors( pSession, &types[0], int( types.size() ), &nTypes ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	bool bFactory = false;
	for ( int i = 0; i < nTypes; ++i )
		bFactory = bFactory || std::string( types[i].name ) == FACTORY_FENCE;
	printf( "editor-bridge: %d fence types\n", nTypes );
	if ( !Check( bFactory, "W_FactoryFence is listed" ) )
		return;
	// A click of every type: each plans one fence or is refused with a reason,
	// none crashes (T-04-07-01: no type reaches an index helper with an empty list).
	{
		int nPlanned = 0, nRefused = 0;
		const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
		const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
		for ( int i = 0; i < nTypes; ++i )
		{
			BkEditorPlannedPiece piece;
			int nCount = 0;
			const BkEditorStatus status = BkEditorPlanFences( pSession, types[i].name, fMiddleX, fMiddleY, fMiddleX, fMiddleY, 0, &piece, 1, &nCount );
			if ( status == BK_EDITOR_OK && nCount == 1 )
				++nPlanned;
			else if ( status == BK_EDITOR_REFUSED )
				++nRefused;
			else
				Check( false, NStr::Format( "%s: a click plans one fence or is refused, not status %d", types[i].name, int( status ) ) );
		}
		printf( "editor-bridge: a click of each fence type: %d planned one fence, %d refused\n", nPlanned, nRefused );
		Check( nPlanned > 0, "some fence type plans a click" );
	}

	NMapGeometry::SFencePlanInput input;
	if ( !Check( FenceInputFromStats( FACTORY_FENCE, original, &input ), "W_FactoryFence's stats give the plan inputs" ) )
		return;
	// The literals the map-file tier plans with (map_file_test.cpp) are these.
	{
		bool bLiterals = input.nTilesX == original.terrain.patches.GetSizeX() * 32;
		const float want[4][2] = { { 16.0f, 16.0f }, { 80.0f, 16.0f }, { 16.0f, 16.0f }, { 80.0f, 16.0f } };
		for ( int nDir = 0; nDir < 4; ++nDir )
			bLiterals = bLiterals && fabsf( input.vOrigin[nDir].x - want[nDir][0] ) < 0.01f && fabsf( input.vOrigin[nDir].y - want[nDir][1] ) < 0.01f;
		Check( bLiterals, NStr::Format( "the stats agree with the map-file tier's literals (dir 0 %g, %g; dir 1 %g, %g; %d tiles)",
		                                input.vOrigin[0].x, input.vOrigin[0].y, input.vOrigin[1].x, input.vOrigin[1].y, input.nTilesX ) );
	}

	const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
	const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
	// The bridge test's empty snow, along x: a rightward run.
	const CVec2 vFirst( fMiddleX - 250.0f, fMiddleY + 250.0f ), vLast( fMiddleX + 250.0f, fMiddleY + 250.0f );
	int nFirstX = 0, nFirstY = 0, nLastX = 0, nLastY = 0;
	BkEditorWorldToAITile( pSession, vFirst.x, vFirst.y, &nFirstX, &nFirstY );
	BkEditorWorldToAITile( pSession, vLast.x, vLast.y, &nLastX, &nLastY );
	std::vector<NMapGeometry::SPlannedPiece> plan;
	if ( !Check( NMapGeometry::PlanFences( input, CTPoint<int>( nFirstX, nFirstY ), CTPoint<int>( nLastX, nLastY ), false, &plan, &szError ), szError.c_str() ) )
		return;
	printf( "editor-bridge: the fence run covers AI tiles %d..%d in x at %d: %d fences\n", nFirstX, nLastX, nFirstY, int( plan.size() ) );

	// The ghost's plan is the function's.
	{
		int nPlanned = 0;
		std::vector<BkEditorPlannedPiece> pieces( 256 );
		Check( BkEditorPlanFences( pSession, FACTORY_FENCE, vFirst.x, vFirst.y, vLast.x, vLast.y, 0, &pieces[0], 256, &nPlanned ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		bool bSame = nPlanned == int( plan.size() );
		for ( int i = 0; bSame && i < nPlanned; ++i )
			bSame = pieces[i].x == plan[i].vPos.x && pieces[i].y == plan[i].vPos.y && pieces[i].type == plan[i].nPackedType && pieces[i].dir == plan[i].nDir;
		Check( bSame, NStr::Format( "BkEditorPlanFences plans what PlanFences plans (%d fences)", nPlanned ) );
		Check( BkEditorPlanFences( pSession, FACTORY_FENCE, vFirst.x, vFirst.y, vLast.x, vLast.y, 0, 0, 0, &nPlanned ) == BK_EDITOR_REFUSED && nPlanned == int( plan.size() ),
		       "a plan with no buffer is refused with the count filled" );
		// A single fence follows ctrl.
		BkEditorPlannedPiece one;
		int nOne = 0;
		Check( BkEditorPlanFences( pSession, FACTORY_FENCE, fMiddleX, fMiddleY, fMiddleX, fMiddleY, 0, &one, 1, &nOne ) == BK_EDITOR_OK && nOne == 1 && one.type == ( 1 | 0x00010000 ),
		       "a click plans one fence, direction 0" );
		Check( BkEditorPlanFences( pSession, FACTORY_FENCE, fMiddleX, fMiddleY, fMiddleX, fMiddleY, 1, &one, 1, &nOne ) == BK_EDITOR_OK && nOne == 1 && one.type == ( 2 | 0x00010000 ),
		       "and with ctrl, direction 1" );
	}

	const std::string szUnedited = szScratch + "\\fences-unedited.bzm";
	const std::string szEdited = szScratch + "\\fences-edited.bzm";
	const std::string szCheck = szScratch + "\\fences-check.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const size_t nObjectsBefore = ReadObjectRecords( pSession ).size();
	const size_t nBridgesBefore = ReadBridges( pSession ).size();

	int nToken = -1;
	if ( !Check( BkEditorDrawFences( pSession, FACTORY_FENCE, vFirst.x, vFirst.y, vLast.x, vLast.y, 0, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( nToken >= 0, NStr::Format( "a run hands out its token (%d)", nToken ) );
	const std::vector<BkEditorObjectRecord> after = ReadObjectRecords( pSession );
	Check( after.size() == nObjectsBefore + plan.size(), "one object per fence" );
	Check( ReadBridges( pSession ).size() == nBridgesBefore, "and no bridges entry" );
	WorldAgrees( pSession, "after a fence run was drawn" );
	std::vector<int> fenceLinks;
	for ( size_t i = nObjectsBefore; i < after.size(); ++i )
		fenceLinks.push_back( after[i].link_id );
	{
		int nMissing = 0;
		for ( size_t i = 0; i < fenceLinks.size(); ++i )
		{
			BkEditorObjectState state;
			if ( BkEditorEngineObjectState( pSession, fenceLinks[i], &state ) != BK_EDITOR_OK )
				++nMissing;
		}
		Check( nMissing == 0, NStr::Format( "every fence is an engine object (%d of %d missing)", nMissing, int( fenceLinks.size() ) ) );
	}
	CMapInfo expected;
	std::vector<int> expectedLinks;
	Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() );
	Check( LayFences( &expected, FACTORY_FENCE, plan, &expectedLinks ), "the expected run lays over the file's map" );
	Check( expectedLinks == fenceLinks, "and takes the link IDs the bridge gave" );
	CheckSavedEquals( pSession, szEdited, expected, "a fence run" );

	// Ordinary objects: one fence moves, another deletes and comes back.
	if ( fenceLinks.size() >= 3 )
	{
		const int nMoved = fenceLinks[1], nDeleted = fenceLinks[2];
		const BkEditorObjectRecord &rMoved = after[nObjectsBefore + 1];
		if ( Check( BkEditorMoveObject( pSession, nMoved, rMoved.x, rMoved.y + 64.0f ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
		     Check( BkEditorDeleteObject( pSession, nDeleted ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			WorldAgrees( pSession, "after a fence was moved and another deleted" );
			CMapInfo saved;
			const std::string szMoved = szScratch + "\\fences-moved.bzm";
			if ( Check( BkEditorSaveMap( pSession, szMoved.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
			     Check( NMapFile::Read( szMoved.c_str(), &saved, &szError ), szError.c_str() ) )
			{
				bool bMoved = false, bGone = true;
				for ( size_t i = 0; i < saved.objects.size(); ++i )
				{
					if ( saved.objects[i].link.nLinkID == nMoved )
						bMoved = saved.objects[i].vPos.x == rMoved.x && saved.objects[i].vPos.y == rMoved.y + 64.0f &&
						         saved.objects[i].nFrameIndex == plan[1].nPackedType;
					if ( saved.objects[i].link.nLinkID == nDeleted )
						bGone = false;
				}
				Check( bMoved, "the moved fence is saved where it was moved, its packed type kept" );
				Check( bGone, "the deleted fence is gone from the saved map" );
			}
			remove( OsPath( szMoved ).c_str() );
		}
		Check( BkEditorMoveObject( pSession, nMoved, rMoved.x, rMoved.y ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BkEditorRestoreObject( pSession, nDeleted ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		WorldAgrees( pSession, "after the move and the delete were undone" );
		CheckSavedEquals( pSession, szEdited, expected, "the run after a fence's move and delete undone" );
	}

	// Undo: the unedited bytes; redo: the expected map again.
	if ( Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		WorldAgrees( pSession, "after the run's undo" );
		Check( ReadObjectRecords( pSession ).size() == nObjectsBefore, "the undo takes every fence out" );
		const std::string szUndone = szScratch + "\\fences-undone.bzm";
		if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szUnedited, szUndone ), "a fence run and its undo save the unedited file byte for byte" );
		remove( OsPath( szUndone ).c_str() );
	}
	if ( Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		WorldAgrees( pSession, "after the run's redo" );
		CheckSavedEquals( pSession, szEdited, expected, "the fence run redone" );
	}
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// Other directions and a single fence, each drawn and undone to the same bytes.
	// The link IDs of the run undone above are not handed out again (the
	// session's floor), so each expected run starts after the last one's.
	{
		int nFloor = fenceLinks.back() + 1;
		const struct { float x0, y0, x1, y1; int ctrl; const char *pszWhat; } runs[] = {
			{ fMiddleX + 250.0f, fMiddleY + 250.0f, fMiddleX - 250.0f, fMiddleY + 250.0f, 0, "a leftward run" },
			{ fMiddleX + 100.0f, fMiddleY + 60.0f, fMiddleX + 100.0f, fMiddleY + 330.0f, 0, "a downward run" },
			{ fMiddleX + 100.0f, fMiddleY + 330.0f, fMiddleX + 100.0f, fMiddleY + 60.0f, 0, "an upward run" },
			{ fMiddleX + 60.0f, fMiddleY + 200.0f, fMiddleX + 60.0f, fMiddleY + 200.0f, 0, "a single fence" },
			{ fMiddleX + 60.0f, fMiddleY + 200.0f, fMiddleX + 60.0f, fMiddleY + 200.0f, 1, "a single flipped fence" },
		};
		for ( size_t i = 0; i < sizeof runs / sizeof runs[0]; ++i )
		{
			int nRunToken = -1;
			int nFrom = 0, nFromY = 0, nTo = 0, nToY = 0;
			BkEditorWorldToAITile( pSession, runs[i].x0, runs[i].y0, &nFrom, &nFromY );
			BkEditorWorldToAITile( pSession, runs[i].x1, runs[i].y1, &nTo, &nToY );
			std::vector<NMapGeometry::SPlannedPiece> runPlan;
			if ( !Check( NMapGeometry::PlanFences( input, CTPoint<int>( nFrom, nFromY ), CTPoint<int>( nTo, nToY ), runs[i].ctrl != 0, &runPlan, &szError ), szError.c_str() ) )
				continue;
			if ( !Check( BkEditorDrawFences( pSession, FACTORY_FENCE, runs[i].x0, runs[i].y0, runs[i].x1, runs[i].y1, runs[i].ctrl, &nRunToken ) == BK_EDITOR_OK,
			             NStr::Format( "%s: %s", runs[i].pszWhat, BkEditorLastMessage( pSession ) ) ) )
				continue;
			CMapInfo runExpected;
			std::vector<int> runLinks;
			NMapFile::Read( SHIPPED_MAP, &runExpected, &szError );
			Check( LayFences( &runExpected, FACTORY_FENCE, runPlan, &runLinks, nFloor ), "the expected run lays" );
			nFloor += int( runPlan.size() );
			CheckSavedEquals( pSession, szEdited, runExpected, runs[i].pszWhat );
			WorldAgrees( pSession, runs[i].pszWhat );
			Check( BkEditorUndoEdit( pSession, nRunToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			const std::string szBack = szScratch + "\\fences-back.bzm";
			if ( Check( BkEditorSaveMap( pSession, szBack.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
				Check( SameBytes( szUnedited, szBack ), NStr::Format( "%s undone saves the unedited bytes (%d fences)", runs[i].pszWhat, int( runPlan.size() ) ) );
			remove( OsPath( szBack ).c_str() );
		}
	}

	// Refusals change nothing: the objects, the engine, the file.
	nToken = 7;
	const float fEdge = original.terrain.tiles.GetSizeX() * fWorldCellSize;
	Check( BkEditorDrawFences( pSession, FACTORY_FENCE, fEdge - 200.0f, fMiddleY, fEdge + 600.0f, fMiddleY, 0, &nToken ) == BK_EDITOR_REFUSED,
	       "a run with an end past the map's edge is refused" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "leaves the map" ) != std::string::npos, NStr::Format( "and says so (%s)", BkEditorLastMessage( pSession ) ) );
	Check( nToken == -1, "a refusal hands out no token" );
	// The last fence of a rightward run is moved two AI tiles on: a run whose
	// end tile is the map's last is refused as a whole though its end is on it.
	{
		int nEdgeX = 0, nEdgeY = 0;
		BkEditorWorldToAITile( pSession, fEdge - 30.0f, fMiddleY, &nEdgeX, &nEdgeY );
		const float fEdgeWorld = fEdge - 30.0f;
		Check( BkEditorDrawFences( pSession, FACTORY_FENCE, fEdgeWorld - 100.0f, fMiddleY, fEdgeWorld, fMiddleY, 0, &nToken ) == BK_EDITOR_REFUSED,
		       NStr::Format( "a rightward run ending on the map's last tile (%d) is refused whole: its last fence is moved past the edge", nEdgeX ) );
	}
	Check( BkEditorDrawFences( pSession, "no_such_fence", vFirst.x, vFirst.y, vLast.x, vLast.y, 0, &nToken ) == BK_EDITOR_REFUSED, "an unknown type is refused" );
	Check( BkEditorDrawFences( pSession, WOODEN_BRIDGE, vFirst.x, vFirst.y, vLast.x, vLast.y, 0, &nToken ) == BK_EDITOR_REFUSED, "a bridge type is refused" );
	Check( BkEditorDrawFences( pSession, "10.5-cm_Flak38", vFirst.x, vFirst.y, vLast.x, vLast.y, 0, &nToken ) == BK_EDITOR_REFUSED, "a type that is not a fence is refused" );
	Check( BkEditorDrawFences( pSession, FACTORY_FENCE, std::numeric_limits<float>::quiet_NaN(), vFirst.y, vLast.x, vLast.y, 0, &nToken ) == BK_EDITOR_BAD_ARGUMENT, "a NaN drag is a bad argument" );
	Check( BkEditorDrawFences( pSession, 0, vFirst.x, vFirst.y, vLast.x, vLast.y, 0, &nToken ) == BK_EDITOR_BAD_ARGUMENT, "a null type is a bad argument" );
	Check( ReadObjectRecords( pSession ).size() == nObjectsBefore, "no refusal added an object" );
	WorldAgrees( pSession, "after the refusals" );
	const std::string szRefused = szScratch + "\\fences-refused.bzm";
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szRefused ), "and the map saves unedited byte for byte" );
	remove( OsPath( szRefused ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szCheck ).c_str() );
	printf( "editor-bridge: M2 fences ok\n" );
}

// The screen point a shipped bridge's span is picked at: the camera on the
// span, then its world point on screen, tried as it is and a little above and
// below (a bridge's sprite need not cover its own ground point). -1 when no
// offset picks the span's bridge.
static bool PickBridgeAt( BkEditorSession *pSession, float fWorldX, float fWorldY, int nWanted, float *pfSx, float *pfSy )
{
	if ( BkEditorSetCamera( pSession, fWorldX, fWorldY ) != BK_EDITOR_OK || BkEditorFrame( pSession ) != BK_EDITOR_OK )
		return false;
	float fSx = 0.0f, fSy = 0.0f;
	if ( BkEditorWorldToScreen( pSession, fWorldX, fWorldY, &fSx, &fSy ) != BK_EDITOR_OK )
		return false;
	const float offsets[] = { 0.0f, -8.0f, 8.0f, -16.0f, 16.0f, -24.0f };
	for ( size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i )
	{
		int nKind = -1, nIndex = -1;
		if ( BkEditorPickGroup( pSession, fSx, fSy + offsets[i], &nKind, &nIndex ) == BK_EDITOR_OK && nKind == 1 && nIndex == nWanted )
		{
			*pfSx = fSx;
			*pfSy = fSy + offsets[i];
			return true;
		}
	}
	return false;
}

// D-11 on the real engine: arnheim's first bridge is picked from a screen
// point over one of its spans (as a group, where BkEditorObjectAt passes the
// span over), deleted whole - the save equals the file's map with the entry
// erased and its spans deleted, every other bridge's links still resolve -
// and its undo saves the unedited bytes.
static void TestM2BridgeDelete( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( original.bridges.size() >= 2, "arnheim has at least two bridges" ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\bridge-delete-unedited.bzm";
	const std::string szEdited = szScratch + "\\bridge-delete-edited.bzm";
	const std::string szCheck = szScratch + "\\bridge-delete-check.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::vector<BkEditorBridgeInfo> infos = ReadBridges( pSession );
	Check( infos.size() == original.bridges.size(), NStr::Format( "BkEditorBridges lists the file's %d bridges", int( original.bridges.size() ) ) );

	// A span of bridge 0 (the middle one), on screen.
	const int nBridge = 0;
	const std::vector<int> &rEntry = original.bridges[nBridge];
	const SMapObjectInfo *pSpan = 0;
	for ( size_t i = 0; i < original.objects.size() && pSpan == 0; ++i )
		if ( original.objects[i].link.nLinkID == rEntry[rEntry.size() / 2] )
			pSpan = &original.objects[i];
	if ( !Check( pSpan != 0, "the bridge's middle span is in the map" ) )
		return;
	const float fWorldX = pSpan->vPos.x * fAITileXCoeff, fWorldY = pSpan->vPos.y * fAITileYCoeff;
	float fSx = 0.0f, fSy = 0.0f;
	if ( !Check( PickBridgeAt( pSession, fWorldX, fWorldY, nBridge, &fSx, &fSy ), "BkEditorPickGroup finds bridge 0 over its span" ) )
		return;
	printf( "editor-bridge: bridge %d (%s, %d spans) picked at %.0f,%.0f\n", nBridge, infos.empty() ? "?" : infos[nBridge].desc, int( rEntry.size() ), fSx, fSy );
	{
		// BkEditorObjectAt keeps its meaning: it passes the span over.
		int nLinkID = -1;
		const BkEditorStatus status = BkEditorObjectAt( pSession, fSx, fSy, &nLinkID );
		bool bSpan = false;
		for ( size_t b = 0; b < original.bridges.size(); ++b )
			bSpan = bSpan || std::find( original.bridges[b].begin(), original.bridges[b].end(), nLinkID ) != original.bridges[b].end();
		Check( status == BK_EDITOR_REFUSED || ( status == BK_EDITOR_OK && !bSpan ), "BkEditorObjectAt at the same point answers no span" );
	}

	int nToken = -1;
	if ( !Check( BkEditorDeleteBridge( pSession, nBridge, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( int( ReadBridges( pSession ).size() ) == int( original.bridges.size() ) - 1, "the entry is gone" );
	WorldAgrees( pSession, "after a bridge was deleted" );
	EveryBridgeLinkIsInTheEngine( pSession, szCheck, "after a bridge was deleted" );
	{
		int nKind = -1, nIndex = -1;
		const BkEditorStatus status = BkEditorPickGroup( pSession, fSx, fSy, &nKind, &nIndex );
		Check( status == BK_EDITOR_REFUSED || nIndex != nBridge || nKind != 1 || ReadBridges( pSession ).size() < original.bridges.size(),
		       "the deleted bridge is not picked any more" );
	}
	CMapInfo expected;
	Check( NMapFile::Read( BRIDGE_MAP, &expected, &szError ), szError.c_str() );
	std::vector<int> erased;
	Check( NMapRecords::EraseBridgeEntry( &expected, nBridge, &erased ), "the expected map loses the entry" );
	for ( size_t i = erased.size(); i-- > 0; )
	{
		std::string szRefusal;
		Check( NMapOverlay::DeleteObject( &expected, erased[i], &szRefusal ), szRefusal.c_str() );
	}
	CheckSavedEquals( pSession, szEdited, expected, "a deleted bridge" );

	if ( Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		WorldAgrees( pSession, "after the delete's undo" );
		EveryBridgeLinkIsInTheEngine( pSession, szCheck, "after the delete's undo" );
		const std::string szUndone = szScratch + "\\bridge-delete-undone.bzm";
		if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szUnedited, szUndone ), "a bridge's delete and its undo save the unedited file byte for byte" );
		remove( OsPath( szUndone ).c_str() );
		int nKind = -1, nIndex = -1;
		Check( BkEditorPickGroup( pSession, fSx, fSy, &nKind, &nIndex ) == BK_EDITOR_OK && nKind == 1 && nIndex == nBridge, "and the bridge is picked again" );
	}
	if ( Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		CheckSavedEquals( pSession, szEdited, expected, "the delete redone" );
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	}
	Check( BkEditorDeleteBridge( pSession, int( original.bridges.size() ), &nToken ) == BK_EDITOR_BAD_ARGUMENT, "a bridge past the end is a bad argument" );
	Check( BkEditorDeleteObject( pSession, rEntry[0] ) == BK_EDITOR_REFUSED, "a span alone is still refused to the object delete" );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	printf( "editor-bridge: M2 bridge delete ok\n" );
}

// WR-A02 (04 review): a bridge whose span another bridges entry also names,
// or whose entry names a span twice, is kept as read. Before the fix the
// delete erased the entry and took the span out of the engine before the map
// refused it, leaving the engine short of an object the saved map names; a
// span named twice was placed twice and orphaned the first engine object.
static void TestM2SharedBridgeLinksKeptAsRead( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( map.bridges.size() >= 2 && !map.bridges[0].empty() && !map.bridges[1].empty(), "arnheim has two bridges with spans" ) )
		return;
	const int nShared = map.bridges[0][0];
	const int nTwice = map.bridges[1][0];
	map.bridges.push_back( std::vector<int>( 1, nShared ) );		// a second entry names bridge 0's first span
	map.bridges[1].push_back( nTwice );							// bridge 1 names its first span twice
	const int nLast = int( map.bridges.size() ) - 1;
	const std::string szMap = szScratch + "\\shared-bridge-links.bzm";
	const std::string szUnedited = szScratch + "\\shared-bridge-links-unedited.bzm";
	const std::string szAfter = szScratch + "\\shared-bridge-links-after.bzm";
	const std::string szCheck = szScratch + "\\shared-bridge-links-check.bzm";
	if ( !Check( NMapFile::Write( szMap.c_str(), map, &szError ), szError.c_str() ) )
		return;
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( !Check( BkEditorOpenMap( pSession, szMap.c_str(), &summary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( summary.bridge_span_placed == summary.bridge_span_count,
	       NStr::Format( "a span named twice by one entry is one span, placed once (%d of %d)", summary.bridge_span_placed, summary.bridge_span_count ) );
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int wanted[3] = { 0, nLast, 1 };
	for ( int n = 0; n < 3; ++n )
	{
		int nToken = -1;
		const BkEditorStatus deleted = BkEditorDeleteBridge( pSession, wanted[n], &nToken );
		Check( deleted == BK_EDITOR_REFUSED && nToken == -1,
		       NStr::Format( "the delete of bridge %d, whose span is shared or repeated, is refused (%d): %s", wanted[n], int( deleted ), BkEditorLastMessage( pSession ) ) );
		const BkEditorStatus rotated = BkEditorRotateBridge( pSession, wanted[n], &nToken );
		Check( rotated == BK_EDITOR_REFUSED, NStr::Format( "the rotate of bridge %d is refused (%d)", wanted[n], int( rotated ) ) );
	}
	EveryBridgeLinkIsInTheEngine( pSession, szCheck, "after the refused deletes of shared spans" );
	if ( Check( BkEditorSaveMap( pSession, szAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szAfter ), "the refused deletes of shared spans save the unedited file byte for byte" );
	remove( OsPath( szMap ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szAfter ).c_str() );
	printf( "editor-bridge: M2 shared bridge links kept as read ok\n" );
}

// WR-A07 (04 review): the build-during-play toggle of a WoodenBig_Heavy bridge
// whose span another entry also names is refused, like its delete and rotate;
// before the fix it flipped the HP through whichever record held the link.
static void TestM2BridgeToggleSharedRefused( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	NMapGeometry::SBridgePlanInput input;
	if ( !Check( PlanInputFromStats( WOODEN_BRIDGE, &input ), "WoodenBig_Heavy gives plan inputs" ) )
		return;
	const float fMiddleX = map.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
	const float fMiddleY = map.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
	std::vector<NMapGeometry::SPlannedPiece> plan;
	if ( !Check( NMapGeometry::PlanBridge( input, CVec2( fMiddleX - 250.0f, fMiddleY - 250.0f ), CVec2( fMiddleX + 250.0f, fMiddleY - 250.0f ), &plan, &szError ), szError.c_str() ) )
		return;
	std::vector<int> linkIDs;
	if ( !Check( LayBridge( &map, WOODEN_BRIDGE, plan, 1.0f, -1, &linkIDs ) && !linkIDs.empty(), "the WoodenBig bridge is laid" ) )
		return;
	const int nWood = int( map.bridges.size() ) - 1;
	map.bridges.push_back( std::vector<int>( 1, linkIDs[0] ) );
	const std::string szMap = szScratch + "\\bridge-toggle-shared.bzm";
	const std::string szUnedited = szScratch + "\\bridge-toggle-shared-unedited.bzm";
	const std::string szAfter = szScratch + "\\bridge-toggle-shared-after.bzm";
	if ( !Check( NMapFile::Write( szMap.c_str(), map, &szError ), szError.c_str() ) ||
	     !Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) ||
	     !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nToken = -1;
	const BkEditorStatus toggled = BkEditorToggleBridgeBuild( pSession, nWood, &nToken );
	Check( toggled == BK_EDITOR_REFUSED && nToken == -1, NStr::Format( "the toggle of a bridge whose span another entry names is refused (%d): %s", int( toggled ), BkEditorLastMessage( pSession ) ) );
	if ( Check( BkEditorSaveMap( pSession, szAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szAfter ), "the refused toggle saves the unedited file byte for byte" );
	remove( OsPath( szMap ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szAfter ).c_str() );
	printf( "editor-bridge: M2 bridge toggle of a shared span refused ok\n" );
}

// WR-A08 (04 review): the plan inputs now check every begin, line and end
// index and every span's segments. Every shipped bridge type still plans: none
// is refused for naming a span or segment it does not have.
static void TestM2EveryBridgeTypePlans( BkEditorSession *pSession )
{
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	int nCount = 0;
	BkEditorBridgeDescriptors( pSession, 0, 0, &nCount );
	std::vector<BkEditorBridgeDescriptor> descs( nCount > 0 ? nCount : 1 );
	if ( !Check( nCount > 0 && BkEditorBridgeDescriptors( pSession, &descs[0], nCount, &nCount ) == BK_EDITOR_OK, "the bridge types read" ) )
		return;
	int nPlanned = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		int nPieces = 0;
		const BkEditorStatus status = BkEditorPlanBridge( pSession, descs[i].name, 1000.0f, 1000.0f, 1500.0f, 1000.0f, 0, 0, &nPieces );
		const std::string szWhy = BkEditorLastMessage( pSession );
		Check( szWhy.find( "does not have" ) == std::string::npos, NStr::Format( "bridge type %s plans (%d): %s", descs[i].name, int( status ), szWhy.c_str() ) );
		if ( nPieces > 0 )
			++nPlanned;
	}
	Check( nPlanned > 0, "at least one bridge type plans a horizontal drag" );
	// WR-A09: a gesture coordinate past +-1e6 is a bad argument before any
	// float reaches an int conversion.
	int nPieces = 0;
	Check( BkEditorPlanBridge( pSession, descs[0].name, 1000.0f, 1000.0f, 1.0e30f, 1000.0f, 0, 0, &nPieces ) == BK_EDITOR_BAD_ARGUMENT, "a bridge drag to 1e30 is a bad argument" );
	Check( BkEditorPlanFences( pSession, "x", 1000.0f, 1000.0f, 1000.0f, -1.0e30f, 0, 0, 0, &nPieces ) == BK_EDITOR_BAD_ARGUMENT, "a fence drag to -1e30 is a bad argument" );
	BkEditorScriptAreaRecord area;
	Check( BkEditorScriptAreaFromVis( pSession, 1, 100.0f, 100.0f, 1.0e30f, 100.0f, "huge", &area ) == BK_EDITOR_BAD_ARGUMENT, "an area drag to 1e30 is a bad argument" );
	// Not "small": the Windows SDK defines it as a macro (rpcndr.h).
	BkEditorScriptAreaRecord saneArea;
	if ( Check( BkEditorScriptAreaFromVis( pSession, 1, 100.0f, 100.0f, 200.0f, 100.0f, "sane", &saneArea ) == BK_EDITOR_OK, "a sane area drag converts" ) )
	{
		Check( BkEditorScriptAreaMoved( pSession, &saneArea, 1.0e30f, 0.0f, &area ) == BK_EDITOR_BAD_ARGUMENT, "an area moved to 1e30 is a bad argument" );
		Check( BkEditorScriptAreaResized( pSession, &saneArea, 0.0f, 1.0e30f, &area ) == BK_EDITOR_BAD_ARGUMENT, "an area resized to 1e30 is a bad argument" );
	}
	printf( "editor-bridge: every bridge type's plan inputs check out (%d types, %d planned a horizontal drag)\n", nCount, nPlanned );
}

// The screen box of a bridge (map-unit box from BkEditorBridges, grown by
// half a tile in world units) with the camera on its centre, and a capture of
// the frame. False when a corner does not convert.
struct SScreenBox { int nLeft, nTop, nRight, nBottom; };

static bool CaptureBridge( BkEditorSession *pSession, const BkEditorBridgeInfo &rInfo, const std::string &szPath, SScreenBox *pBox,
                           std::vector<unsigned char> *pPixels, int *pnWidth, int *pnHeight )
{
	const float fMargin = fWorldCellSize / 2.0f;
	const float fMinX = rInfo.min_x * fAITileXCoeff - fMargin, fMaxX = rInfo.max_x * fAITileXCoeff + fMargin;
	const float fMinY = rInfo.min_y * fAITileYCoeff - fMargin, fMaxY = rInfo.max_y * fAITileYCoeff + fMargin;
	if ( BkEditorSetCamera( pSession, ( fMinX + fMaxX ) / 2.0f, ( fMinY + fMaxY ) / 2.0f ) != BK_EDITOR_OK || BkEditorFrame( pSession ) != BK_EDITOR_OK )
		return false;
	const float corners[4][2] = { { fMinX, fMinY }, { fMaxX, fMinY }, { fMaxX, fMaxY }, { fMinX, fMaxY } };
	float fLeft = 1e9f, fTop = 1e9f, fRight = -1e9f, fBottom = -1e9f;
	for ( int k = 0; k < 4; ++k )
	{
		float fSx = 0.0f, fSy = 0.0f;
		if ( BkEditorWorldToScreen( pSession, corners[k][0], corners[k][1], &fSx, &fSy ) != BK_EDITOR_OK )
			return false;
		fLeft = Min( fLeft, fSx );
		fRight = Max( fRight, fSx );
		fTop = Min( fTop, fSy );
		fBottom = Max( fBottom, fSy );
	}
	// A bridge's sprite stands above its ground box a little.
	pBox->nLeft = int( fLeft );
	pBox->nRight = int( fRight ) + 1;
	pBox->nTop = int( fTop ) - 32;
	pBox->nBottom = int( fBottom ) + 1;
	if ( !SaveFrame( pSession, szPath ) )
		return false;
	*pPixels = ReadFramePixels( szPath, pnWidth, pnHeight );
	return !pPixels->empty();
}

static int BoxArea( const SScreenBox &rBox, int nWidth, int nHeight )
{
	return Max( 0, Min( nWidth, rBox.nRight ) - Max( 0, rBox.nLeft ) ) * Max( 0, Min( nHeight, rBox.nBottom ) - Max( 0, rBox.nTop ) );
}

// D-11/D-12 with C1 on the real engine: a W_WoodenBig_Heavy_01 bridge rotated
// becomes a W_WoodenBig_Heavy_02 bridge of the same span count about the same
// centre (saved as the map the shared geometry builds), toggled built during
// play (every span -1 in the file, the engine's picture visibly marked, the
// mark measured) and untoggled; a rotation off the map and a toggle of
// another type are refused; everything undone saves the unedited bytes.
static void TestM2BridgeRotateToggle( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\bridge-rotate-unedited.bzm";
	const std::string szEdited = szScratch + "\\bridge-rotate-edited.bzm";
	const std::string szCheck = szScratch + "\\bridge-rotate-check.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	NMapGeometry::SBridgePlanInput input01, input02;
	if ( !Check( PlanInputFromStats( WOODEN_BRIDGE, &input01 ) && PlanInputFromStats( WOODEN_BRIDGE_ROTATED, &input02 ), "both WoodenBig_Heavy variants give plan inputs" ) )
		return;
	Check( input02.nDirection == NMapGeometry::BRIDGE_VERTICAL, "W_WoodenBig_Heavy_02 runs vertically" );

	const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
	const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
	const CVec2 vFirst( fMiddleX - 250.0f, fMiddleY + 250.0f ), vLast( fMiddleX + 250.0f, fMiddleY + 250.0f );
	std::vector<NMapGeometry::SPlannedPiece> plan01;
	if ( !Check( NMapGeometry::PlanBridge( input01, vFirst, vLast, &plan01, &szError ), szError.c_str() ) )
		return;
	std::vector<int> tokens;
	int nToken = -1, nIndex = -1;
	if ( !Check( BkEditorDrawBridge( pSession, WOODEN_BRIDGE, vFirst.x, vFirst.y, vLast.x, vLast.y, &nToken, &nIndex ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	tokens.push_back( nToken );

	// Rotate.
	if ( !Check( BkEditorRotateBridge( pSession, nIndex, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	tokens.push_back( nToken );
	WorldAgrees( pSession, "after a rotate" );
	EveryBridgeLinkIsInTheEngine( pSession, szCheck, "after a rotate" );
	std::vector<BkEditorBridgeInfo> infos = ReadBridges( pSession );
	if ( !Check( nIndex < int( infos.size() ), "the rotated bridge keeps its index" ) )
		return;
	const BkEditorBridgeInfo rotated = infos[nIndex];
	const float fOldCentreX = ( plan01.front().vPos.x + plan01.back().vPos.x ) / 2.0f, fOldCentreY = ( plan01.front().vPos.y + plan01.back().vPos.y ) / 2.0f;
	const float fNewCentreX = ( rotated.min_x + rotated.max_x ) / 2.0f, fNewCentreY = ( rotated.min_y + rotated.max_y ) / 2.0f;
	const float fSpanAI = input02.fSpanLength / fAITileXCoeff;
	Check( std::string( rotated.desc ) == WOODEN_BRIDGE_ROTATED && rotated.span_count == int( plan01.size() ) && rotated.min_x == rotated.max_x && rotated.max_y > rotated.min_y,
	       NStr::Format( "the entry names %d spans of W_WoodenBig_Heavy_02 along y (%s, %d)", int( plan01.size() ), rotated.desc, rotated.span_count ) );
	Check( std::fabs( fNewCentreX - fOldCentreX ) <= fSpanAI && std::fabs( fNewCentreY - fOldCentreY ) <= fSpanAI,
	       NStr::Format( "about the old centre within one span length (%.1f,%.1f vs %.1f,%.1f map units, span %.1f)", fNewCentreX, fNewCentreY, fOldCentreX, fOldCentreY, fSpanAI ) );
	// The expected map: the same geometry, the drawn bridge's spans taken out
	// and the rotated ones in, the entry at the same index.
	CMapInfo expected;
	Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() );
	std::vector<int> drawnLinks;
	Check( LayBridge( &expected, WOODEN_BRIDGE, plan01, 1.0f, -1, &drawnLinks ), "the drawn bridge lays over the expected map" );
	{
		CVec3 vCentre;
		AI2Vis( &vCentre, fOldCentreX, fOldCentreY, 0.0f );
		CVec2 vRotFirst, vRotLast;
		NMapGeometry::RotatedBridgeDrag( input02, CVec2( vCentre.x, vCentre.y ), int( plan01.size() ), &vRotFirst, &vRotLast );
		std::vector<NMapGeometry::SPlannedPiece> plan02;
		Check( NMapGeometry::PlanBridge( input02, vRotFirst, vRotLast, &plan02, &szError ), szError.c_str() );
		Check( plan02.size() == plan01.size(), "the rotated plan has the same span count" );
		// The rotated spans take the IDs after the drawn ones: the bridge's floor
		// is above every ID it handed out.
		const int nFirstNew = drawnLinks.back() + 1;
		Check( NMapRecords::EraseBridgeEntry( &expected, int( expected.bridges.size() ) - 1 ), "the expected map loses the drawn entry" );
		for ( size_t i = drawnLinks.size(); i-- > 0; )
		{
			std::string szRefusal;
			Check( NMapOverlay::DeleteObject( &expected, drawnLinks[i], &szRefusal ), szRefusal.c_str() );
		}
		std::vector<int> rotatedLinks;
		for ( size_t i = 0; i < plan02.size(); ++i )
		{
			NMapOverlay::SAddObject add;
			add.szName = WOODEN_BRIDGE_ROTATED;
			add.vPos = plan02[i].vPos;
			add.nFrameIndex = plan02[i].nPackedType;
			add.nLinkID = nFirstNew + int( i );
			int nLinkID = -1;
			Check( NMapOverlay::AddObject( &expected, add, &nLinkID ), "a rotated span lays over the expected map" );
			rotatedLinks.push_back( nLinkID );
		}
		Check( NMapRecords::InsertBridgeEntry( &expected, nIndex, rotatedLinks ), "and the rotated entry at the same index" );
	}
	CheckSavedEquals( pSession, szEdited, expected, "a rotated bridge" );

	// Toggle built during play, and measure the mark.
	SScreenBox box;
	std::vector<unsigned char> before, marked, unmarked;
	int nWidth = 0, nHeight = 0;
	const std::string szShotBefore = szScratch + "/bridge-toggle-before.tga";
	const std::string szShotMarked = szScratch + "/bridge-toggle-marked.tga";
	const std::string szShotUndone = szScratch + "/bridge-toggle-undone.tga";
	Check( CaptureBridge( pSession, rotated, szShotBefore, &box, &before, &nWidth, &nHeight ), "the bridge is captured before the toggle" );
	if ( !Check( BkEditorToggleBridgeBuild( pSession, nIndex, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	tokens.push_back( nToken );
	const int nToggleToken = nToken;
	Check( ReadBridges( pSession )[nIndex].built_during_play == 1, "BkEditorBridges says built during play" );
	{
		CMapInfo saved;
		if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
		     Check( NMapFile::Read( szEdited.c_str(), &saved, &szError ), szError.c_str() ) && Check( nIndex < int( saved.bridges.size() ), "the saved map has the entry" ) )
		{
			bool bAllNegative = !saved.bridges[nIndex].empty();
			for ( size_t i = 0; i < saved.bridges[nIndex].size(); ++i )
			{
				const SMapObjectInfo *pSpan = 0;
				for ( size_t k = 0; k < saved.objects.size() && pSpan == 0; ++k )
					if ( saved.objects[k].link.nLinkID == saved.bridges[nIndex][i] )
						pSpan = &saved.objects[k];
				bAllNegative = bAllNegative && pSpan != 0 && pSpan->fHP == -1.0f;
			}
			Check( bAllNegative, "the saved HP of every span is -1" );
			for ( size_t i = 0; i < expected.bridges[nIndex].size(); ++i )
				NMapRecords::SetObjectHP( &expected, expected.bridges[nIndex][i], -1.0f );
			std::string szWhere;
			Check( NMapFile::AreEquivalent( expected, saved, &szWhere ), NStr::Format( "the toggled map reads back as expected (%s)", szWhere.c_str() ) );
		}
	}
	WorldAgrees( pSession, "after a toggle" );
	Check( CaptureBridge( pSession, rotated, szShotMarked, &box, &marked, &nWidth, &nHeight ), "the bridge is captured marked" );
	const int nArea = BoxArea( box, nWidth, nHeight );
	const int nMarked = ChangedPixels( before, marked, nWidth, nHeight, box.nLeft, box.nTop, box.nRight, box.nBottom );
	printf( "editor-bridge: the built-during-play mark changes %d of %d pixels (%.2f %%) of the bridge's box %d,%d-%d,%d\n", nMarked, nArea,
	        nArea > 0 ? 100.0f * nMarked / nArea : 0.0f, box.nLeft, box.nTop, box.nRight, box.nBottom );
	Check( nArea > 0 && nMarked * 100 > nArea, "the mark changes more than 1 % of the bridge's box" );
	if ( Check( BkEditorUndoEdit( pSession, nToggleToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		tokens.pop_back();
		Check( ReadBridges( pSession )[nIndex].built_during_play == 0, "the toggle's undo: intact again" );
		Check( CaptureBridge( pSession, rotated, szShotUndone, &box, &unmarked, &nWidth, &nHeight ), "the bridge is captured after the undo" );
		const int nBack = ChangedPixels( before, unmarked, nWidth, nHeight, box.nLeft, box.nTop, box.nRight, box.nBottom );
		printf( "editor-bridge: after the toggle's undo %d of %d pixels (%.2f %%) differ from before\n", nBack, nArea, nArea > 0 ? 100.0f * nBack / nArea : 0.0f );
		Check( nBack * 200 <= nArea, "the capture is back within 0.5 %" );
		// Redo and undo once more: the mark comes back with the redo.
		Check( BkEditorRedoEdit( pSession, nToggleToken ) == BK_EDITOR_OK && BkEditorUndoEdit( pSession, nToggleToken ) == BK_EDITOR_OK, "the toggle redoes and undoes" );
	}

	// Refusals.
	const int nRefusedBridges = int( ReadBridges( pSession ).size() );
	int nOther = -1, nOtherToken = -1;
	if ( Check( BkEditorDrawBridge( pSession, "W_WoodenLittle_01", fMiddleX - 250.0f, fMiddleY - 250.0f, fMiddleX + 250.0f, fMiddleY - 250.0f, &nOtherToken, &nOther ) == BK_EDITOR_OK,
	            BkEditorLastMessage( pSession ) ) )
	{
		tokens.push_back( nOtherToken );
		Check( BkEditorToggleBridgeBuild( pSession, nOther, &nToken ) == BK_EDITOR_REFUSED, "toggling a W_WoodenLittle bridge is refused" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "WoodenBig_Heavy" ) != std::string::npos, BkEditorLastMessage( pSession ) );
	}
	// A long horizontal bridge near the map's low-y edge: upright about its
	// centre it would reach past the edge.
	const float fLong = 7.3f * input01.fSpanLength;
	int nEdge = -1, nEdgeToken = -1;
	if ( Check( BkEditorDrawBridge( pSession, WOODEN_BRIDGE, fMiddleX - fLong / 2.0f, 300.0f, fMiddleX + fLong / 2.0f, 300.0f, &nEdgeToken, &nEdge ) == BK_EDITOR_OK,
	            BkEditorLastMessage( pSession ) ) )
	{
		tokens.push_back( nEdgeToken );
		const int nObjects = int( ReadObjectRecords( pSession ).size() );
		Check( BkEditorRotateBridge( pSession, nEdge, &nToken ) == BK_EDITOR_REFUSED, "a rotation that would leave the map is refused" );
		printf( "editor-bridge: rotation off the map: %s\n", BkEditorLastMessage( pSession ) );
		Check( int( ReadObjectRecords( pSession ).size() ) == nObjects && ReadBridges( pSession )[nEdge].span_count > 0 &&
		       std::string( ReadBridges( pSession )[nEdge].desc ) == WOODEN_BRIDGE, "and the bridge stays as it was" );
		WorldAgrees( pSession, "after a refused rotation" );
		EveryBridgeLinkIsInTheEngine( pSession, szCheck, "after a refused rotation" );
	}
	Check( nRefusedBridges + 2 == int( ReadBridges( pSession ).size() ), "the refusals added nothing beyond the two drawn bridges" );

	// Everything undone: the unedited bytes.
	for ( size_t i = tokens.size(); i-- > 0; )
		Check( BkEditorUndoEdit( pSession, tokens[i] ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	WorldAgrees( pSession, "after every undo" );
	const std::string szUndone = szScratch + "\\bridge-rotate-undone.bzm";
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "a draw, rotate, toggle and the refusals undone save the unedited file byte for byte" );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	printf( "editor-bridge: M2 bridge rotate and toggle ok\n" );
}

// ---------------------------------------------------------------------------
// Entrenchments (04-08)
// ---------------------------------------------------------------------------

// The builder's inputs as the object database's "Entrenchment" stats give
// them, read straight (lines[0], arcs[0]).
static bool TrenchInputFromStats( NMapGeometry::STrenchPlanInput *pInput )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc *pDesc = pObjectsDB != 0 ? pObjectsDB->GetDesc( "Entrenchment" ) : 0;
	const SEntrenchmentRPGStats *pStats = pDesc != 0 ? NGDB::GetRPGStats<SEntrenchmentRPGStats>( pObjectsDB, pDesc ) : 0;
	if ( pStats == 0 || pStats->lines.empty() || pStats->arcs.empty() )
		return false;
	pInput->fLineWidth = pStats->segments[pStats->lines[0]].GetVisAABBHalfSize().x * 2.0f;
	pInput->fArcWidth = pStats->segments[pStats->arcs[0]].GetVisAABBHalfSize().x * 2.0f;
	return true;
}

// The map's extent the bridge gives the builder, map units.
static void TrenchExtent( const CMapInfo &rMap, NMapGeometry::STrenchPlanInput *pInput )
{
	pInput->fMapWidth = float( rMap.terrain.patches.GetSizeX() * 32 * NMapGeometry::nAITileSize );
	pInput->fMapHeight = float( rMap.terrain.patches.GetSizeY() * 32 * NMapGeometry::nAITileSize );
}

// The L the map-file tier draws: 600 world units east, then 500 north.
static std::vector<CVec2> EngineTrenchL( float fX, float fY )
{
	std::vector<CVec2> points;
	points.push_back( CVec2( fX, fY ) );
	points.push_back( CVec2( fX + 600.0f, fY ) );
	points.push_back( CVec2( fX + 600.0f, fY + 500.0f ) );
	return points;
}

static std::vector<BkEditorVec3> ToCPoints2( const std::vector<CVec2> &rPoints )
{
	std::vector<BkEditorVec3> out;
	for ( size_t i = 0; i < rPoints.size(); ++i )
	{
		BkEditorVec3 point = { rPoints[i].x, rPoints[i].y, 0.0f };
		out.push_back( point );
	}
	return out;
}

// A planned entrenchment laid over a map the way the bridge lays it: one
// "Entrenchment" object per piece in the plan's order (the packed type, HP 1,
// no script ID, the player), link IDs from NextLinkID up, then the entry of
// the sections' link IDs at nEntryIndex (-1 appends).
static bool LayTrench( CMapInfo *pMap, const NMapGeometry::STrenchPlan &rPlan, int nPlayer, int nEntryIndex = -1 )
{
	std::vector<int> linkIDs;
	for ( size_t i = 0; i < rPlan.pieces.size(); ++i )
	{
		NMapOverlay::SAddObject add;
		add.szName = "Entrenchment";
		add.vPos = rPlan.pieces[i].vPos;
		add.nDir = rPlan.pieces[i].nDir;
		add.nPlayer = nPlayer;
		add.nFrameIndex = rPlan.pieces[i].nPackedType;
		add.fHP = 1.0f;
		add.nScriptID = -1;
		int nLinkID = -1;
		if ( !NMapOverlay::AddObject( pMap, add, &nLinkID ) )
			return false;
		linkIDs.push_back( nLinkID );
	}
	SEntrenchmentInfo entry;
	for ( size_t s = 0; s < rPlan.sections.size(); ++s )
	{
		SEntrenchmentInfo::TSegment section;
		for ( size_t k = 0; k < rPlan.sections[s].size(); ++k )
			section.push_back( linkIDs[rPlan.sections[s][k]] );
		entry.sections.push_back( section );
	}
	return NMapRecords::InsertEntrenchment( pMap, nEntryIndex, entry );
}

static std::vector<BkEditorEntrenchmentInfo> ReadTrenches( BkEditorSession *pSession )
{
	int nCount = 0;
	BkEditorEntrenchments( pSession, 0, 0, &nCount );
	std::vector<BkEditorEntrenchmentInfo> out( nCount > 0 ? nCount : 1 );
	if ( BkEditorEntrenchments( pSession, &out[0], int( out.size() ), &nCount ) != BK_EDITOR_OK && nCount > 0 )
		return std::vector<BkEditorEntrenchmentInfo>();
	out.resize( nCount );
	return out;
}

// Every section of every entrenchment of the saved map at szPath is non-empty
// and every link it names is an object the engine holds (the game's
// LoadEntrenchments takes each section's first and dereferences each link).
static bool EveryTrenchLinkIsInTheEngine( BkEditorSession *pSession, const std::string &szPath, const char *pszWhen )
{
	CMapInfo saved;
	std::string szError;
	if ( !Check( BkEditorSaveMap( pSession, szPath.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) ||
	     !Check( NMapFile::Read( szPath.c_str(), &saved, &szError ), szError.c_str() ) )
		return false;
	int nMissing = 0, nLinks = 0, nEmpty = 0;
	for ( size_t t = 0; t < saved.entrenchments.size(); ++t )
		for ( size_t s = 0; s < saved.entrenchments[t].sections.size(); ++s )
		{
			const std::vector<int> &rSection = saved.entrenchments[t].sections[s];
			if ( rSection.empty() )
				++nEmpty;
			for ( size_t k = 0; k < rSection.size(); ++k, ++nLinks )
			{
				BkEditorObjectState state;
				if ( BkEditorEngineObjectState( pSession, rSection[k], &state ) != BK_EDITOR_OK )
					++nMissing;
			}
		}
	remove( OsPath( szPath ).c_str() );
	return Check( nMissing == 0 && nEmpty == 0, NStr::Format( "every entrenchment section is non-empty and names engine objects %s (%d of %d links missing, %d sections empty)",
	                                                          pszWhen, nMissing, nLinks, nEmpty ) );
}

// D-13/D-03/C6 on the real engine: an L-shaped trench drawn across coldwinter
// is planned exactly as NMapGeometry::PlanEntrenchment plans it with the
// stats' own inputs, saved as the map the map-file tier's builder makes (its
// pieces as ordinary objects, its sections of their link IDs), undone to the
// unedited bytes and redone; a trench shorter than a piece, one off the map
// and bad arguments are refused with nothing changed.
static void TestM2Entrenchments( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	// The packed types MapGeometry repeats are the stats'.
	Check( NMapGeometry::TRENCH_LINE == SEntrenchmentRPGStats::ENTRENCHMENT_LINE && NMapGeometry::TRENCH_FIREPLACE == SEntrenchmentRPGStats::ENTRENCHMENT_FIREPLACE &&
	       NMapGeometry::TRENCH_TERMINATOR == SEntrenchmentRPGStats::ENTRENCHMENT_TERMINATOR && NMapGeometry::TRENCH_ARC == SEntrenchmentRPGStats::ENTRENCHMENT_ARC,
	       "MapGeometry's trench piece types are SEntrenchmentRPGStats'" );
	NMapGeometry::STrenchPlanInput input;
	if ( !Check( TrenchInputFromStats( &input ), "the Entrenchment stats give the builder's inputs" ) )
		return;
	// The literals the map-file tier plans with (map_file_test.cpp) are these.
	Check( input.fLineWidth == 2.0f * 52.0f * fAITileXCoeff && input.fArcWidth == 2.0f * 15.0f * fAITileXCoeff,
	       NStr::Format( "the stats agree with the map-file tier's literals (line %g, arc %g)", input.fLineWidth, input.fArcWidth ) );
	TrenchExtent( original, &input );

	const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
	const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
	const std::vector<CVec2> clicks = EngineTrenchL( fMiddleX - 300.0f, fMiddleY - 250.0f );
	const std::vector<BkEditorVec3> cClicks = ToCPoints2( clicks );
	NMapGeometry::STrenchPlan plan;
	if ( !Check( NMapGeometry::PlanEntrenchment( input, clicks, &plan, &szError ), szError.c_str() ) )
		return;

	// The preview's plan is the function's.
	{
		int nPlanned = 0;
		std::vector<BkEditorPlannedPiece> pieces( 256 );
		Check( BkEditorPlanEntrenchment( pSession, &cClicks[0], int( cClicks.size() ), &pieces[0], 256, &nPlanned ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		bool bSame = nPlanned == int( plan.pieces.size() );
		for ( int i = 0; bSame && i < nPlanned; ++i )
			bSame = pieces[i].x == plan.pieces[i].vPos.x && pieces[i].y == plan.pieces[i].vPos.y && pieces[i].type == plan.pieces[i].nPackedType &&
			        pieces[i].dir == plan.pieces[i].nDir;
		Check( bSame, NStr::Format( "BkEditorPlanEntrenchment plans what PlanEntrenchment plans (%d pieces, %d sections)", nPlanned, int( plan.sections.size() ) ) );
		Check( BkEditorPlanEntrenchment( pSession, &cClicks[0], int( cClicks.size() ), 0, 0, &nPlanned ) == BK_EDITOR_REFUSED && nPlanned == int( plan.pieces.size() ),
		       "a plan with no buffer is refused with the count filled" );
	}

	const std::string szUnedited = szScratch + "\\trench-unedited.bzm";
	const std::string szEdited = szScratch + "\\trench-edited.bzm";
	const std::string szCheck = szScratch + "\\trench-check.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int nObjectsBefore = int( ReadObjectRecords( pSession ).size() );
	EveryTrenchLinkIsInTheEngine( pSession, szCheck, "as opened" );

	const int nPlayer = 1;
	int nToken = -1, nIndex = -1;
	if ( !Check( BkEditorDrawEntrenchment( pSession, &cClicks[0], int( cClicks.size() ), nPlayer, &nToken, &nIndex ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( nToken >= 0 && nIndex == int( original.entrenchments.size() ), NStr::Format( "the entrenchment's entry is appended (token %d, index %d)", nToken, nIndex ) );
	Check( int( ReadObjectRecords( pSession ).size() ) == nObjectsBefore + int( plan.pieces.size() ), "one object per piece" );
	WorldAgrees( pSession, "after an entrenchment was drawn" );
	EveryTrenchLinkIsInTheEngine( pSession, szCheck, "after an entrenchment was drawn" );
	{
		const std::vector<BkEditorEntrenchmentInfo> infos = ReadTrenches( pSession );
		if ( Check( int( infos.size() ) == nIndex + 1, "BkEditorEntrenchments lists the new entry" ) )
		{
			const BkEditorEntrenchmentInfo &rInfo = infos[nIndex];
			Check( rInfo.piece_count == int( plan.pieces.size() ) && rInfo.section_count == int( plan.sections.size() ) && rInfo.player == nPlayer &&
			       rInfo.min_x <= plan.pieces[0].vPos.x && rInfo.max_y >= plan.pieces[1].vPos.y,
			       NStr::Format( "with its piece and section counts, player and box (%d pieces, %d sections)", rInfo.piece_count, rInfo.section_count ) );
		}
	}
	CMapInfo expected;
	Check( NMapFile::Read( SHIPPED_MAP, &expected, &szError ), szError.c_str() );
	Check( LayTrench( &expected, plan, nPlayer ), "the expected entrenchment lays over the file's map" );
	CheckSavedEquals( pSession, szEdited, expected, "a new entrenchment" );

	// Undo: the unedited bytes; redo: the expected map again.
	if ( Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		WorldAgrees( pSession, "after the entrenchment's undo" );
		EveryTrenchLinkIsInTheEngine( pSession, szCheck, "after the entrenchment's undo" );
		Check( int( ReadObjectRecords( pSession ).size() ) == nObjectsBefore, "the undo takes every piece out" );
		const std::string szUndone = szScratch + "\\trench-undone.bzm";
		if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szUnedited, szUndone ), "an entrenchment and its undo save the unedited file byte for byte" );
		remove( OsPath( szUndone ).c_str() );
	}
	if ( Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		WorldAgrees( pSession, "after the entrenchment's redo" );
		EveryTrenchLinkIsInTheEngine( pSession, szCheck, "after the entrenchment's redo" );
		CheckSavedEquals( pSession, szEdited, expected, "the entrenchment redone" );
	}
	Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// Refusals change nothing: the objects, the entries, the engine, the file.
	const size_t nTrenches = ReadTrenches( pSession ).size();
	nToken = nIndex = 7;
	{
		std::vector<BkEditorVec3> one( 1, cClicks[0] );
		Check( BkEditorDrawEntrenchment( pSession, &one[0], 1, 0, &nToken, &nIndex ) == BK_EDITOR_REFUSED, "one point is refused" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "shorter than one piece" ) != std::string::npos, NStr::Format( "and says why (%s)", BkEditorLastMessage( pSession ) ) );
		Check( nToken == -1 && nIndex == -1, "a refusal hands out no token" );
	}
	const float fEdge = original.terrain.tiles.GetSizeX() * fWorldCellSize;
	{
		const std::vector<BkEditorVec3> off = ToCPoints2( EngineTrenchL( fEdge - 300.0f, fMiddleY ) );
		Check( BkEditorDrawEntrenchment( pSession, &off[0], int( off.size() ), 0, &nToken, &nIndex ) == BK_EDITOR_REFUSED &&
		       std::string( BkEditorLastMessage( pSession ) ) == "the trench leaves the map", "a trench running off the map's edge is refused" );
		printf( "editor-bridge: off the edge: %s\n", BkEditorLastMessage( pSession ) );
		std::vector<BkEditorVec3> nan = cClicks;
		nan[1].y = std::numeric_limits<float>::quiet_NaN();
		Check( BkEditorDrawEntrenchment( pSession, &nan[0], int( nan.size() ), 0, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "a NaN point is a bad argument" );
		Check( BkEditorDrawEntrenchment( pSession, 0, 3, 0, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "null points are a bad argument" );
		std::vector<BkEditorVec3> many( 257, cClicks[0] );
		Check( BkEditorDrawEntrenchment( pSession, &many[0], 257, 0, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "257 points are a bad argument" );
		Check( BkEditorDrawEntrenchment( pSession, &cClicks[0], int( cClicks.size() ), 99, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "a player the map lacks is a bad argument" );
		Check( BkEditorDrawEntrenchment( pSession, &cClicks[0], int( cClicks.size() ), -1, &nToken, &nIndex ) == BK_EDITOR_BAD_ARGUMENT, "player -1 is a bad argument" );
	}
	Check( ReadTrenches( pSession ).size() == nTrenches, "no refusal added an entry" );
	Check( int( ReadObjectRecords( pSession ).size() ) == nObjectsBefore, "nor an object" );
	WorldAgrees( pSession, "after the refusals" );
	EveryTrenchLinkIsInTheEngine( pSession, szCheck, "after the refusals" );
	const std::string szRefused = szScratch + "\\trench-refused.bzm";
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szRefused ), "and the map saves unedited byte for byte" );
	remove( OsPath( szRefused ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	printf( "editor-bridge: M2 entrenchments draw ok\n" );
}

// The shipped map the garrison refusal is shown on: the first map under
// Data\Maps the map-file tier's scan finds entrenchments in; every one of its
// 15 entrenchments holds units.
static const char *const GARRISONED_TRENCH_MAP = "Data\\Maps\\allies\\ardennes\\battleofbulge.bzm";

// Which entrenchments of a map hold units: a unit whose nLinkWith names one of
// the entrenchment's pieces is garrisoned in it.
static std::vector<bool> GarrisonedTrenches( const CMapInfo &rMap )
{
	std::set<int> holders;
	const std::vector<SMapObjectInfo> *lists[2] = { &rMap.objects, &rMap.scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
			if ( (*lists[nList])[i].link.nLinkWith > 0 )
				holders.insert( (*lists[nList])[i].link.nLinkWith );
	std::vector<bool> garrisoned( rMap.entrenchments.size(), false );
	for ( size_t t = 0; t < rMap.entrenchments.size(); ++t )
		for ( size_t sIndex = 0; sIndex < rMap.entrenchments[t].sections.size(); ++sIndex )
			for ( size_t k = 0; k < rMap.entrenchments[t].sections[sIndex].size(); ++k )
				if ( holders.count( rMap.entrenchments[t].sections[sIndex][k] ) != 0 )
					garrisoned[t] = true;
	return garrisoned;
}

// The first shipped map, in sorted path order (the same on every platform),
// with an entrenchment that holds no units and whose pieces are each one
// object of `objects`; its path (engine form) and that entrenchment's index.
static bool FindEmptyTrenchMap( std::string *pPath, int *pnTrench, int *pnScanned )
{
	std::vector<std::string> paths;
	std::error_code error;
	for ( std::filesystem::recursive_directory_iterator it( "Data/Maps", error ), itEnd; !error && it != itEnd; it.increment( error ) )
		if ( it->is_regular_file( error ) )
		{
			std::string szPath = it->path().generic_string();
			if ( szPath.size() > 4 && NStr::CompareAsciiNoCase( szPath.c_str() + szPath.size() - 4, ".bzm" ) == 0 )
			{
				std::replace( szPath.begin(), szPath.end(), '/', '\\' );
				paths.push_back( szPath );
			}
		}
	std::sort( paths.begin(), paths.end() );
	*pnScanned = 0;
	for ( size_t p = 0; p < paths.size(); ++p )
	{
		CMapInfo map;
		std::string szError;
		++*pnScanned;
		if ( !NMapFile::Read( paths[p].c_str(), &map, &szError ) || map.entrenchments.empty() )
			continue;
		const std::vector<bool> garrisoned = GarrisonedTrenches( map );
		for ( size_t t = 0; t < map.entrenchments.size(); ++t )
		{
			if ( garrisoned[t] || map.entrenchments[t].sections.empty() )
				continue;
			bool bPlain = true;
			for ( size_t sIndex = 0; sIndex < map.entrenchments[t].sections.size() && bPlain; ++sIndex )
				for ( size_t k = 0; k < map.entrenchments[t].sections[sIndex].size() && bPlain; ++k )
				{
					int nHolders = 0;
					for ( size_t i = 0; i < map.objects.size(); ++i )
						nHolders += map.objects[i].link.nLinkID == map.entrenchments[t].sections[sIndex][k] ? 1 : 0;
					bPlain = nHolders == 1;
				}
			if ( bPlain )
			{
				*pPath = paths[p];
				*pnTrench = int( t );
				return true;
			}
		}
	}
	return false;
}

// The screen point a shipped entrenchment's piece is picked at: the camera on
// the piece, then its world point on screen, tried as it is and a little
// above and below. False when no offset picks entrenchment nWanted.
static bool PickTrenchAt( BkEditorSession *pSession, float fWorldX, float fWorldY, int nWanted, float *pfSx, float *pfSy )
{
	if ( BkEditorSetCamera( pSession, fWorldX, fWorldY ) != BK_EDITOR_OK || BkEditorFrame( pSession ) != BK_EDITOR_OK )
		return false;
	float fSx = 0.0f, fSy = 0.0f;
	if ( BkEditorWorldToScreen( pSession, fWorldX, fWorldY, &fSx, &fSy ) != BK_EDITOR_OK )
		return false;
	const float offsets[] = { 0.0f, -4.0f, 4.0f, -8.0f, 8.0f, -12.0f, 12.0f };
	for ( size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i )
	{
		int nKind = -1, nIndex = -1;
		if ( BkEditorPickGroup( pSession, fSx, fSy + offsets[i], &nKind, &nIndex ) == BK_EDITOR_OK && nKind == 2 && nIndex == nWanted )
		{
			*pfSx = fSx;
			*pfSy = fSy + offsets[i];
			return true;
		}
	}
	return false;
}

// D-04 on the real engine: an entrenchment that holds units (a soldier whose
// nLinkWith names a piece) is refused whole before anything is taken out, and
// the map saves unedited. Moving units out of a trench is M3's links.
static void TestM2GarrisonedTrenchRefused( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( GARRISONED_TRENCH_MAP, &original, &szError ), szError.c_str() ) )
		return;
	const std::vector<bool> garrisoned = GarrisonedTrenches( original );
	const std::vector<bool>::const_iterator itHeld = std::find( garrisoned.begin(), garrisoned.end(), true );
	if ( !Check( itHeld != garrisoned.end(), "battleofbulge has an entrenchment that holds units" ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, GARRISONED_TRENCH_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\trench-held-unedited.bzm";
	const std::string szHeld = szScratch + "\\trench-held.bzm";
	const std::string szCheck = szScratch + "\\trench-held-check.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int nHeld = int( itHeld - garrisoned.begin() );
	int nToken = 7;
	Check( BkEditorDeleteEntrenchment( pSession, nHeld, &nToken ) == BK_EDITOR_REFUSED && nToken == -1,
	       NStr::Format( "entrenchment %d, which holds units, is refused whole (%s)", nHeld, BkEditorLastMessage( pSession ) ) );
	printf( "editor-bridge: %d of %d entrenchments hold units; one refused: %s\n", int( std::count( garrisoned.begin(), garrisoned.end(), true ) ),
	        int( garrisoned.size() ), BkEditorLastMessage( pSession ) );
	if ( Check( BkEditorSaveMap( pSession, szHeld.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szHeld ), "and the map saves unedited byte for byte" );
	WorldAgrees( pSession, "after the refused delete" );
	EveryTrenchLinkIsInTheEngine( pSession, szCheck, "after the refused delete" );
	remove( OsPath( szHeld ).c_str() );
	remove( OsPath( szUnedited ).c_str() );
}

// D-13/D-04 on the real engine: a shipped map's entrenchment (the first in
// sorted path order that holds no units) is picked from a screen point over
// one of its pieces (as a group - BkEditorObjectAt passes a piece over),
// deleted whole - the save equals the file's map with the entry erased and its
// pieces deleted, and every other entrenchment's links still resolve - its
// undo saves the unedited bytes, and a piece alone is still refused to the
// object delete.
static void TestM2EntrenchmentDelete( BkEditorSession *pSession, const std::string &szScratch )
{
	TestM2GarrisonedTrenchRefused( pSession, szScratch );

	std::string szMap;
	int nWantedTrench = -1, nScanned = 0;
	if ( !Check( FindEmptyTrenchMap( &szMap, &nWantedTrench, &nScanned ), NStr::Format( "a shipped map has an entrenchment with no units in it (%d maps read)", nScanned ) ) )
		return;
	printf( "editor-bridge: entrenchment %d of %s holds no units (%d maps read)\n", nWantedTrench, szMap.c_str(), nScanned );
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( szMap.c_str(), &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\trench-delete-unedited.bzm";
	const std::string szEdited = szScratch + "\\trench-delete-edited.bzm";
	const std::string szCheck = szScratch + "\\trench-delete-check.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( ReadTrenches( pSession ).size() == original.entrenchments.size(),
	       NStr::Format( "BkEditorEntrenchments lists the file's %d entrenchments", int( original.entrenchments.size() ) ) );
	EveryTrenchLinkIsInTheEngine( pSession, szCheck, "as opened" );

	// One of its pieces, on screen.
	const int nTrench = nWantedTrench;
	int nPieceLink = -1;
	float fSx = 0.0f, fSy = 0.0f, fPieceX = 0.0f, fPieceY = 0.0f;
	const SEntrenchmentInfo &rTrench = original.entrenchments[nTrench];
	for ( size_t sIndex = 0; sIndex < rTrench.sections.size() && nPieceLink < 0; ++sIndex )
		for ( size_t k = 0; k < rTrench.sections[sIndex].size() && nPieceLink < 0; ++k )
		{
			const int nLink = rTrench.sections[sIndex][k];
			for ( size_t i = 0; i < original.objects.size(); ++i )
				if ( original.objects[i].link.nLinkID == nLink )
				{
					fPieceX = original.objects[i].vPos.x * fAITileXCoeff;
					fPieceY = original.objects[i].vPos.y * fAITileYCoeff;
					if ( PickTrenchAt( pSession, fPieceX, fPieceY, nTrench, &fSx, &fSy ) )
						nPieceLink = nLink;
					break;
				}
		}
	if ( !Check( nPieceLink >= 0, "BkEditorPickGroup finds the entrenchment over one of its pieces" ) )
		return;
	int nPieces = 0;
	for ( size_t sIndex = 0; sIndex < rTrench.sections.size(); ++sIndex )
		nPieces += int( rTrench.sections[sIndex].size() );
	printf( "editor-bridge: entrenchment %d of %d (%d pieces, %d sections) picked at %.0f,%.0f\n", nTrench, int( original.entrenchments.size() ),
	        nPieces, int( rTrench.sections.size() ), fSx, fSy );
	{
		// BkEditorObjectAt keeps its meaning: it passes the piece over.
		int nLinkID = -1;
		const BkEditorStatus status = BkEditorObjectAt( pSession, fSx, fSy, &nLinkID );
		bool bPiece = false;
		for ( size_t t = 0; t < original.entrenchments.size(); ++t )
			for ( size_t sIndex = 0; sIndex < original.entrenchments[t].sections.size(); ++sIndex )
				bPiece = bPiece || std::find( original.entrenchments[t].sections[sIndex].begin(), original.entrenchments[t].sections[sIndex].end(), nLinkID ) !=
				                   original.entrenchments[t].sections[sIndex].end();
		Check( status == BK_EDITOR_REFUSED || ( status == BK_EDITOR_OK && !bPiece ), "BkEditorObjectAt at the same point answers no trench piece" );
	}
	// D-04: a piece alone is refused, and changes nothing.
	Check( BkEditorDeleteObject( pSession, nPieceLink ) == BK_EDITOR_REFUSED, NStr::Format( "a trench piece alone is refused to the object delete (%s)", BkEditorLastMessage( pSession ) ) );

	const int nObjectsBefore = int( ReadObjectRecords( pSession ).size() );
	int nToken = -1;
	if ( !Check( BkEditorDeleteEntrenchment( pSession, nTrench, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( int( ReadTrenches( pSession ).size() ) == int( original.entrenchments.size() ) - 1, "the entry is gone" );
	Check( int( ReadObjectRecords( pSession ).size() ) == nObjectsBefore - nPieces, "and every piece with it" );
	WorldAgrees( pSession, "after an entrenchment was deleted" );
	EveryTrenchLinkIsInTheEngine( pSession, szCheck, "after an entrenchment was deleted" );
	CMapInfo expected;
	Check( NMapFile::Read( szMap.c_str(), &expected, &szError ), szError.c_str() );
	SEntrenchmentInfo erased;
	Check( NMapRecords::EraseEntrenchment( &expected, nTrench, &erased ), "the expected map loses the entry" );
	for ( size_t sIndex = erased.sections.size(); sIndex-- > 0; )
		for ( size_t k = erased.sections[sIndex].size(); k-- > 0; )
		{
			std::string szRefusal;
			Check( NMapOverlay::DeleteObject( &expected, erased.sections[sIndex][k], &szRefusal ), szRefusal.c_str() );
		}
	CheckSavedEquals( pSession, szEdited, expected, "a deleted entrenchment" );

	if ( Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		WorldAgrees( pSession, "after the delete's undo" );
		EveryTrenchLinkIsInTheEngine( pSession, szCheck, "after the delete's undo" );
		const std::string szUndone = szScratch + "\\trench-delete-undone.bzm";
		if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szUnedited, szUndone ), "an entrenchment's delete and its undo save the unedited file byte for byte" );
		remove( OsPath( szUndone ).c_str() );
		// Picked again over the same piece, after a frame has drawn the pieces
		// the undo put back.
		float fAgainX = 0.0f, fAgainY = 0.0f;
		Check( PickTrenchAt( pSession, fPieceX, fPieceY, nTrench, &fAgainX, &fAgainY ), "and the entrenchment is picked again" );
	}
	if ( Check( BkEditorRedoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		CheckSavedEquals( pSession, szEdited, expected, "the delete redone" );
		EveryTrenchLinkIsInTheEngine( pSession, szCheck, "after the delete's redo" );
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	}
	Check( BkEditorDeleteEntrenchment( pSession, int( original.entrenchments.size() ), &nToken ) == BK_EDITOR_BAD_ARGUMENT, "an entrenchment past the end is a bad argument" );
	Check( BkEditorDeleteEntrenchment( pSession, -1, &nToken ) == BK_EDITOR_BAD_ARGUMENT, "index -1 is a bad argument" );
	// A drawn trench deletes whole as well, and its undo puts it back.
	{
		const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
		const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
		const std::vector<BkEditorVec3> clicks = ToCPoints2( EngineTrenchL( fMiddleX - 300.0f, fMiddleY - 250.0f ) );
		int nDrawToken = -1, nIndex = -1, nDeleteToken = -1;
		if ( Check( BkEditorDrawEntrenchment( pSession, &clicks[0], int( clicks.size() ), 0, &nDrawToken, &nIndex ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
		     Check( BkEditorDeleteEntrenchment( pSession, nIndex, &nDeleteToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			Check( int( ReadTrenches( pSession ).size() ) == int( original.entrenchments.size() ), "a drawn trench deleted leaves the file's list" );
			Check( BkEditorUndoEdit( pSession, nDeleteToken ) == BK_EDITOR_OK && int( ReadTrenches( pSession ).size() ) == int( original.entrenchments.size() ) + 1,
			       "its delete undone puts it back at the end" );
			EveryTrenchLinkIsInTheEngine( pSession, szCheck, "after a drawn trench's delete was undone" );
			Check( BkEditorUndoEdit( pSession, nDrawToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		}
		const std::string szBack = szScratch + "\\trench-delete-back.bzm";
		if ( Check( BkEditorSaveMap( pSession, szBack.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szUnedited, szBack ), "and everything undone saves the unedited file byte for byte" );
		remove( OsPath( szBack ).c_str() );
	}
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	printf( "editor-bridge: M2 entrenchment delete ok\n" );
}

// ---------------------------------------------------------------------------
// 04-11: start commands (D-17) and reserve positions (D-18).
// ---------------------------------------------------------------------------

// The link IDs of up to nWanted objects of the map's objects list that are units or
// squads the database knows and that no other object shares, and one that is neither
// (a building, a tree: -1 when the map has none). A start command may name a unit of
// this kind and no other.
static void PickCommandUnits( const CMapInfo &rMap, int nWanted, std::vector<int> *pUnits, int *pnNonUnit )
{
	pUnits->clear();
	*pnNonUnit = -1;
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
		return;
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
	{
		const int nLink = rMap.objects[i].link.nLinkID;
		if ( nLink <= 0 )
			continue;
		int nSharing = 0;
		for ( size_t j = 0; j < rMap.objects.size(); ++j )
			nSharing += rMap.objects[j].link.nLinkID == nLink ? 1 : 0;
		for ( size_t j = 0; j < rMap.scenarioObjects.size(); ++j )
			nSharing += rMap.scenarioObjects[j].link.nLinkID == nLink ? 1 : 0;
		if ( nSharing != 1 )
			continue;
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( rMap.objects[i].szName.c_str() );
		if ( pDesc == 0 )
			continue;
		const bool bUnit = pDesc->eGameType == SGVOGT_UNIT || pDesc->eGameType == SGVOGT_SQUAD;
		if ( bUnit && int( pUnits->size() ) < nWanted )
			pUnits->push_back( nLink );
		else if ( !bUnit && *pnNonUnit < 0 )
			*pnNonUnit = nLink;
	}
}

static BkEditorStartCommandRecord StartRecordOf( int nType, int nTarget, float fX, float fY, int nExplosion, float fNumber, int nUnits )
{
	BkEditorStartCommandRecord record;
	memset( &record, 0, sizeof record );
	record.cmd_type = nType;
	record.link_id = nTarget;
	record.x = fX;
	record.y = fY;
	record.from_explosion = nExplosion;
	record.number = fNumber;
	record.unit_count = nUnits;
	return record;
}

static int StartCommandCountOf( BkEditorSession *pSession )
{
	int nCount = -2;
	if ( BkEditorStartCommandCount( pSession, &nCount ) != BK_EDITOR_OK )
		return -1;
	return nCount;
}

// One command and its units, two-pass; the sizing pass is REFUSED when the command
// has units (a buffer too short is), and the count it leaves is the total.
static bool ReadStartCommandOf( BkEditorSession *pSession, int nIndex, BkEditorStartCommandRecord *pRecord, std::vector<int> *pUnits, BkEditorStatus *pStatus = 0 )
{
	BkEditorStartCommandRecord probe;
	memset( &probe, 0, sizeof probe );
	probe.unit_count = -2;
	BkEditorStatus status = BkEditorStartCommand( pSession, nIndex, &probe, 0, 0 );
	if ( pStatus != 0 )
		*pStatus = status;
	if ( ( status != BK_EDITOR_OK && status != BK_EDITOR_REFUSED ) || probe.unit_count < 0 )
		return false;
	const int nUnits = probe.unit_count;
	pUnits->assign( nUnits > 0 ? nUnits : 1, 0 );
	status = BkEditorStartCommand( pSession, nIndex, pRecord, &( *pUnits )[0], nUnits );
	if ( pStatus != 0 )
		*pStatus = status;
	if ( status != BK_EDITOR_OK || pRecord->unit_count != nUnits )
		return false;
	pUnits->resize( nUnits );
	return true;
}

static SAIStartCommand StartCommandFrom( const BkEditorStartCommandRecord &rRecord, const std::vector<int> &rUnits )
{
	return SAIStartCommand( EActionCommand( rRecord.cmd_type ), rUnits, rRecord.link_id, CVec2( rRecord.x, rRecord.y ), rRecord.from_explosion != 0, rRecord.number );
}

static bool SameStartCommandValue( const SAIStartCommand &rLeft, const SAIStartCommand &rRight )
{
	return rLeft.cmdType == rRight.cmdType && rLeft.unitLinkIDs == rRight.unitLinkIDs && rLeft.linkID == rRight.linkID && rLeft.vPos.x == rRight.vPos.x &&
	       rLeft.vPos.y == rRight.vPos.y && rLeft.fromExplosion == rRight.fromExplosion && rLeft.fNumber == rRight.fNumber;
}

// Every command the bridge reads is the file's, in order.
static bool StartCommandsAreTheFiles( BkEditorSession *pSession, const CMapInfo &rMap )
{
	if ( StartCommandCountOf( pSession ) != int( rMap.startCommandsList.size() ) )
		return false;
	int nIndex = 0;
	for ( SLoadMapInfo::TStartCommandsList::const_iterator it = rMap.startCommandsList.begin(); it != rMap.startCommandsList.end(); ++it, ++nIndex )
	{
		BkEditorStartCommandRecord record;
		std::vector<int> units;
		if ( !ReadStartCommandOf( pSession, nIndex, &record, &units ) || !SameStartCommandValue( StartCommandFrom( record, units ), *it ) )
			return false;
	}
	return true;
}

// D-17 on the real engine: the action types list from Data/Editor/actions.ini with
// STOP at the default; a command is added for a unit, edited (type, target, number)
// and saved as the map NMapRecords builds; a set keeps the file's fromExplosion;
// an empty unit list, a duplicate, a non-unit, link 0 and an unknown unit, target or
// type are refused and change nothing; deleted and put back it is the same; a unit a
// group holds back adds with a warning; and everything undone saves the unedited
// bytes.
static void TestM2StartCommands( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\startcmd-unedited.bzm";
	const std::string szEdited = szScratch + "\\startcmd-edited.bzm";
	const std::string szUndone = szScratch + "\\startcmd-undone.bzm";
	const std::string szRefused = szScratch + "\\startcmd-refused.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// The action types: two passes, STOP at the default entry.
	int nActions = -2, nDefault = -2;
	BkEditorStatus status = BkEditorActionCommands( pSession, 0, 0, &nActions, &nDefault );
	if ( !Check( ( status == BK_EDITOR_OK || status == BK_EDITOR_REFUSED ) && nActions > 9, NStr::Format( "Data/Editor/actions.ini lists action types (%d, status %d): %s", nActions, int( status ), BkEditorLastMessage( pSession ) ) ) )
		return;
	std::vector<BkEditorActionCommand> actions( nActions );
	int nRead = -2, nDefaultRead = -2;
	if ( !Check( BkEditorActionCommands( pSession, &actions[0], nActions, &nRead, &nDefaultRead ) == BK_EDITOR_OK && nRead == nActions, BkEditorLastMessage( pSession ) ) )
		return;
	Check( nDefaultRead >= 0 && nDefaultRead < nActions && actions[nDefaultRead].id == 9 && std::string( actions[nDefaultRead].name ) == "STOP",
	       NStr::Format( "the default action is STOP, entry 9 (entry %d is %s = %d)", nDefaultRead, nDefaultRead >= 0 && nDefaultRead < nActions ? actions[nDefaultRead].name : "?", nDefaultRead >= 0 && nDefaultRead < nActions ? actions[nDefaultRead].id : -1 ) );
	Check( actions[0].id == 0 && std::string( actions[0].name ) == "MOVE_TO", "the first action is MOVE_TO = 0, in the file's order" );
	std::vector<BkEditorActionCommand> tooShort( 2 );
	Check( BkEditorActionCommands( pSession, &tooShort[0], 2, &nRead, &nDefaultRead ) == BK_EDITOR_REFUSED && nRead == nActions, "a capacity below the total is REFUSED with the total answered" );
	Check( BkEditorActionCommands( pSession, 0, 0, 0, &nDefault ) == BK_EDITOR_BAD_ARGUMENT && BkEditorActionCommands( pSession, 0, 0, &nRead, 0 ) == BK_EDITOR_BAD_ARGUMENT &&
	       BkEditorActionCommands( pSession, 0, -1, &nRead, &nDefault ) == BK_EDITOR_BAD_ARGUMENT, "a null count, a null default or a negative capacity is a bad argument" );
	const int nStop = 9;
	const int nMoveTo = 0;
	Check( StartCommandsAreTheFiles( pSession, original ), "the start commands read as the file has them" );

	// The units the test commands.
	std::vector<int> units;
	int nNonUnit = -1;
	PickCommandUnits( original, 2, &units, &nNonUnit );
	if ( !Check( units.size() == 2, "coldwinter has two units a start command may name" ) )
		return;
	const int nA = units[0], nB = units[1];
	const int nBefore = StartCommandCountOf( pSession );
	const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize * fAITileXCoeff1 / 2.0f;
	const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize * fAITileYCoeff1 / 2.0f;

	// Argument checks that need no map state.
	BkEditorStartCommandRecord record = StartRecordOf( nStop, 0, 0, 0, 0, 0, 1 );
	Check( BkEditorAddStartCommand( pSession, -1, 0, &nA ) == BK_EDITOR_BAD_ARGUMENT && BkEditorSetStartCommand( pSession, 0, 0, &nA ) == BK_EDITOR_BAD_ARGUMENT, "a null record is a bad argument" );
	Check( BkEditorAddStartCommand( pSession, -1, &record, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a unit count with no units is a bad argument" );
	BkEditorStartCommandRecord negativeCount = StartRecordOf( nStop, 0, 0, 0, 0, 0, -1 );
	BkEditorStartCommandRecord hugeCount = StartRecordOf( nStop, 0, 0, 0, 0, 0, 1 << 20 );
	Check( BkEditorAddStartCommand( pSession, -1, &negativeCount, &nA ) == BK_EDITOR_BAD_ARGUMENT && BkEditorAddStartCommand( pSession, -1, &hugeCount, &nA ) == BK_EDITOR_BAD_ARGUMENT, "a negative or absurd unit count is a bad argument" );
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	BkEditorStartCommandRecord nanRecord = StartRecordOf( nStop, 0, fNaN, 0, 0, 0, 1 );
	BkEditorStartCommandRecord nanNumber = StartRecordOf( nStop, 0, 0, 0, 0, fNaN, 1 );
	BkEditorStartCommandRecord badExplosion = StartRecordOf( nStop, 0, 0, 0, 2, 0, 1 );
	Check( BkEditorAddStartCommand( pSession, -1, &nanRecord, &nA ) == BK_EDITOR_BAD_ARGUMENT && BkEditorAddStartCommand( pSession, -1, &nanNumber, &nA ) == BK_EDITOR_BAD_ARGUMENT &&
	       BkEditorAddStartCommand( pSession, -1, &badExplosion, &nA ) == BK_EDITOR_BAD_ARGUMENT, "a NaN point or number and an explosion flag of 2 are bad arguments" );
	Check( BkEditorAddStartCommand( pSession, -2, &record, &nA ) == BK_EDITOR_BAD_ARGUMENT && BkEditorAddStartCommand( pSession, nBefore + 1, &record, &nA ) == BK_EDITOR_BAD_ARGUMENT, "an insert index out of range is a bad argument" );
	Check( BkEditorSetStartCommand( pSession, nBefore, &record, &nA ) == BK_EDITOR_BAD_ARGUMENT && BkEditorSetStartCommand( pSession, -1, &record, &nA ) == BK_EDITOR_BAD_ARGUMENT, "a set index out of range is a bad argument" );
	Check( BkEditorDeleteStartCommand( pSession, nBefore ) == BK_EDITOR_BAD_ARGUMENT && BkEditorDeleteStartCommand( pSession, -1 ) == BK_EDITOR_BAD_ARGUMENT, "a delete index out of range is a bad argument" );
	BkEditorStartCommandRecord unusedRecord;
	Check( BkEditorStartCommand( pSession, nBefore, &unusedRecord, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT && BkEditorStartCommand( pSession, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT &&
	       BkEditorStartCommand( pSession, nBefore == 0 ? -1 : 0, &unusedRecord, 0, -1 ) == BK_EDITOR_BAD_ARGUMENT, "a read past the end, to nothing or with a negative capacity is a bad argument" );

	// An add: STOP for unit A, appended.
	if ( !Check( BkEditorAddStartCommand( pSession, -1, &record, &nA ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( StartCommandCountOf( pSession ) == nBefore + 1, "the command is added" );
	BkEditorStartCommandRecord readBack;
	std::vector<int> readUnits;
	if ( Check( ReadStartCommandOf( pSession, nBefore, &readBack, &readUnits ), "the new command reads back" ) )
		Check( SameStartCommandValue( StartCommandFrom( readBack, readUnits ), StartCommandFrom( record, std::vector<int>( 1, nA ) ) ), "as it was given" );
	// An edit: MOVE_TO, unit B as the target, two units, a number.
	const int both[2] = { nA, nB };
	BkEditorStartCommandRecord moved = StartRecordOf( nMoveTo, nB, fMiddleX, fMiddleY, 0, 3.5f, 2 );
	if ( !Check( BkEditorSetStartCommand( pSession, nBefore, &moved, both ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo expected = original;
	Check( NMapRecords::InsertStartCommand( &expected, -1, StartCommandFrom( moved, std::vector<int>( both, both + 2 ) ) ), "the expected map takes the command" );
	CheckSavedEquals( pSession, szEdited, expected, "an added and edited start command" );
	// The point target, and the target back to none.
	BkEditorStartCommandRecord onPoint = StartRecordOf( nMoveTo, 0, fMiddleX + 40.0f, fMiddleY - 12.0f, 0, 0, 2 );
	Check( BkEditorSetStartCommand( pSession, nBefore, &onPoint, both ) == BK_EDITOR_OK, "a point target is accepted" );
	Check( BkEditorSetStartCommand( pSession, nBefore, &moved, both ) == BK_EDITOR_OK, "and the object target goes back" );
	// A set to the very same value is accepted.
	Check( BkEditorSetStartCommand( pSession, nBefore, &moved, both ) == BK_EDITOR_OK, "a set to the command's own value is accepted" );

	// Refusals change nothing.
	if ( !Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const BkEditorStartCommandRecord empty = StartRecordOf( nStop, 0, 0, 0, 0, 0, 0 );
	Check( BkEditorAddStartCommand( pSession, -1, &empty, 0 ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "unit" ) != std::string::npos, "a command with no unit is refused, saying why" );
	Check( BkEditorSetStartCommand( pSession, nBefore, &empty, 0 ) == BK_EDITOR_REFUSED, "so is setting the units to none" );
	const int twice[2] = { nA, nA };
	BkEditorStartCommandRecord dup = StartRecordOf( nStop, 0, 0, 0, 0, 0, 2 );
	Check( BkEditorAddStartCommand( pSession, -1, &dup, twice ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "twice" ) != std::string::npos, "a unit twice is refused" );
	const int zero = 0, negative = -3, missing = 999999;
	Check( BkEditorAddStartCommand( pSession, -1, &record, &zero ) == BK_EDITOR_REFUSED, "link ID 0 as a unit is refused" );
	Check( BkEditorAddStartCommand( pSession, -1, &record, &negative ) == BK_EDITOR_REFUSED, "so is a negative one" );
	Check( BkEditorAddStartCommand( pSession, -1, &record, &missing ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "999999" ) != std::string::npos, "a unit no object has is refused, naming it" );
	if ( nNonUnit >= 0 )
		Check( BkEditorAddStartCommand( pSession, -1, &record, &nNonUnit ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "not a unit" ) != std::string::npos, "an object that is not a unit or a squad is refused" );
	else
		printf( "editor-bridge: noted: coldwinter has no shared-free non-unit object with a link ID, the non-unit refusal is not exercised\n" );
	BkEditorStartCommandRecord missingTarget = StartRecordOf( nStop, 999999, 0, 0, 0, 0, 1 );
	Check( BkEditorAddStartCommand( pSession, -1, &missingTarget, &nA ) == BK_EDITOR_REFUSED, "a target no object has is refused" );
	BkEditorStartCommandRecord negativeTarget = StartRecordOf( nStop, -1, 0, 0, 0, 0, 1 );
	Check( BkEditorAddStartCommand( pSession, -1, &negativeTarget, &nA ) == BK_EDITOR_REFUSED, "a negative target is refused" );
	BkEditorStartCommandRecord unknownType = StartRecordOf( 12345, 0, 0, 0, 0, 0, 1 );
	Check( BkEditorAddStartCommand( pSession, -1, &unknownType, &nA ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "12345" ) != std::string::npos, "an action type the list does not have is refused" );
	Check( BkEditorSetStartCommand( pSession, nBefore, &unknownType, both ) == BK_EDITOR_REFUSED, "and so is setting one" );
	BkEditorStartCommandRecord offMap = StartRecordOf( nMoveTo, 0, -5.0f, 10.0f, 0, 0, 1 );
	Check( BkEditorAddStartCommand( pSession, -1, &offMap, &nA ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "not on the map" ) != std::string::npos, "a target point off the map is refused" );
	offMap.x = 1.0e9f;
	Check( BkEditorAddStartCommand( pSession, -1, &offMap, &nA ) == BK_EDITOR_REFUSED, "so is one far beyond the far edge" );
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szEdited, szRefused ), "none of the refusals changed the map" );

	// Deleted, the map is the unedited one; put back at its own index it is as it was.
	Check( BkEditorDeleteStartCommand( pSession, nBefore ) == BK_EDITOR_OK, "the command is deleted" );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), "add, edit and delete of a start command save the unedited file byte for byte" );
	Check( BkEditorAddStartCommand( pSession, nBefore, &moved, both ) == BK_EDITOR_OK, "the deleted command goes back at its own index" );
	if ( Check( ReadStartCommandOf( pSession, nBefore, &readBack, &readUnits ), "and reads back" ) )
		Check( SameStartCommandValue( StartCommandFrom( readBack, readUnits ), StartCommandFrom( moved, std::vector<int>( both, both + 2 ) ) ), "as it was" );
	Check( BkEditorDeleteStartCommand( pSession, nBefore ) == BK_EDITOR_OK, "and is deleted once more" );

	// The file's own explosion flag: a set that passes 0 keeps 1, and an add that passes 1 stores 1.
	{
		const std::string szMap = szScratch + "\\startcmd-explosion.bzm";
		CMapInfo scratch = original;
		SAIStartCommand exploding;
		exploding.unitLinkIDs.push_back( nA );
		exploding.fromExplosion = true;
		Check( NMapRecords::InsertStartCommand( &scratch, -1, exploding ), "the exploding command is laid over the map" );
		if ( Check( NMapFile::Write( szMap.c_str(), scratch, &szError ), szError.c_str() ) &&
		     Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			const int nExploding = StartCommandCountOf( pSession ) - 1;
			const std::string szBefore = szScratch + "\\startcmd-explosion-before.bzm", szAfter = szScratch + "\\startcmd-explosion-after.bzm";
			Check( BkEditorSaveMap( pSession, szBefore.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			BkEditorStartCommandRecord read;
			std::vector<int> readIDs;
			if ( Check( nExploding >= 0 && ReadStartCommandOf( pSession, nExploding, &read, &readIDs ) && read.from_explosion == 1, "the file's explosion flag reads as 1" ) )
			{
				BkEditorStartCommandRecord changed = StartRecordOf( nMoveTo, 0, fMiddleX, fMiddleY, 0, 2.0f, 1 );
				Check( BkEditorSetStartCommand( pSession, nExploding, &changed, &nA ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
				Check( ReadStartCommandOf( pSession, nExploding, &read, &readIDs ) && read.from_explosion == 1 && read.cmd_type == nMoveTo && read.number == 2.0f, "a set passing 0 keeps the explosion flag at 1" );
				CMapInfo expectedExploding = scratch;
				SAIStartCommand wanted = StartCommandFrom( changed, std::vector<int>( 1, nA ) );
				wanted.fromExplosion = true;
				Check( NMapRecords::ReplaceStartCommand( &expectedExploding, nExploding, wanted ), "the expected map replaces it" );
				CheckSavedEquals( pSession, szAfter, expectedExploding, "the explosion flag stays in the file" );
				// Delete and put back with the flag the read gave: an undo needs it back.
				Check( BkEditorDeleteStartCommand( pSession, nExploding ) == BK_EDITOR_OK, "the exploding command is deleted" );
				BkEditorStartCommandRecord again = StartRecordOf( nMoveTo, 0, fMiddleX, fMiddleY, 1, 2.0f, 1 );
				Check( BkEditorAddStartCommand( pSession, nExploding, &again, &nA ) == BK_EDITOR_OK, "and put back with its flag" );
				Check( ReadStartCommandOf( pSession, nExploding, &read, &readIDs ) && read.from_explosion == 1, "an add stores the flag it is given" );
			}
			remove( OsPath( szBefore ).c_str() );
			remove( OsPath( szAfter ).c_str() );
		}
		remove( OsPath( szMap ).c_str() );
	}

	// A file's own odd command - a unit no object has, an action type nobody lists - is
	// exempt: it can be deleted and put back, as an undo does, though a new one like it is refused.
	{
		const std::string szMap = szScratch + "\\startcmd-odd.bzm";
		CMapInfo odd = original;
		SAIStartCommand strange;
		strange.cmdType = EActionCommand( 777 );
		strange.unitLinkIDs.push_back( 888888 );
		strange.linkID = 777777;
		Check( NMapRecords::InsertStartCommand( &odd, -1, strange ), "the odd command is laid over the map" );
		if ( Check( NMapFile::Write( szMap.c_str(), odd, &szError ), szError.c_str() ) &&
		     Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			const std::string szBefore = szScratch + "\\startcmd-odd-before.bzm", szAfter = szScratch + "\\startcmd-odd-after.bzm";
			Check( BkEditorSaveMap( pSession, szBefore.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			const int nOdd = StartCommandCountOf( pSession ) - 1;
			BkEditorStartCommandRecord read;
			std::vector<int> readIDs;
			if ( Check( nOdd >= 0 && ReadStartCommandOf( pSession, nOdd, &read, &readIDs ), "the odd command reads" ) )
			{
				Check( BkEditorDeleteStartCommand( pSession, nOdd ) == BK_EDITOR_OK && BkEditorAddStartCommand( pSession, nOdd, &read, &readIDs[0] ) == BK_EDITOR_OK,
				       "the file's own odd command is deleted and put back, as an undo needs" );
				BkEditorStartCommandRecord fresh = read;
				fresh.number = 1.0f;
				Check( BkEditorAddStartCommand( pSession, -1, &fresh, &readIDs[0] ) == BK_EDITOR_REFUSED, "but a new command like it is refused" );
				Check( BkEditorSetStartCommand( pSession, nOdd, &fresh, &readIDs[0] ) == BK_EDITOR_OK, "and setting only its number is accepted (what the edit adds is what is judged)" );
				Check( BkEditorSetStartCommand( pSession, nOdd, &read, &readIDs[0] ) == BK_EDITOR_OK, "and back" );
			}
			if ( Check( BkEditorSaveMap( pSession, szAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
				Check( SameBytes( szBefore, szAfter ), "the odd map saves the same bytes after all of it" );
			remove( OsPath( szBefore ).c_str() );
			remove( OsPath( szAfter ).c_str() );
		}
		remove( OsPath( szMap ).c_str() );
	}

	// Assumption A3: a unit a reinforcement group holds back is not refused, and the answer says so.
	{
		const std::string szMap = szScratch + "\\startcmd-held.bzm";
		CMapInfo held = original;
		const int nScriptID = 77;
		bool bFree = true;
		for ( size_t i = 0; i < held.objects.size(); ++i )
			bFree = bFree && held.objects[i].nScriptID != nScriptID;
		Check( bFree, "no object of coldwinter carries script ID 77 already" );
		Check( NMapRecords::SetObjectScriptID( &held, nA, nScriptID ), "unit A takes script ID 77" );
		Check( NMapRecords::PutReinforcementGroup( &held, NMapRecords::FirstFreeGroupID( held, 5 ), std::vector<int>( 1, nScriptID ) ), "a group holds it" );
		if ( Check( NMapFile::Write( szMap.c_str(), held, &szError ), szError.c_str() ) &&
		     Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			Check( BkEditorAddStartCommand( pSession, -1, &record, &nA ) == BK_EDITOR_OK, "a command for a held-back unit is accepted" );
			const std::string szMessage = BkEditorLastMessage( pSession );
			Check( szMessage.find( "held back" ) != std::string::npos && szMessage.find( "reinforcement group" ) != std::string::npos, ( "and the answer warns: " + szMessage ).c_str() );
			printf( "editor-bridge: held-back unit says: %s\n", szMessage.c_str() );
			Check( BkEditorAddStartCommand( pSession, -1, &record, &nB ) == BK_EDITOR_OK && *BkEditorLastMessage( pSession ) == 0, "and one for a unit nobody holds says nothing" );
		}
		remove( OsPath( szMap ).c_str() );
	}

	// Back on coldwinter: a sizing pass and a capacity too short.
	if ( Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) &&
	     Check( BkEditorAddStartCommand( pSession, -1, &moved, both ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
	{
		BkEditorStartCommandRecord probe;
		int units2[2] = { -1, -1 };
		const int nLast = StartCommandCountOf( pSession ) - 1;
		Check( BkEditorStartCommand( pSession, nLast, &probe, 0, 0 ) == BK_EDITOR_REFUSED && probe.unit_count == 2, "the sizing pass of a command with units is REFUSED with its unit count" );
		Check( BkEditorStartCommand( pSession, nLast, &probe, units2, 1 ) == BK_EDITOR_REFUSED && probe.unit_count == 2 && units2[0] == nA && units2[1] == -1, "a capacity below the count writes what fits and never past it" );
		Check( BkEditorStartCommand( pSession, nLast, &probe, units2, 2 ) == BK_EDITOR_OK && units2[1] == nB, "and enough room reads both" );
	}
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szRefused ).c_str() );
	printf( "editor-bridge: M2 start commands ok\n" );
}

// WR-A10 (Research Pitfall 8): giving a group the script ID of a unit a start
// command names - or the unit the script ID a group holds - answers OK with a
// note that the game holds the unit back, so the command may find nothing.
static void TestM2GroupHoldWarning( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\group-hold-unedited.bzm";
	const std::string szAfter = szScratch + "\\group-hold-after.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	std::vector<int> units;
	int nNonUnit = -1;
	PickCommandUnits( original, 1, &units, &nNonUnit );
	if ( !Check( units.size() == 1, "coldwinter has a unit a start command may name" ) )
		return;
	const int nUnit = units[0];
	int nScriptBefore = -1;
	for ( size_t i = 0; i < original.objects.size(); ++i )
		if ( original.objects[i].link.nLinkID == nUnit )
			nScriptBefore = original.objects[i].nScriptID;
	const int nScript = 31999;
	int nGroup = -1;
	if ( !Check( BkEditorFirstFreeGroupID( pSession, 900, &nGroup ) == BK_EDITOR_OK && nGroup >= 900, BkEditorLastMessage( pSession ) ) )
		return;
	const int nIndex = StartCommandCountOf( pSession );
	const BkEditorStartCommandRecord stop = StartRecordOf( 9, 0, 0, 0, 0, 0, 1 );
	if ( !Check( BkEditorAddStartCommand( pSession, nIndex, &stop, &nUnit ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int ids[1] = { nScript };
	// The script ID first, then the group that holds it.
	Check( BkEditorSetObjectScriptID( pSession, nUnit, nScript ) == BK_EDITOR_OK && std::string( BkEditorLastMessage( pSession ) ).empty(),
	       NStr::Format( "a script ID no group holds gives no note: %s", BkEditorLastMessage( pSession ) ) );
	Check( BkEditorSetGroup( pSession, nGroup, ids, 1 ) == BK_EDITOR_OK && std::string( BkEditorLastMessage( pSession ) ).find( "held back by reinforcement group" ) != std::string::npos,
	       NStr::Format( "a group taking the script ID of a commanded unit notes it: %s", BkEditorLastMessage( pSession ) ) );
	// The group first, then the script ID.
	Check( BkEditorSetObjectScriptID( pSession, nUnit, nScriptBefore ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSetObjectScriptID( pSession, nUnit, nScript ) == BK_EDITOR_OK && std::string( BkEditorLastMessage( pSession ) ).find( "held back by reinforcement group" ) != std::string::npos,
	       NStr::Format( "a commanded unit taking a group's script ID notes it: %s", BkEditorLastMessage( pSession ) ) );
	// Back as the file was.
	Check( BkEditorDeleteGroup( pSession, nGroup ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSetObjectScriptID( pSession, nUnit, nScriptBefore ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorDeleteStartCommand( pSession, nIndex ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	if ( Check( BkEditorSaveMap( pSession, szAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szAfter ), "the hold-warning edits taken back save the unedited file byte for byte" );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szAfter ).c_str() );
	printf( "editor-bridge: M2 group hold warning ok\n" );
}

static std::vector<BkEditorCatalogueEntry> ReadCatalogueEntries( BkEditorSession *pSession )
{
	int nCount = 0;
	BkEditorCatalogue( pSession, 0, 0, &nCount );
	std::vector<BkEditorCatalogueEntry> entries( nCount > 0 ? nCount : 1 );
	int nRead = 0;
	if ( BkEditorCatalogue( pSession, &entries[0], nCount, &nRead ) != BK_EDITOR_OK )
		nRead = 0;
	entries.resize( nRead );
	return entries;
}

// The first free spot for a unit of player 0, spiralling out from (fX, fY) map units
// in steps of 120: the engine refuses a unit on another object.
static bool PlaceUnitNear( BkEditorSession *pSession, const std::string &szName, float fX, float fY, int *pnLink )
{
	for ( int nRing = 0; nRing < 14; ++nRing )
		for ( int nDx = -nRing; nDx <= nRing; ++nDx )
			for ( int nDy = -nRing; nDy <= nRing; ++nDy )
			{
				if ( ( abs( nDx ) > abs( nDy ) ? abs( nDx ) : abs( nDy ) ) != nRing )
					continue;
				if ( BkEditorAddObject( pSession, szName.c_str(), fX + nDx * 120.0f, fY + nDy * 120.0f, 0, 0, pnLink ) == BK_EDITOR_OK )
					return true;
			}
	return false;
}

static int ReserveRoleOfCatalogueName( BkEditorSession *pSession, const std::string &szName )
{
	int nRole = -1;
	return BkEditorReserveRole( pSession, szName.c_str(), &nRole ) == BK_EDITOR_OK ? nRole : -1;
}

static bool SameReservePositionRecords( const BkEditorReservePositionRecord &rLeft, const BkEditorReservePositionRecord &rRight )
{
	return rLeft.artillery_link_id == rRight.artillery_link_id && rLeft.truck_link_id == rRight.truck_link_id && rLeft.x == rRight.x && rLeft.y == rRight.y;
}

static int ReservePositionCountOf( BkEditorSession *pSession )
{
	int nCount = -2;
	if ( BkEditorReservePositionCount( pSession, &nCount ) != BK_EDITOR_OK )
		return -1;
	return nCount;
}

// D-18 on the real engine: the roles come from the object database's stats as the MFC
// editor classifies them; a towed gun with the truck that can tow it, and a
// self-propelled gun alone, add and save as the map NMapRecords builds; a towed gun
// without a truck, a self-propelled gun with one, a truck that cannot tow the gun, a
// squad or a non-unit in either role, link ID 0 as the gun, both 0 and a missing
// object or place are refused and change nothing; a file's own odd position is
// exempt; and everything deleted again saves the bytes it had.
static void TestM2ReservePositions( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( !Check( pObjectsDB != 0, "the object database is there" ) )
		return;

	// The roles, read through BkEditorReserveRole over the catalogue.
	int nRole = -2;
	Check( BkEditorReserveRole( pSession, 0, &nRole ) == BK_EDITOR_BAD_ARGUMENT && BkEditorReserveRole( pSession, "x", 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null name or role is a bad argument" );
	Check( BkEditorReserveRole( pSession, "No_Such_Object_At_All", &nRole ) == BK_EDITOR_OK && nRole == 0, "a name the database does not know is role 0" );
	const std::vector<BkEditorCatalogueEntry> catalogue = ReadCatalogueEntries( pSession );
	std::vector<std::string> towed, selfPropelled, trucks, squads, soldiers;
	std::vector<const SMechUnitRPGStats*> towedStats, truckStats;
	for ( size_t i = 0; i < catalogue.size(); ++i )
	{
		const std::string szName = catalogue[i].name;
		if ( catalogue[i].game_type == SGVOGT_SQUAD && catalogue[i].placeable != 0 )
			squads.push_back( szName );
		if ( catalogue[i].game_type != SGVOGT_UNIT )
			continue;
		const int nThis = ReserveRoleOfCatalogueName( pSession, szName );
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( szName.c_str() );
		const SMechUnitRPGStats *pStats = pDesc != 0 ? dynamic_cast<const SMechUnitRPGStats*>( pObjectsDB->GetRPGStats( pDesc ) ) : 0;
		if ( pDesc != 0 && pDesc->IsHuman() && catalogue[i].placeable == 0 )
			soldiers.push_back( szName );
		if ( catalogue[i].placeable == 0 )
			continue;
		if ( nThis == 1 )
			selfPropelled.push_back( szName );
		else if ( nThis == 2 && pStats != 0 )
		{
			towed.push_back( szName );
			towedStats.push_back( pStats );
		}
		else if ( nThis == 3 && pStats != 0 )
		{
			trucks.push_back( szName );
			truckStats.push_back( pStats );
		}
	}
	printf( "editor-bridge: reserve roles: %d towed guns, %d self-propelled, %d trucks, %d squads, %d single soldiers\n", int( towed.size() ), int( selfPropelled.size() ), int( trucks.size() ), int( squads.size() ), int( soldiers.size() ) );
	if ( !Check( !towed.empty() && !trucks.empty(), "the catalogue has a towed gun and a truck" ) )
		return;
	bool bSoldierRoleZero = true;
	for ( size_t i = 0; i < soldiers.size() && i < 20; ++i )
		bSoldierRoleZero = bSoldierRoleZero && ReserveRoleOfCatalogueName( pSession, soldiers[i] ) == 0;
	Check( bSoldierRoleZero, "a single soldier is role 0" );
	if ( !squads.empty() )
		Check( ReserveRoleOfCatalogueName( pSession, squads[0] ) == 0, "a squad is role 0" );

	// A gun and a truck that can tow it, and, where the stats offer one, a pair that cannot.
	int nGoodGun = -1, nGoodTruck = -1, nBadGun = -1, nBadTruck = -1;
	for ( size_t g = 0; g < towed.size(); ++g )
		for ( size_t t = 0; t < trucks.size(); ++t )
		{
			if ( truckStats[t]->fTowingForce > towedStats[g]->fWeight )
			{
				if ( nGoodGun < 0 ) { nGoodGun = int( g ); nGoodTruck = int( t ); }
			}
			else if ( nBadGun < 0 )
			{
				nBadGun = int( g );
				nBadTruck = int( t );
			}
		}
	if ( !Check( nGoodGun >= 0, "some truck can tow some towed gun" ) )
		return;

	// Place them on coldwinter, apart, round the middle of the map (AI units).
	const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize * fAITileXCoeff1 / 2.0f;
	const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize * fAITileYCoeff1 / 2.0f;
	int nGun = -1, nTruck = -1, nBadGunLink = -1, nBadTruckLink = -1, nSP = -1, nSquad = -1;
	bool bPlaced = PlaceUnitNear( pSession, towed[nGoodGun], fMiddleX - 500.0f, fMiddleY - 500.0f, &nGun ) && PlaceUnitNear( pSession, trucks[nGoodTruck], fMiddleX + 500.0f, fMiddleY - 500.0f, &nTruck );
	if ( !Check( bPlaced, "the towed gun and its truck are placed" ) )
		return;
	if ( nBadGun >= 0 )
		Check( PlaceUnitNear( pSession, towed[nBadGun], fMiddleX - 500.0f, fMiddleY + 500.0f, &nBadGunLink ) && PlaceUnitNear( pSession, trucks[nBadTruck], fMiddleX + 500.0f, fMiddleY + 500.0f, &nBadTruckLink ), "the pair that cannot tow is placed" );
	else
		printf( "editor-bridge: noted: every truck of the catalogue can tow every towed gun, the towing refusal is not exercised\n" );
	if ( !selfPropelled.empty() )
		Check( PlaceUnitNear( pSession, selfPropelled[0], fMiddleX, fMiddleY + 800.0f, &nSP ), "a self-propelled gun is placed" );
	else
		printf( "editor-bridge: noted: the catalogue has no self-propelled gun, its cases are not exercised\n" );
	if ( !squads.empty() )
		Check( PlaceUnitNear( pSession, squads[0], fMiddleX, fMiddleY - 800.0f, &nSquad ), "a squad is placed" );
	else
		printf( "editor-bridge: noted: the catalogue has no squad, its refusal is not exercised\n" );

	const std::string szPlaced = szScratch + "\\reserve-placed.bzm";
	const std::string szEdited = szScratch + "\\reserve-edited.bzm";
	const std::string szUndone = szScratch + "\\reserve-undone.bzm";
	const std::string szRefused = szScratch + "\\reserve-refused.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szPlaced.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo placed;
	if ( !Check( NMapFile::Read( szPlaced.c_str(), &placed, &szError ), szError.c_str() ) )
		return;
	const int nBefore = ReservePositionCountOf( pSession );
	Check( nBefore == int( original.reservePositionsList.size() ), "the positions read as the file has them" );

	// Argument checks.
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	BkEditorReservePositionRecord good = { nGun, nTruck, fMiddleX, fMiddleY };
	BkEditorReservePositionRecord nanPlace = { nGun, nTruck, fNaN, 0.0f };
	BkEditorReservePositionRecord unusedRecord;
	Check( BkEditorAddReservePosition( pSession, -1, 0 ) == BK_EDITOR_BAD_ARGUMENT && BkEditorSetReservePosition( pSession, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null record is a bad argument" );
	Check( BkEditorAddReservePosition( pSession, -1, &nanPlace ) == BK_EDITOR_BAD_ARGUMENT, "a NaN place is a bad argument" );
	Check( BkEditorAddReservePosition( pSession, -2, &good ) == BK_EDITOR_BAD_ARGUMENT && BkEditorAddReservePosition( pSession, nBefore + 1, &good ) == BK_EDITOR_BAD_ARGUMENT, "an insert index out of range is a bad argument" );
	Check( BkEditorSetReservePosition( pSession, nBefore, &good ) == BK_EDITOR_BAD_ARGUMENT && BkEditorDeleteReservePosition( pSession, nBefore ) == BK_EDITOR_BAD_ARGUMENT && BkEditorDeleteReservePosition( pSession, -1 ) == BK_EDITOR_BAD_ARGUMENT, "a set or delete index out of range is a bad argument" );
	Check( BkEditorReservePosition( pSession, nBefore, &unusedRecord ) == BK_EDITOR_BAD_ARGUMENT && BkEditorReservePosition( pSession, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT && BkEditorReservePositionCount( pSession, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a read past the end or to nothing is a bad argument" );

	// The towed gun with its truck: added, read back, saved as the expected map.
	if ( !Check( BkEditorAddReservePosition( pSession, -1, &good ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo expected = placed;
	Check( NMapRecords::InsertReservePosition( &expected, -1, SBattlePosition( nGun, nTruck, CVec2( fMiddleX, fMiddleY ) ) ), "the expected map takes the position" );
	BkEditorReservePositionRecord read;
	Check( ReservePositionCountOf( pSession ) == nBefore + 1 && BkEditorReservePosition( pSession, nBefore, &read ) == BK_EDITOR_OK && SameReservePositionRecords( read, good ), "the position reads back as it was given, appended" );
	if ( nSP >= 0 )
	{
		BkEditorReservePositionRecord alone = { nSP, 0, fMiddleX + 40.0f, fMiddleY + 40.0f };
		Check( BkEditorAddReservePosition( pSession, -1, &alone ) == BK_EDITOR_OK, ( std::string( "a self-propelled gun needs no truck: " ) + BkEditorLastMessage( pSession ) ).c_str() );
		Check( NMapRecords::InsertReservePosition( &expected, -1, SBattlePosition( nSP, 0, CVec2( fMiddleX + 40.0f, fMiddleY + 40.0f ) ) ), "the expected map takes it too" );
	}
	CheckSavedEquals( pSession, szEdited, expected, "reserve positions added" );
	// A set of the place moves it; a set back and a set to its own value are accepted.
	BkEditorReservePositionRecord moved = good;
	moved.x += 30.0f;
	Check( BkEditorSetReservePosition( pSession, nBefore, &moved ) == BK_EDITOR_OK && BkEditorSetReservePosition( pSession, nBefore, &good ) == BK_EDITOR_OK && BkEditorSetReservePosition( pSession, nBefore, &good ) == BK_EDITOR_OK,
	       "the place moves and goes back" );

	// Refusals change nothing.
	if ( !Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	BkEditorReservePositionRecord r = { nGun, 0, fMiddleX, fMiddleY };
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "towed gun needs a truck" ) != std::string::npos, "a towed gun without a truck is refused, saying so" );
	if ( nSP >= 0 )
	{
		r = BkEditorReservePositionRecord { nSP, nTruck, fMiddleX, fMiddleY };
		Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "takes no truck" ) != std::string::npos, "a self-propelled gun with a truck is refused" );
	}
	if ( nBadGunLink >= 0 && nBadTruckLink >= 0 )
	{
		r = BkEditorReservePositionRecord { nBadGunLink, nBadTruckLink, fMiddleX, fMiddleY };
		Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "cannot tow" ) != std::string::npos, "a truck that cannot tow the gun is refused, saying so" );
	}
	if ( nSquad >= 0 )
	{
		r = BkEditorReservePositionRecord { nSquad, nTruck, fMiddleX, fMiddleY };
		Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "squad" ) != std::string::npos, "a squad as the gun is refused" );
		r = BkEditorReservePositionRecord { nGun, nSquad, fMiddleX, fMiddleY };
		Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "squad" ) != std::string::npos, "a squad as the truck is refused" );
	}
	std::vector<int> others;
	int nNonUnit = -1;
	PickCommandUnits( original, 1, &others, &nNonUnit );
	if ( nNonUnit >= 0 )
	{
		r = BkEditorReservePositionRecord { nNonUnit, 0, fMiddleX, fMiddleY };
		Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED, "a building or a tree as the gun is refused" );
		r = BkEditorReservePositionRecord { nGun, nNonUnit, fMiddleX, fMiddleY };
		Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED, "and as the truck" );
	}
	r = BkEditorReservePositionRecord { nTruck, 0, fMiddleX, fMiddleY };
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED, "a truck as the gun is refused" );
	r = BkEditorReservePositionRecord { nGun, nGun, fMiddleX, fMiddleY };
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED, "and a gun as the truck" );
	r = BkEditorReservePositionRecord { 0, nTruck, fMiddleX, fMiddleY };
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED, "link ID 0 as the gun is refused" );
	r = BkEditorReservePositionRecord { 0, 0, fMiddleX, fMiddleY };
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "needs a gun" ) != std::string::npos, "a position with both link IDs 0 is refused" );
	r = BkEditorReservePositionRecord { 999999, 0, fMiddleX, fMiddleY };
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "999999" ) != std::string::npos, "a gun no object has is refused, naming it" );
	r = BkEditorReservePositionRecord { nGun, 999999, fMiddleX, fMiddleY };
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED, "so is a truck no object has" );
	r = BkEditorReservePositionRecord { nGun, -1, fMiddleX, fMiddleY };
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED, "and a negative truck" );
	r = BkEditorReservePositionRecord { nGun, nTruck, -5.0f, fMiddleY };
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "not on the map" ) != std::string::npos, "a place off the map is refused" );
	r.x = 1.0e9f;
	Check( BkEditorAddReservePosition( pSession, -1, &r ) == BK_EDITOR_REFUSED, "so is one far beyond the far edge" );
	// A set that would break a rule is refused too (the gun swapped for a truck).
	r = BkEditorReservePositionRecord { nTruck, nTruck, fMiddleX, fMiddleY };
	Check( BkEditorSetReservePosition( pSession, nBefore, &r ) == BK_EDITOR_REFUSED, "a set that makes the truck the gun is refused" );
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szEdited, szRefused ), "none of the refusals changed the map" );

	// Deleted again, the map is the placed one; put back at their own indexes they are as they were.
	const int nAdded = ReservePositionCountOf( pSession ) - nBefore;
	for ( int i = 0; i < nAdded; ++i )
		Check( BkEditorDeleteReservePosition( pSession, nBefore ) == BK_EDITOR_OK, "a position is deleted" );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szPlaced, szUndone ), "add, edit and delete of reserve positions save the placed map byte for byte" );
	Check( BkEditorAddReservePosition( pSession, nBefore, &good ) == BK_EDITOR_OK && BkEditorReservePosition( pSession, nBefore, &read ) == BK_EDITOR_OK && SameReservePositionRecords( read, good ), "a deleted position goes back at its own index" );
	Check( BkEditorDeleteReservePosition( pSession, nBefore ) == BK_EDITOR_OK, "and is deleted once more" );

	// Deleting the gun takes its position with it, and the restore brings it back (the cascade).
	Check( BkEditorAddReservePosition( pSession, -1, &good ) == BK_EDITOR_OK, "the position is there again" );
	{
		const BkEditorStatus deleted = BkEditorDeleteObject( pSession, nGun );
		const std::string szCascade = BkEditorLastMessage( pSession );
		Check( deleted == BK_EDITOR_OK, ( "deleting the gun is accepted: " + szCascade ).c_str() );
		Check( ReservePositionCountOf( pSession ) == nBefore, NStr::Format( "deleting the gun erases its position (%d positions, %d before)", ReservePositionCountOf( pSession ), nBefore ) );
		Check( szCascade.find( "reserve position" ) != std::string::npos, ( "and says so: " + szCascade ).c_str() );
		printf( "editor-bridge: deleting the gun says: %s\n", szCascade.c_str() );
	}
	Check( BkEditorRestoreObject( pSession, nGun ) == BK_EDITOR_OK && ReservePositionCountOf( pSession ) == nBefore + 1 && BkEditorReservePosition( pSession, nBefore, &read ) == BK_EDITOR_OK && SameReservePositionRecords( read, good ),
	       "and the restore brings it back as it was" );
	Check( BkEditorDeleteReservePosition( pSession, nBefore ) == BK_EDITOR_OK, "deleted for the next test" );

	// A file's own odd position - a gun no object has - is exempt: deleted and put back, as an undo does.
	{
		const std::string szMap = szScratch + "\\reserve-odd.bzm";
		CMapInfo odd = original;
		Check( NMapRecords::InsertReservePosition( &odd, -1, SBattlePosition( 888888, 777777, CVec2( 10.0f, 10.0f ) ) ), "the odd position is laid over the map" );
		if ( Check( NMapFile::Write( szMap.c_str(), odd, &szError ), szError.c_str() ) &&
		     Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		{
			const std::string szBefore = szScratch + "\\reserve-odd-before.bzm", szAfter = szScratch + "\\reserve-odd-after.bzm";
			Check( BkEditorSaveMap( pSession, szBefore.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			const int nOdd = ReservePositionCountOf( pSession ) - 1;
			BkEditorReservePositionRecord oddRead;
			if ( Check( nOdd >= 0 && BkEditorReservePosition( pSession, nOdd, &oddRead ) == BK_EDITOR_OK, "the odd position reads" ) )
			{
				Check( BkEditorDeleteReservePosition( pSession, nOdd ) == BK_EDITOR_OK && BkEditorAddReservePosition( pSession, nOdd, &oddRead ) == BK_EDITOR_OK, "the file's own odd position is deleted and put back, as an undo needs" );
				BkEditorReservePositionRecord fresh = oddRead;
				fresh.x = 20.0f;
				Check( BkEditorAddReservePosition( pSession, -1, &fresh ) == BK_EDITOR_REFUSED, "but a new one like it is refused" );
				Check( BkEditorSetReservePosition( pSession, nOdd, &fresh ) == BK_EDITOR_OK && BkEditorSetReservePosition( pSession, nOdd, &oddRead ) == BK_EDITOR_OK, "and moving only its place is accepted, and back" );
			}
			if ( Check( BkEditorSaveMap( pSession, szAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
				Check( SameBytes( szBefore, szAfter ), "the odd map saves the same bytes after all of it" );
			remove( OsPath( szBefore ).c_str() );
			remove( OsPath( szAfter ).c_str() );
		}
		remove( OsPath( szMap ).c_str() );
	}
	remove( OsPath( szPlaced ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szRefused ).c_str() );
	printf( "editor-bridge: M2 reserve positions ok\n" );
}

// The AI general (04-12, D-19): one side read in two passes. The sizing pass is
// BK_EDITOR_REFUSED when the side holds anything (as a buffer too short is) and still
// answers the counts; nothing is indexed before the counts are known.
struct SAISideRead
{
	BkEditorAISideInfo info;
	std::vector<int> mobile;
	std::vector<BkEditorAIParcel> parcels;
	std::vector<BkEditorAIPoint> points;
};

static bool ReadAISideOf( BkEditorSession *pSession, int nSide, SAISideRead *pRead )
{
	memset( &pRead->info, 0, sizeof pRead->info );
	const BkEditorStatus sizing = BkEditorAIGeneralSide( pSession, nSide, &pRead->info, 0, 0, 0, 0, 0, 0 );
	if ( sizing != BK_EDITOR_OK && sizing != BK_EDITOR_REFUSED )
		return false;
	const BkEditorAISideInfo counted = pRead->info;
	if ( counted.mobile_count < 0 || counted.parcel_count < 0 || counted.point_count < 0 || counted.side_count < 0 )
		return false;
	pRead->mobile.assign( counted.mobile_count, 0 );
	pRead->parcels.assign( counted.parcel_count, BkEditorAIParcel() );
	pRead->points.assign( counted.point_count, BkEditorAIPoint() );
	const BkEditorStatus read = BkEditorAIGeneralSide( pSession, nSide, &pRead->info,
		pRead->mobile.empty() ? 0 : &pRead->mobile[0], counted.mobile_count,
		pRead->parcels.empty() ? 0 : &pRead->parcels[0], counted.parcel_count,
		pRead->points.empty() ? 0 : &pRead->points[0], counted.point_count );
	return read == BK_EDITOR_OK && pRead->info.side_count == counted.side_count && pRead->info.parcel_count == counted.parcel_count;
}

static BkEditorStatus PutAISideOf( BkEditorSession *pSession, int nSide, int nSideCount, const SAISideRead &rSide )
{
	return BkEditorSetAIGeneralSide( pSession, nSide, nSideCount,
		rSide.mobile.empty() ? 0 : &rSide.mobile[0], int( rSide.mobile.size() ),
		rSide.parcels.empty() ? 0 : &rSide.parcels[0], int( rSide.parcels.size() ),
		rSide.points.empty() ? 0 : &rSide.points[0], int( rSide.points.size() ) );
}

// The side as the map file holds it, in the record form the ABI has.
static SAISideRead AISideOfMap( const CMapInfo &rMap, int nSide )
{
	SAISideRead side;
	memset( &side.info, 0, sizeof side.info );
	side.info.side_count = int( rMap.aiGeneralMapInfo.sidesInfo.size() );
	if ( nSide < 0 || nSide >= side.info.side_count )
		return side;
	const SAIGeneralSideInfo &rInfo = rMap.aiGeneralMapInfo.sidesInfo[nSide];
	side.mobile = rInfo.mobileScriptIDs;
	for ( size_t i = 0; i < rInfo.parcels.size(); ++i )
	{
		const SAIGeneralParcelInfo &rParcel = rInfo.parcels[i];
		BkEditorAIParcel parcel;
		memset( &parcel, 0, sizeof parcel );
		parcel.type = rParcel.eType;
		parcel.cx = rParcel.vCenter.x;
		parcel.cy = rParcel.vCenter.y;
		parcel.radius = rParcel.fRadius;
		parcel.defence_dir = int( rParcel.wDefenceDirection );
		parcel.first_point = int( side.points.size() );
		parcel.point_count = int( rParcel.reinforcePoints.size() );
		for ( size_t j = 0; j < rParcel.reinforcePoints.size(); ++j )
		{
			BkEditorAIPoint point;
			point.x = rParcel.reinforcePoints[j].vCenter.x;
			point.y = rParcel.reinforcePoints[j].vCenter.y;
			point.dir = int( rParcel.reinforcePoints[j].wDir );
			side.points.push_back( point );
		}
		side.parcels.push_back( parcel );
	}
	return side;
}

static BkEditorAIParcel DefenceParcelAt( float fX, float fY )
{
	BkEditorAIParcel parcel;
	memset( &parcel, 0, sizeof parcel );
	parcel.type = SAIGeneralParcelInfo::EPATCH_DEFENCE;
	parcel.cx = fX;
	parcel.cy = fY;
	parcel.radius = 256.0f;
	return parcel;
}

// WR-A03 (04 review): a put whose side count is below the map's drops the
// sides above it. Only empty ones may go (the undo of a put that created
// sides); a put that would drop a side holding parcels or script IDs is
// refused and changes nothing.
static void TestM2AISideShrinkRefused( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\ai-shrink-unedited.bzm";
	const std::string szAfter = szScratch + "\\ai-shrink-after.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	SAISideRead side0, side1;
	if ( !Check( ReadAISideOf( pSession, 0, &side0 ) && ReadAISideOf( pSession, 1, &side1 ), "sides 0 and 1 read" ) )
		return;
	const int nCount = side0.info.side_count;
	if ( !Check( nCount >= 2, "coldwinter has two AI sides" ) )
		return;
	const float fX = map.terrain.tiles.GetSizeX() * fWorldCellSize * fAITileXCoeff1 / 2.0f;
	const float fY = map.terrain.tiles.GetSizeY() * fWorldCellSize * fAITileYCoeff1 / 2.0f;
	SAISideRead held = side1;
	held.parcels.push_back( DefenceParcelAt( fX, fY ) );
	held.parcels.back().first_point = int( held.points.size() );
	if ( !Check( PutAISideOf( pSession, 1, nCount, held ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( PutAISideOf( pSession, 0, 1, side0 ) == BK_EDITOR_REFUSED, "a put of side 0 with count 1 would drop side 1's parcel: refused" );
	Check( std::string( BkEditorLastMessage( pSession ) ).find( "cannot drop" ) != std::string::npos, "and says why" );
	Check( PutAISideOf( pSession, 0, 0, SAISideRead() ) == BK_EDITOR_REFUSED, "a put with count 0 is refused too" );
	SAISideRead again;
	Check( ReadAISideOf( pSession, 1, &again ) && again.info.side_count == nCount && again.parcels.size() == held.parcels.size(), "side 1 and the side count are unchanged" );
	// The undo of a put that created sides: side count+1 with a parcel, then
	// side count+1 empty under the old count - the side between is empty, so
	// it goes.
	SAISideRead created;
	created.parcels.push_back( DefenceParcelAt( fX, fY ) );
	if ( Check( PutAISideOf( pSession, nCount + 1, nCount + 2, created ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( PutAISideOf( pSession, nCount + 1, nCount, SAISideRead() ) == BK_EDITOR_OK, "a shrink over sides a put created (empty ones, and the put's own) goes through" );
	Check( PutAISideOf( pSession, 1, nCount, side1 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	if ( Check( BkEditorSaveMap( pSession, szAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szAfter ), "the AI sides put back save the unedited file byte for byte" );
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szAfter ).c_str() );
	printf( "editor-bridge: M2 AI side shrink refused ok\n" );
}

// The map with `side` set, through the map tier's own put: what the saved file must equal.
static void PutExpectedAISide( CMapInfo *pMap, int nSide, int nSideCount, const SAISideRead &rSide )
{
	NMapRecords::SAIGeneralSidePut put;
	put.nSideCount = nSideCount;
	put.nSide = nSide;
	for ( size_t i = 0; i < rSide.mobile.size(); ++i )
		put.info.mobileScriptIDs.push_back( rSide.mobile[i] );
	for ( size_t i = 0; i < rSide.parcels.size(); ++i )
	{
		const BkEditorAIParcel &rParcel = rSide.parcels[i];
		SAIGeneralParcelInfo parcel;
		parcel.eType = rParcel.type;
		parcel.vCenter = CVec2( rParcel.cx, rParcel.cy );
		parcel.fRadius = rParcel.radius;
		parcel.wDefenceDirection = WORD( rParcel.defence_dir );
		for ( int j = 0; j < rParcel.point_count; ++j )
		{
			const BkEditorAIPoint &rPoint = rSide.points[rParcel.first_point + j];
			parcel.reinforcePoints.push_back( SAIGeneralParcelInfo::SReinforcePointInfo( CVec2( rPoint.x, rPoint.y ), WORD( rPoint.dir ) ) );
		}
		put.info.parcels.push_back( parcel );
	}
	NMapRecords::PutAIGeneralSide( pMap, put );
}

// One step of the gesture test: the side put, the expected map built by the map tier's own
// put, the saved map compared with it, the side read back, and the state kept for the undo.
static void SaveAIGestureStep( BkEditorSession *pSession, int nSide, int nCount, SAISideRead *pCurrent, CMapInfo *pExpected,
                               std::vector<SAISideRead> *pHistory, const std::string &szPath, const char *pszWhat )
{
	Check( PutAISideOf( pSession, nSide, nCount, *pCurrent ) == BK_EDITOR_OK, NStr::Format( "%s is accepted: %s", pszWhat, BkEditorLastMessage( pSession ) ) );
	PutExpectedAISide( pExpected, nSide, nCount, *pCurrent );
	CheckSavedEquals( pSession, szPath, *pExpected, pszWhat );
	SAISideRead again;
	if ( Check( ReadAISideOf( pSession, nSide, &again ), NStr::Format( "%s reads back", pszWhat ) ) )
		Check( again.parcels.size() == pCurrent->parcels.size() && again.points.size() == pCurrent->points.size() && again.mobile == pCurrent->mobile,
		       NStr::Format( "%s reads back as it was put", pszWhat ) );
	pHistory->push_back( *pCurrent );
}

static void TestM2AIGeneral( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const int nSides0 = int( original.aiGeneralMapInfo.sidesInfo.size() );
	const float fMiddleX = original.terrain.tiles.GetSizeX() * fWorldCellSize * fAITileXCoeff1 / 2.0f;
	const float fMiddleY = original.terrain.tiles.GetSizeY() * fWorldCellSize * fAITileYCoeff1 / 2.0f;
	printf( "editor-bridge: ai general: the map has %d sides\n", nSides0 );

	const std::string szUnedited = szScratch + "\\aigen-unedited.bzm";
	const std::string szEdited = szScratch + "\\aigen-edited.bzm";
	const std::string szUndone = szScratch + "\\aigen-undone.bzm";
	const std::string szRefused = szScratch + "\\aigen-refused.bzm";
	if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;

	// Argument checks.
	BkEditorAISideInfo info;
	int nScratchInt = 0;
	BkEditorAIParcel scratchParcel;
	Check( BkEditorAIGeneralSide( pSession, 0, 0, 0, 0, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a read with no info is a bad argument" );
	Check( BkEditorAIGeneralSide( pSession, -1, &info, 0, 0, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a negative side is a bad argument" );
	Check( BkEditorAIGeneralSide( pSession, 0, &info, 0, 1, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT &&
	       BkEditorAIGeneralSide( pSession, 0, &info, &nScratchInt, -1, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT &&
	       BkEditorAIGeneralSide( pSession, 0, &info, 0, 0, 0, 1, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT &&
	       BkEditorAIGeneralSide( pSession, 0, &info, 0, 0, &scratchParcel, -1, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a capacity with no array, or a negative one, is a bad argument" );

	// A side at or above the count reads empty with the current count.
	SAISideRead beyond;
	Check( ReadAISideOf( pSession, nSides0 + 5, &beyond ) && beyond.info.side_count == nSides0 && beyond.parcels.empty() && beyond.mobile.empty() && beyond.points.empty(),
	       "a side the map does not have reads empty, with the side count" );
	// The sides the file has read as the map tier has them.
	for ( int side = 0; side < nSides0; ++side )
	{
		SAISideRead held;
		const SAISideRead expectedSide = AISideOfMap( original, side );
		Check( ReadAISideOf( pSession, side, &held ) && held.parcels.size() == expectedSide.parcels.size() && held.mobile == expectedSide.mobile && held.points.size() == expectedSide.points.size(),
		       NStr::Format( "side %d reads as the file has it", side ) );
	}

	// A defence parcel of radius 256 on side 1 (created with the sides below it when the map lacks them).
	const int nSideA = 1;
	const int nCountA = Max( nSides0, nSideA + 1 );
	SAISideRead sideA;
	ReadAISideOf( pSession, nSideA, &sideA );
	const int nParcelsA = int( sideA.parcels.size() );
	sideA.parcels.push_back( DefenceParcelAt( fMiddleX, fMiddleY ) );
	sideA.parcels.back().first_point = int( sideA.points.size() );
	SAISideRead sideABefore;
	ReadAISideOf( pSession, nSideA, &sideABefore );
	if ( !Check( PutAISideOf( pSession, nSideA, nCountA, sideA ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo expected = original;
	PutExpectedAISide( &expected, nSideA, nCountA, sideA );
	{
		SAISideRead again;
		Check( ReadAISideOf( pSession, nSideA, &again ) && again.info.side_count == nCountA && int( again.parcels.size() ) == nParcelsA + 1, "the parcel reads back on its side, and the side count is at least 2" );
		if ( int( again.parcels.size() ) == nParcelsA + 1 )
			Check( again.parcels[nParcelsA].type == 1 && again.parcels[nParcelsA].cx == fMiddleX && again.parcels[nParcelsA].cy == fMiddleY && again.parcels[nParcelsA].radius == 256.0f && again.parcels[nParcelsA].defence_dir == 0 && again.parcels[nParcelsA].point_count == 0,
			       "the parcel is a defence parcel of radius 256 and direction 0, as it was given" );
	}
	CheckSavedEquals( pSession, szEdited, expected, "a defence parcel on side 1" );

	// A side two above the current count: the sides between come out empty, and undo takes them all away.
	const int nSideB = nCountA + 1;
	const int nCountB = nSideB + 1;
	SAISideRead sideB;
	sideB.parcels.push_back( DefenceParcelAt( fMiddleX + 200.0f, fMiddleY + 200.0f ) );
	if ( !Check( PutAISideOf( pSession, nSideB, nCountB, sideB ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	PutExpectedAISide( &expected, nSideB, nCountB, sideB );
	{
		SAISideRead between, top;
		Check( ReadAISideOf( pSession, nCountA, &between ) && between.info.side_count == nCountB && between.parcels.empty() && between.mobile.empty(), "the side between comes out empty" );
		Check( ReadAISideOf( pSession, nSideB, &top ) && top.parcels.size() == 1, "and the side asked for holds the parcel" );
	}
	CheckSavedEquals( pSession, szEdited, expected, "a parcel on a side two above the count" );

	// Undo, as the core does: put each side back with the count it had.
	SAISideRead emptySide;
	Check( PutAISideOf( pSession, nSideB, nCountA, emptySide ) == BK_EDITOR_OK, "the created sides are put away with the old count" );
	Check( PutAISideOf( pSession, nSideA, nSides0, sideABefore ) == BK_EDITOR_OK, "and the first side goes back with the count the map had" );
	if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szUnedited, szUndone ), ( std::string( "undoing both saves the unedited map byte for byte (the side count restored): " ) + DescribeDifference( szUnedited, szUndone ) ).c_str() );

	// Refusals change nothing.
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	SAISideRead base;
	ReadAISideOf( pSession, nSideA, &base );
	const int nCountBase = base.info.side_count;
	if ( !Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	{
		SAISideRead bad = base;
		bad.parcels.push_back( DefenceParcelAt( fMiddleX, fMiddleY ) );
		bad.parcels.back().first_point = int( bad.points.size() );
		const int nLast = int( bad.parcels.size() ) - 1;
		const int nCountForBad = Max( nCountBase, nSideA + 1 );
		bad.parcels[nLast].type = 3;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "type 3" ) != std::string::npos, "a type 3 is refused, saying so" );
		bad.parcels[nLast].type = 0;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED, "and so is type 0" );
		bad.parcels[nLast].type = 1;
		bad.parcels[nLast].defence_dir = 70000;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_BAD_ARGUMENT, "a direction of 70000 is a bad argument" );
		bad.parcels[nLast].defence_dir = -1;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_BAD_ARGUMENT, "and so is a negative one" );
		bad.parcels[nLast].defence_dir = 0;
		bad.parcels[nLast].cx = fNaN;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED, "a NaN centre is refused" );
		bad.parcels[nLast].cx = -5.0f;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "not on the map" ) != std::string::npos, "a centre off the map is refused, saying so" );
		bad.parcels[nLast].cx = 1.0e9f;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED, "and so is one far beyond the far edge" );
		bad.parcels[nLast].cx = fMiddleX;
		bad.parcels[nLast].radius = 0.0f;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED, "a radius of 0 is refused" );
		bad.parcels[nLast].radius = -4.0f;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED, "and a negative one" );
		bad.parcels[nLast].radius = fNaN;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED, "and a NaN one" );
		bad.parcels[nLast].radius = 256.0f;
		bad.parcels[nLast].point_count = 1;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_BAD_ARGUMENT, "a point range outside the points array is a bad argument" );
		bad.parcels[nLast].first_point = -1;
		bad.parcels[nLast].point_count = 0;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_BAD_ARGUMENT, "and so is a negative first point" );
		bad.parcels[nLast].first_point = int( bad.points.size() );
		bad.points.push_back( BkEditorAIPoint() );
		bad.points.back().x = fNaN;
		bad.parcels[nLast].point_count = 1;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED, "a point that is not a number is refused" );
		bad.points.back().x = 0.0f;
		bad.points.back().dir = 70000;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_BAD_ARGUMENT, "a point direction of 70000 is a bad argument" );
		bad = base;
		bad.mobile.push_back( 32001 );
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED, "a mobile script ID above 32000 is refused" );
		bad.mobile.back() = -1;
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED, "and -1" );
		bad.mobile.back() = 4245;
		bad.mobile.push_back( 4245 );
		Check( PutAISideOf( pSession, nSideA, nCountForBad, bad ) == BK_EDITOR_REFUSED && std::string( BkEditorLastMessage( pSession ) ).find( "twice" ) != std::string::npos, "a script ID twice is refused, saying so" );
		// A side at or above the count holds nothing; a side or count out of range is a caller bug.
		bad = base;
		bad.parcels.push_back( DefenceParcelAt( fMiddleX, fMiddleY ) );
		Check( PutAISideOf( pSession, nCountBase + 2, nCountBase + 2, bad ) == BK_EDITOR_REFUSED, "a side at the count that holds a parcel is refused" );
		Check( PutAISideOf( pSession, -1, nCountBase, base ) == BK_EDITOR_BAD_ARGUMENT && PutAISideOf( pSession, 1024, 1024, base ) == BK_EDITOR_BAD_ARGUMENT &&
		       PutAISideOf( pSession, 0, 1025, base ) == BK_EDITOR_BAD_ARGUMENT && PutAISideOf( pSession, 0, -1, base ) == BK_EDITOR_BAD_ARGUMENT, "a side or a side count out of range is a bad argument" );
		Check( BkEditorSetAIGeneralSide( pSession, 0, 2, 0, 1, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT && BkEditorSetAIGeneralSide( pSession, 0, 2, 0, 0, 0, 1, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT &&
		       BkEditorSetAIGeneralSide( pSession, 0, 2, 0, -1, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "counts with no arrays, or negative counts, are bad arguments" );
	}
	if ( Check( BkEditorSaveMap( pSession, szRefused.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( SameBytes( szEdited, szRefused ), "none of the refusals changed the map" );

	// ---- The MFC gestures (04-12 Task 2): every value comes from NMapGeometry's functions, the
	// saved map is compared with the expected map the map tier's own put builds, and undoing it all
	// saves the unedited bytes. ----
	{
		SAISideRead start;
		if ( !Check( ReadAISideOf( pSession, nSideA, &start ), "side 1 reads for the gestures" ) )
			return;
		const int nCountE = Max( start.info.side_count, nSideA + 1 );
		SAISideRead current = start;
		CMapInfo expectedE = original;
		const float fCentreX0 = float( int( fMiddleX ) ), fCentreY0 = float( int( fMiddleY ) );
		const CVec2 vCentre0( fCentreX0, fCentreY0 );
		std::vector<SAISideRead> history;
		history.push_back( start );
		int nParcel = -1;

		// The parcel, then a point from a click inside it.
		current.parcels.push_back( DefenceParcelAt( vCentre0.x, vCentre0.y ) );
		nParcel = int( current.parcels.size() ) - 1;
		current.parcels[nParcel].first_point = int( current.points.size() );
		SaveAIGestureStep( pSession, nSideA, nCountE, &current, &expectedE, &history, szEdited, "the parcel" );
		const CVec2 vClickVis( ( vCentre0.x + 100.0f ) * fAITileXCoeff, ( vCentre0.y + 50.0f ) * fAITileXCoeff );
		const CVec2 vPointRel = NMapGeometry::ParcelPointFromVis( vClickVis, vCentre0, 0 );
		Check( vPointRel.x == 100.0f && vPointRel.y == 50.0f, NStr::Format( "the click is 100, 50 from the centre, stored (%.3f, %.3f)", vPointRel.x, vPointRel.y ) );
		BkEditorAIPoint point0;
		point0.x = vPointRel.x;
		point0.y = vPointRel.y;
		point0.dir = 0;
		current.points.push_back( point0 );
		++current.parcels[nParcel].point_count;
		SaveAIGestureStep( pSession, nSideA, nCountE, &current, &expectedE, &history, szEdited, "a reinforce point" );

		// The centre moved: the point is relative, so it does not change.
		CVec2 vNewCentre( ( vCentre0.x + 64.0f ) * fAITileXCoeff, ( vCentre0.y - 32.0f ) * fAITileXCoeff );
		Vis2AI( &vNewCentre );
		current.parcels[nParcel].cx = vNewCentre.x;
		current.parcels[nParcel].cy = vNewCentre.y;
		SaveAIGestureStep( pSession, nSideA, nCountE, &current, &expectedE, &history, szEdited, "a moved centre" );

		// The arrow dragged to 300 left and 400 up the y axis of the new centre: radius 500 and its direction.
		const CVec2 vCentre1( vNewCentre.x, vNewCentre.y );
		const CVec2 vArrow( vCentre1.x - 300.0f, vCentre1.y + 400.0f );
		const float fRadius = NMapGeometry::RadiusFromArrow( vCentre1, vArrow );
		const WORD wDir = NMapGeometry::DirectionFromArrow( vCentre1, vArrow );
		Check( fRadius == 500.0f && wDir != 0, NStr::Format( "the arrow gives radius %.2f and direction %d", fRadius, int( wDir ) ) );
		current.parcels[nParcel].radius = fRadius;
		current.parcels[nParcel].defence_dir = int( wDir );
		SaveAIGestureStep( pSession, nSideA, nCountE, &current, &expectedE, &history, szEdited, "a new radius and direction" );
		// An arrow nearer than 256 keeps the floor.
		Check( NMapGeometry::RadiusFromArrow( vCentre1, CVec2( vCentre1.x, vCentre1.y + 40.0f ) ) == 256.0f, "an arrow within 256 gives 256" );

		// A second point clicked in the turned parcel is stored turned back, and its arrow sets its direction.
		const CVec2 vClickVis2( ( vCentre1.x + 60.0f ) * fAITileXCoeff, ( vCentre1.y + 120.0f ) * fAITileXCoeff );
		const CVec2 vPoint2 = NMapGeometry::ParcelPointFromVis( vClickVis2, vCentre1, wDir );
		BkEditorAIPoint point1;
		point1.x = vPoint2.x;
		point1.y = vPoint2.y;
		point1.dir = int( NMapGeometry::DirectionFromArrow( vPoint2, CVec2( vPoint2.x, vPoint2.y + 90.0f ) ) );
		current.points.push_back( point1 );
		++current.parcels[nParcel].point_count;
		SaveAIGestureStep( pSession, nSideA, nCountE, &current, &expectedE, &history, szEdited, "a point in the turned parcel" );
		{
			const CVec2 vBack = NMapGeometry::ParcelPointToVis( vPoint2, vCentre1, wDir );
			Check( std::fabs( vBack.x - vClickVis2.x ) < 1.5f && std::fabs( vBack.y - vClickVis2.y ) < 1.5f,
			       NStr::Format( "the stored point is drawn where it was clicked (%.2f, %.2f) for (%.2f, %.2f)", vBack.x, vBack.y, vClickVis2.x, vClickVis2.y ) );
		}
		// The first point's own arrow dragged: its direction from the arrow's place relative to it.
		const int nFirstPoint = current.parcels[nParcel].first_point;
		current.points[nFirstPoint].dir = int( NMapGeometry::DirectionFromArrow( CVec2( current.points[nFirstPoint].x, current.points[nFirstPoint].y ), CVec2( current.points[nFirstPoint].x + 50.0f, current.points[nFirstPoint].y - 70.0f ) ) );
		SaveAIGestureStep( pSession, nSideA, nCountE, &current, &expectedE, &history, szEdited, "a point's direction" );

		// The type switched to reinforce.
		current.parcels[nParcel].type = SAIGeneralParcelInfo::EPATCH_REINFORCE;
		SaveAIGestureStep( pSession, nSideA, nCountE, &current, &expectedE, &history, szEdited, "a type switch" );

		// The first point deleted: the points after it close up and the parcel's range shrinks.
		current.points.erase( current.points.begin() + nFirstPoint );
		--current.parcels[nParcel].point_count;
		SaveAIGestureStep( pSession, nSideA, nCountE, &current, &expectedE, &history, szEdited, "a point deleted" );
		// The parcel deleted, then everything undone in reverse: each put back is accepted and
		// the last saves the unedited bytes.
		current.points.resize( nFirstPoint );
		current.parcels.pop_back();
		SaveAIGestureStep( pSession, nSideA, nCountE, &current, &expectedE, &history, szEdited, "a parcel deleted" );
		bool bUndone = true;
		for ( size_t i = history.size() - 1; i-- > 0; )
			bUndone = bUndone && PutAISideOf( pSession, nSideA, i == 0 ? start.info.side_count : nCountE, history[i] ) == BK_EDITOR_OK;
		Check( bUndone, "every step is put back in reverse" );
		if ( Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( SameBytes( szUnedited, szUndone ), ( std::string( "undoing every gesture saves the unedited map byte for byte: " ) + DescribeDifference( szUnedited, szUndone ) ).c_str() );
		printf( "editor-bridge: M2 ai general edits ok\n" );
	}

	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	remove( OsPath( szRefused ).c_str() );
	printf( "editor-bridge: M2 ai general ok\n" );
}

// ---------------------------------------------------------------------------
// D-25 item 4, the engine half (04-13, `--m2-sweep`, the local step
// test-editor-bridge-m2-sweep): every Data\Maps map is opened through the
// bridge and saved unedited; then, where the map allows it, a bridge, a fence
// run and an entrenchment are drawn on free ground, one shipped bridge and one
// shipped entrenchment are deleted whole, and a unit a start command or a
// reserve position names is deleted with its cascade; everything is undone in
// reverse through BkEditorUndoEdit / BkEditorRestoreObject and the map saved
// again: the two saves must be the same bytes. A refusal (a crowded map, a
// garrisoned trench, a unit carrying a passenger) changes nothing and is
// counted, never a failure.
// ---------------------------------------------------------------------------
struct SSweepUndo
{
	int nToken;				// an edit of the edit log, or -1
	int nRestoreLink;	// an object BkEditorRestoreObject puts back, or -1
	std::string szKind;
};

// Free-ground candidates spread over the map, WORLD units.
static std::vector<CVec2> SweepSpots( const CMapInfo &rMap )
{
	const float fWidth = rMap.terrain.tiles.GetSizeX() * fWorldCellSize;
	const float fHeight = rMap.terrain.tiles.GetSizeY() * fWorldCellSize;
	const float fractions[][2] = { { 0.5f, 0.5f }, { 0.3f, 0.3f }, { 0.7f, 0.3f }, { 0.3f, 0.7f }, { 0.7f, 0.7f }, { 0.2f, 0.5f }, { 0.8f, 0.5f }, { 0.5f, 0.2f }, { 0.5f, 0.8f } };
	std::vector<CVec2> spots;
	for ( size_t i = 0; i < sizeof( fractions ) / sizeof( fractions[0] ); ++i )
		spots.push_back( CVec2( fWidth * fractions[i][0], fHeight * fractions[i][1] ) );
	return spots;
}

static void TestM2Sweep( BkEditorSession *pSession, const std::string &szScratch )
{
	const std::vector<std::string> maps = SortedShippedMaps();
	printf( "editor-bridge: M2 sweep over %d maps\n", int( maps.size() ) );
	if ( !Check( maps.size() >= 50, "the M2 sweep found the shipped maps (is Data staged?)" ) )
		return;
	const int nFailuresBefore = g_nFailures;
	const std::string szUnedited = szScratch + "\\m2-sweep-unedited.bzm";
	const std::string szEdited = szScratch + "\\m2-sweep-edited.bzm";
	const std::string szUndone = szScratch + "\\m2-sweep-undone.bzm";
	int nEdits = 0, nMaps = 0;
	std::map<std::string, int> done, refused;
	for ( size_t m = 0; m < maps.size(); ++m )
	{
		const std::string &szMap = maps[m];
		const std::string szName = "M2 sweep (" + szMap + "): ";
		CMapInfo map;
		std::string szError;
		if ( !Check( NMapFile::Read( szMap.c_str(), &map, &szError ), ( szName + "the map reads: " + szError ).c_str() ) )
			continue;
		if ( !Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, ( szName + "opens: " + BkEditorLastMessage( pSession ) ).c_str() ) )
			continue;
		if ( !Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, ( szName + "saves unedited: " + BkEditorLastMessage( pSession ) ).c_str() ) )
			continue;
		++nMaps;
		const std::vector<CVec2> spots = SweepSpots( map );
		std::vector<SSweepUndo> undos;
		const int nShippedBridges = int( ReadBridges( pSession ).size() );
		const int nShippedTrenches = int( ReadTrenches( pSession ).size() );
		// A bridge, a fence run and an entrenchment on free ground.
		{
			SSweepUndo undo = { -1, -1, "bridge drawn" };
			int nIndex = -1;
			for ( size_t i = 0; i < spots.size() && undo.nToken < 0; ++i )
				if ( BkEditorDrawBridge( pSession, "W_WoodenBig_Heavy_01", spots[i].x - 150.0f, spots[i].y, spots[i].x + 150.0f, spots[i].y, &undo.nToken, &nIndex ) != BK_EDITOR_OK )
					undo.nToken = -1;
			if ( undo.nToken >= 0 ) undos.push_back( undo ); else ++refused[undo.szKind];
		}
		{
			SSweepUndo undo = { -1, -1, "fence run drawn" };
			for ( size_t i = 0; i < spots.size() && undo.nToken < 0; ++i )
				if ( BkEditorDrawFences( pSession, "W_FactoryFence", spots[i].x - 200.0f, spots[i].y + 260.0f, spots[i].x + 200.0f, spots[i].y + 260.0f, 0, &undo.nToken ) != BK_EDITOR_OK )
					undo.nToken = -1;
			if ( undo.nToken >= 0 ) undos.push_back( undo ); else ++refused[undo.szKind];
		}
		{
			SSweepUndo undo = { -1, -1, "entrenchment drawn" };
			int nIndex = -1;
			for ( size_t i = 0; i < spots.size() && undo.nToken < 0; ++i )
			{
				const std::vector<BkEditorVec3> clicks = ToCPoints2( EngineTrenchL( spots[i].x - 300.0f, spots[i].y - 700.0f ) );
				if ( BkEditorDrawEntrenchment( pSession, &clicks[0], int( clicks.size() ), 0, &undo.nToken, &nIndex ) != BK_EDITOR_OK )
					undo.nToken = -1;
			}
			if ( undo.nToken >= 0 ) undos.push_back( undo ); else ++refused[undo.szKind];
		}
		// One shipped bridge and one shipped entrenchment deleted whole.
		if ( nShippedBridges > 0 )
		{
			SSweepUndo undo = { -1, -1, "shipped bridge deleted" };
			for ( int i = 0; i < nShippedBridges && undo.nToken < 0; ++i )
				if ( BkEditorDeleteBridge( pSession, i, &undo.nToken ) != BK_EDITOR_OK )
					undo.nToken = -1;
			if ( undo.nToken >= 0 ) undos.push_back( undo ); else ++refused[undo.szKind];
		}
		if ( nShippedTrenches > 0 )
		{
			SSweepUndo undo = { -1, -1, "shipped entrenchment deleted" };
			for ( int i = 0; i < nShippedTrenches && undo.nToken < 0; ++i )
				if ( BkEditorDeleteEntrenchment( pSession, i, &undo.nToken ) != BK_EDITOR_OK )
					undo.nToken = -1;
			if ( undo.nToken >= 0 ) undos.push_back( undo ); else ++refused[undo.szKind];
		}
		// The cascade delete of a unit a start command or a reserve position names.
		{
			std::vector<int> candidates;
			for ( std::list<SAIStartCommand>::const_iterator it = map.startCommandsList.begin(); it != map.startCommandsList.end(); ++it )
				if ( !it->unitLinkIDs.empty() )
					candidates.push_back( it->unitLinkIDs[0] );
			for ( std::list<SBattlePosition>::const_iterator it = map.reservePositionsList.begin(); it != map.reservePositionsList.end(); ++it )
				candidates.push_back( it->nArtilleryLinkID );
			if ( !candidates.empty() )
			{
				SSweepUndo undo = { -1, -1, "cascade delete" };
				for ( size_t i = 0; i < candidates.size() && i < 8 && undo.nRestoreLink < 0; ++i )
					if ( BkEditorDeleteObject( pSession, candidates[i] ) == BK_EDITOR_OK )
						undo.nRestoreLink = candidates[i];
				if ( undo.nRestoreLink >= 0 ) undos.push_back( undo ); else ++refused[undo.szKind];
			}
		}
		if ( !undos.empty() )
		{
			if ( Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, ( szName + "saves edited: " + BkEditorLastMessage( pSession ) ).c_str() ) )
				Check( !SameBytes( szUnedited, szEdited ), ( szName + "the edits changed the saved bytes" ).c_str() );
		}
		bool bUndone = true;
		for ( size_t u = undos.size(); u-- > 0 && bUndone; )
		{
			const SSweepUndo &undo = undos[u];
			const BkEditorStatus status = undo.nRestoreLink >= 0 ? BkEditorRestoreObject( pSession, undo.nRestoreLink ) : BkEditorUndoEdit( pSession, undo.nToken );
			bUndone = Check( status == BK_EDITOR_OK, ( szName + "the undo of \"" + undo.szKind + "\": " + BkEditorLastMessage( pSession ) ).c_str() );
		}
		if ( !bUndone )
			continue;
		if ( !Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, ( szName + "saves undone: " + BkEditorLastMessage( pSession ) ).c_str() ) )
			continue;
		Check( SameBytes( szUnedited, szUndone ), ( szName + "the edits and their undos save the unedited file byte for byte (" + DescribeDifference( szUnedited, szUndone ) + ")" ).c_str() );
		nEdits += int( undos.size() );
		for ( size_t u = 0; u < undos.size(); ++u )
			++done[undos[u].szKind];
	}
	std::string szDone, szRefused;
	for ( std::map<std::string, int>::const_iterator it = done.begin(); it != done.end(); ++it )
		szDone += ( szDone.empty() ? "" : ", " ) + it->first + " " + NStr::Format( "%d", it->second );
	for ( std::map<std::string, int>::const_iterator it = refused.begin(); it != refused.end(); ++it )
		szRefused += ( szRefused.empty() ? "" : ", " ) + it->first + " " + NStr::Format( "%d", it->second );
	printf( "editor-bridge: M2 sweep edits by kind: %s; refused (recorded, not failures): %s\n", szDone.c_str(), szRefused.empty() ? "none" : szRefused.c_str() );
	// WR-C05: a refusal is counted, not failed, so the sweep could pass having
	// applied nothing. Every kind must have been applied on a good share of the
	// maps (about half of what the 57 openable shipped maps give, 04-13).
	struct SMinimum { const char *pszKind; int nAtLeast; };
	const SMinimum minimums[] = {
		{ "bridge drawn", 28 }, { "fence run drawn", 28 }, { "entrenchment drawn", 28 },
		{ "shipped bridge deleted", 10 }, { "shipped entrenchment deleted", 3 }, { "cascade delete", 18 },
	};
	for ( size_t k = 0; k < sizeof minimums / sizeof minimums[0]; ++k )
	{
		const std::map<std::string, int>::const_iterator it = done.find( minimums[k].pszKind );
		const int nDone = it == done.end() ? 0 : it->second;
		Check( nDone >= minimums[k].nAtLeast, NStr::Format( "M2 sweep: \"%s\" applied %d times, at least %d expected", minimums[k].pszKind, nDone, minimums[k].nAtLeast ) );
	}
	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	if ( g_nFailures == nFailuresBefore )
		printf( "editor-bridge: M2 sweep %d maps, %d edits, all restored byte-exact\n", nMaps, nEdits );
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
	// 04-13: `--m2-sweep` after the two positional arguments runs only the M2
	// sweep (the local step test-editor-bridge-m2-sweep).
	bool bM2Sweep = false, bPlayersOnly = false, bMinimapOnly = false;
	const char *pszCraftKind = 0, *pszCraftOut = 0;
	for ( int i = 3; i < argc; ++i )
	{
		bM2Sweep = bM2Sweep || strcmp( argv[i], "--m2-sweep" ) == 0;
		// 05-05: `--m3-players-only` runs the players, unit creation and Check Map
		// fixes alone (the local step test-editor-bridge-m3-players).
		bPlayersOnly = bPlayersOnly || strcmp( argv[i], "--m3-players-only" ) == 0;
		// 05-07: `--m3-minimap-only` runs the minimap reads and image creation alone
		// (the local step test-editor-bridge-m3-minimap).
		bMinimapOnly = bMinimapOnly || strcmp( argv[i], "--m3-minimap-only" ) == 0;
		// 05-05: `--craft <kind> <out>` writes one fixture map (CraftFixture) instead
		// of running the tier (<out> is a file name under the scratch folder) - the build
		// runs it before the scenarios that open them.
		if ( strcmp( argv[i], "--craft" ) == 0 && i + 2 < argc )
		{
			pszCraftKind = argv[i + 1];
			pszCraftOut = argv[i + 2];
		}
	}
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
	else if ( pszCraftKind != 0 )
	{
		Check( CraftFixture( pSession, pszCraftKind, ( szScratch + "\\" + pszCraftOut ).c_str() ), "the fixture map is crafted" );
		Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "and stops" );
	}
	else if ( bMinimapOnly )
	{
		TestM3MinimapReads( pSession, szScratch );
		TestM3MinimapImages( pSession, szScratch );
		Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "and stops" );
	}
	else if ( bPlayersOnly )
	{
		TestM3PlayersAndUnitCreation( pSession, szScratch );
		TestM3CheckMap( pSession, szScratch );
		Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "and stops" );
	}
	else if ( bM2Sweep )
	{
		// 04-13: --m2-sweep runs the M2 edit-and-undo sweep alone.
		TestM2Sweep( pSession, szScratch );
		Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "and stops" );
	}
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
		TestM3Altitudes( pSession, szScratch );
		TestM3NewMap( pSession, szScratch );
		TestM3Heights( pSession, szScratch );
		TestM3UpdateMapAndFill( pSession, szScratch );
		TestM3TileInfo( pSession );
		TestM3Filters( pSession, pszRoot, szScratch );
		TestM3Fields( pSession, szScratch );
		TestM3MultiSelect( pSession, szScratch );
		TestM3PropertiesAndLinks( pSession, szScratch );
		TestM3Damage( pSession, szScratch );
		TestM3PlayersAndUnitCreation( pSession, szScratch );
		TestM3CheckMap( pSession, szScratch );
		TestM3MinimapReads( pSession, szScratch );
		TestM3MinimapImages( pSession, szScratch );
		TestPaintRefusesTileOutsideTileset( pSession );
		TestTilesetTilesAllPaint( pSession );
		TestTilePicturesAndClose( pSession, szScratch );
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
		TestM2VsoRendersOnGpu( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestM2CameraAnchors( pSession, szScratch );
		TestM2OffMapAnchorUndo( pSession, szScratch );
		TestM2ScriptIDs( pSession, szScratch );
		TestM2Groups( pSession, szScratch );
		TestM2ScriptFile( pSession, szScratch );
		TestM2ScriptAreas( pSession, szScratch );
		TestM2HideGroups( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestM2PaletteFilter( pSession, szScratch );
		TestM2CascadeDelete( pSession, szScratch, false );
		TestM2CascadeDelete( pSession, szScratch, true );
		TestM2Roads( pSession, szScratch );
		TestM2Rivers( pSession, szScratch );
		TestM2RoadEdits( pSession, szScratch );
		TestM2ShortVsoKeptAsRead( pSession, szScratch );
		TestM2Bridges( pSession, szScratch );
		TestM2EveryBridgeTypePlans( pSession );
		TestM2BridgeDelete( pSession, szScratch );
		TestM2SharedBridgeLinksKeptAsRead( pSession, szScratch );
		TestM2BridgeRotateToggle( pSession, szScratch );
		TestM2BridgeToggleSharedRefused( pSession, szScratch );
		TestM2Fences( pSession, szScratch );
		TestM2Entrenchments( pSession, szScratch );
		TestM2EntrenchmentDelete( pSession, szScratch );
		TestM2StartCommands( pSession, szScratch );
		TestM2GroupHoldWarning( pSession, szScratch );
		TestM2ReservePositions( pSession, szScratch );
		TestM2AIGeneral( pSession, szScratch );
		TestM2AISideShrinkRefused( pSession, szScratch );
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

// The Fields tool (M3, D-21): a shipped summer field set applies over a
// square polygon on a summer map - tiles and heights exactly what the same
// engine functions build on a copy, objects through the session's add path -
// and one undo writes the unedited save byte for byte. Degenerate polygons
// are refused, changing nothing; the season confirmation is the app's (the
// bridge answers the season and does not gate).
static void TestM3Fields( BkEditorSession *pSession, const std::string &szScratch )
{
	// Argument rules: a null params or out_token is BAD_ARGUMENT; a bad
	// point count too.
	Check( BkEditorApplyField( pSession, 0, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "BkEditorApplyField with a null params is BAD_ARGUMENT" );
	BkEditorFieldApplyParams bad;
	memset( &bad, 0, sizeof bad );
	bad.point_count = 2;
	Check( BkEditorApplyField( pSession, &bad, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a two-point polygon is BAD_ARGUMENT" );

	// The RMG folder scan (D-08): the shipped Scenarios/FieldSets files list,
	// bare names, sorted; the sizing pass refused with the total there.
	int nTotal = 0;
	Check( BkEditorListRmg( pSession, 9, 0, 0, &nTotal ) == BK_EDITOR_BAD_ARGUMENT, "an unknown RMG kind is BAD_ARGUMENT" );
	Check( BkEditorListRmg( pSession, 0, 0, 0, &nTotal ) == BK_EDITOR_REFUSED && nTotal > 0,
	       NStr::Format( "the field-sets folder sizes (%d)", nTotal ) );
	if ( !Check( nTotal > 0, "the shipped data carries field sets" ) )
		return;
	std::vector<BkEditorRmgName> sets = std::vector<BkEditorRmgName>( size_t( nTotal ) );
	int nGot = 0;
	if ( !Check( BkEditorListRmg( pSession, 0, &( sets[0] ), nTotal, &nGot ) == BK_EDITOR_OK && nGot == nTotal,
		     BkEditorLastMessage( pSession ) ) )
		return;
	bool bOrdered = true;
	for ( int i = 1; i < nGot; ++i )
		bOrdered = bOrdered && strcmp( sets[i - 1].name, sets[i].name ) < 0;
	Check( bOrdered, "the scan is sorted" );
	// A summer field set for a summer map: the name's own word is the
	// shipped data's convention; fall back to the first.
	int nSummer = 0;
	for ( int i = 0; i < nGot; ++i )
		if ( strstr( sets[i].name, "summer" ) != 0 || strstr( sets[i].name, "Summer" ) != 0 )
		{
			nSummer = i;
			break;
		}

	// The season answers for the set (the app's confirmation data).
	int nSeason = -1;
	Check( BkEditorFieldSetSeason( pSession, sets[nSummer].name, &nSeason ) == BK_EDITOR_OK && nSeason >= 0,
	       BkEditorLastMessage( pSession ) );
	Check( BkEditorFieldSetSeason( pSession, "no\\such\\fieldset", &nSeason ) == BK_EDITOR_REFUSED, "an unknown set is REFUSED" );

	// The summer map (arnheim is season 0), and the unedited save the undo
	// is judged against.
	if ( !Check( BkEditorOpenMap( pSession, "Data\\Maps\\Multiplayer\\arnheim.bzm", 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szPre = szScratch + "\\m3-fields-pre.bzm";
	const std::string szEdited = szScratch + "\\m3-fields-edited.bzm";
	const std::string szUndone = szScratch + "\\m3-fields-undone.bzm";
	Check( BkEditorSaveMap( pSession, szPre.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// The square, world units over the map's middle: 40..48 cells. The cut
	// by the map bounds keeps it as it is.
	BkEditorVec3 points[4];
	const float fCell = fWorldCellSize;
	const float f0 = 40.0f * fCell, f1 = 48.0f * fCell;
	points[0] = { f0, f0, 0 };
	points[1] = { f1, f0, 0 };
	points[2] = { f1, f1, 0 };
	points[3] = { f0, f1, 0 };

	// Degenerate first: the same square collapsed to a line refuses and
	// changes nothing.
	{
		BkEditorFieldApplyParams degenerate;
		memset( &degenerate, 0, sizeof degenerate );
		strcpy( degenerate.field_set, sets[nSummer].name );
		BkEditorVec3 line[3] = { { f0, f0, 0 }, { f1, f0, 0 }, { 2 * f1, f0, 0 } };
		degenerate.point_count = 3;
		degenerate.points = line;
		degenerate.fill_terrain = 1;
		int nToken = -1, nReport = 0;
		Check( BkEditorApplyField( pSession, &degenerate, 0, 0, &nReport, &nToken ) == BK_EDITOR_REFUSED && nToken == -1,
		       "a degenerate polygon is REFUSED, token -1" );
	}

	// 1. The terrain and heights half, exactly what the same engine calls
	// build on a copy of the pre-map.
	{
		BkEditorFieldApplyParams params;
		memset( &params, 0, sizeof params );
		strcpy( params.field_set, sets[nSummer].name );
		params.point_count = 4;
		params.points = points;
		params.fill_terrain = 1;
		params.place_objects = 0;
		params.modify_heights = 1;
		int nToken = -1, nReport = 0;
		ScrambleRandom();
		Check( BkEditorApplyField( pSession, &params, 0, 0, &nReport, &nToken ) == BK_EDITOR_OK,
		       ( std::string( "the terrain half applies: " ) + BkEditorLastMessage( pSession ) ).c_str() );
		Check( nToken >= 0, "the terrain half has a token" );
		Check( BkEditorSaveMap( pSession, szEdited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

		// The expected map: the same engine functions over a copy.
		CMapInfo pre, expected;
		std::string szError;
		if ( Check( NMapFile::Read( szPre.c_str(), &pre, &szError ) && NMapFile::Read( szEdited.c_str(), &expected, &szError ),
			    "the pre and edited maps read back" ) )
		{
			// The pre map as read, safe from the builder's own fills below.
			CMapInfo *pPreAsRead = new CMapInfo( pre );
			std::list<CVec2> listedPolygon;
			listedPolygon.push_back( CVec2( f0, f0 ) );
			listedPolygon.push_back( CVec2( f1, f0 ) );
			listedPolygon.push_back( CVec2( f1, f1 ) );
			listedPolygon.push_back( CVec2( f0, f1 ) );
			std::list<CVec2> mapRect;
			mapRect.push_back( VNULL2 );
			mapRect.push_back( CVec2( 0.0f, pre.terrain.tiles.GetSizeY() * fWorldCellSize ) );
			mapRect.push_back( CVec2( pre.terrain.tiles.GetSizeX() * fWorldCellSize, pre.terrain.tiles.GetSizeY() * fWorldCellSize ) );
			mapRect.push_back( CVec2( pre.terrain.tiles.GetSizeX() * fWorldCellSize, 0.0f ) );
			std::list<CVec2> cut;
			CutByPolygonCore<std::list<CVec2>, CVec2>( listedPolygon, mapRect, &cut );
			SRMFieldSet fieldSet;
			// The bridge's fills are seeded from a fixed state (the dual-copy
			// discipline: both copies land byte-identical, every apply
			// replays exactly), so the builder replays them with the same
			// seeds: field set and tileset loads, ValidateFieldSet, the tile
			// fill, the profile image, the pattern fill.
			ReseedRandom();
			if ( Check( LoadDataResource( sets[nSummer].name, "", false, 0, RMGC_FIELDSET_XML_NAME, fieldSet ), "the field set loads for the builder" ) )
			{
				STilesetDesc tilesetDesc;
				LoadDataResource( pre.terrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc );
				const std::list<std::list<CVec2>> exclusive;
				std::unordered_map<LPARAM, float> distances;
				fieldSet.ValidateFieldSet( tilesetDesc, CMapInfo::MOST_COMMON_TILES[pre.GetSelectedSeason()] );
				// The bridge seeds immediately before each fill (session_fields
				// SeedFieldFills), so the replay does the same here.
				ReseedRandom();
				Check( CMapInfo::FillTileSet( &pre.terrain, tilesetDesc, cut, exclusive, fieldSet.tilesShells, &distances ),
				       "the builder's tile fill goes" );
				SVAGradient gradient;
				IImageProcessor *pImages = GetImageProcessor();
				if ( CPtr<IDataStream> pImageStream = GetSingleton<IDataStorage>()->OpenStream( ( fieldSet.szProfileFileName + ".tga" ).c_str(), STREAM_ACCESS_READ ) )
				{
					if ( CPtr<IImage> pImage = pImages->LoadImage( pImageStream ) )
						gradient.CreateFromImage( pImage, CTPoint<float>( 0.0f, 1.0f ), CTPoint<float>( 0.0f, fieldSet.fHeight ) );
				}
				if ( !gradient.heights.empty() )
				{
					ReseedRandom();
					Check( CMapInfo::FillProfilePattern( &pre.terrain, cut, exclusive, gradient, fieldSet.patternSize, fieldSet.fPositiveRatio, &distances ),
					       "the builder's profile fill goes" );
					CMapInfo::UpdateTerrainShades( &pre.terrain,
						CTRect<int>( 0, 0, pre.terrain.altitudes.GetSizeX(), pre.terrain.altitudes.GetSizeY() ),
						CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( pre.nSeason ) ) );
				}
				// The same engine calls over the same polygon, the bridge's own
				// fixed seeds: the replay is the apply, tile for tile, over the
				// whole map. Each filled cell takes its terrain type from
				// Random() and its variant of that type from rand()
				// (STileTypeDesc::GetMapsIndex); both are seeded before every
				// fill, so nothing about the values is left to chance. (Before
				// rand() was seeded the variants rolled differently on every
				// fill - 57 of the 64 cells, in every row - and only the
				// changed-ness was compared, which still flipped wherever a
				// cell already had the picked type.) The heights must change
				// at exactly the vertices the engine's own pattern changes.
				int nDifferentTiles = 0, nTouched = 0, nDisagreeHeights = 0, nHeights = 0;
				int nFirstDifferentX = -1, nFirstDifferentRow = -1;
				for ( int nRow = 0; nRow < pre.terrain.tiles.GetSizeY(); ++nRow )
					for ( int nXIndex = 0; nXIndex < pre.terrain.tiles.GetSizeX(); ++nXIndex )
					{
						if ( expected.terrain.tiles[nRow][nXIndex].tile != pre.terrain.tiles[nRow][nXIndex].tile )
						{
							if ( nDifferentTiles == 0 )
							{
								nFirstDifferentX = nXIndex;
								nFirstDifferentRow = nRow;
							}
							++nDifferentTiles;
						}
						if ( expected.terrain.tiles[nRow][nXIndex].tile != pPreAsRead->terrain.tiles[nRow][nXIndex].tile )
							++nTouched;
					}
				for ( int nYIndex = 40; nYIndex < 48; ++nYIndex )
					for ( int nXIndex = 40; nXIndex < 48; ++nXIndex )
					{
						const int nRow = 128 - nYIndex - 1;
						const float fMap = expected.terrain.altitudes[nRow][nXIndex].fHeight - pPreAsRead->terrain.altitudes[nRow][nXIndex].fHeight;
						const float fBuilt = pre.terrain.altitudes[nRow][nXIndex].fHeight - pPreAsRead->terrain.altitudes[nRow][nXIndex].fHeight;
						if ( fabs( fMap ) > 0.01f ) ++nHeights;
						if ( ( fabs( fMap ) > 0.01f ) != ( fabs( fBuilt ) > 0.01f ) ) ++nDisagreeHeights;
					}
				Check( nDifferentTiles == 0 && nTouched > 0,
				       ( "the fields' tiles are exactly the engine's own fill's, cell for cell (" + std::to_string( nTouched ) + " touched, " +
				         std::to_string( nDifferentTiles ) + " different" +
				         ( nDifferentTiles != 0 ? ", the first at row " + std::to_string( nFirstDifferentRow ) + " x " + std::to_string( nFirstDifferentX ) : std::string() ) + ")" ).c_str() );
				Check( nDisagreeHeights == 0,
				       ( "the fields' heights change exactly the vertices the engine's own pattern touches (" + std::to_string( nHeights ) + " changed, " + std::to_string( nDisagreeHeights ) + " disagree)" ).c_str() );
				delete pPreAsRead;
			}
		}
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BridgeFilesAreIdentical( szPre.c_str(), szUndone.c_str() ),
		       "the terrain half undone writes the unedited save byte for byte" );
	}

	// 2. The objects half: the report answers what the shells produced, the
	// placed ones are on the map, and the whole thing undoes byte for byte.
	// A bigger square than the terrain half's: the shells' own placement
	// density (the MFC's Ratio/BetweenDistance) decides how much a square
	// yields, and a small one can honestly yield nothing.
	{
		const int nObjectsBefore = M3CountObjects( pSession );
		BkEditorFieldApplyParams params;
		memset( &params, 0, sizeof params );
		strcpy( params.field_set, sets[nSummer].name );
		BkEditorVec3 big[4] = { { 24.0f * fCell, 24.0f * fCell, 0 }, { 72.0f * fCell, 24.0f * fCell, 0 },
			                      { 72.0f * fCell, 72.0f * fCell, 0 }, { 24.0f * fCell, 72.0f * fCell, 0 } };
		params.point_count = 4;
		params.points = big;
		params.fill_terrain = 0;
		params.place_objects = 1;
		params.modify_heights = 0;
		int nToken = -1;
		// One call with the upper bound room (the fill places at most one
		// object per half-tile cell): the apply runs once, the report
		// answers everything it produced.
		std::vector<BkEditorFieldObjectReport> report = std::vector<BkEditorFieldObjectReport>( size_t( 128 * 128 / 2 + 64 ) );
		int nReport = int( report.size() );
		if ( Check( BkEditorApplyField( pSession, &params, &( report[0] ), nReport, &nReport, &nToken ) == BK_EDITOR_OK,
			    ( std::string( "the objects half applies: " ) + BkEditorLastMessage( pSession ) ).c_str() ) )
		{
			Check( nReport > 0, "the object shells produced something" );
			int nPlaced = 0;
			for ( const BkEditorFieldObjectReport &r : report )
				nPlaced += r.placed != 0 ? 1 : 0;
			Check( M3CountObjects( pSession ) == nObjectsBefore + nPlaced,
			       "the map holds exactly the placed ones more" );
			Check( nToken >= 0, "the objects half has a token" );
			Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			Check( M3CountObjects( pSession ) == nObjectsBefore, "the objects half undone leaves the count" );
			Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			Check( BridgeFilesAreIdentical( szPre.c_str(), szUndone.c_str() ),
			       "the objects half undone writes the unedited save byte for byte" );
		}
	}

	// 3. Randomize Polygon (TR15): the MFC dialog's three numbers reach the
	// engine's RandomizeEdges with its exact arguments (StateTerrainFields.
	// cpp:350), the randomized polygon fills and undoes byte for byte.
	{
		BkEditorFieldApplyParams params;
		memset( &params, 0, sizeof params );
		strcpy( params.field_set, sets[nSummer].name );
		params.point_count = 4;
		params.points = points;
		params.randomize = 1;
		params.min_length = 4.0f;
		params.width = 0.3f;
		params.disturbance = 0.5f;
		params.fill_terrain = 1;
		params.modify_heights = 1;
		int nToken = -1, nReport = 0;
		Check( BkEditorApplyField( pSession, &params, 0, 0, &nReport, &nToken ) == BK_EDITOR_OK,
		       ( std::string( "the randomized apply goes: " ) + BkEditorLastMessage( pSession ) ).c_str() );
		Check( nToken >= 0, "the randomized apply has a token" );
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( BridgeFilesAreIdentical( szPre.c_str(), szUndone.c_str() ),
		       "the randomized apply undone writes the unedited save byte for byte" );
	}
	// 4. Two identical seeded applies run, undo clean and save the same
	// bytes: every generator the fills draw from is seeded before each fill
	// (session_fields.cpp SeedFieldFills), so an apply is a function of its
	// inputs alone.
	{
		BkEditorFieldApplyParams params;
		memset( &params, 0, sizeof params );
		strcpy( params.field_set, sets[nSummer].name );
		params.point_count = 4;
		params.points = points;
		params.fill_terrain = 1;
		params.modify_heights = 1;
		int nToken = -1, nReport = 0;
		Check( BkEditorApplyField( pSession, &params, 0, 0, &nReport, &nToken ) == BK_EDITOR_OK, "the determinism probe A applies" );
		const std::string szProbeA = szScratch + "\\m3-fields-probe-a.bzm";
		Check( BkEditorSaveMap( pSession, szProbeA.c_str() ) == BK_EDITOR_OK, "the determinism probe A saves" );
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, "the determinism probe A undoes" );
		nToken = -1;
		Check( BkEditorApplyField( pSession, &params, 0, 0, &nReport, &nToken ) == BK_EDITOR_OK, "the determinism probe B applies" );
		const std::string szProbeB = szScratch + "\\m3-fields-probe-b.bzm";
		Check( BkEditorSaveMap( pSession, szProbeB.c_str() ) == BK_EDITOR_OK, "the determinism probe B saves" );
		Check( BkEditorUndoEdit( pSession, nToken ) == BK_EDITOR_OK, "the determinism probe B undoes" );
		if ( !Check( BridgeFilesAreIdentical( szProbeA.c_str(), szProbeB.c_str() ),
		             "two identical seeded field applies save byte for byte the same map" ) )
		{
			CMapInfo readA, readB;
			std::string szErr, szWhere;
			if ( NMapFile::Read( szProbeA.c_str(), &readA, &szErr ) && NMapFile::Read( szProbeB.c_str(), &readB, &szErr ) )
				printf( "editor-bridge: the seeded applies diverge at %s\n",
				        NMapFile::AreEquivalent( readA, readB, &szWhere ) ? "nowhere the reader compares" : szWhere.c_str() );
		}
	}
	printf( "editor-bridge: M3 fields ok\n" );
}

static int M3CountObjects( BkEditorSession *pSession )
{
	int nCount = 0;
	if ( BkEditorObjects( pSession, 0, 0, &nCount ) != BK_EDITOR_REFUSED )
		return -1;
	return nCount;
}

// M3 (D-30): players and the Unit Creation Info, on the engine. A player is
// added before the neutral and deleted again, the owners of the objects follow
// by the rule, the saved map is the builder's (the same NMapRecords calls on a
// fresh read) and the undo, redo and undo save the unedited file byte for
// byte; one player's unit creation is put through MutableValidate-style rules
// (a refusal names the field and changes nothing), saved and read back, and a
// put for a player the vector does not hold grows it and its undo shrinks it
// back exactly.
static int M3PlayerEntries( BkEditorSession *pSession )
{
	int nEntries = 0, nValue = 0;
	while ( nEntries < 32 && BkEditorDiplomacy( pSession, nEntries, &nValue ) == BK_EDITOR_OK )
		++nEntries;
	return nEntries;
}

static std::vector<std::string> M3UcChoices( BkEditorSession *pSession, int nKind )
{
	std::vector<std::string> names;
	int nCount = 0;
	const BkEditorStatus nSizing = BkEditorUnitCreationChoices( pSession, nKind, 0, 0, &nCount );
	Check( nSizing == BK_EDITOR_REFUSED || ( nSizing == BK_EDITOR_OK && nCount == 0 ), "the sizing pass answers the total" );
	if ( nCount <= 0 )
		return names;
	std::vector<BkEditorUcName> all( static_cast<size_t>( nCount ) );
	int nRead = 0;
	if ( Check( BkEditorUnitCreationChoices( pSession, nKind, &all[0], nCount, &nRead ) == BK_EDITOR_OK && nRead == nCount, "the choices read" ) )
		for ( int i = 0; i < nRead; ++i )
			names.push_back( all[size_t( i )].name );
	// One short of the total: REFUSED, after writing what fits.
	if ( nCount > 1 )
	{
		int nShort = 0;
		Check( BkEditorUnitCreationChoices( pSession, nKind, &all[0], nCount - 1, &nShort ) == BK_EDITOR_REFUSED && nShort == nCount, "a short buffer is refused and the total is still answered" );
	}
	return names;
}

static bool M3SameUc( const BkEditorUnitCreationRecord &rLeft, const BkEditorUnitCreationRecord &rRight )
{
	return memcmp( &rLeft, &rRight, sizeof rLeft ) == 0;
}

// The builder's entry: the same values the record carries, set through the
// overlay on a read of the file.
static SUnitCreation M3UcFromRecord( const BkEditorUnitCreationRecord &rRecord, const SUnitCreation &rBase )
{
	SUnitCreation unit = rBase;
	unit.szPartyName = rRecord.party;
	for ( int i = 0; i < 5; ++i )
	{
		unit.aviation.aircrafts[i].szName = rRecord.aircraft[i].name;
		unit.aviation.aircrafts[i].nFormationSize = rRecord.aircraft[i].formation_size;
		unit.aviation.aircrafts[i].nPlanes = rRecord.aircraft[i].count;
	}
	unit.aviation.szParadropSquadName = rRecord.paratroop_name;
	unit.aviation.nParadropSquadCount = rRecord.paratroop_count;
	unit.aviation.nRelaxTime = rRecord.relax_time;
	unit.aviation.vAppearPoints.clear();
	for ( int i = 0; i < rRecord.appear_count; ++i )
		unit.aviation.vAppearPoints.push_back( CVec3( rRecord.appear[i].x, rRecord.appear[i].y, rRecord.appear[i].z ) );
	return unit;
}

static std::vector<BkEditorObjectRecord> M3ReadObjects( BkEditorSession *pSession )
{
	int nCount = 0;
	BkEditorObjects( pSession, 0, 0, &nCount );
	std::vector<BkEditorObjectRecord> records( nCount > 0 ? size_t( nCount ) : 1 );
	int nRead = 0;
	BkEditorObjects( pSession, &records[0], nCount, &nRead );
	records.resize( size_t( nRead ) );
	return records;
}

static void TestM3PlayersAndUnitCreation( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &original, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szUnedited = szScratch + "\\m3-players-unedited.bzm";
	const std::string szEdited = szScratch + "\\m3-players-edited.bzm";
	const std::string szUndone = szScratch + "\\m3-players-undone.bzm";
	Check( BkEditorSaveMap( pSession, szUnedited.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	const int nEntries = M3PlayerEntries( pSession );
	Check( nEntries == int( original.diplomacies.size() ) && nEntries >= 2, "the bridge holds the map's table" );
	printf( "editor-bridge: M3 players: %d entries, %d unit-creation slots\n", nEntries, int( original.unitCreation.units.size() ) );

	// --- The unit creation ---------------------------------------------------
	const std::vector<std::string> parties = M3UcChoices( pSession, 0 );
	const std::vector<std::string> aircraft = M3UcChoices( pSession, 1 );
	const std::vector<std::string> squads = M3UcChoices( pSession, 2 );
	if ( !Check( !parties.empty() && !aircraft.empty() && !squads.empty(), "partys.xml, the aviation folders and the squads folders list names" ) )
		return;
	{
		int nCount = 0;
		Check( BkEditorUnitCreationChoices( pSession, 3, 0, 0, &nCount ) == BK_EDITOR_BAD_ARGUMENT, "a choice kind outside 0..2 is a caller bug" );
		Check( BkEditorUnitCreationChoices( pSession, 0, 0, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "and so is a null count" );
	}
	BkEditorUnitCreationRecord before;
	if ( !Check( BkEditorUnitCreation( pSession, 0, &before ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( before.slot_count == int( original.unitCreation.units.size() ), "the record carries the vector's size" );
	Check( BkEditorUnitCreation( pSession, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null record to fill is a caller bug" );
	{
		BkEditorUnitCreationRecord probe;
		Check( BkEditorUnitCreation( pSession, 16, &probe ) == BK_EDITOR_REFUSED, "player 16 is no player" );
		Check( BkEditorUnitCreation( pSession, -1, &probe ) == BK_EDITOR_REFUSED, "player -1 is no player" );
	}

	BkEditorUnitCreationRecord wanted = before;
	wanted.slot_count = Max( before.slot_count, 1 );
	strcpy( wanted.party, parties.back().c_str() );
	strcpy( wanted.aircraft[2].name, aircraft[0].c_str() );
	wanted.aircraft[2].formation_size = 3;
	wanted.aircraft[2].count = 4;
	strcpy( wanted.paratroop_name, squads[0].c_str() );
	wanted.paratroop_count = 5;
	wanted.relax_time = 77;
	wanted.appear_count = before.appear_count < 31 ? before.appear_count + 1 : before.appear_count;
	if ( before.appear_count < 31 )
	{
		wanted.appear[before.appear_count].x = 640.0f;
		wanted.appear[before.appear_count].y = 1280.0f;
		wanted.appear[before.appear_count].z = 0.0f;
	}

	// Refusals name the field and change nothing.
	{
		BkEditorUnitCreationRecord bad = wanted;
		strcpy( bad.party, "Narnia" );
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "an unknown party is refused" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "partys.xml" ) != std::string::npos, "and names partys.xml" );
		bad = wanted;
		strcpy( bad.aircraft[1].name, "Spitfire_Not_There" );
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "an unknown aircraft is refused" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "Fighters" ) != std::string::npos, "and names the slot" );
		bad = wanted;
		bad.aircraft[3].formation_size = -5;
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "a negative formation size is refused" );
		bad.aircraft[3].formation_size = 33;
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "a formation size of 33 is refused" );
		bad = wanted;
		bad.aircraft[4].count = 300;
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "a plane count of 300 is refused" );
		bad = wanted;
		strcpy( bad.paratroop_name, "Ghost_Squad" );
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "an unknown paratroop squad is refused" );
		bad = wanted;
		bad.paratroop_count = -1;
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "a negative paratroop count is refused" );
		bad = wanted;
		bad.relax_time = 0;
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "a relax time below 1 is refused" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "relax" ) != std::string::npos, "and names it" );
		if ( before.appear_count < 31 )
		{
			bad = wanted;
			bad.appear[before.appear_count].x = -5.0f;
			Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "an appear point off the map is refused" );
			bad.appear[before.appear_count].x = 1.0e9f;
			Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_REFUSED, "a far appear point is refused" );
		}
		// The caller's bugs.
		Check( BkEditorSetUnitCreation( pSession, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null record is a caller bug" );
		Check( BkEditorSetUnitCreation( pSession, 16, &wanted ) == BK_EDITOR_BAD_ARGUMENT, "player 16 is a caller bug" );
		bad = wanted;
		bad.slot_count = 17;
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_BAD_ARGUMENT, "17 slots is a caller bug" );
		bad = wanted;
		memset( bad.party, 'x', sizeof bad.party );
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_BAD_ARGUMENT, "an unterminated name is a caller bug" );
		bad = wanted;
		bad.appear_count = 33;
		Check( BkEditorSetUnitCreation( pSession, 0, &bad ) == BK_EDITOR_BAD_ARGUMENT, "33 appear points are a caller bug" );
		BkEditorUnitCreationRecord now;
		Check( BkEditorUnitCreation( pSession, 0, &now ) == BK_EDITOR_OK && M3SameUc( before, now ), "the refusals changed nothing" );
	}

	// The put: saved, it is the builder's map; put back, the unedited file.
	if ( !Check( BkEditorSetUnitCreation( pSession, 0, &wanted ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	{
		BkEditorUnitCreationRecord now;
		Check( BkEditorUnitCreation( pSession, 0, &now ) == BK_EDITOR_OK && M3SameUc( wanted, now ), "the record reads back as put" );
		CMapInfo expected;
		if ( Check( NMapFile::Read( BRIDGE_MAP, &expected, &szError ), szError.c_str() ) )
		{
			SUnitCreation base;
			NMapRecords::GetUnitCreation( expected, 0, &base );
			Check( NMapRecords::PutUnitCreation( &expected, 0, M3UcFromRecord( wanted, base ), wanted.slot_count ), "the builder puts the same entry" );
			CheckSavedEquals( pSession, szEdited, expected, "the unit-creation put" );
		}
	}
	Check( BkEditorSetUnitCreation( pSession, 0, &before ) == BK_EDITOR_OK, "the old record goes back" );
	Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szUnedited, szUndone ), ( "a unit-creation put and its inverse save the unedited file byte for byte (" + DescribeDifference( szUnedited, szUndone ) + ")" ).c_str() );

	// A player the vector does not hold yet: it reads the defaults, a put grows the
	// vector, and the old record (its slot count) shrinks it back exactly.
	{
		const int nBeyond = before.slot_count;
		BkEditorUnitCreationRecord beyond;
		if ( nBeyond < 16 && nBeyond < nEntries - 1 )
		{
			if ( Check( BkEditorUnitCreation( pSession, nBeyond, &beyond ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			{
				Check( beyond.slot_count == nBeyond, "a player past the vector reads with the vector's size" );
				BkEditorUnitCreationRecord grown = beyond;
				grown.slot_count = nBeyond + 1;
				grown.relax_time = 99;
				Check( BkEditorSetUnitCreation( pSession, nBeyond, &grown ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
				BkEditorUnitCreationRecord now;
				Check( BkEditorUnitCreation( pSession, nBeyond, &now ) == BK_EDITOR_OK && now.relax_time == 99 && now.slot_count == nBeyond + 1, "the vector grew by the entry" );
				CMapInfo expected;
				if ( Check( NMapFile::Read( BRIDGE_MAP, &expected, &szError ), szError.c_str() ) )
				{
					SUnitCreation base;
					NMapRecords::GetUnitCreation( expected, nBeyond, &base );
					Check( NMapRecords::PutUnitCreation( &expected, nBeyond, M3UcFromRecord( grown, base ), grown.slot_count ), "the builder grows the vector too" );
					CheckSavedEquals( pSession, szEdited, expected, "the put that grew the vector" );
				}
				// The undo: the record a read answered before the put, its slot count the old size.
				Check( BkEditorSetUnitCreation( pSession, nBeyond, &beyond ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
				Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
				Check( SameBytes( szUnedited, szUndone ), ( "the undo of a put that grew the vector saves the unedited file byte for byte (" + DescribeDifference( szUnedited, szUndone ) + ")" ).c_str() );
			}
		}
		else
		{
			BkEditorUnitCreationRecord grown = before;
			grown.slot_count = Min( nBeyond + 1, 16 );
			Check( BkEditorSetUnitCreation( pSession, nEntries - 1 + 1, &grown ) == BK_EDITOR_REFUSED, "a put for a player the map does not have is refused" );
		}
	}

	// --- Players -------------------------------------------------------------
	// Every object's owner by the rule, for the add and for the delete.
	const std::vector<BkEditorObjectRecord> objectsBefore = M3ReadObjects( pSession );
	int nAddToken = -1;
	Check( BkEditorAddPlayer( pSession, 2, &nAddToken ) == BK_EDITOR_REFUSED, "a side of 2 is no side for a player" );
	Check( nAddToken == -1, "and hands out no token" );
	Check( BkEditorAddPlayer( pSession, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null token is a caller bug" );
	if ( !Check( BkEditorAddPlayer( pSession, 1, &nAddToken ) == BK_EDITOR_OK && nAddToken >= 0, BkEditorLastMessage( pSession ) ) )
		return;
	Check( M3PlayerEntries( pSession ) == nEntries + 1, "the table grew by one" );
	{
		int nSide = -1, nNeutral = -1;
		BkEditorDiplomacy( pSession, nEntries - 1, &nSide );
		BkEditorDiplomacy( pSession, nEntries, &nNeutral );
		Check( nSide == 1 && nNeutral == int( original.diplomacies[size_t( nEntries - 1 )] ), "the new player sits before the neutral, the neutral last" );
		const std::vector<BkEditorObjectRecord> now = M3ReadObjects( pSession );
		bool bOwnersRight = now.size() == objectsBefore.size();
		for ( size_t i = 0; bOwnersRight && i < now.size(); ++i )
			bOwnersRight = now[i].player == ( objectsBefore[i].player >= nEntries - 1 ? objectsBefore[i].player + 1 : objectsBefore[i].player );
		Check( bOwnersRight, "the owners at or above the old neutral moved up with it" );
		BkEditorCameraAnchorRecord anchors;
		if ( Check( BkEditorCameraAnchors( pSession, &anchors ) == BK_EDITOR_OK, "the anchors read" ) )
			Check( anchors.player_count == int( original.playersCameraAnchors.size() ) + ( int( original.playersCameraAnchors.size() ) >= nEntries - 1 ? 1 : 0 ),
			       "the anchors gained a slot exactly when the file held every player's" );
	}
	CMapInfo expectedAdd;
	if ( Check( NMapFile::Read( BRIDGE_MAP, &expectedAdd, &szError ), szError.c_str() ) )
	{
		Check( NMapRecords::InsertPlayer( &expectedAdd, 1 ), "the builder adds the player" );
		// A flag follows its owner's party: only the flags that changed owner can differ.
		CheckSavedEquals( pSession, szEdited, expectedAdd, "the added player" );
	}
	Check( BkEditorUndoEdit( pSession, nAddToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( M3PlayerEntries( pSession ) == nEntries, "the undo takes the player away" );
	Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szUnedited, szUndone ), ( "add and undo save the unedited file byte for byte (" + DescribeDifference( szUnedited, szUndone ) + ")" ).c_str() );
	Check( BkEditorRedoEdit( pSession, nAddToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( M3PlayerEntries( pSession ) == nEntries + 1, "the redo adds it again" );
	CheckSavedEquals( pSession, szEdited, expectedAdd, "the redone player" );
	Check( BkEditorUndoEdit( pSession, nAddToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// The delete: player 0's objects become the neutral's. Done on a table that
	// is certain to keep two players after it: one more player first when needed.
	int nPrefixToken = -1;
	int nPrefixEntries = nEntries;
	CMapInfo expectedDelete;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &expectedDelete, &szError ), szError.c_str() ) )
		return;
	if ( nEntries < 4 )
	{
		// Fewer than four entries: a delete would leave less than two players and the neutral.
		int nSmallToken = -1;
		Check( BkEditorDeletePlayer( pSession, 0, &nSmallToken ) == BK_EDITOR_REFUSED, "a table at its floor keeps its players" );
		Check( nSmallToken == -1, "and hands out no token" );
	}
	if ( nEntries < 4 )
	{
		if ( !Check( BkEditorAddPlayer( pSession, 0, &nPrefixToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			return;
		++nPrefixEntries;
		Check( NMapRecords::InsertPlayer( &expectedDelete, 0 ), "the builder adds the prefix player" );
	}
	const std::vector<BkEditorObjectRecord> objectsBeforeDelete = M3ReadObjects( pSession );
	BkEditorUnitCreationRecord unitOfOne;
	const bool bUnitOfOne = BkEditorUnitCreation( pSession, 1, &unitOfOne ) == BK_EDITOR_OK;
	int nDeleteToken = -1;
	{
		int nIgnored = -1;
		Check( BkEditorDeletePlayer( pSession, nPrefixEntries - 1, &nIgnored ) == BK_EDITOR_REFUSED, "the neutral entry cannot be deleted" );
		Check( BkEditorDeletePlayer( pSession, nPrefixEntries + 5, &nIgnored ) == BK_EDITOR_REFUSED, "a player past the table is refused" );
		Check( BkEditorDeletePlayer( pSession, -1, &nIgnored ) == BK_EDITOR_REFUSED, "player -1 is refused" );
		Check( BkEditorDeletePlayer( pSession, 0, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null token is a caller bug" );
		Check( M3PlayerEntries( pSession ) == nPrefixEntries, "the refusals changed nothing" );
	}
	if ( !Check( BkEditorDeletePlayer( pSession, 0, &nDeleteToken ) == BK_EDITOR_OK && nDeleteToken >= 0, BkEditorLastMessage( pSession ) ) )
		return;
	Check( M3PlayerEntries( pSession ) == nPrefixEntries - 1, "the table shrank by one" );
	{
		const std::vector<BkEditorObjectRecord> now = M3ReadObjects( pSession );
		bool bOwnersRight = now.size() == objectsBeforeDelete.size();
		int nReowned = 0;
		for ( size_t i = 0; bOwnersRight && i < now.size(); ++i )
		{
			const int nOwner = objectsBeforeDelete[i].player;
			const int nWanted = nOwner == 0 ? nPrefixEntries - 2 : ( nOwner > 0 ? nOwner - 1 : nOwner );
			bOwnersRight = now[i].player == nWanted;
			if ( nOwner == 0 )
				++nReowned;
		}
		Check( bOwnersRight, "player 0's objects became the neutral's and the players above moved down" );
		printf( "editor-bridge: M3 delete re-owned %d objects\n", nReowned );
		if ( bUnitOfOne )
		{
			BkEditorUnitCreationRecord moved;
			if ( Check( BkEditorUnitCreation( pSession, 0, &moved ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			{
				BkEditorUnitCreationRecord same = unitOfOne;
				same.slot_count = moved.slot_count;
				Check( M3SameUc( same, moved ), "the unit creation of player 1 followed it down to index 0" );
			}
		}
	}
	Check( NMapRecords::ErasePlayer( &expectedDelete, 0 ), "the builder deletes the player" );
	CheckSavedEquals( pSession, szEdited, expectedDelete, "the deleted player" );
	Check( BkEditorUndoEdit( pSession, nDeleteToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( M3PlayerEntries( pSession ) == nPrefixEntries, "the undo puts the player back" );
	{
		const std::vector<BkEditorObjectRecord> now = M3ReadObjects( pSession );
		bool bSame = now.size() == objectsBeforeDelete.size();
		for ( size_t i = 0; bSame && i < now.size(); ++i )
			bSame = now[i].player == objectsBeforeDelete[i].player && std::string( now[i].name ) == objectsBeforeDelete[i].name;
		Check( bSame, "the undo gives every object its owner and name back" );
	}
	Check( BkEditorRedoEdit( pSession, nDeleteToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	CheckSavedEquals( pSession, szEdited, expectedDelete, "the redone delete" );
	Check( BkEditorUndoEdit( pSession, nDeleteToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	if ( nPrefixToken >= 0 )
		Check( BkEditorUndoEdit( pSession, nPrefixToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szUnedited, szUndone ), ( "delete and undo save the unedited file byte for byte (" + DescribeDifference( szUnedited, szUndone ) + ")" ).c_str() );

	// The bound: 16 players and the neutral, the 17th refused, all of them undone.
	{
		std::vector<int> tokens;
		int nToken = -1;
		while ( M3PlayerEntries( pSession ) < 17 )
		{
			if ( !Check( BkEditorAddPlayer( pSession, 0, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
				break;
			tokens.push_back( nToken );
		}
		Check( M3PlayerEntries( pSession ) == 17, "the table holds 16 players and the neutral" );
		Check( BkEditorAddPlayer( pSession, 0, &nToken ) == BK_EDITOR_REFUSED, "the 17th player is refused" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "16 players" ) != std::string::npos, "and says why" );
		while ( !tokens.empty() )
		{
			Check( BkEditorUndoEdit( pSession, tokens.back() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
			tokens.pop_back();
		}
		Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
		Check( SameBytes( szUnedited, szUndone ), "sixteen adds and their undos save the unedited file byte for byte" );
	}

	remove( OsPath( szUnedited ).c_str() );
	remove( OsPath( szEdited ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	printf( "editor-bridge: M3 players and unit creation ok\n" );
}

// ---------------------------------------------------------------------------
// Check Map (05-05, D-33) on the engine: the fixes' bridge half, and the fixture
// maps the m3-auto and game-reads-it steps open.
// ---------------------------------------------------------------------------

// The defects one fixture map carries, by what names them afterwards.
struct SCheckMapFixture
{
	int nDuplicateSource;   // the object that was copied...
	int nDuplicateLink;     // ...and the copy, at the same place
	int nInvalidLinkObject; // an object whose host (987654) is not on the map
	int nOwnerObject;       // an object owned by player 99
	int nUnknownLink;       // an object whose type is in no database
	int nShortRoad;         // the road with one control point (index in roads3)
	std::string szBadParty; // the party player 0's unit creation was given
	SCheckMapFixture() : nDuplicateSource( -1 ), nDuplicateLink( -1 ), nInvalidLinkObject( -1 ), nOwnerObject( -1 ), nUnknownLink( -1 ), nShortRoad( -1 ) {  }
};

// A plain object of the map: a known type the database lists, a link ID of its own,
// no host or passenger, and nothing that names it - so a defect made of it is the
// only thing wrong with it. `rExcluded` are indices already taken.
static int PickPlainObject( const CMapInfo &rMap, const std::vector<int> &rExcluded )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
		return -1;
	std::map<int, int> idCounts;
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
		++idCounts[rMap.objects[i].link.nLinkID];
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
	{
		const SMapObjectInfo &rObject = rMap.objects[i];
		if ( rObject.link.nLinkID <= 0 || idCounts[rObject.link.nLinkID] != 1 || rObject.link.nLinkWith != 0 )
			continue;
		if ( std::find( rExcluded.begin(), rExcluded.end(), int( i ) ) != rExcluded.end() )
			continue;
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( rObject.szName.c_str() );
		if ( pDesc == 0 || pDesc->eGameType != SGVOGT_OBJECT )
			continue;
		// Nothing may name it: no passenger, no bridge span, no entrenchment piece.
		bool bNamed = false;
		for ( size_t j = 0; j < rMap.objects.size() && !bNamed; ++j )
			bNamed = rMap.objects[j].link.nLinkWith == rObject.link.nLinkID;
		if ( !bNamed )
			return int( i );
	}
	return -1;
}

static bool BuildCheckMapFixture( BkEditorSession *pSession, CMapInfo *pMap, SCheckMapFixture *pOut )
{
	std::vector<int> taken;
	const int nSource = PickPlainObject( *pMap, taken );
	taken.push_back( nSource );
	const int nInvalid = PickPlainObject( *pMap, taken );
	taken.push_back( nInvalid );
	const int nOwner = PickPlainObject( *pMap, taken );
	taken.push_back( nOwner );
	const int nUnknownSource = PickPlainObject( *pMap, taken );
	if ( !Check( nSource >= 0 && nInvalid >= 0 && nOwner >= 0 && nUnknownSource >= 0, "four plain objects for the defects" ) )
		return false;
	pOut->nDuplicateSource = pMap->objects[nSource].link.nLinkID;
	pOut->nInvalidLinkObject = pMap->objects[nInvalid].link.nLinkID;
	pOut->nOwnerObject = pMap->objects[nOwner].link.nLinkID;

	// The duplicate: the same type, place and frame under a link ID of its own.
	SMapObjectInfo duplicate = pMap->objects[nSource];
	duplicate.link.nLinkID = NMapOverlay::NextLinkID( *pMap );
	duplicate.link.nLinkWith = 0;
	pOut->nDuplicateLink = duplicate.link.nLinkID;
	pMap->objects.push_back( duplicate );
	// A host that is not on the map.
	pMap->objects[nInvalid].link.nLinkWith = 987654;
	// An owner the table does not have.
	pMap->objects[nOwner].nPlayer = 99;
	// A type no database lists, a few tiles from where the plain one stood.
	SMapObjectInfo unknown = pMap->objects[nUnknownSource];
	unknown.szName = "No_Such_Object_In_Any_Database";
	unknown.link.nLinkID = NMapOverlay::NextLinkID( *pMap );
	unknown.link.nLinkWith = 0;
	unknown.vPos.x += 256.0f;
	pOut->nUnknownLink = unknown.link.nLinkID;
	pMap->objects.push_back( unknown );
	// A party partys.xml does not list.
	if ( pMap->unitCreation.units.empty() )
		pMap->unitCreation.units.push_back( SUnitCreation() );
	pMap->unitCreation.units[0].szPartyName = "Narnia";
	pOut->szBadParty = "Narnia";
	// A road with one control point (a file can hold one; the editor will not draw one).
	const std::vector<std::string> roads = VsoDescriptorNames( pSession, 0 );
	std::string szRoad;
	for ( size_t i = 0; i < roads.size() && szRoad.empty(); ++i )
		if ( roads[i].find( "rail" ) == std::string::npos )
			szRoad = roads[i];
	if ( !Check( !szRoad.empty() && AppendExpectedVso( pMap, 0, szRoad, MiddleLine( *pMap, 0.0f, -300.0f ), 3.0f, 1.0f ), "the short road builds" ) )
		return false;
	pOut->nShortRoad = int( pMap->terrain.roads3.size() ) - 1;
	pMap->terrain.roads3.back().controlpoints.resize( 1 );
	return true;
}

// The map the game must survive: coldwinter plus two railroads, one with a single
// control point and one with none - the records that crashed CRailroadGraphConstructor
// (RailroadGraph.cpp, CSplineEdge reads controlpoints[0] and edgeParts[-1]).
static bool BuildShortRailroadMap( BkEditorSession *pSession, CMapInfo *pMap )
{
	const std::vector<std::string> roads = VsoDescriptorNames( pSession, 0 );
	std::string szRail;
	for ( size_t i = 0; i < roads.size() && szRail.empty(); ++i )
		if ( roads[i].find( "rail" ) != std::string::npos )
			szRail = roads[i];
	if ( !Check( !szRail.empty(), "a railroad type for the short-railroad map" ) )
		return false;
	if ( !Check( AppendExpectedVso( pMap, 0, szRail, MiddleLine( *pMap, 0.0f, -300.0f ), 3.0f, 1.0f ), "the one-point railroad builds" ) )
		return false;
	pMap->terrain.roads3.back().eType = SVectorStripeObjectDesc::TYPE_RAILROAD;
	pMap->terrain.roads3.back().controlpoints.resize( 1 );
	if ( !Check( AppendExpectedVso( pMap, 0, szRail, MiddleLine( *pMap, 0.0f, 300.0f ), 3.0f, 1.0f ), "the empty railroad builds" ) )
		return false;
	pMap->terrain.roads3.back().eType = SVectorStripeObjectDesc::TYPE_RAILROAD;
	pMap->terrain.roads3.back().controlpoints.clear();
	return true;
}

// `--craft <kind> <out>`: one fixture map written to a path the scenarios open.
static bool CraftFixture( BkEditorSession *pSession, const char *pszKind, const char *pszOut )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &map, &szError ), szError.c_str() ) )
		return false;
	// The season's road types come from an open map.
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return false;
	if ( strcmp( pszKind, "short-railroad" ) == 0 )
	{
		if ( !BuildShortRailroadMap( pSession, &map ) )
			return false;
	}
	else if ( strcmp( pszKind, "check-map" ) == 0 )
	{
		SCheckMapFixture fixture;
		if ( !BuildCheckMapFixture( pSession, &map, &fixture ) )
			return false;
	}
	else
	{
		printf( "FAIL: no fixture named %s\n", pszKind );
		return false;
	}
	if ( !Check( NMapFile::Write( pszOut, map, &szError ), szError.c_str() ) )
		return false;
	printf( "editor-bridge: crafted %s at %s\n", pszKind, pszOut );
	return true;
}

// The fixes Check Map's Fix all makes, as bridge calls on a map that has every defect:
// a duplicate deleted, an invalid host cleared, an owner out of the table moved to
// the neutral, a party set to one partys.xml lists, an unknown-type object deleted
// and a one-point road deleted. The save is the builder's map (the same overlay calls
// on a fresh read); put back in reverse, the saves are the crafted file's bytes.
static void TestM3CheckMap( BkEditorSession *pSession, const std::string &szScratch )
{
	CMapInfo crafted;
	std::string szError;
	if ( !Check( NMapFile::Read( SHIPPED_MAP, &crafted, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	SCheckMapFixture fixture;
	if ( !BuildCheckMapFixture( pSession, &crafted, &fixture ) )
		return;
	const std::string szMap = szScratch + "\\m3-check-map.bzm";
	const std::string szPre = szScratch + "\\m3-check-map-pre.bzm";
	const std::string szFixed = szScratch + "\\m3-check-map-fixed.bzm";
	const std::string szUndone = szScratch + "\\m3-check-map-undone.bzm";
	if ( !Check( NMapFile::Write( szMap.c_str(), crafted, &szError ), szError.c_str() ) )
		return;
	if ( !Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	Check( BkEditorSaveMap( pSession, szPre.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// What Check Map reads: the crafted defects are there to be found.
	{
		const std::vector<BkEditorObjectRecord> objects = M3ReadObjects( pSession );
		int nSeen = 0;
		for ( size_t i = 0; i < objects.size(); ++i )
		{
			if ( objects[i].link_id == fixture.nUnknownLink )
				nSeen += objects[i].known == 0 ? 1 : 0;
			if ( objects[i].link_id == fixture.nOwnerObject )
				nSeen += objects[i].player == 99 ? 1 : 0;
			if ( objects[i].link_id == fixture.nInvalidLinkObject )
				nSeen += objects[i].link_with == 987654 ? 1 : 0;
		}
		Check( nSeen == 3, "the unknown type, the owner 99 and the missing host read back as crafted" );
		BkEditorVsoInfo info;
		Check( BkEditorVso( pSession, 0, fixture.nShortRoad, &info, 0, 0, 0, 0 ) != BK_EDITOR_FAILED && info.control_count == 1, "the short road reads with one control point" );
	}
	const int nEntries = M3PlayerEntries( pSession );
	const std::vector<std::string> parties = M3UcChoices( pSession, 0 );
	if ( !Check( !parties.empty(), "partys.xml lists a party" ) )
		return;
	BkEditorUnitCreationRecord partyBefore;
	if ( !Check( BkEditorUnitCreation( pSession, 0, &partyBefore ) == BK_EDITOR_OK && std::string( partyBefore.party ) == fixture.szBadParty, "the bad party reads back" ) )
		return;

	// The fixes, in Fix all's own order.
	int nUnlinkToken = -1, nOwnerToken = -1, nRoadToken = -1;
	Check( BkEditorDeleteObject( pSession, fixture.nDuplicateLink ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorUnlink( pSession, fixture.nInvalidLinkObject, &nUnlinkToken ) == BK_EDITOR_OK && nUnlinkToken >= 0, BkEditorLastMessage( pSession ) );
	BkEditorObjectFieldsEdit owner;
	memset( &owner, 0, sizeof owner );
	owner.mask = 1;
	owner.player = nEntries - 1;
	Check( BkEditorSetObjectFields( pSession, fixture.nOwnerObject, &owner, &nOwnerToken ) == BK_EDITOR_OK && nOwnerToken >= 0,
	       ( std::string( "the out-of-table owner moves to the neutral: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	BkEditorUnitCreationRecord partyAfter = partyBefore;
	strcpy( partyAfter.party, parties[0].c_str() );
	Check( BkEditorSetUnitCreation( pSession, 0, &partyAfter ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorDeleteObject( pSession, fixture.nUnknownLink ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorDeleteVso( pSession, 0, fixture.nShortRoad, &nRoadToken ) == BK_EDITOR_OK && nRoadToken >= 0, BkEditorLastMessage( pSession ) );

	// The map is the builder's: the same calls on a fresh read of the crafted file.
	CMapInfo expected;
	if ( Check( NMapFile::Read( szMap.c_str(), &expected, &szError ), szError.c_str() ) )
	{
		std::string szRefusal;
		Check( NMapOverlay::DeleteObject( &expected, fixture.nDuplicateLink, &szRefusal ), szRefusal.c_str() );
		Check( NMapRecords::SetObjectLink( &expected, fixture.nInvalidLinkObject, 0 ), "the builder clears the link" );
		Check( NMapRecords::SetObjectPlayer( &expected, fixture.nOwnerObject, nEntries - 1 ), "the builder re-owns the object" );
		SUnitCreation unit;
		NMapRecords::GetUnitCreation( expected, 0, &unit );
		unit.szPartyName = parties[0];
		Check( NMapRecords::PutUnitCreation( &expected, 0, unit, int( expected.unitCreation.units.size() ) ), "the builder sets the party" );
		Check( NMapOverlay::DeleteObject( &expected, fixture.nUnknownLink, &szRefusal ), szRefusal.c_str() );
		Check( NMapRecords::EraseVso( &expected, NMapRecords::VSO_ROAD, fixture.nShortRoad ), "the builder deletes the short road" );
		CheckSavedEquals( pSession, szFixed, expected, "Fix all's fixes" );
	}

	// Put back in reverse: the road, the unknown object, the party, the owner, the
	// link, the duplicate. The save is the crafted file's own bytes.
	Check( BkEditorUndoEdit( pSession, nRoadToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorRestoreObject( pSession, fixture.nUnknownLink ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSetUnitCreation( pSession, 0, &partyBefore ) == BK_EDITOR_OK, ( std::string( "the file's own party goes back: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorUndoEdit( pSession, nOwnerToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorUndoEdit( pSession, nUnlinkToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorRestoreObject( pSession, fixture.nDuplicateLink ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorSaveMap( pSession, szUndone.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szPre, szUndone ), ( "Fix all's fixes put back in reverse save the crafted file byte for byte (" + DescribeDifference( szPre, szUndone ) + ")" ).c_str() );

	remove( OsPath( szMap ).c_str() );
	remove( OsPath( szPre ).c_str() );
	remove( OsPath( szFixed ).c_str() );
	remove( OsPath( szUndone ).c_str() );
	printf( "editor-bridge: M3 check map ok\n" );
}

// M3 05-07 (D-14): the Minimap panel's reads on the engine. The tiles come out
// of the map as the file holds them, and the colour of a tile is
// CreateMiniMapImage's own averaging (recomputed here the way the static does
// it, straight from the tileset's "_h.dds") of the terrain type's first tile;
// the markers are the MFC's own (five AI tiles square around a unit, the
// player's colour of the 17, a squad flagged); the fire-range areas are the
// AI's own and empty until a group shows them; and none of it touches the map.
static void TestM3MinimapReads( BkEditorSession *pSession, const std::string &szScratch )
{
	BkEditorMapSummary summary;
	memset( &summary, 0, sizeof summary );
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, &summary ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( BRIDGE_MAP, &original, &szError ), szError.c_str() ) )
		return;
	const int nWidth = summary.width_tiles, nHeight = summary.height_tiles;
	const std::string szBefore = szScratch + "\\m3-minimap-reads-before.bzm";
	const std::string szAfter = szScratch + "\\m3-minimap-reads-after.bzm";
	Check( BkEditorSaveMap( pSession, szBefore.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );

	// --- The tiles: a sizing pass that answers REFUSED with the count, then the
	// read, equal to the file's own tiles cell for cell and to the engine's.
	{
		const BkEditorTileRegion all = { 0, 0, nWidth, nHeight };
		int nCount = -1;
		Check( BkEditorTiles( pSession, &all, 0, 0, &nCount ) == BK_EDITOR_REFUSED && nCount == nWidth * nHeight,
		       "the tiles' sizing pass answers REFUSED with the whole map's count" );
		std::vector<unsigned char> tiles( size_t( nCount > 0 ? nCount : 1 ) );
		int nRead = 0;
		if ( Check( BkEditorTiles( pSession, &all, &tiles[0], nCount, &nRead ) == BK_EDITOR_OK && nRead == nCount, "the tiles read" ) )
		{
			int nDifferent = 0;
			for ( int y = 0; y < nHeight; ++y )
				for ( int x = 0; x < nWidth; ++x )
					if ( tiles[size_t( y * nWidth + x )] != original.terrain.tiles[y][x].tile )
						++nDifferent;
			Check( nDifferent == 0, "every tile read is the file's own, row 0 first" );
			int nEngineDifferent = 0;
			for ( int y = 0; y < nHeight; y += 37 )
				for ( int x = 0; x < nWidth; x += 29 )
				{
					unsigned char tile = 0;
					if ( BkEditorEngineTile( pSession, x, y, &tile ) != BK_EDITOR_OK || tile != tiles[size_t( y * nWidth + x )] )
						++nEngineDifferent;
				}
			Check( nEngineDifferent == 0, "the tiles read are the engine's own at a sample of cells" );
		}
		const BkEditorTileRegion part = { 10, 20, 14, 23 };
		unsigned char partTiles[12];
		Check( BkEditorTiles( pSession, &part, partTiles, 12, &nRead ) == BK_EDITOR_OK && nRead == 12, "a sub-region reads" );
		bool bPartSame = true;
		for ( int y = 0; y < 3; ++y )
			for ( int x = 0; x < 4; ++x )
				bPartSame = bPartSame && partTiles[y * 4 + x] == original.terrain.tiles[20 + y][10 + x].tile;
		Check( bPartSame, "the sub-region is the file's tiles at those cells" );
		unsigned char tooShort[4];
		Check( BkEditorTiles( pSession, &part, tooShort, 4, &nRead ) == BK_EDITOR_REFUSED && nRead == 12, "a short buffer is refused with the count and nothing past its end" );
		const BkEditorTileRegion empty = { 5, 5, 5, 9 }, off = { 0, 0, nWidth + 1, 4 }, negative = { -1, 0, 4, 4 };
		Check( BkEditorTiles( pSession, &empty, partTiles, 12, &nRead ) == BK_EDITOR_BAD_ARGUMENT, "an empty region is a bad argument" );
		Check( BkEditorTiles( pSession, 0, partTiles, 12, &nRead ) == BK_EDITOR_BAD_ARGUMENT, "a null region is a bad argument" );
		Check( BkEditorTiles( pSession, &off, partTiles, 12, &nRead ) == BK_EDITOR_REFUSED, "a region off the map is refused" );
		Check( BkEditorTiles( pSession, &negative, partTiles, 12, &nRead ) == BK_EDITOR_REFUSED, "a region before the map is refused" );
	}

	// --- The colours: one per tile of the tileset, equal to a recomputation of
	// CreateMiniMapImage's averaging for three spot-check terrain types.
	{
		int nColors = -1;
		Check( BkEditorMinimapTileColors( pSession, 0, 0, &nColors ) == BK_EDITOR_REFUSED && nColors > 0 && nColors <= 256,
		       "the colours' sizing pass answers REFUSED with the tileset's tile count" );
		std::vector<unsigned int> colors( size_t( nColors > 0 ? nColors : 1 ) );
		int nRead = 0;
		if ( Check( BkEditorMinimapTileColors( pSession, &colors[0], nColors, &nRead ) == BK_EDITOR_OK && nRead == nColors, "the tile colours read" ) )
		{
			bool bAllInRange = true;
			for ( int y = 0; y < nHeight && bAllInRange; ++y )
				for ( int x = 0; x < nWidth; ++x )
					if ( int( original.terrain.tiles[y][x].tile ) >= nColors )
					{
						bAllInRange = false;
						break;
					}
			Check( bAllInRange, "every tile the map uses has a colour" );

			BkEditorTile described;
			memset( &described, 0, sizeof described );
			const unsigned char firstTile = original.terrain.tiles[0][0].tile;
			if ( Check( BkEditorDescribeTile( pSession, firstTile, &described ) == BK_EDITOR_OK, "the map's first tile is described" ) )
			{
				const std::string szTileset = described.tileset;
				STilesetDesc tilesetDesc;
				LoadDataResource( szTileset, "", false, 0, "tileset", tilesetDesc );
				CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( ( szTileset + "_h.dds" ).c_str(), STREAM_ACCESS_READ );
				CPtr<IDDSImage> pImage = pStream != 0 ? GetImageProcessor()->LoadDDSImage( pStream ) : 0;
				if ( Check( pImage != 0 && !tilesetDesc.terrtypes.empty(), "the tileset's own description and _h.dds load for the recomputation" ) )
				{
					CTImageAccessor< SColor, IDDSImage, CPtr<IDDSImage> > accessor = pImage;
					// CreateMiniMapImage's loop for one tile, verbatim.
					struct SRecompute
					{
						static unsigned int Average( CTImageAccessor< SColor, IDDSImage, CPtr<IDDSImage> > &rAccessor, IDDSImage *pDDS, const STilesetDesc &rDesc, int nTile )
						{
							const CVec2 *pVertices = rDesc.tilemaps[nTile].maps;
							CTRect<int> colorRect( ( pVertices[0].x * pDDS->GetSizeX() + pVertices[2].x * pDDS->GetSizeX() ) / 2,
							                       ( pVertices[0].y * pDDS->GetSizeY() + pVertices[2].y * pDDS->GetSizeY() ) / 2,
							                       ( pVertices[1].x * pDDS->GetSizeX() + pVertices[3].x * pDDS->GetSizeX() ) / 2,
							                       ( pVertices[1].y * pDDS->GetSizeY() + pVertices[3].y * pDDS->GetSizeY() ) / 2 );
							colorRect.Normalize();
							DWORD dwRed = 0, dwGreen = 0, dwBlue = 0;
							for ( int x = colorRect.left; x < colorRect.right; ++x )
								for ( int y = colorRect.top; y < colorRect.bottom; ++y )
								{
									const SColor &rColor = rAccessor[y][x];
									dwRed += rColor.r;
									dwGreen += rColor.g;
									dwBlue += rColor.b;
								}
							if ( colorRect.Width() * colorRect.Height() > 0 )
							{
								dwRed /= colorRect.Width() * colorRect.Height();
								dwGreen /= colorRect.Width() * colorRect.Height();
								dwBlue /= colorRect.Width() * colorRect.Height();
							}
							return ( ( dwRed & 0xFF ) << 16 ) | ( ( dwGreen & 0xFF ) << 8 ) | ( dwBlue & 0xFF );
						}
					};
					int nSpot = 0, nSpotMismatch = 0;
					const size_t nTypes = tilesetDesc.terrtypes.size();
					const size_t spotTypes[3] = { 0, nTypes / 2, nTypes - 1 };
					for ( int i = 0; i < 3; ++i )
					{
						const std::vector<SMainTileDesc> &rTypeTiles = tilesetDesc.terrtypes[spotTypes[i]].tiles;
						if ( rTypeTiles.empty() )
							continue;
						const int nTile = rTypeTiles[0].nIndex;
						if ( nTile < 0 || nTile >= nColors || nTile >= int( tilesetDesc.tilemaps.size() ) )
							continue;
						// The first terrain type that lists the tile decides the representative.
						int nRepresentative = nTile;
						for ( size_t t = 0; t < nTypes; ++t )
						{
							bool bLists = false;
							for ( size_t k = 0; k < tilesetDesc.terrtypes[t].tiles.size(); ++k )
								bLists = bLists || tilesetDesc.terrtypes[t].tiles[k].nIndex == nTile;
							if ( bLists )
							{
								if ( !tilesetDesc.terrtypes[t].tiles.empty() )
									nRepresentative = tilesetDesc.terrtypes[t].tiles[0].nIndex;
								break;
							}
						}
						const unsigned int expected = SRecompute::Average( accessor, pImage, tilesetDesc, nRepresentative );
						++nSpot;
						if ( colors[size_t( nTile )] != expected )
						{
							++nSpotMismatch;
							printf( "editor-bridge: tile %d colour %06x, CreateMiniMapImage's averaging says %06x\n", nTile, colors[size_t( nTile )], expected );
						}
					}
					Check( nSpot >= 3 && nSpotMismatch == 0, "three spot-check tiles match CreateMiniMapImage's averaging exactly" );
					printf( "editor-bridge: M3 minimap colour spot check: %d tiles compared, %d differ\n", nSpot, nSpotMismatch );
				}
			}
		}
		unsigned int tooShort[2];
		Check( BkEditorMinimapTileColors( pSession, tooShort, 2, &nRead ) == BK_EDITOR_REFUSED && nRead == nColors, "a short colour buffer is refused with the count" );
	}

	// --- The markers: five AI tiles square around a unit, its player's colour,
	// a squad flagged.
	{
		int nUnits = -1;
		Check( BkEditorMinimapUnits( pSession, 0, 0, &nUnits ) == BK_EDITOR_REFUSED && nUnits > 0, "the markers' sizing pass answers REFUSED with the count" );
		std::vector<BkEditorMinimapUnit> units( size_t( nUnits > 0 ? nUnits : 1 ) );
		int nRead = 0;
		Check( BkEditorMinimapUnits( pSession, &units[0], nUnits, &nRead ) == BK_EDITOR_OK && nRead == nUnits, "the markers read" );
		bool bAllSane = true;
		int nSquads = 0;
		for ( int i = 0; i < nRead; ++i )
		{
			const BkEditorMinimapUnit &rUnit = units[size_t( i )];
			bAllSane = bAllSane && rUnit.x0 >= 0 && rUnit.y0 >= 0 && rUnit.x1 > rUnit.x0 && rUnit.y1 > rUnit.y0 &&
			           rUnit.color_index >= 0 && rUnit.color_index <= 16;
			nSquads += rUnit.squad;
		}
		Check( bAllSane, "every marker is a non-empty rectangle with a colour of the 17" );
		Check( nSquads > 0, "arnheim's squads have their markers" );

		// A unit placed at a known point, player 1: a 5x5 square there.
		int nCatalogue = 0;
		BkEditorCatalogue( pSession, 0, 0, &nCatalogue );
		std::vector<BkEditorCatalogueEntry> catalogue( size_t( nCatalogue > 0 ? nCatalogue : 1 ) );
		int nCatalogueRead = 0;
		BkEditorCatalogue( pSession, &catalogue[0], nCatalogue, &nCatalogueRead );
		std::string szUnitName;
		for ( int i = 0; i < nCatalogueRead && szUnitName.empty(); ++i )
			if ( catalogue[size_t( i )].game_type == 1 )
				szUnitName = catalogue[size_t( i )].name;
		int nLinkID = -1;
		const float fMiddleX = nWidth * fWorldCellSize / 2, fMiddleY = nHeight * fWorldCellSize / 2;
		if ( Check( !szUnitName.empty() && BkEditorAddObject( pSession, szUnitName.c_str(), fMiddleX, fMiddleY, 0, 1, &nLinkID ) == BK_EDITOR_OK, "a unit is placed at the middle" ) )
		{
			BkEditorObjectRecord record;
			memset( &record, 0, sizeof record );
			ReadObjectRecord( pSession, nLinkID, &record );
			const int nCentreX = int( record.x / SAIConsts::TILE_SIZE ), nCentreY = int( record.y / SAIConsts::TILE_SIZE );
			Check( BkEditorMinimapUnits( pSession, 0, 0, &nUnits ) == BK_EDITOR_REFUSED, "the markers count again" );
			units.assign( size_t( nUnits ), BkEditorMinimapUnit() );
			Check( BkEditorMinimapUnits( pSession, &units[0], nUnits, &nRead ) == BK_EDITOR_OK, "and read again" );
			const BkEditorMinimapUnit *pPlaced = 0;
			for ( int i = 0; i < nRead; ++i )
				if ( units[size_t( i )].link_id == nLinkID )
					pPlaced = &units[size_t( i )];
			if ( Check( pPlaced != 0, "the placed unit has a marker" ) )
			{
				Check( pPlaced->x0 == nCentreX - 2 && pPlaced->x1 == nCentreX + 3 && pPlaced->y0 == nCentreY - 2 && pPlaced->y1 == nCentreY + 3,
				       ( "the marker is five AI tiles square around the unit (" + std::to_string( pPlaced->x0 ) + "," + std::to_string( pPlaced->y0 ) + "-" +
				         std::to_string( pPlaced->x1 ) + "," + std::to_string( pPlaced->y1 ) + " around " + std::to_string( nCentreX ) + "," + std::to_string( nCentreY ) + ")" ).c_str() );
				Check( pPlaced->color_index == 1 && pPlaced->squad == 0, "its colour is player 1's and it is no squad" );
			}

			// --- The fire-range areas: empty until a group shows them. Units are
			// tried until one has an area the AI draws (artillery does; a plain
			// rifleman's is a line, which the MFC panel skips).
			int nAreas = -1;
			Check( BkEditorMinimapAreas( pSession, 0, 0, &nAreas ) == BK_EDITOR_OK && nAreas == 0, "no areas are shown until a group shows them" );
			IAILogic *pAILogic = GetSingleton<IAILogic>();
			bool bAreasSeen = false;
			if ( Check( pAILogic != 0, "the AI is there" ) )
			{
				int nObjects = 0;
				BkEditorObjects( pSession, 0, 0, &nObjects );
				std::vector<BkEditorObjectRecord> records( size_t( nObjects > 0 ? nObjects : 1 ) );
				int nObjectsRead = 0;
				BkEditorObjects( pSession, &records[0], nObjects, &nObjectsRead );
				for ( int i = 0; i < nObjectsRead && !bAreasSeen; ++i )
				{
					bool bIsUnit = false;
					for ( int j = 0; j < nCatalogueRead; ++j )
						bIsUnit = bIsUnit || ( catalogue[size_t( j )].game_type == 1 && records[size_t( i )].name == std::string( catalogue[size_t( j )].name ) );
					if ( !bIsUnit || records[size_t( i )].link_id <= 0 || !records[size_t( i )].known )
						continue;
					IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
					IRefCount *pAIObject = pAIEditor != 0 ? pAIEditor->ObjectByLink( records[size_t( i )].link_id ) : 0;
					if ( pAIObject == 0 )
						continue;
					const WORD wGroup = pAILogic->GenerateGroupNumber();
					pAILogic->RegisterGroup( &pAIObject, 1, wGroup );
					pAILogic->ShowAreas( wGroup, ACTION_NOTIFY_SHOOT_AREA, true );
					int nShown = 0;
					if ( BkEditorMinimapAreas( pSession, 0, 0, &nShown ) == BK_EDITOR_REFUSED && nShown > 0 )
					{
						std::vector<BkEditorMinimapArea> areas;
						areas.resize( size_t( nShown ) );
						int nAreasRead = 0;
						Check( BkEditorMinimapAreas( pSession, &areas[0], nShown, &nAreasRead ) == BK_EDITOR_OK && nAreasRead == nShown, "the shown areas read" );
						bool bSane = true;
						for ( int a = 0; a < nAreasRead; ++a )
							bSane = bSane && areas[size_t( a )].radius > 0 && areas[size_t( a )].kind != 2 && areas[size_t( a )].kind >= 0 && areas[size_t( a )].kind <= 3;
						Check( bSane, "every area has a radius and is not a line" );
						printf( "editor-bridge: M3 minimap areas: %d shown for %s (first: kind %d, radius %.1f AI units, at %.0f,%.0f)\n", nAreasRead,
						        records[size_t( i )].name, areas[0].kind, areas[0].radius, areas[0].cx, areas[0].cy );
						bAreasSeen = true;
					}
					pAILogic->ShowAreas( wGroup, ACTION_NOTIFY_SHOOT_AREA, false );
					pAILogic->UnregisterGroup( wGroup );
				}
			}
			Check( bAreasSeen, "a unit's fire-range areas are answered while a group shows them" );
			Check( BkEditorDeleteObject( pSession, nLinkID ) == BK_EDITOR_OK, "the placed unit goes again" );
		}
	}

	// The reads never touched the map: what is saved after is what was saved
	// before.
	Check( BkEditorSaveMap( pSession, szAfter.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	remove( OsPath( szBefore ).c_str() );
	remove( OsPath( szAfter ).c_str() );
	printf( "editor-bridge: M3 minimap reads ok\n" );
}

// M3 05-07 (D-16/D-17): Create Minimap Images on the engine. A saved user map
// gets its pictures beside it - the MFC's four parameters, <map>_large at 512
// and <map> at 256, each as TGA and as the engine's DDS trio - verified by
// size; the shipped map is refused and nothing is written beside it; a
// malformed or missing picture is a refusal, never a crash; and the map
// document is never touched.
static void TestM3MinimapImages( BkEditorSession *pSession, const std::string &szScratch )
{
	if ( !Check( BkEditorOpenMap( pSession, BRIDGE_MAP, 0 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		return;
	const std::string szFolder = szScratch + "\\m3-minimap-images";
	MakeDirectory( OsPath( szFolder ).c_str() );
	const std::string szMap = szFolder + "\\m3img.bzm";
	const std::string szMapAgain = szFolder + "\\m3img-again.bzm";
	Check( BkEditorSaveMap( pSession, szMap.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	const std::string szBase = szFolder + "\\m3img";

	const BkEditorStatus nStatus = BkEditorCreateMiniMapImage( pSession, szMap.c_str() );
	if ( !Check( nStatus == BK_EDITOR_OK, ( std::string( "the images are created: " ) + BkEditorLastMessage( pSession ) ).c_str() ) )
		return;
	static const char *const suffixes[] = { "_large.tga", "_large_c.dds", "_large_l.dds", "_large_h.dds", ".tga", "_c.dds", "_l.dds", "_h.dds" };
	int nPresent = 0;
	for ( int i = 0; i < 8; ++i )
	{
		std::error_code error;
		if ( std::filesystem::exists( OsPath( szBase + suffixes[i] ), error ) )
			++nPresent;
		else
			printf( "editor-bridge: missing %s\n", ( szBase + suffixes[i] ).c_str() );
	}
	Check( nPresent == 8, "all eight files (the four pictures, DDS as the engine's trio) are beside the map" );

	// Game mode's read: <map>.tga is tried first and is the 256x256 one.
	std::vector<unsigned char> rgba( 512 * 512 * 4 );
	int nWidth = 0, nHeight = 0;
	if ( Check( BkEditorMinimapImage( pSession, szMap.c_str(), &rgba[0], int( rgba.size() ), 1024, &nWidth, &nHeight ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( nWidth == 256 && nHeight == 256, "the Game mode picture is the 256x256 <map>.tga" );
	// The same call scaled down.
	if ( Check( BkEditorMinimapImage( pSession, szMap.c_str(), &rgba[0], int( rgba.size() ), 64, &nWidth, &nHeight ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
		Check( nWidth == 64 && nHeight == 64, "a smaller max_side scales the picture down" );
	Check( BkEditorMinimapImage( pSession, szMap.c_str(), &rgba[0], 16, 1024, &nWidth, &nHeight ) == BK_EDITOR_REFUSED && nWidth == 256 && nHeight == 256,
	       "a buffer too short is refused and the real size reported" );

	// Never touches the map document.
	Check( BkEditorSaveMap( pSession, szMapAgain.c_str() ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( SameBytes( szMap, szMapAgain ), "creating the pictures left the map's own save byte for byte" );

	// The shipped map: refused, and nothing is written beside it.
	{
		const BkEditorStatus nShipped = BkEditorCreateMiniMapImage( pSession, BRIDGE_MAP );
		Check( nShipped == BK_EDITOR_REFUSED, "a shipped map (a relative Data path) is refused" );
		#if defined(_WIN32) || defined(_WIN64)
		char absolute[_MAX_PATH] = { 0 };
		_fullpath( absolute, "Data/Maps/Multiplayer/arnheim.bzm", _MAX_PATH );
#else
		char absolute[PATH_MAX] = { 0 };
		if ( realpath( "Data/Maps/Multiplayer/arnheim.bzm", absolute ) == 0 )
			absolute[0] = 0;
#endif
		const std::string szAbsolute = absolute;
		Check( BkEditorCreateMiniMapImage( pSession, szAbsolute.c_str() ) == BK_EDITOR_REFUSED, "the same map by its full path is refused" );
		Check( std::string( BkEditorLastMessage( pSession ) ).find( "Data" ) != std::string::npos, ( "and the refusal names the Data folder (" + std::string( BkEditorLastMessage( pSession ) ) + " for " + szAbsolute + ")" ).c_str() );
		std::error_code error;
		Check( !std::filesystem::exists( "Data/Maps/Multiplayer/arnheim_large.tga", error ), "nothing was written beside the shipped map" );
		Check( BkEditorCreateMiniMapImage( pSession, ( szFolder + "\\m3img-not-there.bzm" ).c_str() ) == BK_EDITOR_REFUSED, "a map file that is not there is refused" );
		Check( BkEditorCreateMiniMapImage( pSession, ( szFolder + "\\m3img.txt" ).c_str() ) == BK_EDITOR_BAD_ARGUMENT, "a path that is no .bzm or .xml is a bad argument" );
		Check( BkEditorCreateMiniMapImage( pSession, 0 ) == BK_EDITOR_BAD_ARGUMENT, "a null path is a bad argument" );
	}

	// A malformed picture is a refusal.
	{
		const std::string szGarbageMap = szFolder + "\\m3garbage.bzm";
		FILE *pFile = fopen( OsPath( szFolder + "\\m3garbage.tga" ).c_str(), "wb" );
		if ( Check( pFile != 0, "the malformed picture is written" ) )
		{
			const char garbage[40] = "this is not a tga file at all, no sir!";
			fwrite( garbage, 1, sizeof garbage, pFile );
			fclose( pFile );
			const BkEditorStatus nGarbage = BkEditorMinimapImage( pSession, szGarbageMap.c_str(), &rgba[0], int( rgba.size() ), 1024, &nWidth, &nHeight );
			Check( nGarbage == BK_EDITOR_REFUSED, "a malformed picture is refused" );
		}
		Check( BkEditorMinimapImage( pSession, ( szFolder + "\\m3nothing.bzm" ).c_str(), &rgba[0], int( rgba.size() ), 1024, &nWidth, &nHeight ) == BK_EDITOR_REFUSED,
		       "a map with no picture is refused" );
		Check( BkEditorMinimapImage( pSession, szMap.c_str(), 0, 0, 1024, &nWidth, &nHeight ) == BK_EDITOR_BAD_ARGUMENT, "no buffer is a bad argument" );
		remove( OsPath( szFolder + "\\m3garbage.tga" ).c_str() );
	}

	// The shipped coldwinter has only its _h.dds: Game mode finds it there.
	{
		const std::string szColdwinter = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
		if ( Check( BkEditorMinimapImage( pSession, szColdwinter.c_str(), &rgba[0], int( rgba.size() ), 1024, &nWidth, &nHeight ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) ) )
			Check( nWidth > 0 && nHeight > 0, "a shipped map's _h.dds picture is found for Game mode" );
	}

	for ( int i = 0; i < 8; ++i )
		remove( OsPath( szBase + suffixes[i] ).c_str() );
	remove( OsPath( szMap ).c_str() );
	remove( OsPath( szMapAgain ).c_str() );
	printf( "editor-bridge: M3 minimap images ok\n" );
}
