#include "stdafx.h"
#include <io.h>
#include "..\Common\StingrayCompat.h"

#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"
#include "..\Main\rpgstats.h"

#include "common.h"
#include "editor.h"
#include "TreeDockWnd.h"
#include "SpriteCompose.h"
#include "MainFrm.h"
#include "PropView.h"
#include "TreeItem.h"
#include "ObjTreeItem.h"
#include "BuildTreeItem.h"
#include "BuildFrm.h"
#include "BuildView.h"
#include "GameWnd.h"
#include "frames.h"
#include "RefDlg.h"

static const int zeroSizeX = 32;
static const int zeroSizeY = 32;
static const float zeroShiftX = 15.4f;
static const float zeroShiftY = 15.4f;

static const int MIN_OPACITY = 120;
static const int MAX_OPACITY = 255;

static const int LINE_LENGTH = 100;			//длина линии, используемой для задания конуса стрельбы
static const int EDGE_LENGTH = 200;			//длина ребра конуса

static const int SHOOT_PICTURE_SIZE = 8;

/////////////////////////////////////////////////////////////////////////////
// CBuildingFrame

IMPLEMENT_DYNCREATE(CBuildingFrame, CGridFrame)

BEGIN_MESSAGE_MAP(CBuildingFrame, CGridFrame)
	//{{AFX_MSG_MAP(CBuildingFrame)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_COMMAND(ID_DRAW_GRID, OnDrawGrid)
	ON_COMMAND(ID_MOVE_OBJECT, OnMoveObject)
	ON_UPDATE_COMMAND_UI(ID_MOVE_OBJECT, OnUpdateMoveObject)
	ON_UPDATE_COMMAND_UI(ID_DRAW_GRID, OnUpdateDrawGrid)
	ON_WM_LBUTTONUP()
	ON_COMMAND(ID_SET_ZERO_BUTTON, OnSetZeroButton)
	ON_UPDATE_COMMAND_UI(ID_SET_ZERO_BUTTON, OnUpdateSetZeroButton)
	ON_COMMAND(ID_SET_ENTRANCE_BUTTON, OnSetEntranceButton)
	ON_UPDATE_COMMAND_UI(ID_SET_ENTRANCE_BUTTON, OnUpdateSetEntranceButton)
	ON_COMMAND(ID_SET_SHOOT_POINT, OnSetShootPoint)
	ON_UPDATE_COMMAND_UI(ID_SET_SHOOT_POINT, OnUpdateSetShootPoint)
	ON_COMMAND(ID_SET_SHOOT_ANGLE, OnSetShootAngle)
	ON_UPDATE_COMMAND_UI(ID_SET_SHOOT_ANGLE, OnUpdateSetShootAngle)
	ON_COMMAND(ID_SET_HORIZONTAL_SHOOT, OnSetHorizontalShoot)
	ON_UPDATE_COMMAND_UI(ID_SET_HORIZONTAL_SHOOT, OnUpdateSetHorizontalShoot)
	ON_CBN_SETFOCUS( IDC_BUILDING_TRANSPARENCE, OnSetFocusTranseparence )
	ON_CBN_SELCHANGE( IDC_BUILDING_TRANSPARENCE, OnChangeTranseparence )
	ON_UPDATE_COMMAND_UI(ID_BUILDING_TRANSPARENCE, OnUpdateDrawTransparence)
	ON_COMMAND(ID_SET_FIRE_POINT, OnSetFirePoint)
	ON_UPDATE_COMMAND_UI(ID_SET_FIRE_POINT, OnUpdateSetFirePoint)
	ON_COMMAND(ID_MOVE_POINT, OnMovePoint)
	ON_UPDATE_COMMAND_UI(ID_MOVE_POINT, OnUpdateMovePoint)
	ON_COMMAND(ID_SET_DIRECTION_EXPLOSION, OnSetDirectionExplosion)
	ON_UPDATE_COMMAND_UI(ID_SET_DIRECTION_EXPLOSION, OnUpdateSetDirectionExplosion)
	ON_COMMAND(ID_SET_SMOKE_POINT, OnSetSmokePoint)
	ON_UPDATE_COMMAND_UI(ID_SET_SMOKE_POINT, OnUpdateSetSmokePoint)
	ON_COMMAND(ID_GENERATE_POINTS, OnGeneratePoints)
	ON_UPDATE_COMMAND_UI(ID_GENERATE_POINTS, OnUpdateGeneratePoints)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBuildingFrame construction/destruction

CBuildingFrame::CBuildingFrame()
{
	szComposerName = "Building Editor";
	szExtension = "*.bld";
	szComposerSaveName = "Building_Composer_Project";
	nTreeRootItemID = E_BUILDING_ROOT_ITEM;
	nFrameType = CFrameManager::E_BUILDING_FRAME;
	pWndView = new CBuildingView;
	szAddDir = "buildings\\";
	
	m_mode = -1;
	pActiveShootPoint = 0;
	pActiveFirePoint = 0;
	pActiveDirExpPoint = 0;
	pActiveSmokePoint = 0;
	eActiveMode = E_SHOOT_SLOT;
	eActiveSubMode = E_SUB_MOVE;
	m_pTransparenceCombo = 0;
	m_transValue = 0;
	m_fMinY = m_fMaxY = m_fX = 0;
	m_SpriteLoadPos = CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0);

	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB1555;
}

CBuildingFrame::~CBuildingFrame()
{
}

void CBuildingFrame::Init( IGFX *_pGFX )
{
	CGridFrame::Init( _pGFX );

	pHorizontalPointVertices = pGFX->CreateVertices( 2, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
	{
		CVerticesLock<SGFXTLVertex> vertices( pHorizontalPointVertices );
		vertices[0].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[1].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}
	
	pConeVertices = pGFX->CreateVertices( 12, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
	{
		CVerticesLock<SGFXTLVertex> vertices( pConeVertices );
		vertices[0].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[1].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[2].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[3].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[4].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[5].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[6].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[7].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[8].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[9].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[10].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[11].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}
	
	pFireDirectionVertices = pGFX->CreateVertices( 6, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
	{
		CVerticesLock<SGFXTLVertex> vertices( pFireDirectionVertices );
		vertices[0].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[1].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[2].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[3].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[4].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[5].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}
}

int CBuildingFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CGridFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );
	
	// create a view to occupy the client area of the frame
	if (!pWndView->Create(NULL, NULL,  WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}
	
	//инициализируем уникальное имя для проекта
	GenerateProjectName();
	
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	pToolBar->SetButtonStyle( 0, TBBS_CHECKBOX | TBBS_CHECKED );
	pToolBar->SetButtonStyle( 1, TBBS_CHECKBOX );
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CBuildingFrame message handlers

void CBuildingFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( nCommand );
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->SetAnchor( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );
	
	if ( nCommand == SW_SHOW )
	{
		IScene *pSG = GetSingleton<IScene>();
		if ( pSprite )
			pSG->AddObject( pSprite, SGVOGT_UNIT );
	}
}

void CBuildingFrame::DrawLockedTiles( IGFX *pGFX )
{
	for ( CListOfTiles::iterator it=lockedTiles.begin(); it!=lockedTiles.end(); ++it )
		pGFX->Draw( it->pVertices, pMarkerIndices );
}

void CBuildingFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();

	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );

	CGridFrame::GFXDraw();

	if ( tbStyle == E_DRAW_GRID || tbStyle == E_MOVE_OBJECT )
	{
		DrawLockedTiles( pGFX );
	}

	if ( tbStyle == E_SET_ENTRANCE )
	{
		for ( CListOfTiles::iterator it=entrances.begin(); it!=entrances.end(); ++it )
			pGFX->Draw( it->pVertices, pMarkerIndices );
	}

/*
	if ( tbStyle != E_DRAW_TRANSEPARENCE )
	{
		for ( CListOfTiles::iterator it=lockedTiles.begin(); it!=lockedTiles.end(); ++it )
			pGFX->Draw( it->pVertices, pMarkerIndices );
		for ( CListOfTiles::iterator it=entrances.begin(); it!=entrances.end(); ++it )
			pGFX->Draw( it->pVertices, pMarkerIndices );
	}
*/

	if ( tbStyle == E_SET_SHOOT_POINT  )
	{
		DrawLockedTiles( pGFX );
		if ( pActiveShootPoint )
		{
			if ( eActiveSubMode == E_SUB_HOR )
				pGFX->Draw( pHorizontalPointVertices, 0 );
			else if ( eActiveSubMode == E_SUB_DIR )
				pGFX->Draw( pConeVertices, 0 );
		}
	}

	if ( tbStyle == E_SET_FIRE_POINT )
	{
		DrawLockedTiles( pGFX );
		if ( pActiveFirePoint )
		{
			if ( eActiveSubMode == E_SUB_HOR )
				pGFX->Draw( pHorizontalPointVertices, 0 );
			else if ( eActiveSubMode == E_SUB_DIR )
				pGFX->Draw( pFireDirectionVertices, 0 );
		}
	}
	
	if ( tbStyle == E_SET_DIRECTION_EXPLOSION )
	{
		DrawLockedTiles( pGFX );
		if ( pActiveDirExpPoint )
		{
			if ( eActiveSubMode == E_SUB_HOR )
				pGFX->Draw( pHorizontalPointVertices, 0 );
			else if ( eActiveSubMode == E_SUB_DIR )
				pGFX->Draw( pFireDirectionVertices, 0 );
		}
	}

	if ( tbStyle == E_SET_SMOKE_POINT )
	{
		DrawLockedTiles( pGFX );
		if ( pActiveSmokePoint )
		{
			if ( eActiveSubMode == E_SUB_HOR )
				pGFX->Draw( pHorizontalPointVertices, 0 );
			else if ( eActiveSubMode == E_SUB_DIR )
				pGFX->Draw( pFireDirectionVertices, 0 );
		}
	}

	if ( tbStyle == E_DRAW_TRANSEPARENCE )
	{
		for ( CListOfTiles::iterator it=transeparences.begin(); it!=transeparences.end(); ++it )
			pGFX->Draw( it->pVertices, pMarkerIndices );
	}
	
	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
	pCamera->Update();
  pSG->Draw( pCamera );
	
	
	pGFX->SetShadingEffect( 3 );
	SGFXRect2 rc;
	CVec2 zeroPos2;
	pGFX->SetTexture( 0, pKrestTexture );
	pSG->GetPos2( &zeroPos2, m_zeroPos );
	rc.rect = CTRect<float> ( zeroPos2.x, zeroPos2.y, zeroPos2.x+zeroSizeX, zeroPos2.y+zeroSizeY );
	rc.maps = CTRect<float> ( 0.0f, 0.0f, 1.0f, 1.0f );
	pGFX->SetupDirectTransform();
	pGFX->DrawRects( &rc, 1 );
	pGFX->RestoreTransform();
	
	pGFX->EndScene();
	pGFX->Flip();
}

void CBuildingFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();

	m_mode = -1;
	CreateKrest();
	//Загружаем спрайт
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	CTreeItem *pGraphicsItem = pRootItem->GetChildItem( E_BUILDING_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	CTreeItem *pSeason = pGraphicsItem->GetChildItem( E_BUILDING_SUMMER_PROPS_ITEM );
	NI_ASSERT( pSeason != 0 );
	CBuildingGraphicPropsItem *pGraphicPropsItem = (CBuildingGraphicPropsItem *) pSeason->GetChildItem( E_BUILDING_GRAPHIC1_PROPS_ITEM );
	SetActiveGraphicPropsItem( pGraphicPropsItem );
	
	//Создаем спрайты для всех direction explosion points
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	CBuildingDirExplosionsItem *pDirExpItem = (CBuildingDirExplosionsItem *) pRootItem->GetChildItem( E_BUILDING_DIR_EXPLOSIONS_ITEM );
	NI_ASSERT( pDirExpItem != 0 );
	for ( CTreeItem::CTreeItemList::const_iterator it=pDirExpItem->GetBegin(); it!=pDirExpItem->GetEnd(); ++it )
	{
		CBuildingDirExplosionPropsItem *pProps = static_cast<CBuildingDirExplosionPropsItem *> ( it->GetPtr() );
		
		CPtr<IObjVisObj> pObject;
		{
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
			NI_ASSERT( pObject != 0 );
			pObject->SetPosition( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			pProps->pSprite = pObject;
		}
		
		{
			//создаем спрайт - горизонтальную линию
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
			NI_ASSERT( pObject != 0 );
			pObject->SetPosition( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			pProps->pHLine = pObject;
		}
	}
}

void CBuildingFrame::SpecificClearBeforeBatchMode()
{
	lockedTiles.clear();
	entrances.clear();
	transeparences.clear();
	shootPoints.clear();
	firePoints.clear();
	pActiveShootPoint = 0;
	pActiveFirePoint = 0;
	pActiveDirExpPoint = 0;
	pActiveSmokePoint = 0;
	eActiveMode = E_SHOOT_SLOT;
	pActiveGraphicProps = 0;
	pSprite = 0;
	m_fMinY = m_fMaxY = m_fX = 0;
	m_SpriteLoadPos = CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0);
	pActiveGraphicProps = 0;
//	pKrestTexture = 0;
	m_zeroPos = CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0);

	GetSingleton<IScene>()->Clear();
}

BOOL CBuildingFrame::SpecificTranslateMessage( MSG *pMsg )
{
	if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_DELETE )
	{
		if ( GetFocus() != pWndView )
			return false;

		if ( eActiveMode == E_SHOOT_SLOT && pActiveShootPoint )
		{
			//удаляем текущий shoot point
			pActiveShootPoint->pSlot->DeleteMeInParentTreeItem();
			DeleteShootPoint( pActiveShootPoint->pSlot );
			GFXDraw();
			return true;
		}

		if ( eActiveMode == E_FIRE_POINT && pActiveFirePoint )
		{
			//удаляем текущий fire point
			pActiveFirePoint->pFirePoint->DeleteMeInParentTreeItem();
			DeleteFirePoint( pActiveFirePoint->pFirePoint );
			GFXDraw();
			return true;
		}

		if ( eActiveMode == E_SMOKE_POINT && pActiveSmokePoint )
		{
			//удаляем текущий smoke point
			CTreeItem *pTemp = pActiveSmokePoint;
			DeleteSmokePoint();
			pTemp->DeleteMeInParentTreeItem();
			GFXDraw();
			return true;
		}
		
		return true;
	}

	return false;
}

CTreeItem *CBuildingFrame::GetActiveShootPointItem()
{
	if ( pActiveShootPoint != 0 )
		return pActiveShootPoint->pSlot;
	return 0;
}

CTreeItem *CBuildingFrame::GetActiveFirePointItem()
{
	if ( pActiveFirePoint != 0 )
		return pActiveFirePoint->pFirePoint;
	return 0;
}

void CBuildingFrame::SetActiveGraphicPropsItem( CTreeItem *pGraphicProps )
{
	if ( pActiveGraphicProps == pGraphicProps )
		return;
	
	pActiveGraphicProps = pGraphicProps;
	CBuildingGraphicPropsItem *pGraphicPropsItem = static_cast<CBuildingGraphicPropsItem *> ( pActiveGraphicProps );
	NI_ASSERT( pGraphicPropsItem != 0 );
	
	//так как имя файла относительное, здесь я должен собрать полный путь
	string szDir = GetDirectory( szProjectFileName.c_str() );
	string szObjName;
	const char *pszFileName = pGraphicPropsItem->GetFileName();
	bool bRes = MakeFullPath( szDir.c_str(), pszFileName, szObjName );
	if ( !bRes )
		szObjName = pszFileName;		//popa
	
	LoadSprite( szObjName.c_str() );
}

void CBuildingFrame::UpdateActiveSprite()
{
	NI_ASSERT( pActiveGraphicProps != 0 );
	CBuildingGraphicPropsItem *pGraphicPropsItem = static_cast<CBuildingGraphicPropsItem *> ( pActiveGraphicProps );
	NI_ASSERT( pGraphicPropsItem != 0 );
	
	//так как имя файла относительное, здесь я должен собрать полный путь
	string szDir = GetDirectory( szProjectFileName.c_str() );
	string szObjName;
	const char *pszFileName = pGraphicPropsItem->GetFileName();
	bool bRes = MakeFullPath( szDir.c_str(), pszFileName, szObjName );
	if ( !bRes )
		szObjName = pszFileName;		//popa
	
	LoadSprite( szObjName.c_str() );
}

bool CBuildingFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem->GetItemType() == E_BUILDING_ROOT_ITEM );
	
	//Compose animation
	CBuildingTreeRootItem *pBuildingRoot = static_cast<CBuildingTreeRootItem *> ( pRootItem );
	CVec2 zeroPos2 = ::ComputeSpriteNewZeroPos( pSprite, m_zeroPos, CVec2(zeroShiftX, zeroShiftY) );

	//Тут понадобился массив залоченных тайлов
	SBuildingRPGStats buildingRPGStats;
	IScene *pSG = GetSingleton<IScene>();
	CVec2 krestPos2;
	pSG->GetPos2( &krestPos2, m_zeroPos );
	krestPos2.x += zeroShiftX;
	krestPos2.y += zeroShiftY;
	
	CVec3 realZeroPos3;
	pSG->GetPos3( &realZeroPos3, krestPos2 );

	CVec3 mostLeft3 = realZeroPos3;
	if ( lockedTiles.empty() )
	{
		buildingRPGStats.passability.SetSizes( 0, 0 );
		buildingRPGStats.vOrigin.x = 0;
		buildingRPGStats.vOrigin.y = 0;
	}
	else
	{
		//Сперва найдем минимальные и максимальные координаты тайлов в lockedTiles
		int nTileMinX = lockedTiles.front().nTileX, nTileMaxX = lockedTiles.front().nTileX;
		int nTileMinY = lockedTiles.front().nTileY, nTileMaxY = lockedTiles.front().nTileY;
		CListOfTiles::iterator it=lockedTiles.begin();
		for ( ++it; it!=lockedTiles.end(); ++it )
		{
			if ( nTileMinX > it->nTileX )
				nTileMinX = it->nTileX;
			else if ( nTileMaxX < it->nTileX )
				nTileMaxX = it->nTileX;
			
			if ( nTileMinY > it->nTileY )
				nTileMinY = it->nTileY;
			else if ( nTileMaxY < it->nTileY )
				nTileMaxY = it->nTileY;
		}
		
		buildingRPGStats.passability.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
		BYTE *pBuf = buildingRPGStats.passability.GetBuffer();
		for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
		{
			for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
			{
				for ( it=lockedTiles.begin(); it!=lockedTiles.end(); ++it )
				{
					if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
						break;
				}
				
				if ( it != lockedTiles.end() )
					pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = it->nVal;
				else
					pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = 0;
			}
		}
		
		float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
		CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
		CVec2 mostLeft2(fx2, fy2);
		pSG->GetPos3( &mostLeft3, mostLeft2 );
		
		buildingRPGStats.vOrigin.x = realZeroPos3.x - mostLeft3.x;
		buildingRPGStats.vOrigin.y = realZeroPos3.y - mostLeft3.y;
		
		//		pSG->MoveObject( pSprite, realPos3 );
		GFXDraw();
	}
	pBuildingRoot->ComposeAnimations( pszProjectName, GetDirectory(pszResultFileName).c_str(), zeroPos2, buildingRPGStats.passability, buildingRPGStats.vOrigin );
	
	//загрузим дефалтовый спрайт
	CTreeItem *pGraphicsItem = pRootItem->GetChildItem( E_BUILDING_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	CTreeItem *pSeason = pGraphicsItem->GetChildItem( E_BUILDING_SUMMER_PROPS_ITEM );
	NI_ASSERT( pSeason != 0 );
	CBuildingGraphicPropsItem *pGraphicPropsItem = (CBuildingGraphicPropsItem *) pSeason->GetChildItem( E_BUILDING_GRAPHIC1_PROPS_ITEM );

	//так как имя файла относительное, здесь я должен собрать полный путь
	string szObjName;
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pGraphicPropsItem->GetFileName(), szObjName );
	LoadSprite( szObjName.c_str() );
//	SetActiveGraphicPropsItem( pGraphicPropsItem );		//загружаем спрайт
	if ( pSprite == 0 )
	{
		std::string szErr = "Error: Can not load default sprite\n";
		szErr += pGraphicPropsItem->GetFileName();
		szErr += "\nCan not continue export data";
		AfxMessageBox( szErr.c_str() );

		return false;
	}

	//Сохраняем RPG stats
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	
	//создадим файл icon.tga с изображением домика

	std::string szFullName;
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pGraphicPropsItem->GetFileName(), szFullName );
	std::string szResName = GetDirectory( pszResultFileName );
	szResName += "icon.tga";
	SaveIconFile( szFullName.c_str(), szResName.c_str() );
	
	return true;
}

void CBuildingFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	if ( pSprite == 0 || bNewProjectJustCreated )
		return;

	ASSERT( !pDT->IsReading() );
	
	IScene *pSG = GetSingleton<IScene>();
	CVec2 krestPos2;
	pSG->GetPos2( &krestPos2, m_zeroPos );
	krestPos2.x += zeroShiftX;
	krestPos2.y += zeroShiftY;
	
	CVec3 realZeroPos3;
	pSG->GetPos3( &realZeroPos3, krestPos2 );
	
	SBuildingRPGStats buildingRPGStats;
	CBuildingCommonPropsItem *pCommonProps = static_cast<CBuildingCommonPropsItem *> ( pRootItem->GetChildItem( E_BUILDING_COMMON_PROPS_ITEM ) );
	buildingRPGStats.nRestSlots = pCommonProps->GetNumberOfRestSlots();
	buildingRPGStats.nMedicalSlots = pCommonProps->GetNumberOfMedicalSlots();
	buildingRPGStats.fMaxHP = pCommonProps->GetHealth();
	buildingRPGStats.fRepairCost = pCommonProps->GetRepairCost();
	buildingRPGStats.eType = ( SBuildingRPGStats::EType ) pCommonProps->GetBuildingType();
	buildingRPGStats.szKeyName = pCommonProps->GetName();
	buildingRPGStats.szAmbientSound = pCommonProps->GetAmbientSound();
	buildingRPGStats.szCycledSound = pCommonProps->GetCycledSound();
	
	CTreeItem *pPasses = pRootItem->GetChildItem( E_BUILDING_PASSES_ITEM );
	NI_ASSERT( pPasses != 0 );
	for ( CTreeItem::CTreeItemList::const_iterator it=pPasses->GetBegin(); it!=pPasses->GetEnd(); ++it )
	{
		CBuildingPassPropsItem *pPassProps = static_cast<CBuildingPassPropsItem *> ( it->GetPtr() );
		buildingRPGStats.dwAIClasses |= pPassProps->GetPassAIClass();
	}
	
	CTreeItem *pDefencesItem = pRootItem->GetChildItem( E_BUILDING_DEFENCES_ITEM );
	for ( int i=0; i<6; i++ )
	{
		CBuildingDefencePropsItem *pDefProps = static_cast<CBuildingDefencePropsItem *> ( pDefencesItem->GetChildItem( E_BUILDING_DEFENCE_PROPS_ITEM, i ) );
		int nIndex = 0;
		if ( string( "Left" ) == pDefProps->GetItemName() )
			nIndex = RPG_LEFT;
		else if ( string( "Right" ) == pDefProps->GetItemName() )
			nIndex = RPG_RIGHT;
		else if ( string( "Top" ) == pDefProps->GetItemName() )
			nIndex = RPG_TOP;
		else if ( string( "Bottom" ) == pDefProps->GetItemName() )
			nIndex = RPG_BOTTOM;
		else if ( string( "Front" ) == pDefProps->GetItemName() )
			nIndex = RPG_FRONT;
		else if ( string( "Back" ) == pDefProps->GetItemName() )
			nIndex = RPG_BACK;
		
		buildingRPGStats.defences[ nIndex ].nArmorMin = pDefProps->GetMinArmor();
		buildingRPGStats.defences[ nIndex ].nArmorMax = pDefProps->GetMaxArmor();
		buildingRPGStats.defences[ nIndex ].fSilhouette = pDefProps->GetSilhouette();
	}

	// Сохраняем данные о тайловой проходимости
	CVec3 mostLeft3 = realZeroPos3;
	if ( lockedTiles.empty() )
	{
		buildingRPGStats.passability.SetSizes( 0, 0 );
		buildingRPGStats.vOrigin.x = 0;
		buildingRPGStats.vOrigin.y = 0;
	}
	else
	{
		//Сперва найдем минимальные и максимальные координаты тайлов в lockedTiles
		int nTileMinX = lockedTiles.front().nTileX, nTileMaxX = lockedTiles.front().nTileX;
		int nTileMinY = lockedTiles.front().nTileY, nTileMaxY = lockedTiles.front().nTileY;
		CListOfTiles::iterator it=lockedTiles.begin();
		for ( ++it; it!=lockedTiles.end(); ++it )
		{
			if ( nTileMinX > it->nTileX )
				nTileMinX = it->nTileX;
			else if ( nTileMaxX < it->nTileX )
				nTileMaxX = it->nTileX;
			
			if ( nTileMinY > it->nTileY )
				nTileMinY = it->nTileY;
			else if ( nTileMaxY < it->nTileY )
				nTileMaxY = it->nTileY;
		}

		buildingRPGStats.passability.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
		BYTE *pBuf = buildingRPGStats.passability.GetBuffer();
		for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
		{
			for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
			{
				for ( it=lockedTiles.begin(); it!=lockedTiles.end(); ++it )
				{
					if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
						break;
				}
				
				if ( it != lockedTiles.end() )
					pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = it->nVal;
				else
					pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = 0;
			}
		}

		float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
		CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
		CVec2 mostLeft2(fx2, fy2);
		pSG->GetPos3( &mostLeft3, mostLeft2 );

		buildingRPGStats.vOrigin.x = realZeroPos3.x - mostLeft3.x;
		buildingRPGStats.vOrigin.y = realZeroPos3.y - mostLeft3.y;

//		pSG->MoveObject( pSprite, realPos3 );
		GFXDraw();
	}


	// Сохраняем данные о прозрачности объекта
	{
		if ( transeparences.empty() )
		{
			buildingRPGStats.visibility.SetSizes( 0, 0 );
			buildingRPGStats.vVisOrigin.x = 0;
			buildingRPGStats.vVisOrigin.y = 0;
		}
		else
		{
			//Сперва найдем минимальные и максимальные координаты тайлов в transeparences
			int nTileMinX = transeparences.front().nTileX, nTileMaxX = transeparences.front().nTileX;
			int nTileMinY = transeparences.front().nTileY, nTileMaxY = transeparences.front().nTileY;
			
			CListOfTiles::iterator it = transeparences.begin();
			++it;
			for ( ; it!=transeparences.end(); ++it )
			{
				int nTileX = it->nTileX;
				int nTileY = it->nTileY;
				
				if ( nTileMinX > nTileX )
					nTileMinX = nTileX;
				else if ( nTileMaxX < nTileX )
					nTileMaxX = nTileX;
				
				if ( nTileMinY > nTileY )
					nTileMinY = nTileY;
				else if ( nTileMaxY < nTileY )
					nTileMaxY = nTileY;
			}
			
			buildingRPGStats.visibility.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
			BYTE *pBuf = buildingRPGStats.visibility.GetBuffer();
			for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
			{
				for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
				{
					for ( it=transeparences.begin(); it!=transeparences.end(); ++it )
					{
						if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
							break;
					}

					if ( it != transeparences.end() )
						pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = it->nVal;
					else
						pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = 0;
				}
			}

			float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
			CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
			CVec2 mostLeft2(fx2, fy2);
			CVec3 mostLeft3;
			pSG->GetPos3( &mostLeft3, mostLeft2 );
			
			buildingRPGStats.vVisOrigin.x = realZeroPos3.x - mostLeft3.x;
			buildingRPGStats.vVisOrigin.y = realZeroPos3.y - mostLeft3.y;
		}
	}

	for ( CListOfTiles::iterator it=entrances.begin(); it!=entrances.end(); ++it )
	{
		SBuildingRPGStats::SEntrance entrancePoint;
		entrancePoint.bStormable = false;
		float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
		CGridFrame::GetGameTileCoordinates( it->nTileX, it->nTileY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
		CVec2 vCenter2((fx2 + fx4)/2, (fy1+fy3)/2);			//центра тайла
		CVec3 vCenter3;
		pSG->GetPos3( &vCenter3, vCenter2 );
		
		//Запишем координаты входа в здание
		entrancePoint.vPos.x = vCenter3.x - realZeroPos3.x;
		entrancePoint.vPos.y = vCenter3.y - realZeroPos3.y;
		entrancePoint.vPos.z = 0;

		//TODO добавить проверку на вшивость, чтобы точки входа в здание всегда были снаружи залоканных тайлов и отстояли от них максимум на один AI tile
/*
		//Запишем направление выхода из здания, имеем nTileX, nTileY это тайловые координаты входа, tileVector это массив всех залоченных тайлов
		if  ( !IsTileAlreadySet( nTileX - 1, nTileY ) &&
					!IsTileAlreadySet( nTileX + 1, nTileY ) &&
					!IsTileAlreadySet( nTileX, nTileY - 1 ) &&
					!IsTileAlreadySet( nTileX, nTileY + 1 ) )
			AfxMessageBox( "Error: The entrance point should be at the one AI tile outside the building\nThe editor can not compute the outside direction from building" );
*/
		buildingRPGStats.entrances.push_back( entrancePoint );
	}
	
	
	CTreeItem *pGraphicsItem = pRootItem->GetChildItem( E_BUILDING_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	CTreeItem *pSeason = pGraphicsItem->GetChildItem( E_BUILDING_SUMMER_PROPS_ITEM );
	NI_ASSERT( pSeason != 0 );
	CBuildingGraphicPropsItem *pGraphicPropsItem = (CBuildingGraphicPropsItem *) pSeason->GetChildItem( E_BUILDING_GRAPHIC1_PROPS_ITEM );

	//так как имя файла относительное, здесь я должен собрать полный путь
	string szDir = GetDirectory( pszProjectName );
	string szObjName;
	const char *pszFileName = pGraphicPropsItem->GetFileName();
	bool bRes = MakeFullPath( szDir.c_str(), pszFileName, szObjName );
	if ( !bRes )
		szObjName = pszFileName;		//popa
	
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	CPtr<IDataStream> pImageStream = OpenFileStream( szObjName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT( pImageStream != 0 );
	CPtr<IImage> pBuildingImage = pIP->LoadImage( pImageStream );
	NI_ASSERT( pBuildingImage != 0 );
	CImageAccessor imageAccessor( pBuildingImage );
	CVec2 zeroPos2( 0, 0 );
	if ( pSprite != 0 )
		zeroPos2 = ::ComputeSpriteNewZeroPos( pSprite, m_zeroPos, CVec2(zeroShiftX, zeroShiftY) );

	
	//Сохраняем позиции для всех shoot points
#ifdef _DEBUG
	CTreeItem *pSlotsItem = pRootItem->GetChildItem( E_BUILDING_SLOTS_ITEM );
	NI_ASSERT( pSlotsItem != 0 );
	NI_ASSERT( shootPoints.size() == pSlotsItem->GetChildsCount() );
#endif

	for ( CListOfShootPoints::iterator it=shootPoints.begin(); it!=shootPoints.end(); ++it )
	{
		CBuildingSlotPropsItem *pSlotProps = it->pSlot;
		SBuildingRPGStats::SSlot slot;
		
		if ( !it->pHLine )
			continue;

		slot.vPos = it->pHLine->GetPosition();
		//теперь вычислим относительную координату
		slot.vPos.x -= realZeroPos3.x;
		slot.vPos.y -= realZeroPos3.y;
		slot.vPos.z = 0;
		
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, it->pSprite->GetPosition() );
		//теперь вычислим относительную координату
		vPos2.x -= krestPos2.x;
		vPos2.y -= krestPos2.y;
		slot.vPicturePosition = vPos2;

		//TODO заполнить RPG параметры
		slot.bShowFlashes = false;

		slot.fAngle = pSlotProps->GetConeAngle();
		slot.fDirection = pSlotProps->GetConeDirection();
		slot.fSightMultiplier = pSlotProps->GetSightMultiplier();
		slot.fCoverage = pSlotProps->GetCover();
		slot.bBeforeSprite = ( slot.fDirection > 135.0 && slot.fDirection < 315.0 );
		
		slot.gun.szWeapon = pSlotProps->GetWeaponName();
		slot.gun.nAmmo = pSlotProps->GetAmmo();
		slot.fRotationSpeed = pSlotProps->GetRotationSpeed();
		slot.gun.nPriority = pSlotProps->GetPriority();
		//end of TODO

		slot.vWorldPosition = Get3DPosition( pSprite->GetPosition(), vPos2, zeroPos2, imageAccessor, 1, pGFX ); 
		buildingRPGStats.slots.push_back( slot );
	}

	
	//Сохраняем позиции для всех fire points
#ifdef _DEBUG
	CTreeItem *pFirePointsItem = pRootItem->GetChildItem( E_BUILDING_FIRE_POINTS_ITEM );
	NI_ASSERT( pFirePointsItem != 0 );
	NI_ASSERT( firePoints.size() == pFirePointsItem->GetChildsCount() );
#endif
	
	for ( CListOfFirePoints::iterator it=firePoints.begin(); it!=firePoints.end(); ++it )
	{
		CBuildingFirePointPropsItem *pFireProps = it->pFirePoint;
		SBuildingRPGStats::SFirePoint fire;
		
		if ( !it->pHLine )
			continue;
		
		fire.vPos = it->pHLine->GetPosition();
		//теперь вычислим относительную координату
		fire.vPos.x -= realZeroPos3.x;
		fire.vPos.y -= realZeroPos3.y;
		fire.vPos.z = 0;
		
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, it->pSprite->GetPosition() );
		//теперь вычислим относительную координату
		vPos2.x -= krestPos2.x;
		vPos2.y -= krestPos2.y;
		fire.vPicturePosition = vPos2;
		
		fire.fDirection = pFireProps->GetDirection();
		fire.szFireEffect = pFireProps->GetEffectName();
		fire.fVerticalAngle = pFireProps->GetVerticalAngle();
		
		fire.vWorldPosition = Get3DPosition( pSprite->GetPosition(), vPos2, zeroPos2, imageAccessor, 1, pGFX ); 
		buildingRPGStats.firePoints.push_back( fire );
	}
	
	//Сохраняем позиции для всех direction explosions
	CBuildingDirExplosionsItem *pDirExpItem = (CBuildingDirExplosionsItem *) pRootItem->GetChildItem( E_BUILDING_DIR_EXPLOSIONS_ITEM );
	NI_ASSERT( pDirExpItem != 0 );
	NI_ASSERT( buildingRPGStats.dirExplosions.size() == pDirExpItem->GetChildsCount() );

	buildingRPGStats.szDirExplosionEffect = pDirExpItem->GetEffectName();
	int nDirExpIndex = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pDirExpItem->GetBegin(); it!=pDirExpItem->GetEnd(); ++it )
	{
		CBuildingDirExplosionPropsItem *pProps = static_cast<CBuildingDirExplosionPropsItem *> ( it->GetPtr() );
		SBuildingRPGStats::SDirectionExplosion &dirProps = buildingRPGStats.dirExplosions[nDirExpIndex];
		
		if ( !pProps->pHLine )
			continue;
		
		dirProps.vPos = pProps->pHLine->GetPosition();
		//теперь вычислим относительную координату
		dirProps.vPos.x -= realZeroPos3.x;
		dirProps.vPos.y -= realZeroPos3.y;
		dirProps.vPos.z = 0;
		
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, pProps->pSprite->GetPosition() );
		//теперь вычислим относительную координату
		vPos2.x -= krestPos2.x;
		vPos2.y -= krestPos2.y;
		dirProps.vPicturePosition = vPos2;
		dirProps.fDirection = pProps->GetDirection();
		dirProps.fVerticalAngle = pProps->GetVerticalAngle();
		
		dirProps.vWorldPosition = Get3DPosition( pSprite->GetPosition(), vPos2, zeroPos2, imageAccessor, 1, pGFX ); 
		nDirExpIndex++;
	}


	//Сохраняем позиции для всех smoke points
	CBuildingSmokesItem *pSmokesItem = (CBuildingSmokesItem *) pRootItem->GetChildItem( E_BUILDING_SMOKES_ITEM );
	NI_ASSERT( pSmokesItem != 0 );
	
	buildingRPGStats.szSmokeEffect = pSmokesItem->GetEffectName();
	for ( CTreeItem::CTreeItemList::const_iterator it=pSmokesItem->GetBegin(); it!=pSmokesItem->GetEnd(); ++it )
	{
		CBuildingSmokePropsItem *pProps = static_cast<CBuildingSmokePropsItem *> ( it->GetPtr() );
		SBuildingRPGStats::SFirePoint smokeProps;
		
		if ( !pProps->pHLine )
			continue;
		
		smokeProps.vPos = pProps->pHLine->GetPosition();
		//теперь вычислим относительную координату
		smokeProps.vPos.x -= realZeroPos3.x;
		smokeProps.vPos.y -= realZeroPos3.y;
		smokeProps.vPos.z = 0;
		
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, pProps->pSprite->GetPosition() );
		//теперь вычислим относительную координату
		vPos2.x -= krestPos2.x;
		vPos2.y -= krestPos2.y;
		smokeProps.vPicturePosition = vPos2;
		smokeProps.fDirection = pProps->GetDirection();
		smokeProps.fVerticalAngle = pProps->GetVerticalAngle();

		smokeProps.vWorldPosition = Get3DPosition( pSprite->GetPosition(), vPos2, zeroPos2, imageAccessor, 1, pGFX ); 
		buildingRPGStats.smokePoints.push_back( smokeProps );
	}

	CTreeAccessor tree = pDT;
	tree.Add( "desc", &buildingRPGStats );
}

void CBuildingFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	ASSERT( pDT->IsReading() );
	IScene *pSG = GetSingleton<IScene>();
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	
	SBuildingRPGStats buildingRPGStats;
	CTreeAccessor tree = pDT;
	tree.Add( "desc", &buildingRPGStats );


	CBuildingCommonPropsItem *pCommonProps = static_cast<CBuildingCommonPropsItem *> ( pRootItem->GetChildItem( E_BUILDING_COMMON_PROPS_ITEM ) );
	pCommonProps->SetNumberOfRestSlots( buildingRPGStats.nRestSlots );
	pCommonProps->SetNumberOfMedicalSlots( buildingRPGStats.nMedicalSlots );
	pCommonProps->SetHealth( buildingRPGStats.fMaxHP );
	pCommonProps->SetRepairCost( buildingRPGStats.fRepairCost );
	pCommonProps->SetBuildingType( buildingRPGStats.eType );
	pCommonProps->SetName( buildingRPGStats.szKeyName.c_str() );
	pCommonProps->SetAmbientSound( buildingRPGStats.szAmbientSound.c_str() );
	pCommonProps->SetCycledSound( buildingRPGStats.szCycledSound.c_str() );

	CTreeItem *pDefencesItem = pRootItem->GetChildItem( E_BUILDING_DEFENCES_ITEM );
	for ( int i=0; i<6; i++ )
	{
		CBuildingDefencePropsItem *pDefProps = static_cast<CBuildingDefencePropsItem *> ( pDefencesItem->GetChildItem( E_BUILDING_DEFENCE_PROPS_ITEM, i ) );
		int nIndex = 0;
		if ( string( "Left" ) == pDefProps->GetItemName() )
			nIndex = RPG_LEFT;
		else if ( string( "Right" ) == pDefProps->GetItemName() )
			nIndex = RPG_RIGHT;
		else if ( string( "Top" ) == pDefProps->GetItemName() )
			nIndex = RPG_TOP;
		else if ( string( "Bottom" ) == pDefProps->GetItemName() )
			nIndex = RPG_BOTTOM;
		else if ( string( "Front" ) == pDefProps->GetItemName() )
			nIndex = RPG_FRONT;
		else if ( string( "Back" ) == pDefProps->GetItemName() )
			nIndex = RPG_BACK;
		
		pDefProps->SetMinArmor( buildingRPGStats.defences[nIndex].nArmorMin );
		pDefProps->SetMaxArmor( buildingRPGStats.defences[nIndex].nArmorMax );

		//CRAP коррекция результата
		if ( buildingRPGStats.defences[nIndex].fSilhouette < 0 || buildingRPGStats.defences[nIndex].fSilhouette > 1 )
			buildingRPGStats.defences[nIndex].fSilhouette = 1.0f;
		pDefProps->SetSilhouette( buildingRPGStats.defences[nIndex].fSilhouette );
		//End of CRAP
	}
	

	//Загружаем инфу о проходимости AI тайлов
	CVec3 beginPos3;						//координаты самого левого тайла, который связан с vOrigin
	beginPos3.x = 16*fWorldCellSize;
	beginPos3.y = 16*fWorldCellSize;
	beginPos3.z = 0;

	CVec2 realZeroPos2;
	CVec3 realZeroPos3;				//тут будет точная координата перекрестия
	{
		pSG->GetPos2( &realZeroPos2, m_zeroPos );
		realZeroPos2.x += zeroShiftX;
		realZeroPos2.y += zeroShiftY;
		pSG->GetPos3( &realZeroPos3, realZeroPos2 );
		
		beginPos3.x = realZeroPos3.x - buildingRPGStats.vOrigin.x;
		beginPos3.y = realZeroPos3.y - buildingRPGStats.vOrigin.y;
	}
