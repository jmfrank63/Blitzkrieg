#ifndef __ANIM_UNIT_MECH_H__
#define __ANIM_UNIT_MECH_H__
#pragma ONCE
#include "AnimUnit.h"
class CAIUnit;
class CAnimUnitMech : public IAnimUnit
{
	OBJECT_COMPLETE_METHODS( CAnimUnitMech );
	DECLARE_SERIALIZE;

	CAIUnit *pOwner;

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
	CAnimUnitMech() : pOwner( 0 ) { }
	virtual void Init( class CAIUnit *pOwner );

	virtual void AnimationSet( int nAnimation );

	virtual void Moved();
	virtual void Stopped();

	virtual void Segment();
	
	virtual void StopCurAnimation() { }
};
#endif // __ANIM_UNIT_MECH_H__
