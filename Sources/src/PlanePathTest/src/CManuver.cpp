#include "StdAfx.h"

#include "CManuver.h"
#include "IPlane.h"
#include "ComplexPathFraction.h"
#include "..\..\PlanePathTest\src\CPlanePreferences.h"
#include "..\..\PlanePathTest\src\CManuverBuilder.h"
#include "..\..\ailogic\Trigonometry.h"
/////////////////////////////////////////////////////////////////////////////
extern float g = 0.0000000983f;
BASIC_REGISTER_CLASS( CManuver );
BASIC_REGISTER_CLASS( IManuver );
BASIC_REGISTER_CLASS( CManuverSteepClimb );
BASIC_REGISTER_CLASS( CManuverGeneric );

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	SPlanesConsts
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
const float SPlanesConsts::MIN_HEIGHT = 100.0f;
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	CManuverBuilder ::
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#include "..\..\AILogic\StaticObject.h"
#include "..\..\AILogic\Mine.h"
#include "..\..\AILogic\updater.h"
extern CUpdater updater;

class CManuverVisualizeDEBUG
{
	typedef std::list< CObj<CGivenPassabilityStObject> > CMarkers;
	std::unordered_map< IPlane*, CMarkers, SDefaultPtrHash > planeMarkers;

	CPtr<IObjectsDB> pIDB;

	CGivenPassabilityStObject * Create( const CVec3 &vPos )
	{
		CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( "Mine_APers" );
		CGDBPtr<SMineRPGStats> pStats = static_cast<const SMineRPGStats *>( pIDB->GetRPGStats( pDesc ) );
		const int nDBIndex = pIDB->GetIndex( "Mine_APers" );
		CMineStaticObject *pObj = new CMineStaticObject( pStats, CVec2(vPos.x, vPos.y), nDBIndex, pStats->fMaxHP, -1, 0 );
		pObj->RegisterInWorld();
		return pObj;
	}

public:
	
	void Init( IPathFraction *_pPath, IPlane *pPlane )
	{
		std::unordered_map< IPlane*, CMarkers, SDefaultPtrHash >::iterator marker = planeMarkers.find( pPlane );
		if ( planeMarkers.end() != marker )
		{
			CMarkers &markers = marker->second;
			for ( CMarkers::iterator it = markers.begin(); it != markers.end(); ++it )
				updater.Update(	ACTION_NOTIFY_DELETED_ST_OBJ, *it );
			markers.clear();
		}
		pIDB = GetSingleton<IObjectsDB>();
		for ( float fLength = 0; fLength < _pPath->GetLength(); fLength += 10 )
		{
			planeMarkers[pPlane].push_back( Create( _pPath->GetPoint( fLength ) ) );
		}
	}
};
CManuverVisualizeDEBUG theManuverDEBUG;


