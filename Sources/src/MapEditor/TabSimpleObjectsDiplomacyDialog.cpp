#include "StdAfx.h"

#include "../RandomMapGen/MapInfo_Types.h"
#include "TabSimpleObjectsDiplomacyDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE 
static char THIS_FILE[] = __FILE__;
#endif

const int   PLAYERS_COLUMN_COUNT = 2;
const char *PLAYERS_COLUMN_NAME  [PLAYERS_COLUMN_COUNT] = { "Player", "Side" };
const int   PLAYERS_COLUMN_FORMAT[PLAYERS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT };
int					PLAYERS_COLUMN_WIDTH [PLAYERS_COLUMN_COUNT] = { 80, 80 };

int CALLBACK PlayersCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CTabSimpleObjectsDiplomacyDialog* pDiplomacyDialog = reinterpret_cast<CTabSimpleObjectsDiplomacyDialog*>( lParamSort );

	CString strItem1 = pDiplomacyDialog->m_PlayersList.GetItemText( lParam1, pDiplomacyDialog->nSortColumn );
	CString strItem2 = pDiplomacyDialog->m_PlayersList.GetItemText( lParam2, pDiplomacyDialog->nSortColumn );
	if ( pDiplomacyDialog->bPlayersSortParam[pDiplomacyDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

const int CTabSimpleObjectsDiplomacyDialog::vID[] = 
{
	IDC_SO_DIPLOMACY_PLAYERS_LIST,					//0
	IDC_SO_DIPLOMACY_ADD_BUTTON,						//1
	IDC_SO_DIPLOMACY_DELETE_BUTTON,					//2
	IDC_SO_DIPLOMACY_SIDE0_BUTTON,					//3
	IDC_SO_DIPLOMACY_SIDE1_BUTTON,					//4
	IDOK,																		//5
	IDCANCEL,																//6
	IDC_SO_DIPLOMACY_PLAYERS_LABEL,					//7
	IDC_SO_DIPLOMACY_TYPE_LABEL,						//8
	IDC_SO_DIPLOMACY_TYPE_COMBO_BOX,				//9
	IDC_SO_DIPLOMACY_ATTACK_SIDE_LABEL,			//10
	IDC_SO_DIPLOMACY_ATTACK_SIDE_COMBO_BOX,	//11
};

const char CTabSimpleObjectsDiplomacyDialog::SIDE0_LABEL[] = _T( "Side 0" );
const char CTabSimpleObjectsDiplomacyDialog::SIDE1_LABEL[] = _T( "Side 1" ); 

CTabSimpleObjectsDiplomacyDialog::CTabSimpleObjectsDiplomacyDialog( CWnd* pParent )
	: CResizeDialog( CTabSimpleObjectsDiplomacyDialog::IDD, pParent ), bCreateControls( false ), nType( CMapInfo::TYPE_SINGLE_PLAYER ), nAttackingSide( 0 )
{

	SetControlStyle( IDC_SO_DIPLOMACY_PLAYERS_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	SetControlStyle( IDC_SO_DIPLOMACY_ADD_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SO_DIPLOMACY_DELETE_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SO_DIPLOMACY_SIDE0_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SO_DIPLOMACY_SIDE1_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDOK, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDCANCEL, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDC_SO_DIPLOMACY_PLAYERS_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SO_DIPLOMACY_TYPE_LABEL, ANCHORE_LEFT_BOTTOM | RESIZE_HOR );
	SetControlStyle( IDC_SO_DIPLOMACY_TYPE_COMBO_BOX, ANCHORE_LEFT_BOTTOM | RESIZE_HOR );
	SetControlStyle( IDC_SO_DIPLOMACY_ATTACK_SIDE_LABEL, ANCHORE_LEFT_BOTTOM | RESIZE_HOR );
	SetControlStyle( IDC_SO_DIPLOMACY_ATTACK_SIDE_COMBO_BOX, ANCHORE_LEFT_BOTTOM | RESIZE_HOR );
}

void CTabSimpleObjectsDiplomacyDialog::DoDataExchange( CDataExchange* pDX )
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_SO_DIPLOMACY_TYPE_COMBO_BOX, m_Types);
	DDX_Control(pDX, IDC_SO_DIPLOMACY_ATTACK_SIDE_COMBO_BOX, m_Sides);
	DDX_Control(pDX, IDC_SO_DIPLOMACY_PLAYERS_LIST, m_PlayersList);
}

BEGIN_MESSAGE_MAP(CTabSimpleObjectsDiplomacyDialog, CResizeDialog )
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_SO_DIPLOMACY_PLAYERS_LIST, OnColumnclickPlayersList)
	ON_BN_CLICKED(IDC_SO_DIPLOMACY_ADD_BUTTON, OnAddButton)
	ON_BN_CLICKED(IDC_SO_DIPLOMACY_DELETE_BUTTON, OnDeleteButton)
	ON_BN_CLICKED(IDC_SO_DIPLOMACY_SIDE0_BUTTON, OnSide0Button)
	ON_BN_CLICKED(IDC_SO_DIPLOMACY_SIDE1_BUTTON, OnSide1Button)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_SO_DIPLOMACY_PLAYERS_LIST, OnItemchangedPlayersList)
	ON_COMMAND(TAB_SIMPLE_OBJECTS_DIPLOMACY_ADD_PLAYER_MENU, OnSimpleObjectsDiplomacyAddPlayerMenu)
	ON_COMMAND(TAB_SIMPLE_OBJECTS_DIPLOMACY_DELETE_PLAYER_MENU, OnSimpleObjectsDiplomacyDeletePlayerMenu)
	ON_COMMAND(TAB_SIMPLE_OBJECTS_DIPLOMACY_SIDE_0_MENU, OnSimpleObjectsDiplomacySide0Menu)
	ON_COMMAND(TAB_SIMPLE_OBJECTS_DIPLOMACY_SIDE_1_MENU, OnSimpleObjectsDiplomacySide1Menu)
	ON_NOTIFY(NM_RCLICK, IDC_SO_DIPLOMACY_PLAYERS_LIST, OnRclickPlayersList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_SO_DIPLOMACY_PLAYERS_LIST, OnKeydownPlayersList)
	ON_CBN_SELCHANGE(IDC_SO_DIPLOMACY_TYPE_COMBO_BOX, OnSelchangeSoDiplomacyTypeComboBox)
	ON_CBN_SELCHANGE(IDC_SO_DIPLOMACY_ATTACK_SIDE_COMBO_BOX, OnSelchangeSoDiplomacyAttackSideComboBox)
