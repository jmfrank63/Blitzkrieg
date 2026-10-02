// The composer round trip (M3, 05-09, D-07/D-40.4, first half): every shipped
// container and graph - 404 and 102 in the shipped data - is read through the
// editor's own composer path (BkEditorRmgReadContainer / BkEditorRmgReadGraph, the
// engine's SRMContainer / SRMGraph serialisers behind the record structs), written
// back under a scratch name in the scratch user RMG root, read again and compared:
//
//   1. what the bridge reads is what the engine's own LoadDataResource reads
//      (the record path loses nothing);
//   2. what is written reads back equal (the write path loses nothing - the bridge
//      also reads every write back before it says OK);
//   3. writing what was read back writes the SAME BYTES (the serialisers reach a
//      fixed point, so a composer Save is stable).
//
// The data-only tier: nothing here needs a map or a picture, only the engine's
// storage, which the bridge's start provides. It needs a hidden SDL window and a
// GPU device like the engine tier, and skips honestly where there is none.
//
// argv: <installation> <scratch>
#include "StdAfx.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "../../Sources/src/EditorBridge/bridge.h"
#include "../../Sources/src/Platform/Paths.h"
#include "../../Sources/src/RandomMapGen/RMG_Types.h"
#include "../../Sources/src/RandomMapGen/Resource_Types.h"

#if defined(_WIN32) || defined(_WIN64)
#include <crtdbg.h>
#endif

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

static std::string DirectoryOf( const char *pszPath )
{
	const std::string szPath( pszPath );
	const std::string::size_type nCut = szPath.find_last_of( "/\\" );
	return nCut == std::string::npos ? std::string( "." ) : szPath.substr( 0, nCut );
}

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

static std::vector<std::string> ListNames( BkEditorSession *pSession, int nKind )
{
	std::vector<std::string> names;
	int nTotal = 0;
	BkEditorListRmg( pSession, nKind, 0, 0, &nTotal );
	if ( nTotal <= 0 )
		return names;
	std::vector<BkEditorRmgName> entries( static_cast<size_t>( nTotal ) );
	int nGot = 0;
	if ( BkEditorListRmg( pSession, nKind, &( entries[0] ), nTotal, &nGot ) != BK_EDITOR_OK )
		return names;
	for ( int i = 0; i < nGot && i < nTotal; ++i )
		names.push_back( entries[static_cast<size_t>( i )].name );
	return names;
}

