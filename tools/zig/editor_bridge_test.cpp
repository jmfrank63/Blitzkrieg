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
#include "../../Sources/src/Formats/fmtTerrain.h"
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
	if ( !Check( BkEditorPaint( pSession, &cell, 1 ) == BK_EDITOR_OK, "a cell paints through the bridge" ) )
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
static void TestCatalogueCameraAndFrame( BkEditorSession *pSession )
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
	if ( !Check( BkEditorScreenToWorld( pSession, 320.0f, 240.0f, &wx, &wy ) == BK_EDITOR_OK,
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
		TestRefusedEditsReachNeither( pSession, szScratch );
		TestPartlyRefusedEditRollsTheEngineBack( pSession, szScratch );
		TestMapsOwnFields( pSession, szScratch );
		TestPaintReachesEngineAndFile( pSession, szScratch );
		TestCatalogueCameraAndFrame( pSession );
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
