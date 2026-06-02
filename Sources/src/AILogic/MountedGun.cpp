#include "stdafx.h"

#include "MountedGun.h"
#include "Turret.h"
#include "GunsInternal.h"
#include "Building.h"
CCommonMountedGun::CCommonMountedGun( CBuilding *_pObject, CMountedTurret *_pTurret, const int _nSlot )
: nSlot( _nSlot ), pBuildingStats( static_cast<const SBuildingRPGStats*>(_pObject->GetStats()) ), pTurret( _pTurret ), pBuilding( _pObject )
{
}
const SBaseGunRPGStats& CCommonMountedGun::GetGun() const
{
	return pBuildingStats->slots[nSlot].gun;
}
const SWeaponRPGStats* CCommonMountedGun::GetWeapon() const
{
	return GetGun().pWeapon;
}
CTurret* CCommonMountedGun::GetTurret() const
{
	return pTurret;
}
void CCommonMountedGun::GetInfantryShotInfo( SAINotifyInfantryShot *pInfantryShotInfo, const NTimer::STime &time ) const
{
	pInfantryShotInfo->nSlot = nSlot;
	pInfantryShotInfo->pObj = pBuilding;
	pInfantryShotInfo->pWeapon = GetWeapon();
	pInfantryShotInfo->time = time;
	pInfantryShotInfo->typeID = -1;
}
CBasicGun* CMountedGunsFactory::CreateGun( const EGunTypes eType, const int nShell, SCommonGunInfo *pCommonGunInfo ) const
{
	return new CMountedGun<CBaseGun>( pBuilding, pMountedTurret, nCommonGun, nShell, pCommonGunInfo, eType );
}
