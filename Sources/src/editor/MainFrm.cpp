
#include "StdAfx.h"
#include "editor.h"
#include "resource.h"
#include "PropertyDockBar.h"
#include "GUIFrame.h"
#include "AnimationFrm.h"
#include "SpriteFrm.h"
#include "EffectFrm.h"
#include "ObjectFrm.h"
#include "MeshFrm.h"
#include "WeaponFrm.h"
#include "BuildFrm.h"
#include "TileSetFrm.h"
#include "FenceFrm.h"
#include "ParticleFrm.h"
#include "TrenchFrm.h"
#include "SquadFrm.h"
#include "MineFrm.h"
#include "BridgeFrm.h"
#include "MissionFrm.h"
#include "ChapterFrm.h"
#include "CampaignFrm.h"
#include "3DRoadFrm.h"
#include "3DRiverFrm.h"
#include "MedalFrm.h"
#include "RefDlg.h"

#include "MainFrm.h"
#include "frames.h"
#include "TreeDockWnd.h"
#include "TreeItemFactory.h"
#include "PropView.h"
#include "KeyFrameDock.h"
#include "..\Main\GameDB.h"
#include "..\Main\iMain.h"
#include "..\GameTT\iMission.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const char REG_BARSLAYOUT[] = "Layout\\Controls";

static UINT BASED_CODE defaultButtons[] =
{
	ID_EDIT_CUT,
	ID_EDIT_COPY,
	ID_EDIT_PASTE,
	ID_RUN_GAME,
};

static UINT BASED_CODE objectEditorButtons[] =
{
	ID_MOVE_OBJECT,
	ID_DRAW_GRID,
	ID_OBJECT_TRANSPARENCE,
	ID_SET_ZERO_BUTTON,
	ID_DRAW_ONE_WAY_TRANSEPARENCE,
};

static UINT BASED_CODE tileSetEditorButtons[] =
{
	ID_IMPORT_TERRAINS,
	ID_IMPORT_CROSSETS,
};

static UINT BASED_CODE fenceEditorButtons[] =
{
	ID_MOVE_OBJECT,
	ID_DRAW_GRID,
	ID_FENCE_TRANSPARENCE,
	ID_CENTER_FENCE_ABOUT_TILE,
};

static UINT BASED_CODE buildingEditorButtons[] =
{
	ID_MOVE_OBJECT,
	ID_DRAW_GRID,
	ID_BUILDING_TRANSPARENCE,
	ID_SET_ENTRANCE_BUTTON,
	ID_SET_ZERO_BUTTON,
	ID_SEPARATOR,
	ID_SET_SHOOT_POINT,
	ID_SET_FIRE_POINT,
	ID_SET_DIRECTION_EXPLOSION,
	ID_SET_SMOKE_POINT,
	ID_SEPARATOR,
	ID_MOVE_POINT,
	ID_SET_HORIZONTAL_SHOOT,
	ID_SET_SHOOT_ANGLE,
	ID_GENERATE_POINTS,
};

static UINT BASED_CODE bridgeEditorButtons[] =
{
	ID_DRAW_GRID,
	ID_BRIDGE_TRANSPARENCE,
	ID_DRAW_PASS,
	ID_SET_ZERO_BUTTON,

	ID_SEPARATOR,
	ID_SET_FIRE_POINT,
	ID_SET_SMOKE_POINT,
	ID_SEPARATOR,
	ID_MOVE_POINT,
	ID_SET_HORIZONTAL_SHOOT,
	ID_SET_SHOOT_ANGLE,
	ID_GENERATE_POINTS,
};

static UINT BASED_CODE missionEditorButtons[] =
{
	ID_GENERATE_IMAGE,
};

static UINT BASED_CODE meshEditorButtons[] =
{
	ID_SHOW_LOCATORS_INFO,
};

static UINT BASED_CODE effectEditorButtons[] =
{
	ID_RUN_BUTTON,
	ID_STOP_BUTTON,
	ID_BUTTON_CAMERA,
};

static UINT BASED_CODE particleEditorButtons[] =
{
	ID_RUN_BUTTON,
	ID_STOP_BUTTON,
	ID_BUTTON_CAMERA,
	ID_GET_PARTICLE_INFO,
	ID_SEPARATOR,
	ID_PARTICLE_SOURCE,
};

static UINT BASED_CODE squadEditorButtons[] =
{
	ID_SET_ZERO_BUTTON,
};

static UINT BASED_CODE road3dEditorButtons[] =
{
	ID_SWITCH_WIREFRAME,
};

static UINT BASED_CODE chapterEditorButtons[] =
{
	ID_SHOW_CROSSES,
};

static UINT BASED_CODE infantryEditorButtons[] =
{
	ID_RUN_BUTTON,
	ID_STOP_BUTTON,
};


BEGIN_BUTTON_MAP(btnMap)
	TEXT_BUTTON_EX(ID_MOVE_OBJECT, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_DRAW_GRID, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_DRAW_PASS, 0, TBBS_CHECKBOX)
	COMBO_BUTTON(ID_BRIDGE_TRANSPARENCE, IDC_BRIDGE_TRANSPARENCE, SEC_TBBS_VCENTER, CBS_DROPDOWNLIST, 40, 40, 250 )
	COMBO_BUTTON(ID_BUILDING_TRANSPARENCE, IDC_BUILDING_TRANSPARENCE, SEC_TBBS_VCENTER, CBS_DROPDOWNLIST, 40, 40, 250 )
	COMBO_BUTTON(ID_FENCE_TRANSPARENCE, IDC_FENCE_TRANSPARENCE, SEC_TBBS_VCENTER, CBS_DROPDOWNLIST, 40, 40, 250 )
	COMBO_BUTTON(ID_OBJECT_TRANSPARENCE, IDC_OBJECT_TRANSPARENCE, SEC_TBBS_VCENTER, CBS_DROPDOWNLIST, 40, 40, 250 )
	TEXT_BUTTON_EX(ID_SET_ENTRANCE_BUTTON, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_SET_ZERO_BUTTON, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_SET_SHOOT_POINT, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_SET_HORIZONTAL_SHOOT, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_SET_SHOOT_ANGLE, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_DRAW_ONE_WAY_TRANSEPARENCE, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_SHOW_LOCATORS_INFO, 0, TBBS_CHECKBOX)
	
	TEXT_BUTTON_EX(ID_BUTTON_CAMERA, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_SHOW_CROSSES, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_PARTICLE_SOURCE, 0, TBBS_CHECKBOX)
	
	TEXT_BUTTON_EX(ID_SWITCH_WIREFRAME, 0, TBBS_CHECKBOX)
	TEXT_BUTTON_EX(ID_EDIT_CROSSETS, 0, TBBS_CHECKBOX)
