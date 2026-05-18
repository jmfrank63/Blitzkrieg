#include "stdafx.h"

#include <float.h>

#include "FormationStates.h"
#include "Formation.h"
#include "Commands.h"
#include "Soldier.h"
#include "Guns.h"
#include "GroupLogic.h"
#include "Entrenchment.h"
#include "Updater.h"
#include "Diplomacy.h"
#include "Technics.h"
#include "TransportStates.h"
#include "EntrenchmentCreation.h"
#include "AIStaticMap.h"
#include "StaticObjects.h"
#include "Building.h"
#include "Artillery.h"
#include "Turret.h"
#include "Soldier.h"
#include "PathFinder.h"
#include "UnitCreation.h"
#include "FormationStates.h"
#include "SoldierStates.h"
#include "Bridge.h"
#include "Randomize.h"
#include "ArtilleryBulletStorage.h"
#include "UnitGuns.h"
#include "ArtilleryPaths.h"

// for profiling
#include "TimeCounter.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CStaticMap theStaticMap;
extern CStaticObjects theStatObjs;
extern NTimer::STime curTime;
extern CGroupLogic theGroupLogic;
extern CUpdater updater;
extern CDiplomacy theDipl;
extern CUnitCreation theUnitCreation;

extern CTimeCounter timeCounter;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CPtr<CFormationStatesFactory> CFormationStatesFactory::pFactory = 0;

