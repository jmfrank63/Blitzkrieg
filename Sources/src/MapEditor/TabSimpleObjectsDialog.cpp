#include "stdafx.h"
#include "editor.h"
#include "TabSimpleObjectsDialog.h"
#include "frames.h"
#include "SetupFilterDialog.h"
#include "PropertieDialog.h"

#include "TabSimpleObjectsDiplomacyDialog.h"
#include "CreateFilterDialog.h"
#include "mainfrm.h"
#include "TemplateEditorFrame1.h"

#include "..\RandomMapGen\Resource_Types.h"

#include "RMG_FieldObjectPropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

int SFilterItem::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &szName );
	saver.Add( 2, &szFilter );
	
	return 0;
}

int SFilterItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Name", &szName );
	saver.Add( "FilterStr", &szFilter );
	
	return 0;
}

const int CTabSimpleObjectsDialog::vID[] = 
{
	IDC_SO_FILTER_LABEL,					//0
	IDC_SO_FILTER_SHORTCUT_00,		//1
	IDC_SO_FILTER_SHORTCUT_01,		//2
	IDC_SO_FILTER_SHORTCUT_02,		//3
	IDC_SO_FILTER_SHORTCUT_03,		//4
	IDC_SO_FILTER_SHORTCUT_04,		//5
	IDC_SO_FILTER_SHORTCUT_05,		//6
	IDC_SO_FILTER_SHORTCUT_06,		//7
	IDC_SO_FILTER_SHORTCUT_07,		//7
	IDC_SO_FILTER_SHORTCUT_08,		//7
	IDC_SO_FILTER_SHORTCUT_09,		//7
	IDC_SO_FILTER_COMBO,					//8
	IDC_SO_ADD_FILTER_BUTTON,			//9
	IDC_SO_DELETE_FILTER_BUTTON,	//10
	IDC_SO_DELIMITER_00,					//11
	IDC_SO_PLAYER_LABEL,					//12
	IDC_SO_PLAYER_COMBO,					//13
	IDC_SO_DIRECTION_LABEL,				//14
	IDC_SO_DIRECTION_PLACE,				//15		
	IDC_SO_OBJECTS_LIST,					//16
	IDC_SO_DIRECTION_LABEL,				//17
	IDC_SO_DIPLOMACY_BUTTON,			//18
	IDC_SO_LIST_LIST,							//19
	IDC_SO_LIST_ICONS,						//20
	
};

CTabSimpleObjectsDialog::CTabSimpleObjectsDialog( CWnd* pParent )
	: CResizeDialog( CTabSimpleObjectsDialog::IDD, pParent )// , pIML ( 0 )
{
		
	SetControlStyle( IDC_SO_FILTER_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_00, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_01, ANCHORE_HOR_CENTER | ANCHORE_TOP | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_02, ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_03, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_04, ANCHORE_HOR_CENTER | ANCHORE_TOP | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_05, ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_06, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_07, ANCHORE_HOR_CENTER | ANCHORE_TOP | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_08, ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SO_FILTER_SHORTCUT_09, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SO_FILTER_COMBO, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SO_ADD_FILTER_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_TOP );
	SetControlStyle( IDC_SO_DELETE_FILTER_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_TOP );
	SetControlStyle( IDC_SO_DELIMITER_00, ANCHORE_LEFT_TOP |RESIZE_HOR );
	SetControlStyle( IDC_SO_PLAYER_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SO_PLAYER_COMBO, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SO_DIRECTION_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SO_DIRECTION_PLACE, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SO_OBJECTS_LIST, ANCHORE_LEFT_BOTTOM | RESIZE_HOR_VER );
	SetControlStyle( IDC_SO_DIRECTION_LABEL, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SO_LIST_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SO_LIST_ICONS, ANCHORE_LEFT_TOP | RESIZE_HOR );

	m_filters.resize( 6 );
}


