#include "StdAfx.h"

#include "CommonUnit.h"
#include "AIUnit.h"
#include "Guns.h"
#include "Formation.h"
#include "CommonStates.h"
#include "StaticObject.h"
#include "Diplomacy.h"
#include "Updater.h"
#include "AIStaticMap.h"
#include "AIWarFog.h"
#include "Commands.h"
#include "ShootEstimator.h"
#include "GroupLogic.h"
#include "UnitsIterators2.h"
extern CDiplomacy theDipl;
extern CUpdater updater;
extern CStaticMap theStaticMap;
extern CGlobalWarFog theWarFog;
extern NTimer::STime curTime;
extern CGroupLogic theGroupLogic;
BASIC_REGISTER_CLASS( CCommonUnit );
const float CCommonUnit::GetMaxDamage( CCommonUnit *pTarget ) const
{
	float fMaxDamage = 0;
	for ( int i = 0; i < GetNGuns(); ++i )
	{
		if ( GetGun( i )->CanBreach( pTarget ) && GetGun( i )->GetDamage() > fMaxDamage )
		 fMaxDamage = GetGun( i )->GetDamage();
	}

	return fMaxDamage;
}
CBasicGun* CCommonUnit::ChooseGunForStatObjWOTime( CStaticObject *pObj )
{
	NTimer::STime time;
	return ChooseGunForStatObj( pObj, &time );
}
void CCommonUnit::Init( const int _dbID )
{
	SetUniqueId();

	bSelectable = true;
	dbID = _dbID;
	lastBehTime = 0;
	vBattlePos = CVec2( -1.0f, -1.0f );
	fDesirableSpeed = -1.0f;
	fMinFollowingSpeed = float(1e10);
	
	bCanBeFrozenByState = false;
	bCanBeFrozenByScan = false;
	nextFreezeScan = 0;

	CGroupUnit::Init();
	CQueueUnit::Init();
}
bool CCommonUnit::CanGoToPoint( const CVec2 &point ) const
{
	if ( IsInFormation() )
	{
		CFormation *pFormation = GetFormation();
		return
			!pFormation->IsInWaitingState() ||
			pFormation->GetState()->IsAttackingState() ||
			fabs2( pFormation->GetCenter() - point ) < sqr( (float)SConsts::RADIUS_OF_FORMATION );
	}
	else if ( GetState()->GetName() == EUSN_REST )
		return fabs2( static_cast<CCommonRestState*>( GetState() )->GetGuardPoint() - point ) < sqr( SConsts::GUARD_STATE_RADIUS );

	return true;
}
void CCommonUnit::SetSelectable	( bool _bSelectable )
{
	bSelectable  = _bSelectable;
	updater.Update( ACTION_NOTIFY_SELECTABLE_CHANGED, this, IsSelectable() );
	
	if ( bSelectable )
		updater.Update( ACTION_NOTIFY_SELECTABLE_ON, this );
	else
		updater.Update( ACTION_NOTIFY_SELECTABLE_OFF, this );
}
const BYTE CCommonUnit::GetParty() const
{
	return theDipl.GetNParty( GetPlayer() );
}
void CCommonUnit::Lock( const CBasicGun *pGun ) 
{ 
	
	
	pLockingGun = const_cast<CBasicGun*>(pGun);
}
void CCommonUnit::Unlock( const CBasicGun *pGun ) 
{ 
	if ( pLockingGun != 0 && !IsValidObj( pLockingGun ) ) 
		pLockingGun = 0; 
	else if ( pGun == pLockingGun ) 
		pLockingGun = 0; 
}
bool CCommonUnit::IsLocked( const CBasicGun *pGun ) const 
{ 
	return IsValidObj( pLockingGun ) && pLockingGun != pGun; 
}
bool CCommonUnit::CanShootToUnitWoMove( class CAIUnit *_pTarget )
{
	bool bCan = false;
	int nGun = GetNGuns();
	for ( int i=0; i<nGun; ++i )
	{
		CBasicGun *pGun = GetGun( i );
		if ( pGun->CanShootToUnitWOMove( _pTarget ) )
		{
			bCan = true;
			break;
		}
	}
	return bCan;
}
void CCommonUnit::SetDesirableSpeed( const float _fDesirableSpeed )
{
	fDesirableSpeed = _fDesirableSpeed;
}
void CCommonUnit::UnsetDesirableSpeed()
{
	fDesirableSpeed = -1.0f;
}
float CCommonUnit::GetDesirableSpeed() const
{
	return fDesirableSpeed;
}
void CCommonUnit::AdjustWithDesirableSpeed( float *pfMaxSpeed ) const
{
	if ( fDesirableSpeed != -1.0f && !IsIdle() )
		*pfMaxSpeed = Min( fDesirableSpeed, *pfMaxSpeed );
}
const float CCommonUnit::GetMaxSpeedHere( const CVec2 &point, bool bAdjust ) const
{
	const float fMapPass = theStaticMap.GetPass( point );
	float fSpeed = GetMaxPossibleSpeed() * ( fMapPass + ( 1 - fMapPass ) * GetPassability() );
	if ( bAdjust )
		AdjustWithDesirableSpeed( &fSpeed );

	return fSpeed;
}
void CCommonUnit::SetFollowState( CCommonUnit *_pFollowedUnit )
{
	SendAcknowledgement( ACK_POSITIVE );
	pFollowedUnit = _pFollowedUnit;
	
	CVec2 vFromHeader = GetCenter() - pFollowedUnit->GetCenter();
	Normalize( &vFromHeader );
	vFollowShift = vFromHeader * SConsts::FOLLOW_STOP_RADIUS * 0.75f;
}
void CCommonUnit::UnsetFollowState()
{
	pFollowedUnit = 0;
}
bool CCommonUnit::IsInFollowState()
{
	if ( !IsValidObj( pFollowedUnit ) )
		pFollowedUnit = 0;

	return pFollowedUnit != 0;
}
class CCommonUnit* CCommonUnit::GetFollowedUnit() const
{
	return pFollowedUnit;
}
void CCommonUnit::FollowingByYou( CCommonUnit *pFollowingUnit )
{
	float fDist = fabs( pFollowingUnit->GetCenter() - GetCenter() );
	float fDesirableSpeed = GetSpeedForFollowing();

	if ( fDist > SConsts::FOLLOW_WAIT_RADIUS )
		fDesirableSpeed = 0.0f;
	else if ( fDist > SConsts::FOLLOW_STOP_RADIUS )
		fDesirableSpeed = Min( fDesirableSpeed, pFollowingUnit->GetSpeedForFollowing() );

	fMinFollowingSpeed = Min( fDesirableSpeed, fMinFollowingSpeed );
}
void CCommonUnit::Segment()
{
	if ( !GetState()->IsAttackingState() || !theWarFog.IsTileVisible( AICellsTiles::GetTile( GetState()->GetPurposePoint() ), GetParty() ) )
	{
		if ( fDesirableSpeed == -1.0f )
			SetDesirableSpeed( fMinFollowingSpeed );
		else
			SetDesirableSpeed( Min( fDesirableSpeed, fMinFollowingSpeed ) );
	}

	fMinFollowingSpeed = 1e10;
	CQueueUnit::Segment();
}
void CCommonUnit::FreezeSegment()
{
	if ( bCanBeFrozenByState && nextFreezeScan < curTime )
	{
		nextFreezeScan = curTime + Random( 1500, 3000 );

		if ( !IsIdle() )
			bCanBeFrozenByScan = false;
		else
		{
			CUnitsIter<0,3> iter( GetParty(), EDI_ENEMY, GetCenter(), GetTargetScanRadius() );
			bCanBeFrozenByScan = iter.IsFinished();
		}

		if ( !CanBeFrozen() )
			theGroupLogic.RegisterSegments( this, false, false );
	}
}
bool CCommonUnit::CanBeFrozen() const
{
	return bCanBeFrozenByScan && bCanBeFrozenByState;
}
void CCommonUnit::SetShootEstimator( IShootEstimator *_pShootEstimator )
{
	pShootEstimator = _pShootEstimator;
}
void CCommonUnit::ResetShootEstimator( CAIUnit *pCurEnemy, const bool bDamageUpdated, const DWORD wForbidden )
{
	pShootEstimator->Reset( pCurEnemy, bDamageUpdated, wForbidden );
}
void CCommonUnit::AddUnitToShootEstimator( CAIUnit *pUnit )
{
	pShootEstimator->AddUnit( pUnit );
}
CAIUnit* CCommonUnit::GetBestShootEstimatedUnit() const
{
	return pShootEstimator->GetBestUnit();
}
CBasicGun* CCommonUnit::GetBestShootEstimatedGun() const
{
	return pShootEstimator->GetBestGun();
}
const int CCommonUnit::GetNumOfBestShootEstimatedGun() const
{
	return pShootEstimator->GetNumberOfBestGun();
}
void CCommonUnit::SetTruck( CAIUnit *pUnit )
{
	pTruck = pUnit;
}
CAIUnit* CCommonUnit::GetTruck() const
{
	return pTruck;
}
void CCommonUnit::SetScenarioUnit( IScenarioUnit *_pScenarioUnit )
{
	pScenarioUnit = _pScenarioUnit;
}
IScenarioUnit* CCommonUnit::GetScenarioUnit() const
{
	return pScenarioUnit;
}
void CCommonUnit::FreezeByState( const bool bFreeze )
{
	if ( !IsInFollowState() )
	{
		bCanBeFrozenByState = bFreeze;

		if ( !bFreeze )
			theGroupLogic.RegisterSegments( this, false, false );
	}
}
bool CCommonUnit::IsFrozenByState() const
{
	return bCanBeFrozenByState;
}
