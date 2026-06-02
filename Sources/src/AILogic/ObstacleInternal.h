#ifndef __OBSTACLEINTERNAL_H__
#define __OBSTACLEINTERNAL_H__

#include "Obstacle.h"
class CObstacle : public IObstacle
{
	DECLARE_SERIALIZE;
	float fFirePower;
public:
	CObstacle() : fFirePower( 0 ) {  }

	virtual void UpdateTakenDamagePower( const float fUpdate ) { fFirePower += fUpdate; }
	virtual const float GetTakenDamagePower() const { return fFirePower; }

};
class CStaticObject;
class CObstacleStaticObject : public CObstacle
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CObstacleStaticObject);

	CPtr<CStaticObject> pObj;
public:
	CObstacleStaticObject() {  }
	CObstacleStaticObject( class CStaticObject *pObj ) : pObj( pObj ) {  }

	virtual class CBasicGun* ChooseGunToShootToSelf( class CCommonUnit *pUnit, NTimer::STime *pTime );
	virtual int GetPlayer() const;
	virtual float GetHPPercent() const;
	const CVec2 GetCenter() const;
	virtual bool IsAlive() const ;
	virtual void IssueUnitAttackCommand( class CCommonUnit *pUnit );
	
	virtual bool CanDeleteByMovingOver( class CAIUnit * pUnit );
	interface IUpdatableObj * GetObject() const ;
};
#endif // __OBSTACLEINTERNAL_H__
