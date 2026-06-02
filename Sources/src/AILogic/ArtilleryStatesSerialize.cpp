#include "stdafx.h"

#include "ArtilleryStates.h"
#include "SerializeOwner.h"
int CArtilleryMoveToState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	SerializeOwner( 2, &pArtillery, &saver );
	saver.Add( 3, &startTime );
	saver.Add( 4, &bToFinish );
	saver.Add( 5, &pStaticPath );
	return 0;
}
int CArtilleryTurnToPointState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pArtillery, &saver );
	saver.Add( 2, &lastCheck );
	saver.Add( 3, &targCenter );
	saver.Add( 4, &eState );

	return 0;
}
int CArtilleryBombardmentState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &point );
	saver.Add( 3, &bStop );
	saver.Add( 4, &eState );

	return 0;
}
int CArtilleryRangeAreaState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &pEnemy );
	saver.Add( 4, &pObj );
	saver.Add( 5, &pGun );
	saver.Add( 6, &point );
	saver.Add( 7, &nShootsLast );
	saver.Add( 8, &lastCheck );
	saver.Add( 9, &bFinish );
	saver.Add( 10, &bFired );

	return 0;
}
int CArtilleryInstallTransportState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pArtillery, &saver );

	saver.Add( 2, &eState );
	return 0;
}
int CArtilleryUninstallTransportState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pArtillery, &saver );
	saver.Add( 2, &eState );
	return 0;
}
int CArtilleryBeingTowedState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pArtillery, &saver );
	saver.Add( 2, &pTransport );
	saver.Add( 3, &pTransport );
	saver.Add( 4, &wLastTagDir );
	saver.Add( 5, &vLastTagCenter );
	saver.Add( 6, &bInterrupted );
	saver.Add( 7, &pPath );
	saver.Add( 8, &timeLastUpdate );

	return 0;
}
int CArtilleryAttackState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	saver.Add( 2, &wDirToRotate );
	SerializeOwner( 3, &pArtillery, &saver );
	saver.Add( 4, &pEnemy );
	saver.Add( 5, &bAim );
	saver.Add( 6, &bFinish );
	saver.Add( 7, &pGun );
	saver.Add( 8, &bSwarmAttack );
	saver.Add( 9, &damageToEnemyUpdater );
	saver.AddTypedSuper( 10, static_cast<CFreeFireManager*>(this) );
	saver.Add( 11, &nEnemyParty );

	return 0;
}
int CArtilleryAttackCommonStatObjState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pArtillery, &saver );
	saver.Add( 2 ,&pObj );
	saver.Add( 3, &pGun );
	saver.Add( 4, &eState );
	saver.Add( 5, &wDirToRotate );
	saver.Add( 6, &bAim );
	saver.Add( 7, &bFinish );
	saver.AddTypedSuper( 8, static_cast<CFreeFireManager*>(this) );

	return 0;
}
int CArtilleryRestState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CMechUnitRestState*>(this) );
	SerializeOwner( 2, &pArtillery, &saver );

	return 0;
}
int CArtilleryAttackAviationState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CSoldierAttackAviationState*>(this) );
	SerializeOwner( 2, &pArtillery, &saver );

	return 0;
}