END_BUTTON_MAP()

int wmAppToolBarWndNotify
  =	RegisterWindowMessage(_T("WM_SECTOOLBARWNDNOTIFY"));

IMPLEMENT_DYNAMIC(CMainFrame, SECWorkbook)

BEGIN_MESSAGE_MAP(CMainFrame, SECWorkbook)
	ON_WM_CREATE()
	ON_REGISTERED_MESSAGE(wmAppToolBarWndNotify, OnCreateCombo)
	ON_WM_CLOSE()
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_COORDS,
	ID_INDICATOR_CONTROL,
	ID_INDICATOR_TILEPOS,
	ID_INDICATOR_OBJECTTYPE,

};

static void SetStatusPaneInfoIfPresent( CStatusBar &rStatusBar, UINT nID, UINT nStyle, int nWidth )
{
	int nIndex = rStatusBar.CommandToIndex( nID );
	if ( nIndex >= 0 )
		rStatusBar.SetPaneInfo( nIndex, nID, nStyle, nWidth );
}

static void SetStatusPaneTextIfPresent( CStatusBar &rStatusBar, UINT nID, LPCTSTR pszText )
{
	if ( !::IsWindow( rStatusBar.GetSafeHwnd() ) )
		return;

	int nIndex = rStatusBar.CommandToIndex( nID );
	if ( nIndex >= 0 )
		rStatusBar.SetPaneText( nIndex, pszText );
}


CMainFrame::CMainFrame() : m_pFenceCombo( 0 ), m_pObjectCombo( 0 ), m_pBuildingCombo( 0 ), m_pBridgeCombo( 0 ), m_fireRangeFilterComboBox ( 0 ), m_fireRangePressed( false ), m_nFireRangeRegisterGroup( -1 )
{
	m_pControlBarManager = new SECToolBarManager(this);	// this is a base class member
	m_pMenuBar = new SECMDIMenuBar;	// this is a base class member
	

	m_pDefButtonGroup = NULL;
	m_nDefButtonCount = 0;
	m_hMDIAccel = 0;

	pInfantryToolBar = 0;
	pCommonToolBar = 0;
	pObjectToolBar = 0;
	pMeshToolBar = 0;
	pBuildingToolBar = 0;
	pTileToolBar = 0;
	pFenceToolBar = 0;
	pParticleToolBar = 0;
	pEffectToolBar = 0;
	pBridgeToolBar = 0;
	pMissionToolBar = 0;
	pSquadToolBar = 0;
	p3DRoadToolBar = 0;
	pChapterToolBar = 0;
	
	pGUITreeDockWnd = new CTreeDockWnd;
	pGUIPropView = new CPropView;
	pGUIPropertyDockBar = new CPropertyDockBar;
	pAnimTreeDockWnd = new CTreeDockWnd;
	pAnimPropView = new CPropView;
	pSpriteTreeDockWnd = new CTreeDockWnd;
	pSpritePropView = new CPropView;
	pEffectTreeDockWnd = new CTreeDockWnd;
	pEffectPropView = new CPropView;
	pObjectTreeDockWnd = new CTreeDockWnd;
	pObjectPropView = new CPropView;
	pMeshTreeDockWnd = new CTreeDockWnd;
	pMeshPropView = new CPropView;
	pMeshDirectionButtonDockBar = new CDirectionButtonDockBar;
	pEffectDirectionButtonDockBar = new CDirectionButtonDockBar;
	pWeaponTreeDockWnd = new CTreeDockWnd;
	pWeaponPropView = new CPropView;
	pBuildingTreeDockWnd = new CTreeDockWnd;
	pBuildingPropView = new CPropView;
	pTileTreeDockWnd = new CTreeDockWnd;
	pTilePropView = new CPropView;
	pFenceTreeDockWnd = new CTreeDockWnd;
	pFencePropView = new CPropView;
	pParticleTreeDockWnd = new CTreeDockWnd;
	pParticlePropView = new CPropView;
	pParticleKeyFrameDockWnd = new CKeyFrameDockWnd;
	pTrenchTreeDockWnd = new CTreeDockWnd;
	pTrenchPropView = new CPropView;
	pSquadTreeDockWnd = new CTreeDockWnd;
	pSquadPropView = new CPropView;
	pSquadDirectionButtonDockBar = new CDirectionButtonDockBar;
	pMineTreeDockWnd = new CTreeDockWnd;
	pMinePropView = new CPropView;
	pBridgeTreeDockWnd = new CTreeDockWnd;
	pBridgePropView = new CPropView;
	pMissionTreeDockWnd = new CTreeDockWnd;
	pMissionPropView = new CPropView;
	pChapterTreeDockWnd = new CTreeDockWnd;
	pChapterPropView = new CPropView;
	pCampaignTreeDockWnd = new CTreeDockWnd;
	pCampaignPropView = new CPropView;
	p3DRoadTreeDockWnd = new CTreeDockWnd;
	p3DRoadPropView = new CPropView;
	p3DRiverTreeDockWnd = new CTreeDockWnd;
	p3DRiverPropView = new CPropView;
	pMedalTreeDockWnd = new CTreeDockWnd;
	pMedalPropView = new CPropView;
		
	HFONT hFont = (HFONT) GetStockObject(DEFAULT_GUI_FONT);
	if(hFont == NULL)
		hFont = (HFONT) GetStockObject(ANSI_VAR_FONT);
	
	LOGFONT lf;
	if(::GetObject(hFont, sizeof(lf), &lf))
	{
		lf.lfWeight = FW_BOLD;
		m_hComboFont = ::CreateFontIndirect(&lf);
	}
	wmAppToolBarWndNotify = RegisterWindowMessage(_T("WM_SECTOOLBARWNDNOTIFY"));
}

