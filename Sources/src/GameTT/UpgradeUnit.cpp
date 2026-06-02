#include "StdAfx.h"

#include "UpgradeUnit.h"

#include "CommonId.h"
#include "UnitTypes.h"
#include "etypes.h"
#include "..\Main\RPGStats.h"
#include "..\Main\ScenarioTracker.h"
#include "..\UI\UIMessages.h"
enum ECommands
{
	IMC_DEFAULT_UNITS				= 10003,
	IMC_UNIT_INFO						=	10006,
	E_BUTTON_WAREHOUSE			= 10005,
};
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
CInterfaceUpgradeUnit::~CInterfaceUpgradeUnit()
{
}
bool CInterfaceUpgradeUnit::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );

	return true;
}
void CInterfaceUpgradeUnit::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\upgrades" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	DefaultUpgrades();
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	StoreScreen();
	
	pScene->AddUIScreen( pUIScreen );
}
void CInterfaceUpgradeUnit::OnGetFocus( bool bFocus )
{
	CInterfaceInterMission::OnGetFocus( bFocus );
	if ( GetGlobalVar( "ReinitUpgradeUnit", 0 ) )
	{
		RemoveGlobalVar( "ReinitUpgradeUnit" );
		DefaultUpgrades();
	}
}
void CInterfaceUpgradeUnit::DefaultUpgrades()
{
	ITextManager *pTextM = GetSingleton<ITextManager>();
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	
	IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 100 ) );
	NI_ASSERT_T( pSB != 0, "ShortcutBar is not initialized" );
	pSB->Clear();
	
	IUIShortcutBar *pUSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 101 ) );
	pUSB->Clear();
	
	IPlayerScenarioInfo *pPlayerInfo = GetSingleton<IScenarioTracker>()->GetUserPlayer();
	const int nNumUnits = pPlayerInfo->GetNumUnits();
	
	IObjectsDB * pDB = GetSingleton<IObjectsDB>();

	const std::string &szUpgrade = pPlayerInfo->GetUpgrade();
	const SGDBObjectDesc *pUpgradeDesc = pDB->GetDesc( szUpgrade.c_str() );
	NI_ASSERT_T( pUpgradeDesc != 0, NStr::Format( "wrong desc for upgrage %s", szUpgrade.c_str() ) );
	const SUnitBaseRPGStats *pUpgradeStats = checked_cast<const SUnitBaseRPGStats*>( pDB->GetRPGStats( pUpgradeDesc ) );

	const int nUpgradeClass = pUpgradeStats->GetRPGClass();
	
	int nBarIndex = 0;
	int nUpgradeUnitIndex = -1;
	float fMaxDelta = -100000;
	int nUnitIndex = 0;

	IUIElement *pBar = pSB->AddBar();		//bar невидимый
	std::string szKey = NStr::Format( "textes\\RPGClasses\\class%d", nUpgradeClass );
	CPtr<IText> pText = pTextM->GetDialog( szKey.c_str() );
	NI_ASSERT_T( pText != 0, NStr::Format( "Can not get text by key: %s", szKey.c_str() ) );
	pBar->SetWindowText( 0, pText->GetString() );
	pBar->SetWindowText( 1, pText->GetString() );
	pBar->SetWindowID( nUpgradeClass );
	
	for ( int nOurUnit = 0; nOurUnit < nNumUnits; ++nOurUnit )
	{
		const IScenarioUnit *pOurUnit = pPlayerInfo->GetUnit( nOurUnit );
		const SGDBObjectDesc *pOurUnitDesc = pDB->GetDesc( pOurUnit->GetRPGStats().c_str() );
		NI_ASSERT_T( pOurUnitDesc != 0, NStr::Format( "wrong desc for upgrage %s", pOurUnit->GetRPGStats().c_str() ) );
		const SUnitBaseRPGStats *pOurUnitStats = checked_cast<const SUnitBaseRPGStats*>( pDB->GetRPGStats( pOurUnitDesc ) );

		if ( pOurUnitStats->GetRPGClass() == nUpgradeClass )
		{
			if ( pUpgradeStats->fPrice - pOurUnitStats->fPrice > fMaxDelta )
			{
				fMaxDelta = pUpgradeStats->fPrice - pOurUnitStats->fPrice;
				nUpgradeUnitIndex = nUnitIndex;
			}
			IUIDialog *pItem = checked_cast<IUIDialog *>( pSB->AddItem() );
			FillUnitInfoItem( pOurUnitStats, pItem, nOurUnit, true );
			nUnitIndex++;
		}
	}

	NI_ASSERT_T( nUpgradeUnitIndex != -1, "AI cost of upgrade < min(AI cost of units of current AI class)" );
	if ( nUpgradeUnitIndex == -1 )
		return;
			
	pSB->SetBarExpandState( nBarIndex, true );
	nBarIndex++;
			
	{
		pUSB->AddBar();		//bar невидимый
		IUIDialog *pItem = checked_cast<IUIDialog *>( pUSB->AddItem() );
		FillUnitInfoItem( pUpgradeStats, pItem, nNumUnits, false );		//заполнение без имени командира, у upgrade индекс всегда 0

		pUSB->SetBarExpandState( 0, true );
		
		pItem->EnableWindow( true );
		pUSB->SetSelectionItem( 0, pUSB->GetNumberOfItems( 0 ) - 1 );

		pSB->SetSelectionItem( nBarIndex - 1, nUpgradeUnitIndex );
	}

	pSB->InitialUpdate();
	pUSB->InitialUpdate();
}
bool CInterfaceUpgradeUnit::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
	case IMC_CANCEL:
	{
		if ( bToChapter )
			FinishInterface( MISSION_COMMAND_WAREHOUSE, 0 );
		else
			CloseInterface( true );
		return true;
	}
	
	case IMC_OK:
	{
		IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 100 ) );
		int nBar = -1, nIndex = -1;
		pSB->GetSelectionItem( &nBar, &nIndex );
		IUIElement *pItem = pSB->GetItem( nBar, nIndex );
		int nIndexToUpgrade = 0;
		NI_ASSERT_T( pItem != 0, "Error in upgrade interface (SB selected item is 0)" );
		if ( pItem )
			nIndexToUpgrade = pItem->GetWindowID();

		IUIShortcutBar *pUSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 101 ) );
		IUIElement *pUItem = pUSB->GetItem( 0, 0 );
		NI_ASSERT_T( pUItem != 0, "Error in upgrade interface (USB first item is 0)" );

		{
			IPlayerScenarioInfo *pPlayerInfo = GetSingleton<IScenarioTracker>()->GetUserPlayer();
			IScenarioUnit * pUnitToUpgrade = pPlayerInfo->GetUnit( nIndexToUpgrade );
			if ( 0 != pUnitToUpgrade )
			{
				pUnitToUpgrade->ChangeRPGStats( pPlayerInfo->GetUpgrade() );
			}
		}
		
		if ( bToChapter )
			FinishInterface( MISSION_COMMAND_CHAPTER, 0 );
		else
			CloseInterface( true );
		
		return true;
	}

	case IMC_DEFAULT_UNITS:
		DefaultUpgrades();
		return true;
		
	case UI_NOTIFY_SELECTION_CHANGED:
		return true;
		
	case UI_NOTIFY_BAR_EXPAND:
		return true;
	}
	
	if ( msg.nEventID >= 20000 && msg.nEventID < 29000 )
	{
		std::string szTemp = NStr::Format( "%d;", E_UNIT );
		IPlayerScenarioInfo *pPlayerInfo = GetSingleton<IScenarioTracker>()->GetUserPlayer();
		const int nUnitIndex = msg.nEventID - 20000;
		std::string szUnitRPGStats;
		if ( nUnitIndex == pPlayerInfo->GetNumUnits() )
		{
			szUnitRPGStats = pPlayerInfo->GetUpgrade();
		}
		else
		{
			IScenarioUnit * pUnit = pPlayerInfo->GetUnit( nUnitIndex );
			IObjectsDB * pDB = GetSingleton<IObjectsDB>();
			const SGDBObjectDesc *pOurUnitDesc = pDB->GetDesc( pUnit->GetRPGStats().c_str() );
			NI_ASSERT_T( pOurUnitDesc != 0, NStr::Format( "wrong desc for upgrage %s", pUnit->GetRPGStats().c_str() ) );
			const SUnitBaseRPGStats *pOurUnitStats = checked_cast<const SUnitBaseRPGStats*>( pDB->GetRPGStats( pOurUnitDesc ) );
			szUnitRPGStats = pOurUnitStats->szParentName;
		}

		szTemp += szUnitRPGStats;

		FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szTemp.c_str() );
		return true;
	}
	
	return false;
}
