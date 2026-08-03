#include "StdAfx.h"

#include "ParatrooperPath.h"
#include "AIStaticMap.h"
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
CParatrooperPath::CParatrooperPath( const CVec3 &startPoint )
: vStartPoint ( startPoint ), vCurPoint(startPoint)
{
	Init();
}
void CParatrooperPath::FindFreeTile()
{
	CVec2 landPoint(vStartPoint.x, vStartPoint.y);
	SVector centerTile( AICellsTiles::GetTile( landPoint ) );

	if ( theStaticMap.IsTileInside( centerTile ) && !theStaticMap.IsLocked( centerTile, AI_CLASS_HUMAN ) )
	{
		landPoint.x = vStartPoint.x;
		landPoint.y = vStartPoint.y;
		vFinishPoint = CVec3(  landPoint, theStaticMap.GetZ( centerTile ) );
		return;//found
	}
	else
	{
		for ( int i = centerTile.x-SConsts::PARADROP_SPRED; i < centerTile.x+SConsts::PARADROP_SPRED; ++i )
		{
			for ( int j = centerTile.y-SConsts::PARADROP_SPRED; j < centerTile.y+SConsts::PARADROP_SPRED; ++j )
			{
				if ( theStaticMap.IsTileInside( i, j ) && !theStaticMap.IsLocked( i, j, AI_CLASS_HUMAN ) )
				{
					landPoint = AICellsTiles::GetPointByTile( SVector(i,j));
					vFinishPoint = CVec3(  landPoint, theStaticMap.GetZ( SVector(i,j) ) );
					return;//found
				}
			}
		}
	}
	
	vFinishPoint = CVec3( landPoint, theStaticMap.GetZ( AICellsTiles::GetTile(landPoint) ) );
}
bool CParatrooperPath::IsFinished() const
{
	return vCurPoint.z <= theStaticMap.GetZ( AICellsTiles::GetTile( CVec2( vCurPoint.x, vCurPoint.y ) ) );
}
void CParatrooperPath::Init()
{
	lastPathUpdateTime = curTime;
	FindFreeTile();
	const CVec3 curP( vCurPoint.x, vCurPoint.y, 0 );
	const CVec3 finishP ( vFinishPoint.x, vFinishPoint.y, 0 );
	const int height = vStartPoint.z - theStaticMap.GetZ( AICellsTiles::GetTile( CVec2(finishP.x,finishP.y) ) );
	const float fallTime = height/SConsts::PARATROOPER_FALL_SPEED;
	vHorSpeed = (finishP-curP)/fallTime;
	fSpeedLen = SConsts::TILE_SIZE / 3600.0f; // скорость парашютиста всегда такая, для анимации

	vFinishPoint2D.x = vFinishPoint.x;
	vFinishPoint2D.y = vFinishPoint.y;
}
void CParatrooperPath::GetSpeed3( CVec3 *vSpeed ) const
{
	vSpeed->x = vHorSpeed.x;
	vSpeed->y = vHorSpeed.y;
	vSpeed->z = - SConsts::PARATROOPER_FALL_SPEED;
}
float CParatrooperPath::CalcFallTime( const float fZ )
{
	return fZ / SConsts::PARATROOPER_FALL_SPEED;
}
const CVec3 CParatrooperPath::GetPoint( NTimer::STime timeDiff )
{
	if ( curTime - lastPathUpdateTime > SConsts::PARATROOPER_GROUND_SCAN_PERIOD &&
			 theStaticMap.IsLocked( AICellsTiles::GetTile(vFinishPoint.x,vFinishPoint.y), AI_CLASS_HUMAN ) )
	{
		Init();
	}

	if ( !IsFinished() )
	{
		vCurPoint += vHorSpeed * timeDiff;
		vCurPoint.z -= timeDiff * SConsts::PARATROOPER_FALL_SPEED;
	}			
	if ( IsFinished() )
		vCurPoint = vFinishPoint;

	return vCurPoint;
}
