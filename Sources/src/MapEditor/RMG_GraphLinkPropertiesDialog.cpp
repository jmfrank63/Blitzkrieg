#include "StdAfx.h"

#include "RMG_GraphLinkPropertiesDialog.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int   GRAPH_LINKS_COLUMN_COUNT = 4;
const char *GRAPH_LINKS_COLUMN_NAME  [GRAPH_LINKS_COLUMN_COUNT] = { "Path", "N%", "Start Node", "End Node" };
const int   GRAPH_LINKS_COLUMN_FORMAT[GRAPH_LINKS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT };
int					GRAPH_LINKS_COLUMN_WIDTH [GRAPH_LINKS_COLUMN_COUNT] = { 200, 20, 110, 110 };

int CALLBACK GraphLinksCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGGraphLinkPropertiesDialog* pGraphLinkPropertiesDialog = reinterpret_cast<CRMGGraphLinkPropertiesDialog*>( lParamSort );

	CString strItem1 = pGraphLinkPropertiesDialog->m_GraphLinksList.GetItemText( lParam1, pGraphLinkPropertiesDialog->nSortColumn );
	CString strItem2 = pGraphLinkPropertiesDialog->m_GraphLinksList.GetItemText( lParam2, pGraphLinkPropertiesDialog->nSortColumn );
	if ( pGraphLinkPropertiesDialog->bGraphLinkSortParam[pGraphLinkPropertiesDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

const int CRMGGraphLinkPropertiesDialog::vID[] = 
{
	IDC_RMG_GLP_LINKS_LABEL,									//0
	IDC_RMG_GLP_LINKS_LIST,										//1
	IDC_RMG_GLP_DELETE_LINK_BUTTON,						//2
	IDC_RMG_GLP_LINK_TYPE_LABEL,							//3
	IDC_RMG_GLP_LINK_TYPE_00_RADIO_BUTTON,		//4
	IDC_RMG_GLP_LINK_TYPE_01_RADIO_BUTTON,		//5
	IDC_RMG_GLP_LINK_DESK_LABEL,							//6
	IDC_RMG_GLP_LINK_DESC_EDIT,								//7
	IDC_RMG_GLP_LINK_DESC_BROWSE_BUTTON,			//8
	IDC_RMG_GLP_LINK_CORNER_LABEL,						//9
	IDC_RMG_GLP_LINK_CORNER_RADIUS_LABEL,			//10
	IDC_RMG_GLP_LINK_CORNER_RADIUS_EDIT,			//11
	IDC_RMG_GLP_LINK_CORNER_PARTS_LABEL,			//12
	IDC_RMG_GLP_LINK_CORNER_PARTS_EDIT,				//13
	IDC_RMG_GLP_LINK_LINE_LABEL,							//14
	IDC_RMG_GLP_LINK_LINE_LENGTH_LABEL,				//15
	IDC_RMG_GLP_LINK_LINE_LENGTH_EDIT,				//16
	IDC_RMG_GLP_LINK_LINE_WIDTH_LABEL,				//17
	IDC_RMG_GLP_LINK_LINE_WIDTH_EDIT,					//18
	IDC_RMG_GLP_LINK_LINE_DISTURBANCE_LABEL,	//19
	IDC_RMG_GLP_LINK_LINE_DISTURBANCE_EDIT,		//20
	IDOK,																			//21
	IDCANCEL,																	//22
};

CRMGGraphLinkPropertiesDialog::CRMGGraphLinkPropertiesDialog( CWnd* pParent )
	: CResizeDialog( CRMGGraphLinkPropertiesDialog::IDD, pParent ), bCreateControls( false ), nSortColumn( 0 )
{

	SetControlStyle( vID[0], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[1], ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	SetControlStyle( vID[2], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[3], ANCHORE_LEFT_BOTTOM );
	SetControlStyle( vID[4], ANCHORE_LEFT_BOTTOM );
	SetControlStyle( vID[5], ANCHORE_LEFT_BOTTOM );
	SetControlStyle( vID[6], ANCHORE_LEFT_BOTTOM | RESIZE_HOR );
	SetControlStyle( vID[7], ANCHORE_LEFT_BOTTOM | RESIZE_HOR );
	SetControlStyle( vID[8], ANCHORE_RIGHT_BOTTOM );
	
	SetControlStyle( vID[9], ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( vID[10], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( vID[11], ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( vID[12], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( vID[13], ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	
	SetControlStyle( vID[14], ANCHORE_LEFT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( vID[15], ANCHORE_LEFT_BOTTOM );
	SetControlStyle( vID[16], ANCHORE_LEFT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( vID[17], ANCHORE_LEFT_BOTTOM );
	SetControlStyle( vID[18], ANCHORE_LEFT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( vID[19], ANCHORE_LEFT_BOTTOM );
	SetControlStyle( vID[20], ANCHORE_LEFT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	
	SetControlStyle( vID[21], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[22], ANCHORE_RIGHT_BOTTOM );
}

void CRMGGraphLinkPropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_RMG_GLP_LINK_LINE_DISTURBANCE_EDIT, m_DisturbanceEdit);
	DDX_Control(pDX, IDC_RMG_GLP_LINK_LINE_WIDTH_EDIT, m_WidthEdit);
	DDX_Control(pDX, IDC_RMG_GLP_LINK_LINE_LENGTH_EDIT, m_LengthEdit);
	DDX_Control(pDX, IDC_RMG_GLP_LINK_CORNER_RADIUS_EDIT, m_RadiusEdit);
	DDX_Control(pDX, IDC_RMG_GLP_LINK_CORNER_PARTS_EDIT, m_PartsEdit);
	DDX_Control(pDX, IDC_RMG_GLP_LINK_DESC_EDIT, m_DescEdit);
	DDX_Control(pDX, IDC_RMG_GLP_LINKS_LIST, m_GraphLinksList);
}

BEGIN_MESSAGE_MAP(CRMGGraphLinkPropertiesDialog, CResizeDialog)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_RMG_GLP_LINKS_LIST, OnColumnclickLinksList)
	ON_COMMAND(IDC_RMG_GLP_DELETE_MENU, OnDeleteMenu)
	ON_BN_CLICKED(IDC_RMG_GLP_DELETE_LINK_BUTTON, OnDeleteLinkButton)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RMG_GLP_LINKS_LIST, OnKeydownLinksList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RMG_GLP_LINKS_LIST, OnItemchangedLinksList)
	ON_EN_CHANGE(IDC_RMG_GLP_LINK_CORNER_PARTS_EDIT, OnChangeCornerPartsEdit)
	ON_EN_CHANGE(IDC_RMG_GLP_LINK_CORNER_RADIUS_EDIT, OnChangeCornerRadiusEdit)
	ON_EN_CHANGE(IDC_RMG_GLP_LINK_DESC_EDIT, OnChangeDescEdit)
	ON_EN_CHANGE(IDC_RMG_GLP_LINK_LINE_DISTURBANCE_EDIT, OnChangeLineDisturbanceEdit)
	ON_EN_CHANGE(IDC_RMG_GLP_LINK_LINE_LENGTH_EDIT, OnChangeLengthEdit)
	ON_EN_CHANGE(IDC_RMG_GLP_LINK_LINE_WIDTH_EDIT, OnChangeWidthEdit)
	ON_BN_CLICKED(IDC_RMG_GLP_LINK_TYPE_00_RADIO_BUTTON, OnType00RadioButton)
	ON_BN_CLICKED(IDC_RMG_GLP_LINK_TYPE_01_RADIO_BUTTON, OnType01RadioButton)
	ON_BN_CLICKED(IDC_RMG_GLP_LINK_DESC_BROWSE_BUTTON, OnDescBrowseButton)
	ON_NOTIFY(NM_RCLICK, IDC_RMG_GLP_LINKS_LIST, OnRclickLinksList)
END_MESSAGE_MAP()


BOOL CRMGGraphLinkPropertiesDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();

	if ( resizeDialogOptions.szParameters.size() < 1 )
	{
		resizeDialogOptions.szParameters.resize( 1, "" );
	}
	if ( resizeDialogOptions.nParameters.size() < GRAPH_LINKS_COLUMN_COUNT )
	{
		resizeDialogOptions.nParameters.resize( GRAPH_LINKS_COLUMN_COUNT, 0 );
	}

	CreateControls();
	LoadGraphLinks();
	UpdateControls();
	return TRUE;
}

void CRMGGraphLinkPropertiesDialog::OnOK() 
{
	for ( int nColumnIndex = 0; nColumnIndex < GRAPH_LINKS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex] = m_GraphLinksList.GetColumnWidth( nColumnIndex );
	}
	CResizeDialog::OnOK();
}

void CRMGGraphLinkPropertiesDialog::OnCancel() 
{
	for ( int nColumnIndex = 0; nColumnIndex < GRAPH_LINKS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex] = m_GraphLinksList.GetColumnWidth( nColumnIndex );
	}
	CResizeDialog::OnCancel();
}

void CRMGGraphLinkPropertiesDialog::OnColumnclickLinksList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < GRAPH_LINKS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, GRAPH_LINKS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_GraphLinksList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_GraphLinksList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_GraphLinksList.SortItems( GraphLinksCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bGraphLinkSortParam[nSortColumn] = !bGraphLinkSortParam[nSortColumn];
	*pResult = 0;
}

void CRMGGraphLinkPropertiesDialog::OnItemchangedLinksList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if ( !bCreateControls )
	{
		LoadGraphLinkToControls();
	}
	UpdateControls();
	*pResult = 0;
}

void CRMGGraphLinkPropertiesDialog::OnDeleteMenu() 
{
	OnDeleteLinkButton();
}

void CRMGGraphLinkPropertiesDialog::OnDeleteLinkButton() 
{
	CString strTitle;
	strTitle.LoadString( IDR_EDITORTYPE );
	if ( MessageBox( "Do you really want to DELETE selected Graph Links?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
	{
		std::vector<int> indicesToDelete;
		int nSelectedItem = m_GraphLinksList.GetNextItem( -1, LVIS_SELECTED );
		while ( nSelectedItem != ( -1 ) )
		{
			bCreateControls = true;
			CString szBuffer = m_GraphLinksList.GetItemText( nSelectedItem, 1 );
			int nSelectedLink = -1;
			sscanf( szBuffer, "%d", &nSelectedLink );
			if ( ( nSelectedLink >= 0 ) && ( nSelectedLink < graph.links.size() ) )
			{
				indicesToDelete.push_back( nSelectedLink );
			}
			nSelectedItem = m_GraphLinksList.GetNextItem( nSelectedItem, LVIS_SELECTED );
		}
		
		for ( int nIndex = 0; nIndex < indicesToDelete.size(); ++nIndex )
		{
			graph.links.erase( graph.links.begin() + indicesToDelete[nIndex] );
			
			for ( int nLinkIndex = 0; nLinkIndex < linkIndices.size(); )
			{
				if ( linkIndices[nLinkIndex] == indicesToDelete[nIndex] )
				{
					linkIndices.erase( linkIndices.begin() + nLinkIndex );
				}
				else
				{
					++nLinkIndex;
					if ( linkIndices[nLinkIndex] > indicesToDelete[nIndex] )
					{
						linkIndices[nLinkIndex] -= 1;
					}
				}
			}
			for ( int nInnerIndex = ( nIndex + 1 ); nInnerIndex < indicesToDelete.size(); ++nInnerIndex )
			{
				if ( indicesToDelete[nInnerIndex] > indicesToDelete[nIndex] )
				{
					indicesToDelete[nInnerIndex] -= 1;
				}
			}
		}
		LoadGraphLinks();
	}
}

void CRMGGraphLinkPropertiesDialog::OnKeydownLinksList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDoww = (LV_KEYDOWN*)pNMHDR;
	if ( pLVKeyDoww->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[2] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeleteLinkButton();
			}
		}
	}
	*pResult = 0;
}

void CRMGGraphLinkPropertiesDialog::OnRclickLinksList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 4 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[2] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_GLP_DELETE_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGGraphLinkPropertiesDialog::OnChangeCornerPartsEdit() 
{
	SaveGraphLinkFromControls();
}

void CRMGGraphLinkPropertiesDialog::OnChangeCornerRadiusEdit() 
{
	SaveGraphLinkFromControls();
}

void CRMGGraphLinkPropertiesDialog::OnChangeDescEdit() 
{
	SaveGraphLinkFromControls();
}

void CRMGGraphLinkPropertiesDialog::OnChangeLineDisturbanceEdit() 
{
	SaveGraphLinkFromControls();
}

void CRMGGraphLinkPropertiesDialog::OnChangeLengthEdit() 
{
	SaveGraphLinkFromControls();
	int nFocusedItem = m_GraphLinksList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		CString szBuffer;
		m_DescEdit.GetWindowText( szBuffer );
		m_GraphLinksList.SetItemText( nFocusedItem, 0, szBuffer );
	}
}

void CRMGGraphLinkPropertiesDialog::OnChangeWidthEdit() 
{
	SaveGraphLinkFromControls();
}

void CRMGGraphLinkPropertiesDialog::OnType00RadioButton() 
{
	SaveGraphLinkFromControls();
}

void CRMGGraphLinkPropertiesDialog::OnType01RadioButton() 
{
	SaveGraphLinkFromControls();
}

void CRMGGraphLinkPropertiesDialog::OnDescBrowseButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	int nFocusedItem = m_GraphLinksList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		CFileDialog fileDialog( true, ".xml", "", OFN_FILEMUSTEXIST | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, "XML files (*.xml)|*.xml||" );
		fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[0].c_str();

		if ( fileDialog.DoModal() == IDOK )
		{
			BeginWaitCursor();
			POSITION position = fileDialog.GetStartPosition();
			while ( position )
			{
				std::string szFilePath = fileDialog.GetNextPathName( position );
				std::string szStorageName = pDataStorage->GetName();
				NStr::ToLower( szFilePath );
				NStr::ToLower( szStorageName );
				if ( szFilePath.find( szStorageName ) != 0 )
				{
					return;
				}

				int nPointIndex = szFilePath.rfind( "." );
				if ( nPointIndex >= 0 )
				{
					szFilePath = szFilePath.substr( 0, nPointIndex );
				}

				int nSlashIndex = szFilePath.rfind( "\\" );
				if ( nSlashIndex >= 0 )
				{
					resizeDialogOptions.szParameters[0] = szFilePath.substr( 0, nSlashIndex );
				}
				
				szFilePath = szFilePath.substr( szStorageName.size() );
				
				m_DescEdit.SetWindowText( szFilePath.c_str() );
			}
		}
	}
}

