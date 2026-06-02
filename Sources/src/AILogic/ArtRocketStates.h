#ifndef __ART_ROCKET_STATES_H__
#define __ART_ROCKET_STATES_H__

#pragma ONCE
#include "StatesFactory.h"
#include "UnitStates.h"
class CArtRocketStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CArtRocketStatesFactory );

	static CPtr<CArtRocketStatesFactory> pFactory;
public:
	static IStatesFactory* Instance();

	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );

	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	friend class CStaticMembers;
};
class CArtRocketAttackGroundState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CArtRocketAttackGroundState );
	DECLARE_SERIALIZE;
	
	enum EAttackGroundState { EAGS_ROTATING, EAGS_FIRING };
	EAttackGroundState eState;

	class CArtillery *pArtillery;

	CVec2 point;
	bool bFired;
	bool bFinished;
	WORD wDirToRotate;

public:
	static IUnitState* Instance( class CArtillery *pArtillery, const CVec2 &point );

	CArtRocketAttackGroundState() : pArtillery( 0 ) { }
	CArtRocketAttackGroundState( class CArtillery *pArtillery, const CVec2 &point );

	virtual void Segment();
	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const { return point; }

	virtual bool IsAttacksUnit() const { return false; }
	virtual class CAIUnit* GetTargetUnit() const { return 0; }
};
#endif // __ART_ROCKET_STATES_H__
