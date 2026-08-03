#include "StdAfx.h"
#include "editor.h"
#include "PEPointsListDialog.h"
#include "PE_PointPropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int   POINTS_COLUMN_START = 0;
const int   POINTS_COLUMN_COUNT = 3;
const char *POINTS_COLUMN_NAME  [POINTS_COLUMN_COUNT] = { "Index", "X", "Y" };
const int   POINTS_COLUMN_FORMAT[POINTS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT };
const int		POINTS_COLUMN_WIDTH [POINTS_COLUMN_COUNT] = { 60, 30, 30 };

int CALLBACK PointsListCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CPEPointsListDialog* pPointsListDialog = reinterpret_cast<CPEPointsListDialog*>( lParamSort );

	CString strItem1 = pPointsListDialog->m_PointsList.GetItemText( lParam1, pPointsListDialog->nSortColumn );
	CString strItem2 = pPointsListDialog->m_PointsList.GetItemText( lParam2, pPointsListDialog->nSortColumn );
	if ( pPointsListDialog->bPointsSortParam[pPointsListDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

const int CPEPointsListDialog::vID[] = 
{
	IDC_PL_POINTS_LIST,							//0
	IDC_PL_ADD_POINT_BUTTON,				//1
	IDC_PL_DELETE_POINT_BUTTON,			//2
	IDC_PL_POINT_PROPERTIES_BUTTON,	//3
	IDOK,														//4
	IDCANCEL,												//5
};

CPEPointsListDialog::CPEPointsListDialog( CWnd* pParent )
: CResizeDialog( CPEPointsListDialog::IDD, pParent ), bCreateControls( false )
{
	SetControlStyle( vID[0], ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	SetControlStyle( vID[1], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[2], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[3], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[4], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[5], ANCHORE_RIGHT_BOTTOM );
}

void CPEPointsListDialog::DoDataExchange( CDataExchange* pDX )
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_PL_POINTS_LIST, m_PointsList);
}

BEGIN_MESSAGE_MAP( CPEPointsListDialog, CResizeDialog )
	ON_BN_CLICKED(IDC_PL_ADD_POINT_BUTTON, OnAddPointButton)
	ON_BN_CLICKED(IDC_PL_DELETE_POINT_BUTTON, OnDeletePointButton)
	ON_BN_CLICKED(IDC_PL_POINT_PROPERTIES_BUTTON, OnPointPropertiesButton)
	ON_COMMAND(IDC_PE_PL_ADD_MENU, OnAddMenu)
	ON_COMMAND(IDC_PE_PL_DELETE_MENU, OnDeleteMenu)
	ON_COMMAND(IDC_PE_PL_PROPERTIES_MENU, OnPropertiesMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_PL_POINTS_LIST, OnDblclkPointsList)
	ON_NOTIFY(NM_RCLICK, IDC_PL_POINTS_LIST, OnRclickPointsList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_PL_POINTS_LIST, OnColumnclickPointsList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_PL_POINTS_LIST, OnItemchangedPointsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_PL_POINTS_LIST, OnKeydownPointsList)
END_MESSAGE_MAP()

BOOL CPEPointsListDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	SetWindowText( szDialogName.c_str() );
	
	if ( resizeDialogOptions.nParameters.size() < POINTS_COLUMN_COUNT )
	{
		resizeDialogOptions.nParameters.resize( POINTS_COLUMN_COUNT, 0 );
	}
	
	CreateControls();
	LoadPointsList();
	UpdateControls();
	return true;
}

void CPEPointsListDialog::OnOK() 
{
	for ( int nColumnIndex = 0; nColumnIndex < POINTS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + POINTS_COLUMN_START] = m_PointsList.GetColumnWidth( nColumnIndex );
	}
	CResizeDialog::OnOK();
}

void CPEPointsListDialog::OnCancel() 
{
	for ( int nColumnIndex = 0; nColumnIndex < POINTS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + POINTS_COLUMN_START] = m_PointsList.GetColumnWidth( nColumnIndex );
	}
	CResizeDialog::OnCancel();
}