bool CRMGGraphLinkPropertiesDialog::LoadGraphLinks()
{
	bCreateControls = true;
	m_GraphLinksList.DeleteAllItems();
	for ( int nSelectedLink = 0 ;nSelectedLink < linkIndices.size(); ++nSelectedLink )
	{
		int nNewItem = -1;
		if ( nSelectedLink == 0 )
		{
			nNewItem = m_GraphLinksList.InsertItem( LVIF_TEXT | LVIF_STATE, 0, graph.links[linkIndices[nSelectedLink]].szDescFileName.c_str(), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED, 0, 0 );
		}
		else
		{
			nNewItem = m_GraphLinksList.InsertItem( LVIF_TEXT, 0, graph.links[linkIndices[nSelectedLink]].szDescFileName.c_str(), 0, 0, 0, 0 );
		}
		if ( nNewItem != ( -1 ) )
		{
			SetGraphLinkItem( nNewItem, linkIndices[nSelectedLink] );
		}
	}
	bCreateControls = false;
	LoadGraphLinkToControls();
	return true;
}

void  CRMGGraphLinkPropertiesDialog::SetGraphLinkItem( int nItem, int nSelectedLink )
{
	m_GraphLinksList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%d", nSelectedLink ), 0, 0, 0, 0 );
	m_GraphLinksList.SetItem( nItem, 2, LVIF_TEXT, NStr::Format( "%d, (%d, %d, %d, %d), [%dx%d]",
																															 graph.links[nSelectedLink].link.a,
																															 graph.nodes[graph.links[nSelectedLink].link.a].rect.minx,
																															 graph.nodes[graph.links[nSelectedLink].link.a].rect.miny,
																															 graph.nodes[graph.links[nSelectedLink].link.a].rect.maxx,
																															 graph.nodes[graph.links[nSelectedLink].link.a].rect.maxy,
																															 graph.nodes[graph.links[nSelectedLink].link.a].rect.GetSizeX(),
																															 graph.nodes[graph.links[nSelectedLink].link.a].rect.GetSizeY() ),
														0, 0, 0, 0 );
	m_GraphLinksList.SetItem( nItem, 3, LVIF_TEXT, NStr::Format( "%d, (%d, %d, %d, %d), [%dx%d]",
																															 graph.links[nSelectedLink].link.b,
																															 graph.nodes[graph.links[nSelectedLink].link.b].rect.minx,
																															 graph.nodes[graph.links[nSelectedLink].link.b].rect.miny,
																															 graph.nodes[graph.links[nSelectedLink].link.b].rect.maxx,
																															 graph.nodes[graph.links[nSelectedLink].link.b].rect.maxy,
																															 graph.nodes[graph.links[nSelectedLink].link.b].rect.GetSizeX(),
																															 graph.nodes[graph.links[nSelectedLink].link.b].rect.GetSizeY() ),
														0, 0, 0, 0 );
}

