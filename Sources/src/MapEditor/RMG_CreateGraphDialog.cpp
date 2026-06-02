#include "stdafx.h"

#include "RMG_CreateGraphDialog.h"
#include "..\RandomMapGen\LA_Types.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\RandomMapGen\Polygons_Types.h"
#include "..\RandomMapGen\Resource_Types.h"

#include "RMG_GraphNodePropertiesDialog.h"
#include "RMG_GraphLinkPropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const char *CG_GRAPHS_XML_NAME = "Graphs";
const char *CG_GRAPHS_FILE_NAME = "Editor\\DefaultGraphs";
const char *CG_GRAPHS_DIALOG_TITLE = "Graphs Composer";

const int CG_GRAPH_NODE_PLACE_FRAME_OFFSET_MIN = 2;
const int CG_GRAPH_NODE_PLACE_FRAME_OFFSET_MAX = 3;
const int CG_GRAPH_LINK_HALF_WIDTH = 2;

const int   CG_GRAPHS_COLUMN_COUNT = 11;
const char *CG_GRAPHS_COLUMN_NAME  [CG_GRAPHS_COLUMN_COUNT] = { "Path", "Max Size", "Nodes", "Links", "Empty Nodes", "Empty Links", "Season", "Season Folder", "Supported Settings", "Used ScriptIDs", "Used ScriptAreas" };
const int   CG_GRAPHS_COLUMN_FORMAT[CG_GRAPHS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
int					CG_GRAPHS_COLUMN_WIDTH [CG_GRAPHS_COLUMN_COUNT] = { 200, 60, 60, 80, 80, 80, 80, 80, 80, 80, 80 };

int CALLBACK CG_GraphsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGCreateGraphDialog* pGraphDialog = reinterpret_cast<CRMGCreateGraphDialog*>( lParamSort );

	CString strItem1 = pGraphDialog->m_GraphsList.GetItemText( lParam1, pGraphDialog->nSortColumn );
	CString strItem2 = pGraphDialog->m_GraphsList.GetItemText( lParam2, pGraphDialog->nSortColumn );
	if ( pGraphDialog->bGraphSortParam[pGraphDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

const DWORD CRMGCreateGraphDialog::SGraphCheckInfo::SIDE_MINX		= 0x01;
const DWORD CRMGCreateGraphDialog::SGraphCheckInfo::SIDE_MINY		= 0x02;
const DWORD CRMGCreateGraphDialog::SGraphCheckInfo::SIDE_MAXX		= 0x04;
const DWORD CRMGCreateGraphDialog::SGraphCheckInfo::SIDE_MAXY		= 0x08;
const DWORD CRMGCreateGraphDialog::SGraphCheckInfo::SIDE_CENTER	= 0x10;

const int CRMGCreateGraphDialog::vID[] = 
{
	IDC_RMG_CG_GRAPHS_LABEL,							//0
	IDC_RMG_CG_GRAPHS_LIST,								//1
	IDC_RMG_CG_NODES_LABEL_TOP,						//2
	IDC_RMG_CG_NODES_PLACE,								//3
	IDC_RMG_CG_ADD_GRAPH_BUTTON,					//4
	IDC_RMG_CG_DELETE_GRAPH_BUTTON,				//5
	IDC_RMG_CG_GRAPH_PROPERTIES_BUTTON,		//6
	IDC_RMG_CG_NODES_SLIDER,							//7
	IDC_RMG_CG_NODES_SLIDER_LABEL_TOP,		//8
	IDC_RMG_CG_NODES_SLIDER_LABEL_BOTTOM,	//9
	IDC_RMG_CG_SAVE_BUTTON,								//10
	IDOK,																	//11
	IDCANCEL,															//12
	IDC_RMG_CG_NODES_LABEL_BOTTOM,				//13
	IDC_RMG_CG_DELIMITER_00,							//14
	IDC_RMG_CG_CHECK_GRAPHS_BUTTON,				//15
};

CRMGCreateGraphDialog::CRMGCreateGraphDialog( CWnd* pParent )
	: CResizeDialog( CRMGCreateGraphDialog::IDD, pParent ),
		isChanged( false ), bCreateControls( false ), inputState( STATE_NONE ), nPatchesCount( 8 ), mousePoints( 0, 0, 0, 0 )
{
	SetControlStyle( vID[0], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[1], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[2], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[3], ANCHORE_LEFT_BOTTOM | RESIZE_HOR_VER );
	SetControlStyle( vID[4], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[5], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[6], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[7], ANCHORE_RIGHT_BOTTOM | RESIZE_VER );
	SetControlStyle( vID[8], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[9], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[10], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[11], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[12], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[13], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[14], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[15], ANCHORE_RIGHT_TOP );
}

void CRMGCreateGraphDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RMG_CG_NODES_LABEL_TOP, m_NodesMessageTop);
	DDX_Control(pDX, IDC_RMG_CG_NODES_LABEL_BOTTOM, m_NodesMessageBottom);
	DDX_Control(pDX, IDC_RMG_CG_GRAPHS_LIST, m_GraphsList);
	DDX_Control(pDX, IDC_RMG_CG_NODES_SLIDER, m_NodesSlider);
}

BEGIN_MESSAGE_MAP(CRMGCreateGraphDialog, CResizeDialog)
	ON_BN_CLICKED(IDC_RMG_CG_ADD_GRAPH_BUTTON, OnAddGraphButton)
	ON_BN_CLICKED(IDC_RMG_CG_DELETE_GRAPH_BUTTON, OnDeleteGraphButton)
	ON_BN_CLICKED(IDC_RMG_CG_GRAPH_PROPERTIES_BUTTON, OnGraphPropertiesButton)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RMG_CG_GRAPHS_LIST, OnItemchangedGraphsList)
	ON_NOTIFY(NM_DBLCLK, IDC_RMG_CG_GRAPHS_LIST, OnDblclkGraphsList)
	ON_NOTIFY(NM_RCLICK, IDC_RMG_CG_GRAPHS_LIST, OnRclickGraphsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RMG_CG_GRAPHS_LIST, OnKeydownGraphsList)
	ON_COMMAND(IDC_RMG_CG_ADD_GRAPH_MENU, OnAddGraphMenu)
	ON_COMMAND(IDC_RMG_CG_DELETE_GRAPH_MENU, OnDeleteGraphMenu)
	ON_WM_HSCROLL()
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_RMG_CG_GRAPHS_LIST, OnColumnclickGraphsList)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONUP()
	ON_WM_PAINT()
	ON_WM_VSCROLL()
	ON_WM_KEYDOWN()
	ON_WM_ACTIVATE()
	ON_BN_CLICKED(IDC_RMG_CG_SAVE_BUTTON, OnSaveButton)
	ON_COMMAND(IDC_RMG_CG_PROPERTIES_MENU, OnPropertiesMenu)
	ON_COMMAND(IDC_RMG_CG_DELETE_MENU, OnDeleteMenu)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_FILE_SAVEAS, OnFileSaveas)
	ON_COMMAND(ID_FILE_EXIT, OnFileExit)
	ON_BN_CLICKED(IDC_RMG_CG_CHECK_GRAPHS_BUTTON, OnCheckGraphsButton)
END_MESSAGE_MAP()

BOOL CRMGCreateGraphDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();

	if ( resizeDialogOptions.szParameters.size() < 3 )
	{
		resizeDialogOptions.szParameters.resize( 3, "" );
	}
	if ( resizeDialogOptions.szParameters[2].empty() )
	{
		resizeDialogOptions.szParameters[2] = CG_GRAPHS_FILE_NAME;
	}
	if ( resizeDialogOptions.nParameters.size() < CG_GRAPHS_COLUMN_COUNT )
	{
		resizeDialogOptions.nParameters.resize( CG_GRAPHS_COLUMN_COUNT, 0 );
	}
	defaultCursor = GetCursor();
	CreateControls();
	LoadGraphsList();
	UpdateControls();
	return true;
}	

void CRMGCreateGraphDialog::OnOK() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CG_GRAPHS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex] = m_GraphsList.GetColumnWidth( nColumnIndex );
	}
	
	SaveGraphsList();
	CResizeDialog::OnOK();
}

void CRMGCreateGraphDialog::OnCancel() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CG_GRAPHS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex] = m_GraphsList.GetColumnWidth( nColumnIndex );
	}

	CResizeDialog::OnCancel();
}

void CRMGCreateGraphDialog::OnAddGraphButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	CFileDialog fileDialog( true, ".xml", "", OFN_ALLOWMULTISELECT, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
	fileDialog.m_ofn.lpstrFile[0] = 0;			
	fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
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
			
			SRMGraph graph;
			graph.size.x = 0;
			graph.size.y = 0;
			graph.nSeason = 0;
			graph.szSeasonFolder.clear();
			LoadDataResource( szFilePath, "", false, 0, RMGC_GRAPH_XML_NAME, graph );
			
			LVFINDINFO findInfo;
			findInfo.flags = LVFI_STRING;
			findInfo.psz = szFilePath.c_str();

			int nOldItem = m_GraphsList.FindItem( &findInfo, -1 );
			if ( nOldItem != ( -1 ) )
			{
				m_GraphsList.DeleteItem( nOldItem );
			}
			
			int nNewItem = m_GraphsList.InsertItem( LVIF_TEXT, 0, szFilePath.c_str(), 0, 0, 0, 0 );
			if ( nNewItem != ( -1 ) )
			{
				graphs[szFilePath] = graph;
				SetGraphItem( nNewItem, graph );
			}
		}		
		LoadGraphToControls();
		EndWaitCursor();
	}
	delete[] fileDialog.m_ofn.lpstrFile;
}

