#include "stdafx.h"

#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\Scene\Terrain.h"

#include "editor.h"
#include "PropView.h"
#include "TreeItem.h"
#include "3DRiverTreeItem.h"
#include "3DRiverView.h"
#include "3DRiverFrm.h"
#include "GameWnd.h"
#include "frames.h"
#include "SetDirDialog.h"
#include "KeyFrameDock.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(C3DRiverFrame, CParentFrame)

BEGIN_MESSAGE_MAP(C3DRiverFrame, CParentFrame)
ON_WM_CREATE()
ON_COMMAND(ID_SWITCH_WIREFRAME, OnSwitchWireframeMode)
ON_UPDATE_COMMAND_UI(ID_SWITCH_WIREFRAME, OnUpdateSwitchWireframeMode)
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()


C3DRiverFrame::C3DRiverFrame()
{
	szComposerName = "River Editor";
	szExtension = "*.3rv";
	szComposerSaveName = "River3D_Composer_Project";
	nTreeRootItemID = E_3DRIVER_ROOT_ITEM;
	nFrameType = CFrameManager::E_3DRIVER_FRAME;
	pWndView = new C3DRiverView;
	szAddDir = "terrain\\sets\\";
	
	bMapLoaded = false;
	bWireFrameMode = 0;

	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB4444;
}

C3DRiverFrame::~C3DRiverFrame()
{
}

int C3DRiverFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CParentFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );

	if (!pWndView->Create(NULL, NULL,  WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}
	
	return 0;
}


void C3DRiverFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();
	pGFX->SetWireframe( bWireFrameMode );
	
	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	
	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
  pSG->Draw( pCamera );
	
	pGFX->EndScene();
	pGFX->Flip();
}

void C3DRiverFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( SW_SHOW );
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->SetAnchor( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );
	
	IScene *pSG = GetSingleton<IScene>();
	if ( nCommand == SW_SHOW )
	{
		while ( !pSG->ToggleShow( SCENE_SHOW_TERRAIN ) )
			;
	}
	else
	{
		while ( pSG->ToggleShow( SCENE_SHOW_TERRAIN ) )
			;
	}
}

void C3DRiverFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
	LoadRiverMap();
	UpdateRiverView();
}

void C3DRiverFrame::SpecificClearBeforeBatchMode()
{
}

void C3DRiverFrame::FillRPGStats( SVectorStripeObjectDesc &desc, CTreeItem *pRoot )
{
	NI_ASSERT( pRoot->GetItemType() == E_3DRIVER_ROOT_ITEM );

	C3DRiverBottomLayerPropsItem *pBottomLayerPropsItem = static_cast<C3DRiverBottomLayerPropsItem *> ( pRoot->GetChildItem( E_3DRIVER_BOTTOM_LAYER_PROPS_ITEM ) );
	desc.szAmbientSound = pBottomLayerPropsItem->GetAmbientSound();
	desc.bottom.nNumCells = pBottomLayerPropsItem->GetBottomWidth();
	desc.bottom.bAnimated = false;
	desc.bottom.fDisturbance = 0;
	desc.bottom.fRelWidth = 1.0f;
	desc.bottom.fStreamSpeed = 0;
	desc.bottom.fTextureStep = pBottomLayerPropsItem->GetTextureStep();
	desc.bottom.opacityBorder = pBottomLayerPropsItem->GetBorderOpacity();
	desc.bottom.opacityCenter = pBottomLayerPropsItem->GetCenterOpacity();
	desc.bottom.szTexture = pBottomLayerPropsItem->GetTexture();

	desc.layers.clear();
	C3DRiverLayersItem *pLayersItem = static_cast<C3DRiverLayersItem *> ( pRoot->GetChildItem( E_3DRIVER_LAYERS_ITEM ) );
	for ( CTreeItem::CTreeItemList::const_iterator it=pLayersItem->GetBegin(); it!=pLayersItem->GetEnd(); ++it )
	{
		SVectorStripeObjectDesc::SLayer layer;
		C3DRiverLayerPropsItem *pLayerPropsItem = static_cast<C3DRiverLayerPropsItem *> ( it->GetPtr() );
		layer.nNumCells = pBottomLayerPropsItem->GetBottomWidth();
		layer.bAnimated = pLayerPropsItem->GetAnimatedFlag();
		layer.fDisturbance = pLayerPropsItem->GetDisturbance();
		layer.fRelWidth = 1.0f;
		layer.fStreamSpeed = pLayerPropsItem->GetStreamSpeed();
		layer.fTextureStep = pLayerPropsItem->GetTextureStep();
		layer.opacityBorder = pLayerPropsItem->GetBorderOpacity();
		layer.opacityCenter = pLayerPropsItem->GetCenterOpacity();
		layer.szTexture = pLayerPropsItem->GetTexture();
		desc.layers.push_back( layer );
	}
}

void C3DRiverFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRoot, const char *pszProjectName )
{
	NI_ASSERT( !pDT->IsReading() );
	
	SVectorStripeObjectDesc desc;
	FillRPGStats( desc, pRoot );
	
	CTreeAccessor tree = pDT;
	tree.Add( "VSODescription", &desc );
}

void C3DRiverFrame::GetRPGStats( const SVectorStripeObjectDesc &desc, CTreeItem *pRootItem )
{
}

void C3DRiverFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	ASSERT( pDT->IsReading() );
	
	SVectorStripeObjectDesc desc;
	CTreeAccessor tree = pDT;
	tree.Add( "VSODescription", &desc );
	
	GetRPGStats( desc, pRootItem );
	LoadRiverMap();
	UpdateRiverView();
}

bool C3DRiverFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	return true;
}

void C3DRiverFrame::UpdateRiverView()
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
	FillRPGStats( rTerrainInfo.rivers[0], pRootItem );
	pTerrainEditor->UpdateRiver( rTerrainInfo.rivers[0].nID );
	GFXDraw();
}

void C3DRiverFrame::LoadRiverMap()
{
	if ( bMapLoaded )
		return;
	
	std::string szTerrainName = "maps\\river3d";
	CMapInfo mapinfo;
	{
		CPtr<IDataStream> pStreamXML = GetSingleton<IDataStorage>()->OpenStream( (szTerrainName + ".xml").c_str(), STREAM_ACCESS_READ );
		if ( pStreamXML == 0 )
		{
			std::string szErr = NStr::Format( "Error: Can not load %s.xml", szTerrainName.c_str() );
			AfxMessageBox( szErr.c_str() );
			return;
		}
		CTreeAccessor saver = CreateDataTreeSaver( pStreamXML, IDataTree::READ );
		saver.AddTypedSuper( &mapinfo );
	}
	
	ITerrain *pTerrain = CreateTerrain();
	pTerrain->Load( szTerrainName.c_str(), mapinfo.terrain );
	GetSingleton<IScene>()->SetTerrain( pTerrain );
}

void C3DRiverFrame::OnSwitchWireframeMode() 
{
	bWireFrameMode = !bWireFrameMode;
	GFXDraw();
}

void C3DRiverFrame::OnUpdateSwitchWireframeMode(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}

BOOL C3DRiverFrame::Run()
{
	GFXDraw();
	g_frameManager.GetGameWnd()->ValidateRect( 0 );
	return TRUE;
}

void C3DRiverFrame::OnSetFocus(CWnd* pOldWnd) 
{
	if ( pTreeDockBar && pTreeDockBar->GetTreeWithIndex( 0 ) )
	{
		LoadRiverMap();
		UpdateRiverView();
	}
	CParentFrame::OnSetFocus(pOldWnd);	
}

