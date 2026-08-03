#include "StdAfx.h"
#include <io.h>
#include "..\Common\LegacyUiCompat.h"

#include "..\GFX\GFX.h"
#include "..\GFX\GFXHelper.h"
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
#include "WeaponTreeItem.h"
#include "ObjectFrm.h"
#include "ObjectView.h"
#include "GameWnd.h"
#include "frames.h"
#include "RefDlg.h"

static int zeroSizeX = 32;
static int zeroSizeY = 32;
static float zeroShiftX = 15.4f;
static float zeroShiftY = 15.4f;
const float DELTA_X = 10, DELTA_Y = 10;
static const float NORMAL_LENGTH = 40;

void MyFillBrezenham( int x1, int y1, int x2, int y2, std::list<CVec2> &tiles )
{
	int dx = abs( x2 - x1 );
	int dy = abs( y2 - y1 );
	int inc1;
	int inc2;
	int d;
	int x, y, xend, yend;
	int s;
	
	if ( dx > dy )
	{
		inc1 = dy << 1;
		inc2 = (dy - dx) << 1;
		d = 2 * dy - dx;
		
		if ( x2 > x1 )
		{
			x = x1;
			y = y1;
			xend = x2;
			if ( y1 < y2 )
				s = 1;
			else
				s = -1;
		}
		else
		{
			x = x2;
			y = y2;
			xend = x1;
			if ( y2 < y1 )
				s = 1;
			else
				s = -1;
		}
		
		tiles.push_back( CVec2(x, y) );
		while ( x < xend )
		{
			if ( d > 0 )
			{
				y += s;
				d += inc2;
				tiles.push_back( CVec2(x, y) );
			}
			else
				d += inc1;
			x++;
			tiles.push_back( CVec2(x, y) );
		}
	}
	else
	{
		inc1 = dx << 1;
		inc2 = (dx - dy) << 1;
		d = 2 * dx - dy;
		
		if ( y2 > y1 )
		{
			x = x1;
			y = y1;
			yend = y2;
			if ( x1 < x2 )
				s = 1;
			else
				s = -1;
		}
		else
		{
			x = x2;
			y = y2;
			yend = y1;
			if ( x2 < x1 )
				s = 1;
			else
				s = -1;
		}
		
		tiles.push_back( CVec2(x, y) );
		while ( y < yend )
		{
			if ( d > 0 )
			{
				x += s;
				d += inc2;
				tiles.push_back( CVec2(x, y) );
			}
			else
				d += inc1;
			y++;
			tiles.push_back( CVec2(x, y) );
		}
	}
}


IMPLEMENT_DYNCREATE(CObjectFrame, CGridFrame)

BEGIN_MESSAGE_MAP(CObjectFrame, CGridFrame)
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
	ON_UPDATE_COMMAND_UI(ID_OBJECT_TRANSPARENCE, OnUpdateDrawTransparence)
	ON_CBN_SETFOCUS( IDC_OBJECT_TRANSPARENCE, OnSetFocusTranseparence )
	ON_CBN_SELCHANGE( IDC_OBJECT_TRANSPARENCE, OnChangeTranseparence )
	ON_COMMAND(ID_DRAW_ONE_WAY_TRANSEPARENCE, OnDrawOneWayTranseparence)
	ON_UPDATE_COMMAND_UI(ID_DRAW_ONE_WAY_TRANSEPARENCE, OnUpdateDrawOneWayTranseparence)
END_MESSAGE_MAP()


CObjectFrame::CObjectFrame()
{
	szComposerName = "Object Editor";
	szExtension = "*.obt";
	szComposerSaveName = "Object_Composer_Project";
	nTreeRootItemID = E_OBJECT_ROOT_ITEM;
	nFrameType = CFrameManager::E_OBJECT_FRAME;
	pWndView = new CObjectView;
	szAddDir = "objects\\";
	
	m_nSelected = -1;
	m_pTransparenceCombo = 0;
	m_SpriteLoadPos = CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0);
	pActiveGraphicProps = 0;
	m_mode = -1;

	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB1555;
}

CObjectFrame::~CObjectFrame()
{
}

void CObjectFrame::Init( IGFX *_pGFX )
{
	CGridFrame::Init( _pGFX );

	pRectVertices = pGFX->CreateVertices( 5, SGFXTLVertex::format, GFXPT_LINESTRIP, GFXD_DYNAMIC );
	{
		CVerticesLock<SGFXTLVertex> vertices( pRectVertices );
		vertices[0].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[1].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[2].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[3].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[4].Setup( 0, 0, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}
}

int CObjectFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CGridFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );

	if (!pWndView->Create(NULL, NULL,  WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}
	
	GenerateProjectName();
	
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6)->SetButtonStyle( 0, TBBS_CHECKBOX | TBBS_CHECKED );
	pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6)->SetButtonStyle( 1, TBBS_CHECKBOX );
	return 0;
}


void CObjectFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( nCommand );
	
	if ( nCommand == SW_SHOW )
	{
		ICamera *pCamera = GetSingleton<ICamera>();
		pCamera->SetAnchor( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );
		pCamera->Update();
		
		IGFX *pGFX = GetSingleton<IGFX>();
		pGFX->SetViewTransform( pCamera->GetPlacement() );

		IScene *pSG = GetSingleton<IScene>();
		if ( pSprite )
			pSG->AddObject( pSprite, SGVOGT_UNIT );
	}
}

void CObjectFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();

	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );
	CGridFrame::GFXDraw();
	
	if ( tbStyle == E_DRAW_GRID || tbStyle == E_MOVE_OBJECT )
	{
		for ( CListOfTiles::iterator it=lockedTiles.begin(); it!=lockedTiles.end(); ++it )
			pGFX->Draw( it->pVertices, pMarkerIndices );
	}
	
	if ( tbStyle == E_DRAW_TRANSEPARENCE )
	{
		for ( CListOfTiles::iterator it=transeparences.begin(); it!=transeparences.end(); ++it )
			pGFX->Draw( it->pVertices, pMarkerIndices );
	}
	
	if ( tbStyle == E_DRAW_ONE_WAY_TRANSEPARENCE )
	{
		for ( CListOfNormalTiles::iterator it=dirTiles.begin(); it!=dirTiles.end(); ++it )
		{
			pGFX->Draw( it->pVertices, pMarkerIndices );
			pGFX->Draw( it->pNormalVertices, 0 );
		}
		for ( CTransLineList::iterator it=transLines.begin(); it!=transLines.end(); ++it )
		{
			pGFX->Draw( it->pVertices, 0 );
			pGFX->Draw( it->pNormalVertices, 0 );
		}
		if ( bDragging )
		{
			pGFX->Draw( currentLine.pVertices, 0 );
			pGFX->Draw( currentLine.pNormalVertices, 0 );
		}
		if ( bDrawRect )
			pGFX->Draw( pRectVertices, 0 );
	}
	
	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
	pCamera->Update();
  pSG->Draw( pCamera );
	
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

void CObjectFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();

	m_mode = -1;
	CreateKrest();
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	CTreeItem *pGraphicsItem = pRootItem->GetChildItem( E_OBJECT_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	CObjectGraphicPropsItem *pGraphicPropsItem = (CObjectGraphicPropsItem *) pGraphicsItem->GetChildItem( E_OBJECT_GRAPHIC1_PROPS_ITEM );
	SetActiveGraphicPropsItem( pGraphicPropsItem );
}

void CObjectFrame::SpecificClearBeforeBatchMode()
{
	lockedTiles.clear();
	transeparences.clear();
	bonuses.clear();
	dirTiles.clear();
	transLines.clear();
	bDragging = false;
	bDrawRect = false;
	m_nSelected = -1;
	pSprite = 0;
	m_SpriteLoadPos = CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0);
	pActiveGraphicProps = 0;
	m_zeroPos = CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0);

	GetSingleton<IScene>()->Clear();
}

BOOL CObjectFrame::SpecificTranslateMessage( MSG *pMsg )
{
	if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_DELETE )
	{
		if ( GetFocus() == pTreeDockBar )
			return false;

		if ( tbStyle == E_DRAW_ONE_WAY_TRANSEPARENCE )
		{
			if ( m_nSelected >= 0 )
			{
				int i = 0;
				CTransLineList::iterator it=transLines.begin();
				for ( ; it!=transLines.end(); ++it )
				{
					if ( i == m_nSelected )
					{
						STransLine &line = *it;
						std::list<CVec2> coords;
						float ftx1, fty1, ftx2, fty2;
						POINT pt;
						pt.x = line.p1.x;
						pt.y = line.p1.y;
						ComputeGameTileCoordinates( pt, ftx1, fty1 );
						pt.x = line.p2.x;
						pt.y = line.p2.y;
						ComputeGameTileCoordinates( pt, ftx2, fty2 );
						
						MyFillBrezenham( ftx1, fty1, ftx2, fty2, coords );
						for ( std::list<CVec2>::iterator it=coords.begin(); it!=coords.end(); ++it )
						{
							DeleteTileInListOfNormalTiles( dirTiles, it->x, it->y );
						}
						
						transLines.erase( it );
						break;
					}
					i++;
				}
				m_nSelected = -1;
				bDrawRect = false;
				SetChangedFlag( true );
				
				bDragging = true;
				for ( it=transLines.begin(); it!=transLines.end(); ++it )
				{
					currentLine = *it;
					int nRes = UpdateNormalForSelectedLine();		//��������� �������
					std::list<CVec2> coords;
					float ftx1, fty1, ftx2, fty2;
					POINT pt;
					pt.x = currentLine.p1.x;
					pt.y = currentLine.p1.y;
					ComputeGameTileCoordinates( pt, ftx1, fty1 );
					pt.x = currentLine.p2.x;
					pt.y = currentLine.p2.y;
					ComputeGameTileCoordinates( pt, ftx2, fty2 );
					
					MyFillBrezenham( ftx1, fty1, ftx2, fty2, coords );
					for ( std::list<CVec2>::iterator ext=coords.begin(); ext!=coords.end(); ++ext )
					{
						SetTileInListOfNormalTiles( dirTiles, ext->x, ext->y, nRes );
					}
				}
				bDragging = false;
				GFXDraw();
			}
		}
		return true;
	}
	
	return false;
}