/////////////////////////////////////////////////////////////////////////////
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

	//SerializeOwner( 1, &pPlane, &saver );
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	CManuver
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
const CVec3 CManuver::CalcPredictedPoint( interface IPlane *pPos, interface IPlane *pEnemy )
{
	// distance to enemy
	const float fDist = fabs( pEnemy->GetPosB2() - pPos->GetPosB2() );
	
	// speed
	const float fSpeed = fabs( pPos->GetSpeedB2() );

	// time
	const float fTime = fDist / fSpeed;

	return pEnemy->GetManuver()->GetProspectivePoint( fTime );
}
/////////////////////////////////////////////////////////////////////////////
CVec3 CManuver::GetProspectivePoint( const float fT ) const
{
	// assume that speed is constant
	const float fAdd = fT * fSpeed;
	const float fDiff = fAdd + fProgress  - pPath->GetLength();
	
	if ( fDiff > 0.0f ) // asseme that further movement is by line
		return pPath->GetEndPoint() + pPath->GetEndTangent() * fSpeed * fDiff;
	else
		return pPath->GetPoint( fAdd + fProgress );
}
/////////////////////////////////////////////////////////////////////////////
void CManuver::InitCommon( interface IPathFraction *_pPath, interface IPlane *_pPlane )
{
	pPlane = _pPlane;
	pPath = _pPath;
	theManuverDEBUG.Init( _pPath, _pPlane );

	fSpeed = fabs( pPlane->GetSpeedB2() );
	fProgress = 0;
	
	CalcPoint();
	CalcSpeed();
	CalcNormale();
	//NStr::DebugTrace( NStr::Format( "Initted(%f,%f,%f)\n", vSpeed.x, vSpeed.y,vSpeed.z ) );
}
/////////////////////////////////////////////////////////////////////////////
void CManuver::CalcSpeed()
{
	vSpeed = pPath->GetTangent( fProgress );
	Normalize( &vSpeed );
	vSpeed *= fSpeed;
	//NStr::DebugTrace( NStr::Format( "dir (%f,%f,%f)\n", vSpeed.x, vSpeed.y,vSpeed.z ) );
}
/////////////////////////////////////////////////////////////////////////////
void CManuver::CalcPoint()
{
	vCenter = pPath->GetPoint( fProgress );
}
/////////////////////////////////////////////////////////////////////////////
void CManuver::CalcNormale()
{
	vNormal = pPath->GetNormale( fProgress );
	Normalize( &vNormal );
}
/////////////////////////////////////////////////////////////////////////////
bool CManuver::GetToHorisontalOffset( const CVec3 &vSpeed, const float _fTurnRadius, const float fHeight, CVec3 *pManuverPos ) const
{
	//NI_ASSERT_T( vSpeed.z < 0, "not diving, need not check" );
	const float fAlpha = NTrg::ASin( vSpeed.z / fabs( vSpeed ) );
	const float fSinAHalf = NTrg::Sin( 0.5f * fAlpha );
	const float fCrit = 2 * _fTurnRadius * sqr( fSinAHalf );
	
	if ( (vSpeed.z < 0 && fCrit >= fHeight - SPlanesConsts::MIN_HEIGHT) || vSpeed.z > 0 )
	{
		const float fHorDist = _fTurnRadius * fSinAHalf * ( 1.0f + NTrg::Cos( fAlpha ) );
		CVec2 vSpeed2D( vSpeed.x, vSpeed.y );
		Normalize( &vSpeed2D );
		*pManuverPos = CVec3( vSpeed2D * fHorDist, -fCrit * Sign( vSpeed.z ) );
		return true;
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////
bool CManuver::AdvanceCommon( const NTimer::STime timeDiff )
{
	fProgress += fSpeed * timeDiff;
	float fDz = vCenter.z;
	CalcPoint();
	
	fDz -= vCenter.z;

	//CRAP{ IMPLEMENT PREFERENCES
	fSpeed += 2 * g * fDz / fSpeed;
	//CRAP}

	CalcSpeed();
	CVec3 vTmp = vNormal;
	CalcNormale();
	if ( vNormal == VNULL3 )
		vNormal = vTmp;

	// check if it is time to finish diving
	if ( vSpeed.z < 0 ) // plane is currently diving
	{
		CVec3 vOffset;
		if ( GetToHorisontalOffset( vSpeed, pPlane->GetPreferencesB2().GetR( fSpeed ), vCenter.z, &vOffset ) )
		{
			// to horisontal manuver
			CPathFractionArcLine3D * pNewPath = new CPathFractionArcLine3D;
			pNewPath->Init( vCenter, vSpeed, vCenter + vOffset, pPlane->GetPreferencesB2().GetR( fSpeed ) );
		}
	}
	// check if it is time to gain speed
	else if ( fSpeed <= pPlane->GetPreferencesB2().GetStallSpeed() ) 
	{
		// to horisontal manuver
		
	}

	
	return fProgress + fSpeed * timeDiff >= pPath->GetLength();
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	CManuverSteepClimb
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void CManuverSteepClimb::Init( const enum EManuverDestination dest, interface IPlane *_pPlane, interface IPlane *pEnemy )
{
	NI_ASSERT_T( EMD_MANUVER_DEPENDENT == dest, "CANNOT DO GORKA ANYWERE OTHER THEN EMD_MANUVER_DEPENDENT" );
	//CRAP{ SOME DIFFERENCES BASED ON DISTANCE WILL BE GOOD
	Init( _pPlane );
	//CRAP}
}
/////////////////////////////////////////////////////////////////////////////
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
/////////////////////////////////////////////////////////////////////////////
bool CManuverSteepClimb::Advance( const NTimer::STime timeDiff )
{
	// determine if it is circle path fraction or not.
	const bool bRet = AdvanceCommon( timeDiff );
	return bRet;
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	CManuverGeneric::
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void CManuverGeneric::Init( interface IPlane *pPos, const CVec3 &vPos )
{
	const CPlanePreferences &pref = pPos->GetPreferencesB2();

	CPathFractionArcLine3D *pNewPath = new CPathFractionArcLine3D();
	pNewPath->Init( pPos->GetPosB2(), pPos->GetSpeedB2(), vPos, pref.GetR( fabs(pPos->GetSpeedB2()) ) );
	pNewPath->DoSubstitute( 0 );
	
	InitCommon( pNewPath, pPos );
}
/////////////////////////////////////////////////////////////////////////////
bool CManuverGeneric::Advance( const NTimer::STime timeDiff )
{
	// determine if it is circle path fraction or not.
	const bool bRet = AdvanceCommon( timeDiff );
	return bRet;
}