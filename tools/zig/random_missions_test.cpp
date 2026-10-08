// The random missions tier: every template a chapter can offer, generated at
// every difficulty the way the briefing generates it (GameTT/Mission.cpp), then
// read back, checked and opened in the engine. Needs a hidden SDL window and a
// GPU device, as the engine tier does, and skips honestly where there is none.
//
// argv: <installation> <scratch> [all | cover | only=<text>]
//   all     every gated chapter x every template of its setting x 3 difficulties
//   cover   every gated chapter x template pair once, the difficulty rotating
//   only=   the cases whose chapter or template name contains <text>
#include "StdAfx.h"
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <set>
#include "../../Sources/src/EditorBridge/bridge.h"
#include "../../Sources/src/MapFile/MapFile.h"
#include "../../Sources/src/MapFile/MapEquivalence.h"
#include "../../Sources/src/RandomMapGen/MapInfo_Types.h"
#include "../../Sources/src/RandomMapGen/Resource_Types.h"
#include "../../Sources/src/Main/GameStats.h"
#include "../../Sources/src/Main/GameDB.h"
#include "../../Sources/src/StreamIO/RandomGen.h"
#include "../../Sources/src/StreamIO/StreamIOTypes.h"

#if defined(_WIN32) || defined(_WIN64)
#include <crtdbg.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <fstream>
#endif

// The process's resident memory in KiB, by the platform's own counter: VmRSS on Linux, the
// physical footprint on macOS (which includes GPU allocations on unified memory), the private
// bytes on Windows. -1 where it cannot be read. Printed after every case, so a leak shows as
// growth per mission rather than as the runner killing the sweep.
static long long ProcessMemoryKb()
{
#if defined(_WIN32) || defined(_WIN64)
	PROCESS_MEMORY_COUNTERS_EX counters = {};
	if ( !K32GetProcessMemoryInfo( GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>( &counters ), sizeof( counters ) ) )
		return -1;
	return (long long)( counters.PrivateUsage / 1024 );
#elif defined(__APPLE__)
	task_vm_info_data_t info = {};
	mach_msg_type_number_t nCount = TASK_VM_INFO_COUNT;
	if ( task_info( mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>( &info ), &nCount ) != KERN_SUCCESS )
		return -1;
	return (long long)( info.phys_footprint / 1024 );
#else
	std::ifstream status( "/proc/self/status" );
	std::string szLine;
	while ( std::getline( status, szLine ) )
		if ( szLine.compare( 0, 6, "VmRSS:" ) == 0 )
			return std::atoll( szLine.c_str() + 6 );
	return -1;
#endif
}

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

static std::string Lower( std::string sz )
{
	NStr::ToLower( sz );
	return sz;
}

// A generated-data root the way NGeneratedData::Root spells one: backslashes
// and a trailing one, which is what CreateRandomMap appends names to.
static std::string GeneratedRoot( const std::filesystem::path &directory )
{
	std::string sz = std::filesystem::absolute( directory ).string();
	for ( char &c : sz )
		if ( c == '/' )
			c = '\\';
	return sz + "\\";
}

struct SCase
{
	std::string szCampaign;
	std::string szChapter;
	std::string szContext;
	std::string szTemplate;
	int nDifficulty;
	bool bRegenerate;					// the first case of each template
};

static const char *const CAMPAIGNS[] = {
	"scenarios\\campaigns\\german\\german",
	"scenarios\\campaigns\\allies\\allies",
	"scenarios\\campaigns\\ussr\\ussr",
};

