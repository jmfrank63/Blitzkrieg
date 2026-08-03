#include "StdAfx.h"

#include "AIUnit.h"
#include "Soldier.h"
#include "Technics.h"
#include "Artillery.h"
#include "Technics.h"
#include "SerializeOwner.h"
#include "Aviation.h"
#include "PlanePath.h"
#include "SaveDBID.h"
#include "AnimUnit.h"
int SBehaviour::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &moving );
	saver.Add( 2, &fire );

	return 0;
}
int CAIUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;	

	saver.AddTypedSuper( 1, static_cast<CCommonUnit*>(this) );
	saver.Add( 2, &id );
	saver.Add( 4, &bAlive );
	saver.Add( 5, &timeToDeath );
	saver.Add( 6, &player );
	saver.Add( 11, &fCamouflage );
	saver.Add( 12, &wVisionAngle );
	saver.Add( 13, &fSightMultiplier );
	saver.Add( 15, &fHitPoints );
	saver.Add( 16, &pPathUnit );
	saver.Add( 17, &pAntiArtillery );
	saver.Add( 18, &fMorale );
	saver.Add( 22, &pTankPit );
	saver.Add( 23, &camouflateTime );
	saver.Add( 24, &bVisibleByPlayer );
	saver.Add( 25, &fTakenDamagePower );
	saver.Add( 26, &nGrenades );
	saver.Add( 27, &targetScanRandom );
	saver.Add( 29, &timeLastmoraleUpdate );
	saver.Add( 30, &bHasMoraleSupport );
	saver.Add( 31, &wWisibility );
	saver.Add( 32, &pUnitInfoForGeneral );
	saver.Add( 33, &bUnitUnderSupply );
	
	saver.Add( 37, &fExperience );
	saver.Add( 38, &nLevel );
	saver.Add( 39, &pExpLevels );
	
	if ( saver.IsReading() )
	{
		if ( pExpLevels == 0 )
			pExpLevels = checked_cast<const SAIExpLevel*>( GetSingleton<IObjectsDB>()->GetExpLevels( RPG_TYPE_INFANTRY ) );
	}
	
	saver.Add( 40, &bFreeEnemySearch );
	saver.Add( 41, &pAnimUnit );
	saver.Add( 42, &creationTime );
	
	saver.Add( 43, &bAlwaysVisible );
	saver.Add( 46, &lastTimeOfVis );
	
	saver.Add( 47, &bRevealed );
	saver.Add( 48, &bQueredToReveal );
	saver.Add( 49, &nextRevealCheck );
	saver.Add( 50, &vPlaceOfReveal );
	saver.Add( 51, &visible4Party );

	if ( visible4Party.empty() )
		visible4Party.resize( 3 );
	
	saver.Add( 52, &nVisIndexInUnits );

	return 0;
}
int CSoldier::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CAIUnit*>(this) );
	saver.Add( 2, &eInsideType );
	saver.Add( 3, &pStats );
	saver.Add( 4, &pGuns );
	if ( !saver.IsReading() )
	{
		if ( IsFree() )
			pObjInside = 0;
	}
	SerializeOwner( 5, &pObjInside, &saver );
	saver.Add( 7, &wMinAngle );
	saver.Add( 8, &wMaxAngle );
	saver.Add( 9, &bInFirePlace );
	saver.Add( 10, &bInSolidPlace );
	saver.Add( 11, &lastHit );
	saver.Add( 12, &lastCheck );
	saver.Add( 13, &bLying );
	saver.Add( 14, &pFormation );
	saver.Add( 15, &cFormSlot );
	saver.Add( 16, &bAllowLieDown );
	saver.Add( 17, &lastMineCheck );
	saver.Add( 18, &lastDirUpdate );
	saver.Add( 19, &pMemorizedFormation );
	saver.Add( 21, &pVirtualFormation );

	saver.Add( 26, &slotInfo );
	saver.Add( 27, &nextSegmTime );
	saver.Add( 28, &timeBWSegments );
	saver.Add( 29, &nextPathSegmTime );
	saver.Add( 31, &nextLogicSegmTime );
	
	return 0;
}
int CInfantry::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CSoldier*>(this) );

	return 0;
}
int CSniper::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CSoldier*>(this) );
	saver.Add( 2, &lastVisibilityCheck );
	saver.Add( 3, &bVisible );
	saver.Add( 4, &bSneak );
	saver.Add( 5, &fCamouflageRemoveWhenShootProbability );

	return 0;
}
int CAviation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CAIUnit*>(this) ); 
	saver.Add( 2, &pStats );
	saver.Add( 3, &pGuns );
	saver.Add( 4, &turrets );
	saver.Add( 5, &fTiltAnge );
	saver.Add( 6, &lastPos );
	saver.Add( 8, &vSpeedHorVer );
	saver.Add( 9, &vNormal );
	saver.Add( 10, &fFormerCurvatureSign );
	saver.Add( 11, &vFormerNormal );
	saver.Add( 12, &wLastDir );
	saver.Add( 13, &timeLastTilt );
	saver.Add( 14, &vFormerHorVerSpeed );
	saver.Add( 15, &vFormerDir );
	saver.Add( 16, &pFormation );
	saver.Add( 17, &vPlanesShift );
	saver.Add( 18, &eAviationType );	
	return 0;
}
int CPlanesFormation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pPath );
	saver.Add( 2, &vCenter2D );
	saver.Add( 3, &fZ );
	saver.Add( 4, &fMaxSpeed );
	saver.Add( 5, &wDirection );
	saver.Add( 6, &vDirection );
	saver.Add( 7, &vSpeedHorVer );
	saver.Add( 8, &vNewPos );
	saver.Add( 9, &wNewDirection );
	saver.Add( 10, &vNewDirection );
	saver.Add( 11, &memberCache );
	saver.Add( 12, &fBombPointOffset );
	saver.Add( 13, &nProcessed );
	saver.Add( 14, &nAlive );

	if ( saver.IsReading() && pPath )
	{
		pPath->SetAviationUnit( this, this );
	}
	return 0;
}
int CTank::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CMilitaryCar*>(this) ); 
	saver.Add( 2, &bTrackDamaged );

	saver.Add( 3, &wDangerousDir );
	saver.Add( 4, &bDangerousDirSet );
	saver.Add( 5, &bDangerousDirSetInertia );
	saver.Add( 6, &nextTimeOfDangerousDirScan );
	saver.Add( 7, &lastTimeOfDangerousDirChanged );
	saver.Add( 8, &wDangerousDirUnderFire );
	saver.Add( 9, &fDangerousDamageUnderFire );

	return 0;
}
int CArtillery::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CAIUnit*>(this) ); 
	saver.Add( 2, &turrets );
	saver.Add( 3, &pStats );
	saver.Add( 4, &pGuns );
	
	saver.Add( 5, &bInstalled );
	saver.Add( 8, &installActionTime );
	saver.Add( 9, &eCurInstallAction );
	saver.Add( 10, &eNextInstallAction );
	saver.Add( 11, &pStaticPathToSend );
	saver.Add( 12, &vShift );
	saver.Add( 13, &pIPathToSend );
	
	saver.Add( 15, &fDispersionBonus );
	saver.Add( 16, &pCrew );
	saver.Add( 17, &pBulletStorage );
	saver.Add( 18, &fOperable );

	saver.Add( 19, &pSlaveTransport );
	saver.Add( 20, &bBulletStorageVisible );
	saver.Add( 21, &lastCheckToInstall );
	saver.Add( 22, &eCurrentStateOfInstall );
	saver.Add( 23, &bInstallActionInstant );
	saver.Add( 25, &nInitialPlayer );

	saver.Add( 26, &pCapturingUnit );
	saver.Add( 27, &pHookingTransport );

	int cnt = -1;
	saver.Add( 29, &cnt );
	if ( saver.IsReading() && cnt == 0 )
	{
		behUpdateDuration = SConsts::LONG_RANGE_ARTILLERY_UPDATE_DURATION;
	}
	
	saver.Add( 28, &behUpdateDuration );

	return 0;
}
int CMilitaryCar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.AddTypedSuper( 1, static_cast<CAIUnit*>(this) ); 
	saver.Add( 2, &turrets );
	saver.Add( 3, &pass );
	saver.Add( 4, &pStats );
	saver.Add( 5, &pGuns );
	saver.Add( 6, &pLockingUnit );
	saver.Add( 7, &fDispersionBonus );
	saver.Add( 8, &lastResupplyMorale );

	return 0;
}
int CAITransportUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMilitaryCar*>(this) ); 
	saver.Add( 2, &fResursUnits );
	saver.Add( 3, &pTowedArtillery );
	saver.Add( 4, &externLoaders );
	saver.Add( 5, &pTowedArtilleryCrew );
	saver.Add( 6, &pMustTow );
	
	return 0;
}
int CCommonUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CLinkObject*>(this) );
	saver.AddTypedSuper( 2, static_cast<CGroupUnit*>(this) );
	saver.AddTypedSuper( 3, static_cast<CQueueUnit*>(this) );

	saver.Add( 4, &beh );
	saver.Add( 5, &lastBehTime );
	saver.Add( 7, &pLockingGun );
	saver.Add( 9, &wReserveDir );
	saver.Add( 10, &bSelectable );
	saver.Add( 11, &fDesirableSpeed );
	saver.Add( 12, &pFollowedUnit );
	saver.Add( 14, &fMinFollowingSpeed );
	saver.Add( 15, &vFollowShift );
	saver.Add( 16, &pShootEstimator );
	saver.Add( 17, &pTruck );
	saver.Add( 18, &vBattlePos );

	saver.AddTypedSuper( 20, static_cast<IBasePathUnit*>(this) );

	if ( saver.IsReading() )
	{
		LoadDBID( &saver, 21, &dbID );

		if ( dbID == -1 )
			saver.Add( 6, &dbID );
	}
	else
		SaveDBID( &saver, 21, dbID );

	saver.Add( 22, &pScenarioUnit );
	saver.Add( 23, &bCanBeFrozenByState );
	saver.Add( 24, &bCanBeFrozenByScan );
	saver.Add( 25, &nextFreezeScan );

	return 0;
}
int CQueueUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pState );
	saver.Add( 3, &bCmdFinished );
	saver.Add( 4, &pCmdCurrent );
	saver.Add( 5, &lastChangeStateTime );

	return 0;
}