void CPEPointsListDialog::OnAddPointButton() 
{
	CVec3 vNewPoint = VNULL3;
	CPEPointPropertiesDialog pointPropertiesDialog;
	pointPropertiesDialog.m_strXCoord.Format( "%.2f", vNewPoint.x / 64.0f );
	pointPropertiesDialog.m_strYCoord.Format( "%.2f", vNewPoint.y / 64.0f );
	if ( pointPropertiesDialog.DoModal() == IDOK )
	{
		vNewPoint /= 64.0f;
		sscanf( pointPropertiesDialog.m_strXCoord, "%g", &( vNewPoint.x ) );
		sscanf( pointPropertiesDialog.m_strYCoord, "%g", &( vNewPoint.y ) );
		vNewPoint *= 64.0f;
		points.push_back( vNewPoint );
	}
	LoadPointsList();
}

void CPEPointsListDialog::OnDeletePointButton() 
{
	int nSelectedItem = m_PointsList.GetNextItem( -1, LVIS_SELECTED );
	if ( nSelectedItem != ( -1 ) )
	{
		bCreateControls = true;
		std::list<int> indicesToDelete;
		while ( nSelectedItem != ( -1 ) )
		{
			std::string szKey = m_PointsList.GetItemText( nSelectedItem, 0 );
			int nPointIndex = -1;
			sscanf( szKey.c_str(), "%d", &nPointIndex );
			if ( ( nPointIndex >= 0 ) && ( nPointIndex < points.size() ) )
			{
				indicesToDelete.push_back( nPointIndex );
			}
			m_PointsList.DeleteItem( nSelectedItem );
			nSelectedItem = m_PointsList.GetNextItem( -1, LVIS_SELECTED );
		}
		std::vector<CVec3> newPoints;
		for ( int nPointIndex = 0; nPointIndex < points.size(); ++nPointIndex )
		{
			bool bExist = true;
			for( std::list<int>::const_iterator indexIterator = indicesToDelete.begin(); indexIterator != indicesToDelete.end(); ++indexIterator )
			{
				if ( ( *indexIterator ) == nPointIndex )
				{
					bExist = false;
					break;
				}
			}
			if ( bExist )
			{
				newPoints.push_back( points[nPointIndex] );
			}
		}
		points = newPoints;
		bCreateControls = false;
		LoadPointsList();
	}
}

void CPEPointsListDialog::OnPointPropertiesButton() 
{
	int nFocusedtem = m_PointsList.GetNextItem( -1, LVIS_FOCUSED );
	if ( nFocusedtem != ( -1 ) )
	{
		std::string szKey = m_PointsList.GetItemText( nFocusedtem, 0 );
		int nPointIndex = -1;
		sscanf( szKey.c_str(), "%d", &nPointIndex );
		if ( ( nPointIndex >= 0 ) && ( nPointIndex < points.size() ) )
		{
			CPEPointPropertiesDialog pointPropertiesDialog;
			pointPropertiesDialog.m_strXCoord.Format( "%.2f", points[nPointIndex].x / 64.0f );
			pointPropertiesDialog.m_strYCoord.Format( "%.2f", points[nPointIndex].y / 64.0f );
			if ( pointPropertiesDialog.DoModal() == IDOK )
			{
				points[nPointIndex] /= 64.0f;
				sscanf( pointPropertiesDialog.m_strXCoord, "%g", &( points[nPointIndex].x ) );
				sscanf( pointPropertiesDialog.m_strYCoord, "%g", &( points[nPointIndex].y ) );
				points[nPointIndex] *= 64.0f;
				SetPointItem( nFocusedtem, nPointIndex );
				UpdateControls();
			}
		}
	}
}