END_MESSAGE_MAP()

BOOL CTabSimpleObjectsDiplomacyDialog::OnInitDialog() 
{
	CResizeDialog ::OnInitDialog();

	if ( resizeDialogOptions.nParameters.size() < PLAYERS_COLUMN_COUNT )
	{
		resizeDialogOptions.nParameters.resize( PLAYERS_COLUMN_COUNT, 0 );
	}

	CreateControls();
	FillPlayers();
	return TRUE;
}

void CTabSimpleObjectsDiplomacyDialog::OnOK() 
{
	for ( int nColumnIndex = 0; nColumnIndex < PLAYERS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex] = m_PlayersList.GetColumnWidth( nColumnIndex );
	}

	CResizeDialog::OnOK();
}

void CTabSimpleObjectsDiplomacyDialog::OnCancel() 
{
	for ( int nColumnIndex = 0; nColumnIndex < PLAYERS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex] = m_PlayersList.GetColumnWidth( nColumnIndex );
	}

	CResizeDialog::OnCancel();
}

void CTabSimpleObjectsDiplomacyDialog::UpdateControls()
{
	CWnd* pWnd = 0;

	int nSelectedCount = 0;
	int nSide = 0;
	int nSelectedItem = m_PlayersList.GetNextItem( -1, LVNI_SELECTED );
	while ( nSelectedItem >= 0 )
	{
		std::string szKey = m_PlayersList.GetItemText( nSelectedItem, 0 );
		int nPlayerIndex = -1;
		if ( ( sscanf( szKey.c_str(), "%d", &nPlayerIndex ) > 0 ) &&
				 ( nPlayerIndex >= 0 ) && 
				 ( nPlayerIndex < ( diplomacies.size() - 1 ) ) )
		{
			++nSelectedCount;
			nSide = diplomacies[nPlayerIndex];

		}
		nSelectedItem = m_PlayersList.GetNextItem( nSelectedItem, LVNI_SELECTED );
	}

	if ( pWnd = GetDlgItem( IDC_SO_DIPLOMACY_ADD_BUTTON ) )
	{
		pWnd->EnableWindow( diplomacies.size() < 17 );
	}
	if ( pWnd = GetDlgItem( IDC_SO_DIPLOMACY_DELETE_BUTTON ) )
	{
		pWnd->EnableWindow( nSelectedCount > 0 );
	}
	if ( pWnd = GetDlgItem( IDC_SO_DIPLOMACY_SIDE0_BUTTON ) )
	{
		pWnd->EnableWindow( nSelectedCount > 0 );
	}
	if ( pWnd = GetDlgItem( IDC_SO_DIPLOMACY_SIDE1_BUTTON ) )
	{
		pWnd->EnableWindow( nSelectedCount > 0 );
	}
	if ( ( nSelectedCount > 0 ) )
	{
		CheckRadioButton( IDC_SO_DIPLOMACY_SIDE0_BUTTON, IDC_SO_DIPLOMACY_SIDE1_BUTTON, IDC_SO_DIPLOMACY_SIDE0_BUTTON + nSide );
	}
	if ( pWnd = GetDlgItem( IDC_SO_DIPLOMACY_ATTACK_SIDE_LABEL ) )
	{
		pWnd->EnableWindow( nType == CMapInfo::TYPE_SABOTAGE );
	}
	if ( pWnd = GetDlgItem( IDC_SO_DIPLOMACY_ATTACK_SIDE_COMBO_BOX ) )
	{
		pWnd->EnableWindow( nType == CMapInfo::TYPE_SABOTAGE );
	}
}

