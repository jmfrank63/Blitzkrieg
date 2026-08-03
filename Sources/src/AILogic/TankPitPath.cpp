#include "StdAfx.h"

#include "TankPitPath.h"
#include "BasePathUnit.h"
#include "AIStaticMap.h"
extern CStaticMap theStaticMap;
BASIC_REGISTER_CLASS( CTankPitPath );
CTankPitPath::CTankPitPath ( IBasePathUnit *_pUnit, const class CVec2 &vStartPoint, const class CVec2 &vEndPoint )
: vCurPoint( vStartPoint ), vEndPoint( vEndPoint ), fSpeedLen( 0.0f ), pUnit( _pUnit )
{
}
bool CTankPitPath::IsFinished() const
{
	return vEndPoint == vCurPoint || pUnit == 0;
}
const CVec3 CTankPitPath::GetPoint( NTimer::STime timeDiff )
{
	if ( vEndPoint == vCurPoint ) // уже дошли
		fSpeedLen = 0;
	else if ( pUnit )
	{
		fSpeedLen = pUnit->GetMaxSpeedHere( pUnit->GetCenter() );
		float fPassedLenght = fSpeedLen*timeDiff;
		CVec2 vDir = vEndPoint - vCurPoint;
		float fDistToGo = fabs( vDir );
		if ( fDistToGo >= fPassedLenght )// еще нужно идти
		{
			Normalize( &vDir );	
			vDir *= fPassedLenght ;
			vCurPoint += vDir;
		}
		else
			vCurPoint = vEndPoint;		
		
	}
	return CVec3( vCurPoint.x, vCurPoint.y, theStaticMap.GetZ( AICellsTiles::GetTile(vCurPoint) ) );
}

