#include "StdAfx.h"

#include "../GFX/GFX.H"
#include "../Scene/Scene.h"
#include "../RandomMapGen/MapInfo_Types.h"
#include "../Scene/Terrain.h"

#include "editor.h"
#include "PropView.h"
#include "TreeItem.h"
#include "3DRoadTreeItem.h"
#include "3DRoadView.h"
#include "3DRoadFrm.h"
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


IMPLEMENT_DYNCREATE(C3DRoadFrame, CParentFrame)

BEGIN_MESSAGE_MAP(C3DRoadFrame, CParentFrame)
ON_WM_CREATE()
ON_COMMAND(ID_SWITCH_WIREFRAME, OnSwitchWireframeMode)
ON_UPDATE_COMMAND_UI(ID_SWITCH_WIREFRAME, OnUpdateSwitchWireframeMode)
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()


C3DRoadFrame::C3DRoadFrame()
{
	szComposerName = "Road Editor";
	szExtension = "*.3rd";
	szComposerSaveName = "Road3D_Composer_Project";
	nTreeRootItemID = E_3DROAD_ROOT_ITEM;
	nFrameType = CFrameManager::E_3DROAD_FRAME;
	pWndView = new C3DRoadView;
	szAddDir = "terrain\\sets\\";
	
	bMapLoaded = false;
	bWireFrameMode = 0;

	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB4444;
}

C3DRoadFrame::~C3DRoadFrame()
{
}

int C3DRoadFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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


void C3DRoadFrame::GFXDraw()
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

void C3DRoadFrame::ShowFrameWindows( int nCommand )
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

void C3DRoadFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
	LoadRoadMap();
}

void C3DRoadFrame::SpecificClearBeforeBatchMode()
{
}

void C3DRoadFrame::FillRPGStats( SVectorStripeObjectDesc &desc, CTreeItem *pRoot )
{
	NI_ASSERT( pRoot->GetItemType() == E_3DROAD_ROOT_ITEM );
	C3DRoadCommonPropsItem *pCommonPropsItem = static_cast<C3DRoadCommonPropsItem *> ( pRoot->GetChildItem( E_3DROAD_COMMON_PROPS_ITEM ) );

	desc.bottom.nNumCells = pCommonPropsItem->GetBottomWidth();

	C3DRoadLayerPropsItem *pLayerPropsItem = static_cast<C3DRoadLayerPropsItem *> ( pRoot->GetChildItem( E_3DROAD_LAYER_PROPS_ITEM ) );
	desc.bottom.bAnimated = false;
	desc.bottom.fDisturbance = 0;
	desc.bottom.fRelWidth = 1.0f;
	desc.bottom.fStreamSpeed = 0;
	desc.bottom.fTextureStep = pLayerPropsItem->GetTextureStep();
	desc.bottom.opacityBorder = pLayerPropsItem->GetBorderOpacity();
	desc.bottom.opacityCenter = pLayerPropsItem->GetCenterOpacity();
	desc.bottom.szTexture = pLayerPropsItem->GetTexture();
	
	desc.eType = SVectorStripeObjectDesc::TYPE_ROAD;
	desc.nPriority = pCommonPropsItem->GetPriority();
	desc.fPassability = pCommonPropsItem->GetPassability();

	desc.dwAIClasses = 0;
	if ( pCommonPropsItem->GetPassForInfantry() )
		desc.dwAIClasses |= AI_CLASS_HUMAN;
	if ( pCommonPropsItem->GetPassForWheels() )
		desc.dwAIClasses |= AI_CLASS_WHEEL;
	if ( pCommonPropsItem->GetPassForHalfTracks() )
		desc.dwAIClasses |= AI_CLASS_HALFTRACK;
	if ( pCommonPropsItem->GetPassForTracks() )
		desc.dwAIClasses |= AI_CLASS_TRACK;
	desc.dwAIClasses = ~desc.dwAIClasses;

	desc.eType = pCommonPropsItem->GetRoadType();

	desc.miniMapCenterColor = 0xffffffff;
	desc.miniMapBorderColor = 0x00000000;
	

	desc.miniMapCenterColor = pCommonPropsItem->GetMinimapCenterColor();
	desc.miniMapBorderColor = pCommonPropsItem->GetMinimapBorderColor();


	desc.bottomBorders.resize( 0 );

	if ( pCommonPropsItem->HasBorders() )
	{
		pLayerPropsItem = static_cast<C3DRoadLayerPropsItem *> ( pRoot->GetChildItem( E_3DROAD_LAYER_PROPS_ITEM, 1 ) );
		NI_ASSERT( pLayerPropsItem != 0 );
		desc.bottomBorders.resize( 2 );

		for ( int i=0; i<2; i++ )
		{
			SVectorStripeObjectDesc::SLayer &layer = desc.bottomBorders[i];
			layer.bAnimated = false;
			layer.nNumCells = 1;
			layer.fDisturbance = 0;
			layer.fRelWidth = pCommonPropsItem->GetBorderRelativeWidth();
			layer.fStreamSpeed = 0;
			layer.fTextureStep = pLayerPropsItem->GetTextureStep();
			layer.opacityBorder = pLayerPropsItem->GetBorderOpacity();
			layer.opacityCenter = pLayerPropsItem->GetCenterOpacity();
			layer.szTexture = pLayerPropsItem->GetTexture();
		}

		desc.bottom.fRelWidth = 1.0 - pCommonPropsItem->GetBorderRelativeWidth();
	}
	desc.cSoilParams = pCommonPropsItem->GetSoilParams();
}

