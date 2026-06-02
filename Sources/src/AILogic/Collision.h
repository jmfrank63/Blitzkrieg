#ifndef __COLLISION_H__
#define __COLLISION_H__

#pragma ONCE
interface ICollision : public IRefCount
{
	enum ECollisionName { ECN_FREE, ECN_GIVE_PLACE, ECN_GIVE_PLACE_ROTATE, ECN_WAIT, ECN_STOP };
	
	virtual int GetPriority() const = 0;
	virtual interface IPath* GetPath() const = 0;
	virtual class CAIUnit* GetPushUnit() const = 0;

	virtual int FindCandidates() = 0;
	virtual bool IsSolved() = 0;

	virtual ECollisionName GetName() const = 0;

	virtual void Segment() = 0;
};
#endif // __COLLISION_H__
