// test-bk-map's judge. The build runs the installed bk-map over the tracked
// maps and the obj2mod fixture into zig-out/local-test/bk-map (see addBkMap);
// this reads what it wrote back with the engine's readers:
//   - every .bzm of each swept folder (Data/Maps/Allies and Tutorial, or all
//     of Data/Maps for test-bk-map-all) went to .xml and back to .bzm under
//     <work>/<folder name>, and what came back is MapEquivalence-equal to the
//     shipped map (D-17, the M1 sweep's oracle). The .xml is judged through
//     the .bzm made from it;
//   - bytes where the format allows (default tier): from the .bzm bk-map
//     wrote, .bzm -> .xml -> .bzm and then .xml -> .bzm -> .xml give the
//     same bytes back. The shipped files' own bytes are not the yardstick,
//     for two reasons the formats leave open. A .bzm saves road and river
//     points raw (SVectorStripeObjectPoint, fmtVSO.h), and the tool that wrote
//     the shipped maps left memory in the three bytes after bKeyPoint, which
//     an .xml does not carry (588 bytes of Allies/Norway/Narwick.bzm come back
//     as zeros). And both savers write a std::unordered_map (the reinforcement
//     groups) in iteration order, which follows the order it was filled in,
//     so the first .xml of a shipped map can list them in another order than
//     the next one (16 of the 57 Data/Maps maps). Once written by today's
//     savers a map is a fixed point of both cycles;
//   - the two shipped .xml maps went to .bzm (directory mode) and one back
//     to .xml (one file), equivalent, and that .xml is the one NMapFile
//     writes for the shipped .xml;
//   - the minimap is three DDS files of the asked size whose top level holds
//     a picture, not a fill (D-18);
//   - the .mod reads back through the engine's savers as the fixture says.
// Usage: bk-map-test <module root> <Data> <work dir> <maps folder>... [--maps-only]
#include "StdAfx.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "data_only_startup.h"
#include "tool_host.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"
#include "../../Sources/src/Formats/fmtMesh.h"
#include "../../Sources/src/Image/Image.h"

namespace fs = std::filesystem;

static int g_nFailures = 0;

static bool Check( bool bCondition, const std::string &szWhat )
{
	if ( !bCondition )
	{
		printf( "FAIL: %s\n", szWhat.c_str() );
		++g_nFailures;
	}
	return bCondition;
}

static std::string Engine( const fs::path &path )
{
	return NToolHost::EnginePath( fs::absolute( path ) );
}

static bool ReadMap( const fs::path &path, CMapInfo *pMap )
{
	std::string szError;
	return Check( NMapFile::Read( Engine( path ).c_str(), pMap, &szError ), path.string() + " reads: " + szError );
}

static bool Equivalent( const CMapInfo &rLeft, const CMapInfo &rRight, const std::string &szWhat )
{
	std::string szWhere;
	return Check( NMapFile::AreEquivalent( rLeft, rRight, &szWhere ), szWhat + " is equivalent (differs at " + szWhere + ")" );
}

