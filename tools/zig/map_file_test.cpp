// The map file tier. Runs with no window and no GPU device: see
// tools/zig/data_only_startup.cpp for what "no window" costs.
#include "StdAfx.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include "data_only_startup.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
#include "../../Sources/src/MapFile/MapOverlay.h"
#include "../../Sources/src/MapFile/MapRecords.h"
#include "../../Sources/src/MapFile/MapGeometry.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"
#include "../../Sources/src/RandomMapGen/VSO_Types.h"
#include "../../Sources/src/Formats/fmtTerrain.h"

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

static bool TestReadsASmallMap()
{
	CMapInfo map;
	std::string szError;
	// Backslashes: OpenFileStream splits a path on '\\' only
	// (StreamIO/StructureSaver.h:96), so a forward-slash path becomes one long
	// file name in a storage rooted at ".\". Every path in this tier is written
	// the way the engine writes them.
	// 157 KB, the smallest shipped map; see the research note's fixture list.
	const bool bRead = NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError );
	Check( bRead, szError.empty() ? "coldwinter.bzm reads" : szError.c_str() );
	if ( !bRead )
		return false;
	Check( map.terrain.tiles.GetSizeX() > 0, "the map has tiles" );
	Check( !map.terrain.szTilesetDesc.empty(), "the map names a tileset" );
	return true;
}

static void TestReadsXmlAndPicksTheNewer()
{
	CMapInfo fromXml;
	std::string szError;
	// Data\Maps ships exactly two .xml maps: river3d and road3d.
	Check( NMapFile::Read( "Data\\Maps\\river3d.xml", &fromXml, &szError ),
	       szError.empty() ? "an .xml map reads" : szError.c_str() );

	CMapInfo newest;
	szError.clear();
	// ReadNewest takes a storage-relative base name, not a path: it asks the
	// storage for both files' stats, as the game does.
	Check( NMapFile::ReadNewest( "maps\\Multiplayer\\coldwinter", &newest, &szError ),
	       szError.empty() ? "a map name with no extension reads" : szError.c_str() );
	Check( newest.terrain.tiles.GetSizeX() > 0, "the map picked by mtime has tiles" );

	CMapInfo missing;
	szError.clear();
	Check( !NMapFile::ReadNewest( "maps\\Multiplayer\\no_such_map", &missing, &szError ),
	       "a map that is not there fails" );
	Check( !szError.empty(), "and says so" );
}

// A file that is not a map at all: the structure saver will not open over it
// and comes back null, which the reader used to call through.
static void TestRejectsAFileThatIsNotAMap()
{
	// Written by hand, so with the OS's separator; read the engine's way.
	if ( FILE *pFile = fopen( "zig-out/local-test/not-a-map.bzm", "wb" ) )
	{
		const char garbage[] = "this is not a map, and the reader has to say so";
		fwrite( garbage, 1, sizeof garbage, pFile );
		fclose( pFile );
	}
	CMapInfo map;
	std::string szError;
	Check( !NMapFile::Read( "zig-out\\local-test\\not-a-map.bzm", &map, &szError ), "a file that is not a map does not read" );
	Check( szError.find( "not a map" ) != std::string::npos, szError.empty() ? "and says why" : szError.c_str() );
	remove( "zig-out/local-test/not-a-map.bzm" );
}

static void TestWritesWhatItRead()
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), "read for write test" ) )
		return;
	const char *pszOut = "zig-out\\local-test\\coldwinter-roundtrip.bzm";
	Check( NMapFile::Write( pszOut, map, &szError ), szError.empty() ? "the map writes" : szError.c_str() );
	CMapInfo reread;
	szError.clear();
	Check( NMapFile::Read( pszOut, &reread, &szError ), szError.empty() ? "what was written reads" : szError.c_str() );
	Check( reread.objects.size() == map.objects.size(), "the same number of objects came back" );
	Check( reread.terrain.tiles.GetSizeX() == map.terrain.tiles.GetSizeX(), "the terrain is the same size" );
}

static void TestComparatorSeesADifference()
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), "read for comparator test" ) )
		return;
	CMapInfo same = map;
	std::string szWhere;
	Check( NMapFile::AreEquivalent( map, same, &szWhere ), "a copy is equivalent" );

	CMapInfo moved = map;
	if ( !moved.objects.empty() )
	{
		moved.objects[0].vPos.x += 1.0f;
		szWhere.clear();
		Check( !NMapFile::AreEquivalent( map, moved, &szWhere ), "a moved object is not equivalent" );
		Check( szWhere.find( "objects[0].vPos" ) != std::string::npos, "and the comparator names the field" );
	}

	CMapInfo repainted = map;
	if ( repainted.terrain.tiles.GetSizeX() > 0 )
	{
		repainted.terrain.tiles[0][0].tile = BYTE( repainted.terrain.tiles[0][0].tile + 1 );
		szWhere.clear();
		Check( !NMapFile::AreEquivalent( map, repainted, &szWhere ), "a changed tile is not equivalent" );
		Check( szWhere.find( "terrain.tiles" ) != std::string::npos, "and it names the tile" );
	}
}

// What the writer actually preserves, named. The round trip must be
// equivalent; it need not be byte-identical with a file some older tool
// wrote, which is why the spec asks for equivalence plus an idempotent save
// rather than for the original's bytes.
static void TestRoundTripIsEquivalent()
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &original, &szError ), "read for round trip" ) )
		return;
	const char *pszOut = "zig-out\\local-test\\coldwinter-roundtrip.bzm";
	if ( !Check( NMapFile::Write( pszOut, original, &szError ), szError.c_str() ) )
		return;
	CMapInfo reread;
	szError.clear();
	if ( !Check( NMapFile::Read( pszOut, &reread, &szError ), szError.c_str() ) )
		return;
	std::string szWhere;
	Check( NMapFile::AreEquivalent( original, reread, &szWhere ),
	       szWhere.empty() ? "the round trip is equivalent" : ( "round trip differs at " + szWhere ).c_str() );
}

static void TestObjectOverlay()
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
		return;
	const CMapInfo original = map;

	// An added object lands at the end of its list with a fresh link ID.
	NMapOverlay::SAddObject add;
	add.szName = original.objects.empty() ? std::string( "Unknown_Test_Object" ) : original.objects[0].szName;
	add.vPos = CVec3( 100.0f, 100.0f, 0.0f );
	add.nDir = 0;
	add.nPlayer = 0;
	add.bScenario = false;
	const int nExpectedLinkID = NMapOverlay::NextLinkID( original );
	int nNewLinkID = -1;
	Check( NMapOverlay::AddObject( &map, add, &nNewLinkID ), "an object is added" );
	Check( map.objects.size() == original.objects.size() + 1, "at the end of the list" );
	Check( nNewLinkID == nExpectedLinkID, "with a link ID above every one in use" );
	if ( !map.objects.empty() )
	{
		const SMapObjectInfo &rAdded = map.objects.back();
		Check( rAdded.link.nLinkID == nNewLinkID, "and the record carries it" );
		Check( rAdded.nFrameIndex == 0, "and an unpacked frame index, for the bridge to pack" );
	}

	// A moved object keeps its record and changes three fields.
	if ( !original.objects.empty() )
	{
		NMapOverlay::SMoveObject move;
		move.nLinkID = original.objects[0].link.nLinkID;
		move.vPos = CVec3( original.objects[0].vPos.x + 32.0f, original.objects[0].vPos.y, original.objects[0].vPos.z );
		move.nDir = original.objects[0].nDir + 1024;
		move.nPlayer = original.objects[0].nPlayer;
		Check( NMapOverlay::MoveObject( &map, move ), "an object moves" );
		Check( map.objects[0].vPos.x == move.vPos.x, "to where it was put" );
		Check( map.objects[0].nDir == move.nDir, "and turns" );
		Check( map.objects[0].nFrameIndex == original.objects[0].nFrameIndex, "and keeps its packed frame index" );
		Check( map.objects[0].fHP == original.objects[0].fHP, "and its HP" );
		Check( map.objects[0].nScriptID == original.objects[0].nScriptID, "and its script ID" );
		Check( map.objects[0].szName == original.objects[0].szName, "and its name" );
	}

	// An object a start command or reserve position names cascades (D-04) and
	// its restore gives the map back. Coldwinter names none of its objects that
	// way, so the reference is laid over a copy through NMapRecords.
	CMapInfo forDelete = original;
	int nNamed = -1;
	for ( size_t i = 0; i < forDelete.objects.size() && nNamed < 0; ++i )
	{
		const int nCandidate = forDelete.objects[i].link.nLinkID;
		std::vector<std::string> references;
		NMapOverlay::FindReferences( forDelete, nCandidate, &references );
		if ( nCandidate != 0 && references.empty() )
			nNamed = nCandidate;
	}
	if ( Check( nNamed >= 0, "coldwinter has an object nothing refers to" ) )
	{
		SAIStartCommand command;
		command.unitLinkIDs.push_back( nNamed );
		NMapRecords::InsertStartCommand( &forDelete, -1, command );
		const CMapInfo before = forDelete;
		NMapOverlay::SDeletedObject deleted;
		std::string szRefusal;
		Check( NMapOverlay::DeleteObject( &forDelete, nNamed, &szRefusal, &deleted ), "an object a start command names goes, the command with it" );
		Check( forDelete.startCommandsList.size() + 1 == before.startCommandsList.size(), "and the command left with no unit is erased" );
		Check( NMapOverlay::RestoreObject( &forDelete, deleted ), "and it restores" );
		std::string szWhere;
		Check( NMapFile::AreEquivalent( before, forDelete, &szWhere ), "with the start command back as it was" );
	}

	// A bridge span still refuses to be deleted, says what holds it, and changes
	// nothing.
	if ( nNamed >= 0 )
	{
		CMapInfo withBridge = original;
		std::vector<int> span;
		span.push_back( nNamed );
		NMapRecords::InsertBridgeEntry( &withBridge, -1, span );
		const CMapInfo before = withBridge;
		std::string szRefusal;
		Check( !NMapOverlay::DeleteObject( &withBridge, span[0], &szRefusal ), "a bridge span refuses to go" );
		Check( szRefusal.find( "still referred to by bridge" ) != std::string::npos, "and names the bridge" );
		std::string szWhere;
		Check( NMapFile::AreEquivalent( before, withBridge, &szWhere ), "and a refused delete changes nothing" );
	}
}

// A delete nothing refers to takes the record out and leaves every other
// record alone - including the link IDs, which are never renumbered.
static void TestDeleteWithNoReferences()
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
		return;
	int nFree = -1;
	size_t nIndex = 0;
	for ( size_t i = 0; i < map.objects.size() && nFree < 0; ++i )
	{
		std::vector<std::string> references;
		NMapOverlay::FindReferences( map, map.objects[i].link.nLinkID, &references );
		if ( references.empty() )
		{
			nFree = map.objects[i].link.nLinkID;
			nIndex = i;
		}
	}
	if ( !Check( nFree >= 0, "the map has an object nothing refers to" ) )
		return;
	const size_t nBefore = map.objects.size();
	const int nNeighbourLinkID = map.objects[nIndex + 1 < nBefore ? nIndex + 1 : 0].link.nLinkID;
	std::string szRefusal;
	Check( NMapOverlay::DeleteObject( &map, nFree, &szRefusal ), "an unreferenced object deletes" );
	Check( map.objects.size() == nBefore - 1, "and the list is one shorter" );
	bool bGone = true, bNeighbourKept = false;
	for ( size_t i = 0; i < map.objects.size(); ++i )
	{
		bGone = bGone && map.objects[i].link.nLinkID != nFree;
		bNeighbourKept = bNeighbourKept || map.objects[i].link.nLinkID == nNeighbourLinkID;
	}
	Check( bGone, "the deleted link ID is gone" );
	Check( bNeighbourKept, "and nothing else was renumbered" );
}

// Diplomacy is a byte per player, and changing one changes exactly one.
static void TestDiplomacyChange()
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( map.diplomacies.size() >= 2, "the map has at least two players" ) )
		return;
	const std::vector<BYTE> before = map.diplomacies;
	const BYTE nNew = BYTE( before[1] == 0 ? 1 : 0 );
	Check( NMapOverlay::SetDiplomacy( &map, 1, nNew ), "diplomacy changes" );
	Check( map.diplomacies[1] == nNew, "for the player asked for" );
	Check( map.diplomacies[0] == before[0], "and for no one else" );
	Check( !NMapOverlay::SetDiplomacy( &map, int( before.size() ) + 5, 0 ),
	       "a player that does not exist is refused" );
}

// An object whose type the database does not know is still an object: it goes
// out exactly as it came in. This is the check that guards the frame-index
// trap in the spec's "Frame indices and unknown types".
static void TestUnknownObjectSurvives()
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
		return;
	if ( map.objects.empty() )
		return;
	map.objects[0].szName = "No_Such_Object_In_Any_Database";
	map.objects[0].nFrameIndex = 12345;
	const CMapInfo expected = map;
	Check( NMapFile::Write( "zig-out\\local-test\\unknown-object.bzm", map, &szError ), szError.c_str() );
	CMapInfo reread;
	szError.clear();
	if ( !Check( NMapFile::Read( "zig-out\\local-test\\unknown-object.bzm", &reread, &szError ), szError.c_str() ) )
		return;
	std::string szWhere;
	Check( NMapFile::AreEquivalent( expected, reread, &szWhere ),
	       szWhere.empty() ? "the unknown object survived" : ( "unknown object changed at " + szWhere ).c_str() );
}

// A paint that fails has to leave the map exactly as it was. Paint writes the
// cells before it loads the tileset it needs, so every way out after that has
// to put the region back - a caller that took "false" at its word and saved
// would otherwise write a paint that never happened.
//
// Naming a tileset that is not there is the arrangeable version of that: the
// descriptor comes back empty, which Paint refuses because
// CTerrainBuilder::ComparePriority would index it without checking.
static void TestFailedPaintChangesNothing()
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
		return;
	map.terrain.szTilesetDesc = "terrain\\sets\\no_such_tileset_anywhere";
	const CMapInfo before = map;

	std::vector<NMapOverlay::SPaintCell> cells;
	NMapOverlay::SPaintCell cell;
	cell.nX = 20;
	cell.nY = 20;
	cell.noise = map.terrain.tiles[20][20].noise;
	cell.tile = BYTE( map.terrain.tiles[20][20].tile + 1 );
	cells.push_back( cell );

	NMapOverlay::SPaintUndo undo;
	Check( !NMapOverlay::Paint( &map, cells, &undo ), "a paint with no tileset is refused" );
	std::string szWhere;
	Check( NMapFile::AreEquivalent( before, map, &szWhere ),
	       szWhere.empty() ? "and the map is untouched" : ( "a refused paint changed " + szWhere ).c_str() );
	Check( undo.tiles.empty() && undo.patches.empty(), "and left nothing to undo" );
}

// A deleted object comes back exactly: same record, same list, same place in it.
static void TestDeleteThenRestoreIsTheOriginal( const char *pszMap )
{
	CMapInfo map, original;
	std::string szError;
	if ( !Check( NMapFile::Read( pszMap, &map, &szError ), szError.c_str() ) )
		return;
	original = map;
	// The first object nothing refers to.
	int nLinkID = -1;
	for ( size_t i = 0; i < map.objects.size() && nLinkID < 0; ++i )
	{
		std::vector<std::string> references;
		NMapOverlay::FindReferences( map, map.objects[i].link.nLinkID, &references );
		if ( references.empty() )
			nLinkID = map.objects[i].link.nLinkID;
	}
	if ( !Check( nLinkID >= 0, "the map has an unreferenced object" ) )
		return;
	NMapOverlay::SDeletedObject deleted;
	std::string szRefusal;
	Check( NMapOverlay::DeleteObject( &map, nLinkID, &szRefusal, &deleted ), szRefusal.c_str() );
	Check( deleted.object.link.nLinkID == nLinkID, "the delete hands back the record" );
	Check( NMapOverlay::RestoreObject( &map, deleted ), "the record goes back" );
	std::string szWhere;
	Check( NMapFile::AreEquivalent( original, map, &szWhere ), ( "and the map is the original: " + szWhere ).c_str() );
	Check( !NMapOverlay::RestoreObject( &map, deleted ), "a second restore is refused: the link ID is in use" );
}

// An add can be told its link ID, and is refused one already taken.
static void TestAddTakesAGivenLinkID( const char *pszMap )
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( pszMap, &map, &szError ), szError.c_str() ) )
		return;
	if ( !Check( !map.objects.empty(), "the map has objects" ) )
		return;
	NMapOverlay::SAddObject add;
	add.szName = map.objects[0].szName;
	add.nLinkID = NMapOverlay::NextLinkID( map ) + 10;
	int nLinkID = -1;
	Check( NMapOverlay::AddObject( &map, add, &nLinkID ) && nLinkID == add.nLinkID, "an add takes the link ID it is given" );
	add.nLinkID = map.objects[0].link.nLinkID;
	Check( !NMapOverlay::AddObject( &map, add, &nLinkID ), "and refuses one in use" );
}

// CaptureRegion reads what UndoPaint writes, so capture-paint-restore is the identity.
static void TestCaptureRestoresAPaint( const char *pszMap )
{
	CMapInfo map, original;
	std::string szError;
	if ( !Check( NMapFile::Read( pszMap, &map, &szError ), szError.c_str() ) )
		return;
	original = map;
	std::vector<NMapOverlay::SPaintCell> cells( 1 );
	cells[0].nX = 5;
	cells[0].nY = 5;
	cells[0].tile = BYTE( ( map.terrain.tiles[5][5].tile + 1 ) % 4 );
	NMapOverlay::SPaintUndo undo, after;
	if ( !Check( NMapOverlay::Paint( &map, cells, &undo ), "the paint runs" ) )
		return;
	NMapOverlay::CaptureRegion( map, undo.rPatches, &after );
	NMapOverlay::UndoPaint( &map, undo );
	std::string szWhere;
	Check( NMapFile::AreEquivalent( original, map, &szWhere ), ( "undo is the original: " + szWhere ).c_str() );
	NMapOverlay::UndoPaint( &map, after );
	NMapOverlay::SPaintUndo again;
	NMapOverlay::CaptureRegion( map, undo.rPatches, &again );
	Check( !after.tiles.empty() && again.tiles.size() == after.tiles.size() &&
	       memcmp( &again.tiles[0], &after.tiles[0], after.tiles.size() * sizeof after.tiles[0] ) == 0,
	       "restoring the capture is the painted state" );
}

// The spec's terrain cases: inside one patch, across a patch border, undo
// putting the region back exactly, and the preprocessing pass changing tiles
// that were never painted.
static void TestPaint()
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
		return;
	const CMapInfo original = map;
	if ( !Check( original.terrain.tiles.GetSizeX() > 64, "the map is big enough to paint in" ) )
		return;

	std::vector<NMapOverlay::SPaintCell> cells;
	NMapOverlay::SPaintCell cell;
	cell.nX = 20; cell.nY = 20;
	cell.noise = original.terrain.tiles[20][20].noise;
	cell.tile = BYTE( original.terrain.tiles[20][20].tile + 1 );
	cells.push_back( cell );

	NMapOverlay::SPaintUndo undo;
	if ( !Check( NMapOverlay::Paint( &map, cells, &undo ),
	             "one cell paints (needs Data\\Terrain\\sets\\*\\tileset.xml)" ) )
		return;
	Check( map.terrain.tiles[20][20].tile == cell.tile, "the painted cell has the new tile" );

	// The function must never touch these, anywhere.
	Check( NMapFile::CompareAltitudeArrays( map.terrain, original.terrain ), "altitudes are untouched" );
	std::string szWhere;
	Check( map.terrain.rivers.size() == original.terrain.rivers.size(), "rivers are untouched" );
	Check( map.terrain.roads3.size() == original.terrain.roads3.size(), "roads are untouched" );
	Check( map.terrain.szTilesetDesc == original.terrain.szTilesetDesc, "the tileset is untouched" );

	// Undo restores the region exactly - the whole map compares equal again.
	NMapOverlay::UndoPaint( &map, undo );
	szWhere.clear();
	Check( NMapFile::AreEquivalent( original, map, &szWhere ),
	       szWhere.empty() ? "undo put the region back" : ( "undo left " + szWhere ).c_str() );
}

// A cell on a patch border pulls in the neighbouring patch, whose crosses read
// across the border. Patches are 16x16 (fmtMap.cpp:6-7).
static void TestPaintOnAPatchBorder()
{
	CMapInfo map;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &map, &szError ), szError.c_str() ) )
		return;
	std::vector<NMapOverlay::SPaintCell> cells;
	NMapOverlay::SPaintCell cell;
	cell.nX = STerrainPatchInfo::nSizeX - 1;   // last column of patch 0
	cell.nY = 4;
	cell.noise = map.terrain.tiles[cell.nY][cell.nX].noise;
	cell.tile = BYTE( map.terrain.tiles[cell.nY][cell.nX].tile + 1 );
	cells.push_back( cell );
	const CTRect<int> r = NMapOverlay::AffectedPatches( map.terrain, cells );
	Check( r.minx == 0, "the region starts at the painted cell's patch" );
	Check( r.maxx >= 2, "and a border cell pulls in the next patch" );

	// A cell in the middle of a patch does not.
	std::vector<NMapOverlay::SPaintCell> middle;
	NMapOverlay::SPaintCell inner;
	inner.nX = 8; inner.nY = 8;
	inner.noise = map.terrain.tiles[8][8].noise;
	inner.tile = BYTE( map.terrain.tiles[8][8].tile + 1 );
	middle.push_back( inner );
	const CTRect<int> rInner = NMapOverlay::AffectedPatches( map.terrain, middle );
	Check( rInner.maxx - rInner.minx == 1 && rInner.maxy - rInner.miny == 1,
	       "a cell well inside a patch affects that patch alone" );
}

