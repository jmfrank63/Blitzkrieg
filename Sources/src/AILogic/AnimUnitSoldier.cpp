#include "StdAfx.h"

#include "AnimUnitSoldier.h"
#include "Soldier.h"
#include "Updater.h"
extern CUpdater updater;
extern NTimer::STime curTime;
void CAnimUnitSoldier::Init( CAIUnit *_pOwner )
{
	pOwner = checked_cast<CSoldier*>(_pOwner);
	nCurAnimation = ANIMATION_IDLE;
	timeOfFinishAnimation = 0;
	bMustFinishCurAnimation = true;
	
	pOwnerStats = checked_cast<const SInfantryRPGStats*>(pOwner->GetStats());
	bComplexAttack = !pOwnerStats->bCanAttackDown || !pOwnerStats->bCanAttackUp;
}
void CAnimUnitSoldier::Moved()
{
	if ( movingState.state == SMovingState::EMS_STOPPED || movingState.state == SMovingState::EMS_MOVING_TO_STOPPED )
	{
		movingState.state = SMovingState::EMS_STOPPED_TO_MOVING;
		movingState.timeOfIntentionStart = 0;
	}
}
void CAnimUnitSoldier::Stopped()
{
	if ( movingState.state == SMovingState::EMS_MOVING || movingState.state == SMovingState::EMS_STOPPED_TO_MOVING )
	{
		movingState.state = SMovingState::EMS_MOVING_TO_STOPPED;
		movingState.timeOfIntentionStart = curTime;
	}
}
void CAnimUnitSoldier::AnimationSet( int nAnimation )
{
	nCurAnimation = nAnimation;
	switch ( nAnimation )
	{
		case ANIMATION_IDLE:
		case ANIMATION_IDLE_DOWN:
		case ANIMATION_MOVE:
		case ANIMATION_CRAWL:
		case ANIMATION_DEATH:
		case ANIMATION_DEATH_DOWN:
		case ANIMATION_USE:
		case ANIMATION_USE_DOWN:
		case ANIMATION_POINTING:
		case ANIMATION_BINOCULARS:
		case ANIMATION_DEATH_FATALITY:
		case ANIMATION_IDLE2:
			bMustFinishCurAnimation = false;
			break;

		case ANIMATION_AIMING:
		case ANIMATION_AIMING_TRENCH:
		case ANIMATION_AIMING_DOWN:
			bMustFinishCurAnimation = true;
			timeOfFinishAnimation = curTime + pOwnerStats->GetAnimTime( nAnimation );
			timeOfFinishAnimation += 2000;
			break;
				
		case ANIMATION_SHOOT:
		case ANIMATION_SHOOT_DOWN:
			bMustFinishCurAnimation = true;
			timeOfFinishAnimation = curTime + pOwnerStats->GetAnimTime( nAnimation );
			if ( bComplexAttack )
				timeOfFinishAnimation += 800;
			break;

		case ANIMATION_SHOOT_TRENCH:
		case ANIMATION_THROW: 
		case ANIMATION_THROW_TRENCH: 
		case ANIMATION_RADIO:
		case ANIMATION_LIE:
		case ANIMATION_STAND:
		case ANIMATION_THROW_DOWN:
		case ANIMATION_PRISONING:
			bMustFinishCurAnimation = true;
			timeOfFinishAnimation = curTime + pOwnerStats->GetAnimTime( nAnimation );
			break;

		case ANIMATION_INSTALL:
		case ANIMATION_UNINSTALL:
		case ANIMATION_INSTALL_ROT:
		case ANIMATION_UNINSTALL_ROT:
		case ANIMATION_INSTALL_PUSH:
		case ANIMATION_UNINSTALL_PUSH:
			NI_ASSERT_T( false, NStr::Format( "Wrong animation for soldier (%d)", nAnimation ) );

		default:
			NI_ASSERT_T( false, NStr::Format( "Unknown animation for soldier (%d)", nAnimation ) );
	}
}
void CAnimUnitSoldier::Segment()
{
	if ( movingState.state == SMovingState::EMS_STOPPED_TO_MOVING || movingState.state == SMovingState::EMS_MOVING_TO_STOPPED )
	{
		if ( movingState.timeOfIntentionStart + 100 < curTime )
		{
			movingState.state = 
				movingState.state == SMovingState::EMS_STOPPED_TO_MOVING ? 
														 SMovingState::EMS_MOVING : SMovingState::EMS_STOPPED;

			if ( nCurAnimation == ANIMATION_MOVE || nCurAnimation == ANIMATION_CRAWL ||
				/*!bMustFinishCurAnimation || */
						movingState.state == SMovingState::EMS_MOVING && nCurAnimation != ANIMATION_STAND && nCurAnimation != ANIMATION_LIE )
			{
				bMustFinishCurAnimation = false;												
				StopCurAnimation();
			}
		}
	}

	if ( bMustFinishCurAnimation && curTime >= timeOfFinishAnimation )
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

		bMustFinishCurAnimation = false;
	}
}
void CAnimUnitSoldier::StopCurAnimation()
{
	if ( movingState.state == SMovingState::EMS_STOPPED )
	{
		updater.Update( ACTION_NOTIFY_STOP, pOwner );
		updater.Update( pOwner->GetIdleAction(), pOwner );
	}
	else
		updater.Update( pOwner->GetMovingAction(), pOwner );
}