void CTabSimpleObjectsDialog::DoDataExchange(CDataExchange* pDX)
{ 
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SO_PLAYER_COMBO, m_players);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_09, m_listCheck);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_08, m_check9);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_07, m_check8);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_06, m_check7);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_05, m_check6);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_04, m_check5);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_03, m_check4);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_02, m_check3);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_01, m_check2);
	DDX_Control(pDX, IDC_SO_FILTER_SHORTCUT_00, m_check1);
	DDX_Control(pDX, IDC_SO_FILTER_COMBO, m_filtersCtrl);
	DDX_Control(pDX, IDC_SO_OBJECTS_LIST, m_imageList);
}

BEGIN_MESSAGE_MAP(CTabSimpleObjectsDialog, CResizeDialog)
	ON_WM_CREATE()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_SO_OBJECTS_LIST, OnItemchangedList1)
	ON_CBN_SELCHANGE(IDC_SO_FILTER_COMBO, OnSelchangeCombo1)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_00, OnCheck0)
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_01, OnCheck1)
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_02, OnCheck2)
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_03, OnCheck3)
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_04, OnCheck4)
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_05, OnCheck5)
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_06, OnCheck6)
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_07, OnCheck7)
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_08, OnCheck8)
	ON_BN_CLICKED(IDC_SO_FILTER_SHORTCUT_09, OnCheck9)
	ON_BN_CLICKED(IDC_SO_DELETE_FILTER_BUTTON, OnButtonDeleteFilter)
	ON_CBN_SELCHANGE(IDC_SO_PLAYER_COMBO, OnSelchangeSoPlayerCombo)
	ON_BN_CLICKED(IDC_SO_LIST_LIST, OnSoListList)
	ON_BN_CLICKED(IDC_SO_LIST_ICONS, OnSoListIcons)
	ON_WM_DESTROY()
	ON_NOTIFY(NM_DBLCLK, IDC_SO_OBJECTS_LIST, OnDblclkObjectsList)
	ON_NOTIFY(NM_RCLICK, IDC_SO_OBJECTS_LIST, OnRclickObjectsList)
	ON_BN_CLICKED(IDC_SO_ADD_FILTER_BUTTON, OnButtonNewFilter)
	ON_BN_CLICKED(IDC_SO_DIPLOMACY_BUTTON, OnDiplomacyButton)
	ON_COMMAND(IDC_SO_OBJECT_PROPERTIES_MENU, OnObjectPropertiesMenu)
END_MESSAGE_MAP()

int CTabSimpleObjectsDialog::OnCreate( LPCREATESTRUCT lpCreateStruct ) 
{
	if ( CResizeDialog::OnCreate(lpCreateStruct) == -1 )
		return -1;

	m_angelButton.Create( NULL, NULL,  WS_CHILD | WS_VISIBLE, CRect(0, 0, 30, 30), this, AFX_IDW_PANE_FIRST, NULL );
	return 0;
}	

bool CTabSimpleObjectsDialog::CommonFilterName( const std::string &rszName )
{
	if ( ( ( rszName.find( "buildings" ) != 0 ) &&
				 ( rszName.find( "objects" ) != 0 ) &&
				 ( rszName.find( "squads" ) != 0 ) &&
				 ( rszName.find( "units" ) != 0 ) ) ||
			 ( rszName.find( "humans" ) != std::string::npos ) ||
			 ( rszName.find( "aviation" ) != std::string::npos ) ||
			 ( rszName.find( "3dcoast" ) != std::string::npos ) ||
			 ( rszName.find( "entrenchment" ) != std::string::npos ) ||
			 ( rszName.find( "tank_pit" ) != std::string::npos ) ||
			 ( rszName.find( "aisingleunitformation" ) != std::string::npos ) )
	{
		return false;
	}
	return true;
}
bool CTabSimpleObjectsDialog::FilterName( const std::string &rszName )
{
	std::string szName = rszName;
	NStr::ToLower( szName );
	if ( !CommonFilterName( szName ) )
	{
		return false;
	}
	CString curFilter;
	if ( m_filtersCtrl.GetCurSel() != -1 && m_listCheck.GetCheck() == 1 )
	{
		m_filtersCtrl.GetLBText( m_filtersCtrl.GetCurSel(), curFilter );	
		TFilterHashMap::const_iterator filterIterator = m_allFilters.find( std::string( curFilter ) );
		if ( filterIterator != m_allFilters.end() )
		{
			if ( filterIterator->second.Check( szName ) )
			{
				return true;
			}
		}
	}
	for( int i = 0; i != m_filters.size(); ++i )
	{
		if ( m_checkButtons[i]->GetCheck() == 1 )
		{
			const SSimpleFilter &rFilter = m_allFilters[ m_filters[i].szFilter ];
			if ( rFilter.Check( szName ) )
			{
				return true;
			}
		}
	}
	return false;
}

