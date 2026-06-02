#include "stdafx.h"

#include "FormationStates.h"
#include "SerializeOwner.h"
int CFormationRestState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.AddTypedSuper( 3, static_cast<CCommonRestState*>(this) );

	return 0;
}
int CFormationMoveToState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &startTime );
	saver.Add( 3, &bWaiting );
	saver.Add( 4, &eMoveToState );

	return 0;
}
int CFormationEnterBuildingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &state );
	saver.Add( 3, &pBuilding );
	saver.Add( 4, &nEntrance );

	return 0;
}
int CFormationEnterEntrenchmentState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &state );
	saver.Add( 3, &pEntrenchment );

	return 0;
}
int CFormationIdleBuildingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &pBuilding );

	return 0;
}
int CFormationIdleEntrenchmentState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &pEntrenchment );

	return 0;
}
int CFormationLeaveBuildingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &pBuilding );
	saver.Add( 3, &point );

	return 0;
}
int CFormationLeaveEntrenchmentState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &pEntrenchment );
	saver.Add( 3, &point );

	return 0;
}
int CFormationPlaceMine::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &point );
	saver.Add( 4, &eType );
	saver.Add( 5, &pHomeTransport );

	return 0;
}
int CFormationClearMine::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &point );

	return 0;
}
int CFormationAttackUnitState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 2, &pFormation, &saver );
	saver.Add( 3, &eState );
	saver.Add( 4, &bSwarmAttack );
	saver.Add( 5, &pEnemy );
	saver.Add( 6, &nEnemyParty );

	return 0;
}
int CFormationAttackCommonStatObjState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 2, &pFormation, &saver );
	saver.Add( 3, &eState );
	saver.Add( 4, &pObj );
	
	return 0;
}
int CFormationRotateState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );

	return 0;
}
int CFormationEnterTransportState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &pTransport );
	saver.Add( 4, &lastCheck );
	saver.Add( 5, &lastTransportPos );
	saver.Add( 6, &lastTransportDir );

	return 0;
}
int CFormationIdleTransportState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &pTransport );

	return 0;
}
int CFormationEnterTransportByCheatPathState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 3, &pTransport );
	return 0;
}
int CFormationEnterTransportNowState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &pTransport );

	return 0;
}
int CFormationServeUnitState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &eState );
	saver.Add( 2, &pHomeTransport );
	saver.Add( 5, &fWorkAccumulator );
	saver.Add( 6, &fWorkLeft );
	saver.Add( 7, &pPreferredUnit );
	return 0;
}
int CFormationRepairUnitState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );

	saver.Add( 3, &pUnitInQuiestion );
	saver.Add( 4, &vPointInQuestion ); 
	saver.Add( 5, &lastRepearTime );
	saver.AddTypedSuper( 6, static_cast<CFormationServeUnitState*>(this));
	saver.Add( 8, &fRepCost );
	saver.Add( 9, &bNearTruck );
	return 0;
}
int CFormationResupplyUnitState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 3, &pUnitInQuiestion );
	saver.Add( 4, &vPointInQuestion ); 
	saver.Add( 5, &lastResupplyTime );

	saver.AddTypedSuper( 6, static_cast<CFormationServeUnitState*>(this));
	saver.Add( 7, &pSquadInQuestion );
	saver.Add( 8, &iCurUnitInFormation );
	saver.Add( 9, &bNearTruck );
	return 0;
}
int CFormationLoadRuState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &pStorage );
	saver.Add( 3, &lastResupplyTime );
	saver.Add( 4, &nEntrance );
	return 0;
}
int CFormationCatchTransportState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &pTransportToCatch);
	saver.Add( 3, &timeLastUpdate);
	saver.Add( 4, &vEnterPoint );
	saver.Add( 5, &fResursPerSoldier );
	saver.Add( 6, &eState );
	return 0;
}
int CFormationPlaceAntitankState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pUnit, &saver );
	
	saver.Add( 2, &eState );
	
	saver.Add( 3, &fWorkAccumulator );
	saver.Add( 4, &vDesiredPoint );
	saver.Add( 5, &pAntitank );
	saver.Add( 6, &timeBuild );
	saver.Add( 7, &pHomeTransport );

	return 0;
}
int CFormationBuildLongObjectState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &eState);
	saver.Add( 5, &lastTime );
	saver.Add( 6, &unitsPreventing );
	saver.Add( 7, &pCreation );
	saver.Add( 8, &fCompletion );
	saver.Add( 9, &fWorkLeft );
	saver.Add( 10, &pHomeTransport );
	return 0;
}
int CFormationParaDropState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 3, &eState );
	return 0;
}
int CFormationUseSpyglassState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pFormation, &saver );

	return 0;
}
int CFormationAttackFormationState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &pTarget );
	saver.Add( 3, &bSwarmAttack );
	saver.Add( 4, &nEnemyParty );

	return 0;
}
int CFormationInstallMortarState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &timeInstall );
	saver.Add( 3, &pArt );
	saver.Add( 4, &nStage );
	return 0;
}
int CFormationGunCrewState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pUnit, &saver );

	saver.Add( 2, &crew );

	saver.Add( 4, &pArtillery );

	saver.Add( 5, &freeUnits );
	saver.Add( 6, &startTime );
	saver.Add( 7, &bReloadInProgress );

	saver.Add( 8, &fReloadPrice );
	saver.Add( 9, &fReloadProgress );	
	saver.Add( 10, &bSegmPriorMove );
	saver.Add( 11, &eGunOperateSubState );
	saver.Add( 12, &wTurretHorDir );
	saver.Add( 13, &wTurretVerDir );
	
	saver.Add( 16, &timeLastUpdate );
	saver.Add( 17, &eGunState );
	saver.Add( 18, &wGunTurretDir );
	saver.Add( 19, &nFormationSize	);
	saver.Add( 20, &nReloadPhaze );
	saver.Add( 21, &pStats );
	saver.Add( 22, &b360DegreesRotate );
	saver.Add( 23, &wGunBaseDir );
	saver.Add( 24, &vGunPos );
	return 0;
}
int CFormationCaptureArtilleryState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pUnit, &saver );

	saver.Add( 2, &eState );
	saver.Add( 8, &pArtillery );
	saver.Add( 9, &usedSoldiers );
	return 0;
}
int CFormationParadeState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &startTime );

	return 0;
}
int CFormationDisbandState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver )	;

	return 0;
}
int CFormationFormState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );

	return 0;
}
int CFormationWaitToFormState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	SerializeOwner( 1, &pFormation, &saver );
	saver.Add( 2, &bMain );
	saver.Add( 3, &pFormFormation );
	saver.Add( 4, &pMainSoldier );
	saver.Add( 5, &startTime );

	return 0;
}
int CCatchFormationState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	saver.Add( 2, &lastFormationPos );
	saver.Add( 3, &pLastFormationObject );
	SerializeOwner( 4, &pCatchingFormation, &saver );
	saver.Add( 5, &pFormation );
	saver.Add( 6, &lastUpdateTime );

	return 0;
}
int CFormationSwarmState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &state );
	SerializeOwner( 2, &pFormation, &saver );
	saver.Add( 3, &point );
	saver.Add( 4, &startTime );
	saver.Add( 5, &bContinue );

	return 0;
}
int CFormationRepairBridgeState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &pBridgeToRepair );
	saver.Add( 4, &pHomeTransport );
	saver.Add( 5, &bridgeSpans );
	saver.Add( 6, &fWorkDone );
	saver.Add( 7, &timeLastCheck );
	saver.Add( 8, &fWorkLeft );

	return 0;
}
int CFormationRepairBuildingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &fWorkAccumulator );
	saver.Add( 4, &lastRepearTime );
	saver.Add( 5, &pHomeTransport );
	saver.Add( 6, &pBuilding );
	saver.Add( 7, &lastRepearTime );
	saver.Add( 8, &fWorkLeft );

	return 0;
}
int CFormationEnterBuildingNowState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );

	return 0;
}
int CFormationEnterEntrenchmentNowState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pFormation, &saver );

	return 0;
}
