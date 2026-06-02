#include "StdAfx.h"

#include "ScenarioTracker2Internal.h"

#include "..\Misc\Checker.h"
#include "..\Misc\Win32Random.h"
#include "GameStats.h"
#include "RPGStats.h"
#include "..\LuaLib\Script.h"
#include "ScenarioTrackerTypes.h"
#include "..\StreamIO\OptionSystem.h"
#include "..\Main\TextSystem.h"
#include "..\GameTT\MultiplayerCommandManager.h"
#include "..\Main\GameStats.h"
BASIC_REGISTER_CLASS(IPlayerScenarioInfo);
BASIC_REGISTER_CLASS(IScenarioStatistics);
CPlayerScenarioInfoIterator::CPlayerScenarioInfoIterator( const CPlayersList &_players )
	: players( _players ), itCurrPlayer( _players.begin() )
{
}
void CPlayerScenarioInfoIterator::Next()
{
	++itCurrPlayer;
}
bool CPlayerScenarioInfoIterator::IsEnd() const
{
	return itCurrPlayer == players.end();
}
IPlayerScenarioInfo* CPlayerScenarioInfoIterator::Get() const
{
	return itCurrPlayer->second;
}
int CPlayerScenarioInfoIterator::GetID() const
{
	return itCurrPlayer->first;
}
namespace NScenarioScript2
{
struct SChanges
{
	const std::string szOldValue;					// old value (upgrade from)
	const std::string szNewValue;					// new value (upgrade to)
	SChanges( const std::string &_szOld, const std::string &_szNew )
		: szOldValue( _szOld ), szNewValue( _szNew ) {  }
};
struct SUpgrades
{
	const std::string szName;							// name for upgrade
	const int nUpgradeFor;								// this upgrade valid for
	SUpgrades( const std::string &_szName, const int _nUpgradeFor )
		: szName( _szName ), nUpgradeFor( _nUpgradeFor ) {  }
};
typedef std::pair<std::string, int> SMedal;
static std::list<std::string> szNewSlots;
static std::list<SUpgrades> newUpgrades;
static std::list<SChanges> changeCurrents;
static std::list<SMedal> newMedals;
static std::list<std::string> newBaseUpgrades;
static std::list<std::string> newRemoveBaseUpgrades;
int FinishCampaign( struct lua_State *state )
{
	SetGlobalVar( "FinishingCampaign", 1 );
	return 0;
}
int AddNewSlot( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "AddNewSlot: the first parameter is not a string", return 0 );

	szNewSlots.push_back( script.GetObject(1).GetString() );
	script.Pop();