BOOL CTabSimpleObjectsDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	CreateObjectsImageList();

	if ( resizeDialogOptions.szParameters.size() < 1 )
	{
		resizeDialogOptions.szParameters.resize( 1, "" );
	}
	if ( resizeDialogOptions.nParameters.size() < 1 )
	{
		resizeDialogOptions.nParameters.resize( 1 );
	}
	
	LoadDataResource( "editor\\filter", "", false, 0, "filters", m_allFilters );
	LoadDataResource( "editor\\filterSetup", "", false, 0, "filters", m_filters );
	m_filters.resize( 9 );

	for( TFilterHashMap::iterator filtersIterator = m_allFilters.begin(); filtersIterator != m_allFilters.end(); ++filtersIterator )
	{
		const std::string szFilter = filtersIterator->first;
		if ( szFilter != "" )
		{
			m_filtersCtrl.AddString( szFilter.c_str() );		
		}
	}

	m_filtersCtrl.SelectString( -1, resizeDialogOptions.szParameters[0].c_str() );

	m_checkButtons.push_back( &m_check1 );
	m_checkButtons.push_back( &m_check2 );
	m_checkButtons.push_back( &m_check3 );
	m_checkButtons.push_back( &m_check4 );
	m_checkButtons.push_back( &m_check5 );
	m_checkButtons.push_back( &m_check6 );
	m_checkButtons.push_back( &m_check7 );
	m_checkButtons.push_back( &m_check8 );
	m_checkButtons.push_back( &m_check9 );
	for( int i = 0; i != m_checkButtons.size(); ++i )
	{
		m_checkButtons[i]->SetWindowText( m_filters[i].szName.c_str() );
	}
	FillPlayers();

	UpdateObjectsListStyle();
	return TRUE;
} 

void CTabSimpleObjectsDialog::OnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if ( pNMListView->uChanged == LVIF_STATE && pNMListView->uNewState == 3 )
	{
		reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 3 );
	}	
	*pResult = 0;
}

int CTabSimpleObjectsDialog::GetDefaultDirAngel() // returns dir angel 0..360 
{
	float angle  = m_angelButton.GetAngle();
	angle = angle < 0 ? ( 2 * PI - fabs( angle ) ) : angle;
	angle = angle > PI / 4 ? angle - PI / 4 : 2 * PI - ( PI / 4 -  angle ) ;
	return  ( angle * 180 ) / PI;
}

int CALLBACK ObjectsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CTabSimpleObjectsDialog* pDialog = reinterpret_cast<CTabSimpleObjectsDialog*>( lParamSort );

	lParam1 = UnpackLowWORD( lParam1 );
	lParam2 = UnpackLowWORD( lParam2 );
	std::string szItem1 = pDialog->m_imageList.GetItemText( lParam1, 0 );
	std::string szItem2 = pDialog->m_imageList.GetItemText( lParam2, 0 );
	return szItem1.compare( szItem2 );
}

void CTabSimpleObjectsDialog::SetObjectsListStyle( bool bPictures )
{
	if ( ::IsWindow( m_imageList.m_hWnd ) )
	{
		if ( bPictures )
		{
			m_imageList.ModifyStyle( LVS_LIST, LVS_ICON );
		}
		else
		{
			m_imageList.ModifyStyle( LVS_ICON, LVS_LIST );
		}
		
		m_imageList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + 
																TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_X,
																TEFConsts::THUMBNAILTILE_HEIGHT +
																TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_Y );
		m_imageList.Arrange( LVA_DEFAULT );
	}
}

