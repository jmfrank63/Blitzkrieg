#include "stdafx.h"

#include "Guns.h"
#include "GunsInternal.h"
#include "UnitGuns.h"
#include "SerializeOwner.h"
#include "AIUnit.h"
int SCommonGunInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &bFiring );
	saver.Add( 2, &nAmmo );
	saver.Add( 3, &lastShoot );
	saver.Add( 4, &nGun );

	return 0;
}
int CUnitGuns::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &guns );
	saver.Add( 2, &bCanShootToPlanes );
	saver.Add( 3, &fMaxFireRange );
	saver.Add( 4, &commonGunsInfo );
	saver.Add( 5, &gunsBegins );
	saver.Add( 6, &nCommonGuns );
	saver.Add( 7, &nMainGun );

	return 0;
}
int CMechUnitGuns::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CUnitGuns*>(this) );
	saver.Add( 2, &nFirstArtGun );

	return 0;
}
int CInfantryGuns::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CUnitGuns*>(this) );

	return 0;
}
int CBasicGun::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CLinkObject*>(this) );

	return 0;
}
int CGun::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &shootState );
	saver.Add( 4, &lastCheck );
	saver.Add( 5, &nShotsLast );
	saver.Add( 6, &target );
	saver.Add( 7, &z );
	saver.Add( 8, &pEnemy );
	saver.Add( 56, &nShellType );
	saver.Add( 59, &bAim );
	saver.Add( 60, &lastEnemyPos );
	saver.Add( 62, &bAngleLocked );
	SerializeOwner( 63, &pOwner, &saver );
	saver.Add( 64, &bCanShoot );
	saver.Add( 65, &pCommonGunInfo );
	saver.Add( 66, &bWaitForReload	);
	saver.Add( 67, &eType );
	saver.Add( 68, &bGrenade );

	saver.AddTypedSuper( 69, static_cast<CLinkObject*>(this) );
	saver.AddTypedSuper( 70, static_cast<CBasicGun*>(this) );

	saver.Add( 71, &parallelGuns );
	saver.Add( 72, &bParallelGun );
	
	saver.Add( 73, &vLastShotPoint );
	
	saver.Add( 74, &fRandom4Aim );
	saver.Add( 75, &fRandom4Relax );

	if ( saver.IsReading() )
	{
		nOwnerParty = -1;
		saver.Add( 76, &nOwnerParty );
		if ( nOwnerParty == -1 )
			nOwnerParty = pOwner->GetParty();
	}
	else
		saver.Add( 76, &nOwnerParty );
	
	return 0;
}
int CBaseGun::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CGun*>(this) );

	return 0;
}
int CTurretGun::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CGun*>(this) );
	saver.Add( 2, &wBestWayDir );
	saver.Add( 3, &pTurret );
	saver.Add( 4, &bTurnByBestWay );
	saver.Add( 5, &lastCheckTurnTime );

	return 0;
}
