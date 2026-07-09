#include "stdafx.h"
#include <io.h>
#include "..\Common\LegacyUiCompat.h"

#include "..\GFX\GFX.h"
#include "..\GFX\GFXHelper.h"
#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"

#include "editor.h"
#include "MainFrm.h"			//��� ������ � ��������
#include "SpriteCompose.h"
#include "BuildCompose.h"
#include "PropView.h"
#include "TreeItem.h"
#include "BridgeFrm.h"
#include "BridgeView.h"
#include "GameWnd.h"
#include "frames.h"


static const float fOX = -622;			//�� ������ ��� ��������, ����������� ���������������� ��� ������������� �� ������� ���������.
static const float fOY = 296;				//����� ������ ����� ���������� AI ������

static int zeroSizeX = 32;
static int zeroSizeY = 32;
static float zeroShiftX = 15.4f;
static float zeroShiftY = 15.4f;

static const int MIN_OPACITY = 120;
static const int MAX_OPACITY = 255;

const CVec3 vCenterPosition( 628.357f, 730.381f, 0 );	//��� ����������� ���������, ����� ����������� ����� ����� ���������� �� ����������� ������
const CVec3 vCenterKrest( 596.657f, 742.038f, 0 );		//��� ���������� ������ �������� ��� ������� ����� ����������� �����


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(CBridgeFrame, CGridFrame)

BEGIN_MESSAGE_MAP(CBridgeFrame, CGridFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_DRAW_GRID, OnDrawGrid)
	ON_UPDATE_COMMAND_UI(ID_DRAW_GRID, OnUpdateDrawGrid)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_CBN_SETFOCUS( IDC_BRIDGE_TRANSPARENCE, OnSetFocusTranseparence )
	ON_CBN_SELCHANGE( IDC_BRIDGE_TRANSPARENCE, OnChangeTranseparence )
	ON_COMMAND(ID_SET_ZERO_BUTTON, OnSetZeroButton)
	ON_UPDATE_COMMAND_UI(ID_SET_ZERO_BUTTON, OnUpdateSetZeroButton)
	ON_UPDATE_COMMAND_UI(ID_BRIDGE_TRANSPARENCE, OnUpdateDrawTransparence)
	ON_COMMAND(ID_DRAW_PASS, OnDrawPass)
	ON_UPDATE_COMMAND_UI(ID_DRAW_PASS, OnUpdateDrawPass)
	ON_COMMAND(ID_FILE_BATCH_MODE, OnFileBatchMode)

	ON_COMMAND(ID_SET_SHOOT_ANGLE, OnSetShootAngle)
	ON_UPDATE_COMMAND_UI(ID_SET_SHOOT_ANGLE, OnUpdateSetShootAngle)
	ON_COMMAND(ID_SET_HORIZONTAL_SHOOT, OnSetHorizontalShoot)
	ON_UPDATE_COMMAND_UI(ID_SET_HORIZONTAL_SHOOT, OnUpdateSetHorizontalShoot)
	ON_COMMAND(ID_SET_FIRE_POINT, OnSetFirePoint)
	ON_UPDATE_COMMAND_UI(ID_SET_FIRE_POINT, OnUpdateSetFirePoint)
	ON_COMMAND(ID_MOVE_POINT, OnMovePoint)
	ON_UPDATE_COMMAND_UI(ID_MOVE_POINT, OnUpdateMovePoint)
	ON_COMMAND(ID_SET_SMOKE_POINT, OnSetSmokePoint)
	ON_UPDATE_COMMAND_UI(ID_SET_SMOKE_POINT, OnUpdateSetSmokePoint)
	ON_COMMAND(ID_GENERATE_POINTS, OnGeneratePoints)
	ON_UPDATE_COMMAND_UI(ID_GENERATE_POINTS, OnUpdateGeneratePoints)
END_MESSAGE_MAP()


CBridgeFrame::CBridgeFrame() : vSpriteCommonPos( vCenterPosition )
{
	szComposerName = "Bridge Editor";
	szExtension = "*.bdg";
	szComposerSaveName = "Bridge_Composer_Project";
	nTreeRootItemID = E_BRIDGE_ROOT_ITEM;
	nFrameType = CFrameManager::E_BRIDGE_FRAME;
	pWndView = new CBridgeView;
	szAddDir = "bridges\\";
	
	pActiveSpansItem = 0;
	pActiveSpanPropsItem = 0;
	m_transValue = 0;
	vBeginPos = vEndPos = CVec3(16*fWorldCellSize - 300, 16*fWorldCellSize, 0);
	m_fBack = 0.0f;
	m_fFront = 0.0f;
	m_bHorizontal = true;
	bEditSpansEnabled = false;

	m_mode = -1;
	pActiveFirePoint = 0;
	pActiveSmokePoint = 0;
	eActiveMode = E_FIRE_POINT;
	eActiveSubMode = E_SUB_MOVE;
	m_pTransparenceCombo = 0;
	m_transValue = 0;
	m_fMinY = m_fMaxY = m_fX = 0;

	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB1555;
}

CBridgeFrame::~CBridgeFrame()
{
}

int CBridgeFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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

	return 0;
}

void CBridgeFrame::Init( IGFX *_pGFX )
{
	CGridFrame::Init( _pGFX );
	
	pLineIndices = pGFX->CreateIndices( 2, GFXIF_INDEX16, GFXPT_LINELIST, GFXD_DYNAMIC );
	{
		CIndicesLock<WORD> indices( pLineIndices );
		indices[0] = 0;
		indices[1] = 1;
	}
	
	pLineVertices = pGFX->CreateVertices( 2, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
	SetBridgeType( true );
	
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

void CBridgeFrame::SetBridgeType( bool bHorizontal )
{
	m_bHorizontal = bHorizontal;
	float fX = fOX;
	float fY = fOY;
	const int MY_SIZE_X = 1000;
	const int MY_SIZE_Y = 500;

	{
		CVerticesLock<SGFXTLVertex> vertices( pLineVertices );
		
		if ( !bHorizontal )
		{
			const int nIndex = 14;
			m_fx1 = fX + fCellSizeX*nIndex;
			m_fy1 = fY + fCellSizeY*nIndex;
			m_fx2 = fX + fCellSizeX*nIndex + MY_SIZE_X;
			m_fy2 = fY + fCellSizeY*nIndex - MY_SIZE_Y;
			vertices[0].Setup( m_fx1, m_fy1,
				1, 1, 0xffff0000, 0xff00000, 0, 0 );
			vertices[1].Setup( m_fx2, m_fy2,
				1, 1, 0xffff0000, 0xff00000, 0, 0 );
		}
		else
		{
			const int nIndex = 16;
			m_fx1 = fX + fCellSizeX*nIndex;
			m_fy1 = fY - fCellSizeY*nIndex;
			m_fx2 = fX + fCellSizeX*nIndex + MY_SIZE_X;
			m_fy2 = fY - fCellSizeY*nIndex + MY_SIZE_Y;
			vertices[0].Setup( m_fx1, m_fy1,
				1, 1, 0xffff0000, 0xff00000, 0, 0 );
			vertices[1].Setup( m_fx2, m_fy2,
				1, 1, 0xffff0000, 0xff00000, 0, 0 );
		}
	}

	GFXDraw();
}

float CBridgeFrame::GetPointOnLine( float fx )
{
	return m_fy1 + (fx - m_fx1)*(m_fy2-m_fy1)/(m_fx2-m_fx1);
}

void CBridgeFrame::CreateKrest()
{
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	pKrestTexture = pTM->GetTexture( "editor\\krest\\1" );
}

void CBridgeFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( nCommand );
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->SetAnchor( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );
	
	/*
	CVec2 v2;
	IScene *pScene = GetSingleton<IScene>();
	pScene->GetPos2( &v2, vCenterPosition );
	v2.x += 256;
	v2.y += 256;
	CVec3 v3;
	pScene->GetPos3( &v3, v2 );
	*/
}

void CBridgeFrame::DrawLockedTiles( IGFX *pGFX )
{
	if ( !pActiveSpansItem )
		return;

	for ( CListOfTiles::iterator it=pActiveSpansItem->lockedTiles.begin(); it!=pActiveSpansItem->lockedTiles.end(); ++it )
		pGFX->Draw( it->pVertices, pMarkerIndices );
}

void CBridgeFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();
	
	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );
	CGridFrame::GFXDraw();

	CETreeCtrl *pTree = 0;
	if ( pTreeDockBar && (pTree = pTreeDockBar->GetTreeWithIndex( 0 )) && pTree->GetRootItem() )
	{
		if ( m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
		{
			if ( tbStyle == E_DRAW_GRID )
			{
				DrawLockedTiles( pGFX );
			}
			else if ( tbStyle == E_DRAW_PASS )
			{
				for ( CListOfTiles::iterator it=pActiveSpansItem->unLockedTiles.begin(); it!=pActiveSpansItem->unLockedTiles.end(); ++it )
					pGFX->Draw( it->pVertices, pMarkerIndices );
			}
			else if ( tbStyle == E_DRAW_TRANSEPARENCE )
			{
				for ( CListOfTiles::iterator it=pActiveSpansItem->transeparences.begin(); it!=pActiveSpansItem->transeparences.end(); ++it )
					pGFX->Draw( it->pVertices, pMarkerIndices );
			}
		}
		
		pGFX->Draw( pLineVertices, pLineIndices );
		
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

		ICamera *pCamera = GetSingleton<ICamera>();
		IScene *pSG = GetSingleton<IScene>();
		pCamera->Update();
		pSG->Draw( pCamera );
		
		if ( pTreeDockBar && pTreeDockBar->GetTreeWithIndex( 0 ) && 
			!( tbStyle == E_SET_FIRE_POINT || tbStyle == E_SET_SMOKE_POINT ) )
		{
			SGFXRect2 rc;
			rc.maps = CTRect<float> ( 0.0f, 0.0f, 1.0f, 1.0f );
			CVec2 krestPos2;
			pGFX->SetTexture( 0, pKrestTexture );
			if ( m_drawMode == E_DRAW_SPANS )
			{
				if ( pActiveSpansItem != 0 )
				{
					CTreeItem *pPapa = pActiveSpansItem->GetParentTreeItem();
					CVec3 vPapa;
					switch( pPapa->GetItemType() )
					{
					case E_BRIDGE_CENTER_SPANS_ITEM:
						vPapa = vCenterKrest;
						break;
					case E_BRIDGE_BEGIN_SPANS_ITEM:
						vPapa = vBeginPos;
						break;
					case E_BRIDGE_END_SPANS_ITEM:
						vPapa = vEndPos;
						break;
					}

					CVec3 v3 = vPapa;
					pSG->GetPos2( &krestPos2, v3 );
					rc.rect = CTRect<float> ( krestPos2.x, krestPos2.y, krestPos2.x+zeroSizeX, krestPos2.y+zeroSizeY );
					pGFX->SetupDirectTransform();
					pGFX->DrawRects( &rc, 1 );

					if ( m_bHorizontal )
						v3.y += m_fFront;
					else
						v3.x += m_fFront;
					pSG->GetPos2( &krestPos2, v3 );
					rc.rect = CTRect<float> ( krestPos2.x, krestPos2.y, krestPos2.x+zeroSizeX, krestPos2.y+zeroSizeY );
					pGFX->DrawRects( &rc, 1 );
					
					v3 = vPapa;
					if ( m_bHorizontal )
						v3.y += m_fBack;
					else
						v3.x += m_fBack;
					pSG->GetPos2( &krestPos2, v3 );
					rc.rect = CTRect<float> ( krestPos2.x, krestPos2.y, krestPos2.x+zeroSizeX, krestPos2.y+zeroSizeY );
					pGFX->DrawRects( &rc, 1 );
					pGFX->RestoreTransform();
				}
			}
			else
			{
				if ( pActiveSpanPropsItem != 0 )
				{
					CTreeItem *pPapa = pActiveSpanPropsItem->GetParentTreeItem();
					CTreeItem *pTop = pPapa->GetParentTreeItem();
					CVec3 v3;
					switch( pTop->GetItemType() )
					{
					case E_BRIDGE_CENTER_SPANS_ITEM:
						v3 = vCenterKrest;
						break;
					case E_BRIDGE_BEGIN_SPANS_ITEM:
						v3 = vBeginPos;
						break;
					case E_BRIDGE_END_SPANS_ITEM:
						v3 = vEndPos;
						break;
					}
					
					int i = 0;
					for ( CTreeItem::CTreeItemList::const_iterator it=pPapa->GetBegin(); it!=pPapa->GetEnd(); ++it )
					{
						if ( it->GetPtr() == pActiveSpanPropsItem )
							break;
						i++;
					}
					
					if ( i == 0 )
					{
						if ( m_bHorizontal )
							v3.y += m_fBack;
						else
							v3.x += m_fBack;
					}
					else if ( i == 1 )
					{
						if ( m_bHorizontal )
							v3.y += m_fFront;
						else
							v3.x += m_fFront;
					}
					
					pSG->GetPos2( &krestPos2, v3 );
					rc.rect = CTRect<float> ( krestPos2.x, krestPos2.y, krestPos2.x+zeroSizeX, krestPos2.y+zeroSizeY );
					pGFX->SetupDirectTransform();
					pGFX->DrawRects( &rc, 1 );
					pGFX->RestoreTransform();
				}
			}
		}
	}		//end if pTree
	
	pGFX->EndScene();
	pGFX->Flip();
}

void CBridgeFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
	CreateKrest();
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	CBridgeCommonPropsItem *pCommonProps = static_cast<CBridgeCommonPropsItem *>( pRootItem->GetChildItem( E_BRIDGE_COMMON_PROPS_ITEM ) );
	SetBridgeType( pCommonProps->GetDirection() );

/*
	CBridgeCenterSpansItem *pCenterSpans = static_cast<CBridgeCenterSpansItem *>( pRootItem->GetChildItem( E_BRIDGE_CENTER_SPANS_ITEM ) );
	for ( CTreeItem::CTreeItemList::const_iterator ext=pCenterSpans->GetBegin(); ext!=pCenterSpans->GetEnd(); ++ext )
	{
		CBridgePartPropsItem *pProps = static_cast<CBridgePartPropsItem *> ( (*ext)->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 2 ) );
		NI_ASSERT( pProps != 0 );
		pProps->vKrestPos = vCenterKrest;
	}
*/
}

void CBridgeFrame::SpecificClearBeforeBatchMode()
{
	OnMovePoint();

	freeSpanIndexes[0].clear();
	freeSpanIndexes[0].push_back( 0 );

	freeSpanIndexes[1].clear();
	freeSpanIndexes[1].push_back( 0 );

	freeSpanIndexes[2].clear();
	freeSpanIndexes[2].push_back( 0 );
	
	bEditSpansEnabled = false;
	m_fx1 = m_fx2 = m_fy1 = m_fy2 = 0.0f;
	pActiveSpansItem = 0;
	pActiveSpanPropsItem = 0;

	pActiveFirePoint = 0;
	pActiveSmokePoint = 0;
	eActiveMode = E_FIRE_POINT;
	m_fMinY = m_fMaxY = m_fX = 0;
	
	IScene *pSG = GetSingleton<IScene>();
	pSG->Clear();
}

