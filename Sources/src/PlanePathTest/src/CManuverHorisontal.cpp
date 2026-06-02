#include "StdAfx.h"

#include "CManuverHorisontal.h"

#include "CPlanePreferences.h"
#include "IPlane.h"
#include "ComplexPathFraction.h"
void CManuverHorisontal::Init( interface IPlane *pPos, const CVec3 &vPos, const CVec3 &vSpeed )
{
	pPlane = pPos;
	const CPlanePreferences &pref = pPos->GetPreferences();
	
	CPathFractionCircleLineCircle3D *pNewPath = new CPathFractionCircleLineCircle3D;
	fSpeed = fabs( pPos->GetSpeed() );

	pNewPath->Init( pPos->GetPos(), pEnemy->GetPos(), pPos->GetSpeed(), vSpeed, pref.GetR( fSpeed ), pref.GetR( fSpeed ) );
	pNewPath->DoSubstitute( 0 );
	
	pPath = pNewPath;
	fProgress = 0;
	fZ = GetPos().z;
}
CVec3 CManuverHorisontal::GetPos() const
{
	return pPath->GetPoint( fProgress );
}
CVec3 CManuverHorisontal::GetSpeed() const
{
	return pPath->GetTangent( fProgress ) * fSpeed;
}
bool CManuverHorisontal::Advance()
{
	fProgress += fSpeed;

	const CVec3 vNewPos( GetPos() );
	const float fDz = vNewPos.z - fZ;
	fZ = vNewPos.z;

	fSpeed += 2 * g * fDz / fSpeed;
	return fProgress + fSpeed >= pPath->GetLength();
}