CMainFrame::~CMainFrame()
{
	if ( m_pControlBarManager )
		delete m_pControlBarManager;

	if (m_pDefButtonGroup)
		delete [] m_pDefButtonGroup;
	if ( m_pMenuBar )
		delete m_pMenuBar;
	if ( pGUITreeDockWnd )
		delete pGUITreeDockWnd;
	if ( pGUIPropView )
		delete pGUIPropView;
	if ( pGUIPropertyDockBar )
		delete pGUIPropertyDockBar;
	if ( pAnimTreeDockWnd )
		delete pAnimTreeDockWnd;
	if ( pAnimPropView )
		delete pAnimPropView;
	if ( pSpriteTreeDockWnd )
		delete pSpriteTreeDockWnd;
	if ( pSpritePropView )
		delete pSpritePropView;
	if ( pEffectTreeDockWnd )
		delete pEffectTreeDockWnd;
	if ( pEffectPropView )
		delete pEffectPropView;
	if ( pObjectTreeDockWnd )
		delete pObjectTreeDockWnd;
	if ( pObjectPropView )
		delete pObjectPropView;
	if ( pMeshTreeDockWnd )
		delete pMeshTreeDockWnd;
	if ( pMeshPropView )
		delete pMeshPropView;
	if ( pMeshDirectionButtonDockBar )
		delete pMeshDirectionButtonDockBar;
	if ( pEffectDirectionButtonDockBar )
		delete pEffectDirectionButtonDockBar;
	if ( pWeaponTreeDockWnd )
		delete pWeaponTreeDockWnd;
	if ( pWeaponPropView )
		delete pWeaponPropView;
	if ( pBuildingTreeDockWnd )
		delete pBuildingTreeDockWnd;
	if ( pBuildingPropView )
		delete pBuildingPropView;
	if ( pTileTreeDockWnd )
		delete pTileTreeDockWnd;
	if ( pTilePropView )
		delete pTilePropView;
	if ( pFenceTreeDockWnd )
		delete pFenceTreeDockWnd;
	if ( pFencePropView )
		delete pFencePropView;
	if ( pParticleTreeDockWnd )
		delete pParticleTreeDockWnd;
	if ( pParticlePropView )
		delete pParticlePropView;
	if ( pParticleKeyFrameDockWnd )
		delete pParticleKeyFrameDockWnd;
	if ( pTrenchTreeDockWnd )
		delete pTrenchTreeDockWnd;
	if ( pTrenchPropView )
		delete pTrenchPropView;
	if ( pSquadTreeDockWnd )
		delete pSquadTreeDockWnd;
	if ( pSquadPropView )
		delete pSquadPropView;
	if ( pSquadDirectionButtonDockBar )
		delete pSquadDirectionButtonDockBar;
	if ( pMineTreeDockWnd )
		delete pMineTreeDockWnd;
	if ( pMinePropView )
		delete pMinePropView;
	if ( pBridgeTreeDockWnd )
		delete pBridgeTreeDockWnd;
	if ( pBridgePropView )
		delete pBridgePropView;
	if ( pMissionTreeDockWnd )
		delete pMissionTreeDockWnd;
	if ( pMissionPropView )
		delete pMissionPropView;
	if ( pChapterTreeDockWnd )
		delete pChapterTreeDockWnd;
	if ( pChapterPropView )
		delete pChapterPropView;
	if ( pCampaignTreeDockWnd )
		delete pCampaignTreeDockWnd;
	if ( pCampaignPropView )
		delete pCampaignPropView;
	if ( p3DRoadTreeDockWnd )
		delete p3DRoadTreeDockWnd;
	if ( p3DRoadPropView )
		delete p3DRoadPropView;
	if ( p3DRiverTreeDockWnd )
		delete p3DRiverTreeDockWnd;
	if ( p3DRiverPropView )
		delete p3DRiverPropView;
	if ( pMedalTreeDockWnd )
		delete pMedalTreeDockWnd;
	if ( pMedalPropView )
		delete pMedalPropView;
	
	
	if(m_hComboFont)
	{
		::DeleteObject(m_hComboFont);
		m_hComboFont = NULL;
	}

	NMain::Finalize();
}


int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (SECWorkbook::OnCreate(lpCreateStruct) == -1)
		return -1;




	SECToolBarManager* pToolBarMgr=(SECToolBarManager *)m_pControlBarManager;	
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_MENU_BUTTONS),
		MAKEINTRESOURCE(IDR_MENU_BUTTONS)));

	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_OBJECTTOOLBAR),
		MAKEINTRESOURCE(IDR_OBJECTTOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_TILESET_TOOLBAR),
		MAKEINTRESOURCE(IDR_TILESET_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_BUILDING_TOOLBAR),
		MAKEINTRESOURCE(IDR_BUILDING_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_FENCE_TOOLBAR),
		MAKEINTRESOURCE(IDR_FENCE_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_BRIDGE_TOOLBAR),
		MAKEINTRESOURCE(IDR_BRIDGE_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_MESH_TOOLBAR),
		MAKEINTRESOURCE(IDR_MESH_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_PARTICLE_TOOLBAR),
		MAKEINTRESOURCE(IDR_PARTICLE_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_SQUAD_TOOLBAR),
		MAKEINTRESOURCE(IDR_SQUAD_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_3DROAD_TOOLBAR),
		MAKEINTRESOURCE(IDR_3DROAD_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_CHAPTER_TOOLBAR),
		MAKEINTRESOURCE(IDR_CHAPTER_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_EFFECT_TOOLBAR),
		MAKEINTRESOURCE(IDR_EFFECT_TOOLBAR)));
	VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_MISSION_TOOLBAR),
		MAKEINTRESOURCE(IDR_MISSION_TOOLBAR)));
	
	pToolBarMgr->SetButtonMap(btnMap);
	
	


/*	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR,
		_T("Default"),
		IDR_MAINFRAME,
		m_nDefButtonCount,
		m_pDefButtonGroup,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_FLOAT,
		NULL,			//dock after this
		TRUE,			//dock state
		FALSE);		//visible*/


	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR,
		_T("Default"),
		NUMELEMENTS(defaultButtons),
		defaultButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP);


/*
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 6,
		_T("ObjectEditor"),
		IDR_MAINFRAME,
		m_nDefButtonCount,
		m_pDefButtonGroup,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP);
*/

	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 6, 
		_T("ObjectEditor"),
		NUMELEMENTS(objectEditorButtons),
		objectEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);

	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 7, 
		_T("TileSetEditor"),
		NUMELEMENTS(tileSetEditorButtons),
		tileSetEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 8, 
		_T("BuildingEditor"),
		NUMELEMENTS(buildingEditorButtons),
		buildingEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 9, 
		_T("FenceEditor"),
		NUMELEMENTS(fenceEditorButtons),
		fenceEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 10, 
		_T("BridgeEditor"),
		NUMELEMENTS(bridgeEditorButtons),
		bridgeEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 11, 
		_T("BridgeEditor"),
		NUMELEMENTS(meshEditorButtons),
		meshEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 12, 
		_T("BridgeEditor"),
		NUMELEMENTS(particleEditorButtons),
		particleEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 13, 
		_T("SquadEditor"),
		NUMELEMENTS(squadEditorButtons),
		squadEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 14, 
		_T("Road3DEditor"),
		NUMELEMENTS(road3dEditorButtons),
		road3dEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 15, 
		_T("ChapterEditor"),
		NUMELEMENTS(chapterEditorButtons),
		chapterEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 16, 
		_T("EffectEditor"),
		NUMELEMENTS(effectEditorButtons),
		effectEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 17, 
		_T("InfantryEditor"),
		NUMELEMENTS(infantryEditorButtons),
		infantryEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);

	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 18, 
		_T("MissionEditor"),
		NUMELEMENTS(missionEditorButtons),
		missionEditorButtons,
		CBRS_ALIGN_ANY,
		AFX_IDW_DOCKBAR_TOP,
		NULL,
		TRUE,
		FALSE);
	