BOOL CBridgeFrame::SpecificTranslateMessage( MSG *pMsg )
{
	if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_DELETE )
	{
		if ( GetFocus() != pWndView )
			return false;
		
		if ( eActiveMode == E_FIRE_POINT && pActiveFirePoint )
		{
			pActiveFirePoint->pFirePoint->DeleteMeInParentTreeItem();
			DeleteFirePoint( pActiveFirePoint->pFirePoint );
			GFXDraw();
			return true;
		}
		
		if ( eActiveMode == E_SMOKE_POINT && pActiveSmokePoint )
		{
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

bool CBridgeFrame::LoadFramePreExportData( const char *pszProjectFile, CTreeItem *pRootItem )
{
	CreateKrest();

	CBridgeCommonPropsItem *pCommonProps = static_cast<CBridgeCommonPropsItem *>( pRootItem->GetChildItem( E_BRIDGE_COMMON_PROPS_ITEM ) );
	SetBridgeType( pCommonProps->GetDirection() );
	return true;
}

void CBridgeFrame::SaveFrameOwnData( IDataTree *pDT )
{
	pDT->StartChunk( "own_data" );
	CTreeAccessor tree = pDT;
	
	string szPrevExportDir;
	tree.Add( "export_dir", &szPrevExportDir );
	if ( szPrevExportDir.size() > 0 )
	{
		szPrevExportFileName = szPrevExportDir;
		szPrevExportFileName += "1.xml";
	}
	else
		tree.Add( "export_file_name", &szPrevExportFileName );

	tree.Add( "Begin", &vBeginPos );
	tree.Add( "End", &vEndPos );
	tree.Add( "Front", &m_fFront );
	tree.Add( "Back", &m_fBack );

	pDT->FinishChunk();
}

void CBridgeFrame::LoadFrameOwnData( IDataTree *pDT )
{
	pDT->StartChunk( "own_data" );
	CTreeAccessor tree = pDT;

	tree.Add( "export_file_name", &szPrevExportFileName );

	tree.Add( "Begin", &vBeginPos );
	tree.Add( "End", &vEndPos );
	tree.Add( "Front", &m_fFront );
	tree.Add( "Back", &m_fBack );
	
	pDT->FinishChunk();
}

void CBridgeFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	for ( int nDamageIndex=0; nDamageIndex<3; nDamageIndex++ )
	{
/*
		std::string szPostfix = "C";
		if ( nDamageIndex == 1 )
			szPostfix += "1";
		else if ( nDamageIndex == 2 )
			szPostfix += "2";
*/
		CTreeItem *pStage = pRootItem->GetChildItem( E_BRIDGE_STAGE_PROPS_ITEM, nDamageIndex );
		
		std::set<int> indexSet;
		CTreeItem::CTreeItemList::const_iterator it;

		CTreeItem *pBegin = pStage->GetChildItem( E_BRIDGE_BEGIN_SPANS_ITEM );
		it = pBegin->GetBegin();
		for ( ; it!=pBegin->GetEnd(); ++it )
		{
			CBridgePartsItem *pPartsItem = static_cast<CBridgePartsItem *> ( it->GetPtr() );
			indexSet.insert( pPartsItem->nSpanIndex );
/*
			for ( CTreeItem::CTreeItemList::const_iterator in=pPartsItem->GetBegin(); in!=pPartsItem->GetEnd(); ++in )
			{
				CBridgePartPropsItem *pProps = static_cast<CBridgePartPropsItem *> ( in->GetPtr() );
				pProps->
			}
*/
		}
		
		CTreeItem *pCenter = pStage->GetChildItem( E_BRIDGE_CENTER_SPANS_ITEM );
		it = pCenter->GetBegin();
		for ( ; it!=pCenter->GetEnd(); ++it )
		{
			CBridgePartsItem *pPartsItem = static_cast<CBridgePartsItem *> ( it->GetPtr() );
			indexSet.insert( pPartsItem->nSpanIndex );
		}
		
		CTreeItem *pEnd = pStage->GetChildItem( E_BRIDGE_END_SPANS_ITEM );
		it = pEnd->GetBegin();
		for ( ; it!=pEnd->GetEnd(); ++it )
		{
			CBridgePartsItem *pPartsItem = static_cast<CBridgePartsItem *> ( it->GetPtr() );
			indexSet.insert( pPartsItem->nSpanIndex );
		}
		
		freeSpanIndexes[nDamageIndex].clear();
		int nPrev = -1;
		for ( std::set<int>::iterator it=indexSet.begin(); it!=indexSet.end(); ++it )
		{
			if ( *it != nPrev + 1 )				//���� ���� ������ �������
			{
				for ( int i=nPrev+1; i!=*it; i++ )
					freeSpanIndexes[nDamageIndex].push_back( i );
			}
			nPrev = *it;
		}
		freeSpanIndexes[nDamageIndex].push_back( nPrev + 1 );			//��� ����� ��������� ������
	}
	
	ASSERT( pDT->IsReading() );
	SBridgeRPGStats rpgStats;
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	
	GetRPGStats( rpgStats, pRootItem );
}

void CBridgeFrame::RemoveBridgeIndex( int nActiveDamage, int nIndex )
{
	NI_ASSERT( nIndex != -1 );
	for ( std::list<int>::iterator it=freeSpanIndexes[nActiveDamage].begin(); it!=freeSpanIndexes[nActiveDamage].end(); ++it )
	{
		if ( nIndex < *it )
		{
			freeSpanIndexes[nActiveDamage].insert( it, nIndex );
			return;
		}
	}
}

int CBridgeFrame::GetFreeBridgeIndex( int nActiveDamage )
{
	int nRes = -1;
	if ( freeSpanIndexes[nActiveDamage].size() == 1 )
	{
		nRes = freeSpanIndexes[nActiveDamage].back()++;
	}
	else
	{
		nRes = freeSpanIndexes[nActiveDamage].front();
		freeSpanIndexes[nActiveDamage].pop_front();
	}
	
	return nRes;
}

bool CBridgeFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_BRIDGE_ROOT_ITEM );

	IScene *pSG = GetSingleton<IScene>();
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();

	SBridgeRPGStats rpgStats;
	CBridgeCommonPropsItem *pCommonProps = static_cast<CBridgeCommonPropsItem *>( pRootItem->GetChildItem( E_BRIDGE_COMMON_PROPS_ITEM ) );
	if ( pCommonProps->GetDirection() )
		rpgStats.direction = SBridgeRPGStats::HORIZONTAL;
	else
		rpgStats.direction = SBridgeRPGStats::VERTICAL;
	rpgStats.fMaxHP = pCommonProps->GetHealth();
	rpgStats.fRepairCost = pCommonProps->GetRepairCost();

	if ( pCommonProps->GetPassForInfantry() )
		rpgStats.dwAIClasses |= AI_CLASS_HUMAN;
	if ( pCommonProps->GetPassForWheels() )
		rpgStats.dwAIClasses |= AI_CLASS_WHEEL;
	if ( pCommonProps->GetPassForHalfTracks() )
		rpgStats.dwAIClasses |= AI_CLASS_HALFTRACK;
	if ( pCommonProps->GetPassForTracks() )
		rpgStats.dwAIClasses |= AI_CLASS_TRACK;
	
	string szDir = GetDirectory( pszResultFileName );
	
	/*
	CTreeItem *pMainDamagePropsItem = pRootItem->GetChildItem( E_BRIDGE_STAGE_PROPS_ITEM, 0 );
	*/
	
	for ( int nActiveDamage=0; nActiveDamage<3; nActiveDamage++ )
	{
		CTreeItem *pDamagePropsItem = pRootItem->GetChildItem( E_BRIDGE_STAGE_PROPS_ITEM, nActiveDamage );
		int nPackSegmentIndex = 0;
		CSpritesPackBuilder::CPackParameters packs;
		CSpritesPackBuilder::CPackParameters shadowPacks;
		std::string szShortName = NStr::Format( "%d", nActiveDamage+1 );
		
		for ( int i=0; i<3; i++ )
		{
			CTreeItem *pBridgeParts = 0;
			CVec3 vPapa;
			CVec3 vTemp;
			if ( i == 0 )
			{
				vPapa = vBeginPos;
				pBridgeParts = pDamagePropsItem->GetChildItem( E_BRIDGE_BEGIN_SPANS_ITEM );
			}
			else if ( i == 1 )
			{
				vPapa = vCenterKrest;
				pBridgeParts = pDamagePropsItem->GetChildItem( E_BRIDGE_CENTER_SPANS_ITEM );
			}
			else if ( i == 2 )
			{
				vPapa = vEndPos;
				pBridgeParts = pDamagePropsItem->GetChildItem( E_BRIDGE_END_SPANS_ITEM );
			}

			{
				CVec2 v2;
				pSG->GetPos2( &v2, vPapa );
				v2.x += zeroShiftX;
				v2.y += zeroShiftY;
				pSG->GetPos3( &vPapa, v2 );
			}

			string szSpriteName, szShadowName, szResultName;
			string szProjectDir = GetDirectory( pszProjectName );
			CVec2 zeroPos2;
			int nPos = 0;
			int nFrontIndex = -1, nBackIndex = -1;

			int nActiveBridgePart = -1;
			for ( CTreeItem::CTreeItemList::const_iterator ext=pBridgeParts->GetBegin(); ext!=pBridgeParts->GetEnd(); ++ext )
			{
				nActiveBridgePart++;
				CBridgePartsItem *pBridgeSpansItem = static_cast<CBridgePartsItem *> ( ext->GetPtr() );
				SetActivePartsItem( pBridgeSpansItem, pszProjectName );
				
				CBridgePartPropsItem *pBack = static_cast<CBridgePartPropsItem *> ( pBridgeSpansItem->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 0 ) );
				CBridgePartPropsItem *pFront = static_cast<CBridgePartPropsItem *> ( pBridgeSpansItem->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 1 ) );
				CBridgePartPropsItem *pBottom = static_cast<CBridgePartPropsItem *> ( pBridgeSpansItem->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 2 ) );

				SBridgeRPGStats::SSegmentRPGStats segment;
				if ( pBack->pSprite )
				{
					segment.eType = SBridgeRPGStats::SSegmentRPGStats::GIRDER;
					segment.szModel = szShortName;
					vTemp = vPapa;
					if ( m_bHorizontal )
					{
						segment.vRelPos = CVec3( 0, m_fBack, 0 );
						vTemp.y += m_fBack;
					}
					else
					{
						segment.vRelPos = CVec3( m_fBack, 0, 0 );
						vTemp.x += m_fBack;
					}
					
					AddSpriteAndShadow( pszProjectName, &packs, &shadowPacks, pBack, vTemp );
					segment.nFrameIndex = nPackSegmentIndex++;
					nBackIndex = rpgStats.segments.size();
					rpgStats.segments.push_back( segment );
				}
				
				if ( pFront->pSprite )
				{
					segment.eType = SBridgeRPGStats::SSegmentRPGStats::GIRDER;
					segment.szModel = szShortName;
					vTemp = vPapa;
					if ( m_bHorizontal )
					{
						segment.vRelPos = CVec3( 0, m_fFront, 0 );
						vTemp.y += m_fFront;
					}
					else
					{
						segment.vRelPos = CVec3( m_fFront, 0, 0 );
						vTemp.x += m_fFront;
					}
					
					AddSpriteAndShadow( pszProjectName, &packs, &shadowPacks, pFront, vTemp );
					segment.nFrameIndex = nPackSegmentIndex++;
					nFrontIndex = rpgStats.segments.size();
					rpgStats.segments.push_back( segment );
				}
				
				if ( !pBottom->pSprite )
				{
					AfxMessageBox( "Error: There is no bottom part for this bridge, can not export bridge!" );
					return false;
				}
				
				segment.eType = SBridgeRPGStats::SSegmentRPGStats::SLAB;
				segment.szModel = szShortName;
				segment.vRelPos = VNULL3;
				AddSpriteAndShadow( pszProjectName, &packs, &shadowPacks, pBottom, vPapa );
				segment.nFrameIndex = nPackSegmentIndex++;
				
				int nUMinX = 0, nUMaxX = 0, nUMinY = 0, nUMaxY = 0;		//unlocked min max ��� ���������� ����� � ������ �����
				if ( nActiveDamage == 0 && nActiveBridgePart == 0 )
				{
					SaveSegmentInformation( segment, pBridgeSpansItem, &packs, vPapa, nUMinX, nUMaxX, nUMinY, nUMaxY );
				}
				rpgStats.segments.push_back( segment );

				SBridgeRPGStats::SSpan span;
				span.nSlab = rpgStats.segments.size() - 1;
				span.nBackGirder = nBackIndex;
				span.nFrontGirder = nFrontIndex;

				span.fLength = span.fWidth = 0;
				if ( !pBridgeSpansItem->unLockedTiles.empty() )
				{
					if ( m_bHorizontal )
					{
						span.fLength = nUMaxX - nUMinX + 1;
						span.fWidth = nUMaxY - nUMinY + 1;
					}
					else
					{
						span.fLength = nUMaxY - nUMinY + 1;
						span.fWidth = nUMaxX - nUMinX + 1;
					}
				}

				if ( nActiveDamage == 0 && i == 1 && span.fLength == 0 )
				{
					AfxMessageBox( "Error: automatically computed length for line part of the bridge is 0\nYou need to fill unlocked tiles for line part before you export project" );
				}
				

				{
					int nSize = rpgStats.states[nActiveDamage].spans.size();
					if ( nSize < pBridgeSpansItem->nSpanIndex + 1 )
						rpgStats.states[nActiveDamage].spans.resize( pBridgeSpansItem->nSpanIndex + 1 );
					rpgStats.states[nActiveDamage].spans[ pBridgeSpansItem->nSpanIndex ] = span;
				}

				
				if ( i == 0 )
					rpgStats.states[nActiveDamage].begins.push_back( pBridgeSpansItem->nSpanIndex );
				else if ( i == 1 )
					rpgStats.states[nActiveDamage].lines.push_back( pBridgeSpansItem->nSpanIndex );
				else if ( i == 2 )
					rpgStats.states[nActiveDamage].ends.push_back( pBridgeSpansItem->nSpanIndex );
				else
					NI_ASSERT( 0 );
			}
		}

		if ( packs.size() > 0 )
		{
			std::string szSaveSpriteName = szDir;
			szSaveSpriteName += szShortName;
			if ( !BuildSpritesPack( packs, szSaveSpriteName ) )
				return false;			//error

			std::string szTGAName = szSaveSpriteName + ".tga";
			CPtr<IDataStream> pStream = OpenFileStream( szTGAName.c_str(), STREAM_ACCESS_READ );
			NI_ASSERT( pStream != 0 );
			CPtr<IImage> pImage = pIP->LoadImage( pStream );
			pStream = 0;

			SaveCompressedTexture( pImage, szSaveSpriteName.c_str() );
			remove( szTGAName.c_str() );
		}
		if ( shadowPacks.size() > 0 )
		{
			std::string szSaveSpriteName = szDir;
			szSaveSpriteName += szShortName;
			szSaveSpriteName += "s";
			if ( BuildSpritesPack( shadowPacks, szSaveSpriteName ) )
			{
				std::string szTGAName = szSaveSpriteName + ".tga";
				CPtr<IDataStream> pStream = OpenFileStream( szTGAName.c_str(), STREAM_ACCESS_READ );
				NI_ASSERT( pStream != 0 );
				CPtr<IImage> pImage = pIP->LoadImage( pStream );
				pStream = 0;
				
				SaveCompressedShadow( pImage, szSaveSpriteName.c_str() );
				remove( szTGAName.c_str() );
			}
		}
	}
	
	NI_ASSERT( rpgStats.states[0].begins.size() > 0 );
	NI_ASSERT( rpgStats.states[0].lines.size() > 0 );
	NI_ASSERT( rpgStats.states[0].ends.size() > 0 );
	
	{
		const SBridgeRPGStats::SSegmentRPGStats &beginSegment = rpgStats.segments[ rpgStats.states[0].spans[ rpgStats.states[0].begins[0] ].nSlab ];
		const SBridgeRPGStats::SSegmentRPGStats &lineSegment = rpgStats.segments[ rpgStats.states[0].spans[ rpgStats.states[0].lines[0] ].nSlab ];
		const SBridgeRPGStats::SSegmentRPGStats &endSegment = rpgStats.segments[ rpgStats.states[0].spans[ rpgStats.states[0].ends[0] ].nSlab ];
		float fBeginLength = rpgStats.states[0].spans[ rpgStats.states[0].begins[0] ].fLength;
		float fBeginWidth = rpgStats.states[0].spans[ rpgStats.states[0].begins[0] ].fWidth;
		float fLineLength = rpgStats.states[0].spans[ rpgStats.states[0].lines[0] ].fLength;
		float fLineWidth = rpgStats.states[0].spans[ rpgStats.states[0].lines[0] ].fWidth;
		float fEndLength = rpgStats.states[0].spans[ rpgStats.states[0].ends[0] ].fLength;
		float fEndWidth = rpgStats.states[0].spans[ rpgStats.states[0].ends[0] ].fWidth;
		
		for ( int nActiveDamage=0; nActiveDamage<3; nActiveDamage++ )
		{
			for ( int i=0; i<rpgStats.states[nActiveDamage].begins.size(); i++ )
			{
				SBridgeRPGStats::SSpan &span = rpgStats.states[nActiveDamage].spans[ rpgStats.states[nActiveDamage].begins[i] ];
				span.fLength = fBeginLength;
				span.fWidth = fBeginWidth;

				for ( int z=0; z<3; z++ )
				{
					if ( nActiveDamage == 0 && z == 0 )
						continue;		//slab � ������������� ��������� ��� ��������

					int nSegmentIndex = 0;
					switch ( z )
					{
					case 0:
						nSegmentIndex = span.nSlab;
						break;
					case 1:
						nSegmentIndex = span.nBackGirder;
						break;
					case 2:
						nSegmentIndex = span.nFrontGirder;
						break;
					default:
						NI_ASSERT( 0 );
					}

					SBridgeRPGStats::SSegmentRPGStats &segment = rpgStats.segments[ nSegmentIndex ];
					segment.passability = beginSegment.passability;
					segment.vOrigin = beginSegment.vOrigin;
					segment.visibility = beginSegment.visibility;
					segment.vVisOrigin = beginSegment.vVisOrigin;
				}
			}

			for ( int i=0; i<rpgStats.states[nActiveDamage].lines.size(); i++ )
			{
				SBridgeRPGStats::SSpan &span = rpgStats.states[nActiveDamage].spans[ rpgStats.states[nActiveDamage].lines[i] ];
				span.fLength = fLineLength;
				span.fWidth = fLineWidth;

				for ( int z=0; z<3; z++ )
				{
					if ( nActiveDamage == 0 && z == 0 )
						continue;		//slab � ������������� ��������� ��� ��������
					
					int nSegmentIndex = 0;
					switch ( z )
					{
					case 0:
						nSegmentIndex = span.nSlab;
						break;
					case 1:
						nSegmentIndex = span.nBackGirder;
						break;
					case 2:
						nSegmentIndex = span.nFrontGirder;
						break;
					default:
						NI_ASSERT( 0 );
					}
					
					SBridgeRPGStats::SSegmentRPGStats &segment = rpgStats.segments[ nSegmentIndex ];
					segment.passability = lineSegment.passability;
					segment.vOrigin = lineSegment.vOrigin;
					segment.visibility = lineSegment.visibility;
					segment.vVisOrigin = lineSegment.vVisOrigin;
				}
			}

			for ( int i=0; i<rpgStats.states[nActiveDamage].ends.size(); i++ )
			{
				SBridgeRPGStats::SSpan &span = rpgStats.states[nActiveDamage].spans[ rpgStats.states[nActiveDamage].ends[i] ];
				span.fLength = fEndLength;
				span.fWidth = fEndWidth;

				for ( int z=0; z<3; z++ )
				{
					if ( nActiveDamage == 0 && z == 0 )
						continue;		//slab � ������������� ��������� ��� ��������
					
					int nSegmentIndex = 0;
					switch ( z )
					{
					case 0:
						nSegmentIndex = span.nSlab;
						break;
					case 1:
						nSegmentIndex = span.nBackGirder;
						break;
					case 2:
						nSegmentIndex = span.nFrontGirder;
						break;
					default:
						NI_ASSERT( 0 );
					}
					
					SBridgeRPGStats::SSegmentRPGStats &segment = rpgStats.segments[ nSegmentIndex ];
					segment.passability = endSegment.passability;
					segment.vOrigin = endSegment.vOrigin;
					segment.visibility = endSegment.visibility;
					segment.vVisOrigin = endSegment.vVisOrigin;
				}
			}
		}
	}
	
	
	FillRPGStats( rpgStats, pRootItem, pszProjectName );
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	
	bool bWarning = false;
	for ( int i=0; i<3; i++ )
	{
		if ( freeSpanIndexes[i].size() > 1 )
		{
			bWarning = true;
			break;
		}
	}
	if ( bWarning )
		AfxMessageBox( "Note, that this bridge will be changed on map because of deleted some spans!!!\nYou will need to replace all that bridges by new one\n" );
	
	CTreeItem *pDamagePropsItem = pRootItem->GetChildItem( E_BRIDGE_STAGE_PROPS_ITEM );
	CTreeItem *pBeginSpansItem = pDamagePropsItem->GetChildItem( E_BRIDGE_BEGIN_SPANS_ITEM );
	CTreeItem *pPartsItem = pBeginSpansItem->GetChildItem( E_BRIDGE_PARTS_ITEM );
	CBridgePartPropsItem *pBridgePartPropsItem = static_cast<CBridgePartPropsItem *> ( pPartsItem->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 2 ) );
	
	std::string szFullName;
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pBridgePartPropsItem->GetSpriteName(), szFullName );
	std::string szResName = GetDirectory( pszResultFileName );
	szResName += "icon.tga";
	SaveIconFile( szFullName.c_str(), szResName.c_str() );
	
	return true;
}