// The preprocessing pass removes one-cell-thin strips of lower-priority
// terrain, so it can change tiles inside the region that were never painted.
// That is the engine's behaviour and the saved map has to match it, so this
// asserts it happens rather than tolerating it. Painting one cell with a
// neighbour's tile is what tends to leave such a strip, so the search walks
// cells and tries each neighbouring tile value until one does.
static void TestPreprocessingChangesUnpaintedTiles()
{
	CMapInfo clean;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &clean, &szError ), szError.c_str() ) )
		return;
	int nFoundX = -1, nFoundY = -1, nChangedOutside = 0;
	for ( int y = 20; y < 120 && nFoundX < 0; y += 3 )
	{
		for ( int x = 20; x < 120 && nFoundX < 0; x += 3 )
		{
			const BYTE nHere = clean.terrain.tiles[y][x].tile;
			const BYTE nThere = clean.terrain.tiles[y][x + 2].tile;
			if ( nHere == nThere )
				continue;
			CMapInfo map = clean;
			std::vector<NMapOverlay::SPaintCell> cells;
			NMapOverlay::SPaintCell cell;
			cell.nX = x; cell.nY = y;
			cell.tile = nThere;
			cell.noise = clean.terrain.tiles[y][x].noise;
			cells.push_back( cell );
			NMapOverlay::SPaintUndo undo;
			if ( !NMapOverlay::Paint( &map, cells, &undo ) )
				continue;
			const CTRect<int> r = undo.rPatches;
			int nOutside = 0;
			for ( int ty = r.miny * STerrainPatchInfo::nSizeY; ty < r.maxy * STerrainPatchInfo::nSizeY; ++ty )
				for ( int tx = r.minx * STerrainPatchInfo::nSizeX; tx < r.maxx * STerrainPatchInfo::nSizeX; ++tx )
				{
					if ( tx == x && ty == y )
						continue;
					if ( map.terrain.tiles[ty][tx].tile != clean.terrain.tiles[ty][tx].tile )
						++nOutside;
				}
			if ( nOutside > 0 )
			{
				nFoundX = x; nFoundY = y; nChangedOutside = nOutside;
				// And undo still restores all of it, strip and all.
				NMapOverlay::UndoPaint( &map, undo );
				std::string szWhere;
				Check( NMapFile::AreEquivalent( clean, map, &szWhere ),
				       szWhere.empty() ? "undo restores the preprocessed tiles too"
				                       : ( "undo left " + szWhere ).c_str() );
			}
		}
	}
	if ( Check( nFoundX >= 0, "a paint exists that the preprocessing pass widens" ) )
		printf( "map-file: preprocessing changed %d unpainted tiles around %d,%d\n",
		        nChangedOutside, nFoundX, nFoundY );
}

static const char *Extension( const std::string &szPath )
{
	return szPath.size() >= 4 && NStr::CompareAsciiNoCase( szPath.c_str() + szPath.size() - 4, ".xml" ) == 0 ? ".xml" : ".bzm";
}

static bool FilesAreIdentical( const char *pszLeft, const char *pszRight )
{
	CPtr<IDataStream> pL = OpenFileStream( pszLeft, STREAM_ACCESS_READ );
	CPtr<IDataStream> pR = OpenFileStream( pszRight, STREAM_ACCESS_READ );
	if ( pL == 0 || pR == 0 )
		return false;
	if ( pL->GetSize() != pR->GetSize() )
		return false;
	// Read each side whole and compare once. Reading in blocks and comparing
	// block by block looks tidier and is wrong: IDataStream::Read may return
	// fewer bytes than asked for without being at the end, and two streams over
	// two files need not break at the same offsets - which made every map in
	// the sweep look like it saved differently twice when the files were in
	// fact identical. The largest shipped map is 1.6 MB.
	const int nSize = pL->GetSize();
	std::vector<char> left( nSize > 0 ? nSize : 1 ), right( nSize > 0 ? nSize : 1 );
	int nReadLeft = 0, nReadRight = 0;
	while ( nReadLeft < nSize )
	{
		const int n = pL->Read( &(left[nReadLeft]), nSize - nReadLeft );
		if ( n <= 0 ) break;
		nReadLeft += n;
	}
	while ( nReadRight < nSize )
	{
		const int n = pR->Read( &(right[nReadRight]), nSize - nReadRight );
		if ( n <= 0 ) break;
		nReadRight += n;
	}
	if ( nReadLeft != nSize || nReadRight != nSize )
	{
		printf( "  (identical? sizes L=%d R=%d, read L=%d R=%d)\n", nSize, pR->GetSize(), nReadLeft, nReadRight );
		return false;
	}
	if ( nSize == 0 || memcmp( &(left[0]), &(right[0]), nSize ) == 0 )
		return true;
	for ( int i = 0; i < nSize; ++i )
		if ( left[i] != right[i] )
		{
			printf( "  (identical? size %d, first difference at %d: %02x vs %02x)\n",
			             nSize, i, (unsigned char)left[i], (unsigned char)right[i] );
			break;
		}
	return false;
}

// D-01/D-22 at the map-file tier: setting player 5's camera anchor pads the
// vector (never shrinks it), the saved map reads back equal to the map the same
// call builds, and putting the original vector back writes the unedited file
// byte for byte.
static void TestCameraAnchorRecords()
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &original, &szError ), "read for the camera anchor test" ) )
		return;
	NMapRecords::SCameraAnchors before;
	NMapRecords::GetCameraAnchors( original, &before );
	printf( "map-file: coldwinter has %d camera anchor slots, neutral %s\n",
	        int( before.players.size() ), before.vNeutral == VNULL3 ? "unset" : "set" );
	const CVec3 vAnchor( 100.0f, 120.0f, 4.0f );

	// The expected map: the same calls on a second copy.
	CMapInfo expected = original;
	NMapRecords::SCameraAnchors wanted = before;
	Check( NMapRecords::SetPlayerCameraAnchor( &wanted, 5, vAnchor ), "the anchor is set on the value" );
	Check( NMapRecords::PutCameraAnchors( &expected, wanted ), "the expected map takes it" );
	Check( int( expected.playersCameraAnchors.size() ) == Max( int( before.players.size() ), 6 ),
	       "the vector is padded to player 5 + 1 and never shrunk" );
	Check( expected.playersCameraAnchors[5] == vAnchor, "and holds the anchor in its slot" );
	for ( size_t i = before.players.size(); i < 5; ++i )
		Check( expected.playersCameraAnchors[i] == VNULL3, "a padded slot is unset" );

	// The bytes are compared between maps READ from the file, never between a
	// map and a copy of it: SVertexAltitude is written as a raw struct, its
	// three padding bytes included, and a copied map holds whatever the copy
	// left there. That is a property of the test's copies, not of an edit.
	CMapInfo edited;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &edited, &szError ), "read the map to edit" ) )
		return;
	NMapRecords::SCameraAnchors again;
	NMapRecords::GetCameraAnchors( edited, &again );
	NMapRecords::SetPlayerCameraAnchor( &again, 5, vAnchor );
	NMapRecords::PutCameraAnchors( &edited, again );
	const char *pszUnedited = "zig-out\\local-test\\anchors-unedited.bzm";
	const char *pszEdited = "zig-out\\local-test\\anchors-edited.bzm";
	const char *pszUndone = "zig-out\\local-test\\anchors-undone.bzm";
	Check( NMapFile::Write( pszEdited, edited, &szError ), szError.c_str() );
	CMapInfo reread;
	szError.clear();
	if ( Check( NMapFile::Read( pszEdited, &reread, &szError ), szError.c_str() ) )
	{
		std::string szWhere;
		Check( NMapFile::AreEquivalent( expected, reread, &szWhere ),
		       szWhere.empty() ? "the saved map equals the expected map" : ( "the anchor edit differs at " + szWhere ).c_str() );
		szWhere.clear();
		Check( !NMapFile::AreEquivalent( original, reread, &szWhere ) && szWhere.find( "playersCameraAnchors" ) != std::string::npos,
		       "and the comparator sees the anchor edit" );
	}

	// The inverse: the original vector back, byte for byte.
	NMapRecords::PutCameraAnchors( &edited, before );
	Check( NMapFile::Write( pszUndone, edited, &szError ), szError.c_str() );
	CMapInfo unedited;
	if ( !Check( NMapFile::Read( "Data\\Maps\\Multiplayer\\coldwinter.bzm", &unedited, &szError ), "read the map to write unedited" ) )
		return;
	Check( NMapFile::Write( pszUnedited, unedited, &szError ), szError.c_str() );
	const bool bIdentical = Check( FilesAreIdentical( pszUnedited, pszUndone ), "an anchor edit and its inverse write the unedited file byte for byte" );

	// Clearing is in place: no shrink.
	NMapRecords::SCameraAnchors cleared = wanted;
	Check( NMapRecords::ClearPlayerCameraAnchor( &cleared, 5 ), "an anchor clears" );
	Check( cleared.players.size() == wanted.players.size() && cleared.players[5] == VNULL3, "clearing keeps the size" );
	Check( NMapRecords::ClearPlayerCameraAnchor( &cleared, 50 ), "clearing a player past the end is already done" );
	Check( cleared.players.size() == wanted.players.size(), "and does not grow the vector" );
	Check( !NMapRecords::SetPlayerCameraAnchor( &cleared, -1, vAnchor ), "a negative player is refused" );
	Check( !NMapRecords::SetPlayerCameraAnchor( &cleared, NMapRecords::nMaxCameraAnchorPlayers, vAnchor ), "an absurd player is refused" );
	Check( cleared.players.size() == wanted.players.size(), "a refusal changes nothing" );
	// Kept when the bytes differ, for whoever has to look at them.
	if ( bIdentical )
	{
		remove( "zig-out/local-test/anchors-unedited.bzm" );
		remove( "zig-out/local-test/anchors-edited.bzm" );
		remove( "zig-out/local-test/anchors-undone.bzm" );
	}
	printf( "map-file: M2 camera anchor records ok\n" );
}

// Walks a directory through a storage of its own, opened on the folder and
// nothing else. The registered data storage will not do: it mounts the .pak
// archives as pseudo-directories, so enumerating "Maps\\*.*" through it
// descends into the whole object database - 21,978 entries, none of them maps.
// A plain file storage mounts nothing, so what it lists is what is on disk.
//
// std::filesystem would be the obvious tool and is not usable here: including
// <filesystem> in a translation unit built with the engine's cppflags puts
// libstdc++'s headers next to the engine's PortableCrt arrangement, and on
// Linux the two collide (std_abs.h: "declaration conflicts with target of
// using declaration already in scope").
static void CollectMapsIn( IDataStorage *pStorage, const std::string &szPrefix,
                           const std::string &szRoot, bool bTopLevelXmlToo,
                           std::vector<std::string> *pPaths )
{
	CPtr<IStorageEnumerator> pEnum = pStorage->CreateEnumerator();
	if ( pEnum == 0 )
		return;
	std::vector<std::string> subfolders;
	for ( pEnum->Reset( ( szPrefix + "*.*" ).c_str() ); pEnum->Next(); )
	{
		const SStorageElementStats *pStats = pEnum->GetStats();
		if ( pStats == 0 || pStats->pszName == 0 )
			continue;
		const std::string szName = pStats->pszName;
		if ( szName == "." || szName == ".." )
			continue;
		if ( pStats->type == SET_STORAGE )
		{
			subfolders.push_back( szPrefix + szName + "\\" );
			continue;
		}
		if ( szName.size() <= 4 )
			continue;
		const char *pszExt = szName.c_str() + szName.size() - 4;
		// .bzm is always a map. .xml is not: under Data\Scenarios it is also
		// settings, chapter and context files, which are not maps and say so by
		// failing IsValid. The only .xml maps shipped are the two at the top of
		// Data\Maps.
		const bool bBzm = NStr::CompareAsciiNoCase( pszExt, ".bzm" ) == 0;
		const bool bXml = NStr::CompareAsciiNoCase( pszExt, ".xml" ) == 0 &&
		                  bTopLevelXmlToo && szPrefix.empty();
		if ( bBzm || bXml )
			pPaths->push_back( szRoot + szPrefix + szName );
	}
	// Recurse after the enumerator is done with this level: one enumerator at a
	// time (StaticObjectsIters.h has the same rule for its own iterators).
	pEnum = 0;
	for ( size_t i = 0; i < subfolders.size(); ++i )
		CollectMapsIn( pStorage, subfolders[i], szRoot, bTopLevelXmlToo, pPaths );
}