void CRMGCreateGraphDialog::OnDeleteGraphButton() 
{
	int nSelectedItem = m_GraphsList.GetNextItem( -1, LVIS_SELECTED );
	if ( nSelectedItem != ( -1 ) )
	{
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );
		if ( MessageBox( "Do you really want to DELETE selected Graphs?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
		{
			bCreateControls = true;
			while ( nSelectedItem != ( -1 ) )
			{
				std::string szKey = m_GraphsList.GetItemText( nSelectedItem, 0 );
				m_GraphsList.DeleteItem( nSelectedItem );
				graphs.erase( szKey );
				nSelectedItem = m_GraphsList.GetNextItem( -1, LVIS_SELECTED );
			}
			bCreateControls = false;
			LoadGraphToControls();
		}
	}
}

void CRMGCreateGraphDialog::OnGraphPropertiesButton() 
{
	
}

void CRMGCreateGraphDialog::OnItemchangedGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if ( !bCreateControls )
	{
		LoadGraphToControls();
	}
	UpdateControls();
	*pResult = 0;
}

void CRMGCreateGraphDialog::OnDblclkGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	OnGraphPropertiesButton();
	*pResult = 0;
}

void CRMGCreateGraphDialog::OnRclickGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 2 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[4] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CG_ADD_GRAPH_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( vID[5] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CG_DELETE_GRAPH_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		/**
		if ( CWnd* pWnd = GetDlgItem( vID[6] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CG_GRAPH_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		/**/
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGCreateGraphDialog::OnKeydownGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CPoint mousePoint;
	CRect graphListRect;

	GetCursorPos( &mousePoint );
	m_GraphsList.GetWindowRect( &graphListRect );
	
	if ( graphListRect.PtInRect( mousePoint ) )
	{

		LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
		if (  pLVKeyDown->wVKey == VK_INSERT )
		{
			if ( CWnd* pWnd = GetDlgItem( vID[4] ) )
			{
				if ( pWnd->IsWindowEnabled() )
				{
					OnAddGraphButton();
				}
			}
		}
		else if ( pLVKeyDown->wVKey == VK_DELETE )
		{
			if ( CWnd* pWnd = GetDlgItem( vID[5] ) )
			{
				if ( pWnd->IsWindowEnabled() )
				{
					OnDeleteGraphButton();
				}
			}
		}
		else if (  pLVKeyDown->wVKey == VK_SPACE )
		{
			if ( CWnd* pWnd = GetDlgItem( vID[6] ) )
			{
				if ( pWnd->IsWindowEnabled() )
				{
					OnGraphPropertiesButton();
				}
			}
		}
	}
	*pResult = 0;
}

void CRMGCreateGraphDialog::OnColumnclickGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CG_GRAPHS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CG_GRAPHS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_GraphsList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_GraphsList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_GraphsList.SortItems( CG_GraphsCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bGraphSortParam[nSortColumn] = !bGraphSortParam[nSortColumn];
	*pResult = 0;
	*pResult = 0;
}

void CRMGCreateGraphDialog::OnAddGraphMenu() 
{
	OnAddGraphButton();
}

void CRMGCreateGraphDialog::OnDeleteGraphMenu() 
{
	OnDeleteGraphButton();
}

/**
void CRMGCreateGraphDialog::OnGraphPropertiesMenu() 
{
	OnGraphPropertiesButton();
}
/**/

void CRMGCreateGraphDialog::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	if ( !bCreateControls )
	{
		nPatchesCount = m_NodesSlider.GetPos();
		LoadGraphToControls();
	}
	CResizeDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CRMGCreateGraphDialog::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	if ( !bCreateControls )
	{
		nPatchesCount = m_NodesSlider.GetPos();
		LoadGraphToControls();
	}
	CResizeDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}

bool CRMGCreateGraphDialog::LoadGraphsList()
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return false;
	}

	SetWindowText( NStr::Format( "%s - [%s]", CG_GRAPHS_DIALOG_TITLE, resizeDialogOptions.szParameters[2] ) );
	BeginWaitCursor();
	LoadDataResource( resizeDialogOptions.szParameters[2], "", false, 0, CG_GRAPHS_XML_NAME, graphs );
	
	m_GraphsList.DeleteAllItems();
	for ( CRMGraphsHashMap::iterator graphIterator = graphs.begin();  graphIterator != graphs.end(); ++graphIterator )
	{
		int nNewItem = m_GraphsList.InsertItem( LVIF_TEXT, 0, graphIterator->first.c_str(), 0, 0, 0, 0 );
		if ( nNewItem == ( -1 ) )
		{
			EndWaitCursor();
			return false;
		}
		SetGraphItem( nNewItem, graphIterator->second );
	}
	LoadGraphToControls();
	EndWaitCursor();
	return true;
}

void CRMGCreateGraphDialog::OnSaveButton() 
{
	SaveGraphsList();
}

bool CRMGCreateGraphDialog::SaveGraphsList()
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return false;
	}

	SetWindowText( NStr::Format( "%s - [%s]", CG_GRAPHS_DIALOG_TITLE, resizeDialogOptions.szParameters[2] ) );
	BeginWaitCursor();
	for ( CRMGraphsHashMap::const_iterator graphIterator = graphs.begin();  graphIterator != graphs.end(); ++graphIterator )
	{
		SRMGraph graph = graphIterator->second;
		SaveDataResource( graphIterator->first, "", false, 0, RMGC_GRAPH_XML_NAME, graph );
	}

	if ( !SaveDataResource( resizeDialogOptions.szParameters[2], "", false, 0, CG_GRAPHS_XML_NAME, graphs ) )
	{
		EndWaitCursor();
		return false;
	}
	EndWaitCursor();
	return true;
}

bool CRMGCreateGraphDialog::LoadGraphToControls()
{
	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szSize = m_GraphsList.GetItemText( nFocusedItem, 1 );
		int nX = 0;
		int nY = 0;
		sscanf( szSize.c_str(), "%dx%d", &nX, &nY );
		if ( nX > nPatchesCount )
		{
			nPatchesCount = nX;
		}
		if ( nY > nPatchesCount )
		{
			nPatchesCount = nY;
		}
		if ( nPatchesCount != m_NodesSlider.GetPos() )
		{
			bCreateControls = true;
			m_NodesSlider.SetPos( nPatchesCount );
			bCreateControls = false;
		}
	}

	CRect updateRect;
	GetNodesPlaceRect( &updateRect );
	InvalidateRect( updateRect, false );
	UpdateWindow();
	return true;
}

bool CRMGCreateGraphDialog::SaveGraphFromControls()
{
	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
		SetGraphItem( nFocusedItem, graphs[szKey] );
	}
	return true;
}

void CRMGCreateGraphDialog::SetGraphItem( int nItem, SRMGraph &rGraph )
{
	int nX = 0;
	int nY = 0;
	int nNodesCount = 0;
	int nEmptyNodesCount = 0;
	for ( CRMGraphNodesList::const_iterator graphNodeIterator = rGraph.nodes.begin(); graphNodeIterator != rGraph.nodes.end(); ++graphNodeIterator )
	{
		int nPatchXIndex = ( ( graphNodeIterator->rect.maxx - 1 ) + STerrainPatchInfo::nSizeX ) / STerrainPatchInfo::nSizeX;
		int nPatchYIndex = ( ( graphNodeIterator->rect.maxy - 1 ) + STerrainPatchInfo::nSizeY ) / STerrainPatchInfo::nSizeY;
		if ( nX < nPatchXIndex )
		{
			nX = nPatchXIndex;
		}
		if ( nY < nPatchYIndex )
		{
			nY = nPatchYIndex;
		}
		++nNodesCount;
		if ( graphNodeIterator->szContainerFileName.empty() )
		{
			++nEmptyNodesCount;
		}
	}
	rGraph.size.x = nX;
	rGraph.size.y = nY;
	int nLinksCount = 0;
	int nEmptyLinksCount = 0;
	for ( CRMGraphLinksList::const_iterator linkNodeIterator = rGraph.links.begin(); linkNodeIterator != rGraph.links.end(); ++linkNodeIterator )
	{
		++nLinksCount;
		if ( linkNodeIterator->szDescFileName.empty() )
		{
			++nEmptyLinksCount;
		}
	}
	
	m_GraphsList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%4dx%-4d", rGraph.size.x, rGraph.size.y ), 0, 0, 0, 0 );
	m_GraphsList.SetItem( nItem, 2, LVIF_TEXT, NStr::Format( "%4d", nNodesCount ), 0, 0, 0, 0 );
	m_GraphsList.SetItem( nItem, 3, LVIF_TEXT, NStr::Format( "%4d", nLinksCount ), 0, 0, 0, 0 );
	m_GraphsList.SetItem( nItem, 4, LVIF_TEXT, NStr::Format( "%4d", nEmptyNodesCount ), 0, 0, 0, 0 );
	m_GraphsList.SetItem( nItem, 5, LVIF_TEXT, NStr::Format( "%4d", nEmptyLinksCount ), 0, 0, 0, 0 );
	
	std::string szSeasonName;
	RMGGetSeasonNameString( rGraph.nSeason, rGraph.szSeasonFolder, &szSeasonName );
	m_GraphsList.SetItem( nItem, 6, LVIF_TEXT, szSeasonName.c_str(), 0, 0, 0, 0 );
	m_GraphsList.SetItem( nItem, 7, LVIF_TEXT, rGraph.szSeasonFolder.c_str(), 0, 0, 0, 0 );

	std::string szSettingsNames;
	std::list<std::string> settings;
	if ( rGraph.GetSupportedSettings( &settings ) > 0 )
	{
		for ( std::list<std::string>::const_iterator settingNameIterator = settings.begin(); settingNameIterator != settings.end(); ++settingNameIterator )
		{
			if ( settingNameIterator == settings.begin() )
			{
				szSettingsNames = ( *settingNameIterator );
			}
			else
			{
				szSettingsNames += std::string( "; " ) + ( *settingNameIterator );
			}
		}
	}
	m_GraphsList.SetItem( nItem, 8, LVIF_TEXT, szSettingsNames.c_str(), 0, 0, 0, 0 );
	
	std::string szUsedScriptIDs;
	RMGGetUsedScriptIDsString( rGraph.usedScriptIDs, &szUsedScriptIDs );
	m_GraphsList.SetItem( nItem, 9, LVIF_TEXT, szUsedScriptIDs.c_str(), 0, 0, 0, 0 );
	
	std::string szUsedScripAreas;
	RMGGetUsedScriptAreasString( rGraph.usedScriptAreas, &szUsedScripAreas );
	m_GraphsList.SetItem( nItem, 10, LVIF_TEXT, szUsedScripAreas.c_str(), 0, 0, 0, 0 );
}

