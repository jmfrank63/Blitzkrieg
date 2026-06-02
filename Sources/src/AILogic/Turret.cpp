#include "stdafx.h"

#include "Turret.h"
#include "UpdatableObject.h"
#include "Updater.h"
#include "BasePathUnit.h"
#include "AIUnit.h"
#include "Building.h"
#include "Guns.h"
#include "Diplomacy.h"
extern CUpdater updater;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
BASIC_REGISTER_CLASS( CTurret );
CTurret::CTurret( const WORD wHorRotationSpeed, const WORD wVerRotationSpeed, bool _bReturnToNULLVerAngle )
{
	SetUniqueId();
	
	hor.wRotationSpeed = wHorRotationSpeed;
	hor.wCurAngle = 0;
	hor.wFinalAngle = 0;
	hor.bFinished = true;

	ver.wRotationSpeed = wVerRotationSpeed;
	ver.wCurAngle = 16384 * 3;
	hor.wFinalAngle = 16384 * 3;
	ver.bFinished = true;

	bVerAiming = wVerRotationSpeed > 0;
	
	wDefaultHorAngle = 0;
	bReturnToNULLVerAngle = _bReturnToNULLVerAngle;

	bCanReturn = false;
}
void CTurret::SetTurnParameters( SRotating *pRotateInfo, const WORD wAngle, const bool bInstantly )
{
	if ( !bInstantly )
	{
		const WORD wRotateAngle = DirsDifference( wAngle, pRotateInfo->wCurAngle );
		pRotateInfo->sign = ( WORD( wAngle - pRotateInfo->wCurAngle ) == wRotateAngle ) ? 1 : -1;

		pRotateInfo->startTime = curTime;
		pRotateInfo->endTime = curTime + wRotateAngle / pRotateInfo->wRotationSpeed;

		pRotateInfo->wFinalAngle = wAngle;
		
		pRotateInfo->bFinished = false;
	}
	else
	{
		pRotateInfo->sign = 1;
		pRotateInfo->startTime = pRotateInfo->endTime = curTime;
		pRotateInfo->wCurAngle = pRotateInfo->wFinalAngle = wAngle;
		pRotateInfo->bFinished = true;
	}
}
bool CTurret::TurnHor( const WORD wHorAngle, const bool bInstantly )
{
	hor.wCurAngle = GetHorCurAngle();

	SetTurnParameters( &hor, wHorAngle, bInstantly );

	bCanReturn = false;

	return true;
}
bool CTurret::TurnVer( const WORD wVerAngle, const bool bInstantly )
{
	if ( bVerAiming )
	{
		ver.wCurAngle = GetVerCurAngle();

		SetTurnParameters( &ver, wVerAngle, bInstantly );
		bCanReturn = false;

		return true;
	}

	return false;
}
void CTurret::Turn( const WORD wHorAngle, const WORD wVerAngle, const bool bInstantly )
{
	TurnHor( wHorAngle, bInstantly );
	TurnVer( wVerAngle, bInstantly );
}
WORD CTurret::ConstraintAngle( const WORD wDesAngle, const WORD wTurnConstraint ) const
{
	if ( DirsDifference( wDesAngle, 0 ) > wTurnConstraint )
	{
		if ( DirsDifference( wDesAngle, wTurnConstraint ) < DirsDifference( wDesAngle, -wTurnConstraint ) )
			return wTurnConstraint;
		else
			return -wTurnConstraint;
	}
	else
		return wDesAngle;
}
int GetSignOfTurn( const WORD wStartAngle, const WORD wFinishAngle )
{
	const WORD wRotateAngle = DirsDifference( wStartAngle, wFinishAngle );
	return ( WORD( wFinishAngle - wStartAngle ) == wRotateAngle ) ? 1 : -1;
}
void CTurret::Segment()
{
	if ( !hor.bFinished && curTime >= hor.endTime )
		hor.bFinished = true, hor.wCurAngle = hor.wFinalAngle;

	if ( !ver.bFinished && curTime >= ver.endTime )
		ver.bFinished = true, ver.wCurAngle = ver.wFinalAngle;
/*	
	if ( !hor.bFinished && GetHorCurAngle() > GetHorTurnConstraint() || 
			 !ver.bFinished && GetVerCurAngle() > GetVerTurnConstraint() )
		StopTurning();		
*/
	if ( pTracedUnit != 0 )
	{
		if ( !IsValidObj( pTracedUnit ) || !pTracedUnit->IsVisible( GetOwnerParty() ) ||
				  fabs2( GetOwnerCenter() - pTracedUnit->GetCenter() ) > sqr( SAIConsts::TILE_SIZE * 60 ) )
			pTracedUnit = 0;
		else 
		{
			const CVec2 toTracedUnit( pTracedUnit->GetCenter() - GetOwnerCenter() );

			const WORD wDesHorAngle = ConstraintAngle( GetDirectionByVector( toTracedUnit ) - GetOwnerFrontDir(), GetHorTurnConstraint() );
			if ( hor.bFinished || GetSignOfTurn( GetHorCurAngle(), wDesHorAngle ) != hor.sign )
				TurnHor( wDesHorAngle );

			if ( bVerAiming )
			{
				const WORD wDesVerAngle = ConstraintAngle( GetZAngle( toTracedUnit, pTracedUnit->GetZ() - GetOwnerZ() ), GetVerTurnConstraint() );
				if ( ver.bFinished || GetSignOfTurn( GetVerCurAngle(), wDesVerAngle ) != ver.sign )
					TurnVer( wDesVerAngle + 16384 * 3 );
			}
		}
	}
	else if ( bCanReturn && IsOwnerOperable() )
	{
		bCanReturn = false;

		if ( bReturnToNULLVerAngle )
			Turn( wDefaultHorAngle, 16384 * 3 );
	}
}
WORD CTurret::GetCurAngle( const SRotating &rotateInfo ) const 
{
	if ( rotateInfo.bFinished )
		return rotateInfo.wCurAngle; 
	else
	{
		if ( curTime >= rotateInfo.endTime )
			return rotateInfo.wFinalAngle;
		else
			return rotateInfo.wCurAngle + rotateInfo.sign * rotateInfo.wRotationSpeed * ( curTime - rotateInfo.startTime );
	}
}
void CTurret::TraceAim( CAIUnit *pUnit, CBasicGun *pGun )
{
	if ( !IsLocked( pGun ) )
		pTracedUnit = pUnit;
}
void CTurret::StopTracing()
{
	if ( pTracedUnit != 0 )
	{
		pTracedUnit = 0;
		StopTurning();
	}
}
void CTurret::StopHorTurning()
{
	TurnHor( GetHorCurAngle() );
}
void CTurret::StopVerTurning()
{
	TurnVer( GetVerCurAngle() );
}
void CTurret::StopTurning()
{
	StopHorTurning();
	StopVerTurning();
}
void CTurret::Lock( const CBasicGun *pGun ) 
{ 
	pLockingGun = const_cast<CBasicGun*>(pGun); 
}
void CTurret::Unlock( const CBasicGun *pGun ) 
{ 
	if ( pLockingGun != 0 && ( !pLockingGun->IsValid() || !pLockingGun->IsAlive() ) ) 
		pLockingGun = 0; 
	if ( pGun == pLockingGun ) pLockingGun = 0; 
}
bool CTurret::IsLocked( const CBasicGun *pGun ) 
{ 
	return IsValidObj( pLockingGun ) && pLockingGun != pGun; 
}
void CTurret::SetCanReturn() 
{ 
	if ( bReturnToNULLVerAngle && GetVerCurAngle() != 16384 * 3 || GetHorCurAngle() != 0 )
		bCanReturn = true; 
}
CUnitTurret::CUnitTurret( CAIUnit *_pOwner, const int _nModelPart, const DWORD _dwGunCarriageParts, const WORD wHorRotationSpeed, const WORD wVerRotationSpeed, const WORD _wHorConstraint, const WORD _wVerConstraint )
: CTurret( wHorRotationSpeed, wVerRotationSpeed, _pOwner->GetStats()->type != RPG_TYPE_ART_AAGUN ),
	pOwner( _pOwner ), nModelPart( _nModelPart ), dwGunCarriageParts( _dwGunCarriageParts ),
	wHorConstraint( _wHorConstraint ), wVerConstraint( _wVerConstraint ), bCanRotateTurret( true )
{
}
WORD CUnitTurret::GetHorTurnConstraint() const 
{ 
	if ( !bCanRotateTurret )
		return 0;
	else
		return wHorConstraint;
}
bool CUnitTurret::TurnHor( const WORD wHorAngle, const bool bInstantly )
{
	if ( CTurret::TurnHor( wHorAngle, bInstantly ) )
	{
		updater.Update( ACTION_NOTIFY_TURRET_HOR_TURN, this );
		return true;
	}

	return false;
}
bool CUnitTurret::TurnVer( const WORD wVerAngle, const bool bInstantly )
{
	if ( CTurret::TurnVer( wVerAngle, bInstantly ) )
	{
		updater.Update( ACTION_NOTIFY_TURRET_VERT_TURN, this );
		return true;
	}

	return false;
}
void CUnitTurret::GetHorTurretTurnInfo( SAINotifyTurretTurn *pTurretTurn )
{ 
	pTurretTurn->pObj = pOwner;
	pTurretTurn->endTime = GetHorEndTime();
	pTurretTurn->nModelPart = DWORD(nModelPart) | 0xffffff00;
	pTurretTurn->wAngle = GetHorFinalAngle();
}
void CUnitTurret::GetVerTurretTurnInfo( SAINotifyTurretTurn *pTurretTurn )
{
	pTurretTurn->pObj = pOwner;
	pTurretTurn->endTime = GetVerEndTime();
	pTurretTurn->nModelPart = dwGunCarriageParts;
	pTurretTurn->wAngle = GetVerFinalAngle() + 16384;
}
CVec2 CUnitTurret::GetOwnerCenter()
{
	return pOwner->GetCenter();
}
WORD CUnitTurret::GetOwnerFrontDir()
{
	return pOwner->GetFrontDir();
}
float CUnitTurret::GetOwnerZ()
{
	return pOwner->GetZ();
}
bool CUnitTurret::IsOwnerOperable() const
{
	return pOwner->IsOperable();
}
bool CUnitTurret::IsAlive() const
{
	return pOwner->IsAlive();
}
const int CUnitTurret::GetOwnerParty() const
{
	return pOwner->GetParty();
}
BASIC_REGISTER_CLASS( CMountedTurret );
CMountedTurret::CMountedTurret( CBuilding *pBuliding, const int nSlot )
:	CTurret( static_cast<const SBuildingRPGStats*>(pBuliding->GetStats())->slots[nSlot].wRotationSpeed, 0, true ),
	center( pBuliding->GetCenter().x + static_cast<const SBuildingRPGStats*>(pBuliding->GetStats())->slots[nSlot].vPos.x, pBuliding->GetCenter().y + static_cast<const SBuildingRPGStats*>(pBuliding->GetStats())->slots[nSlot].vPos.y ),
	dir( static_cast<const SBuildingRPGStats*>(pBuliding->GetStats())->slots[nSlot].wDirection ),
	wHorTurnConstraint( static_cast<const SBuildingRPGStats*>(pBuliding->GetStats())->slots[nSlot].wAngle ),
	wVerTurnConstraint( 16384 )
{
}
WORD CMountedTurret::GetHorTurnConstraint() const
{
	return wHorTurnConstraint;
}
WORD CMountedTurret::GetVerTurnConstraint() const
{
	return wVerTurnConstraint;
}
bool CMountedTurret::IsAlive() const
{
	return pBuilding->IsAlive();
}
const int CMountedTurret::GetOwnerParty() const
{
	return theDipl.GetNParty( pBuilding->GetPlayer() );
}