void CBridgeFrame::FillRPGStats( SBridgeRPGStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName )
{
	CTreeItem *pDefencesItem = pRootItem->GetChildItem( E_BRIDGE_DEFENCES_ITEM );
	for ( int i=0; i<6; i++ )
	{
		CBridgeDefencePropsItem *pDefProps = static_cast<CBridgeDefencePropsItem *> ( pDefencesItem->GetChildItem( E_BRIDGE_DEFENCE_PROPS_ITEM, i ) );
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
		
		rpgStats.defences[ nIndex ].nArmorMin = pDefProps->GetMinArmor();
		rpgStats.defences[ nIndex ].nArmorMax = pDefProps->GetMaxArmor();
		rpgStats.defences[ nIndex ].fSilhouette = pDefProps->GetSilhouette();
	}
	CTreeItem *pDamagePropsItem = pRootItem->GetChildItem( E_BRIDGE_STAGE_PROPS_ITEM );
	CTreeItem *pCenterSpansItem = pDamagePropsItem->GetChildItem( E_BRIDGE_CENTER_SPANS_ITEM );
	CTreeItem *pPartsItem = pCenterSpansItem->GetChildItem( E_BRIDGE_PARTS_ITEM );
	CBridgePartPropsItem *pSlabItem = static_cast<CBridgePartPropsItem *>( pPartsItem->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 2 ) );
	
	std::string szFullName;
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pSlabItem->GetSpriteName(), szFullName );
	
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	IScene *pSG = GetSingleton<IScene>();
	CPtr<IDataStream> pImageStream = OpenFileStream( szFullName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT( pImageStream != 0 );
	if ( !pImageStream )
		return;
	CPtr<IImage> pBridgeImage = pIP->LoadImage( pImageStream );
	NI_ASSERT( pBridgeImage != 0 );
	if ( !pBridgeImage )
		return;
	CImageAccessor imageAccessor( pBridgeImage );
	
	CVec2 vRealZeroPos2;
	pSG->GetPos2( &vRealZeroPos2, vCenterKrest );
	vRealZeroPos2.x += zeroShiftX;
	vRealZeroPos2.y += zeroShiftY;

	CVec3 vRealZeroPos3;
	pSG->GetPos3( &vRealZeroPos3, vRealZeroPos2 );

	for ( CListOfFirePoints::iterator it=firePoints.begin(); it!=firePoints.end(); ++it )
	{
		CBridgeFirePointPropsItem *pFireProps = it->pFirePoint;
		SBridgeRPGStats::SFirePoint fire;
		
		if ( !it->pHLine )
			continue;
		
		fire.vPos = it->pHLine->GetPosition();
		fire.vPos.x -= vRealZeroPos3.x;
		fire.vPos.y -= vRealZeroPos3.y;
		fire.vPos.z = 0;
		
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, it->pSprite->GetPosition() );
		vPos2.x -= vRealZeroPos2.x;
		vPos2.y -= vRealZeroPos2.y;
		fire.vPicturePosition = vPos2;
		
		fire.fDirection = pFireProps->GetDirection();
		fire.szFireEffect = pFireProps->GetEffectName();
		
		fire.vWorldPosition = Get3DPosition( vSpriteCommonPos, vPos2, vRealZeroPos2, imageAccessor, 1, pGFX ); 
		rpgStats.firePoints.push_back( fire );
	}
	
	CBridgeSmokesItem *pSmokesItem = (CBridgeSmokesItem *) pRootItem->GetChildItem( E_BRIDGE_SMOKES_ITEM );
	NI_ASSERT( pSmokesItem != 0 );
	
	rpgStats.szSmokeEffect = pSmokesItem->GetEffectName();
	for ( CTreeItem::CTreeItemList::const_iterator it=pSmokesItem->GetBegin(); it!=pSmokesItem->GetEnd(); ++it )
	{
		CBridgeSmokePropsItem *pProps = static_cast<CBridgeSmokePropsItem *> ( it->GetPtr() );
		SBridgeRPGStats::SFirePoint smokeProps;
		
		if ( !pProps->pHLine )
			continue;
		
		smokeProps.vPos = pProps->pHLine->GetPosition();
		smokeProps.vPos.x -= vRealZeroPos3.x;
		smokeProps.vPos.y -= vRealZeroPos3.y;
		smokeProps.vPos.z = 0;
		
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, pProps->pSprite->GetPosition() );
		vPos2.x -= vRealZeroPos2.x;
		vPos2.y -= vRealZeroPos2.y;
		smokeProps.vPicturePosition = vPos2;
		smokeProps.fDirection = pProps->GetDirection();
		
		smokeProps.vWorldPosition = Get3DPosition( vSpriteCommonPos, vPos2, vRealZeroPos2, imageAccessor, 1, pGFX ); 
		rpgStats.smokePoints.push_back( smokeProps );
	}
}