bool CTabSimpleObjectsDialog::IsPictures()
{
	if( !resizeDialogOptions.nParameters.empty() )
	{
		return ( resizeDialogOptions.nParameters[0] > 0 );
	}
	return false;
}

void CTabSimpleObjectsDialog::UpdateObjectsListStyle()
{
	if ( !resizeDialogOptions.nParameters.empty() )
	{
		CheckRadioButton( IDC_SO_LIST_LIST, IDC_SO_LIST_ICONS, IDC_SO_LIST_LIST + resizeDialogOptions.nParameters[0] );
		SetObjectsListStyle( resizeDialogOptions.nParameters[0] > 0 );
	}
}

int CTabSimpleObjectsDialog::GetObjectIndex()
{
	const int nIndex = m_imageList.GetNextItem( -1, LVNI_SELECTED );
	if ( nIndex >= 0 )
	{
		const DWORD nData = m_imageList.GetItemData( nIndex );
		return UnpackHighWORD( nData );
	}
	return -1;
}


LRESULT CTabSimpleObjectsDialog::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( message == WM_ANGLE_CHANGED )
	{
		if ( g_frameManager.GetTemplateEditorFrame() )	
		{
			reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 4 );
		}
	}
	return CResizeDialog::WindowProc(message, wParam, lParam);
}

void CTabSimpleObjectsDialog::OnSize(UINT nType, int cx, int cy) 
{
	CResizeDialog::OnSize( nType, cx, cy );

	CWnd *pWnd = GetDlgItem( IDC_SO_DIRECTION_PLACE );
	if( pWnd && ::IsWindow( pWnd->m_hWnd ) )
	{
		CRect rect;
		pWnd->GetWindowRect( &rect );
		ScreenToClient( &rect );
		rect.top += 5;
		rect.left += 5;
		rect.bottom -= 5;
		rect.right  -= 5;
		m_angelButton.MoveWindow( &rect );
	}

	UpdateObjectsListStyle();
}


void CTabSimpleObjectsDialog::OnButtonNewFilter() 
{
	g_frameManager.GetTemplateEditorFrame()->GetFilesInDataStorage();
	CCreateFilterDialog	createFilterDialog;
	createFilterDialog.folders = g_frameManager.GetTemplateEditorFrame()->enumFolderStructureParameter.folders;
	createFilterDialog.filters = m_allFilters;
	if ( createFilterDialog.DoModal() == IDOK )
	{
		m_allFilters = createFilterDialog.filters;
		m_filtersCtrl.ResetContent();
		for( TFilterHashMap::const_iterator filtersIterator = m_allFilters.begin(); filtersIterator != m_allFilters.end(); ++filtersIterator )
		{
			const std::string szFilter = filtersIterator->first;
			if ( szFilter != "" )
			{
				m_filtersCtrl.AddString( szFilter.c_str() );		
			}
		}
		m_filtersCtrl.SelectString( -1, resizeDialogOptions.szParameters[0].c_str() );

		if( theApp.GetMainFrame() )
		{
			theApp.GetMainFrame()->FillRangeFilterComboBox( resizeDialogOptions.szParameters[0], m_allFilters );
		}
	}
}

void CTabSimpleObjectsDialog::OnSelchangeCombo1() 
{
	int nPos = m_filtersCtrl.GetCurSel();
	if ( nPos >= 0 )
	{
		CString strString;
		m_filtersCtrl.GetLBText(nPos, strString );
		resizeDialogOptions.szParameters[0] = strString;
		if( theApp.GetMainFrame() )
		{
			nPos = theApp.GetMainFrame()->pwndFireRangeFilterComboBox->GetCurSel();
			theApp.GetMainFrame()->pwndFireRangeFilterComboBox->GetLBText( nPos, strString );
			if ( strString.Compare( CSetupFilterDialog::SELECTED_UNITS ) != 0 )
			{
				theApp.GetMainFrame()->pwndFireRangeFilterComboBox->SelectString( -1, resizeDialogOptions.szParameters[0].c_str() );
				theApp.GetMainFrame()->OnChangeFireRangeFilter();
			}
			else
			{
				reinterpret_cast<CWnd *>( g_frameManager.GetTemplateEditorFrame() )->SendMessage( WM_USER + 2 );
			}
		}
	}
}

