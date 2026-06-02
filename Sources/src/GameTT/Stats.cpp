#include "StdAfx.h"

#include "Stats.h"

#include "CommonId.h"
#include "UnitTypes.h"
#include "Campaign.h"
#include "MainMenu.h"

#include "..\Main\ScenarioTracker.h"
#include "..\Main\ScenarioTrackerTypes.h"
#include "..\Main\GameStats.h"
#include "..\Main\Transceiver.h"
#include "..\Main\RPGStats.h"
#include "..\UI\UIListSorter.h"
#include "MultiplayerCommandManager.h"
enum E_InterfaceConsts
{
	IMC_SAVE_REPLAY			= 10003,

	E_FRIENDLY_LIST_MP  = 1002,
	E_FRIENDLY_LIST_SP  = 1004,

	E_ENEMY_LIST_MP			= 1003,
	E_ENEMY_LIST_SP  =		1005,

	E_COMMON_STATS_LIST_SP	 = 1007,
	E_COMMON_STATS_LIST_MP	 = 1008,

	E_PARTY_NAME					= 5,
	E_ACTUAL_TEXT_ID_PARTY		= 3,
	E_ACTUAL_TEXT_ID		= 2,
	E_STAR_ID						= 1,
};
void CICStats::Configure( const char *pszConfig )
{
	if ( pszConfig != 0 && strlen(pszConfig) > 0 )
		nCurrentMissionStats = NStr::ToInt( pszConfig );
	else
		nCurrentMissionStats = 0;
}
void CICStats::PreCreate( IMainLoop *pML )
{
	const std::string szChapterName = GetGlobalVar( "Chapter.Current.Name", "" );
	if ( GetGlobalVar( "MultiplayerGame", 0 ) || szChapterName == "custom_mission" )
		pML->ResetStack();
}
void CICStats::PostCreate( IMainLoop *pML, CInterfaceStats *pIS )
{
	pIS->Create( nCurrentMissionStats );
	pML->PushInterface( pIS );
}
int CInterfaceStats::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		InitSorter();
	}
	saver.Add( 1, &playerStatsConfigure );
	saver.Add( 2, &playerInfos );
	saver.Add( 3, &pPartyList );

	saver.Add( 4, &bTutorialWindow );
	saver.Add( 5, &bCheckReplay );
	saver.Add( 6, &m_nStatsType );
	saver.Add( 7, &nMedalIterator );
	saver.Add( 8, &bStatsShown );
	saver.Add( 9, &bUpgradesShown );
	saver.Add( 10, &bNewUnitsShown );
	saver.Add( 11, &bNextChapterShown );
	saver.Add( 12, &bPlayerRankShown );
	saver.Add( 13, &bLastFullScreen );
	saver.AddTypedSuper( 14, static_cast<CInterfaceInterMission*>( this ) );
	return 0;
}
void CInterfaceStats::CCommonStats::Init ( const CPlayerFullInfo &info )
{
	nTime = info.second->GetValue( STMT_TIME_ELAPSED );
	nObjectivesCompleted = info.second->GetValue( STMT_OBJECTIVES_COMPLETED );
	nUpgrades = info.second->GetValue( STMT_UNITS_UPGRADED );
	nSaves = info.second->GetValue( STMT_GAME_LOADED );
}
int CInterfaceStats::CCommonStats::GetNStats( const bool bMultiplayer ) const
{
	return bMultiplayer ? 1 : 4;
}
std::string CInterfaceStats::CCommonStats::GetStatTitleKey( const int nIndex ) const
{
	switch( nIndex )
	{
		case 0: // time
			return "imheader-stats-timeelapsed";
		case 1: // completed obj
			return "imheader-stats-objcompleted";
		case 2: //failed obj
			return "Textes\\Options\\GamePlay.Difficulty.name";
		case 3: //saves
			return "imheader-stats-gameloaded";
		default:
			NI_ASSERT_T( false, NStr::Format( "wrong index %d", nIndex ) );
			return "";
	}
}
std::wstring CInterfaceStats::CCommonStats::GetStatValue( const int nIndex ) const
{
	switch( nIndex )
	{
		case 0: // time
			return NStr::ToUnicode( NStr::Format( "%02d:%02d", nTime / 60, nTime %60 ) );
		case 1: // completed obj
			return NStr::ToUnicode( NStr::Format( "%d", nObjectivesCompleted ) );
		case 2: // difficulty //failed obj
			{			//return NStr::ToUnicode( NStr::Format( "%d", nObjectivesFailed ) );
				std::string szKey = "Textes\\Options\\GamePlay.Difficulty";
				szKey += "\\";
				szKey += GetSingleton<IScenarioTracker>()->GetMinimumDifficulty();
				szKey += ".name";
				IText * pT = GetSingleton<ITextManager>()->GetDialog( szKey.c_str() );
				
				if ( !pT ) return L"";
				return pT->GetString();
			}
		case 3: //saves
			return NStr::ToUnicode( NStr::Format( "%d", nSaves ) );
		default:
			NI_ASSERT_T( false, NStr::Format( "wrong index %d", nIndex ) );
			return L"";
	}
}
bool CInterfaceStats::CSorter::operator() ( int nSortColumn, 
																						const IUIListRow *pRow1, 
																						const IUIListRow *pRow2, const bool bForward ) const
{

	const int nData1 = pRow1->GetUserData();
	const int nData2 = pRow2->GetUserData();
	if ( nData1 == 0 ) return true;
	if ( nData2 == 0 ) return false;

	IUIElement *pElement = pRow1->GetElement( nSortColumn );
	std::wstring wsz1 = pElement->GetWindowText( 0 );
	pElement = pRow2->GetElement( nSortColumn );
	std::wstring wsz2 = pElement->GetWindowText( 0 );
	std::string sz1 = NStr::ToAscii( wsz1 );
	std::string sz2 = NStr::ToAscii( wsz2 );
	double d1 = atof( sz1.c_str() );
	double d2 = atof( sz2.c_str() );
	if ( d1 == d2 )
	{
		return false;
	}
	else
	{
		return (bForward ? d1 > d2 : d1 < d2);
	}
}
CInterfaceStats::SStatConfugure::SStatConfugure(	const  int _eStatType,
																									const /*EPartyInfoType*/ int _eAccumulateType, 
																									const int _eType1, const int _eType2,
																									const int _nIndexToCountBest,
																									const bool bGreatest )