CFormationStatesFactory* CFormationStatesFactory::Instance()
{
	if ( pFactory == 0 )
		pFactory = new CFormationStatesFactory();

	return pFactory;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationStatesFactory::CanCommandBeExecuted( CAICommand *pCommand )
{
	const EActionCommand &cmdType = pCommand->ToUnitCmd().cmdType;
	return 
		( cmdType == ACTION_COMMAND_MOVE_TO				||
			cmdType == ACTION_COMMAND_ROTATE_TO			||
			cmdType == ACTION_COMMAND_ENTER					||
			cmdType == ACTION_COMMAND_IDLE_BUILDING	||
			cmdType == ACTION_COMMAND_IDLE_TRENCH		||
			cmdType == ACTION_COMMAND_LEAVE					||
			cmdType == ACTION_COMMAND_SWARM_TO			||
			cmdType == ACTION_COMMAND_ATTACK_UNIT		||
			cmdType == ACTION_COMMAND_ATTACK_OBJECT	||
			cmdType == ACTION_COMMAND_AMBUSH				||
			cmdType == ACTION_COMMAND_GUARD					||
			cmdType == ACTION_COMMAND_PARADE				||
			cmdType == ACTION_COMMAND_LOAD					||
			cmdType == ACTION_COMMAND_IDLE_TRANSPORT ||
			cmdType == ACTION_COMMAND_LOAD_NOW ||
			cmdType == ACTION_MOVE_CATCH_TRANSPORT ||
			cmdType == ACTION_MOVE_PARACHUTE ||
			cmdType == ACTION_COMMAND_CATCH_ARTILLERY ||
			cmdType == ACTION_MOVE_SET_HOME_TRANSPORT ||
			cmdType == ACTION_COMMAND_USE_SPYGLASS ||
			cmdType == ACTION_MOVE_ATTACK_FORMATION ||
			cmdType == ACTION_MOVE_GUNSERVE  ||
			cmdType == ACTION_COMMAND_DISBAND_FORMATION ||
			cmdType == ACTION_COMMAND_FORM_FORMATION || 
			cmdType == ACTION_COMMAND_WAIT_TO_FORM ||
			cmdType == ACTION_COMMAND_FOLLOW ||
			cmdType == ACTION_COMMAND_FOLLOW_NOW ||
			cmdType == ACTION_COMMAND_STOP ||
			cmdType == ACTION_COMMAND_CATCH_FORMATION ||
			cmdType == ACTION_COMMAND_SWARM_ATTACK_UNIT ||
			cmdType == ACTION_MOVE_SWARM_ATTACK_FORMATION ||
			cmdType == ACTION_MOVE_REPAIR_UNIT ||
			cmdType == ACTION_MOVE_RESUPPLY_UNIT ||
			cmdType == ACTION_MOVE_LOAD_RU ||
			cmdType == ACTION_MOVE_BUILD_LONGOBJECT ||
			cmdType == ACTION_MOVE_PLACE_ANTITANK ||
			cmdType == ACTION_MOVE_PLACEMINE ||
			cmdType == ACTION_MOVE_CLEARMINE ||
			cmdType == ACTION_MOVE_REPAIR_BRIDGE ||
			cmdType == ACTION_MOVE_REPAIR_BUILDING ||
			cmdType == ACTION_COMMAND_ROTATE_TO_DIR ||
			cmdType == ACTION_MOVE_ENTER_TRANSPORT_CHEAT_PATH ||
			cmdType == ACTION_COMMAND_STAND_GROUND ||
			cmdType == ACTION_COMMAND_ENTER_BUILDING_NOW ||
			cmdType == ACTION_COMMAND_ENTER_ENTREHCMNENT_NOW ||
			cmdType == ACTION_COMMAND_MOVE_TO_GRID
		);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void StopAllUnits( CFormation *pFormation )
{
	for ( int i = 0; i < pFormation->Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), (*pFormation)[i], false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsFormationCloseToEnemy( const CVec2 &enemyCenter, CFormation *pFormation )
{
	const float fGoodDistance = pFormation->GetMaxFireRange() * 1.2f;

	if ( fabs( pFormation->GetCenter() - enemyCenter ) < fGoodDistance )
		return true;
	
	for ( int i = 0; i < pFormation->Size(); ++i )
	{
		if ( fabs( (*pFormation)[i]->GetCenter() - enemyCenter ) < fGoodDistance )
			return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationStatesFactory::ProduceState( CQueueUnit *pObj, CAICommand *pCommand )
{
	NI_ASSERT_T( dynamic_cast<CFormation*>(pObj) != 0, "Wrong unit passed" );
	CFormation *pFormation = static_cast<CFormation*>(pObj);

	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	IUnitState* pResult = 0;
	bool bSwarmAttack = false;

	switch ( cmd.cmdType )
	{
		case ACTION_MOVE_ENTER_TRANSPORT_CHEAT_PATH:
			{
				CONVERT_OBJECT_PTR( CMilitaryCar, pCar, cmd.pObject, "Wrong unit in idle in transport command" );
				pResult = CFormationEnterTransportByCheatPathState::Instance( pFormation, pCar );
			}

			break;
		case ACTION_MOVE_REPAIR_BUILDING:
			{
				CONVERT_OBJECT_PTR( CBuilding, pBuilding, cmd.pObject, "not building asked to repair" );
				pResult = CFormationRepairBuildingState::Instance( pFormation, pBuilding );
			}
			
			break;
		case ACTION_MOVE_REPAIR_BRIDGE:
			{
				CONVERT_OBJECT_PTR( CFullBridge, pFullBridge, cmd.pObject, "not bridge asked to repair as a bridge" );
				pResult = CFormationRepairBridgeState::Instance( pFormation, pFullBridge );
			}

			break;
		case ACTION_MOVE_LOAD_RU:
			{
				CONVERT_OBJECT_PTR( CBuildingStorage, pBuildingStorage, cmd.pObject, "Wrong storage building" );
				pResult = CFormationLoadRuState::Instance( pFormation, pBuildingStorage );
			}

			break;
		case ACTION_MOVE_GUNSERVE:
			{
				CONVERT_OBJECT_PTR( CArtillery, pArtillery, cmd.pObject, "Wrong artillery unit" );
				pResult = CFormationGunCrewState::Instance( pFormation, pArtillery );
			}

			break;
		case ACTION_COMMAND_CATCH_ARTILLERY:
			{
				CONVERT_OBJECT_PTR( CArtillery, pArtillery, cmd.pObject, "Wrong artillery unit" );
				pResult = CFormationCaptureArtilleryState::Instance( pFormation, pArtillery, cmd.fNumber );
			}

			break;
		case ACTION_MOVE_PARACHUTE:
			pResult = CFormationParaDropState::Instance( pFormation );

			break;
		case ACTION_MOVE_BUILD_LONGOBJECT:
			NI_ASSERT_T( dynamic_cast_ptr<CLongObjectCreation*>(cmd.pObject) != 0, "wrong creator passed" );
			pResult = CFormationBuildLongObjectState::Instance( pFormation, static_cast_ptr<CLongObjectCreation*>(cmd.pObject) );

			break;
		case ACTION_MOVE_PLACE_ANTITANK:
			pResult = CFormationPlaceAntitankState::Instance( pFormation, cmd.vPos );

			break;
		case ACTION_COMMAND_MOVE_TO:
			pFormation->UnsetFollowState();
			if ( pFormation->IsInEntrenchment() )
			{
				StopAllUnits( pFormation );					
				pResult = CFormationLeaveEntrenchmentState::Instance( pFormation, pFormation->GetEntrenchment(), cmd.vPos + pFormation->GetGroupShift() );
			}
			else if ( pFormation->IsInBuilding() )
			{
				pResult = CFormationLeaveBuildingState::Instance( pFormation, pFormation->GetBuilding(), cmd.vPos );
			}
			else
			{
				StopAllUnits( pFormation );
				pResult = CFormationMoveToState::Instance( pFormation, cmd.vPos );
			}

			break;
		case ACTION_COMMAND_USE_SPYGLASS:
			pResult = CFormationUseSpyglassState::Instance( pFormation, cmd.vPos );

			break;
		case ACTION_COMMAND_PARADE:
			{
				StopAllUnits( pFormation );
				pResult = CFormationParadeState::Instance( pFormation, cmd.fNumber );
			}

			break;
		case ACTION_COMMAND_ROTATE_TO:
			StopAllUnits( pFormation );
			pResult = CFormationRotateState::Instance( pFormation, GetDirectionByVector( cmd.vPos - pFormation->GetCenter() ) );

			break;
		case ACTION_COMMAND_ROTATE_TO_DIR:
			StopAllUnits( pFormation );
			pResult = CFormationRotateState::Instance( pFormation, GetDirectionByVector( cmd.vPos ) );

			break;
		case ACTION_COMMAND_ENTER:
			if ( cmd.pObject != 0 )
			{
				StopAllUnits( pFormation );
				
				CONVERT_OBJECT_PTR( CStaticObject, pObj, cmd.pObject, "Not static object for command ACTION_COMMAND_ENTER passed" );
				switch ( pObj->GetObjectType() )
				{
					case ESOT_ENTRENCHMENT:
						pResult = CFormationEnterEntrenchmentState::Instance( pFormation, static_cast<CEntrenchment*>(pObj) );
						break;
					case ESOT_ENTR_PART:
						if ( CEntrenchment *pEntrenchment = static_cast<CEntrenchmentPart*>(pObj)->GetOwner() )
							pResult = CFormationEnterEntrenchmentState::Instance( pFormation, pEntrenchment );

						break;
					case ESOT_BUILDING:
						pResult = CFormationEnterBuildingState::Instance( pFormation, static_cast<CBuilding*>(pObj) );
						break;
					default:
						NI_ASSERT_T( false, NStr::Format( "Can't enter to object of type %d, from AI flag %d", pObj->GetObjectType(), (int)cmd.bFromAI ) );
				}
			}

			break;
		case ACTION_COMMAND_IDLE_BUILDING:
			NI_ASSERT_T( dynamic_cast_ptr<CBuilding*>( cmd.pObject ) != 0, NStr::Format( "Wrong static object (%s) is passed, command ACTION_COMMAND_IDLE_BUILDING", typeid(*cmd.pObject.GetPtr()).name() ) );
			pResult = CFormationIdleBuildingState::Instance( pFormation, static_cast_ptr<CBuilding*>( cmd.pObject ) );

			break;
		case ACTION_COMMAND_IDLE_TRENCH:
			NI_ASSERT_T( dynamic_cast_ptr<CEntrenchment*>( cmd.pObject ) != 0, NStr::Format( "Wrong static object (%s) is passed, command ACTION_COMMAND_IDLE_TRENCH", typeid(*cmd.pObject.GetPtr()).name() ) );
			pResult = CFormationIdleEntrenchmentState::Instance( pFormation, static_cast_ptr<CEntrenchment*>( cmd.pObject ) );

			break;
		case ACTION_COMMAND_LEAVE:
			StopAllUnits( pFormation );			
			if ( pFormation->IsInBuilding() )
				pResult = CFormationLeaveBuildingState::Instance( pFormation, pFormation->GetBuilding(), cmd.vPos );
			else if ( pFormation->IsInEntrenchment() )
				pResult = CFormationLeaveEntrenchmentState::Instance( pFormation, pFormation->GetEntrenchment(), cmd.vPos );

			break;
		case ACTION_MOVE_PLACEMINE:
			StopAllUnits( pFormation );
			pResult = CFormationPlaceMine::Instance( pFormation, cmd.vPos, static_cast<enum SMineRPGStats::EType>(int(cmd.fNumber)) );

			break;
		case ACTION_MOVE_CLEARMINE:
			StopAllUnits( pFormation );			
			pResult = CFormationClearMine::Instance( pFormation, cmd.vPos );

			break;
		case ACTION_COMMAND_SWARM_TO:
			StopAllUnits( pFormation );
			pResult = CFormationSwarmState::Instance( pFormation, cmd.vPos, cmd.fNumber );

			break;
		case ACTION_MOVE_SWARM_ATTACK_FORMATION:
			bSwarmAttack = true;
		case ACTION_MOVE_ATTACK_FORMATION:
			NI_ASSERT_T( dynamic_cast_ptr<CFormation*>( cmd.pObject ) != 0, "must be formation unit" );
			pResult = CFormationAttackFormationState::Instance( pFormation, static_cast_ptr<CFormation*>( cmd.pObject), bSwarmAttack );

			break;
		case ACTION_COMMAND_SWARM_ATTACK_UNIT:
			bSwarmAttack = true;
		case ACTION_COMMAND_ATTACK_UNIT:
			{
				CONVERT_OBJECT_PTR( CAIUnit, pTarget, cmd.pObject, "Wrong unit to attack" );

				if ( !pTarget || !pTarget->IsAlive() )
					pFormation->SendAcknowledgement( ACK_INVALID_TARGET, !pCommand->IsFromAI() );
				else if ( pTarget->GetStats()->IsArtillery()  )
				{
					NI_ASSERT_T( dynamic_cast_ptr<CArtillery*>( cmd.pObject ) != 0, "must be artillery unit" );
					CArtillery *pArt = static_cast_ptr<CArtillery*>( cmd.pObject );
					if ( pArt->GetCrew() )
						pResult = CFormationAttackFormationState::Instance( pFormation, pArt->GetCrew(), bSwarmAttack );
					else
						pResult = CFormationAttackUnitState::Instance( pFormation, pArt, bSwarmAttack );
				}
				else
					pResult = CFormationAttackUnitState::Instance( pFormation, static_cast_ptr<CAIUnit*>( cmd.pObject ), bSwarmAttack );
					
			}

			break;
		case ACTION_COMMAND_ATTACK_OBJECT:
			{
				CONVERT_OBJECT_PTR( CStaticObject, pStaticObj, cmd.pObject, NStr::Format( "Wrong static object to attack (%s)", typeid(cmd.pObject).name() ) );
				// attack the artillery
				if ( pStaticObj->GetObjectType() == ESOT_ARTILLERY_BULLET_STORAGE )
				{
					pCommand->ToUnitCmd().cmdType = ACTION_COMMAND_ATTACK_UNIT;
					pCommand->ToUnitCmd().pObject = static_cast<CArtilleryBulletStorage*>(pStaticObj)->GetOwner();
					pCommand->ToUnitCmd().fNumber = 0;
					pResult = ProduceState( pObj, pCommand );
				}
				else
					pResult = CFormationAttackCommonStatObjState::Instance( pFormation, pStaticObj );
			}

			break;
		case ACTION_COMMAND_GUARD:
			if ( pFormation->IsFree() && pFormation->HasMortar() )
				return CFormationInstallMortarState::Instance( pFormation );
			else if ( pFormation->IsInBuilding() || pFormation->IsInEntrenchment() )
				return pFormation->GetState();
			else
			{
				StopAllUnits( pFormation );
				pResult = CFormationRestState::Instance( pFormation, cmd.vPos, cmd.fNumber );
			}

			break;
		case ACTION_COMMAND_AMBUSH:
			StopAllUnits( pFormation );
			pResult = CCommonAmbushState::Instance( pFormation );

			break;
		case ACTION_COMMAND_IDLE_TRANSPORT:
			NI_ASSERT_T( dynamic_cast_ptr<CMilitaryCar*>( cmd.pObject ) != 0, "Wrong unit in idle in transport command" );
			pResult = CFormationIdleTransportState::Instance( pFormation, static_cast_ptr<CMilitaryCar*>( cmd.pObject ) );

			break;
		case ACTION_COMMAND_LOAD:
			{
				CONVERT_OBJECT_PTR( CMilitaryCar, pCar, cmd.pObject, NStr::Format( "Wrong unit to load to %s",typeid( *pObj ).name()) );
				pResult = CFormationEnterTransportState::Instance( pFormation, pCar );
			}

			break;
		case ACTION_COMMAND_LOAD_NOW:
			NI_ASSERT_T( dynamic_cast_ptr<CMilitaryCar*>( cmd.pObject ) != 0, "Wrong unit to load to" );
			pResult = CFormationEnterTransportNowState::Instance( pFormation, static_cast_ptr<CMilitaryCar*>( cmd.pObject ) );

			break;
		case ACTION_MOVE_REPAIR_UNIT:
			NI_ASSERT_T( cmd.pObject ? dynamic_cast_ptr<CAIUnit*>( cmd.pObject ) != 0 : true, NStr::Format( "Wrong preferred unit %s",typeid( *pObj ).name()) );
			pResult = CFormationRepairUnitState::Instance( pFormation, static_cast_ptr<CAIUnit*>( cmd.pObject ) );

			break;
		case ACTION_MOVE_SET_HOME_TRANSPORT:
			NI_ASSERT_T( dynamic_cast<IEngineerFormationState*>( pFormation->GetState())!=0, "bad state sequence" );
			NI_ASSERT_T( dynamic_cast_ptr<CAITransportUnit*>( cmd.pObject ) != 0, "Wrong home transport" );

			static_cast<IEngineerFormationState*>( pFormation->GetState() )->SetHomeTransport(  static_cast_ptr<CAITransportUnit*>( cmd.pObject ) );
			pResult = pFormation->GetState();
			
			break;
		case ACTION_MOVE_RESUPPLY_UNIT:
			NI_ASSERT_T( cmd.pObject ? dynamic_cast_ptr<CAIUnit*>( cmd.pObject ) != 0 : true, NStr::Format( "Wrong preferred unit %s",typeid( *pObj ).name()) );
			pResult = CFormationResupplyUnitState::Instance( pFormation, static_cast_ptr<CAIUnit*>(cmd.pObject) );

			break;
		case ACTION_MOVE_CATCH_TRANSPORT:
			NI_ASSERT_T( dynamic_cast_ptr<CAITransportUnit*>( cmd.pObject ) != 0, "Wrong unit to load to" );
			pResult = CFormationCatchTransportState::Instance( pFormation, static_cast_ptr<CAITransportUnit*>(cmd.pObject), cmd.fNumber );

			break;
		case ACTION_COMMAND_DISBAND_FORMATION:
			pResult = CFormationDisbandState::Instance( pFormation );

			break;
		case ACTION_COMMAND_FORM_FORMATION:
			pResult = CFormationFormState::Instance( pFormation );

			break;
		case ACTION_COMMAND_WAIT_TO_FORM:
			NI_ASSERT_T( dynamic_cast_ptr<CSoldier*>(cmd.pObject) != 0, "Not common unit in follow command" );
			pResult = CFormationWaitToFormState::Instance( pFormation, cmd.fNumber, static_cast_ptr<CSoldier*>(cmd.pObject) );

			break;
		case ACTION_COMMAND_FOLLOW:
			{
				CONVERT_OBJECT_PTR( CCommonUnit, pUnit, cmd.pObject, "Not common unit in follow command" );
				if ( IsValidObj( pUnit ) )
					pFormation->SetFollowState( pUnit );
			}

			break;
		case ACTION_COMMAND_FOLLOW_NOW:
			{
				CONVERT_OBJECT_PTR( CCommonUnit, pUnit, cmd.pObject, "Not common unit in follow command" );
				pResult = CFollowState::Instance( pFormation, pUnit );
			}

			break;
		case ACTION_COMMAND_STOP:
			pFormation->BalanceCenter();

			break;
		case ACTION_COMMAND_CATCH_FORMATION:
			{
				CONVERT_OBJECT_PTR( CFormation, pFormationToCatch, cmd.pObject, "Not formation in ACTION_COMMAND_CATCH_FORMATION command" );
				pResult = CCatchFormationState::Instance( pFormation, pFormationToCatch );
			}

			break;
		case ACTION_COMMAND_STAND_GROUND:
			pFormation->StopUnit();
			pFormation->GetBehaviour().moving = SBehaviour::EMHoldPos;
			
			for ( int i = 0; i < pFormation->Size(); ++i )
			{
				CSoldier *pSoldier = (*pFormation)[i];
				pSoldier->StopUnit();
				pSoldier->GetBehaviour().moving = SBehaviour::EMHoldPos;
			}

			break;
		case ACTION_COMMAND_ENTER_BUILDING_NOW:
			{
				CONVERT_OBJECT_PTR( CBuilding, pBuilding, cmd.pObject, "Not building in ACTION_COMMAND_ENTER_BUILDING_NOW command" );
				pResult = CFormationEnterBuildingNowState::Instance( pFormation, pBuilding );
			}

			break;
		case ACTION_COMMAND_ENTER_ENTREHCMNENT_NOW:
			{
				CONVERT_OBJECT_PTR( CEntrenchmentPart, pEntrenchmentPart, cmd.pObject, "Not entrechmnent in ACTION_COMMAND_ENTER_BUILDING_NOW command" );
				pResult = CFormationEnterEntrenchmentNowState::Instance( pFormation, pEntrenchmentPart->GetOwner() );
			}

			break;
		case ACTION_COMMAND_MOVE_TO_GRID:
			pResult = CCommonMoveToGridState::Instance( pFormation, cmd.vPos, GetVectorByDirection( cmd.fNumber ) );

			break;
		default:
			NI_ASSERT_T( false, "Wrong command" );
	}

	return pResult;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationStatesFactory::ProduceRestState( CQueueUnit *pUnit )
{
	NI_ASSERT_T( dynamic_cast<CFormation*>(pUnit) != 0, "Wrong unit's type" );

	CFormation *pFormation = static_cast<CFormation*>(pUnit);
	StopAllUnits( pFormation );
	pFormation->StopUnit();

	if ( pFormation->IsFree() )
	{
		if ( pFormation->HasMortar()  )
			return CFormationInstallMortarState::Instance( pFormation );
		return CFormationRestState::Instance( pFormation, CVec2( -1, -1 ), 0 );
	}
	else if ( pFormation->IsInBuilding() )
		return CFormationIdleBuildingState::Instance( pFormation, pFormation->GetBuilding() );
	else if ( pFormation->IsInEntrenchment() )
		return CFormationIdleEntrenchmentState::Instance( pFormation, pFormation->GetEntrenchment() );
	else
		return CFormationIdleTransportState::Instance( pFormation, pFormation->GetTransportUnit() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											 CFormationRestState												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationRestState::Instance( CFormation *pFormation, const CVec2 &guardPoint, const WORD wDir )
{
	return new CFormationRestState( pFormation, guardPoint, wDir );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationRestState::CFormationRestState( CFormation *_pFormation, const CVec2 &guardPoint, const WORD wDir )
: pFormation( _pFormation ), CCommonRestState( guardPoint, wDir, _pFormation )
{
	pFormation->SetToWaitingState();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationRestState::Segment()
{
	CCommonRestState::Segment();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CCommonUnit* CFormationRestState::GetUnit() const 
{ 
	return pFormation; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationRestState::TryInterruptState( CAICommand *pCommand )
{
	pFormation->UnsetFromWaitingState();
	pFormation->SetCommandFinished();

	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											 CFormationMoveToState											*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationMoveToState::Instance( CFormation *pFormation, const CVec2 &point )
{
	return new CFormationMoveToState( pFormation, point );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationMoveToState::CFormationMoveToState(  CFormation *_pFormation, const CVec2 &_point )
: pFormation( _pFormation ), startTime( curTime ), bWaiting( true ), eMoveToState( EMTS_FORMATION_MOVING )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationMoveToState::FormationMovingState()
{
	if ( bWaiting )
	{
		if ( curTime - startTime >= TIME_OF_WAITING )
		{
			bWaiting = false;

			if ( CPtr<IStaticPath> pStaticPath = pFormation->GetCurCmd()->CreateStaticPath( pFormation ) )
				pFormation->SendAlongPath( pStaticPath, pFormation->GetGroupShift() );
			else
			{
				pFormation->SendAcknowledgement( ACK_NEGATIVE, true );
				pFormation->SetCommandFinished();
			}
		}
	}
	else if ( pFormation->IsIdle() )
	{
		for ( int i = 0; i < pFormation->Size(); ++i )
		{
			CSoldier *pSoldier = (*pFormation)[i];
			const CVec2 vPoint = pSoldier->GetUnitPointInFormation();
			const bool bTooFar = mDistance( vPoint, pSoldier->GetCenter() ) > 1.5f * SConsts::TILE_SIZE;

			if ( bTooFar )
			{
				CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( vPoint, VNULL2, pSoldier ); 
				if ( pStaticPath )
					pSoldier->SendAlongPath( pStaticPath, VNULL2 );
			}
		}

		eMoveToState = EMTS_UNITS_MOVING_TO_FORMATION_POINTS;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationMoveToState::UnitsMovingToFormationPoints()
{
	int i = 0;
	while ( i < pFormation->Size() && (*pFormation)[i]->IsIdle() )
		++i;

	if ( i >= pFormation->Size() )
		pFormation->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationMoveToState::Segment()
{
	switch ( eMoveToState )
	{
		case EMTS_FORMATION_MOVING:
			FormationMovingState();

			break;
		case EMTS_UNITS_MOVING_TO_FORMATION_POINTS:
			UnitsMovingToFormationPoints();

			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationMoveToState::TryInterruptState( class CAICommand *pCommand )
{ 
	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationEnterBuildingState									*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationEnterBuildingState::Instance( CFormation *pFormation, CBuilding *pBuilding )
{
	return new CFormationEnterBuildingState( pFormation, pBuilding );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationEnterBuildingState::CFormationEnterBuildingState( CFormation *_pFormation, CBuilding *_pBuilding )
: pFormation( _pFormation ), pBuilding( _pBuilding ), state( EES_START )
{
	int i = 0;
	while ( i < pBuilding->GetNEntrancePoints() )
	{
		if ( pBuilding->IsGoodPointForRunIn( pFormation->GetCenter(), i, pFormation->GetMaxProjection() * 1.2f ) )
		{
			nEntrance = i;
			state = EES_RUN_UP;
			break;
		}
		else
			++i;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationEnterBuildingState::SetPathForRunUp()
{
	CPtr<IStaticPath> pBestPath = pFormation->GetPathToBuilding( pBuilding, &nEntrance );
	
	if ( pBestPath == 0 )
		return false;
	else
	{
		pFormation->SendAlongPath( pBestPath, VNULL2 );
		return true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterBuildingState::SendUnitsToBuilding()
{
	// послать юнитов в здание
	pFormation->StopUnit();
	for ( int i = 0; i < pFormation->Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ENTER, pBuilding, 0 ), (*pFormation)[i], false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationEnterBuildingState::IsNotEnoughSpace()
{
	return
		theDipl.GetDiplStatus( pFormation->GetPlayer(), pBuilding->GetPlayer() ) != EDI_ENEMY && pBuilding->GetNFreePlaces() < pFormation->Size() ||
		theDipl.GetDiplStatus( pFormation->GetPlayer(), pBuilding->GetPlayer() ) == EDI_ENEMY && pBuilding->GetNFriendlyAttackers( pFormation->GetPlayer() ) + pFormation->Size() > pBuilding->GetNOverallPlaces();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterBuildingState::Segment()
{
	NI_ASSERT_T( !pFormation->IsInWaitingState(), "Wrong formation waiting state" );
	
	// здание разрушено или там не хватит места
	if ( !IsValidObj( pBuilding ) || 
			 state != EES_WAITINIG_TO_ENTER && pBuilding->GetPlayer() != -1 && IsNotEnoughSpace() )
	{
		pFormation->SendAcknowledgement( ACK_NEGATIVE );
		TryInterruptState( 0 );
	}
	else
	{
		switch ( state )
		{
			case EES_START:
				if ( SetPathForRunUp() )
					state = EES_RUN_UP;
				else
				{
					pFormation->SendAcknowledgement( ACK_NEGATIVE );					
					pFormation->SetCommandFinished();
				}
	
				break;
			case EES_RUN_UP:
				{
					bool bGoodPoint = pBuilding->IsGoodPointForRunIn( pFormation->GetCenter(), nEntrance, pFormation->GetRadius() );
					if ( pFormation->IsIdle() || bGoodPoint )
					{
						if ( !bGoodPoint )
							state = EES_START;
						else
						{
							if ( pFormation->Size() > pBuilding->GetNFreePlaces() )
								pFormation->SetCommandFinished();
							else if ( pBuilding->IsLocked( pFormation->GetPlayer() ) )
								state = EES_WAIT_FOR_UNLOCK;
							else
							{
								pBuilding->Lock( pFormation );
								state = EES_WAITINIG_TO_ENTER;
								SendUnitsToBuilding();
							}
						}
					}
				}

				break;
			case EES_WAIT_FOR_UNLOCK:
				if ( pFormation->Size() > pBuilding->GetNFreePlaces() )
					pFormation->SetCommandFinished();
				else if ( !pBuilding->IsLocked( pFormation->GetPlayer() ) )
				{
					pBuilding->Lock( pFormation );
					state = EES_WAITINIG_TO_ENTER;
					SendUnitsToBuilding();
				}

				break;
			case EES_WAITINIG_TO_ENTER:
				{
					int i = 0;
					while ( i < pFormation->Size() && (*pFormation)[i]->IsInBuilding() )
						++i;

					if ( i == pFormation->Size() )
					{
						pBuilding->Unlock( pFormation );
						state = EES_FINISHED;
						pFormation->SetCommandFinished();
						theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_BUILDING, pBuilding ), pFormation );
					}
				}

				break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationEnterBuildingState::TryInterruptState( class CAICommand *pCommand )
{
	const EActionCommand eCmd = pCommand ? pCommand->ToUnitCmd().cmdType : EActionCommand(0);
	if ( state == EES_WAITINIG_TO_ENTER && pCommand && eCmd != ACTION_COMMAND_LEAVE )
	{
		int i = 0;
		while ( i < pFormation->Size() && !(*pFormation)[i]->IsInBuilding() )
			++i;

		// part of the squad is already in the building
		if ( i < pFormation->Size() )
			return TSIR_NO_COMMAND_INCOMPATIBLE;
		else
		{
			for ( int i = 0; i < pFormation->Size(); ++i )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), (*pFormation)[i], false );

			if ( IsValidObj( pBuilding ) )
				pBuilding->Unlock( pFormation );

			pFormation->SetCommandFinished();

			return TSIR_YES_IMMIDIATELY;
		}
	}

	if ( state != EES_WAITINIG_TO_ENTER )
	{
		pFormation->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else if ( pCommand && ( eCmd == ACTION_COMMAND_LEAVE || eCmd == ACTION_COMMAND_MOVE_TO ) && IsValidObj( pBuilding ) )
	{
		pBuilding->Unlock( pFormation );
		pFormation->SetInBuilding( pBuilding );
		pFormation->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else
		return TSIR_YES_WAIT;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationEnterBuildingState::GetPurposePoint() const
{
	if ( IsValidObj( pBuilding ) )
		return pBuilding->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CFormationEnterEntrenchmentState								*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationEnterEntrenchmentState::Instance( CFormation *pFormation, CEntrenchment *pEntrenchment )
{
	return new CFormationEnterEntrenchmentState( pFormation, pEntrenchment );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationEnterEntrenchmentState::CFormationEnterEntrenchmentState( CFormation *_pFormation, CEntrenchment *_pEntrenchment )
: pFormation( _pFormation ), pEntrenchment( _pEntrenchment ), state( EES_START )
{
	if ( IsAnyPartCloseToEntrenchment() )
		state = EES_RUN;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationEnterEntrenchmentState::IsAnyPartCloseToEntrenchment() const
{
	// посмотреть положение центра формации
	const float fFormationRadius2 = sqr( Max( pFormation->GetRadius(), SConsts::TILE_SIZE * 4.0f ) );
	const CVec2 vFormationCenter = pFormation->GetCenter();
	CVec2 vClosestPoint;
	pEntrenchment->GetClosestPoint( vFormationCenter, &vClosestPoint );
	
	if ( fabs2( vClosestPoint - vFormationCenter ) < fFormationRadius2 )
		return true;
	
	// посмотреть положения каждого из солдатиков
	int i = 0;
	while ( i < pFormation->Size() )
	{
		const CVec2 vSoldierCenter = (*pFormation)[i]->GetCenter();
		CVec2 vClosestPoint;
		pEntrenchment->GetClosestPoint( vSoldierCenter, &vClosestPoint );

		if ( fabs2( vClosestPoint - vSoldierCenter ) < fFormationRadius2 )
			return true;
		else
			++i;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationEnterEntrenchmentState::SetPathForRunIn()
{
	CPtr<IStaticPath> pPath = pFormation->GetPathToEntrenchment( pEntrenchment );

	if ( pPath )
	{
		pFormation->SendAlongPath( pPath, VNULL2 );
		return true;
	}
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterEntrenchmentState::EnterToEntrenchment()
{
	for ( int i = 0; i < pFormation->Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ENTER, pEntrenchment, 1 ), (*pFormation)[i], false );

	state = EES_WAIT_TO_ENTER;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterEntrenchmentState::Segment()
{
	// окоп non valid или там кто-то уже сидит
	if ( !IsValidObj( pEntrenchment ) )
		TryInterruptState( 0 );	
	switch ( state )
	{
		case EES_START:
			if ( SetPathForRunIn() )
				state = EES_RUN;
			else
			{
				pFormation->SendAcknowledgement( ACK_NEGATIVE );
				pFormation->SetCommandFinished();
			}

			break;
		case EES_RUN:
			if ( !IsValidObj( pEntrenchment ) )
				pFormation->SetCommandFinished();
			else if ( IsAnyPartCloseToEntrenchment() )
				EnterToEntrenchment();
			else if ( pFormation->IsIdle() )
				state = EES_START;

			break;
		case EES_WAIT_TO_ENTER:
		{
			int i = 0;
			while ( i < pFormation->Size() && (*pFormation)[i]->IsInEntrenchment() )
				++i;

			if ( i >= pFormation->Size() )
			{
				pFormation->SetCommandFinished();
				theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRENCH, pEntrenchment ), pFormation );
			}
		}

		break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationEnterEntrenchmentState::TryInterruptState( class CAICommand *pCommand )
{
	if ( state == EES_WAIT_TO_ENTER && pCommand && pCommand->ToUnitCmd().cmdType != ACTION_COMMAND_LEAVE )
	{
		int i = 0;
		while ( i < pFormation->Size() && !(*pFormation)[i]->IsInEntrenchment() )
			++i;

		if ( i < pFormation->Size() )
			return TSIR_NO_COMMAND_INCOMPATIBLE;
		else
		{
			for ( int i = 0; i < pFormation->Size(); ++i )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), (*pFormation)[i], false );
			pFormation->SetCommandFinished();

			return TSIR_YES_IMMIDIATELY;
		}
	}

	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationEnterEntrenchmentState::GetPurposePoint() const
{
	return pEntrenchment->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CFormationIdleBuildingState											*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationIdleBuildingState::Instance( CFormation *pFormation, CBuilding *pBuilding )
{
	return new CFormationIdleBuildingState( pFormation, pBuilding );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationIdleBuildingState::CFormationIdleBuildingState( CFormation *_pFormation, CBuilding *_pBuilding )
: pFormation( _pFormation ), pBuilding( _pBuilding )
{
	pFormation->StopFormationCenter();
	pFormation->SetInBuilding( pBuilding );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationIdleBuildingState::Segment()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationIdleBuildingState::TryInterruptState( CAICommand *pCommand )
{
	if ( !pCommand || pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_LEAVE )
	{
		pFormation->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	const EActionCommand cmdType = pCommand->ToUnitCmd().cmdType;

	if ( cmdType == ACTION_COMMAND_ENTER )
	{
		// the same building or command from mass load
		if ( pFormation->GetBuilding() == pCommand->ToUnitCmd().pObject || pCommand->GetFlag() == 1 )
			return TSIR_NO_COMMAND_INCOMPATIBLE;
	}

	// mass load
	if ( cmdType == ACTION_COMMAND_LOAD && pCommand->GetFlag() == 1 )
		return TSIR_NO_COMMAND_INCOMPATIBLE;
	
	//некотороые команды в данном состоянии невозможны
	if ( cmdType == ACTION_COMMAND_ROTATE_TO || cmdType == ACTION_COMMAND_ROTATE_TO_DIR ||
			 cmdType == ACTION_COMMAND_USE_SPYGLASS || IsRestCommand( cmdType ) || cmdType == ACTION_COMMAND_STAND_GROUND )
		return TSIR_NO_COMMAND_INCOMPATIBLE;

	// эти команды можно выполнить, не выходя из здания
	if ( cmdType == ACTION_COMMAND_CATCH_FORMATION )
	{
		pFormation->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}

	// некоторые команды передать солдатам
	if (	cmdType == ACTION_COMMAND_ATTACK_OBJECT ||
				cmdType == ACTION_COMMAND_ATTACK_UNIT ||
				cmdType == ACTION_COMMAND_AMBUSH ||
				cmdType == ACTION_COMMAND_STOP ||
				cmdType == ACTION_COMMAND_STOP_THIS_ACTION )
	{
		bool bPossible = true;
		for ( int i = 0; i < pFormation->Size(); ++i )
		{
			if ( !(*pFormation)[i]->IsInBuilding() )
			{
				bPossible = false;
				break;
			}
		}
		
		if ( bPossible )		
		{
			for ( int i = 0; i< pFormation->Size(); ++i )
				theGroupLogic.UnitCommand( pCommand->ToUnitCmd(), (*pFormation)[i], false );

			return TSIR_YES_MANAGED_ALREADY;
		}
		else
			return TSIR_NO_COMMAND_INCOMPATIBLE;
	}

	// все остальные команды - через выход из здания
	if ( cmdType != ACTION_COMMAND_WAIT_TO_FORM	&&
			 cmdType != ACTION_COMMAND_DISBAND_FORMATION &&
			 cmdType != ACTION_COMMAND_FORM_FORMATION )
	{
		int nEntrance;
		CVec2 vPoint( GetGoPointByCommand( pCommand->ToUnitCmd() ) );
		if ( pBuilding->ChooseEntrance( pFormation, vPoint, &nEntrance ) )
		{
			vPoint = pBuilding->GetEntrancePoint( nEntrance );
			theGroupLogic.PushFrontUnitCommand( pCommand->ToUnitCmd(), pFormation );
			theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_COMMAND_LEAVE, vPoint ), pFormation );
		}
		else
			return TSIR_NO_COMMAND_INCOMPATIBLE;
	}
	else
		theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pFormation );

	pFormation->SetCommandFinished();
	return TSIR_YES_MANAGED_ALREADY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationIdleBuildingState::GetPurposePoint() const
{
	return pFormation->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*								CFormationIdleEntrenchmentState										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationIdleEntrenchmentState::Instance( CFormation *pFormation, CEntrenchment *pEntrenchment )
{
	return new CFormationIdleEntrenchmentState( pFormation, pEntrenchment );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationIdleEntrenchmentState::CFormationIdleEntrenchmentState( CFormation *_pFormation, CEntrenchment *_pEntrenchment )
: pFormation( _pFormation ), pEntrenchment( _pEntrenchment )
{
	pFormation->StopFormationCenter();
	pFormation->SetInEntrenchment( pEntrenchment );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationIdleEntrenchmentState::Segment()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationIdleEntrenchmentState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand || pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_MOVE_TO )
	{
		pFormation->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}

	const EActionCommand cmdType = pCommand->ToUnitCmd().cmdType;

	if ( cmdType == ACTION_COMMAND_ENTER )
	{
		// the same entrehcment or command from mass load
		if ( pFormation->GetEntrenchment() == pCommand->ToUnitCmd().pObject || pCommand->GetFlag() == 1 )
			return TSIR_NO_COMMAND_INCOMPATIBLE;
	}

		// mass load
	if ( cmdType == ACTION_COMMAND_LOAD && pCommand->GetFlag() == 1 )
		return TSIR_NO_COMMAND_INCOMPATIBLE;

	//некотороые команды в данном состоянии невозможны
	if ( cmdType == ACTION_COMMAND_ROTATE_TO || cmdType == ACTION_COMMAND_ROTATE_TO_DIR || 
			 IsRestCommand( cmdType ) || cmdType == ACTION_COMMAND_STAND_GROUND )
		return TSIR_NO_COMMAND_INCOMPATIBLE;

		// эти команды можно выполнить, не выходя из здания
	if ( cmdType == ACTION_COMMAND_CATCH_FORMATION )
	{
		pFormation->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}

	// некоторые команды передать солдатам
	if (	cmdType == ACTION_COMMAND_ATTACK_OBJECT ||
				cmdType == ACTION_COMMAND_ATTACK_UNIT ||
				cmdType == ACTION_COMMAND_AMBUSH ||
				cmdType == ACTION_COMMAND_USE_SPYGLASS ||
				cmdType == ACTION_COMMAND_STOP ||
				cmdType == ACTION_COMMAND_STOP_THIS_ACTION )
	{
		bool bPossible = true;
		for ( int i = 0; i < pFormation->Size(); ++i )
		{
			if ( !(*pFormation)[i]->IsInEntrenchment() )
			{
				bPossible = false;
				break;
			}
		}
		
		if ( bPossible )
		{
			for ( int i=0; i< pFormation->Size(); ++i )
				theGroupLogic.UnitCommand( pCommand->ToUnitCmd(), (*pFormation)[i], false );

			return TSIR_YES_MANAGED_ALREADY;
		}
		else
			return TSIR_NO_COMMAND_INCOMPATIBLE;
	}

	// все остальные команды - через выход из окопа.
	if ( cmdType != ACTION_COMMAND_WAIT_TO_FORM	&&
			 cmdType != ACTION_COMMAND_DISBAND_FORMATION &&
			 cmdType != ACTION_COMMAND_FORM_FORMATION )
	{
		theGroupLogic.PushFrontUnitCommand( pCommand->ToUnitCmd(), pFormation );

		CVec2 vPos;
		pEntrenchment->GetClosestPoint( GetGoPointByCommand( pCommand->ToUnitCmd() ), &vPos );
		theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_COMMAND_LEAVE, vPos ), pFormation );
	}
	else
		theGroupLogic.InsertUnitCommand( pCommand->ToUnitCmd(), pFormation );

	pFormation->SetCommandFinished();

	return TSIR_YES_MANAGED_ALREADY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationIdleEntrenchmentState::GetPurposePoint() const
{
	return pFormation->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CFormationLeaveBuildingState										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationLeaveBuildingState::Instance( CFormation *pFormation, CBuilding *pBuilding, const CVec2 &point )
{
	return new CFormationLeaveBuildingState( pFormation, pBuilding, point );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationLeaveBuildingState::CFormationLeaveBuildingState( CFormation *_pFormation, CBuilding *_pBuilding, const CVec2 &_point )
: pFormation( _pFormation ), pBuilding( _pBuilding ), point( _point )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationLeaveBuildingState::Segment()
{
	if ( pBuilding->GetLastLeaveTime( pFormation->GetPlayer() ) + 2000 < curTime )
	{
		pBuilding->SetLastLeaveTime( pFormation->GetPlayer() );
		
		int nEntrance;
		if ( pBuilding->ChooseEntrance( pFormation, point, &nEntrance ) )
		{
			for ( int i = 0; i < pFormation->Size(); ++i )
			{
				CSoldier *pSoldier = (*pFormation)[i];
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), pSoldier, false );
				if ( !pSoldier->IsFree() )
				{
					if ( pSoldier->IsInSolidPlace() )
					{
						pSoldier->SetCoordWOUpdate( CVec3( pBuilding->GetEntrancePoint( nEntrance ), 0 ) );
						updater.Update( ACTION_NOTIFY_PLACEMENT, pSoldier );
					}
					else 
						pSoldier->SetNewCoordinates( CVec3( pBuilding->GetEntrancePoint( nEntrance ), 0 ) );

					pBuilding->GoOutFromEntrance( nEntrance, pSoldier );
					pSoldier->SetFree();
				}

				pSoldier->SetCommandFinished();
				pSoldier->FreezeByState( false );
			}

			pFormation->SetNewCoordinates( CVec3( pBuilding->GetEntrancePoint( nEntrance ), 0 ) );
			
			CVec2 vRand;
			RandUniformlyInCircle( 1.5f * SConsts::TILE_SIZE, &vRand );
			theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_COMMAND_MOVE_TO, point.x + vRand.x, point.y + vRand.y ), pFormation );

			pFormation->SetFree();
		}

		pFormation->SetCommandFinished();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationLeaveBuildingState::TryInterruptState( class CAICommand *pCommand )
{
	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CFormationLeaveEntrenchmentState								*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationLeaveEntrenchmentState::Instance( CFormation *pFormation, CEntrenchment *pEntrenchment, const CVec2 &point )
{
	return new CFormationLeaveEntrenchmentState( pFormation, pEntrenchment, point );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationLeaveEntrenchmentState::CFormationLeaveEntrenchmentState( CFormation *_pFormation, CEntrenchment *_pEntrenchment, const CVec2 &_point )
: pFormation( _pFormation ), pEntrenchment( _pEntrenchment ), point( _point )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationLeaveEntrenchmentState::Segment()
{
	CVec2 startPoint;
	pEntrenchment->GetClosestPoint( point, &startPoint );

	// найти первую точку пути вне окопа
	CPtr<IStaticPath> pPath = CreateStaticPathToPoint( startPoint, point, VNULL2, pFormation );
	if ( pPath != 0 )
	{
		int nFirePlace = 0;
		const int nFirePlaces = pEntrenchment->GetNFirePlaces();
		for ( int i = 0; i < pFormation->Size(); ++i )
		{
			CSoldier *pSoldier = (*pFormation)[i];
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), pSoldier, false );
			if ( !pSoldier->IsFree() )
			{
				if ( pSoldier->IsInSolidPlace() )
				{
					const CVec3 vPointToGo = 
						nFirePlaces == 0 ? CVec3( pSoldier->GetCenter(), 0.0f ) : 
															 CVec3( pEntrenchment->GetFirePlaceCoord( nFirePlace ), 0.0f );
					if ( nFirePlaces )
						nFirePlace = ( nFirePlace + 1 ) % nFirePlaces;

					pSoldier->SetCoordWOUpdate( vPointToGo );
				}
				else
					pSoldier->SetNewCoordinates( CVec3( pSoldier->GetCenter(), 0.0f ) );

				pEntrenchment->DelInsider( pSoldier );				
				pSoldier->SetFree();
			}
		}

		pFormation->SetNewCoordinates( CVec3( startPoint.x, startPoint.y, 0 ) );
		pFormation->SetFree();
		pFormation->TurnToDir( -GetDirectionByVector( point - startPoint ) );

		CVec2 vRand;
		RandUniformlyInCircle( 1.5f * SConsts::TILE_SIZE, &vRand );
		theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_COMMAND_MOVE_TO, point.x + vRand.x, point.y + vRand.y ), pFormation );
	}

	pFormation->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationLeaveEntrenchmentState::TryInterruptState( class CAICommand *pCommand )
{
	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CFormationAttackUnitState										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationAttackUnitState::Instance( CFormation *pFormation, CAIUnit *pEnemy, const bool bSwarmAttack )
{
	return new CFormationAttackUnitState( pFormation, pEnemy, bSwarmAttack );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationAttackUnitState::CFormationAttackUnitState( CFormation *_pFormation, CAIUnit *_pEnemy, const bool _bSwarmAttack )
: pFormation( _pFormation ), pEnemy( _pEnemy ), eState( EPM_MOVING ), bSwarmAttack( _bSwarmAttack ),
	nEnemyParty( _pEnemy->GetParty() )
{
/*	
	if ( bSwarmAttack )
	{
		for ( int i = 0; i < pFormation->Size(); ++i )
			(*pFormation)[i]->ResetTargetScan();
	}
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationAttackUnitState::SetToWaitingState()
{
	eState = EPM_WAITING;
	pFormation->StopFormationCenter();
	pFormation->SetToWaitingState();

	const int nSize = pFormation->Size();
	for ( int i = 0; i < nSize; ++i )
	{
		CSoldier *pSoldier = (*pFormation)[i];
		const bool bOfficer = pSoldier->GetStats()->type == RPG_TYPE_OFFICER;
		const bool bShouldSendAttackCommand = 
			nSize == 1 || 
			!bOfficer ||
			bOfficer && fabs2( pSoldier->GetCenter() - pEnemy->GetCenter() ) < sqr( SConsts::OFFICER_COEFFICIENT_FOR_SCAN ) * sqr( pSoldier->GetGun( 0 )->GetFireRange( 0 ) );

		if ( bShouldSendAttackCommand )
		{
			SAIUnitCmd cmd( ACTION_COMMAND_ATTACK_UNIT, pEnemy, bSwarmAttack );
			if ( pFormation->GetCurCmd() && pFormation->GetCurCmd()->IsValid() && pFormation->Size() == 1 )
				cmd.bFromAI = pFormation->GetCurCmd()->ToUnitCmd().bFromAI;

			theGroupLogic.UnitCommand( cmd, (*pFormation)[i], false );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationAttackUnitState::SetToMovingState()
{
	if ( CPtr<IStaticPath> pStaticPath = CreateStaticPathForAttack( pFormation, pEnemy, 0, pFormation->GetMaxFireRange() ) )
	{
		eState = EPM_MOVING;
		StopAllUnits( pFormation );

		pFormation->SendAlongPath( pStaticPath, VNULL2 );
	}
	else
	{
		pFormation->SendAcknowledgement( ACK_CANNOT_FIND_PATH_TO_TARGET );
		TryInterruptState( 0 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationAttackUnitState::Segment()
{
	if ( !IsValidObj( pEnemy ) )
		TryInterruptState( 0 );
	else
	{
		switch ( eState )
		{
			case EPM_MOVING:
				if ( bSwarmAttack )
				{
					for ( int i = 0; i < pFormation->Size(); ++i )
						(*pFormation)[i]->AnalyzeTargetScan( pEnemy, false, false );
				}

				if ( IsFormationCloseToEnemy( pEnemy->GetCenter(), pFormation ) )
					SetToWaitingState();
				else if ( pFormation->IsIdle() )
					SetToMovingState();

				break;
			case EPM_WAITING:
				if ( pFormation->IsEveryUnitResting() || !IsValidObj( pEnemy ) ||
						 pEnemy->GetParty() != nEnemyParty )
					TryInterruptState( 0 );

				break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationAttackUnitState::TryInterruptState( class CAICommand *pCommand )
{
	pFormation->UnsetFromWaitingState();
	StopAllUnits( pFormation );
	pFormation->SetCommandFinished();

	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationAttackUnitState::GetPurposePoint() const
{
	if ( IsValidObj( pEnemy ) )
		return pEnemy->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitStateNames CFormationAttackUnitState::GetName()
{
	if ( !IsValidObj( pEnemy ) || pEnemy->IsFree() )
		return EUSN_ATTACK_UNIT;
	else 
		return EUSN_ATTACK_UNIT_IN_BUILDING;
}
//*******************************************************************
//*										CFormationAttackCommonStatObjState						*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationAttackCommonStatObjState::Instance( CFormation *pFormation, CStaticObject *pObj )
{
	return new CFormationAttackCommonStatObjState( pFormation, pObj );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationAttackCommonStatObjState::CFormationAttackCommonStatObjState( CFormation *_pFormation, CStaticObject *_pObj )
: pFormation( _pFormation ), pObj( _pObj ), eState( EPM_START )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationAttackCommonStatObjState::SetToWaitingState()
{
	eState = EPM_WAITING;
	pFormation->StopFormationCenter();
	pFormation->SetToWaitingState();

	for ( int i = 0; i < pFormation->Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ATTACK_OBJECT, pObj ), (*pFormation)[i], false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationAttackCommonStatObjState::Segment()
{
	if ( !IsValidObj( pObj ) )
		TryInterruptState( 0 );
	else
	{
		switch ( eState )
		{
			case EPM_START:
				if ( IsFormationCloseToEnemy( pObj->GetAttackCenter( pFormation->GetCenter() ), pFormation ) )
					SetToWaitingState();
				else if ( CPtr<IStaticPath> pPath = CreateStaticPathToPoint( pObj->GetAttackCenter( pFormation->GetCenter() ), VNULL2, pFormation, true ) )
				{
					StopAllUnits( pFormation );
					pFormation->SendAlongPath( pPath, VNULL2 );
					eState = EPM_MOVING;
				}
				else
				{
					pFormation->SendAcknowledgement( ACK_CANNOT_FIND_PATH_TO_TARGET );
					TryInterruptState( 0 );
				}

			case EPM_MOVING:
				if ( IsFormationCloseToEnemy( pObj->GetAttackCenter( pFormation->GetCenter() ), pFormation ) )
					SetToWaitingState();
				else if ( pFormation->IsIdle() )
					eState = EPM_START;

				break;
			case EPM_WAITING:
				if ( pFormation->IsEveryUnitResting() || !IsValidObj( pObj ) )
					TryInterruptState( 0 );

				break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationAttackCommonStatObjState::TryInterruptState( class CAICommand *pCommand )
{
	pFormation->UnsetFromWaitingState();
	StopAllUnits( pFormation );
	pFormation->SetCommandFinished();

	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationAttackCommonStatObjState::GetPurposePoint() const
{
	if ( IsValidObj( pObj ) && pFormation && pFormation->IsValid() && pFormation->IsAlive() )
		return pObj->GetAttackCenter( pFormation->GetCenter() );
	else
		return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationRotateState													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationRotateState::Instance( CFormation *pFormation, const WORD wDir )
{
	return new CFormationRotateState( pFormation, wDir );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationRotateState::CFormationRotateState( CFormation *_pFormation, const WORD wDir )
: pFormation( _pFormation )
{
	// повернуть формацию
	pFormation->TurnToDir( wDir, false );

	// сказать каждому юниту формации построиться
	const SAIUnitCmd paradeCmd( ACTION_COMMAND_PARADE );
	for ( int i = 0; i < pFormation->Size(); ++i )
		theGroupLogic.UnitCommand( paradeCmd, (*pFormation)[i], false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationRotateState::Segment()
{
	int i = 0;
	while ( i < pFormation->Size() && (*pFormation)[i]->GetState()->GetName() == EUSN_REST )
		++i;

	if ( i == pFormation->Size() )
		pFormation->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationRotateState::TryInterruptState( class CAICommand *pCommand )
{
	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationRotateState::GetPurposePoint() const
{
	return pFormation->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CFormationEnterTransportState										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationEnterTransportState::Instance( CFormation *pFormation, CMilitaryCar *pTransport )
{
	return new CFormationEnterTransportState( pFormation, pTransport );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationEnterTransportState::CFormationEnterTransportState( CFormation *_pFormation, CMilitaryCar *_pTransport )
: pFormation( _pFormation ), pTransport( _pTransport ), eState( EETS_START ), lastCheck( curTime ), 
	lastTransportPos( _pTransport->GetCenter() ), lastTransportDir( _pTransport->GetFrontDir() )
{
	if ( pTransport->GetNAvailableSeats() < pFormation->Size() )
		pFormation->SendAcknowledgement( 0, ACK_NEGATIVE );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationEnterTransportState::SetPathToRunUp()
{
	lastTransportPos = pTransport->GetCenter();
	CPtr<IStaticPath> pPath = CreateStaticPathToPoint( pTransport->GetEntrancePoint(), VNULL2, pFormation, true );
	if ( pPath != 0 )
	{
		pFormation->SendAlongPath( pPath, VNULL2 );
		return true;
	}
	else
		return false;
}	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterTransportState::SendUnitsToTransport()
{
	pTransport->StopUnit();
	pFormation->StopUnit();
	for ( int i = 0; i < pFormation->Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ENTER_TRANSPORT_NOW, pTransport ), (*pFormation)[i], false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationEnterTransportState::IsAllUnitsInside()
{
	int i = 0;
	while ( i < pFormation->Size() && (*pFormation)[i]->IsInTransport() )
		++i;

	return i >= pFormation->Size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterTransportState::SetTransportToWaitState()
{
	if ( pTransport->GetState()->GetName() == EUSN_WAIT_FOR_PASSENGER )
		static_cast<CTransportWaitPassengerState*>(pTransport->GetState())->AddFormationToWait( pFormation );
	else
	{
		SAIUnitCmd cmd( ACTION_COMMAND_WAIT_FOR_UNITS, pFormation  );
		theGroupLogic.UnitCommand( cmd, pTransport, false );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationEnterTransportState::IsAllTransportTurretsReturned() const
{
	for ( int i = 0; i < pTransport->GetNTurrets(); ++i )
	{
		if ( pTransport->GetTurret( i )->GetHorCurAngle() != 0 )
			return false;
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterTransportState::Segment()
{
	if ( !IsValidObj( pTransport ) || eState != EETS_WAITING && pTransport->GetNAvailableSeats() < pFormation->Size() )
	{
		pFormation->SendAcknowledgement( ACK_NEGATIVE );
		pFormation->SetCommandFinished();
	}
	else 
	{
		switch ( eState )
		{
			case EETS_START:
				if ( SetPathToRunUp() )
					eState = EETS_MOVING;
				else
				{
					pFormation->SendAcknowledgement( ACK_NEGATIVE );
					pFormation->SetCommandFinished();
				}
				break;
			case EETS_MOVING:
				{
					const bool bTransportMoved = curTime - lastCheck >= CHECK_PERIOD && fabs( pTransport->GetCenter() - lastTransportPos ) >= SConsts::TILE_SIZE;
					const float fAddLength = ( pFormation->Size() == 1 ) ? ( 4 * SConsts::TILE_SIZE ) : ( SConsts::TILE_SIZE + pFormation->GetRadius() );
					const bool bCloseToEntrance = fabs( pFormation->GetCenter() - pTransport->GetEntrancePoint() ) < fAddLength;

					if ( bCloseToEntrance || pFormation->IsIdle() || bTransportMoved || lastTransportDir != pTransport->GetFrontDir() )
					{
						if ( bCloseToEntrance )
							eState = EETS_WAIT_TO_UNLOCK_TRANSPORT;
						else if ( pFormation->IsIdle() || bTransportMoved || lastTransportDir != pTransport->GetFrontDir() )
							eState = EETS_START;

						lastCheck = curTime;
						lastTransportPos = pTransport->GetCenter();
						lastTransportDir = pTransport->GetFrontDir();
					}
				}

				break;
			case EETS_WAIT_TO_UNLOCK_TRANSPORT:
				if ( !pTransport->IsLocked() )
				{
					SetTransportToWaitState();
					pTransport->Lock( pFormation );
					eState = EETS_WAIT_FOR_TURRETS_RETURN;
				}

				break;
			case EETS_WAIT_FOR_TURRETS_RETURN:
				if ( IsAllTransportTurretsReturned() )
				{
					SendUnitsToTransport();
					eState = EETS_WAITING;
				}

				break;
			case EETS_WAITING:
				if ( IsAllUnitsInside() )
				{
					eState = EETS_FINISHED;
					theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRANSPORT, pTransport ), pFormation, false );
					pTransport->Unlock();
				}

				break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationEnterTransportState::TryInterruptState( class CAICommand *pCommand )
{
	if ( pCommand && pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_LOAD &&
			 pTransport == pCommand->ToUnitCmd().pObject )
		return TSIR_NO_COMMAND_INCOMPATIBLE;

	if ( eState != EETS_WAITING && eState != EETS_WAIT_FOR_TURRETS_RETURN )
	{
		pFormation->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else
	{
		int i = 0;
		while ( i < pFormation->Size() && !(*pFormation)[i]->IsInTransport() )
			++i;

		if ( i < pFormation->Size() )
			return TSIR_YES_WAIT;
		else
		{
			for ( int i = 0; i < pFormation->Size(); ++i )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), (*pFormation)[i], false );

			pTransport->Unlock();
			pFormation->SetCommandFinished();

			return TSIR_YES_IMMIDIATELY;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationEnterTransportState::GetPurposePoint() const
{
	if ( IsValidObj( pTransport ) )
		return pTransport->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CFormationIdleTransportState										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationIdleTransportState::Instance( CFormation *pFormation, CMilitaryCar *pTransport )
{
	return new CFormationIdleTransportState( pFormation, pTransport );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationIdleTransportState::CFormationIdleTransportState( CFormation *_pFormation, CMilitaryCar *_pTransport )
: pFormation( _pFormation ), pTransport( _pTransport )
{
	pFormation->SetInTransport( pTransport );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationIdleTransportState::Segment()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationIdleTransportState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand ||
			pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_MOVE_TO  || 
			pCommand->ToUnitCmd().cmdType == ACTION_MOVE_REPAIR_UNIT ||
			pCommand->ToUnitCmd().cmdType == ACTION_MOVE_RESUPPLY_UNIT )
	{
		pFormation->SetCommandFinished();
		pFormation->SetFree();
		return TSIR_YES_IMMIDIATELY;
	}

	return TSIR_NO_COMMAND_INCOMPATIBLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationIdleTransportState::GetPurposePoint() const
{
	if ( IsValidObj( pTransport ) )
		return pTransport->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CFormationEnterTransportByCheatPathState*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationEnterTransportByCheatPathState::Instance( class CFormation *_pFormation, class CMilitaryCar *_pTransport )
{
	return new CFormationEnterTransportByCheatPathState( _pFormation, _pTransport );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationEnterTransportByCheatPathState::CFormationEnterTransportByCheatPathState( class CFormation *_pFormation, class CMilitaryCar *_pTransport )
: pFormation( _pFormation ), pTransport( _pTransport )
{
	// send all squad soldiers to transport via cheat path
	const int nSoldiers = pFormation->Size();
	for ( int i = 0; i < nSoldiers; ++i )
	{
		CSoldier *pSoldier = (*pFormation)[i];
		//pSoldier->AddRef()
		pSoldier->SetCurPath( new CArtilleryCrewPath( pSoldier, pSoldier->GetCenter(), pTransport->GetEntrancePoint(), pSoldier->GetMaxPossibleSpeed() ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterTransportByCheatPathState::Segment()
{
	// wait while all is near transport,
	// set them inside and set squad inside.
	const int nSoldiers = pFormation->Size();
	bool bAllInTransport = true;

	for ( int i = 0; i < nSoldiers; ++i )
	{
		CSoldier *pSoldier = (*pFormation)[i];
		if ( pSoldier->GetCurPath()->IsFinished() && !pSoldier->IsInTransport() )
		{
			pSoldier->RestoreDefaultPath();
			pSoldier->SetInTransport( pTransport );
			pTransport->AddPassenger( pSoldier );
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRANSPORT, pTransport ), pSoldier, false );
		}
		if ( !pSoldier->IsInTransport() )
		{
			bAllInTransport = false;
		}
	}

	if ( bAllInTransport )
	{
		pFormation->SetCommandFinished();
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRANSPORT, pTransport ), pFormation, false );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationEnterTransportByCheatPathState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand )
	{
		pFormation->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else
		return TSIR_YES_WAIT;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationEnterTransportByCheatPathState::GetPurposePoint() const 
{ 
	return pTransport->GetEntrancePoint(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									CFormationEnterTransportNowState								*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationEnterTransportNowState::Instance( CFormation *pFormation, CMilitaryCar *pTransport )
{
	return new CFormationEnterTransportNowState( pFormation, pTransport );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationEnterTransportNowState::CFormationEnterTransportNowState( CFormation *_pFormation, CMilitaryCar *_pTransport )
: pFormation( _pFormation ), pTransport( _pTransport )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterTransportNowState::Segment()
{
	for ( int i = 0; i < pFormation->Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRANSPORT, pTransport ), (*pFormation)[i], false );

	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRANSPORT, pTransport ), pFormation, false );

	pFormation->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationEnterTransportNowState::TryInterruptState( class CAICommand *pCommand )
{
	return TSIR_YES_WAIT;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationEnterTransportNowState::GetPurposePoint() const
{
	if ( IsValidObj( pTransport ) )
		return pTransport->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationCatchTransportState									*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationCatchTransportState::Instance( CFormation *pUnit, CAITransportUnit *pTransportToCatch, float fResursPerSoldier)
{
	return new CFormationCatchTransportState( pUnit, pTransportToCatch, fResursPerSoldier );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationCatchTransportState::CFormationCatchTransportState( class CFormation *_pUnit, class CAITransportUnit *_pTransportToCatch, float fResursPerSoldier )
: pUnit ( _pUnit ), pTransportToCatch( _pTransportToCatch ), vEnterPoint( -1, -1 ),
	fResursPerSoldier(  fResursPerSoldier ), timeLastUpdate( curTime )
{
	if ( !pTransportToCatch || !IsValidObj( pTransportToCatch ) )
	{
		pUnit->SendAcknowledgement( ACK_NEGATIVE, true );
		pUnit->SetCommandFinished();
	}
	else
	{
		pUnit->StopUnit();
		for ( int i = 0; i < pUnit->Size(); ++i )
		{
			NI_ASSERT_T( dynamic_cast<CSoldier*>((*pUnit)[i])!=0, "not soldier attempting to catch transport");
			CSoldier * pSold = static_cast<CSoldier*>((*pUnit)[i]);
			pSold->RestoreDefaultPath();
			pSold->StopUnit();
		}
			
		eState = E_SENDING;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCatchTransportState::Segment()
{
	if ( !IsValidObj( pTransportToCatch ) )
		Interrupt();
	else 
	if ( curTime > timeLastUpdate + 100 )
	{
		timeLastUpdate = curTime ;

		switch( eState )
		{
		case E_SENDING:
			for ( int i = 0; i < pUnit->Size(); ++i )
			{
				NI_ASSERT_T( dynamic_cast<CSoldier*>((*pUnit)[i])!=0, "not soldier attempting to catch transport");
				CSoldier * pSold = static_cast<CSoldier*>((*pUnit)[i]);
				UpdatePath( pSold, true );
			}
			
			eState = E_CHECKING;
			break;
		case E_CHECKING:
			{
				int nNotInTransport = 0;//number of solders, that didn't catch transport yet.
				SRect transpRect = pTransportToCatch->GetUnitRect();
				for ( int i = 0; i < pUnit->Size(); ++i )
				{
					NI_ASSERT_T( dynamic_cast<CSoldier*>((*pUnit)[i]) != 0, "not soldier attempting to catch transport");
					CSoldier *pSold = static_cast<CSoldier*>( (*pUnit)[i] );
					if ( pSold && pSold->IsAlive() )
					{
						++nNotInTransport;
						if ( IsUnitNearUnit( pSold, pTransportToCatch ) )
							deleted.push_back( pSold );
						else
							UpdatePath( pSold, pSold->IsIdle() );
					}
				}
				while ( !deleted.empty() )
				{
					theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_DISAPPEAR), *deleted.begin(), false );
					deleted.pop_front();

					float fRes = pTransportToCatch->GetResursUnitsLeft() + fResursPerSoldier;
					if ( fRes < SConsts::TRANSPORT_RU_CAPACITY )
						pTransportToCatch->SetResursUnitsLeft( fRes );
					else
						pTransportToCatch->SetResursUnitsLeft( SConsts::TRANSPORT_RU_CAPACITY );
				}
				
				if ( 0 != nNotInTransport ) //не все добежали
					vEnterPoint = pTransportToCatch->GetEntrancePoint();
			}

			break;
		}
		vEnterPoint = pTransportToCatch->GetEntrancePoint();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCatchTransportState::Interrupt()
{
	if ( pUnit->IsIdle() )
		pUnit->StopUnit();
	pUnit->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationCatchTransportState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand )
	{
		pUnit->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else
		return TSIR_NO_COMMAND_INCOMPATIBLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCatchTransportState::UpdatePath( CSoldier * pSold, const bool bForce )
{
	const CVec2 vPt( pTransportToCatch->GetEntrancePoint() );
	if ( vEnterPoint != vPt || bForce )
	{
		CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( vPt, VNULL2, pSold, true );
		if ( pStaticPath )
			pSold->SendAlongPath( pStaticPath, VNULL2 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationCatchTransportState::GetPurposePoint() const
{
	if ( IsValidObj( pTransportToCatch ) )
		return pTransportToCatch->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationParaDropState												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationParaDropState::Instance( CFormation *pFormation )
{
	return new CFormationParaDropState( pFormation );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationParaDropState::CFormationParaDropState( class CFormation *pFormation )
: pFormation( pFormation ), eState( EPS_WAIT_FOR_PARADROP_BEGIN )
{
	pFormation->SetSelectable( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationParaDropState::Segment()
{
	switch( eState )
	{
	case EPS_WAIT_FOR_PARADROP_BEGIN:
		for ( int i = 0; i < pFormation->Size(); ++i )
		{
			if ( EUSN_PARTROOP == (*pFormation)[i]->GetState()->GetName() )
			{
				//кто-то уже выпрыгнул
				eState = EPS_WAIT_FOR_PARADROP_END;
				return;
			}
		}
		break;
	case EPS_WAIT_FOR_PARADROP_END:
		{
			for ( int i = 0; i < pFormation->Size(); ++i )
			{
				if ( EUSN_PARTROOP == (*pFormation)[i]->GetState()->GetName() )
				{
					//кто-то еще не долетел
					return;
				}
			}
			CSoldier * pCenter = (*pFormation)[pFormation->Size()/2];
			pFormation->SetNewCoordinates( CVec3(pCenter->GetCenter(), pCenter->GetZ()) );
			pFormation->SetCommandFinished();
			pFormation->SetSelectable( pFormation->GetPlayer() == theDipl.GetMyNumber() );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationParaDropState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand )
	{
		pFormation->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else if ( pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_PARADE )
		return TSIR_YES_WAIT;
	else
		return TSIR_NO_COMMAND_INCOMPATIBLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationParaDropState::GetPurposePoint() const
{
	return pFormation->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationAttackFormationState								*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationAttackFormationState::Instance( CFormation *pFormation, CFormation *pTarget, const bool bSwarmAttack )
{
	return new CFormationAttackFormationState( pFormation, pTarget, bSwarmAttack );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationAttackFormationState::CFormationAttackFormationState( CFormation *pFormation, CFormation *_pTarget, const bool _bSwarmAttack )
: pUnit( pFormation ), pTarget( _pTarget ), bSwarmAttack( _bSwarmAttack ), nEnemyParty( _pTarget->GetParty() )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationAttackFormationState::Segment()
{
	if ( !IsValidObj( pTarget ) || pTarget->Size() == 0 || pTarget->GetParty() != nEnemyParty )
	{
	}
	else
	{
		if ( bSwarmAttack )
		{
			theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_MOVE_SWARM_ATTACK_FORMATION, pTarget), pUnit );
			theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_COMMAND_SWARM_ATTACK_UNIT, (*pTarget)[0]), pUnit );
		}
		else
		{
			theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_MOVE_ATTACK_FORMATION, pTarget), pUnit );
			theGroupLogic.PushFrontUnitCommand( SAIUnitCmd( ACTION_COMMAND_ATTACK_UNIT, (*pTarget)[0]), pUnit );
		}
	}

	TryInterruptState( 0 ) ;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationAttackFormationState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationAttackFormationState::GetPurposePoint() const
{
	if ( IsValidObj( pTarget ) )
		return pTarget->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationParadeState													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationParadeState::Instance( CFormation *pFormation, const int nType )
{
	return new CFormationParadeState( pFormation, nType );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationParadeState::CFormationParadeState( CFormation *_pFormation, const int nType )
: pFormation( _pFormation ), startTime( curTime )
{
	if ( nType != -1 )
	{
		int nGeometry = 0;
		while ( nGeometry < pFormation->GetStats()->formations.size() &&
						pFormation->GetStats()->formations[nGeometry].type != nType )
			++nGeometry;

		if ( nGeometry < pFormation->GetStats()->formations.size() &&
				 nGeometry < pFormation->GetNGeometries() )
		{
			pFormation->ChangeGeometry( nGeometry );

			for ( int i = 0; i < pFormation->Size(); ++i )
			{
				CSoldier *pSold = (*pFormation)[i];
				pSold->FreezeByState( false );
				if ( pSold->GetStats()->type == RPG_TYPE_SNIPER )
				{
					CSniper *pSniper = checked_cast<CSniper*>(pSold);
					if ( nType == 4 )
					{
						pSniper->SetCamoulfage();
						pSniper->SetSneak( true );
					}
					else
						pSniper->SetSneak( false );
				}

				if ( pSold->GetState()->GetName() == EUSN_REST )
					checked_cast<CSoldierRestState*>(pSold->GetState())->SetNullLastMoveTime();
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationParadeState::Segment()
{
	if ( startTime + 1000 < curTime && pFormation->IsEveryUnitResting() )
		pFormation->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationParadeState::TryInterruptState( class CAICommand *pCommand )
{
	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationParadeState::GetPurposePoint() const
{
	return pFormation->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationDisbandState												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationDisbandState::Instance( CFormation *pFormation )
{
	return new CFormationDisbandState( pFormation );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationDisbandState::CFormationDisbandState( CFormation *_pFormation )
: pFormation( _pFormation )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationDisbandState::Segment()
{
	const int nGroup = pFormation->GetNGroup();
	IRefCount **pUnitsBuffer = 0;
	if ( nGroup > 0 )
		theGroupLogic.DelUnitFromGroup( pFormation );
	
	pFormation->StopUnit();
	for ( int i = 0; i < pFormation->Size(); ++i )
	{
		CSoldier *pSoldier = (*pFormation)[i];
		pSoldier->MemCurFormation();
		CCommonUnit *pSingleFormation = theUnitCreation.CreateSingleUnitFormation( pSoldier );

		if ( pSoldier->IsFree() )
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_GUARD, CVec2( -1.0f, -1.0f ), 0 ), pSingleFormation, false );
		else if ( pSoldier->IsInTransport() )
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRANSPORT, pSoldier->GetTransportUnit() ), pSingleFormation, false );
		else if ( pSoldier->IsInBuilding() )
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_BUILDING, pSoldier->GetBuilding() ), pSingleFormation, false );
		else if ( pSoldier->IsInEntrenchment() )
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRENCH, pSoldier->GetEntrenchment() ), pSingleFormation, false );

		if ( nGroup > 0 )
			theGroupLogic.AddUnitToGroup( pSingleFormation, nGroup );
	}

	pFormation->Disable();
	pFormation->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationDisbandState::TryInterruptState( CAICommand *pCommand )
{
	return TSIR_NO_COMMAND_INCOMPATIBLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationDisbandState::GetPurposePoint() const
{
	return pFormation->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationFormState														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationFormState::Instance( CFormation *pFormation )
{
	return new CFormationFormState( pFormation );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationFormState::CFormationFormState( CFormation *_pFormation )
: pFormation( _pFormation )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationFormState::Segment()
{
	CSoldier *pMainSoldier = (*pFormation)[0];
	NI_ASSERT_T( pMainSoldier->IsFree() || pMainSoldier->IsInEntrenchment() || pMainSoldier->IsInBuilding(), "Wrong soldier state for command FORM_FORMATION" );

	CFormation *pMemFormation = pMainSoldier->GetMemFormation();
	
	int i = 0;
	while ( i < pMemFormation->Size() && !(*pMemFormation)[i]->IsInBuilding() )
		++i;

	if ( i < pMemFormation->Size() )
	{
		for ( int j = 0; j < pMemFormation->Size(); ++j )
			(*pMemFormation)[j]->GetFormation()->SendAcknowledgement( ACK_NEGATIVE );

		pFormation->SetCommandFinished();
	}
	else
	{
		pMemFormation->SetNewCoordinates( CVec3( pMainSoldier->GetCenter(), pMainSoldier->GetZ() ) );

		// сформировать команду
		SAIUnitCmd cmd;
		if ( pMainSoldier->IsFree() )
			cmd.cmdType = ACTION_COMMAND_MOVE_TO;
		else
		{
			cmd.cmdType = ACTION_COMMAND_ENTER;
			if ( pMainSoldier->IsInEntrenchment() )
			{
				cmd.pObject = pMainSoldier->GetEntrenchment();
				cmd.fNumber = 1;
			}
			else
			{
				cmd.pObject	= pMainSoldier->GetBuilding();
				cmd.fNumber = 0;
			}
		}

		// раздать команду всем
		SAIUnitCmd waitCmd( ACTION_COMMAND_WAIT_TO_FORM, pMainSoldier, 0.0f );
		for ( int i = 0; i < pMemFormation->Size(); ++i )
		{
			CSoldier *pSoldier = (*pMemFormation)[i];
			if ( pSoldier != pMainSoldier && pSoldier->GetFormation()->GetState()->GetName() != EUSN_WAIT_TO_FORM )
			{
				if ( cmd.cmdType == ACTION_COMMAND_MOVE_TO )
					cmd.vPos = pMemFormation->GetUnitCoord( i );

				theGroupLogic.UnitCommand( cmd, pSoldier->GetFormation(), false );
				theGroupLogic.UnitCommand( waitCmd, pSoldier->GetFormation(), true );
				pSoldier->SetWait2FormFlag( true );
			}
		}

		waitCmd.fNumber = 1;
		theGroupLogic.PushFrontUnitCommand( waitCmd, pFormation );
		pMainSoldier->SetWait2FormFlag( true );

		pFormation->SetCommandFinished();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationFormState::TryInterruptState( CAICommand *pCommand )
{
	return TSIR_YES_WAIT;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationFormState::GetPurposePoint() const
{
	return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationWaitToFormState											*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationWaitToFormState::Instance( CFormation *pFormation, const float fMain, class CSoldier *pMainSoldier )
{
	return new CFormationWaitToFormState( pFormation, fMain, pMainSoldier );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationWaitToFormState::CFormationWaitToFormState( CFormation *_pFormation, const float fMain, class CSoldier *_pMainSoldier )
: pFormation( _pFormation ), bMain( fMain == 1.0f ), pMainSoldier( _pMainSoldier )
{
	pFormFormation = pMainSoldier->GetMemFormation();
	startTime = curTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationWaitToFormState::FinishState()
{
	if ( IsValidObj( pFormFormation ) && pFormFormation->Size() > 0 )
	{
		for ( int i = 0; i < pFormFormation->Size(); ++i )
		{
			CSoldier *pSoldier = (*pFormFormation)[i];
			if ( pSoldier != pMainSoldier && pSoldier->IsInWait2Form() )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), pSoldier->GetFormation(), false );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationWaitToFormState::FormFormation()
{
	CVec2 vCenter( VNULL2 );
	int nGroupID = 0;
	for ( int i = 0; i < pFormFormation->Size(); ++i )
	{
		CSoldier *pSoldier = (*pFormFormation)[i];
		CFormation *pSingleFormation = pSoldier->GetFormation();
		if ( pSingleFormation->GetNGroup() != 0 )
		{
			nGroupID = pSingleFormation->GetNGroup();
			theGroupLogic.DelUnitFromGroup( pSingleFormation );
		}
		
		pSingleFormation->DelUnit( pSoldier->GetFormationSlot() );
		if ( pSoldier != pMainSoldier )
			pSingleFormation->Disappear();

		pSoldier->MemorizeFormation();
		pSoldier->SetFormation( pFormFormation, i );
		updater.Update( ACTION_NOTIFY_NEW_FORMATION, pSoldier );

		vCenter += pSoldier->GetCenter();
	}

	vCenter /= float( pFormFormation->Size() );
	
	pFormFormation->KillStatesAndCmdsInfo();
	pFormFormation->Enable();

	if ( nGroupID != 0 )
		theGroupLogic.AddUnitToGroup( pFormFormation, nGroupID );

	if ( pMainSoldier->IsFree() )
		pFormFormation->SetCurState( CFormationRestState::Instance( pFormFormation, CVec2( -1.0f, -1.0f ), 0 ) );
	else if ( pMainSoldier->IsInBuilding() )
		pFormFormation->SetCurState( CFormationIdleBuildingState::Instance( pFormFormation, pMainSoldier->GetBuilding() ) );
	else if ( pMainSoldier->IsInEntrenchment() )
		pFormFormation->SetCurState( CFormationIdleEntrenchmentState::Instance( pFormFormation, pMainSoldier->GetEntrenchment() ) );
	else
		NI_ASSERT_T( false, "Wrong main soldier state" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationWaitToFormState::Segment()
{
	// проверять не сразу же, а когда команда дойдёт до всех солдат формации
	if ( curTime - startTime > 10 * SConsts::AI_SEGMENT_DURATION )
	{
		if ( bMain )
		{
			bool bCanForm = true;
			for ( int i = 0; i < pFormFormation->Size(); ++i )
			{
				// команда на формирование прервана
				if ( !(*pFormFormation)[i]->IsInWait2Form() )
				{
					pFormation->SetCommandFinished();
					return;
				}

				bCanForm &= ( (*pFormFormation)[i]->GetFormation()->GetState()->GetName() == EUSN_WAIT_TO_FORM );
			}

			if ( bCanForm )
			{
				FormFormation();
				pFormation->Disappear();
			}
		}
		else if ( !pMainSoldier->IsInWait2Form() )
			pFormation->SetCommandFinished();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationWaitToFormState::TryInterruptState( CAICommand *pCommand )
{
	if ( pCommand && pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_WAIT_TO_FORM )
		return TSIR_NO_COMMAND_INCOMPATIBLE;
	else
	{
		if ( bMain )	
			FinishState();

		if ( pFormation && pFormation->IsAlive() && pFormation->Size() > 0 )
		{
			if ( IsValidObj( pMainSoldier ) )
				pMainSoldier->SetWait2FormFlag( false );
		}

		pFormation->SetCommandFinished();

		return TSIR_YES_IMMIDIATELY;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationWaitToFormState::GetPurposePoint() const
{
	return CVec2( -1.0f, -1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CCatchFormationState												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CCatchFormationState::Instance( CFormation *pCatchingFormation, CFormation *pFormation )
{
	return new CCatchFormationState( pCatchingFormation, pFormation );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CCatchFormationState::CCatchFormationState( CFormation *_pCatchingFormation, CFormation *_pFormation )
: pCatchingFormation( _pCatchingFormation ), pFormation( _pFormation ), lastUpdateTime( 0 ), eState( ECFS_NONE ), 
	lastFormationPos( _pFormation->GetCenter() ), pLastFormationObject( 0 )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCatchFormationState::MoveSoldierToFormation()
{
	pCatchingFormation->StopUnit();
	CSoldier *pSoldier = (*pCatchingFormation)[0];
	
	pCatchingFormation->DelUnit( (BYTE)0 );

	pFormation->MakeVirtualUnitReal( pSoldier );
	pSoldier->SetSelectable( pFormation->IsSelectable() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCatchFormationState::JoinToFormation()
{
	CSoldier *pSoldier = (*pCatchingFormation)[0];
	
	MoveSoldierToFormation();
	pSoldier->SendAlongPath( 0, VNULL2 );
	pCatchingFormation->Disappear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCatchFormationState::LeaveCurStaticObject()
{
	const CVec2 vFormationCenter = pFormation->GetCenter();
	CSoldier *pSoldier = (*pCatchingFormation)[0];
	
	CVec2 vLeavePoint( -1.0f, -1.0f );
	if ( pSoldier->IsInEntrenchment() )
		vLeavePoint = pSoldier->GetCenter();
	else if ( pSoldier->IsInBuilding() )
	{
		CBuilding *pBuilding = pSoldier->GetBuilding();			
		int nEntrance;
		if ( pBuilding->ChooseEntrance( pFormation, vFormationCenter, &nEntrance ) )
			vLeavePoint = pSoldier->GetBuilding()->GetEntrancePoint( nEntrance );
	}
	else
		NI_ASSERT_T( false, "Wrong soldier state" );

	if ( vLeavePoint.x != -1.0f )
		theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_LEAVE, vLeavePoint ), pCatchingFormation );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCatchFormationState::AnalyzeFreeFormation()
{
	const CVec2 vFormationCenter = pFormation->GetCenter();
	CSoldier *pSoldier = (*pCatchingFormation)[0];
	
	// если нужно откуда-то выбегать
	if ( !pSoldier->IsFree() )
		LeaveCurStaticObject();
	// просто бежать к формации
	else 
	{
		// добежали или формация сдвинулась
		if ( pCatchingFormation->IsIdle() || fabs2( vFormationCenter - lastFormationPos ) >= sqr( float( 2 * SConsts::TILE_SIZE ) ) )
		{
			lastFormationPos = vFormationCenter;
			const float fDist2 = fabs2( vFormationCenter - pCatchingFormation->GetCenter() );
			
			// близко
			if ( fDist2 <= sqr( float( 8 * SConsts::TILE_SIZE ) ) )
			{
				if ( (*pFormation)[0]->CanJoinToFormation() )
					JoinToFormation();
			}
			else
			{
				CPtr<IStaticPath> pPath = CreateStaticPathToPoint( lastFormationPos, VNULL2, pCatchingFormation, true );
				if ( pPath.IsValid() )
					pCatchingFormation->SendAlongPath( pPath, VNULL2, true );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCatchFormationState::AnalyzeFormationInBuilding( CBuilding *pBuilding )
{
	CSoldier *pSoldier = (*pCatchingFormation)[0];

	// внутри объекта, но не того, который нужно
	if ( !pSoldier->IsFree() && ( !pSoldier->IsInBuilding() || pSoldier->GetBuilding() != pBuilding ) )
		LeaveCurStaticObject();
	// не внутри объекта
	else if ( pSoldier->IsFree() )
	{
		const CVec2 vCatchingFormationCenter = pCatchingFormation->GetCenter();
		int nEntrance;
		CVec2 vPointToGo( -1.0f, -1.0f );
		if ( pBuilding->ChooseEntrance( pCatchingFormation, vCatchingFormationCenter, &nEntrance ) )
			vPointToGo = pBuilding->GetEntrancePoint( nEntrance );

		// можно вбежать в это здание
		if ( vPointToGo.x != -1.0f )
		{
			// уже близко
			if ( fabs2( vCatchingFormationCenter - vPointToGo ) <= sqr( float( 5 * SConsts::TILE_SIZE ) ) )
				theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_ENTER, pBuilding, 0 ), pCatchingFormation );
			else
			{
				// нужно подправить путь
				if ( pLastFormationObject.GetPtr() != pBuilding || pCatchingFormation->IsIdle() )
				{
					pLastFormationObject = pBuilding;
					if ( CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vPointToGo, VNULL2, pCatchingFormation, true ) )
						pCatchingFormation->SendAlongPath( pPath, VNULL2 );
				}
			}
		}
	}
	else
	{
		NI_ASSERT_T( pSoldier->IsInBuilding() && pSoldier->GetBuilding() == pBuilding, "Wrong state of CCatchFormationState state" );

		if ( (*pFormation)[0]->CanJoinToFormation() )
			JoinToFormation();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCatchFormationState::AnalyzeFormationInEntrenchment( CEntrenchment *pEntrenchment )
{
	CSoldier *pSoldier = (*pCatchingFormation)[0];

	if ( !pSoldier->IsFree() && ( !pSoldier->IsInEntrenchment() || pSoldier->GetEntrenchment() != pEntrenchment ) )
		LeaveCurStaticObject();
	else if ( pSoldier->IsFree() )
	{
		const CVec2 vCatchingFormationCenter = pCatchingFormation->GetCenter();
		CVec2 vPointToGo;
		pEntrenchment->GetClosestPoint( pSoldier->GetCenter(), &vPointToGo );

		// близко к точке входа
		if ( fabs2( vCatchingFormationCenter - vPointToGo ) <= sqr( float( 5 * SConsts::TILE_SIZE ) ) )
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_ENTER, pEntrenchment, 1 ), pCatchingFormation );
		else if ( pCatchingFormation->IsIdle() || pLastFormationObject.GetPtr() != (IRefCount*)pEntrenchment )
		{
			pLastFormationObject = pEntrenchment;
			if ( CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vPointToGo, VNULL2, pCatchingFormation, true ) )
				pCatchingFormation->SendAlongPath( pPath, VNULL2 );
		}
	}
	else
	{
		NI_ASSERT_T( pSoldier->IsInEntrenchment() && pSoldier->GetEntrenchment() == pEntrenchment, "Wrong state of CCatchFormationState" );

		if ( (*pFormation)[0]->CanJoinToFormation() )
			JoinToFormation();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCatchFormationState::AnalyzeFormationInTransport( CMilitaryCar *pCar )
{
	CSoldier *pSoldier = (*pCatchingFormation)[0];

	if ( !pSoldier->IsFree() )
		LeaveCurStaticObject();
	else
	{
		const CVec2 vTransportCenter = pCar->GetCenter();
		const float fDist2 = fabs2( pCatchingFormation->GetCenter() - vTransportCenter );
		// далеко, нужно подбежать
		if ( fDist2 > sqr( float( 5*SConsts::TILE_SIZE ) ) && 
				( pLastFormationObject.GetPtr() != (IRefCount*)pCar || pCatchingFormation->IsIdle() ) )
		{
			pLastFormationObject = pCar;
			if ( CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vTransportCenter, VNULL2, pCatchingFormation, true ) )
				pCatchingFormation->SendAlongPath( pPath, VNULL2 );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCatchFormationState::SetToDisbandedState()
{
	CSoldier *pSoldier = (*pCatchingFormation)[0];
	MoveSoldierToFormation();

	pSoldier->MemCurFormation();
	CCommonUnit *pSingleFormation = theUnitCreation.CreateSingleUnitFormation( pSoldier );

	if ( pSoldier->IsFree() )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_GUARD, CVec2( -1.0f, -1.0f ), 0 ), pSingleFormation, false );
	else if ( pSoldier->IsInTransport() )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRANSPORT, pSoldier->GetTransportUnit() ), pSingleFormation, false );
	else if ( pSoldier->IsInBuilding() )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_BUILDING, pSoldier->GetBuilding() ), pSingleFormation, false );
	else if ( pSoldier->IsInEntrenchment() )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRENCH, pSoldier->GetEntrenchment() ), pSingleFormation, false );

	pCatchingFormation->Disappear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCatchFormationState::Segment()
{
	// формация расформирована
	if ( pFormation->IsDisabled() )
		SetToDisbandedState();
	// пора проверить состояние формации
	else if ( curTime - lastUpdateTime >= 1500 )
	{
		// всех невиртуальных убили
		if ( pFormation->Size() == 0 )
			JoinToFormation();
		else
		{
			CSoldier *pFormationSoldier = (*pFormation)[0];

			if ( pFormationSoldier->IsFree() )
				AnalyzeFreeFormation();
			else if ( pFormationSoldier->IsInBuilding() )
				AnalyzeFormationInBuilding( pFormationSoldier->GetBuilding() );
			else if ( pFormationSoldier->IsInEntrenchment() )
				AnalyzeFormationInEntrenchment( pFormationSoldier->GetEntrenchment() );
			else
				AnalyzeFormationInTransport( pFormationSoldier->GetTransportUnit() );
		}

		lastUpdateTime = curTime;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CCatchFormationState::TryInterruptState( CAICommand *pCommand )
{
	pCatchingFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												CFormationSwarmState											*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationSwarmState::Instance( CFormation *pFormation, const CVec2 &point, const float fContinue )
{
	return new CFormationSwarmState( pFormation, point, fContinue );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationSwarmState::CFormationSwarmState( CFormation *_pFormation, const CVec2 &_point, const float fContinue )
: pFormation( _pFormation ), point( _point ), state( EFSS_START ), startTime( curTime ), bContinue( fContinue)
{
	for ( int i = 0; i < pFormation->Size(); ++i )
		(*pFormation)[i]->ResetTargetScan();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationSwarmState::AnalyzeTargetScan()
{
	bool bAttacking = false;
	for ( int i = 0; i < pFormation->Size(); ++ i )
	{
		if ( ( (*pFormation)[i]->AnalyzeTargetScan( 0, false, false ) & 1 ) )
		{
			bAttacking = true;
			break;
		}
	}

	if ( bAttacking )
	{
		state = EFSS_WAIT;
		pFormation->StopUnit();
		pFormation->SetToWaitingState();
		startTime = curTime;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationSwarmState::Segment()
{
	switch ( state )
	{
		case EFSS_START:
			if ( curTime - startTime >= TIME_OF_WAITING )
			{
				state = EFSS_MOVING;
				if ( !bContinue )
				{
					if ( CPtr<IStaticPath> pStaticPath = pFormation->GetCurCmd()->CreateStaticPath( pFormation ) )
						pFormation->SendAlongPath( pStaticPath, pFormation->GetGroupShift() );
				}
				else if ( CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( point, pFormation->GetGroupShift(), pFormation ) )
					pFormation->SendAlongPath( pStaticPath, pFormation->GetGroupShift() );
				else
				{
					pFormation->SendAcknowledgement( ACK_NEGATIVE );
					TryInterruptState( 0 );
				}
			}
			AnalyzeTargetScan();

			break;
		case EFSS_MOVING:
			if ( pFormation->IsIdle() )
				pFormation->SetCommandFinished();
			else
				AnalyzeTargetScan();

			break;
		case EFSS_WAIT:
			if ( curTime - startTime >= TIME_OF_WAITING && pFormation->IsEveryUnitResting() )
			{
				pFormation->BalanceCenter();
				startTime = curTime - TIME_OF_WAITING;
				bContinue = true;

				state = EFSS_START;
			}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationSwarmState::TryInterruptState( CAICommand *pCommand )
{
	if ( pFormation->GetCurCmd() != 0 )
		pFormation->GetCurCmd()->ToUnitCmd().fNumber = 1;

	pFormation->SetCommandFinished();
	pFormation->UnsetFromWaitingState();

	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationEnterBuildingNowState								*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationEnterBuildingNowState::Instance( CFormation *pFormation, CBuilding *pBuilding )
{
	return new CFormationEnterBuildingNowState( pFormation, pBuilding );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationEnterBuildingNowState::CFormationEnterBuildingNowState( CFormation *_pFormation, CBuilding *pBuilding )
: pFormation( _pFormation )
{
	for ( int i = 0; i < pFormation->Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_BUILDING, pBuilding ), (*pFormation)[i], false );

	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_BUILDING, pBuilding ), pFormation, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterBuildingNowState::Segment()
{
	pFormation->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationEnterBuildingNowState::TryInterruptState( CAICommand *pCommand )
{
	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFormationEnterEntrenchmentNowState						*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CFormationEnterEntrenchmentNowState::Instance( CFormation *pFormation, CEntrenchment *pEntrenchment )
{
	return new CFormationEnterEntrenchmentNowState( pFormation, pEntrenchment );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFormationEnterEntrenchmentNowState::CFormationEnterEntrenchmentNowState( CFormation *_pFormation, CEntrenchment *pEntrenchment )
: pFormation( _pFormation )
{
	for ( int i = 0; i < pFormation->Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRENCH, pEntrenchment, 2 ), (*pFormation)[i], false );

	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRENCH, pEntrenchment, 2 ), pFormation, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationEnterEntrenchmentNowState::Segment()
{
	pFormation->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CFormationEnterEntrenchmentNowState::TryInterruptState( CAICommand *pCommand )
{
	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
