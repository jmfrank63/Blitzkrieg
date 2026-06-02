#ifndef __OBSTACLE_H__
#define __OBSTACLE_H__

interface IObstacle : public IRefCount
{
	virtual void UpdateTakenDamagePower( const float fUpdate ) = 0;
	virtual const float GetTakenDamagePower() const = 0;
	
	virtual class CBasicGun *ChooseGunToShootToSelf( class CCommonUnit *pUnit, NTimer::STime *pTime ) = 0;

	virtual int GetPlayer() const = 0;
	virtual float GetHPPercent() const = 0;
	virtual const CVec2 GetCenter() const = 0;
	virtual bool IsAlive() const = 0;
	
	virtual void IssueUnitAttackCommand( class CCommonUnit *pUnit ) = 0;
	
	virtual bool CanDeleteByMovingOver( class CAIUnit * pUnit ) = 0;
	virtual interface IUpdatableObj *GetObject() const = 0;
};
interface IObstacleEnumerator
{
	virtual bool AddObstacle( IObstacle *pObstacle ) = 0;
};
#endif // __OBSTACLE_H__