void CObjectFrame::SetActiveGraphicPropsItem( CTreeItem *pGraphicProps )
{
	if ( pActiveGraphicProps == pGraphicProps )
		return;
	
	pActiveGraphicProps = pGraphicProps;
	CObjectGraphicPropsItem *pGraphicPropsItem = static_cast<CObjectGraphicPropsItem *> ( pActiveGraphicProps );
	NI_ASSERT( pGraphicPropsItem != 0 );
	
	string szDir = GetDirectory( szProjectFileName.c_str() );
	string szObjName;
	const char *pszFileName = pGraphicPropsItem->GetFileName();
	bool bRes = MakeFullPath( szDir.c_str(), pszFileName, szObjName );
	if ( !bRes )
		szObjName = pszFileName;		//popa
	
	LoadSprite( szObjName.c_str() );
}

void CObjectFrame::UpdateActiveSprite()
{
	NI_ASSERT( pActiveGraphicProps != 0 );
	CObjectGraphicPropsItem *pGraphicPropsItem = static_cast<CObjectGraphicPropsItem *> ( pActiveGraphicProps );
	NI_ASSERT( pGraphicPropsItem != 0 );
	
	string szDir = GetDirectory( szProjectFileName.c_str() );
	string szObjName;
	const char *pszFileName = pGraphicPropsItem->GetFileName();
	bool bRes = MakeFullPath( szDir.c_str(), pszFileName, szObjName );
	if ( !bRes )
		szObjName = pszFileName;		//popa
	
	LoadSprite( szObjName.c_str() );
}