void CTabSimpleObjectsDiplomacyDialog::CreateControls()
{
	m_PlayersList.SetExtendedStyle( m_PlayersList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < PLAYERS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex] = PLAYERS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_PlayersList.InsertColumn( nColumnIndex, PLAYERS_COLUMN_NAME[nColumnIndex], PLAYERS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bPlayersSortParam.push_back( true );
	}

	m_Types.ResetContent();
	for ( int nTypeIndex = 0; nTypeIndex < CMapInfo::TYPE_COUNT; ++nTypeIndex )
	{
		m_Types.AddString( CMapInfo::TYPE_NAMES[nTypeIndex] );	
	}
	if ( ( nType < 0 ) || ( nType >= CMapInfo::TYPE_COUNT ) )
	{
		nType = 0;
	}
	m_Types.SelectString( -1, CMapInfo::TYPE_NAMES[nType] );

	m_Sides.ResetContent();
	m_Sides.AddString( SIDE0_LABEL );	
	m_Sides.AddString( SIDE1_LABEL );	
	if ( nAttackingSide == 0 )
	{
		m_Sides.SelectString( -1, SIDE0_LABEL );
	}
	else
	{
		m_Sides.SelectString( -1, SIDE1_LABEL );
	}
}

void CTabSimpleObjectsDiplomacyDialog::FillPlayers()
{
	bCreateControls = true;
	m_PlayersList.DeleteAllItems();

	for ( int nPlayerIndex = 0; nPlayerIndex < diplomacies.size(); ++nPlayerIndex )
	{
		if ( nPlayerIndex == ( diplomacies.size() - 1 ) )
		{
			diplomacies[nPlayerIndex] = 2;
		}
		
		int nNewItem = m_PlayersList.InsertItem( LVIF_TEXT, 0, NStr::Format( "%2d", nPlayerIndex ), 0, 0, 0, 0 );
		if ( nNewItem  != ( -1 ) )
		{
			m_PlayersList.SetItem( nNewItem, 1, LVIF_TEXT, NStr::Format( "%2d", diplomacies[nPlayerIndex] ), 0, 0, 0, 0 );
		}
	}

	bCreateControls = false;
	UpdateControls();
}
	
