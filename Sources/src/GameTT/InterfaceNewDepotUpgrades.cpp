#include "StdAfx.h"

#include "InterfaceNewDepotUpgrades.h"
#include "CommonId.h"
#include "..\Main\ScenarioTracker.h"
#include "..\Main\RPGStats.h"
#include "..\GameTT\UnitTypes.h"
#include "..\GameTT\etypes.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ "inter_cancel"		, IMC_OK				},
	{ 0									,	0							}
};
enum
{
	E_UNIT_LIST														= 103,

};
CInterfaceNewDepotUpgrades::CInterfaceNewDepotUpgrades() : CInterfaceInterMission( "InterMission" )
{
}
CInterfaceNewDepotUpgrades::~CInterfaceNewDepotUpgrades()
{
}
void CInterfaceNewDepotUpgrades::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	
	commandMsgs.Init( pInput, commands );
	pUIScreen->Load( "ui\\Popup\\NewDepotUpgrades" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	IUIShortcutBar *pSB = checked_cast<IUIShortcutBar*>( pUIScreen->GetChildByID( E_UNIT_LIST ) );
	pSB->Clear();
	IPlayerScenarioInfo * pPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
	const int nNewUpgrades = pPlayer->GetNumNewDepotUpgrades();

	IObjectsDB *pIDB = GetSingleton<IObjectsDB>();
	pSB->AddBar();
	
	for ( int i = 0; i < nNewUpgrades; ++i )
	{
		IUIDialog *pEl = checked_cast<IUIDialog*>( pSB->AddItem() );
		const SGDBObjectDesc *pDesc = pIDB->GetDesc( pPlayer->GetNewDepotUpgrade( i ).c_str() );
		NI_ASSERT_T( pDesc != 0, NStr::Format( "unit not valid %s", pPlayer->GetNewDepotUpgrade( i ).c_str() ) );
		const SUnitBaseRPGStats *pRPG = static_cast<const SUnitBaseRPGStats*>( pIDB->GetRPGStats( pDesc ) );

		FillUnitInfoItem( pRPG, pEl, i, false );
		pEl->EnableWindow( true );
	}
	pSB->SetBarExpandState( 0, true );
	pSB->SetSelectionItem( 0, 0 );
	
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pSB->InitialUpdate();
	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceNewDepotUpgrades::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	if ( msg.nEventID >= 20000 && msg.nEventID < 29000 )
	{
		const int nUnitNumber = msg.nEventID - 20000;
		
		IPlayerScenarioInfo * pPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
		const int nNewUpgrades = pPlayer->GetNumNewDepotUpgrades();
		if ( nUnitNumber < nNewUpgrades )
		{
			std::string szTemp = NStr::Format( "%d;", E_UNIT );
			szTemp += pPlayer->GetNewDepotUpgrade( nUnitNumber );
			FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szTemp.c_str() );
		}
		return true;
	}

	switch ( msg.nEventID )
	{
	case IMC_CANCEL:
	case IMC_OK:
		{
			IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
			CloseInterface();
			pPlayer->ClearNewDepotUpgrade();
		}
		break;
	}
	
	return false;
}

