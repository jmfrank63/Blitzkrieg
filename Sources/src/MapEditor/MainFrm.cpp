#include "stdafx.h"
#include "editor.h"
#include "resource.h"

#include "MainFrm.h"
#include "frames.h"
#include "..\Main\GameDB.h"
#include "..\Main\iMain.h"
#include "..\GameTT\iMission.h"

#include "TemplateEditorFrame1.h"
#include "SetupFilterDialog.h"

#include "htmlhelp.h"

#include "..\RandomMapGen\RMG_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\Misc\FileUtils.h"

#include "ProgressDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

int GAME_SIZE_X = 1024;
int GAME_SIZE_Y = 768;

#define IDC_TOOLBAR_LARGEBTNS           43103

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static UINT BASED_CODE EDIT_BUTTONS[] =
{
	ID_FILE_CREATENEWPROJECT,
	ID_FILE_OPEN,
	ID_FILE_SAVE,
	ID_SEPARATOR,
	ID_FILE_SAVE_XML,
	ID_FILE_SAVE_BZM,
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static UINT BASED_CODE COMBO_BUTTONS[] =
{
	ID_BUTTONFILLAREA,
	ID_BRUSH,
	ID_SEPARATOR,
	ID_SHOW_FIRE_RANGE,
	ID_SHOW_FIRE_RANGE_FILTER,
	ID_SEPARATOR,
	ID_BUTTONSETCAMERA,
	//ID_SHOW_STORAGE_COVERAGE,
	ID_PLAYER_NUMBER,
};

static UINT BASED_CODE MAP_BUTTONS[] =
{
	ID_DIPLOMACY,
	ID_EDIT_UNIT_CREATION_INFO,
	ID_OPTIONS,
	ID_SEPARATOR,
	ID_TOOLS_DELR,
	ID_BUTTONUPDATE,
	ID_SHOW_SCENE_14,
	ID_SEPARATOR,
	ID_BUTTONFIT,
};

static UINT BASED_CODE WINDOW_BUTTONS[] =
{
	ID_VIEW_LEFT_BAR,
	ID_SHOWMINIMAP,
	ID_TOOLS_RUN_GAME,
};

static UINT BASED_CODE VIEW_BUTTONS[] =
{
	ID_BUTTONAI,
	ID_SEPARATOR,
	ID_SHOW_SCENE_6,
	ID_SHOW_SCENE_7,
	ID_SHOW_SCENE_10,
	ID_SHOW_SCENE_9,
	ID_SHOW_SCENE_11,	
	ID_SHOW_SCENE_13,
	ID_SEPARATOR,
	ID_SHOW_SCENE_1,
	ID_SHOW_SCENE_2,
	ID_SHOW_SCENE_3,
	ID_SHOW_SCENE_4,
	ID_SHOW_SCENE_0,
	ID_SEPARATOR,
	ID_SHOW_SCENE_12,
};

static UINT BASED_CODE TOOLS_BUTTONS[] =
{
	ID_ADD_START_COMMANDS,
	ID_START_COMMANDS_LIST,
	ID_SEPARATOR,
	ID_RESERVE_POSITIONS,
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_BUTTON_MAP(COMMON_BUTTON_MAP)
	COMBO_BUTTON(ID_BRUSH, IDC_BRUSHSIZE, SEC_TBBS_VCENTER, CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | WS_VSCROLL, 75, 40, 120 )
	STD_BUTTON( ID_SHOW_FIRE_RANGE, TBBS_CHECKBOX )
	COMBO_BUTTON(ID_SHOW_FIRE_RANGE_FILTER, IDC_SHOW_FIRE_RANGE_FILTER, SEC_TBBS_VCENTER, CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | CBS_SORT | WS_VSCROLL, 160, 40, 120 )
	COMBO_BUTTON(ID_PLAYER_NUMBER, IDC_PLAYER_NUMBER, SEC_TBBS_VCENTER, CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | CBS_SORT | WS_VSCROLL, 50, 40, 120 )
	STD_BUTTON( ID_SHOW_SCENE_14, TBBS_CHECKBOX )
	STD_BUTTON( ID_BUTTONFIT, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_6, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_7, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_10, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_11, TBBS_CHECKBOX	)
	STD_BUTTON( ID_SHOW_SCENE_13, TBBS_CHECKBOX	)
	STD_BUTTON( ID_SHOW_SCENE_1, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_2, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_3, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_4, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_8, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_0, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_9, TBBS_CHECKBOX )
	STD_BUTTON( ID_BUTTONAI, TBBS_CHECKBOX )
	//STD_BUTTON( ID_SHOW_STORAGE_COVERAGE, TBBS_CHECKBOX )
	STD_BUTTON( ID_SHOW_SCENE_12, TBBS_CHECKBOX )
	STD_BUTTON( ID_RESERVE_POSITIONS, TBBS_CHECKBOX )
END_BUTTON_MAP()

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int wmAppToolBarWndNotify = RegisterWindowMessage( _T( "WM_SECTOOLBARWNDNOTIFY" ) );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CMainFrame, SECWorkbook)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMainFrame, SECWorkbook)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_COMMAND(ID_TOOLS_CUSTOMIZE, OnToolsCustomize)
	ON_WM_CREATE()
	ON_REGISTERED_MESSAGE(wmAppToolBarWndNotify, OnCreateCombo)
	ON_CBN_SELCHANGE( IDC_BRUSHSIZE, OnChangeBrushSize)
	ON_CBN_SELCHANGE( IDC_PLAYER_NUMBER, OnChangePlayerNumber)
	ON_WM_CLOSE()
	ON_UPDATE_COMMAND_UI(ID_HELP, OnUpdateHelp)
	ON_COMMAND(ID_VIEW_LEFT_BAR, OnViewLeftBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LEFT_BAR, OnUpdateViewLeftBar)
	ON_COMMAND(ID_VIEW_TOOLBAR_0, OnViewToolbar0)
	ON_COMMAND(ID_VIEW_TOOLBAR_1, OnViewToolbar1)
	ON_COMMAND(ID_VIEW_TOOLBAR_2, OnViewToolbar2)
	ON_COMMAND(ID_VIEW_TOOLBAR_3, OnViewToolbar3)
	ON_COMMAND(ID_VIEW_TOOLBAR_4, OnViewToolbar4)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR_0, OnUpdateViewToolbar0)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR_1, OnUpdateViewToolbar1)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR_2, OnUpdateViewToolbar2)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR_3, OnUpdateViewToolbar3)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR_4, OnUpdateViewToolbar4)
	ON_UPDATE_COMMAND_UI(ID_SHOWMINIMAP, OnUpdateShowminimap)
	ON_COMMAND(ID_SHOWMINIMAP, OnShowminimap)
	ON_COMMAND(ID_TOOL_0, OnTool0)
	ON_COMMAND(ID_TOOL_1, OnTool1)
	ON_COMMAND(ID_TOOL_2, OnTool2)
	ON_COMMAND(ID_TOOL_3, OnTool3)
	ON_COMMAND(ID_TOOL_4, OnTool4)
	ON_WM_DROPFILES()
	ON_COMMAND(ID_HELP, OnHelp)
	ON_CBN_SELCHANGE( IDC_SHOW_FIRE_RANGE_FILTER, OnChangeFireRangeFilter )
	ON_WM_COPYDATA()
	//}}AFX_MSG_MAP
	ON_COMMAND_RANGE(ID_RECENT_MAP_0, ID_RECENT_MAP_9, OnRecentMap)
	ON_UPDATE_COMMAND_UI(ID_RECENT_MAP_0, OnUpdateRecentMap)
	ON_UPDATE_COMMAND_UI_RANGE(ID_RECENT_MAP_0, ID_RECENT_MAP_9, OnUpdateRecentMapRange)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static UINT indicators[] =
{
	ID_SEPARATOR,
	ID_INDICATOR_TILEPOS,
	ID_INDICATOR_OBJECTTYPE,
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMainFrame::CMainFrame()
 : pwndBrushSizeComboBox  ( 0 ), pwndFireRangeFilterComboBox ( 0 ), pwndPlayerNumberComboBox( 0 ), nFireRangeRegisterGroup( -1 ), bFixedDimensions( false )
{
	m_pControlBarManager = new SECToolBarManager(this);
	m_pMenuBar = new SECMDIMenuBar;
	
	EnableBmpMenus();
}

CMainFrame::~CMainFrame()
{
	if ( m_pControlBarManager )
	{
		delete m_pControlBarManager;
	}
	if ( m_pMenuBar )
	{
		delete m_pMenuBar;
	}
	NMain::Finalize();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMainFrame::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	if ( SECWorkbook::OnCreate( lpCreateStruct ) == -1 )
	{
		return -1;
	}

	{
		char pBuffer[0xFFF + 1];
		::GetCurrentDirectory( 0xFFF, pBuffer );
		szHelpFilePath = std::string( pBuffer ) + std::string( "\\mapEditor.chm" );
	}
	
	SECToolBarManager* pToolBarMgr = static_cast<SECToolBarManager*>( m_pControlBarManager );	
	VERIFY( pToolBarMgr->LoadToolBarResource( MAKEINTRESOURCE( IDT_EDIT_BUTTONS ), MAKEINTRESOURCE( IDT_EDIT_BUTTONS ) ) );
	VERIFY( pToolBarMgr->AddToolBarResource( MAKEINTRESOURCE( IDT_TOOLS_BUTTONS ), MAKEINTRESOURCE( IDT_TOOLS_BUTTONS ) ) );
	
	CString strToolbarName;
	strToolbarName.LoadString( IDS_TOOLBAR_FILE );
	pToolBarMgr->DefineDefaultToolBar( AFX_IDW_TOOLBAR,
																		 strToolbarName,
																		 NUMELEMENTS( EDIT_BUTTONS ),
																		 EDIT_BUTTONS,
																		 CBRS_ALIGN_ANY,
																		 AFX_IDW_DOCKBAR_TOP );
	
	strToolbarName.LoadString( IDS_TOOLBAR_SETTINGS );
	pToolBarMgr->DefineDefaultToolBar( AFX_IDW_TOOLBAR + 5, 
																		 strToolbarName,
																		 NUMELEMENTS( COMBO_BUTTONS ),
																		 COMBO_BUTTONS,
																		 CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM,
																		 AFX_IDW_DOCKBAR_TOP,
																		 AFX_IDW_TOOLBAR );

	strToolbarName.LoadString( IDS_TOOLBAR_MAP );
	pToolBarMgr->DefineDefaultToolBar( AFX_IDW_TOOLBAR + 6, 
																		 strToolbarName,
																		 NUMELEMENTS( MAP_BUTTONS ),
																		 MAP_BUTTONS,
																		 CBRS_ALIGN_ANY,
																		 AFX_IDW_DOCKBAR_TOP,
																		 AFX_IDW_TOOLBAR + 5 );

	strToolbarName.LoadString( IDS_TOOLBAR_LAYERS );
	pToolBarMgr->DefineDefaultToolBar( AFX_IDW_TOOLBAR + 7, 
																		 strToolbarName,
																		 NUMELEMENTS( VIEW_BUTTONS ),
																		 VIEW_BUTTONS,
																		 CBRS_ALIGN_ANY,
																		 AFX_IDW_DOCKBAR_TOP,
																		 AFX_IDW_TOOLBAR + 6 );

	strToolbarName.LoadString( IDS_TOOLBAR_UNIT );
	pToolBarMgr->DefineDefaultToolBar( AFX_IDW_TOOLBAR + 8, 
																		 strToolbarName,
																		 NUMELEMENTS( TOOLS_BUTTONS ),
																		 TOOLS_BUTTONS,
																		 CBRS_ALIGN_ANY,
																		 AFX_IDW_DOCKBAR_TOP,
																		 AFX_IDW_TOOLBAR + 7 );

	strToolbarName.LoadString( IDS_TOOLBAR_VIEW );
	pToolBarMgr->DefineDefaultToolBar( AFX_IDW_TOOLBAR + 9, 
																		 strToolbarName,
																		 NUMELEMENTS( WINDOW_BUTTONS ),
																		 WINDOW_BUTTONS,
																		 CBRS_ALIGN_ANY,
																		 AFX_IDW_DOCKBAR_TOP,
																		 AFX_IDW_TOOLBAR + 8 );

	pToolBarMgr->SetButtonMap( COMMON_BUTTON_MAP );
	pToolBarMgr->EnableCoolLook( TRUE );

	if ( !m_wndStatusBar.Create( this ) || !m_wndStatusBar.SetIndicators( indicators, sizeof( indicators ) / sizeof( UINT ) ) )
	{
		return -1;
	}

	int nCoordsIndex = m_wndStatusBar.CommandToIndex( ID_INDICATOR_TILEPOS );
	m_wndStatusBar.SetPaneInfo( nCoordsIndex, ID_INDICATOR_TILEPOS, SBPS_NORMAL, 350 );
	
	nCoordsIndex = m_wndStatusBar.CommandToIndex( ID_INDICATOR_OBJECTTYPE );
	m_wndStatusBar.SetPaneInfo( nCoordsIndex, ID_INDICATOR_OBJECTTYPE, SBPS_NORMAL, 500 );

	pToolBarMgr->SetMenuInfo( 1,IDR_EDITORTYPE );
	EnableDocking( CBRS_ALIGN_ANY );
	pToolBarMgr->SetDefaultDockState();

	if ( bFixedDimensions )
	{
		GAME_SIZE_X = 1024;
		GAME_SIZE_Y = 768;
	}
	else
	{
		const int nXScreenSize = ::GetSystemMetrics( SM_CXSCREEN );
		const int nYScreenSize = ::GetSystemMetrics( SM_CYSCREEN );

		GAME_SIZE_X = nXScreenSize - 4;
		GAME_SIZE_Y = nYScreenSize - 20;
	}

	NStr::DebugTrace( "CMainFrame::OnCreate() GAME_SIZE: ( %d, %d )\n", GAME_SIZE_X, GAME_SIZE_Y );

	DWORD dwTime = ::GetTickCount();
	if ( !wndGameWnd.Create( 0, 0, WS_CHILD | WS_VISIBLE, CRect( 0, 0, GAME_SIZE_X, GAME_SIZE_Y ), this, AFX_IDW_PANE_FIRST, NULL ) )
	{
		return -1;
	}
	dwTime = ::GetTickCount() - dwTime;
	NStr::DebugTrace( "CMainFrame::OnCreate() GameWnd created: %d ms\n", dwTime );
	
	dwTime = ::GetTickCount();
	InitGameWindow();
	g_frameManager.SetGameWnd( &wndGameWnd );
	wndGameWnd.ShowWindow( SW_HIDE );
	if ( ICamera *pCamera = GetSingleton<ICamera>() )
	{
		pCamera->SetPlacement( VNULL3, 700, -ToRadian( 90.0f + 30.0f ), ToRadian( 45.0f ) );
	}
	GetSingleton<IRandomGen>()->Init();
	dwTime = ::GetTickCount() - dwTime;
	NStr::DebugTrace( "CMainFrame::OnCreate() GameWnd initialized: %d ms\n", dwTime );


	dwTime = ::GetTickCount();
	if ( CreateTemplateEditorFrame() )
	{
		return -1;
	}
	dwTime = ::GetTickCount() - dwTime;
	NStr::DebugTrace( "CMainFrame::OnCreate() EditorWnd created: %d ms\n", dwTime );
	
	dwTime = ::GetTickCount();
	CString strRegistryKeyName;
	strRegistryKeyName.LoadString( IDS_WINDOWBAR_KEY );
	LoadBarState( strRegistryKeyName );

	FillBrushSize();
	if ( CTabSimpleObjectsDialog* pWnd = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd() )
	{
		FillRangeFilterComboBox( pWnd->resizeDialogOptions.szParameters[0], pWnd->m_allFilters, false );
	}
	
  DragAcceptFiles( true );
	g_frameManager.GetTemplateEditorFrame()->m_cursorName = IDC_ARROW;
	g_frameManager.GetTemplateEditorFrame()->RedrawWindow();

	editorWindowSingletonApp.CreateMapFile( GetSafeHwnd() );
	dwTime = ::GetTickCount() - dwTime;
	NStr::DebugTrace( "CMainFrame::OnCreate() EditorWnd initialized: %d ms\n", dwTime );
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMainFrame::CreateTemplateEditorFrame()
{
  CMDIChildWnd *pChildWnd = 0;
  pChildWnd = CreateNewChild( RUNTIME_CLASS( CTemplateEditorFrame ), IDR_TEMPLATE_EDITOR, NULL, hMDIAccel );
  pChildWnd->MDIMaximize();
  pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	
	g_frameManager.GetTemplateEditorFrame()->Init( GetSingleton<IGFX>() );
	int nID104 = SECControlBar::GetUniqueBarID( this, 104 );
	int nID105 = SECControlBar::GetUniqueBarID( this, 105 );

	DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_RIGHT | CBRS_TOOLTIPS | CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL  | CBRS_EX_BORDERSPACE;
	if ( !wndInputControlBar.Create( this, _T( "Workspace Bar" ), dwStyle, dwStyleEx, nID104, NULL ) )
	{
		return -1;
	}
	wndMiniMapBar.SetMainFrameWindow( g_frameManager.GetTemplateEditorFrame() );
	if ( !wndMiniMapBar.Create( this, _T( "MiniMap Bar" ), dwStyle, dwStyleEx, nID105, NULL ) )
	{
		return -1;
	}
	wndMiniMapBar.EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx( &wndMiniMapBar, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.35f, 270 );
	wndInputControlBar.EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx( &wndInputControlBar, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.65f, 270 );

	g_frameManager.GetTemplateEditorFrame()->SetMapEditorBar( &wndInputControlBar );
	g_frameManager.GetTemplateEditorFrame()->SetTabTileEditDialog( wndInputControlBar.GetTabTileEditDialog() );
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnToolsCustomize() 
{
	SECToolBarsPage toolbarPage;
	toolbarPage.SetManager( dynamic_cast<SECToolBarManager*>( m_pControlBarManager ) );

	SECToolBarCmdPage cmdPage( SECToolBarCmdPage::IDD, IDS_COMMAND );
	cmdPage.SetManager( dynamic_cast<SECToolBarManager*>( m_pControlBarManager ) );
	
	CString strToolbarName;
	strToolbarName.LoadString( IDS_TOOLBAR_FILE );
	cmdPage.DefineBtnGroup( strToolbarName, NUMELEMENTS( EDIT_BUTTONS ), EDIT_BUTTONS );
	strToolbarName.LoadString( IDS_TOOLBAR_SETTINGS );
	cmdPage.DefineBtnGroup( strToolbarName, NUMELEMENTS( COMBO_BUTTONS ), COMBO_BUTTONS );
	strToolbarName.LoadString( IDS_TOOLBAR_MAP );
	cmdPage.DefineBtnGroup( strToolbarName, NUMELEMENTS( MAP_BUTTONS ), MAP_BUTTONS );
	strToolbarName.LoadString( IDS_TOOLBAR_LAYERS );
	cmdPage.DefineBtnGroup( strToolbarName, NUMELEMENTS( VIEW_BUTTONS ), VIEW_BUTTONS );
	strToolbarName.LoadString( IDS_TOOLBAR_UNIT );
	cmdPage.DefineBtnGroup( strToolbarName, NUMELEMENTS( TOOLS_BUTTONS ), TOOLS_BUTTONS );
	strToolbarName.LoadString( IDS_TOOLBAR_VIEW );
	cmdPage.DefineBtnGroup( strToolbarName, NUMELEMENTS( WINDOW_BUTTONS ), WINDOW_BUTTONS );
	strToolbarName.LoadString( IDS_TOOLBAR_MENU );
	cmdPage.DefineMenuGroup( strToolbarName );

	SECToolBarSheet toolbarSheet;
	toolbarSheet.AddPage( &toolbarPage );
	toolbarSheet.AddPage( &cmdPage );

	toolbarSheet.DoModal();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMainFrame::InitGameWindow()
{
	int nSizeX = GAME_SIZE_X;
	int nSizeY = GAME_SIZE_Y;

	CPtr<IDataStorage> pStorage = OpenStorage( ".\\data\\*.pak", STREAM_ACCESS_READ, STORAGE_TYPE_MOD );
	RegisterSingleton( IDataStorage::tidTypeID, pStorage );
	// CRAP{ load game database
	{
		CPtr<IObjectsDB> pODB = CreateObjectsDB();
		pODB->LoadDB();
		RegisterSingleton( IObjectsDB::tidTypeID, pODB );
		GetSLS()->SetGDB( pODB );
	}
	// CRAP} 
	//
	// load constants and set global vars from it
	{
		CTableAccessor table = NDB::OpenDataTable( "consts.xml" );
		NMain::SetupGlobalVarConsts( table );
	}
	// set global var to enable overdraw to fix shadows
	SetGlobalVar( "overdraw", 1 );
	// initialize all game system
	NMain::Initialize( wndGameWnd.GetSafeHwnd(), AfxGetMainWnd()->GetSafeHwnd(), AfxGetMainWnd()->GetSafeHwnd(), false );

	{
		// load key bindings
		if ( CPtr<IDataStream> pStream = OpenFileStream(".\\config.cfg", STREAM_ACCESS_READ) )
		{
			CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
			GetSingleton<IInput>()->SerializeConfig( pDT );
		}
		if ( CPtr<IDataStream> pStreamRepair = OpenFileStream( ".\\defconf.cfg", STREAM_ACCESS_READ ) )
		{
			CPtr<IDataTree> pRepair = CreateDataTreeSaver( pStreamRepair, IDataTree::READ );
			GetSingleton<IInput>()->Repair( pRepair, false );
		}
	}
	
	// open resources
	bool bUseDXT = false;

	{
		CPtr<IGFX> pGFX = GetSingleton<IGFX>();
		
		pGFX->SetMode( nSizeX, nSizeY, 16, 0, GFXFS_WINDOWED, 0 );
		// some GFX setup
		pGFX->SetCullMode( GFXC_CW );	// setup right-handed coordinate system
		SHMatrix matrix;
		CreateOrthographicProjectionMatrixRH( &matrix, nSizeX, nSizeY, -nSizeY, nSizeY*3 );
		pGFX->SetProjectionTransform( matrix );
		pGFX->EnableLighting( false );
		//
		GetSingleton<ITextureManager>()->SetQuality( ITextureManager::TEXTURE_QUALITY_HIGH );
	}

	// create and set font - for test purposes
	{
		CPtr<IGFXFont> pFont = GetSingleton<IFontManager>()->GetFont( "fonts\\medium" );
		GetSingleton<IGFX>()->SetFont( pFont );
	}
	
	ICursor *pCursor = GetSingleton<ICursor>();
	pCursor->SetBounds( 0, 0, 800, 600 );
	pCursor->SetMode( 0 );
	pCursor->Show( false );
	//
	SetGlobalVar( "editor", 1 );
	//
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::ShowSECControlBar( SECControlBar *pControlBar, int nCommand )
{
	if ( nCommand == SW_SHOW )
	{
		ShowControlBar( pControlBar, TRUE, FALSE );
	}
	else
	{
		ShowControlBar( pControlBar, FALSE, FALSE );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::FillBrushSize()
{
	if ( pwndBrushSizeComboBox )
	{
		pwndBrushSizeComboBox->ResetContent();

		for ( int nBrushSize = 1; nBrushSize <= 16; ++nBrushSize )
		{
			int nIndex = pwndBrushSizeComboBox->AddString( NStr::Format( _T( "%dx%d" ), nBrushSize, nBrushSize ) );
			if ( nIndex >= 0 ) 
			{
				pwndBrushSizeComboBox->SetItemData( nIndex, nBrushSize );
			}
		}
		pwndBrushSizeComboBox->SelectString( -1,  _T( "2x2" ) );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::FillRangeFilterComboBox( const std::string &rszCurrentFilter, const TFilterHashMap &rAllFilters, bool bUpdate )
{
	if ( pwndFireRangeFilterComboBox )
	{
		pwndFireRangeFilterComboBox->ResetContent();
		for( TFilterHashMap::const_iterator filtersIterator = rAllFilters.begin(); filtersIterator != rAllFilters.end(); ++filtersIterator )
		{
			const std::string szFilter = filtersIterator->first;
			if ( szFilter != "" )
			{
				pwndFireRangeFilterComboBox->AddString( szFilter.c_str() );		
			}
		}
		pwndFireRangeFilterComboBox->AddString( CSetupFilterDialog::SELECTED_UNITS );
		if ( rszCurrentFilter.empty() )
		{
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				if ( pFrame->m_mapEditorBarPtr )
				{
					if ( pFrame->m_mapEditorBarPtr->GetObjectWnd() )
					{
						if ( !pFrame->m_mapEditorBarPtr->GetObjectWnd()->resizeDialogOptions.szParameters.empty() )
						{
							std::string szSelection = pFrame->m_mapEditorBarPtr->GetObjectWnd()->resizeDialogOptions.szParameters[0];
							pwndFireRangeFilterComboBox->SelectString( -1, szSelection.c_str() );
						}
					}
				}
			}
		}
		else
		{
			pwndFireRangeFilterComboBox->SelectString( -1, rszCurrentFilter.c_str() );
		}
		if ( bUpdate )
		{
			 OnChangeFireRangeFilter();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::FillPlayerNumbers( const std::string &rszPlayerNumber )
{
	if ( pwndPlayerNumberComboBox )
	{
		pwndPlayerNumberComboBox->ResetContent();
		if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
		{
			for ( int nPlayerIndex = 0; nPlayerIndex < pFrame->currentMapInfo.diplomacies.size(); ++nPlayerIndex )
			{
				int nStringNumber = pwndPlayerNumberComboBox->AddString( NStr::Format( "%2d", nPlayerIndex ) );
				pwndPlayerNumberComboBox->SetItemData( nStringNumber, nPlayerIndex );
			}
			pwndPlayerNumberComboBox->SelectString( -1, rszPlayerNumber.c_str() );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CMainFrame::OnCreateCombo( WPARAM wParam, LPARAM lParam )
{
	HWND hWnd		 = HWND(lParam);
	UINT nNotifyCode = HIWORD(wParam);
	UINT nIDCtl		 = LOWORD(wParam);
	ASSERT(::IsWindow(hWnd));
	CWnd* pWnd = CWnd::FromHandle( hWnd );


	switch(nIDCtl)
	{
		case IDC_BRUSHSIZE:
		{
			ASSERT_KINDOF( CComboBox, pWnd );
			CComboBox* pCombo = static_cast<CComboBox*>( pWnd );
			switch( nNotifyCode )
			{
				case SECWndBtn::WndInit:
				{
					pwndBrushSizeComboBox = pCombo;
				}
			}
			break;
		}

		case IDC_SHOW_FIRE_RANGE_FILTER:
		{
			ASSERT_KINDOF( CComboBox, pWnd );
			CComboBox* pCombo = static_cast<CComboBox*>( pWnd );
			switch(nNotifyCode)
			{
				case SECWndBtn::WndInit:
				{
					pwndFireRangeFilterComboBox = pCombo;
				}
			}
			break;
		}

		case IDC_PLAYER_NUMBER:
		{
			ASSERT_KINDOF( CComboBox, pWnd );
			CComboBox* pCombo = static_cast<CComboBox*>( pWnd );
			switch(nNotifyCode)
			{
				case SECWndBtn::WndInit:
				{
					pwndPlayerNumberComboBox = pCombo;
				}
			}
			break;
		}

	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnChangeFireRangeFilter()
{
	if ( !pwndFireRangeFilterComboBox  )
	{
		return;
	}
	int nPos = pwndFireRangeFilterComboBox->GetCurSel();
	if ( nPos >= 0 )
	{
		CString strString;
		pwndFireRangeFilterComboBox->GetLBText( nPos, strString );
		if ( strString.Compare( CSetupFilterDialog::SELECTED_UNITS ) != 0 )
		{
			if ( g_frameManager.GetTemplateEditorFrame() && ::IsWindow( g_frameManager.GetTemplateEditorFrame()->GetSafeHwnd() ) )
			{
				if ( g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr )
				{
					if ( g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd() )
					{
						g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd()->resizeDialogOptions.szParameters[0] = strString;
						g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd()->m_filtersCtrl.SelectString( -1, strString );
					}
				}
			}
		}
		reinterpret_cast<CWnd *>( g_frameManager.GetTemplateEditorFrame() )->SendMessage( WM_USER + 2 );
		if( g_frameManager.GetTemplateEditorFrame()->bFireRangePressed )
		{
			if ( g_frameManager.GetTemplateEditorFrame() )
			{
				g_frameManager.GetTemplateEditorFrame()->ShowFireRange( true );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnChangeBrushSize()
{
	if ( !pwndBrushSizeComboBox  )
	{
		return;
	}
 	int nIndex = pwndBrushSizeComboBox->GetCurSel();
	if ( nIndex >= 0 )
	{
		int nBrushSize = pwndBrushSizeComboBox->GetItemData( nIndex );
		g_frameManager.GetTemplateEditorFrame()->m_brushDX = nBrushSize;
		g_frameManager.GetTemplateEditorFrame()->m_brushDY = nBrushSize;
		g_frameManager.GetTemplateEditorFrame()->RedrawWindow();
		
	};
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnChangePlayerNumber()
{
	if ( !pwndPlayerNumberComboBox  )
	{
		return;
	}
 	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		int nIndex = pwndPlayerNumberComboBox->GetCurSel();
		if ( nIndex >= 0 )
		{
			int nPlayerIndex = pwndPlayerNumberComboBox->GetItemData( nIndex );
			pFrame->m_mapEditorBarPtr->GetObjectWnd()->m_players.SelectString( -1, NStr::Format( "%2d", nPlayerIndex ) );
			pFrame->ShowStorageCoverage();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnClose() 
{
	editorWindowSingletonApp.RemoveMapFile();
	if ( g_frameManager.GetTemplateEditorFrame()->NeedSaveChanges() )
	{
		CString strRegistryKeyName;

		strRegistryKeyName.LoadString( IDS_WINDOWBAR_KEY );
		SaveBarState( strRegistryKeyName );

		theApp.SaveRegisterData();
		SECWorkbook::OnClose();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnHelp() 
{
	if ( NFile::IsFileExist( szHelpFilePath.c_str() ) )
	{
		::HtmlHelpA( NULL, szHelpFilePath.c_str(), HH_DISPLAY_TOPIC, 0 );
  }
  else
  {
    CString strTitle;
    CString strMessagePattern;
    CString strMessage;
    
    strTitle.LoadString( IDS_NO_HELP_FILE_TITLE );
    strMessagePattern.LoadString( IDS_NO_HELP_FILE_MESSAGE );
		strMessage.Format( strMessagePattern, szHelpFilePath.c_str() );
    ::MessageBox( GetSafeHwnd(), strMessage, strTitle, MB_ICONERROR | MB_OK );
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateHelp(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( NFile::IsFileExist( szHelpFilePath.c_str() ) );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewLeftBar() 
{
	ShowControlBar( &wndInputControlBar, !wndInputControlBar.IsVisible(), true );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewLeftBar(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
	pCmdUI->SetCheck( wndInputControlBar.IsVisible() );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewToolbar0() 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 5 ) )
	{
		ShowControlBar( pBar, !pBar->IsVisible(), true );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewToolbar1() 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 6 ) )
	{
		ShowControlBar( pBar, !pBar->IsVisible(), true );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewToolbar2() 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 7 ) )
	{
		ShowControlBar( pBar, !pBar->IsVisible(), true );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewToolbar3() 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 8 ) )
	{
		ShowControlBar( pBar, !pBar->IsVisible(), true );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewToolbar4() 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 9 ) )
	{
		ShowControlBar( pBar, !pBar->IsVisible(), true );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewToolbar0(CCmdUI* pCmdUI) 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 5 ) )
	{
		pCmdUI->Enable( true );
		pCmdUI->SetCheck( pBar->IsVisible() );	
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewToolbar1(CCmdUI* pCmdUI) 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 6 ) )
	{
		pCmdUI->Enable( true );
		pCmdUI->SetCheck( pBar->IsVisible() );	
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewToolbar2(CCmdUI* pCmdUI) 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 7 ) )
	{
		pCmdUI->Enable( true );
		pCmdUI->SetCheck( pBar->IsVisible() );	
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewToolbar3(CCmdUI* pCmdUI) 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 8 ) )
	{
		pCmdUI->Enable( true );
		pCmdUI->SetCheck( pBar->IsVisible() );	
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewToolbar4(CCmdUI* pCmdUI) 
{
	if ( CControlBar *pBar = GetControlBar( AFX_IDW_TOOLBAR + 9 ) )
	{
		pCmdUI->Enable( true );
		pCmdUI->SetCheck( pBar->IsVisible() );	
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateShowminimap(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
	pCmdUI->SetCheck( wndMiniMapBar.IsVisible() );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnShowminimap() 
{
	ShowControlBar( &wndMiniMapBar, !wndMiniMapBar.IsVisible(), true );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::AddToRecentList( const std::string &rszMapFileName )
{
	std::string szMapFileName = rszMapFileName;
	NStr::ToLower( szMapFileName );
	int nRecentCount = 0;
	for ( std::list<std::string>::iterator recentIterator = recentList.begin(); recentIterator != recentList.end(); )
	{
		std::string szRecentName = ( *recentIterator );
		NStr::ToLower( szRecentName );
		if ( szRecentName == szMapFileName )
		{
			recentIterator = recentList.erase( recentIterator );
		}
		else
		{
			++recentIterator;
			++nRecentCount;
		}
	}
	recentList.push_front( rszMapFileName );
	++nRecentCount;
	if ( nRecentCount > 10 )
	{
		recentList.pop_back();
	}
	UpdateRecentList();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::RemoveFromRecentList( const std::string &rszMapFileName )
{
	std::string szMapFileName = rszMapFileName;
	NStr::ToLower( szMapFileName );
	int nRecentCount = 0;
	for ( std::list<std::string>::iterator recentIterator = recentList.begin(); recentIterator != recentList.end(); )
	{
		std::string szRecentName = ( *recentIterator );
		NStr::ToLower( szRecentName );
		if ( szRecentName == szMapFileName )
		{
			recentIterator = recentList.erase( recentIterator );
		}
		else
		{
			++recentIterator;
			++nRecentCount;
		}
	}
	UpdateRecentList();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnRecentMap( UINT nID ) 
{
	int nRecentCount = 0;
	for ( std::list<std::string>::const_iterator recentIterator = recentList.begin(); recentIterator != recentList.end(); ++recentIterator )
	{
		if ( nRecentCount == ( nID - ID_RECENT_MAP_0 ) )
		{
			g_frameManager.GetTemplateEditorFrame()->OnFileLoadMap( *recentIterator );
			return;
		}
		++nRecentCount;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::UpdateRecentList()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateRecentMap( CCmdUI* pCmdUI ) 
{
	pCmdUI->Enable( !recentList.empty() );
	if ( CMenu *pFileMenu = pCmdUI->m_pMenu )
	{
		if ( CMenu *pRecentMenu = pFileMenu->GetSubMenu( 10 ) )
		{
			while( pRecentMenu->GetMenuItemCount() > 0 )
			{
				pRecentMenu->RemoveMenu( 0 , MF_BYPOSITION );
			}
			int nRecentCount = 0;
			for ( std::list<std::string>::const_iterator recentIterator = recentList.begin(); recentIterator != recentList.end(); ++recentIterator )
			{
				pRecentMenu->InsertMenu( -1, MF_BYPOSITION, ID_RECENT_MAP_0 + nRecentCount, recentIterator->c_str() );
				//pRecentMenu->EnableMenuItem( ID_RECENT_MAP_0 + nRecentCount, MF_ENABLED | MF_BYCOMMAND );
				++nRecentCount;
			}
			if ( pRecentMenu->GetMenuItemCount() == 0 )
			{
				CString strMenuLabel;
				strMenuLabel.LoadString( IDS_RECENT_MAP );
				pRecentMenu->InsertMenu( -1, MF_BYPOSITION, ID_RECENT_MAP_0, strMenuLabel );
				//pRecentMenu->EnableMenuItem( ID_RECENT_MAP_0, MF_GRAYED | MF_BYCOMMAND );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateRecentMapRange( CCmdUI* pCmdUI )
{
	pCmdUI->Enable( true );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnTool0() 
{
	if ( IDataStorage *pStorage = GetSingleton<IDataStorage>() )
	{
		BeginWaitCursor();
		
		const std::string szTemplatesFolder( "Scenarios\\Templates\\" );
		const std::string szOutputFile( "logs\\graphs_list.txt" );
		std::string szOutputString;

		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );

		if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
		{
			if ( CPtr<IDataStream> pStream = CreateFileStream( pStorage->GetName() + szOutputFile, STREAM_ACCESS_WRITE ) )
			{
				std::list<std::string> files;
				if ( pFrame->GetEnumFilesInDataStorage( szTemplatesFolder, &files ) )
				{
					for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
					{
						const std::string szFileName = ( *fileIterator );
						szOutputString = NStr::Format( "%s\r\n", szFileName.c_str() );
						pStream->Write( szOutputString.c_str(), szOutputString.size() );
						
						std::string szResourceName = szFileName.substr( 0, szFileName.size() - 4 );
						SRMTemplate randomMapTemplate;
						bool bResult = LoadDataResource( szResourceName, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate );
						NI_ASSERT_T( bResult,
												 NStr::Format( "CMainFrame::OnTool0(), Can't load SRMTemplate from %s", szResourceName.c_str() ) );
						for ( int nGraphIndex = 0; nGraphIndex < randomMapTemplate.graphs.size(); ++nGraphIndex )
						{
							szOutputString = NStr::Format( "	%d %d %s\r\n", nGraphIndex, randomMapTemplate.graphs.GetWeight( nGraphIndex ), randomMapTemplate.graphs[nGraphIndex].c_str() );
							pStream->Write( szOutputString.c_str(), szOutputString.size() );
						}
					}
				}
				const CString strMessage = NStr::Format( "RMG graps list created: %s%s", pStorage->GetName(), szOutputFile.c_str() );
				MessageBox( strMessage, strTitle, MB_OK | MB_ICONINFORMATION );
			}
			else
			{
				const CString strMessage = NStr::Format( "File creation error: %s%s", pStorage->GetName(), szOutputFile.c_str() );
				MessageBox( strMessage, strTitle, MB_OK | MB_ICONSTOP );
			}
		}
		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnTool1()
{
	if ( IDataStorage *pStorage = GetSingleton<IDataStorage>() )
	{
		BeginWaitCursor();

		const std::string szChaptersFolder( "Scenarios\\Chapters\\" );
		const std::string szOutputFile( "logs\\contexts_list.txt" );
		std::string szOutputString;
	
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );

		if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
		{
			if ( CPtr<IDataStream> pStream = CreateFileStream( pStorage->GetName() + szOutputFile, STREAM_ACCESS_WRITE ) )
			{
				std::list<std::string> files;
				if ( pFrame->GetEnumFilesInDataStorage( szChaptersFolder, &files ) )
				{
					for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
					{
						const std::string szFileName = ( *fileIterator );
						szOutputString = NStr::Format( "%s\r\n", szFileName.c_str() );
						pStream->Write( szOutputString.c_str(), szOutputString.size() );
					}
				}
				const CString strMessage = NStr::Format( "RMG contexts list created: %s%s", pStorage->GetName(), szOutputFile.c_str() );
				MessageBox( strMessage, strTitle, MB_OK | MB_ICONINFORMATION );
			}
			else
			{
				const CString strMessage = NStr::Format( "File creation error: %s%s", pStorage->GetName(), szOutputFile.c_str() );
				MessageBox( strMessage, strTitle, MB_OK | MB_ICONSTOP );
			}
		}

		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnTool2()
{
	if ( IDataStorage *pStorage = GetSingleton<IDataStorage>() )
	{
		BeginWaitCursor();

		const std::string szScenariousFolder( "Scenarios\\Patches\\" );
		const std::string szOutputFile( "logs\\patches_list.txt" );
		std::string szOutputString;
	
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );

		if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
		{
			if ( CPtr<IDataStream> pStream = CreateFileStream( pStorage->GetName() + szOutputFile, STREAM_ACCESS_WRITE ) )
			{
				std::list<std::string> files;
				if ( pFrame->GetEnumFilesInDataStorage( szScenariousFolder, &files ) )
				{
					for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
					{
						const std::string szFileName = ( *fileIterator );
						szOutputString = NStr::Format( "%s\r\n", szFileName.c_str() );
						pStream->Write( szOutputString.c_str(), szOutputString.size() );
					}
				}
				const CString strMessage = NStr::Format( "RMG patches list created: %s%s", pStorage->GetName(), szOutputFile.c_str() );
				MessageBox( strMessage, strTitle, MB_OK | MB_ICONINFORMATION );
			}
			else
			{
				const CString strMessage = NStr::Format( "File creation error: %s%s", pStorage->GetName(), szOutputFile.c_str() );
				MessageBox( strMessage, strTitle, MB_OK | MB_ICONSTOP );
			}
		}

		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnTool3()
{
	if ( IDataStorage *pStorage = GetSingleton<IDataStorage>() )
	{
		BeginWaitCursor();

		const std::string szMapsFolder( "Maps\\" );
		const std::string szOutputFile( "logs\\maps_list.txt" );
		std::string szOutputString;
	
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );

		const std::string szRivers3DName( CTemplateEditorFrame::RIVERS_3D_MAP_NAME );
		const std::string szRoads3DName( CTemplateEditorFrame::ROADS_3D_MAP_NAME );

		if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
		{
			if ( CPtr<IDataStream> pStream = CreateFileStream( pStorage->GetName() + szOutputFile, STREAM_ACCESS_WRITE ) )
			{
				std::list<std::string> files;
				if ( pFrame->GetEnumFilesInDataStorage( szMapsFolder, &files ) )
				{
					for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
					{
						const std::string szFileName = ( *fileIterator );
						if ( ( szFileName != szRivers3DName ) &&
								 ( szFileName != szRoads3DName ) )
						{
							szOutputString = NStr::Format( "%s\r\n", szFileName.c_str() );
							pStream->Write( szOutputString.c_str(), szOutputString.size() );
						}
					}
				}
				const CString strMessage = NStr::Format( "Maps list created: %s%s", pStorage->GetName(), szOutputFile.c_str() );
				MessageBox( strMessage, strTitle, MB_OK | MB_ICONINFORMATION );
			}
			else
			{
				const CString strMessage = NStr::Format( "File creation error: %s%s", pStorage->GetName(), szOutputFile.c_str() );
				MessageBox( strMessage, strTitle, MB_OK | MB_ICONSTOP );
			}
		}

		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnDropFiles( HDROP hDropInfo ) 
{
	int nFileCount = ::DragQueryFile( hDropInfo, 0xFFFFFFFF, 0, 0 );
	for ( int nFileIndex = 0; nFileIndex < nFileCount; ++nFileIndex )
	{
		CString strFileName;
		::DragQueryFile( hDropInfo,
										 nFileIndex,
										 strFileName.GetBuffer( 0xFFF ),
										 0xFFF );
		strFileName.ReleaseBuffer();
		std::string szFileName = strFileName;
		g_frameManager.GetTemplateEditorFrame()->OnFileLoadMap( szFileName );
		break;
	}
	::DragFinish( hDropInfo );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CMainFrame::OnTool4()
{
	if ( IDataStorage *pStorage = GetSingleton<IDataStorage>() )
	{
		BeginWaitCursor();

		const std::string szScenariousFolder( "Scenarios\\Patches\\" );

		if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
		{
			std::list<std::string> files;
			if ( pFrame->GetEnumFilesInDataStorage( szScenariousFolder, &files ) )
			{
				CProgressDialog progressDialog;
				progressDialog.Create( IDD_PROGRESS, this );

				if ( progressDialog.GetSafeHwnd() != 0 )
				{
					CString strTitle;
					strTitle.LoadString( IDR_EDITORTYPE );
					progressDialog.SetWindowText( strTitle );
					progressDialog.ShowWindow( SW_SHOW ); 
					progressDialog.SetProgressRange( 0, files.size() ); 
				}

				for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
				{
					const std::string szFileName = ( *fileIterator );
					
					if ( progressDialog.GetSafeHwnd() != 0 )
					{
						progressDialog.SetProgressMessage( NStr::Format( "Updating Blitzkrieg Map: %s...", szFileName.c_str() ) );
					}

					const std::string szMapName = szFileName.substr( 0, szFileName.rfind( '.' ) );

					CMapInfo patchMap;
					if ( LoadTypedSuperDataResource( szMapName, ".bzm", true, 1, patchMap ) )
					{
						const CTRect<int> updateRect( 0, 0, patchMap.terrain.patches.GetSizeX() - 1, patchMap.terrain.patches.GetSizeY() - 1 );
						patchMap.UpdateTerrain( updateRect );
						patchMap.UpdateObjects( updateRect );

						SaveTypedSuperDataResource( szMapName, ".bzm", true, 1, patchMap );
					}
					
					if ( progressDialog.GetSafeHwnd() != 0 )
					{
						progressDialog.IterateProgressPosition();
					}
				}
				if ( progressDialog.GetSafeHwnd() != 0 )
				{
					progressDialog.DestroyWindow();
				}
			}
		}

		EndWaitCursor();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::OnCopyData( CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct ) 
{
	if ( ( pWnd == 0 ) && ( pCopyDataStruct->dwData == CEditorWindowSingletonBase::OPEN_FILE ) )
	{
		std::string szCommandLine = static_cast<char*>( pCopyDataStruct->lpData );
		g_frameManager.GetTemplateEditorFrame()->OnFileLoadMap( szCommandLine );
		return true;
	}
	else
	{
		return SECWorkbook::OnCopyData(pWnd, pCopyDataStruct);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