void CTabSimpleObjectsDiplomacyDialog::OnColumnclickPlayersList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < PLAYERS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, PLAYERS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_PlayersList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_PlayersList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_PlayersList.SortItems( PlayersCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bPlayersSortParam[nSortColumn] = !bPlayersSortParam[nSortColumn];
	*pResult = 0;
}

void CTabSimpleObjectsDiplomacyDialog::OnAddButton() 
{
	if ( diplomacies.size() < 17 )
	{
		bCreateControls = true;
		diplomacies.insert( diplomacies.begin() + diplomacies.size() - 1, 0 );
		bCreateControls = false;
		FillPlayers();
	}
}

void CTabSimpleObjectsDiplomacyDialog::OnDeleteButton() 
{
	bCreateControls = true;
	int nSelectedCount = 0;
	int nSelectedItem = m_PlayersList.GetNextItem( -1, LVNI_SELECTED );
	while ( nSelectedItem >= 0 )
	{
		std::string szKey = m_PlayersList.GetItemText( nSelectedItem, 0 );
		int nPlayerIndex = -1;
		if ( ( sscanf( szKey.c_str(), "%d", &nPlayerIndex ) > 0 ) &&
				 ( nPlayerIndex >= 0 ) && 
				 ( nPlayerIndex < ( diplomacies.size() - 1 ) ) )
		{
			diplomacies[nPlayerIndex] = 3;
		}
		nSelectedItem = m_PlayersList.GetNextItem( nSelectedItem, LVNI_SELECTED );
	}
	for ( std::vector<BYTE>::iterator playerIterator = diplomacies.begin(); playerIterator < diplomacies.end(); )
	{
		if ( ( *playerIterator ) == 3 )
		{
			playerIterator = diplomacies.erase( playerIterator );
		}
		else
		{
			++playerIterator;
		}
	}
	bCreateControls = false;
	FillPlayers();
}

void CTabSimpleObjectsDiplomacyDialog::OnSide0Button() 
{
	bCreateControls = true;
	int nSelectedItem = m_PlayersList.GetNextItem( -1, LVNI_SELECTED );
	while ( nSelectedItem >= 0 )
	{
		std::string szKey = m_PlayersList.GetItemText( nSelectedItem, 0 );
		int nPlayerIndex = -1;
		if ( ( sscanf( szKey.c_str(), "%d", &nPlayerIndex ) > 0 ) &&
				 ( nPlayerIndex >= 0 ) && 
				 ( nPlayerIndex < ( diplomacies.size() - 1 ) ) )
		{
			diplomacies[nPlayerIndex] = 0;
			m_PlayersList.SetItem( nSelectedItem, 1, LVIF_TEXT, NStr::Format( "%2d", diplomacies[nPlayerIndex] ), 0, 0, 0, 0 );
		}
		nSelectedItem = m_PlayersList.GetNextItem( nSelectedItem, LVNI_SELECTED );
	}
	bCreateControls = false;
	UpdateControls();
}

void CTabSimpleObjectsDiplomacyDialog::OnSide1Button() 
{
	bCreateControls = true;
	int nSelectedItem = m_PlayersList.GetNextItem( -1, LVNI_SELECTED );
	while ( nSelectedItem >= 0 )
	{
		std::string szKey = m_PlayersList.GetItemText( nSelectedItem, 0 );
		int nPlayerIndex = -1;
		if ( ( sscanf( szKey.c_str(), "%d", &nPlayerIndex ) > 0 ) &&
				 ( nPlayerIndex >= 0 ) && 
				 ( nPlayerIndex < ( diplomacies.size() - 1 ) ) )
		{
			diplomacies[nPlayerIndex] = 1;
			m_PlayersList.SetItem( nSelectedItem, 1, LVIF_TEXT, NStr::Format( "%d", diplomacies[nPlayerIndex] ), 0, 0, 0, 0 );
		}
		nSelectedItem = m_PlayersList.GetNextItem( nSelectedItem, LVNI_SELECTED );
	}
	bCreateControls = false;
	UpdateControls();
}