	return 0;
}
int AddUpgrade( struct lua_State *state )
{
	Script script( state );
	const int nArgs = script.GetTop();
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "AddUpgrade: the first parameter is not a string", return 0 );
	newUpgrades.push_back( SUpgrades(script.GetObject(1).GetString(), 0) );
	script.Pop( nArgs );
	return 0;
}
int AddBaseUpgrade( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "AddUpgrade: the first parameter is not a string", return 0 );
	newBaseUpgrades.push_back( script.GetObject(1).GetString() );
	script.Pop();
	return 0;
}
int RemoveBaseUpgrade( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "AddUpgrade: the first parameter is not a string", return 0 );
	newRemoveBaseUpgrades.push_back( script.GetObject(1).GetString() );
	script.Pop();
	return 0;
}
int ChangeDefault( struct lua_State *state )
{
	Script script( state );
	script.Pop( 2 );

	return 0;
}
int ChangeCurrent( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "ChangeCurrent: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "ChangeCurrent: the second parameter is not a number", return 0 );

	changeCurrents.push_back( SChanges(script.GetObject(1).GetString(), script.GetObject(2).GetString()) );
	script.Pop( 2 );

	return 0;
}
int AddMedal( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "AddMedal: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "AddMedal: the second parameter is not a number", return 0 );
	newMedals.push_back( SMedal(script.GetObject(1).GetString(), script.GetObject(2).GetInteger()) );
	NStr::ToLower( newMedals.back().first );
	script.Pop( 2 );

	return 0;
}
int EnableChapter( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "EnableChapter: the first parameter is not a string", return 0 );
	std::string szValue = script.GetObject(1).GetString();
	NStr::ToLower( szValue );
	NI_ASSERT_T( NGDB::GetGameStats<SChapterStats>(szValue.c_str(), IObjectsDB::CHAPTER) != 0, NStr::Format("Can't find chapter \"%s\" to enable", szValue.c_str()) );
	const std::string szVarName = NStr::Format( "Chapter.%s.Status", szValue.c_str() );
	SetGlobalVar( szVarName.c_str(), 1 );
	SetGlobalVar( "Chapter.New.Available", szValue.c_str() );
	script.Pop();

	return 0;
}
int EnableMission( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "EnableMission: the first parameter is not a string", return 0 );
	std::string szValue = script.GetObject(1).GetString();
	NStr::ToLower( szValue );
	NI_ASSERT_T( NGDB::GetGameStats<SMissionStats>(szValue.c_str(), IObjectsDB::MISSION) != 0, NStr::Format("Can't find mission \"%s\" to enable", szValue.c_str()) );
	std::string szVarName = "Mission.";
	szVarName += szValue;
	szVarName += ".Enabled";
	SetGlobalVar( szVarName.c_str(), 1 );
	script.Pop();

	return 0;
}
int GetStatisticsValue( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsNumber( 1 ), "GetStatisticsValue: the first parameter is not a number", return 0 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "GetStatisticsValue: the second parameter is not a number", return 0 );
	const int nStatType = script.GetObject(1).GetInteger();
	const int nStatComplexity = script.GetObject(2).GetInteger();
	script.Pop( 2 );
	IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
	IScenarioStatistics *pStatistics = 0;
	switch ( nStatComplexity ) 
	{
		case 0:															// total
			pStatistics = pPlayer->GetCampaignStats();
			break;
		case 1:															// chapter
			pStatistics = pPlayer->GetChapterStats();
			break;
		case 2:															// last mission
			pStatistics = pPlayer->GetMissionStats();
			break;
	}
	const double fValue = pStatistics != 0 ? pStatistics->GetValue( nStatType ) : 0;
	script.PushNumber( fValue );
	return 1;
}
int HasMedal( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "HasMedal: the first parameter is not a string", return 0 );
	std::string szMedalName = script.GetObject(1).GetString();
	script.Pop();
	NStr::ToLower( szMedalName );
	IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
	const double fValue = pPlayer->HasMedal( szMedalName );
	script.PushNumber( fValue );
	return 1;
}
inline int GetNumMissionsFromChapter( IChapterStatistics *pChapter, bool bWon )
{
	int nNumMissions = 0;
	for ( int i = 0; i < pChapter->GetNumMissions(); ++i )
		nNumMissions += int( pChapter->GetMission(i)->GetFinishStatus() == MISSION_FINISH_WIN );
	return nNumMissions;
}
int GetNumMissions( struct lua_State *state )
{
	Script script( state );
	NI_ASSERT_SLOW_TF( script.IsNumber( 1 ), "GetNumMissions: the first parameter is not a number", return 0 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "GetNumMissions: the second parameter is not a number", return 0 );
	const bool bWon = script.GetObject(1).GetInteger() != 0;
	const bool bInChapter = script.GetObject(2).GetInteger() != 0;
	script.Pop( 2 );
	double fValue = 0;
	IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
	if ( bInChapter ) 
	{
		IChapterStatistics *pChapter = pPlayer->GetChapterStats();
		fValue = pChapter != 0 ? GetNumMissionsFromChapter( pChapter, bWon ) : 0;
	}
	else
	{
		ICampaignStatistics *pCampaign = pPlayer->GetCampaignStats();
		if ( pCampaign != 0 ) 
		{
			for ( int i = 0; i < pCampaign->GetNumChapters(); ++i )
				fValue += GetNumMissionsFromChapter( pCampaign->GetChapter(i), bWon );
		}
	}
	script.PushNumber( fValue );
	return 1;
}
int SetIGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "SetIGlobalVar: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "SetIGlobalVar: the second parameter is not a number", return 0 );

	SetGlobalVar( script.GetObject( 1 ), int(script.GetObject( 2 )) );

	return 0;
}
int SetFGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "SetFGlobalVar: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "SetFGlobalVar: the second parameter is not a number", return 0 );

	SetGlobalVar( script.GetObject( 1 ), float(script.GetObject( 2 )) );

	return 0;
}
int SetSGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "SetSGlobalVar: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "SetSGlobalVar: the second parameter is not a number", return 0 );

	SetGlobalVar( script.GetObject( 1 ), (const char *)(script.GetObject( 2 )) );

	return 0;
}
int GetIGlobalVar( struct lua_State *state )
{
	Script script( state );

	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetIGlobalVar: the first parameter is not a string", return 1 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "GetIGlobalVar: the second parameter is not a number", return 1 );

	script.PushNumber( GetGlobalVar( script.GetObject(1), int(script.GetObject(2)) ) );

	return 1;
}
int GetFGlobalVar( struct lua_State *state )
{
	Script script( state );

	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetFGlobalVar: the first parameter is not a string", return 1 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "GetFGlobalVar: the second parameter is not a number", return 1 );

	script.PushNumber( GetGlobalVar( script.GetObject(1), float(script.GetObject(2)) ) );

	return 1;
}
int GetSGlobalVar( struct lua_State *state )
{
	Script script( state );

	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetSGlobalVar: the first parameter is not a string", return 1 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "GetSGlobalVar: the second parameter is not a string", return 1 );

	script.PushString( GetGlobalVar( script.GetObject(1), (const char*)(script.GetObject(2)) ) );

	return 1;
}
int RandomFloat( struct lua_State *pState )
{
	Script script( pState );
	script.PushNumber( Random( 0.0f, 1.0f ) );
	return 1;
}
int ScriptErrorOut( struct lua_State *state )
{
	Script script( state );
	Script::Object obj = script.GetObject(script.GetTop());
	const std::string szError = NStr::Format( "Script error: %s", obj.GetString() );
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, szError.c_str(), 0xffff0000, true );
	NStr::DebugTrace( "%s\n", szError.c_str() );
	return 0;
}
static int Sqrt( struct lua_State *pState )
{
	Script script( pState );
	script.PushNumber( sqrt( static_cast<double>( script.GetObject(1) ) ) );
	return 1;
}
static int IsBitSet( struct lua_State *pState )
{
	Script script( pState );
	script.PushNumber( script.GetObject( 1 ) & (1<<script.GetObject( 2 )) );
	return 1;
}
static int GetUserProfileVar( struct lua_State *state )
{
	Script script(state);
	NI_ASSERT_T( script.GetTop() == 2, "Script function must have 2 arguments on the stack" );			//два аргумента
	const std::string szStr = script.GetObject( -2 );
	const int nValue = script.GetObject( -1 );
	script.PushNumber( GetSingleton<IUserProfile>()->GetVar( szStr.c_str(), nValue ) );
	return 1;
}
static int SetUserProfileVar( struct lua_State *state )
{
	Script script(state);
	NI_ASSERT_T( script.GetTop() == 2, "Script function must have 2 arguments on the stack" );			//два аргумента
	const std::string szStr = script.GetObject( -2 );
	const int nValue = script.GetObject( -1 );
	GetSingleton<IUserProfile>()->AddVar( szStr.c_str(), nValue );
	return 0;
}
static int OutputStringValue( struct lua_State *state )
{
	Script script(state);
	NI_ASSERT_T( script.GetTop() == 2, "Script function must have 2 arguments on the stack" );			//два аргумента
	std::string szStr = script.GetObject( -2 );
	int nValue = script.GetObject( -1 );
	NStr::DebugTrace( "****Debug LUA script: %s %s\n", szStr.c_str(), nValue );
	return 0;
}
Script::SRegFunction reglist[] =
{
	{ "_ERRORMESSAGE"			,	ScriptErrorOut			},
	{ "OutputStringValue"	, OutputStringValue		},
	{ "FinishCampaign"		, FinishCampaign			},
	{ "AddNewSlot"				, AddNewSlot					},
	{ "ChangeDefault"			, ChangeDefault				},
	{ "ChangeCurrent"			, ChangeCurrent				},
	{ "AddUpgrade"				,	AddUpgrade					},
	{ "AddBaseUpgrade"		,	AddBaseUpgrade			},
	{ "RemoveBaseUpgrade"	,	RemoveBaseUpgrade		},
	{ "AddMedal"					,	AddMedal						},
	{ "EnableChapter"			, EnableChapter				},
	{ "EnableMission"			, EnableMission				},
	{ "SetIGlobalVar"			,	SetIGlobalVar				},
	{ "SetFGlobalVar"			,	SetFGlobalVar				},
	{ "SetSGlobalVar"			,	SetSGlobalVar				},
	{ "GetIGlobalVar"			,	GetIGlobalVar				},
	{ "GetFGlobalVar"			,	GetFGlobalVar				},
	{ "GetSGlobalVar"			,	GetSGlobalVar				},
	{ "GetSGlobalVar"			,	GetSGlobalVar				},
	{ "RandomFloat"				, RandomFloat					},
	{ "Sqrt"							,	Sqrt								},
	{ "GetStatisticsValue", GetStatisticsValue	},
	{ "HasMedal"					, HasMedal						},
	{ "GetNumMissions"		, GetNumMissions			},
	{ "IsBitSet"					, IsBitSet						},
	{ "SetUserProfileVar", SetUserProfileVar		},
	{ "GetUserProfileVar", GetUserProfileVar		},
	{ 0, 0 },
};
};
int CScenarioTracker2::SOpponentDesc::SRPGClassDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Class", &szClassName );
	saver.Add( "Names", &names );
	if ( saver.IsReading() ) 
	{
		NStr::ToLower( szClassName );
		if ( szClassName == "artillery" ) 
			eRPGClass = RPG_CLASS_ARTILLERY;
		else if ( szClassName == "tank" ) 
			eRPGClass = RPG_CLASS_TANK;
		else
			eRPGClass = RPG_CLASS_UNKNOWN;
		for ( std::vector<std::string>::iterator it = names.begin(); it != names.end(); ++it )
			NStr::ToLower( *it );
	}
	return 0;
}
int CScenarioTracker2::SOpponentDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Side", &szSide );
	saver.Add( "Classes", &classes );
	if ( saver.IsReading() ) 
		NStr::ToLower( szSide );
	return 0;
}
bool CScenarioTracker2::LoadChapterScript( const std::string &szScriptFileName )
{
	if ( pChapterScript ) 
		delete pChapterScript;
	pChapterScript = 0;
	if ( (szScriptFileName != "") && !szScriptFileName.empty() )
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( (szScriptFileName + ".lua").c_str(), STREAM_ACCESS_READ );
		if ( pStream )
		{
			pChapterScript = new Script;
			pChapterScript->Register( NScenarioScript2::reglist );
			const int nSize = pStream->GetSize();
			std::vector<char> buffer( nSize + 10 );
			pStream->Read( &(buffer[0]), nSize );
			if ( pChapterScript->DoBuffer( &(buffer[0]), nSize, "Script" ) == 0 ) 
			{
				szChapterScriptFileName = szScriptFileName;
				return true;
			}
			else
			{
				delete pChapterScript;
				pChapterScript = 0;
				szChapterScriptFileName.clear();
				NI_ASSERT_T( false, NStr::Format("Can't execute chapter script \"%s\"", szScriptFileName.c_str()) );
				return false;
			}
		}
		else
			return false;
	}
	else
		return false;
}
void CScenarioTracker2::ProcessScriptChanges( const bool bPostMission )
{
	using namespace NScenarioScript2;
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	for ( std::list<std::string>::const_iterator it = szNewSlots.begin(); it != szNewSlots.end(); ++it )
	{
		if ( CheckRPGStats(*it) == false )
			continue;
		CScenarioUnit *pUnit = pUserPlayer->AddNewSlot( *it );
		AssignBestPersonalName( pUnit, pUserPlayer->GetGeneralSide() );
		NStr::DebugTrace( "*** ScenarioScript: New slot \"%s\" added with name \"%s\"\n", it->c_str(), pUnit->GetPersonalNameFileName().c_str() );
	}
	if ( bPostMission ) 
		pUserPlayer->GetMissionStats()->SetValue( STMT_NEW_UNITS, szNewSlots.size() );
	szNewSlots.clear();
	NI_ASSERT_SLOW_T( newUpgrades.size() <= 1, "Only one upgrade supported by interface at a time. using first" );
	if ( !newUpgrades.empty() && CheckRPGStats(newUpgrades.front().szName) ) 
	{
		pUserPlayer->SetUpgrade( newUpgrades.front().szName );
		NStr::DebugTrace( "*** ScenarioScript: New upgrade set to \"%s\"\n", newUpgrades.front().szName.c_str() );
	}
	else
	{
		pUserPlayer->SetUpgrade( "" );
		NStr::DebugTrace( "*** ScenarioScript: No upgrades were set\n" );
	}
	newUpgrades.clear();
	for ( std::list<std::string>::const_iterator it = newBaseUpgrades.begin(); it != newBaseUpgrades.end(); ++it )
	{
		if ( CheckRPGStats(*it) == false )
			continue;
		pUserPlayer->AddDepotUpgrade( *it );
		NStr::DebugTrace( "*** ScenarioScript: New depot upgrade \"%s\" added\n", it->c_str() );
	}
	newBaseUpgrades.clear();
	for ( std::list<std::string>::const_iterator it = newRemoveBaseUpgrades.begin(); it != newRemoveBaseUpgrades.end(); ++it )
	{
		if ( CheckRPGStats(*it) == false )
			continue;
		pUserPlayer->RemoveDepotUpgrade( *it );
		NStr::DebugTrace( "*** ScenarioScript: Old depot upgrade \"%s\" removed\n", it->c_str() );
	}
	newRemoveBaseUpgrades.clear();
	for ( std::list<SChanges>::const_iterator it = changeCurrents.begin(); it != changeCurrents.end(); ++it )
	{
		if ( !CheckRPGStats(it->szOldValue) || !CheckRPGStats(it->szNewValue) )
			continue;
		for ( int i = 0; i < pUserPlayer->GetNumUnits(); ++i )
		{
			IScenarioUnit *pUnit = pUserPlayer->GetUnit( i );
			if ( pUnit->GetRPGStats() == it->szOldValue )
			{
				pUnit->ChangeRPGStats( it->szNewValue );
				NStr::DebugTrace( "*** ScenarioScript: Current changed from \"%s\" to \"%s\"\n", it->szOldValue.c_str(), it->szNewValue.c_str() );
			}
		}
	}
	changeCurrents.clear();
	for ( std::list<SMedal>::const_iterator it = newMedals.begin(); it != newMedals.end(); ++it )
	{
		pUserPlayer->AddMedal( it->first, it->second );
		NStr::DebugTrace( "*** ScenarioScript: new medal \"%s\" added at slot %d\n", it->first.c_str(), it->second );
	}
	newMedals.clear();
}
bool RecalcPlayerSkills( CPlayerScenarioInfo *pPlayer )
{
	if ( GetGlobalVar("TutorialMode", 0) != 0 ) // don't recalc player skills in tutorial mode
		return true;
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "scenarios\\scripts\\player_skills_recalc.lua", STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, "Can't find script file \"Scenarios\\Scripts\\player_skills_recalc.lua\"" );
	if ( pStream == 0 ) 
		return false;
	Script script;
	script.Register( NScenarioScript2::reglist );
	const int nSize = pStream->GetSize();
	std::vector<char> buffer( nSize + 10 );
	pStream->Read( &(buffer[0]), nSize );
	if ( script.DoBuffer( &(buffer[0]), nSize, "Script" ) == 0 ) 
	{
		const int oldtop = script.GetTop();

		IScenarioStatistics *pStats = pPlayer->GetCampaignStats();
		IScenarioStatistics *pStatsMission = pPlayer->GetMissionStats();

		const std::string szToRun = 
			NStr::Format("return RecalcSkills( %f, %f, %f, %f, %f, %f,     %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,      %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d )", 

			pPlayer->GetSkill( 0 ).fValue,
			pPlayer->GetSkill( 1 ).fValue,
			pPlayer->GetSkill( 2 ).fValue,
			pPlayer->GetSkill( 3 ).fValue,
			pPlayer->GetSkill( 4 ).fValue,
			pPlayer->GetSkill( 5 ).fValue,


			pStatsMission->GetValue( STMT_ENEMY_KILLED ),
			pStatsMission->GetValue( STMT_ENEMY_MACHINERY_CAPTURED ),
			pStatsMission->GetValue( STMT_FRIENDLY_KILLED ),

			pStatsMission->GetValue( STMT_UNITS_RETURN_AFTER_DAMAGE ),
			pStatsMission->GetValue( STMT_RESOURCES_USED ),
			pStatsMission->GetValue( STMT_AVIATION_CALLED ),
			
			pStatsMission->GetValue( STMT_HOUSES_DESTROYED ),
			pStatsMission->GetValue( STMT_UNITS_LEVELED_UP ),
			pStatsMission->GetValue( STMT_OBJECTIVES_COMPLETED ),
			
			pStatsMission->GetValue( STMT_OBJECTIVES_FAILED ),
			pStatsMission->GetValue( STMT_TIME_ELAPSED ),
			pStatsMission->GetValue( STMT_GAME_LOADED ),


			pStats->GetValue( STMT_ENEMY_KILLED ),
			pStats->GetValue( STMT_ENEMY_MACHINERY_CAPTURED ),
			pStats->GetValue( STMT_FRIENDLY_KILLED ),

			pStats->GetValue( STMT_UNITS_RETURN_AFTER_DAMAGE ),
			pStats->GetValue( STMT_RESOURCES_USED ),
			pStats->GetValue( STMT_AVIATION_CALLED ),
			
			pStats->GetValue( STMT_HOUSES_DESTROYED ),
			pStats->GetValue( STMT_UNITS_LEVELED_UP ),
			pStats->GetValue( STMT_OBJECTIVES_COMPLETED ),
			
			pStats->GetValue( STMT_OBJECTIVES_FAILED ),
			pStats->GetValue( STMT_TIME_ELAPSED ),
			pStats->GetValue( STMT_GAME_LOADED ) );
		
		script.DoString( szToRun.c_str() );


		const int nNumRetArgs = script.GetTop();
		NI_ASSERT_T( nNumRetArgs == 6, NStr::Format( "script returned %d skills instead of 6 required", nNumRetArgs ) );
		NI_ASSERT_T( pPlayer != 0, "no user player" );
		for ( int i = 1; i <= nNumRetArgs; ++i )
		{	
			Script::Object obj = script.GetObject( i );
			NI_ASSERT_T( obj.IsNumber(), "recalc skills returned not a number" );
			const double fSkill = obj.GetNumber();
			pPlayer->SetSkill( i - 1, fSkill );
		}

		script.SetTop( oldtop );
	}

	return true;
}
CScenarioTracker2::CScenarioTracker2()
{
	eCampaignType = CAMPAIGN_TYPE_UNKNOWN;
	pChapterScript = 0;
	nUserPlayerID = -1;
	Zero( guidMission );
}
bool CScenarioTracker2::Init( ISingleton *pSingleton )
{
	Zero( guidMission );
	randomBonuses.resize( 3 ); //по количеству сложностей рандомных миссий
	return true;
}
IPlayerScenarioInfo* CScenarioTracker2::AddPlayer( const int nPlayerID )
{
	NI_ASSERT_SLOW_T( !szCurrCampaign.empty() && eCampaignType != CAMPAIGN_TYPE_UNKNOWN, "Can't add player - start campaign first!" );
	NI_ASSERT_SLOW_T( GetPlayer(nPlayerID) == 0, NStr::Format("Player %d already exist", nPlayerID) );
	CPlayerScenarioInfo *pPlayer = CreateObject<CPlayerScenarioInfo>( MAIN_PLAYER_SCENARIO_INFO );
	pPlayer->Init();
	players.push_back( CPlayerScenarioInfoPair(nPlayerID, pPlayer) );
	CCampaignStatistics *pCampaign = CreateObject<CCampaignStatistics>( MAIN_CAMPAIGN_STATISTICS );
	pCampaign->SetName( szCurrCampaign, eCampaignType );
	pPlayer->StartCampaign( pCampaign );
	return pPlayer;
}
bool CScenarioTracker2::RemovePlayer( const int nPlayerID )
{
	if ( nPlayerID == -1 ) 
	{
		players.clear();
		return true;
	}
	for ( CPlayersList::iterator it = players.begin(); it != players.end(); ++it )
	{
		if ( it->first == nPlayerID ) 
		{
			players.erase( it );
			return true;
		}
	}
	return false;
}
IPlayerScenarioInfo* CScenarioTracker2::GetPlayer( const int nPlayerID ) const
{
	for ( CPlayersList::const_iterator it = players.begin(); it != players.end(); ++it )
	{
		if ( it->first == nPlayerID ) 
			return it->second;
	}
	return 0;
}
void CScenarioTracker2::SetUserPlayer( const int nPlayerID )
{
	pUserPlayer = 0;
	nUserPlayerID = -1;
	for ( CPlayersList::iterator it = players.begin(); it != players.end(); ++it )
	{
		if ( it->first == nPlayerID ) 
		{
			pUserPlayer = it->second;
			nUserPlayerID = nPlayerID;
			return;
		}
	}
	NI_ASSERT_SLOW_T( false, NStr::Format("Can't set player %d as a user player - such player still not exist", nPlayerID) );
}
IPlayerScenarioInfo* CScenarioTracker2::GetUserPlayer() const
{
	return pUserPlayer;
}
int CScenarioTracker2::GetUserPlayerID() const
{
	return nUserPlayerID;
}
IPlayerScenarioInfoIterator* CScenarioTracker2::CreatePlayerScenarioInfoIterator() const
{
	return new CPlayerScenarioInfoIterator( players );
}
void CScenarioTracker2::LoadOpponents() const
{
	if ( !opponents.empty() ) 
		return;
	if ( CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream("textes\\personals\\personals.xml", STREAM_ACCESS_READ) )
	{
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
		saver.Add( "Opponents", &opponents );
	}
}
std::string CScenarioTracker2::GetBestPersonalName( const std::string &szRPGStats, const std::string &szSide ) const
{
	LoadOpponents();
	for ( std::vector<SOpponentDesc>::const_iterator opponent = opponents.begin(); opponent != opponents.end(); ++opponent )
	{
		if ( opponent->szSide == szSide )
		{
			const SUnitBaseRPGStats *pRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>( szRPGStats.c_str() );
			const EUnitRPGClass eRPGClass = pRPG->GetRPGClass();
			for ( std::vector<SOpponentDesc::SRPGClassDesc>::const_iterator it = opponent->classes.begin(); it != opponent->classes.end(); ++it )
			{
				if ( it->eRPGClass == eRPGClass )
				{
					std::list<SNameUsageStats> usages;
					for ( std::vector<std::string>::const_iterator name = it->names.begin(); name != it->names.end(); ++name )
					{
						usages.push_back( SNameUsageStats() );
						usages.back().szName = *name;
						CNamesUsageMap::const_iterator posNameUsage = personalNamesUsage.find( *name );
						if ( posNameUsage != personalNamesUsage.end() ) 
							usages.back() = posNameUsage->second;
					}
					usages.sort();
					return usages.front().szName;
				}
			}
		}
	}
	return "";
}
void CScenarioTracker2::AssignBestPersonalName( CScenarioUnit *pUnit, const std::string &szSide )
{
	std::string szNewPersonalName = GetBestPersonalName( pUnit->GetRPGStats(), szSide );
	NStr::ToLower( szNewPersonalName );

	personalNamesUsage[pUnit->GetPersonalNameFileName()].nUsage--;
	pUnit->SetPersonalName( szNewPersonalName );
	SNameUsageStats &namestats = personalNamesUsage[szNewPersonalName];
	namestats.szName = szNewPersonalName;
	namestats.timeLastUsage = GetSingleton<IGameTimer>()->GetAbsTime();
	namestats.nUsedCounter++;
	namestats.nUsage++;
}
int Difficulty2Int( const std::string &szProgName )
{
	if ( szProgName == "Easy" )
		return 0;
	else if ( szProgName == "Normal" )
		return 1;
	else if ( szProgName == "Hard" )
		return 2;
	else if ( szProgName == "Ironman" )
		return 3;
	
	return 0;
}
const std::string &CScenarioTracker2::GetMinimumDifficulty() const
{
	return szMinimumDifficulty;
}
void CScenarioTracker2::InitMinimumDifficulty()
{
	variant_t var;
	GetSingleton<IOptionSystem>()->Get( "GamePlay.Difficulty", &var );
	szMinimumDifficulty = (const char *)bstr_t(var);
}
void CScenarioTracker2::UpdateMinimumDifficulty()
{
	if ( szMinimumDifficulty.empty() )
		InitMinimumDifficulty();

	variant_t var;
	GetSingleton<IOptionSystem>()->Get( "GamePlay.Difficulty", &var );
	const std::string szNewMinimumDifficulty = (const char *)bstr_t(var);
	const int nNewDifficulty = Difficulty2Int( szNewMinimumDifficulty );

	const int nDifficulty = Difficulty2Int( szMinimumDifficulty );
	if ( nNewDifficulty < nDifficulty )
		szMinimumDifficulty = szNewMinimumDifficulty;
}
void CScenarioTracker2::StartCampaign( const std::string &_szCampaignName, const ECampaignType eType )
{
	pUserPlayer = 0;
	players.clear();
	szCurrCampaign = _szCampaignName;
	NStr::ToLower( szCurrCampaign );
	eCampaignType = eType;
	templateMissions.clear();
	const SCampaignStats *pStats = NGDB::GetGameStats<SCampaignStats>( szCurrCampaign.c_str(), IObjectsDB::CAMPAIGN );
	if ( pStats )
	{
		templateMissions = pStats->templateMissions;
		for ( std::vector<std::string>::const_iterator it = templateMissions.begin(); it != templateMissions.end(); ++it )
		{
			NI_ASSERT_T( NGDB::GetGameStats<SMissionStats>(it->c_str(), IObjectsDB::MISSION) != 0, NStr::Format("Chaeck - can't load mission stats \"%s\"", it->c_str()) );
		}
	}
	szCurrChapter.clear();
	szCurrMission.clear();
	if ( eType != CAMPAIGN_TYPE_MULTIPLAYER ) 
	{
		ITextManager *pTM = GetSingleton<ITextManager>();
		if ( IPlayerScenarioInfo *pPlayer = AddPlayer(0) )
		{
			SetUserPlayer( 0 );
			pPlayer->SetDiplomacySide( 0 );
			if ( pStats )
				pPlayer->SetSide( pStats->szSideName );
			variant_t varPlayerName;
			GetSingleton<IOptionSystem>()->Get( "GamePlay.PlayerName", &varPlayerName );
			pPlayer->SetName( (wchar_t*)bstr_t(varPlayerName) );
		}
		if ( IPlayerScenarioInfo *pPlayer = AddPlayer(1) ) 
		{
			pPlayer->SetDiplomacySide( 1 );
			CPtr<IText> pText = pTM->GetDialog( "textes\\opponents\\enemy" );
			NI_ASSERT_T( pText != 0, "Text \"textes\\opponents\\enemy\" with enemy player name doesn't exist" );
			if ( pText )
				pPlayer->SetName( MakeWideStringFromWordString( pText->GetString() ) );
			pPlayer->SetSide( "enemy" );
		}
		if ( IPlayerScenarioInfo *pPlayer = AddPlayer(2) ) 
		{
			pPlayer->SetDiplomacySide( 2 );
			CPtr<IText> pText = pTM->GetDialog( "textes\\opponents\\neutral" );
			NI_ASSERT_T( pText != 0, "Text \"textes\\opponents\\neutral\" with neutral player name doesn't exist" );
			if ( pText )
				pPlayer->SetName( MakeWideStringFromWordString( pText->GetString() ) );
			pPlayer->SetSide( "neutral" );
		}
	}
	InitMinimumDifficulty();
}
bool CScenarioTracker2::StartChapter( const std::string &_szChapterName )
{
	std::string szChapter = _szChapterName;
	NStr::ToLower( szChapter );
	{
		const std::string szVarName = NStr::Format( "Chapter.%s.Visited", szChapter.c_str() );
		const int nNumVisited = GetGlobalVar( szVarName.c_str(), 0 );
		SetGlobalVar( szVarName.c_str(), nNumVisited + 1 );
	}
	if ( szCurrChapter == szChapter ) 
		return false;
	RemoveGlobalVar( "Chapter.IsFirst" );
	if ( !szCurrChapter.empty() ) 
	{
		const std::string szVarName = NStr::Format( "Chapter.%s.Status", szCurrChapter.c_str() );
		SetGlobalVar( szVarName.c_str(), 2 );
	}
	szCurrChapter = szChapter;
	NI_ASSERT_SLOW_T( !players.empty(), "Can't start chapter - no players added" );
	for ( CPlayersList::iterator it = players.begin(); it != players.end(); ++it )
	{
		CChapterStatistics *pChapter = CreateObject<CChapterStatistics>( MAIN_CHAPTER_STATISTICS );
		pChapter->SetName( szCurrChapter );
		it->second->StartChapter( pChapter );
	}
	if ( const SChapterStats *pStats = NGDB::GetGameStats<SChapterStats>(szCurrChapter.c_str(), IObjectsDB::CHAPTER) )
	{
		if ( LoadChapterScript(pStats->szScript) != false )
		{
			pChapterScript->DoString( NStr::Format("return EnterChapter(\"%s\")", szCurrChapter.c_str()) );
			const int nNumRetArgs = pChapterScript->GetTop();
			pChapterScript->Pop( nNumRetArgs );
			ProcessScriptChanges( false );
		}
		else
		{
			const std::string szError = NStr::Format( "Can't load chapter script from file \"%s\"", pStats->szScript.c_str() );
			GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, szError.c_str(), 0xffff0000, true );
		}
	}
	return true;
}
void CScenarioTracker2::StartMission( const std::string &szMissionName )
{
	NI_ASSERT_SLOW_T( !players.empty(), "Can't start mission - no players added" );
	szCurrMission = szMissionName;
	if ( CoCreateGuid(&guidMission) != S_OK ) 
		Zero( guidMission );	
	NStr::ToLower( szCurrMission );
	for ( CPlayersList::iterator it = players.begin(); it != players.end(); ++it )
	{
		CMissionStatistics *pMission = CreateObject<CMissionStatistics>( MAIN_MISSION_STATISTICS );
		pMission->SetName( szMissionName );
		it->second->StartMission( pMission );
	}
	const int nOurSide = pUserPlayer->GetDiplomacySide();
	int nAlliedCounter = 0;
	int nEnemyCounter = 0;
	for ( CPlayersList::iterator player = players.begin(); player != players.end(); ++player )
	{
		DWORD dwColor;
		if ( player->second.GetPtr() == pUserPlayer.GetPtr() )
			dwColor = GetGlobalVar( "Scene.PlayerColors.Player", int(0xff000000) );
		else if ( nOurSide == 2 || player->second->GetDiplomacySide() == 2 )
			dwColor = GetGlobalVar( "Scene.PlayerColors.Neutral", int(0xff808080) );			
		else if ( nOurSide == player->second->GetDiplomacySide() )
		{
			const std::string szVarName = NStr::Format( "Scene.PlayerColors.Allied%d", nAlliedCounter + 1 );
			dwColor = GetGlobalVar( szVarName.c_str(), int(0xff00ffff) );			
			nAlliedCounter = ( nAlliedCounter + 1 ) % 4;
		}
		else 
		{
			const std::string szVarName = NStr::Format( "Scene.PlayerColors.Enemy%d", nEnemyCounter + 1 );
			dwColor = GetGlobalVar( szVarName.c_str(), int(0xffff0000) );					
			nEnemyCounter = ( nEnemyCounter + 1 ) % 4;
		}
		const std::string szVarName = NStr::Format( "Scene.PlayerColors.Player%d", player->first );
		SetGlobalVar( szVarName.c_str(), int(dwColor) );
		player->second->SetColor( dwColor );
	}
}
void CScenarioTracker2::FinishMission( const EMissionFinishStatus eStatus )
{
	{
		int nMissionTime = 0;
		
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		const float fTimeCoeff = GetGlobalVar( "MultiplayerGame", 0 ) ? NTimer::GetCoeffFromSpeed( (float)pTimer->GetSpeed() ) : 1.0f;

		const NTimer::STime timeStart = GetGlobalVar( "Mission.Current.StartTime", -1 );
		const NTimer::STime timeFinish = pTimer->GetGameTime();
		if ( (timeStart != -1) && (timeStart <= timeFinish) ) 
			nMissionTime = int( double(timeFinish - timeStart) / (60.0 * 1000.0) / fTimeCoeff );
		for ( CPlayersList::iterator player = players.begin(); player != players.end(); ++player )
		{
			player->second->GetMissionStats()->SetValue( STMT_TIME_ELAPSED, nMissionTime );
		}
	}
	if ( pUserPlayer ) 
	{
		if ( IMissionStatistics *pStats = pUserPlayer->GetMissionStats() )
		{
			const SMissionStats *pDesc = NGDB::GetGameStats<SMissionStats>( szCurrMission.c_str(), IObjectsDB::MISSION );
			int nGiven = 0;
			if ( pDesc )
			{
				for ( int i = 0; i < pDesc->objectives.size(); ++i )
				{
					if ( !pDesc->objectives[i].bSecret )// count visible objectives
						++nGiven;
					else
					{
						const std::string szVarName = NStr::Format( "temp.%s.objective%d", szCurrMission.c_str(), i );
						nGiven += -1 != GetGlobalVar( szVarName.c_str(), -1 );						
					}
				}
			}

			pStats->AddValue( STMT_OBJECTIVES_RECIEVED, nGiven );
			const int nNumLoads = GetSingleton<IUserProfile>()->GetLoadCounter( guidMission );
			pStats->AddValue( STMT_GAME_LOADED, nNumLoads );
			
			pStats->AddValue( STMT_OBJECTIVES_FAILED, pStats->GetValue(STMT_OBJECTIVES_RECIEVED) - pStats->GetValue(STMT_OBJECTIVES_COMPLETED) - pStats->GetValue(STMT_OBJECTIVES_FAILED));
		}
	}	

	RemoveGlobalVar( "Mission.Current.Random" );
	switch ( eStatus ) 
	{
		case MISSION_FINISH_ABORT:
		case MISSION_FINISH_WIN:
			{
				const int nNumFinishedMissions = GetGlobalVar( "Mission.Finished.Counter", 0 );
				SetGlobalVar( "Mission.Finished.Counter", nNumFinishedMissions + 1 );
				const std::string szVarName = NStr::Format( "Mission.Finished.%d", nNumFinishedMissions );
				SetGlobalVar( szVarName.c_str(), szCurrMission.c_str() );
				SetGlobalVar( (szVarName + ".ChapterName").c_str(), szCurrChapter.c_str() );
				SetGlobalVar( ("Mission." + szCurrMission + ".Finished").c_str(), 1 );
				const int nRandom = GetGlobalVar( ("Mission." + szCurrMission + ".Random").c_str(), 0 );
				SetGlobalVar( "Mission.Current.Random", nRandom );
			}
			if ( eStatus == MISSION_FINISH_WIN ) 
			{
				const std::string szMissionBonus = GetGlobalVar( "Mission.Current.Bonus", "" );
				if ( !szMissionBonus.empty() ) 
				{
					CheckRPGStats( szMissionBonus );
					const SUnitBaseRPGStats *pRPGStats = NGDB::GetRPGStats<SUnitBaseRPGStats>( szMissionBonus.c_str() );
					NScenarioScript2::newUpgrades.push_back( NScenarioScript2::SUpgrades(szMissionBonus, pRPGStats->GetRPGClass()) );
				}
			}
			else
			{
				RemoveGlobalVar( "Mission.Current.Bonus" );
				pUserPlayer->SetUpgrade( "" );
			}
			SetGlobalVar( "Mission.Last.FinishStatus", MISSION_FINISH_WIN );
			{
				std::list< CPtr<CScenarioUnit> > kiaUnits;
				for ( int i = 0; i < pUserPlayer->GetNumUnits(); ++i )
				{
					CScenarioUnit *pUnit = checked_cast<CScenarioUnit*>( pUserPlayer->GetUnit(i) );
					if ( !pUnit->IsKilled() ) 
						continue;
					kiaUnits.push_back( pUnit );
				}
				if ( !kiaUnits.empty() ) 
				{
					LoadOpponents();
					const float fReincarnateProbability = GetGlobalVar( "World.ReincarnateProbability", 0.5f );
					for ( std::list< CPtr<CScenarioUnit> >::iterator it = kiaUnits.begin(); it != kiaUnits.end(); ++it )
					{
						const std::string szOldKIAName = (*it)->GetPersonalNameFileName();
						const std::string szRPGStats = (*it)->GetRPGStats();
						(*it)->Reincarnate( NWin32Random::Random(0.0f, 1.0f) > fReincarnateProbability );
						AssignBestPersonalName( *it, pUserPlayer->GetGeneralSide() );
						const std::string szNewKIAName = (*it)->GetPersonalNameFileName();
						checked_cast<CMissionStatistics*>( pUserPlayer->GetMissionStats() )->AddKIAUnit( szOldKIAName, szNewKIAName, szRPGStats );
					}
				}
			}
			for ( CPlayersList::iterator player = players.begin(); player != players.end(); ++player )
				player->second->FinishMission( eStatus );
			RecalcPlayerSkills( pUserPlayer );
			if ( pUserPlayer ) 
				pUserPlayer->SetExperience( pUserPlayer->GetCampaignStats()->GetValue(STMT_PLAYER_EXPERIENCE) );
			
			if ( MISSION_FINISH_ABORT != eStatus ) // run scripts only if win, not abort
			{
				if ( pChapterScript ) 
				{
					if ( pUserPlayer && pUserPlayer->IsGainLevel() ) 
						pChapterScript->DoString( NStr::Format("return PlayerGainLevel(%d)", pUserPlayer->GetRankInfo().nRankNumber) );
					pChapterScript->DoString( NStr::Format("return MissionFinished(\"%s\")", szCurrMission.c_str()) );
					const int nNumRetArgs = pChapterScript->GetTop();
					pChapterScript->Pop( nNumRetArgs );
					ProcessScriptChanges( true );
				}
			}
			
			break;
		case MISSION_FINISH_LOSE:
		case MISSION_FINISH_RESTART:
			for ( CPlayersList::iterator player = players.begin(); player != players.end(); ++player )
				player->second->FinishMission( eStatus );
			break;
			
	}
	Zero( guidMission );
}
int CScenarioTracker2::GetNumRandomTemplates() const
{
	return templateMissions.size();
}
const std::string& CScenarioTracker2::GetTemplateName( const int nIndex ) const
{
	CheckRange( templateMissions, nIndex );
	return templateMissions[nIndex];
}
IScenarioTracker* CScenarioTracker2::Duplicate() const
{
	CScenarioTracker2 *pNewTracker = new CScenarioTracker2();

	pNewTracker->players = players;	// copy is OK
	pNewTracker->pUserPlayer = pUserPlayer;
	pNewTracker->nUserPlayerID = nUserPlayerID;
	pNewTracker->opponents = opponents; // copy is OK
	pNewTracker->personalNamesUsage = personalNamesUsage; // copy is OK
	pNewTracker->szChapterScriptFileName = szChapterScriptFileName;
	pNewTracker->szCurrCampaign = szCurrCampaign;
	pNewTracker->szCurrChapter = szCurrChapter;
	pNewTracker->szCurrMission = szCurrMission;
	pNewTracker->eCampaignType = eCampaignType;
	pNewTracker->templateMissions = templateMissions;
	pNewTracker->guidMission = guidMission;

	pNewTracker->LoadChapterScript( pNewTracker->szChapterScriptFileName.c_str() );

	return pNewTracker;
}
int CScenarioTracker2::operator&( IDataTree &ss )
{
	return 0;
}
int CScenarioTracker2::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &players );
	saver.Add( 2, &pUserPlayer );
	saver.Add( 4, &personalNamesUsage );
	saver.Add( 5, &szChapterScriptFileName );
	saver.Add( 6, &szCurrCampaign );
	saver.Add( 7, &szCurrChapter );
	saver.Add( 8, &szCurrMission );
	saver.Add( 9, &eCampaignType );
	saver.Add( 10, &templateMissions );
	saver.Add( 11, &guidMission );
	saver.Add( 12, &szMinimumDifficulty );
	saver.Add( 13, &randomBonuses );

	if ( saver.IsReading() ) 
	{
		LoadChapterScript( szChapterScriptFileName.c_str() );
		if ( randomBonuses.size() < 3 )
		{
			randomBonuses.resize( 3 );
		}
	}
	return 0;
}

