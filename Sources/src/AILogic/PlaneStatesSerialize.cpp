#include "stdafx.h"

#include "PlaneStates.h"
#include "SerializeOwner.h"
int CPlaneRestState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &vertices );
	saver.Add( 2, &fHeight );
	saver.Add( 4, &curVertex );
	SerializeOwner( 5, &pPlane, &saver );
	
	return 0;
}
int CPlaneDeffensiveFire::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pOwner, &saver );
	saver.Add( 4, &timeLastBSUUpdate );
	saver.Add( 5, &pShootEstimator );
	return 0;
}
int CPlanePatrolState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pPlane, &saver );

	saver.Add( 2, &vPatrolPoints );
	saver.Add( 3, &nCurPointIndex );
	return 0;
}
int CPlaneBombState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	
	saver.Add( 1, &eState );
	saver.Add( 4, &fInitialHeight );
	
	saver.Add( 7, &fStartAttackDist );
	saver.Add( 10, &bDive );
	saver.Add( 11, &bDiveInProgress );
	saver.AddTypedSuper( 13, static_cast<CPlanePatrolState*>(this) );
	saver.Add( 14, &fFormerVerticalSpeed );
	saver.Add( 15, &bExitDive );
	saver.Add( 16, &timeOfStart );
	saver.AddTypedSuper( 17, static_cast<CPlaneDeffensiveFire*>(this) );
	return 0;
}
int CPlaneParaDropState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	saver.Add( 3, &pSquad );
	saver.Add( 4, &vLastDrop );
	saver.AddTypedSuper( 5, static_cast<CPlanePatrolState*>(this) );
	saver.AddTypedSuper( 17, static_cast<CPlaneDeffensiveFire*>(this) );
	saver.Add( 18, &nSquadNumber );
	saver.Add( 19, &nDroppingSoldier );
	return 0;
}
int CPlaneScoutState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &eState );
	saver.Add( 2, &fPatrolHeight );
	saver.AddTypedSuper( 4, static_cast<CPlanePatrolState*>(this) );
	saver.AddTypedSuper( 17, static_cast<CPlaneDeffensiveFire*>(this) );	
	saver.Add( 5, &timeOfStart );
	return 0;
}
int CPlaneLeaveState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pMoveToExitPoint );
	saver.Add( 2, &eState );
	SerializeOwner( 3, &pPlane, &saver );
	saver.AddTypedSuper( 17, static_cast<CPlaneDeffensiveFire*>(this) );
	saver.Add( 18, &nAviaType );
	return 0;
}
int CPlaneShturmovikPatrolState::CEnemyContainer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &pEnemy );
	SerializeOwner( 3, &pOwner, &saver );
	saver.Add( 4, &fTakenDamage );
	saver.Add( 5, &pBuilding );

	return 0;
}
int CPlaneShturmovikPatrolState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pPlane, &saver );
	saver.Add( 2, &eState );
	saver.Add( 6, &timeOfStart );
	saver.Add( 7, &timeOfLastPathUpdate );
	saver.Add( 8, &fStartAttackDist );
	saver.Add( 9, &vCurTargetPoint );
	saver.Add( 11, &fTurnRadius );
	saver.Add( 12, &timeLastCheck );
	saver.Add( 13, &fFinishAttckDist );
	saver.AddTypedSuper( 14, static_cast<CPlanePatrolState*>(this) );
	saver.Add( 15, &fPatrolHeight );
	saver.AddTypedSuper( 17, static_cast<CPlaneDeffensiveFire*>(this) );
	saver.Add( 18, &pShootEstimator );
	saver.Add( 20, &enemie );
	saver.Add( 21, &eCalledAs );
	return 0;
}
int CPlaneFighterPatrolState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	saver.Add( 3, &pEnemie );
	saver.Add( 5, &timeOfStart );
	saver.Add( 6, &timeOfLastPathUpdate );

	SerializeOwner( 8, &pPlane, &saver );

	saver.Add( 9, &timeLastCheck );
	saver.AddTypedSuper( 11, static_cast<CPlanePatrolState*>(this) );
	saver.AddTypedSuper( 17, static_cast<CPlaneDeffensiveFire*>(this) );


	saver.Add( 10, &fPatrolHeight );
	saver.Add( 11, &fPartolRadius );
	return 0;
}
int CPlaneFlyDeadState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pPlane, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &deadZone );
	saver.Add( 4, &fHeight );
	saver.Add( 5, &bFatality );
	saver.Add( 6, &timeStart );
	saver.Add( 7, &bExplodeInstantly );
	
	return 0;
}
