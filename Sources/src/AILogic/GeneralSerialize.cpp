#include "stdafx.h"

#include "GeneralInternal.h"
#include "SerializeOwner.h"
#include "GeneralTasks.h"
#include "GeneralAirForce.h"
#include "GeneralArtillery.h"
#include "EnemyRememberer.h"
#include "GeneralIntendant.h"
#include "GeneralConsts.h"
#include "CommonUnit.h"
int CSupremeBeing::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &generals );
	saver.Add( 2, &ironmans );
	return 0;
}
int CCommander::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &tasks );
	saver.Add( 2, &fMeanSeverity );
	return 0;
}
int CGeneral::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	if ( saver.IsReading() )
	{
		SGeneralConsts::Init();
	}
	saver.Add( 1, &nParty );
	saver.Add( 3, &infantryInTrenches );
	saver.Add( 4, &infantryFree );
	saver.Add( 5, &tanksFree );
	saver.Add( 6, &stationaryTanks );
	saver.Add( 7, &mobileReinforcementGroupIDs );
	saver.AddTypedSuper( 8, static_cast<CCommander*>(this) );
	saver.Add( 9, &pAirForce );
	saver.Add( 10, &pGeneralArtillery );

	saver.Add( 15, &lastBombardmentCheck );
	saver.Add( 17, &cBombardmentType );
	saver.Add( 18, &transportsFree );
	saver.Add( 19, &timeNextUpdate );
	saver.Add( 20, &pIntendant );
	saver.Add( 21, &requestIDs );
	saver.Add( 22, &requestedTasks );
	saver.Add( 23, &enemys );
	saver.Add( 24, &antiAviation );
	saver.Add( 25, &bSendReserves );

	if ( saver.IsReading() )
	{
		for ( Tasks::iterator it = tasks.begin(); it != tasks.end(); ++it )
			(*it)->SetEnemyConatiner( this );

		pAirForce->SetEnemyContainer( this );
		pGeneralArtillery->SetEnemyContainer( this );

		curProcessed = enemys.begin();
	}

	saver.Add( 26, &resContainer );

	return 0;
}
int CGeneralTaskToDefendPatch::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;


	saver.Add( 2, &patchInfo );
	saver.Add( 3, &nCurReinforcePoint );
			
	saver.Add( 4, &fSeverity );
	saver.Add( 5, &fEnemyForce );
	saver.Add( 6, &fFriendlyForce );
	saver.Add( 7, &fMaxSeverity );
	saver.Add( 10, &infantryInTrenches );
	saver.Add( 11, &infantryFree );
	saver.Add( 12, &tanksMobile );
	saver.Add( 13, &stationaryUnits );
	saver.Add( 14, &nRequestForGunPlaneID );
	saver.Add( 15, &enemyForces );
	saver.Add( 16, &fFriendlyMobileForce );
	SerializeOwner( 17, &pOwner, &saver );

	saver.Add( 18, &bFinished );
	saver.Add( 19, &bWaitForFinish );
	saver.Add( 20, &timeLastUpdate );
	return 0;
}
int CGeneralTaskToHoldReinforcement::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &tanksFree );
	saver.Add( 2, &patchInfo );
	saver.Add( 3, &fSeverity );
	saver.Add( 4, &nCurReinforcePoint );	
	saver.Add( 5, &unitsPositions );

	return 0;
}
int CGeneralArtilleryGoToPosition::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
		
	saver.Add( 1, &eState );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &vPos );
	saver.Add( 4, &bToReservePosition );
	saver.Add( 5, &bFinished );
	saver.Add( 6, &timeOfFinish );
	saver.Add( 7, &startTime );

	return 0;
}
int CGeneralArtilleryTask::SBombardmentUnitState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &vReservePosition );
	saver.Add( 2, &vAttackPos );
	saver.Add( 3, &pUnit );
	saver.Add( 4, &pGoToPosition );

	return 0;
}
int CGeneralArtilleryTask::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &bBombardmentFinished );
	saver.Add( 2, &vBombardmentCenter );
	saver.Add( 3, &fBombardmentRadius );
	saver.Add( 4, &timeToFinishBombardment );
	saver.Add( 5, &timeToSendAntiArtilleryAck );
	saver.Add( 6, &vAntiArtilleryAckCenter );
	saver.Add( 7, &bIsAntiArtilleryFight );
	saver.Add( 8, &startRotatingTime );
	saver.Add( 9, &eState );
	saver.Add( 10, &bombardmentUnits );
	SerializeOwner( 11, &pOwner, &saver );
	saver.Add( 12, &nCellNumber );
	saver.Add( 13, &nParty );

	return 0;
}
int CGeneralArtillery::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &nParty );
	saver.Add( 2, &freeUnits );
	saver.Add( 12, &trucks );
	saver.Add( 13, &tasks );
	SerializeOwner( 14, &pGeneralOwner, &saver );

	return 0;
}
int CGeneralAirForce::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &players );
	saver.Add( 2, &requestsID );
	saver.Add( 4, &requests );
	saver.Add( 5, &nParty );
	saver.Add( 6, &requestsID );
	saver.Add( 7, &bReservedByFighters );
	saver.Add( 8, &antiAviation );
	saver.Add( 9, &vFighterPoints );
	saver.Add( 10, &reservedTimes );
	saver.Add( 11, &timeLastCheck );
	saver.Add( 12, &timeLastFighterCheck );
	saver.Add( 13, &checkPeriod );
	saver.Add( 14, &fighterCheckPeriod );
	return 0;
}
int CGeneralAirForceLaunchFighters::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pAirForce );
	saver.Add( 2, &timeToRun );
	saver.Add( 3, &nPlayer );
	return 0;
}
int CEnemyRememberer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vPosition );
	saver.Add( 2, &timeLastSeen );
	saver.Add( 3, &timeBeforeForget );
	return 0;
}
int CResupplyCellInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 12, &resupplyCount );  
	saver.Add( 13, &cMarkedUnderSupply );
	saver.Add( 14, &fCount );
	saver.Add( 15, &resupplyInfo );
	saver.Add( 16, &vCell );

	return 0;
}
int CGeneralIntendant::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nParty );
	saver.Add( 2, &resupplyTrucks );
	saver.AddTypedSuper( 3, static_cast<CCommander*>(this) );
	SerializeOwner( 4, &pGeneral, &saver );
	saver.Add( 5, &cells );
	saver.Add( 6, &cellsWithRequests );

	saver.Add( 7, &vPositions );
	saver.Add( 8, &nCurPosition );
	saver.Add( 9, &bInitedByParcel );

	saver.Add( 11, &freeArtillery );

	return 0;
}
int CGeneralTaskToDefendStorage::CWaitForChangePlayer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pMainTask );
	saver.Add( 2, &pStorage );
	saver.Add( 3, &nParty );
	return 0;
}
int CGeneralTaskToDefendStorage::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &fSeverity );
	saver.Add( 3, &pRepairTransport );
	saver.Add( 4, &pStorage );
	saver.Add( 5, &nParty );
	saver.Add( 8, &eState );
	return 0;
}
int CGeneralTaskToSwarmToPoint::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &curResistanceToAttack  );
	saver.Add( 2, &fSeverity );
	saver.Add( 3, &fMaxSeverity );
	saver.Add( 4, &fMinSeverity );
	saver.Add( 5, &nAdditionalIterations );
	saver.Add( 6, &bFinished );
	saver.Add( 7, &bReleaseWorkers );
	saver.Add( 8, &timeNextCheck );
	saver.Add( 9, &swarmingTanks );
	saver.Add( 10, &vTanksPosition );
	saver.Add( 11, &fCurDistance );
	saver.Add( 12, &eState );
	saver.Add( 13, &vPrepearCenter );
	saver.Add( 14, &bResistanesBusyByUs );
	SerializeOwner( 16, &pOwner, &saver );

	return 0;
}
int CGeneralTaskRecaptureStorage::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &fSeverity );
	saver.Add( 2, &tanksFree );
	saver.Add( 3, &vReinforcePoint );
	saver.Add( 4, &bFinished );
	return 0;
}
int CGeneralTaskCheckCellDanger::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pCell );
	saver.Add( 2, &pTask );
	saver.Add( 3, &eResupplyType );
	saver.Add( 4, &pCommander );
	return 0;
}
int CGeneralTaskToResupplyCell::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pCell );
	saver.Add( 2, &vResupplyCenter );
	saver.Add( 3, &nParty );
	saver.Add( 4, &eResupplyType );
	saver.Add( 5, &pResupplyTransport );
	saver.Add( 6,&bFinished );
	saver.Add( 7, &fSeverity );
	saver.Add( 8, &timeNextCheck );
	SerializeOwner( 9, &pCells, &saver );
	return 0;
}
int CGeneralSwarmWaitForReady::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pGeneralTask );
	return 0;
}