void CTabSimpleObjectsDialog::UpdateCheck( int n )
{
	CSetupFilterDialog dlg;
	dlg.m_allFilters = m_allFilters;
	dlg.m_filterName = m_filters[n].szName.c_str();
	if ( dlg.DoModal() == IDOK )
	{	
		m_filters[n] = SFilterItem( std::string( dlg.m_filterName ), std::string( dlg.m_filterName ) );
		m_checkButtons[n]->SetWindowText( dlg.m_filterName );
	}
}

void CTabSimpleObjectsDialog::OnCheck0() 
{
	if ( ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
	{
			UpdateCheck( 0 );
	}
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );

}

void CTabSimpleObjectsDialog::OnCheck1() 
{
	if ( ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
	{
			UpdateCheck( 1 );
	}
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );

}

void CTabSimpleObjectsDialog::OnCheck2() 
{
	if ( ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
	{
			UpdateCheck( 2 );
	}
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );
}

void CTabSimpleObjectsDialog::OnCheck3() 
{
	if ( ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
	{
			UpdateCheck( 3 );
	}
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );
}

void CTabSimpleObjectsDialog::OnCheck4() 
{
	if ( ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
	{
			UpdateCheck( 4 );
	}
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );
}

void CTabSimpleObjectsDialog::OnCheck5() 
{
	if ( ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
	{
			UpdateCheck( 5 );
	}
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );
}

void CTabSimpleObjectsDialog::OnCheck6() 
{
	if ( ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
	{
			UpdateCheck( 6 );
	}
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );
}

void CTabSimpleObjectsDialog::OnCheck7() 
{
	if ( ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
	{
			UpdateCheck( 7 );
	}
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );
}
void CTabSimpleObjectsDialog::OnCheck8() 
{
	if ( ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
	{
			UpdateCheck( 8 );
	}
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );
}

void CTabSimpleObjectsDialog::OnCheck9() 
{
	reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 2 );
}

void CTabSimpleObjectsDialog::OnButtonDeleteFilter() 
{
	if( m_filtersCtrl.GetCurSel() != -1 )
	{
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );
		if ( MessageBox( "Do you really want to DELETE selected filter?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
		{
			CString curFilter;
			m_filtersCtrl.GetLBText( m_filtersCtrl.GetCurSel(), curFilter );
			if( m_allFilters.find( std::string( curFilter ) ) != m_allFilters.end() )
			{
				m_allFilters.erase( std::string( curFilter ) );
				m_filtersCtrl.DeleteString( m_filtersCtrl.GetCurSel() );
				for( int i = 0; i != m_checkButtons.size(); ++i )
				{
					if( m_allFilters.find( m_filters[i].szFilter ) == m_allFilters.end() )
					{
						m_filters[i].szFilter = "";
						m_filters[i].szName = "";
						m_checkButtons[i]->SetWindowText( "" );
					}
				}
			}
			resizeDialogOptions.szParameters[0].clear();
			m_filtersCtrl.SelectString( -1, resizeDialogOptions.szParameters[0].c_str() );
			if( theApp.GetMainFrame() )
			{
				theApp.GetMainFrame()->FillRangeFilterComboBox( resizeDialogOptions.szParameters[0], m_allFilters );
			}
		}
	}
}

void CTabSimpleObjectsDialog::FillPlayers()
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	std::string szOldSelection;
	if ( m_players.GetCount() > 0 )
	{
		CString strBuffer;
		m_players.GetWindowText( strBuffer );
		szOldSelection = strBuffer;
	}
	m_players.ResetContent();
	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		for ( int nPlayerIndex = 0; nPlayerIndex < pFrame->currentMapInfo.diplomacies.size(); ++nPlayerIndex )
		{
			int nStringNumber = m_players.AddString( NStr::Format( "%2d", nPlayerIndex ) );
			m_players.SetItemData( nStringNumber, nPlayerIndex );
		}
		
		if ( m_players.GetCount() > 0 )
		{
			if ( !szOldSelection.empty() )
			{
				m_players.SelectString( -1, szOldSelection.c_str() );
			}
			else
			{
				m_players.SetCurSel( 0 );
			}
			
			if ( theApp.GetMainFrame() )
			{
				theApp.GetMainFrame()->FillPlayerNumbers( szOldSelection );
			}
		}
	}
}