static void CollectMaps( const char *pszFolder, bool bTopLevelXmlToo, std::vector<std::string> *pPaths )
{
	const std::string szRoot = std::string( pszFolder ) + "\\";
	CPtr<IDataStorage> pStorage = OpenStorage( szRoot.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	if ( pStorage == 0 )
		return;
	CollectMapsIn( pStorage, std::string(), szRoot, bTopLevelXmlToo, pPaths );
}

// ---------------------------------------------------------------------------
// D-01 at the map-file tier: every M2 collection has a record-level overlay
// operation, and an edit followed by its inverse writes the unedited file byte
// for byte. Each case reads the map FRESH for every write it compares (never a
// copy: SVertexAltitude's padding bytes are written raw, see
// TestCameraAnchorRecords), applies an optional setup, and then
//   - forward: the edited map, written and read back, equals the map the same
//     call builds on another fresh read (and differs from the unedited one);
//   - inverse: forward then inverse writes exactly the setup-only bytes.
// ---------------------------------------------------------------------------
typedef std::function<bool( SLoadMapInfo* )> TMapOp;

static int g_nM2Cases = 0;

static const char *const M2_BASELINE = "zig-out\\local-test\\m2-baseline.bzm";
static const char *const M2_EDITED = "zig-out\\local-test\\m2-edited.bzm";
static const char *const M2_UNDONE = "zig-out\\local-test\\m2-undone.bzm";

static bool ReadFresh( const std::string &szPath, CMapInfo *pMap )
{
	std::string szError;
	return Check( NMapFile::Read( szPath.c_str(), pMap, &szError ), szError.empty() ? szPath.c_str() : szError.c_str() );
}

static bool ApplyOp( const TMapOp &rOp, CMapInfo *pMap )
{
	return !rOp || rOp( pMap );
}

static void RemoveM2Files()
{
	remove( "zig-out/local-test/m2-baseline.bzm" );
	remove( "zig-out/local-test/m2-edited.bzm" );
	remove( "zig-out/local-test/m2-undone.bzm" );
}

// One collection operation, forwards and back. `setup` may be empty.
static void RunM2Case( const std::string &szPath, const char *pszName, const TMapOp &setup, const TMapOp &forward, const TMapOp &inverse )
{
	++g_nM2Cases;
	const std::string szName = std::string( "M2 case \"" ) + pszName + "\" (" + szPath + "): ";
	std::string szError;

	CMapInfo unedited;
	if ( !ReadFresh( szPath, &unedited ) )
		return;
	if ( !Check( ApplyOp( setup, &unedited ), ( szName + "the setup applies" ).c_str() ) )
		return;
	if ( !Check( NMapFile::Write( M2_BASELINE, unedited, &szError ), ( szName + "the unedited map writes: " + szError ).c_str() ) )
		return;

	// The edit, saved and read back, is the map the same call builds.
	CMapInfo edited;
	if ( !ReadFresh( szPath, &edited ) )
		return;
	ApplyOp( setup, &edited );
	if ( !Check( forward( &edited ), ( szName + "the forward operation is accepted" ).c_str() ) )
		return;
	if ( !Check( NMapFile::Write( M2_EDITED, edited, &szError ), ( szName + "the edited map writes: " + szError ).c_str() ) )
		return;
	CMapInfo reread;
	if ( !Check( NMapFile::Read( M2_EDITED, &reread, &szError ), ( szName + "the edited map reads back: " + szError ).c_str() ) )
		return;
	CMapInfo expected;
	if ( !ReadFresh( szPath, &expected ) )
		return;
	ApplyOp( setup, &expected );
	forward( &expected );
	std::string szWhere;
	Check( NMapFile::AreEquivalent( expected, reread, &szWhere ),
	       ( szName + "the saved edit differs from the expected map at " + szWhere ).c_str() );
	szWhere.clear();
	Check( !NMapFile::AreEquivalent( unedited, reread, &szWhere ), ( szName + "the edit changed nothing the comparator sees" ).c_str() );

	// Forward, then the inverse: the unedited bytes.
	CMapInfo undone;
	if ( !ReadFresh( szPath, &undone ) )
		return;
	ApplyOp( setup, &undone );
	forward( &undone );
	if ( !Check( inverse( &undone ), ( szName + "the inverse is accepted" ).c_str() ) )
		return;
	if ( !Check( NMapFile::Write( M2_UNDONE, undone, &szError ), ( szName + "the undone map writes: " + szError ).c_str() ) )
		return;
	Check( FilesAreIdentical( M2_BASELINE, M2_UNDONE ), ( szName + "an edit and its inverse write the unedited file byte for byte" ).c_str() );
}

// An operation with a bad index or value is refused and the file's bytes do not
// move.
static void RunM2Refusal( const std::string &szPath, const char *pszName, const TMapOp &setup, const TMapOp &badOp )
{
	++g_nM2Cases;
	const std::string szName = std::string( "M2 refusal \"" ) + pszName + "\" (" + szPath + "): ";
	std::string szError;
	CMapInfo unedited;
	if ( !ReadFresh( szPath, &unedited ) )
		return;
	ApplyOp( setup, &unedited );
	if ( !Check( NMapFile::Write( M2_BASELINE, unedited, &szError ), ( szName + "the unedited map writes" ).c_str() ) )
		return;
	CMapInfo touched;
	if ( !ReadFresh( szPath, &touched ) )
		return;
	ApplyOp( setup, &touched );
	Check( !badOp( &touched ), ( szName + "is refused" ).c_str() );
	if ( !Check( NMapFile::Write( M2_UNDONE, touched, &szError ), ( szName + "the map writes" ).c_str() ) )
		return;
	Check( FilesAreIdentical( M2_BASELINE, M2_UNDONE ), ( szName + "and the map is byte for byte unchanged" ).c_str() );
}

// The first shipped map with each collection non-empty, found in one pass over
// Data\Maps (at most 60 maps read), so the erase and replace cases run on data
// a real map holds where one exists.
struct SM2Maps
{
	std::string szScriptFile, szScriptAreas, szGroups, szStartCommands, szReserve, szAISides, szRoads, szRivers, szBridges, szEntrenchments;
};

static SM2Maps FindM2Maps()
{
	SM2Maps found;
	std::vector<std::string> paths;
	CollectMaps( "Data\\Maps", true, &paths );
	int nRead = 0;
	for ( size_t i = 0; i < paths.size() && nRead < 60; ++i )
	{
		CMapInfo map;
		std::string szError;
		if ( !NMapFile::Read( paths[i].c_str(), &map, &szError ) )
			continue;
		++nRead;
		if ( found.szScriptFile.empty() && !map.szScriptFile.empty() ) found.szScriptFile = paths[i];
		if ( found.szScriptAreas.empty() && !map.scriptAreas.empty() ) found.szScriptAreas = paths[i];
		if ( found.szGroups.empty() && !map.reinforcements.groups.empty() ) found.szGroups = paths[i];
		if ( found.szStartCommands.empty() && !map.startCommandsList.empty() ) found.szStartCommands = paths[i];
		if ( found.szReserve.empty() && !map.reservePositionsList.empty() ) found.szReserve = paths[i];
		if ( found.szAISides.empty() && !map.aiGeneralMapInfo.sidesInfo.empty() ) found.szAISides = paths[i];
		if ( found.szRoads.empty() && !map.terrain.roads3.empty() ) found.szRoads = paths[i];
		if ( found.szRivers.empty() && !map.terrain.rivers.empty() ) found.szRivers = paths[i];
		if ( found.szBridges.empty() && !map.bridges.empty() ) found.szBridges = paths[i];
		if ( found.szEntrenchments.empty() && !map.entrenchments.empty() ) found.szEntrenchments = paths[i];
	}
	printf( "map-file: M2 data found in %d maps: script file %s, areas %s, groups %s, start commands %s, reserve %s, AI sides %s, roads %s, rivers %s, bridges %s, entrenchments %s\n",
	        nRead, found.szScriptFile.c_str(), found.szScriptAreas.c_str(), found.szGroups.c_str(), found.szStartCommands.c_str(),
	        found.szReserve.c_str(), found.szAISides.c_str(), found.szRoads.c_str(), found.szRivers.c_str(), found.szBridges.c_str(),
	        found.szEntrenchments.c_str() );
	return found;
}

static SScriptArea MakeArea( const char *pszName, float fX, float fY, float fR )
{
	SScriptArea area;
	area.eType = SScriptArea::EAT_CIRCLE;
	area.szName = pszName;
	area.center = CVec2( fX, fY );
	area.fR = fR;
	return area;
}

static SAIStartCommand MakeStartCommand( float fX, float fY, int nUnit )
{
	std::vector<int> units;
	units.push_back( nUnit );
	return SAIStartCommand( ACTION_COMMAND_MOVE_TO, units, 0, CVec2( fX, fY ), false, 0.0f );
}

static SVectorStripeObject MakeVso( int nID, float fX, float fY )
{
	SVectorStripeObject vso;
	vso.szDescName = "m2 test descriptor";
	vso.nID = nID;
	vso.controlpoints.push_back( CVec3( fX, fY, 0.0f ) );
	vso.controlpoints.push_back( CVec3( fX + 64.0f, fY + 32.0f, 0.0f ) );
	for ( size_t i = 0; i < vso.controlpoints.size(); ++i )
	{
		SVectorStripeObjectPoint point;
		point.vPos = vso.controlpoints[i];
		point.fWidth = 8.0f;
		point.bKeyPoint = true;
		vso.points.push_back( point );
	}
	return vso;
}

// An object of the map whose link ID is nonzero and unique, for the two object
// field edits. Returns 0 when there is none.
static int FindUsableLinkID( const SLoadMapInfo &rMap )
{
	std::map<int, int> counts;
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
		++counts[rMap.objects[i].link.nLinkID];
	for ( size_t i = 0; i < rMap.scenarioObjects.size(); ++i )
		++counts[rMap.scenarioObjects[i].link.nLinkID];
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
		if ( rMap.objects[i].link.nLinkID != 0 && counts[rMap.objects[i].link.nLinkID] == 1 )
			return rMap.objects[i].link.nLinkID;
	return 0;
}

static const SMapObjectInfo* ObjectByLinkID( const SLoadMapInfo &rMap, int nLinkID )
{
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
		if ( rMap.objects[i].link.nLinkID == nLinkID ) return &rMap.objects[i];
	for ( size_t i = 0; i < rMap.scenarioObjects.size(); ++i )
		if ( rMap.scenarioObjects[i].link.nLinkID == nLinkID ) return &rMap.scenarioObjects[i];
	return 0;
}

static void TestM2RecordOps()
{
	const std::string szCold = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	const std::string szArnheim = "Data\\Maps\\Multiplayer\\arnheim.bzm";
	const SM2Maps found = FindM2Maps();
	const TMapOp none;

	// ---- The script file: an exact put, the check a NEW name passes apart ----
	{
		std::string szOld;
		RunM2Case( szCold, "script file put",
		           none,
		           [&]( SLoadMapInfo *p ) { szOld = p->szScriptFile; return NMapRecords::PutScriptFile( p, "m2_script" ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::PutScriptFile( p, szOld ); } );
		if ( !found.szScriptFile.empty() )
			RunM2Case( found.szScriptFile, "script file cleared",
			           none,
			           [&]( SLoadMapInfo *p ) { szOld = p->szScriptFile; return NMapRecords::PutScriptFile( p, "" ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::PutScriptFile( p, szOld ); } );
		// An odd name a file held is kept verbatim and put back exactly.
		RunM2Case( szCold, "script file with an odd name kept",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::PutScriptFile( p, "..\\odd name.lua" ); },
		           [&]( SLoadMapInfo *p ) { szOld = p->szScriptFile; return NMapRecords::PutScriptFile( p, "m2_script" ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::PutScriptFile( p, szOld ); } );
		const char *pszGood[] = { "m2_script", "Script.v2", "" };
		for ( int i = 0; i < 3; ++i, ++g_nM2Cases )
			Check( NMapRecords::IsBareScriptName( pszGood[i] ), ( std::string( "IsBareScriptName accepts \"" ) + pszGood[i] + "\"" ).c_str() );
		const std::string bad[] = { "..", "a/b", "a\\b", "C:x", ".hidden", "x.lua", "X.LUA", "a..b", std::string( 64, 'a' ) };
		for ( int i = 0; i < 9; ++i, ++g_nM2Cases )
			Check( !NMapRecords::IsBareScriptName( bad[i] ), ( "IsBareScriptName refuses \"" + bad[i] + "\"" ).c_str() );
		Check( NMapRecords::IsBareScriptName( std::string( 63, 'a' ) ), "IsBareScriptName accepts 63 characters" );
		SLoadMapInfo *pNull = 0;
		Check( !NMapRecords::PutScriptFile( pNull, "x" ), "PutScriptFile refuses a null map" );
	}

	// ---- Script areas ----
	{
		const SScriptArea areaA = MakeArea( "M2 area A", 100.0f, 120.0f, 10.0f );
		const SScriptArea areaB = MakeArea( "M2 area B", 200.0f, 220.0f, 20.0f );
		int nAt = 0;
		SScriptArea saved;
		RunM2Case( szCold, "script area appended",
		           none,
		           [&]( SLoadMapInfo *p ) { nAt = int( p->scriptAreas.size() ); return NMapRecords::InsertScriptArea( p, -1, areaA ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseScriptArea( p, nAt ); } );
		RunM2Case( szCold, "script area inserted in front",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertScriptArea( p, -1, areaA ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertScriptArea( p, 0, areaB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseScriptArea( p, 0 ); } );
		RunM2Case( szCold, "script area replaced",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertScriptArea( p, -1, areaA ); },
		           [&]( SLoadMapInfo *p ) { const int n = int( p->scriptAreas.size() ) - 1; saved = p->scriptAreas[n]; return NMapRecords::ReplaceScriptArea( p, n, areaB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceScriptArea( p, int( p->scriptAreas.size() ) - 1, saved ); } );
		RunM2Case( szCold, "script area erased and put back at its index",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertScriptArea( p, -1, areaA ) && NMapRecords::InsertScriptArea( p, -1, areaB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseScriptArea( p, int( p->scriptAreas.size() ) - 2, &saved ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertScriptArea( p, int( p->scriptAreas.size() ) - 1, saved ); } );
		if ( !found.szScriptAreas.empty() )
			RunM2Case( found.szScriptAreas, "a shipped map's first script area erased and put back",
			           none,
			           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseScriptArea( p, 0, &saved ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertScriptArea( p, 0, saved ); } );
		RunM2Refusal( szCold, "script area replace past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceScriptArea( p, int( p->scriptAreas.size() ), areaA ); } );
		RunM2Refusal( szCold, "script area erase of a negative index", none, [&]( SLoadMapInfo *p ) { return NMapRecords::EraseScriptArea( p, -1 ); } );
		RunM2Refusal( szCold, "script area insert past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::InsertScriptArea( p, int( p->scriptAreas.size() ) + 1, areaA ); } );
		RunM2Refusal( szCold, "script area insert at -2", none, [&]( SLoadMapInfo *p ) { return NMapRecords::InsertScriptArea( p, -2, areaA ); } );

		CMapInfo map;
		if ( ReadFresh( szCold, &map ) )
		{
			NMapRecords::InsertScriptArea( &map, -1, areaA );
			NMapRecords::InsertScriptArea( &map, -1, areaB );
			g_nM2Cases += 4;
			Check( !NMapRecords::IsAreaNameFree( map, "M2 area A", -1 ), "a taken area name is not free" );
			Check( NMapRecords::IsAreaNameFree( map, "M2 area A", int( map.scriptAreas.size() ) - 2 ), "a name is free for the area that holds it (a rename)" );
			Check( NMapRecords::IsAreaNameFree( map, "m2 area a", -1 ), "area names are case-sensitive (Pitfall 14)" );
			Check( !NMapRecords::IsAreaNameFree( map, "", -1 ), "an empty area name is never free" );
		}
	}

	// ---- Reinforcement groups ----
	{
		std::vector<int> ids5, ids9;
		ids5.push_back( 1 ); ids5.push_back( 2 );
		ids9.push_back( 9 );
		std::vector<int> saved;
		int nFree = 0;
		RunM2Case( szCold, "group created",
		           none,
		           [&]( SLoadMapInfo *p ) { nFree = NMapRecords::FirstFreeGroupID( *p, 0 ); return NMapRecords::PutReinforcementGroup( p, nFree, ids5 ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseReinforcementGroup( p, nFree ); } );
		RunM2Case( szCold, "group ids replaced",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::PutReinforcementGroup( p, 5, ids5 ); },
		           [&]( SLoadMapInfo *p ) { saved = p->reinforcements.groups[5].ids; return NMapRecords::PutReinforcementGroup( p, 5, ids9 ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::PutReinforcementGroup( p, 5, saved ); } );
		RunM2Case( szCold, "group erased and put back",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::PutReinforcementGroup( p, 5, ids5 ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseReinforcementGroup( p, 5, &saved ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::PutReinforcementGroup( p, 5, saved ); } );
		if ( !found.szGroups.empty() )
			RunM2Case( found.szGroups, "a shipped map's first group erased and put back",
			           none,
			           [&]( SLoadMapInfo *p ) { nFree = p->reinforcements.groups.begin()->first; return NMapRecords::EraseReinforcementGroup( p, nFree, &saved ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::PutReinforcementGroup( p, nFree, saved ); } );
		RunM2Refusal( szCold, "group erase of one that is not there", none, [&]( SLoadMapInfo *p ) { return NMapRecords::EraseReinforcementGroup( p, 424242 ); } );
		RunM2Refusal( szCold, "group put with a negative ID", none, [&]( SLoadMapInfo *p ) { return NMapRecords::PutReinforcementGroup( p, -1, ids5 ); } );

		// Written sorted: the order the groups were put in is not in the file.
		CMapInfo first, second;
		if ( ReadFresh( szCold, &first ) && ReadFresh( szCold, &second ) )
		{
			std::vector<int> one( 1, 11 ), two( 1, 22 );
			NMapRecords::PutReinforcementGroup( &first, 7, one );
			NMapRecords::PutReinforcementGroup( &first, 3, two );
			NMapRecords::PutReinforcementGroup( &second, 3, two );
			NMapRecords::PutReinforcementGroup( &second, 7, one );
			std::string szError;
			NMapFile::Write( M2_EDITED, first, &szError );
			NMapFile::Write( M2_UNDONE, second, &szError );
			++g_nM2Cases;
			Check( FilesAreIdentical( M2_EDITED, M2_UNDONE ), "groups put as 7, 3 write the same bytes as 3, 7" );
		}
		CMapInfo map;
		if ( ReadFresh( szCold, &map ) )
		{
			g_nM2Cases += 3;
			NMapRecords::PutReinforcementGroup( &map, 0, ids5 );
			NMapRecords::PutReinforcementGroup( &map, 1, ids5 );
			NMapRecords::PutReinforcementGroup( &map, 3, ids5 );
			Check( NMapRecords::FirstFreeGroupID( map, 0 ) == 2, "FirstFreeGroupID skips the taken IDs from 0 up" );
			Check( NMapRecords::FirstFreeGroupID( map, 3 ) == 4, "and from 3 up" );
			Check( NMapRecords::FirstFreeGroupID( map, -5 ) == 2, "and clamps a negative start to 0" );
		}
	}

	// ---- Start commands (std::list) ----
	{
		const SAIStartCommand commandA = MakeStartCommand( 10.0f, 20.0f, 5 );
		const SAIStartCommand commandB = MakeStartCommand( 30.0f, 40.0f, 6 );
		SAIStartCommand saved;
		int nAt = 0;
		RunM2Case( szCold, "start command appended",
		           none,
		           [&]( SLoadMapInfo *p ) { nAt = int( p->startCommandsList.size() ); return NMapRecords::InsertStartCommand( p, -1, commandA ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseStartCommand( p, nAt ); } );
		RunM2Case( szCold, "start command inserted in front",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, -1, commandA ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, 0, commandB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseStartCommand( p, 0 ); } );
		RunM2Case( szCold, "start command replaced",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, -1, commandA ); },
		           [&]( SLoadMapInfo *p ) { saved = p->startCommandsList.back(); return NMapRecords::ReplaceStartCommand( p, int( p->startCommandsList.size() ) - 1, commandB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceStartCommand( p, int( p->startCommandsList.size() ) - 1, saved ); } );
		RunM2Case( szCold, "start command erased and put back at its index",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, -1, commandA ) && NMapRecords::InsertStartCommand( p, -1, commandB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseStartCommand( p, int( p->startCommandsList.size() ) - 2, &saved ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, int( p->startCommandsList.size() ) - 1, saved ); } );
		if ( !found.szStartCommands.empty() )
			RunM2Case( found.szStartCommands, "a shipped map's first start command erased and put back",
			           none,
			           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseStartCommand( p, 0, &saved ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, 0, saved ); } );
		RunM2Refusal( szCold, "start command replace past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceStartCommand( p, int( p->startCommandsList.size() ), commandA ); } );
		RunM2Refusal( szCold, "start command insert past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, int( p->startCommandsList.size() ) + 1, commandA ); } );
	}

	// ---- Reserve positions (std::list) ----
	{
		const SBattlePosition positionA( 11, 12, CVec2( 50.0f, 60.0f ) );
		const SBattlePosition positionB( 13, 0, CVec2( 70.0f, 80.0f ) );
		SBattlePosition saved;
		int nAt = 0;
		RunM2Case( szCold, "reserve position appended",
		           none,
		           [&]( SLoadMapInfo *p ) { nAt = int( p->reservePositionsList.size() ); return NMapRecords::InsertReservePosition( p, -1, positionA ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseReservePosition( p, nAt ); } );
		RunM2Case( szCold, "reserve position inserted in front",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertReservePosition( p, -1, positionA ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertReservePosition( p, 0, positionB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseReservePosition( p, 0 ); } );
		RunM2Case( szCold, "reserve position replaced",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertReservePosition( p, -1, positionA ); },
		           [&]( SLoadMapInfo *p ) { saved = p->reservePositionsList.back(); return NMapRecords::ReplaceReservePosition( p, int( p->reservePositionsList.size() ) - 1, positionB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceReservePosition( p, int( p->reservePositionsList.size() ) - 1, saved ); } );
		RunM2Case( szCold, "reserve position erased and put back at its index",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertReservePosition( p, -1, positionA ) && NMapRecords::InsertReservePosition( p, -1, positionB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseReservePosition( p, int( p->reservePositionsList.size() ) - 2, &saved ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertReservePosition( p, int( p->reservePositionsList.size() ) - 1, saved ); } );
		if ( !found.szReserve.empty() )
			RunM2Case( found.szReserve, "a shipped map's first reserve position erased and put back",
			           none,
			           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseReservePosition( p, 0, &saved ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertReservePosition( p, 0, saved ); } );
		RunM2Refusal( szCold, "reserve position erase past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::EraseReservePosition( p, int( p->reservePositionsList.size() ) ); } );
		RunM2Refusal( szCold, "reserve position insert past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::InsertReservePosition( p, int( p->reservePositionsList.size() ) + 1, positionA ); } );
	}

	// ---- The AI general: a side and the side count together ----
	{
		SAIGeneralSideInfo sideInfo;
		sideInfo.mobileScriptIDs.push_back( 4 );
		sideInfo.mobileScriptIDs.push_back( 8 );
		SAIGeneralParcelInfo parcel;
		parcel.eType = SAIGeneralParcelInfo::EPATCH_DEFENCE;
		parcel.vCenter = CVec2( 300.0f, 310.0f );
		parcel.fRadius = 256.0f;
		parcel.wDefenceDirection = 1000;
		parcel.reinforcePoints.push_back( SAIGeneralParcelInfo::SReinforcePointInfo( CVec2( 20.0f, 30.0f ), 5 ) );
		sideInfo.parcels.push_back( parcel );
		NMapRecords::SAIGeneralSidePut before;
		// Side 2 created on a map with fewer sides: the lower ones come out
		// empty, and the put of the old count takes them away again.
		RunM2Case( szCold, "AI general side 2 created (lower sides created empty)",
		           none,
		           [&]( SLoadMapInfo *p )
		           {
			           NMapRecords::GetAIGeneralSide( *p, 2, &before );
			           NMapRecords::SAIGeneralSidePut put;
			           put.nSideCount = Max( before.nSideCount, 3 );
			           put.nSide = 2;
			           put.info = sideInfo;
			           return NMapRecords::PutAIGeneralSide( p, put );
		           },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::PutAIGeneralSide( p, before ); } );
		RunM2Case( szCold, "AI general side replaced",
		           [&]( SLoadMapInfo *p )
		           {
			           NMapRecords::SAIGeneralSidePut put;
			           put.nSideCount = 2;
			           put.nSide = 1;
			           put.info = sideInfo;
			           return NMapRecords::PutAIGeneralSide( p, put );
		           },
		           [&]( SLoadMapInfo *p )
		           {
			           NMapRecords::GetAIGeneralSide( *p, 1, &before );
			           NMapRecords::SAIGeneralSidePut put;
			           put.nSideCount = 2;
			           put.nSide = 1;
			           put.info = SAIGeneralSideInfo();
			           return NMapRecords::PutAIGeneralSide( p, put );
		           },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::PutAIGeneralSide( p, before ); } );
		if ( !found.szAISides.empty() )
			RunM2Case( found.szAISides, "a shipped map's side 0 changed and put back",
			           none,
			           [&]( SLoadMapInfo *p )
			           {
				           NMapRecords::GetAIGeneralSide( *p, 0, &before );
				           NMapRecords::SAIGeneralSidePut put = before;
				           put.info = SAIGeneralSideInfo();
				           put.info.mobileScriptIDs.push_back( 77 );
				           return NMapRecords::PutAIGeneralSide( p, put );
			           },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::PutAIGeneralSide( p, before ); } );
		RunM2Refusal( szCold, "AI general negative side count", none,
		              [&]( SLoadMapInfo *p ) { NMapRecords::SAIGeneralSidePut put; put.nSideCount = -1; return NMapRecords::PutAIGeneralSide( p, put ); } );
		RunM2Refusal( szCold, "AI general absurd side count", none,
		              [&]( SLoadMapInfo *p ) { NMapRecords::SAIGeneralSidePut put; put.nSideCount = NMapRecords::nMaxAIGeneralSides + 1; return NMapRecords::PutAIGeneralSide( p, put ); } );
	}

	// ---- Roads and rivers ----
	{
		SVectorStripeObject saved;
		const SVectorStripeObject vsoA = MakeVso( 9001, 100.0f, 100.0f );
		const SVectorStripeObject vsoB = MakeVso( 9002, 300.0f, 200.0f );
		int nAt = 0;
		RunM2Case( szCold, "road appended",
		           none,
		           [&]( SLoadMapInfo *p ) { nAt = int( p->terrain.roads3.size() ); return NMapRecords::InsertVso( p, NMapRecords::VSO_ROAD, -1, vsoA ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseVso( p, NMapRecords::VSO_ROAD, nAt ); } );
		RunM2Case( szCold, "river appended",
		           none,
		           [&]( SLoadMapInfo *p ) { nAt = int( p->terrain.rivers.size() ); return NMapRecords::InsertVso( p, NMapRecords::VSO_RIVER, -1, vsoA ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseVso( p, NMapRecords::VSO_RIVER, nAt ); } );
		RunM2Case( szCold, "road replaced",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertVso( p, NMapRecords::VSO_ROAD, -1, vsoA ); },
		           [&]( SLoadMapInfo *p ) { const int n = int( p->terrain.roads3.size() ) - 1; saved = p->terrain.roads3[n]; return NMapRecords::ReplaceVso( p, NMapRecords::VSO_ROAD, n, vsoB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceVso( p, NMapRecords::VSO_ROAD, int( p->terrain.roads3.size() ) - 1, saved ); } );
		RunM2Case( szCold, "river erased and put back at its index",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertVso( p, NMapRecords::VSO_RIVER, -1, vsoA ) && NMapRecords::InsertVso( p, NMapRecords::VSO_RIVER, -1, vsoB ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseVso( p, NMapRecords::VSO_RIVER, int( p->terrain.rivers.size() ) - 2, &saved ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertVso( p, NMapRecords::VSO_RIVER, int( p->terrain.rivers.size() ) - 1, saved ); } );
		if ( !found.szRoads.empty() )
			RunM2Case( found.szRoads, "a shipped road erased and put back",
			           none,
			           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseVso( p, NMapRecords::VSO_ROAD, 0, &saved ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertVso( p, NMapRecords::VSO_ROAD, 0, saved ); } );
		if ( !found.szRivers.empty() )
			RunM2Case( found.szRivers, "a shipped river erased and put back",
			           none,
			           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseVso( p, NMapRecords::VSO_RIVER, 0, &saved ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertVso( p, NMapRecords::VSO_RIVER, 0, saved ); } );
		RunM2Refusal( szCold, "road erase past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::EraseVso( p, NMapRecords::VSO_ROAD, int( p->terrain.roads3.size() ) ); } );
		RunM2Refusal( szCold, "river replace past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceVso( p, NMapRecords::VSO_RIVER, int( p->terrain.rivers.size() ), vsoA ); } );
		SLoadMapInfo *pNullMap = 0;
		++g_nM2Cases;
		Check( !NMapRecords::InsertVso( pNullMap, NMapRecords::VSO_ROAD, -1, vsoA ), "InsertVso refuses a null map" );

		// NextVsoID is above every road and river ID, arnheim's included.
		CMapInfo map;
		if ( ReadFresh( szArnheim, &map ) )
		{
			++g_nM2Cases;
			const int nNext = NMapRecords::NextVsoID( map );
			bool bAbove = nNext >= 1;
			for ( size_t i = 0; i < map.terrain.roads3.size(); ++i ) bAbove = bAbove && nNext > map.terrain.roads3[i].nID;
			for ( size_t i = 0; i < map.terrain.rivers.size(); ++i ) bAbove = bAbove && nNext > map.terrain.rivers[i].nID;
			Check( bAbove, "NextVsoID on arnheim is above every road and river nID" );
			printf( "map-file: arnheim has %d roads and %d rivers, NextVsoID %d\n", int( map.terrain.roads3.size() ), int( map.terrain.rivers.size() ), nNext );
		}
	}

	// ---- Bridge and entrenchment entries ----
	{
		std::vector<int> savedEntry;
		SEntrenchmentInfo savedTrench;
		std::vector<int> spans;
		spans.push_back( 101 ); spans.push_back( 102 ); spans.push_back( 103 );
		std::vector<int> otherSpans( 1, 201 );
		SEntrenchmentInfo trench;
		trench.sections.push_back( std::vector<int>( 3, 301 ) );
		trench.sections[0][1] = 302; trench.sections[0][2] = 303;
		SEntrenchmentInfo otherTrench;
		otherTrench.sections.push_back( std::vector<int>( 2, 401 ) );
		otherTrench.sections.push_back( std::vector<int>( 1, 402 ) );
		int nAt = 0;
		RunM2Case( szCold, "bridge entry appended",
		           none,
		           [&]( SLoadMapInfo *p ) { nAt = int( p->bridges.size() ); return NMapRecords::InsertBridgeEntry( p, -1, spans ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseBridgeEntry( p, nAt ); } );
		RunM2Case( szCold, "bridge entry replaced",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertBridgeEntry( p, -1, spans ); },
		           [&]( SLoadMapInfo *p ) { const int n = int( p->bridges.size() ) - 1; savedEntry = p->bridges[n]; return NMapRecords::ReplaceBridgeEntry( p, n, otherSpans ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceBridgeEntry( p, int( p->bridges.size() ) - 1, savedEntry ); } );
		if ( !found.szBridges.empty() )
			RunM2Case( found.szBridges, "a shipped bridge entry erased and put back",
			           none,
			           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseBridgeEntry( p, 0, &savedEntry ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertBridgeEntry( p, 0, savedEntry ); } );
		RunM2Case( szCold, "entrenchment appended",
		           none,
		           [&]( SLoadMapInfo *p ) { nAt = int( p->entrenchments.size() ); return NMapRecords::InsertEntrenchment( p, -1, trench ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseEntrenchment( p, nAt ); } );
		RunM2Case( szCold, "entrenchment replaced",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertEntrenchment( p, -1, trench ); },
		           [&]( SLoadMapInfo *p ) { const int n = int( p->entrenchments.size() ) - 1; savedTrench = p->entrenchments[n]; return NMapRecords::ReplaceEntrenchment( p, n, otherTrench ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceEntrenchment( p, int( p->entrenchments.size() ) - 1, savedTrench ); } );
		RunM2Case( szCold, "entrenchment erased and put back at its index",
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertEntrenchment( p, -1, trench ) && NMapRecords::InsertEntrenchment( p, -1, otherTrench ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseEntrenchment( p, int( p->entrenchments.size() ) - 2, &savedTrench ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertEntrenchment( p, int( p->entrenchments.size() ) - 1, savedTrench ); } );
		if ( !found.szEntrenchments.empty() )
			RunM2Case( found.szEntrenchments, "a shipped entrenchment erased and put back",
			           none,
			           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseEntrenchment( p, 0, &savedTrench ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertEntrenchment( p, 0, savedTrench ); } );
		RunM2Refusal( szCold, "bridge entry erase past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::EraseBridgeEntry( p, int( p->bridges.size() ) ); } );
		RunM2Refusal( szCold, "entrenchment replace past the end", none, [&]( SLoadMapInfo *p ) { return NMapRecords::ReplaceEntrenchment( p, int( p->entrenchments.size() ), trench ); } );
	}

	// ---- An object's script ID and HP ----
	{
		CMapInfo probe;
		int nLinkID = 0;
		if ( ReadFresh( szCold, &probe ) )
			nLinkID = FindUsableLinkID( probe );
		if ( Check( nLinkID != 0, "coldwinter has an object with a unique nonzero link ID" ) )
		{
			int nOldScript = 0;
			float fOldHP = 0.0f;
			RunM2Case( szCold, "object script ID set",
			           none,
			           [&]( SLoadMapInfo *p ) { nOldScript = ObjectByLinkID( *p, nLinkID )->nScriptID; return NMapRecords::SetObjectScriptID( p, nLinkID, 4242 ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectScriptID( p, nLinkID, nOldScript ); } );
			RunM2Case( szCold, "object script ID cleared to -1",
			           [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectScriptID( p, nLinkID, 12 ); },
			           [&]( SLoadMapInfo *p ) { nOldScript = ObjectByLinkID( *p, nLinkID )->nScriptID; return NMapRecords::SetObjectScriptID( p, nLinkID, -1 ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectScriptID( p, nLinkID, nOldScript ); } );
			RunM2Case( szCold, "object HP set",
			           none,
			           [&]( SLoadMapInfo *p ) { fOldHP = ObjectByLinkID( *p, nLinkID )->fHP; return NMapRecords::SetObjectHP( p, nLinkID, fOldHP == 0.5f ? 0.25f : 0.5f ); },
			           [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectHP( p, nLinkID, fOldHP ); } );
			RunM2Refusal( szCold, "script ID above 32000", none, [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectScriptID( p, nLinkID, 32001 ); } );
			RunM2Refusal( szCold, "script ID below -1", none, [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectScriptID( p, nLinkID, -2 ); } );
			RunM2Refusal( szCold, "script ID on link ID 0", none, [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectScriptID( p, 0, 5 ); } );
			RunM2Refusal( szCold, "script ID on an unknown link", none, [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectScriptID( p, 987654321, 5 ); } );
			RunM2Refusal( szCold, "HP that is not a number", none, [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectHP( p, nLinkID, std::numeric_limits<float>::quiet_NaN() ); } );
			RunM2Refusal( szCold, "HP on link ID 0", none, [&]( SLoadMapInfo *p ) { return NMapRecords::SetObjectHP( p, 0, 0.5f ); } );
		}
		// An add's three fields reach the record (Pitfall 5); the defaults keep
		// the M1 values.
		CMapInfo map;
		if ( ReadFresh( szCold, &map ) )
		{
			g_nM2Cases += 2;
			NMapOverlay::SAddObject add;
			add.szName = "M2_object";
			int nDefault = 0, nGiven = 0;
			NMapOverlay::AddObject( &map, add, &nDefault );
			add.nFrameIndex = 3; add.fHP = 0.5f; add.nScriptID = 77;
			NMapOverlay::AddObject( &map, add, &nGiven );
			const SMapObjectInfo *pDefault = ObjectByLinkID( map, nDefault ), *pGiven = ObjectByLinkID( map, nGiven );
			Check( pDefault != 0 && pDefault->nFrameIndex == 0 && pDefault->fHP == 1.0f && pDefault->nScriptID == -1, "an add's defaults are the M1 values" );
			Check( pGiven != 0 && pGiven->nFrameIndex == 3 && pGiven->fHP == 0.5f && pGiven->nScriptID == 77, "an add's frame index, HP and script ID reach the record" );
		}
	}

	RemoveM2Files();
	Check( g_nM2Cases >= 30, "at least 30 M2 cases were checked" );
	printf( "map-file: M2 record ops ok (%d cases)\n", g_nM2Cases );
}

// ---------------------------------------------------------------------------
// D-04 at the map-file tier: what names an object is found correctly, and a
// delete edits those records the way the MFC editor does, with an exact undo.
// ---------------------------------------------------------------------------

// Up to nWanted objects of the file with a link ID of their own (nonzero, held
// by one object) whose delete the overlay does not refuse, in file order.
static std::vector<int> FindUsableLinkIDs( const SLoadMapInfo &rMap, size_t nWanted )
{
	std::map<int, int> counts;
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
		++counts[rMap.objects[i].link.nLinkID];
	for ( size_t i = 0; i < rMap.scenarioObjects.size(); ++i )
		++counts[rMap.scenarioObjects[i].link.nLinkID];
	std::vector<int> found;
	for ( size_t i = 0; i < rMap.objects.size() && found.size() < nWanted; ++i )
	{
		const int nLinkID = rMap.objects[i].link.nLinkID;
		if ( nLinkID == 0 || counts[nLinkID] != 1 )
			continue;
		SLoadMapInfo copy = rMap;
		std::string szRefusal;
		if ( NMapOverlay::DeleteObject( &copy, nLinkID, &szRefusal ) )
			found.push_back( nLinkID );
	}
	return found;
}

// A script ID no object of the map carries.
static int FreeScriptID( const SLoadMapInfo &rMap )
{
	int nMax = 0;
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
		nMax = Max( nMax, rMap.objects[i].nScriptID );
	for ( size_t i = 0; i < rMap.scenarioObjects.size(); ++i )
		nMax = Max( nMax, rMap.scenarioObjects[i].nScriptID );
	return nMax + 1;
}

static bool HasReference( const std::vector<std::string> &rReferences, const char *pszPrefix )
{
	for ( size_t i = 0; i < rReferences.size(); ++i )
		if ( rReferences[i].compare( 0, strlen( pszPrefix ), pszPrefix ) == 0 )
			return true;
	return false;
}

static SAIStartCommand MakeStartCommand( int nUnitA, int nUnitB, int nTarget )
{
	SAIStartCommand command;
	if ( nUnitA >= 0 )
		command.unitLinkIDs.push_back( nUnitA );
	if ( nUnitB >= 0 )
		command.unitLinkIDs.push_back( nUnitB );
	command.linkID = nTarget;
	return command;
}

static bool PutMobileScriptID( SLoadMapInfo *pMap, int nScriptID )
{
	NMapRecords::SAIGeneralSidePut put;
	NMapRecords::GetAIGeneralSide( *pMap, 0, &put );
	put.nSideCount = Max( put.nSideCount, 1 );
	put.nSide = 0;
	put.info.mobileScriptIDs.push_back( nScriptID );
	return NMapRecords::PutAIGeneralSide( pMap, put );
}

// One length the MFC way, written out here so the test states the rule in its own
// arithmetic rather than through the function under test: Vis2AI's scaling by
// 1 / fAITileXCoeff (sqrt 2 for the 32 sqrt 2 / 64 the coefficient is), then
// int( x + 0.3f ).
static float MfcLength( float fVis )
{
	return float( int( fVis * 1.41421356f + 0.3f ) );
}

// D-21's conversion (04-10): literal Vis drags give the AI values the truncation
// rule computes, for both shapes, the two handle edits and the radius through x;
// an area goes through InsertScriptArea, a write and a read; and the areas a
// shipped map holds stay byte for byte where they were after an insert and an
// erase.
static void TestM2ScriptAreaConversion()
{
	// A rectangle dragged from (100, 200) to (300, 260), world units: the centre is
	// (200, 230) and the half size (100, 30); in AI units 283, 325, 141 and 42 -
	// hand-computed below AND asserted through the rule.
	const SScriptArea rect = NMapGeometry::AreaFromVis( SScriptArea::EAT_RECTANGLE, CVec2( 100.0f, 200.0f ), CVec2( 300.0f, 260.0f ), "rect" );
	Check( rect.eType == SScriptArea::EAT_RECTANGLE && rect.szName == "rect", "a rectangle drag makes a rectangle with the name" );
	Check( rect.center.x == 283.0f && rect.center.y == 325.0f && rect.vAABBHalfSize.x == 141.0f && rect.vAABBHalfSize.y == 42.0f,
	       NStr::Format( "the rectangle is centre (%.0f, %.0f) half size (%.0f, %.0f) in AI units", rect.center.x, rect.center.y, rect.vAABBHalfSize.x, rect.vAABBHalfSize.y ) );
	Check( rect.center.x == MfcLength( 200.0f ) && rect.center.y == MfcLength( 230.0f ) &&
	       rect.vAABBHalfSize.x == MfcLength( 100.0f ) && rect.vAABBHalfSize.y == MfcLength( 30.0f ) && rect.fR == 0.0f,
	       "and equals the truncation rule's arithmetic, the radius left 0" );
	// The drag's direction does not matter: half sizes are absolute values.
	const SScriptArea rectBack = NMapGeometry::AreaFromVis( SScriptArea::EAT_RECTANGLE, CVec2( 300.0f, 260.0f ), CVec2( 100.0f, 200.0f ), "rect" );
	Check( rectBack.center.x == rect.center.x && rectBack.center.y == rect.center.y && rectBack.vAABBHalfSize.x == rect.vAABBHalfSize.x &&
	       rectBack.vAABBHalfSize.y == rect.vAABBHalfSize.y, "dragging the rectangle the other way makes the same area" );

	// A circle from (50, 60) to (80, 100): centre the first point, radius the
	// Euclidean distance 50 - converted through x.
	const SScriptArea circle = NMapGeometry::AreaFromVis( SScriptArea::EAT_CIRCLE, CVec2( 50.0f, 60.0f ), CVec2( 80.0f, 100.0f ), "ring" );
	Check( circle.eType == SScriptArea::EAT_CIRCLE && circle.szName == "ring", "a circle drag makes a circle with the name" );
	Check( circle.center.x == 71.0f && circle.center.y == 85.0f && circle.fR == 71.0f,
	       NStr::Format( "the circle is centre (%.0f, %.0f) radius %.0f in AI units", circle.center.x, circle.center.y, circle.fR ) );
	Check( circle.center.x == MfcLength( 50.0f ) && circle.center.y == MfcLength( 60.0f ) && circle.fR == MfcLength( 50.0f ) &&
	       circle.vAABBHalfSize.x == 0.0f && circle.vAABBHalfSize.y == 0.0f, "and equals the rule's arithmetic, the half size left 0" );

	// The truncation and its 0.3: 0.35 world units is 0.495 AI units plus 0.3 = 0.795, which truncates to 0; 0.5 is 0.707 + 0.3 = 1.007 and gives 1.
	const SScriptArea tiny = NMapGeometry::AreaFromVis( SScriptArea::EAT_CIRCLE, CVec2( 0.35f, 0.5f ), CVec2( 0.35f, 0.5f ), "tiny" );
	Check( tiny.center.x == 0.0f && tiny.center.y == 1.0f && tiny.fR == 0.0f, "the centre truncates with Vis2AI's +0.3 (0.35 -> 0, 0.5 -> 1) and a zero drag has radius 0" );

	// Moving keeps the size and name; the new centre takes the rule.
	const SScriptArea moved = NMapGeometry::MoveArea( rect, CVec2( 10.0f, 20.0f ) );
	Check( moved.center.x == MfcLength( 10.0f ) && moved.center.y == MfcLength( 20.0f ) && moved.vAABBHalfSize.x == rect.vAABBHalfSize.x &&
	       moved.vAABBHalfSize.y == rect.vAABBHalfSize.y && moved.szName == rect.szName && moved.eType == rect.eType, "MoveArea moves the centre only" );
	// Resizing: the handle's distance from the centre in world units (the stored
	// centre brought back with AI2Vis), converted by the rule.
	CVec2 vRectCentreVis;
	AI2Vis( &vRectCentreVis, rect.center );
	const SScriptArea resizedRect = NMapGeometry::ResizeArea( rect, CVec2( vRectCentreVis.x + 40.0f, vRectCentreVis.y - 12.0f ) );
	Check( resizedRect.vAABBHalfSize.x == MfcLength( 40.0f ) && resizedRect.vAABBHalfSize.y == MfcLength( 12.0f ) &&
	       resizedRect.center.x == rect.center.x && resizedRect.center.y == rect.center.y, "ResizeArea sets a rectangle's half size from the handle, the centre kept" );
	CVec2 vCircleCentreVis;
	AI2Vis( &vCircleCentreVis, circle.center );
	const SScriptArea resizedCircle = NMapGeometry::ResizeArea( circle, CVec2( vCircleCentreVis.x + 30.0f, vCircleCentreVis.y + 40.0f ) );
	Check( resizedCircle.fR == MfcLength( 50.0f ) && resizedCircle.center.x == circle.center.x && resizedCircle.vAABBHalfSize.x == 0.0f,
	       "ResizeArea sets a circle's radius from the handle's distance, through x" );

	// Through InsertScriptArea, a write and a read, the area comes back as it went in.
	const std::string szCold = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	CMapInfo map;
	std::string szError;
	if ( ReadFresh( szCold, &map ) )
	{
		NMapRecords::InsertScriptArea( &map, -1, rect );
		NMapRecords::InsertScriptArea( &map, -1, circle );
		if ( Check( NMapFile::Write( M2_EDITED, map, &szError ), szError.c_str() ) )
		{
			CMapInfo reread;
			if ( Check( NMapFile::Read( M2_EDITED, &reread, &szError ), szError.c_str() ) && Check( reread.scriptAreas.size() == map.scriptAreas.size(), "both areas are in the file" ) )
			{
				const SScriptArea &rRect = reread.scriptAreas[reread.scriptAreas.size() - 2];
				const SScriptArea &rCircle = reread.scriptAreas.back();
				Check( rRect.szName == "rect" && rRect.eType == SScriptArea::EAT_RECTANGLE && rRect.center.x == 283.0f && rRect.center.y == 325.0f &&
				       rRect.vAABBHalfSize.x == 141.0f && rRect.vAABBHalfSize.y == 42.0f, "the rectangle reads back as stored" );
				Check( rCircle.szName == "ring" && rCircle.eType == SScriptArea::EAT_CIRCLE && rCircle.center.x == 71.0f && rCircle.center.y == 85.0f && rCircle.fR == 71.0f,
				       "the circle reads back as stored" );
				std::string szWhere;
				Check( NMapFile::AreEquivalent( map, reread, &szWhere ), szWhere.empty() ? "the map with both areas is equivalent" : ( "the areas differ at " + szWhere ).c_str() );
			}
		}
	}

	// A shipped map's own areas are untouched bytes after an insert and an erase.
	const SM2Maps found = FindM2Maps();
	if ( !found.szScriptAreas.empty() )
	{
		int nAt = 0;
		RunM2Case( found.szScriptAreas, "an area inserted in and erased from a shipped map that holds areas",
		           TMapOp(),
		           [&]( SLoadMapInfo *p ) { nAt = int( p->scriptAreas.size() ); return NMapRecords::InsertScriptArea( p, -1, rect ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseScriptArea( p, nAt ); } );
		RunM2Case( found.szScriptAreas, "an area inserted in front of a shipped map's areas and erased",
		           TMapOp(),
		           [&]( SLoadMapInfo *p ) { return NMapRecords::InsertScriptArea( p, 0, circle ); },
		           [&]( SLoadMapInfo *p ) { return NMapRecords::EraseScriptArea( p, 0 ); } );
	}
	else
		printf( "map-file: no shipped map of the first 60 holds a script area; the byte-exact case ran on coldwinter only\n" );
	RemoveM2Files();
	printf( "map-file: M2 script areas ok\n" );
}

static void TestM2FindReferences()
{
	const char *pszCold = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	CMapInfo map;
	if ( !ReadFresh( pszCold, &map ) )
		return;
	const std::vector<int> ids = FindUsableLinkIDs( map, 6 );
	if ( !Check( ids.size() == 6, "coldwinter has six objects to refer to" ) )
		return;
	const int nA = ids[0], nB = ids[1], nC = ids[2], nD = ids[3], nE = ids[4], nF = ids[5];
	const int nScript = FreeScriptID( map );

	// A: a script ID a reinforcement group and the AI general's mobile
	// reinforcements hold.
	Check( NMapRecords::SetObjectScriptID( &map, nA, nScript ), "A takes a script ID" );
	const int nGroup = NMapRecords::FirstFreeGroupID( map, 5 );
	std::vector<int> groupIDs( 1, nScript );
	Check( NMapRecords::PutReinforcementGroup( &map, nGroup, groupIDs ), "the group holds A's script ID" );
	Check( PutMobileScriptID( &map, nScript ), "side 0's mobile reinforcements hold it too" );
	// F: the old bug. A group's ids are script IDs; this one holds a number that
	// equals F's LINK ID, and F's script ID is something else.
	Check( ObjectByLinkID( map, nF )->nScriptID != nF, "F's script ID differs from its link ID" );
	const int nGroupOfF = NMapRecords::FirstFreeGroupID( map, nGroup + 1 );
	std::vector<int> groupOfF( 1, nF );
	Check( NMapRecords::PutReinforcementGroup( &map, nGroupOfF, groupOfF ), "a group holds a number equal to F's link ID" );
	// B: a start command's unit and another's target.
	NMapRecords::InsertStartCommand( &map, -1, MakeStartCommand( nB, -1, 0 ) );
	NMapRecords::InsertStartCommand( &map, -1, MakeStartCommand( nA, -1, nB ) );
	// C: a trench piece. D and E: a reserve position's artillery and truck.
	SEntrenchmentInfo trench;
	trench.sections.push_back( SEntrenchmentInfo::TSegment( 1, nC ) );
	NMapRecords::InsertEntrenchment( &map, -1, trench );
	NMapRecords::InsertReservePosition( &map, -1, SBattlePosition( nD, 0, CVec2( 100.0f, 100.0f ) ) );
	NMapRecords::InsertReservePosition( &map, -1, SBattlePosition( 0, nE, CVec2( 120.0f, 100.0f ) ) );
	// Link ID 0 everywhere a record uses it for "none".
	NMapRecords::InsertStartCommand( &map, -1, MakeStartCommand( 0, -1, 0 ) );

	std::vector<std::string> refs;
	NMapOverlay::FindReferences( map, nA, &refs );
	Check( HasReference( refs, "reinforcement group" ) && HasReference( refs, "AI general side" ), "a group and the AI general are found by the script ID" );
	Check( HasReference( refs, "start command" ), "A is found as a unit of a start command" );
	NMapOverlay::FindReferences( map, nB, &refs );
	Check( HasReference( refs, "start command" ) && refs.size() == 2, "B is found as a unit and as a target, in both commands" );
	NMapOverlay::FindReferences( map, nC, &refs );
	Check( HasReference( refs, "entrenchment" ), "C is found as a trench piece" );
	NMapOverlay::FindReferences( map, nD, &refs );
	Check( HasReference( refs, "reserve position" ), "D is found as reserve artillery" );
	NMapOverlay::FindReferences( map, nE, &refs );
	Check( HasReference( refs, "reserve position" ), "E is found as a reserve truck" );
	NMapOverlay::FindReferences( map, nF, &refs );
	Check( refs.empty(), "F is not found by a group holding a number equal to its link ID" );
	NMapOverlay::FindReferences( map, 0, &refs );
	Check( refs.empty(), "link ID 0 finds nothing, though records list it" );
	printf( "map-file: M2 find references ok\n" );
}

typedef std::function<void( const SLoadMapInfo&, const NMapOverlay::SDeletedObject& )> TCascadeVerify;

// One cascade rule: the delete written, read back and compared, its restore
// byte-exact (RunM2Case), and then the rule itself on one more fresh read.
static void RunCascadeCase( const char *pszName, int nTarget, const TMapOp &setup, const TCascadeVerify &verify )
{
	const char *pszCold = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	NMapOverlay::SDeletedObject deleted;
	RunM2Case( pszCold, pszName, setup,
	           [&]( SLoadMapInfo *p ) { std::string szRefusal; return NMapOverlay::DeleteObject( p, nTarget, &szRefusal, &deleted ); },
	           [&]( SLoadMapInfo *p ) { return NMapOverlay::RestoreObject( p, deleted ); } );
	CMapInfo map;
	if ( !ReadFresh( pszCold, &map ) )
		return;
	ApplyOp( setup, &map );
	NMapOverlay::SDeletedObject once;
	std::string szRefusal;
	if ( Check( NMapOverlay::DeleteObject( &map, nTarget, &szRefusal, &once ), ( std::string( "cascade case \"" ) + pszName + "\" deletes: " + szRefusal ).c_str() ) )
		verify( map, once );
}

static void RunCascadeRefusal( const char *pszName, int nTarget, const char *pszWhy, const TMapOp &setup )
{
	const char *pszCold = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	RunM2Refusal( pszCold, pszName, setup, [&]( SLoadMapInfo *p ) { std::string szRefusal; return NMapOverlay::DeleteObject( p, nTarget, &szRefusal ); } );
	CMapInfo map;
	if ( !ReadFresh( pszCold, &map ) )
		return;
	ApplyOp( setup, &map );
	std::string szRefusal;
	NMapOverlay::DeleteObject( &map, nTarget, &szRefusal );
	Check( szRefusal.find( pszWhy ) != std::string::npos, ( std::string( "refusal \"" ) + pszName + "\" says \"" + pszWhy + "\", not \"" + szRefusal + "\"" ).c_str() );
}

static void TestM2CascadeKinds()
{
	const char *pszCold = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	CMapInfo probe;
	if ( !ReadFresh( pszCold, &probe ) )
		return;
	const std::vector<int> ids = FindUsableLinkIDs( probe, 2 );
	if ( !Check( ids.size() == 2, "coldwinter has two objects for the cascade cases" ) )
		return;
	const int nU = ids[0], nV = ids[1];
	const size_t nCommands = probe.startCommandsList.size(), nReserves = probe.reservePositionsList.size();
	const int nScript = FreeScriptID( probe );
	const size_t nGroups = probe.reinforcements.groups.size();

	RunCascadeCase( "cascade: unit removed from a command", nU,
	                [=]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, -1, MakeStartCommand( nU, nV, 0 ) ); },
	                [=]( const SLoadMapInfo &m, const NMapOverlay::SDeletedObject &d )
	                {
		                Check( m.startCommandsList.size() == nCommands + 1, "the command stays" );
		                Check( m.startCommandsList.back().unitLinkIDs.size() == 1 && m.startCommandsList.back().unitLinkIDs[0] == nV, "and loses only the deleted unit" );
		                Check( d.cascade.startCommands.size() == 1 && !d.cascade.startCommands[0].bErased, "the cascade records an edit, not an erase" );
	                } );
	RunCascadeCase( "cascade: empty command erased", nU,
	                [=]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, -1, MakeStartCommand( nU, -1, 0 ) ) &&
	                                                NMapRecords::InsertStartCommand( p, -1, MakeStartCommand( nV, -1, 0 ) ); },
	                [=]( const SLoadMapInfo &m, const NMapOverlay::SDeletedObject &d )
	                {
		                Check( m.startCommandsList.size() == nCommands + 1, "the command that named only the unit is gone" );
		                Check( m.startCommandsList.back().unitLinkIDs.size() == 1 && m.startCommandsList.back().unitLinkIDs[0] == nV, "and the one after it is untouched" );
		                Check( d.cascade.startCommands.size() == 1 && d.cascade.startCommands[0].bErased, "the cascade records an erase" );
	                } );
	RunCascadeCase( "cascade: target cleared to 0", nU,
	                [=]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, -1, MakeStartCommand( nV, -1, nU ) ); },
	                [=]( const SLoadMapInfo &m, const NMapOverlay::SDeletedObject &d )
	                {
		                Check( m.startCommandsList.size() == nCommands + 1, "the command stays" );
		                Check( m.startCommandsList.back().linkID == 0, "with target link ID 0, not a dangling link" );
		                Check( m.startCommandsList.back().unitLinkIDs.size() == 1 && m.startCommandsList.back().unitLinkIDs[0] == nV, "and its units as they were" );
		                Check( d.cascade.startCommands.size() == 1 && d.cascade.startCommands[0].bTargetCleared, "the cascade records the cleared target" );
	                } );
	RunCascadeCase( "cascade: a command with the unit as unit and target is erased", nU,
	                [=]( SLoadMapInfo *p ) { return NMapRecords::InsertStartCommand( p, -1, MakeStartCommand( nU, -1, nU ) ); },
	                [=]( const SLoadMapInfo &m, const NMapOverlay::SDeletedObject & )
	                {
		                Check( m.startCommandsList.size() == nCommands, "the command goes" );
	                } );
	RunCascadeCase( "cascade: reserve position erased as artillery", nU,
	                [=]( SLoadMapInfo *p ) { return NMapRecords::InsertReservePosition( p, -1, SBattlePosition( nU, nV, CVec2( 100.0f, 100.0f ) ) ); },
	                [=]( const SLoadMapInfo &m, const NMapOverlay::SDeletedObject &d )
	                {
		                Check( m.reservePositionsList.size() == nReserves, "the position that named it as artillery is gone" );
		                Check( d.cascade.reservePositions.size() == 1, "and the cascade holds it" );
	                } );
	RunCascadeCase( "cascade: reserve position erased as truck", nU,
	                [=]( SLoadMapInfo *p ) { return NMapRecords::InsertReservePosition( p, -1, SBattlePosition( nV, nU, CVec2( 100.0f, 100.0f ) ) ) &&
	                                                NMapRecords::InsertReservePosition( p, -1, SBattlePosition( nV, nV, CVec2( 140.0f, 100.0f ) ) ); },
	                [=]( const SLoadMapInfo &m, const NMapOverlay::SDeletedObject & )
	                {
		                Check( m.reservePositionsList.size() == nReserves + 1, "the position that named it as truck is gone, the other stays" );
		                Check( m.reservePositionsList.back().nArtilleryLinkID == nV && m.reservePositionsList.back().nTruckLinkID == nV, "untouched" );
	                } );
	// Reinforcement groups and mobileScriptIDs are never edited; the note says so.
	const TMapOp scriptSetup = [=]( SLoadMapInfo *p )
	{
		std::vector<int> group( 1, nScript );
		return NMapRecords::SetObjectScriptID( p, nU, nScript ) &&
		       NMapRecords::PutReinforcementGroup( p, NMapRecords::FirstFreeGroupID( *p, 5 ), group ) &&
		       PutMobileScriptID( p, nScript );
	};
	RunCascadeCase( "cascade: groups and mobile script IDs untouched, with a note", nU, scriptSetup,
	                [=]( const SLoadMapInfo &m, const NMapOverlay::SDeletedObject &d )
	                {
		                Check( m.reinforcements.groups.size() == nGroups + 1, "the group is still there" );
		                Check( !m.aiGeneralMapInfo.sidesInfo.empty() && !m.aiGeneralMapInfo.sidesInfo[0].mobileScriptIDs.empty() &&
		                       m.aiGeneralMapInfo.sidesInfo[0].mobileScriptIDs.back() == nScript, "and so is the mobile reinforcement" );
		                Check( d.cascade.notes.size() == 2, "two notes: the group and the AI general" );
		                std::string szLine;
		                NMapOverlay::DescribeCascade( d.cascade, &szLine );
		                char szNeedle[32];
		                snprintf( szNeedle, sizeof szNeedle, "script ID %d", nScript );
		                Check( szLine.find( szNeedle ) != std::string::npos, ( "the summary names the script ID: " + szLine ).c_str() );
	                } );
	RunCascadeCase( "cascade: no note while another object carries the script ID", nU,
	                [=]( SLoadMapInfo *p ) { return scriptSetup( p ) && NMapRecords::SetObjectScriptID( p, nV, nScript ); },
	                [=]( const SLoadMapInfo &, const NMapOverlay::SDeletedObject &d )
	                {
		                Check( d.cascade.notes.empty(), "V still carries the script ID, so nothing is said" );
	                } );
	// Link ID 0 is no link ID: records that list it are not naming the object.
	const TMapOp zeroSetup = [=]( SLoadMapInfo *p )
	{
		bool bHasZero = false;
		for ( size_t i = 0; i < p->objects.size(); ++i )
			bHasZero = bHasZero || p->objects[i].link.nLinkID == 0;
		if ( !bHasZero )
		{
			NMapOverlay::SAddObject add;
			add.szName = p->objects[0].szName;
			add.nLinkID = 0;
			if ( !NMapOverlay::AddObject( p, add, 0 ) )
				return false;
		}
		return NMapRecords::InsertStartCommand( p, -1, MakeStartCommand( 0, nV, 0 ) ) &&
		       NMapRecords::InsertReservePosition( p, -1, SBattlePosition( 0, 0, CVec2( 100.0f, 100.0f ) ) );
	};
	// A real object's delete leaves a command's 0 alone: it removes U and nothing else.
	RunCascadeCase( "cascade: link ID 0 in a command survives another unit's delete", nU,
	                [=]( SLoadMapInfo *p ) { return zeroSetup( p ) && NMapRecords::InsertStartCommand( p, -1, MakeStartCommand( 0, nU, 0 ) ); },
	                [=]( const SLoadMapInfo &m, const NMapOverlay::SDeletedObject &d )
	                {
		                Check( m.startCommandsList.size() == nCommands + 2, "both commands stay" );
		                Check( m.startCommandsList.back().unitLinkIDs.size() == 1 && m.startCommandsList.back().unitLinkIDs[0] == 0, "the command keeps its 0" );
		                Check( d.cascade.startCommands.size() == 1 && d.cascade.reservePositions.empty(), "and only that command was edited" );
	                } );
	// Deleting an object whose link ID is 0 (the session never asks: it refuses a
	// shared ID) is not a reference either: the records listing 0 stay as they are.
	{
		CMapInfo map;
		if ( ReadFresh( pszCold, &map ) )
		{
			++g_nM2Cases;
			zeroSetup( &map );
			NMapOverlay::SDeletedObject once;
			std::string szRefusal;
			if ( Check( NMapOverlay::DeleteObject( &map, 0, &szRefusal, &once ), "an object with link ID 0 deletes" ) )
			{
				Check( map.startCommandsList.size() == nCommands + 1 && map.startCommandsList.back().unitLinkIDs.size() == 2, "the command listing 0 is untouched" );
				Check( map.reservePositionsList.size() == nReserves + 1, "and so is the reserve position" );
				Check( once.cascade.startCommands.empty() && once.cascade.reservePositions.empty() && once.cascade.notes.empty(), "the cascade is empty" );
			}
		}
	}

	// What is still refused: a bridge span, a trench piece, a vehicle holding a
	// passenger. Each changes nothing and says why.
	RunCascadeRefusal( "cascade refusal: bridge span", nU, "still referred to by bridge",
	                   [=]( SLoadMapInfo *p ) { return NMapRecords::InsertBridgeEntry( p, -1, std::vector<int>( 1, nU ) ); } );
	RunCascadeRefusal( "cascade refusal: trench piece", nU, "still part of entrenchment",
	                   [=]( SLoadMapInfo *p )
	                   {
		                   SEntrenchmentInfo trench;
		                   trench.sections.push_back( SEntrenchmentInfo::TSegment( 1, nU ) );
		                   return NMapRecords::InsertEntrenchment( p, -1, trench );
	                   } );
	RunCascadeRefusal( "cascade refusal: vehicle with a passenger", nU, "still referred to by object",
	                   [=]( SLoadMapInfo *p )
	                   {
		                   for ( size_t i = 0; i < p->objects.size(); ++i )
			                   if ( p->objects[i].link.nLinkID == nV )
			                   {
				                   p->objects[i].link.nLinkWith = nU;
				                   return true;
			                   }
		                   return false;
	                   } );
	// A start command naming a span does not lift the refusal, and the refused
	// delete leaves the command alone.
	RunCascadeRefusal( "cascade refusal: a span named by a start command too", nU, "still referred to by bridge",
	                   [=]( SLoadMapInfo *p )
	                   {
		                   return NMapRecords::InsertBridgeEntry( p, -1, std::vector<int>( 1, nU ) ) &&
		                          NMapRecords::InsertStartCommand( p, -1, MakeStartCommand( nU, nV, 0 ) );
	                   } );
	RemoveM2Files();
	printf( "map-file: M2 cascade kinds ok\n" );
}

// Read a map, write it untouched, read it back: equivalent. Then write it a
// second time and compare the two files byte for byte - the spec's idempotent
// save. A map that survives both has not been quietly normalised. The bytes of
// the shipped file are deliberately not the yardstick: a rewrite is not
// byte-identical with whatever tool wrote the original, only field-equivalent.
// ---------------------------------------------------------------------------
// 04-05: roads and rivers are derived by the MFC tool's own builder
// (CVSOBuilder::CreateVSO, Update with DEFAULT_STEP, UpdateZ) - the same calls
// the bridge makes, so this tier builds the expected record with them and
// proves they are deterministic (D-03 stores the result, but a test that
// rebuilds it must get the same record).
// ---------------------------------------------------------------------------
static bool SameVsoRecord( const SVectorStripeObject &rLeft, const SVectorStripeObject &rRight )
{
	if ( rLeft.szDescName != rRight.szDescName || rLeft.nID != rRight.nID || rLeft.fPassability != rRight.fPassability )
		return false;
	if ( rLeft.controlpoints.size() != rRight.controlpoints.size() || rLeft.points.size() != rRight.points.size() )
		return false;
	for ( size_t i = 0; i < rLeft.controlpoints.size(); ++i )
		if ( !( rLeft.controlpoints[i] == rRight.controlpoints[i] ) )
			return false;
	for ( size_t i = 0; i < rLeft.points.size(); ++i )
	{
		const SVectorStripeObjectPoint &a = rLeft.points[i], &b = rRight.points[i];
		if ( !( a.vPos == b.vPos ) || !( a.vNorm == b.vNorm ) || a.fRadius != b.fRadius || a.fWidth != b.fWidth ||
		     a.bKeyPoint != b.bKeyPoint || a.fOpacity != b.fOpacity )
			return false;
	}
	return true;
}

// The bridge's add, as plain calls: CreateVSO, Update( false ), UpdateZ, the
// road passability fix and the bridge's own nID.
static bool BuildTestVso( const CMapInfo &rMap, const std::string &szDesc, const std::vector<CVec3> &rControls,
                          float fWidthTiles, float fOpacity, bool bRoad, SVectorStripeObject *pOut )
{
	SVectorStripeObject vso;
	if ( !CVSOBuilder::CreateVSO( &vso, szDesc, rControls ) || vso.controlpoints.size() < 2 )
		return false;
	CVSOBuilder::Update( &vso, false, CVSOBuilder::DEFAULT_STEP, fWidthTiles * fWorldCellSize / 2.0f, fOpacity );
	if ( vso.points.size() < 2 )
		return false;
	CVSOBuilder::UpdateZ( rMap.terrain.altitudes, &vso );
	if ( bRoad && vso.fPassability == 0 )
		vso.fPassability = 1;
	vso.nID = NMapRecords::NextVsoID( rMap );
	*pOut = vso;
	return true;
}

static void TestM2VsoBuilder()
{
	const char *pszMap = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	CMapInfo map;
	if ( !ReadFresh( pszMap, &map ) )
		return;
	if ( !Check( !map.terrain.roads3.empty(), "coldwinter has a road whose descriptor the builder can use" ) )
		return;
	const std::string szDesc = map.terrain.roads3[0].szDescName;
	const float fMiddleX = map.terrain.tiles.GetSizeX() * fWorldCellSize / 2.0f;
	const float fMiddleY = map.terrain.tiles.GetSizeY() * fWorldCellSize / 2.0f;
	std::vector<CVec3> controls;
	controls.push_back( CVec3( fMiddleX - 300.0f, fMiddleY - 100.0f, 0.0f ) );
	controls.push_back( CVec3( fMiddleX, fMiddleY + 60.0f, 0.0f ) );
	controls.push_back( CVec3( fMiddleX + 300.0f, fMiddleY - 40.0f, 0.0f ) );

	// Deterministic: two builds from the same input are the same record.
	SVectorStripeObject first, second;
	if ( !Check( BuildTestVso( map, szDesc, controls, 3.0f, 1.0f, true, &first ), ( "the builder makes a road from " + szDesc ).c_str() ) )
		return;
	Check( BuildTestVso( map, szDesc, controls, 3.0f, 1.0f, true, &second ) && SameVsoRecord( first, second ), "two builds from the same input are the same record" );
	Check( first.controlpoints.size() == 3, "the three control points are kept" );
	int nKeys = 0;
	for ( size_t i = 0; i < first.points.size(); ++i )
		if ( first.points[i].bKeyPoint )
			++nKeys;
	Check( nKeys == 3, NStr::Format( "one key point per control point (%d)", nKeys ) );
	Check( first.points.size() > 10, NStr::Format( "the road is sampled every 30 units (%d points)", int( first.points.size() ) ) );
	Check( first.nID > 0 && first.nID == NMapRecords::NextVsoID( map ), "the new nID is the map's next free one" );
	Check( std::fabs( first.points[0].fWidth - 3.0f * fWorldCellSize / 2.0f ) < 0.01f, "width 3 is 3 * fWorldCellSize / 2 world units" );

	// Too short: one point, and two points 1 unit apart (UniquePolygon's 2).
	SVectorStripeObject shortVso;
	std::vector<CVec3> one( 1, controls[0] );
	Check( !BuildTestVso( map, szDesc, one, 3.0f, 1.0f, true, &shortVso ), "a one-point road is refused" );
	std::vector<CVec3> close;
	close.push_back( controls[0] );
	close.push_back( controls[0] + CVec3( 1.0f, 0.0f, 0.0f ) );
	Check( !BuildTestVso( map, szDesc, close, 3.0f, 1.0f, true, &shortVso ), "two points 1 unit apart are refused" );
	std::vector<CVec3> tiny;
	tiny.push_back( controls[0] );
	tiny.push_back( controls[0] + CVec3( 10.0f, 0.0f, 0.0f ) );
	Check( !BuildTestVso( map, szDesc, tiny, 3.0f, 1.0f, true, &shortVso ), "a road shorter than one sampling step is refused" );

	// Inserted with InsertVso and saved, it reads back as the map the same
	// calls build.
	CMapInfo edited;
	if ( !ReadFresh( pszMap, &edited ) )
		return;
	Check( NMapRecords::InsertVso( &edited, NMapRecords::VSO_ROAD, -1, first ), "the new road inserts" );
	std::string szError;
	if ( Check( NMapFile::Write( M2_EDITED, edited, &szError ), szError.c_str() ) )
	{
		CMapInfo reread;
		if ( Check( NMapFile::Read( M2_EDITED, &reread, &szError ), szError.c_str() ) )
		{
			std::string szWhere;
			Check( NMapFile::AreEquivalent( edited, reread, &szWhere ), ( "the saved road differs at " + szWhere ).c_str() );
			Check( reread.terrain.roads3.size() == map.terrain.roads3.size() + 1 && SameVsoRecord( reread.terrain.roads3.back(), first ),
			       "the saved road is the built record" );
		}
	}

	// Pitfall 2: a road whose descriptor has passability 0 reads back as 1, so
	// an unfixed record fails the save's read-back; the bridge's fix makes it
	// read back equal.
	SVectorStripeObject zero = first;
	zero.fPassability = 0;
	CMapInfo unfixed;
	if ( ReadFresh( pszMap, &unfixed ) && NMapRecords::InsertVso( &unfixed, NMapRecords::VSO_ROAD, -1, zero ) &&
	     Check( NMapFile::Write( M2_EDITED, unfixed, &szError ), szError.c_str() ) )
	{
		CMapInfo reread;
		std::string szWhere;
		if ( Check( NMapFile::Read( M2_EDITED, &reread, &szError ), szError.c_str() ) )
			Check( !NMapFile::AreEquivalent( unfixed, reread, &szWhere ) && szWhere.find( "Passability" ) != std::string::npos,
			       ( "a road saved with passability 0 reads back different (at " + szWhere + ")" ).c_str() );
	}
	if ( zero.fPassability == 0 )
		zero.fPassability = 1;
	CMapInfo fixed;
	if ( ReadFresh( pszMap, &fixed ) && NMapRecords::InsertVso( &fixed, NMapRecords::VSO_ROAD, -1, zero ) &&
	     Check( NMapFile::Write( M2_EDITED, fixed, &szError ), szError.c_str() ) )
	{
		CMapInfo reread;
		std::string szWhere;
		if ( Check( NMapFile::Read( M2_EDITED, &reread, &szError ), szError.c_str() ) )
			Check( NMapFile::AreEquivalent( fixed, reread, &szWhere ), ( "the fixed road reads back equal (differs at " + szWhere + ")" ).c_str() );
	}

	// How often the shipped maps repeat a road's or river's nID inside one
	// list: the bridge maps saved to engine IDs by list position, and the
	// engine removes by ID, so a shared ID would make an edit ambiguous.
	std::vector<std::string> paths;
	CollectMaps( "Data\\Maps", true, &paths );
	int nRead = 0, nShared = 0;
	for ( size_t i = 0; i < paths.size() && nRead < 60; ++i )
	{
		CMapInfo shipped;
		if ( !NMapFile::Read( paths[i].c_str(), &shipped, &szError ) )
			continue;
		++nRead;
		const TVSOList *lists[2] = { &shipped.terrain.roads3, &shipped.terrain.rivers };
		bool bShared = false;
		for ( int k = 0; k < 2 && !bShared; ++k )
		{
			std::set<int> ids;
			for ( size_t j = 0; j < lists[k]->size(); ++j )
				if ( !ids.insert( ( *lists[k] )[j].nID ).second )
					bShared = true;
		}
		if ( bShared )
		{
			++nShared;
			printf( "map-file: %s repeats a road or river nID\n", paths[i].c_str() );
		}
	}
	printf( "map-file: %d of %d maps repeat a road or river nID within a list\n", nShared, nRead );
	RemoveM2Files();
	printf( "map-file: M2 vso builder ok\n" );
}

// The inputs of the shipped W_WoodenBig_Heavy_01 and _02
// (Data/Bridges/w_woodenbig_heavy/01/1.xml and 02/1.xml), read once and
// written here as literals, as the map-file tier has no object database (C5):
// the first line span (Spans item 1) has Length="6", so the span length is
// 6 * fWorldCellSize / 2 world units; the begin span (Begins item 0, Spans
// item 0, slab segment 2) has Origin x="226.274" y="113.137" in _01 and
// x="67.8826" y="216.375" in _02, which ToAIUnits turns into the map units
// below (Vis2AI: int( v * sqrt 2 + 0.3 )). _01 has Direction 01000000
// (horizontal), _02 00000000 (vertical). The engine tier plans with the real
// stats and checks the bridge's plan against these same functions.
static NMapGeometry::SBridgePlanInput WoodenBigHeavyInput( bool bHorizontal )
{
	NMapGeometry::SBridgePlanInput input;
	input.nDirection = bHorizontal ? NMapGeometry::BRIDGE_HORIZONTAL : NMapGeometry::BRIDGE_VERTICAL;
	input.fSpanLength = 6.0f * fWorldCellSize / 2.0f;
	input.vBeginOrigin = bHorizontal ? CVec2( 320.0f, 160.0f ) : CVec2( 96.0f, 306.0f );
	return input;
}

// Where PlanBridge's fitted start lands, map units before the truncation: the
// arithmetic the test compares with, not a written decimal (Pitfall 6).
static CVec3 FittedStart( const NMapGeometry::SBridgePlanInput &rInput, float fX, float fY )
{
	CVec3 v( fX, fY, 0.0f );
	FitVisOrigin2AIGrid( &v, rInput.vBeginOrigin );
	return v;
}

static bool TypesInOrder( const std::vector<NMapGeometry::SPlannedPiece> &rSpans )
{
	if ( rSpans.size() < 2 || rSpans.front().nPackedType != NMapGeometry::BRIDGE_SPAN_BEGIN || rSpans.back().nPackedType != NMapGeometry::BRIDGE_SPAN_END )
		return false;
	for ( size_t i = 1; i + 1 < rSpans.size(); ++i )
		if ( rSpans[i].nPackedType != NMapGeometry::BRIDGE_SPAN_CENTER )
			return false;
	for ( size_t i = 0; i < rSpans.size(); ++i )
		if ( rSpans[i].nDir != 0 || rSpans[i].vPos.z != 0.0f )
			return false;
	return true;
}

// D-10/D-03/C6: the bridge span plan, then a planned bridge laid over a map
// the way the bridge lays it (objects with the packed type, then the entry):
// saved, read back, equal to the same build; its entry erased and its spans
// deleted, in that order, write the unedited file byte for byte.
static void TestM2BridgePlan()
{
	const float fL = 6.0f * fWorldCellSize / 2.0f;
	std::vector<NMapGeometry::SPlannedPiece> spans;
	std::string szWhy;

	// Horizontal: 700 world units along x, 5 middle spans.
	const NMapGeometry::SBridgePlanInput horizontal = WoodenBigHeavyInput( true );
	if ( Check( NMapGeometry::PlanBridge( horizontal, CVec2( 1000.0f, 800.0f ), CVec2( 1700.0f, 820.0f ), &spans, &szWhy ), szWhy.c_str() ) )
	{
		const int nParts = int( 700.0f / fL );
		Check( int( spans.size() ) == nParts + 2, NStr::Format( "a 700-unit horizontal drag plans %d middle spans and the two ends (%d)", nParts, int( spans.size() ) ) );
		Check( TypesInOrder( spans ), "begin, middles, end, direction 0, z 0" );
		const CVec3 vStart = FittedStart( horizontal, 1000.0f, 800.0f );
		const float fFirstX = vStart.x * fAITileXCoeff1 + 0.3f;
		Check( spans[0].vPos.x == float( int( fFirstX ) ) - 0.1f, "the first span of a horizontal bridge is the truncated x less 0.1" );
		const float fY = float( int( vStart.y * fAITileYCoeff1 + 0.3f ) );
		bool bSameY = true;
		for ( size_t i = 0; i < spans.size(); ++i )
			bSameY = bSameY && spans[i].vPos.y == fY;
		Check( bSameY, "every span of a horizontal bridge keeps the first point's truncated y" );
		const float fEndX = ( vStart.x + float( nParts ) * fL ) * fAITileXCoeff1 + 0.3f;
		Check( spans.back().vPos.x == float( int( fEndX ) ), "the end span is n span lengths on, truncated, with no nudge" );
		const float fMidX = ( vStart.x + 0.5f * fL ) * fAITileXCoeff1 + 0.3f;
		Check( spans[1].vPos.x == float( int( fMidX ) ), "the first middle span is half a span length on" );
		// The same drag the other way round plans the same bridge.
		std::vector<NMapGeometry::SPlannedPiece> reversed;
		Check( NMapGeometry::PlanBridge( horizontal, CVec2( 1700.0f, 800.0f ), CVec2( 1000.0f, 790.0f ), &reversed, &szWhy ), szWhy.c_str() );
		bool bSame = reversed.size() == spans.size();
		for ( size_t i = 0; bSame && i < spans.size(); ++i )
			bSame = reversed[i].vPos.x == spans[i].vPos.x && reversed[i].nPackedType == spans[i].nPackedType;
		Check( bSame, "a drag right to left plans the same spans along x" );
	}

	// Vertical: the _02 variant along y.
	const NMapGeometry::SBridgePlanInput vertical = WoodenBigHeavyInput( false );
	if ( Check( NMapGeometry::PlanBridge( vertical, CVec2( 1000.0f, 800.0f ), CVec2( 990.0f, 1500.0f ), &spans, &szWhy ), szWhy.c_str() ) )
	{
		const int nParts = int( 700.0f / fL );
		Check( int( spans.size() ) == nParts + 2, "a 700-unit vertical drag plans the same count" );
		Check( TypesInOrder( spans ), "begin, middles, end along y" );
		const CVec3 vStart = FittedStart( vertical, 1000.0f, 800.0f );
		const float fLastY = ( vStart.y + float( nParts ) * fL ) * fAITileYCoeff1 + 0.3f;
		Check( spans.back().vPos.y == float( int( fLastY ) ) + 0.1f, "the last span of a vertical bridge is the truncated y plus 0.1" );
		Check( spans[0].vPos.y == float( int( vStart.y * fAITileYCoeff1 + 0.3f ) ), "and its first span has no nudge" );
		const float fX = float( int( vStart.x * fAITileXCoeff1 + 0.3f ) );
		Check( spans[0].vPos.x == fX && spans.back().vPos.x == fX, "every span keeps the first point's truncated x" );
	}

	// n = 0: a drag shorter than one span is a begin and an end span.
	if ( Check( NMapGeometry::PlanBridge( horizontal, CVec2( 1000.0f, 800.0f ), CVec2( 1000.0f + fL * 0.5f, 800.0f ), &spans, &szWhy ), szWhy.c_str() ) )
		Check( spans.size() == 2 && TypesInOrder( spans ), "a drag shorter than a span plans a begin and an end span" );
	Check( NMapGeometry::PlanBridge( horizontal, CVec2( 1000.0f, 800.0f ), CVec2( 1000.0f, 800.0f ), &spans, &szWhy ), "a click with no drag plans a bridge of either direction" );

	// Refusals.
	szWhy.clear();
	Check( !NMapGeometry::PlanBridge( horizontal, CVec2( 1000.0f, 800.0f ), CVec2( 1010.0f, 1500.0f ), &spans, &szWhy ), "a vertical drag of a horizontal bridge is refused" );
	Check( szWhy.find( "horizontally" ) != std::string::npos && spans.empty(), NStr::Format( "and says the bridge runs horizontally (%s)", szWhy.c_str() ) );
	szWhy.clear();
	Check( !NMapGeometry::PlanBridge( vertical, CVec2( 1000.0f, 800.0f ), CVec2( 1700.0f, 810.0f ), &spans, &szWhy ), "a horizontal drag of a vertical bridge is refused" );
	Check( szWhy.find( "vertically" ) != std::string::npos, "and says the bridge runs vertically" );
	NMapGeometry::SBridgePlanInput zero = horizontal;
	zero.fSpanLength = 0.0f;
	Check( !NMapGeometry::PlanBridge( zero, CVec2( 1000.0f, 800.0f ), CVec2( 1700.0f, 800.0f ), &spans, &szWhy ), "a zero span length is refused" );
	zero.fSpanLength = std::numeric_limits<float>::quiet_NaN();
	Check( !NMapGeometry::PlanBridge( zero, CVec2( 1000.0f, 800.0f ), CVec2( 1700.0f, 800.0f ), &spans, &szWhy ), "a NaN span length is refused" );
	Check( !NMapGeometry::PlanBridge( horizontal, CVec2( 1000.0f, std::numeric_limits<float>::infinity() ), CVec2( 1700.0f, 800.0f ), &spans, &szWhy ), "a non-finite point is refused" );

	// The partner names.
	Check( NMapGeometry::BridgePartnerName( "W_WoodenBig_Heavy_01" ) == "W_WoodenBig_Heavy_02", "_01's partner is _02" );
	Check( NMapGeometry::BridgePartnerName( "asphaltbridge_02" ) == "asphaltbridge_01", "_02's partner is _01, the case of the rest kept" );
	Check( NMapGeometry::BridgePartnerName( "SomeBridge" ).empty(), "a name without the suffix has no partner" );
	Check( NMapGeometry::BridgePartnerName( "SomeBridge_03" ).empty() && NMapGeometry::BridgePartnerName( "_0" ).empty(), "nor has _03 or a short name" );

	// A planned bridge over coldwinter, as the bridge lays it.
	const char *pszMap = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	std::vector<NMapGeometry::SPlannedPiece> plan;
	if ( !Check( NMapGeometry::PlanBridge( horizontal, CVec2( 1000.0f, 800.0f ), CVec2( 1700.0f, 800.0f ), &plan, &szWhy ), szWhy.c_str() ) )
		return;
	const TMapOp addBridge = [plan]( SLoadMapInfo *pMap ) -> bool
	{
		std::vector<int> linkIDs;
		for ( size_t i = 0; i < plan.size(); ++i )
		{
			NMapOverlay::SAddObject add;
			add.szName = "W_WoodenBig_Heavy_01";
			add.vPos = plan[i].vPos;
			add.nDir = plan[i].nDir;
			add.nPlayer = 0;
			add.nFrameIndex = plan[i].nPackedType;
			add.fHP = 1.0f;
			add.nScriptID = -1;
			int nLinkID = -1;
			if ( !NMapOverlay::AddObject( pMap, add, &nLinkID ) )
				return false;
			linkIDs.push_back( nLinkID );
		}
		return NMapRecords::InsertBridgeEntry( pMap, -1, linkIDs );
	};
	const TMapOp removeBridge = []( SLoadMapInfo *pMap ) -> bool
	{
		if ( pMap->bridges.empty() )
			return false;
		std::vector<int> linkIDs;
		// The entry first: a span a bridge still names is not deleted.
		if ( !NMapRecords::EraseBridgeEntry( pMap, int( pMap->bridges.size() ) - 1, &linkIDs ) )
			return false;
		for ( size_t i = linkIDs.size(); i-- > 0; )
		{
			std::string szRefusal;
			if ( !NMapOverlay::DeleteObject( pMap, linkIDs[i], &szRefusal ) )
				return false;
		}
		return true;
	};
	RunM2Case( pszMap, "bridge drawn and removed", TMapOp(), addBridge, removeBridge );
	// The saved spans hold what the plan said, packed type included.
	{
		CMapInfo edited;
		if ( ReadFresh( pszMap, &edited ) && Check( addBridge( &edited ), "the bridge lays over the map" ) )
		{
			std::string szError;
			if ( Check( NMapFile::Write( M2_EDITED, edited, &szError ), szError.c_str() ) )
			{
				CMapInfo reread;
				if ( Check( NMapFile::Read( M2_EDITED, &reread, &szError ), szError.c_str() ) && Check( !reread.bridges.empty(), "the saved map has the entry" ) )
				{
					const std::vector<int> &rEntry = reread.bridges.back();
					bool bAll = rEntry.size() == plan.size();
					for ( size_t i = 0; bAll && i < rEntry.size(); ++i )
					{
						const SMapObjectInfo *pSpan = ObjectByLinkID( reread, rEntry[i] );
						bAll = pSpan != 0 && pSpan->vPos.x == plan[i].vPos.x && pSpan->vPos.y == plan[i].vPos.y &&
						       pSpan->nFrameIndex == plan[i].nPackedType && pSpan->fHP == 1.0f && pSpan->nScriptID == -1 && pSpan->nDir == 0;
					}
					Check( bAll, "every saved span is where the plan put it, with its packed type, HP 1, no script ID" );
				}
			}
		}
	}
	RemoveM2Files();
	printf( "map-file: M2 bridge plan ok\n" );
}

// ---------------------------------------------------------------------------
// Fences (04-07, D-14)
// ---------------------------------------------------------------------------

// The origins (AI units) of W_FactoryFence's centre segment of each direction,
// as its stats give them after ToAIUnits: 11.3139 / fAITileXCoeff = 16 and
// 56.5682 / fAITileXCoeff = 80 (the engine tier reads the real stats and
// checks these are the same). Directions 0 and 2 are the one-tile-wide
// vertical segments, 1 and 3 the three-tile-wide horizontal ones.
static NMapGeometry::SFencePlanInput FactoryFenceInput( int nTiles )
{
	NMapGeometry::SFencePlanInput input;
	input.vOrigin[0] = CVec2( 16.0f, 16.0f );
	input.vOrigin[1] = CVec2( 80.0f, 16.0f );
	input.vOrigin[2] = CVec2( 16.0f, 16.0f );
	input.vOrigin[3] = CVec2( 80.0f, 16.0f );
	input.nTilesX = nTiles;
	input.nTilesY = nTiles;
	return input;
}

// A fence at an AI tile as plain arithmetic: the tile in AI units, moved to
// the segment's origin grid (round to 32 about the origin), which is what
// FitVisOrigin2AIGrid does through the vis conversion and back. The origins
// here are whole, so the truncated conversion gives the same number.
static CVec2 FenceAtTile( int nTileX, int nTileY, const CVec2 &rOrigin )
{
	return CVec2( float( int( ( float( nTileX * 32 ) - rOrigin.x ) / 32.0f + 0.5f ) ) * 32.0f + rOrigin.x,
	              float( int( ( float( nTileY * 32 ) - rOrigin.y ) / 32.0f + 0.5f ) ) * 32.0f + rOrigin.y );
}

static int PackedFence( int nDir )
{
	return ( 1 << nDir ) | 0x00010000;
}

// One run: the planned fences are these tiles, moved by (nShiftX, nShiftY),
// in this direction and no others.
static bool RunIs( const std::vector<NMapGeometry::SPlannedPiece> &rFences, const NMapGeometry::SFencePlanInput &rInput,
                   const CTPoint<int> &rFirst, int nStepX, int nStepY, int nCount, int nShiftX, int nShiftY, int nDir )
{
	if ( int( rFences.size() ) != nCount )
		return false;
	for ( int i = 0; i < nCount; ++i )
	{
		const CVec2 vWant = FenceAtTile( rFirst.x + nStepX * 2 * i + nShiftX, rFirst.y + nStepY * 2 * i + nShiftY, rInput.vOrigin[nDir] );
		if ( rFences[i].vPos.x != vWant.x || rFences[i].vPos.y != vWant.y || rFences[i].vPos.z != 0.0f ||
		     rFences[i].nPackedType != PackedFence( nDir ) || rFences[i].nDir != 0 )
			return false;
	}
	return true;
}

// D-14/C6: the fence run plan - the line rasterizer, the axis lock, one fence
// every second tile, the direction and its shift, the single fence, the
// refusals; then a planned run laid over a map as plain objects, saved, read
// back equal, deleted -> the unedited bytes.
static void TestM2FencePlan()
{
	std::vector< CTPoint<int> > tiles;
	// a_dirLine, by hand: dx 5, dy 2 steps x and bumps y at e >= 0.
	NMapGeometry::RasterizeLine( CTPoint<int>( 0, 0 ), CTPoint<int>( 5, 2 ), &tiles );
	{
		const int want[6][2] = { { 0, 0 }, { 1, 0 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 } };
		bool bSame = tiles.size() == 6;
		for ( size_t i = 0; bSame && i < 6; ++i )
			bSame = tiles[i].x == want[i][0] && tiles[i].y == want[i][1];
		Check( bSame, "RasterizeLine( 0,0 -> 5,2 ) is the six tiles a_dirLine gives" );
	}
	NMapGeometry::RasterizeLine( CTPoint<int>( 3, 3 ), CTPoint<int>( 0, 0 ), &tiles );
	Check( tiles.size() == 4 && tiles[0].x == 3 && tiles[1].x == 2 && tiles[1].y == 2 && tiles[3].x == 0 && tiles[3].y == 0, "a diagonal runs down to the end tile" );
	NMapGeometry::RasterizeLine( CTPoint<int>( 4, 4 ), CTPoint<int>( 4, 4 ), &tiles );
	Check( tiles.size() == 1 && tiles[0].x == 4 && tiles[0].y == 4, "a point is one tile" );
	NMapGeometry::RasterizeLine( CTPoint<int>( 2, 9 ), CTPoint<int>( 2, 5 ), &tiles );
	Check( tiles.size() == 5 && tiles[0].y == 9 && tiles[4].y == 5, "a vertical line steps y down" );
	NMapGeometry::RasterizeLine( CTPoint<int>( 0, 0 ), CTPoint<int>( 2, 5 ), &tiles );
	Check( tiles.size() == 6 && tiles[5].x == 2 && tiles[5].y == 5, "a steep line steps y and ends on the end tile" );

	const NMapGeometry::SFencePlanInput input = FactoryFenceInput( 128 );
	std::vector<NMapGeometry::SPlannedPiece> fences;
	std::string szWhy;

	// Right: horizontal, direction 3, the tile moved two to the right; the last
	// tile is locked to the first's row.
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 10, 20 ), CTPoint<int>( 30, 22 ), false, &fences, &szWhy ), szWhy.c_str() ) )
		Check( RunIs( fences, input, CTPoint<int>( 10, 20 ), 1, 0, 11, 2, 0, 3 ),
		       NStr::Format( "a drag to the right places 11 fences, every second tile, direction 3, moved +2 (%d planned)", int( fences.size() ) ) );
	// Left: direction 1, no shift.
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 30, 20 ), CTPoint<int>( 10, 23 ), true, &fences, &szWhy ), szWhy.c_str() ) )
		Check( RunIs( fences, input, CTPoint<int>( 30, 20 ), -1, 0, 11, 0, 0, 1 ), "a drag to the left is direction 1 with no shift, and ctrl is ignored for a run" );
	// Up (smaller y): direction 0, the tile moved two up.
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 20, 30 ), CTPoint<int>( 22, 10 ), false, &fences, &szWhy ), szWhy.c_str() ) )
		Check( RunIs( fences, input, CTPoint<int>( 20, 30 ), 0, -1, 11, 0, -2, 0 ), "a drag up is direction 0, moved -2 in y" );
	// Down: direction 2, no shift.
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 20, 10 ), CTPoint<int>( 22, 30 ), false, &fences, &szWhy ), szWhy.c_str() ) )
		Check( RunIs( fences, input, CTPoint<int>( 20, 10 ), 0, 1, 11, 0, 0, 2 ), "a drag down is direction 2 with no shift" );
	// A tie of the two deltas is horizontal (GetCurrentDirection).
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 10, 10 ), CTPoint<int>( 14, 14 ), false, &fences, &szWhy ), szWhy.c_str() ) )
		Check( RunIs( fences, input, CTPoint<int>( 10, 10 ), 1, 0, 3, 2, 0, 3 ), "a tie of the deltas is a horizontal run" );
	// Ten tiles, inclusive: five fences; eleven: six.
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 10, 20 ), CTPoint<int>( 19, 20 ), false, &fences, &szWhy ), szWhy.c_str() ) )
		Check( fences.size() == 5, "a run over 10 tiles is 5 fences" );
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 10, 20 ), CTPoint<int>( 20, 20 ), false, &fences, &szWhy ), szWhy.c_str() ) )
		Check( fences.size() == 6, "a run over 11 tiles is 6 fences" );

	// A single fence: direction 0, or 1 with ctrl; no shift, one fence.
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 15, 15 ), CTPoint<int>( 15, 15 ), false, &fences, &szWhy ), szWhy.c_str() ) )
		Check( RunIs( fences, input, CTPoint<int>( 15, 15 ), 0, 0, 1, 0, 0, 0 ), "a click is one fence, direction 0" );
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 15, 15 ), CTPoint<int>( 15, 15 ), true, &fences, &szWhy ), szWhy.c_str() ) )
		Check( RunIs( fences, input, CTPoint<int>( 15, 15 ), 0, 0, 1, 0, 0, 1 ), "a click with ctrl is one fence, direction 1" );

	// The independent arithmetic, spelled out for one fence: tile 12 in x, the
	// origin 80 in x - ( 384 - 80 ) / 32 = 9.5, rounded up to 10, 320 + 80.
	if ( Check( NMapGeometry::PlanFences( input, CTPoint<int>( 10, 20 ), CTPoint<int>( 30, 20 ), false, &fences, &szWhy ), szWhy.c_str() ) )
		Check( fences[0].vPos.x == 400.0f && fences[0].vPos.y == 656.0f, NStr::Format( "the first fence of the rightward run is at 400, 656 (%g, %g)", fences[0].vPos.x, fences[0].vPos.y ) );

	// Refusals are of the whole run and leave nothing planned.
	szWhy.clear();
	Check( !NMapGeometry::PlanFences( input, CTPoint<int>( 10, 10 ), CTPoint<int>( 200, 10 ), false, &fences, &szWhy ) && fences.empty() && szWhy.find( "leaves the map" ) != std::string::npos,
	       NStr::Format( "an end past the map is refused whole (%s)", szWhy.c_str() ) );
	Check( !NMapGeometry::PlanFences( input, CTPoint<int>( -1, 5 ), CTPoint<int>( 10, 5 ), false, &fences, &szWhy ) && fences.empty(), "a first tile at -1 is refused (its cell is -1)" );
	Check( !NMapGeometry::PlanFences( input, CTPoint<int>( 10, 5 ), CTPoint<int>( 128, 5 ), false, &fences, &szWhy ), "a tile 128 is off a 128-tile map" );
	Check( NMapGeometry::PlanFences( input, CTPoint<int>( 100, 5 ), CTPoint<int>( 125, 5 ), false, &fences, &szWhy ), "a run ending on tile 125 is on the map" );
	Check( !NMapGeometry::PlanFences( input, CTPoint<int>( 100, 5 ), CTPoint<int>( 126, 5 ), false, &fences, &szWhy ) && fences.empty(),
	       "a rightward run whose last fence is moved two tiles past the edge is refused whole" );
	Check( NMapGeometry::PlanFences( input, CTPoint<int>( 5, 100 ), CTPoint<int>( 5, 1 ), false, &fences, &szWhy ),
	       "an upward run ending on tile 1 stays on the map (its last fence moved to tile 0)" );
	Check( !NMapGeometry::PlanFences( input, CTPoint<int>( 5, 100 ), CTPoint<int>( 5, 0 ), false, &fences, &szWhy ) && fences.empty(),
	       "an upward run whose last fence is moved two tiles past the top edge is refused whole" );
	NMapGeometry::SFencePlanInput empty = input;
	empty.nTilesX = 0;
	Check( !NMapGeometry::PlanFences( empty, CTPoint<int>( 1, 1 ), CTPoint<int>( 1, 1 ), false, &fences, &szWhy ), "a map with no extent is refused" );
	NMapGeometry::SFencePlanInput bad = input;
	bad.vOrigin[2].y = std::numeric_limits<float>::quiet_NaN();
	Check( !NMapGeometry::PlanFences( bad, CTPoint<int>( 1, 1 ), CTPoint<int>( 1, 1 ), false, &fences, &szWhy ), "a non-finite origin is refused" );

	// A planned run over coldwinter, as the bridge lays it: plain objects with
	// the packed type, HP 1, no script ID, no bridges entry.
	const char *pszMap = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	std::vector<NMapGeometry::SPlannedPiece> plan;
	if ( !Check( NMapGeometry::PlanFences( input, CTPoint<int>( 40, 60 ), CTPoint<int>( 60, 60 ), false, &plan, &szWhy ), szWhy.c_str() ) )
		return;
	std::vector<int> linkIDs;
	const TMapOp addRun = [plan, &linkIDs]( SLoadMapInfo *pMap ) -> bool
	{
		linkIDs.clear();
		for ( size_t i = 0; i < plan.size(); ++i )
		{
			NMapOverlay::SAddObject add;
			add.szName = "W_FactoryFence";
			add.vPos = plan[i].vPos;
			add.nDir = plan[i].nDir;
			add.nPlayer = 0;
			add.nFrameIndex = plan[i].nPackedType;
			add.fHP = 1.0f;
			add.nScriptID = -1;
			int nLinkID = -1;
			if ( !NMapOverlay::AddObject( pMap, add, &nLinkID ) )
				return false;
			linkIDs.push_back( nLinkID );
		}
		return true;
	};
	const TMapOp removeRun = [&linkIDs]( SLoadMapInfo *pMap ) -> bool
	{
		for ( size_t i = linkIDs.size(); i-- > 0; )
		{
			std::string szRefusal;
			if ( !NMapOverlay::DeleteObject( pMap, linkIDs[i], &szRefusal ) )
				return false;
		}
		return true;
	};
	RunM2Case( pszMap, "fence run placed and removed", TMapOp(), addRun, removeRun );
	{
		CMapInfo edited;
		if ( ReadFresh( pszMap, &edited ) && Check( addRun( &edited ), "the fence run lays over the map" ) )
		{
			std::string szError;
			if ( Check( NMapFile::Write( M2_EDITED, edited, &szError ), szError.c_str() ) )
			{
				CMapInfo reread;
				if ( Check( NMapFile::Read( M2_EDITED, &reread, &szError ), szError.c_str() ) )
				{
					bool bAll = !linkIDs.empty();
					for ( size_t i = 0; bAll && i < linkIDs.size(); ++i )
					{
						const SMapObjectInfo *pFence = ObjectByLinkID( reread, linkIDs[i] );
						bAll = pFence != 0 && pFence->vPos.x == plan[i].vPos.x && pFence->vPos.y == plan[i].vPos.y &&
						       pFence->nFrameIndex == plan[i].nPackedType && pFence->fHP == 1.0f && pFence->nScriptID == -1 && pFence->nPlayer == 0;
					}
					Check( bAll, "every saved fence is where the plan put it, with its packed type, HP 1, no script ID" );
				}
			}
		}
	}
	RemoveM2Files();
	printf( "map-file: M2 fence plan ok\n" );
}

