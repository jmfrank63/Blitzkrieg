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


void CBuildingFrame::SelectDirExpPoint( CBuildingDirExplosionPropsItem *pDirExpPoint )
{
	if ( pActiveDirExpPoint )
	{
		pActiveDirExpPoint->pSprite->SetOpacity( MIN_OPACITY );
		pActiveDirExpPoint->pHLine->SetOpacity( 0 );
	}
	pActiveDirExpPoint = pDirExpPoint;
	if ( !pActiveDirExpPoint )
		return;
	
	pActiveDirExpPoint->pSprite->SetOpacity( MAX_OPACITY );
	pActiveDirExpPoint->pHLine->SetOpacity( MAX_OPACITY );
	
	if ( eActiveSubMode == E_SUB_MOVE || eActiveSubMode == E_SUB_HOR )
		ComputeMaxAndMinPositions( pActiveDirExpPoint->pSprite->GetPosition() );
	else if ( eActiveSubMode == E_SUB_DIR )
		ComputeDirExpDirectionLines();
}

void CBuildingFrame::ComputeDirExpDirectionLines()
{
	NI_ASSERT( pActiveDirExpPoint != 0 );
	NI_ASSERT( pActiveDirExpPoint->pHLine != 0 );

	IScene *pSG = GetSingleton<IScene>();
	CVec3 vCenter3 = pActiveDirExpPoint->pHLine->GetPosition();		//положение центра линии
	CVec2 vCenter2;
	pSG->GetPos2( &vCenter2, vCenter3 );
	
	float fA;
	fA = ToRadian( pActiveDirExpPoint->GetDirection() );

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
	
	pOIDockBar->SetItemProperty( pActiveDirExpPoint->GetItemName(), pActiveDirExpPoint );
	GFXDraw();
}

void CBuildingFrame::SetHorDirExpPoint( const POINT &point )
{
	if ( pActiveDirExpPoint == 0 )
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
		
		NI_ASSERT( pActiveDirExpPoint->pHLine != 0 );
		pActiveDirExpPoint->pHLine->SetPosition( vPos3 );
		pSG->MoveObject( pActiveDirExpPoint->pHLine, vPos3 );
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		pActiveDirExpPoint->pHLine->Update( pTimer->GetGameTime() );
		SetChangedFlag( true );
		GFXDraw();
	}
}

void CBuildingFrame::SetDirExpPointAngle( const POINT &point )
{
	if ( pActiveDirExpPoint == 0 || pActiveDirExpPoint->pHLine == 0 )
		return;
	IScene *pSG = GetSingleton<IScene>();
	
	CVec3 vCenter3 = pActiveDirExpPoint->pHLine->GetPosition();		//положение центра конуса
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
	pActiveDirExpPoint->SetDirection( fA );
	
	SetChangedFlag( true );
	SetCapture();
	ComputeDirExpDirectionLines();
}

void CBuildingFrame::MoveDirExpPoint( const POINT &point )
{
	if ( !pActiveDirExpPoint )
		return;

	IScene *pSG = GetSingleton<IScene>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
	
	CVec2 pos2;
	CVec3 pos3;
	pos2.x = point.x;
	pos2.y = point.y;
	pSG->GetPos3( &pos3, pos2 );
	pSG->MoveObject( pActiveDirExpPoint->pSprite, pos3 );
	pActiveDirExpPoint->pSprite->Update( pTimer->GetGameTime() );
	
	pos2.x = point.x + zeroShift.x;
	pos2.y = point.y + zeroShift.y;
	pSG->GetPos3( &pos3, pos2 );
	pSG->MoveObject( pActiveDirExpPoint->pHLine, pos3 );
	pActiveDirExpPoint->pHLine->Update( pTimer->GetGameTime() );
	GFXDraw();
}