void CTabSimpleObjectsDialog::OnDiplomacyButton() 
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		if ( pFrame->dlg ) 
		{
			pFrame->dlg->DestroyWindow();
			delete pFrame->dlg;
			pFrame->dlg = 0;
			pFrame->isStartCommandPropertyActive = false;
		}

		CTabSimpleObjectsDiplomacyDialog diplomacyDialog;
		diplomacyDialog.diplomacies = pFrame->currentMapInfo.diplomacies;
		diplomacyDialog.SetType( pFrame->currentMapInfo.nType );
		diplomacyDialog.SetAttackingSide( pFrame->currentMapInfo.nAttackingSide );
		if ( diplomacyDialog.DoModal() == IDOK )
		{
			pFrame->currentMapInfo.diplomacies = diplomacyDialog.diplomacies;
			FillPlayers();
			pFrame->MakeCamera();
			pFrame->m_unitCreationInfo.Resize( pFrame->currentMapInfo.diplomacies.size() - 1 );
			pFrame->m_unitCreationInfo.MutableValidate();
			pFrame->currentMapInfo.nType = diplomacyDialog.GetType();
			pFrame->currentMapInfo.nAttackingSide = diplomacyDialog.GetAttackingSide();
			pFrame->ResetPlayersForFlags();
			pFrame->SetMapModified();
		}
	}
}

void CTabSimpleObjectsDialog::UpdateControls()
{
	CWnd* pWnd = 0;
	
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	bool bFrameExists = ( pFrame && pAIEditor && pIDB && pTerrain );

	/**
	if ( pWnd = GetDlgItem( IDC_SO_DIPLOMACY_BUTTON ) )
	{
		pWnd->EnableWindow( bFrameExists );
	}
	/**/
}

int CTabSimpleObjectsDialog::GetPlayer()
{
	int nPlayer = m_players.GetCurSel();
	if ( nPlayer >= 0 )
	{
		return m_players.GetItemData( nPlayer );
	}
	else
	{
		return 0;
	}
}

void CTabSimpleObjectsDialog::OnSelchangeSoPlayerCombo() 
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->ShowStorageCoverage();
		if ( theApp.GetMainFrame() && theApp.GetMainFrame()->pwndPlayerNumberComboBox )
		{
			theApp.GetMainFrame()->pwndPlayerNumberComboBox->SelectString( -1, NStr::Format( "%2d", GetPlayer() ) );
		}
	}
}