// ---------------------------------------------------------------------------
// Entrenchments (04-08, D-13)
// ---------------------------------------------------------------------------

// The shipped "Entrenchment" stats' piece lengths as the builder reads them
// (GetVisAABBHalfSize().x * 2): the line segments' half size 37.1722 and the
// arc's 11.0562 become 52 and 15 AI units in ToAIUnits (Vis2AI truncates), so
// 2 * 52 * fAITileXCoeff and 2 * 15 * fAITileXCoeff world units. The engine
// tier reads the real stats and checks it gets the same.
static NMapGeometry::STrenchPlanInput ShippedTrenchInput()
{
	NMapGeometry::STrenchPlanInput input;
	input.fLineWidth = 2.0f * 52.0f * fAITileXCoeff;
	input.fArcWidth = 2.0f * 15.0f * fAITileXCoeff;
	return input;
}

// The L the overlay and the engine tier draw: 600 world units east, then 500
// north, in the middle of a small map.
static std::vector<CVec2> TrenchL( float fX, float fY )
{
	std::vector<CVec2> points;
	points.push_back( CVec2( fX, fY ) );
	points.push_back( CVec2( fX + 600.0f, fY ) );
	points.push_back( CVec2( fX + 600.0f, fY + 500.0f ) );
	return points;
}