//	AfxMessageBox( NStr::Format( "beginPos3 %lf, %lf, %lf", beginPos3.x, beginPos3.y, beginPos3.z ) );

	POINT pt;
	CVec2 pos2;
	float ftX, ftY;

	{
		pSG->GetPos2( &pos2, beginPos3 );
		//сдвинемся на центр тайла
		pt.x = pos2.x + fCellSizeX/2;
		pt.y = pos2.y;
		CGridFrame::ComputeGameTileCoordinates( pt, ftX, ftY );
		int nBeginTileX = ftX, nBeginTileY = ftY;
//		AfxMessageBox( NStr::Format( "nBeginTileX = %d, nBeginTileY = %d", nBeginTileX, nBeginTileY ) );
		
		BYTE *pBuf = buildingRPGStats.passability.GetBuffer();
		for ( int y=0; y<buildingRPGStats.passability.GetSizeY(); y++ )
		{
			for ( int x=0; x<buildingRPGStats.passability.GetSizeX(); x++ )
			{
				if ( pBuf[x+y*buildingRPGStats.passability.GetSizeX()] )
				{
					SetTileInListOfTiles( lockedTiles, nBeginTileX + x, nBeginTileY + y, 1, E_LOCKED_TILE );
				}
			}
		}
	}

	{
		//Загружаем инфу о видимости AI тайлов
		CVec3 beginVis3;						//координаты самого левого тайла, который связан с vVisOrigin
		beginVis3.x = realZeroPos3.x - buildingRPGStats.vVisOrigin.x;
		beginVis3.y = realZeroPos3.y - buildingRPGStats.vVisOrigin.y;
		beginVis3.z = 0;
		pSG->GetPos2( &pos2, beginVis3 );
		//сдвинемся на центр тайла
		pt.x = pos2.x + fCellSizeX/2;
		pt.y = pos2.y;
		CGridFrame::ComputeGameTileCoordinates( pt, ftX, ftY );
		int nVisTileX = ftX, nVisTileY = ftY;
//		AfxMessageBox( NStr::Format( "nVisTileX = %d, nVisTileY = %d", nVisTileX, nVisTileY ) );
		
		BYTE *pBuf = buildingRPGStats.visibility.GetBuffer();
		for ( int y=0; y<buildingRPGStats.visibility.GetSizeY(); y++ )
		{
			for ( int x=0; x<buildingRPGStats.visibility.GetSizeX(); x++ )
			{
				if ( pBuf[x+y*buildingRPGStats.visibility.GetSizeX()] )
				{
					SetTileInListOfTiles( transeparences, nVisTileX + x, nVisTileY + y, pBuf[x+y*buildingRPGStats.visibility.GetSizeX()], E_TRANSEPARENCE_TILE );
				}
			}
		}
	}
	
	for ( int i=0; i<buildingRPGStats.entrances.size(); ++i )
	{
		//Загружаем инфу о входах в здание
		float fSaveX = buildingRPGStats.entrances[i].vPos.x;
		float fSaveY = buildingRPGStats.entrances[i].vPos.y;
		CVec3 vPos3;
		vPos3.x = realZeroPos3.x + fSaveX;
		vPos3.y = realZeroPos3.y + fSaveY;
		vPos3.z = 0;
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, vPos3 );
		
		pt.x = vPos2.x;
		pt.y = vPos2.y;
		CGridFrame::ComputeGameTileCoordinates( pt, ftX, ftY );
		SetTileInListOfTiles( entrances, ftX, ftY, 1, E_ENTRANCE_TILE );
	}

	//Загружаем позиции для всех shoot points
	int nCurrentSlot = 0;
	CTreeItem *pSlotsItem = pRootItem->GetChildItem( E_BUILDING_SLOTS_ITEM );
	NI_ASSERT( pSlotsItem != 0 );
	NI_ASSERT( buildingRPGStats.slots.size() == pSlotsItem->GetChildsCount() );
	shootPoints.clear();
	for ( CTreeItem::CTreeItemList::const_iterator it=pSlotsItem->GetBegin(); it!=pSlotsItem->GetEnd(); ++it )
	{
		SBuildingRPGStats::SSlot &slot = buildingRPGStats.slots[ nCurrentSlot ];
		CBuildingSlotPropsItem *pSlotProps = static_cast<CBuildingSlotPropsItem *> ( it->GetPtr() );
		
		SShootPoint shoot;
		shoot.pSlot = pSlotProps;
		
		CPtr<IObjVisObj> pObject;
		{
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
			//добавляем спрайт 'точка стрельбы' с такими координатами
			CVec2 vPos2 = slot.vPicturePosition;
			vPos2.x += realZeroPos2.x;
			vPos2.y += realZeroPos2.y;
			CVec3 vPos3;
			pSG->GetPos3( &vPos3, vPos2 );
			NI_ASSERT( pObject != 0 );
			pObject->SetPosition( vPos3 );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( MIN_OPACITY );
			shoot.pSprite = pObject;
		}
		
		{
			//создаем спрайт - горизонтальную линию
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
			NI_ASSERT( pObject != 0 );

			CVec3 vPos3 = slot.vPos;
			vPos3.x += realZeroPos3.x;
			vPos3.y += realZeroPos3.y;
			vPos3.z = 0;
			
			pObject->SetPosition( vPos3 );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			shoot.pHLine = pObject;
		}

		shoot.fAngle = slot.fAngle;
		shoot.fDirection = slot.fDirection;
		
		pSlotProps->SetConeAngle( slot.fAngle );
		pSlotProps->SetConeDirection( slot.fDirection );
		pSlotProps->SetSightMultiplier( slot.fSightMultiplier );
		pSlotProps->SetCover( slot.fCoverage );
		
		pSlotProps->SetWeaponName( slot.gun.szWeapon.c_str() );
		pSlotProps->SetAmmo( slot.gun.nAmmo );
		pSlotProps->SetRotationSpeed( slot.fRotationSpeed );
		pSlotProps->SetPriority( slot.gun.nPriority );
		
		shootPoints.push_back( shoot );
		nCurrentSlot++;
	}

	//Загружаем позиции для всех fire points
	int nCurrentFire = 0;
	CTreeItem *pFiresItem = pRootItem->GetChildItem( E_BUILDING_FIRE_POINTS_ITEM );
	NI_ASSERT( pFiresItem != 0 );
	NI_ASSERT( buildingRPGStats.firePoints.size() == pFiresItem->GetChildsCount() );
	firePoints.clear();
	for ( CTreeItem::CTreeItemList::const_iterator it=pFiresItem->GetBegin(); it!=pFiresItem->GetEnd(); ++it )
	{
		SBuildingRPGStats::SFirePoint &fire = buildingRPGStats.firePoints[ nCurrentFire ];
		CBuildingFirePointPropsItem *pFirePointProps = static_cast<CBuildingFirePointPropsItem *> ( it->GetPtr() );
		
		SFirePoint firePoint;
		firePoint.pFirePoint = pFirePointProps;
		
		CPtr<IObjVisObj> pObject;
		{
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
			//добавляем спрайт 'точка огня' с такими координатами
			CVec2 vPos2 = fire.vPicturePosition;
			vPos2.x += realZeroPos2.x;
			vPos2.y += realZeroPos2.y;
			CVec3 vPos3;
			pSG->GetPos3( &vPos3, vPos2 );
			NI_ASSERT( pObject != 0 );
			pObject->SetPosition( vPos3 );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			firePoint.pSprite = pObject;
		}
		
		{
			//создаем спрайт - горизонтальную линию
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
			NI_ASSERT( pObject != 0 );
			
			CVec3 vPos3 = fire.vPos;
			vPos3.x += realZeroPos3.x;
			vPos3.y += realZeroPos3.y;
			vPos3.z = 0;
			
			pObject->SetPosition( vPos3 );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			firePoint.pHLine = pObject;
		}
		
		firePoint.fDirection = fire.fDirection;
		
		pFirePointProps->SetDirection( fire.fDirection );
		pFirePointProps->SetEffectName( fire.szFireEffect.c_str() );
		pFirePointProps->SetVerticalAngle( fire.fVerticalAngle );
		
		firePoints.push_back( firePoint );
		nCurrentFire++;
	}

	
	//Загружаем позиции для всех direction explosion points
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
	pTimer->Update( timeGetTime() );
	int nCurrentDirExp = 0;
	CBuildingDirExplosionsItem *pDirExpItem = (CBuildingDirExplosionsItem *) pRootItem->GetChildItem( E_BUILDING_DIR_EXPLOSIONS_ITEM );
	NI_ASSERT( pDirExpItem != 0 );
	NI_ASSERT( buildingRPGStats.dirExplosions.size() == pDirExpItem->GetChildsCount() );
	pDirExpItem->SetEffectName( buildingRPGStats.szDirExplosionEffect.c_str() );
	for ( CTreeItem::CTreeItemList::const_iterator it=pDirExpItem->GetBegin(); it!=pDirExpItem->GetEnd(); ++it )
	{
		CBuildingDirExplosionPropsItem *pProps = static_cast<CBuildingDirExplosionPropsItem *> ( it->GetPtr() );
		SBuildingRPGStats::SDirectionExplosion &dirExp = buildingRPGStats.dirExplosions[ nCurrentDirExp ];
		
		CPtr<IObjVisObj> pObject;
		{
			//добавляем спрайт 'точка направленного дыма' с такими координатами
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
			//добавляем спрайт 'точка огня' с такими координатами
			CVec2 vPos2 = dirExp.vPicturePosition;
			vPos2.x += realZeroPos2.x;
			vPos2.y += realZeroPos2.y;
			CVec3 vPos3;
			pSG->GetPos3( &vPos3, vPos2 );
			NI_ASSERT( pObject != 0 );
			pObject->SetPosition( vPos3 );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			pProps->pSprite = pObject;
		}
		
		{
			//создаем спрайт - горизонтальную линию
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
			NI_ASSERT( pObject != 0 );
			
			CVec3 vPos3 = dirExp.vPos;
			vPos3.x += realZeroPos3.x;
			vPos3.y += realZeroPos3.y;
			vPos3.z = 0;
			
			pObject->SetPosition( vPos3 );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			pProps->pHLine = pObject;
		}
		
		pProps->SetDirection( dirExp.fDirection );
		pProps->SetVerticalAngle( dirExp.fVerticalAngle );
		nCurrentDirExp++;
	}


	
	//Загружаем позиции для всех smoke points
	int nCurrentSmoke = 0;
	CBuildingSmokesItem *pSmokesItem = (CBuildingSmokesItem *) pRootItem->GetChildItem( E_BUILDING_SMOKES_ITEM );
	NI_ASSERT( pSmokesItem != 0 );
	pSmokesItem->SetEffectName( buildingRPGStats.szSmokeEffect.c_str() );
	for ( CTreeItem::CTreeItemList::const_iterator it=pSmokesItem->GetBegin(); it!=pSmokesItem->GetEnd(); ++it )
	{
		CBuildingSmokePropsItem *pProps = static_cast<CBuildingSmokePropsItem *> ( it->GetPtr() );
		SBuildingRPGStats::SFirePoint &smoke = buildingRPGStats.smokePoints[ nCurrentSmoke ];
		
		CPtr<IObjVisObj> pObject;
		{
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
			//добавляем спрайт 'точка огня' с такими координатами
			CVec2 vPos2 = smoke.vPicturePosition;
			vPos2.x += realZeroPos2.x;
			vPos2.y += realZeroPos2.y;
			CVec3 vPos3;
			pSG->GetPos3( &vPos3, vPos2 );
			NI_ASSERT( pObject != 0 );
			pObject->SetPosition( vPos3 );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			pProps->pSprite = pObject;
		}
		
		{
			//создаем спрайт - горизонтальную линию
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
			NI_ASSERT( pObject != 0 );
			
			CVec3 vPos3 = smoke.vPos;
			vPos3.x += realZeroPos3.x;
			vPos3.y += realZeroPos3.y;
			vPos3.z = 0;
			
			pObject->SetPosition( vPos3 );
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			pProps->pHLine = pObject;
		}
		
		pProps->SetDirection( smoke.fDirection );
		pProps->SetVerticalAngle( smoke.fVerticalAngle );
		nCurrentSmoke++;
	}
}