static std::vector<SCase> CollectCases( const std::string &szSweep )
{
	std::vector<SCase> cases;
	std::set<std::string> regenerated;
	// Chapter and template names are compared lower-cased, so the filter is too.
	const bool bOnly = szSweep.compare( 0, 5, "only=" ) == 0;
	// "cover-from=N": the cover sweep without its first N cases, to carry on
	// after a run that was cut short (a harness's time limit). The cases are the
	// same ones, in the same order, with the same regenerate flags.
	const bool bResume = szSweep.compare( 0, 11, "cover-from=" ) == 0;
	const size_t nSkip = bResume ? size_t( atoi( szSweep.c_str() + 11 ) ) : 0;
	const bool bCover = szSweep == "cover" || bResume;
	const std::string szOnly = bOnly ? Lower( szSweep.substr( 5 ) ) : std::string();
	for ( const char *pszCampaign : CAMPAIGNS )
	{
		const SCampaignStats *pCampaign = NGDB::GetGameStats<SCampaignStats>( pszCampaign, IObjectsDB::CAMPAIGN );
		if ( !Check( pCampaign != 0, std::string( "campaign stats " ) + pszCampaign ) )
			continue;
		for ( const SCampaignStats::SChapter &chapter : pCampaign->chapters )
		{
			const std::string szChapter = Lower( chapter.szChapter );
			const SChapterStats *pChapter = NGDB::GetGameStats<SChapterStats>( szChapter.c_str(), IObjectsDB::CHAPTER );
			if ( !Check( pChapter != 0, "chapter stats " + szChapter ) )
				continue;
			// The first chapters have no placeholders and no random missions,
			// in the original as here: their scripts enable missions directly.
			if ( pChapter->placeHolders.empty() )
				continue;
			int nPair = 0;
			for ( const std::string &szTemplateName : pCampaign->templateMissions )
			{
				const std::string szTemplate = Lower( szTemplateName );
				const SMissionStats *pMission = NGDB::GetGameStats<SMissionStats>( szTemplate.c_str(), IObjectsDB::MISSION );
				if ( !Check( pMission != 0, "template mission stats " + szTemplate ) )
					continue;
				// The chapter screen's own rule (GameTT/Chapter.cpp).
				if ( pMission->szSettingName != pChapter->szSettingName )
					continue;
				for ( int nDifficulty = 0; nDifficulty < 3; ++nDifficulty )
				{
					if ( bCover && nDifficulty != nPair % 3 )
						continue;
					if ( bOnly && szChapter.find( szOnly ) == std::string::npos && szTemplate.find( szOnly ) == std::string::npos )
						continue;
					SCase c;
					c.szCampaign = pszCampaign;
					c.szChapter = szChapter;
					c.szContext = pChapter->szContextName;
					c.szTemplate = szTemplate;
					c.nDifficulty = nDifficulty;
					c.bRegenerate = regenerated.insert( szTemplate ).second;
					cases.push_back( c );
				}
				++nPair;
			}
		}
	}
	cases.erase( cases.begin(), cases.begin() + (std::min)( nSkip, cases.size() ) );
	return cases;
}

static int GraphIndex( const SMissionStats *pMission, const std::string &szGraphName )
{
	SRMTemplate randomMapTemplate;
	if ( !LoadDataResource( pMission->szTemplateMap, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate ) )
		return -1;
	for ( int i = 0; i < randomMapTemplate.graphs.size(); ++i )
		if ( randomMapTemplate.graphs[i] == szGraphName )
			return i;
	return -1;
}

// The graph and angle the briefing (GameTT/Mission.cpp) picks for a profile
// that has used none of them yet: any graph by the template's weights, any
// angle. The briefing always passes them in, and must: the generator stores
// its seed before it would draw them itself, so a map generated with -1 here
// would not regenerate from its seed with the graph and angle a save names.
static bool ChooseGraphAndAngle( const SMissionStats *pMission, int *pnGraph, int *pnAngle )
{
	SRMTemplate randomMapTemplate;
	if ( !LoadDataResource( pMission->szTemplateMap, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate ) || randomMapTemplate.graphs.empty() )
		return false;
	CWeightVector<int> graphIndices;
	for ( int i = 0; i < randomMapTemplate.graphs.size(); ++i )
		graphIndices.push_back( i, randomMapTemplate.graphs.GetWeight( i ) );
	*pnGraph = graphIndices.size() == 1 ? graphIndices[0] : graphIndices.GetRandom();
	*pnAngle = rand() % 4;
	return true;
}

