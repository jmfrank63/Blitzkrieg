#include "StdAfx.h"
#include "CManuverGorka.h"

#include "CPlanePreferences.h"
#include "IPlane.h"
#include "ComplexPathFraction.h"

void CManuverGorka::Init( interface IPlane *pPos )
{
	pPlane = pPos;
	const CPlanePreferences &pref = pPos->GetPreferences();
	CPathFractionArcLine3D *pNewPath = new CPathFractionArcLine3D;
	
	CVec3 vSpeed ( pPos->GetSpeed() );
	fSpeed = fabs( vSpeed );
	Normalize( &vSpeed );
	
	CVec2 vHorSpeed( vSpeed.x, vSpeed.y );
	float fPathLength = pref.GetR( fSpeed ) * 2.0f; 
	CVec3 vDesiredPos(  fPathLength * vHorSpeed, fPathLength / 2.0f + pPos->GetPos().z );
	
	pNewPath->Init( pPos->GetPos(), pPos->GetSpeed(), vDesiredPos, pref.GetR( fSpeed ) );
	
	pNewPath->DoSubstitute( 0 );
	
	pPath = pNewPath;
	fProgress = 0;
	fZ = GetPos().z;
}
CVec3 CManuverGorka::GetPos() const
{
	return pPath->GetPoint( fProgress );
}
CVec3 CManuverGorka::GetSpeed() const
{
	return pPath->GetTangent( fProgress ) * fSpeed;
}
bool CManuverGorka::Advance()
{
	fProgress += fSpeed;

	const CVec3 vNewPos( GetPos() );
	const float fDz = vNewPos.z - fZ;
	fZ = vNewPos.z;

	fSpeed += 2 * g * fDz / fSpeed;
	return fProgress + fSpeed >= pPath->GetLength();
}