bool CObjectFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_OBJECT_ROOT_ITEM );
	CObjectTreeRootItem *pObjectRoot = (CObjectTreeRootItem *) pRootItem;
	
	SObjectRPGStats objectRPGStats;
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
		objectRPGStats.passability.SetSizes( 0, 0 );
		objectRPGStats.vOrigin.x = 0;
		objectRPGStats.vOrigin.y = 0;
	}
	else
	{
		int nTileMinX = lockedTiles.front().nTileX;
		int nTileMaxX = lockedTiles.front().nTileX;
		int nTileMinY = lockedTiles.front().nTileY;
		int nTileMaxY = lockedTiles.front().nTileY;
		
		for ( CListOfTiles::iterator it=lockedTiles.begin(); it!=lockedTiles.end(); ++it )
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
		
		objectRPGStats.passability.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
		BYTE *pBuf = objectRPGStats.passability.GetBuffer();
		for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
		{
			for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
			{
				CListOfTiles::iterator it=lockedTiles.begin();
				for ( ; it!=lockedTiles.end(); ++it )
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
		
		objectRPGStats.vOrigin.x = realZeroPos3.x - mostLeft3.x;
		objectRPGStats.vOrigin.y = realZeroPos3.y - mostLeft3.y;
		
		GFXDraw();
	}
	
	CVec2 zeroPos2 = ::ComputeSpriteNewZeroPos( pSprite, m_zeroPos, CVec2(zeroShiftX, zeroShiftY) );
	pObjectRoot->ComposeAnimations( pszProjectName, GetDirectory(pszResultFileName).c_str(), zeroPos2, objectRPGStats.passability, objectRPGStats.vOrigin );
	
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	
	
	CTreeItem *pGraphicsItem = pRootItem->GetChildItem( E_OBJECT_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	CObjectGraphicPropsItem *pGraphicPropsItem = (CObjectGraphicPropsItem *) pGraphicsItem->GetChildItem( E_OBJECT_GRAPHIC1_PROPS_ITEM );
	
	std::string szFullName;
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pGraphicPropsItem->GetFileName(), szFullName );
	std::string szResName = GetDirectory( pszResultFileName );
	szResName += "icon.tga";
	SaveIconFile( szFullName.c_str(), szResName.c_str() );
	if ( GetFileAttributes( (GetDirectory( pszProjectName ) + "name.txt").c_str() ) != -1 )
	{
		string szRes = GetDirectory( pszResultFileName );
		szRes += "name.txt";
		MyCopyFile( (GetDirectory( pszProjectName ) + "name.txt" ).c_str(), szRes.c_str() );
	}
	return true;
}

void CObjectFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	ASSERT( !pDT->IsReading() );

	SObjectRPGStats objectRPGStats;
	CObjectCommonPropsItem *pCommonProps = static_cast<CObjectCommonPropsItem *> ( pRootItem->GetChildItem( E_OBJECT_COMMON_PROPS_ITEM ) );
	NI_ASSERT( pCommonProps != 0 );
	objectRPGStats.fMaxHP = pCommonProps->GetHealth();
	for ( int i=0; i<6; i++ )
	{
		objectRPGStats.defences[ i ].nArmorMin = pCommonProps->GetArmor();
		objectRPGStats.defences[ i ].nArmorMax = pCommonProps->GetArmor();
		objectRPGStats.defences[ i ].fSilhouette = pCommonProps->GetSilhouette();
	}

	objectRPGStats.szAmbientSound = pCommonProps->GetAmbientSound();
	objectRPGStats.szCycledSound = pCommonProps->GetCycledSound();

	CTreeItem *pPasses = pRootItem->GetChildItem( E_OBJECT_PASSES_ITEM );
	NI_ASSERT( pPasses != 0 );
	for ( CTreeItem::CTreeItemList::const_iterator it=pPasses->GetBegin(); it!=pPasses->GetEnd(); ++it )
	{
		CObjectPassPropsItem *pPassProps = static_cast<CObjectPassPropsItem *> ( it->GetPtr() );
		objectRPGStats.dwAIClasses |= pPassProps->GetPassAIClass();
	}
	
	CObjectEffectsItem *pEffects = static_cast<CObjectEffectsItem *> ( pRootItem->GetChildItem( E_OBJECT_EFFECTS_ITEM ) );
	objectRPGStats.szEffectExplosion = pEffects->GetEffectExplosion();
	objectRPGStats.szEffectDeath = pEffects->GetEffectDeath();

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
		objectRPGStats.passability.SetSizes( 0, 0 );
		objectRPGStats.vOrigin.x = 0;
		objectRPGStats.vOrigin.y = 0;
	}
	else
	{
		int nTileMinX = lockedTiles.front().nTileX;
		int nTileMaxX = lockedTiles.front().nTileX;
		int nTileMinY = lockedTiles.front().nTileY;
		int nTileMaxY = lockedTiles.front().nTileY;
		
		for ( CListOfTiles::iterator it=lockedTiles.begin(); it!=lockedTiles.end(); ++it )
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

		objectRPGStats.passability.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
		BYTE *pBuf = objectRPGStats.passability.GetBuffer();
		for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
		{
			for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
			{
				CListOfTiles::iterator it=lockedTiles.begin();
				for ( ; it!=lockedTiles.end(); ++it )
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

		objectRPGStats.vOrigin.x = realZeroPos3.x - mostLeft3.x;
		objectRPGStats.vOrigin.y = realZeroPos3.y - mostLeft3.y;

		GFXDraw();
	}


	{
		if ( transeparences.empty() && dirTiles.empty() )
		{
			objectRPGStats.visibility.SetSizes( 0, 0 );
			objectRPGStats.vVisOrigin.x = 0;
			objectRPGStats.vVisOrigin.y = 0;
		}
		else
		{
			int nTileMinX, nTileMaxX, nTileMinY, nTileMaxY;
			if ( !transeparences.empty() )
			{
				nTileMinX = transeparences.front().nTileX;
				nTileMaxX = transeparences.front().nTileX;
				nTileMinY = transeparences.front().nTileY;
				nTileMaxY = transeparences.front().nTileY;
			}
			else
			{
				nTileMinX = dirTiles.front().nTileX;
				nTileMaxX = dirTiles.front().nTileX;
				nTileMinY = dirTiles.front().nTileY;
				nTileMaxY = dirTiles.front().nTileY;
			}
			
			{
				for ( CListOfTiles::iterator it=transeparences.begin(); it!=transeparences.end(); ++it )
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
			}
			
			{
				for ( CListOfNormalTiles::iterator it=dirTiles.begin(); it!=dirTiles.end(); ++it )
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
			}
			
			objectRPGStats.visibility.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
			BYTE *pBuf = objectRPGStats.visibility.GetBuffer();
			for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
			{
				for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
				{
					{
						CListOfTiles::iterator it = transeparences.begin();
						for ( ; it!=transeparences.end(); ++it )
						{
							if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
								break;
						}
						
						if ( it != transeparences.end() )
							pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = it->nVal;
						else
							pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = 0;
					}

					{
						CListOfNormalTiles::iterator it = dirTiles.begin();
						for ( ; it!=dirTiles.end(); ++it )
						{
							if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
								break;
						}
						if ( it != dirTiles.end() )
							pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = (it->nVal << 4) | 0x08;
					}
				}
			}
			
			float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
			CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
			CVec2 mostLeft2(fx2, fy2);
			CVec3 mostLeft3;
			pSG->GetPos3( &mostLeft3, mostLeft2 );
			
			objectRPGStats.vVisOrigin.x = realZeroPos3.x - mostLeft3.x;
			objectRPGStats.vVisOrigin.y = realZeroPos3.y - mostLeft3.y;
		}
	}
	
	CTreeAccessor tree = pDT;
	tree.Add( "desc", &objectRPGStats );
}

void CObjectFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	ASSERT( pDT->IsReading() );
	IScene *pSG = GetSingleton<IScene>();
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	
	SObjectRPGStats objectRPGStats;
	CTreeAccessor tree = pDT;
	tree.Add( "desc", &objectRPGStats );
	
	CObjectCommonPropsItem *pCommonProps = static_cast<CObjectCommonPropsItem *> ( pRootItem->GetChildItem( E_OBJECT_COMMON_PROPS_ITEM ) );
	NI_ASSERT( pCommonProps != 0 );
	pCommonProps->SetHealth( objectRPGStats.fMaxHP );
	pCommonProps->SetArmor( objectRPGStats.defences[0].nArmorMax );
	pCommonProps->SetSilhouette( objectRPGStats.defences[0].fSilhouette );
	pCommonProps->SetAmbientSound( objectRPGStats.szAmbientSound.c_str() );
	pCommonProps->SetCycledSound( objectRPGStats.szCycledSound.c_str() );

	CObjectEffectsItem *pEffects = static_cast<CObjectEffectsItem *> ( pRootItem->GetChildItem( E_OBJECT_EFFECTS_ITEM ) );
	pEffects->SetEffectExplosion( objectRPGStats.szEffectExplosion.c_str() );
	pEffects->SetEffectDeath( objectRPGStats.szEffectDeath.c_str() );

	CVec3 beginPos3;						//���������� ������ ������ �����, ������� ������ � vOrigin
	beginPos3.x = 16*fWorldCellSize;
	beginPos3.y = 16*fWorldCellSize;
	beginPos3.z = 0;
	
	CVec2 realZeroPos2;
	CVec3 realZeroPos3;				//��� ����� ������ ���������� �����������
	{
		pSG->GetPos2( &realZeroPos2, m_zeroPos );
		realZeroPos2.x += zeroShiftX;
		realZeroPos2.y += zeroShiftY;
		pSG->GetPos3( &realZeroPos3, realZeroPos2 );
		
		beginPos3.x = realZeroPos3.x - objectRPGStats.vOrigin.x;
		beginPos3.y = realZeroPos3.y - objectRPGStats.vOrigin.y;
	}
	
	POINT pt;
	CVec2 pos2;
	float ftX, ftY;
	
	{
		pSG->GetPos2( &pos2, beginPos3 );
		pt.x = pos2.x + fCellSizeX/2;
		pt.y = pos2.y;
		CGridFrame::ComputeGameTileCoordinates( pt, ftX, ftY );
		int nBeginTileX = ftX, nBeginTileY = ftY;
		
		BYTE *pBuf = objectRPGStats.passability.GetBuffer();
		for ( int y=0; y<objectRPGStats.passability.GetSizeY(); y++ )
		{
			for ( int x=0; x<objectRPGStats.passability.GetSizeX(); x++ )
			{
				int nTemp = pBuf[x+y*objectRPGStats.passability.GetSizeX()];
				if ( nTemp )
				{
					SetTileInListOfTiles( lockedTiles, nBeginTileX + x, nBeginTileY + y, nTemp, E_LOCKED_TILE );
				}
			}
		}
	}
	
	{
		CVec3 beginVis3;						//���������� ������ ������ �����, ������� ������ � vVisOrigin
		beginVis3.x = realZeroPos3.x - objectRPGStats.vVisOrigin.x;
		beginVis3.y = realZeroPos3.y - objectRPGStats.vVisOrigin.y;
		beginVis3.z = 0;
		pSG->GetPos2( &pos2, beginVis3 );
		pt.x = pos2.x + fCellSizeX/2;
		pt.y = pos2.y;
		CGridFrame::ComputeGameTileCoordinates( pt, ftX, ftY );
		int nVisTileX = ftX, nVisTileY = ftY;
		
		BYTE *pBuf = objectRPGStats.visibility.GetBuffer();
		for ( int y=0; y<objectRPGStats.visibility.GetSizeY(); y++ )
		{
			for ( int x=0; x<objectRPGStats.visibility.GetSizeX(); x++ )
			{
				int nTemp = pBuf[x+y*objectRPGStats.visibility.GetSizeX()];
				if ( nTemp & 0x07 )
				{
					SetTileInListOfTiles( transeparences, nVisTileX + x, nVisTileY + y, nTemp & 0x07, E_TRANSEPARENCE_TILE );
				}
				else if ( nTemp & 0x08 )
				{
					SetTileInListOfNormalTiles( dirTiles, nVisTileX + x, nVisTileY + y, nTemp >> 4 );
				}
			}
		}
	}
}

