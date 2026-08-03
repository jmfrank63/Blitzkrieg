#include "StdAfx.h"

#include "ArtilleryStates.h"
#include "TechnicsStates.h"
#include "Commands.h"
#include "Artillery.h"
#include "Guns.h"
#include "SoldierStates.h"
#include "Artillery.h"
#include "Diplomacy.h"
#include "UnitsIterators2.h"
#include "Behaviour.h"
#include "StaticObjects.h"
#include "AIStaticMap.h"
#include "GroupLogic.h"
#include "Formation.h"
#include "Soldier.h"
#include "Technics.h"
#include "Turret.h"
#include "StaticObject.h"
#include "Aviation.h"
#include "Updater.h"
#include "ArtilleryPaths.h"
#include "Entrenchment.h"
#include "ArtilleryBulletStorage.h"
#include "StaticObjectsIters.h"

#include "TimeCounter.h"
extern CGroupLogic theGroupLogic;
extern NTimer::STime curTime;
extern CStaticMap theStaticMap;
extern CDiplomacy theDipl;
extern CStaticObjects theStatObjs;
extern CUpdater updater;

extern CTimeCounter timeCounter;
CPtr<CArtilleryStatesFactory> CArtilleryStatesFactory::pFactory = 0;
IStatesFactory* CArtilleryStatesFactory::Instance()
{
	if ( pFactory == 0 )
		pFactory = new CArtilleryStatesFactory();

	return pFactory;
}
bool CArtilleryStatesFactory::CanCommandBeExecuted( CAICommand *pCommand )
{
	const EActionCommand &cmdType = pCommand->ToUnitCmd().cmdType;
	return 
		( cmdType == ACTION_COMMAND_DIE							||
			cmdType == ACTION_COMMAND_MOVE_TO					||
			cmdType == ACTION_COMMAND_ATTACK_UNIT			||
			cmdType == ACTION_COMMAND_ATTACK_OBJECT		||
			cmdType == ACTION_COMMAND_ROTATE_TO				||
			cmdType == ACTION_COMMAND_GUARD						||
			cmdType == ACTION_COMMAND_AMBUSH					||
			cmdType == ACTION_COMMAND_RANGE_AREA			||
			cmdType == ACTION_COMMAND_INSTALL					||
			cmdType == ACTION_COMMAND_UNINSTALL				||
			cmdType == ACTION_COMMAND_ART_BOMBARDMENT	||
			cmdType == ACTION_COMMAND_DISAPPEAR				||
			cmdType == ACTION_MOVE_BEING_TOWED				||
			cmdType == ACTION_COMMAND_LEAVE						||
			cmdType == ACTION_MOVE_IDLE								||
			cmdType == ACTION_COMMAND_SWARM_ATTACK_UNIT ||
			cmdType == ACTION_COMMAND_ENTRENCH_SELF ||
			cmdType == ACTION_COMMAND_CHANGE_SHELLTYPE ||
			cmdType == ACTION_MOVE_LEAVE_TANK_PIT	||
			cmdType == ACTION_COMMAND_SWARM_TO ||
			cmdType == ACTION_COMMAND_ROTATE_TO_DIR ||
			cmdType == ACTION_COMMAND_FOLLOW ||
			cmdType == ACTION_COMMAND_FOLLOW_NOW ||
			cmdType == ACTION_COMMAND_STAND_GROUND ||
			cmdType == ACTION_COMMAND_MOVE_TO_GRID
		);
}
IUnitState* CArtilleryStatesFactory::ProduceState( class CQueueUnit *pObj, CAICommand *pCommand )
{
	NI_ASSERT_T( dynamic_cast<CArtillery*>( pObj ) != 0, "Wrong unit type" );
	CArtillery *pArtillery = static_cast<CArtillery*>( pObj );

	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();	
	IUnitState* pResult = 0;
	bool bSwarmAttack = false;
	
	switch ( cmd.cmdType )
	{
		case ACTION_MOVE_LEAVE_TANK_PIT:
			pResult = CTankPitLeaveState::Instance( pArtillery );

			break;
		case ACTION_COMMAND_CHANGE_SHELLTYPE:
			pArtillery->SetActiveShellType( static_cast<SWeaponRPGStats::SShell::EDamageType>( int(cmd.fNumber)) );

			break;
		case ACTION_COMMAND_ENTRENCH_SELF:
			pResult = CSoldierEntrenchSelfState::Instance( pArtillery );

			break;
		case ACTION_COMMAND_LEAVE:
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_STOP_THIS_ACTION), pArtillery->GetCrew(), false );
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_MOVE_TO, cmd.vPos), pArtillery->GetCrew(), false );
			pArtillery->DelCrew();

			break;
		case ACTION_MOVE_IDLE:
			pResult = CSoldierIdleState::Instance( pArtillery );

			break;
		case ACTION_MOVE_BEING_TOWED:
			{
				CONVERT_OBJECT_PTR( CAITransportUnit, pTransport, cmd.pObject, "Wrong unit to attach artillery" );
				pResult = CArtilleryBeingTowedState::Instance( pArtillery, pTransport );
			}

			break;
		case ACTION_COMMAND_DIE:
			NI_ASSERT_T( false, "Command to die in the queue" );

			break;
		case ACTION_COMMAND_MOVE_TO:
			if ( pArtillery->IsInTankPit() && !pCommand->IsFromAI() )// сначала выйти из TankPit, потом поехать куда послали
			{
				theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pArtillery );
				pResult = CTankPitLeaveState::Instance( pArtillery );
			}
			else
			{
				pArtillery->UnsetFollowState();				
				pResult = CArtilleryMoveToState::Instance( pArtillery, cmd.vPos );
			}

			break;
		case	ACTION_COMMAND_SWARM_ATTACK_UNIT:
			bSwarmAttack = true;
		case ACTION_COMMAND_ATTACK_UNIT:
			if ( cmd.pObject.IsValid() )
			{
				if( !cmd.bFromAI )
					pArtillery->SetActiveShellType( SWeaponRPGStats::SShell::DAMAGE_HEALTH );

				CONVERT_OBJECT_PTR( CAIUnit, pTarget, cmd.pObject, "Wrong unit to attack" );
				if ( pTarget->IsAlive() )
				{
					if ( pArtillery->GetStats()->nUninstallRotate == 0 && pArtillery->GetStats()->nUninstallTransport == 0 )
					{
						if ( pTarget->GetStats()->IsInfantry() && static_cast<CSoldier*>(pTarget)->IsInBuilding() )
							pResult = CSoldierAttackUnitInBuildingState::Instance( pArtillery, static_cast<CSoldier*>(pTarget), cmd.fNumber == 0, bSwarmAttack );
						else
							pResult = CMechAttackUnitState::Instance( pArtillery, static_cast_ptr<CAIUnit*>( cmd.pObject ), cmd.fNumber == 0, bSwarmAttack );
					}
					else if ( pTarget->GetStats()->IsAviation() )
						pResult = CArtilleryAttackAviationState::Instance( pArtillery, static_cast<CAviation*>(cmd.pObject.GetPtr()) );
					else
						pResult = CArtilleryAttackState::Instance( pArtillery, static_cast<CAIUnit*>( cmd.pObject.GetPtr() ), cmd.fNumber == 0, bSwarmAttack );
				}
			}
			
			break;
		case ACTION_COMMAND_SWARM_ATTACK_OBJECT:
			bSwarmAttack = true;
		case ACTION_COMMAND_ATTACK_OBJECT:
			{
				CONVERT_OBJECT_PTR( CStaticObject, pStaticObj, cmd.pObject, "Wrong static object to attack" );

				if ( pArtillery->GetStats()->nUninstallRotate == 0 && pArtillery->GetStats()->nUninstallTransport == 0 )
				{
					if ( pArtillery->IsInTankPit() && !pCommand->IsFromAI() )
					{
						theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pArtillery );
						pResult = CTankPitLeaveState::Instance( pArtillery );
						break;
					}
				}

				if ( pStaticObj->GetObjectType() == ESOT_ARTILLERY_BULLET_STORAGE )
				{
					pCommand->ToUnitCmd().cmdType = ACTION_COMMAND_ATTACK_UNIT;
					pCommand->ToUnitCmd().pObject = static_cast<CArtilleryBulletStorage*>(pStaticObj)->GetOwner();
					pCommand->ToUnitCmd().fNumber = 0;

					pResult = ProduceState( pObj, pCommand );
				}
				else if ( pArtillery->GetStats()->nUninstallRotate == 0 && pArtillery->GetStats()->nUninstallTransport == 0 )
					pResult = CSoldierAttackCommonStatObjState::Instance( pArtillery, pStaticObj, bSwarmAttack );
				else
					pResult = CArtilleryAttackCommonStatObjState::Instance( pArtillery, pStaticObj );
			}

			break;
		case ACTION_COMMAND_ROTATE_TO:
			if ( pArtillery->IsInTankPit() && !pCommand->IsFromAI() && pArtillery->GetStats()->type != RPG_TYPE_ART_AAGUN )// сначала выйти из TankPit, потом поехать куда послали
			{
				theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pArtillery );
				pResult = CTankPitLeaveState::Instance( pArtillery );
			}
			else
				pResult = CArtilleryTurnToPointState::Instance( pArtillery, cmd.vPos );

			break;
		case ACTION_COMMAND_ROTATE_TO_DIR:
			if ( pArtillery->IsInTankPit() && !pCommand->IsFromAI() )// сначала выйти из TankPit, потом поехать куда послали
			{
				theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pArtillery );
				pResult = CTankPitLeaveState::Instance( pArtillery );
			}
			else
			{
				CVec2 vDir = cmd.vPos;
				Normalize( &vDir );
				pResult = CArtilleryTurnToPointState::Instance( pArtillery, pArtillery->GetCenter() + vDir );
			}

			break;
		case ACTION_COMMAND_AMBUSH:
			pResult = CCommonAmbushState::Instance( pArtillery );

			break;
		case ACTION_COMMAND_INSTALL:
			pResult = CArtilleryInstallTransportState::Instance( pArtillery );

			break;
		case ACTION_COMMAND_UNINSTALL:
			pResult = CArtilleryUninstallTransportState::Instance( pArtillery );

			break;
		case ACTION_COMMAND_ART_BOMBARDMENT:
			if ( pArtillery->GetFirstArtilleryGun() != 0 )
				pResult = CArtilleryBombardmentState::Instance( pArtillery, cmd.vPos );
			else
				pArtillery->SendAcknowledgement( pCommand, ACK_NEGATIVE, !pCommand->IsFromAI() );

			break;
		case ACTION_COMMAND_RANGE_AREA:
			if ( pArtillery->GetFirstArtilleryGun() != 0 )
				pResult = CArtilleryRangeAreaState::Instance( pArtillery, cmd.vPos );
			else
				pArtillery->SendAcknowledgement( pCommand, ACK_NEGATIVE, !pCommand->IsFromAI() );

			break;
		case ACTION_COMMAND_GUARD:
			pResult = CArtilleryRestState::Instance( pArtillery, cmd.vPos, cmd.fNumber );

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
		case ACTION_COMMAND_FOLLOW:
			{
				CONVERT_OBJECT_PTR( CCommonUnit, pCommonUnit, cmd.pObject, "Not common unit in follow command" );
				pArtillery->SetFollowState( pCommonUnit );
			}

			break;
		case ACTION_COMMAND_FOLLOW_NOW:
			{
				CONVERT_OBJECT_PTR( CCommonUnit, pCommonUnit, cmd.pObject, "Not common unit in follow command" );
				pResult = CFollowState::Instance( pArtillery, pCommonUnit );
			}

			break;
		case ACTION_COMMAND_STAND_GROUND:
			pArtillery->StopUnit();
			pArtillery->GetBehaviour().moving = SBehaviour::EMHoldPos;

			break;
		case ACTION_COMMAND_MOVE_TO_GRID:
			pResult = CCommonMoveToGridState::Instance( pArtillery, cmd.vPos, GetVectorByDirection( cmd.fNumber ) );

			break;
		default:
			NI_ASSERT_T( false, "Wrong command" );
	}

	return pResult;
}
IUnitState* CArtilleryStatesFactory::ProduceRestState( class CQueueUnit *pUnit )
{
	NI_ASSERT_T( dynamic_cast<CArtillery*>( pUnit ) != 0, "Wrong unit type" );	
	return CArtilleryRestState::Instance( static_cast<CArtillery*>( pUnit ), CVec2( -1, -1 ), 0 );
}
IUnitState* CArtilleryMoveToState::Instance( CArtillery *pArtillery, const CVec2 &point )
{
	return new CArtilleryMoveToState( pArtillery, point );
}
CArtilleryMoveToState::CArtilleryMoveToState( CArtillery *_pArtillery, const CVec2 &_point )
: pArtillery( _pArtillery ), startTime( curTime ), eState( EAMTS_WAIT_FOR_PATH ), bToFinish( false )
{
	pArtillery->UnlockTiles();
	pArtillery->FixUnlocking();
}
void CArtilleryMoveToState::Segment()
{
	if ( pArtillery->MustHaveCrewToOperate() && pArtillery->HasSlaveTransport() )
	{
		const CVec2 &vPos = pArtillery->GetCurCmd()->ToUnitCmd().vPos;
		if ( pArtillery->HasSlaveTransport() && pArtillery->GetSlaveTransport()->GetState()->GetName() == EUSN_REST )
		{
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_TAKE_ARTILLERY,pArtillery), pArtillery->GetSlaveTransport(), false );
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_DEPLOY_ARTILLERY,vPos ), pArtillery->GetSlaveTransport(), true );
		}
	}
	else
	{
		switch ( eState )
		{
		case EAMTS_WAIT_FOR_PATH:
			if ( curTime - startTime >= TIME_OF_WAITING )
			{
				pStaticPath = pArtillery->GetCurCmd()->CreateStaticPath( pArtillery );
				if ( pStaticPath )
				{
					pArtillery->UnfixUnlocking();				
					if ( pArtillery->IsInstalled() )
					{
						pArtillery->LockTiles();			
						pArtillery->InstallAction( ACTION_NOTIFY_UNINSTALL_MOVE );
						eState = EAMTS_UNINSTALLING;
					}
					else
						eState = EAMTS_START_MOVING;
				}
				else
				{
					pArtillery->SetCommandFinished();
					pArtillery->SendAcknowledgement( ACK_NEGATIVE, true );
				}
				
			}
			break;
		case EAMTS_UNINSTALLING:
			if ( pArtillery->IsUninstalled() )
			{
				if ( bToFinish )
					pArtillery->SetCommandFinished();
				else
					eState = EAMTS_START_MOVING;
			}

			break;
		case EAMTS_START_MOVING:
			{
				if ( !pStaticPath )
					pArtillery->SetCommandFinished();
				else
				{
					pArtillery->SendAlongPath( pStaticPath, pArtillery->GetGroupShift() );
					eState = EAMTS_MOVING;
				}
			}

			break;
		case EAMTS_MOVING:
			if ( pArtillery->IsOperable() )
			{
				if ( pArtillery->IsIdle() )
					pArtillery->SetCommandFinished();
			}
			else if ( !pArtillery->HasServeCrew() )
				pArtillery->SetCommandFinished();

			break;
		}
	}
}
ETryStateInterruptResult CArtilleryMoveToState::TryInterruptState(class CAICommand *pCommand)
{ 
	pArtillery->UnfixUnlocking();
	if ( pArtillery->MustHaveCrewToOperate() )
	{
		if ( eState == EAMTS_UNINSTALLING )
		{
			bToFinish = true;
			return TSIR_YES_WAIT;
		}
	}
	pArtillery->SetCommandFinished(); 
	return TSIR_YES_IMMIDIATELY;
}
IUnitState* CArtilleryTurnToPointState::Instance( CArtillery *pArtillery, const CVec2 &targCenter )
{
	return new CArtilleryTurnToPointState( pArtillery, targCenter );
}
CArtilleryTurnToPointState::CArtilleryTurnToPointState( CArtillery *_pArtillery, const CVec2 &_targCenter )
: pArtillery( _pArtillery ), targCenter( _targCenter ), eState( EATRS_ESTIMATING ), lastCheck( 0 )
{
	pArtillery->StopUnit();
}
void CArtilleryTurnToPointState::Segment()
{
	if ( pArtillery->IsOperable() )
	{
		switch ( eState )
		{
			case EATRS_ESTIMATING:
				if ( pArtillery->GetStats()->type == RPG_TYPE_ART_AAGUN && pArtillery->IsInstalled() )
				{
					for ( int i = 0; i < pArtillery->GetNTurrets(); ++i )
						pArtillery->GetTurret( i )->TurnHor( GetDirectionByVector( targCenter - pArtillery->GetCenter() ) - pArtillery->GetFrontDir() );

					eState = EATPS_TURNING;
				}
				else
				{
					if ( !pArtillery->CanRotateTo( pArtillery->GetUnitRect(), targCenter - pArtillery->GetCenter(), false, false ) )
					{
						pArtillery->SendAcknowledgement( ACK_NEGATIVE );
						pArtillery->SetCommandFinished();
					}
					else
					{
						eState = EATPS_UNINSTALLING;
						if ( pArtillery->IsInstalled() )
							pArtillery->InstallAction( ACTION_NOTIFY_UNINSTALL_ROTATE );
					}
				}

				break;
			case EATPS_UNINSTALLING:
				if ( pArtillery->IsUninstalled() )
				{
					lastCheck = curTime;
					eState = EATPS_TURNING;
				}

				break;
			case EATPS_TURNING:
				if ( pArtillery->IsInstalled() && pArtillery->GetStats()->type == RPG_TYPE_ART_AAGUN )
				{
					int i = 0;
					while ( i < pArtillery->GetNTurrets() && pArtillery->GetTurret( i )->IsFinished() )
						++i;

					if ( i >= pArtillery->GetNTurrets() )
						pArtillery->SetCommandFinished();
				}
				else if ( pArtillery->TurnToUnit( targCenter ) )
					pArtillery->SetCommandFinished();
				else
					lastCheck = curTime;

				break;
		}
	}
	else if ( !pArtillery->HasServeCrew() )
		pArtillery->SetCommandFinished();
}
ETryStateInterruptResult CArtilleryTurnToPointState::TryInterruptState(class CAICommand *pCommand)
{ 
	if ( pArtillery->GetStats()->type == RPG_TYPE_ART_AAGUN )
	{
		for ( int i = 0; i < pArtillery->GetNTurrets(); ++i )
			pArtillery->GetTurret( i )->StopTurning();
	}
	
	pArtillery->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CArtilleryTurnToPointState::GetPurposePoint() const
{
	return pArtillery->GetCenter();
}
IUnitState* CArtilleryBombardmentState::Instance( CAIUnit *pUnit, const CVec2 &point )
{
	return new CArtilleryBombardmentState( pUnit, point );
}
CArtilleryBombardmentState::CArtilleryBombardmentState( CAIUnit *_pUnit, const CVec2 &_point )
: pUnit( _pUnit ), point( _point ), bStop( false ), eState( EABS_START )
{
}
void CArtilleryBombardmentState::Segment()
{
	if ( pUnit->IsOperable() )
	{
		switch ( eState )
		{
			case EABS_START:
				{
					pUnit->StopUnit();

					bool bCanShootWOMove = pUnit->GetFirstArtilleryGun()->CanShootToPointWOMove( point, 0 );
					bool bNeedTurn = false;

					if ( !bCanShootWOMove && pUnit->GetFirstArtilleryGun()->GetRejectReason() == ACK_NOT_IN_ATTACK_ANGLE )
					{
						if ( pUnit->CanRotate() )
						{
							bCanShootWOMove = true;
							bNeedTurn = true;
						}
					}
					
					if ( bCanShootWOMove )
					{
						if ( !bNeedTurn )
							eState = EABS_FIRING;
						else
						{
							if ( pUnit->IsInTankPit() )
							{
								theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LEAVE_TANK_PIT), pUnit );
								return;
							}
							eState = EABS_TURNING;
						}

						CExistingObject *pPit = pUnit->GetTankPit();
						if ( pPit )
							pPit->UnlockTiles();

						if ( bNeedTurn && !pUnit->CanRotateTo( pUnit->GetUnitRect(), point - pUnit->GetCenter(), false, false ) )
						{
							pUnit->SendAcknowledgement( ACK_NEGATIVE );
							pUnit->SetCommandFinished();
						}
						if ( pPit )
							pPit->LockTiles();
					}
					else
					{
						pUnit->SendAcknowledgement( pUnit->GetFirstArtilleryGun()->GetRejectReason() );
						pUnit->SetCommandFinished();
					}
				}

				break;
			case EABS_TURNING:
				if ( pUnit->TurnToUnit( point ) )
					eState = EABS_FIRING;

				break;
			case EABS_FIRING:
				if ( 
						 !pUnit->GetFirstArtilleryGun()->IsBursting() && 
						 ( bStop || !pUnit->GetFirstArtilleryGun()->CanShootToPointWOMove( point, 0 ) )
					 )
				 pUnit->SetCommandFinished();
				else if ( pUnit->CanShoot() )
				{
					if ( !pUnit->GetFirstArtilleryGun()->IsFiring() )
						pUnit->GetFirstArtilleryGun()->StartPointBurst( point, false );
					else if ( pUnit->GetFirstArtilleryGun()->GetNAmmo() == 0 )
						pUnit->SendAcknowledgement( ACK_NO_AMMO );
				}

				break;
			default: NI_ASSERT_T( false, "Wrong state" );
		}
	}
}
ETryStateInterruptResult CArtilleryBombardmentState::TryInterruptState( CAICommand *pCommand )
{
	if ( pCommand == 0 || !pUnit->GetFirstArtilleryGun()->IsBursting() )
	{
		pUnit->GetFirstArtilleryGun()->StopFire();

		pUnit->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	bStop = true;
	return TSIR_YES_WAIT;
}
IUnitState* CArtilleryRangeAreaState::Instance( CAIUnit *pUnit, const CVec2 &point )
{
	return new CArtilleryRangeAreaState( pUnit, point );
}
CArtilleryRangeAreaState::CArtilleryRangeAreaState( CAIUnit *_pUnit, const CVec2 &_point )
: pUnit( _pUnit ), point( _point ), eState( ERAS_RANGING ), bFinish( false ),
	pGun( _pUnit->GetFirstArtilleryGun() ), nShootsLast( SConsts::SHOOTS_TO_RANGE ),
	bFired( true )
{
	pUnit->StopUnit();

	bool bCanShootWOMove = pUnit->GetFirstArtilleryGun()->CanShootToPointWOMove( point, 0 );
	bool bNeedTurn = false;

	if ( !bCanShootWOMove && pUnit->GetFirstArtilleryGun()->GetRejectReason() == ACK_NOT_IN_ATTACK_ANGLE )
	{
		if ( pUnit->CanRotate() )
		{
			bCanShootWOMove = true;
			bNeedTurn = true;
		}
	}
	
	if ( bCanShootWOMove )
	{
		if ( !bNeedTurn )
			eState = ERAS_RANGING;
		else
		{
			eState = ERAS_TURNING;
		}
	}
	else
	{
		pUnit->SendAcknowledgement( pUnit->GetFirstArtilleryGun()->GetRejectReason(), true );
		pUnit->SetCommandFinished();
	}

	if ( bNeedTurn && !pUnit->CanRotateTo( pUnit->GetUnitRect(), point - pUnit->GetCenter(), false, false ) )
	{
		pUnit->SendAcknowledgement( ACK_NEGATIVE, true );
		pUnit->SetCommandFinished();
	}
}
void CArtilleryRangeAreaState::CheckArea()
{
	pUnit->ResetShootEstimator( 0, false );
	for ( CUnitsIter<1,2> iter( pUnit->GetParty(), EDI_ENEMY, point, SConsts::RANGED_AREA_RADIUS ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pTarget = *iter;
		if ( pTarget->IsVisible( pUnit->GetParty() ) &&
				 fabs2( pTarget->GetCenter() - point ) <= sqr( SConsts::RANGED_AREA_RADIUS ) &&
				 pGun->CanShootToUnitWOMove( pTarget ) )
			pUnit->AddUnitToShootEstimator( pTarget );
	}

	if ( pEnemy = pUnit->GetBestShootEstimatedUnit() )
	{
		eState = ERAS_SHOOT_UNIT;
		pGun->StartEnemyBurst( pEnemy, true );
	}
	else
	{
		for ( CStObjCircleIter<true> iter( point, SConsts::RANGED_AREA_RADIUS ); !iter.IsFinished() && pObj == 0; iter.Iterate() )
		{
			CExistingObject *pIteratingObject = *iter;
			if ( pIteratingObject->GetObjectType() == ESOT_BUILDING && theDipl.GetDiplStatus( pIteratingObject->GetPlayer(), pUnit->GetPlayer() ) == EDI_ENEMY && 
					 pIteratingObject->IsVisible( pUnit->GetParty() ) && pIteratingObject->GetNDefenders() > 0 &&
					 fabs2( pIteratingObject->GetAttackCenter( point ) - point ) <= sqr( SConsts::RANGED_AREA_RADIUS ) && pGun->CanShootToObjectWOMove( pIteratingObject ) )
			{
				pObj = pIteratingObject;
			}
		}

		if ( pObj != 0 )
		{
			eState = ERAS_SHOOT_OBJECT;
			pGun->StartPointBurst( pObj->GetAttackCenter( pUnit->GetCenter() ), true );
		}
	}
}
void CArtilleryRangeAreaState::Segment()
{
	if ( pUnit->IsOperable() && ( eState == ERAS_TURNING || pUnit->CanShoot() ) )
	{
		const bool bFiringNow = pGun->IsFiring();
		if ( !bFiringNow )
		{
			if ( bFinish )
				FinishCommand();
			else
			{
				switch ( eState )
				{
					case ERAS_TURNING:
						if ( pUnit->IsInTankPit() )
						{
							theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LEAVE_TANK_PIT), pUnit );
							return;
						}
				
						if ( pUnit->TurnToUnit( point ) )
							eState = ERAS_RANGING;

						break;
					case ERAS_RANGING:
						pGun->StartPointBurst( point, nShootsLast == SConsts::SHOOTS_TO_RANGE );
						if ( bFired )
							--nShootsLast;
						bFired = false;

						if ( nShootsLast == 0 )
						{
							pUnit->SetDispersionBonus( SConsts::RANDGED_DISPERSION_RADIUS_BONUS );
							eState = ERAS_WAITING;
							pGun->LockInCurAngle();
							lastCheck = curTime;
						}

						break;
					case ERAS_WAITING:
						if ( curTime - lastCheck >= CHECK_TIME )
							CheckArea();

						break;
					case ERAS_SHOOT_UNIT:
						if ( IsValidObj( pEnemy ) && pEnemy->IsVisible( pUnit->GetParty() ) && 
								 pGun->CanShootToUnitWOMove( pEnemy ) && fabs2( pEnemy->GetCenter() - point ) <= sqr( SConsts::RANGED_AREA_RADIUS ) )
							pGun->StartEnemyBurst( pEnemy, false );
						else
						{
							eState = ERAS_WAITING;
							lastCheck = curTime - CHECK_TIME;
							pEnemy = 0;
						}

						break;
					case ERAS_SHOOT_OBJECT:
						if ( IsValidObj( pObj ) && theDipl.GetDiplStatus( pObj->GetPlayer(), pUnit->GetPlayer() ) == EDI_ENEMY )
							pGun->StartPointBurst( pObj->GetAttackCenter( pUnit->GetCenter() ), false );
						else
						{
							eState = ERAS_WAITING;
							lastCheck = curTime - CHECK_TIME;
							pObj = 0;
						}

						break;
				}
			}
		}
		else	
		{
			if ( pGun->GetNAmmo() == 0 )
				pUnit->SendAcknowledgement( ACK_NO_AMMO );
			
			bFired = true;
		}
	}
}
void CArtilleryRangeAreaState::FinishCommand()
{
	if ( pGun.IsValid() )
		pGun->StopFire();

	pUnit->GetFirstArtilleryGun()->UnlockCurAngle();
	pUnit->SetDispersionBonus( 1.0f );	
	pUnit->SetCommandFinished();
}
ETryStateInterruptResult CArtilleryRangeAreaState::TryInterruptState(class CAICommand *pCommand)
{
	if ( pCommand == 0 || !pGun.IsValid() || !pGun->IsBursting() )
	{
		FinishCommand();
		return TSIR_YES_IMMIDIATELY;
	}
	bFinish = true;
	return TSIR_YES_WAIT;
}
void CArtilleryRangeAreaState::GetRangeCircle( CCircle *pCircle ) const
{
	pCircle->center = point;
	pCircle->r = SConsts::RANGED_AREA_RADIUS;
}
bool CArtilleryRangeAreaState::IsAttacksUnit() const
{
	return eState == ERAS_SHOOT_UNIT;
}
CAIUnit* CArtilleryRangeAreaState::GetTargetUnit() const
{
	if ( IsAttacksUnit() )
		return pEnemy;
	else
		return 0;
}
IUnitState* CArtilleryInstallTransportState::Instance( CArtillery *pArtillery )
{
	return new CArtilleryInstallTransportState( pArtillery );
}
CArtilleryInstallTransportState::CArtilleryInstallTransportState( CArtillery *_pArtillery )
: pArtillery( _pArtillery ), eState( AITS_WAITING_FOR_CREW )
{
}
void CArtilleryInstallTransportState::Segment()
{
	CFormation * pCrew = pArtillery->GetCrew();
	switch ( eState )
	{
	case AITS_WAITING_FOR_CREW:
		if ( pArtillery->MustHaveCrewToOperate() )
		{
			pArtillery->InstallAction( ACTION_NOTIFY_INSTALL_TRANSPORT );
			eState = AITS_INSTALLING;
		}
		else if ( !pArtillery->HasServeCrew() )
		{
			pArtillery->InstallAction( ACTION_NOTIFY_INSTALL_TRANSPORT );
			pArtillery->SetCommandFinished();
		}
		else if ( pArtillery->IsOperable() )
		{
			pArtillery->InstallAction( ACTION_NOTIFY_INSTALL_TRANSPORT );
			eState = AITS_INSTALLING;
		}

		break;
	case AITS_INSTALLING:
		if ( pArtillery->IsInstalled() )
			pArtillery->SetCommandFinished();
		break;
	}
}
ETryStateInterruptResult CArtilleryInstallTransportState::TryInterruptState(class CAICommand *pCommand)
{
	if ( 0 == pCommand )
	{
		pArtillery->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	return TSIR_YES_WAIT;
}
const CVec2 CArtilleryInstallTransportState::GetPurposePoint() const
{
	return pArtillery->GetCenter();
}
IUnitState* CArtilleryUninstallTransportState::Instance( CArtillery *pArtillery )
{
	return new CArtilleryUninstallTransportState( pArtillery );
}
CArtilleryUninstallTransportState::CArtilleryUninstallTransportState( CArtillery *_pArtillery )
: pArtillery( _pArtillery )
{
	if ( pArtillery->IsUninstalled() && pArtillery->GetCurUninstallAction() == ACTION_NOTIFY_UNINSTALL_TRANSPORT )
		pArtillery->SetCommandFinished();
	else if ( !pArtillery->IsInstalled() )
		eState = AUTS_WAIT_FOR_UNINSTALL;
	else
		eState = AUTS_INSTALLING;
}
void CArtilleryUninstallTransportState::Segment()
{
	switch( eState )
	{
	case AUTS_WAIT_FOR_UNINSTALL:
		if ( pArtillery->IsUninstalled() )
		{
			if ( pArtillery->GetCurUninstallAction() == ACTION_NOTIFY_UNINSTALL_TRANSPORT )
				eState = AUTS_WAIT_FOR_UNINSTALL_TRANSPORT;
			else
			{
				pArtillery->InstallBack( false );
				eState = AUTS_INSTALLING; // 
			}
		}
	case AUTS_INSTALLING:
		if ( pArtillery->IsInstalled() )
		{
			pArtillery->InstallAction( ACTION_NOTIFY_UNINSTALL_TRANSPORT );
			eState = AUTS_WAIT_FOR_UNINSTALL_TRANSPORT;
		}
		break;

	case AUTS_WAIT_FOR_UNINSTALL_TRANSPORT:
		if ( pArtillery->IsUninstalled() )
		{
			pArtillery->SetCommandFinished();
		}
	}
}
ETryStateInterruptResult CArtilleryUninstallTransportState::TryInterruptState(class CAICommand *pCommand)
{
	if ( !pCommand )
	{
		pArtillery->SetCommandFinished();
	}
	else if ( pCommand->ToUnitCmd().cmdType == ACTION_MOVE_BEING_TOWED )
	{
		return TSIR_YES_WAIT;
	}
	return TSIR_NO_COMMAND_INCOMPATIBLE; // этот стейт завершается сам
}
const CVec2 CArtilleryUninstallTransportState::GetPurposePoint() const
{
	return pArtillery->GetCenter();
}
IUnitState* CArtilleryBeingTowedState::Instance( class CArtillery *pArtillery, class CAITransportUnit * pTransport )
{
	return new CArtilleryBeingTowedState( pArtillery, pTransport );
}
CArtilleryBeingTowedState::CArtilleryBeingTowedState( CArtillery *pArtillery, CAITransportUnit * _pTransport )
: pArtillery( pArtillery ), pTransport ( _pTransport ), wLastTagDir ( _pTransport->GetFrontDir() ),
	vLastTagCenter ( _pTransport->GetCenter() ), bInterrupted( false ), timeLastUpdate( curTime )
{
	pArtillery->SetSelectable( false );

	pPath = new CArtilleryBeingTowedPath( 0.0f, pArtillery->GetCenter(), VNULL2 );
	pPath->Init( 0.0f, pArtillery->GetCenter(), VNULL2 );
	pArtillery->SetCurPath( pPath );
	pArtillery->SetRightDir( false );

	updater.Update( ACTION_NOTIFY_PLACEMENT, pArtillery );
}
void CArtilleryBeingTowedState::Segment()
{
	if ( !IsValidObj( pTransport ) )
	{
		pArtillery->ChangePlayer( theDipl.GetNeutralPlayer() );
		TryInterruptState( 0 );
	}
	else if (	wLastTagDir != pTransport->GetFrontDir() || vLastTagCenter != pTransport->GetCenter() )
	{
		CVec2 vFormerPos( pArtillery->GetCenter() );

		const CVec2 tagUnitDir = GetVectorByDirection( pTransport->GetFrontDir() );
		const CVec2 hookPoint = pTransport->GetHookPoint();

		CVec2 newDirVec = hookPoint - pArtillery->GetCenter();

		bool bZeroSpeed = false;
		if ( newDirVec  *  tagUnitDir < 0 )
		{
			bZeroSpeed = true;
			const CVec2 perpVec( -tagUnitDir.y, tagUnitDir.x ); 
			
			if ( perpVec*newDirVec >= 0 )
				newDirVec = perpVec;
			else
				newDirVec = CVec2( tagUnitDir.y, -tagUnitDir.x );
		}

		pArtillery->UpdateDirection( newDirVec );

		wLastTagDir = pTransport->GetFrontDir();
		vLastTagCenter = pTransport->GetCenter();

		const CVec2 vArtilleryHookPoint = pArtillery->GetHookPoint();
		const CVec2 vShiftFromHookPointToArtillery = pArtillery->GetCenter() - vArtilleryHookPoint;
		const CVec2 coord = hookPoint + vShiftFromHookPointToArtillery;
		pArtillery->SetNewCoordinates( CVec3( coord.x, coord.y, theStaticMap.GetZ( AICellsTiles::GetTile(coord) ) ), false );

		const float fSpeed = ( bZeroSpeed ? 0 : fabs(coord - vFormerPos) ) / ( curTime - timeLastUpdate );
		pPath->Init( fSpeed, coord, ( coord - vFormerPos ) / ( curTime - timeLastUpdate ) );
		pArtillery->SetCurPath( pPath );
	}
	else
		pPath->Init( 0.0f, pArtillery->GetCenter(), VNULL2 );

	timeLastUpdate = curTime;
}
ETryStateInterruptResult CArtilleryBeingTowedState::TryInterruptState( CAICommand *pCommand )
{
	if ( pCommand == 0 )
	{
		if ( IsValidObj( pTransport ) )
			pArtillery->SetSelectable( false );

		pArtillery->SetRightDir( true );
		bInterrupted = true;
		pArtillery->SetCommandFinished();
		pArtillery->RestoreDefaultPath();
		pArtillery->StopUnit();

		updater.Update( ACTION_NOTIFY_PLACEMENT, pArtillery );

		return TSIR_YES_IMMIDIATELY;
	}
	return TSIR_NO_COMMAND_INCOMPATIBLE;
}
const CVec2 CArtilleryBeingTowedState::GetPurposePoint() const
{
	return pArtillery->GetCenter();
}
CAITransportUnit* CArtilleryBeingTowedState::GetTowingTransport() const 
{ 
	return pTransport;
}
IUnitState* CArtilleryAttackState::Instance( CArtillery *pArtillery, CAIUnit *pEnemy, bool bAim, const bool bSwarmAttack )
{
	return new CArtilleryAttackState( pArtillery, pEnemy, bAim, bSwarmAttack );
}
CArtilleryAttackState::CArtilleryAttackState( CArtillery *_pArtillery, CAIUnit *_pEnemy, bool _bAim, bool _bSwarmAttack )
: pArtillery( _pArtillery ), pEnemy( _pEnemy ), bAim( _bAim ), eState( EAS_NONE ), CFreeFireManager( _pArtillery ), 
	bFinish( false ), bSwarmAttack( _bSwarmAttack ), nEnemyParty( _pEnemy->GetParty() )
{
	pArtillery->StopUnit();
/*
	if ( bSwarmAttack )
		pArtillery->ResetTargetScan();
*/
}
void CArtilleryAttackState::FinishState()
{
	for ( int i = 0; i < pArtillery->GetNGuns(); ++i )
		pArtillery->GetGun( i )->StopFire();

	damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );

	pArtillery->SetCommandFinished();
}
void CArtilleryAttackState::Segment()
{
	if ( pArtillery->IsOperable() )
	{
		if ( ( !pGun.IsValid() || !pGun->IsBursting() ) && 
		 		 ( !IsValidObj( pEnemy ) || !pEnemy->IsVisible( pArtillery->GetParty() ) || bFinish ||
					 pEnemy->GetParty() != nEnemyParty ) )
			FinishState();
		else if ( pArtillery->IsInstalled() || eState == EAS_NONE || eState == EAS_ROTATING )
		{
			damageToEnemyUpdater.SetDamageToEnemy( pArtillery, pEnemy, pGun );
			if ( bSwarmAttack )
				pArtillery->AnalyzeTargetScan( pEnemy, damageToEnemyUpdater.IsDamageUpdated(), false );

			switch ( eState )
			{
				case EAS_NONE:
					pArtillery->ResetShootEstimator( pEnemy, false );
					pGun = pArtillery->GetBestShootEstimatedGun();

					if ( pGun == 0 )
					{
						bAim = true;
						if ( pArtillery->DoesExistRejectGunsReason( ACK_NOT_IN_ATTACK_ANGLE ) )
						{
							const CVec2 vDirToRotate = pEnemy->GetCenter() - pArtillery->GetCenter();
							wDirToRotate = GetDirectionByVector( vDirToRotate );
							if ( !pArtillery->CanRotateTo( pArtillery->GetUnitRect(), vDirToRotate, false, false ) )
							{
								pArtillery->SendAcknowledgement( ACK_NOT_IN_ATTACK_ANGLE );
								FinishState();
							}
							else
							{
								if ( pArtillery->IsInTankPit() )
								{
									theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LEAVE_TANK_PIT), pArtillery );
									return;
								}
								eState = EAS_ROTATING;
							}
						}
						else
						{
							pArtillery->SendAcknowledgement( pArtillery->GetGunsRejectReason() );
							FinishState();
						}
					}
					else
					{
						pArtillery->Lock( pGun );
						if ( pGun->GetTurret() )
							pGun->GetTurret()->Lock( pGun );

						eState = EAS_FIRING;
					}

					break;
				case EAS_ROTATING:
					if ( pArtillery->TurnToDir( wDirToRotate, false ) )
						eState = EAS_NONE;

					break;
				case EAS_FIRING:
					if ( pArtillery->IsInstalled() )
					{
						Analyze( pArtillery, pGun );
						
						if ( !pGun->IsFiring() )
						{
							if ( pGun->CanShootToUnit( pEnemy ) )
							{
								pGun->StartEnemyBurst( pEnemy, bAim );
								bAim = false;
							}
							else
								FinishState();
						}
						else if ( pGun->GetNAmmo() == 0 )
							pArtillery->SendAcknowledgement( ACK_NO_AMMO );
					}
					else if ( !pArtillery->IsInInstallAction() )
						pArtillery->ForceInstallAction();
			}
		}
	}
	else if ( !pArtillery->HasServeCrew() )
		FinishState();
}
ETryStateInterruptResult CArtilleryAttackState::TryInterruptState( CAICommand *pCommand )
{
	if ( !pCommand )
	{
		FinishState();
		return TSIR_YES_IMMIDIATELY;
	}
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	if ( cmd.cmdType == ACTION_COMMAND_ATTACK_UNIT && cmd.pObject.GetPtr() == pEnemy )
		return TSIR_NO_COMMAND_INCOMPATIBLE;

	bFinish = true;
	return TSIR_YES_WAIT;
}
const CVec2 CArtilleryAttackState::GetPurposePoint() const
{
	if ( IsValidObj( pEnemy ) )
		return pEnemy->GetCenter();
	return CVec2( -1.0f, -1.0f );
}
CAIUnit* CArtilleryAttackState::GetTargetUnit() const
{
	return pEnemy;
}
IUnitState* CArtilleryAttackCommonStatObjState::Instance( CArtillery *pArtillery, CStaticObject *pObj )
{
	return new CArtilleryAttackCommonStatObjState( pArtillery, pObj );
}
CArtilleryAttackCommonStatObjState::CArtilleryAttackCommonStatObjState( CArtillery *_pArtillery, CStaticObject *_pObj )
: pArtillery( _pArtillery ), pObj( _pObj ), eState( EAS_NONE ), CFreeFireManager( _pArtillery ), bFinish( false ), bAim( true )
{
	pArtillery->StopUnit();
}
void CArtilleryAttackCommonStatObjState::FinishState()
{
	for ( int i = 0; i < pArtillery->GetNGuns(); ++i )
		pArtillery->GetGun( i )->StopFire();

	pArtillery->SetCommandFinished();
}
void CArtilleryAttackCommonStatObjState::Segment()
{
	if ( pArtillery->IsOperable() )
	{
		if ( !IsValidObj( pObj ) && ( !pGun.IsValid() || !pGun->IsBursting() ) ||
				 bFinish && ( !pGun.IsValid() || !pGun->IsBursting() ) )
			 FinishState();
		else if ( pArtillery->IsInstalled() || eState == EAS_ROTATING || eState == EAS_NONE )
		{
			switch ( eState )
			{
				case EAS_NONE:
					pGun = pArtillery->ChooseGunForStatObjWOTime( pObj );

					if ( pGun == 0 )
					{
						bAim = true;
						if ( pArtillery->DoesExistRejectGunsReason( ACK_NOT_IN_ATTACK_ANGLE ) )
						{
							const CVec2 vDirToRotate = pObj->GetAttackCenter( pArtillery->GetCenter() ) - pArtillery->GetCenter();
							wDirToRotate = GetDirectionByVector( vDirToRotate );
							if ( !pArtillery->CanRotateTo( pArtillery->GetUnitRect(), vDirToRotate, false, false ) )
							{
								pArtillery->SendAcknowledgement( ACK_NOT_IN_ATTACK_ANGLE );
								FinishState();
							}
							else
							{
								if ( pArtillery->IsInTankPit() )
								{
									theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LEAVE_TANK_PIT), pArtillery );
									return;
								}
								eState = EAS_ROTATING;
							}
						}
						else
						{
							pArtillery->SendAcknowledgement( pArtillery->GetGunsRejectReason() );
							FinishState();
						}
					}
					else
					{
						pArtillery->Lock( pGun );
						if ( pGun->GetTurret() )
							pGun->GetTurret()->Lock( pGun );

						eState = EAS_FIRING;
					}

					break;
				case EAS_ROTATING:
					if ( pArtillery->TurnToDir( wDirToRotate, false ) )
						eState = EAS_NONE;

					break;
				case EAS_FIRING:
					if ( pArtillery->IsInstalled() )
					{
						Analyze( pArtillery, pGun );
						
						if ( !pGun->IsFiring() )
						{
							if ( pGun->CanShootToObject( pObj ) )
							{
								pGun->StartPointBurst( pObj->GetAttackCenter( pArtillery->GetCenter() ), bAim );
								bAim = false;
							}
							else
								FinishState();
						}
						else if ( pGun->GetNAmmo() == 0 )
							pArtillery->SendAcknowledgement( ACK_NO_AMMO );
					}
					else if ( !pArtillery->IsInInstallAction() )
						pArtillery->ForceInstallAction();
			}
		}
	}
	else if ( !pArtillery->HasServeCrew() )
		FinishState();
}
ETryStateInterruptResult CArtilleryAttackCommonStatObjState::TryInterruptState( CAICommand *pCommand )
{
	if ( !pCommand )
	{
		FinishState();
		return TSIR_YES_IMMIDIATELY;
	}
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	if ( cmd.cmdType == ACTION_COMMAND_ATTACK_OBJECT && cmd.pObject.GetPtr() == pObj )
		return TSIR_NO_COMMAND_INCOMPATIBLE;

	bFinish = true;
	return TSIR_YES_WAIT;
}
const CVec2 CArtilleryAttackCommonStatObjState::GetPurposePoint() const
{
	if ( IsValidObj( pObj ) && pArtillery && pArtillery->IsValid() && pArtillery->IsAlive() )
		return pObj->GetAttackCenter( pArtillery->GetCenter() );
	return CVec2( -1.0f, -1.0f );
}
IUnitState* CArtilleryRestState::Instance( CArtillery *pArtillery, const CVec2 &guardPoint, const WORD wDir )
{
	return new CArtilleryRestState( pArtillery, guardPoint, wDir );
}
CArtilleryRestState::CArtilleryRestState( CArtillery *_pArtillery, const CVec2 &guardPoint, const WORD wDir )
: pArtillery( _pArtillery ), CMechUnitRestState( _pArtillery, guardPoint, wDir, 0 )
{
	pArtillery->StopUnit();
}
void CArtilleryRestState::Segment()
{
	if ( pArtillery->IsInstalled() )
		CMechUnitRestState::Segment();
}
IUnitState* CArtilleryAttackAviationState::Instance( CArtillery *pArtillery, CAviation *pPlane )
{
	return new CArtilleryAttackAviationState( pArtillery, pPlane );
}
CArtilleryAttackAviationState::CArtilleryAttackAviationState( CArtillery *_pArtillery, CAviation *pPlane )
: pArtillery( _pArtillery ), CSoldierAttackAviationState( _pArtillery, pPlane )
{
}
void CArtilleryAttackAviationState::Segment()
{
	if ( pArtillery->IsInstalled() )
		CSoldierAttackAviationState::Segment();
}