static bool ReadBytes( const fs::path &path, std::vector<char> *pBytes )
{
	std::ifstream in( path, std::ios::binary );
	if ( !in )
		return false;
	pBytes->assign( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
	return true;
}

static bool SameBytes( const fs::path &left, const fs::path &right, const std::string &szWhat )
{
	std::vector<char> l, r;
	if ( !Check( ReadBytes( left, &l ) && ReadBytes( right, &r ), szWhat + ": both files read" ) )
		return false;
	if ( l == r )
		return true;
	size_t i = 0;
	while ( i < l.size() && i < r.size() && l[i] == r[i] )
		++i;
	return Check( false, NStr::Format( "%s: byte identical (sizes %d and %d, first difference at %d)", szWhat.c_str(), int( l.size() ), int( r.size() ), int( i ) ) );
}

static bool WriteMap( const fs::path &path, const CMapInfo &rMap )
{
	std::error_code ec;
	fs::create_directories( path.parent_path(), ec );
	std::string szError;
	return Check( NMapFile::Write( Engine( path ).c_str(), rMap, &szError ), path.string() + " writes: " + szError );
}

// Every shipped .bzm of the folder, through .xml and back. The byte cycles
// parse each .xml once more, the slow part of the tier, so test-bk-map-all
// (bBytes false) judges equivalence only.
static void TestMapCycles( const fs::path &maps, const fs::path &work, size_t nAtLeast, bool bBytes )
{
	std::vector<fs::path> files;
	std::error_code ec;
	for ( fs::recursive_directory_iterator it( maps, ec ), end; !ec && it != end; it.increment( ec ) )
		if ( it->is_regular_file( ec ) && NStr::CompareAsciiNoCase( it->path().extension().string().c_str(), ".bzm" ) == 0 )
			files.push_back( it->path() );
	std::sort( files.begin(), files.end() );
	Check( files.size() >= nAtLeast, NStr::Format( "%s holds the shipped maps (%d .bzm)", maps.string().c_str(), int( files.size() ) ) );
	const fs::path folder = work / maps.filename();
	int nPassed = 0;
	for ( size_t i = 0; i < files.size(); ++i )
	{
		const fs::path relative = files[i].lexically_relative( maps );
		const fs::path bzm = folder / "bzm" / relative;
		const int nFailures = g_nFailures;
		CMapInfo shipped, fromBzm;
		if ( !ReadMap( files[i], &shipped ) || !ReadMap( bzm, &fromBzm ) )
			continue;
		Equivalent( shipped, fromBzm, relative.string() + " back from .xml" );
		if ( bBytes )
		{
			const fs::path xml = fs::path( folder / "cycle" / relative ).replace_extension( ".xml" );
			const fs::path bzmAgain = folder / "cycle" / relative;
			const fs::path xmlAgain = fs::path( folder / "cycle" / relative ).replace_extension( ".again.xml" );
			CMapInfo fromXml, fromBzmAgain;
			if ( WriteMap( xml, fromBzm ) && ReadMap( xml, &fromXml ) && WriteMap( bzmAgain, fromXml ) )
				SameBytes( bzmAgain, bzm, relative.string() + " .bzm -> .xml -> .bzm" );
			if ( ReadMap( bzmAgain, &fromBzmAgain ) && WriteMap( xmlAgain, fromBzmAgain ) )
				SameBytes( xmlAgain, xml, relative.string() + " .xml -> .bzm -> .xml" );
		}
		if ( g_nFailures == nFailures )
			++nPassed;
	}
	printf( "bk-map-test: %d of %d shipped maps of %s come back from .xml equivalent%s\n", nPassed, int( files.size() ),
	        maps.string().c_str(), bBytes ? ", and both cycles are byte-identical" : "" );
}

// The two .xml maps Data/Maps ships, to .bzm in directory mode, one back.
static void TestShippedXml( const fs::path &data, const fs::path &work )
{
	const char *const names[2] = { "river3d", "road3d" };
	for ( int i = 0; i < 2; ++i )
	{
		CMapInfo shipped, bzm;
		if ( !ReadMap( data / "Maps" / ( std::string( names[i] ) + ".xml" ), &shipped ) ||
		     !ReadMap( work / "shipped-xml" / ( std::string( names[i] ) + ".bzm" ), &bzm ) )
			continue;
		Equivalent( shipped, bzm, std::string( names[i] ) + ".xml as .bzm" );
	}
	CMapInfo shipped, back;
	if ( ReadMap( data / "Maps" / "river3d.xml", &shipped ) && ReadMap( work / "shipped-xml" / "back" / "river3d.xml", &back ) )
	{
		Equivalent( shipped, back, "river3d .xml -> .bzm -> .xml" );
		const fs::path direct = work / "direct" / "river3d.xml";
		if ( WriteMap( direct, shipped ) )
			SameBytes( direct, work / "shipped-xml" / "back" / "river3d.xml", "river3d .xml -> .bzm -> .xml" );
	}
	printf( "bk-map-test: the shipped .xml maps convert to .bzm and back\n" );
}

static CPtr<IDDSImage> LoadDds( const fs::path &path )
{
	CPtr<IDataStream> pStream = OpenFileStream( Engine( path ).c_str(), STREAM_ACCESS_READ );
	return pStream != 0 ? GetImageProcessor()->LoadDDSImage( pStream ) : 0;
}

static void TestMinimap( const fs::path &work )
{
	const char *const suffixes[3] = { "_c.dds", "_l.dds", "_h.dds" };
	for ( int i = 0; i < 3; ++i )
	{
		const fs::path file = work / "minimap" / ( std::string( "coldwinter" ) + suffixes[i] );
		CPtr<IDDSImage> pDds = LoadDds( file );
		if ( !Check( pDds != 0, file.string() + " decodes as DDS" ) )
			continue;
		Check( pDds->GetSizeX() == 128 && pDds->GetSizeY() == 128, NStr::Format( "%s is 128x128 (%dx%d)", file.string().c_str(), pDds->GetSizeX(), pDds->GetSizeY() ) );
		if ( i != 2 )
			continue;
		// The lossless level, measured: a minimap of a real map is terrain,
		// roads and forest, so many colours and no empty pixels.
		CPtr<IImage> pImage = GetImageProcessor()->Decompress( pDds );
		if ( !Check( pImage != 0 && pImage->GetSizeX() == 128 && pImage->GetSizeY() == 128, "the _h level decompresses at 128x128" ) )
			continue;
		std::set<DWORD> colours;
		int nBlank = 0;
		const SColor *pPixels = pImage->GetLFB();
		for ( int n = 0; n < 128 * 128; ++n )
		{
			const DWORD dwColour = DWORD( pPixels[n] );
			colours.insert( dwColour & 0x00FFFFFF );
			nBlank += ( dwColour & 0x00FFFFFF ) == 0;
		}
		printf( "bk-map-test: minimap _h: %d distinct colours, %d black pixels of %d\n", int( colours.size() ), nBlank, 128 * 128 );
		Check( colours.size() >= 64, "the minimap holds a picture, not a fill" );
		Check( nBlank == 0, "no minimap pixel is left black" );
	}
}

static bool Near( float fLeft, float fRight )
{
	return fabs( fLeft - fRight ) < 1e-5f;
}

// tank.obj (one quad), tank.skeleton.txt (Root and a Turret locator) and
// tank.anim.txt (Idle, two keys), as the engine reads the .mod.
static void TestMod( const fs::path &work )
{
	SSkeletonFormat skeleton;
	std::vector<SMeshFormat> meshes;
	std::vector<SAnimationFormat> animations;
	SAABBFormat aabb;
	{
		CPtr<IDataStream> pStream = OpenFileStream( Engine( work / "obj" / "tank.mod" ).c_str(), STREAM_ACCESS_READ );
		if ( !Check( pStream != 0, "tank.mod opens" ) )
			return;
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
		CSaverAccessor saver = pSaver;
		saver.Add( 1, &skeleton );
		saver.Add( 2, &meshes );
		saver.Add( 3, &animations );
		saver.Add( 4, &aabb );
	}
	Check( skeleton.GetNumNodes() == 2 && skeleton.nTopNode == 0, "the skeleton is two nodes under node 0" );
	if ( skeleton.GetNumNodes() == 2 )
	{
		Check( skeleton.nodes[0].szName == "Root" && skeleton.nodes[1].szName == "Turret", "the nodes keep their names" );
		Check( skeleton.nodes[0].children.size() == 1 && skeleton.nodes[0].children[0] == 1, "the turret is the root's child" );
		Check( Near( skeleton.nodes[1].bone.z, 1.0f ) && Near( skeleton.nodes[1].quat.w, 1.0f ), "the turret's bone and rotation" );
		Check( skeleton.locators.size() == 1 && skeleton.locators[0] == 1, "the turret is the one locator" );
	}
	if ( Check( meshes.size() == 1, "one mesh" ) )
	{
		const SMeshFormat &mesh = meshes[0];
		Check( mesh.geoms.size() == 4 && mesh.components.size() == 4 && mesh.indices.size() == 6, "the quad is four vertices in two triangles" );
		Check( mesh.indices.size() == 6 && mesh.indices[3] == 0 && mesh.indices[4] == 2 && mesh.indices[5] == 3, "the quad is fanned from its first vertex" );
		Check( mesh.texes.size() == 4 && Near( mesh.texes[2].x, 1.0f ) && Near( mesh.texes[2].y, 0.0f ), "texture v is flipped" );
		Check( Near( mesh.aabb.vCenter.x, 0.5f ) && Near( mesh.aabb.vHalfSize.y, 0.5f ) && Near( mesh.aabb.vHalfSize.z, 0.0f ), "the bounding box" );
		Check( Near( aabb.vCenter.y, 0.5f ), "the model box is the mesh's" );
	}
	if ( Check( animations.size() == 1, "one animation" ) )
	{
		const SAnimationFormat &anim = animations[0];
		// SAnimationFormat's reader lowercases the name (fmtMesh.cpp).
		Check( anim.szName == "idle" && anim.nAction == 1 && anim.nodes.GetSizeX() == 2 && anim.nodes.GetSizeY() == 2,
		       NStr::Format( "Idle: two keys over two nodes, action key 1 (%s: %d keys, %d nodes, action %d)", anim.szName.c_str(), anim.nodes.GetSizeX(), anim.nodes.GetSizeY(), anim.nAction ) );
		if ( anim.nodes.GetSizeX() == 2 && anim.nodes.GetSizeY() == 2 )
		{
			Check( Near( anim.nodes[1][1].vPos.z, 0.5f ) && Near( anim.nodes[1][1].vRot.w, 0.7071068f ), "the turret's second key" );
			Check( Near( anim.nodes[0][1].vRot.w, 1.0f ), "a node without keys stays at rest" );
		}
	}
	printf( "bk-map-test: tank.mod reads back as the fixture says\n" );
}

int main( int argc, char *argv[] )
{
	const bool bMapsOnly = argc > 5 && strcmp( argv[argc - 1], "--maps-only" ) == 0;
	if ( argc < 5 || ( bMapsOnly && argc < 6 ) )
	{
		printf( "usage: bk-map-test <module root> <Data> <work dir> <maps folder>... [--maps-only]\n" );
		return 2;
	}
	// The storage too: a map's roads and rivers read their descriptions
	// through it while the map is read.
	const fs::path data = argv[2], work = argv[3];
	if ( !NDataOnly::Start( argv[1], NToolHost::EnginePath( fs::absolute( data ) ).c_str() ) || !NDataOnly::StartImage( argv[1] ) )
		return 1;
	for ( int i = 4; i < ( bMapsOnly ? argc - 1 : argc ); ++i )
		TestMapCycles( argv[i], work, bMapsOnly ? 50 : 1, !bMapsOnly );
	if ( !bMapsOnly )
	{
		TestShippedXml( data, work );
		TestMinimap( work );
		TestMod( work );
	}
	NDataOnly::Stop();
	if ( g_nFailures != 0 )
	{
		printf( "bk-map-test: %d failures\n", g_nFailures );
		return 1;
	}
	printf( "bk-map-test: ok\n" );
	return 0;
}
