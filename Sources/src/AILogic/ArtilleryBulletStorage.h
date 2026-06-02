#ifndef __BULLETSTORAGE_H__
#define __BULLETSTORAGE_H__
#pragma ONCE
#include "StaticObject.h"
class CAIUnit;
class CArtilleryBulletStorage: public CGivenPassabilityStObject
{
	OBJECT_COMPLETE_METHODS( CArtilleryBulletStorage );
	DECLARE_SERIALIZE;

	CGDBPtr<SStaticObjectRPGStats> pStats;

	CAIUnit *pOwner;
public:
	CArtilleryBulletStorage() : pOwner( 0 ) { }
	CArtilleryBulletStorage( const SStaticObjectRPGStats* pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, CAIUnit *pOwner );

	virtual const SHPObjectRPGStats* GetStats() const { return pStats; }

	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit ) {}
	virtual void Die( const float fDamage ) { }
	virtual void TakeEditorDamage( const float fDamage ) {}
	virtual void Segment() { }

	void MoveTo( const CVec2 &newCenter );

	virtual EStaticObjType GetObjectType() const { return ESOT_ARTILLERY_BULLET_STORAGE; }
	
	virtual bool IsContainer() const { return false; }
	virtual const int GetNDefenders() const { return 0; }
	virtual class CSoldier* GetUnit( const int n ) const { return 0; }
	
	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const { return true; }

	CAIUnit* GetOwner() const { return pOwner; }
};
#endif // __BRIDGE_H__
