#include "StdAfx.h"

#include "CManuverBoevoyZahod.h"
#include "CPlanePreferences.h"
#include "IPlane.h"
#include "ComplexPathFraction.h"
void CManuverBoevoyZahod::Init( interface IPlane *pPos, const IPlane * pEnemy )
{
	pPlane = pPos;
	const CPlanePreferences &pref = pPos->GetPreferences();
	
	CPathFractionArcLine3D *pNewPath = new CPathFractionArcLine3D;
	fSpeed = fabs( pPos->GetSpeed() );
	pNewPath->Init( pPos->GetPos(), pPos->GetSpeed(), pEnemy->GetPos(), pref.GetR( fSpeed ) );
	pNewPath->DoSubstitute( 0 );
	
	pPath = pNewPath;
	fProgress = 0;
	fZ = GetPos().z;
}
CVec3 CManuverBoevoyZahod::GetPos() const
{
	return pPath->GetPoint( fProgress );
}
CVec3 CManuverBoevoyZahod::GetSpeed() const
{
	return pPath->GetTangent( fProgress ) * fSpeed;
}
bool CManuverBoevoyZahod::Advance()
{
	fProgress += fSpeed;

	const CVec3 vNewPos( GetPos() );
	const float fDz = vNewPos.z - fZ;
	fZ = vNewPos.z;

	fSpeed += 2 * g * fDz / fSpeed;
	return fProgress + fSpeed >= pPath->GetLength();
}