/*
	pToolBarMgr->SetMenuInfo( 2, IDR_MAINFRAME, IDR_EDITORTYPE );
	LoadAdditionalMenus( 1, IDR_EDITORTYPE );
	pToolBarMgr->SetMenuInfo( 2, IDR_MAINFRAME, IDR_EDITORTYPE );
	EnableDocking( CBRS_ALIGN_ANY );
*/



	pToolBarMgr->EnableCoolLook(TRUE);
	if ( !m_wndStatusBar.CreateEx(this, SBARS_SIZEGRIP | SBT_TOOLTIPS, WS_CHILD | WS_VISIBLE | CBRS_BOTTOM ) )
	{
		TRACE0("Failed to create status bar\n");
	}
	else if ( !m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT)) )
	{
		static UINT fallbackIndicators[] = { ID_SEPARATOR };
		TRACE0("Failed to create full status bar indicators\n");
		m_wndStatusBar.SetIndicators(fallbackIndicators, sizeof(fallbackIndicators)/sizeof(UINT));
	}

	SetStatusPaneInfoIfPresent( m_wndStatusBar, ID_INDICATOR_COORDS, SBPS_NORMAL, 70 );
	SetStatusPaneInfoIfPresent( m_wndStatusBar, ID_INDICATOR_CONTROL, SBPS_NORMAL, 100 );
	SetStatusPaneInfoIfPresent( m_wndStatusBar, ID_INDICATOR_TILEPOS, SBPS_NORMAL, 400 );
	SetStatusPaneInfoIfPresent( m_wndStatusBar, ID_INDICATOR_OBJECTTYPE, SBPS_NORMAL, 450 );
	
	pToolBarMgr->SetMenuInfo( 1, IDR_EDITORTYPE );
	EnableDocking( CBRS_ALIGN_ANY );
	pToolBarMgr->SetDefaultDockState();
	
	if (!m_gameWnd.Create(NULL, NULL, WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, GAME_SIZE_X, GAME_SIZE_Y), this, AFX_IDW_PANE_FIRST, NULL))
	{
		
		TRACE0("Failed to create GAME window\n");
		return -1;
	}
	
	if ( InitGameWindow() != 0 )
		return -1;
	g_frameManager.SetGameWnd( &m_gameWnd );
	m_gameWnd.ShowWindow( SW_HIDE );
	
	SetDefaultCamera();

	pCommonToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR);
	ShowSECToolBar( pCommonToolBar, SW_HIDE );

	pObjectToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 6);
	pTileToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 7);
	pBuildingToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 8);
	pFenceToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 9);
	pBridgeToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 10);
	pMeshToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 11);
	pParticleToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 12);
	pSquadToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 13);
	p3DRoadToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 14);
	pChapterToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 15);
	pEffectToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 16);
	pInfantryToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 17);
	pMissionToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR + 18);
		
/*	
	if ( CreateGUIFrame() )
		return -1;
*/
	if ( CreateAnimationFrame() )
		return -1;
	if ( CreateSpriteFrame() )
		return -1;
	if ( CreateEffectFrame() )
		return -1;
	if ( CreateObjectFrame() )
		return -1;
	if ( CreateMeshFrame() )
		return -1;
	if ( CreateWeaponFrame() )
		return -1;
	if ( CreateBuildingFrame() )
		return -1;
	if ( CreateTileSetFrame() )
		return -1;
	if ( CreateFenceFrame() )
		return -1;
	if ( CreateParticleFrame() )
		return -1;
	if ( CreateTrenchFrame() )
		return -1;
	if ( CreateSquadFrame() )
		return -1;
	if ( CreateMineFrame() )
		return -1;
	if ( CreateBridgeFrame() )
		return -1;
	if ( CreateMissionFrame() )
		return -1;
	if ( CreateChapterFrame() )
		return -1;
	if ( CreateCampaignFrame() )
		return -1;
	if ( Create3DRoadFrame() )
		return -1;
	if ( Create3DRiverFrame() )
		return -1;
	if ( CreateMedalFrame() )
		return -1;
	
	DockControlToLeft( pObjectToolBar );
	DockControlToLeft( pTileToolBar );
	DockControlToLeft( pBuildingToolBar );
	DockControlToLeft( pFenceToolBar );
	DockControlToLeft( pBridgeToolBar );
	DockControlToLeft( pMissionToolBar );
	DockControlToLeft( pMeshToolBar );
	DockControlToLeft( pParticleToolBar );
	DockControlToLeft( pSquadToolBar );
	DockControlToLeft( p3DRoadToolBar );
	DockControlToLeft( pChapterToolBar );
	DockControlToLeft( pEffectToolBar );
	DockControlToLeft( pInfantryToolBar );
	pCommonToolBar->DestroyWindow();		//��� ��� ������ ���� ������

	GetSingleton<ICursor>()->Show( false );
	GetSingleton<ICursor>()->SetMode( 2 );
	SetTitle( "Blitzkrieg Resource Editor" );

	return 0;
}

void CMainFrame::DockControlToLeft(SECCustomToolBar *pBar)
{
	if ( pBar == 0 || !::IsWindow(pBar->GetSafeHwnd()) )
		return;

	CRect rect;
	
	RecalcLayout();
	pBar->GetWindowRect(&rect);
	rect.right = rect.Width();
	rect.left = 0;
/*
	rect.bottom = rect.Height();			//�� ���� ���������, ����� ����� ���� ��� ����
	rect.top = 0;
*/

	UINT n = AFX_IDW_DOCKBAR_TOP;
	
	DockControlBar( pBar, n, &rect );

}