void CBuildingFrame::GenerateDirExpPoints()
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	CTreeItem *pDirExpItems = pRootItem->GetChildItem( E_BUILDING_DIR_EXPLOSIONS_ITEM );
	NI_ASSERT( pDirExpItems != 0 );
	
	NI_ASSERT( !lockedTiles.empty() );
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
	IScene *pSG = GetSingleton<IScene>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();

	POINT pt;

	int i = 0, k = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pDirExpItems->GetBegin(); it!=pDirExpItems->GetEnd(); ++it )
	{
		CBuildingDirExplosionPropsItem *pProps = static_cast<CBuildingDirExplosionPropsItem *> ( it->GetPtr() );
		switch( i )
		{
		case 0:
			v2.x = (fLeftX + fBottomX) / 2;
			v2.y = (fLeftY + fBottomY) / 2;

			pt.x = v2.x;
			pt.y = v2.y - fCellSizeY / 4;

			for ( k = 0; k < nTileMaxY-nTileMinY; k++ )
			{
				if ( IsTileLocked( pt ) )
					break;
				pt.x += fCellSizeX / 2;
				pt.y -= fCellSizeY / 2;
			}

			if ( k != nTileMaxY-nTileMinY )
			{
				v2.x += k*fCellSizeX / 2;
				v2.y -= k*fCellSizeY / 2;
			}

			pSG->GetPos3( &v3, v2 );
			pProps->pSprite->SetPosition( v3 );
			pProps->pSprite->Update( pTimer->GetGameTime() );
			pProps->pHLine->SetPosition( v3 );
			pProps->pHLine->Update( pTimer->GetGameTime() );
			pProps->SetDirection( 180 );
			break;

		case 1:
			v2.x = (fRightX + fBottomX) / 2;
			v2.y = (fRightY + fBottomY) / 2;

			pt.x = v2.x;
			pt.y = v2.y - fCellSizeY / 4;
			
			for ( k = 0; k < nTileMaxX-nTileMinX; k++ )
			{
				if ( IsTileLocked( pt ) )
					break;
				pt.x -= fCellSizeX / 2;
				pt.y -= fCellSizeY / 2;
			}
			
			if ( k != nTileMaxX-nTileMinX )
			{
				v2.x -= k*fCellSizeX / 2;
				v2.y -= k*fCellSizeY / 2;
			}
			
			pSG->GetPos3( &v3, v2 );
			pProps->pSprite->SetPosition( v3 );
			pProps->pSprite->Update( pTimer->GetGameTime() );
			pProps->pHLine->SetPosition( v3 );
			pProps->pHLine->Update( pTimer->GetGameTime() );
			pProps->SetDirection( 270 );
			break;

		case 2:
			v2.x = (fTopX + fRightX) / 2;
			v2.y = (fTopY + fRightY) / 2;

			pt.x = v2.x;
			pt.y = v2.y + fCellSizeY / 4;
			
			for ( k = 0; k < nTileMaxY-nTileMinY; k++ )
			{
				if ( IsTileLocked( pt ) )
					break;
				pt.x -= fCellSizeX / 2;
				pt.y += fCellSizeY / 2;
			}
			
			if ( k != nTileMaxY-nTileMinY )
			{
				v2.x -= k*fCellSizeX / 2;
				v2.y += k*fCellSizeY / 2;
			}

			pSG->GetPos3( &v3, v2 );
			pProps->pSprite->SetPosition( v3 );
			pProps->pSprite->Update( pTimer->GetGameTime() );
			pProps->pHLine->SetPosition( v3 );
			pProps->pHLine->Update( pTimer->GetGameTime() );
			pProps->SetDirection( 0 );
			break;

		case 3:
			v2.x = (fLeftX + fTopX) / 2;
			v2.y = (fLeftY + fTopY) / 2;
			
			pt.x = v2.x;
			pt.y = v2.y + fCellSizeY / 4;
			
			for ( k = 0; k < nTileMaxX-nTileMinX; k++ )
			{
				if ( IsTileLocked( pt ) )
					break;
				pt.x += fCellSizeX / 2;
				pt.y += fCellSizeY / 2;
			}
			
			if ( k != nTileMaxX-nTileMinX )
			{
				v2.x += k*fCellSizeX / 2;
				v2.y += k*fCellSizeY / 2;
			}
			
			pSG->GetPos3( &v3, v2 );
			pProps->pSprite->SetPosition( v3 );
			pProps->pSprite->Update( pTimer->GetGameTime() );
			pProps->pHLine->SetPosition( v3 );
			pProps->pHLine->Update( pTimer->GetGameTime() );
			pProps->SetDirection( 90 );
			break;

		case 4:
			v2.x = (fLeftX + fRightX) / 2;
			v2.y = (fLeftY + fRightY) / 2;
			pSG->GetPos3( &v3, v2 );
			pProps->pSprite->SetPosition( v3 );
			pProps->pSprite->Update( pTimer->GetGameTime() );
			pProps->pHLine->SetPosition( v3 );
			pProps->pHLine->Update( pTimer->GetGameTime() );
			pProps->SetDirection( 225 );
			break;

		default:
			NI_ASSERT( 0 );
		}

		i++;
	}

	SetChangedFlag( true );
	GFXDraw();
}

bool CBuildingFrame::IsTileLocked( const POINT &pt )
{
	float fX, fY;
	ComputeGameTileCoordinates( pt, fX, fY );
	int nX = fX, nY = fY;
	for ( CListOfTiles::iterator it=lockedTiles.begin(); it!=lockedTiles.end(); ++it )
	{
		if ( it->nTileX == nX && it->nTileY == nY )
			return true;
	}

	return false;
}
