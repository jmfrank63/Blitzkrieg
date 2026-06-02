#include "StdAfx.h"

#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"

#include "editor.h"
#include "TreeDockWnd.h"
#include "SpriteCompose.h"
#include "MainFrm.h"
#include "PropView.h"
#include "TreeItem.h"
#include "BridgeTreeItem.h"
#include "BridgeFrm.h"
#include "BridgeView.h"
#include "GameWnd.h"
#include "frames.h"

static const int MIN_OPACITY = 120;
static const int MAX_OPACITY = 255;

static const int LINE_LENGTH = 100;			//длина линии, используемой для задания конуса стрельбы
static const int EDGE_LENGTH = 200;			//длина ребра конуса
static const int SHOOT_PICTURE_SIZE = 8;



void CBridgeFrame::SelectSmokePoint( CBridgeSmokePropsItem *pSmokePoint )
{
	if ( pActiveSmokePoint )
	{
		pActiveSmokePoint->pSprite->SetOpacity( MIN_OPACITY );
		pActiveSmokePoint->pHLine->SetOpacity( 0 );
	}
	pActiveSmokePoint = pSmokePoint;
	if ( !pActiveSmokePoint )
		return;
	
	pActiveSmokePoint->pSprite->SetOpacity( MAX_OPACITY );
	if ( pActiveSmokePoint->pHLine )
		pActiveSmokePoint->pHLine->SetOpacity( MAX_OPACITY );
	
	if ( eActiveSubMode == E_SUB_MOVE || eActiveSubMode == E_SUB_HOR )
		ComputeMaxAndMinPositions( pActiveSmokePoint->pSprite->GetPosition() );
	else if ( eActiveSubMode == E_SUB_DIR )
		ComputeSmokeLines();
}

void CBridgeFrame::DeleteSmokePoint()
{
	if ( !pActiveSmokePoint )
		return;

	IScene *pSG = GetSingleton<IScene>();
	pSG->RemoveObject( pActiveSmokePoint->pSprite );
	if ( pActiveSmokePoint->pHLine )
		pSG->RemoveObject( pActiveSmokePoint->pHLine );
	pActiveSmokePoint = 0;
	SetChangedFlag( true );
	GFXDraw();
}

