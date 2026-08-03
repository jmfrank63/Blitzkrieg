#include "StdAfx.h"

#include "../Scene/Scene.h"
#include "../Anim/Animation.h"

#include "editor.h"
#include "TreeDockWnd.h"
#include "SpriteCompose.h"
#include "MainFrm.h"
#include "PropView.h"
#include "TreeItem.h"
#include "BuildTreeItem.h"
#include "BuildFrm.h"
#include "BuildView.h"
#include "GameWnd.h"
#include "frames.h"

static const int MIN_OPACITY = 120;
static const int MAX_OPACITY = 255;

static const int LINE_LENGTH = 100;			//длина линии, используемой для задания конуса стрельбы
static const int EDGE_LENGTH = 200;			//длина ребра конуса
static const int SHOOT_PICTURE_SIZE = 8;


void CBuildingFrame::SetActiveShootPoint( SShootPoint *pShootPoint )
{
	if ( pActiveShootPoint )
	{
		pActiveShootPoint->pSprite->SetOpacity( MIN_OPACITY );
		if ( pActiveShootPoint->pHLine )
			pActiveShootPoint->pHLine->SetOpacity( 0 );
	}
	pActiveShootPoint = pShootPoint;
	if ( !pShootPoint )
		return;
	
	pActiveShootPoint->pSprite->SetOpacity( MAX_OPACITY );
	pActiveShootPoint->pHLine->SetOpacity( MAX_OPACITY );

	if ( eActiveSubMode == E_SUB_MOVE || eActiveSubMode == E_SUB_HOR )
		ComputeMaxAndMinPositions( pActiveShootPoint->pSprite->GetPosition() );
	else if ( eActiveSubMode == E_SUB_DIR )
		ComputeAngleLines();
}

void CBuildingFrame::SetConeDirection( float fVal )
{
	NI_ASSERT( pActiveShootPoint != 0 );
	
	pActiveShootPoint->fDirection = fVal;
	ComputeAngleLines();
}

void CBuildingFrame::SetConeAngle( float fVal )
{
	NI_ASSERT( pActiveShootPoint != 0 );
	
	pActiveShootPoint->fAngle = fVal;
	ComputeAngleLines();
}

