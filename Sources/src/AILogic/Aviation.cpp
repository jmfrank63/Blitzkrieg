#include "stdafx.h"

#include "Aviation.h"
#include "Guns.h"
#include "UnitGuns.h"
#include "Turret.h"
#include "PlaneStates.h"
#include "ShootEstimatorInternal.h"
#include "PlanePath.h"
#include "GroupLogic.h"
#include "Updater.h"
#include "AckManager.h"
#include "CombatEstimator.h"
#include "Units.h"
#include "Statistics.h"
#include "Diplomacy.h"
#include "AIUnitInfoForGeneral.h"
#include "AIWarFog.h"
#include "Graveyard.h"

#include "TimeCounter.h"
#include "MPLog.h"
#include "AAFeedBacks.h"
extern CAAFeedBacks theAAFeedBacks;
extern CDiplomacy theDipl;
extern CStatistics theStatistics;
extern CUnits units;
extern CCombatEstimator theCombatEstimator;
extern CAckManager theAckManager;
extern CUpdater updater;
extern CGroupLogic theGroupLogic;
extern NTimer::STime curTime;
extern CGlobalWarFog theWarFog;

extern CTimeCounter timeCounter;
BASIC_REGISTER_CLASS( CAviation );
BASIC_REGISTER_CLASS( CPlanesFormation );
const CVec2 & CPlanesFormation::GetSpeedByFormationOffset( const CVec2 &vFormationOffset )
{
	CPlaneSmoothPath::SMemberInfo &cachValue = memberCache[vFormationOffset];
	if ( cachValue.lastMoveTime != curTime )
		pPath->CalculateMemberInfo( vFormationOffset, &cachValue );
	return cachValue.vSpeed;
}
CVec2 CPlanesFormation::GetPointByFormationOffset( const CVec2 &vFormationOffset )
{
	CPlaneSmoothPath::SMemberInfo &cachValue = memberCache[vFormationOffset];
	if ( cachValue.lastMoveTime != curTime )
		pPath->CalculateMemberInfo( vFormationOffset, &cachValue );
	return cachValue.vWorldPosition;
}
float CPlanesFormation::GetCurvatureRadius( const CVec2 &vFormationOffset )
{
	CPlaneSmoothPath::SMemberInfo &cachValue = memberCache[vFormationOffset];
	if ( cachValue.lastMoveTime != curTime )
		pPath->CalculateMemberInfo( vFormationOffset, &cachValue );
	return cachValue.fCurvatureRadius;
}
WORD CPlanesFormation::GetDirByFormationOffset( const CVec2 &vFormationOffset )
{
	CPlaneSmoothPath::SMemberInfo &cachValue = memberCache[vFormationOffset];
	if ( cachValue.lastMoveTime != curTime )
		pPath->CalculateMemberInfo( vFormationOffset, &cachValue );
	return cachValue.wDirection;
}
void CPlanesFormation::UpdateDirection( const CVec2 &newDir ) 
{ 
	if ( newDir != vNewDirection )
	{
		vNewDirection = newDir; 
		Normalize( &vNewDirection ); 
		wNewDirection = GetDirectionByVector( vNewDirection ); 
	}
}
void CPlanesFormation::UpdateDirection( const WORD newDir ) 
{ 
	if ( newDir != wNewDirection )
	{
		wNewDirection = newDir; 
		vNewDirection = GetVectorByDirection( wNewDirection ); 
	}
}
void CPlanesFormation::AddProcessed() 
{ 
	++nProcessed; 
}
void CPlanesFormation::SetNewPos( const CVec3 &vCenter ) 
{ 
	vNewPos = vCenter;
}
void CPlanesFormation::AddAlive() 
{ 
	++nAlive; 
}
bool CPlanesFormation::IsAllProcessed() const 
{ 
	return nProcessed >= nAlive; 
}
void CPlanesFormation::SecondSegment()
{
	nAlive = nProcessed = 0;
	vCenter2D.x = vNewPos.x;
	vCenter2D.y = vNewPos.y;
	fZ = vNewPos.z;
	wDirection = wNewDirection;
	vDirection = vNewDirection;
	pPath->ClearUnisedHistory();
}
interface ISmoothPath* CPlanesFormation::GetCurPath() const 
{ 
	return pPath; 
}
bool CPlanesFormation::SendAlongPath( interface IPath *_pPath )
{
	return pPath->Init( this, this, _pPath, false );
}
void CPlanesFormation::Init( const CVec2 &vCenter, const float _fZ, const float fTurnRadiusMin, const float fTurnRadiusMax, const WORD _wDirection, const float _fMaxSpeed, const float _fBombPointOffset )
{
	nAlive = nProcessed = 0;
	fBombPointOffset = _fBombPointOffset;

	fMaxSpeed = _fMaxSpeed;
	vCenter2D = vCenter;
	fZ = _fZ;
	vNewPos = CVec3( vCenter2D, fZ );
	wDirection = wNewDirection = _wDirection;
	vDirection = vNewDirection = GetVectorByDirection( wDirection );
	vSpeedHorVer = CVec2( fMaxSpeed, 0 );
	pPath = new CPlaneSmoothPath( fTurnRadiusMin, fTurnRadiusMax, fMaxSpeed, 0, true );
}
CAviation::~CAviation()
{
	int a = 0;
}
const SRect CAviation::GetUnitRect() const
{
	const float length = GetStats()->vAABBHalfSize.y * SConsts::BOUND_RECT_FACTOR;
	const float width = GetStats()->vAABBHalfSize.x * SConsts::BOUND_RECT_FACTOR;

	SRect unitRect;

	const CVec2 realDirVec( GetDirVector() );
	const CVec2 dirPerp( realDirVec.y, -realDirVec.x );
	const CVec2 vCenterShift( realDirVec * GetStats()->vAABBCenter.y + dirPerp * GetStats()->vAABBCenter.x );

	unitRect.InitRect( GetCenter() + vCenterShift, GetDirVector(), length, width );
	return unitRect;
}
void CAviation::Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *_pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID )
{
	wLastDir = dir;
	fFormerCurvatureSign = 0.0f;
	vNormal = V3_AXIS_Z;
	vFormerNormal = V3_AXIS_Z;
	pStats = static_cast<const SMechUnitRPGStats*>( _pStats );
	fTiltAnge = 0;
	timeLastTilt = curTime;
	vSpeedHorVer = vFormerHorVerSpeed = CVec2( pStats->fSpeed, 0 );

	CAIUnit::Init( center, z, fHP, dir, player, id, eVisType, dbID );
	lastPos = CVec3( float(1e15), float(1e15), float(1e15) );

	GetCurPath()->Init
	( 
		this, 
		new CPlanePath( CVec3(GetCenter(),GetZ()),
		CVec3(GetCenter()+GetDirVector()*100,GetZ()) ),
		true
	);
	
	vFormerDir = GetDirVector();
}
IStatesFactory* CAviation::GetStatesFactory() const 
{ 
	return CPlaneStatesFactory::Instance(); 
}
void CAviation::InitGuns()
{
	if ( pStats->platforms.size() > 1 )
	{
		const int nTurrets = pStats->platforms.size() - 1;
		turrets.resize( nTurrets );

		for ( int i = 0; i < nTurrets; ++i )
		{
			const SMechUnitRPGStats::SPlatform &platform = pStats->platforms[i+1];
			turrets[i] = new CUnitTurret( 
																		this, platform.nModelPart, platform.dwGunCarriageParts, 
																		platform.wHorizontalRotationSpeed, platform.wVerticalRotationSpeed,
																		platform.constraint.wMax, platform.constraintVertical.wMax
																	);
		}
	}

	pGuns = new CMechUnitGuns();
	pGuns->Init( this );

	SetShootEstimator( new CTankShootEstimator( this ) );
}
const WORD CAviation::GetDir() const
{
	if ( pFormation )
		return pFormation->GetDirByFormationOffset( vPlanesShift );
	return CAIUnit::GetDir();
}
float CAviation::GetPathCurvatureRadius() const
{
	if ( pFormation )
		return pFormation->GetCurvatureRadius( vPlanesShift );
	return GetCurPath()->GetCurvatureRadius();
}
const CVec2& CAviation::GetDirVector() const
{
	static CVec2 dirvec;
	if ( pFormation )
		return (dirvec = GetVectorByDirection( pFormation->GetDirByFormationOffset( vPlanesShift ) ));
	return CAIUnit::GetDirVector();
}
const WORD CAviation::GetFrontDir() const
{
	if ( pFormation )
		return pFormation->GetDirByFormationOffset( vPlanesShift );
	return CAIUnit::GetFrontDir();
}
const float CAviation::GetZ() const
{
	if ( pFormation )
		return pFormation->GetZ();
	return CAIUnit::GetZ();
}
const CVec2& CAviation::GetSpeed() const
{
	static CVec2 vSpeed;
	if ( pFormation )
		return ( vSpeed = pFormation->GetSpeedByFormationOffset( vPlanesShift ) );
	return CAIUnit::GetSpeed();
}
void CAviation::SetPlanesFormation( class CPlanesFormation *_pFormation, const CVec2 &_vShift )
{
	if ( pFormation && !_pFormation )
		UpdateDirection( GetFrontDir() );
	pFormation = _pFormation;
	vPlanesShift = _vShift;
}
CPlanesFormation * CAviation::GetPlanesFormation()
{
	return pFormation;
}
void CAviation::SecondSegment( const bool bUpdate )
{
	CAIUnit::SecondSegment( bUpdate );

	if ( pFormation )
	{
		pFormation->AddProcessed();
		if ( pFormation->IsAllProcessed() )
			pFormation->SecondSegment();
	}
}
void CAviation::Segment()
{
	CAIUnit::Segment();
	if ( !bAlive ) return;
	if ( pFormation )
		pFormation->AddAlive();
	
	/*bool bDiveBomberTilt = false;
	if ( pStats->type == RPG_TYPE_AVIA_BOMBER ) // ������ ����� ���� ����������
	{
		if ( vFormerHorVerSpeed.y < 0 && vSpeedHorVer.y < vFormerHorVerSpeed.y )
		{
			bDiveBomberTilt = true;
		}
	}*/



	const float R = GetPathCurvatureRadius();
	const float fD = SConsts::PLANE_TILT_PER_SECOND * ( curTime - timeLastTilt ) / 1000.0f;

	timeLastTilt = curTime;
	const float fMaxTilt = pStats->fTiltAngle;
	if ( R > 0 )
	{
		if( fTiltAnge < 0.0f )
			fTiltAnge = fTiltAnge < fMaxTilt ? fTiltAnge + 5 * fD : fMaxTilt ;
		else
			fTiltAnge = fTiltAnge < fMaxTilt ? fTiltAnge + 1 * fD : fMaxTilt ;
	}
	else if ( R < 0 )
	{
		if( fTiltAnge >= 3 )
			fTiltAnge  = fTiltAnge > -fMaxTilt ? fTiltAnge - 5 * fD : -fMaxTilt;
		else
			fTiltAnge  = fTiltAnge > -fMaxTilt ? fTiltAnge - 1 * fD : -fMaxTilt;
	}
	/*else if ( bDiveBomberTilt )
	{
		fTiltAnge = 60;
		int nSign = Sign( fTiltAnge );
		nSign = nSign == 0 ? 1 : nSign;
		fTiltAnge = fabs( fTiltAnge ) < 80 ? fTiltAnge + nSign * 10 * fD : nSign * 80;
	}*/
	else if ( R == 0 )
	{
		// decay the tilt toward zero at a constant rate; the original divided by
		// abs() — the INT overload — which was 0/0 = NaN for zero tilt and a
		// division by zero for |tilt| < 1, poisoning fTiltAnge with NaN/inf
		const int nTmp = Sign(fTiltAnge);
		if ( nTmp != 0 )
		{
			fTiltAnge -= 3.0f * fD * nTmp;
			if ( Sign(fTiltAnge) != nTmp )
				fTiltAnge = 0.0f;
		}
	}

	CVec3 vPerp;
	CVec2 vSpeepHor = GetSpeed();
	float fSpLengh = fabs( vSpeepHor );
	if ( fSpLengh < 1e-6f )
	{
		// degenerate horizontal speed: the divisions below were 0/0 and poisoned
		// the plane's normal with NaN; a level-flight normal is the intent
		vPerp = V3_AXIS_Z;
	}
	else if ( vSpeedHorVer.x > 0.0f )
	{
		vPerp.x = -vSpeedHorVer.y * vSpeepHor.x / fSpLengh;
		vPerp.y = -vSpeedHorVer.y * vSpeepHor.y / fSpLengh;
		vPerp.z = vSpeedHorVer.x;
	}
	else
	{
		vPerp.x = vSpeedHorVer.y * vSpeepHor.x / fSpLengh;
		vPerp.y = vSpeedHorVer.y * vSpeepHor.y / fSpLengh;
		vPerp.z = -vSpeedHorVer.x;
	}
	Normalize( &vPerp ); // ��� ������� � ��������, ������������� � ��������� ��������� ��������
	float mult = 1.0f;
	CVec3 dirToCenter( VNULL3 );
	float fCurvatureRadiusSign = /*bDiveBomberTilt ? 1 : */Sign( GetCurPath()->GetCurvatureRadius() );
	if ( fTiltAnge != 0.0f )
	{
		if ( 0.0f == fCurvatureRadiusSign )
			fCurvatureRadiusSign = fFormerCurvatureSign;

		const CVec3 dirVec ( GetDirVector(), 0 );
		dirToCenter = ( V3_AXIS_Z ^ dirVec ) * fCurvatureRadiusSign;
		const float ScalarMult = V3_CAMERA_HOR * dirToCenter ;
		if ( ScalarMult < -0.5f && Sign(fTiltAnge) == Sign(fCurvatureRadiusSign) )
			mult = 1.0f - ( ScalarMult + 0.5f ) * pStats->fTiltRatio;
		Normalize( &dirToCenter );
	}
	// can be negative or exceed 65535 for large tilt*mult; the WORD relies on
	// 65536-based angle wraparound (MSVC truncated through int implicitly)
	const WORD wCurTilt = WORD( int( fTiltAnge * mult * 65535 / 360 ) + 65535*3/4 );
	const CVec2 vCurTilt( GetVectorByDirection( wCurTilt ) );
	fFormerCurvatureSign = fCurvatureRadiusSign;

	const CVec3 vDesiredNormal  = vPerp * vCurTilt.x + fCurvatureRadiusSign * dirToCenter * vCurTilt.y ;
	vFormerNormal = vNormal;
	vNormal = vDesiredNormal;

	pGuns->Segment();
	lastPos = CVec3( GetCenter(), GetZ() );
	wLastDir = GetDir();	
	vFormerHorVerSpeed = vSpeedHorVer;
	vFormerDir = GetDirVector();
}
void CAviation::GetSpeed3( CVec3 *pSpeed ) const 
{
	CVec3 vSpd3( 0, 0 , vSpeedHorVer.y );
	CVec2 vSpeepHor = GetSpeed();
	const float fSpLengh = fabs( vSpeepHor );
	if ( 0.0f != fSpLengh )
	{
		vSpd3.x = vSpeedHorVer.x * vSpeepHor.x / fSpLengh;
		vSpd3.y = vSpeedHorVer.x * vSpeepHor.y / fSpLengh;
	}
	*pSpeed = vSpd3;
}
void CAviation::GetPlacement( SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff )
{
	pPlacement->pObj = this;
	const CVec3 vPos = CVec3( GetCenter(), GetZ() );
	CVec3 vSpeed3;
	GetSpeed3( &vSpeed3 );
	const CVec3 vDiff( vSpeed3 * timeDiff );
	const CVec3 vNewPos( vPos - vDiff );

	pPlacement->center.x = vNewPos.x;
	pPlacement->center.y = vNewPos.y;
	pPlacement->z = vNewPos.z;

	const CVec2 vAngleDiff = vFormerDir - GetDirVector();
	pPlacement->dir = GetDirectionByVector( GetDirVector() + 
		vAngleDiff * (float)timeDiff / (float)SConsts::AI_SEGMENT_DURATION );
	
	if ( vFormerNormal != VNULL3 )
	{
		const CVec3 vNormalDiff( vFormerNormal - vNormal );
		const CVec3 vCurNormal( vNormal + vNormalDiff * (float)timeDiff / (float)SConsts::AI_SEGMENT_DURATION );
		pPlacement->dwNormal =  Vec3ToDWORD( vCurNormal );
	}
	else
		pPlacement->dwNormal =  Vec3ToDWORD( V3_AXIS_Z );	

	pPlacement->fSpeed = 1.0f;
}
float CAviation::GetMaxFireRange() const
{
	return pGuns->GetMaxFireRange( this );
}
const NTimer::STime CAviation::GetNextSecondPathSegmTime() const
{
	return 0;
}
const bool CAviation::CanShootToPlanes() const 
{ 
	return pGuns->CanShootToPlanes(); 
}
void CAviation::Die( const bool fromExplosion, const float fDamage )
{
	theStatistics.UnitDead( this );

	pUnitInfoForGeneral->Die();

	
	int nParam = 0;
	if ( GetPlayer() == theDipl.GetMyNumber() )
		nParam = 1;
	else if ( GetParty() == theDipl.GetMyParty() )
		nParam = 2;
	else if ( EDI_ENEMY == theDipl.GetDiplStatus( GetParty(), theDipl.GetMyParty() ) )
		nParam = 3;
	
	if ( nParam != 0 )
		updater.AddFeedBack( SAIFeedBack( EFB_AVIA_KILLED, (nParam << 16) | GetAviationType() ) );
	
	ChangePlayer( theDipl.GetNeutralPlayer() );
	
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_FLY_DEAD), this, false );
	
	updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
	updater.Update( ACTION_NOTIFY_DEADPLANE, this );
	theAckManager.UnitDead( this );
	theAAFeedBacks.PlaneDeleted( this );
}
int CAviation::GetNGuns() const 
{ 
	return pGuns->GetNTotalGuns(); 
}
CBasicGun* CAviation::GetGun( const int n ) const 
{ 
	return pGuns->GetGun( n ); 
}
void CAviation::Disappear()
{
	PrepareToDelete();
	updater.DelUpdate( ACTION_NOTIFY_PLACEMENT, this );
	updater.DelActionUpdates( this );
	CDeadUnit *pDeadUnit = new CDeadUnit( this, 0, ACTION_NOTIFY_NONE, GetDBID(), false );
	updater.Update( ACTION_NOTIFY_DISSAPEAR_UNIT, pDeadUnit );
	theWarFog.DeleteUnit( GetID() );
	units.DeleteUnitFromMap( this );
	SetPlanesFormation( 0, VNULL2 );
	RestoreDefaultPath();
	theAAFeedBacks.PlaneDeleted( this );
}
const WORD CAviation::GetDivingAngle() const 
{ 
	return pStats->wDivingAngle == 0 ? 65535 / 16 : pStats->wDivingAngle;  
}
const WORD CAviation::GetClimbingAngle() const 
{ 
	return pStats->wClimbingAngle == 0 ? 65535 / 16 : pStats->wClimbingAngle; 
}
CBasicGun* CAviation::ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime ) 
{ 
	return pGuns->ChooseGunForStatObj( this, pObj, pTime ); 
}
int CAviation::GetMovingType() const
{
	if ( GetState() && EUSN_DIVE_BOMBING == GetState()->GetName() )
	{
		bool bDiving = static_cast<CPlaneBombState*>(GetState())->IsDiving();
		if ( bDiving )
			return MOVE_TYPE_DIVE;
	}

	return MOVE_TYPE_MOVE;
}
const float CAviation::GetSightRadius() const
{
	return GetStats()->fSight * SConsts::TILE_SIZE;
}
