#include "stdafx.h"

#include "ArtilleryPaths.h"
#include "BasePathUnit.h"
#include "AIStaticMap.h"
extern CStaticMap theStaticMap;
BASIC_REGISTER_CLASS( CArtilleryBeingTowedPath );
CArtilleryCrewPath::CArtilleryCrewPath( interface IBasePathUnit *_pUnit, const CVec2 &vStartPoint, const CVec2 &_vEndPoint, const float fMaxSpeed )
: pUnit( _pUnit ), vCurPoint( vStartPoint )
{
	SetParams( _vEndPoint, fMaxSpeed );
}
/*void CArtilleryCrewPath::GetSpeed3( CVec3 *pSpeed ) const
{
	*pSpeed = vSpeed3;
}*/
void CArtilleryCrewPath::SetParams( const CVec2 &_vEndPoint, const float fMaxSpeed, const CVec2 &_vSpeed2 )
{ 
	SetParams( _vEndPoint, fMaxSpeed );
	vSpeed3 = CVec3( _vSpeed2, 0.0f );
}
void CArtilleryCrewPath::SetParams( const CVec2 &_vEndPoint, const float fMaxSpeed )
{ 
	bNotInitialized = false;
	
	vEndPoint = _vEndPoint;
	fSpeedLen = fMaxSpeed;
	bSelfSpeed = fMaxSpeed == 0.0f;
}
bool CArtilleryCrewPath::Init( IBasePathUnit *_pUnit, IPath *pPath, bool bSmoothTurn, bool bCheckTurn ) 
{
	CPtr<IPath> p = pPath;
	pUnit = _pUnit;
	vCurPoint = pUnit->GetCenter();
	fSpeedLen = 0.0f;
	bNotInitialized = true;

	return true;
}
bool CArtilleryCrewPath::IsFinished() const
{
	return bNotInitialized || fabs2( vEndPoint - vCurPoint ) < 0.01f;
}
bool CArtilleryCrewPath::Init( IMemento *pMemento, IBasePathUnit *_pUnit ) 
{
	CPtr<IMemento> p = pMemento;
	pUnit = _pUnit;
	vCurPoint = pUnit->GetCenter();
	fSpeedLen = 0.0f;
	bNotInitialized = true;

	return true;
}
const CVec3 CArtilleryCrewPath::GetPoint( NTimer::STime timeDiff )
{
	if ( vEndPoint == vCurPoint || bNotInitialized ) // уже дошли
		fSpeedLen = 0.0f;
	else
	{
		if ( bSelfSpeed )
			fSpeedLen = pUnit->GetMaxSpeedHere( pUnit->GetCenter() );
		
		float fPassedLenght = fSpeedLen * timeDiff;
		CVec2 vDir = vEndPoint - vCurPoint;
		float fDistToGo = fabs( vDir );
		if ( fDistToGo >= fPassedLenght )// еще нужно идти
		{
			Normalize( &vDir );	
			vDir *= fPassedLenght ;
			vCurPoint += vDir;
			pUnit->UpdateDirection( GetDirectionByVector( vDir ) );
		}
		else
			vCurPoint = vEndPoint;		
	}

	return CVec3( vCurPoint.x, vCurPoint.y, theStaticMap.GetZ( AICellsTiles::GetTile( vCurPoint ) ) );
}
CArtilleryBeingTowedPath::CArtilleryBeingTowedPath( const float fSpeedLen, const CVec2 &vCurPoint, const CVec2 &vSpeed )
{ 
	Init( fSpeedLen, vCurPoint, vSpeed ); 
}
bool CArtilleryBeingTowedPath::Init( float _fSpeedLen, const class CVec2 &_vCurPoint, const CVec2 &_vSpeed )
{ 
	fSpeedLen = _fSpeedLen;
	vCurPoint = CVec3( _vCurPoint, 0.0f );
	vCurPoint2D = _vCurPoint;
	vSpeed = _vSpeed;

	return true;
}