bool CRMGCreateGraphDialog::IsValidGraphEntered()
{
	return true;
}

void CRMGCreateGraphDialog::UpdateControls()
{
	CWnd* pWnd = 0;
	if ( pWnd = GetDlgItem( vID[8] ) )
	{
		pWnd->EnableWindow( bCreateControls || IsValidGraphEntered() );
	}
	if ( pWnd = GetDlgItem( vID[5] ) )
	{
		pWnd->EnableWindow( m_GraphsList.GetSelectedCount() > 0 );
	}
	if ( pWnd = GetDlgItem( vID[6] ) )
	{
		pWnd->EnableWindow( false );
	}
}

void CRMGCreateGraphDialog::CreateControls()
{
	bCreateControls = true;
	m_GraphsList.SetExtendedStyle( m_GraphsList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CG_GRAPHS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex] = CG_GRAPHS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_GraphsList.InsertColumn( nColumnIndex, CG_GRAPHS_COLUMN_NAME[nColumnIndex], CG_GRAPHS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bGraphSortParam.push_back( true );
	}

	m_NodesSlider.SetRange( 1, 32 );
	m_NodesSlider.SetPos( nPatchesCount );

	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );
	mousePoints = nodesPlaceRect;

	bCreateControls = false;
}

void CRMGCreateGraphDialog::ClearControls()
{
}

void CRMGCreateGraphDialog::GetNodesPlaceRect( CRect* pRect, bool onlyDimensions )
{
	NI_ASSERT_T( pRect != 0,
							 NStr::Format( "Wrong parameters: %x", pRect ) );
	if( CWnd* pwnd = GetDlgItem( vID[3] ) )
	{
		pwnd->GetWindowRect( pRect );
		pRect->left		+= CG_GRAPH_NODE_PLACE_FRAME_OFFSET_MIN;
		pRect->top		+= CG_GRAPH_NODE_PLACE_FRAME_OFFSET_MIN;
		pRect->right	-= CG_GRAPH_NODE_PLACE_FRAME_OFFSET_MAX;
		pRect->bottom -= CG_GRAPH_NODE_PLACE_FRAME_OFFSET_MAX;
		ScreenToClient( pRect );
		if ( onlyDimensions )
		{
			pRect->right -= pRect->left;
			pRect->left = 0;
			pRect->bottom -= pRect->top;
			pRect->top = 0;
		}
		return;
	}
	pRect->SetRectEmpty();
}

CTPoint<int> CRMGCreateGraphDialog::GetTilePoint( int x, int y )
{
	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );

	return CTPoint<int>( ( ( x - nodesPlaceRect.left ) * nPatchesCount * STerrainPatchInfo::nSizeX ) / nodesPlaceRect.Width(),
											 ( nPatchesCount * STerrainPatchInfo::nSizeY - 1 ) - ( ( y - nodesPlaceRect.top ) * nPatchesCount * STerrainPatchInfo::nSizeY ) / nodesPlaceRect.Height() );
}

CTRect<int> CRMGCreateGraphDialog::GetTileRect( int minx, int miny, int maxx, int maxy )
{
	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );

	return CTRect<int>( ( ( minx - nodesPlaceRect.left ) * nPatchesCount * STerrainPatchInfo::nSizeX ) / nodesPlaceRect.Width(),
											( nPatchesCount * STerrainPatchInfo::nSizeY - 1 ) - ( ( miny - nodesPlaceRect.top ) * nPatchesCount * STerrainPatchInfo::nSizeY ) / nodesPlaceRect.Height(),
											( ( maxx - nodesPlaceRect.left ) * nPatchesCount * STerrainPatchInfo::nSizeX ) / nodesPlaceRect.Width(),
											( nPatchesCount * STerrainPatchInfo::nSizeY - 1 ) - ( ( maxy - nodesPlaceRect.top ) * nPatchesCount * STerrainPatchInfo::nSizeY ) / nodesPlaceRect.Height() );
}

CTRect<int> CRMGCreateGraphDialog::GetScreenRect( int minx, int miny, int maxx, int maxy )
{
	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );
	CTRect<int> resultRect;
	if ( minx < maxx )
	{
		resultRect.minx = minx * nodesPlaceRect.Width() / ( nPatchesCount * STerrainPatchInfo::nSizeX );
		resultRect.maxx = maxx * nodesPlaceRect.Width() / ( nPatchesCount * STerrainPatchInfo::nSizeX ) + 1;
	}
	else
	{
		resultRect.minx = maxx * nodesPlaceRect.Width() / ( nPatchesCount * STerrainPatchInfo::nSizeX );
		resultRect.maxx = minx * nodesPlaceRect.Width() / ( nPatchesCount * STerrainPatchInfo::nSizeX ) + 1;
	}
	
	miny = nPatchesCount * STerrainPatchInfo::nSizeY - miny;
	maxy = nPatchesCount * STerrainPatchInfo::nSizeY - maxy;
	if ( miny < maxy )
	{
		resultRect.miny = miny * nodesPlaceRect.Height() / ( nPatchesCount * STerrainPatchInfo::nSizeY );
		resultRect.maxy = maxy * nodesPlaceRect.Height() / ( nPatchesCount * STerrainPatchInfo::nSizeY ) + 1;
	}	
	else
	{
		resultRect.miny = maxy * nodesPlaceRect.Height() / ( nPatchesCount * STerrainPatchInfo::nSizeY );
		resultRect.maxy = miny * nodesPlaceRect.Height() / ( nPatchesCount * STerrainPatchInfo::nSizeY ) + 1;
	}
	return resultRect;
}

