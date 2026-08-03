#include "StdAfx.h"

#include "UnitsPool.h"

#include "../Main/ScenarioTracker.h"
#include "../RandomMapGen/MapInfo_Types.h"
#include "CommonId.h"
#include "UnitTypes.h"
#include "etypes.h"
enum ECommands
{
	IMC_UNIT_INFO			=	10006,
};
void CICUnitsPool::Configure( const char *pszConfig )
{
	if ( pszConfig != 0 && strlen(pszConfig) > 0 )
		nNewUnits = NStr::ToInt( pszConfig );
	else
		nNewUnits = 0;
}
void CICUnitsPool::PostCreate( IMainLoop *pML, CInterfaceUnitsPool *pIUP )
{
	pIUP->Create( nNewUnits );
	pML->PushInterface( pIUP );
}
CInterfaceUnitsPool::~CInterfaceUnitsPool()
{
}
bool CInterfaceUnitsPool::Init()
{
	CInterfaceInterMission::Init();

	return true;
}
void CInterfaceUnitsPool::Create( int nNewUnits )
{
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\unitspool" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	ITextureManager *pTM = GetSingleton<ITextureManager>();
	ITextManager *pTextM = GetSingleton<ITextManager>();

	if ( nNewUnits )
	{
		IUIElement *pHeader = pUIScreen->GetChildByID( 20000 );
		NI_ASSERT_T( pHeader != 0, "Invalid UnitsPool interface header control" );
		CPtr<IText> p2 = pTextM->GetString( "newunits" );
		if ( p2 )
		{
			pHeader->SetWindowText( 0, p2->GetString() );
		}

		pHeader = pUIScreen->GetChildByID( 20001 );
		p2 = pTextM->GetString( "newunitssubheader" );
		NI_ASSERT_T( pHeader != 0, "Invalid UnitsPool interface subheader control" );
		if ( p2 )
		{
			pHeader->SetWindowText( 0, p2->GetString() );
		}
	}
	
	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	IPlayerScenarioInfo *pUserPlayer = pST->GetUserPlayer();
	IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 100 ) );
	NI_ASSERT_T( pSB != 0, "ShortcutBar is not initialized" );
	pSB->Clear();

	std::vector< std::vector<int> > units( nUnitClassesSize );
	if ( nNewUnits )
	{
		for ( int i = 0; i < pUserPlayer->GetNumNewUnits(); ++i )
		{
			IScenarioUnit *pUnit = pUserPlayer->GetNewUnit( i );
			const SUnitBaseRPGStats *pRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>( pUnit->GetRPGStats().c_str() );
			units[pRPG->GetRPGClass()].push_back( pUnit->GetScenarioID() );
		}
	}
	else
	{
		const int nNumUnits = pUserPlayer->GetNumUnits();
		for ( int i = 0; i < nNumUnits; ++i )
		{
			IScenarioUnit *pUnit = pUserPlayer->GetUnit( i );
			const SUnitBaseRPGStats *pRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>( pUnit->GetRPGStats().c_str() );
			units[pRPG->GetRPGClass()].push_back( i );
		}
	}

	int nBarIndex = 0;
	for ( int i=0; i<nUnitClassesSize; i++ )
	{
		if ( units[i].empty() )
			continue;
		
		IUIElement *pBar = pSB->AddBar();
		std::string szKey = NStr::Format( "textes\\RPGClasses\\class%d", i );
		CPtr<IText> pText = pTextM->GetDialog( szKey.c_str() );
		NI_ASSERT_T( pText != 0, NStr::Format( "Can not get text by key: %s", szKey.c_str() ) );
		pBar->SetWindowText( 0, pText->GetString() );
		pBar->SetWindowText( 1, pText->GetString() );
		pBar->SetWindowID( unitClasses[i].nClass );
		
		if ( nNewUnits )
		{
			pSB->SetBarExpandState( nBarIndex, true );
		}
		nBarIndex++;
		
		for ( int k=0; k<units[i].size(); k++ )
		{
			IUIDialog *pItem = checked_cast<IUIDialog *>( pSB->AddItem() );
			IScenarioUnit *pUnit = pUserPlayer->GetUnit( units[i][k] );
			const SUnitBaseRPGStats *pRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>( pUnit->GetRPGStats().c_str() );
			FillUnitInfoItem( pRPG, pItem, units[i][k], true );
		}
	}
	
	pSB->InitialUpdate();
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	StoreScreen();
	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceUnitsPool::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
	case IMC_CANCEL:
		{
			CloseInterface( true );
			return true;
		}
		
		/*
		case UI_NOTIFY_SELECTION_CHANGED:
		if ( msg.nParam == 100 )
		UpdateUnitsList();
		else if ( msg.nParam == 101 )
		SelectItem();
		else
		NI_ASSERT_T( 0, "WTF" );
		return true;
		*/
		
		/*
		case UI_NOTIFY_BAR_EXPAND:
		if ( msg.nParam == 100 )
		UpdateUnitsList();
		else if ( msg.nParam != 101 )
		NI_ASSERT_T( 0, "WTF" );
		return true;
		*/
	}
	

	if ( msg.nEventID >= 20000 && msg.nEventID < 21000 )
	{
		IScenarioUnit *pUnit = GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetUnit( msg.nEventID - 20000 );
		const std::string szTemp = NStr::Format( "%d;%s", E_UNIT, pUnit->GetRPGStats().c_str() );
		FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szTemp.c_str() );
	}

	return false;
}
