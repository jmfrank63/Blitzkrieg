#include "StdAfx.h"

#include "InterfaceAfterMissionPopups.h"
#include "..\Main\ScenarioTracker.h"
#include "..\Main\RPGStats.h"
#include "..\Main\GameStats.h"
#include "CutScenesHelper.h"
#include "UIConsts.h"
#include "..\Main\ScenarioTrackerTypes.h"
BASIC_REGISTER_CLASS( CAfterMissionPopups );
int CAfterMissionPopups::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &bTutorialWindow );
	saver.Add( 2, &bCheckReplay );
	saver.Add( 3, &nMedalIterator );
	saver.Add( 4, &bMainScreenShown );
	saver.Add( 5, &bUpgradesShown );
	saver.Add( 6, &bNewUnitsShown );
	saver.Add( 7, &bNextChapterShown );
	saver.Add( 8, &bPlayerRankShown );
	saver.Add( 9, &bLastFullScreen );
	saver.Add( 10, &bNeedFinish );
	saver.Add( 11, &nCommandID );
	saver.Add( 12, &szCommandParams );
	saver.Add( 13, &bUnitsPerformanceShown );
	return 0;
}
void CAfterMissionPopups::FinishInterface( const int _nCommandID, const char *pszParam )
{
	bNeedFinish = true;
	nCommandID = _nCommandID;
	if ( pszParam )
		szCommandParams = pszParam;
	else
		szCommandParams.clear();
}
void CAfterMissionPopups::OnGetFocus( bool bFocus )
{
	bNeedFinish = false;
	nCommandID = 0;
	szCommandParams.clear();

	if ( !bFocus ) 
		return;
	

	/*if ( !bMainScreenShown )
	{
		bMainScreenShown = 1;
		return;
	}*/
	
	/*if ( bTutorialWindow )
	{
		bTutorialWindow = false;
		return;
	}*/
	
	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	IPlayerScenarioInfo *pUserPlayer = pST->GetUserPlayer();
	IMainLoop *pML = GetSingleton<IMainLoop>();
	
	/*
	*/

	bool bMultiplayerGame = GetGlobalVar( "MultiplayerGame", 0 );
	if ( bMultiplayerGame )
	{
		FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );
		return;
	}

	if ( nMedalIterator < pUserPlayer->GetNumNewMedals() )
	{
		const std::string &szMedalName = pUserPlayer->GetNewMedal( nMedalIterator++ );
		pML->Command( MISSION_COMMAND_SINGLE_MEDAL, szMedalName.c_str() );
		bLastFullScreen = false;
		return;
	}

	if ( !bPlayerRankShown )
	{
		bPlayerRankShown = true;
		if ( pUserPlayer->IsGainLevel() )
		{
			pML->Command( MISSION_COMMAND_PLAYER_GAIN_LEVEL, 0 );
			bLastFullScreen = false;
			return;
		}
	}
	
	if ( !bMainScreenShown )
	{
		bMainScreenShown = true;
		return;
	}
	
	std::string szMissionName = GetGlobalVar( "Mission.Current.Name", "" );
	if ( szMissionName.size() > 0 )
	{
		NStr::ToLower( szMissionName );
		std::string szVarName = "Mission.";
		szVarName += szMissionName;
		szVarName += ".Finished";
		if ( GetGlobalVar( szVarName.c_str(), -1 ) == 1 )
		{
			if ( !bUnitsPerformanceShown )
			{
				bUnitsPerformanceShown = true;
				IMissionStatistics * pStat = pUserPlayer->GetMissionStats();
				bool bNeedShow = pStat->GetNumKIA();

				IPlayerScenarioInfo *pPlayerInfo = GetSingleton<IScenarioTracker>()->GetUserPlayer();
				const int nPlayerUnits = pPlayerInfo->GetNumUnits();
				if ( !bNeedShow )
				{
					for ( int i = 0; i < nPlayerUnits; ++i )
					{
						IScenarioUnit * pUnit = pPlayerInfo->GetUnit( i );
						const int nDiff = pUnit->GetValueDiff( STUT_LEVEL );
						if ( nDiff > 0 )
						{
							bNeedShow = true;
							break;
						}
					}
				}
				
				if ( bNeedShow )
				{
					FinishInterface( MISSION_COMMAND_UNIT_PERFORMANCE, "1" );
					bShowBlack = true;
					bLastFullScreen = true;
					return;
				}
			}
			if ( !bNewUnitsShown )
			{
				bNewUnitsShown = true;
				if ( pUserPlayer->GetNumNewUnits() > 0 )
				{
					FinishInterface( MISSION_COMMAND_UNITS_POOL, "1" );
					bShowBlack = true;
					bLastFullScreen = true;
					return;
				}
			}
			
			if ( !bUpgradesShown )
			{
				bUpgradesShown = true;
				const std::string &szUpgrade = pUserPlayer->GetUpgrade();
				if ( !szUpgrade.empty() ) 
				{
					if ( const SUnitBaseRPGStats *pUpgradeRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>(szUpgrade.c_str()) )
					{
						for ( int i = 0; i < pUserPlayer->GetNumUnits(); ++i )
						{
							const std::string &szUnitRPGStatsName = pUserPlayer->GetUnit(i)->GetRPGStats();
							const SUnitBaseRPGStats *pUnitRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>( szUnitRPGStatsName.c_str() );
							if ( pUnitRPG->GetRPGClass() == pUpgradeRPG->GetRPGClass() ) 
							{
								FinishInterface( MISSION_COMMAND_UPGRADE_UNIT, 0 );
								bLastFullScreen = true;
								return;
							}
						}
					}
				}
			}
		}
	}

	bShowBlack = false;
	std::string szNewChapter = GetGlobalVar( "Chapter.New.Available", "" );
	if ( szNewChapter.size() > 0 && !bNextChapterShown )
	{
		bNextChapterShown = true;
		std::string szSaveName;
		szSaveName += CUIConsts::GetCampaignNameAddition();
		szSaveName += " Chapter End Auto";
		szSaveName += ".sav";

		pML->Command( MAIN_COMMAND_SAVE, NStr::Format( "%s;1", szSaveName.c_str() ) );
		pML->Command( MISSION_COMMAND_NEXT_CHAPTER, 0 );
		bLastFullScreen = false;
		return;
	}

	const std::string szChapterName = GetGlobalVar( "Chapter.Current.Name", "" );
	{
		if ( bLastFullScreen )	// Если играем в кампанию, то загружаем chapter interface
		{
			SetGlobalVar( "CurtainsClosed", 1 );
			if ( GetGlobalVar( "NextChapter.Confirmed", 0 ) )
				pML->Command( MISSION_COMMAND_CAMPAIGN, 0 );
			else
				pML->Command( MISSION_COMMAND_CHAPTER, 0 );
			RemoveGlobalVar( "NextChapter.Confirmed" );
		}
		else	// последний экранчик был popup, закроем шторки
		{
			if ( GetGlobalVar( "NextChapter.Confirmed", 0 ) )
				FinishInterface( MISSION_COMMAND_CAMPAIGN, 0 );
			else
				FinishInterface( MISSION_COMMAND_CHAPTER, 0 );
			RemoveGlobalVar( "NextChapter.Confirmed" );

		}
		return;
	}
}