void CBuildingFrame::SaveFrameOwnData( IDataTree *pDT )
{
	pDT->StartChunk( "own_data" );
	CTreeAccessor tree = pDT;
/*
	if ( pSprite )
	{
		//Сохраняем позицию спрайта
		CVec3 pos3 = pSprite->GetPosition();
		tree.Add( "sprite_pos", &pos3 );
	}
*/
	tree.Add( "sprite_pos", &m_SpriteLoadPos );
	
	//Сохраняем позицию креста
	tree.Add( "krest_pos", &m_zeroPos );
	
	//Сохраняем export file name
	string szPrevExportDir;
	tree.Add( "export_dir", &szPrevExportDir );
	if ( szPrevExportDir.size() > 0 )
	{
		szPrevExportFileName = szPrevExportDir;
		szPrevExportFileName += "1.xml";
	}
	else
		tree.Add( "export_file_name", &szPrevExportFileName );
	
	pDT->FinishChunk();
}

void CBuildingFrame::LoadFrameOwnData( IDataTree *pDT )
{
	pDT->StartChunk( "own_data" );
	CTreeAccessor tree = pDT;
	
	//Загружаем позицию спрайта
	tree.Add( "sprite_pos", &m_SpriteLoadPos );
	if ( pSprite )
	{
		GetSingleton<IScene>()->MoveObject( pSprite, m_SpriteLoadPos );
		pSprite->SetPosition( m_SpriteLoadPos );
	}
	
	//Загружаем позицию креста
	tree.Add( "krest_pos", &m_zeroPos );
	
	//Загружаем export file name
	string szPrevExportDir;
	tree.Add( "export_dir", &szPrevExportDir );
	if ( szPrevExportDir.size() > 0 )
	{
		szPrevExportFileName = szPrevExportDir;
		szPrevExportFileName += "1.xml";
	}
	else
		tree.Add( "export_file_name", &szPrevExportFileName );
	
	pDT->FinishChunk();
}

void CBuildingFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 || pSprite == 0 )
	{
		//Если проект не был создан
		CGridFrame::OnLButtonDown(nFlags, point);
		return;
	}
	
	if ( tbStyle == E_DRAW_GRID )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( lockedTiles, ftX, ftY, 1, E_LOCKED_TILE );
		GFXDraw();
	}
	
	else if ( tbStyle == E_DRAW_TRANSEPARENCE )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( transeparences, ftX, ftY, m_transValue, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}
	
	else if ( tbStyle == E_MOVE_OBJECT && pSprite )
	{
		CVec2 pt;
		pt.x = point.x;
		pt.y = point.y;

		if ( IsSpriteHit( pSprite, pt, &objShift ) )
		{
			SetChangedFlag( true );
			m_mode = E_MOVE_OBJECT;
			g_frameManager.GetGameWnd()->SetCapture();

			IScene *pSG = GetSingleton<IScene>();
			CVec2 zeroPos2;
			pSG->GetPos2( &zeroPos2, m_zeroPos );
			zeroShift.x = zeroPos2.x - point.x;
			zeroShift.y = zeroPos2.y - point.y;
			
			m_beginDrag.x = point.x;
			m_beginDrag.y = point.y;
		}
	}
	
	else if ( tbStyle == E_SET_ZERO )
	{
		IScene *pSG = GetSingleton<IScene>();
		CVec2 pos2;
		pos2.x = point.x - zeroShiftX;
		pos2.y = point.y - zeroShiftY;
		pSG->GetPos3( &m_zeroPos, pos2 );
		GFXDraw();
	}
	
	else if ( tbStyle == E_SET_ENTRANCE )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( entrances, ftX, ftY, 1, E_ENTRANCE_TILE );
		GFXDraw();
	}
	
	else if ( tbStyle == E_SET_SHOOT_POINT )
	{
		if ( eActiveSubMode == E_SUB_MOVE )
			AddOrSelectShootPoint( point );
		else if ( eActiveSubMode == E_SUB_HOR )
			SetHorShootPoint( point );
		else if ( eActiveSubMode == E_SUB_DIR )
			SetShootPointAngle( point );
	}
	
	else if ( tbStyle == E_SET_FIRE_POINT )
	{
		if ( eActiveSubMode == E_SUB_MOVE )
			AddOrSelectFirePoint( point );
		else if ( eActiveSubMode == E_SUB_HOR )
			SetHorFirePoint( point );
		else if ( eActiveSubMode == E_SUB_DIR )
			SetFirePointAngle( point );
	}

	else if ( tbStyle == E_SET_DIRECTION_EXPLOSION )
	{
		if ( eActiveSubMode == E_SUB_MOVE )
		{
			if ( !pActiveDirExpPoint )
				return;

			IScene *pSG = GetSingleton<IScene>();
			CVec2 vPos2;
			CVec3 vPos3 = pActiveDirExpPoint->pHLine->GetPosition();
			pSG->GetPos2( &zeroShift, vPos3 );
			vPos3 = pActiveDirExpPoint->pSprite->GetPosition();
			pSG->GetPos2( &vPos2, vPos3 );
			zeroShift.x -= vPos2.x;
			zeroShift.y -= vPos2.y;

			MoveDirExpPoint( point );
		}
		else if ( eActiveSubMode == E_SUB_HOR )
			SetHorDirExpPoint( point );
		else if ( eActiveSubMode == E_SUB_DIR )
			SetDirExpPointAngle( point );
	}

	else if ( tbStyle == E_SET_SMOKE_POINT )
	{
		if ( eActiveSubMode == E_SUB_MOVE )
			AddOrSelectSmokePoint( point );
		else if ( eActiveSubMode == E_SUB_HOR )
			SetHorSmokePoint( point );
		else if ( eActiveSubMode == E_SUB_DIR )
			SetSmokePointAngle( point );
	}
	
	CGridFrame::OnLButtonDown(nFlags, point);
}

void CBuildingFrame::OnRButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 || pSprite == 0 )
	{
		//Если проект не был создан
		CGridFrame::OnRButtonDown(nFlags, point);
		return;
	}
	SetChangedFlag( true );
	
	if ( tbStyle == E_DRAW_GRID )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( lockedTiles, ftX, ftY, 0, E_LOCKED_TILE );
		GFXDraw();
	}

	else if ( tbStyle == E_DRAW_TRANSEPARENCE )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( transeparences, ftX, ftY, 0, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}

	else if ( tbStyle == E_SET_ENTRANCE )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( entrances, ftX, ftY, 0, E_ENTRANCE_TILE );
		GFXDraw();
	}
	
	CGridFrame::OnRButtonDown(nFlags, point);
}

void CBuildingFrame::OnMouseMove(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 || pSprite == 0 )
	{
		//Если проект не был создан
		CGridFrame::OnMouseMove(nFlags, point);
		return;
	}
	
	if ( tbStyle == E_DRAW_GRID && nFlags & MK_RBUTTON )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( lockedTiles, ftX, ftY, 0, E_LOCKED_TILE );
		GFXDraw();
	}
	else if ( tbStyle == E_DRAW_GRID && nFlags & MK_LBUTTON )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( lockedTiles, ftX, ftY, 1, E_LOCKED_TILE );
		GFXDraw();
	}

	if ( tbStyle == E_DRAW_TRANSEPARENCE && nFlags & MK_RBUTTON )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( transeparences, ftX, ftY, 0, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}
	else if ( tbStyle == E_DRAW_TRANSEPARENCE && nFlags & MK_LBUTTON )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( transeparences, ftX, ftY, m_transValue, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}
	
	else if ( m_mode == E_MOVE_OBJECT && nFlags & MK_LBUTTON )
	{
		if ( !pSprite )
			return;
		
		IScene *pSG = GetSingleton<IScene>();
		CVec2 pos2;
		CVec3 pos3;
		pos2.x = point.x + objShift.x;
		pos2.y = point.y + objShift.y;
		pSG->GetPos3( &pos3, pos2 );
		pSG->MoveObject( pSprite, pos3 );
		m_SpriteLoadPos = pos3;
		
		pos2.x = point.x + zeroShift.x;
		pos2.y = point.y + zeroShift.y;
		pSG->GetPos3( &m_zeroPos, pos2 );

		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		for ( CListOfShootPoints::iterator it=shootPoints.begin(); it!=shootPoints.end(); ++it )
		{
			pos3 = it->pSprite->GetPosition();
			pSG->GetPos2( &pos2, pos3 );
			pos2.x += point.x - m_beginDrag.x;
			pos2.y += point.y - m_beginDrag.y;
			pSG->GetPos3( &pos3, pos2 );
			it->pSprite->SetPosition( pos3 );
			it->pSprite->Update( pTimer->GetGameTime() );

			if ( it->pHLine )
			{
				pos3 = it->pHLine->GetPosition();
				pSG->GetPos2( &pos2, pos3 );
				pos2.x += point.x - m_beginDrag.x;
				pos2.y += point.y - m_beginDrag.y;
				pSG->GetPos3( &pos3, pos2 );
				it->pHLine->SetPosition( pos3 );
				it->pHLine->Update( pTimer->GetGameTime() );
			}
		}

		for ( CListOfFirePoints::iterator it=firePoints.begin(); it!=firePoints.end(); ++it )
		{
			pos3 = it->pSprite->GetPosition();
			pSG->GetPos2( &pos2, pos3 );
			pos2.x += point.x - m_beginDrag.x;
			pos2.y += point.y - m_beginDrag.y;
			pSG->GetPos3( &pos3, pos2 );
			it->pSprite->SetPosition( pos3 );
			it->pSprite->Update( pTimer->GetGameTime() );
			
			if ( it->pHLine )
			{
				pos3 = it->pHLine->GetPosition();
				pSG->GetPos2( &pos2, pos3 );
				pos2.x += point.x - m_beginDrag.x;
				pos2.y += point.y - m_beginDrag.y;
				pSG->GetPos3( &pos3, pos2 );
				it->pHLine->SetPosition( pos3 );
				it->pHLine->Update( pTimer->GetGameTime() );
			}
		}
		
		{
			CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
			CTreeItem *pRootItem = pTree->GetRootItem();
			CTreeItem *pDirExpItems = pRootItem->GetChildItem( E_BUILDING_DIR_EXPLOSIONS_ITEM );
			NI_ASSERT( pDirExpItems != 0 );
			
			for ( CTreeItem::CTreeItemList::const_iterator it=pDirExpItems->GetBegin(); it!=pDirExpItems->GetEnd(); ++it )
			{
				CBuildingDirExplosionPropsItem *pProps = static_cast<CBuildingDirExplosionPropsItem *> ( it->GetPtr() );
				pos3 = pProps->pSprite->GetPosition();
				pSG->GetPos2( &pos2, pos3 );
				pos2.x += point.x - m_beginDrag.x;
				pos2.y += point.y - m_beginDrag.y;
				pSG->GetPos3( &pos3, pos2 );
				pProps->pSprite->SetPosition( pos3 );
				pProps->pSprite->Update( pTimer->GetGameTime() );

				pos3 = pProps->pHLine->GetPosition();
				pSG->GetPos2( &pos2, pos3 );
				pos2.x += point.x - m_beginDrag.x;
				pos2.y += point.y - m_beginDrag.y;
				pSG->GetPos3( &pos3, pos2 );
				pProps->pHLine->SetPosition( pos3 );
				pProps->pHLine->Update( pTimer->GetGameTime() );
			}
		}
		
		{
			CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
			CTreeItem *pRootItem = pTree->GetRootItem();
			CTreeItem *pSmokeItems = pRootItem->GetChildItem( E_BUILDING_SMOKES_ITEM );
			NI_ASSERT( pSmokeItems != 0 );
			
			for ( CTreeItem::CTreeItemList::const_iterator it=pSmokeItems->GetBegin(); it!=pSmokeItems->GetEnd(); ++it )
			{
				CBuildingSmokePropsItem *pProps = static_cast<CBuildingSmokePropsItem *> ( it->GetPtr() );
				pos3 = pProps->pSprite->GetPosition();
				pSG->GetPos2( &pos2, pos3 );
				pos2.x += point.x - m_beginDrag.x;
				pos2.y += point.y - m_beginDrag.y;
				pSG->GetPos3( &pos3, pos2 );
				pProps->pSprite->SetPosition( pos3 );
				pProps->pSprite->Update( pTimer->GetGameTime() );
				
				pos3 = pProps->pHLine->GetPosition();
				pSG->GetPos2( &pos2, pos3 );
				pos2.x += point.x - m_beginDrag.x;
				pos2.y += point.y - m_beginDrag.y;
				pSG->GetPos3( &pos3, pos2 );
				pProps->pHLine->SetPosition( pos3 );
				pProps->pHLine->Update( pTimer->GetGameTime() );
			}
		}

		m_beginDrag.x = point.x;
		m_beginDrag.y = point.y;
		GFXDraw();
	}
	
	else if ( tbStyle == E_SET_ZERO && nFlags & MK_LBUTTON )
	{
		IScene *pSG = GetSingleton<IScene>();
		CVec2 pos2;
		pos2.x = point.x - zeroShiftX;
		pos2.y = point.y - zeroShiftY;
		pSG->GetPos3( &m_zeroPos, pos2 );
		GFXDraw();
	}

	else if ( tbStyle == E_SET_SHOOT_POINT && nFlags & MK_LBUTTON )
	{
		if ( eActiveSubMode == E_SUB_MOVE && m_mode == E_SET_SHOOT_POINT )
			MoveShootPoint( point );
		else if ( eActiveSubMode == E_SUB_HOR )
			SetHorShootPoint( point );
		else if ( eActiveSubMode == E_SUB_DIR )
			SetShootPointAngle( point );
	}
	
	else if ( tbStyle == E_SET_FIRE_POINT && nFlags & MK_LBUTTON )
	{
		if ( eActiveSubMode == E_SUB_MOVE && m_mode == E_SET_FIRE_POINT )
			MoveFirePoint( point );
		else if ( eActiveSubMode == E_SUB_HOR )
			SetHorFirePoint( point );
		else if ( eActiveSubMode == E_SUB_DIR )
			SetFirePointAngle( point );
	}

	else if ( tbStyle == E_SET_DIRECTION_EXPLOSION && nFlags & MK_LBUTTON )
	{
		if ( eActiveSubMode == E_SUB_MOVE )
			MoveDirExpPoint( point );
		else if ( eActiveSubMode == E_SUB_HOR )
			SetHorDirExpPoint( point );
		else if ( eActiveSubMode == E_SUB_DIR )
			SetDirExpPointAngle( point );
	}

	else if ( tbStyle == E_SET_SMOKE_POINT && nFlags & MK_LBUTTON )
	{
		if ( eActiveSubMode == E_SUB_MOVE )
			MoveSmokePoint( point );
		else if ( eActiveSubMode == E_SUB_HOR )
			SetHorSmokePoint( point );
		else if ( eActiveSubMode == E_SUB_DIR )
			SetSmokePointAngle( point );
	}

	CGridFrame::OnMouseMove(nFlags, point);
}