: eStatType( _eStatType ), nIndexToCountBest( _nIndexToCountBest ),
	eAccumulateType( _eAccumulateType ), bLeaderIsGreatest( bGreatest )
{ 
	eType[0] = _eType1;
	eType[1] = _eType2;
}
bool CInterfaceStats::SStatConfugure::IsToDisplay( const bool bMultiplayerGame ) const 
{ 
	return EST_BOTH == eStatType || (bMultiplayerGame ? EST_MULTIPLAYER == eStatType : EST_SINGLEPLAYER == eStatType); 
}
std::wstring CInterfaceStats::SPlayerStatInfo::GetValueToSort( const SStatConfugure &config ) const
{
	return NStr::ToUnicode( NStr::Format( "%d", int(fVal[config.nIndexToCountBest])) );
}
std::wstring CInterfaceStats::SPlayerStatInfo::GetValue( const SStatConfugure &config ) const
{
	if ( E_BEST_NO_PLAYER_DISPLAY == config.eAccumulateType )
	{
		return L"--";
	}
	if ( config.eType[1] != -1 )
	{
		return NStr::ToUnicode( NStr::Format( "%d(%d)", int(fVal[0]), int(fVal[1]) ) );
	}
	else
		return NStr::ToUnicode( NStr::Format( "%d", int(fVal[0]) ) );
}
void CInterfaceStats::SPlayerStatInfo::Init( const CPlayerFullInfo &playerInfo, const CInterfaceStats::SStatConfugure &config)
{
	for ( int i = 0; i < 2; ++i )
	{
		if ( config.eType[i] != -1 )
		{
			fVal[i] = playerInfo.second->GetValue( config.eType[i] );
			if ( config.eAccumulateType == E_SUMMARY_DEVIDED_BY_1000 )
				fVal[i] /= 1000;
		}
	}
}
std::wstring CInterfaceStats::SPartyInfo::GetValForSort( const SStatConfugure &config ) const
{
	return NStr::ToUnicode( NStr::Format( "%d", int(fVal[config.nIndexToCountBest]) ) );
}
std::wstring CInterfaceStats::SPartyInfo::GetPartyVal( const SStatConfugure &config ) const 
{ 
	if ( config.eType[1] != -1 )
	{
		return NStr::ToUnicode( NStr::Format( "%d(%d)", int(fVal[0]), int(fVal[1]) ) );
	}
	else
		return NStr::ToUnicode( NStr::Format( "%d", int(fVal[0]) ) );
}
void CInterfaceStats::SPartyInfo::Init( const CInterfaceStats::CPlayersFullInformation &info, const CInterfaceStats::SStatConfugure &config )
{
	fVal[0] = fVal[1] = 0;
	
	for ( int nPlayer = 0; nPlayer < info.size(); ++nPlayer )
	{
		NI_ASSERT_T( config.eType[0] != -1, "unitialized" );

		for ( int i = 0; i < 2; ++i )
		{
			if ( config.eType[i] != -1 )
			{
				const double fCurrentVal = info[nPlayer].second->GetValue( config.eType[i] );
				
				switch( config.eAccumulateType )
				{
				case E_SUMMARY:
					fVal[i] += fCurrentVal;
					break;
				case E_BEST_NO_PLAYER_DISPLAY:
					fVal[i] = fVal[i] > fCurrentVal ? fVal[i] : fCurrentVal;
					break;
				case E_SUMMARY_DEVIDED_BY_1000:
					fVal[i] += int(fCurrentVal / 1000.0f);
					break;
				default:
					NI_ASSERT_T( false, NStr::Format( "wrong party info type %d", config.eAccumulateType ) );
					return;
				}

				if ( i == config.nIndexToCountBest )
				{
					if ( (config.bLeaderIsGreatest ? fCurrentVal > fBestVal[i] : fCurrentVal < fBestVal[i]) ||
								nBestIndex < 0 )
					{
						fBestVal[i] = fCurrentVal;
						nBestIndex = nPlayer;
					}
				}
			}
		}
	}
}
CInterfaceStats::~CInterfaceStats()
{

}
bool CInterfaceStats::Init()
{
	CInterfaceInterMission::Init();

	bTutorialWindow = false;
	return true;
}
void CInterfaceStats::Done()
{
	CInterfaceInterMission::Done();
	if ( GetGlobalVar("History.Playing", 0) != 0 ) 
	{
		GetSingleton<IMainLoop>()->RestoreScenarioTracker();
		RemoveGlobalVar( "History.Playing" );
		RemoveGlobalVar( "MultiplayerGame" );

		UnRegisterSingleton( ITransceiver::tidTypeID );
		ITransceiver *pTrans = CreateObject<ITransceiver>( MAIN_SP_TRANSCEIVER );
		RegisterSingleton( ITransceiver::tidTypeID, pTrans );
	}
}
void CInterfaceStats::FillCommonStatsList( const bool bMultiplayer, const CInterfaceStats::CCommonStats &commonStats )
{
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( bMultiplayer ? E_COMMON_STATS_LIST_MP : E_COMMON_STATS_LIST_SP ) );
	ITextManager *pTM = GetSingleton<ITextManager>();

	for ( int i = 0; i < commonStats.GetNStats( bMultiplayer ); ++i )
	{
		pList->AddItem( i );
		const int nRowID = pList->GetItemByID( i );
		IUIListRow * pRow = pList->GetItem( nRowID );
		const std::string szKey = commonStats.GetStatTitleKey( i ).c_str();
		IText * pText = pTM->GetString( szKey.c_str() );
		if ( pText )
		{
			IUIElement * pActualText = pRow->GetElement( 0 );
			pActualText->SetWindowText( 0, pText->GetString() );
			pActualText->ShowWindow( UI_SW_SHOW );
		}		
		

		const std::wstring szValue = commonStats.GetStatValue( i );
		IUIElement * pActualText = checked_cast<IUIContainer*>(pRow->GetElement( 1 ))->GetChildByID( E_ACTUAL_TEXT_ID );
		pActualText->SetWindowText( 0, reinterpret_cast<const WORD*>( szValue.c_str() ) );

		pActualText->ShowWindow( UI_SW_SHOW );
	}
	pList->ShowWindow( UI_SW_SHOW );
	pList->InitialUpdate();
	
}
void CInterfaceStats::InitSorter()
{
	pSorter = new CSorter;
}
void CInterfaceStats::AquireLists()
{
	const bool bMultiplayer = GetGlobalVar( "MultiplayerGame", 0 );

	pPartyList[0] = checked_cast< IUIListControl *> ( pUIScreen->GetChildByID( bMultiplayer ? E_FRIENDLY_LIST_MP : E_FRIENDLY_LIST_SP ) );
	pPartyList[0]->ShowWindow( UI_SW_SHOW );
	pPartyList[0]->InitialUpdate();
	pPartyList[1] = checked_cast< IUIListControl *> ( pUIScreen->GetChildByID( bMultiplayer ? E_ENEMY_LIST_MP : E_ENEMY_LIST_SP ) );
}
void CInterfaceStats::RepositionList()
{
	const bool bMultiplayer = GetGlobalVar( "MultiplayerGame", 0 );
	AquireLists();
	std::vector< std::vector<SPartyInfo> > partyInfo(2);
	

	CCommonStats commonStats;
	commonStats.Init( playerInfos[0].empty() ? playerInfos[1][0] : playerInfos[0][0] );		// it doesn't matter what player to use in MP, in SP we must use local. [0][0] will fit
	FillCommonStatsList( bMultiplayer, commonStats );

	std::vector< std::vector<SPlayerStatInfo> > playerStat( playerInfos[0].size() + playerInfos[1].size() );
	const int nPlayersIn0Party = playerInfos[0].size();

	for ( int nParty = 0; nParty < 2; ++nParty )
	{
		for ( int nStatFeild = 0; nStatFeild < playerStatsConfigure.size(); ++nStatFeild )
		{
			partyInfo[nParty].push_back( SPartyInfo() );
			partyInfo[nParty][nStatFeild].Init( playerInfos[nParty], playerStatsConfigure[nStatFeild] );
		
			for ( int nPlayer = 0; nPlayer < playerInfos[nParty].size(); ++nPlayer )
			{
				playerStat[nPlayer + (nParty == 1 ? nPlayersIn0Party : 0 )].resize( playerStatsConfigure.size() ); // not nice, but will work
				playerStat[nPlayer + (nParty == 1 ? nPlayersIn0Party : 0 )][nStatFeild].Init( playerInfos[nParty][nPlayer], playerStatsConfigure[nStatFeild] );
			}
		}
	}

	for ( int nParty = 0; nParty < 2; ++nParty )
	{
		if ( playerInfos[nParty].empty() ) 
			continue;		// don't fill empty party's info

		const int nOppositeParty = 1 - nParty;
		IUIListControl *pList = pPartyList[nParty];
		const CPlayersFullInformation &playersInfo = playerInfos[nParty];
		
		pList->ShowWindow( UI_SW_SHOW );
		
		{
			pList->AddItem( 0 );
		
			const int nRowID = pList->GetItemByID( 0 );
			IUIListRow * pRow = pList->GetItem( nRowID );
			const IText * pSideName = playerInfos[nParty][0].first->GetSideName();
			IUIElement * pPartyName = checked_cast<IUIContainer*>(pRow->GetElement( 0 ))->GetChildByID( E_PARTY_NAME );
			pPartyName->SetWindowText( 0, pSideName->GetString() );
			
			int nIndex = 1;	// +1 because of party's name
			for ( int nStatFeild = 0; nStatFeild < playerStatsConfigure.size(); ++nStatFeild )
			{
				if ( playerStatsConfigure[nStatFeild].IsToDisplay( bMultiplayer ) )
				{
					pList->SetSortFunctor( nIndex, pSorter );
					IUIContainer * pElement = checked_cast<IUIContainer*>( pRow->GetElement( nIndex++ ) ); 
					pElement->SetWindowText( 0, reinterpret_cast<const WORD*>( partyInfo[nParty][nStatFeild].GetValForSort( playerStatsConfigure[nStatFeild] ).c_str() ) );
					IUIElement * pActualText = pElement->GetChildByID( E_ACTUAL_TEXT_ID_PARTY );
					pActualText->SetWindowText( 0, reinterpret_cast<const WORD*>( partyInfo[nParty][nStatFeild].GetPartyVal( playerStatsConfigure[nStatFeild] ).c_str() ) );
					pActualText->ShowWindow( UI_SW_SHOW );
					pElement->EnableWindow( false );
				}
			}
		}
		
		const bool bDisplayStar = playerInfos[1].size() + playerInfos[0].size() > 1 ; // only if more than 1 playe exists
		{
			for ( int nPlayer = 0; nPlayer < playersInfo.size(); ++nPlayer )
			{
				pList->AddItem( nPlayer + 1 );
				const int nRowID = pList->GetItemByID( nPlayer + 1 );
				IUIListRow * pRow = pList->GetItem( nRowID );
				pRow->GetElement( 0 )->SetWindowText( 0, reinterpret_cast<const WORD*>( playersInfo[nPlayer].first->GetName().c_str() ) );
				pRow->GetElement( 0 )->EnableWindow( false );
				int nIndex = 1;	// +1 because of party's name
				for ( int nStatFeild = 0; nStatFeild < playerStatsConfigure.size(); ++nStatFeild )
				{
					const SStatConfugure &curConfigure = playerStatsConfigure[nStatFeild];

					if ( curConfigure.IsToDisplay( bMultiplayer ) )
					{
						const SPlayerStatInfo  &curPlayerStat = playerStat[nPlayer + (nParty == 1 ? nPlayersIn0Party : 0 )][nStatFeild];
												
						IUIContainer * pElement = checked_cast<IUIContainer*>( pRow->GetElement( nIndex++ ) ); 
						pElement->SetWindowText( 0, reinterpret_cast<const WORD*>( curPlayerStat.GetValueToSort( curConfigure ).c_str() ) );
						IUIElement * pActualText = pElement->GetChildByID( E_ACTUAL_TEXT_ID );
						pActualText->SetWindowText( 0, reinterpret_cast<const WORD*>( curPlayerStat.GetValue( curConfigure ).c_str() ) );
						pActualText->ShowWindow( UI_SW_SHOW );
						pElement->EnableWindow( false );

						if ( bDisplayStar &&
								 partyInfo[nParty][nStatFeild].GetLeader() == nPlayer && 
								 curConfigure.eAccumulateType != E_BEST_NO_PLAYER_DISPLAY ) // the leader will get star
						{
							const double fCurValue = partyInfo[nParty][nStatFeild].GetLeaderValue( curConfigure );
														
							const double fOppValue = partyInfo[nOppositeParty][nStatFeild].GetLeaderValue( curConfigure );

							if ( curConfigure.bLeaderIsGreatest ? fCurValue > fOppValue : fCurValue < fOppValue )	
							{
								IUIElement * pStar = pElement->GetChildByID( E_STAR_ID );
								pStar->ShowWindow( UI_SW_SHOW );
							}
						}
					}
				}
			}
		}
		pList->Sort( 1, -1 );
		while( pList->GetNumberOfItems() > 5 )
		{
			pList->RemoveItem( 5 );
		}
		pList->InitialUpdate();
	}
}
void CInterfaceStats::CollectPlayerStats( interface IPlayerScenarioInfo *pPlayer, interface IScenarioStatistics *pStats )
{
	const int nParty = pPlayer->GetDiplomacySide();
	if ( nParty > 1 ) 
		return;
	
	playerInfos[nParty].push_back( CPlayerFullInfo( pPlayer, pStats ) );
}
void CInterfaceStats::Create( const int /*EStatsComplexity*/ nStatsType )
{
	playerStatsConfigure.push_back( SStatConfugure( EST_BOTH,					E_SUMMARY,									STMT_ENEMY_KILLED,							STMT_ENEMY_KILLED_AI_PRICE, 0, true ) );
	playerStatsConfigure.push_back( SStatConfugure( EST_BOTH,					E_SUMMARY,									STMT_FRIENDLY_KILLED,						STMT_FRIENDLY_KILLED_AI_PRICE, 0, false ) );
	playerStatsConfigure.push_back( SStatConfugure( EST_BOTH,					E_SUMMARY,									STMT_ENEMY_MACHINERY_CAPTURED,	-1, 0, true ) );
	playerStatsConfigure.push_back( SStatConfugure( EST_BOTH,					E_SUMMARY,									STMT_AVIATION_CALLED,						-1, 0, true ) );
	playerStatsConfigure.push_back( SStatConfugure( EST_BOTH,					E_SUMMARY_DEVIDED_BY_1000,	STMT_RESOURCES_USED,						-1, 0, true ) );
	playerStatsConfigure.push_back( SStatConfugure( EST_BOTH,					E_SUMMARY,									STMT_HOUSES_DESTROYED,					-1, 0, true ) );
	playerStatsConfigure.push_back( SStatConfugure( EST_SINGLEPLAYER, E_SUMMARY,									STMT_UNITS_LEVELED_UP,					-1, 0, true ) );
	playerStatsConfigure.push_back( SStatConfugure( EST_MULTIPLAYER,	E_BEST_NO_PLAYER_DISPLAY,		STMT_FLAGS_CAPTURED,						-1, 0, true ) );
	playerStatsConfigure.push_back( SStatConfugure( EST_MULTIPLAYER,	E_BEST_NO_PLAYER_DISPLAY,		STMT_FLAGPOINTS,								-1, 0, true ) );

	InitSorter();
	m_nStatsType = nStatsType;
	ITextManager *pTM = GetSingleton<ITextManager>();
	
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\stats" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );			//первый reposition для инициализации размера ListControl
	
	IUIElement *pHeader = pUIScreen->GetChildByID( 20000 );
	NI_ASSERT_T( pHeader != 0, "Invalid interface statistics header control" );
	CPtr<IText> p2;
	switch ( m_nStatsType )
	{
		case STATS_COMPLEXITY_TOTAL:
			p2 = pTM->GetString( "Textes\\UI\\Intermission\\Statistics\\total_statistics_caption" );
			break;

		case STATS_COMPLEXITY_CHAPTER:
			p2 = pTM->GetString( "imheader-stats-header" );
			break;

		case STATS_COMPLEXITY_MISSION:
			{
				p2 = pTM->GetString( "imheader-stats-mission-header" );
				if ( GetGlobalVar("MultiplayerGame", 0) == 1 && GetGlobalVar("History.Playing", 0) != 1 )
				{
					IUIElement *pSaveReplayButton = pUIScreen->GetChildByID( 10003 );
					pSaveReplayButton->ShowWindow( UI_SW_SHOW );
				}
			}
			break;
	}
	if ( p2 )
		pHeader->SetWindowText( 0, p2->GetString() );
	
	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	int nListItemsCounter = 0;
	for ( CPtr<IPlayerScenarioInfoIterator> pIt = pST->CreatePlayerScenarioInfoIterator(); !pIt->IsEnd(); pIt->Next(), ++nListItemsCounter )
	{
		IPlayerScenarioInfo *pPlayer = pIt->Get();
		if ( pPlayer->GetDiplomacySide() == 2 ) 
			continue;	// не выводим статистику для нейтральных игроков

		IScenarioStatistics *pStats = 0;
		switch ( m_nStatsType ) 
		{
			case STATS_COMPLEXITY_TOTAL:
				pStats = pPlayer->GetCampaignStats();
				break;
			case STATS_COMPLEXITY_CHAPTER:
				pStats = pPlayer->GetChapterStats();
				break;
			case STATS_COMPLEXITY_MISSION:
				pStats = pPlayer->GetMissionStats();
				break;
		}
		if ( pStats == 0 ) 
			continue;

		CollectPlayerStats( pPlayer, pStats );
	}
	

	RepositionList();
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	if ( GetGlobalVar( "MultiplayerGame", 0 ) )
	{
		StoreScreen();
		CInterfaceMainMenu::PlayIntermissionSound();
	}
	pScene->AddUIScreen( pUIScreen );

	bStatsShown = false;
	nMedalIterator = 0;
	bUpgradesShown = false;
	bNewUnitsShown = false;
	bNextChapterShown = false;
	bPlayerRankShown = false;
	bLastFullScreen = false;
	bTutorialWindow = false;
	
}
void CInterfaceStats::OnGetFocus( bool bFocus )
{
	CInterfaceScreenBase::OnGetFocus( bFocus );
	if ( !bPopupsShowed )
	{
		bPopupsShowed = true;
	}
}
bool CInterfaceStats::ProcessMessage( const SGameMessage &msg )
{
	if ( msg.nEventID == TUTORIAL_WINDOW_ID )
	{
		bTutorialWindow = true;
	}

	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;

	if ( pPartyList[0] && pPartyList[1] )
	{
		const int nColumn = msg.nParam >> 8;
		if ( pPartyList[0]->GetWindowID() == msg.nEventID )
			pPartyList[1]->Sort( nColumn );	
		else if ( pPartyList[1]->GetWindowID() == msg.nEventID )
			pPartyList[0]->Sort( nColumn );	
	}
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			if ( GetGlobalVar( "demoversion", 0 ) )
			{
				OnDemoversionExit();
				return true;
			}
			else if ( GetGlobalVar( "History.Playing", 0 ) > 0 )
			{
				SetGlobalVar( "CurtainsClosed", 1 );
				IMainLoop * pML = GetSingleton<IMainLoop>();

				pML->RestoreScenarioTracker();
				RemoveGlobalVar( "History.Playing" );
				RemoveGlobalVar( "MultiplayerGame" );

				pML->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d 0", MAIN_SP_TRANSCEIVER) );
				pML->Command( MISSION_COMMAND_MAIN_MENU, "6" );
				pML->Command( MISSION_COMMAND_REPLAY_LIST, 0 );
				return true;
			}
			{
				if ( GetGlobalVar( "MultiplayerGame", 0 ) )
				{
					if ( GetSingleton<IMPToUICommandManager>()->GetConnectionType() == EMCT_INTERNET )
						FinishInterface( MISSION_COMMAND_ADDRESS_BOOK, 0 );
					else
						FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );
					return true;
				}
				const std::string szChapterName = GetGlobalVar( "Chapter.Current.Name", "" );
				if ( szChapterName == "custom_mission" )
				{
					IMainLoop * pML = GetSingleton<IMainLoop>();
					pML->Command( MISSION_COMMAND_MAIN_MENU, "5" );
					pML->Command( MISSION_COMMAND_CUSTOM_MISSION, 0 );
					return true;
				}
			}
			CloseInterface( true );
			return true;
			
		case IMC_SAVE_REPLAY:
			bCheckReplay = true;
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_SAVE_REPLAY, 0 );
			return true;
	}
	
	return false;
}
void CInterfaceStats::OnDemoversionExit()
{
	const bool bWin = GetGlobalVar( "demoversion.Win", 0 );
	RemoveGlobalVar( "demoversion.Win" );

	if ( bWin == false )					// looser will get to main menu
	{
		FinishInterface( MISSION_COMMAND_MAIN_MENU, 0 );
		return;
	}

	const std::string szCurrentMission = GetGlobalVar( "Mission.Current.Name", "" );
	const std::string szMissionGlobalVar = "demomission.";

	const int nDemomissions = GetGlobalVar( "demomission.number", 0 );
	int nCurrentMission = 0;
	for ( ; nCurrentMission < nDemomissions; ++nCurrentMission )
	{
		const std::string szMissionName = GetGlobalVar( (szMissionGlobalVar + NStr::Format( "%d", nCurrentMission )).c_str(), "" );
		if ( szMissionName == szCurrentMission )
			break;
	}

	if ( nCurrentMission + 1 >= nDemomissions ) // all missions completed
	{
		FinishInterface( MISSION_COMMAND_MAIN_MENU, 0 );
	}
	else
	{
		const std::string szMission = GetGlobalVar( (szMissionGlobalVar + NStr::Format( "%d", nCurrentMission+1)).c_str(), "" );

		SetGlobalVar( "Chapter.Current.Name", "custom_mission" );
		SetGlobalVar( "Mission.Current.Name", szMission.c_str() );
		const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( szMission.c_str(), IObjectsDB::MISSION );
		NI_ASSERT_T( pStats != 0, (std::string("Invalid custom mission ") + szMission).c_str() );
		if ( !pStats )
			return ;
		IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
		pST->StartCampaign( "custom_mission", CAMPAIGN_TYPE_CUSTOM_MISSION );
		pST->StartChapter( "custom_mission" );
		FinishInterface( MISSION_COMMAND_MISSION, (pStats->szFinalMap +".xml").c_str() );
	}
	return ;
}