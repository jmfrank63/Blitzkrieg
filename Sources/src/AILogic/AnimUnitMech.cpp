#include "StdAfx.h"

#include "AnimUnitMech.h"
#include "AIUnit.h"
#include "Updater.h"
extern CUpdater updater;
extern NTimer::STime curTime;
void CAnimUnitMech::Init( CAIUnit *_pOwner )
{
	pOwner = _pOwner;
}
void CAnimUnitMech::Moved()
{
	if ( movingState.state == SMovingState::EMS_STOPPED || movingState.state == SMovingState::EMS_MOVING_TO_STOPPED )
	{
		movingState.state = SMovingState::EMS_STOPPED_TO_MOVING;
		movingState.timeOfIntentionStart = curTime;
	}
}
void CAnimUnitMech::Stopped()
{
	if ( movingState.state == SMovingState::EMS_MOVING || movingState.state == SMovingState::EMS_STOPPED_TO_MOVING )
	{
		movingState.state = SMovingState::EMS_MOVING_TO_STOPPED;
		movingState.timeOfIntentionStart = curTime;
	}
}
void CAnimUnitMech::AnimationSet( int nAnimation )
{
}
void CAnimUnitMech::Segment()
{
	if ( pOwner->IsAlive() )
	{
		if ( movingState.state == SMovingState::EMS_STOPPED_TO_MOVING || movingState.state == SMovingState::EMS_MOVING_TO_STOPPED )
		{
			if ( movingState.timeOfIntentionStart + 200 < curTime )
			{
				movingState.state = 
					movingState.state == SMovingState::EMS_STOPPED_TO_MOVING ? SMovingState::EMS_MOVING : SMovingState::EMS_STOPPED;

				if ( movingState.state == SMovingState::EMS_STOPPED )
				{
					updater.Update( ACTION_NOTIFY_STOP, pOwner );
					updater.Update( pOwner->GetIdleAction(), pOwner );
				}
				else
					updater.Update( pOwner->GetMovingAction(), pOwner );
			}
		}
	}
}