float GetRoundFloat( float fValue )
{
	int nValue = (int)( fValue + 0.5f );
	if ( fabs( fValue - (float)( nValue ) ) < 0.001f )
	{
		fValue = (float)( nValue );
	}
	return fValue;
}

bool CRMGGraphLinkPropertiesDialog::LoadGraphLinkToControls()
{
	int nFocusedItem = m_GraphLinksList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		bCreateControls = true;
		CString szBuffer = m_GraphLinksList.GetItemText( nFocusedItem, 1 );
		int nSelectedLink = -1;
		sscanf( szBuffer, "%d", &nSelectedLink );
		if ( ( nSelectedLink >= 0 ) && ( nSelectedLink < graph.links.size() ) )
		{
			CheckRadioButton( vID[4], vID[5], vID[4] + graph.links[nSelectedLink].nType );
			m_DescEdit.SetWindowText( graph.links[nSelectedLink].szDescFileName.c_str() );
			m_RadiusEdit.SetWindowText( NStr::Format( "%.2f", GetRoundFloat( graph.links[nSelectedLink].fRadius / fWorldCellSize ) ) );
			m_PartsEdit.SetWindowText( NStr::Format( "%d", graph.links[nSelectedLink].nParts ) );
			m_LengthEdit.SetWindowText( NStr::Format( "%.2f", GetRoundFloat( graph.links[nSelectedLink].fMinLength / fWorldCellSize ) ) );
			m_WidthEdit.SetWindowText( NStr::Format( "%.2f", graph.links[nSelectedLink].fDistance ) );
			m_DisturbanceEdit.SetWindowText( NStr::Format( "%.2f", graph.links[nSelectedLink].fDisturbance ) );
		}
		bCreateControls = false;
	}
	else
	{
		m_DescEdit.SetWindowText( "" );
		m_RadiusEdit.SetWindowText( "" );
		m_PartsEdit.SetWindowText( "" );
		m_LengthEdit.SetWindowText( "" );
		m_WidthEdit.SetWindowText( "" );
		m_DisturbanceEdit.SetWindowText( "" );
	}
	return false;
}