void CBuildingFrame::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 || pSprite == 0 )
	{
		//Если проект не был создан
		CGridFrame::OnLButtonUp(nFlags, point);
		return;
	}
	
	ReleaseCapture();
	GFXDraw();
	m_mode = -1;
	
	CGridFrame::OnLButtonUp(nFlags, point);
}

void CBuildingFrame::CreateKrest()
{
	if ( pKrestTexture == 0 )
	{
		ITextureManager *pTM = GetSingleton<ITextureManager>();
		pKrestTexture = pTM->GetTexture( "editor\\krest\\1" );
	}
}

void CBuildingFrame::LoadSprite( const char *pszSpriteFullName )
{
	IScene *pSG = GetSingleton<IScene>();
	if ( pSprite )
		pSG->RemoveObject( pSprite );

	//Скомпонуем спрайт в editor temp dir
	string szTempDir = theApp.GetEditorTempDir();
	if ( !ComposeSingleSprite( pszSpriteFullName, szTempDir.c_str(), "Building" ) )
	{
		pSprite = 0;
		GFXDraw();
		return;
	}

	szTempDir = theApp.GetEditorTempResourceDir();
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	
	pSprite = static_cast<IObjVisObj *> ( pVOB->BuildObject( (szTempDir + "\\Building").c_str(), 0, SGVOT_SPRITE ) );
	NI_ASSERT( pSprite != 0 );
	pSprite->SetPosition( m_SpriteLoadPos );
	pSprite->SetDirection( 0 );
	pSprite->SetAnimation( 0 );
	pSG->AddObject( pSprite, SGVOGT_UNIT );
	pSprite->SetOpacity( 140 );
	GFXDraw();
}

bool CBuildingFrame::LoadFramePreExportData( const char *pszProjectFile, CTreeItem *pRootItem )
{
	CTreeItem *pGraphicsItem = pRootItem->GetChildItem( E_BUILDING_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	CTreeItem *pSeason = pGraphicsItem->GetChildItem( E_BUILDING_SUMMER_PROPS_ITEM );
	NI_ASSERT( pSeason != 0 );
	CBuildingGraphicPropsItem *pGraphicPropsItem = (CBuildingGraphicPropsItem *) pSeason->GetChildItem( E_BUILDING_GRAPHIC1_PROPS_ITEM );
	NI_ASSERT( pGraphicPropsItem != 0 );
	const char *pszFileName = pGraphicPropsItem->GetFileName();
	NI_ASSERT( pszFileName != 0 );
	
	//так как имя файла относительное, здесь я должен собрать полный путь
	string szDir = GetDirectory( pszProjectFile );
	string szObjName;
	bool bRes = MakeFullPath( szDir.c_str(), pszFileName, szObjName );
	if ( !bRes )
		szObjName = pszFileName;		//popa

	LoadSprite( szObjName.c_str() );
	pSprite->Update( timeGetTime() );
	
	//временно сохраним, чтобы не испортилась координата
	CVec3 vSave = m_zeroPos;
	CreateKrest();
	m_zeroPos = vSave;
	GFXDraw();
	return true;
}

void CBuildingFrame::SetActiveMode( EActiveMode mode )
{
	if ( eActiveMode == mode )
		return;

	//скрываем старый режим
	if ( eActiveMode == E_SHOOT_SLOT )
	{
		for ( CListOfShootPoints::iterator it=shootPoints.begin(); it!=shootPoints.end(); ++it )
		{
			it->pSprite->SetOpacity( 0 );
			if ( it->pHLine )
				it->pHLine->SetOpacity( 0 );
		}
	}
	else if ( eActiveMode == E_FIRE_POINT )
	{
		for ( CListOfFirePoints::iterator it=firePoints.begin(); it!=firePoints.end(); ++it )
		{
			it->pSprite->SetOpacity( 0 );
			if ( it->pHLine )
				it->pHLine->SetOpacity( 0 );
		}
	}
	else if ( eActiveMode == E_DIR_EXPLOSION )
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRootItem = pTree->GetRootItem();
		CTreeItem *pDirExpItems = pRootItem->GetChildItem( E_BUILDING_DIR_EXPLOSIONS_ITEM );
		NI_ASSERT( pDirExpItems != 0 );

		for ( CTreeItem::CTreeItemList::const_iterator it=pDirExpItems->GetBegin(); it!=pDirExpItems->GetEnd(); ++it )
		{
			CBuildingDirExplosionPropsItem *pProps = static_cast<CBuildingDirExplosionPropsItem *> ( it->GetPtr() );
			pProps->pSprite->SetOpacity( 0 );
			pProps->pHLine->SetOpacity( 0 );
		}
	}
	else if ( eActiveMode == E_SMOKE_POINT )
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRootItem = pTree->GetRootItem();
		CTreeItem *pSmokeItems = pRootItem->GetChildItem( E_BUILDING_SMOKES_ITEM );
		NI_ASSERT( pSmokeItems != 0 );
		
		for ( CTreeItem::CTreeItemList::const_iterator it=pSmokeItems->GetBegin(); it!=pSmokeItems->GetEnd(); ++it )
		{
			CBuildingSmokePropsItem *pProps = static_cast<CBuildingSmokePropsItem *> ( it->GetPtr() );
			pProps->pSprite->SetOpacity( 0 );
			pProps->pHLine->SetOpacity( 0 );
		}
	}
	
	eActiveMode = mode;
	if ( eActiveMode == E_SHOOT_SLOT )
	{
		for ( CListOfShootPoints::iterator it=shootPoints.begin(); it!=shootPoints.end(); ++it )
		{
			it->pSprite->SetOpacity( MIN_OPACITY );
//			if ( it->pHLine )
//				it->pHLine->SetOpacity( MIN_OPACITY );
		}
		SetActiveShootPoint( pActiveShootPoint );
	}
	else if ( eActiveMode == E_FIRE_POINT )
	{
		for ( CListOfFirePoints::iterator it=firePoints.begin(); it!=firePoints.end(); ++it )
		{
			it->pSprite->SetOpacity( MIN_OPACITY );
//			if ( it->pHLine )
//				it->pHLine->SetOpacity( MIN_OPACITY );
		}
		SetActiveFirePoint( pActiveFirePoint );
	}
	else if ( eActiveMode == E_DIR_EXPLOSION )
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRootItem = pTree->GetRootItem();
		CTreeItem *pDirExpItems = pRootItem->GetChildItem( E_BUILDING_DIR_EXPLOSIONS_ITEM );
		NI_ASSERT( pDirExpItems != 0 );
		
		for ( CTreeItem::CTreeItemList::const_iterator it=pDirExpItems->GetBegin(); it!=pDirExpItems->GetEnd(); ++it )
		{
			CBuildingDirExplosionPropsItem *pProps = static_cast<CBuildingDirExplosionPropsItem *> ( it->GetPtr() );
			pProps->pSprite->SetOpacity( MIN_OPACITY );
		}
		SelectDirExpPoint( pActiveDirExpPoint );
	}
	else if ( eActiveMode == E_SMOKE_POINT )
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRootItem = pTree->GetRootItem();
		CTreeItem *pSmokeItems = pRootItem->GetChildItem( E_BUILDING_SMOKES_ITEM );
		NI_ASSERT( pSmokeItems != 0 );
		
		for ( CTreeItem::CTreeItemList::const_iterator it=pSmokeItems->GetBegin(); it!=pSmokeItems->GetEnd(); ++it )
		{
			CBuildingSmokePropsItem *pProps = static_cast<CBuildingSmokePropsItem *> ( it->GetPtr() );
			pProps->pSprite->SetOpacity( MIN_OPACITY );
		}
		SelectSmokePoint( pActiveSmokePoint );
	}
}

