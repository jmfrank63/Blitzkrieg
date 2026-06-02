#ifndef __DAMAGE_TO_ENEMY_UPDATER__
#define __DAMAGE_TO_ENEMY_UPDATER__
#pragma ONCE
class CAIUnit;
class CDamageToEnemyUpdater
{
	DECLARE_SERIALIZE;

	int nTakenDamageUpdated;
	float fTakenDamagePower;
	
	CPtr<CAIUnit> pCurEnemy;

public:
	CDamageToEnemyUpdater() : nTakenDamageUpdated( 0 ), fTakenDamagePower( 0.0f ) { }

	void SetDamageToEnemy( CAIUnit *pOwner, CAIUnit *pEnemy, const DWORD dwGuns );
	void SetDamageToEnemy( class CAIUnit *pOwner, class CAIUnit *pEnemy, class CBasicGun *pGun );
	void UnsetDamageFromEnemy( class CAIUnit *pEnemy );

	const bool IsDamageUpdated() { return nTakenDamageUpdated != 0; }
};
#endif //__DAMAGE_TO_ENEMY_UPDATER__