void CTabSimpleObjectsDialog::CreateObjectsImageList()
{
	DWORD dwTime = GetTickCount();

	objectsImageList.DeleteImageList();
	objectsImageIndices.clear();

	if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
	{
		if ( CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>() )
		{
			if ( CPtr<IImageProcessor> pImageProseccor = GetImageProcessor() )
			{
				m_imageList.DeleteAllItems();
				UpdateObjectsListStyle();

				const COLORREF zeroColor = RGB( 0, 0, 0 );
				CBitmap defaultObjectBitmap;
				defaultObjectBitmap.LoadBitmap( IDB_DEFAULT_OBJECT_IMAGE );
				
				CBitmap objectBitmap;

				int nImageCount = pODB->GetNumDescs();

				objectsImageList.Create( TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT, ILC_COLOR24, 0, 10 );
				int nImageAdded = 0;
				{
					const int nImageAddedIndex = objectsImageList.Add( &defaultObjectBitmap, zeroColor );
					NI_ASSERT_T( nImageAddedIndex >= 0, NStr::Format( "CTabSimpleObjectsDialog::CreateObjectsImageList(), Can't add image to image list, defaultObjectBitmap" ) );
					++nImageAdded;
				}
				m_imageList.SetImageList( &objectsImageList, LVSIL_NORMAL );

				const SGDBObjectDesc *pDescriptions = pODB->GetAllDescs(); 
				for ( int nObjectIndex = 0; nObjectIndex < nImageCount; ++nObjectIndex ) 
				{
					std::string szName = pDescriptions[nObjectIndex].szPath;
					NStr::ToLower( szName );
					if ( CTabSimpleObjectsDialog::CommonFilterName( szName ) )
					{
						bool bBitmapNotSet = true;
						int nImageAddedIndex = 0;
						if ( CPtr<IDataStream> pDataStream = pDataStorage->OpenStream( ( pDescriptions[nObjectIndex].szPath + "\\icon.tga" ).c_str(), STREAM_ACCESS_READ ) )
						{
							if ( CPtr<IImage> pImage = pImageProseccor->LoadImage( pDataStream ) ) 
							{
								if ( ( TEFConsts::THUMBNAILTILE_WIDTH != pImage->GetSizeX() ) ||
										 ( TEFConsts::THUMBNAILTILE_HEIGHT != pImage->GetSizeY() ) )
								{
									CPtr<IImage> pScaleImage = pImageProseccor->CreateScaleBySize( pImage, TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT, ISM_LANCZOS3 );
									pImage = pScaleImage;
								}
								objectBitmap.DeleteObject();
								
								BITMAPINFO bmi;
								bmi.bmiHeader.biSize = sizeof( bmi.bmiHeader );
								bmi.bmiHeader.biWidth = pImage->GetSizeX();
								bmi.bmiHeader.biHeight = -pImage->GetSizeY();
								bmi.bmiHeader.biPlanes = 1;
								bmi.bmiHeader.biBitCount = 32;
								bmi.bmiHeader.biCompression = BI_RGB;
								bmi.bmiHeader.biSizeImage = 0;
								bmi.bmiHeader.biClrUsed = 0;
								
								CDC *pDC = GetDC();
								HBITMAP hbm = CreateCompatibleBitmap( pDC->m_hDC, pImage->GetSizeX(), pImage->GetSizeY() );
								::SetDIBits( pDC->m_hDC, hbm, 0, pImage->GetSizeY(), pImage->GetLFB(), &bmi, DIB_RGB_COLORS );
								ReleaseDC( pDC );
								objectBitmap.Attach( hbm );
								nImageAddedIndex = objectsImageList.Add( &objectBitmap, zeroColor );
								if ( nImageAddedIndex >= 0 )
								{
									bBitmapNotSet = false;
								}
							}
						}
						if ( bBitmapNotSet )
						{
							nImageAddedIndex = objectsImageList.Add( &defaultObjectBitmap, zeroColor );
							NStr::DebugTrace( "%s\n", pDescriptions[nObjectIndex].szKey.c_str() );
							NI_ASSERT_T( nImageAddedIndex >= 0, NStr::Format( "CTabSimpleObjectsDialog::CreateObjectsImageList, Can't add image to image list, %s, ", pDescriptions[nObjectIndex].szKey.c_str() ) );
						}
						objectsImageIndices[pDescriptions[nObjectIndex].szKey] = nImageAddedIndex;
						++nImageAdded;
					}
				}
				objectsImageList.SetImageCount( nImageAdded );
			}
		}
	}
	dwTime = GetTickCount() - dwTime;
	NStr::DebugTrace( "CTabSimpleObjectsDialog::CreateObjectsImageList(): %d ms\n", dwTime );
}

void CTabSimpleObjectsDialog::OnSoListList() 
{
	resizeDialogOptions.nParameters[0] = 0;
	UpdateObjectsListStyle();
}

void CTabSimpleObjectsDialog::OnSoListIcons() 
{
	resizeDialogOptions.nParameters[0] = 1;
	UpdateObjectsListStyle();
}

