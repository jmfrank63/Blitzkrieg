#include "stdafx.h"

#include "SoldierStates.h"
#include "InBuildingStates.h"
#include "InEntrenchmentStates.h"
#include "InTransportStates.h"
#include "SerializeOwner.h"
int CSoldierRestState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &startTime );
	saver.Add( 4, &nextMove );
	saver.Add( 5, &bScanned );
	saver.Add( 6, &guardPoint );
	saver.Add( 7, &fDistToGuardPoint );

	return 0;
}
int CSoldierAttackState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &state );
	saver.Add( 2, &nextShootCheck );
	saver.Add( 3, &lastEnemyTile );
	saver.Add( 4, &wLastEnemyDir );
	saver.Add( 5, &lastEnemyCenter );
	saver.Add( 6, &damageToEnemyUpdater );
	SerializeOwner( 8, &pUnit, &saver );
	saver.Add( 9, &pEnemy );
	saver.Add( 10, &pGun );
	saver.Add( 11, &bAim );
	saver.Add( 12, &bFinish );
	saver.AddTypedSuper( 13, static_cast<CStandartBehaviour*>(this) );
	saver.Add( 14, &bSwarmAttack );
	saver.Add( 15, &runUpToEnemy );
	saver.Add( 16, &nEnemyParty );
	
	return 0;
}
int CSoldierMoveToState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &startTime );
	saver.Add( 3, &bWaiting );
	saver.Add( 4, &point );
	saver.AddTypedSuper( 5, static_cast<CFreeFireManager*>(this) );
	saver.Add( 6, &wDirToPoint );

	return 0;
}
int CSoldierTurnToPointState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &lastCheck );
	saver.Add( 3, &targCenter );

	return 0;
}
int CSoldierMoveByDirState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	
	return 0;
}
int CSoldierEnterState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &state );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &nEntrance );
	saver.Add( 4, &pBuilding );
	saver.Add( 5, &nEfforts );

	return 0;
}
int CSoldierEnterEntrenchmentState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &state );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &pEntrenchment );

	return 0;
}
int CSoldierAttackCommonStatObjState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCommonAttackCommonStatObjState*>(this) );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &bSwarmAttack );	
	return 0;
}
int CSoldierParadeState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );

	return 0;
}
int CSoldierPlaceMineNowState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &point );
	saver.Add( 3, &nType );
	saver.Add( 4, &beginAnimTime );

	return 0;
}
int CSoldierClearMineRadiusState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &pMine );
	saver.Add( 4, &clearCenter );
	saver.Add( 5, &beginAnimTime );

	return 0;
}
int CSoldierRestOnBoardState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pSoldier, &saver );

	return 0;
}
int CSoldierRestInEntrenchmentState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pSoldier, &saver );
	saver.Add( 2, &startTime );
	saver.AddTypedSuper( 3, static_cast<CStandartBehaviour*>(this) );

	return 0;
}
int CSoldierAttackInEtrenchState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pSoldier, &saver );
	saver.Add( 2, &pEnemy );
	saver.Add( 3, &pGun );
	saver.Add( 4, &bFinish );
	saver.Add( 5, &bAim );
	saver.Add( 6, &bSwarmAttack );
	saver.Add( 7, &damageToEnemyUpdater );
	saver.Add( 8, &nEnemyParty );

	return 0;
}
int CSoldierRestInBuildingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pSoldier, &saver );
	saver.Add( 2, &startTime );
	saver.AddTypedSuper( 3, static_cast<CStandartBehaviour*>(this) );

	return 0;
}
int CSoldierAttackInBuildingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pSoldier, &saver );
	saver.Add( 2, &pEnemy );
	saver.Add( 3, &pGun );
	saver.Add( 4, &bFinish );
	saver.Add( 5, &bAim );
	saver.Add( 6, &nEnemyParty );

	return 0;
}
int CSoldierAttackUnitInBuildingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.AddTypedSuper( 2, static_cast<CCommonAttackUnitInBuildingState*>(this) );
	saver.Add( 3, &bTriedToShootBuilding );

	return 0;
}
int CSoldierEnterTransportNowState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &eState );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &pTransport );
	saver.Add( 4, &timeLastTrajectoryUpdate );
	saver.Add( 5, &vLastTransportCenter );
	saver.Add( 6, &wLastTransportDir );

	return 0;
}
int CSoldierParaDroppingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &eState	);
	saver.Add( 3, &timeToCloseParashute );
	saver.Add( 4, &timeToOpenParashute );
	saver.Add( 5, &timeToFallWithParashute );

	saver.Add( 7, &eStateToSwitch );

	return 0;
}
int CSoldierAttackFormationState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &pTarget );
	saver.Add( 3, &bSwarmAttack );

	return 0;
}
int CSoldierIdleState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	return 0;
}
int CSoldierUseSpyglassState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pSoldier, &saver );
	saver.AddTypedSuper( 2, static_cast<CStandartBehaviour*>(this) );
	saver.Add( 3, &vPoint2Look );

	return 0;
}
int CSoldierAttackAviationState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &pPlane );
	saver.Add( 4, &bAttacking );
	
	return 0;
}
int CSoldierFireMoraleShellState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &nMoraleGun );
	saver.Add( 3, &vTarget );

	return 0;
}
int CSoldierUseState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &eState );

	return 0;
}