static bool RunCase( BkEditorSession *pSession, const SCase &c, const std::filesystem::path &scratch )
{
	const std::string szName = c.szChapter + " " + c.szTemplate + " d" + std::to_string( c.nDifficulty );
	// The briefing hands the shared stats to the generator, which writes the
	// objectives' map positions into them; so does this.
	SMissionStats *pMission = const_cast<SMissionStats*>( NGDB::GetGameStats<SMissionStats>( c.szTemplate.c_str(), IObjectsDB::MISSION ) );
	std::string szDir = c.szChapter + "_" + c.szTemplate + "_d" + std::to_string( c.nDifficulty );
	for ( char &ch : szDir )
		if ( ch == '\\' || ch == '/' )
			ch = '_';
	const std::filesystem::path dir = scratch / "random-missions" / szDir;
	std::filesystem::remove_all( dir );
	const std::string szRoot = GeneratedRoot( dir / "a" );
	const int nFailuresBefore = g_nFailures;

	int nGraph = -1;
	int nAngle = -1;
	Check( ChooseGraphAndAngle( pMission, &nGraph, &nAngle ), szName + ": its template has graphs" );
	// Before the generator runs: if it takes the process down, this is the
	// line that names the case, and the graph and angle reproduce it.
	printf( "random-missions: %s %s start graph=%d angle=%d\n", c.szCampaign.c_str(), szName.c_str(), nGraph, nAngle );
	fflush( stdout );
	const auto start = std::chrono::steady_clock::now();
	SRMUsedTemplateInfo used;
	const bool bGenerated = CMapInfo::CreateRandomMap( pMission, c.szContext, c.nDifficulty, nGraph, nAngle, true, true, &used, 0, szRoot );
	const long long nMs = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - start ).count();
	printf( "random-missions: %s %s %lld ms\n", c.szCampaign.c_str(), szName.c_str(), nMs );
	fflush( stdout );

	if ( Check( bGenerated, szName + ": generates" ) )
	{
		const std::string szMap = szRoot + "maps\\" + pMission->szFinalMap + ".bzm";
		CMapInfo map;
		std::string szError;
		if ( Check( NMapFile::Read( szMap.c_str(), &map, &szError ), szName + ": the generated map reads (" + szError + ")" ) )
		{
			std::set<int> scriptIDs;
			for ( const SMapObjectInfo &object : map.objects )
				scriptIDs.insert( object.nScriptID );
			for ( const SMapObjectInfo &object : map.scenarioObjects )
				scriptIDs.insert( object.nScriptID );
			for ( int i = 0; i < pMission->objectives.size(); ++i )
			{
				const SMissionStats::SObjective &objective = pMission->objectives[i];
				if ( objective.nAnchorScriptID == RMGC_INVALID_SCRIPT_ID_VALUE || objective.nAnchorScriptID == RMGC_DEFAULT_SCRIPT_ID_VALUE )
					continue;
				const std::string szObjective = szName + ": objective " + std::to_string( i ) + " (anchor " + std::to_string( objective.nAnchorScriptID ) + ")";
				Check( scriptIDs.count( objective.nAnchorScriptID ) != 0, szObjective + " has its anchor on the map" );
				// The briefing map is 512x512 (CreateRandomMap, "Place objectives").
				Check( objective.vPosOnMap.x >= 0.0f && objective.vPosOnMap.x < 512.0f && objective.vPosOnMap.y >= 0.0f && objective.vPosOnMap.y < 512.0f,
				       szObjective + " lands on the briefing map at " + std::to_string( objective.vPosOnMap.x ) + "," + std::to_string( objective.vPosOnMap.y ) );
			}
			Check( BkEditorOpenMap( pSession, szMap.c_str(), 0 ) == BK_EDITOR_OK, szName + ": the engine opens it (" + BkEditorLastMessage( pSession ) + ")" );

			if ( c.bRegenerate )
			{
				// What loading a save does (Main/RandomMapHelper.cpp): the seed
				// the first run stored, the graph and angle it chose.
				CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
				CPtr<IDataStream> pSeedStream = CreateFileStream( ( szRoot + "maps\\" + pMission->szFinalMap + ".seed" ).c_str(), STREAM_ACCESS_READ );
				if ( Check( pSeedStream != 0, szName + ": the seed was stored" ) )
				{
					pSeed->Restore( pSeedStream );
					pSeedStream = 0;					// regeneration rewrites this file; an open stream depends on share modes, which differ on Windows.
					GetSingleton<IRandomGen>()->SetSeed( pSeed );
					// Keep the first run's map, since regenerating overwrites it
					// in place: the game regenerates into the same root it
					// generated into (Main/RandomMapHelper.cpp, GameTT/Mission.cpp).
					// szMap is spelled with backslashes throughout (GeneratedRoot,
					// and szFinalMap may itself have subdirectories); std::filesystem
					// only recognizes the native separator.
					std::string szMapNative = szMap;
					for ( char &ch : szMapNative )
						if ( ch == '\\' )
							ch = '/';
					std::filesystem::copy_file( szMapNative, dir / "first.bzm", std::filesystem::copy_options::overwrite_existing );
					const bool bAgain = CMapInfo::CreateRandomMap( pMission, c.szContext, c.nDifficulty, GraphIndex( pMission, used.szGraphName ), used.nGraphAngle, true, true, 0, 0, szRoot );
					CMapInfo again;
					std::string szWhere;
					if ( Check( bAgain, szName + ": regenerates from its seed" )
					     && Check( NMapFile::Read( szMap.c_str(), &again, &szError ), szName + ": the regenerated map reads" ) )
						Check( NMapFile::AreEquivalent( map, again, &szWhere ), szName + ": the seed gives the same map (differs at " + szWhere + ")" );
				}
			}
		}
	}

	if ( g_nFailures == nFailuresBefore )
		std::filesystem::remove_all( dir );
	else
		printf( "kept: %s\n", dir.string().c_str() );
	return g_nFailures == nFailuresBefore;
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
			return SkipOrFail( "random-missions", std::string( "no video driver (" ) + pszError + ")" );
		}
		printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	SDL_Window *pWindow = SDL_CreateWindow( "random-missions-test", 640, 480, SDL_WINDOW_HIDDEN );
	if ( pWindow == 0 )
	{
		printf( "FAIL: SDL_CreateWindow: %s\n", SDL_GetError() );
		SDL_Quit();
		return 1;
	}
	const std::string szSelfDir = DirectoryOf( argv[0] != 0 ? argv[0] : "." );
	const char *pszRoot = argc > 1 ? argv[1] : szSelfDir.c_str();
	const std::filesystem::path scratch = argc > 2 ? argv[2] : szSelfDir;
	const std::string szSweep = argc > 3 ? argv[3] : "all";
	std::filesystem::create_directories( scratch );
	if ( !std::filesystem::exists( std::string( pszRoot ) + "/Data/consts.xml" ) )
	{
		const int nSkipped = SkipOrFail( "random-missions", std::string( "no staged game at " ) + pszRoot + " (run: zig build install-game)" );
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
		const int nSkipped = SkipOrFail( "random-missions", std::string( "no GPU device (" ) + BkEditorLastMessage( pSession ) + ")" );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	if ( Check( status == BK_EDITOR_OK, std::string( "the engine starts: " ) + BkEditorLastMessage( pSession ) ) )
	{
		const auto start = std::chrono::steady_clock::now();
		const std::vector<SCase> cases = CollectCases( szSweep );
		// A sweep that selects nothing (a mistyped only= filter) tests nothing and must not pass.
		Check( !cases.empty() && ( szSweep.compare( 0, 5, "only=" ) == 0 || szSweep.compare( 0, 11, "cover-from=" ) == 0 || cases.size() > 150 ), "the sweep found the chapters' templates (" + std::to_string( cases.size() ) + ")" );
		int nFailedCases = 0;
		const long long nMemoryAtStart = ProcessMemoryKb();
		int nCase = 0;
		for ( const SCase &c : cases )
		{
			if ( !RunCase( pSession, c, scratch ) )
				++nFailedCases;
			printf( "random-missions: memory after case %d: %lld KiB\n", ++nCase, ProcessMemoryKb() );
			fflush( stdout );
		}
		printf( "random-missions: memory %lld KiB before the first case, %lld KiB after the last\n", nMemoryAtStart, ProcessMemoryKb() );
		const long long nSeconds = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::steady_clock::now() - start ).count();
		printf( "random-missions: %d cases, %d failed, %lld s\n", int( cases.size() ), nFailedCases, nSeconds );
	}
	BkEditorStop( pSession );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	return g_nFailures == 0 ? 0 : 1;
}
