#ifndef __ANIM_UNIT_SOLDIER_H__
#define __ANIM_UNIT_SOLDIER_H__
#pragma ONCE
#include "AnimUnit.h"
class CAIUnit;
class CSoldier;
class CAnimUnitSoldier : public IAnimUnit
{
	OBJECT_COMPLETE_METHODS( CAnimUnitSoldier );
	DECLARE_SERIALIZE;

	CSoldier *pOwner;
	CGDBPtr<SInfantryRPGStats> pOwnerStats;
	bool bComplexAttack;

	int nCurAnimation;
	NTimer::STime timeOfFinishAnimation;
	bool bMustFinishCurAnimation;

	struct SMovingState
	{
		DECLARE_SERIALIZE;

	public:
		enum EMovingState { EMS_STOPPED, EMS_MOVING, EMS_STOPPED_TO_MOVING, EMS_MOVING_TO_STOPPED };
		EMovingState state;
		NTimer::STime timeOfIntentionStart;

		SMovingState() : state( EMS_STOPPED ), timeOfIntentionStart( 0 ) { }
	};
	SMovingState movingState;
public:
	CAnimUnitSoldier() : pOwner( 0 ) { }
	virtual void Init( CAIUnit *pOwner );

	virtual void AnimationSet( int nAnimation );

	virtual void Moved();
	virtual void Stopped();

	virtual void Segment();

	virtual void StopCurAnimation();
};
#endif // __ANIM_UNIT_INTERNAL_H__
