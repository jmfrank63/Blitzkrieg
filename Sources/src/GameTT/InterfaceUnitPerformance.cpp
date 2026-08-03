#include "StdAfx.h"

#include "InterfaceUnitPerformance.h"

#include "CommonId.h"
#include "../Main/ScenarioTracker.h"
#include "../Main/ScenarioTrackerTypes.h"
#include "../Main/RPGStats.h"
#include "UnitTypes.h"
#include "InterfaceAfterMissionPopups.h"
#include "Campaign.h"
#include "MainMenu.h"
#include "../GameTT/eTypes.h"
CAfterMissionPopups * pPopups = 0;
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{	"inter_ok", 				IMC_OK				},
	{ 0									,	0							}
};
enum 
{
	E_BUTTON_OK														= 10002,
	E_BUTTON_CANCEL												= 10001,
	E_BUTTON_STATS												= 10003,

	E_LEVELED_UP_SB												= 100,
	E_DEAD_UNITS_SB												= 101,
	
	E_STARS																= 115,	
	E_STARS_0_STAR												= 0,		// 0 STAR
};
int CInterfaceUnitPerformance::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pPopups );
	saver.AddTypedSuper( 3, static_cast<CInterfaceInterMission*>( this ) );
	saver.Add( 4, &nPlayerUnits );
	saver.Add( 5, &nTotalNumUnits );
	return 0;
}
const char * CInterfaceUnitPerformance::GetUnitNameByWindowID( const int nWindowID )
{
	if ( nWindowID >= 20000 && nWindowID < 29000 )
	{
		const int nUnit = nWindowID - 20000;
		IPlayerScenarioInfo *pPlayerInfo = GetSingleton<IScenarioTracker>()->GetUserPlayer();
		IMissionStatistics *pMissionStats = pPlayerInfo->GetMissionStats();
		
		if ( nUnit < nPlayerUnits )
		{
			IScenarioUnit *pUnit = pPlayerInfo->GetUnit( nUnit );
			return pUnit->GetRPGStats().c_str();
		}
		else if ( nUnit < nTotalNumUnits )
		{
			return pMissionStats->GetKIAStats( nUnit - nPlayerUnits ).c_str();
		}
	}
	return 0;
}
bool CInterfaceUnitPerformance::ProcessMessage( const SGameMessage &msg )
{
	/*if ( msg.nEventID == TUTORIAL_WINDOW_ID || msg.nEventID == 	TUTORIAL_BUTTON_ID )
	{
		bDisableGetFocus = true;
	}*/
	
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;

	switch( msg.nEventID )
	{
	case IMC_CANCEL:
	case IMC_OK:
		CloseInterface( true );
		return true;
		
	case E_BUTTON_STATS:
		FinishInterface( MISSION_COMMAND_STATS, "2" /*STATS_COMPLEXITY_MISSION*/ );
		
		return true;
	}

	
	const char *pszUnitName = GetUnitNameByWindowID( msg.nEventID );
	if ( 0 != pszUnitName )
	{
		std::string szTemp = NStr::Format( "%d;", E_UNIT );
		szTemp += pszUnitName;
		FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szTemp.c_str() );
		return true;
	}
	return false;
}
bool CInterfaceUnitPerformance::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );

	return true;
}
void CInterfaceUnitPerformance::StartInterface()
{
	RemoveGlobalVar( "EncyclopediaShown");
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\UnitsMissionPerformance" );

	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	PrepairShortcutBar();
	pPopups = new CAfterMissionPopups;

	pUIScreen->Reposition( pGFX->GetScreenRect() );
	StoreScreen();
	pScene->AddUIScreen( pUIScreen );
	GetSingleton<IInput>()->AddMessage( SGameMessage(TUTORIAL_TRY_SHOW_IF_NOT_SHOWN) );
}
void CInterfaceUnitPerformance::OnGetFocus( bool bFocus )
{
	CInterfaceScreenBase::OnGetFocus( bFocus );

	/*if ( GetGlobalVar( "EncyclopediaShown", 0 ) ) 
	{
		RemoveGlobalVar( "EncyclopediaShown" );
		return;
	}
	if ( bDisableGetFocus && bFocus )
	{
		bDisableGetFocus = false;
		return;
	}
	
	if ( pPopups )
	{
		pPopups->OnGetFocus( bFocus );
		if ( pPopups->IsNeedFinish() )
		{
			FinishInterface( pPopups->GetFinishCommandID(), pPopups->GetFinishCommandParams() );
		}
	}*/
}
void CInterfaceUnitPerformance::PrepairShortcutBar()
{
	ITextManager *pTextM = GetSingleton<ITextManager>();
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	IPlayerScenarioInfo *pPlayerInfo = GetSingleton<IScenarioTracker>()->GetUserPlayer();
	IObjectsDB * pDB = GetSingleton<IObjectsDB>();
	
	IUIShortcutBar *pLeveledUpSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( E_LEVELED_UP_SB ) );
	IUIShortcutBar *pDeadUnitsSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( E_DEAD_UNITS_SB ) );
	pLeveledUpSB->Clear();
	pDeadUnitsSB->Clear();
	{
		IUIElement *pBar = pLeveledUpSB->AddBar();		//bar невидимый
		pBar->SetWindowText( 0, reinterpret_cast<const WORD*>( L"1" ) );
		pBar->SetWindowText( 1, reinterpret_cast<const WORD*>( L"2" ) );
		pBar->SetWindowID( 0 );
		nPlayerUnits = pPlayerInfo->GetNumUnits();
		
		for ( int i = 0; i < nPlayerUnits; ++i )
		{
			IScenarioUnit * pUnit = pPlayerInfo->GetUnit( i );
			const int nDiff = pUnit->GetValueDiff( STUT_LEVEL );
			if ( nDiff > 0 )
			{
				const SGDBObjectDesc *pOurUnitDesc = pDB->GetDesc( pUnit->GetRPGStats().c_str() );
				const SUnitBaseRPGStats *pOurUnitStats = checked_cast<const SUnitBaseRPGStats*>( pDB->GetRPGStats( pOurUnitDesc ) );
				IUIDialog *pItem = checked_cast<IUIDialog *>( pLeveledUpSB->AddItem() );
				FillUnitInfoItem( pOurUnitStats, pItem, i, true );
				IUIDialog * pStars = checked_cast<IUIDialog*>( pItem->GetChildByID( E_STARS ) );
				IUIElement * pStar = pStars->GetChildByID( E_STARS_0_STAR + nDiff );
				NI_ASSERT_T( pStar != 0, NStr::Format( "no star window for %d stars", nDiff ) );
				pStar->ShowWindow( UI_SW_SHOW );
			}
		}
		pLeveledUpSB->SetBarExpandState( 0, true );
	}
	{
		IUIElement *pBar = pDeadUnitsSB->AddBar();		//bar невидимый
		IMissionStatistics * pStat = pPlayerInfo->GetMissionStats();
		const int nNumKIA = pStat->GetNumKIA();
		for ( int i = 0; i < nNumKIA; ++i )
		{
			IUIDialog *pItem = checked_cast<IUIDialog *>( pDeadUnitsSB->AddItem() );
			const SGDBObjectDesc *pKIADesc = pDB->GetDesc( pStat->GetKIAStats( i ).c_str() );
			NI_ASSERT_T( pKIADesc != 0, NStr::Format( "wrond desc for unit \"%s\"", pStat->GetKIAStats( i ).c_str() ) );
			const SUnitBaseRPGStats *pKIAStats= checked_cast<const SUnitBaseRPGStats*>( pDB->GetRPGStats( pKIADesc ) );
			FillUnitInfoItem( pKIAStats, pItem, nPlayerUnits + i, false, pStat->GetKIAName( i ).c_str() );
		}
		pDeadUnitsSB->SetBarExpandState( 0, true );
		nTotalNumUnits = nPlayerUnits + nNumKIA;
	}
	

	pLeveledUpSB->InitialUpdate();
	pDeadUnitsSB->InitialUpdate();
}
