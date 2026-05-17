#include "StdAfx.h"

#include "AddUnitToMission.h"

#include "..\Main\ScenarioTracker.h"
#include "..\Main\ScenarioTrackerTypes.h"
#include "..\Common\ObjectStatus.h"
#include "..\Main\gamestats.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\UI\UIMessages.h"
#include "CommonId.h"
#include "UnitTypes.h"
#include "etypes.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SUnitClass unitClasses[] =
{
	RPG_CLASS_UNKNOWN, "_unknown",
	RPG_CLASS_ARTILLERY, "_artillery",
	RPG_CLASS_TANK, "_tank",
	RPG_CLASS_SNIPER, "_sniper",
	/*
	RPG_CLASS_HEAVY_ARTILLERY, "_hartillery",
	RPG_CLASS_ROCKET, "_rocket",
	RPG_CLASS_SPG, "_spg",
	RPG_CLASS_SPG_ASSAULT, "_spgassault",
	RPG_CLASS_HEAVY_TANK, "_htank",
	*/
};
int nUnitClassesSize = ARRAY_SIZE( unitClasses );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SUnitClass unitTypes[] =
{
	RPG_TYPE_INFANTRY, "infantry_type",
	RPG_TYPE_TRANSPORT, "transport_type",
	RPG_TYPE_ARTILLERY, "artillery_type",
	RPG_TYPE_SPG, "spg_type",
	RPG_TYPE_ARMOR, "armor_type",
	RPG_TYPE_AVIATION, "aviation_type",
	RPG_TYPE_TRAIN, "train_type",
};
int nUnitTypesSize = ARRAY_SIZE( unitTypes );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTRect<float> rcTechnicsInfoPanelMap( 0, 0, 90.5f/128.0f, 90.5f/128.0f );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector< std::vector<int> > CInterfaceAddUnitToMission::m_missionSlots;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *GetUnitClassName( int nUnitClass )
{
	for ( int i=0; i<nUnitClassesSize; i++ )
	{
		if ( unitClasses[i].nClass == nUnitClass )
			return unitClasses[i].pszName;
	}
	NI_ASSERT( 0 );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FillUnitInfoItem( const SUnitBaseRPGStats *pRPG, IUIDialog *pItem, int nIndex, bool bFillCommanderName, const char *pszCommanderName )
{
	IUIElement *pHelpButton = pItem->GetChildByID( 10000 );
	if ( pHelpButton )
		pHelpButton->SetWindowID( 20000 + nIndex );		//��� ������������
	pItem->SetWindowID( nIndex );
	FillUnitInfoItemNoIDs( pRPG, pItem, nIndex, bFillCommanderName, pszCommanderName );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FillUnitInfoItemNoIDs( const SUnitBaseRPGStats *pRPG, IUIDialog *pItem, int nIndex, bool bFillCommanderName, const char *pszCommanderName )
{
	//nIndex is a Scenario Tracker index for unit, else -1
	ITextManager *pTextM = GetSingleton<ITextManager>();
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	
	const SGDBObjectDesc *pObjectDesc = GetSingleton<IObjectsDB>()->GetDesc( pRPG->szParentName.c_str() );
	CPtr<IText> p1 = pTextM->GetDialog( (pObjectDesc->szPath + "\\name").c_str() );
	
	//��������� ��� �����
	IUIElement *pElement = pItem->GetChildByID( 20 );
	pElement->SetWindowText( 0, p1->GetString() );

	IPlayerScenarioInfo *pPlayerInfo = pST->GetUserPlayer();
	
	IText * pCommanderName = 0;

	//��� ���������
	if ( bFillCommanderName )
	{
		IScenarioUnit *pUnit = pPlayerInfo->GetUnit( nIndex );
		pCommanderName = pUnit->GetName();
	}
	else if ( pszCommanderName )
		pCommanderName = pTextM->GetString( pszCommanderName );

	if ( pCommanderName )
	{
		pElement = pItem->GetChildByID( 21 );
		pElement->SetWindowText( 0, pCommanderName->GetString() );
	}

	if ( bFillCommanderName )
	{
		IScenarioUnit *pUnit = pPlayerInfo->GetUnit( nIndex );
		//������ ������� �����
		const int nLevel = pUnit->GetValue( STUT_LEVEL );
		NI_ASSERT_T( nLevel >= 0 && nLevel < 4, "Unit's level is out of bounds" );
		IUIElement *pElement = pItem->GetChildByID( 30 + nLevel );
		if ( pElement )
		{
			//������ ������� � ��������� ������� ����� �����
			const int nExp = pUnit->GetValue( STUT_EXP );
			const int nExpNextLevel = pUnit->GetValue( STUT_EXP_NEXT_LEVEL );
			//
			IText *pText = GetSingleton<ITextManager>()->GetDialog( NStr::Format("textes\\ui\\mission\\status\\tt_unit_level%d", nLevel) );
			std::wstring wToolTip = pText != 0 ? pText->GetString() : L"";
			wToolTip += NStr::ToUnicode( NStr::Format("(%d / %d)", nExp, nExpNextLevel) );

			pElement->SetHelpContext( 0, wToolTip.c_str() );
			pElement->ShowWindow( UI_SW_SHOW );
		}
	}


	//������� ������ �����
	{
		static std::wstring szText;
		IUIElement *pElement = 0;
		
		SMissionStatusObject status;
		GetStatusFromRPGStats( &status, static_cast<const SMechUnitRPGStats *> ( pRPG ), false, false );

		for ( int i=0; i<4; i++ )
		{
			const int nID = 101 + i;
			NStr::ToUnicode( &szText, NStr::Format("%d", status.armors[i] ) );
			pElement = pItem->GetChildByID( nID );
			NI_ASSERT_T( pElement != 0, NStr::Format( "Can not find window id %d (armor string)", nID ) );
			pElement->SetWindowText( 0, szText.c_str() );
		}

		for ( int i=0; i<2; i++ )
		{
			const int nID = 111 + i;
			NStr::ToUnicode( &szText, NStr::Format("%d", status.weaponstats[i] ) );
			pElement = pItem->GetChildByID( nID );
			NI_ASSERT_T( pElement != 0, NStr::Format( "Can not find window id %d (weapon string)", nID ) );
			pElement->SetWindowText( 0, szText.c_str() );
		}
	}

	//
	//��������� �������� �����
	IUIElement *pPicture = pItem->GetChildByID( 11 );
	//��������� map ��� ��������
	pPicture->SetWindowMap( rcTechnicsInfoPanelMap );
	//�������� � ��������� ��������
	IGFXTexture *pTexture = pTM->GetTexture( (pObjectDesc->szPath + "\\icon").c_str() );
	pPicture->SetWindowTexture( pTexture );

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECommands
{
	IMC_DEFAULT_UNITS				= 10003,
	IMC_UNIT_INFO						=	10006,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceAddUnitToMission::CInterfaceAddUnitToMission() : CInterfaceInterMission( "InterMission" )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceAddUnitToMission::~CInterfaceAddUnitToMission()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceAddUnitToMission::Init()
{
	CInterfaceInterMission::Init();
	//	SetBindSection( "intermission" );
	
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceAddUnitToMission::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\AddUnitToMission" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	DisplaySlotsFromST();
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	StoreScreen();
	pScene->AddUIScreen( pUIScreen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceAddUnitToMission::AddDefaultSlotsToST()
{
	NI_ASSERT_T( FALSE, "OLD INTERFACE" );
	//��������� ���� �� ���������� ��������� ���� �� ������ �����
	/*std::string szMissionName = GetGlobalVar( "Mission.Current.Name", "" );
	NI_ASSERT_T( szMissionName.size() != 0, "Can not read mission name" );
	if ( szMissionName.size() == 0 )
		return false;
	
	m_missionSlots.resize( nUnitClassesSize );
	for ( int i=0; i<m_missionSlots.size(); i++ )
	{
		m_missionSlots[i].clear();
	}
	
	IObjectsDB *pIDB = GetSingleton<IObjectsDB>();
	CGDBPtr<SGDBObjectDesc> pDesc = 0;
	const SUnitBaseRPGStats *pRPG = 0;
	
	std::vector<SMapObjectInfo> mapObjects;
	const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( szMissionName.c_str(), IObjectsDB::MISSION );
	std::string szMapName = "maps\\";
	szMapName += pStats->szFinalMap;
	bool bRes = CMapInfo::GetScenarioObjects( szMapName.c_str(), &mapObjects );
	NI_ASSERT_T( bRes != 0, "CMapInfo::GetScenarioObjects() FAILED" );
	if ( !bRes )
		return false;

	typedef std::unordered_map<int, int> CMissionAIClasses;
	CMissionAIClasses missionAIClasses;
	for ( int i=0; i<mapObjects.size(); i++ )
	{
		pDesc = pIDB->GetDesc( mapObjects[i].szName.c_str() );
		pRPG = checked_cast<const SUnitBaseRPGStats*>( pIDB->GetRPGStats( pDesc ) );
		missionAIClasses[ pRPG->GetRPGClass() ]++;
	}

	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	int nNumUnits = pST->GetNumUnits();
	//remove all units from mission
	for ( int z=0; z<nNumUnits; z++ )
	{
		pST->RemoveUnitFromMission( z );
	}

	for ( int i=0; i<nUnitClassesSize; i++ )
	{
		int nNumTypes = missionAIClasses[(EUnitRPGClass) unitClasses[i].nClass];
		if ( nNumTypes == 0 )
			continue;

		int nActiveType = 0;
		for ( int z=0; z<nNumUnits; z++ )
		{
			const SUnitBaseRPGStats *pRPG = pST->GetUnitRPGStats( z );
			if ( pRPG->GetRPGClass() == unitClasses[i].nClass && !pST->IsUnitKilled( z ) )
			{
				//������� item � ������ RPG stats
				m_missionSlots[i].push_back( z );
				
				nActiveType++;
				if ( nActiveType == nNumTypes )
					break;
			}
		}
		
		nNumTypes = min( nActiveType, nNumTypes );
		for ( int k=nActiveType; k<nNumTypes; k++ )
		{
			//������� ������ item
			m_missionSlots[i].push_back( -1 );
		}
	}

	//�������� �� ��������������� ������� � ��������� ����� � ������
	for ( int i=0; i<m_missionSlots.size(); i++ )
	{
		for ( int k=0; k<m_missionSlots[i].size(); k++ )
		{
			if ( m_missionSlots[i][k] != -1 )
				pST->AddUnitToMission( m_missionSlots[i][k] );
		}
	}
	*/
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceAddUnitToMission::DisplaySlotsFromST()
{
	NI_ASSERT_T( FALSE, "OLD INTERFACE" );
	/*ITextureManager *pTM = GetSingleton<ITextureManager>();
	ITextManager *pTextM = GetSingleton<ITextManager>();
	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();

	//init Shortcut Bar
	IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 100 ) );
	NI_ASSERT_T( pSB != 0, "ShortcutBar is not initialized" );
	pSB->Clear();
	
	//���������� ������ �� m_missionSlots �� ������
	for ( int i=0; i<m_missionSlots.size(); i++ )
	{
		if ( m_missionSlots[i].empty() )
			continue;
		
		//Add bar
		IUIElement *pBar = pSB->AddBar();
		std::string szKey = NStr::Format( "textes\\RPGClasses\\class%d", i );
		CPtr<IText> pText = pTextM->GetDialog( szKey.c_str() );
		//		CPtr<IText> pText = pTextM->GetString( unitClasses[i].pszName );
		NI_ASSERT_T( pText != 0, NStr::Format( "Can not get text by key: %s", szKey.c_str() ) );
		pBar->SetWindowText( 0, pText->GetString() );
		pBar->SetWindowText( 1, pText->GetString() );
		pBar->SetWindowID( unitClasses[i].nClass );
		
		for ( int k=0; k<m_missionSlots[i].size(); k++ )
		{
			//������� item � ������ RPG stats
			IUIDialog *pItem = checked_cast<IUIDialog *>( pSB->AddItem() );

			if ( m_missionSlots[i][k] == -1 )
			{
				//empty slot
				pItem->GetChildByID( 10000 )->SetWindowID( 20000 + m_missionSlots[i][k] );
				pItem->SetWindowID( m_missionSlots[i][k] );		//-1

				CPtr<IText> p1 = pTextM->GetString( "emptyslot" );
				NI_ASSERT_T( p1 != 0, NStr::Format( "Can not get text by key: emptyslot" ) );
				pItem->SetWindowText( 0, p1->GetString() );

				//������ ��������
				IUIElement *pPicture = pItem->GetChildByID( 11 );
				pPicture->ShowWindow( UI_SW_HIDE );
			}
			else
			{
				const SUnitBaseRPGStats *pRPG = pST->GetUnitRPGStats( pItem->GetWindowID() );
				FillUnitInfoItem( pRPG, pItem, m_missionSlots[i][k], true );
			}
		}
	}
	
	pSB->InitialUpdate();
	
	//init choose units Shortcut Bar
	IUIShortcutBar *pCSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 101 ) );
	NI_ASSERT_T( pCSB != 0, "ShortcutBar is not initialized" );
	pCSB->Clear();
	pCSB->InitialUpdate();*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceAddUnitToMission::UpdateUnitsList()
{
	NI_ASSERT_T( FALSE, "OLD INTERFACE" );
	/*IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 100 ) );
	int nBar = -1, nItem = -1;
	pSB->GetSelectionItem( &nBar, &nItem );
	if ( nBar == -1 )
		return;
	
	//choose ShortcutBar
	IUIShortcutBar *pCSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 101 ) );
	//Clear choose ShortcutBar
	pCSB->Clear();
	if ( nItem == -1 || !pSB->GetBarExpandState( nBar ) )
	{
		pCSB->InitialUpdate();
		return;
	}
	
	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	ITextManager *pTextM = GetSingleton<ITextManager>();
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	IUIElement *pSelBar = pSB->GetBar( nBar );
	NI_ASSERT( pSelBar != 0 );
	IUIElement *pSelElement = pSB->GetItem( nBar, nItem );
	NI_ASSERT( pSelElement != 0 );
	//� ��������� ���� �������� ��� ������
	//� ��������� ����� �������� ������ ���������� �����
	
	//��������� choose ShortcutBar
	//������� ��� � ����� ������
	{
		IUIElement *pBar = pCSB->AddBar();
		std::string szKey = NStr::Format( "textes\\RPGClasses\\class%d", pSelBar->GetWindowID() );
		CPtr<IText> pText = pTextM->GetDialog( szKey.c_str() );
//		const char *pTemp = GetUnitClassName( pSelBar->GetWindowID() );
//		CPtr<IText> pText = pTextM->GetString( pTemp );
		NI_ASSERT_T( pText != 0, NStr::Format( "Can not get text for key: %s", szKey.c_str() ) );
		pBar->SetWindowText( 0, pText->GetString() );
		pBar->SetWindowText( 1, pText->GetString() );
	}

	//������ ����� ����� ������ ���� ��� ��������� � ������
	std::list<int> addedUnits;
	for ( int i=0; i<pSB->GetNumberOfItems(nBar); i++ )
	{
		IUIElement *pElement = pSB->GetItem( nBar, i );
		int nIndex = pElement->GetWindowID();
		if ( nIndex != -1 )
			addedUnits.push_back( nIndex );
	}
	
	//������� ����� ������ ����
	int nNumUnits = pST->GetNumUnits();
	{
		//������� item ��� ������ ������
		IUIDialog *pItem = checked_cast<IUIDialog *>( pCSB->AddItem() );
		pItem->GetChildByID( 10000 )->SetWindowID( 20000 + (-1) );
		CPtr<IText> pText = pTextM->GetString( "clearunit" );
		pItem->SetWindowID( -1 );

		//��������� ��� �����
		pItem->SetWindowText( 0, pText->GetString() );

		//������ ��������
		IUIElement *pPicture = pItem->GetChildByID( 11 );
		pPicture->ShowWindow( UI_SW_HIDE );

		if ( pSelElement->GetWindowID() == -1 )
		{
			//�������� ���� item
			pCSB->SetSelectionItem( 0, 0 );
		}
	}
	for ( int z=0; z<nNumUnits; z++ )
	{
		const SUnitBaseRPGStats *pRPG = pST->GetUnitRPGStats( z );
		if ( pRPG->GetRPGClass() == pSelBar->GetWindowID() && !pST->IsUnitKilled( z ) )
		{
			//������� item � ������ RPG stats
			IUIDialog *pItem = checked_cast<IUIDialog *>( pCSB->AddItem() );
			FillUnitInfoItem( pRPG, pItem, z, false );
			
			//���������, �������� �� ���� ���� � ������
			if ( z == pSelElement->GetWindowID() )
			{
				EnableItem( pItem, true );
//				pItem->EnableWindow( true );
				pCSB->SetSelectionItem( 0, pCSB->GetNumberOfItems( 0 ) - 1 );
			}
			else
			{
				std::list<int>::iterator findIt = std::find( addedUnits.begin(), addedUnits.end(), z );
				if ( findIt != addedUnits.end() )
				{
					EnableItem( pItem, false );
//				pItem->EnableWindow( false );
				}
				else
				{
					EnableItem( pItem, true );
//				pItem->EnableWindow( true );
				}
			}
		}
	}

	pCSB->SetBarExpandState( 0, true );
	pCSB->InitialUpdate();*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceAddUnitToMission::EnableItem( IUIContainer *pItem, bool bEnable )
{
	pItem->EnableWindow( bEnable );
	IUIElement *pE = pItem->GetChildByID( 20 );
	pE->EnableWindow( bEnable );
	pE = pItem->GetChildByID( 21 );
	pE->EnableWindow( bEnable );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceAddUnitToMission::SelectItem()
{
	NI_ASSERT_T( FALSE, "OLD INTERFACE" );
	/*
	//choose units ShortcutBar
	IUIShortcutBar *pCSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 101 ) );
	int nCBar = -1, nCItem = -1;
	pCSB->GetSelectionItem( &nCBar, &nCItem );
	if ( nCBar == -1 || nCItem == -1 )
		return;
	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	IUIContainer *pCEl = static_cast<IUIContainer *> ( pCSB->GetItem( nCBar, nCItem ) );
	
	IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 100 ) );
	int nBar = -1, nItem = -1;
	pSB->GetSelectionItem( &nBar, &nItem );
	if ( nBar == -1 )
		return;
	IUIContainer *pItem = static_cast<IUIContainer *> ( pSB->GetItem( nBar, nItem ) );
	NI_ASSERT( pItem != 0 );
	
	ITextManager *pTextM = GetSingleton<ITextManager>();
	//����������� �� ������
	if ( pItem->GetWindowID() != -1 )
	{
		pST->RemoveUnitFromMission( pItem->GetWindowID() );
	}
	
	pItem->SetWindowID( pCEl->GetWindowID() );
	if ( pCEl->GetWindowID() == -1 )
	{
		//������ ������ �����
		CPtr<IText> pText = pTextM->GetString( "emptyslot" );
		NI_ASSERT_T( pText != 0, NStr::Format( "Can not get text by key: emptyslot" ) );
		pItem->SetWindowText( 0, pText->GetString() );
		
		//��������� ��� �����
		IUIElement *pElement = pItem->GetChildByID( 20 );
		pElement->SetWindowText( 0, L"" );
		
		//��� ���������
		pElement = pItem->GetChildByID( 21 );
		pElement->SetWindowText( 0, L"" );
		
		//������ ��������
		IUIElement *pPicture = pItem->GetChildByID( 11 );
		pPicture->ShowWindow( UI_SW_HIDE );
		
		return;
	}
	else
	{
		//��������� ��� �����
		pItem->SetWindowText( 0, L"" );

		IUIElement *pElement = pItem->GetChildByID( 20 );
		IUIElement *pCElement = pCEl->GetChildByID( 20 );
		pElement->SetWindowText( 0, pCElement->GetWindowText( 0 ) );
		
		//��� ���������
		pElement = pItem->GetChildByID( 21 );
		pCElement = pCEl->GetChildByID( 21 );
		pElement->SetWindowText( 0, pCElement->GetWindowText( 0 ) );
		
		//��������� �������� �����
		IUIElement *pPicture = pItem->GetChildByID( 11 );
		pPicture->ShowWindow( UI_SW_SHOW );
		IUIElement *pCPicure = pCEl->GetChildByID( 11 );
		pPicture->SetWindowTexture( pCPicure->GetWindowTexture() );

		return;
	}
	*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceAddUnitToMission::ProcessMessage( const SGameMessage &msg )
{
	NI_ASSERT_T( FALSE, "OLD INTERFACE" );
	/*
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
		{
			//���������� ����������
			//choose shortcut bar
			IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
			IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 100 ) );
			int i = 0;
			IUIElement *pBar = 0;
			while ( pBar = pSB->GetBar( i ) )
			{
				int nNumOfItems = pSB->GetNumberOfItems( i );
				for ( int k=0; k<nNumOfItems; k++ )
				{
					IUIElement *pItem = pSB->GetItem( i, k );
					NI_ASSERT_T( pItem != 0, "Error in AddUnitToMission interface" );
					int nID = pItem->GetWindowID();
					if ( nID != -1 )
					{
						pST->AddUnitToMission( nID );
					}

					m_missionSlots[pBar->GetWindowID()][k] = pItem->GetWindowID();
				}
				i++;
			}

			CloseInterface( true );
			FinishInterface( MAIN_COMMAND_POP, 0 );
			return true;
		}
		
		case IMC_DEFAULT_UNITS:
			AddDefaultSlotsToST();
			DisplaySlotsFromST();
			return true;
			
		case UI_NOTIFY_SELECTION_CHANGED:
			if ( msg.nParam == 100 )
				UpdateUnitsList();
			else if ( msg.nParam == 101 )
				SelectItem();
			else
				NI_ASSERT_T( 0, "WTF" );
			return true;
			
		case UI_NOTIFY_BAR_EXPAND:
			if ( msg.nParam == 100 )
				UpdateUnitsList();
			else if ( msg.nParam != 101 )
				NI_ASSERT_T( 0, "WTF" );
			return true;
	}
	
	if ( msg.nEventID >= 20000 && msg.nEventID < 21000 )
	{
		//������� ������������
		std::string szTemp = NStr::Format( "%d;", E_UNIT );
		
		const SUnitBaseRPGStats *pRPG = GetSingleton<IScenarioTracker>()->GetUnitRPGStats( msg.nEventID - 20000 );
		szTemp += pRPG->szParentName.c_str();
		FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szTemp.c_str() );
	}
	*/
	
	
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