bool CPEPointsListDialog::LoadPointsList()
{
	BeginWaitCursor();
	m_PointsList.DeleteAllItems();
	for ( int nPointIndex = 0; nPointIndex < points.size(); ++nPointIndex )
	{
		int nNewItem = m_PointsList.InsertItem( LVIF_TEXT, 0, NStr::Format("%4d", nPointIndex ), 0, 0, 0, 0 );
		if ( nNewItem == ( -1 ) )
		{
			EndWaitCursor();
			return false;
		}
		SetPointItem( nNewItem, nPointIndex );
	}
	UpdateControls();
	EndWaitCursor();
	return true;
}

void CPEPointsListDialog::SetPointItem( int nItem, int nPointIndex )
{
	m_PointsList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%.2f", points[nPointIndex].x / 64.0f ), 0, 0, 0, 0 );
	m_PointsList.SetItem( nItem, 2, LVIF_TEXT, NStr::Format( "%.2f", points[nPointIndex].y / 64.0f ), 0, 0, 0, 0 );
}

void CPEPointsListDialog::CreateControls()
{
	bCreateControls = true;
	m_PointsList.SetExtendedStyle( m_PointsList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < POINTS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + POINTS_COLUMN_START] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + POINTS_COLUMN_START] = POINTS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_PointsList.InsertColumn( nColumnIndex, POINTS_COLUMN_NAME[nColumnIndex], POINTS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + POINTS_COLUMN_START], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bPointsSortParam.push_back( true );
	}
	bCreateControls = false;
}

void CPEPointsListDialog::UpdateControls()
{
	CWnd* pWnd = 0;
	if ( pWnd = GetDlgItem( vID[2] ) )
	{
		pWnd->EnableWindow( m_PointsList.GetSelectedCount() > 0 );
	}
	if ( pWnd = GetDlgItem( vID[3] ) )
	{
		pWnd->EnableWindow( m_PointsList.GetSelectedCount() > 0 );
	}
	if ( pWnd = GetDlgItem( vID[4] ) )
	{
		pWnd->EnableWindow( true );
	}
}

void CPEPointsListDialog::OnAddMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( vID[1] ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnAddPointButton();
		}
	}
}

void CPEPointsListDialog::OnDeleteMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( vID[2] ) )
	{
		if( pWnd->IsWindowEnabled() )
		{
			OnDeletePointButton();
		}
	}
}

void CPEPointsListDialog::OnPropertiesMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( vID[3] ) )
	{
		if( pWnd->IsWindowEnabled() )
		{
			OnPointPropertiesButton();
		}
	}
}

void CPEPointsListDialog::OnDblclkPointsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if ( CWnd* pWnd = GetDlgItem( vID[3] ) )
	{
		if( pWnd->IsWindowEnabled() )
		{
			OnPointPropertiesButton();
		}
	}
	*pResult = 0;
}

void CPEPointsListDialog::OnRclickPointsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu peMenu;
	peMenu.LoadMenu( IDM_PE_POPUP_MENUS );
	CMenu *pMenu = peMenu.GetSubMenu( 0 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[1] ) )
		{
			pMenu->EnableMenuItem( IDC_PE_PL_ADD_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( vID[2] ) )
		{
			pMenu->EnableMenuItem( IDC_PE_PL_DELETE_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( vID[3] ) )
		{
			pMenu->EnableMenuItem( IDC_PE_PL_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	peMenu.DestroyMenu();
	*pResult = 0;
}

void CPEPointsListDialog::OnColumnclickPointsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < POINTS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, POINTS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_PointsList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_PointsList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_PointsList.SortItems( PointsListCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bPointsSortParam[nSortColumn] = !bPointsSortParam[nSortColumn];
	*pResult = 0;
}

void CPEPointsListDialog::OnItemchangedPointsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	UpdateControls();
	*pResult = 0;
}

void CPEPointsListDialog::OnKeydownPointsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[1] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddPointButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[2] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeletePointButton();
			}
		}
	}
	else if (  pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[3] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnPointPropertiesButton();
			}
		}
	}
	*pResult = 0;
}