void CBuildingFrame::OnMoveObject() 
{
	UINT nIndex = 0;
	tbStyle = E_MOVE_OBJECT;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ENTRANCE_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_DIRECTION_EXPLOSION );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetActiveMode( E_UNKNOWN_MODE );
	SetFocus();
	GFXDraw();
}

void CBuildingFrame::OnDrawGrid() 
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_GRID;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ENTRANCE_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_DIRECTION_EXPLOSION );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetActiveMode( E_UNKNOWN_MODE );
	SetFocus();
	GFXDraw();
}

void CBuildingFrame::OnChangeTranseparence()
{
	NI_ASSERT( m_pTransparenceCombo != 0 );
 	m_transValue = m_pTransparenceCombo->GetCurSel();
	SetActiveMode( E_UNKNOWN_MODE );
}

void CBuildingFrame::OnSetFocusTranseparence()
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_TRANSEPARENCE;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ENTRANCE_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_DIRECTION_EXPLOSION );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetActiveMode( E_UNKNOWN_MODE );
	m_pTransparenceCombo->SetFocus();
	GFXDraw();
}

void CBuildingFrame::OnSetEntranceButton() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_ENTRANCE;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_SET_ENTRANCE_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_DIRECTION_EXPLOSION );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetActiveMode( E_UNKNOWN_MODE );
	SetFocus();
	GFXDraw();
}

void CBuildingFrame::OnSetZeroButton() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_ZERO;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ENTRANCE_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_DIRECTION_EXPLOSION );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetActiveMode( E_UNKNOWN_MODE );
	SetFocus();
	GFXDraw();
}

void CBuildingFrame::OnSetShootPoint() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_SHOOT_POINT;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ENTRANCE_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_DIRECTION_EXPLOSION );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	if ( eActiveSubMode == E_SUB_MOVE )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );

	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	if ( eActiveSubMode == E_SUB_HOR )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );

	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	if ( eActiveSubMode == E_SUB_DIR )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );

	SetActiveMode( E_SHOOT_SLOT );
	SetFocus();
	GFXDraw();
}

void CBuildingFrame::OnSetFirePoint() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_FIRE_POINT;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ENTRANCE_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_DIRECTION_EXPLOSION );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	if ( eActiveSubMode == E_SUB_MOVE )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	if ( eActiveSubMode == E_SUB_HOR )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	if ( eActiveSubMode == E_SUB_DIR )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetActiveMode( E_FIRE_POINT );
	SetFocus();
	GFXDraw();
}

void CBuildingFrame::OnSetDirectionExplosion() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_DIRECTION_EXPLOSION;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_SET_DIRECTION_EXPLOSION );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ENTRANCE_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	if ( eActiveSubMode == E_SUB_MOVE )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	if ( eActiveSubMode == E_SUB_HOR )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	if ( eActiveSubMode == E_SUB_DIR )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetActiveMode( E_DIR_EXPLOSION );
	SetFocus();
	GFXDraw();
}

void CBuildingFrame::OnSetSmokePoint() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_SMOKE_POINT;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ENTRANCE_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_DIRECTION_EXPLOSION );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	if ( eActiveSubMode == E_SUB_MOVE )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	if ( eActiveSubMode == E_SUB_HOR )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	if ( eActiveSubMode == E_SUB_DIR )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetActiveMode( E_SMOKE_POINT );
	SetFocus();
	GFXDraw();
}

void CBuildingFrame::OnMovePoint() 
{
	UINT nIndex = 0;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	eActiveSubMode = E_SUB_MOVE;
	SetFocus();
}

void CBuildingFrame::OnSetHorizontalShoot() 
{
	UINT nIndex = 0;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	eActiveSubMode = E_SUB_HOR;
	CVec3 vPos3;
	if ( eActiveMode == E_SHOOT_SLOT && pActiveShootPoint )
		vPos3 = pActiveShootPoint->pSprite->GetPosition();
	else if ( eActiveMode == E_FIRE_POINT && pActiveFirePoint )
		vPos3 = pActiveFirePoint->pSprite->GetPosition();
	else if ( eActiveMode == E_DIR_EXPLOSION && pActiveDirExpPoint )
		vPos3 = pActiveDirExpPoint->pSprite->GetPosition();
	else if ( eActiveMode == E_SMOKE_POINT && pActiveSmokePoint )
		vPos3 = pActiveSmokePoint->pSprite->GetPosition();
	else
	{
		NI_ASSERT_T( 0, "Unknown Active Mode" );
		return;
	}
	ComputeMaxAndMinPositions( vPos3 );
}

void CBuildingFrame::OnSetShootAngle() 
{
	UINT nIndex = 0;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+8);
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE);
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	eActiveSubMode = E_SUB_DIR;
	if ( eActiveMode == E_SHOOT_SLOT )
		ComputeAngleLines();
	else if ( eActiveMode == E_FIRE_POINT )
		ComputeFireDirectionLines();
	else if ( eActiveMode == E_DIR_EXPLOSION )
		ComputeDirExpDirectionLines();
	else if ( eActiveMode == E_SMOKE_POINT )
		ComputeSmokeLines();
	SetFocus();
}

void CBuildingFrame::OnGeneratePoints() 
{
	if ( eActiveMode == E_DIR_EXPLOSION )
		GenerateDirExpPoints();
	else if ( eActiveMode == E_SMOKE_POINT )
		GenerateSmokePoints();
	SetFocus();
}


void CBuildingFrame::OnUpdateMoveObject(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CBuildingFrame::OnUpdateDrawGrid(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CBuildingFrame::OnUpdateSetZeroButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CBuildingFrame::OnUpdateSetEntranceButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CBuildingFrame::OnUpdateSetShootPoint(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CBuildingFrame::OnUpdateSetDirectionExplosion(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		if ( !lockedTiles.empty() )
			pCmdUI->Enable( true );
		else
			pCmdUI->Enable( false );
	}
	else
		pCmdUI->Enable( false );
}

void CBuildingFrame::OnUpdateSetSmokePoint(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		if ( !lockedTiles.empty() )
			pCmdUI->Enable( true );
		else
			pCmdUI->Enable( false );
	}
	else
		pCmdUI->Enable( false );
}

void CBuildingFrame::OnUpdateMovePoint(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 && eActiveMode == E_SHOOT_SLOT || eActiveMode == E_FIRE_POINT || eActiveMode == E_DIR_EXPLOSION || eActiveMode == E_SMOKE_POINT )
	{
		//Если уже был создан проект
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
/*
	if ( eActiveMode == E_SHOOT_SLOT )
	{
		//Если есть shoot points
		pCmdUI->Enable( true );
		return;
	}
	else if ( eActiveMode == E_FIRE_POINT )
	{
		//Если есть fire points
		pCmdUI->Enable( true );
		return;
	}
	
	pCmdUI->Enable( false );
	return;
*/
}

void CBuildingFrame::OnUpdateSetHorizontalShoot(CCmdUI* pCmdUI) 
{
	if ( eActiveMode == E_SHOOT_SLOT )
	{
		if ( pActiveShootPoint != 0 )
		{
			//Если есть shoot points
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	else if ( eActiveMode == E_FIRE_POINT )
	{
		if ( pActiveFirePoint != 0 )
		{
			//Если есть fire points
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	else if ( eActiveMode == E_DIR_EXPLOSION )
	{
		if ( pActiveDirExpPoint != 0 )
		{
			//Если есть fire points
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	else if ( eActiveMode == E_SMOKE_POINT )
	{
		if ( pActiveSmokePoint != 0 )
		{
			//Если есть smoke points
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	
	pCmdUI->Enable( false );
	return;
}

void CBuildingFrame::OnUpdateSetShootAngle(CCmdUI* pCmdUI) 
{
	if ( eActiveMode == E_SHOOT_SLOT )
	{
		if ( pActiveShootPoint != 0 )
		{
			//Если есть shoot points
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	else if ( eActiveMode == E_FIRE_POINT )
	{
		if ( pActiveFirePoint != 0 )
		{
			//Если есть fire points
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	else if ( eActiveMode == E_DIR_EXPLOSION )
	{
		if ( pActiveDirExpPoint != 0 )
		{
			//Если есть fire points
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	else if ( eActiveMode == E_SMOKE_POINT )
	{
		if ( pActiveSmokePoint != 0 )
		{
			//Если есть smoke points
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	
	pCmdUI->Enable( false );
	return;
}

void CBuildingFrame::OnUpdateDrawTransparence(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

void CBuildingFrame::OnUpdateSetFirePoint(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CBuildingFrame::OnUpdateGeneratePoints(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		//Если уже был создан проект
		if ( ( eActiveMode == E_DIR_EXPLOSION || eActiveMode == E_SMOKE_POINT ) && !lockedTiles.empty() )
			pCmdUI->Enable( true );
		else
			pCmdUI->Enable( false );
	}
	else
		pCmdUI->Enable( false );
}

FILETIME CBuildingFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime, current;
	minTime.dwHighDateTime = -1;
	minTime.dwLowDateTime = -1;
	string szTempFileName;
	string szDestDir = GetDirectory( pszResultFileName );
	
	for ( int i=1; i<=3; i++ )
	{
		//Найдем время создания 1.san файла
		szTempFileName = szDestDir;
		szTempFileName += NStr::Format( "%d", i );
		szTempFileName += ".san";
		current = GetFileChangeTime( szTempFileName.c_str() );
		if ( current < minTime )
			minTime = current;
		
		szTempFileName = szDestDir;
		szTempFileName += NStr::Format( "%d", i );
		current = GetTextureFileChangeTime( szTempFileName.c_str() );
		if ( current < minTime )
			minTime = current;
		
		szTempFileName = szDestDir;
		szTempFileName += NStr::Format( "%d", i );
		szTempFileName += "s.san";
		current = GetFileChangeTime( szTempFileName.c_str() );
		if ( current < minTime )
			minTime = current;
		
		szTempFileName = szDestDir;
		szTempFileName += NStr::Format( "%d", i );
		szTempFileName += "s";
		current = GetTextureFileChangeTime( szTempFileName.c_str() );
		if ( current < minTime )
			minTime = current;
	}
	
	szTempFileName = szDestDir;
	szTempFileName += "1.xml";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	return minTime;
}

FILETIME CBuildingFrame::FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem )
{
	CBuildingTreeRootItem *pBuildRoot = static_cast<CBuildingTreeRootItem *> ( pRootItem );
	return pBuildRoot->FindMaximalSourceTime( pszProjectName );
}