void CScenarioTracker2::ClearRandomBonuses( int nDifficulty )
{
	if ( randomBonuses.size() < 3 )
	{
		randomBonuses.resize( 3 );
	}
	if ( ( nDifficulty >= 0 ) && ( nDifficulty < 3 ) )
	{
		randomBonuses[nDifficulty].clear();
	}
}

bool CScenarioTracker2::AddRandomBonus( int nDifficulty, const std::string &rszRandomBonus )
{
	if ( randomBonuses.size() < 3 )
	{
		randomBonuses.resize( 3 );
	}
	if ( ( nDifficulty >= 0 ) && ( nDifficulty < 3 ) )
	{
		randomBonuses[nDifficulty].push_back( rszRandomBonus );
		return true;
	}
	return false;
}

std::string CScenarioTracker2::GetRandomBonus( int nDifficulty )
{
	if ( randomBonuses.size() < 3 )
	{
		randomBonuses.resize( 3 );
	}
	std::string szBonus;
	if ( ( nDifficulty >= 0 ) && ( nDifficulty < 3 ) )
	{
		if ( !randomBonuses[nDifficulty].empty() )
		{
			szBonus = randomBonuses[nDifficulty].back();
			randomBonuses[nDifficulty].pop_back();
		}
	}
	return szBonus;
}
