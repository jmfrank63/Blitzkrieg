#include "StdAfx.h"

#include "AnimUnit.h"
#include "AIUnit.h"
#include "Updater.h"
extern CUpdater updater;
extern NTimer::STime curTime;
BASIC_REGISTER_CLASS( CAnimUnit );
CAnimUnit::CAnimUnit( CAIUnit *_pOwner )
: pOwner( _pOwner ), bTechnics( !_pOwner->GetStats()->IsInfantry() ),
	nCurAnimation( ANIMATION_IDLE ), timeOfFinishAnimation( 0 )
{
}
void CAnimUnit::Moved()
{
	if ( movingState.state == SMovingState::EMS_STOPPED || movingState.state == SMovingState::EMS_MOVING_TO_STOPPED )
	{
		movingState.state = SMovingState::EMS_STOPPED_TO_MOVING;
		movingState.timeOfIntentionStart = curTime;
	}
}
void CAnimUnit::Stopped()
{
	if ( movingState.state == SMovingState::EMS_MOVING || movingState.state == SMovingState::EMS_STOPPED_TO_MOVING )
	{
		movingState.state = SMovingState::EMS_MOVING_TO_STOPPED;
		movingState.timeOfIntentionStart = curTime;
	}
}
void CAnimUnit::AnimationSet( int nAnimation, int nLength )
{
	nCurAnimation = nAnimation;

	const int nStatsAnimLength = pOwner->GetStats()->GetAnimTime( nAnimation );

	timeOfFinishAnimation = 
		( nLength == -1 || nStatsAnimLength == -1 ) ? 0 : curTime + nStatsAnimLength;
}
void CAnimUnit::Segment()
{
	if ( pOwner->IsAlive() )
	{
		if ( movingState.state == SMovingState::EMS_STOPPED_TO_MOVING || movingState.state == SMovingState::EMS_MOVING_TO_STOPPED )
		{
			if ( movingState.timeOfIntentionStart + 200 < curTime )
			{
				movingState.state = 
					movingState.state == SMovingState::EMS_STOPPED_TO_MOVING ? SMovingState::EMS_MOVING : SMovingState::EMS_STOPPED;

				if ( bTechnics || (nCurAnimation != ANIMATION_LIE && nCurAnimation != ANIMATION_STAND) )
				{
					if ( movingState.state == SMovingState::EMS_STOPPED )
					{
						updater.Update( ACTION_NOTIFY_STOP, pOwner );
						updater.Update( pOwner->GetIdleAction(), pOwner );
					}
					else
						updater.Update( pOwner->GetMovingAction(), pOwner );

					timeOfFinishAnimation = 0;
				}
			}
		}

		if ( (nCurAnimation == ANIMATION_LIE || nCurAnimation == ANIMATION_STAND) && curTime >= timeOfFinishAnimation )
		{
			if ( movingState.state == SMovingState::EMS_MOVING || movingState.state == SMovingState::EMS_STOPPED_TO_MOVING )
			{
				updater.Update( pOwner->GetMovingAction(), pOwner );
				movingState.state = SMovingState::EMS_MOVING;
			}
			else
			{
				updater.Update( ACTION_NOTIFY_STOP, pOwner );				
				updater.Update( pOwner->GetIdleAction(), pOwner );
				movingState.state = SMovingState::EMS_STOPPED;
			}
		}
	}
}