int CMainFrame::CreateGUIFrame()
{
	NI_ASSERT_T( 0, "This module is swtiched off" );
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CGUIFrame), IDR_GUI_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 0 );
	CGUIFrame *pFrame = static_cast<CGUIFrame *> ( g_frameManager.GetFrame( CFrameManager::E_GUI_FRAME ) );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 98);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;

	if (!pGUIPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create GUI Property docking window\n"));
		return -1;
	}
	pGUIPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pGUIPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.50f, 220);

	nID = SECControlBar::GetUniqueBarID(this, 99);
	if (!pGUITreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create GUI Tree Dock Bar\n"));
		return -1;
	}
	pGUITreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pGUITreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 1.0f, 220 );

	nID = SECControlBar::GetUniqueBarID(this, 100);
	if (!pGUIPropertyDockBar->Create(this, _T("Property Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Property Dock Bar\n"));
		return -1;
	}
	pGUIPropertyDockBar->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pGUIPropertyDockBar, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.50f, 300 );

	pFrame->SetTreeDockBar( pGUITreeDockWnd );
	pFrame->SetOIDockBar( pGUIPropView );
	pFrame->SetPropertyDockBar( pGUIPropertyDockBar );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateAnimationFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CAnimationFrame), IDR_ANIMATIONS_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 0 );
/*
	if ( pwsh )
	{
		pwsh->SetWindowText( "Animations" );
		pwsh->SetTitle( "Animations" );
	}
*/
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );

	int nID = SECControlBar::GetUniqueBarID(this, 101);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;

	if (!pAnimPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Animation Property docking window\n"));
		return -1;
	}
	pAnimPropView->EnableDocking( CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT );
	DockControlBarEx(pAnimPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);

	nID = SECControlBar::GetUniqueBarID(this, 102);
	if (!pAnimTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Animations Tree Dock Bar\n"));
		return -1;
	}
	pAnimTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pAnimTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pAnimTreeDockWnd );
	pFrame->SetOIDockBar( pAnimPropView );
	pFrame->ShowFrameWindows( SW_HIDE );
	pFrame->SetToolBar( pInfantryToolBar );
	
	return 0;
}

int CMainFrame::CreateSpriteFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CSpriteFrame), IDR_SPRITE_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 1 );
/*
	if ( pwsh )
	{
		pwsh->SetWindowText( "Sprite Compose" );
		pwsh->SetTitle( "Sprite" );
	}
*/
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );

	int nID = SECControlBar::GetUniqueBarID(this, 106);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;

	if (!pSpritePropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Sprite Property docking window\n"));
		return -1;
	}
	pSpritePropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pSpritePropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);

	nID = SECControlBar::GetUniqueBarID(this, 107);
	if (!pSpriteTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Sprite Tree Dock Bar\n"));
		return -1;
	}
	pSpriteTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pSpriteTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pSpriteTreeDockWnd );
	pFrame->SetOIDockBar( pSpritePropView );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}


int CMainFrame::CreateEffectFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CEffectFrame), IDR_EFFECT_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 2 );
/*
	if ( pwsh )
	{
		pwsh->SetWindowText( "Effect Compose" );
		pwsh->SetTitle( "Effect" );
	}
*/
	CEffectFrame *pFrame = static_cast<CEffectFrame *>( g_frameManager.GetFrame( CFrameManager::E_EFFECT_FRAME ) );
	pFrame->Init( GetSingleton<IGFX>() );

	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;

	int nID = SECControlBar::GetUniqueBarID(this, 108);
	if (!pEffectPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Effect Property docking window\n"));
		return -1;
	}
	pEffectPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pEffectPropView, AFX_IDW_DOCKBAR_RIGHT, 0, 0, 0.80f, 220);

	nID = SECControlBar::GetUniqueBarID(this, 201);
	if (!pEffectDirectionButtonDockBar->Create(this, _T("Direction Button Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Mesh Direction Button Dock Bar\n"));
		return -1;
	}
	pEffectDirectionButtonDockBar->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pEffectDirectionButtonDockBar, AFX_IDW_DOCKBAR_RIGHT, 0, 0, 0.20f, 220 );
	
	nID = SECControlBar::GetUniqueBarID(this, 109);
	if (!pEffectTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Effect Tree Dock Bar\n"));
		return -1;
	}
	pEffectTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pEffectTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.99f, 220 );
	
	pFrame->SetTreeDockBar( pEffectTreeDockWnd );
	pFrame->SetOIDockBar( pEffectPropView );
	pFrame->SetDirectionButtonDockBar( pEffectDirectionButtonDockBar );
	pFrame->SetToolBar( pEffectToolBar );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateObjectFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CObjectFrame), IDR_OBJECT_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 3 );
	CObjectFrame *pFrame = static_cast<CObjectFrame *> ( g_frameManager.GetFrame( CFrameManager::E_OBJECT_FRAME ) );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 110);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pObjectPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Object Property docking window\n"));
		return -1;
	}
	pObjectPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pObjectPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 111);
	if (!pObjectTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Object Tree Dock Bar\n"));
		return -1;
	}
	pObjectTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pObjectTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pObjectTreeDockWnd );
	pFrame->SetOIDockBar( pObjectPropView );
	pFrame->SetTranseparenceCombo( m_pObjectCombo );
	pFrame->SetToolBar( pObjectToolBar );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateMeshFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CMeshFrame), IDR_MESH_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 4 );
