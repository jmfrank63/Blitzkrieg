#include "StdAfx.h"

#include "CommonStates.h"
#include "SerializeOwner.h"
int CMechAttackUnitState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;	

	saver.AddTypedSuper( 1, static_cast<CFreeFireManager*>(this) );
	saver.AddTypedSuper( 2, static_cast<CStandartBehaviour*>(this) );

	saver.Add( 3, &state );
	saver.Add( 4, &lastShootCheck );
	saver.Add( 5, &lastEnemyTile );
	saver.Add( 6, &wLastEnemyDir );
	saver.Add( 7, &fProb );
	saver.Add( 8, &eAttackType );
	saver.Add( 9, &nBestSide );
	saver.Add( 10, &bTurningToBest );
	saver.Add( 11, &nBestAngle );
	saver.Add( 12, &lastEnemyCenter );
	saver.Add( 13, &damageToEnemyUpdater );
	saver.Add( 15, &pEnemy );
	saver.Add( 16, &pGun );
	saver.Add( 17, &bAim );
	saver.Add( 18, &bFinish );
	saver.Add( 19, &bSwarmAttack );
	SerializeOwner( 20, &pUnit, &saver );
	saver.Add( 21, &nEnemyParty );
	saver.Add( 22, &vEnemyCenter );
	saver.Add( 23, &wEnemyDir );

	return 0;
}
int CCommonAttackCommonStatObjState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pObj );
	saver.Add( 2, &pGun );
	saver.Add( 3, &bAim );
	saver.AddTypedSuper( 5, static_cast<CFreeFireManager*>(this) );
	saver.Add( 6, &bFinish );
	saver.Add( 7, &bSwarmAttack );
	saver.Add( 8, &nStartObjParty );

	return 0;
}
int CCommonSwarmState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &state );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &point );
	saver.Add( 4, &startTime );
	saver.Add( 5, &bContinue );

	return 0;
}
int CCommonRestState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &guardPoint );
	saver.Add( 2, &nextMove );
	saver.Add( 3, &wDir );
	saver.AddTypedSuper( 4, static_cast<CStandartBehaviour*>(this) );
	SerializeOwner( 7, &pUnit, &saver );
	saver.Add( 8, &bScanned );
	saver.Add( 9, &startMoveTime );

	return 0;
}
int CMechUnitRestState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCommonRestState*>(this) );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 4, &bFinishWhenCanMove );

	return 0;
}
int CCommonAmbushState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &startTime );
	saver.Add( 3, &lastCheckTime );
	saver.Add( 4, &pGun );
	saver.AddTypedSuper( 5, static_cast<CStandartBehaviour*>(this) );
	saver.Add( 6, &pTarget );
	saver.Add( 7, &lastVisibleCheck );
	saver.Add( 8, &eState );

	return 0;
}
int CCommonAttackUnitInBuildingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &eState );
	saver.Add( 2, &pTarget );
	saver.Add( 3, &targetCenter );
	saver.Add( 4, &pGun );
	saver.Add( 5, &bAim );
	saver.AddTypedSuper( 6, static_cast<CFreeFireManager*>(this) );
	saver.Add( 7, &bSwarmAttack );
	saver.Add( 8, &damageToEnemyUpdater );
	saver.Add( 9, &runUpToEnemy );
	saver.Add( 10, &nSlot );

	return 0;
}
int CFollowState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CStandartBehaviour*>(this) );
	SerializeOwner( 2, &pUnit, &saver );
	saver.Add( 3, &pHeadUnit );
	saver.Add( 4, &lastHeadUnitPos );
	saver.Add( 5, &lastCheck );

	return 0;
}
int CDamageToEnemyUpdater::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nTakenDamageUpdated );
	saver.Add( 2, &fTakenDamagePower );
	saver.Add( 3, &pCurEnemy );

	return 0;
}
int CCommonMoveToGridState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &vPoint );
	saver.Add( 3, &vDir );
	saver.Add( 4, &startMoveTime );
	saver.Add( 5, &eState );

	return 0;
}