void CBridgeFrame::SavePointsInformation( SBridgeRPGStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName )
{
}

void CBridgeFrame::GetRPGStats( const SBridgeRPGStats &rpgStats, CTreeItem *pRootItem )
{
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();

	CVec2 vRealZeroPos2;
	pSG->GetPos2( &vRealZeroPos2, vCenterKrest );
	vRealZeroPos2.x += zeroShiftX;
	vRealZeroPos2.y += zeroShiftY;

	CVec3 vRealZeroPos3;
	pSG->GetPos3( &vRealZeroPos3, vRealZeroPos2 );

	int nCurrentFire = 0;
	CTreeItem *pFiresItem = pRootItem->GetChildItem( E_BRIDGE_FIRE_POINTS_ITEM );
	NI_ASSERT( pFiresItem != 0 );
	NI_ASSERT( rpgStats.firePoints.size() == pFiresItem->GetChildsCount() );
	firePoints.clear();
	for ( CTreeItem::CTreeItemList::const_iterator it=pFiresItem->GetBegin(); it!=pFiresItem->GetEnd(); ++it )
	{
		const SBridgeRPGStats::SFirePoint &fire = rpgStats.firePoints[ nCurrentFire ];
		CBridgeFirePointPropsItem *pFirePointProps = static_cast<CBridgeFirePointPropsItem *> ( it->GetPtr() );
		
		SFirePoint firePoint;
		firePoint.pFirePoint = pFirePointProps;
		
		CPtr<IObjVisObj> pObject;
		{
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
			CVec2 vPos2 = fire.vPicturePosition;
			vPos2.x += vRealZeroPos2.x;
			vPos2.y += vRealZeroPos2.y;
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
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
			NI_ASSERT( pObject != 0 );
			CVec3 vPos3 = fire.vPos;
			vPos3.x += vRealZeroPos3.x;
			vPos3.y += vRealZeroPos3.y;
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
		
		firePoints.push_back( firePoint );
		nCurrentFire++;
	}

	
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
	pTimer->Update( timeGetTime() );
	
	int nCurrentSmoke = 0;
	CBridgeSmokesItem *pSmokesItem = (CBridgeSmokesItem *) pRootItem->GetChildItem( E_BRIDGE_SMOKES_ITEM );
	NI_ASSERT( pSmokesItem != 0 );
	NI_ASSERT( rpgStats.smokePoints.size() == pSmokesItem->GetChildsCount() );
	pSmokesItem->SetEffectName( rpgStats.szSmokeEffect.c_str() );
	for ( CTreeItem::CTreeItemList::const_iterator it=pSmokesItem->GetBegin(); it!=pSmokesItem->GetEnd(); ++it )
	{
		CBridgeSmokePropsItem *pProps = static_cast<CBridgeSmokePropsItem *> ( it->GetPtr() );
		const SBridgeRPGStats::SFirePoint &smoke = rpgStats.smokePoints[ nCurrentSmoke ];
		
		CPtr<IObjVisObj> pObject;
		{
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
			CVec2 vPos2 = smoke.vPicturePosition;
			vPos2.x += vRealZeroPos2.x;
			vPos2.y += vRealZeroPos2.y;
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
			pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
			NI_ASSERT( pObject != 0 );
			CVec3 vPos3 = smoke.vPos;
			vPos3.x += vRealZeroPos3.x;
			vPos3.y += vRealZeroPos3.y;
			vPos3.z = 0;
			
			pObject->SetPosition( vPos3 );
			
			pObject->SetDirection( 0 );
			pObject->SetAnimation( 0 );
			pSG->AddObject( pObject, SGVOGT_OBJECT );
			pObject->SetOpacity( 0 );
			pProps->pHLine = pObject;
		}
		
		pProps->SetDirection( smoke.fDirection );
		nCurrentSmoke++;
	}
}

void CBridgeFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	ASSERT( !pDT->IsReading() );
	
	SBridgeRPGStats rpgStats;
	if ( !bNewProjectJustCreated )
		FillRPGStats( rpgStats, pRootItem, pszProjectName );
	else
		GetRPGStats( rpgStats, pRootItem );
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CBridgeFrame::AddSpriteAndShadow( const char *pszProjectFileName, CSpritesPackBuilder::CPackParameters *pPacks, CSpritesPackBuilder::CPackParameters *pShadowPacks, CBridgePartPropsItem *pProps, const CVec3 &vTemp )
{
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	CSpritesPackBuilder::SPackParameter pack;
	CVec2 vZeroPos2 = ::ComputeSpriteNewZeroPos( pProps->pSprite, vTemp, VNULL2 );
	pack.center = CTPoint<int>( vZeroPos2.x, vZeroPos2.y );
	pack.lockedTiles.Clear();
	pack.lockedTilesCenter.x = 0;
	pack.lockedTilesCenter.y = 0;
	
	std::string szResultName;
	MakeFullPath( GetDirectory(pszProjectFileName).c_str(), pProps->GetSpriteName(), szResultName );
	CPtr<IDataStream> pStream = OpenFileStream( szResultName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
	{
		std::string szErr = "Error: can not open stream ";
		szErr += szResultName;
		AfxMessageBox( szErr.c_str() );
		return;
	}
	pack.pImage = pIP->LoadImage( pStream );
	if ( pack.pImage == 0 )
	{
		std::string szErr = "Error: can not open image ";
		szErr += szResultName;
		AfxMessageBox( szErr.c_str() );
		return;
	}
	pPacks->push_back( pack );


	std::string szShadowName = szResultName.substr( 0, szResultName.rfind('.') );
	szShadowName += "s.tga";
	pStream = OpenFileStream( szShadowName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
	{
		std::string szErr = "Error: can not open stream ";
		szErr += szShadowName;
		AfxMessageBox( szErr.c_str() );
		return;
	}
	CPtr<IImage> pShadowImage = pIP->LoadImage( pStream );
	if ( pShadowImage == 0 )
	{
		std::string szErr = "Error: can not open image ";
		szErr += szShadowName;
		AfxMessageBox( szErr.c_str() );
		return;
	}

	CPtr<IImage> pInverseSprite = pack.pImage->Duplicate();
	pInverseSprite->SharpenAlpha( 128 );
	pInverseSprite->InvertAlpha();
	if ( pInverseSprite->GetSizeX() != pShadowImage->GetSizeX() || pInverseSprite->GetSizeY() != pShadowImage->GetSizeY() )
	{
		string szErr = "The size of sprite does not equal the size of shadow: ";
		szErr += szResultName;
		szErr += ",  ";
		szErr += szShadowName;
		
		NI_ASSERT_T( 0, szErr.c_str() );
		return;
	}
	RECT rc;
	rc.left = 0;
	rc.top = 0;
	rc.right = pInverseSprite->GetSizeX();
	rc.bottom = pInverseSprite->GetSizeY();
	pShadowImage->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );
	pShadowImage->SetColor( DWORD(0) );
	pack.pImage = pShadowImage;
	pShadowPacks->push_back( pack );
}

void CBridgeFrame::SetActivePartsItem( CBridgePartsItem *pItem, const char *pszProjectFileName )
{
	if ( m_drawMode == E_DRAW_SPANS && pActiveSpansItem == pItem )
		return;
	m_drawMode = E_DRAW_SPANS;

	IScene *pSG = GetSingleton<IScene>();
	if ( pActiveSpansItem != 0 )
	{
		for ( CTreeItem::CTreeItemList::const_iterator it=pActiveSpansItem->GetBegin(); it!=pActiveSpansItem->GetEnd(); ++it )
		{
			CBridgePartPropsItem *pSpanProps = static_cast<CBridgePartPropsItem *> ( it->GetPtr() );
			pSG->RemoveObject( pSpanProps->pSprite );
		}
	}
		
	if ( pActiveSpansItem != pItem )
	{
		pActiveSpansItem = pItem;
		for ( CTreeItem::CTreeItemList::const_iterator it=pActiveSpansItem->GetBegin(); it!=pActiveSpansItem->GetEnd(); ++it )
		{
			CBridgePartPropsItem *pSpanProps = static_cast<CBridgePartPropsItem *> ( it->GetPtr() );
			pSpanProps->bLoaded = false;
		}
	}
	
	int i = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pActiveSpansItem->GetBegin(); it!=pActiveSpansItem->GetEnd(); ++it )
	{
		i++;
		CBridgePartPropsItem *pSpanProps = static_cast<CBridgePartPropsItem *> ( it->GetPtr() );
		if ( !pSpanProps->bLoaded )
		{
			pSpanProps->bLoaded = true;
			LoadSpriteItem( pSpanProps, NStr::Format( "%d", i ), pszProjectFileName );
		}
		else
		{
			if ( i != 3 )
				pSG->AddObject( pSpanProps->pSprite, SGVOGT_OBJECT );
			else
				pSG->AddObject( pSpanProps->pSprite, SGVOGT_BRIDGE );
		}
	}

	bEditSpansEnabled = false;
	if ( pTreeDockBar && pTreeDockBar->GetTreeWithIndex( 0 ) )
	{
		if ( pActiveSpansItem->GetActiveStage() == 0 )
		{
			CTreeItem *pPapa = pItem->GetParentTreeItem();
			NI_ASSERT( pPapa != 0 );
			int i = 0;
			NI_ASSERT( pPapa->GetChildsCount() >= 1 );
			if ( pPapa->GetBegin()->GetPtr() == pActiveSpansItem )
				bEditSpansEnabled = true;
		}
	}

	GFXDraw();
}

void CBridgeFrame::LoadSpriteItem( CBridgePartPropsItem *pItem, const char *pszName, const char *pszProjectFileName )
{
	string szTemp = theApp.GetEditorTempDir();
	string szSpriteName = pItem->GetSpriteName();
	if ( szSpriteName.size() == 0 )
		return;

	if ( IsRelatedPath( szSpriteName.c_str() ) )
	{
		string szRes;
		if ( MakeFullPath( GetDirectory(pszProjectFileName).c_str(), szSpriteName.c_str(), szRes ) )
			szSpriteName = szRes;
		else
			szSpriteName = pItem->GetSpriteName();
	}
	
	if ( !ComposeSingleSprite( szSpriteName.c_str(), szTemp.c_str(), pszName, true ) )
		return;
	
	szTemp = theApp.GetEditorTempResourceDir();
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	if ( pItem->pSprite )
	{
		pSG->RemoveObject( pItem->pSprite );
		pItem->pSprite = 0;
	}
	
	szTemp += "\\";
	szTemp += pszName;
	pItem->pSprite = static_cast<IObjVisObj *> ( pVOB->BuildObject( szTemp.c_str(), 0, SGVOT_SPRITE ) );
	NI_ASSERT( pItem->pSprite != 0 );
	CVec3 v3 = CVec3(14*fWorldCellSize, 16*fWorldCellSize, 0);
	
	pItem->pSprite->SetPosition( v3 );
	pItem->pSprite->SetDirection( 0 );
	pItem->pSprite->SetAnimation( 0 );
	pItem->pSprite->SetOpacity( 150 );
	if ( *pszName != '3' )
		pSG->AddObject( pItem->pSprite, SGVOGT_OBJECT );
	else
	{
		pSG->AddObject( pItem->pSprite, SGVOGT_BRIDGE );
/*
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, pItem->vKrestPos );
		vPos2.x += zeroShiftX;
		vPos2.y = GetPointOnLine( vPos2.x );
		vPos2.x -= zeroShiftX;
		vPos2.y -= zeroShiftY;
		pSG->GetPos3( &pItem->vKrestPos, vPos2 );
*/
	}
}

void CBridgeFrame::SetActivePartPropsItem( CBridgePartPropsItem *pItem )
{
	if ( m_drawMode == E_DRAW_SPAN_PROPS && pActiveSpanPropsItem == pItem )
		return;
	
	CTreeItem *pPapa = pItem->GetParentTreeItem();
	if ( pActiveSpansItem != pPapa )
	{
		pActiveSpansItem = static_cast<CBridgePartsItem *> ( pPapa );
		for ( CTreeItem::CTreeItemList::const_iterator it=pActiveSpansItem->GetBegin(); it!=pActiveSpansItem->GetEnd(); ++it )
		{
			CBridgePartPropsItem *pSpanProps = static_cast<CBridgePartPropsItem *> ( it->GetPtr() );
			pSpanProps->bLoaded = false;
		}
	}

	m_drawMode = E_DRAW_SPAN_PROPS;
	IScene *pSG = GetSingleton<IScene>();
	pActiveSpanPropsItem = pItem;
	if ( !pActiveSpanPropsItem->bLoaded )
	{
		int i = 1;
		for ( CTreeItem::CTreeItemList::const_iterator it=pPapa->GetBegin(); it!=pPapa->GetEnd(); ++it )
		{
			if ( it->GetPtr() == pItem )
				break;
			i++;
		}
		NI_ASSERT( i <= 3 );
		
		pActiveSpanPropsItem->bLoaded = true;
		LoadSpriteItem( pActiveSpanPropsItem, NStr::Format( "%d", i ), szProjectFileName.c_str() );
	}
	else
		pSG->AddObject( pItem->pSprite, SGVOGT_UNIT );
	GFXDraw();
}

void CBridgeFrame::UpdatePartPropsItem()
{
	if ( !pActiveSpanPropsItem )
		return;
	
	int i = 1;
	CTreeItem *pPapa = pActiveSpanPropsItem->GetParentTreeItem();
	for ( CTreeItem::CTreeItemList::const_iterator it=pPapa->GetBegin(); it!=pPapa->GetEnd(); ++it )
	{
		if ( it->GetPtr() == pActiveSpanPropsItem )
			break;
		i++;
	}
	NI_ASSERT( i <= 3 );
	
	LoadSpriteItem( pActiveSpanPropsItem, NStr::Format( "%d", i ), szProjectFileName.c_str() );
	GFXDraw();
}

void CBridgeFrame::OnDrawGrid() 
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_GRID;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+10);
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_PASS );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	GFXDraw();
}