/*
	if ( pwsh )
	{
		pwsh->SetWindowText( "Mesh Compose" );
		pwsh->SetTitle( "Mesh" );
	}
*/
	CMeshFrame *pFrame = static_cast<CMeshFrame *> ( g_frameManager.GetFrame( CFrameManager::E_MESH_FRAME ) );
	pFrame->Init( GetSingleton<IGFX>() );
	
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;

	int nID = SECControlBar::GetUniqueBarID(this, 200);
	if (!pMeshDirectionButtonDockBar->Create(this, _T("Direction Button Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Mesh Direction Button Dock Bar\n"));
		return -1;
	}
	pMeshDirectionButtonDockBar->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pMeshDirectionButtonDockBar, AFX_IDW_DOCKBAR_RIGHT, 0, 0, 0.20f, 220 );
	
	nID = SECControlBar::GetUniqueBarID(this, 112);
	if (!pMeshPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Mesh Property docking window\n"));
		return -1;
	}
	pMeshPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pMeshPropView, AFX_IDW_DOCKBAR_RIGHT, 0, 0, 0.80f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 113);
	if (!pMeshTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Mesh Tree Dock Bar\n"));
		return -1;
	}
	pMeshTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pMeshTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.99f, 220 );

	pFrame->SetTreeDockBar( pMeshTreeDockWnd );
	pFrame->SetOIDockBar( pMeshPropView );
	pFrame->SetDirectionButtonDockBar( pMeshDirectionButtonDockBar );
	pFrame->SetToolBar( pMeshToolBar );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateWeaponFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CWeaponFrame), IDR_WEAPON_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 5 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_WEAPON_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 114);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pWeaponPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Weapon Property docking window\n"));
		return -1;
	}
	pWeaponPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pWeaponPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 115);
	if (!pWeaponTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Weapon Tree Dock Bar\n"));
		return -1;
	}
	pWeaponTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pWeaponTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pWeaponTreeDockWnd );
	pFrame->SetOIDockBar( pWeaponPropView );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateBuildingFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CBuildingFrame), IDR_BUILDING_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 6 );
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 116);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pBuildingPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Building Property docking window\n"));
		return -1;
	}
	pBuildingPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pBuildingPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 117);
	if (!pBuildingTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Building Tree Dock Bar\n"));
		return -1;
	}
	pBuildingTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pBuildingTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pBuildingTreeDockWnd );
	pFrame->SetOIDockBar( pBuildingPropView );
	pFrame->SetTranseparenceCombo( m_pBuildingCombo );
	pFrame->SetToolBar( pBuildingToolBar );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateTileSetFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CTileSetFrame), IDR_TILE_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 7 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 118);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pTilePropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Tile Property docking window\n"));
		return -1;
	}
	pTilePropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pTilePropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 119);
	if (!pTileTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Tile Tree Dock Bar\n"));
		return -1;
	}
	pTileTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pTileTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pTileTreeDockWnd );
	pFrame->SetOIDockBar( pTilePropView );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateFenceFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CFenceFrame), IDR_FENCE_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 8 );
	CFenceFrame *pFrame = static_cast<CFenceFrame *> ( g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME ) );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 122);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pFencePropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Fence Property docking window\n"));
		return -1;
	}
	pFencePropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pFencePropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 123);
	if (!pFenceTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Fence Tree Dock Bar\n"));
		return -1;
	}
	pFenceTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pFenceTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pFenceTreeDockWnd );
	pFrame->SetOIDockBar( pFencePropView );
	pFrame->SetTranseparenceCombo( m_pFenceCombo );
	pFrame->SetToolBar( pFenceToolBar );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateParticleFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CParticleFrame), IDR_PARTICLE_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 9 );
	CParticleFrame *pFrame = static_cast<CParticleFrame *> ( g_frameManager.GetFrame( CFrameManager::E_PARTICLE_FRAME ) );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 124);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pParticlePropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Particle Property docking window\n"));
		return -1;
	}
	pParticlePropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pParticlePropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.6f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 125);
	if (!pParticleTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Particle Tree Dock Bar\n"));
		return -1;
	}
	pParticleTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pParticleTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.6f, 220 );

	nID = SECControlBar::GetUniqueBarID(this, 126);
	if (!pParticleKeyFrameDockWnd->Create(this, _T("Dockable Key Frame"), dwStyle, CBRS_EX_BORDERSPACE, nID))
	{
		TRACE(_T("Failed to create Particle Key Frame Dock Wnd\n"));
		return -1;
	}
	pParticleKeyFrameDockWnd->EnableDocking( CBRS_ALIGN_BOTTOM );
	DockControlBarEx(pParticleKeyFrameDockWnd, AFX_IDW_DOCKBAR_BOTTOM, 1, 0, 1.0f, 220 );

	pFrame->SetTreeDockBar( pParticleTreeDockWnd );
	pFrame->SetOIDockBar( pParticlePropView );
	pFrame->SetKeyFrameDockBar( pParticleKeyFrameDockWnd );
	pFrame->SetToolBar( pParticleToolBar );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateTrenchFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CTrenchFrame), IDR_TRENCH_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 10 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_TRENCH_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 127);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pTrenchPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Trench Property docking window\n"));
		return -1;
	}
	pTrenchPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pTrenchPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 128);
	if (!pTrenchTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Trench Tree Dock Bar\n"));
		return -1;
	}
	pTrenchTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pTrenchTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pTrenchTreeDockWnd );
	pFrame->SetOIDockBar( pTrenchPropView );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateSquadFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CSquadFrame), IDR_SQUAD_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 11 );
	CSquadFrame *pFrame = static_cast<CSquadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME ) );
	pFrame->Init( GetSingleton<IGFX>() );
	
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;

	int nID = SECControlBar::GetUniqueBarID(this, 129);
	if (!pSquadDirectionButtonDockBar->Create(this, _T("Direction Button Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Squad Direction Button Dock Bar\n"));
		return -1;
	}
	pSquadDirectionButtonDockBar->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pSquadDirectionButtonDockBar, AFX_IDW_DOCKBAR_RIGHT, 0, 0, 0.20f, 220 );
	
	nID = SECControlBar::GetUniqueBarID(this, 130);
	if (!pSquadPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Squad Property docking window\n"));
		return -1;
	}
	pSquadPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pSquadPropView, AFX_IDW_DOCKBAR_RIGHT, 0, 0, 0.80f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 131);
	if (!pSquadTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Squad Tree Dock Bar\n"));
		return -1;
	}
	pSquadTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pSquadTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.99f, 220 );
	
	pFrame->SetTreeDockBar( pSquadTreeDockWnd );
	pFrame->SetOIDockBar( pSquadPropView );
	pFrame->SetDirectionButtonDockBar( pSquadDirectionButtonDockBar );
	pFrame->SetToolBar( pSquadToolBar );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateMineFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CMineFrame), IDR_MINE_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 12 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_MINE_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 132);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pMinePropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Mine Property docking window\n"));
		return -1;
	}
	pMinePropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pMinePropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 133);
	if (!pMineTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Mine Tree Dock Bar\n"));
		return -1;
	}
	pMineTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pMineTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pMineTreeDockWnd );
	pFrame->SetOIDockBar( pMinePropView );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateBridgeFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CBridgeFrame), IDR_BRIDGE_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 13 );
	CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 134);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pBridgePropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Bridge Property docking window\n"));
		return -1;
	}
	pBridgePropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pBridgePropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 135);
	if (!pBridgeTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Bridge Tree Dock Bar\n"));
		return -1;
	}
	pBridgeTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	DockControlBarEx(pBridgeTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pBridgeTreeDockWnd );
	pFrame->SetOIDockBar( pBridgePropView );
	pFrame->SetTranseparenceCombo( m_pBridgeCombo );
	pFrame->SetToolBar( pBridgeToolBar );
	pFrame->ShowFrameWindows( SW_HIDE );
	
	return 0;
}