void C3DRoadFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRoot, const char *pszProjectName )
{
	NI_ASSERT( !pDT->IsReading() );
	
	SVectorStripeObjectDesc desc;
	FillRPGStats( desc, pRoot );
	
	CTreeAccessor tree = pDT;
	tree.Add( "VSODescription", &desc );
}

void C3DRoadFrame::GetRPGStats( const SVectorStripeObjectDesc &desc, CTreeItem *pRootItem )
{
	C3DRoadCommonPropsItem *pCommonPropsItem = static_cast<C3DRoadCommonPropsItem *> ( pRootItem->GetChildItem( E_3DROAD_COMMON_PROPS_ITEM ) );
	pCommonPropsItem->SetBorderRelativeWidth( 1.0f - desc.bottom.fRelWidth );


	pCommonPropsItem->SetPriority( desc.nPriority );
	pCommonPropsItem->SetPassability( desc.fPassability );
	pCommonPropsItem->SetPassForInfantry( !(desc.dwAIClasses & AI_CLASS_HUMAN) );
	pCommonPropsItem->SetPassForWheels( !(desc.dwAIClasses & AI_CLASS_WHEEL) );
	pCommonPropsItem->SetPassForHalfTracks( !(desc.dwAIClasses & AI_CLASS_HALFTRACK) );
	pCommonPropsItem->SetPassForTracks( !(desc.dwAIClasses & AI_CLASS_TRACK) );

	pCommonPropsItem->SetRoadType( desc.eType );

	pCommonPropsItem->SetMinimapBorderColor( desc.miniMapBorderColor );
	pCommonPropsItem->SetMinimapCenterColor( desc.miniMapCenterColor );

	if ( desc.bottomBorders.size() > 0 )
	{
		C3DRoadLayerPropsItem *pLayerProps = new C3DRoadLayerPropsItem;

		pLayerProps->SetItemName( "Border layer" );
		pLayerProps->SetTextureStep( desc.bottomBorders[0].fTextureStep );
		pLayerProps->SetBorderOpacity( desc.bottomBorders[0].opacityBorder );
		pLayerProps->SetCenterOpacity( desc.bottomBorders[0].opacityCenter );
		pLayerProps->SetTexture( desc.bottomBorders[0].szTexture.c_str() );

		pRootItem->AddChild( pLayerProps );
	}
	pCommonPropsItem->SetSoilParams( desc.cSoilParams );
}

void C3DRoadFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	ASSERT( pDT->IsReading() );
	
	SVectorStripeObjectDesc desc;
	CTreeAccessor tree = pDT;
	tree.Add( "VSODescription", &desc );
	
	GetRPGStats( desc, pRootItem );
	LoadRoadMap();
	UpdateRoadView();
}

bool C3DRoadFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	return true;
}

void C3DRoadFrame::UpdateRoadView()
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
	FillRPGStats( rTerrainInfo.roads3[0], pRootItem );
	pTerrainEditor->UpdateRoad( rTerrainInfo.roads3[0].nID );
	FillRPGStats( rTerrainInfo.roads3[1], pRootItem );
	pTerrainEditor->UpdateRoad( rTerrainInfo.roads3[1].nID );
	GFXDraw();
}

void C3DRoadFrame::LoadRoadMap()
{
	if ( bMapLoaded )
		return;
	
	std::string szTerrainName = "maps\\road3d";
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

void C3DRoadFrame::OnSwitchWireframeMode() 
{
	bWireFrameMode = !bWireFrameMode;
	GFXDraw();
}

void C3DRoadFrame::OnUpdateSwitchWireframeMode(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}

void C3DRoadFrame::OnSetFocus(CWnd* pOldWnd) 
{
	if ( pTreeDockBar && pTreeDockBar->GetTreeWithIndex( 0 ) )
	{
		LoadRoadMap();
		UpdateRoadView();
	}
	CParentFrame::OnSetFocus(pOldWnd);
}

