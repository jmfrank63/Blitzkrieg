#include "StdAfx.h"

#include "TankStates.h"
#include "TechnicsStates.h"
#include "SoldierStates.h"
#include "Commands.h"
#include "ArtilleryStates.h"
#include "Technics.h"
#include "Soldier.h"
#include "Guns.h"
#include "StaticObject.h"
#include "Updater.h"
#include "Entrenchment.h"
#include "GroupLogic.h"
#include "Updater.h"
#include "AIStaticMap.h"
#include "TransportStates.h"
#include "Formation.h"
#include "ArtilleryBulletStorage.h"
extern CGroupLogic theGroupLogic;
extern NTimer::STime curTime;
extern CUpdater updater;
extern CStaticMap theStaticMap;
CPtr<CTankStatesFactory> CTankStatesFactory::pFactory = 0;

IStatesFactory* CTankStatesFactory::Instance()
{
	if ( pFactory == 0 )
		pFactory = new CTankStatesFactory();

	return pFactory;
}
bool CTankStatesFactory::CanCommandBeExecuted( CAICommand *pCommand )
{
	const EActionCommand &cmdType = pCommand->ToUnitCmd().cmdType;
	return 
		( cmdType == ACTION_COMMAND_DIE					||
			cmdType == ACTION_COMMAND_MOVE_TO			||
			cmdType == ACTION_COMMAND_ATTACK_UNIT	||
			cmdType == ACTION_COMMAND_ATTACK_OBJECT ||
			cmdType == ACTION_COMMAND_ROTATE_TO		||
			cmdType == ACTION_MOVE_BY_DIR					||
			cmdType == ACTION_COMMAND_SWARM_TO		||
			cmdType == ACTION_COMMAND_GUARD				||
			cmdType == ACTION_COMMAND_AMBUSH			||
			cmdType == ACTION_COMMAND_RANGE_AREA	||
			cmdType == ACTION_COMMAND_ART_BOMBARDMENT ||
			cmdType == ACTION_COMMAND_DISAPPEAR ||
			cmdType == ACTION_MOVE_LEAVE_TANK_PIT ||
			cmdType == ACTION_COMMAND_FOLLOW ||
			cmdType == ACTION_COMMAND_FOLLOW_NOW ||
			cmdType == ACTION_COMMAND_UNLOAD ||
			cmdType == ACTION_COMMAND_WAIT_FOR_UNITS ||
			cmdType == ACTION_COMMAND_SWARM_ATTACK_UNIT ||
			cmdType == ACTION_COMMAND_ENTRENCH_SELF ||
			cmdType == ACTION_COMMAND_SWARM_ATTACK_OBJECT  ||
			cmdType == ACTION_MOVE_WAIT_FOR_TRUCKREPAIR ||
			cmdType == ACTION_MOVE_TO_NOT_PRESIZE ||
			cmdType == ACTION_COMMAND_ROTATE_TO_DIR ||
			cmdType == ACTION_COMMAND_CHANGE_SHELLTYPE ||
			cmdType == ACTION_COMMAND_STAND_GROUND ||
			cmdType == ACTION_COMMAND_MOVE_TO_GRID
		);
}
IUnitState* CTankStatesFactory::ProduceState( class CQueueUnit *pObj, CAICommand *pCommand )
{
	NI_ASSERT_T( dynamic_cast<CTank*>( pObj ) != 0, "Wrong unit type" );
	CTank *pUnit = static_cast<CTank*>( pObj );
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();	
	IUnitState* pResult = 0;
	bool bSwarmAttack = false;
	
	switch ( cmd.cmdType )
	{		
		case ACTION_MOVE_TO_NOT_PRESIZE:
			pResult = CMoveToPointNotPresize::Instance( pUnit, cmd.vPos, cmd.fNumber );

			break;

		case ACTION_MOVE_WAIT_FOR_TRUCKREPAIR:
			pResult = CMechUnitRestState::Instance( pUnit, cmd.vPos, cmd.fNumber, 1 );

			break;
		case ACTION_COMMAND_DIE:
			NI_ASSERT_T( false, "Command to die in the queue" );

			break;
		case ACTION_MOVE_LEAVE_TANK_PIT:
			pResult = CTankPitLeaveState::Instance( pUnit );

			break;
		case ACTION_COMMAND_MOVE_TO:
			{
				pUnit->UnsetFollowState();

				if ( pUnit->IsTrackDamaged() )
					pUnit->SendAcknowledgement( pCommand, ACK_CANNOT_MOVE_TRACK_DAMAGED, !pCommand->IsFromAI() );
				else
				{
					if ( pUnit->IsInTankPit() && !pCommand->IsFromAI() )// сначала выйти из TankPit, потом поехать куда послали
					{
						theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pUnit );
						pResult = CTankPitLeaveState::Instance( pUnit );
					}
					else
						pResult = CSoldierMoveToState::Instance( pUnit, cmd.vPos );
				}
			}

			break;
		case ACTION_COMMAND_ENTRENCH_SELF:
			pResult = CSoldierEntrenchSelfState::Instance( pUnit );

			break;
		case ACTION_COMMAND_SWARM_ATTACK_UNIT:
			bSwarmAttack = true;
		case ACTION_COMMAND_ATTACK_UNIT:
			{
				if ( cmd.pObject && cmd.pObject.IsValid() )
				{
					CONVERT_OBJECT_PTR( CAIUnit, pTarget, cmd.pObject, "Wrong unit to attack" );

					if ( pTarget->IsAlive() )
					{
						if ( pTarget->GetStats()->IsInfantry() && static_cast<CSoldier*>(pTarget)->IsInBuilding() )
							pResult = CSoldierAttackUnitInBuildingState::Instance( pUnit, static_cast<CSoldier*>(pTarget), cmd.fNumber == 0, bSwarmAttack );
						else
							pResult = CMechAttackUnitState::Instance( pUnit, static_cast_ptr<CAIUnit*>( cmd.pObject ), cmd.fNumber == 0, bSwarmAttack );
					}
				}
				else
					pUnit->SendAcknowledgement( pCommand, ACK_INVALID_TARGET, !pCommand->IsFromAI() );
			}

			break;
		case ACTION_COMMAND_SWARM_ATTACK_OBJECT:
			bSwarmAttack = true;
		case ACTION_COMMAND_ATTACK_OBJECT:
			{
				CONVERT_OBJECT_PTR( CStaticObject, pStaticObj, cmd.pObject, "Wrong object to attack" );
				if ( pStaticObj->GetObjectType() == ESOT_ARTILLERY_BULLET_STORAGE )
				{
					pCommand->ToUnitCmd().cmdType = bSwarmAttack ? ACTION_COMMAND_SWARM_ATTACK_UNIT : ACTION_COMMAND_ATTACK_UNIT;
					pCommand->ToUnitCmd().pObject = static_cast<CArtilleryBulletStorage*>(pStaticObj)->GetOwner();
					pCommand->ToUnitCmd().fNumber = 0;
					pResult = ProduceState( pObj, pCommand );
				}
				else
					pResult = CSoldierAttackCommonStatObjState::Instance( pUnit, pStaticObj, bSwarmAttack );
			}

			break;

		case ACTION_COMMAND_ROTATE_TO_DIR:
		case ACTION_COMMAND_ROTATE_TO:
			if ( pUnit->IsTrackDamaged() )
			{
				pUnit->SendAcknowledgement( pCommand, ACK_CANNOT_MOVE_TRACK_DAMAGED, !pCommand->IsFromAI() );
			}
			else if ( !pUnit->NeedDeinstall() )
			{
				if ( pUnit->IsInTankPit() && !pCommand->IsFromAI() )// сначала выйти из TankPit, потом поехать куда послали
				{
					theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pUnit );
					pResult = CTankPitLeaveState::Instance( pUnit );
				}
				else
				{
					if ( cmd.cmdType == ACTION_COMMAND_ROTATE_TO )
						pResult = CSoldierTurnToPointState::Instance( pUnit, cmd.vPos );
					else
					{
						CVec2 vDir = cmd.vPos;
						Normalize( &vDir );
						pResult = CSoldierTurnToPointState::Instance( pUnit, pUnit->GetCenter() + vDir );
					}
				}
			}

			break;
		case ACTION_MOVE_BY_DIR:
			if ( !pUnit->NeedDeinstall() )
				pResult = CSoldierMoveByDirState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_SWARM_TO:
			if ( pUnit->IsTrackDamaged() )
				pUnit->SendAcknowledgement( pCommand, ACK_CANNOT_MOVE_TRACK_DAMAGED, !pCommand->IsFromAI() );
			else 
			{
				if ( pUnit->IsInTankPit() && !pCommand->IsFromAI() )// сначала выйти из TankPit, потом поехать куда послали
				{
					theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pUnit );
					pResult = CTankPitLeaveState::Instance( pUnit );
				}
				else				
					pResult = CCommonSwarmState::Instance( pUnit, cmd.vPos, cmd.fNumber );
			}
			
			break;
		case ACTION_COMMAND_GUARD:
			pResult = CMechUnitRestState::Instance( pUnit, cmd.vPos, cmd.fNumber, 0 );

			break;
		case ACTION_COMMAND_AMBUSH:
			pResult = CCommonAmbushState::Instance( pUnit );
			
			break;
		case ACTION_COMMAND_ART_BOMBARDMENT:
			if ( pUnit->GetFirstArtilleryGun() != 0 )
			{ 
				if ( pUnit->GetFirstArtilleryGun()->CanShootToPointWOMove( cmd.vPos, 0.0f ) )
					pResult = CArtilleryBombardmentState::Instance( pUnit, cmd.vPos );
				else
					pUnit->SendAcknowledgement( pCommand, pUnit->GetFirstArtilleryGun()->GetRejectReason(), !pCommand->IsFromAI() );
			}

			break;
		case ACTION_COMMAND_RANGE_AREA:
			if ( pUnit->GetFirstArtilleryGun() != 0 && pUnit->GetFirstArtilleryGun()->CanShootToPointWOMove( cmd.vPos, 0.0f ) )
				pResult = CArtilleryRangeAreaState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_FOLLOW:
			{
				CONVERT_OBJECT_PTR( CCommonUnit, pUnitToFollow, cmd.pObject, "Wrong unit to follow" );
				if ( pUnit->IsTrackDamaged() )
					pUnit->SendAcknowledgement( pCommand, ACK_CANNOT_MOVE_TRACK_DAMAGED, !pCommand->IsFromAI() );
				else
				{
					if ( pUnit->IsInTankPit() && !pCommand->IsFromAI() )// сначала выйти из TankPit, потом поехать куда послали
					{
						theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pUnit );
						pResult = CTankPitLeaveState::Instance( pUnit );
					}
					else
						pUnit->SetFollowState( pUnitToFollow );
				}
			}

			break;
		case ACTION_COMMAND_FOLLOW_NOW:
			{
				CONVERT_OBJECT_PTR( CCommonUnit, pUnitToFollow, cmd.pObject, "Not common unit in follow command" );
				pResult = CFollowState::Instance( pUnit, pUnitToFollow );
			}

			break;
		case ACTION_COMMAND_UNLOAD:
			pResult = CTransportLandState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_WAIT_FOR_UNITS:
			{
				CONVERT_OBJECT_PTR( CFormation, pFormationToWait, cmd.pObject, "Wrong unit to wait" );
				pResult = CTransportWaitPassengerState::Instance( pUnit, pFormationToWait );
			}

			break;
		case ACTION_COMMAND_CHANGE_SHELLTYPE:
			if ( pUnit->GetFirstArtilleryGun() != 0 )
				pUnit->SetActiveShellType( static_cast<SWeaponRPGStats::SShell::EDamageType>( int(cmd.fNumber)) );

			break;
		case ACTION_COMMAND_STAND_GROUND:
			pUnit->StopUnit();
			pUnit->GetBehaviour().moving = SBehaviour::EMHoldPos;

			break;
		case ACTION_COMMAND_MOVE_TO_GRID:
			pResult = CCommonMoveToGridState::Instance( pUnit, cmd.vPos, GetVectorByDirection( cmd.fNumber ) );

			break;
		default:
			NI_ASSERT_T( false, "Wrong command" );
	}

	return pResult;
}
IUnitState* CTankStatesFactory::ProduceRestState( class CQueueUnit *pUnit )
{
	return CMechUnitRestState::Instance( checked_cast<CTank*>( pUnit ), CVec2( -1, -1 ), 0 );
}