int CMainFrame::CreateMissionFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CMissionFrame), IDR_MISSION_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 14 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 136);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pMissionPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Mission Property docking window\n"));
		return -1;
	}
	pMissionPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pMissionPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 137);
	if (!pMissionTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Mission Tree Dock Bar\n"));
		return -1;
	}
	pMissionTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	pFrame->ShowFrameWindows( SW_HIDE );
	DockControlBarEx(pMissionTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pMissionTreeDockWnd );
	pFrame->SetOIDockBar( pMissionPropView );
	pFrame->SetToolBar( pMissionToolBar );
	
	return 0;
}

int CMainFrame::CreateChapterFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CChapterFrame), IDR_CHAPTER_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 15 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 138);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pChapterPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Chapter Property docking window\n"));
		return -1;
	}
	pChapterPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pChapterPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 139);
	if (!pChapterTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Chapter Tree Dock Bar\n"));
		return -1;
	}
	pChapterTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	pFrame->ShowFrameWindows( SW_HIDE );
	DockControlBarEx(pChapterTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pChapterTreeDockWnd );
	pFrame->SetOIDockBar( pChapterPropView );
	pFrame->SetToolBar( pChapterToolBar );
	
	return 0;
}

int CMainFrame::CreateCampaignFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CCampaignFrame), IDR_CAMPAIGN_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 16 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 140);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pCampaignPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Campaign Property docking window\n"));
		return -1;
	}
	pCampaignPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pCampaignPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 141);
	if (!pCampaignTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Campaign Tree Dock Bar\n"));
		return -1;
	}
	pCampaignTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	pFrame->ShowFrameWindows( SW_HIDE );
	DockControlBarEx(pCampaignTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pCampaignTreeDockWnd );
	pFrame->SetOIDockBar( pCampaignPropView );
	
	return 0;
}

int CMainFrame::Create3DRoadFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(C3DRoadFrame), IDR_3DROAD_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 17 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_3DROAD_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 142);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!p3DRoadPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create 3DRoad Property docking window\n"));
		return -1;
	}
	p3DRoadPropView->EnableDocking( CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT );
	DockControlBarEx(p3DRoadPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 143);
	if (!p3DRoadTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create 3DRoad Tree Dock Bar\n"));
		return -1;
	}
	p3DRoadTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	pFrame->ShowFrameWindows( SW_HIDE );
	DockControlBarEx(p3DRoadTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( p3DRoadTreeDockWnd );
	pFrame->SetOIDockBar( p3DRoadPropView );
	pFrame->SetToolBar( p3DRoadToolBar );
	
	return 0;
}

int CMainFrame::Create3DRiverFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(C3DRiverFrame), IDR_3DRIVER_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 18 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 144);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!p3DRiverPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create 3DRiver Property docking window\n"));
		return -1;
	}
	p3DRiverPropView->EnableDocking( CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT );
	DockControlBarEx(p3DRiverPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 145);
	if (!p3DRiverTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create 3DRiver Tree Dock Bar\n"));
		return -1;
	}
	p3DRiverTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	pFrame->ShowFrameWindows( SW_HIDE );
	DockControlBarEx(p3DRiverTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( p3DRiverTreeDockWnd );
	pFrame->SetOIDockBar( p3DRiverPropView );
	pFrame->SetToolBar( p3DRoadToolBar );
	
	return 0;
}

int CMainFrame::CreateMedalFrame()
{
	CMDIChildWnd *pChildWnd = 0;
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CMedalFrame), IDR_MEDAL_EDITOR, NULL, m_hMDIAccel);
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	SECWorksheet *pwsh = GetWorksheet( 19 );
	CParentFrame *pFrame = g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME );
	pFrame->Init( GetSingleton<IGFX>() );
	
	int nID = SECControlBar::GetUniqueBarID(this, 140);
	DWORD dwStyle = WS_CHILD|CBRS_RIGHT|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
	DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
	
	if (!pMedalPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Medal Property docking window\n"));
		return -1;
	}
	pMedalPropView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
	DockControlBarEx(pMedalPropView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220);
	
	nID = SECControlBar::GetUniqueBarID(this, 141);
	if (!pMedalTreeDockWnd->Create(this, _T("Tree Dock Bar"), dwStyle, dwStyleEx, nID))
	{
		TRACE(_T("Failed to create Medal Tree Dock Bar\n"));
		return -1;
	}
	pMedalTreeDockWnd->EnableDocking( CBRS_ALIGN_RIGHT | CBRS_ALIGN_LEFT );
	pFrame->ShowFrameWindows( SW_HIDE );
	DockControlBarEx(pMedalTreeDockWnd, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.5f, 220 );
	pFrame->SetTreeDockBar( pMedalTreeDockWnd );
	pFrame->SetOIDockBar( pMedalPropView );
	
	return 0;
}