void CBridgeFrame::OnDrawPass() 
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_PASS;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+10);
	nIndex = pToolBar->CommandToIndex( ID_DRAW_PASS );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	GFXDraw();
}

void CBridgeFrame::OnChangeTranseparence()
{
	NI_ASSERT( m_pTransparenceCombo != 0 );
 	m_transValue = m_pTransparenceCombo->GetCurSel();
}

void CBridgeFrame::OnSetFocusTranseparence()
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_TRANSEPARENCE;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+10);
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_PASS );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	m_pTransparenceCombo->SetFocus();
	
	GFXDraw();
}

void CBridgeFrame::OnSetZeroButton() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_ZERO;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+10);
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_PASS );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	GFXDraw();
}

void CBridgeFrame::OnUpdateDrawTransparence(CCmdUI* pCmdUI) 
{
	if ( m_drawMode == E_DRAW_SPANS )
		pCmdUI->Enable( bEditSpansEnabled );
	else
		pCmdUI->Enable( false );
}

void CBridgeFrame::OnUpdateDrawPass(CCmdUI* pCmdUI) 
{
	if ( m_drawMode == E_DRAW_SPANS )
		pCmdUI->Enable( bEditSpansEnabled );
	else
		pCmdUI->Enable( false );
}

void CBridgeFrame::OnUpdateDrawGrid(CCmdUI* pCmdUI) 
{
	if ( m_drawMode == E_DRAW_SPANS )
		pCmdUI->Enable( bEditSpansEnabled );
	else
		pCmdUI->Enable(  false );
}

void CBridgeFrame::OnUpdateSetZeroButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		if ( m_drawMode == E_DRAW_SPAN_PROPS )
			pCmdUI->Enable( true );
		else
			pCmdUI->Enable( false );
	}
	else
		pCmdUI->Enable( false );
}

void CBridgeFrame::SetZeroCoordinate( POINT point )
{
	NI_ASSERT( pActiveSpanPropsItem != 0 );
	IScene *pSG = GetSingleton<IScene>();
	
	CVec2 pt;
	CTreeItem *pPapa = pActiveSpanPropsItem->GetParentTreeItem();
	CTreeItem *pTopPapa = pPapa->GetParentTreeItem();
	int i = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pPapa->GetBegin(); it!=pPapa->GetEnd(); ++it )
	{
		if ( it->GetPtr() == pActiveSpanPropsItem )
			break;
		i++;
	}

	if ( i == 2 )
	{
		if ( pTopPapa->GetItemType() == E_BRIDGE_CENTER_SPANS_ITEM )
		{
			return;
		}

		SetChangedFlag( true );
		float fx = point.x;
		float fy = GetPointOnLine( fx );
		pt.x = fx - zeroShiftX;
		pt.y = fy - zeroShiftY;
		if ( pTopPapa->GetItemType() == E_BRIDGE_BEGIN_SPANS_ITEM )
			pSG->GetPos3( &vBeginPos, pt );
		else
			pSG->GetPos3( &vEndPos, pt );
	}
	else
	{
		CVec3 vPapa;
		if ( pTopPapa->GetItemType() == E_BRIDGE_BEGIN_SPANS_ITEM )
		{
			vPapa = vBeginPos;
		}
		else if ( pTopPapa->GetItemType() == E_BRIDGE_END_SPANS_ITEM )
		{
			vPapa = vEndPos;
		}
		else
		{
			vPapa = vCenterKrest;
		}
		
		SetChangedFlag( true );
		pt.x = point.x - zeroShiftX;
		pt.y = point.y - zeroShiftY;
		CVec3 v3;
		pSG->GetPos3( &v3, pt );

		CBridgePartPropsItem *pSlab = static_cast<CBridgePartPropsItem *> ( pPapa->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 2 ) );
		if ( m_bHorizontal )
		{
			if ( i == 0 )
				m_fBack = v3.y - vPapa.y;
			else
				m_fFront = v3.y - vPapa.y;
		}
		else
		{
			if ( i == 0 )
				m_fBack = v3.x - vPapa.x;
			else
				m_fFront = v3.x - vPapa.x;
		}
	}

	GFXDraw();
}

void CBridgeFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//���� ������ �� ��� ������
		return;

	if ( tbStyle == E_DRAW_GRID && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->lockedTiles, ftX, ftY, 1, E_LOCKED_TILE );
		GFXDraw();
	}
	
	else if ( tbStyle == E_DRAW_PASS && m_drawMode == E_DRAW_SPANS && pActiveSpansItem )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->unLockedTiles, ftX, ftY, 1, E_UNLOCKED_TILE );
		GFXDraw();
	}
	
	else if ( tbStyle == E_SET_ZERO && m_drawMode == E_DRAW_SPAN_PROPS )
	{
		SetZeroCoordinate( point );
	}
	
	else if ( tbStyle == E_DRAW_TRANSEPARENCE && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->transeparences, ftX, ftY, m_transValue, E_TRANSEPARENCE_TILE );
		GFXDraw();
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

