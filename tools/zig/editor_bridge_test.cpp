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
#include "../../Sources/src/Image/Image.h"

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
	if ( !Check( BkEditorOpenMap( pSession, SHIPPED_MAP, 0 ) == BK_EDITOR_OK, "the map opens" ) )
		return;

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

// The frame just drawn, as an uncompressed 32-bit TGA, so a person can look at
// what the pick ratio only counts. Alpha is forced opaque for the reason
// CMainLoop gives for its own screenshots: the scene texture's alpha is
// whatever the passes left behind.
static bool SaveFrame( const std::string &szPath )
{
	IGFX *pGFX = GetSingleton<IGFX>();
	IImageProcessor *pImages = GetImageProcessor();
	if ( pGFX == 0 || pImages == 0 )
		return false;
	const RECT rcScreen = pGFX->GetScreenRect();
	const int nWidth = rcScreen.right - rcScreen.left, nHeight = rcScreen.bottom - rcScreen.top;
	CPtr<IImage> pImage = pImages->CreateImage( nWidth, nHeight );
	if ( pImage == 0 || !pGFX->TakeScreenShot( pImage ) )
		return false;
	FILE *pFile = fopen( szPath.c_str(), "wb" );
	if ( pFile == 0 )
		return false;
	// Type 2, 32 bits per pixel, top-left origin (descriptor 0x28: 8 alpha bits
	// and the top-to-bottom flag).
	unsigned char header[18] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                             (unsigned char)( nWidth & 0xff ), (unsigned char)( nWidth >> 8 ),
	                             (unsigned char)( nHeight & 0xff ), (unsigned char)( nHeight >> 8 ), 32, 0x28 };
	fwrite( header, 1, sizeof header, pFile );
	const SColor *pPixels = pImage->GetLFB();
	for ( int i = 0; i < nWidth * nHeight; ++i )
	{
		const unsigned char bgra[4] = { (unsigned char)pPixels[i].b, (unsigned char)pPixels[i].g, (unsigned char)pPixels[i].r, 255 };
		fwrite( bgra, 1, 4, pFile );
	}
	fclose( pFile );
	return true;
}

// A camera put on an object answers, at the middle of the screen, with that
// object - the picking half of "the camera is on cell 83,36 and the middle of
// the screen is 83,36".
// How far above the middle of the screen the pick is made. The camera's anchor
// is at height 0 and an object stands on the ground, which on this map is up to
// about 12 pixels above or below that (measured: an object at height 11.7 lands
// 9 pixels higher on the screen, one at -17 11 pixels lower). A sprite's hit box
// rises from its foot and never reaches below it, so a point exactly in the
// middle misses every object standing a little higher than height 0; measured,
// 4 of 20 were picked there and 11 of 20 at this rise.
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
				const bool bWritten = SaveFrame( szFrame );
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
	// 11 of 20; the misses are a neighbour the scene listed first (the MFC
	// editor, copied here, takes the first) and one object at the map's edge
	// where the camera stops short of it. Half is the bar, just under that.
	Check( nTried > 0 && nPicked * 2 >= nTried, "the object under the camera is the one picked, for most objects" );

	int nNothing = -1;
	BkEditorSetCamera( pSession, 16.0f, 16.0f );
	BkEditorFrame( pSession );
	Check( BkEditorObjectAt( pSession, 0.0f, 0.0f, &nNothing ) != BK_EDITOR_FAILED, "a point over nothing is an answer, not a failure" );
	Check( BkEditorObjectAt( pSession, 0.0f, 0.0f, 0 ) == BK_EDITOR_BAD_ARGUMENT, "and nowhere to put the answer is a bad argument" );
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
	const unsigned char tile = (unsigned char)( ( original.terrain.tiles[20][20].tile + 1 ) % 4 );
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
	                                   (unsigned char)( ( original.terrain.tiles[nSizeY - 1][nSizeX - 1].tile + 1 ) % 4 ) };
	int nToken = -1;
	Check( BkEditorPaint( pSession, &corner, 1, &nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK,
	       ( std::string( "a paint in the last patch row and column: " ) + BkEditorLastMessage( pSession ) ).c_str() );
	Check( BkEditorUndoPaint( pSession, nToken ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorTerrainMatchesEngine( pSession ) == BK_EDITOR_OK,
	       ( std::string( "and its undo: " ) + BkEditorLastMessage( pSession ) ).c_str() );

	const BkEditorPaintCell partly[] = {
		{ 5, 5, (unsigned char)( ( original.terrain.tiles[5][5].tile + 1 ) % 4 ) },
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
	Check( BkEditorRestoreObject( pSession, nLinkID ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
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
		TestShippedMapOpens( pSession );
		TestBridgeSpansAreBuilt( pSession );
		TestUneditedSaveIsEquivalent( pSession, szScratch );
		TestObjectEdits( pSession, szScratch );
		TestRefusedEditsReachNeither( pSession, szScratch );
		TestPartlyRefusedEditRollsTheEngineBack( pSession, szScratch );
		TestMapsOwnFields( pSession, szScratch );
		TestObjectsReadBack( pSession );
		TestUnknownObjectIsReadOnly( pSession, szScratch );
		TestPaintReachesEngineAndFile( pSession, szScratch );
		TestPaintUndoIsExact( pSession, szScratch );
		TestPaintAtTheEdgeAndRefused( pSession, szScratch );
		TestDeleteRestoreKeepsTheObject( pSession, szScratch );
		// Not the 640x480 the window was created at: BkEditorStart sets the mode
		// with no size, and the SDL GPU adapter gives the window its display's
		// desktop size (GraphicsEngineGpu::SetMode). The middle of the screen is
		// the middle of what the engine draws.
		const RECT rcScreen = GetSingleton<IGFX>()->GetScreenRect();
		const int nScreenWidth = rcScreen.right - rcScreen.left, nScreenHeight = rcScreen.bottom - rcScreen.top;
		printf( "editor-bridge: the screen is %dx%d\n", nScreenWidth, nScreenHeight );
		TestCatalogueCameraAndFrame( pSession, nScreenWidth, nScreenHeight );
		TestObjectUnderTheCursor( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestSquadDeletesAndRestores( pSession, nScreenWidth, nScreenHeight, szScratch );
		TestDeleteIsRefusedWhileReferred( pSession, szScratch );
		TestSharedLinkIDIsReadOnly( pSession, szScratch );
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
