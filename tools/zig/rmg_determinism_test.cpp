// The Create Random Map determinism harness (M3, 05-08, D-04/D-40.5): the editor's
// own generation path - BkEditorCreateRandomMap, which wraps the engine's
// CMapInfo::CreateRandomMap - generates the same map twice from a fixed seed with a
// fixed graph and angle, into two different user folders, and the two .bzm files are
// compared byte for byte, each read fresh from disk (D-40.5 is stricter than
// NMapFile::AreEquivalent: a padding byte that differs is a difference). It also
// shows the seed is what decides: another seed gives another map, a seed with the
// graph and angle left to the generator gives the same map twice, and a blank seed
// draws a different one each time. This is the harness backlog 999.1's polygon-fill
// speed-up needs as its own gate (the fill is where the time goes).
//
// The first comparison generates into two DIFFERENT user folders: a map names its script
// relative to its own folder (the bare name, WINDOWS.md 5, ruled 2026-10-03), so nothing of
// the computer it was made on is in the file - an older engine stored the absolute output
// path and the two folders differed in that one path. Every generation's output is copied
// away before the next one replaces it; the copies are of the files as the generator wrote
// them, and the comparison reads them from disk.
//
// The authored leg (05-10, D-40.5): a template, a graph, a container and a field set
// the composers wrote - through the same portable BkEditorRmgWrite* entries the
// composers' Save uses, into the user RMG root, never touching a shipped file - are
// generated from twice with the fixed seed and the two maps are byte identical. The
// authored set is a copy of a shipped one under new names (the template keeps the
// shipped header, diplomacy and units; its graph list and field list name the
// authored graph and field set; the graph's nodes name authored copies of the
// containers they held), and it is written again after each generation wipes the
// scratch user folder.
//
// Needs a hidden SDL window and a GPU device, as the engine tier does, and skips
// honestly where there is none.
//
// argv: <installation> <scratch>
#include "StdAfx.h"
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include "../../Sources/src/EditorBridge/bridge.h"
#include "../../Sources/src/Platform/Paths.h"
#include "rmg_record_io.h"

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

