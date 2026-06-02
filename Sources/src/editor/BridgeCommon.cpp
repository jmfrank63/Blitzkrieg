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

bool CBridgeFrame::IsTileLocked( const POINT &pt )
{
	float fX, fY;
	ComputeGameTileCoordinates( pt, fX, fY );
	int nX = fX, nY = fY;
	for ( CListOfTiles::iterator it=pActiveSpansItem->lockedTiles.begin(); it!=pActiveSpansItem->lockedTiles.end(); ++it )
	{
		if ( it->nTileX == nX && it->nTileY == nY )
			return true;
	}
	
	return false;
}

bool CBridgeFrame::ComputeMaxAndMinPositions( const CVec3 &vPos3 )
{
	IScene *pSG = GetSingleton<IScene>();
	CVec2 vPos2;
	pSG->GetPos2( &vPos2, vPos3 );
	
	if ( pActiveSpansItem->lockedTiles.empty() )
		return false;
	
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

bool CBridgeFrame::GetLineIntersection( const CVec2 &vPos2, float fx1, float fy1, float fx2, float fy2, float *y )
{
	if ( vPos2.x < fx1 || vPos2.x > fx2 )
		return false;
	
	*y = fy1 + (vPos2.x - fx1)*(fy2 - fy1)/(fx2-fx1);
	return true;
}