bool CRMGGraphLinkPropertiesDialog::SaveGraphLinkFromControls()
{
	int nFocusedItem = m_GraphLinksList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		CString szBuffer = m_GraphLinksList.GetItemText( nFocusedItem, 1 );
		int nSelectedLink = -1;
		sscanf( szBuffer, "%d", &nSelectedLink );
		if ( ( nSelectedLink >= 0 ) && ( nSelectedLink < graph.links.size() ) )
		{
			if ( ( GetCheckedRadioButton( vID[4], vID[5] ) ) == vID[4] )
			{
				graph.links[nSelectedLink].nType = SRMGraphLink::TYPE_ROAD;
			}
			else
			{
				graph.links[nSelectedLink].nType = SRMGraphLink::TYPE_RIVER;
			}
			
			m_DescEdit.GetWindowText( szBuffer );
			graph.links[nSelectedLink].szDescFileName = szBuffer;
			
			float fBuffer = 0.0f;
			int nBuffer = -1;
			m_RadiusEdit.GetWindowText( szBuffer );
			if ( sscanf( szBuffer, "%g", &fBuffer ) == 1 )
			{
				graph.links[nSelectedLink].fRadius = fBuffer * fWorldCellSize;
			}
			m_PartsEdit.GetWindowText( szBuffer );
			if ( sscanf( szBuffer, "%d", &nBuffer ) == 1 )
			{
				graph.links[nSelectedLink].nParts = nBuffer;
			}
			m_LengthEdit.GetWindowText( szBuffer );
			if ( sscanf( szBuffer, "%g", &fBuffer ) == 1 )
			{
				graph.links[nSelectedLink].fMinLength = fBuffer * fWorldCellSize;
			}
			m_WidthEdit.GetWindowText( szBuffer );
			if ( sscanf( szBuffer, "%g", &fBuffer ) == 1 )
			{
				graph.links[nSelectedLink].fDistance = fBuffer;
			}
			m_DisturbanceEdit.GetWindowText( szBuffer );
			if ( sscanf( szBuffer, "%g", &fBuffer ) == 1 )
			{
				graph.links[nSelectedLink].fDisturbance = fBuffer;
			}
			return true;
		}
	}
	return false;
}

void CRMGGraphLinkPropertiesDialog::CreateControls()
{
	bCreateControls = true;
	m_GraphLinksList.SetExtendedStyle( m_GraphLinksList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < GRAPH_LINKS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex] = GRAPH_LINKS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_GraphLinksList.InsertColumn( nColumnIndex, GRAPH_LINKS_COLUMN_NAME[nColumnIndex], GRAPH_LINKS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bGraphLinkSortParam.push_back( true );
	}
	bCreateControls = false;
}

void CRMGGraphLinkPropertiesDialog::UpdateControls()
{
	if ( CWnd* pWnd = GetDlgItem( vID[2] ) )
	{
		pWnd->EnableWindow( m_GraphLinksList.GetSelectedCount() > 0 );
	}
	if ( CWnd* pWnd = GetDlgItem( vID[8] ) )
	{
		pWnd->EnableWindow( m_GraphLinksList.GetSelectedCount() > 0 );
	}
}
