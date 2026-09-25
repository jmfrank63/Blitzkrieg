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
#include <SDL3/SDL.h>
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
					if ( szSweep == "cover" && nDifficulty != nPair % 3 )
						continue;
					if ( szSweep.compare( 0, 5, "only=" ) == 0 && szChapter.find( szSweep.substr( 5 ) ) == std::string::npos && szTemplate.find( szSweep.substr( 5 ) ) == std::string::npos )
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
					GetSingleton<IRandomGen>()->SetSeed( pSeed );
					const std::string szRootB = GeneratedRoot( dir / "b" );
					const bool bAgain = CMapInfo::CreateRandomMap( pMission, c.szContext, c.nDifficulty, GraphIndex( pMission, used.szGraphName ), used.nGraphAngle, true, true, 0, 0, szRootB );
					CMapInfo again;
					std::string szWhere;
					if ( Check( bAgain, szName + ": regenerates from its seed" )
					     && Check( NMapFile::Read( ( szRootB + "maps\\" + pMission->szFinalMap + ".bzm" ).c_str(), &again, &szError ), szName + ": the regenerated map reads" ) )
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
			printf( "random-missions: skipped: no video driver (%s)\n", pszError );
			return 0;
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
		printf( "random-missions: skipped: no staged game at %s (run: zig build install-game)\n", pszRoot );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
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
		printf( "random-missions: skipped: no GPU device (%s)\n", BkEditorLastMessage( pSession ) );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 0;
	}
	if ( Check( status == BK_EDITOR_OK, std::string( "the engine starts: " ) + BkEditorLastMessage( pSession ) ) )
	{
		const auto start = std::chrono::steady_clock::now();
		const std::vector<SCase> cases = CollectCases( szSweep );
		Check( szSweep.compare( 0, 5, "only=" ) == 0 || cases.size() > 150, "the sweep found the chapters' templates (" + std::to_string( cases.size() ) + ")" );
		int nFailedCases = 0;
		for ( const SCase &c : cases )
			if ( !RunCase( pSession, c, scratch ) )
				++nFailedCases;
		const long long nSeconds = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::steady_clock::now() - start ).count();
		printf( "random-missions: %d cases, %d failed, %lld s\n", int( cases.size() ), nFailedCases, nSeconds );
	}
	BkEditorStop( pSession );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	return g_nFailures == 0 ? 0 : 1;
}