void CBridgeFrame::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )
	{
		CGridFrame::OnLButtonUp(nFlags, point);
		return;
	}
	
	ReleaseCapture();
	GFXDraw();
	m_mode = -1;
	
	CGridFrame::OnLButtonUp(nFlags, point);
}

void CBridgeFrame::OnRButtonDown(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//���� ������ �� ��� ������
		return;
	SetChangedFlag( true );
	
	if ( tbStyle == E_DRAW_GRID && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->lockedTiles, ftX, ftY, 0, E_LOCKED_TILE );
		GFXDraw();
	}
	
	if ( tbStyle == E_DRAW_PASS && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->unLockedTiles, ftX, ftY, 0, E_UNLOCKED_TILE );
		GFXDraw();
	}

	else if ( tbStyle == E_DRAW_TRANSEPARENCE && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->transeparences, ftX, ftY, 0, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}

	CGridFrame::OnRButtonDown(nFlags, point);
}

void CBridgeFrame::OnMouseMove(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//���� ������ �� ��� ������
		return;

	if ( tbStyle == E_DRAW_GRID && nFlags & MK_RBUTTON && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->lockedTiles, ftX, ftY, 0, E_LOCKED_TILE );
		GFXDraw();
	}
	else if ( tbStyle == E_DRAW_GRID && nFlags & MK_LBUTTON && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->lockedTiles, ftX, ftY, 1, E_LOCKED_TILE );
		GFXDraw();
	}
	
	if ( tbStyle == E_DRAW_PASS && nFlags & MK_RBUTTON && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->unLockedTiles, ftX, ftY, 0, E_UNLOCKED_TILE );
		GFXDraw();
	}
	else if ( tbStyle == E_DRAW_PASS && nFlags & MK_LBUTTON && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->unLockedTiles, ftX, ftY, 1, E_UNLOCKED_TILE );
		GFXDraw();
	}
	
	if ( tbStyle == E_DRAW_TRANSEPARENCE && nFlags & MK_RBUTTON && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->transeparences, ftX, ftY, 0, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}
	else if ( tbStyle == E_DRAW_TRANSEPARENCE && nFlags & MK_LBUTTON && m_drawMode == E_DRAW_SPANS && pActiveSpansItem != 0 )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( pActiveSpansItem->transeparences, ftX, ftY, m_transValue, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}
	
	else if ( tbStyle == E_SET_ZERO && m_drawMode == E_DRAW_SPAN_PROPS && nFlags & MK_LBUTTON )
	{
		SetZeroCoordinate( point );
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

void CBridgeFrame::SaveMyData( CBridgePartsItem *pSpansItem, CTreeAccessor saver )
{
	saver.Add( "LockedTiles", &pSpansItem->lockedTiles );
	saver.Add( "Transparences", &pSpansItem->transeparences );
	saver.Add( "UnLockedTiles", &pSpansItem->unLockedTiles );
}

void CBridgeFrame::LoadMyData( CBridgePartsItem *pSpansItem, CTreeAccessor saver )
{
	saver.Add( "LockedTiles", &pSpansItem->lockedTiles );
	saver.Add( "Transparences", &pSpansItem->transeparences );
	saver.Add( "UnLockedTiles", &pSpansItem->unLockedTiles );
	
	for ( CListOfTiles::iterator it=pSpansItem->lockedTiles.begin(); it!=pSpansItem->lockedTiles.end(); ++it )
	{
		SetTileInListOfTiles( pSpansItem->lockedTiles, it->nTileX, it->nTileY, it->nVal, E_LOCKED_TILE );
	}
	
	for ( CListOfTiles::iterator it=pSpansItem->transeparences.begin(); it!=pSpansItem->transeparences.end(); ++it )
	{
		SetTileInListOfTiles( pSpansItem->transeparences, it->nTileX, it->nTileY, it->nVal, E_TRANSEPARENCE_TILE );
	}

	for ( CListOfTiles::iterator it=pSpansItem->unLockedTiles.begin(); it!=pSpansItem->unLockedTiles.end(); ++it )
	{
		SetTileInListOfTiles( pSpansItem->unLockedTiles, it->nTileX, it->nTileY, it->nVal, E_UNLOCKED_TILE );
	}
}

FILETIME CBridgeFrame::FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem )
{
	FILETIME maxTime;
	maxTime = GetFileChangeTime( pszProjectName );

/*
	FILETIME current;
	for ( int i=0; i<3; i++ )
	{
		CTreeItem *pBridgeParts = 0;
		if ( i == 0 )
			pBridgeParts = pRootItem->GetChildItem( E_BRIDGE_BEGIN_SPANS_ITEM );
		else if ( i == 1 )
			pBridgeParts = pRootItem->GetChildItem( E_BRIDGE_CENTER_SPANS_ITEM );
		else if ( i == 2 )
			pBridgeParts = pRootItem->GetChildItem( E_BRIDGE_END_SPANS_ITEM );

		string szSpriteName, szShadowName, szResultName;
		string szProjectDir = GetDirectory( pszProjectName );
		for ( CTreeItem::CTreeItemList::const_iterator ext=pBridgeParts->GetBegin(); ext!=pBridgeParts->GetEnd(); ++ext )
		{
			CBridgePartsItem *pBridgeSpansItem = static_cast<CBridgePartsItem *> ( ext->GetPtr() );
			CBridgePartPropsItem *pBack = static_cast<CBridgePartPropsItem *> ( pBridgeSpansItem->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 0 ) );
			CBridgePartPropsItem *pFront = static_cast<CBridgePartPropsItem *> ( pBridgeSpansItem->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 1 ) );
			CBridgePartPropsItem *pBottom = static_cast<CBridgePartPropsItem *> ( pBridgeSpansItem->GetChildItem( E_BRIDGE_PART_PROPS_ITEM, 2 ) );

			MakeFullPath( szProjectDir.c_str(), pBack->GetSpriteName(), szResultName );
			szSpriteName = szResultName;
			current = GetFileChangeTime( szResultName.c_str() );
			if ( current > maxTime )
				maxTime = current;

			szShadowName = szSpriteName.substr( 0, szSpriteName.rfind('.') );
			szShadowName += "s.tga";
			current = GetFileChangeTime( szShadowName.c_str() );
			if ( current > maxTime )
				maxTime = current;

			MakeFullPath( szProjectDir.c_str(), pFront->GetSpriteName(), szResultName );
			szSpriteName = szResultName;
			current = GetFileChangeTime( szResultName.c_str() );
			if ( current > maxTime )
				maxTime = current;

			szShadowName = szSpriteName.substr( 0, szSpriteName.rfind('.') );
			szShadowName += "s.tga";
			current = GetFileChangeTime( szShadowName.c_str() );
			if ( current > maxTime )
				maxTime = current;

			MakeFullPath( szProjectDir.c_str(), pBottom->GetSpriteName(), szResultName );
			szSpriteName = szResultName;
			current = GetFileChangeTime( szResultName.c_str() );
			if ( current > maxTime )
				maxTime = current;
			
			szShadowName = szSpriteName.substr( 0, szSpriteName.rfind('.') );
			szShadowName += "s.tga";
			current = GetFileChangeTime( szShadowName.c_str() );
			if ( current > maxTime )
				maxTime = current;
		}
	}
*/

	return maxTime;
}

FILETIME CBridgeFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
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
	
	szTempFileName = szDestDir;
	szTempFileName += "1.xml";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	return minTime;
}

void CBridgeFrame::SetActiveMode( EActiveMode mode )
{
	if ( eActiveMode == mode )
		return;

	if ( eActiveMode == E_FIRE_POINT )
	{
		for ( CListOfFirePoints::iterator it=firePoints.begin(); it!=firePoints.end(); ++it )
		{
			it->pSprite->SetOpacity( 0 );
			if ( it->pHLine )
				it->pHLine->SetOpacity( 0 );
		}
	}
	else if ( eActiveMode == E_SMOKE_POINT )
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRootItem = pTree->GetRootItem();
		CTreeItem *pSmokeItems = pRootItem->GetChildItem( E_BRIDGE_SMOKES_ITEM );
		NI_ASSERT( pSmokeItems != 0 );
		
		for ( CTreeItem::CTreeItemList::const_iterator it=pSmokeItems->GetBegin(); it!=pSmokeItems->GetEnd(); ++it )
		{
			CBridgeSmokePropsItem *pProps = static_cast<CBridgeSmokePropsItem *> ( it->GetPtr() );
			pProps->pSprite->SetOpacity( 0 );
			pProps->pHLine->SetOpacity( 0 );
		}
	}
	
	eActiveMode = mode;
	if ( eActiveMode == E_FIRE_POINT )
	{
		for ( CListOfFirePoints::iterator it=firePoints.begin(); it!=firePoints.end(); ++it )
		{
			it->pSprite->SetOpacity( MIN_OPACITY );
		}
		SetActiveFirePoint( pActiveFirePoint );
	}
	else if ( eActiveMode == E_SMOKE_POINT )
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRootItem = pTree->GetRootItem();
		CTreeItem *pSmokeItems = pRootItem->GetChildItem( E_BRIDGE_SMOKES_ITEM );
		NI_ASSERT( pSmokeItems != 0 );
		
		for ( CTreeItem::CTreeItemList::const_iterator it=pSmokeItems->GetBegin(); it!=pSmokeItems->GetEnd(); ++it )
		{
			CBridgeSmokePropsItem *pProps = static_cast<CBridgeSmokePropsItem *> ( it->GetPtr() );
			pProps->pSprite->SetOpacity( MIN_OPACITY );
		}
		SelectSmokePoint( pActiveSmokePoint );
	}
}

void CBridgeFrame::OnSetFirePoint() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_FIRE_POINT;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+10);
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_PASS );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
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