bool CBuildingFrame::ComputeMaxAndMinPositions( const CVec3 &vPos3 )
{
	IScene *pSG = GetSingleton<IScene>();
	CVec2 vPos2;
	pSG->GetPos2( &vPos2, vPos3 );
	
	if ( lockedTiles.empty() )
		return false;
	
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

	float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
	float x1, x2, y1, y2, yMin, yMax, y;
	bool bRes = 0;
	bool bFound = false;
/*
       1
      /\
    /    \ 4
2 /      /
  \    /
    \/
    3
*/


	CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
	x1 = fx2;
	y1 = fy2;

	CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMaxY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
	x2 = fx1;
	y2 = fy1;

	bRes = GetLineIntersection( vPos2, x1, y1, x2, y2, &y );
	if ( bRes )
	{
		yMin = y;
		bFound = true;
	}
	else
	{
		x1 = x2;
		y1 = y2;
		
		CGridFrame::GetGameTileCoordinates( nTileMaxX, nTileMaxY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
		x2 = fx4;
		y2 = fy4;
		
		bRes = GetLineIntersection( vPos2, x1, y1, x2, y2, &y );
		if ( bRes )
		{
			yMin = y;
			bFound = true;
		}
		else
		{
		}
	}

	if ( !bFound )
	{
		return false;
	}

	CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
	x1 = fx2;
	y1 = fy2;

	CGridFrame::GetGameTileCoordinates( nTileMaxX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
	x2 = fx3;
	y2 = fy3;
	
	bRes = GetLineIntersection( vPos2, x1, y1, x2, y2, &y );
	if ( bRes )
		yMax = y;
	else
	{
		x1 = x2;
		y1 = y2;

		CGridFrame::GetGameTileCoordinates( nTileMaxX, nTileMaxY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
		x2 = fx4;
		y2 = fy4;

		bRes = GetLineIntersection( vPos2, x1, y1, x2, y2, &y );
		if ( bRes )
			yMax = y;
		else
			NI_ASSERT( 0 );
	}
	
	m_fMinY = yMin;
	m_fMaxY = yMax;
	m_fX = vPos2.x;
	{
		CVerticesLock<SGFXTLVertex> vertices( pHorizontalPointVertices );
		vertices[0].Setup( vPos2.x, yMin, 1, 1, 0xffffffff, 0xff000000, 0, 0 );
		vertices[1].Setup( vPos2.x, yMax, 1, 1, 0xffffffff, 0xff000000, 0, 0 );
	}
	
	GFXDraw();
	return true;
}


void CBuildingFrame::ComputeAngleLines()
{
	NI_ASSERT( pActiveShootPoint != 0 );
	if ( !pActiveShootPoint->pHLine )
		return;
	
	IScene *pSG = GetSingleton<IScene>();
	CVec3 vCenter3 = pActiveShootPoint->pHLine->GetPosition();		//положение центра конуса
	CVec2 vCenter2;
	pSG->GetPos2( &vCenter2, vCenter3 );
	
	CVec3 v1, v2;
	v1.z = v2.z = 0;
	
	float fA, fB;
	fA = ToRadian( pActiveShootPoint->fDirection );
	fB = ToRadian( pActiveShootPoint->fAngle );
	fB = fB / 2;
	
	v1.x = vCenter3.x - (float) EDGE_LENGTH * sin( fA - fB );
	v1.y = vCenter3.y + (float) EDGE_LENGTH * cos( fA - fB );
	v2.x = vCenter3.x - (float) EDGE_LENGTH * sin( fA + fB );
	v2.y = vCenter3.y + (float) EDGE_LENGTH * cos( fA + fB );
	
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
		CVerticesLock<SGFXTLVertex> vertices( pConeVertices );
		
		CVec2 v;
		pSG->GetPos2( &v, v1 );
		
		DWORD dwColor = 0xffffff00;
		vertices[0].Setup( vCenter2.x, vCenter2.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[1].Setup( v.x, v.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[4].Setup( v.x, v.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		
		pSG->GetPos2( &v, v2 );
		vertices[2].Setup( vCenter2.x, vCenter2.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[3].Setup( v.x, v.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[5].Setup( v.x, v.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		
		vertices[6].Setup( vCenter2.x, vCenter2.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[7].Setup( vPos2.x, vPos2.y, 1, 1, dwColor, 0xff000000, 0, 0 );
		
		pSG->GetPos2( &v, vLine1 );
		vertices[8].Setup( v.x, v.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[9].Setup( vPos2.x, vPos2.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		
		pSG->GetPos2( &v, vLine2 );
		vertices[10].Setup( v.x, v.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[11].Setup( vPos2.x, vPos2.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}
	
	pActiveShootPoint->pSlot->SetConeAngle( pActiveShootPoint->fAngle );
	pActiveShootPoint->pSlot->SetConeDirection( pActiveShootPoint->fDirection );
	pOIDockBar->SetItemProperty( pActiveShootPoint->pSlot->GetItemName(), pActiveShootPoint->pSlot );
	
	GFXDraw();
}

bool CBuildingFrame::GetLineIntersection( const CVec2 &vPos2, float fx1, float fy1, float fx2, float fy2, float *y )
{
	if ( vPos2.x < fx1 || vPos2.x > fx2 )
		return false;
	
	*y = fy1 + (vPos2.x - fx1)*(fy2 - fy1)/(fx2-fx1);
	return true;
}

void CBuildingFrame::SelectShootPoint( CTreeItem *pShoot )
{
	for ( CListOfShootPoints::iterator it=shootPoints.begin(); it!=shootPoints.end(); ++it )
	{
		if ( it->pSlot == pShoot )
		{
			SetActiveShootPoint( &(*it) );
			GFXDraw();
			break;
		}
	}
}

void CBuildingFrame::DeleteShootPoint( CTreeItem *pShoot )
{
	for ( CListOfShootPoints::iterator it=shootPoints.begin(); it!=shootPoints.end(); ++it )
	{
		if ( it->pSlot == pShoot )
		{
			IScene *pSG = GetSingleton<IScene>();
			pSG->RemoveObject( it->pSprite );
			if ( it->pHLine )
				pSG->RemoveObject( it->pHLine );
			shootPoints.erase( it );
			pActiveShootPoint = 0;
			SetChangedFlag( true );
			GFXDraw();
			break;
		}
	}
}

void CBuildingFrame::AddOrSelectShootPoint( const POINT &point )
{
	CVec2 shootPos2;
	shootPos2.x = point.x;
	shootPos2.y = point.y;
	CVec3 shootPos3;
	IScene *pSG = GetSingleton<IScene>();
	pSG->GetPos3( &shootPos3, shootPos2 );

	objShift = VNULL2;
	zeroShift = VNULL2;

	CListOfShootPoints::iterator it=shootPoints.begin();
	for ( ; it!=shootPoints.end(); ++it )
	{
		CVec3 vPos3 = it->pSprite->GetPosition();
		CVec2 vPos2;
		pSG->GetPos2( &vPos2, vPos3 );

		if ( point.x >= vPos2.x - SHOOT_PICTURE_SIZE && point.x <= vPos2.x + SHOOT_PICTURE_SIZE &&
			point.y >= vPos2.y - SHOOT_PICTURE_SIZE && point.y <= vPos2.y + SHOOT_PICTURE_SIZE )
		{
			SetActiveShootPoint( &(*it) );
			it->pSlot->SelectMeInTheTree();

			SetChangedFlag( true );
			objShift.x = vPos2.x - point.x;
			objShift.y = vPos2.y - point.y;

			vPos3 = it->pHLine->GetPosition();
			pSG->GetPos2( &vPos2, vPos3 );
			zeroShift.x = vPos2.x - point.x;
			zeroShift.y = vPos2.y - point.y;

			m_mode = E_SET_SHOOT_POINT;
			g_frameManager.GetGameWnd()->SetCapture();
			return;
		}
	}
	
	if ( !ComputeMaxAndMinPositions( shootPos3 ) )
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
	
	pObject->SetPosition( shootPos3 );
	pObject->SetDirection( 0 );
	pObject->SetAnimation( 0 );
	pSG->AddObject( pObject, SGVOGT_OBJECT );
	pObject->SetOpacity( MAX_OPACITY );
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRoot = pTree->GetRootItem();
	CTreeItem *pShootsItem = pRoot->GetChildItem( E_BUILDING_SLOTS_ITEM );
	CBuildingSlotPropsItem *pNewSlot = new CBuildingSlotPropsItem;
	pNewSlot->SetItemName( "Shoot point" );
	pShootsItem->AddChild( pNewSlot );
	pNewSlot->SelectMeInTheTree();
	
	SShootPoint shoot;
	shoot.pSlot = pNewSlot;
	shoot.pSprite = pObject;

	if ( pActiveShootPoint )
	{
		shoot.fAngle = pActiveShootPoint->fAngle;
		shoot.fDirection = pActiveShootPoint->fDirection;
		shoot.pSlot->SetConeAngle( shoot.fAngle );
		shoot.pSlot->SetConeDirection( shoot.fDirection );
		shoot.pSlot->SetSightMultiplier( pActiveShootPoint->pSlot->GetSightMultiplier() );
		shoot.pSlot->SetCover( pActiveShootPoint->pSlot->GetCover() );
	}
	else
	{
		shoot.fAngle = 80;
		shoot.fDirection = 0;
		shoot.pSlot->SetConeAngle( shoot.fAngle );
		shoot.pSlot->SetConeDirection( shoot.fDirection );
	}

	CVec3 vHPos3 = shootPos3;
	if ( pActiveShootPoint )
	{
		CVec2 vHPos2, vSprite2;
		pSG->GetPos2( &vHPos2, pActiveShootPoint->pHLine->GetPosition() );
		pSG->GetPos2( &vSprite2, pActiveShootPoint->pSprite->GetPosition() );
		
		CVec2 vNewSpritePos2;
		pSG->GetPos2( &vNewSpritePos2, shootPos3 );
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
		pSG->GetPos2( &vPos2, shootPos3 );
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
	shoot.pHLine = pObject;
	
	shootPoints.push_back( shoot );
	SetActiveShootPoint( &(shootPoints.back()) );
	pOIDockBar->SetItemProperty( pActiveShootPoint->pSlot->GetItemName(), pActiveShootPoint->pSlot );
	SetChangedFlag( true );
	GFXDraw();
}

void CBuildingFrame::SetHorShootPoint( const POINT &point )
{
	if ( pActiveShootPoint == 0 )
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
		
		NI_ASSERT( pActiveShootPoint->pHLine != 0 );
		if ( pActiveShootPoint->pHLine )
		{
			pActiveShootPoint->pHLine->SetPosition( vPos3 );
			pSG->MoveObject( pActiveShootPoint->pHLine, vPos3 );
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			pActiveShootPoint->pHLine->Update( pTimer->GetGameTime() );
		}
		SetChangedFlag( true );
		GFXDraw();
	}
}

void CBuildingFrame::SetShootPointAngle( const POINT &point )
{
	if ( pActiveShootPoint == 0 || pActiveShootPoint->pHLine == 0 )
		return;
	IScene *pSG = GetSingleton<IScene>();
	
	CVec3 vCenter3 = pActiveShootPoint->pHLine->GetPosition();		//положение центра конуса
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
	float fB = atan2( (float) (LINE_LENGTH/2), sqrt(vCone.x*vCone.x + vCone.y*vCone.y) );
	fA = ToDegree( fA );
	fB = ToDegree( fB );
	if ( fA < 0 )
		fA = 360 + fA;
	if ( fB < 0 )
		fB = -fB;
	pActiveShootPoint->fDirection = fA;
	pActiveShootPoint->fAngle = fB * 2;
	
	SetChangedFlag( true );
	SetCapture();
	ComputeAngleLines();
}

void CBuildingFrame::MoveShootPoint( const POINT &point )
{
	IScene *pSG = GetSingleton<IScene>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
	
	CVec2 pos2;
	CVec3 pos3;
	pos2.x = point.x + objShift.x;
	pos2.y = point.y + objShift.y;
	pSG->GetPos3( &pos3, pos2 );
	pSG->MoveObject( pActiveShootPoint->pSprite, pos3 );
	pActiveShootPoint->pSprite->Update( pTimer->GetGameTime() );
	
	pos2.x = point.x + zeroShift.x;
	pos2.y = point.y + zeroShift.y;
	pSG->GetPos3( &pos3, pos2 );
	pSG->MoveObject( pActiveShootPoint->pHLine, pos3 );
	pActiveShootPoint->pHLine->Update( pTimer->GetGameTime() );
	GFXDraw();
}