// The entrenchment a plan lays over a map: every piece an object of the
// "Entrenchment" type (the packed type, HP 1, no script ID), then the entry of
// the sections' link IDs appended - what the bridge's draw saves.
static bool LayTrench( SLoadMapInfo *pMap, const NMapGeometry::STrenchPlan &rPlan, int nPlayer, std::vector<int> *pLinkIDs )
{
	pLinkIDs->clear();
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
		pLinkIDs->push_back( nLinkID );
	}
	SEntrenchmentInfo entry;
	for ( size_t s = 0; s < rPlan.sections.size(); ++s )
	{
		SEntrenchmentInfo::TSegment section;
		for ( size_t k = 0; k < rPlan.sections[s].size(); ++k )
			section.push_back( (*pLinkIDs)[rPlan.sections[s][k]] );
		entry.sections.push_back( section );
	}
	return NMapRecords::InsertEntrenchment( pMap, -1, entry );
}

static void TestM2TrenchOverlay()
{
	const char *const pszMap = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	const NMapGeometry::STrenchPlanInput input = ShippedTrenchInput();
	NMapGeometry::STrenchPlan plan;
	std::string szWhy;
	if ( !Check( NMapGeometry::PlanEntrenchment( input, TrenchL( 1000.0f, 1000.0f ), &plan, &szWhy ), szWhy.c_str() ) )
		return;
	// What the L is: terminators first, a straight run east, an arc round the
	// corner and a straight run north, in at least two sections.
	int nArcs = 0, nStraight = 0;
	for ( size_t i = 2; i < plan.pieces.size(); ++i )
	{
		nArcs += plan.pieces[i].nPackedType == NMapGeometry::TRENCH_ARC ? 1 : 0;
		nStraight += plan.pieces[i].nPackedType == NMapGeometry::TRENCH_LINE || plan.pieces[i].nPackedType == NMapGeometry::TRENCH_FIREPLACE ? 1 : 0;
	}
	Check( plan.pieces.size() > 6 && plan.pieces[0].nPackedType == NMapGeometry::TRENCH_TERMINATOR &&
	       plan.pieces[1].nPackedType == NMapGeometry::TRENCH_TERMINATOR, "the L has its two terminators first and pieces after them" );
	Check( nArcs >= 2 && nStraight >= 10, NStr::Format( "the L turns through arcs and runs straight (%d arcs, %d straight)", nArcs, nStraight ) );
	Check( plan.sections.size() >= 2 && plan.sections.front().front() == 0 && plan.sections.back().back() == 1,
	       "the sections start with the begin terminator and end with the end terminator" );
	Check( plan.pieces[2].nPackedType == NMapGeometry::TRENCH_FIREPLACE && plan.pieces[3].nPackedType == NMapGeometry::TRENCH_LINE,
	       "the first straight run starts with a fireplace, then a line" );
	// The first step is due east, so the begin terminator faces west: pi.
	Check( plan.pieces[0].nDir == int( FP_PI / FP_2PI * 65535 ) && plan.pieces[2].nDir == 0,
	       NStr::Format( "the begin terminator is turned pi from the first step (%d, %d)", plan.pieces[0].nDir, plan.pieces[2].nDir ) );
	// A piece is the Vis2AI of its step's integer midpoint.
	{
		const CTPoint<int> &a = plan.path[0], &b = plan.path[1];
		CVec3 vMid( float( ( a.x + b.x ) / 2 ), float( ( a.y + b.y ) / 2 ), 0.0f );
		Vis2AI( &vMid );
		Check( plan.pieces[2].vPos.x == vMid.x && plan.pieces[2].vPos.y == vMid.y, "the first piece is at its step's midpoint, in map units" );
	}

	std::vector<int> linkIDs;
	const TMapOp addTrench = [plan, &linkIDs]( SLoadMapInfo *pMap ) -> bool { return LayTrench( pMap, plan, 1, &linkIDs ); };
	const TMapOp removeTrench = []( SLoadMapInfo *pMap ) -> bool
	{
		if ( pMap->entrenchments.empty() )
			return false;
		// The entry first: a piece a section still names is not deleted.
		SEntrenchmentInfo erased;
		if ( !NMapRecords::EraseEntrenchment( pMap, int( pMap->entrenchments.size() ) - 1, &erased ) )
			return false;
		for ( size_t s = erased.sections.size(); s-- > 0; )
			for ( size_t k = erased.sections[s].size(); k-- > 0; )
			{
				std::string szRefusal;
				if ( !NMapOverlay::DeleteObject( pMap, erased.sections[s][k], &szRefusal ) )
					return false;
			}
		return true;
	};
	RunM2Case( pszMap, "entrenchment drawn and removed", TMapOp(), addTrench, removeTrench );
	// The saved pieces hold what the plan said and the sections name them.
	{
		CMapInfo edited;
		if ( ReadFresh( pszMap, &edited ) && Check( addTrench( &edited ), "the entrenchment lays over the map" ) )
		{
			std::string szError;
			if ( Check( NMapFile::Write( M2_EDITED, edited, &szError ), szError.c_str() ) )
			{
				CMapInfo reread;
				if ( Check( NMapFile::Read( M2_EDITED, &reread, &szError ), szError.c_str() ) && Check( !reread.entrenchments.empty(), "the saved map has the entry" ) )
				{
					const SEntrenchmentInfo &rEntry = reread.entrenchments.back();
					bool bAll = rEntry.sections.size() == plan.sections.size();
					for ( size_t s = 0; bAll && s < rEntry.sections.size(); ++s )
					{
						bAll = !rEntry.sections[s].empty() && rEntry.sections[s].size() == plan.sections[s].size();
						for ( size_t k = 0; bAll && k < rEntry.sections[s].size(); ++k )
						{
							const NMapGeometry::SPlannedPiece &rPiece = plan.pieces[plan.sections[s][k]];
							const SMapObjectInfo *pPiece = ObjectByLinkID( reread, rEntry.sections[s][k] );
							bAll = pPiece != 0 && pPiece->szName == "Entrenchment" && pPiece->vPos.x == rPiece.vPos.x && pPiece->vPos.y == rPiece.vPos.y &&
							       pPiece->nDir == rPiece.nDir && pPiece->nFrameIndex == rPiece.nPackedType && pPiece->nPlayer == 1 &&
							       pPiece->fHP == 1.0f && pPiece->nScriptID == -1;
						}
					}
					Check( bAll, "every section of the saved entry names a saved piece where the plan put it, with its type, direction and player" );
				}
			}
		}
	}
	// Refusals: fewer than two distinct points, a step shorter than a piece, a
	// NaN, a width that is no width.
	{
		std::vector<CVec2> one( 1, CVec2( 100.0f, 100.0f ) );
		Check( !NMapGeometry::PlanEntrenchment( input, one, &plan, &szWhy ) && plan.pieces.empty(), "one point is refused" );
		std::vector<CVec2> same( 2, CVec2( 100.0f, 100.0f ) );
		Check( !NMapGeometry::PlanEntrenchment( input, same, &plan, &szWhy ), "the same point twice is refused" );
		std::vector<CVec2> close;
		close.push_back( CVec2( 100.0f, 100.0f ) );
		close.push_back( CVec2( 150.0f, 100.0f ) );
		Check( !NMapGeometry::PlanEntrenchment( input, close, &plan, &szWhy ) && szWhy.find( "shorter than one piece" ) != std::string::npos,
		       "two points closer than one line piece are refused" );
		std::vector<CVec2> nan = TrenchL( 100.0f, 100.0f );
		nan[1].x = std::numeric_limits<float>::quiet_NaN();
		Check( !NMapGeometry::PlanEntrenchment( input, nan, &plan, &szWhy ), "a NaN is refused" );
		NMapGeometry::STrenchPlanInput none;
		Check( !NMapGeometry::PlanEntrenchment( none, TrenchL( 100.0f, 100.0f ), &plan, &szWhy ), "no piece length is refused" );
		std::vector<CVec2> distant = TrenchL( 100.0f, 100.0f );
		distant[2].y = 5.0e7f;
		Check( !NMapGeometry::PlanEntrenchment( input, distant, &plan, &szWhy ), "a point far off every map is refused" );
		// With the map's extent given, a piece past it refuses the whole trench;
		// one inside it does not.
		NMapGeometry::STrenchPlanInput bounded = input;
		bounded.fMapWidth = bounded.fMapHeight = 2048.0f;					// 2048 map units: about 1448 world units
		Check( NMapGeometry::PlanEntrenchment( bounded, TrenchL( 100.0f, 100.0f ), &plan, &szWhy ), "an L inside a bounded map plans" );
		Check( !NMapGeometry::PlanEntrenchment( bounded, TrenchL( 1000.0f, 100.0f ), &plan, &szWhy ) && szWhy == "the trench leaves the map",
		       "an L running past the map's extent is refused" );
	}
	RemoveM2Files();
	printf( "map-file: M2 trench overlay ok\n" );
}