void CTabSimpleObjectsDialog::OnDestroy() 
{
	SaveDataResource( "editor\\filter", "", false, 0, "filters", m_allFilters );
	SaveDataResource( "editor\\filterSetup", "", false, 0, "filters", m_filters );

	CResizeDialog::SaveResizeDialogOptions();
	CResizeDialog::OnDestroy();
	objectsImageList.DeleteImageList();
	objectsImageIndices.clear();
}

void CTabSimpleObjectsDialog::DeleteImageList()
{
	objectsImageList.DeleteImageList();
	objectsImageIndices.clear();
	m_imageList.DeleteAllItems();
}

void CTabSimpleObjectsDialog::CreateImageList()
{
	CreateObjectsImageList();
	reinterpret_cast<CWnd *>( g_frameManager.GetTemplateEditorFrame() )->SendMessage( WM_USER + 2 );
}

void CTabSimpleObjectsDialog::OnDblclkObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	ShowObjectProperties();
	*pResult = 0;
}

void CTabSimpleObjectsDialog::OnRclickObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu tabsMenu;
	tabsMenu.LoadMenu( IDM_TAB_POPUP_MENUS );
	CMenu *pMenu = tabsMenu.GetSubMenu( 5 );
	if ( pMenu )
	{
		pMenu->EnableMenuItem( IDC_SO_OBJECT_PROPERTIES_MENU, ( m_imageList.GetSelectedCount() > 0 ) ? MF_ENABLED : MF_GRAYED );
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	tabsMenu.DestroyMenu();
	*pResult = 0;
}

void CTabSimpleObjectsDialog::OnObjectPropertiesMenu() 
{
	ShowObjectProperties();
}

void CTabSimpleObjectsDialog::ShowObjectProperties()
{
	if ( m_imageList.GetSelectedCount() > 0 )
	{
		int nSelectedItem = m_imageList.GetNextItem( CB_ERR, LVNI_SELECTED );
		if (	nSelectedItem != CB_ERR )
		{
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				if ( CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>() )
				{
					const std::string szSelectedObjectName = m_imageList.GetItemText( nSelectedItem, 0 );
					if ( objectsImageIndices.find( szSelectedObjectName ) != objectsImageIndices.end() )
					{
						if ( const SGDBObjectDesc *pObjectDesc = pODB->GetDesc( szSelectedObjectName.c_str() ) )
						{
							CRMGFieldObjectPropertiesDialog fieldObjectPropertiesDialog;
							fieldObjectPropertiesDialog.bDisableEditWeight = true;

							fieldObjectPropertiesDialog.m_szStats = CRMGFieldObjectPropertiesDialog::GetObjectStats( pODB, pObjectDesc ).c_str();
							fieldObjectPropertiesDialog.m_szName = szSelectedObjectName.c_str();
							fieldObjectPropertiesDialog.m_szPath = pObjectDesc->szPath.c_str();
							fieldObjectPropertiesDialog.hIcon = objectsImageList.ExtractIcon( objectsImageIndices[szSelectedObjectName] );

							if ( ( pObjectDesc->eVisType >= 0) && ( pObjectDesc->eVisType < CRMGFieldObjectPropertiesDialog::VIS_TYPES_COUNT ) )
							{
								fieldObjectPropertiesDialog.m_szVisType = CRMGFieldObjectPropertiesDialog::VIS_TYPES[pObjectDesc->eVisType];
							}
							else
							{
								fieldObjectPropertiesDialog.m_szVisType = CRMGFieldObjectPropertiesDialog::VIS_TYPES[0];
							}
							if ( ( pObjectDesc->eGameType >= 0) && ( pObjectDesc->eGameType < CRMGFieldObjectPropertiesDialog::GAME_TYPES_COUNT ) )
							{
								fieldObjectPropertiesDialog.m_szGameType = CRMGFieldObjectPropertiesDialog::GAME_TYPES[pObjectDesc->eGameType];
							}
							else
							{
								fieldObjectPropertiesDialog.m_szGameType = CRMGFieldObjectPropertiesDialog::GAME_TYPES[0];
							}
							fieldObjectPropertiesDialog.DoModal();
						}
					}
				}
			}
		}
	}
}