void CBridgeFrame::OnSetSmokePoint() 
{
	UINT nIndex = 0;
	tbStyle = E_SET_SMOKE_POINT;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+10);
	nIndex = pToolBar->CommandToIndex( ID_SET_SMOKE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_PASS );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_ZERO_BUTTON );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_FIRE_POINT );
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

void CBridgeFrame::OnMovePoint() 
{
	UINT nIndex = 0;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+10);
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	eActiveSubMode = E_SUB_MOVE;
	SetFocus();
}

void CBridgeFrame::OnSetHorizontalShoot() 
{
	UINT nIndex = 0;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+10);
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	eActiveSubMode = E_SUB_HOR;
	CVec3 vPos3;
	if ( eActiveMode == E_FIRE_POINT && pActiveFirePoint )
		vPos3 = pActiveFirePoint->pSprite->GetPosition();
	else if ( eActiveMode == E_SMOKE_POINT && pActiveSmokePoint )
		vPos3 = pActiveSmokePoint->pSprite->GetPosition();
	else
	{
		NI_ASSERT_T( 0, "Unknown Active Mode" );
		return;
	}
	ComputeMaxAndMinPositions( vPos3 );
}

void CBridgeFrame::OnSetShootAngle() 
{
	UINT nIndex = 0;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+10);
	nIndex = pToolBar->CommandToIndex( ID_SET_SHOOT_ANGLE);
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_POINT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_SET_HORIZONTAL_SHOOT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	eActiveSubMode = E_SUB_DIR;
	if ( eActiveMode == E_FIRE_POINT )
		ComputeFireDirectionLines();
	else if ( eActiveMode == E_SMOKE_POINT )
		ComputeSmokeLines();
	SetFocus();
}

void CBridgeFrame::OnGeneratePoints() 
{
	if ( eActiveMode == E_SMOKE_POINT )
		GenerateSmokePoints();
	else if ( eActiveMode == E_FIRE_POINT )
		GenerateFirePoints();
	SetFocus();
}

void CBridgeFrame::OnUpdateSetSmokePoint(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		if ( pActiveSpansItem && !pActiveSpansItem->lockedTiles.empty() )
			pCmdUI->Enable( true );
		else
			pCmdUI->Enable( false );
	}
	else
		pCmdUI->Enable( false );
}

void CBridgeFrame::OnUpdateMovePoint(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 && pActiveSpansItem && eActiveMode == E_FIRE_POINT || eActiveMode == E_SMOKE_POINT )
	{
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
/*
	if ( eActiveMode == E_FIRE_POINT )
	{
		pCmdUI->Enable( true );
		return;
	}
	
	pCmdUI->Enable( false );
	return;
*/
}

void CBridgeFrame::OnUpdateSetHorizontalShoot(CCmdUI* pCmdUI) 
{
	if ( !pActiveSpansItem )
	{
		pCmdUI->Enable( false );
		return;
	}
	
	if ( eActiveMode == E_FIRE_POINT )
	{
		if ( pActiveFirePoint != 0 )
		{
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
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	
	pCmdUI->Enable( false );
	return;
}

void CBridgeFrame::OnUpdateSetShootAngle(CCmdUI* pCmdUI) 
{
	if ( !pActiveSpansItem )
	{
		pCmdUI->Enable( false );
		return;
	}
	
	if ( eActiveMode == E_FIRE_POINT )
	{
		if ( pActiveFirePoint != 0 )
		{
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
			pCmdUI->Enable( true );
		}
		else
			pCmdUI->Enable( false );
		return;
	}
	
	pCmdUI->Enable( false );
	return;
}

void CBridgeFrame::OnUpdateSetFirePoint(CCmdUI* pCmdUI) 
{
	if ( !pActiveSpansItem )
	{
		pCmdUI->Enable( false );
		return;
	}
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 && pActiveSpansItem )
	{
		if ( pActiveSpansItem && !pActiveSpansItem->lockedTiles.empty() )
			pCmdUI->Enable( true );
		else
			pCmdUI->Enable( false );
	}
	else
		pCmdUI->Enable( false );
}

void CBridgeFrame::OnUpdateGeneratePoints(CCmdUI* pCmdUI) 
{
	if ( !pActiveSpansItem )
	{
		pCmdUI->Enable( false );
		return;
	}
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 && pActiveSpansItem )
	{
		if ( ( eActiveMode == E_SMOKE_POINT || eActiveMode == E_FIRE_POINT ) && !pActiveSpansItem->lockedTiles.empty() )
			pCmdUI->Enable( true );
		else
			pCmdUI->Enable( false );
	}
	else
		pCmdUI->Enable( false );
}

void CBridgeFrame::SaveSegmentInformation( SBridgeRPGStats::SSegmentRPGStats &segment, CBridgePartsItem *pBridgeSpansItem, CSpritesPackBuilder::CPackParameters *pPacks, const CVec3 &vPapa, int &nUMinX, int &nUMaxX, int &nUMinY, int &nUMaxY )
{
	IScene *pSG = GetSingleton<IScene>();

	if ( pActiveSpansItem->lockedTiles.empty() && pBridgeSpansItem->unLockedTiles.empty() )
	{
		segment.passability.SetSizes( 0, 0 );
		segment.vOrigin.x = 0;
		segment.vOrigin.y = 0;
	}
	else
	{
		int nTileMinX, nTileMaxX, nTileMinY, nTileMaxY;
		if ( !pActiveSpansItem->lockedTiles.empty() )
		{
			nTileMinX = pActiveSpansItem->lockedTiles.front().nTileX;
			nTileMaxX = pActiveSpansItem->lockedTiles.front().nTileX;
			nTileMinY = pActiveSpansItem->lockedTiles.front().nTileY;
			nTileMaxY = pActiveSpansItem->lockedTiles.front().nTileY;
		}
		else
		{
			nTileMinX = pBridgeSpansItem->unLockedTiles.front().nTileX;
			nTileMaxX = pBridgeSpansItem->unLockedTiles.front().nTileX;
			nTileMinY = pBridgeSpansItem->unLockedTiles.front().nTileY;
			nTileMaxY = pBridgeSpansItem->unLockedTiles.front().nTileY;
		}
		
		CListOfTiles::iterator it=pActiveSpansItem->lockedTiles.begin();
		for ( ; it!=pActiveSpansItem->lockedTiles.end(); ++it )
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
		
		nUMinX = nUMaxX = nUMinY = nUMaxY = 0;
		if ( !pBridgeSpansItem->unLockedTiles.empty() )
		{
			nUMinX = pBridgeSpansItem->unLockedTiles.front().nTileX;
			nUMaxX = pBridgeSpansItem->unLockedTiles.front().nTileX;
			nUMinY = pBridgeSpansItem->unLockedTiles.front().nTileY;
			nUMaxY = pBridgeSpansItem->unLockedTiles.front().nTileY;
		}
		it=pBridgeSpansItem->unLockedTiles.begin();
		for ( ; it!=pBridgeSpansItem->unLockedTiles.end(); ++it )
		{
			if ( nUMinX > it->nTileX )
				nUMinX = it->nTileX;
			else if ( nUMaxX < it->nTileX )
				nUMaxX = it->nTileX;
			
			if ( nUMinY > it->nTileY )
				nUMinY = it->nTileY;
			else if ( nUMaxY < it->nTileY )
				nUMaxY = it->nTileY;
		}

		if ( nUMinX < nTileMinX )
			nTileMinX = nUMinX;
		if ( nUMaxX > nTileMaxX )
			nTileMaxX = nUMaxX;
		if ( nUMinY < nTileMinY )
			nTileMinY = nUMinY;
		if ( nUMaxY > nTileMaxY )
			nTileMaxY = nUMaxY;
		
		segment.passability.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
		BYTE *pBuf = segment.passability.GetBuffer();
		for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
		{
			for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
			{
				for ( it=pActiveSpansItem->lockedTiles.begin(); it!=pActiveSpansItem->lockedTiles.end(); ++it )
				{
					if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
						break;
				}
				
				if ( it != pActiveSpansItem->lockedTiles.end() )
					pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = it->nVal;
				else
					pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = 0;

				for ( it=pBridgeSpansItem->unLockedTiles.begin(); it!=pBridgeSpansItem->unLockedTiles.end(); ++it )
				{
					if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
						break;
				}
				
				if ( it != pBridgeSpansItem->unLockedTiles.end() )
				{
					pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = ( it->nVal << 4 );
				}
			}
		}
		
		float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
		CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
		CVec2 mostLeft2(fx2, fy2);
		CVec3 mostLeft3;
		pSG->GetPos3( &mostLeft3, mostLeft2 );

		segment.vOrigin.x = vPapa.x - mostLeft3.x;// + zeroShiftX;
		segment.vOrigin.y = vPapa.y - mostLeft3.y;// + zeroShiftY;
	}

	pPacks->back().lockedTiles = segment.passability;
	pPacks->back().lockedTilesCenter.x = segment.vOrigin.x;
	pPacks->back().lockedTilesCenter.y = segment.vOrigin.y;
	
	{
		if ( pBridgeSpansItem->transeparences.empty() )
		{
			segment.visibility.SetSizes( 0, 0 );
			segment.vVisOrigin.x = 0;
			segment.vVisOrigin.y = 0;
		}
		else
		{
			int nTileMinX = pBridgeSpansItem->transeparences.front().nTileX, nTileMaxX = pBridgeSpansItem->transeparences.front().nTileX;
			int nTileMinY = pBridgeSpansItem->transeparences.front().nTileY, nTileMaxY = pBridgeSpansItem->transeparences.front().nTileY;
			
			CListOfTiles::iterator it = pBridgeSpansItem->transeparences.begin();
			++it;
			for ( ; it!=pBridgeSpansItem->transeparences.end(); ++it )
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
			
			segment.visibility.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
			BYTE *pBuf = segment.visibility.GetBuffer();
			for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
			{
				for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
				{
					for ( it=pBridgeSpansItem->transeparences.begin(); it!=pBridgeSpansItem->transeparences.end(); ++it )
					{
						if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
							break;
					}
					
					if ( it != pBridgeSpansItem->transeparences.end() )
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
			
			segment.vVisOrigin.x = vPapa.x - mostLeft3.x;
			segment.vVisOrigin.y = vPapa.y - mostLeft3.y;
		}
	}
}