// The MFC editor cannot be run here for golden outputs, so the builder's
// fidelity is pinned by what its rules make true of every trench, over 500
// random polylines of 2 to 8 clicks in a 4000-unit square: a fixed-seed
// linear congruential generator written here (no C library random call), so
// the same 500 run everywhere.
struct STrenchLcg
{
	unsigned int nState;
	explicit STrenchLcg( unsigned int nSeed ) : nState( nSeed ) {  }
	unsigned int Next() { nState = nState * 1664525u + 1013904223u; return nState >> 8; }
	float Coordinate() { return float( Next() % 4000000u ) / 1000.0f; }
	int Count( int nFrom, int nTo ) { return nFrom + int( Next() % unsigned( nTo - nFrom + 1 ) ); }
};

static bool SamePlan( const NMapGeometry::STrenchPlan &a, const NMapGeometry::STrenchPlan &b )
{
	if ( a.pieces.size() != b.pieces.size() || a.sections != b.sections || a.path.size() != b.path.size() )
		return false;
	for ( size_t i = 0; i < a.path.size(); ++i )
		if ( a.path[i].x != b.path[i].x || a.path[i].y != b.path[i].y )
			return false;
	for ( size_t i = 0; i < a.pieces.size(); ++i )
		if ( a.pieces[i].vPos.x != b.pieces[i].vPos.x || a.pieces[i].vPos.y != b.pieces[i].vPos.y ||
		     a.pieces[i].nPackedType != b.pieces[i].nPackedType || a.pieces[i].nDir != b.pieces[i].nDir )
			return false;
	return true;
}

