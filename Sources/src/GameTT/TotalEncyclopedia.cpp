#include "StdAfx.h"

#include "TotalEncyclopedia.h"

#include "..\Main\ScenarioTracker.h"
#include "..\UI\UIMessages.h"
#include "CommonId.h"
#include "UnitTypes.h"
#include "etypes.h"
#include "SaveLoadCommon.h"
#include "..\StreamIO\ProgressHook.h"
#include "..\Misc\Checker.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get index in unitTypes by EUnitRPGClass
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int GetUnitClassNumberByRPGClass( const EUnitRPGClass eClass )
{
	for ( int k = 0; k < nUnitClassesSize; ++k )
	{
		if ( eClass == unitClasses[k].nClass )
		{
			return k;
		}
	}
	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum 
{
	E_START_WINDOW_ID					= 20000,
	E_PLAYER_UNITS_SB					= 100,
	E_DEPOT_UNITS_SB					= 101,

	E_UPGRADE_BUTTON					= 10003,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*****************************************************************************************
//	CInterfaceUnitsEncyclopediaBase
//*****************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct
{
	char szDirName[20];
} dirNames[3] = 
{
	"allies",
	"german",
	"ussr",
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char * CInterfaceUnitsEncyclopediaBase::GetUnitNameByWindowID( const int nID )
{
	if ( nID >= E_START_WINDOW_ID && nID < E_START_WINDOW_ID + 9000 )
	{
		std::unordered_map< int/*nWindowID*/, const SGDBObjectDesc * >::const_iterator it = gdbByWindowID.find( nID - E_START_WINDOW_ID );
		if ( it != gdbByWindowID.end() )
			return it->second->szKey.c_str();
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceUnitsEncyclopediaBase::FillUnitsList( const int nListType, IUIShortcutBar *pSB, interface IMovieProgressHook *pProgress, const bool bFillName )
{
	if ( nViewUnitsType != -1 && nListType == nViewUnitsType ) return;
	nViewUnitsType = nListType;

	//init Shortcut Bar
	ITextManager *pTextM = GetSingleton<ITextManager>();
	NI_ASSERT_T( pSB != 0, "ShortcutBar is not initialized" );
	pSB->Clear();
	
	IObjectsDB *pIDB = GetSingleton<IObjectsDB>();
	CPtr<IText> pText;
	int nBarIndex = 0;
	const SGDBObjectDesc *pDescs = pIDB->GetAllDescs();
	
	if ( nListType >= unitsArray.size() )		// merge all lists and show it in separate window 
	{
		//���������� ���� �� ���� ���� ��������������� �������
		for ( int i = 0; i < nUnitTypesSize; ++i )
		{
			int nSum = 0;
			for ( int z = 0; z < nSides; ++z )
				nSum += unitsArray[z][i].size();
			if ( nSum == 0 )
				continue;			//�� ���� ������ ���������
			
			//Add bar
			IUIElement *pBar = pSB->AddBar();
			std::string szKey = NStr::Format( "textes\\RPGTypes\\type%d", i );
			pText = pTextM->GetDialog( szKey.c_str() );
			NI_ASSERT_T( pText != 0, NStr::Format( "Can not get text by key: %s", szKey.c_str() ) );
			pBar->SetWindowText( 0, pText->GetString() );
			pBar->SetWindowText( 1, pText->GetString() );
			pBar->SetWindowID( unitTypes[i].nClass );
			nBarIndex++;

			pSB->AddMultyItems( nSum );
			int nItemIndex = 0;
			for ( int z = 0; z < nSides; ++z )
			{
				const CUnitTypesVector &vec = unitsArray[ z ];
				if ( vec[i].empty() )
					continue;
				
				for ( int k = 0; k < vec[i].size(); ++k )
				{
					//������� item � ������ RPG stats
					IUIDialog *pItem = checked_cast<IUIDialog *>( pSB->GetItem( nBarIndex-1, nItemIndex + k ) );
					const SUnitBaseRPGStats *pRPG = vec[i][k];
					if ( pProgress )
						pProgress->Step();
					FillUnitInfoItem( pRPG, pItem, windowIDs[vec[i][k]], bFillName );
				}

				nItemIndex += vec[i].size();
			}
		}
	}
	else
	{
		const CUnitTypesVector &vec = unitsArray[ nViewUnitsType ];
		if ( vec.empty() )
		{
			pSB->InitialUpdate();
			return;
		}

		for ( int i = 0; i < nUnitTypesSize; ++i )
		{
			if ( vec[i].empty() )
				continue;
			
			//Add bar
			IUIElement *pBar = pSB->AddBar();
			std::string szKey = NStr::Format( "textes\\RPGTypes\\type%d", i );
			pText = pTextM->GetDialog( szKey.c_str() );
			NI_ASSERT_T( pText != 0, NStr::Format( "Can not get text by key: %s", szKey.c_str() ) );
			pBar->SetWindowText( 0, pText->GetString() );
			pBar->SetWindowText( 1, pText->GetString() );
			pBar->SetWindowID( unitTypes[i].nClass );
			nBarIndex++;
			
			pSB->AddMultyItems( vec[i].size() );
			for ( int k = 0; k < vec[i].size(); ++k )
			{
				//������� item � ������ RPG stats
				IUIDialog *pItem = checked_cast<IUIDialog *>( pSB->GetItem( nBarIndex-1, k ) );
				const SUnitBaseRPGStats *pRPG = vec[i][k];
				if ( pProgress )
					pProgress->Step();
				FillUnitInfoItem( pRPG, pItem, windowIDs[vec[i][k]], bFillName );
			}
		}
	}

	pSB->InitialUpdate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*****************************************************************************************
//	CInterfaceUnitsEncyclopediaBase
//*****************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECommands
{
	IMC_UNIT_INFO			=	10006,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CICTotalEncyclopedia::PostCreate( IMainLoop *pML, CInterfaceTotalEncyclopedia *pITE )
{
	pML->PushInterface( pITE );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceTotalEncyclopedia::~CInterfaceTotalEncyclopedia()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceTotalEncyclopedia::Init()
{
	CInterfaceInterMission::Init();
	//	SetBindSection( "intermission" );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceTotalEncyclopedia::StartInterface()
{
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\TotalEncyclopedia" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	//������� ���������� �� ������ � �������� ��������������� �������
	InitUnitLists();

	int nType = 0;
	std::string szSide = GetSingleton<IScenarioTracker>()->GetPlayer( 0 )->GetGeneralSide();
	NStr::ToLower( szSide );
	if ( szSide == "german" )
	{
		nType = GERMAN;
	}
	else if ( szSide == "allies" )
	{
		nType = ALLIES;
	}
	else if ( szSide == "ussr" )
	{
		nType = USSR;
	}
	else
	{
		NI_ASSERT_T( 0, NStr::Format( "Unknown player side in Total Encyclopedia: %s", szSide.c_str() ) );
	}


	SetActiveUnitsType( nType );
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	StoreScreen();
	pScene->AddUIScreen( pUIScreen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUnitTypeSortFunctor
{
public:
	bool operator()( const CGDBPtr<SUnitBaseRPGStats> p1, const CGDBPtr<SUnitBaseRPGStats> p2 ) const
	{
		return p1->szKeyName < p2->szKeyName;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceTotalEncyclopedia::InitUnitLists()
{
	IObjectsDB *pIDB = GetSingleton<IObjectsDB>();
	const int nMaxDescs = pIDB->GetNumDescs();
	NI_ASSERT( nMaxDescs < 9000);		// �� ��� ���, ����� ����, ������������ ����� �� ���������� ��� ��������� ������ �� TotalEncyclopedia. ����� ������� hash_map, ��� ����� ����� � TotalEncyclopedia ����� E_START_WINDOW_ID <= id <= E_START_WINDOW_ID +9000. ����� ������� �������. ����� ����� �������� �� �������, ������� ����, �� �� ������...
	const SGDBObjectDesc *pDescs = pIDB->GetAllDescs();

	CPtr<IMovieProgressHook> pProgress = CreateObject<IMovieProgressHook>( MAIN_PROGRESS_INDICATOR );
	pProgress->Init( IMovieProgressHook::PT_TOTAL_ENCYCLOPEDIA_LOAD );
	pProgress->SetNumSteps( nMaxDescs / 9  );
	
	int nCurrentWindowID = 0;
	
	for ( int k = 0; k < nSides; ++k )
		unitsArray[k].resize( nUnitTypesSize );

	IDataStorage * pStorage = GetSingleton<IDataStorage>();

	for ( int i = 0; i < nMaxDescs; ++i )
	{
		if ( !pDescs[i].IsTechnics() )
			continue;
		const std::string szDesctFile = pDescs[i].szPath + "\\desc.txt";
		
		if ( pStorage->IsStreamExist( szDesctFile.c_str() ) )
		{
			int z = 0;
			for ( ; z < nSides; ++z )
			{
				const char *pTemp = strstr( pDescs[i].szPath.c_str(), dirNames[z].szDirName );
				if ( !pTemp )
					continue;

				//������� ���� � ������ � ����������� �� ����
				CUnitTypesVector &vec = unitsArray[z];
				const SUnitBaseRPGStats *pRPG = checked_cast<const SUnitBaseRPGStats *>( pIDB->GetRPGStats( pDescs+i ) );
				// check if this unit has desc.txt file
				
				const EUnitRPGType type = pRPG->GetMainType();
				int k = 0;
				for ( ; k < nUnitTypesSize; ++k )
				{
					if ( type & unitTypes[k].nClass )
					{
						windowIDs[pRPG] = nCurrentWindowID;
						gdbByWindowID[nCurrentWindowID] = &pDescs[i];
						++nCurrentWindowID;
						pProgress->Step();
						vec[k].push_back( pRPG );
						break;
					}
				}
				NI_ASSERT_T( k != nUnitTypesSize, "Can not find unit type in list of types" );
				break;		//����� ������� ������� �����
			}
			NI_ASSERT_T( z != nSides, NStr::Format( "Unit of unknown side: %s", pDescs[i].szPath.c_str() ) );
		}
	}
	//��������� ������� �� ����� ������
	SUnitTypeSortFunctor sf;
	for ( int z = 0; z < nSides; ++z )
	{
		for ( int i = 0; i < nUnitTypesSize; i++ )
		{
			std::sort( unitsArray[z][i].begin(), unitsArray[z][i].end(), sf );
		}
	}

	for ( int i = 0; i < 4; ++i )
	{
		IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 100 + i ) );
		FillUnitsList( i, pSB, pProgress );
	}
	InitialUpdate();
	pProgress->Stop();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceTotalEncyclopedia::SetActiveUnitsType( int nType )
{
	//������� ���������� state � ������� ��������
	IUIElement *pElement = 0;
	if ( GetViewIndex() != -1 )
	{
		pElement = pUIScreen->GetChildByID( GetViewIndex() + 1000 );
		NI_ASSERT_T( pElement != 0, NStr::Format( "No control with id %d", GetViewIndex() + 1000 ) );
		pElement->EnableWindow( true );
	}
	
	//��������� ���������� state � �������� nType
	pElement = pUIScreen->GetChildByID( nType + 1000 );
	NI_ASSERT_T( pElement != 0, NStr::Format( "No control with id %d", nType + 1000 ) );
	pElement->EnableWindow( false );
	
	
	for ( int i = 0; i < 4; ++i )
	{
		IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( 100 + i ) );
		pSB->ShowWindow( nType == i ? UI_SW_SHOW : UI_SW_HIDE );
	}
	SetViewIndex( nType );

	IUIElement *pHeader = pUIScreen->GetChildByID( 20001 );
	NI_ASSERT_T( pHeader != 0, "Error: there is no text control in Total Encyclopedia with id 20001" );
	std::string szKey = "textes\\ui\\intermission\\mainmenu\\totalencyclopedia\\";
	switch ( nType )
	{
		case 0:
			szKey += "allies";
			break;
		case 1:
			szKey += "german";
			break;
		case 2:
			szKey += "ussr";
			break;
		case 3:
			szKey += "all";
			break;
		default:
			NI_ASSERT_T( 0, "Unknown units side" );
	}
	
	IText * pText = GetSingleton<ITextManager>()->GetDialog( szKey.c_str() );
	NI_ASSERT_T( pText != 0, NStr::Format( "Can not get text by key %s", szKey.c_str() ) );
	if ( pText != 0 )
		pHeader->SetWindowText( 0, pText->GetString() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceTotalEncyclopedia::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			CloseInterface( true );
			return true;
	}

	if ( msg.nEventID >= 1000 && msg.nEventID < 1010 )
	{
		//������ �� ������ ����� ������� �������, ������� ������
		SetActiveUnitsType( msg.nEventID - 1000 );
		return true;
	}

	const char *pszUnitName = GetUnitNameByWindowID( msg.nEventID );
	if ( 0 != pszUnitName )
	{
		std::string szTemp = NStr::Format( "%d;", E_UNIT );
		szTemp += pszUnitName;
		FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szTemp.c_str() );
	}
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*****************************************************************************************
//	CUnitInfoItem
//*****************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceWarehouse::CUnitInfoItem::CUnitInfoItem( const int _nCommanderName, const std::string &_szRPGStats, const int _nWindowID )
: szCurrentRPGStats( _szRPGStats ), nCommanderName( _nCommanderName ), nWindowID( _nWindowID )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceWarehouse::CUnitInfoItem::OnHelpCalled( const SGameMessage &msg, std::string *pHelp )
{
	if ( msg.nEventID - 20000 == nWindowID )
	{
		*pHelp = NStr::Format( "%d;", E_UNIT );
		*pHelp += szCurrentRPGStats;
		return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CInterfaceWarehouse::CUnitInfoItem::ApplyUpgrades()
{
	if ( IsEmpty() ) return 0;
	GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetUnit( nCommanderName )->ChangeRPGStats( szCurrentRPGStats );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitInfoItem::EnableWindow( const bool bEnable )
{
	NI_ASSERT_T( pSBItem != 0, "trying to enable unitialized window" );
	pSBItem->EnableWindow( bEnable );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitInfoItem::Init( IUIDialog *_pSBItem, IObjectsDB *pIDB )
{
	pSBItem = _pSBItem;
	ApplyRPGStats( szCurrentRPGStats, pIDB );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitInfoItem::ApplyRPGStats( const std::string &szNewStats, IObjectsDB *pIDB )
{
	szCurrentRPGStats = szNewStats;

	const SGDBObjectDesc *pDesc = pIDB->GetDesc( szCurrentRPGStats.c_str() );
	NI_ASSERT_T( pDesc != 0, NStr::Format( "unit not valid %s", szCurrentRPGStats.c_str() ) );
	const SUnitBaseRPGStats *pRPG = static_cast<const SUnitBaseRPGStats*>( pIDB->GetRPGStats( pDesc ) );

	ITextManager *pTextM = GetSingleton<ITextManager>();
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	
	IUIElement * pHelpButton = pSBItem->GetChildByID( 10000 );
	if ( pHelpButton )
		pHelpButton->SetWindowID( 20000 + nWindowID );		//��� ������������
	pSBItem->SetWindowID( nWindowID );

	FillUnitInfoItemNoIDs( pRPG, pSBItem, nCommanderName, nCommanderName != -1, 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitInfoItem::PerformUpgrade()
{
	if ( nCommanderName != -1 )
	{
		IPlayerScenarioInfo *pPlayerInfo = GetSingleton<IScenarioTracker>()->GetUserPlayer();
		IScenarioUnit *pUnit = pPlayerInfo->GetUnit( nCommanderName );
		pUnit->ChangeRPGStats( szCurrentRPGStats );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*****************************************************************************************
//	CUnitClassInfo
//*****************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitClassInfo::Expand( IUIShortcutBar *pSB, const bool bExpand, const bool bDisableCollapsed, const bool bNotify )
{
	if ( !IsEmpty() )
	{
		pSB->SetBarExpandState( nSBIndex, bExpand, bNotify );
		if ( bDisableCollapsed )
		{
			for ( int i = 0; i < units.size(); ++i )
				units[i]->EnableWindow( bExpand );
		}
		if ( bExpand )
			SelectFirstItem( pSB );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CInterfaceWarehouse::CUnitClassInfo::ApplyUpgrades()
{
	std::for_each( units.begin(), units.end(), std::mem_fun( CUnitInfoItem::ApplyUpgrades ) );
	// fuck
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceWarehouse::CUnitInfoItem * CInterfaceWarehouse::CUnitClassInfo::GetUnit( const int nIndex )
{
	CheckRange( units, nIndex );
	return units[nIndex];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char * CInterfaceWarehouse::CUnitClassInfo::GetRPGStats( const int nIndex )
{
	return GetUnit( nIndex )->GetRPGStats();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceWarehouse::CUnitClassInfo::OnHelpCalled( const SGameMessage &msg, std::string *pHelp )
{
	for ( int i = 0; i < units.size(); ++i )
	{
		if ( units[i]->OnHelpCalled( msg, pHelp ) )
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitClassInfo::AddItem( const std::string &szRPGStats, const int nCommanderNumber, const int nWindowID )
{
	units.push_back( new CUnitInfoItem( nCommanderNumber, szRPGStats, nWindowID ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitClassInfo::Init( IUIShortcutBar *pSB, const int _nSBIndex, const EUnitRPGClass eType, IObjectsDB *pIDB )
{
	nSBIndex = _nSBIndex;
	//Add bar
	IUIElement *pBar = pSB->AddBar();
	const std::string szKey = NStr::Format( "textes\\RPGClasses\\class%d", eType );
	IText * pText = GetSingleton<ITextManager>()->GetDialog( szKey.c_str() );
	NI_ASSERT_T( pText != 0, NStr::Format( "Can not get text by key: %s", szKey.c_str() ) );
	
	pBar->SetWindowText( 0, pText->GetString() );
	pBar->SetWindowText( 1, pText->GetString() );

	//CRAP{ WHY?
	pBar->SetWindowID( unitClasses[eType].nClass );
	//CRAP}

	// add every item
	for ( int i = 0; i < units.size(); ++i )
	{
		IUIDialog *pItem = checked_cast<IUIDialog *>( pSB->AddItem() );
		units[i]->Init( pItem, pIDB );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceWarehouse::CUnitClassInfo::OnSelectionChanged( IUIShortcutBar *pSB )
{
	if ( !IsEmpty() )
	{
		int nBarSelectedIndex = 0;
		int nIntemSelectedIndex = 0;
		pSB->GetSelectionItem( &nBarSelectedIndex, &nIntemSelectedIndex );
		if ( nBarSelectedIndex == nSBIndex )
		{
			if ( pSB->GetBarExpandState( nSBIndex ) ) // bar is expanded
			{
				if ( -1 == nIntemSelectedIndex )
				{
					pSB->SetSelectionItem( nSBIndex, 0 );
				}
			}
			return true;
		}
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitClassInfo::SelectFirstItem( IUIShortcutBar *pSB ) const
{
	if ( !IsEmpty() )
		pSB->SetSelectionItem( nSBIndex, 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*****************************************************************************************
//	CUnitsPane
//*****************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitsPane::Init( IUIShortcutBar *_pSB, int *pWindowIDs, IObjectsDB * pDB )
{
	IObjectsDB *pIDB = GetSingleton<IObjectsDB>();
	pSB = _pSB;

	// create classes 
	classes.resize( nUnitClassesSize );
	for ( int i = 0; i < classes.size(); ++i )
		classes[i] = new CUnitClassInfo;

	// init them. fill with items
	const int nMax = GetNUnits();
	
	// init items
	for ( int nUnit = 0; nUnit < nMax; ++nUnit )
	{
		const std::string &szStatsName = GetUnitStats( nUnit );
		const SGDBObjectDesc *pDesc = pDB->GetDesc( szStatsName.c_str() );
		NI_ASSERT_T( pDesc != 0, NStr::Format( "valid unit %s", szStatsName.c_str() ) );
		const SUnitBaseRPGStats *pRPG = static_cast<const SUnitBaseRPGStats*>( pDB->GetRPGStats( pDesc ) );
		const EUnitRPGClass type = pRPG->GetRPGClass();
	
		classes[GetUnitClassNumberByRPGClass(type)]->AddItem( szStatsName, GetScenarioNumber( nUnit ), *pWindowIDs );
		++(*pWindowIDs);
	}

	int nSBIdex = 0;
	bool bFirst = true;
	// add items to ShortcutBar
	for ( int i = 0; i < classes.size(); ++i )
	{
		if ( !classes[i]->IsEmpty() )
		{
			classes[i]->Init( pSB, nSBIdex++, static_cast<EUnitRPGClass>(i), pIDB );
			if ( bFirst )	// expand first bar
			{
				bFirst = false;
				classes[i]->Expand( pSB, true, IsDisableCollapsed(), true );
			}
			else
				classes[i]->Expand( pSB, false, IsDisableCollapsed(), false );
		}
	}
	pSB->InitialUpdate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitsPane::ExpandClass( const EUnitRPGClass eType )
{
	// expand current class
	// compact former expanded
	// select first item in this class
	ExpandIndex( GetUnitClassNumberByRPGClass(eType) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CUnitsPane::ExpandIndex( const int nTypeIndex )
{
	if ( nExpandedNumber != -1 )
		classes[nExpandedNumber]->Expand( pSB, false, IsDisableCollapsed(), true );
	
	nExpandedNumber = nTypeIndex;
	
	if ( nTypeIndex != -1 )
		classes[nExpandedNumber]->Expand( pSB, true, IsDisableCollapsed(), true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceWarehouse::CUnitsPane::OnSelectionChanged( int *pNewType )
{
	*pNewType = -1;
	for ( int i = 0; i < classes.size(); ++i )
	{
		if ( classes[i]->OnSelectionChanged( pSB ) )
		{
			*pNewType = i;
			break;
		}
	}

	int nNewSelectedBar;
	int nNewSelectedItem;
	pSB->GetSelectionItem( &nNewSelectedBar, &nNewSelectedItem );

	if ( nNewSelectedBar == -1 || nNewSelectedItem == -1 ) 
	{
		nSelectedBar = -1;
		*pNewType = -1;											// deselect all
		return true;
	}

	const bool bBarExpanded = pSB->GetBarExpandState( nNewSelectedBar );
	if ( !bBarExpanded )				// bar selected bar closed
	{
		nSelectedBar = -1;
		*pNewType = -1;											// deselect all
		return true;
	}

	if ( nNewSelectedBar != nSelectedBar )
	{
		nSelectedBar = nNewSelectedBar;
		return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceWarehouse::CUnitsPane::OnHelpCalled( const SGameMessage &msg, std::string *pHelp )
{
	for ( int i = 0; i < classes.size(); ++i )
	{
		if ( classes[i]->OnHelpCalled( msg, pHelp ) )
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceWarehouse::CUnitInfoItem * CInterfaceWarehouse::CUnitsPane::GetCurrentUnitPtr()
{
	int nSelectedBar;
	int nSelectedItem;
	
	pSB->GetSelectionItem( &nSelectedBar, &nSelectedItem );
	if ( nSelectedBar == -1 || nSelectedItem == -1 ) 
		return 0;
	
	const char * pRet = 0;
	for ( int i = nSelectedBar; i < classes.size() && !pRet; ++i )
	{
		if ( classes[i]->GetID() == nSelectedBar )
			return classes[i]->GetUnit( nSelectedItem );
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char * CInterfaceWarehouse::CUnitsPane::GetCurrentUnit()
{
	CUnitInfoItem *pInfo = GetCurrentUnitPtr();
	if ( pInfo ) 
		return pInfo->GetRPGStats();
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*****************************************************************************************
//	CDepotUnitsPane
//*****************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CInterfaceWarehouse::CDepotUnitsPane::GetNUnits() const
{
	return GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetNumDepotUpgrades();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string & CInterfaceWarehouse::CDepotUnitsPane::GetUnitStats( const int nUnitIndex ) const
{
	return GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetDepotUpgrade( nUnitIndex );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CInterfaceWarehouse::CDepotUnitsPane::GetScenarioNumber( const int nIndex ) const
{
	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceWarehouse::CDepotUnitsPane::IsDisableCollapsed() const
{
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*****************************************************************************************
//	CPlayerUnitsPane
//*****************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CInterfaceWarehouse::CPlayerUnitsPane::GetNUnits() const
{
	return GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetNumUnits();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string & CInterfaceWarehouse::CPlayerUnitsPane::GetUnitStats( const int nUnitIndex ) const
{
	return GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetUnit( nUnitIndex )->GetRPGStats();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CInterfaceWarehouse::CPlayerUnitsPane::GetScenarioNumber( const int nIndex ) const
{
	return nIndex;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceWarehouse::CPlayerUnitsPane::IsDisableCollapsed() const
{
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::CPlayerUnitsPane::SetUpgrade( const std::string &szNewStats, IObjectsDB *pDB )
{
	CUnitInfoItem *pUnit = GetCurrentUnitPtr();
	NI_ASSERT_T( pUnit != 0, "no unit to upgrade" );
	if ( pUnit )
		pUnit->ApplyRPGStats( szNewStats, pDB );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CInterfaceWarehouse::CPlayerUnitsPane::ApplyUpgrades()
{
	std::for_each( classes.begin(), classes.end(), std::mem_fun( CUnitClassInfo::ApplyUpgrades ) );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*****************************************************************************************
//	CInterfaceWarehouse
//*****************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceWarehouse::~CInterfaceWarehouse()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::EnableButton( const int nButtonID, const bool bEnable ) const 
{
	IUIElement *pButton = pUIScreen->GetChildByID( nButtonID );
	if ( pButton )
		pButton->EnableWindow( bEnable );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceWarehouse::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;


	if ( msg.nEventID >= E_START_WINDOW_ID && msg.nEventID < E_START_WINDOW_ID + 9000 )
	{
		std::string szHelp;
		if ( depotPane.OnHelpCalled( msg, &szHelp ) || playerPane.OnHelpCalled( msg, &szHelp ) )
			FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szHelp.c_str() );
		return true;
	}
	
	switch ( msg.nEventID )
	{
	case IMC_CANCEL:
		CloseInterface( true );
		
		return true;
	case UI_NOTIFY_SELECTION_CHANGED:
		if ( msg.nParam == E_PLAYER_UNITS_SB )
		{
			int nNewType;
			if ( playerPane.OnSelectionChanged( &nNewType ) )
				depotPane.ExpandIndex( nNewType );
		}
		else if ( msg.nParam == E_DEPOT_UNITS_SB )
		{
			int nNewType;
			if ( depotPane.OnSelectionChanged( &nNewType ) )
				playerPane.ExpandIndex( nNewType );	
		}
		
		EnableButton( E_UPGRADE_BUTTON, depotPane.GetCurrentUnit() && playerPane.GetCurrentUnit() );

		return true;
	case IMC_OK:
		playerPane.ApplyUpgrades();
		CloseInterface( true );

		return true;
	case E_UPGRADE_BUTTON:
		{
			const char *pszUpgrade = depotPane.GetCurrentUnit();
			if ( pszUpgrade )
			{
				const std::string szUpgrade = pszUpgrade;
				playerPane.SetUpgrade( szUpgrade, GetSingleton<IObjectsDB>() );
			}
		}

		break;
	}
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceWarehouse::Init()
{
	CInterfaceInterMission::Init();
	//	SetBindSection( "intermission" );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceWarehouse::StartInterface()
{
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\Warehouse" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	IUIShortcutBar *pSBPlayer = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( E_PLAYER_UNITS_SB ) );
	IUIShortcutBar *pSBDepot = checked_cast<IUIShortcutBar *> ( pUIScreen->GetChildByID( E_DEPOT_UNITS_SB ) );
	
	int nWindowIDs = 0;
	IObjectsDB * pDB = GetSingleton<IObjectsDB>();
	playerPane.Init( pSBPlayer, &nWindowIDs, pDB );
	depotPane.Init( pSBDepot, &nWindowIDs, pDB );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	StoreScreen();

	//depotPane.ExpandClass( RPG_TYPE_ARTILLERY );
	
	pScene->AddUIScreen( pUIScreen );
}