bool CRMGCreateGraphDialog::CheckForGraphElement( const SRMGraph &rGraph, const CTPoint<int> rMousePoint, SGraphCheckInfo *pGraphCheckInfo )
{
	NI_ASSERT_TF( pGraphCheckInfo != 0,
								NStr::Format( "Invalid parameter: pGraphCheckInfo %x", pGraphCheckInfo ),
								return false );

	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );
	CTPoint<int> localMousePoint( rMousePoint.x - nodesPlaceRect.left, rMousePoint.y - nodesPlaceRect.top );

	pGraphCheckInfo->bSomeChecked = false;
	pGraphCheckInfo->linksIndices.clear();
	pGraphCheckInfo->nNodeIndex = -1;
	pGraphCheckInfo->dwSide = 0;
	
	for ( int nLinkIndex = 0; nLinkIndex < rGraph.links.size(); ++nLinkIndex )
	{
		if ( ( rGraph.links[nLinkIndex].link.a >= 0 ) &&
				 ( rGraph.links[nLinkIndex].link.a < rGraph.nodes.size() ) &&
				 ( rGraph.links[nLinkIndex].link.b >= 0 ) &&
				 ( rGraph.links[nLinkIndex].link.b < rGraph.nodes.size() ) )
		{
			CTRect<int> rectA = GetScreenRect( rGraph.nodes[rGraph.links[nLinkIndex].link.a].rect );
			rectA.Normalize();
			CTRect<int> rectB = GetScreenRect( rGraph.nodes[rGraph.links[nLinkIndex].link.b].rect );
			rectB.Normalize();

			CTPoint<int> centerPointA = rectA.GetCenter();
			CTPoint<int> centerPointB = rectB.GetCenter();
		
			CVec2 vA( centerPointA.x, centerPointA.y );
			CVec2 vB( centerPointB.x, centerPointB.y );
			CVec2 vLine = ( vB - vA );
			CVec2 vNormal = GetNormal( vB - vA );
			Normalize( &vLine );
			Normalize( &vNormal );
			
			std::list<CVec2> polygon;
			polygon.push_back( vA - ( vLine * CG_GRAPH_LINK_HALF_WIDTH ) + ( vNormal * CG_GRAPH_LINK_HALF_WIDTH ) );
			polygon.push_back( vB + ( vLine * CG_GRAPH_LINK_HALF_WIDTH ) + ( vNormal * CG_GRAPH_LINK_HALF_WIDTH ) );
			polygon.push_back( vB + ( vLine * CG_GRAPH_LINK_HALF_WIDTH ) - ( vNormal * CG_GRAPH_LINK_HALF_WIDTH ) );
			polygon.push_back( vA - ( vLine * CG_GRAPH_LINK_HALF_WIDTH ) - ( vNormal * CG_GRAPH_LINK_HALF_WIDTH ) );
			if ( ClassifyPolygon( polygon, CVec2( localMousePoint.x, localMousePoint.y ) ) != CP_OUTSIDE )
			{
				pGraphCheckInfo->bSomeChecked = true;
				pGraphCheckInfo->linksIndices.push_back( nLinkIndex );
			}
		}
	}

	CTPoint<int> tilePoint = GetTilePoint( rMousePoint );
	for ( int nNodeIndex = 0; nNodeIndex < rGraph.nodes.size(); ++nNodeIndex )
	{
		if ( IsValidPoint( rGraph.nodes[nNodeIndex].rect, tilePoint ) )
		{
			pGraphCheckInfo->bSomeChecked = true;
			pGraphCheckInfo->nNodeIndex = nNodeIndex;
			pGraphCheckInfo->dwSide |= SGraphCheckInfo::SIDE_CENTER;
			if ( tilePoint.x == rGraph.nodes[nNodeIndex].rect.minx )
			{
				pGraphCheckInfo->dwSide |= SGraphCheckInfo::SIDE_MINX;
			}
			if ( tilePoint.x == ( rGraph.nodes[nNodeIndex].rect.maxx - 1 ) )
			{
				pGraphCheckInfo->dwSide |= SGraphCheckInfo::SIDE_MAXX;
			}
			if ( tilePoint.y == rGraph.nodes[nNodeIndex].rect.miny )
			{
				pGraphCheckInfo->dwSide |= SGraphCheckInfo::SIDE_MINY;
			}
			if ( tilePoint.y == ( rGraph.nodes[nNodeIndex].rect.maxy - 1 ) )
			{
				pGraphCheckInfo->dwSide |= SGraphCheckInfo::SIDE_MAXY;
			}
			break;
		}
	}
	return pGraphCheckInfo->bSomeChecked;
}

void CRMGCreateGraphDialog::OnPaint()
{

	bool bCursorNotSet = true;
	CPaintDC paintDC(this);

	if ( GetDrawGripper() )
	{
		CRect clientRect;
		GetClientRect( &clientRect );

		clientRect.left = clientRect.right - ::GetSystemMetrics( SM_CXHSCROLL );
		clientRect.top = clientRect.bottom - ::GetSystemMetrics( SM_CYVSCROLL );

		paintDC.DrawFrameControl( clientRect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP );
	}

	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );

	CDC dc;
	int nRes = dc.CreateCompatibleDC( &paintDC );
	CBitmap bmp;
	nRes = bmp.CreateCompatibleBitmap( &paintDC, nodesPlaceRect.Width(), nodesPlaceRect.Height() );
	CBitmap *pOldBitmap = dc.SelectObject( &bmp );
	
	CTRect<int> currentTileRect = GetTileRect( mousePoints );
	CTPoint<int> mousePoint = currentTileRect.GetLeftTop();
	currentTileRect.Normalize();
	currentTileRect.maxx += 1;
	currentTileRect.maxy += 1;
	std::string szAdditionalInfo;

	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
		const SRMGraph &rGraph = graphs[szKey];

		if ( ( inputState != STATE_ADD ) && ( inputState != STATE_ADD_LINK ) )
		{
			SGraphCheckInfo graphCheckInfo;
			if ( ( inputState == STATE_MOVE ) || ( inputState == STATE_RESIZE ) )
			{
				graphCheckInfo = lastGraphCheckInfo;
			}
			else
			{
				CheckForGraphElement( rGraph, mousePoints.GetRightBottom(), &graphCheckInfo );
			}
			if ( graphCheckInfo.bSomeChecked )
			{
				if ( graphCheckInfo.linksIndices.size() > 1 )
				{
					szAdditionalInfo = NStr::Format( "%d Links", graphCheckInfo.linksIndices.size() );
				}
				else if ( graphCheckInfo.linksIndices.size() == 1 )
				{
					if ( rGraph.links[ graphCheckInfo.linksIndices[0] ].szDescFileName.empty() )
					{
						szAdditionalInfo = NStr::Format( "Empty Link %d", graphCheckInfo.linksIndices[0] );
					}
					else
					{
						szAdditionalInfo = NStr::Format( "Link %d: %s", graphCheckInfo.linksIndices[0], rGraph.links[ graphCheckInfo.linksIndices[0] ].szDescFileName.c_str() );
					}
				}
				if ( graphCheckInfo.nNodeIndex >= 0 )
				{
					currentTileRect = rGraph.nodes[graphCheckInfo.nNodeIndex].rect;
					if ( rGraph.nodes[graphCheckInfo.nNodeIndex].szContainerFileName.empty() )
					{
						if ( szAdditionalInfo.empty() )
						{
							szAdditionalInfo = NStr::Format( "Empty Node %d", graphCheckInfo.nNodeIndex );
						}
						else
						{
							szAdditionalInfo += NStr::Format( ", Empty Node %d", graphCheckInfo.nNodeIndex );
						}
					}
					else
					{
						if ( szAdditionalInfo.empty() )
						{
							szAdditionalInfo = NStr::Format( "Node %d: %s", graphCheckInfo.nNodeIndex, rGraph.nodes[graphCheckInfo.nNodeIndex].szContainerFileName.c_str() );
						}
						else
						{
							szAdditionalInfo += NStr::Format( ", Node %d: %s", graphCheckInfo.nNodeIndex, rGraph.nodes[graphCheckInfo.nNodeIndex].szContainerFileName.c_str() );
						}
					}
					if ( graphCheckInfo.linksIndices.empty() )
					{
						if ( ( ( graphCheckInfo.dwSide & ( SGraphCheckInfo::SIDE_MINX | SGraphCheckInfo::SIDE_MINY ) ) == ( SGraphCheckInfo::SIDE_MINX | SGraphCheckInfo::SIDE_MINY ) ) ||
								 ( ( graphCheckInfo.dwSide & ( SGraphCheckInfo::SIDE_MAXX | SGraphCheckInfo::SIDE_MAXY ) ) == ( SGraphCheckInfo::SIDE_MAXX | SGraphCheckInfo::SIDE_MAXY ) ) )
						{
							SetCursor( LoadCursor( 0, IDC_SIZENESW ) );
						}
						else if ( ( ( graphCheckInfo.dwSide & ( SGraphCheckInfo::SIDE_MINX | SGraphCheckInfo::SIDE_MAXY ) ) == ( SGraphCheckInfo::SIDE_MINX | SGraphCheckInfo::SIDE_MAXY ) ) ||
											( ( graphCheckInfo.dwSide & ( SGraphCheckInfo::SIDE_MAXX | SGraphCheckInfo::SIDE_MINY ) ) == ( SGraphCheckInfo::SIDE_MAXX | SGraphCheckInfo::SIDE_MINY ) ) )
						{
							SetCursor( LoadCursor( 0, IDC_SIZENWSE ) );
						}
						else if ( ( ( graphCheckInfo.dwSide & SGraphCheckInfo::SIDE_MINX ) == SGraphCheckInfo::SIDE_MINX ) ||
											( ( graphCheckInfo.dwSide & SGraphCheckInfo::SIDE_MAXX ) == SGraphCheckInfo::SIDE_MAXX ) )
						{
							SetCursor( LoadCursor( 0, IDC_SIZEWE ) );
						}
						else if ( ( ( graphCheckInfo.dwSide & SGraphCheckInfo::SIDE_MINY ) == SGraphCheckInfo::SIDE_MINY ) ||
											( ( graphCheckInfo.dwSide & SGraphCheckInfo::SIDE_MAXY ) == SGraphCheckInfo::SIDE_MAXY ) )
						{
							SetCursor( LoadCursor( 0, IDC_SIZENS ) );
						}
						else
						{
							SetCursor( LoadCursor( 0, IDC_SIZEALL ) );
						}
						bCursorNotSet = false;
					}
				}
			}
		}
		CPen nodePen( PS_SOLID, 1, RGB( 0xFF, 0xFF, 0xFF ) );
		CPen* pOldPen = dc.SelectObject( &nodePen );

		for ( int nNodeIndex = 0; nNodeIndex < rGraph.nodes.size(); ++nNodeIndex )
		{
			CTRect<int> outerTileRect = GetScreenRect( rGraph.nodes[nNodeIndex].rect );
			CTRect<int> innerTileRect = GetScreenRect( rGraph.nodes[nNodeIndex].rect.minx + 1,
																								 rGraph.nodes[nNodeIndex].rect.miny + 1,
																								 rGraph.nodes[nNodeIndex].rect.maxx - 1,
																								 rGraph.nodes[nNodeIndex].rect.maxy - 1 );
			CBrush* pOldBrush = 0;
			COLORREF oldColor;
			if ( rGraph.nodes[nNodeIndex].szContainerFileName.empty() )
			{
				pOldBrush = (CBrush*)dc.SelectStockObject( DKGRAY_BRUSH );
				dc.Rectangle( &static_cast<RECT>( outerTileRect ) );
				dc.Rectangle( &static_cast<RECT>( innerTileRect ) );
				oldColor = dc.SetTextColor ( RGB( 0xFF, 0xA0, 0xA0 ) );
			}
			else
			{
				pOldBrush = (CBrush*)dc.SelectStockObject( GRAY_BRUSH );
				dc.Rectangle( &static_cast<RECT>( outerTileRect ) );
				dc.Rectangle( &static_cast<RECT>( innerTileRect ) );
				oldColor = dc.SetTextColor ( RGB( 0xA0, 0xFF, 0xA0 ) );
			}
			int oldMode = dc.SetBkMode( TRANSPARENT );
			dc.TextOut( innerTileRect.minx + 2, innerTileRect.miny + 2, NStr::Format( "%d", nNodeIndex ) );
			dc.SetBkMode( oldMode );
			dc.SelectObject( pOldBrush );
		}
		dc.SelectObject( pOldPen );

		CPen linkPen( PS_SOLID, CG_GRAPH_LINK_HALF_WIDTH * 2, RGB( 0xA0, 0xFF, 0xA0 ) );
		CPen emptylinkPen( PS_SOLID, CG_GRAPH_LINK_HALF_WIDTH * 2, RGB( 0xFF, 0xA0, 0xA0 ) );

		for ( CRMGraphLinksList::const_iterator graphLinkIterator = rGraph.links.begin(); graphLinkIterator != rGraph.links.end(); ++graphLinkIterator )
		{
			if ( ( graphLinkIterator->link.a >= 0 ) &&
					 ( graphLinkIterator->link.a < rGraph.nodes.size() ) &&
					 ( graphLinkIterator->link.b >= 0 ) &&
					 ( graphLinkIterator->link.b < rGraph.nodes.size() ) )
			{
				CTRect<int> rectA = GetScreenRect( rGraph.nodes[graphLinkIterator->link.a].rect );
				rectA.Normalize();
				CTRect<int> rectB = GetScreenRect( rGraph.nodes[graphLinkIterator->link.b].rect );
				rectB.Normalize();

				CTPoint<int> centerPointA = rectA.GetCenter();
				CTPoint<int> centerPointB = rectB.GetCenter();
				
				if ( graphLinkIterator->szDescFileName.empty() )
				{
					CPen* pOldPen = dc.SelectObject( &emptylinkPen );
					dc.MoveTo( centerPointA.x, centerPointA.y );
					dc.LineTo( centerPointB.x, centerPointB.y );
					dc.SelectObject( pOldPen );
				}
				else
				{
					CPen* pOldPen = dc.SelectObject( &linkPen );
					dc.MoveTo( centerPointA.x, centerPointA.y );
					dc.LineTo( centerPointB.x, centerPointB.y );
					dc.SelectObject( pOldPen );
				}
			}
		}

		if ( inputState == STATE_ADD )
		{
			CPen mouseFramePen( PS_DASH, 1, RGB( 0xFF, 0xFF, 0xFF ) );
			CPen* pOldPen = dc.SelectObject( &mouseFramePen );
			CTRect<int> rect( mousePoints );
			rect.Normalize();
			rect.Move( -nodesPlaceRect.left, -nodesPlaceRect.top );
			dc.MoveTo( rect.minx, rect.miny );
			dc.LineTo( rect.minx, rect.maxy );
			dc.LineTo( rect.maxx, rect.maxy );
			dc.LineTo( rect.maxx, rect.miny );
			dc.LineTo( rect.minx, rect.miny );
			dc.SelectObject( pOldPen );
		}
		else if ( inputState == STATE_ADD_LINK )
		{
			CPen* pOldPen = dc.SelectObject( &emptylinkPen );
			CTRect<int> rect( mousePoints );
			rect.Move( -nodesPlaceRect.left, -nodesPlaceRect.top );
			dc.MoveTo( rect.minx, rect.miny );
			dc.LineTo( rect.maxx, rect.maxy );
			dc.SelectObject( pOldPen );
		}
	}
		
	m_NodesMessageTop.SetWindowText( NStr::Format( "Map: [%dx%d], position: (%d, %d), rect: (%d, %d, %d, %d) [%dx%d]",
																									nPatchesCount,
																									nPatchesCount,
																									mousePoint.x,
																									mousePoint.y,
																									currentTileRect.minx,
																									currentTileRect.miny,
																									currentTileRect.maxx,
																									currentTileRect.maxy,
																									currentTileRect.GetSizeX(),
																									currentTileRect.GetSizeY() ) );
	m_NodesMessageBottom.SetWindowText( NStr::Format( "%s", szAdditionalInfo ) );
	
	if ( nPatchesCount > 1 )
	{
		CPen gridPen( PS_DOT, 1, RGB( 0x80, 0x80, 0x80 ) );
		CPen* pOldPen = dc.SelectObject( &gridPen );
		for ( int nGridLineIndex = 1; nGridLineIndex < nPatchesCount; ++nGridLineIndex )
		{
			dc.MoveTo( 0, nGridLineIndex * nodesPlaceRect.Height() / nPatchesCount );
			dc.LineTo( nodesPlaceRect.right - 1, nGridLineIndex * nodesPlaceRect.Height() / nPatchesCount );
			dc.MoveTo( nGridLineIndex * nodesPlaceRect.Width() / nPatchesCount, 0 );
			dc.LineTo( nGridLineIndex * nodesPlaceRect.Width() / nPatchesCount, nodesPlaceRect.bottom - 1 );
		}
		dc.SelectObject( pOldPen );
	}
	
	paintDC.BitBlt( nodesPlaceRect.left, nodesPlaceRect.top, nodesPlaceRect.Width(), nodesPlaceRect.Height(), &dc, 0, 0, SRCCOPY );
	dc.SelectObject( pOldBitmap );
}