static bool IsStraight( int nType )
{
	return nType == NMapGeometry::TRENCH_LINE || nType == NMapGeometry::TRENCH_FIREPLACE;
}

static void TestM2TrenchProperties()
{
	const NMapGeometry::STrenchPlanInput input = ShippedTrenchInput();
	STrenchLcg random( 20260930u );
	const int nPolylines = 500;
	int nPlanned = 0, nShort = 0, nPieces = 0, nArcs = 0, nSections = 0, nBad = 0;
	for ( int nLine = 0; nLine < nPolylines; ++nLine )
	{
		std::vector<CVec2> clicks;
		const int nClicks = random.Count( 2, 8 );
		for ( int k = 0; k < nClicks; ++k )
		{
			const float fX = random.Coordinate();
			clicks.push_back( CVec2( fX, random.Coordinate() ) );
		}
		NMapGeometry::STrenchPlan plan;
		std::string szWhy;
		if ( !NMapGeometry::PlanEntrenchment( input, clicks, &plan, &szWhy ) )
		{
			// The only refusal a polyline inside the square may meet: its clicks
			// never made a path of two points (every later click within a piece
			// of the first).
			std::vector< CTPoint<int> > path;
			NMapGeometry::TrenchPath( input, clicks, &path, 0 );
			if ( !Check( szWhy.find( "shorter than one piece" ) != std::string::npos && path.size() < 2,
			             NStr::Format( "polyline %d is refused only for being shorter than a piece (%s)", nLine, szWhy.c_str() ) ) )
				++nBad;
			++nShort;
			continue;
		}
		++nPlanned;
		const std::vector<NMapGeometry::SPlannedPiece> &rPieces = plan.pieces;
		const std::vector< CTPoint<int> > &rPath = plan.path;
		const int n = int( rPieces.size() );
		nPieces += n;
		nSections += int( plan.sections.size() );
		bool bOk = n == int( rPath.size() ) + 1 && n >= 3;
		// The terminators: the first two made, and the ends of the trench.
		bOk = bOk && rPieces[0].nPackedType == NMapGeometry::TRENCH_TERMINATOR && rPieces[1].nPackedType == NMapGeometry::TRENCH_TERMINATOR;
		bOk = bOk && !plan.sections.empty() && !plan.sections.front().empty() && plan.sections.front().front() == 0 && plan.sections.back().back() == 1;
		// The begin terminator faces back along the first step: half a turn
		// from the step's own direction, within the two truncations.
		if ( bOk )
		{
			double fStep = atan2( double( rPath[1].y - rPath[0].y ), double( rPath[1].x - rPath[0].x ) );
			if ( fStep < 0 )
				fStep += 2.0 * 3.14159265358979323846;
			const int nStep = int( fStep / ( 2.0 * 3.14159265358979323846 ) * 65535.0 );
			const int nDiff = ( ( rPieces[0].nDir - nStep ) % 65535 + 65535 ) % 65535;
			bOk = Check( nDiff >= 32766 && nDiff <= 32769, NStr::Format( "polyline %d: the begin terminator is half a turn from the first step (%d against %d)", nLine, rPieces[0].nDir, nStep ) );
		}
		// Every piece: a direction in 0..65535; a step piece at the Vis2AI of
		// its step's integer midpoint, straight exactly when the step is longer
		// than 0.9 line pieces; straight pieces alternating fireplace, line,
		// fireplace... through the whole trench.
		int nStraightSoFar = 0;
		for ( int i = 0; bOk && i < n; ++i )
		{
			bOk = rPieces[i].nDir >= 0 && rPieces[i].nDir <= 65535;
			if ( !bOk || i < 2 )
				continue;
			const CTPoint<int> &a = rPath[i - 2], &b = rPath[i - 1];
			CVec3 vMid( float( ( a.x + b.x ) / 2 ), float( ( a.y + b.y ) / 2 ), 0.0f );
			Vis2AI( &vMid );
			bOk = rPieces[i].vPos.x == vMid.x && rPieces[i].vPos.y == vMid.y;
			const bool bLong = float( std::hypot( float( b.x - a.x ), float( b.y - a.y ) ) ) > double( input.fLineWidth ) * 0.9;
			bOk = bOk && IsStraight( rPieces[i].nPackedType ) == bLong && ( bLong || rPieces[i].nPackedType == NMapGeometry::TRENCH_ARC );
			if ( bOk && bLong )
			{
				bOk = rPieces[i].nPackedType == ( nStraightSoFar % 2 == 0 ? NMapGeometry::TRENCH_FIREPLACE : NMapGeometry::TRENCH_LINE );
				++nStraightSoFar;
			}
			nArcs += rPieces[i].nPackedType == NMapGeometry::TRENCH_ARC ? 1 : 0;
		}
		// Sections: none empty, every piece in exactly one, in trench order
		// (the begin terminator, the steps, the end terminator), and a new one
		// starts exactly at a straight piece that follows an arc.
		std::vector<int> walk, starts;
		for ( size_t sIndex = 0; bOk && sIndex < plan.sections.size(); ++sIndex )
		{
			bOk = !plan.sections[sIndex].empty();
			starts.push_back( int( walk.size() ) );
			walk.insert( walk.end(), plan.sections[sIndex].begin(), plan.sections[sIndex].end() );
		}
		if ( bOk )
		{
			std::vector<int> expectedWalk( 1, 0 );
			for ( int i = 2; i < n; ++i )
				expectedWalk.push_back( i );
			expectedWalk.push_back( 1 );
			bOk = walk == expectedWalk;
		}
		for ( size_t k = 1; bOk && k + 1 < walk.size(); ++k )
		{
			const bool bStarts = std::find( starts.begin(), starts.end(), int( k ) ) != starts.end();
			const bool bAfterArc = IsStraight( rPieces[walk[k]].nPackedType ) && rPieces[walk[k - 1]].nPackedType == NMapGeometry::TRENCH_ARC;
			bOk = bStarts == bAfterArc;
		}
		// The same clicks plan the same trench.
		NMapGeometry::STrenchPlan again;
		bOk = bOk && NMapGeometry::PlanEntrenchment( input, clicks, &again, 0 ) && SamePlan( plan, again );
		if ( !Check( bOk, NStr::Format( "polyline %d (%d clicks, %d pieces, %d sections) keeps every builder property", nLine, nClicks, n, int( plan.sections.size() ) ) ) )
			++nBad;
	}
	printf( "map-file: trench properties: %d planned, %d shorter than a piece, %d pieces, %d arcs, %d sections\n", nPlanned, nShort, nPieces, nArcs, nSections );
	Check( nPlanned >= nPolylines * 9 / 10 && nArcs > 100 && nSections > nPlanned, "the random polylines exercise turns and sections, not only straight runs" );
	if ( nBad == 0 )
		printf( "map-file: M2 trench properties ok (%d polylines)\n", nPolylines );
}