void CTabSimpleObjectsDiplomacyDialog::OnItemchangedPlayersList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if ( !bCreateControls )
	{
		UpdateControls();
	}
	*pResult = 0;
}

void CTabSimpleObjectsDiplomacyDialog::OnSimpleObjectsDiplomacyAddPlayerMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_ADD_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnAddButton();
		}
	}
}

void CTabSimpleObjectsDiplomacyDialog::OnSimpleObjectsDiplomacyDeletePlayerMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_DELETE_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnDeleteButton();
		}
	}
}

void CTabSimpleObjectsDiplomacyDialog::OnSimpleObjectsDiplomacySide0Menu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_SIDE0_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnSide0Button();
		}
	}
}

void CTabSimpleObjectsDiplomacyDialog::OnSimpleObjectsDiplomacySide1Menu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_SIDE1_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnSide1Button();
		}
	}
}

void CTabSimpleObjectsDiplomacyDialog::OnRclickPlayersList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu tabsMenu;
	tabsMenu.LoadMenu( IDM_TAB_POPUP_MENUS );
	CMenu *pMenu = tabsMenu.GetSubMenu( 2 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_ADD_BUTTON ) )
		{
			pMenu->EnableMenuItem( TAB_SIMPLE_OBJECTS_DIPLOMACY_ADD_PLAYER_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_DELETE_BUTTON ) )
		{
			pMenu->EnableMenuItem( TAB_SIMPLE_OBJECTS_DIPLOMACY_DELETE_PLAYER_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_SIDE0_BUTTON ) )
		{
			pMenu->EnableMenuItem( TAB_SIMPLE_OBJECTS_DIPLOMACY_SIDE_0_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_SIDE1_BUTTON ) )
		{
			pMenu->EnableMenuItem( TAB_SIMPLE_OBJECTS_DIPLOMACY_SIDE_1_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	tabsMenu.DestroyMenu();
	*pResult = 0;
}

void CTabSimpleObjectsDiplomacyDialog::OnKeydownPlayersList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_ADD_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_DELETE_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeleteButton();
			}
		}
	}
	else if ( ( pLVKeyDown->wVKey == 0x30 ) || ( pLVKeyDown->wVKey == VK_NUMPAD0 ) )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_SIDE0_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnSide0Button();
			}
		}
	}
	else if ( ( pLVKeyDown->wVKey == 0x31 ) || ( pLVKeyDown->wVKey == VK_NUMPAD1 ) )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_SO_DIPLOMACY_SIDE1_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnSide1Button();
			}
		}
	}
	*pResult = 0;
}

void CTabSimpleObjectsDiplomacyDialog::OnSelchangeSoDiplomacyTypeComboBox() 
{
	CString szBuffer;
	m_Types.GetWindowText( szBuffer );
	std::string szSelectedType = szBuffer;
	
	for ( int nTypeIndex = 0; nTypeIndex < CMapInfo::TYPE_COUNT; ++nTypeIndex )
	{
		if ( szSelectedType == std::string( CMapInfo::TYPE_NAMES[nTypeIndex] ) )
		{
			nType = nTypeIndex;
			UpdateControls();
			return;
		}
	}
	nType = CMapInfo::TYPE_SINGLE_PLAYER;
	UpdateControls();
}

void CTabSimpleObjectsDiplomacyDialog::OnSelchangeSoDiplomacyAttackSideComboBox() 
{
	CString szBuffer;
	m_Sides.GetWindowText( szBuffer );
	std::string szSelectedType = szBuffer;
	
	if ( szSelectedType == SIDE0_LABEL )
	{
		nAttackingSide = 0;
	}
	else
	{
		nAttackingSide = 1;
	}
	UpdateControls();
}