void CRMGCreateGraphDialog::OnLButtonDown( UINT nFlags, CPoint point ) 
{
	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem == ( -1 ) )
	{
		CResizeDialog::OnLButtonDown( nFlags, point );
		return;
	}
	std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
	const SRMGraph &rGraph = graphs[szKey];

	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );
	if ( nodesPlaceRect.PtInRect( point ) )
	{
		mousePoints.minx = point.x;
		mousePoints.miny = point.y;
		mousePoints.maxx = point.x;
		mousePoints.maxy = point.y;

		if ( nFlags & MK_CONTROL )
		{
			inputState = STATE_ADD_LINK;
		}
		else
		{
			if ( CheckForGraphElement( rGraph, mousePoints.GetRightBottom(), &lastGraphCheckInfo ) )
			{
				if ( lastGraphCheckInfo.nNodeIndex >= 0 )
				{
					if ( ( lastGraphCheckInfo.dwSide & ( SGraphCheckInfo::SIDE_MINX | SGraphCheckInfo::SIDE_MINY | SGraphCheckInfo::SIDE_MAXX | SGraphCheckInfo::SIDE_MAXY ) ) > 0 )
					{
						inputState = STATE_RESIZE;
					}
					else
					{
						inputState = STATE_MOVE;
					}
					lastNodePlace = rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect;
				}
			}
			else
			{
				inputState = STATE_ADD;
			}
		}

		
		GetClipCursor( &oldClipRect ); 
		ClientToScreen( &nodesPlaceRect );
		ClipCursor( &nodesPlaceRect ); 
		SetCapture();
		LoadGraphToControls();
	}
	else
	{
		CResizeDialog::OnLButtonDown( nFlags, point );
	}
}

