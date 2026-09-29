// The map file tier. Runs with no window and no GPU device: see
// tools/zig/data_only_startup.cpp for what "no window" costs.
#include "StdAfx.h"
#include <functional>
#include <limits>
#include <map>
#include "data_only_startup.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
#include "../../Sources/src/MapFile/MapOverlay.h"
#include "../../Sources/src/MapFile/MapRecords.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"

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

// Read a map, write it untouched, read it back: equivalent. Then write it a
// second time and compare the two files byte for byte - the spec's idempotent
// save. A map that survives both has not been quietly normalised. The bytes of
// the shipped file are deliberately not the yardstick: a rewrite is not
// byte-identical with whatever tool wrote the original, only field-equivalent.
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
	SweepMaps( bAll );
	if ( g_nFailures == 0 )
		printf( "map-file: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
