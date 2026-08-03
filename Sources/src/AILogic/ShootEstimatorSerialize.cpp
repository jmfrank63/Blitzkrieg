#include "StdAfx.h"

#include "ShootEstimatorInternal.h"
#include "SerializeOwner.h"
int CTankShootEstimator::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pOwner, &saver );
	saver.Add( 2, &pBestUnit );
	saver.Add( 3, &pBestGun );
	saver.Add( 4, &pCurTarget );
	saver.Add( 5, &bDamageToCurTargetUpdated );
	saver.Add( 6, &fBestRating );
	saver.Add( 7, &dwForbidden );
	saver.Add( 8, &nBestGun );
	saver.Add( 9, &dwDefaultForbidden );

	saver.Add( 10, &pMosinStats );
	if ( saver.IsReading() && pMosinStats == 0 )
		pMosinStats = NGDB::GetRPGStats<SUnitBaseRPGStats>( "USSR_Mosin" );

	return 0;
}
int CSoldierShootEstimator::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pOwner, &saver );
	saver.Add( 2, &pBestUnit );
	saver.Add( 3, &pBestGun );
	saver.Add( 4, &pCurTarget );
	saver.Add( 5, &bDamageToCurTargetUpdated );
	saver.Add( 6, &fBestRating );
	saver.Add( 7, &bHasGrenades );
	saver.Add( 8, &bThrowGrenade );
	saver.Add( 9, &dwForbidden );
	saver.Add( 10, &nBestGun );

	saver.Add( 11, &pMosinStats );
	if ( saver.IsReading() && pMosinStats == 0 )
		pMosinStats = NGDB::GetRPGStats<SUnitBaseRPGStats>( "USSR_Mosin" );

	return 0;
}
int CPlaneDeffensiveFireShootEstimator::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pOwner, &saver );
	
	saver.Add( 2, &pBestUnit );
	saver.Add( 3, &pCurTarget );
	saver.Add( 4, &pGun );
	saver.Add( 5, &bDamageToCurTargetUpdated );
	saver.Add( 6, &fBestRating );
	return 0;
}
int CPlaneShturmovikShootEstimator::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pOwner, &saver );
	
	saver.Add( 2, &bestForGuns );
	saver.Add( 3, &bestForBombs );
	saver.Add( 4, &bestAviation );
	saver.Add( 5, &pCurEnemy );
	saver.Add( 6, &vCenter );
	saver.Add( 7, &pBestBuilding );
	return 0;
}
int CPlaneShturmovikShootEstimator::STargetInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &pTarget );
	saver.Add( 3, &bCanTargetShootToPlanes );
	saver.Add( 4, &bCanAttackerBreakTarget );
	saver.Add( 5, &wSpeedDiff );
	saver.Add( 6, &fRating );
	saver.Add( 7, &wDirToTarget );
	return 0;		
}
	
