// The map file tier. Runs with no window and no GPU device: see
// tools/zig/data_only_startup.cpp for what "no window" costs.
#include "StdAfx.h"
#include <cstdio>
#include <cstring>
#include <string>
#include "data_only_startup.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"

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

int main( int argc, char **argv )
{
	if ( !NDataOnly::Start( argc > 1 ? argv[1] : ".", "Data" ) )
		return 1;
	std::printf( "map-file: sizeof(SLoadMapInfo)=%lu\n", NMapFile::LoadMapInfoSize() );
	TestReadsASmallMap();
	TestReadsXmlAndPicksTheNewer();
	TestWritesWhatItRead();
	TestComparatorSeesADifference();
	TestRoundTripIsEquivalent();
	if ( g_nFailures == 0 )
		std::printf( "map-file: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