int CMainFrame::InitGameWindow()
{
	int nSizeX = GAME_SIZE_X;
	int nSizeY = GAME_SIZE_Y;

	if ( GetFileAttributesA( ".\\Data\\consts.xml" ) == INVALID_FILE_ATTRIBUTES &&
		 GetFileAttributesA( ".\\data\\consts.xml" ) == INVALID_FILE_ATTRIBUTES &&
		 GetFileAttributesA( "..\\..\\..\\..\\Data\\consts.xml" ) != INVALID_FILE_ATTRIBUTES )
	{
		SetCurrentDirectoryA( "..\\..\\..\\..\\" );
	}

	CPtr<IDataStorage> pStorage = OpenStorage( ".\\data\\*.pak", STREAM_ACCESS_READ, STORAGE_TYPE_MOD );
	if ( CPtr<IDataStorage> pMODStorage = OpenStorage( (theApp.GetDestDir() + "*.pak").c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON ) )
		pStorage->AddStorage( pMODStorage, "MOD" );
	if ( CPtr<IDataStorage> pDataStorage = OpenStorage( "..\\..\\..\\..\\Data\\", STREAM_ACCESS_READ, STORAGE_TYPE_FILE ) )
		pStorage->AddStorage( pDataStorage, "WORKSPACE_DATA" );
	if ( CPtr<IDataStorage> pSourceDataStorage = OpenStorage( "..\\..\\data\\", STREAM_ACCESS_READ, STORAGE_TYPE_FILE ) )
		pStorage->AddStorage( pSourceDataStorage, "SOURCE_DATA" );
	RegisterSingleton( IDataStorage::tidTypeID, pStorage );
	{
		CPtr<IObjectsDB> pODB = CreateObjectsDB();
		pODB->LoadDB();
		RegisterSingleton( IObjectsDB::tidTypeID, pODB );
		GetSLS()->SetGDB( pODB );
	}
	{
		CTableAccessor table = NDB::OpenDataTable( "consts.xml" );
		NMain::SetupGlobalVarConsts( table );
	}
	if ( NMain::Initialize( m_gameWnd.GetSafeHwnd(), AfxGetMainWnd()->GetSafeHwnd(), AfxGetMainWnd()->GetSafeHwnd(), false ) != true )
	{
		TRACE0( "Failed to initialize game modules\n" );
		AfxMessageBox( "Failed to initialize game modules. Check that required DLLs are available in the editor output folder.", MB_OK | MB_ICONEXCLAMATION );
		return -1;
	}
	GetSLS()->AddFactory( GetTreeItemObjectFactory() );
	
	{
		CPtr<IDataStream> pStream = OpenFileStream( ".\\config.cfg", STREAM_ACCESS_READ );
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
		GetSingleton<IInput>()->SerializeConfig( pDT );
	}
	
	bool bUseDXT = false;

	{
		CPtr<IGFX> pGFX = GetSingleton<IGFX>();
		
		pGFX->SetMode( nSizeX, nSizeY, 16, 0, GFXFS_WINDOWED, 0 );
		pGFX->SetCullMode( GFXC_CW );	// setup right-handed coordinate system
		SHMatrix matrix;
		CreateOrthographicProjectionMatrixRH( &matrix, nSizeX, nSizeY, -nSizeY, nSizeY*3 );
		pGFX->SetProjectionTransform( matrix );
		pGFX->EnableLighting( false );
		GetSingleton<ITextureManager>()->SetQuality( ITextureManager::TEXTURE_QUALITY_HIGH );
	}

	{
		CPtr<IGFXFont> pFont = GetSingleton<IFontManager>()->GetFont( "fonts\\medium" );
		GetSingleton<IGFX>()->SetFont( pFont );
	}
	
	ICursor *pCursor = GetSingleton<ICursor>();
	pCursor->SetBounds( 0, 0, 800, 600 );
	pCursor->SetMode( 2 );
	pCursor->SetPos( 400, 300 );

	IScene *pSG = GetSingleton<IScene>();
	while ( pSG->ToggleShow( SCENE_SHOW_WARFOG ) )
		;
	while ( pSG->ToggleShow( SCENE_SHOW_HAZE ) )
		;
	SetGlobalVar( "editor", 1 );
	CReferenceDialog::InitLists();
	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !SECWorkbook::PreCreateWindow(cs) )
		return FALSE;

/*
	if ( cs.style && FWS_ADDTOTITLE )
		cs.style &= ~FWS_ADDTOTITLE;
*/

	return TRUE;
}

void CMainFrame::UpdateStatusBarIndicators()
{
	
}

void CMainFrame::UpdateStatusBarCoordsIndicator( const POINT &pt )
{
	CString szText;
	szText.Format("x=%d, y=%d", pt.x, pt.y);
	SetStatusPaneTextIfPresent( m_wndStatusBar, ID_INDICATOR_COORDS, szText );
}

void CMainFrame::UpdateStatusBarControlIndicator( const RECT &rc )
{
	CString szText;
	szText.Format("(%d,%d)x(%d,%d)", rc.left, rc.top, rc.right, rc.bottom);
	SetStatusPaneTextIfPresent( m_wndStatusBar, ID_INDICATOR_CONTROL, szText );
}

void CMainFrame::UpdateStatusBarControlIndicator( const CTRect<float> &rc )
{
	CString szText;
	szText.Format("(%d,%d)x(%d,%d)", (int)rc.left, (int)rc.top, (int)rc.right, (int)rc.bottom);
	SetStatusPaneTextIfPresent( m_wndStatusBar, ID_INDICATOR_CONTROL, szText );
}

void CMainFrame::ClearStatusBarControlIndicator()
{
	SetStatusPaneTextIfPresent( m_wndStatusBar, ID_INDICATOR_CONTROL, "" );
}

void CMainFrame::ShowSECControlBar( SECControlBar *pControlBar, int nCommand )
{
	if ( !pControlBar )
		return;

	if ( nCommand == SW_SHOW )
		ShowControlBar( pControlBar, TRUE, FALSE );
	else
		ShowControlBar( pControlBar, FALSE, FALSE );
}

void CMainFrame::ShowSECToolBar( SECControlBar *pToolBar, int nCommand )
{
	if ( !pToolBar )
		return;

	if ( nCommand == SW_SHOW )
		ShowControlBar( pToolBar, TRUE, TRUE );
	else
		ShowControlBar( pToolBar, FALSE, FALSE );
}


LRESULT CMainFrame::OnCreateCombo( WPARAM wParam, LPARAM lParam )
{
	HWND hWnd		 = HWND(lParam);
	UINT nNotifyCode = HIWORD(wParam);
	UINT nIDCtl		 = LOWORD(wParam);
	ASSERT(::IsWindow(hWnd));
	CWnd* pWnd = CWnd::FromHandle(hWnd);

	switch(nIDCtl)
	{
		case IDC_BRIDGE_TRANSPARENCE:
		case IDC_OBJECT_TRANSPARENCE:
		case IDC_BUILDING_TRANSPARENCE:
		case IDC_FENCE_TRANSPARENCE:
		{
			ASSERT_KINDOF(CComboBox, pWnd);
			CComboBox* 	pCombo = (CComboBox*) pWnd;
			if ( nNotifyCode == SECWndBtn::WndInit )
			{
				pCombo->SendMessage(WM_SETFONT, (WPARAM) m_hComboFont);		
				pCombo->AddString( "0" );
				pCombo->AddString( "1" );
				pCombo->AddString( "2" );
				pCombo->AddString( "3" );
				pCombo->AddString( "4" );
				pCombo->AddString( "5" );
				pCombo->AddString( "6" );
				pCombo->AddString( "7" );
				pCombo->SetCurSel( 0 );

				if ( nIDCtl == IDC_BRIDGE_TRANSPARENCE )
					m_pBridgeCombo = pCombo;
				else if ( nIDCtl == IDC_OBJECT_TRANSPARENCE )
					m_pObjectCombo = pCombo;
				else if ( nIDCtl == IDC_BUILDING_TRANSPARENCE )
					m_pBuildingCombo = pCombo;
				else if ( nIDCtl == IDC_FENCE_TRANSPARENCE )
					m_pFenceCombo = pCombo;
			}
			break;
		}
	}

	return 0;
}

void CMainFrame::OnClose() 
{
	theApp.SaveNewFrameTypeToRegister();
	theApp.SaveRegisterData();

	SaveBarState( REG_BARSLAYOUT );
	SECWorkbook::OnClose();
}
