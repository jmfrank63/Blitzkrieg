#include "StdAfx.h"

#include "CManuver.h"
#include "IPlane.h"
#include "ComplexPathFraction.h"
#include "../../PlanePathTest/src/CPlanePreferences.h"
#include "../../PlanePathTest/src/CManuverBuilder.h"
extern float g = 0.0000000983f;
BASIC_REGISTER_CLASS( CManuver );
BASIC_REGISTER_CLASS( IManuver );
BASIC_REGISTER_CLASS( CManuverSteepClimb );
BASIC_REGISTER_CLASS( CManuverGeneric );

const float SPlanesConsts::MIN_HEIGHT = 100.0f;
int CManuverGeneric::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CManuver*>(this) );
	return 0;
}
int CManuverSteepClimb::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CManuver*>(this) );
	return 0;
}
int CManuver::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pPath );
	saver.Add( 2, &fProgress );
	saver.Add( 3, &fSpeed );
	
	saver.Add( 4, &vCenter );
	saver.Add( 5, &vSpeed );
	saver.Add( 6, &vNormal );			

	return 0;
}

const CVec3 CManuver::CalcPredictedPoint( interface IPlane *pPos, interface IPlane *pEnemy )
{
	const float fDist = fabs( pEnemy->GetPosB2() - pPos->GetPosB2() );
	
	const float fSpeed = fabs( pPos->GetSpeedB2() );

	const float fTime = fDist / fSpeed;

	return pEnemy->GetManuver()->GetProspectivePoint( fTime );
}
CVec3 CManuver::GetProspectivePoint( const float fT ) const
{
	const float fAdd = fT * fSpeed;
	const float fDiff = fAdd + fProgress  - pPath->GetLength();
	
	if ( fDiff > 0.0f ) // asseme that further movement is by line
		return pPath->GetEndPoint() + pPath->GetEndTangent() * fSpeed * fDiff;
	else
		return pPath->GetPoint( fAdd + fProgress );
}
void CManuver::InitCommon( interface IPathFraction *_pPath, interface IPlane *_pPlane )
{
	pPlane = _pPlane;
	pPath = _pPath;

	fSpeed = fabs( pPlane->GetSpeedB2() );
	fProgress = 0;
	
	CalcPoint();
	CalcSpeed();
	CalcNormale();
}
void CManuver::CalcSpeed()
{
	vSpeed = pPath->GetTangent( fProgress );
	Normalize( &vSpeed );
	vSpeed *= fSpeed;
}
void CManuver::CalcPoint()
{
	vCenter = pPath->GetPoint( fProgress );
}
void CManuver::CalcNormale()
{
	vNormal = pPath->GetNormale( fProgress );
	Normalize( &vNormal );
}
bool CManuver::GetToHorisontalOffset( const CVec3 &vSpeed, const float _fTurnRadius, const float fHeight, CVec3 *pManuverPos ) const
{
	const float fAlpha = asinf( vSpeed.z / fabs( vSpeed ) );
	const float fSinAHalf = sinf( 0.5f * fAlpha );
	const float fCrit = 2 * _fTurnRadius * sqr( fSinAHalf );
	
	if ( (vSpeed.z < 0 && fCrit >= fHeight - SPlanesConsts::MIN_HEIGHT) || vSpeed.z > 0 )
	{
		const float fHorDist = _fTurnRadius * fSinAHalf * ( 1.0f + cosf( fAlpha ) );
		CVec2 vSpeed2D( vSpeed.x, vSpeed.y );
		Normalize( &vSpeed2D );
		*pManuverPos = CVec3( vSpeed2D * fHorDist, -fCrit * Sign( vSpeed.z ) );
		return true;
	}
	return false;
}
bool CManuver::AdvanceCommon( const NTimer::STime timeDiff )
{
	fProgress += fSpeed * timeDiff;
	float fDz = vCenter.z;
	CalcPoint();
	
	fDz -= vCenter.z;

	fSpeed += 2 * g * fDz / fSpeed;

	CalcSpeed();
	CVec3 vTmp = vNormal;
	CalcNormale();
	if ( vNormal == VNULL3 )
		vNormal = vTmp;

	if ( vSpeed.z < 0 ) // plane is currently diving
	{
		CVec3 vOffset;
		if ( GetToHorisontalOffset( vSpeed, pPlane->GetPreferencesB2().GetR( fSpeed ), vCenter.z, &vOffset ) )
		{
			CPathFractionArcLine3D * pNewPath = new CPathFractionArcLine3D;
			pNewPath->Init( vCenter, vSpeed, vCenter + vOffset, pPlane->GetPreferencesB2().GetR( fSpeed ) );
		}
	}
	else if ( fSpeed <= pPlane->GetPreferencesB2().GetStallSpeed() ) 
	{
		
	}

	
	return fProgress + fSpeed * timeDiff >= pPath->GetLength();
}
void CManuverSteepClimb::Init( const enum EManuverDestination dest, interface IPlane *_pPlane, interface IPlane *pEnemy )
{
	NI_ASSERT_T( EMD_MANUVER_DEPENDENT == dest, "CANNOT DO GORKA ANYWERE OTHER THEN EMD_MANUVER_DEPENDENT" );
	Init( _pPlane );
}
void CManuverSteepClimb::Init( interface IPlane *pPos )
{
	const CPlanePreferences &pref = pPos->GetPreferencesB2();

	CPathFractionArcLine3D *pNewPath = new CPathFractionArcLine3D ;
	
	const CVec3 vPos( pPos->GetPosB2() );
	CVec3 vSpeed ( pPos->GetSpeedB2() );
	CVec2 vHorSpeed( vSpeed.x, vSpeed.y );

	float fPathLength = pref.GetR( fabs(vSpeed) ) * 2.0f; 
	Normalize( &vHorSpeed );
	const CVec3 vDesiredPos( vPos + CVec3( fPathLength * vHorSpeed, fPathLength / 2.0f ) );
	
	pNewPath->Init( vPos, vSpeed, vDesiredPos, pref.GetR( fabs(vSpeed) ) );
	pNewPath->DoSubstitute( 0 );
	
	InitCommon( pNewPath, pPos );
}
bool CManuverSteepClimb::Advance( const NTimer::STime timeDiff )
{
	const bool bRet = AdvanceCommon( timeDiff );
	return bRet;
}
void CManuverGeneric::Init( interface IPlane *pPos, const CVec3 &vPos )
{
	const CPlanePreferences &pref = pPos->GetPreferencesB2();

	CPathFractionArcLine3D *pNewPath = new CPathFractionArcLine3D();
	pNewPath->Init( pPos->GetPosB2(), pPos->GetSpeedB2(), vPos, pref.GetR( fabs(pPos->GetSpeedB2()) ) );
	pNewPath->DoSubstitute( 0 );
	
	InitCommon( pNewPath, pPos );
}
bool CManuverGeneric::Advance( const NTimer::STime timeDiff )
{
	const bool bRet = AdvanceCommon( timeDiff );
	return bRet;
}