static bool FileBytes( const std::string &szPath, std::vector<char> *pBytes )
{
	std::ifstream file( szPath.c_str(), std::ios::binary );
	if ( !file )
		return false;
	pBytes->assign( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	return true;
}

// The user RMG root's file for a storage name, in the host's separators.
static std::string RmgFile( const std::filesystem::path &rUserRoot, std::string szName )
{
	for ( char &c : szName )
		if ( c == '\\' )
			c = '/';
	return ( rUserRoot / "rmg" / ( szName + ".xml" ) ).string();
}

static bool FloatsEqual( float fLeft, float fRight )
{
	return fabsf( fLeft - fRight ) <= 1e-5f * ( fabsf( fLeft ) > fabsf( fRight ) ? ( fabsf( fLeft ) > 1.0f ? fabsf( fLeft ) : 1.0f ) : ( fabsf( fRight ) > 1.0f ? fabsf( fRight ) : 1.0f ) );
}

// --- Containers -------------------------------------------------------------

struct SContainerBuf
{
	BkEditorRmgContainerRecord record;
	std::vector<BkEditorRmgPatch> patches;
	std::vector<int> indices, ids;
	std::vector<BkEditorRmgName> areas;
	SContainerBuf() { memset( &record, 0, sizeof record ); }
	void Bind()
	{
		record.patches = &patches[0];
		record.indices = &indices[0];
		record.scripts.ids = &ids[0];
		record.scripts.areas = &areas[0];
	}
};

// The two passes: size with empty arrays, then exactly what was answered.
static bool ReadContainer( BkEditorSession *pSession, const std::string &szName, SContainerBuf *pOut )
{
	*pOut = SContainerBuf();
	BkEditorStatus status = BkEditorRmgReadContainer( pSession, szName.c_str(), &pOut->record );
	if ( status == BK_EDITOR_OK )
		return true;
	const BkEditorRmgContainerRecord &r = pOut->record;
	const int nIndexTotal = r.index_counts[0] + r.index_counts[1] + r.index_counts[2] + r.index_counts[3];
	if ( status != BK_EDITOR_REFUSED || ( r.patch_count == 0 && nIndexTotal == 0 && r.scripts.id_count == 0 && r.scripts.area_count == 0 ) )
		return false;
	const int nPatches = r.patch_count, nIds = r.scripts.id_count, nAreas = r.scripts.area_count;
	pOut->patches.resize( size_t( nPatches > 0 ? nPatches : 1 ) );
	pOut->indices.resize( size_t( nIndexTotal > 0 ? nIndexTotal : 1 ) );
	pOut->ids.resize( size_t( nIds > 0 ? nIds : 1 ) );
	pOut->areas.resize( size_t( nAreas > 0 ? nAreas : 1 ) );
	pOut->Bind();
	pOut->record.patch_capacity = nPatches;
	pOut->record.index_capacity = nIndexTotal;
	pOut->record.scripts.id_capacity = nIds;
	pOut->record.scripts.area_capacity = nAreas;
	status = BkEditorRmgReadContainer( pSession, szName.c_str(), &pOut->record );
	return status == BK_EDITOR_OK && pOut->record.patch_count == nPatches && pOut->record.scripts.id_count == nIds && pOut->record.scripts.area_count == nAreas;
}

static bool SameContainerRecords( const SContainerBuf &rA, const SContainerBuf &rB )
{
	const BkEditorRmgContainerRecord &a = rA.record;
	const BkEditorRmgContainerRecord &b = rB.record;
	if ( a.size_x != b.size_x || a.size_y != b.size_y || a.season != b.season || strcmp( a.season_folder, b.season_folder ) != 0 || a.patch_count != b.patch_count ||
	     a.scripts.id_count != b.scripts.id_count || a.scripts.area_count != b.scripts.area_count )
		return false;
	for ( int d = 0; d < 4; ++d )
		if ( a.index_counts[d] != b.index_counts[d] )
			return false;
	for ( int i = 0; i < a.patch_count; ++i )
		if ( strcmp( a.patches[i].name, b.patches[i].name ) != 0 || strcmp( a.patches[i].place, b.patches[i].place ) != 0 || a.patches[i].size_x != b.patches[i].size_x || a.patches[i].size_y != b.patches[i].size_y )
			return false;
	const int nIndices = a.index_counts[0] + a.index_counts[1] + a.index_counts[2] + a.index_counts[3];
	for ( int i = 0; i < nIndices; ++i )
		if ( a.indices[i] != b.indices[i] )
			return false;
	for ( int i = 0; i < a.scripts.id_count; ++i )
		if ( a.scripts.ids[i] != b.scripts.ids[i] )
			return false;
	for ( int i = 0; i < a.scripts.area_count; ++i )
		if ( strcmp( a.scripts.areas[i].name, b.scripts.areas[i].name ) != 0 )
			return false;
	return true;
}

// The bridge's record against the engine's own load of the same file.
static bool ContainerIsEngines( const SContainerBuf &rBuf, const SRMContainer &rC )
{
	const BkEditorRmgContainerRecord &r = rBuf.record;
	if ( r.patch_count != int( rC.patches.size() ) || r.size_x != rC.size.x || r.size_y != rC.size.y || r.season != rC.nSeason || std::string( r.season_folder ) != rC.szSeasonFolder ||
	     r.scripts.id_count != int( rC.usedScriptIDs.size() ) || r.scripts.area_count != int( rC.usedScriptAreas.size() ) )
		return false;
	for ( int i = 0; i < r.patch_count; ++i )
		if ( rC.patches[size_t( i )].szFileName != r.patches[i].name || rC.patches[size_t( i )].szPlace != r.patches[i].place ||
		     rC.patches[size_t( i )].size.x != r.patches[i].size_x || rC.patches[size_t( i )].size.y != r.patches[i].size_y )
			return false;
	int nAt = 0;
	for ( int d = 0; d < 4; ++d )
	{
		if ( r.index_counts[d] != int( rC.indices[d].size() ) )
			return false;
		for ( int i = 0; i < r.index_counts[d]; ++i )
			if ( r.indices[nAt++] != rC.indices[d][size_t( i )] )
				return false;
	}
	int nId = 0;
	for ( CUsedScriptIDs::const_iterator it = rC.usedScriptIDs.begin(); it != rC.usedScriptIDs.end(); ++it )
		if ( r.scripts.ids[nId++] != *it )
			return false;
	int nArea = 0;
	for ( CUsedScriptAreas::const_iterator it = rC.usedScriptAreas.begin(); it != rC.usedScriptAreas.end(); ++it )
		if ( *it != r.scripts.areas[nArea++].name )
			return false;
	return true;
}

// --- Graphs -----------------------------------------------------------------

struct SGraphBuf
{
	BkEditorRmgGraphRecord record;
	std::vector<BkEditorRmgNode> nodes;
	std::vector<BkEditorRmgLink> links;
	std::vector<int> ids;
	std::vector<BkEditorRmgName> areas;
	SGraphBuf() { memset( &record, 0, sizeof record ); }
	void Bind()
	{
		record.nodes = &nodes[0];
		record.links = &links[0];
		record.scripts.ids = &ids[0];
		record.scripts.areas = &areas[0];
	}
};

static bool ReadGraph( BkEditorSession *pSession, const std::string &szName, SGraphBuf *pOut )
{
	*pOut = SGraphBuf();
	BkEditorStatus status = BkEditorRmgReadGraph( pSession, szName.c_str(), &pOut->record );
	if ( status == BK_EDITOR_OK )
		return true;
	const BkEditorRmgGraphRecord &r = pOut->record;
	if ( status != BK_EDITOR_REFUSED || ( r.node_count == 0 && r.link_count == 0 && r.scripts.id_count == 0 && r.scripts.area_count == 0 ) )
		return false;
	const int nNodes = r.node_count, nLinks = r.link_count, nIds = r.scripts.id_count, nAreas = r.scripts.area_count;
	pOut->nodes.resize( size_t( nNodes > 0 ? nNodes : 1 ) );
	pOut->links.resize( size_t( nLinks > 0 ? nLinks : 1 ) );
	pOut->ids.resize( size_t( nIds > 0 ? nIds : 1 ) );
	pOut->areas.resize( size_t( nAreas > 0 ? nAreas : 1 ) );
	pOut->Bind();
	pOut->record.node_capacity = nNodes;
	pOut->record.link_capacity = nLinks;
	pOut->record.scripts.id_capacity = nIds;
	pOut->record.scripts.area_capacity = nAreas;
	status = BkEditorRmgReadGraph( pSession, szName.c_str(), &pOut->record );
	return status == BK_EDITOR_OK && pOut->record.node_count == nNodes && pOut->record.link_count == nLinks;
}

static bool SameGraphRecords( const SGraphBuf &rA, const SGraphBuf &rB )
{
	const BkEditorRmgGraphRecord &a = rA.record;
	const BkEditorRmgGraphRecord &b = rB.record;
	if ( a.size_x != b.size_x || a.size_y != b.size_y || a.season != b.season || strcmp( a.season_folder, b.season_folder ) != 0 || a.node_count != b.node_count || a.link_count != b.link_count ||
	     a.scripts.id_count != b.scripts.id_count || a.scripts.area_count != b.scripts.area_count )
		return false;
	for ( int i = 0; i < a.node_count; ++i )
		if ( a.nodes[i].x1 != b.nodes[i].x1 || a.nodes[i].y1 != b.nodes[i].y1 || a.nodes[i].x2 != b.nodes[i].x2 || a.nodes[i].y2 != b.nodes[i].y2 || strcmp( a.nodes[i].container, b.nodes[i].container ) != 0 )
			return false;
	for ( int i = 0; i < a.link_count; ++i )
		if ( a.links[i].a != b.links[i].a || a.links[i].b != b.links[i].b || a.links[i].type != b.links[i].type || a.links[i].parts != b.links[i].parts || strcmp( a.links[i].desc, b.links[i].desc ) != 0 ||
		     !FloatsEqual( a.links[i].radius, b.links[i].radius ) || !FloatsEqual( a.links[i].min_length, b.links[i].min_length ) ||
		     !FloatsEqual( a.links[i].distance, b.links[i].distance ) || !FloatsEqual( a.links[i].disturbance, b.links[i].disturbance ) )
			return false;
	for ( int i = 0; i < a.scripts.id_count; ++i )
		if ( a.scripts.ids[i] != b.scripts.ids[i] )
			return false;
	for ( int i = 0; i < a.scripts.area_count; ++i )
		if ( strcmp( a.scripts.areas[i].name, b.scripts.areas[i].name ) != 0 )
			return false;
	return true;
}

static bool GraphIsEngines( const SGraphBuf &rBuf, const SRMGraph &rG )
{
	const BkEditorRmgGraphRecord &r = rBuf.record;
	if ( r.node_count != int( rG.nodes.size() ) || r.link_count != int( rG.links.size() ) || r.size_x != rG.size.x || r.size_y != rG.size.y || r.season != rG.nSeason ||
	     std::string( r.season_folder ) != rG.szSeasonFolder || r.scripts.id_count != int( rG.usedScriptIDs.size() ) || r.scripts.area_count != int( rG.usedScriptAreas.size() ) )
		return false;
	for ( int i = 0; i < r.node_count; ++i )
	{
		const SRMGraphNode &n = rG.nodes[size_t( i )];
		if ( r.nodes[i].x1 != n.rect.minx || r.nodes[i].y1 != n.rect.miny || r.nodes[i].x2 != n.rect.maxx || r.nodes[i].y2 != n.rect.maxy || n.szContainerFileName != r.nodes[i].container )
			return false;
	}
	for ( int i = 0; i < r.link_count; ++i )
	{
		const SRMGraphLink &l = rG.links[size_t( i )];
		if ( r.links[i].a != l.link.a || r.links[i].b != l.link.b || r.links[i].type != l.nType || l.szDescFileName != r.links[i].desc || r.links[i].parts != l.nParts ||
		     r.links[i].radius != l.fRadius || r.links[i].min_length != l.fMinLength || r.links[i].distance != l.fDistance || r.links[i].disturbance != l.fDisturbance )
			return false;
	}
	int nId = 0;
	for ( CUsedScriptIDs::const_iterator it = rG.usedScriptIDs.begin(); it != rG.usedScriptIDs.end(); ++it )
		if ( r.scripts.ids[nId++] != *it )
			return false;
	int nArea = 0;
	for ( CUsedScriptAreas::const_iterator it = rG.usedScriptAreas.begin(); it != rG.usedScriptAreas.end(); ++it )
		if ( *it != r.scripts.areas[nArea++].name )
			return false;
	return true;
}

// The two files hold the same bytes (read whole, fresh from disk).
static bool SameFileBytes( const std::string &szLeft, const std::string &szRight )
{
	std::vector<char> left, right;
	return FileBytes( szLeft, &left ) && FileBytes( szRight, &right ) && left == right;
}

int main( int argc, char **argv )
{
#if defined(_WIN32) || defined(_WIN64)
	_set_error_mode( _OUT_TO_STDERR );
	_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
#endif
	if ( !SDL_Init( SDL_INIT_VIDEO ) )
	{
		const char *pszError = SDL_GetError();
		if ( strstr( pszError, "video driver" ) != 0 || strstr( pszError, "No available" ) != 0 )
		{
			printf( "composer-roundtrip: skipped: no video driver (%s)\n", pszError );
			return 0;
		}
		printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	SDL_Window *pWindow = SDL_CreateWindow( "composer-roundtrip-test", 640, 480, SDL_WINDOW_HIDDEN );
	if ( pWindow == 0 )
	{
		printf( "FAIL: SDL_CreateWindow: %s\n", SDL_GetError() );
		SDL_Quit();
		return 1;
	}
	const std::string szSelfDir = DirectoryOf( argv[0] != 0 ? argv[0] : "." );
	const char *pszRoot = argc > 1 ? argv[1] : szSelfDir.c_str();
	const std::filesystem::path scratch = argc > 2 ? argv[2] : szSelfDir;
	std::filesystem::create_directories( scratch );
	if ( !std::filesystem::exists( std::string( pszRoot ) + "/Data/consts.xml" ) )
	{
		printf( "composer-roundtrip: skipped: no staged game at %s (run: zig build install-game)\n", pszRoot );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
	}
	if ( !Check( SamePath( szSelfDir.c_str(), pszRoot ), "the executable lives in the installation it tests" ) )
	{
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 1;
	}

	BkEditorSession *pSession = 0;
	const BkEditorStatus status = BkEditorStart( pWindow, pszRoot, &pSession );
	if ( status == BK_EDITOR_NO_DEVICE )
	{
		printf( "composer-roundtrip: skipped: no GPU device (%s)\n", BkEditorLastMessage( pSession ) );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
	}
	if ( Check( status == BK_EDITOR_OK, std::string( "the engine starts: " ) + BkEditorLastMessage( pSession ) ) )
	{
		const std::string szBase = NPlatform::Paths::BaseRoot();
		const std::string szOriginalUser = NPlatform::Paths::UserRoot();
		const std::filesystem::path root = scratch / "composer-roundtrip";
		const std::filesystem::path user = root / "user";
		std::error_code error;
		std::filesystem::remove_all( root, error );
		const std::string szUser = user.string() + "/";
		NPlatform::Paths::SetInjectedRootsForTest( szBase.c_str(), szUser.c_str() );

		// The shipped files come from the folder scan (D-08), taken before anything
		// is written under the user root.
		const std::vector<std::string> containers = ListNames( pSession, 3 );
		const std::vector<std::string> graphs = ListNames( pSession, 2 );
		Check( !containers.empty() && !graphs.empty(), "the scan finds containers and graphs" );
		int nContainersOk = 0, nGraphsOk = 0;
		for ( size_t i = 0; i < containers.size(); ++i )
		{
			const std::string &szName = containers[i];
			char szCopy[64], szAgain[64];
			sprintf( szCopy, "scenarios\\containers\\roundtrip\\c%04d", int( i ) );
			sprintf( szAgain, "scenarios\\containers\\roundtrip\\c%04d_b", int( i ) );
			SContainerBuf first, second, third;
			SRMContainer direct;
			bool bOk = Check( LoadDataResource( szName, "", false, 0, RMGC_CONTAINER_XML_NAME, direct ), szName + ": the engine loads it" ) &&
			           Check( ReadContainer( pSession, szName, &first ), szName + ": the bridge reads it (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( ContainerIsEngines( first, direct ), szName + ": the bridge's record is the engine's own load" ) &&
			           Check( BkEditorRmgWriteContainer( pSession, szCopy, &first.record ) == BK_EDITOR_OK, szName + ": writes under a scratch name (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( ReadContainer( pSession, szCopy, &second ), szName + ": the copy reads back" ) &&
			           Check( SameContainerRecords( first, second ), szName + ": the copy is the original, field for field" ) &&
			           Check( BkEditorRmgWriteContainer( pSession, szAgain, &second.record ) == BK_EDITOR_OK, szName + ": the copy writes again" ) &&
			           Check( ReadContainer( pSession, szAgain, &third ), szName + ": the second copy reads back" ) &&
			           Check( SameContainerRecords( second, third ), szName + ": the second copy is the first" ) &&
			           Check( SameFileBytes( RmgFile( user, szCopy ), RmgFile( user, szAgain ) ), szName + ": the two files hold the same bytes" );
			if ( bOk )
				++nContainersOk;
		}
		for ( size_t i = 0; i < graphs.size(); ++i )
		{
			const std::string &szName = graphs[i];
			char szCopy[64], szAgain[64];
			sprintf( szCopy, "scenarios\\graphs\\roundtrip\\g%04d", int( i ) );
			sprintf( szAgain, "scenarios\\graphs\\roundtrip\\g%04d_b", int( i ) );
			SGraphBuf first, second, third;
			SRMGraph direct;
			bool bOk = Check( LoadDataResource( szName, "", false, 0, RMGC_GRAPH_XML_NAME, direct ), szName + ": the engine loads it" ) &&
			           Check( ReadGraph( pSession, szName, &first ), szName + ": the bridge reads it (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( GraphIsEngines( first, direct ), szName + ": the bridge's record is the engine's own load" ) &&
			           Check( BkEditorRmgWriteGraph( pSession, szCopy, &first.record ) == BK_EDITOR_OK, szName + ": writes under a scratch name (" + BkEditorLastMessage( pSession ) + ")" ) &&
			           Check( ReadGraph( pSession, szCopy, &second ), szName + ": the copy reads back" ) &&
			           Check( SameGraphRecords( first, second ), szName + ": the copy is the original, field for field" ) &&
			           Check( BkEditorRmgWriteGraph( pSession, szAgain, &second.record ) == BK_EDITOR_OK, szName + ": the copy writes again" ) &&
			           Check( ReadGraph( pSession, szAgain, &third ), szName + ": the second copy reads back" ) &&
			           Check( SameGraphRecords( second, third ), szName + ": the second copy is the first" ) &&
			           Check( SameFileBytes( RmgFile( user, szCopy ), RmgFile( user, szAgain ) ), szName + ": the two files hold the same bytes" );
			if ( bOk )
				++nGraphsOk;
		}
		const bool bAll = nContainersOk == int( containers.size() ) && nGraphsOk == int( graphs.size() );
		if ( bAll )
			printf( "composer-roundtrip: %d containers, %d graphs ok\n", nContainersOk, nGraphsOk );
		else
			printf( "composer-roundtrip: only %d of %d containers and %d of %d graphs round-tripped\n", nContainersOk, int( containers.size() ), nGraphsOk, int( graphs.size() ) );
		if ( g_nFailures == 0 )
			std::filesystem::remove_all( root, error );
		else
			printf( "kept: %s\n", root.string().c_str() );
		NPlatform::Paths::SetInjectedRootsForTest( szBase.c_str(), szOriginalUser.c_str() );
	}
	BkEditorStop( pSession );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	if ( g_nFailures == 0 )
		printf( "composer-roundtrip: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