// A file read whole, fresh from disk.
static bool FileBytes( const std::string &szPath, std::vector<char> *pBytes )
{
	std::ifstream file( szPath.c_str(), std::ios::binary );
	if ( !file )
		return false;
	pBytes->assign( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	return true;
}

// The offset of the first difference between two files, or -1 when they are the
// same bytes; -2 when one cannot be read.
static long long FirstDifference( const std::string &szLeft, const std::string &szRight )
{
	std::vector<char> left, right;
	if ( !FileBytes( szLeft, &left ) || !FileBytes( szRight, &right ) )
		return -2;
	size_t i = 0;
	while ( i < left.size() && i < right.size() && left[i] == right[i] )
		++i;
	if ( i == left.size() && i == right.size() )
		return -1;
	return static_cast<long long>( i );
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

// The authored set's names (all under the "user" folder of each kind).
static const char *const g_pszAuthoredTemplate = "scenarios\\templates\\user\\authored_t";
static const char *const g_pszAuthoredGraph = "scenarios\\graphs\\user\\authored_g";
static const char *const g_pszAuthoredField = "scenarios\\fieldsets\\user\\authored_f";

// Writes the authored template, graph, containers and field set under the user RMG
// root, copied from the shipped template `szBase` (its first graph, its default
// field set); the same files every time it is called.
static bool AuthorSet( BkEditorSession *pSession, const std::string &szBase )
{
	STemplateBuf tpl;
	if ( !Check( ReadTemplate( pSession, szBase, &tpl ), "the base template reads: " + std::string( BkEditorLastMessage( pSession ) ) ) )
		return false;
	if ( !Check( tpl.record.graph_count > 0 && tpl.record.field_count > 0, "the base template names a graph and a field set" ) )
		return false;
	SGraphBuf graph;
	SFieldSetBuf field;
	const int nDefault = tpl.record.default_field >= 0 && tpl.record.default_field < tpl.record.field_count ? tpl.record.default_field : 0;
	if ( !Check( ReadGraph( pSession, tpl.record.graphs[0].name, &graph ), "the base graph reads: " + std::string( BkEditorLastMessage( pSession ) ) ) ||
	     !Check( ReadFieldSet( pSession, tpl.record.fields[nDefault].name, &field ), "the base field set reads: " + std::string( BkEditorLastMessage( pSession ) ) ) )
		return false;
	// One authored copy of every distinct container the graph's nodes hold.
	std::vector<std::string> held;
	for ( int i = 0; i < graph.record.node_count; ++i )
	{
		const std::string szHeld = graph.record.nodes[i].container;
		if ( szHeld.empty() )
			continue;
		size_t nAt = 0;
		while ( nAt < held.size() && held[nAt] != szHeld )
			++nAt;
		if ( nAt == held.size() )
			held.push_back( szHeld );
		SContainerBuf container;
		if ( !Check( ReadContainer( pSession, szHeld, &container ), szHeld + ": the base container reads" ) )
			return false;
		char szAuthored[96];
		sprintf( szAuthored, "scenarios\\containers\\user\\authored_c%d", int( nAt ) );
		if ( !Check( BkEditorRmgWriteContainer( pSession, szAuthored, &container.record ) == BK_EDITOR_OK, std::string( szAuthored ) + ": the container writes (" + BkEditorLastMessage( pSession ) + ")" ) )
			return false;
		strcpy( graph.record.nodes[i].container, szAuthored );
	}
	if ( !Check( BkEditorRmgWriteGraph( pSession, g_pszAuthoredGraph, &graph.record ) == BK_EDITOR_OK, std::string( "the graph writes (" ) + BkEditorLastMessage( pSession ) + ")" ) ||
	     !Check( BkEditorRmgWriteFieldSet( pSession, g_pszAuthoredField, &field.record ) == BK_EDITOR_OK, std::string( "the field set writes (" ) + BkEditorLastMessage( pSession ) + ")" ) )
		return false;
	strcpy( tpl.record.graphs[0].name, g_pszAuthoredGraph );
	tpl.record.graphs[0].weight = 1;
	tpl.record.graph_count = 1;
	strcpy( tpl.record.fields[0].name, g_pszAuthoredField );
	tpl.record.fields[0].weight = 1;
	tpl.record.field_count = 1;
	tpl.record.default_field = 0;
	return Check( BkEditorRmgWriteTemplate( pSession, g_pszAuthoredTemplate, &tpl.record ) == BK_EDITOR_OK, std::string( "the template writes (" ) + BkEditorLastMessage( pSession ) + ")" );
}

// One generation into the harness's user folder (the bridge builds its output folder
// from the platform's user root, so the root is pointed at the scratch folder first),
// the generated map copied to `szCopy` before the next generation replaces it.
static bool Generate( BkEditorSession *pSession, const std::string &szBase, const std::filesystem::path &userFolder,
                      const BkEditorRmgGenerateParams &rParams, BkEditorRmgGenerateResult *pResult, const std::string &szWhat,
                      const std::filesystem::path &szCopy, const std::function<bool()> &prepare = std::function<bool()>() )
{
	std::error_code error;
	std::filesystem::remove_all( userFolder, error );
	const std::string szUser = userFolder.string() + "/";
	NPlatform::Paths::SetInjectedRootsForTest( szBase.c_str(), szUser.c_str() );
	if ( prepare && !Check( prepare(), szWhat + ": the authored files are written" ) )
		return false;
	const bool bOk = Check( BkEditorCreateRandomMap( pSession, &rParams, pResult ) == BK_EDITOR_OK, szWhat + ": generates (" + BkEditorLastMessage( pSession ) + ")" );
	if ( !bOk )
		return false;
	std::filesystem::create_directories( szCopy.parent_path(), error );
	std::filesystem::copy_file( pResult->map_path, szCopy, std::filesystem::copy_options::overwrite_existing, error );
	return Check( !error, szWhat + ": the map is copied aside (" + error.message() + ")" );
}

// A skip is a pass that checked nothing, so CI sets BK_REQUIRE_ENGINE=1 on the runners that
// do have a video driver, the staged game and a GPU device: there a skip is the runner
// regressing, not a green result (05-REVIEW WR-D03). Unset (a laptop with no display), a
// skip stays an exit code of 0.
static int SkipOrFail( const char *pszTool, const std::string &szWhy )
{
	const char *pszRequire = getenv( "BK_REQUIRE_ENGINE" );
	if ( pszRequire != 0 && *pszRequire != 0 && strcmp( pszRequire, "0" ) != 0 )
	{
		printf( "FAIL: %s: %s, and BK_REQUIRE_ENGINE is set\n", pszTool, szWhy.c_str() );
		return 1;
	}
	printf( "%s: skipped: %s\n", pszTool, szWhy.c_str() );
	return 0;
}

int main( int argc, char **argv )
{
	// See the engine tier: a Windows debug assert must not wait behind a dialog.
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
			return SkipOrFail( "rmg-determinism", std::string( "no video driver (" ) + pszError + ")" );
		}
		printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	SDL_Window *pWindow = SDL_CreateWindow( "rmg-determinism-test", 640, 480, SDL_WINDOW_HIDDEN );
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
		const int nSkipped = SkipOrFail( "rmg-determinism", std::string( "no staged game at " ) + pszRoot + " (run: zig build install-game)" );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	// Every engine module derives its roots from the executable's location; see
	// the engine tier (editor_bridge_test.cpp) for why this must hold.
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
		const int nSkipped = SkipOrFail( "rmg-determinism", std::string( "no GPU device (" ) + BkEditorLastMessage( pSession ) + ")" );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	if ( Check( status == BK_EDITOR_OK, std::string( "the engine starts: " ) + BkEditorLastMessage( pSession ) ) )
	{
		const std::string szBase = NPlatform::Paths::BaseRoot();
		const std::string szOriginalUser = NPlatform::Paths::UserRoot();
		// A shipped summer template, a chapter context and a setting, as the dialog's
		// combos list them.
		std::string szTemplate, szContext, szSetting;
		for ( const std::string &szName : ListNames( pSession, 1 ) )
			if ( szName == "scenarios\\templates\\summer\\template02" )
				szTemplate = szName;
		for ( const std::string &szName : ListNames( pSession, 5 ) )
			if ( szContext.empty() && szName.size() > 8 && szName.compare( szName.size() - 8, 8, "\\context" ) == 0 )
				szContext = szName;
		for ( const std::string &szName : ListNames( pSession, 4 ) )
			if ( szName == "scenarios\\settings\\summer_france" )
				szSetting = szName;
		if ( Check( !szTemplate.empty() && !szContext.empty() && !szSetting.empty(), "the shipped data lists a summer template, a context and a setting" ) )
		{
			BkEditorRmgGenerateParams params;
			memset( &params, 0, sizeof params );
			strcpy( params.template_name, szTemplate.c_str() );
			strcpy( params.context_name, szContext.c_str() );
			strcpy( params.setting_name, szSetting.c_str() );
			strcpy( params.map_name, "rmg_determinism" );
			params.level = 0;
			params.save_as_bzm = 1;
			params.write_dds = 0;
			params.overwrite = 1;
			params.has_seed = 1;
			params.seed = 424242;
			params.graph = 0;
			params.angle = 0;
			const std::filesystem::path root = scratch / "rmg-determinism";
			const std::filesystem::path user = root / "user";

			// 1. The same seed, graph and angle, two user folders: the same bytes.
			BkEditorRmgGenerateResult first, second;
			memset( &first, 0, sizeof first );
			memset( &second, 0, sizeof second );
			const std::filesystem::path otherUser = root / "user-other-folder";
			if ( Generate( pSession, szBase, user, params, &first, "the first generation", root / "first.bzm" ) &&
			     Generate( pSession, szBase, otherUser, params, &second, "the second generation", root / "second.bzm" ) )
			{
				Check( first.seed == 424242 && second.seed == 424242, "both report the seed they were given" );
				Check( strcmp( first.map_path, second.map_path ) != 0, "the two maps were generated into different folders" );
				const long long nDifference = FirstDifference( ( root / "first.bzm" ).string(), ( root / "second.bzm" ).string() );
				Check( nDifference == -1, nDifference == -2 ? std::string( "the generated maps cannot be read" )
				                                            : "the two maps differ at byte " + std::to_string( nDifference ) );
				if ( nDifference == -1 )
					printf( "rmg-determinism: byte identical ok across two folders (seed 424242, graph 0, angle 0)\n" );
			}

			// 2. The seed decides: another seed gives another map.
			BkEditorRmgGenerateParams other = params;
			other.seed = 424243;
			BkEditorRmgGenerateResult third;
			memset( &third, 0, sizeof third );
			if ( Generate( pSession, szBase, user, other, &third, "the generation from another seed", root / "third.bzm" ) )
				Check( FirstDifference( ( root / "first.bzm" ).string(), ( root / "third.bzm" ).string() ) != -1, "another seed gives another map" );

			// 3. With the graph and angle left to the generator the seed picks them,
			// and the same seed picks the same ones.
			BkEditorRmgGenerateParams loose = params;
			loose.graph = -1;
			loose.angle = -1;
			BkEditorRmgGenerateResult looseFirst, looseSecond;
			memset( &looseFirst, 0, sizeof looseFirst );
			memset( &looseSecond, 0, sizeof looseSecond );
			if ( Generate( pSession, szBase, user, loose, &looseFirst, "the first generation with the graph and angle left open", root / "loose-first.bzm" ) &&
			     Generate( pSession, szBase, user, loose, &looseSecond, "the second generation with the graph and angle left open", root / "loose-second.bzm" ) )
			{
				Check( looseFirst.graph == looseSecond.graph && looseFirst.angle == looseSecond.angle,
				       "the seed picks the same graph and angle" );
				const long long nDifference = FirstDifference( ( root / "loose-first.bzm" ).string(), ( root / "loose-second.bzm" ).string() );
				Check( nDifference == -1, nDifference == -2 ? std::string( "the generated maps cannot be read" )
				                                            : "the open-graph maps differ at byte " + std::to_string( nDifference ) );
				if ( nDifference == -1 )
					printf( "rmg-determinism: byte identical ok with the graph and angle left open (graph %d, angle %d)\n", looseFirst.graph, looseFirst.angle );
			}

			// 4. A blank seed draws a fresh one each time, and the one it reports
			// regenerates that map.
			BkEditorRmgGenerateParams blank = params;
			blank.has_seed = 0;
			BkEditorRmgGenerateResult drawnA, drawnB;
			memset( &drawnA, 0, sizeof drawnA );
			memset( &drawnB, 0, sizeof drawnB );
			if ( Generate( pSession, szBase, user, blank, &drawnA, "the first generation with a blank seed", root / "drawn-a.bzm" ) &&
			     Generate( pSession, szBase, user, blank, &drawnB, "the second generation with a blank seed", root / "drawn-b.bzm" ) )
			{
				Check( drawnA.seed != drawnB.seed, "two blank seeds draw different seeds (" + std::to_string( drawnA.seed ) + ", " + std::to_string( drawnB.seed ) + ")" );
				BkEditorRmgGenerateParams again = params;
				again.seed = drawnA.seed;
				BkEditorRmgGenerateResult regenerated;
				memset( &regenerated, 0, sizeof regenerated );
				if ( Generate( pSession, szBase, user, again, &regenerated, "the generation from the drawn seed", root / "regenerated.bzm" ) )
				{
					const long long nDifference = FirstDifference( ( root / "drawn-a.bzm" ).string(), ( root / "regenerated.bzm" ).string() );
					Check( nDifference == -1, nDifference == -2 ? std::string( "the generated maps cannot be read" )
					                                            : "the drawn seed regenerates another map: they differ at byte " + std::to_string( nDifference ) );
				}
			}
			// 5. The authored set: a template, graph, container and field set written
			// through the composers' I/O, generated from twice (the scratch folder is
			// wiped and the set written again before each), byte identical.
			{
				BkEditorRmgGenerateParams authored = params;
				strcpy( authored.template_name, g_pszAuthoredTemplate );
				strcpy( authored.map_name, "rmg_determinism_authored" );
				const std::function<bool()> author = [&]() { return AuthorSet( pSession, szTemplate ); };
				BkEditorRmgGenerateResult authoredFirst, authoredSecond;
				memset( &authoredFirst, 0, sizeof authoredFirst );
				memset( &authoredSecond, 0, sizeof authoredSecond );
				if ( Generate( pSession, szBase, user, authored, &authoredFirst, "the first generation from the authored set", root / "authored-first.bzm", author ) &&
				     Generate( pSession, szBase, user, authored, &authoredSecond, "the second generation from the authored set", root / "authored-second.bzm", author ) )
				{
					const long long nDifference = FirstDifference( ( root / "authored-first.bzm" ).string(), ( root / "authored-second.bzm" ).string() );
					Check( nDifference == -1, nDifference == -2 ? std::string( "the generated maps cannot be read" )
					                                            : "the authored-set maps differ at byte " + std::to_string( nDifference ) );
					if ( nDifference == -1 )
						printf( "rmg-determinism: authored set byte identical ok (seed 424242, graph 0, angle 0)\n" );
				}
			}

			std::error_code error;
			if ( g_nFailures == 0 )
				std::filesystem::remove_all( root, error );
			else
				printf( "kept: %s\n", root.string().c_str() );
		}
		NPlatform::Paths::SetInjectedRootsForTest( szBase.c_str(), szOriginalUser.c_str() );
	}
	BkEditorStop( pSession );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	if ( g_nFailures == 0 )
		printf( "rmg-determinism: PASS\n" );
	return g_nFailures == 0 ? 0 : 1;
}