void CBridgeFrame::ComputeSmokeLines()
{
	NI_ASSERT( pActiveSmokePoint != 0 );
	NI_ASSERT( pActiveSmokePoint->pHLine != 0 );

	IScene *pSG = GetSingleton<IScene>();
	CVec3 vCenter3 = pActiveSmokePoint->pHLine->GetPosition();		//положение центра линии
	CVec2 vCenter2;
	pSG->GetPos2( &vCenter2, vCenter3 );
	
	float fA;
	fA = ToRadian( pActiveSmokePoint->GetDirection() );

	CVec3 vPos3;
	vPos3.x = vCenter3.x - (float) EDGE_LENGTH * sin( fA );
	vPos3.y = vCenter3.y + (float) EDGE_LENGTH * cos( fA );
	vPos3.z = 0;
	CVec2 vPos2;
	pSG->GetPos2( &vPos2, vPos3 );
	
	CVec3 vLine1, vLine2;			//линии, отображающие красную стрелочку
	vLine1.z = vLine2.z = 0;
	float fTemp = ToRadian( 5.0f );
	vLine1.x = vCenter3.x - (float) (EDGE_LENGTH - 20) * sin( fA - fTemp );
	vLine1.y = vCenter3.y + (float) (EDGE_LENGTH - 20) * cos( fA - fTemp );
	vLine2.x = vCenter3.x - (float) (EDGE_LENGTH - 20) * sin( fA + fTemp );
	vLine2.y = vCenter3.y + (float) (EDGE_LENGTH - 20) * cos( fA + fTemp );
	
	{
		CVerticesLock<SGFXTLVertex> vertices( pFireDirectionVertices );
		
		CVec2 v;
		
		DWORD dwColor = 0xffffff00;
		vertices[0].Setup( vCenter2.x, vCenter2.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[1].Setup( vPos2.x, vPos2.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		
		pSG->GetPos2( &v, vLine1 );
		vertices[2].Setup( v.x, v.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[3].Setup( vPos2.x, vPos2.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		
		pSG->GetPos2( &v, vLine2 );
		vertices[4].Setup( v.x, v.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[5].Setup( vPos2.x, vPos2.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}
	
	pOIDockBar->SetItemProperty( pActiveSmokePoint->GetItemName(), pActiveSmokePoint );
	GFXDraw();
}

void CBridgeFrame::SetHorSmokePoint( const POINT &point )
{
	if ( pActiveSmokePoint == 0 )
		return;
	
	const int DELTA_X = 10;
	if ( point.x >= m_fX - DELTA_X && point.x <= m_fX + DELTA_X && point.y >= m_fMinY && point.y <= m_fMaxY )
	{
		CVec2 vPos2;
		vPos2.x = m_fX;
		vPos2.y = point.y;
		CVec3 vPos3;
		IScene *pSG = GetSingleton<IScene>();
		pSG->GetPos3( &vPos3, vPos2 );
		
		NI_ASSERT( pActiveSmokePoint->pHLine != 0 );
		pActiveSmokePoint->pHLine->SetPosition( vPos3 );
		pSG->MoveObject( pActiveSmokePoint->pHLine, vPos3 );
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		pActiveSmokePoint->pHLine->Update( pTimer->GetGameTime() );
		SetChangedFlag( true );
		GFXDraw();
	}
}

void CBridgeFrame::SetSmokePointAngle( const POINT &point )
{
	if ( pActiveSmokePoint == 0 || pActiveSmokePoint->pHLine == 0 )
		return;
	IScene *pSG = GetSingleton<IScene>();
	
	CVec3 vCenter3 = pActiveSmokePoint->pHLine->GetPosition();		//положение центра конуса
	CVec2 vCenter2;
	pSG->GetPos2( &vCenter2, vCenter3 );
	float temp = (vCenter2.x - point.x)*(vCenter2.x - point.x) + (vCenter2.y - point.y)*(vCenter2.y - point.y);
	if ( sqrt( temp ) < 5 )
		return;				//если очень маленькие расстояния, то будет сильно скакать, избегаем скачков
	
	CVec2 vPos2;
	vPos2.x = point.x;
	vPos2.y = point.y;
	CVec3 vPos3;
	pSG->GetPos3( &vPos3, vPos2 );
	
	CVec3 vCone;
	vCone.x = vPos3.x - vCenter3.x;
	vCone.y = vPos3.y - vCenter3.y;
	float fA = -atan2( vCone.x, vCone.y );
	fA = ToDegree( fA );
	if ( fA < 0 )
		fA = 360 + fA;
	pActiveSmokePoint->SetDirection( fA );
	
	SetChangedFlag( true );
	SetCapture();
	ComputeSmokeLines();
}

void CBridgeFrame::MoveSmokePoint( const POINT &point )
{
	if ( !pActiveSmokePoint )
		return;
	
	IScene *pSG = GetSingleton<IScene>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
	
	CVec2 pos2;
	CVec3 pos3;
	pos2.x = point.x + objShift.x;
	pos2.y = point.y + objShift.y;
	pSG->GetPos3( &pos3, pos2 );
	pSG->MoveObject( pActiveSmokePoint->pSprite, pos3 );
	pActiveSmokePoint->pSprite->Update( pTimer->GetGameTime() );
	
	pos2.x = point.x + zeroShift.x;
	pos2.y = point.y + zeroShift.y;
	pSG->GetPos3( &pos3, pos2 );
	pSG->MoveObject( pActiveSmokePoint->pHLine, pos3 );
	pActiveSmokePoint->pHLine->Update( pTimer->GetGameTime() );
	GFXDraw();
}

void CBridgeFrame::AddOrSelectSmokePoint( const POINT &point )
{
	CVec2 smokePos2;
	smokePos2.x = point.x;
	smokePos2.y = point.y;
	CVec3 smokePos3;
	IScene *pSG = GetSingleton<IScene>();
	pSG->GetPos3( &smokePos3, smokePos2 );
	
	objShift = VNULL2;
	zeroShift = VNULL2;
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	CTreeItem *pSmokeItems = pRootItem->GetChildItem( E_BRIDGE_SMOKES_ITEM );
	NI_ASSERT( pSmokeItems != 0 );
	
	for ( CTreeItem::CTreeItemList::const_iterator it=pSmokeItems->GetBegin(); it!=pSmokeItems->GetEnd(); ++it )
	{
		CBridgeSmokePropsItem *pProps = static_cast<CBridgeSmokePropsItem *> ( it->GetPtr() );
		CVec3 vPos3 = pProps->pSprite->GetPosition();
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, vPos3 );

		if ( point.x >= vPos2.x - SHOOT_PICTURE_SIZE && point.x <= vPos2.x + SHOOT_PICTURE_SIZE &&
			point.y >= vPos2.y - SHOOT_PICTURE_SIZE && point.y <= vPos2.y + SHOOT_PICTURE_SIZE )
		{
			SelectSmokePoint( pProps );
			pProps->SelectMeInTheTree();

			SetChangedFlag( true );
			objShift.x = vPos2.x - point.x;
			objShift.y = vPos2.y - point.y;

			vPos3 = pProps->pHLine->GetPosition();
			pSG->GetPos2( &vPos2, vPos3 );
			zeroShift.x = vPos2.x - point.x;
			zeroShift.y = vPos2.y - point.y;

			m_mode = E_SET_SMOKE_POINT;
			g_frameManager.GetGameWnd()->SetCapture();
			return;
		}
	}
	
	if ( !ComputeMaxAndMinPositions( smokePos3 ) )
	{
		return;
	}
	
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	CPtr<IObjVisObj> pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
	NI_ASSERT( pObject != 0 );
	if ( !pObject )
	{
		return;
	}
	
	pObject->SetPosition( smokePos3 );
	pObject->SetDirection( 0 );
	pObject->SetAnimation( 0 );
	pSG->AddObject( pObject, SGVOGT_OBJECT );
	pObject->SetOpacity( MAX_OPACITY );
	
	CBridgeSmokePropsItem *pNewPoint = new CBridgeSmokePropsItem;
	pNewPoint->SetItemName( "Smoke point" );
	pNewPoint->pSprite = pObject;
	
	if ( pActiveSmokePoint )
		pNewPoint->SetDirection( pActiveSmokePoint->GetDirection() );
	else
		pNewPoint->SetDirection( 0 );
	
	CVec3 vHPos3 = smokePos3;
	if ( pActiveSmokePoint )
	{
		CVec2 vHPos2, vSprite2;
		pSG->GetPos2( &vHPos2, pActiveSmokePoint->pHLine->GetPosition() );
		pSG->GetPos2( &vSprite2, pActiveSmokePoint->pSprite->GetPosition() );
		
		CVec2 vNewSpritePos2;
		pSG->GetPos2( &vNewSpritePos2, smokePos3 );
		vNewSpritePos2.y += vHPos2.y - vSprite2.y;
		if ( vNewSpritePos2.y < m_fMinY )
			vNewSpritePos2.y = m_fMinY;
		if ( vNewSpritePos2.y > m_fMaxY )
			vNewSpritePos2.y = m_fMaxY;
		pSG->GetPos3( &vHPos3, vNewSpritePos2 );
	}
	else
	{
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, smokePos3 );
		vPos2.y = m_fMaxY;
		pSG->GetPos3( &vHPos3, vPos2 );
	}
	
	pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
	NI_ASSERT( pObject != 0 );
	
	pObject->SetDirection( 0 );
	pObject->SetAnimation( 0 );
	pObject->SetPosition( vHPos3 );
	pSG->AddObject( pObject, SGVOGT_OBJECT );
	pObject->Update( GetSingleton<IGameTimer>()->GetGameTime() );
	pObject->SetOpacity( MAX_OPACITY );
	pNewPoint->pHLine = pObject;
	pSmokeItems->AddChild( pNewPoint );
	
	SelectSmokePoint( pNewPoint );
	pNewPoint->SelectMeInTheTree();
	SetChangedFlag( true );
	GFXDraw();
}

void CBridgeFrame::GenerateSmokePoints()
{
	IScene *pSG = GetSingleton<IScene>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();

	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	CTreeItem *pSmokeItems = pRootItem->GetChildItem( E_BRIDGE_SMOKES_ITEM );
	NI_ASSERT( pSmokeItems != 0 );
	if ( pSmokeItems->GetChildsCount() > 0 )
	{
		int nRes = AfxMessageBox( "This action will remove all smoke points, do you want to proceed?", MB_YESNO );
		if ( nRes == IDNO )
			return;

		for ( CTreeItem::CTreeItemList::const_iterator it=pSmokeItems->GetBegin(); it!=pSmokeItems->GetEnd(); ++it )
		{
			CBridgeSmokePropsItem *pProps = static_cast<CBridgeSmokePropsItem *> ( it->GetPtr() );
			pSG->RemoveObject( pProps->pSprite );
			if ( pProps->pHLine )
				pSG->RemoveObject( pProps->pHLine );
		}

		pSmokeItems->RemoveAllChilds();
		pActiveSmokePoint = 0;
	}
	
	NI_ASSERT( !pActiveSpansItem->lockedTiles.empty() );
	int nTileMinX = pActiveSpansItem->lockedTiles.front().nTileX, nTileMaxX = pActiveSpansItem->lockedTiles.front().nTileX;
	int nTileMinY = pActiveSpansItem->lockedTiles.front().nTileY, nTileMaxY = pActiveSpansItem->lockedTiles.front().nTileY;
	CListOfTiles::iterator it=pActiveSpansItem->lockedTiles.begin();
	for ( ++it; it!=pActiveSpansItem->lockedTiles.end(); ++it )
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
	
	float fLeftX, fLeftY, fTopX, fTopY, fRightX, fRightY, fBottomX, fBottomY;
	float fX1, fY1, fX2, fY2, fX3, fY3, fX4, fY4;
	GetGameTileCoordinates( nTileMinX, nTileMinY, fX1, fY1, fX2, fY2, fX3, fY3, fX4, fY4 );
	fLeftX = fX2;
	fLeftY = fY2;
	
	GetGameTileCoordinates( nTileMinX, nTileMaxY, fX1, fY1, fX2, fY2, fX3, fY3, fX4, fY4 );
	fTopX = fX1;
	fTopY = fY1;
	
	GetGameTileCoordinates( nTileMaxX, nTileMaxY, fX1, fY1, fX2, fY2, fX3, fY3, fX4, fY4 );
	fRightX = fX4;
	fRightY = fY4;
	
	GetGameTileCoordinates( nTileMaxX, nTileMinY, fX1, fY1, fX2, fY2, fX3, fY3, fX4, fY4 );
	fBottomX = fX3;
	fBottomY = fY3;
	
	CVec2 v2;
	CVec3 v3;
	pTimer->Update( timeGetTime() );
	
	if ( m_bHorizontal )
	{
		for ( int i=0; i<(nTileMaxX-nTileMinX+1)/2; i++ )
		{
			v2.x = fLeftX + i * fCellSizeX + fCellSizeX / 4;
			v2.y = fLeftY + i * fCellSizeY + fCellSizeY / 4;
			pSG->GetPos3( &v3, v2 );
			
			CBridgeSmokePropsItem *pProps = new CBridgeSmokePropsItem;
			pProps->SetItemName( "Smoke point" );
			CreateSmokeSprites( pProps );
			pProps->pSprite->SetPosition( v3 );
			pProps->pHLine->SetPosition( v3 );
			pSG->AddObject( pProps->pSprite, SGVOGT_OBJECT );
			pSG->AddObject( pProps->pHLine, SGVOGT_OBJECT );
			
			pProps->SetDirection( 180 );
			pSmokeItems->AddChild( pProps );
		}
		for ( int i=0; i<(nTileMaxX-nTileMinX+1)/2; i++ )
		{
			v2.x = fRightX - i * fCellSizeX - fCellSizeX / 4;
			v2.y = fRightY - i * fCellSizeY - fCellSizeY / 4;
			pSG->GetPos3( &v3, v2 );
			
			CBridgeSmokePropsItem *pProps = new CBridgeSmokePropsItem;
			pProps->SetItemName( "Smoke point" );
			CreateSmokeSprites( pProps );
			pProps->pSprite->SetPosition( v3 );
			pProps->pHLine->SetPosition( v3 );
			pSG->AddObject( pProps->pSprite, SGVOGT_OBJECT );
			pSG->AddObject( pProps->pHLine, SGVOGT_OBJECT );
			
			pProps->SetDirection( 0 );
			pSmokeItems->AddChild( pProps );
		}
	}
	else
	{
		for ( int i=0; i<(nTileMaxY-nTileMinY+1)/2; i++ )
		{
			v2.x = fBottomX + i * fCellSizeX + fCellSizeX / 4;
			v2.y = fBottomY - i * fCellSizeY - fCellSizeY / 4;
			pSG->GetPos3( &v3, v2 );
			
			CBridgeSmokePropsItem *pProps = new CBridgeSmokePropsItem;
			pProps->SetItemName( "Smoke point" );
			CreateSmokeSprites( pProps );
			pProps->pSprite->SetPosition( v3 );
			pProps->pHLine->SetPosition( v3 );
			pSG->AddObject( pProps->pSprite, SGVOGT_OBJECT );
			pSG->AddObject( pProps->pHLine, SGVOGT_OBJECT );
			
			pProps->SetDirection( 270 );
			pSmokeItems->AddChild( pProps );
		}
		for ( int i=0; i<(nTileMaxY-nTileMinY+1)/2; i++ )
		{
			v2.x = fTopX - i * fCellSizeX - fCellSizeX / 4;
			v2.y = fTopY + i * fCellSizeY + fCellSizeY / 4;
			pSG->GetPos3( &v3, v2 );
			
			CBridgeSmokePropsItem *pProps = new CBridgeSmokePropsItem;
			pProps->SetItemName( "Smoke point" );
			CreateSmokeSprites( pProps );
			pProps->pSprite->SetPosition( v3 );
			pProps->pHLine->SetPosition( v3 );
			pSG->AddObject( pProps->pSprite, SGVOGT_OBJECT );
			pSG->AddObject( pProps->pHLine, SGVOGT_OBJECT );
			
			pProps->SetDirection( 90 );
			pSmokeItems->AddChild( pProps );
		}
	}	
	SetChangedFlag( true );
	GFXDraw();
}

void CBridgeFrame::CreateSmokeSprites( CBridgeSmokePropsItem *pSmokePoint )
{
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	
	pSmokePoint->pSprite = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot\\1", 0, SGVOT_SPRITE ) );
	NI_ASSERT( pSmokePoint->pSprite != 0 );
	pSmokePoint->pSprite->SetDirection( 0 );
	pSmokePoint->pSprite->SetAnimation( 0 );
	pSmokePoint->pSprite->SetOpacity( MIN_OPACITY );
	
	pSmokePoint->pHLine = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\shoot_horizontal\\1", 0, SGVOT_SPRITE ) );
	NI_ASSERT( pSmokePoint->pHLine != 0 );
	pSmokePoint->pHLine->SetDirection( 0 );
	pSmokePoint->pHLine->SetAnimation( 0 );
	pSmokePoint->pHLine->SetOpacity( 0 );
}
