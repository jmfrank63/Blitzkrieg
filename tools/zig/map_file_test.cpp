// The map file tier. Runs with no window and no GPU device: see
// tools/zig/data_only_startup.cpp for what "no window" costs.
#include "StdAfx.h"
#include "data_only_startup.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
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
	TestComparatorSeesADifference();
	TestRoundTripIsEquivalent();
	bool bAll = false;
	for ( int i = 1; i < argc; ++i )
		bAll = bAll || strcmp( argv[i], "--all" ) == 0;
	SweepMaps( bAll );
	if ( g_nFailures == 0 )
		printf( "map-file: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