int CObjectFrame::STransLine::operator &( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Point1", &p1 );
	saver.Add( "Point2", &p2 );
	return 0;
}

void CObjectFrame::SaveFrameOwnData( IDataTree *pDT )
{
	pDT->StartChunk( "own_data" );
	CTreeAccessor tree = pDT;
/*
	if ( pSprite )
	{
		CVec3 pos3 = pSprite->GetPosition();
		tree.Add( "sprite_pos", &pos3 );
	}
*/
	tree.Add( "sprite_pos", &m_SpriteLoadPos );
	
	tree.Add( "krest_pos", &m_zeroPos );
	
	tree.Add( "export_file_name", &szPrevExportFileName );
	
	tree.Add( "TransLines", &transLines );
	
	pDT->FinishChunk();
}

void CObjectFrame::LoadFrameOwnData( IDataTree *pDT )
{
	pDT->StartChunk( "own_data" );
	CTreeAccessor tree = pDT;
	
	tree.Add( "sprite_pos", &m_SpriteLoadPos );
	if ( pSprite )
	{
		GetSingleton<IScene>()->MoveObject( pSprite, m_SpriteLoadPos );
		pSprite->SetPosition( m_SpriteLoadPos );
	}

	tree.Add( "krest_pos", &m_zeroPos );
	
	string szPrevExportDir;
	tree.Add( "export_dir", &szPrevExportDir );
	if ( szPrevExportDir.size() > 0 )
	{
		szPrevExportFileName = szPrevExportDir;
		szPrevExportFileName += "1.xml";
	}
	else
		tree.Add( "export_file_name", &szPrevExportFileName );
	
	tree.Add( "TransLines", &transLines );
	bDragging = true;
	for ( CTransLineList::iterator it=transLines.begin(); it!=transLines.end(); ++it )
	{
		it->pVertices = pGFX->CreateVertices( 2, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
		{
			CVerticesLock<SGFXTLVertex> vertices( it->pVertices );
			vertices[0].Setup( it->p1.x, it->p1.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
			vertices[1].Setup( it->p2.x, it->p2.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
		}
		currentLine.p1 = it->p1;
		currentLine.p2 = it->p2;
		currentLine.pNormalVertices = pGFX->CreateVertices( 2, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
		UpdateNormalForSelectedLine();
		it->pNormalVertices = currentLine.pNormalVertices;
	}
	currentLine.pNormalVertices = 0;
	bDragging = false;
	
	pDT->FinishChunk();
}

int CObjectFrame::UpdateNormalForSelectedLine()
{
	CVec2 vShift;
	NI_ASSERT( bDragging == true );

	IScene *pSG = GetSingleton<IScene>();
	CVec3 v1, v2, r;												//r ��� ������ ����� �������
	r.z = 0;
	pSG->GetPos3( &v1, currentLine.p1 );
	pSG->GetPos3( &v2, currentLine.p2 );
	
	CVerticesLock<SGFXTLVertex> vertices( currentLine.pNormalVertices );
	float fcx = (v1.x + v2.x) / 2;
	float fcy = (v1.y + v2.y) / 2;
	float alpha = atan2( v2.y - v1.y, v2.x - v1.x );
	alpha += FP_PI / 16.0f;
	if ( alpha > FP_PI )								//�� -PI �� PI
		alpha -= FP_2PI;

	int nRes = (float) ( FP_PI + alpha ) * 8 / FP_PI;				//���� � �������� 0..15
	nRes = ( nRes + 8 ) % 16;
	NI_ASSERT( nRes >= 0 && nRes <= 15 );
	alpha = (float) nRes * FP_PI8;
	r.x = fcx - NORMAL_LENGTH * sin( alpha );
	r.y = fcy + NORMAL_LENGTH * cos( alpha );
	
	CVec2 r2;
	pSG->GetPos2( &r2, r );
	vertices[0].Setup( (currentLine.p1.x+currentLine.p2.x)/2, (currentLine.p1.y+currentLine.p2.y)/2, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
	vertices[1].Setup( r2.x, r2.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
	return nRes;
}

class CLockedTilesFunc
{
public:
	std::list<CVec2> coords;
	
	void operator() ( int x, int y )
	{
		coords.push_back( CVec2(x, y) );
	}
};

void CObjectFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//���� ������ �� ��� ������
		return;
	
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
		}
	}

	else if ( tbStyle == E_SET_ZERO )
	{
		CVec2 pt( point.x - zeroShiftX, point.y - zeroShiftY );
		GetSingleton<IScene>()->GetPos3( &m_zeroPos, pt );
		m_mode = E_SET_ZERO;
		SetChangedFlag( true );
	}
	
	else if ( tbStyle == E_DRAW_ONE_WAY_TRANSEPARENCE )
	{
		if ( bDragging )
		{
			currentLine.p2.x = point.x;
			currentLine.p2.y = point.y;
			{
				CVerticesLock<SGFXTLVertex> vertices( currentLine.pVertices );
				vertices[1].Setup( point.x, point.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
			}
			int nRes = UpdateNormalForSelectedLine();		//��������� �������

			float ftx1, fty1, ftx2, fty2;
			POINT pt;
			pt.x = currentLine.p1.x;
			pt.y = currentLine.p1.y;
			ComputeGameTileCoordinates( pt, ftx1, fty1 );
			pt.x = currentLine.p2.x;
			pt.y = currentLine.p2.y;
			ComputeGameTileCoordinates( pt, ftx2, fty2 );
			
			std::list<CVec2> coords;
			MyFillBrezenham( ftx1, fty1, ftx2, fty2, coords );
			for ( std::list<CVec2>::iterator it=coords.begin(); it!=coords.end(); ++it )
				SetTileInListOfNormalTiles( dirTiles, it->x, it->y, nRes );

			transLines.push_back( currentLine );
			bDragging = false;
			SetChangedFlag( true );
			GFXDraw();
		}
		else
		{
			CTransLineList::iterator it = transLines.begin();
			int i = 0;
			for ( ; it!=transLines.end(); ++it )
			{
				float fcx = (it->p1.x + it->p2.x) / 2;
				float fcy = (it->p1.y + it->p2.y) / 2;
				if ( point.x >= fcx - DELTA_X && point.x <= fcx + DELTA_X && point.y >= fcy - DELTA_Y && point.y <= fcy + DELTA_Y )
				{
					{
						if ( m_nSelected == i )
							break;		//�������� ����� �� �� ����� �����
						
						int k = 0;
						for ( CTransLineList::iterator it=transLines.begin(); it!=transLines.end(); ++it )
						{
							if ( k == m_nSelected )
							{
								CVerticesLock<SGFXTLVertex> vertices( it->pVertices );
								vertices[0].Setup( it->p1.x, it->p1.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
								vertices[1].Setup( it->p2.x, it->p2.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
								m_nSelected = -1;
								break;
							}
							k++;
						}
					}
					
					CVerticesLock<SGFXTLVertex> vertices( it->pVertices );
					vertices[0].Setup( it->p1.x, it->p1.y, 1, 1, 0xffffff00, 0xff000000, 0, 0 );
					vertices[1].Setup( it->p2.x, it->p2.y, 1, 1, 0xffffff00, 0xff000000, 0, 0 );
					m_nSelected = i;
					break;
				}
				i++;
			}
			
			if ( it == transLines.end() )
			{
				if ( m_nSelected >= 0 )
				{
					int i = 0;
					for ( it=transLines.begin(); it!=transLines.end(); ++it )
					{
						if ( i == m_nSelected )
						{
							CVerticesLock<SGFXTLVertex> vertices( it->pVertices );
							vertices[0].Setup( it->p1.x, it->p1.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
							vertices[1].Setup( it->p2.x, it->p2.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
							m_nSelected = -1;
							break;
						}
						i++;
					}
					
					NI_ASSERT( it != transLines.end() );
				}
				
				currentLine.p1.x = point.x;
				currentLine.p1.y = point.y;
				currentLine.p2.x = point.x;
				currentLine.p2.y = point.y;
				currentLine.pVertices = pGFX->CreateVertices( 2, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
				{
					CVerticesLock<SGFXTLVertex> vertices( currentLine.pVertices );
					vertices[0].Setup( point.x, point.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
					vertices[1].Setup( point.x, point.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
				}
				currentLine.pNormalVertices = pGFX->CreateVertices( 2, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
				{
					CVerticesLock<SGFXTLVertex> vertices( currentLine.pNormalVertices );
					vertices[0].Setup( point.x, point.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
					vertices[1].Setup( point.x, point.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
				}
				bDragging = true;
			}
			GFXDraw();
		}
	}
	
	CGridFrame::OnLButtonDown(nFlags, point);
}

void CObjectFrame::OnRButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//���� ������ �� ��� ������
		return;
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

	else if ( tbStyle == E_DRAW_ONE_WAY_TRANSEPARENCE && bDragging )
	{
		bDragging = false;
		GFXDraw();
	}

	CGridFrame::OnRButtonDown(nFlags, point);
}

void CObjectFrame::OnMouseMove(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//���� ������ �� ��� ������
		return;
	
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
	
	else if ( tbStyle == E_DRAW_TRANSEPARENCE && nFlags & MK_RBUTTON )
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
		GFXDraw();
	}
	
	else if ( m_mode == E_SET_ZERO && nFlags & MK_LBUTTON )
	{
		CVec2 pt( point.x - zeroShiftX, point.y - zeroShiftY );
		GetSingleton<IScene>()->GetPos3( &m_zeroPos, pt );
		GFXDraw();
		SetChangedFlag( true );
	}
	
	else if ( tbStyle == E_DRAW_ONE_WAY_TRANSEPARENCE  )
	{
		if ( bDragging )
		{
			currentLine.p2.x = point.x;
			currentLine.p2.y = point.y;
			CVerticesLock<SGFXTLVertex> vertices( currentLine.pVertices );
			vertices[1].Setup( point.x, point.y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
			UpdateNormalForSelectedLine();		//��������� �������
		}
		else
		{
			bDrawRect = false;
			for ( CTransLineList::iterator it=transLines.begin(); it!=transLines.end(); ++it )
			{
				float fcx = (it->p1.x + it->p2.x) / 2;
				float fcy = (it->p1.y + it->p2.y) / 2;
				if ( point.x >= fcx - DELTA_X && point.x <= fcx + DELTA_X && point.y >= fcy - DELTA_Y && point.y <= fcy + DELTA_Y )
				{
					CVerticesLock<SGFXTLVertex> vertices( pRectVertices );
					vertices[0].Setup( fcx-DELTA_X, fcy-DELTA_Y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
					vertices[1].Setup( fcx+DELTA_X, fcy-DELTA_Y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
					vertices[2].Setup( fcx+DELTA_X, fcy+DELTA_Y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
					vertices[3].Setup( fcx-DELTA_X, fcy+DELTA_Y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
					vertices[4].Setup( fcx-DELTA_X, fcy-DELTA_Y, 1, 1, 0xff0000ff, 0xff000000, 0, 0 );
					bDrawRect = true;
					break;
				}
			}
		}
		
		GFXDraw();
	}

	CGridFrame::OnMouseMove(nFlags, point);
}

void CObjectFrame::OnLButtonUp(UINT nFlags, CPoint point) 
{
	ReleaseCapture();
	m_mode = -1;
	GFXDraw();

	CGridFrame::OnLButtonUp(nFlags, point);
}

void CObjectFrame::CreateKrest()
{
	if ( pKrestTexture == 0 )
	{
		ITextureManager *pTM = GetSingleton<ITextureManager>();
		pKrestTexture = pTM->GetTexture( "editor\\krest\\1" );
	}
}

void CObjectFrame::LoadSprite( const char *pszSpriteFullName )
{
	IScene *pSG = GetSingleton<IScene>();
	if ( pSprite )
		pSG->RemoveObject( pSprite );
	
	string szTempDir = theApp.GetEditorTempDir();
	if ( !ComposeSingleSprite( pszSpriteFullName, szTempDir.c_str(), "Object" ) )
	{
		pSprite = 0;
		GFXDraw();
		return;
	}
	
	szTempDir = theApp.GetEditorTempResourceDir();
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	
	pSprite = static_cast<IObjVisObj *> ( pVOB->BuildObject( (szTempDir + "\\Object").c_str(), 0, SGVOT_SPRITE ) );
	NI_ASSERT( pSprite != 0 );
	pSprite->SetPosition( m_SpriteLoadPos );
	pSprite->SetDirection( 0 );
	pSprite->SetAnimation( 0 );
	pSG->AddObject( pSprite, SGVOGT_UNIT );
	pSprite->SetOpacity( 140 );
	GFXDraw();
}

bool CObjectFrame::LoadFramePreExportData( const char *pszProjectFile, CTreeItem *pRootItem )
{
	CTreeItem *pGraphicsItem = pRootItem->GetChildItem( E_OBJECT_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	CObjectGraphicPropsItem *pGraphicPropsItem = (CObjectGraphicPropsItem *) pGraphicsItem->GetChildItem( E_OBJECT_GRAPHIC1_PROPS_ITEM );
	NI_ASSERT( pGraphicPropsItem != 0 );
	const char *pszFileName = pGraphicPropsItem->GetFileName();
	NI_ASSERT( pszFileName != 0 );
	
	string szDir = GetDirectory( pszProjectFile );
	string szObjName;
	bool bRes = MakeFullPath( szDir.c_str(), pszFileName, szObjName );
	if ( !bRes )
		szObjName = pszFileName;		//popa
	
	LoadSprite( szObjName.c_str() );
	pSprite->Update( timeGetTime() );
	
	CVec3 vSave = m_zeroPos;
	CreateKrest();
	m_zeroPos = vSave;
	GFXDraw();
	return true;
}

void CObjectFrame::OnDrawGrid() 
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_GRID;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6);
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_ONE_WAY_TRANSEPARENCE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	GFXDraw();
}

void CObjectFrame::OnChangeTranseparence()
{
	NI_ASSERT( m_pTransparenceCombo != 0 );
 	m_transValue = m_pTransparenceCombo->GetCurSel();
}

void CObjectFrame::OnSetFocusTranseparence()
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_TRANSEPARENCE;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6);
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	m_pTransparenceCombo->SetFocus();
	nIndex = pToolBar->CommandToIndex( ID_DRAW_ONE_WAY_TRANSEPARENCE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	GFXDraw();
}

void CObjectFrame::OnMoveObject() 
{
	UINT nIndex = 0;
	tbStyle = E_MOVE_OBJECT;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6);
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_ONE_WAY_TRANSEPARENCE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	GFXDraw();
}

void CObjectFrame::OnSetZeroButton() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_ZERO;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6);
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_ONE_WAY_TRANSEPARENCE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	GFXDraw();
}

void CObjectFrame::OnDrawOneWayTranseparence() 
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_ONE_WAY_TRANSEPARENCE;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6);
	nIndex = pToolBar->CommandToIndex( ID_DRAW_ONE_WAY_TRANSEPARENCE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	GFXDraw();
}

void CObjectFrame::OnUpdateMoveObject(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CObjectFrame::OnUpdateDrawGrid(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CObjectFrame::OnUpdateSetZeroButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CObjectFrame::OnUpdateDrawTransparence(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

void CObjectFrame::OnUpdateDrawOneWayTranseparence(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

FILETIME CObjectFrame::FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem )
{
	CObjectTreeRootItem *pObjectRoot = static_cast<CObjectTreeRootItem *> ( pRootItem );
	return pObjectRoot->FindMaximalSourceTime( pszProjectName );
}

FILETIME CObjectFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime, current;
	string szDestDir = GetDirectory( pszResultFileName );
	
	string szTempFileName = szDestDir;
	szTempFileName += "1.san";
	current = GetFileChangeTime( szTempFileName.c_str() );
	minTime = current;
	
	szTempFileName = szDestDir;
	szTempFileName += "1.tga";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;

	szTempFileName = szDestDir;
	szTempFileName += "1s.san";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szTempFileName = szDestDir;
	szTempFileName += "1s.tga";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;

	CTreeItem *pGraphics = pRootItem->GetChildItem( E_OBJECT_GRAPHICS_ITEM );
	NI_ASSERT( pGraphics != 0 );
	CObjectGraphicW1PropsItem *pWinterGraphProps = static_cast<CObjectGraphicW1PropsItem *> ( pGraphics->GetChildItem( E_OBJECT_GRAPHICW1_PROPS_ITEM ) );
	NI_ASSERT( pWinterGraphProps != 0 );
	if ( strlen( pWinterGraphProps->GetFileName() ) )
	{
		string szTempFileName = szDestDir;
		szTempFileName += "1w.san";
		current = GetFileChangeTime( szTempFileName.c_str() );
		minTime = current;
		
		szTempFileName = szDestDir;
		szTempFileName += "1w.tga";
		current = GetFileChangeTime( szTempFileName.c_str() );
		if ( current < minTime )
			minTime = current;
		
		szTempFileName = szDestDir;
		szTempFileName += "1ws.san";
		current = GetFileChangeTime( szTempFileName.c_str() );
		if ( current < minTime )
			minTime = current;
		
		szTempFileName = szDestDir;
		szTempFileName += "1ws.tga";
		current = GetFileChangeTime( szTempFileName.c_str() );
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