void CRMGCreateGraphDialog::OnLButtonUp( UINT nFlags, CPoint point )
{  
	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem == ( -1 ) )
	{
		CResizeDialog::OnLButtonUp( nFlags, point );
		return;
	}
	std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
	SRMGraph &rGraph = graphs[szKey];


	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );
	if ( nodesPlaceRect.PtInRect( point ) )
	{
		mousePoints.maxx = point.x;
		mousePoints.maxy = point.y;
		
		CTRect<int> currentTileRect = GetTileRect( mousePoints );
		if ( inputState == STATE_ADD )
		{
			currentTileRect.Normalize();
			currentTileRect.maxx += 1;
			currentTileRect.maxy += 1;

			bool bOutside = true;
			if ( ( currentTileRect.GetSizeX() < STerrainPatchInfo::nSizeX ) || ( currentTileRect.GetSizeY() < STerrainPatchInfo::nSizeY ) )
			{
				bOutside = false;
			}
			else
			{
				for ( CRMGraphNodesList::const_iterator graphNodeIterator = rGraph.nodes.begin(); graphNodeIterator != rGraph.nodes.end(); ++graphNodeIterator )
				{
					if ( graphNodeIterator->rect.IsIntersect( currentTileRect ) )
					{
						bOutside = false;
						break;
					}
				}
			}
			if ( bOutside )
			{
				SRMGraphNode graphNode;
				graphNode.rect = currentTileRect;
				rGraph.nodes.push_back( graphNode );
				SaveGraphFromControls();
			}
		}
		else if ( ( inputState == STATE_MOVE ) || ( inputState == STATE_RESIZE ) )
		{
			bool bInside = false;
			for ( int nNodeIndex = 0; nNodeIndex < rGraph.nodes.size(); ++nNodeIndex )
			{
				if ( nNodeIndex != lastGraphCheckInfo.nNodeIndex )
				{
					if ( rGraph.nodes[nNodeIndex].rect.IsIntersect( rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect ) )
					{
						bInside = true;
						break;
					}
				}
			}
			if ( bInside )
			{
				rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect = lastNodePlace;
				SaveGraphFromControls();
			}
		}
		else if ( inputState == STATE_ADD_LINK )
		{
			int nAIndex = -1;
			int nBIndex = -1;
			int nNodeIndex = 0;

			for ( int nNodeIndex = 0; nNodeIndex < rGraph.nodes.size(); ++nNodeIndex )
			{
				if ( IsValidPoint( rGraph.nodes[nNodeIndex].rect, currentTileRect.GetLeftTop() ) )
				{
					nAIndex = nNodeIndex;
				}
				if ( IsValidPoint( rGraph.nodes[nNodeIndex].rect, currentTileRect.GetRightBottom() ) )
				{
					nBIndex = nNodeIndex;
				}
			}
			if ( ( nAIndex >= 0 ) && ( nBIndex >= 0 ) && ( nAIndex != nBIndex ) )
			{
				int nLinksCount = 0;
				for ( CRMGraphLinksList::const_iterator graphLinkIterator = rGraph.links.begin(); graphLinkIterator != rGraph.links.end(); ++graphLinkIterator )
				{
					if ( ( ( nAIndex == graphLinkIterator->link.a ) && ( nBIndex == graphLinkIterator->link.b ) ) ||
							 ( ( nAIndex == graphLinkIterator->link.b ) && ( nBIndex == graphLinkIterator->link.a ) ) )
					{
						++nLinksCount;
					}
				}
				SRMGraphLink graphLink;
				graphLink.link.a = nAIndex;
				graphLink.link.b = nBIndex;
				rGraph.links.push_back( graphLink );
				SaveGraphFromControls();
			}
		}

		inputState = STATE_NONE;
		ClipCursor( &oldClipRect ); 
		ReleaseCapture();
		LoadGraphToControls();
	}
	else
	{
		CResizeDialog::OnLButtonUp( nFlags, point );
	}
}

void CRMGCreateGraphDialog::OnMouseMove( UINT nFlags, CPoint point ) 
{
	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem == ( -1 ) )
	{
		CResizeDialog::OnMouseMove( nFlags, point );
		return;
	}
	std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
	SRMGraph &rGraph = graphs[szKey];

	if ( nFlags & MK_LBUTTON )
	{
		mousePoints.maxx = point.x;
		mousePoints.maxy = point.y;

		if ( inputState == STATE_ADD )
		{
		}
		else if ( ( inputState == STATE_MOVE ) || ( inputState == STATE_RESIZE ) )
		{
			CTRect<int> movedRect = lastNodePlace;
			CTPoint<int> nOldPoint = GetTilePoint( mousePoints.GetLeftTop() );
			CTPoint<int> nNewPoint = GetTilePoint( mousePoints.GetRightBottom() );
			if ( inputState == STATE_MOVE )
			{
				movedRect.Move( nNewPoint - nOldPoint );
			}			
			else
			{
				if ( ( lastGraphCheckInfo.dwSide & SGraphCheckInfo::SIDE_MINX ) > 0 )
				{
					movedRect.minx += ( nNewPoint.x - nOldPoint.x );
					if ( ( movedRect.maxx - movedRect.minx ) < STerrainPatchInfo::nSizeX )
					{
						movedRect.minx = movedRect.maxx - STerrainPatchInfo::nSizeX;
					}
				}
				else if ( ( lastGraphCheckInfo.dwSide & SGraphCheckInfo::SIDE_MAXX ) > 0 )
				{
					movedRect.maxx += ( nNewPoint.x - nOldPoint.x );
					if ( ( movedRect.maxx - movedRect.minx ) < STerrainPatchInfo::nSizeX )
					{
						movedRect.maxx = movedRect.minx + STerrainPatchInfo::nSizeX;
					}
				}
				if ( ( lastGraphCheckInfo.dwSide & SGraphCheckInfo::SIDE_MINY ) > 0 )
				{
					movedRect.miny += ( nNewPoint.y - nOldPoint.y );
					if ( ( movedRect.maxy - movedRect.miny ) < STerrainPatchInfo::nSizeY )
					{
						movedRect.miny = movedRect.maxy - STerrainPatchInfo::nSizeY;
					}
				}
				else if ( ( lastGraphCheckInfo.dwSide & SGraphCheckInfo::SIDE_MAXY ) > 0 )
				{
					movedRect.maxy += ( nNewPoint.y - nOldPoint.y );
					if ( ( movedRect.maxy - movedRect.miny ) < STerrainPatchInfo::nSizeY )
					{
						movedRect.maxy = movedRect.miny + STerrainPatchInfo::nSizeY;
					}
				}
			}
			
			if ( movedRect.minx < 0 )
			{
				movedRect.maxx -= movedRect.minx; 
				movedRect.minx = 0;
			}
			if ( movedRect.maxx > ( nPatchesCount * STerrainPatchInfo::nSizeX ) )
			{
				movedRect.minx -= ( movedRect.maxx - ( nPatchesCount * STerrainPatchInfo::nSizeX ) );
				movedRect.maxx = ( nPatchesCount * STerrainPatchInfo::nSizeX );
			}
			if ( movedRect.miny < 0 )
			{
				movedRect.maxy -= movedRect.miny; 
				movedRect.miny = 0;
			}
			if ( movedRect.maxy > ( nPatchesCount * STerrainPatchInfo::nSizeY ) )
			{
				movedRect.miny -= ( movedRect.maxy - ( nPatchesCount * STerrainPatchInfo::nSizeY ) );
				movedRect.maxy = ( nPatchesCount * STerrainPatchInfo::nSizeY );
			}
			rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect = movedRect;
			SaveGraphFromControls();
		}
		else if ( inputState == STATE_ADD_LINK )
		{
		}
		LoadGraphToControls();
	}
	else
	{
		CRect nodesPlaceRect;
		GetNodesPlaceRect( &nodesPlaceRect );
		if ( nodesPlaceRect.PtInRect( point ) )
		{
			mousePoints.minx = point.x;
			mousePoints.miny = point.y;
			mousePoints.maxx = point.x;
			mousePoints.maxy = point.y;
			
			LoadGraphToControls();
		}
		CResizeDialog::OnMouseMove( nFlags, point );
	}
}

void CRMGCreateGraphDialog::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags ) 
{
/**
	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem == ( -1 ) )
	{
		CResizeDialog::OnKeyDown( nChar, nRepCnt, nFlags );
		return;
	}
	std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
	SRMGraph &rGraph = graphs[szKey];

	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );
	if ( nodesPlaceRect.PtInRect( CPoint( mousePoints.maxx, mousePoints.maxy ) ) )
	{
		if ( inputState == STATE_NONE )
		{
			SGraphCheckInfo graphCheckInfo;
			CheckForGraphElement( rGraph, mousePoints.GetRightBottom(), &graphCheckInfo );
			if ( graphCheckInfo.bSomeChecked )
			{
				if ( !graphCheckInfo.linksIndices.empty() )
				{
					rGraph.links.erase( rGraph.links.begin() + graphCheckInfo.linksIndices[0] );
				}
				else if ( graphCheckInfo.nNodeIndex >= 0 )
				{
					for ( CRMGraphLinksList::iterator graphLinkIterator = rGraph.links.begin(); graphLinkIterator != rGraph.links.end(); )
					{
						if ( ( graphCheckInfo.nNodeIndex == graphLinkIterator->link.a ) || ( graphCheckInfo.nNodeIndex == graphLinkIterator->link.b ) )
						{
							graphLinkIterator = rGraph.links.erase( graphLinkIterator );
						}
						else
						{
							++graphLinkIterator;
						}
					}
					rGraph.nodes.erase( rGraph.nodes.begin() + graphCheckInfo.nNodeIndex );
				}
				SaveGraphFromControls();
				LoadGraphToControls();
			}			
		}
	}
	else
	{
		CResizeDialog::OnKeyDown( nChar, nRepCnt, nFlags );
	}
/**/
}

void CRMGCreateGraphDialog::OnRButtonUp( UINT nFlags, CPoint point ) 
{
	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem == ( -1 ) )
	{
		CResizeDialog::OnRButtonUp( nFlags, point );
		return;
	}
	std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
	SRMGraph &rGraph = graphs[szKey];


	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );
	if ( nodesPlaceRect.PtInRect( point ) )
	{
		if ( inputState == STATE_NONE )
		{
			CheckForGraphElement( rGraph, CTPoint<int>( point.x, point.y ), &lastGraphCheckInfo );
			if ( lastGraphCheckInfo.bSomeChecked )
			{
				CMenu composersMenu;
				composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
				CMenu *pMenu = composersMenu.GetSubMenu( 3 );
				if ( pMenu )
				{
					GetCursorPos( &point );
					pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
				}
				composersMenu.DestroyMenu();
			}			
		}
	}
	else
	{
		CResizeDialog::OnRButtonUp( nFlags, point );
	}
}