// D-19's store formulas (04-12): the MFC editor's AI general arithmetic as literal
// cases, each computed by hand from StateAIGeneral.cpp's formulas (a point stored
// relative to its parcel's centre and turned by minus the defence direction, the
// direction an arrow handle gives, the radius with its floor of 256) - never through
// the function under test - and a side holding those values through PutAIGeneralSide,
// a write and a read.
static bool Near( float fLeft, float fRight, float fTolerance )
{
	return std::fabs( fLeft - fRight ) <= fTolerance;
}

static void TestM2ParcelFormulas()
{
	// A click at world (283, 141) is AI (400, 199) (283 * sqrt 2 + 0.3 and 141 * sqrt 2 + 0.3,
	// cut); the parcel's centre is (300, 150), so the offset is (100, 49).
	const CVec2 vClick( 283.0f, 141.0f );
	const CVec2 vCentre( 300.0f, 150.0f );
	// Direction 0: no turn, the offset as it is.
	const CVec2 vStraight = NMapGeometry::ParcelPointFromVis( vClick, vCentre, 0 );
	Check( vStraight.x == 100.0f && vStraight.y == 49.0f, NStr::Format( "a point in a parcel of direction 0 is stored as the offset (100, 49), got (%.4f, %.4f)", vStraight.x, vStraight.y ) );
	// Direction 16384 is an angle of 16384 * 2 pi / 65535 = 1.5708203: the offset turned by
	// minus that, x' = x cos a + y sin a and y' = -x sin a + y cos a.
	const CVec2 vQuarter = NMapGeometry::ParcelPointFromVis( vClick, vCentre, 16384 );
	Check( Near( vQuarter.x, 48.9976f, 0.01f ) && Near( vQuarter.y, -100.0012f, 0.01f ),
	       NStr::Format( "direction 16384 stores (48.9976, -100.0012), got (%.4f, %.4f)", vQuarter.x, vQuarter.y ) );
	const CVec2 vEighth = NMapGeometry::ParcelPointFromVis( vClick, vCentre, 8192 );
	Check( Near( vEighth.x, 105.3585f, 0.01f ) && Near( vEighth.y, -36.0637f, 0.01f ),
	       NStr::Format( "direction 8192 stores (105.3585, -36.0637), got (%.4f, %.4f)", vEighth.x, vEighth.y ) );
	// The click is cut before the centre is taken away, not after: (283.4, 141.4) is the same AI point.
	const CVec2 vSame = NMapGeometry::ParcelPointFromVis( CVec2( 283.2f, 141.2f ), vCentre, 0 );
	Check( vSame.x == 100.0f && vSame.y == 49.0f, "a click is truncated by Vis2AI before the centre is taken away" );

	// The inverse for drawing: the stored point turned by the angle plus the centre, in world
	// units (AI2Vis scales by sqrt 2 / 2).
	const CVec2 vRel( 100.0f, 49.0f );
	const CVec2 vDraw0 = NMapGeometry::ParcelPointToVis( vRel, vCentre, 0 );
	Check( Near( vDraw0.x, 282.8427f, 0.01f ) && Near( vDraw0.y, 140.7142f, 0.01f ), NStr::Format( "a point drawn at direction 0 is at (282.8427, 140.7142), got (%.4f, %.4f)", vDraw0.x, vDraw0.y ) );
	const CVec2 vDraw1 = NMapGeometry::ParcelPointToVis( vRel, vCentre, 16384 );
	Check( Near( vDraw1.x, 177.4821f, 0.01f ) && Near( vDraw1.y, 176.7759f, 0.01f ), NStr::Format( "at direction 16384 it is at (177.4821, 176.7759), got (%.4f, %.4f)", vDraw1.x, vDraw1.y ) );
	const CVec2 vDraw2 = NMapGeometry::ParcelPointToVis( vRel, vCentre, 8192 );
	Check( Near( vDraw2.x, 237.6311f, 0.01f ) && Near( vDraw2.y, 180.5663f, 0.01f ), NStr::Format( "at direction 8192 it is at (237.6311, 180.5663), got (%.4f, %.4f)", vDraw2.x, vDraw2.y ) );
	// Stored and drawn are inverses: the drawn point, stored again, is the point (up to the cut).
	const CVec2 vBack = NMapGeometry::ParcelPointFromVis( vDraw2, vCentre, 8192 );
	Check( Near( vBack.x, 100.0f, 1.6f ) && Near( vBack.y, 49.0f, 1.6f ), "a point drawn and clicked again is stored where it was (to the AI unit)" );

	// The direction an arrow handle gives: the polar angle less a quarter turn, wrapped. The
	// four quadrants, the four axes, and the wrap at 2 pi.
	const CVec2 vOrigin( 1000.0f, 1000.0f );
	struct SArrowCase { float dx, dy; int nDir; };
	const SArrowCase arrows[] = {
		{ 0.0f, 100.0f, 0 },        // straight up the y axis: polar pi / 2, no turn
		{ 100.0f, 0.0f, 49151 },    // along x: polar 0, less pi / 2 is negative, plus 2 pi
		{ 0.0f, -100.0f, 32767 },   // down: polar 3 pi / 2
		{ -100.0f, 0.0f, 16383 },   // along -x: polar pi
		{ 100.0f, 100.0f, 57343 },  // first quadrant
		{ -100.0f, 100.0f, 8191 },  // second
		{ -100.0f, -100.0f, 24575 },// third
		{ 100.0f, -100.0f, 40959 }, // fourth
		{ 0.01f, 1000.0f, 65534 },  // a hair before the wrap
		{ -0.01f, 1000.0f, 0 },     // and just after it
	};
	for ( size_t i = 0; i < sizeof( arrows ) / sizeof( arrows[0] ); ++i )
	{
		const int nGot = NMapGeometry::DirectionFromArrow( vOrigin, CVec2( vOrigin.x + arrows[i].dx, vOrigin.y + arrows[i].dy ) );
		Check( nGot == arrows[i].nDir, NStr::Format( "an arrow at (%g, %g) from the centre gives direction %d, got %d", arrows[i].dx, arrows[i].dy, arrows[i].nDir, nGot ) );
	}
	// An arrow on the centre has no direction, and the MFC's -1 polar angle gives one: within one of 38721.
	const int nNone = NMapGeometry::DirectionFromArrow( vOrigin, vOrigin );
	Check( std::abs( nNone - 38721 ) <= 1, NStr::Format( "an arrow on the centre gives 38721 (the polar angle -1), got %d", nNone ) );

	// The radius: the distance, never below 256.
	Check( NMapGeometry::RadiusFromArrow( vOrigin, CVec2( 1300.0f, 1400.0f ) ) == 500.0f, "an arrow 300 x 400 away gives radius 500" );
	Check( NMapGeometry::RadiusFromArrow( vOrigin, CVec2( 1100.0f, 1000.0f ) ) == 256.0f, "an arrow nearer than 256 gives 256" );
	Check( NMapGeometry::RadiusFromArrow( vOrigin, vOrigin ) == 256.0f, "an arrow on the centre gives 256" );
	Check( NMapGeometry::RadiusFromArrow( vOrigin, CVec2( 1256.0f, 1000.0f ) ) == 256.0f, "and exactly 256 away stays 256" );
	Check( Near( NMapGeometry::RadiusFromArrow( vOrigin, CVec2( 1257.0f, 1000.0f ) ), 257.0f, 0.001f ), "and 257 away is 257" );
	Check( NMapGeometry::fParcelMinRadius == 256.0f && 4.0f * fWorldCellSize * fAITileXCoeff1 > 255.99f && 4.0f * fWorldCellSize * fAITileXCoeff1 < 256.01f,
	       "the minimum radius is four map tiles: fWorldCellSize * 4 in AI units is 256" );

	const TMapOp none;
	// A side holding those values goes through PutAIGeneralSide, a write and a read, and comes
	// back the same; put away again it saves the unedited bytes.
	const std::string szCold = "Data\\Maps\\Multiplayer\\coldwinter.bzm";
	NMapRecords::SAIGeneralSidePut before;
	RunM2Case( szCold, "AI general parcel with the formulas' values",
	           none,
	           [&]( SLoadMapInfo *p )
	           {
		           NMapRecords::GetAIGeneralSide( *p, 1, &before );
		           NMapRecords::SAIGeneralSidePut put;
		           put.nSideCount = Max( before.nSideCount, 2 );
		           put.nSide = 1;
		           put.info = before.info;
		           SAIGeneralParcelInfo parcel;
		           parcel.eType = SAIGeneralParcelInfo::EPATCH_REINFORCE;
		           parcel.vCenter = vCentre;
		           parcel.fRadius = NMapGeometry::RadiusFromArrow( vCentre, CVec2( 600.0f, 550.0f ) );
		           parcel.wDefenceDirection = NMapGeometry::DirectionFromArrow( vCentre, CVec2( 600.0f, 550.0f ) );
		           const CVec2 vPoint = NMapGeometry::ParcelPointFromVis( vClick, vCentre, parcel.wDefenceDirection );
		           parcel.reinforcePoints.push_back( SAIGeneralParcelInfo::SReinforcePointInfo( vPoint, NMapGeometry::DirectionFromArrow( vPoint, CVec2( vPoint.x, vPoint.y + 90.0f ) ) ) );
		           put.info.parcels.push_back( parcel );
		           return NMapRecords::PutAIGeneralSide( p, put );
	           },
	           [&]( SLoadMapInfo *p ) { return NMapRecords::PutAIGeneralSide( p, before ); } );
	// The radius is the distance (300 x 400 -> 500), the direction the arrow's.
	Check( NMapGeometry::RadiusFromArrow( vCentre, CVec2( 600.0f, 550.0f ) ) == 500.0f, "the round trip's radius is the arrow's distance, 500" );
	Check( NMapGeometry::DirectionFromArrow( vCentre, CVec2( 600.0f, 550.0f ) ) == 58823, "and its direction the arrow's, 58823" );
	printf( "map-file: M2 parcel formulas ok\n" );
}

static void TestRoundTrip( const std::string &szPath )
{
	CMapInfo original;
	std::string szError;
	if ( !Check( NMapFile::Read( szPath.c_str(), &original, &szError ), szError.empty() ? szPath.c_str() : szError.c_str() ) )
		return;
	const std::string szFirst = std::string( "zig-out\\local-test\\roundtrip-1" ) + Extension( szPath );
	const std::string szSecond = std::string( "zig-out\\local-test\\roundtrip-2" ) + Extension( szPath );
	if ( !Check( NMapFile::Write( szFirst.c_str(), original, &szError ), szError.c_str() ) )
		return;
	CMapInfo reread;
	szError.clear();
	if ( !Check( NMapFile::Read( szFirst.c_str(), &reread, &szError ), szError.c_str() ) )
		return;
	std::string szWhere;
	if ( !NMapFile::AreEquivalent( original, reread, &szWhere ) )
		Check( false, ( szPath + ": differs at " + szWhere ).c_str() );
	if ( !Check( NMapFile::Write( szSecond.c_str(), reread, &szError ), szError.c_str() ) )
		return;
	if ( !FilesAreIdentical( szFirst.c_str(), szSecond.c_str() ) )
	{
		// Narrow it before reporting: writing the SAME map twice isolates a
		// non-deterministic writer from a read that loses something the
		// comparator does not know to look at.
		const std::string szThird = std::string( "zig-out\\local-test\\roundtrip-3" ) + Extension( szPath );
		NMapFile::Write( szThird.c_str(), original, &szError );
		const bool bWriterDeterministic = FilesAreIdentical( szFirst.c_str(), szThird.c_str() );
		Check( false, ( szPath + ( bWriterDeterministic
		                           ? ": read+write is not the identity, and the comparator did not see it"
		                           : ": the writer is not deterministic" ) ).c_str() );
	}
}

// The CI sample: every map under Data\Maps, plus seven of the 1,696 scenario
// patches, so the tier covers scenario maps without reading 144 MB on six
// runners. The seven were taken with
//   find Data/Scenarios -name '*.bzm' | sort | awk 'NR%250==1'
// which spreads them over the seasons and patch kinds. --all sweeps
// everything; that is the local step, test-map-files-all.
static const char *g_pszScenarioSample[] = {
	"Data\\Scenarios\\Patches\\Africa\\p_settle_E_1.bzm",
	"Data\\Scenarios\\Patches\\common\\road_junc\\winter\\p_junc_gr_asph_W_1.bzm",
	"Data\\Scenarios\\Patches\\spring_Ukraine\\p_army_N_2.bzm",
	"Data\\Scenarios\\Patches\\spring_Ukraine\\p_troops_gr_sw_1_2.bzm",
	"Data\\Scenarios\\Patches\\summer_Russia\\p_ambush_gr_NS_1.bzm",
	"Data\\Scenarios\\Patches\\summer_Ukraine\\p_lg_village_a_10.bzm",
	"Data\\Scenarios\\Patches\\winter_Russia\\p_bridge_rail_n_4.bzm",
};

static void SweepMaps( bool bAll )
{
	std::vector<std::string> paths;
	CollectMaps( "Data\\Maps", true, &paths );
	if ( bAll )
		CollectMaps( "Data\\Scenarios", false, &paths );
	else
		for ( size_t i = 0; i < sizeof( g_pszScenarioSample ) / sizeof( g_pszScenarioSample[0] ); ++i )
			paths.push_back( g_pszScenarioSample[i] );
	printf( "map-file: sweeping %d maps\n", int( paths.size() ) );
	// A tier that silently swept nothing would be worse than no tier: CI checks
	// out sparsely, and Data is exactly the kind of thing that gets left out.
	if ( !Check( paths.size() >= 50, "the sweep found the shipped maps (is Data checked out?)" ) )
		return;
	const int nFailuresBefore = g_nFailures;
	for ( size_t i = 0; i < paths.size(); ++i )
		TestRoundTrip( paths[i] );
	printf( "map-file: %d of %d maps round-tripped\n",
	             int( paths.size() ) - ( g_nFailures - nFailuresBefore ), int( paths.size() ) );
}

int main( int argc, char **argv )
{
	if ( !NDataOnly::Start( argc > 1 ? argv[1] : ".", "Data" ) )
		return 1;
	printf( "map-file: sizeof(SLoadMapInfo)=%lu\n", NMapFile::LoadMapInfoSize() );
	TestReadsASmallMap();
	TestReadsXmlAndPicksTheNewer();
	TestWritesWhatItRead();
	TestRejectsAFileThatIsNotAMap();
	TestComparatorSeesADifference();
	TestRoundTripIsEquivalent();
	bool bAll = false;
	for ( int i = 1; i < argc; ++i )
		bAll = bAll || strcmp( argv[i], "--all" ) == 0;
	TestObjectOverlay();
	TestDeleteWithNoReferences();
	TestDiplomacyChange();
	TestUnknownObjectSurvives();
	TestPaint();
	TestFailedPaintChangesNothing();
	TestDeleteThenRestoreIsTheOriginal( "Data\\Maps\\Multiplayer\\coldwinter.bzm" );
	TestAddTakesAGivenLinkID( "Data\\Maps\\Multiplayer\\coldwinter.bzm" );
	TestCaptureRestoresAPaint( "Data\\Maps\\Multiplayer\\coldwinter.bzm" );
	TestPaintOnAPatchBorder();
	TestPreprocessingChangesUnpaintedTiles();
	TestCameraAnchorRecords();
	TestM2RecordOps();
	TestM2ScriptAreaConversion();
	TestM2FindReferences();
	TestM2CascadeKinds();
	TestM2VsoBuilder();
	TestM2BridgePlan();
	TestM2FencePlan();
	TestM2TrenchOverlay();
	TestM2TrenchProperties();
	TestM2ParcelFormulas();
	SweepMaps( bAll );
	if ( g_nFailures == 0 )
		printf( "map-file: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
