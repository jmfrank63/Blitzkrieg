#include "StdAfx.h"

#include "ArtRocketStates.h"
#include "TechnicsStates.h"
#include "Commands.h"
#include "Artillery.h"
#include "ArtilleryStates.h"
#include "SoldierStates.h"
#include "Guns.h"
#include "GroupLogic.h"
#include "Technics.h"
#include "StaticObject.h"
#include "Turret.h"
#include "ArtilleryBulletStorage.h"
extern NTimer::STime curTime;
extern CGroupLogic theGroupLogic;
CPtr<CArtRocketStatesFactory> CArtRocketStatesFactory::pFactory = 0;
IStatesFactory* CArtRocketStatesFactory::Instance()
{
	if ( pFactory == 0 )
		pFactory = new CArtRocketStatesFactory();

	return pFactory;
}
bool CArtRocketStatesFactory::CanCommandBeExecuted( CAICommand *pCommand )
{
	const EActionCommand &cmdType = pCommand->ToUnitCmd().cmdType;
	return 
		( cmdType == ACTION_COMMAND_DIE							||
			cmdType == ACTION_COMMAND_MOVE_TO					||
			cmdType == ACTION_COMMAND_ART_BOMBARDMENT	||
			cmdType == ACTION_COMMAND_ROTATE_TO				||
			cmdType == ACTION_COMMAND_INSTALL					||
			cmdType == ACTION_COMMAND_UNINSTALL				||
			cmdType == ACTION_COMMAND_DISAPPEAR				||
			cmdType == ACTION_MOVE_IDLE								||
			cmdType == ACTION_COMMAND_GUARD						||
			cmdType == ACTION_MOVE_BEING_TOWED				||
			cmdType == ACTION_COMMAND_ATTACK_OBJECT		||
			cmdType == ACTION_COMMAND_ENTRENCH_SELF		||
			cmdType == ACTION_COMMAND_CHANGE_SHELLTYPE ||
			cmdType == ACTION_MOVE_LEAVE_TANK_PIT	||
			cmdType == ACTION_COMMAND_ROTATE_TO_DIR ||
			cmdType == ACTION_COMMAND_STAND_GROUND ||
			cmdType == ACTION_COMMAND_MOVE_TO_GRID ||
			cmdType == ACTION_COMMAND_SWARM_TO
		);
}
IUnitState* CArtRocketStatesFactory::ProduceState( class CQueueUnit *pObj, CAICommand *pCommand )
{
	NI_ASSERT_T( dynamic_cast<CArtillery*>( pObj ) != 0, "Wrong unit type" );
	CArtillery *pArtillery = static_cast<CArtillery*>( pObj );

	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();	
	IUnitState* pResult = 0;
	
	switch ( cmd.cmdType )
	{
		case ACTION_MOVE_LEAVE_TANK_PIT:
			pResult = CTankPitLeaveState::Instance( pArtillery );

			break;

		case ACTION_COMMAND_ENTRENCH_SELF:
			pResult = CSoldierEntrenchSelfState::Instance( pArtillery );

			break;
		case ACTION_MOVE_BEING_TOWED:
			{
				CONVERT_OBJECT_PTR( CAITransportUnit, pTransportUnit, cmd.pObject, "Wrong unit to attach artillery" );
				pResult = CArtilleryBeingTowedState::Instance( pArtillery, pTransportUnit );
			}

			break;
		case ACTION_COMMAND_DIE:
			NI_ASSERT_T( false, "Command to die in the queue" );

			break;
		case ACTION_MOVE_IDLE:
			pResult = CSoldierIdleState::Instance( pArtillery );
			
			break;
		case ACTION_COMMAND_MOVE_TO:
			{
				pArtillery->UnsetFollowState();				
				if ( pArtillery->IsInTankPit() )// сначала выйти из TankPit, потом поехать куда послали
				{
					theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pArtillery );
					pResult = CTankPitLeaveState::Instance( pArtillery );
				}
				else 
					pResult = CArtilleryMoveToState::Instance( pArtillery, cmd.vPos );
			}

			break;
		case ACTION_COMMAND_ART_BOMBARDMENT:
			pResult = CArtRocketAttackGroundState::Instance( pArtillery, cmd.vPos );

			break;
		case ACTION_COMMAND_ROTATE_TO:
			pResult = CArtilleryTurnToPointState::Instance( pArtillery, cmd.vPos );

			break;
		case ACTION_COMMAND_ROTATE_TO_DIR:
			{
				CVec2 vDir = cmd.vPos;
				Normalize( &vDir );
				pResult = CArtilleryTurnToPointState::Instance( pArtillery, pArtillery->GetCenter() + vDir );
			}

			break;
		case ACTION_COMMAND_GUARD:
			pResult = CArtilleryRestState::Instance( pArtillery, cmd.vPos, cmd.fNumber );

			break;
		case ACTION_COMMAND_INSTALL:
			pResult = CArtilleryInstallTransportState::Instance( pArtillery );

			break;
		case ACTION_COMMAND_UNINSTALL:
			pResult = CArtilleryUninstallTransportState::Instance( pArtillery );

			break;
		case ACTION_COMMAND_ATTACK_OBJECT:
			{
				CONVERT_OBJECT_PTR( CStaticObject, pStaticObj, cmd.pObject, "Wrong static object to attack" );
				if ( pStaticObj->GetObjectType() == ESOT_ARTILLERY_BULLET_STORAGE )
				{
					pResult = CArtRocketAttackGroundState::Instance( pArtillery, static_cast<CArtilleryBulletStorage*>(pStaticObj)->GetOwner()->GetCenter() );
				}
				else
					pResult = CArtilleryAttackCommonStatObjState::Instance( pArtillery, pStaticObj );

			}

			break;
		case ACTION_COMMAND_CHANGE_SHELLTYPE:
			pArtillery->SetActiveShellType( static_cast<SWeaponRPGStats::SShell::EDamageType>( int(cmd.fNumber)) );

			break;
		case ACTION_COMMAND_STAND_GROUND:
			pArtillery->StopUnit();
			pArtillery->GetBehaviour().moving = SBehaviour::EMHoldPos;

			break;
		case ACTION_COMMAND_MOVE_TO_GRID:
			pResult = CCommonMoveToGridState::Instance( pArtillery, cmd.vPos, GetVectorByDirection( cmd.fNumber ) );

			break;
		case ACTION_COMMAND_SWARM_TO:
			if ( pArtillery->IsInTankPit() && !pCommand->IsFromAI() )
			{
				theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pArtillery );
				pResult = CTankPitLeaveState::Instance( pArtillery );
			}
			else if ( !pArtillery->CanMove() )
				pArtillery->SendAcknowledgement( pCommand, ACK_CANNOT_MOVE_NEED_TO_BE_TOWED_TO_MOVE, !pCommand->IsFromAI() );
			else 
				pResult = CCommonSwarmState::Instance( pArtillery, cmd.vPos, cmd.fNumber );
			
			break;
		default:
			NI_ASSERT_T( false, "Wrong command" );
	}

	return pResult;
}
IUnitState* CArtRocketStatesFactory::ProduceRestState( class CQueueUnit *pUnit )
{
	NI_ASSERT_T( dynamic_cast<CArtillery*>( pUnit ) != 0, "Wrong unit type" );	
	return CArtilleryRestState::Instance( static_cast<CArtillery*>( pUnit ), static_cast<CArtillery*>( pUnit )->GetCenter(), 0 );
}
IUnitState* CArtRocketAttackGroundState::Instance( CArtillery *pArtillery, const CVec2 &point )
{
	return new CArtRocketAttackGroundState( pArtillery, point );
}
CArtRocketAttackGroundState::CArtRocketAttackGroundState( CArtillery *_pArtillery, const CVec2 &_point )
: pArtillery( _pArtillery ), point( _point ), bFired( false ), eState( EAGS_FIRING ), wDirToRotate( 0 ), bFinished( false )
{
	pArtillery->StopUnit();
	NI_ASSERT_T( pArtillery->GetFirstArtilleryGun() != 0, "ArtRocket unit doesn't have an aritillery gun" );
	bool bCanShoot = pArtillery->GetFirstArtilleryGun()->CanShootToPointWOMove( point, 0 );

	if ( !bCanShoot )
	{
		if ( pArtillery->GetFirstArtilleryGun()->GetRejectReason() == ACK_NOT_IN_ATTACK_ANGLE )
		{
			const CVec2 vDirToRotate = point - pArtillery->GetCenter();
			if ( !pArtillery->CanRotateTo( pArtillery->GetUnitRect(), vDirToRotate, false, false ) )
			{
				pArtillery->SendAcknowledgement( ACK_INVALID_TARGET, true );
				bFinished = true;
			}
			else
			{
				wDirToRotate = GetDirectionByVector( vDirToRotate );
				eState = EAGS_ROTATING;
			}
		}
		else
		{
			pArtillery->SendAcknowledgement( pArtillery->GetFirstArtilleryGun()->GetRejectReason(), true );
			bFinished = true;
		}
	}
	else
		eState = EAGS_FIRING;
}
void CArtRocketAttackGroundState::Segment()
{
	NI_ASSERT_T( pArtillery->GetFirstArtilleryGun() != 0, "Rocket unit doesn't have any ballistic guns" );
	if ( bFinished )
		pArtillery->SetCommandFinished();
	else
	{
		switch ( eState )
		{
			case EAGS_ROTATING:
				if ( pArtillery->TurnToDir( wDirToRotate, false ) )
				{
					eState = EAGS_FIRING;
					pArtillery->ForceInstallAction();
				}

				break;
			case EAGS_FIRING:
				if ( pArtillery->IsInstalled() )
				{
					if ( !pArtillery->GetFirstArtilleryGun()->IsFiring() )
					{
						if ( bFired )
						{
							CBasicGun *pGun = pArtillery->GetFirstArtilleryGun();							
							CTurret *pTurret = pGun->GetTurret();
							pTurret->Unlock( pArtillery->GetFirstArtilleryGun() );
							const int nGuns = pArtillery->GetNGuns();
							for ( int i = 0; i < nGuns; ++i )
							{
								CBasicGun *pGun = pArtillery->GetGun( i );
								if ( pTurret == pGun->GetTurret() )
									pGun->StopFire();
							}

							pArtillery->SetCommandFinished();
						}
						else
						{
							CBasicGun *pGun = pArtillery->GetFirstArtilleryGun();
							pGun->GetTurret()->Lock( pGun );
							pGun->StartPointBurst( point, true );

							bFired = true;
						}
					}
					else if ( pArtillery->GetFirstArtilleryGun()->GetNAmmo() == 0 )
						pArtillery->SendAcknowledgement( ACK_NO_AMMO );
				}

				break;
			default: NI_ASSERT_T( false, NStr::Format( "Wrong CArtRocketAttackGroundState (%d)", (int)eState ) );
		}
	}
}
ETryStateInterruptResult CArtRocketAttackGroundState::TryInterruptState(class CAICommand *pCommand)
{
	if ( pCommand == 0 || !pArtillery->GetFirstArtilleryGun()->IsBursting() )
	{
		pArtillery->GetFirstArtilleryGun()->StopFire();
		pArtillery->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	return TSIR_YES_WAIT;
}