void CRMGCreateGraphDialog::OnDeleteMenu() 
{
	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
		SRMGraph &rGraph = graphs[szKey];

		if ( inputState == STATE_NONE )
		{
			if ( lastGraphCheckInfo.bSomeChecked )
			{
				if ( !lastGraphCheckInfo.linksIndices.empty() )
				{
					CString strTitle;
					strTitle.LoadString( IDR_EDITORTYPE );
					if ( MessageBox( "Do you really want to DELETE selected Links?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
					{
						rGraph.links.erase( rGraph.links.begin() + lastGraphCheckInfo.linksIndices[0] );
					}
					else
					{
						return;
					}
				}
				else if ( lastGraphCheckInfo.nNodeIndex >= 0 )
				{
					CString strTitle;
					strTitle.LoadString( IDR_EDITORTYPE );
					if ( MessageBox( "Do you really want to DELETE selected Nodes?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
					{
						for ( CRMGraphLinksList::iterator graphLinkIterator = rGraph.links.begin(); graphLinkIterator != rGraph.links.end(); )
						{
							if ( ( lastGraphCheckInfo.nNodeIndex == graphLinkIterator->link.a ) || ( lastGraphCheckInfo.nNodeIndex == graphLinkIterator->link.b ) )
							{
								graphLinkIterator = rGraph.links.erase( graphLinkIterator );
							}
							else
							{
								if ( graphLinkIterator->link.a > lastGraphCheckInfo.nNodeIndex )
								{
									graphLinkIterator->link.a -= 1;
								}
								if ( graphLinkIterator->link.b > lastGraphCheckInfo.nNodeIndex )
								{
									graphLinkIterator->link.b -= 1;
								}
								++graphLinkIterator;
							}
						}

						bool bFilledNodeNotExists = true;
						for ( int nNodeIndex = 0; nNodeIndex < rGraph.nodes.size(); ++nNodeIndex )
						{
							if ( ( nNodeIndex != lastGraphCheckInfo.nNodeIndex ) && 
									 ( !rGraph.nodes[nNodeIndex].szContainerFileName.empty() ) )
							{
								bFilledNodeNotExists = false;
								break;
							}
						}
						if ( bFilledNodeNotExists )
						{
							rGraph.nSeason = 0;
							rGraph.szSeasonFolder.clear();
							rGraph.usedScriptIDs.clear();
							rGraph.usedScriptAreas.clear();
						}
						rGraph.nodes.erase( rGraph.nodes.begin() + lastGraphCheckInfo.nNodeIndex );
					}
					else
					{
						return;
					}
				}
				SaveGraphFromControls();
				LoadGraphToControls();
			}
		}
	}
}

void CRMGCreateGraphDialog::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem == ( -1 ) )
	{
		CResizeDialog::OnLButtonDblClk( nFlags, point );
		return;
	}
	std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
	SRMGraph &rGraph = graphs[szKey];


	CRect nodesPlaceRect;
	GetNodesPlaceRect( &nodesPlaceRect );
	if ( nodesPlaceRect.PtInRect( point ) )
	{
		if ( inputState == STATE_NONE )
		{
			CheckForGraphElement( rGraph, CTPoint<int>( point.x, point.y ), &lastGraphCheckInfo );
			if ( lastGraphCheckInfo.bSomeChecked )
			{
				OnPropertiesMenu();
			}
		}
	}
	else
	{
		CResizeDialog::OnLButtonDblClk(nFlags, point);
	}
}

void CRMGCreateGraphDialog::OnPropertiesMenu() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	int nFocusedItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_GraphsList.GetItemText( nFocusedItem, 0 );
		SRMGraph &rGraph = graphs[szKey];

		if ( inputState == STATE_NONE )
		{
			if ( lastGraphCheckInfo.bSomeChecked )
			{
				if ( !lastGraphCheckInfo.linksIndices.empty() )
				{
					CRMGGraphLinkPropertiesDialog graphLinkPropertiesDialog;
					graphLinkPropertiesDialog.graph = rGraph;
					graphLinkPropertiesDialog.linkIndices = lastGraphCheckInfo.linksIndices;
					if ( graphLinkPropertiesDialog.DoModal() == IDOK )
					{
						rGraph.links = graphLinkPropertiesDialog.graph.links;
						SaveGraphFromControls();
						LoadGraphToControls();
					}
				}
				else if ( lastGraphCheckInfo.nNodeIndex >= 0 )
				{
					CRMGGraphNodePropertiesDialog graphNodePropertiesDialog;
					graphNodePropertiesDialog.szContainerInitialFileName = rGraph.nodes[lastGraphCheckInfo.nNodeIndex].szContainerFileName;
					
					CTPoint<int> containerSize( 0, 0 );
					{
						SRMContainer container;
						LoadDataResource( rGraph.nodes[lastGraphCheckInfo.nNodeIndex].szContainerFileName, "", false ,0, RMGC_CONTAINER_XML_NAME, container );
					}

					int nLinksCount = 0;
					for ( CRMGraphLinksList::iterator graphLinkIterator = rGraph.links.begin(); graphLinkIterator != rGraph.links.end(); ++graphLinkIterator )
					{
						if ( ( lastGraphCheckInfo.nNodeIndex == graphLinkIterator->link.a ) || ( lastGraphCheckInfo.nNodeIndex == graphLinkIterator->link.b ) )
						{
							++nLinksCount;
						}
					}
					graphNodePropertiesDialog.m_strSize.Format( "(%d, %d, %d, %d), [%dx%d], Container size: [%dx%d], Links: %d",
																											rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.minx,
																											rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.miny,
																											rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.maxx,
																											rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.maxy,
																											rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.Width(),
																											rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.Height(),
																											containerSize.x * STerrainPatchInfo::nSizeX,
																											containerSize.y * STerrainPatchInfo::nSizeY,
																											nLinksCount );
					if ( graphNodePropertiesDialog.DoModal() == IDOK )
					{
						SRMContainer container;
						if ( LoadDataResource( graphNodePropertiesDialog.szContainerInitialFileName, "", false ,0, RMGC_CONTAINER_XML_NAME, container ) )
						{
							if ( ( ( rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.Width() / STerrainPatchInfo::nSizeX ) < container.size.x ) ||
									 ( ( rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.Height() / STerrainPatchInfo::nSizeY ) < container.size.y ) )
							{
								CString strTitle;
								strTitle.LoadString( IDR_EDITORTYPE );
								std::string szCheckResult = NStr::Format( "Possibly invalid size in some directions:\r\nGraph Node: (%d, %d, %d, %d), [%dx%d]\r\nContainer: [%dx%d]",
																													rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.minx,
																													rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.miny,
																													rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.maxx,
																													rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.maxy,
																													rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.Width(),
																													rGraph.nodes[lastGraphCheckInfo.nNodeIndex].rect.Height(),
																													container.size.x * STerrainPatchInfo::nSizeX,
																													container.size.y * STerrainPatchInfo::nSizeY );
								MessageBox( NStr::Format( "Warning!\r\nContainer <%s>,\r\nGraph <%s>,\r\n%s",
																					graphNodePropertiesDialog.szContainerInitialFileName.c_str(),
																					szKey.c_str(),
																					szCheckResult.c_str() ),
														strTitle,
														MB_ICONEXCLAMATION | MB_OK );
							}
							bool bFilledNodeNotExists = true;
							for ( int nNodeIndex = 0; nNodeIndex < rGraph.nodes.size(); ++nNodeIndex )
							{
								if ( ( nNodeIndex != lastGraphCheckInfo.nNodeIndex ) && 
										 ( !rGraph.nodes[nNodeIndex].szContainerFileName.empty() ) )
								{
									bFilledNodeNotExists = false;
									break;
								}
							}
							
							if ( bFilledNodeNotExists )
							{
								rGraph.nSeason = container.nSeason;
								rGraph.szSeasonFolder = container.szSeasonFolder;
								rGraph.usedScriptIDs = container.usedScriptIDs;
								rGraph.usedScriptAreas = container.usedScriptAreas;
							}
							else
							{
								std::string szCheckResult;
								if ( rGraph.nSeason != container.nSeason )
								{
									std::string szSeason0;
									std::string szSeason1;
									RMGGetSeasonNameString( rGraph.nSeason, rGraph.szSeasonFolder, &szSeason0 );
									RMGGetSeasonNameString( container.nSeason, container.szSeasonFolder, &szSeason1 );
									szCheckResult += NStr::Format( "Invalid Season:\r\nGraph: %s\r\nContainer: %s.\r\n", szSeason0.c_str(), szSeason1.c_str() );
								}
								std::string szStringToCompare0 = rGraph.szSeasonFolder;
								std::string szStringToCompare1 = container.szSeasonFolder;
								NStr::ToLower( szStringToCompare0 );
								NStr::ToLower( szStringToCompare1 );
								if ( szStringToCompare0.compare( szStringToCompare1 ) != 0 )
								{
									szCheckResult += NStr::Format( "Invalid Season Folder:\r\nGraph: <%s>\r\nContainer: <%s>.\r\n", szStringToCompare0.c_str(), szStringToCompare1.c_str() );
								}
								if ( !szCheckResult.empty() )
								{
									CString strTitle;
									strTitle.LoadString( IDR_EDITORTYPE );
									MessageBox( NStr::Format( "Can't Add Container to Graph!\r\nContainer <%s>,\r\nGrapth <%s>,\r\n%s",
																						graphNodePropertiesDialog.szContainerInitialFileName.c_str(),
																						szKey.c_str(),
																						szCheckResult.c_str() ),
															strTitle,
															MB_ICONEXCLAMATION | MB_OK );
									return;
								}
								rGraph.usedScriptIDs.insert( container.usedScriptIDs.begin(), container.usedScriptIDs.end() );
								rGraph.usedScriptAreas.insert( container.usedScriptAreas.begin(), container.usedScriptAreas.end() );
							}
							rGraph.nodes[lastGraphCheckInfo.nNodeIndex].szContainerFileName = graphNodePropertiesDialog.szContainerInitialFileName;	
							SaveGraphFromControls();
							LoadGraphToControls();
						}
					}
				}
			}
		}
	}
}

void CRMGCreateGraphDialog::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized) 
{
	if ( inputState != STATE_NONE )
	{
		inputState = STATE_NONE;
		ClipCursor( &oldClipRect ); 
		ReleaseCapture();
		LoadGraphToControls();
	}
	
	CResizeDialog::OnActivate(nState, pWndOther, bMinimized);
}

void CRMGCreateGraphDialog::OnFileNew() 
{
	SaveGraphsList();
	resizeDialogOptions.szParameters[2] = CG_GRAPHS_FILE_NAME;
	LoadGraphsList();
	UpdateControls();
}

void CRMGCreateGraphDialog::OnFileOpen() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	SaveGraphsList();

	CFileDialog fileDialog( true, ".xml", "", 0, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[1].c_str();
	
	if ( fileDialog.DoModal() == IDOK )
	{
		std::string szFilePath = fileDialog.GetPathName();
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
			resizeDialogOptions.szParameters[1] = szFilePath.substr( 0, nSlashIndex );
		}

		szFilePath = szFilePath.substr( szStorageName.size() );
		resizeDialogOptions.szParameters[2] = szFilePath;
		
		LoadGraphsList();
		UpdateControls();
	}
}

void CRMGCreateGraphDialog::OnFileSave() 
{
	SaveGraphsList();
}

void CRMGCreateGraphDialog::OnFileSaveas() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}


	CFileDialog fileDialog( false, ".xml", "", OFN_OVERWRITEPROMPT, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[1].c_str();
	
	if ( fileDialog.DoModal() == IDOK )
	{
		std::string szFilePath = fileDialog.GetPathName();
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
			resizeDialogOptions.szParameters[1] = szFilePath.substr( 0, nSlashIndex );
		}

		szFilePath = szFilePath.substr( szStorageName.size() );
		resizeDialogOptions.szParameters[2] = szFilePath;
		SaveGraphsList();
	}
}

void CRMGCreateGraphDialog::OnFileExit() 
{
	OnOK();
}

void CRMGCreateGraphDialog::OnCheckGraphsButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	BeginWaitCursor();
	SaveGraphsList();

	std::string szStorageName = pDataStorage->GetName();
	NStr::ToLower( szStorageName );
	for ( CRMGraphsHashMap::iterator graphIterator = graphs.begin(); graphIterator != graphs.end(); ++graphIterator )
	{
		graphIterator->second.nSeason = 0;
		graphIterator->second.szSeasonFolder.clear();
		graphIterator->second.usedScriptIDs.clear();
		graphIterator->second.usedScriptAreas.clear();

		for ( int nLinkIndex = 0; nLinkIndex < graphIterator->second.links.size(); ++nLinkIndex )
		{
			SRMGraphLink &rGraphLink = graphIterator->second.links[nLinkIndex];
			if ( rGraphLink.szDescFileName.find( szStorageName ) == 0 )
			{
				rGraphLink.szDescFileName = rGraphLink.szDescFileName.substr( szStorageName.size() );
				if ( rGraphLink.nParts < 8 )
				{
					rGraphLink.nParts = 8;
				}
			}
		}

		bool bFilledNodeNotExists = true;
		for ( int nNodeIndex = 0; nNodeIndex < graphIterator->second.nodes.size(); ++nNodeIndex )
		{
			if ( graphIterator->second.nodes[nNodeIndex].szContainerFileName.find( szStorageName ) == 0 )
			{
				graphIterator->second.nodes[nNodeIndex].szContainerFileName = graphIterator->second.nodes[nNodeIndex].szContainerFileName.substr( szStorageName.size() );
			}

			if ( !graphIterator->second.nodes[nNodeIndex].szContainerFileName.empty() )
			{
				SRMContainer container;
				if ( !LoadDataResource( graphIterator->second.nodes[nNodeIndex].szContainerFileName, "", false ,0, RMGC_CONTAINER_XML_NAME, container ) )
				{
					CString strTitle;
					strTitle.LoadString( IDR_EDITORTYPE );
					MessageBox( NStr::Format( "Check FAILED!\r\nCan't load Container <%s>\r\nfor Graph <%s>.",
																		graphIterator->second.nodes[nNodeIndex].szContainerFileName.c_str(),
																		graphIterator->first.c_str() ),
											strTitle,
											MB_ICONEXCLAMATION | MB_OK );
					LoadGraphsList();
					EndWaitCursor();
					return;
				}

				std::string szCheckResult;
				if ( ( ( graphIterator->second.nodes[nNodeIndex].rect.Width() / STerrainPatchInfo::nSizeX ) < container.size.x ) ||
						 ( ( graphIterator->second.nodes[nNodeIndex].rect.Height() / STerrainPatchInfo::nSizeY ) < container.size.y ) )
				{
					/**
					szCheckResult += NStr::Format( "Invalid size:\r\nGraph Node: (%d, %d, %d, %d), [%dx%d]\r\nContainer: [%dx%d]",
																					graphIterator->second.nodes[nNodeIndex].rect.minx,
																					graphIterator->second.nodes[nNodeIndex].rect.miny,
																					graphIterator->second.nodes[nNodeIndex].rect.maxx,
																					graphIterator->second.nodes[nNodeIndex].rect.maxy,
																					graphIterator->second.nodes[nNodeIndex].rect.Width(),
																					graphIterator->second.nodes[nNodeIndex].rect.Height(),
																					container.size.x * STerrainPatchInfo::nSizeX,
																					container.size.y * STerrainPatchInfo::nSizeY );
					/**/
				}
				if ( bFilledNodeNotExists )
				{
					graphIterator->second.nSeason = container.nSeason;
					graphIterator->second.szSeasonFolder = container.szSeasonFolder;
					graphIterator->second.usedScriptIDs = container.usedScriptIDs;
					graphIterator->second.usedScriptAreas = container.usedScriptAreas;
					bFilledNodeNotExists = false;
				}
				else
				{
					if ( graphIterator->second.nSeason != container.nSeason )
					{
						std::string szSeason0;
						std::string szSeason1;
						RMGGetSeasonNameString( graphIterator->second.nSeason, graphIterator->second.szSeasonFolder, &szSeason0 );
						RMGGetSeasonNameString( container.nSeason, container.szSeasonFolder, &szSeason1 );
						szCheckResult += NStr::Format( "Invalid Season:\r\nGraph: %s\r\nContainer: %s.\r\n", szSeason0.c_str(), szSeason1.c_str() );
					}
					std::string szStringToCompare0 = graphIterator->second.szSeasonFolder;
					std::string szStringToCompare1 = container.szSeasonFolder;
					NStr::ToLower( szStringToCompare0 );
					NStr::ToLower( szStringToCompare1 );
					if ( szStringToCompare0.compare( szStringToCompare1 ) != 0 )
					{
						szCheckResult += NStr::Format( "Invalid Season Folder:\r\nGraph: <%s>\r\nContainer: <%s>.\r\n", szStringToCompare0.c_str(), szStringToCompare1.c_str() );
					}
					graphIterator->second.usedScriptIDs.insert( container.usedScriptIDs.begin(), container.usedScriptIDs.end() );
					graphIterator->second.usedScriptAreas.insert( container.usedScriptAreas.begin(), container.usedScriptAreas.end() );
				}
				if ( !szCheckResult.empty() )
				{
					CString strTitle;
					strTitle.LoadString( IDR_EDITORTYPE );
					MessageBox( NStr::Format( "Check FAILED!\r\nContainer <%s>,\r\nGrapth <%s>,\r\n%s",
																		graphIterator->second.nodes[nNodeIndex].szContainerFileName.c_str(),
																		graphIterator->first.c_str(),
																		szCheckResult.c_str() ),
											strTitle,
											MB_ICONEXCLAMATION | MB_OK );
					LoadGraphsList();
					EndWaitCursor();
					return;
				}
			}
		}
	}
	SaveGraphsList();
	LoadGraphsList();
	EndWaitCursor();
}
