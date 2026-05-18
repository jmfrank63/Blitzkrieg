#include "stdafx.h"

#include <float.h>

#include "TransportStates.h"
#include "SoldierStates.h"
#include "Units.h"
#include "Technics.h"
#include "GroupLogic.h"
#include "PointChecking.h"
#include "AIStaticMap.h"
#include "Entrenchment.h"
#include "Commands.h"
#include "Guns.h"
#include "UnitCreation.h"
#include "StaticObjects.h"
#include "..\Formats\fmtTerrain.h"
#include "Updater.h"
#include "Building.h"
#include "Formation.h"
#include "Soldier.h"
#include "Artillery.h"
#include "FormationStates.h"
#include "Diplomacy.h"
#include "TechnicsStates.h"
#include "PresizePath.h"
#include "EntrenchmentCreation.h"
#include "Turret.h"
#include "Bridge.h"
#include "Randomize.h"
#include "ArtilleryBulletStorage.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CDiplomacy theDipl;
extern CUpdater updater;
extern CUnitCreation theUnitCreation;
extern CGroupLogic theGroupLogic;
extern CUnits units;
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
extern CStaticObjects theStatObjs;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										  CTransportUnitFactory												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CPtr<CTransportStatesFactory> CTransportStatesFactory::pFactory = 0;

IStatesFactory* CTransportStatesFactory::Instance()
{
	if ( pFactory == 0 )
		pFactory = new CTransportStatesFactory();

	return pFactory;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTransportStatesFactory::CanCommandBeExecuted( CAICommand *pCommand )
{
	const EActionCommand &cmdType = pCommand->ToUnitCmd().cmdType;
	return 
		(
			cmdType == ACTION_COMMAND_DIE					||
			cmdType == ACTION_COMMAND_MOVE_TO			||
			cmdType == ACTION_COMMAND_WAIT_FOR_UNITS ||
			cmdType == ACTION_COMMAND_ROTATE_TO		||
			cmdType == ACTION_MOVE_BY_DIR					||
			cmdType == ACTION_COMMAND_UNLOAD			||
			cmdType == ACTION_COMMAND_GUARD				||
			cmdType == ACTION_COMMAND_DISAPPEAR		||
			cmdType == ACTION_COMMAND_RESUPPLY_HR ||
			cmdType == ACTION_COMMAND_RESUPPLY ||
			cmdType == ACTION_COMMAND_REPAIR ||
			cmdType == ACTION_MOVE_LOAD_RU ||
			cmdType == ACTION_COMMAND_TAKE_ARTILLERY ||
			cmdType == ACTION_COMMAND_DEPLOY_ARTILLERY ||
			cmdType == ACTION_COMMAND_FOLLOW ||
			cmdType == ACTION_COMMAND_FOLLOW_NOW ||
			cmdType == ACTION_COMMAND_ENTRENCH_SELF ||
			cmdType == ACTION_COMMAND_ENTRENCH_BEGIN ||
			cmdType == ACTION_COMMAND_ENTRENCH_END ||
			cmdType == ACTION_COMMAND_BUILD_FENCE_BEGIN ||
			cmdType == ACTION_COMMAND_BUILD_FENCE_END ||
			cmdType == ACTION_COMMAND_CLEARMINE ||
			cmdType == ACTION_COMMAND_PLACEMINE ||
			cmdType == ACTION_COMMAND_PLACE_ANTITANK ||
			cmdType == ACTION_COMMAND_REPEAR_OBJECT ||
			cmdType == ACTION_COMMAND_BUILD_BRIDGE ||
			cmdType == ACTION_MOVE_TO_NOT_PRESIZE ||
			cmdType == ACTION_COMMAND_ROTATE_TO_DIR ||
			cmdType == ACTION_COMMAND_STAND_GROUND ||
			cmdType == ACTION_COMMAND_FILL_RU ||
			cmdType == ACTION_COMMAND_MOVE_TO_GRID ||
			cmdType == ACTION_COMMAND_ATTACK_UNIT ||
			cmdType == ACTION_COMMAND_SWARM_ATTACK_UNIT ||
			cmdType == ACTION_COMMAND_SWARM_ATTACK_OBJECT ||
			cmdType == ACTION_COMMAND_ATTACK_OBJECT
		);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CTransportStatesFactory::ProduceState( class CQueueUnit *pObj, class CAICommand *pCommand )
{
	NI_ASSERT_T( dynamic_cast<CAITransportUnit*>( pObj ) != 0, "Wrong unit type" );
	CAITransportUnit *pUnit = static_cast<CAITransportUnit*>( pObj );
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	IUnitState* pResult = 0;

	bool bSwarmAttack = false;
	
	switch ( cmd.cmdType )
	{
		case ACTION_COMMAND_FILL_RU:
			pResult = CTransportLoadRuState::Instance( pUnit, false, static_cast_ptr<CBuildingStorage*>( cmd.pObject ) );

			break;
		case ACTION_MOVE_TO_NOT_PRESIZE:
			pResult = CMoveToPointNotPresize::Instance( pUnit, cmd.vPos, cmd.fNumber );

			break;
		case ACTION_COMMAND_REPEAR_OBJECT:
			{
				CStaticObject * pObject = static_cast_ptr<CStaticObject*>( cmd.pObject );
				if ( ESOT_BRIDGE_SPAN == pObject->GetObjectType() )
				{
					CBridgeSpan * pSpan = static_cast_ptr<CBridgeSpan*>(cmd.pObject);
					pResult = CTransportRepairBridgeState::Instance( pUnit, pSpan->GetFullBridge() );
				}
				else if ( ESOT_BUILDING == pObject->GetObjectType() )
				{
					CBuilding * pBuilding = static_cast_ptr<CBuilding*>(cmd.pObject);
					pResult = CTransportRepairBuildingState::Instance( pUnit, pBuilding );
				}
			}

			break;
		case ACTION_COMMAND_BUILD_BRIDGE:
			{
				CBridgeSpan * pSpan = static_cast_ptr<CBridgeSpan*>(cmd.pObject);
				pResult = CTransportBuildBridgeState::Instance( pUnit, pSpan->GetFullBridge() );
			}

			break;
		case ACTION_COMMAND_ENTRENCH_BEGIN:
			pResult = CTransportBuildEntrenchmentState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_ENTRENCH_END:
			if ( pUnit->GetState()->GetName() == EUSN_BUILD_ENTRENCHMENT )
			{
				NI_ASSERT_T( dynamic_cast<CTransportBuildEntrenchmentState*>( pUnit->GetState()) != 0, "bad state sequence" );
				static_cast<CTransportBuildEntrenchmentState*>( pUnit->GetState() )->SetEndPoint( cmd.vPos );
				pResult = pUnit->GetState();
			}

			break;
		case ACTION_COMMAND_PLACE_ANTITANK:
			pResult = CTransportPlaceAntitankState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_PLACEMINE:
			pResult = CTransportPlaceMineState::Instance( pUnit, cmd.vPos, cmd.fNumber );
			
			break;
		case ACTION_COMMAND_CLEARMINE:
			pResult = CTransportClearMineState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_BUILD_FENCE_BEGIN:
			pResult = CTransportBuildFenceState::Instance( pUnit, cmd.vPos * SConsts::TILE_SIZE );
			
			break;
		case ACTION_COMMAND_BUILD_FENCE_END:
			if ( pUnit->GetState()->GetName() == EUSN_BUILD_FENCE )
			{
				NI_ASSERT_T( dynamic_cast<CTransportBuildFenceState*>( pUnit->GetState())!=0, "bad state sequence" );
				static_cast<CTransportBuildFenceState*>( pUnit->GetState() )->SetEndPoint( cmd.vPos * SConsts::TILE_SIZE );
				pResult = pUnit->GetState();
			}

			break;
		case ACTION_COMMAND_ENTRENCH_SELF:
			pResult = CSoldierEntrenchSelfState::Instance( pUnit );

			break;
		case ACTION_MOVE_LOAD_RU:
			pResult = CTransportLoadRuState::Instance( pUnit );

			break;
		case ACTION_COMMAND_DEPLOY_ARTILLERY:
			if ( pUnit->IsTowing() )
			{
				if ( pUnit->IsTowing() != 0 )
					pResult = CTransportUnhookArtilleryState::Instance( pUnit, cmd.vPos, bool(cmd.fNumber) );
			}
			
			break;
		case ACTION_COMMAND_TAKE_ARTILLERY:
			{
				CONVERT_OBJECT_PTR( CArtillery, pArtillery, cmd.pObject, "Wrong artillery unit" );
				pResult = CTransportHookArtilleryState::Instance( pUnit, pArtillery );
			}

			break;
		case ACTION_COMMAND_RESUPPLY_HR:
			NI_ASSERT_T( cmd.pObject ? dynamic_cast_ptr<CArtillery*>( cmd.pObject ) != 0 : true, NStr::Format( "Wrong preferred unit %s",typeid( *pObj ).name()) );
			pResult = CTransportResupplyHumanResourcesState::Instance( pUnit, cmd.vPos, static_cast_ptr<CArtillery*>(cmd.pObject) );

			break;
		case ACTION_COMMAND_REPAIR:
			NI_ASSERT_T( cmd.pObject ? dynamic_cast_ptr<CAIUnit*>( cmd.pObject ) != 0 : true, NStr::Format( "Wrong preferred unit %s",typeid( *pObj ).name()) );
			pResult = CTransportRepairState::Instance( pUnit, cmd.vPos, static_cast_ptr<CAIUnit*>(cmd.pObject) );

			break;
		case ACTION_COMMAND_RESUPPLY:
			NI_ASSERT_T( cmd.pObject ? dynamic_cast_ptr<CAIUnit*>( cmd.pObject ) != 0 : true, NStr::Format( "Wrong preferred unit %s",typeid( *pObj ).name()) );
			pResult = CTransportResupplyState::Instance( pUnit, cmd.vPos, static_cast_ptr<CAIUnit*>(cmd.pObject) );
			
			break;
		case ACTION_COMMAND_DIE:
			NI_ASSERT_T( false, "Command to die in the queue" );

			break;
		case ACTION_COMMAND_MOVE_TO:
			pUnit->UnsetFollowState();
			pResult = CSoldierMoveToState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_WAIT_FOR_UNITS:
			pResult = CTransportWaitPassengerState::Instance( pUnit, checked_cast_ptr<CFormation*>(cmd.pObject) );

			break;
		case ACTION_COMMAND_ROTATE_TO:
			pResult = CSoldierTurnToPointState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_ROTATE_TO_DIR:
			{
				CVec2 vDir = cmd.vPos;
				Normalize( &vDir );
				pResult = CSoldierTurnToPointState::Instance( pUnit, pUnit->GetCenter() + vDir );
			}
			
			break;
		case ACTION_MOVE_BY_DIR:
			pResult = CSoldierMoveByDirState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_UNLOAD:
			pResult = CTransportLandState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_GUARD:
			pResult = CMechUnitRestState::Instance( pUnit, cmd.vPos, cmd.fNumber );

			break;
		case ACTION_COMMAND_FOLLOW:
			{
				CONVERT_OBJECT_PTR( CCommonUnit, pUnitToFollow, cmd.pObject, "Wrong unit to follow" );
				pUnit->SetFollowState( pUnitToFollow );
			}

			break;
		case ACTION_COMMAND_FOLLOW_NOW:
			{
				CONVERT_OBJECT_PTR( CCommonUnit, pUnitToFollow, cmd.pObject, "Wrong unit to follow" );
				pResult = CFollowState::Instance( pUnit, pUnitToFollow );
			}

			break;
		case ACTION_COMMAND_STAND_GROUND:
			pUnit->StopUnit();
			pUnit->GetBehaviour().moving = SBehaviour::EMHoldPos;

			break;
		case ACTION_COMMAND_MOVE_TO_GRID:
			pResult = CCommonMoveToGridState::Instance( pUnit, cmd.vPos, GetVectorByDirection( cmd.fNumber ) );

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
				// attack the artillery
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
		default:
			NI_ASSERT_T( false, "Wrong command" );
	}

	return pResult;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CTransportStatesFactory::ProduceRestState( class CQueueUnit *pUnit )
{
	return CMechUnitRestState::Instance( checked_cast<CAITransportUnit*>( pUnit ), CVec2( -1, -1 ), 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									 CTransportWaitPassengerState										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CTransportWaitPassengerState::Instance( CMilitaryCar *pTransport, CFormation *pFormation )
{
	return new CTransportWaitPassengerState( pTransport, pFormation );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTransportWaitPassengerState::CTransportWaitPassengerState( CMilitaryCar *_pTransport, CFormation *pFormation )
: pTransport( _pTransport )
{
	for ( int i = 0; i < pTransport->GetNTurrets(); ++i )
		pTransport->GetTurret( i )->SetCanReturn();

	formationsToWait.push_back( pFormation );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransportWaitPassengerState::Segment()
{
	std::list< CPtr<CFormation> >::iterator iter = formationsToWait.begin();
	while ( iter != formationsToWait.end() )
	{
		if ( !IsValidObj( *iter ) || (*iter)->GetState()->GetName() != EUSN_ENTER_TRANSPORT )
			iter = formationsToWait.erase( iter );
		else
			++iter;
	}

	if ( formationsToWait.empty() )
		pTransport->SetCommandFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CTransportWaitPassengerState::TryInterruptState( CAICommand *pCommand )
{ 
	if ( !pCommand )
	{
		pTransport->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}

	return TSIR_YES_WAIT;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransportWaitPassengerState::AddFormationToWait( CFormation *pFormation  )
{
	std::list< CPtr<CFormation> >::iterator iter = formationsToWait.begin();
	while ( iter != formationsToWait.end() && iter->GetPtr() != pFormation )
		++iter;

	if ( iter == formationsToWait.end() )
		formationsToWait.push_back( pFormation );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CTransportWaitPassengerState::GetPurposePoint() const
{
	return pTransport->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CTransportLandState													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CTransportLandState::Instance( CMilitaryCar *pTransport, const CVec2 &vLandPoint )
{
	return new CTransportLandState( pTransport, vLandPoint );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTransportLandState::CTransportLandState( CMilitaryCar *_pTransport, const CVec2 &_vLandPoint )
: pTransport( _pTransport ), vLandPoint( _vLandPoint ), state( ELS_STARTING )
{
	vLandPoint += pTransport->GetGroupShift();
	
	if ( pTransport->GetNPassengers() == 0 )
	{
		pTransport->SendAcknowledgement( ACK_NEGATIVE, true );
		pTransport->SetCommandFinished();
	}

	if ( !pTransport->CanMove() )
		state = ELS_LANDING;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransportLandState::LandPassenger( CSoldier *pLandUnit )
{
	if ( pLandUnit->IsInSolidPlace() )
		pLandUnit->SetCoordWOUpdate( CVec3( pTransport->GetEntrancePoint(), 0 ) );
	else
		pLandUnit->SetNewCoordinates( CVec3( pTransport->GetEntrancePoint(), 0 ) );

	pLandUnit->SetFree();
	pTransport->DelPassenger( pLandUnit );
	pLandUnit->GetState()->TryInterruptState( 0 );

	updater.Update( ACTION_NOTIFY_ENTRANCE_STATE, pLandUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransportLandState::Segment()
{
	switch ( state )
	{
		case ELS_STARTING:
			{
				if ( CPtr<IStaticPath> pPath = pTransport->GetCurCmd()->CreateStaticPath( pTransport ) )
				{
					pTransport->SendAlongPath( pPath, pTransport->GetGroupShift() );
					state = ELS_MOVING;
				}
				else
				{
					pTransport->SendAcknowledgement( ACK_NEGATIVE );
					pTransport->SetCommandFinished();
				}
			}

			break;
		case ELS_MOVING:
			if ( fabs2( pTransport->GetCenter() - vLandPoint ) <= sqr( 2 * pTransport->GetDistanceToLandPoint() ) || pTransport->IsIdle() )
				state = ELS_LANDING;

			break;
		case ELS_LANDING:
			{
				// найти все формации в транспорте, которые не принадлежат пушке, которая болтается сзади
				CPtr<CArtillery> pArt = pTransport->GetTowedArtillery();
				CFormation *pGunCrew = !IsValidObj( pArt ) ? 0 : pArt->GetCrew();
				const int nGunCrew = pGunCrew ==0 ? 0 : pGunCrew->Size();

				while ( pTransport->GetNPassengers() != nGunCrew )
				{
					CFormation *pFormation =0;
					const int nPassangers= pTransport->GetNPassengers();

					// найти формацию, которую нужно высадить
					for ( int i = 0; i < nPassangers && pFormation == 0; ++i )
					{
						CFormation *pTmp = pTransport->GetPassenger( i )->GetFormation();
						if ( pTmp != pGunCrew )
							pFormation = pTmp;
					}
					
					NI_ASSERT_T( pFormation!=0, "Something wrong inside this transport" );
					const int nFormSize = pFormation->Size();
					CVec3 vGoToPoint( pTransport->GetEntrancePoint(), 0 );
					pFormation->SetNewCoordinates( vGoToPoint );
					for ( int i = 0; i < nFormSize ; ++i )
					{
						CSoldier *pSoldier = (*pFormation)[i];
						if ( pSoldier->IsInTransport() )
							LandPassenger( pSoldier );
					}

					CVec2 vRand;
					RandUniformlyInCircle( 1.5f * SConsts::TILE_SIZE, &vRand );
					theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_MOVE_TO, vLandPoint.x + vRand.x, vLandPoint.y + vRand.y ), pFormation, false );
					if ( pFormation == pTransport->GetTowedArtilleryCrew() )
						pTransport->SetTowedArtilleryCrew( 0 );
				}

				pTransport->SetCommandFinished();
			}

			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CTransportLandState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand || state != ELS_LANDING )
	{
		pTransport->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else
		return TSIR_YES_WAIT;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CTransportLandState::GetPurposePoint() const
{
	return vLandPoint;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CTransportHookArtilleryState								*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CTransportHookArtilleryState::Instance( CAITransportUnit *pTransport, CArtillery *pArtillery )
{
	return new CTransportHookArtilleryState( pTransport, pArtillery );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTransportHookArtilleryState::CTransportHookArtilleryState( class CAITransportUnit *pTransport, class CArtillery * _pArtillery )
: pArtillery( _pArtillery ), pTransport( pTransport ), eState( TTGS_ESTIMATING ), 
	vArtilleryPoint( _pArtillery->GetCenter() ), timeLast( 0 ), wDesiredTransportDir( 0 ), bInterrupted( false )
{
	if ( !pArtillery->MustHaveCrewToOperate() || pTransport->IsTowing() )
	{
		TryInterruptState( 0 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTransportHookArtilleryState::CanInterrupt()
{
	return eState == TTGS_ESTIMATING || 
				eState == TTGS_APPROACHING ||
				eState == TTGS_APPROACH_BY_MOVE_BACK;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransportHookArtilleryState::InterruptBecauseOfPath()
{
	pTransport->SendAcknowledgement( ACK_NO_CANNOT_HOOK_ARTILLERY_NO_PATH );
	pTransport->RestoreDefaultPath();
	pTransport->SetCommandFinished();
	if ( IsValidObj( pArtillery ) )
	{
		if ( pTransport == pArtillery->GetHookingTransport() )
			pArtillery->SetBeingHooked( 0 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransportHookArtilleryState::Segment()
{
	if ( !IsValidObj( pArtillery ) || ( pArtillery->IsBeingHooked() && pArtillery->GetHookingTransport() != pTransport ))
	{
		pTransport->SetCommandFinished();
		return;
	}
	if (	eState != TTGS_WAIT_FOR_TURN &&
				eState != TTGS_SEND_CREW_TO_TRANSPORT &&
				eState != TTGS_WAIT_FOR_CREW &&
				fabs2( vArtilleryPoint - pArtillery->GetCenter() ) > 1.0f  )
	{
		InterruptBecauseOfPath();
		return;
	}

	bool bRepeat = true;
	while ( bRepeat )
	{
		bRepeat = false;
		switch ( eState )
		{
		case TTGS_ESTIMATING:		
			{
				if ( !pTransport->CanHookUnit( pArtillery ) )
				{
					pTransport->SendAcknowledgement( ACK_NO_TOO_HEAVY_ARTILLERY_FOR_TRANSPORT, true );
					pTransport->SetCommandFinished();
				}
				else if ( !pArtillery->HasServeCrew() || pArtillery->GetPlayer() != pTransport->GetPlayer() )
				{
					//not possible, cannot take nither other player's artillery nor free artillery
					pTransport->SendAcknowledgement( ACK_NEGATIVE, true );
					pTransport->SetCommandFinished();
				}
				else		
				{
					if ( fabs2( pArtillery->GetCenter() - pTransport->GetCenter()) < SConsts::TRANSPORT_MOVE_BACK_DISTANCE )
					{
						eState = TTGS_APPROACHING;
					}
					else
					{
						// подъехать передом.
						CPtr<IStaticPath> pPath = CreateStaticPathToPoint( pArtillery->GetCenter(), VNULL2, pTransport, true );
						if ( pPath )
						{
							eState = TTGS_APPROACHING;
							pTransport->SendAlongPath( pPath, VNULL2 );
						}
						else
							InterruptBecauseOfPath();
					}
				}
			}
			
			break;
		case TTGS_APPROACHING:
			if ( !pArtillery->HasServeCrew() || pArtillery->GetPlayer() != pTransport->GetPlayer() )
			{
				//not possible, cannot take nither other player's artillery nor free artillery
				pTransport->SetCommandFinished();
			}
			else if ( fabs2( pArtillery->GetCenter() - pTransport->GetCenter()) < SConsts::TRANSPORT_MOVE_BACK_DISTANCE ||
								pTransport->IsIdle() )
			{
				pTransport->StopUnit();
				pTransport->SetRightDir( false );

				CPtr<IStaticPath> pPath = CreateStaticPathToPoint( pArtillery->GetCenter(), VNULL2, pTransport, true );
				if ( pPath )
				{
					eState = TTGS_APPROACH_BY_MOVE_BACK;
					pArtillery->SetBeingHooked( pTransport );
					pTransport->SendAlongPath( pPath, VNULL2 );
				}
				else
					InterruptBecauseOfPath();
			}

			break;
		case TTGS_APPROACH_BY_MOVE_BACK:
			// расстояние между пушкой и грузовиком почти равно размерам грузовичка и пушки
			if ( !pArtillery->HasServeCrew() || pArtillery->GetPlayer() != pTransport->GetPlayer() )
			{
				//not possible, cannot take nither other player's artillery nor free artillery
				pArtillery->SetBeingHooked( 0 );
				pTransport->SetCommandFinished();
			}
			else		
			{
				// подъезжаем на нужное расстояние к пушке
				const float dist2 = fabs( pArtillery->GetCenter() - pArtillery->GetHookPoint() ) +
														fabs( pTransport->GetCenter() - pTransport->GetHookPoint() );
				const float dist1 = fabs( pArtillery->GetCenter() - pTransport->GetCenter() );
				const float diff = fabs( dist2 - dist1 );
				
				if ( !pArtillery->IsInTankPit() && diff < SConsts::GUN_CREW_TELEPORT_RADIUS )
				{
					pTransport->RestoreDefaultPath();
					pTransport->StopUnit();
					eState = TTGS_START_UNINSTALL;
				}
 				else if ( pTransport->IsIdle() && ( pArtillery->IsInTankPit() || diff < SConsts::TILE_SIZE * 3 ) )
				{
					eState = TTGS_START_APPROACH_BY_CHEAT_PATH;
				}
				else if ( pTransport->IsIdle() )
				{
					pTransport->SetRightDir( true );
					InterruptBecauseOfPath();
				}
			}
			
			break;
		case TTGS_WAIT_FOR_LEAVE_TANKPIT:
			if ( !pArtillery->IsInTankPit() )
				eState = TTGS_APPROACHING;

			break;
		case TTGS_START_APPROACH_BY_CHEAT_PATH:
			{
				if ( pArtillery->IsInTankPit() )
				{
					theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_LEAVE_TANK_PIT), pArtillery, false );
					eState = TTGS_WAIT_FOR_LEAVE_TANKPIT;
				}
				else
				{
					const float dist2 = fabs( pArtillery->GetCenter() - pArtillery->GetHookPoint() ) +
															fabs( pTransport->GetCenter() - pTransport->GetHookPoint() );

					CVec2 vDestPoint( pArtillery->GetCenter() - pArtillery->GetDirVector()*dist2 );
					pTransport->SetCurPath( new CPresizePath( pTransport, vDestPoint, -pArtillery->GetDirVector() ) );

					pTransport->SetRightDir( false );
					CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vDestPoint, VNULL2, pTransport, true );
					if ( pPath )
					{
						eState = TTGS_APPROACH_BY_CHEAT_PATH;
					}
					else
					{
						pTransport->SetRightDir( true );
						InterruptBecauseOfPath();
					}
				}
			}
			break;
		case TTGS_APPROACH_BY_CHEAT_PATH:
			if ( fabs( pTransport->GetHookPoint() - pArtillery->GetHookPoint() )< SConsts::GUN_CREW_TELEPORT_RADIUS )
			{
				pTransport->RestoreDefaultPath();
				///eState = TTGS_APPROACHING;
				eState = TTGS_START_UNINSTALL;
				bRepeat = true;
			}
			else if ( pTransport->GetCurPath()->IsFinished() )
			{
				pTransport->RestoreDefaultPath();
				eState = TTGS_APPROACHING;
				bRepeat = true;
			}

			break;
		case TTGS_START_UNINSTALL:
			pTransport->StopUnit();
			pArtillery->GetState()->TryInterruptState( 0 );
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_UNINSTALL), pArtillery, false );
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_IDLE), pArtillery, true );
			//theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_BEING_TOWED, pTransport ), pArtillery, false );
			eState = TTGS_WAIT_FOR_UNINSTALL;

			break;
		case TTGS_WAIT_FOR_UNINSTALL:
			if ( !pArtillery->HasServeCrew() || pArtillery->GetPlayer() != pTransport->GetPlayer() )
			{
				//not possible, cannot take nither other player's artillery nor free artillery
				pArtillery->SetBeingHooked( 0 );
				pTransport->SetCommandFinished();
			}
			else if ( pArtillery->IsUninstalled() && 
								pArtillery->GetCurUninstallAction() == ACTION_NOTIFY_UNINSTALL_TRANSPORT )
			{
					// повернуть пушку к грузовичку
				pTransport->SetRightDir( true );
				eState = TTGS_WAIT_FOR_TURN;
				timeLast = curTime;
				wDesiredTransportDir = GetDirectionByVector( pTransport->GetCenter() - pArtillery->GetCenter() );
			}

			break;
		case TTGS_WAIT_FOR_TURN:
			{
				CFormation *pArtCrew = pArtillery->GetCrew();
				if ( !IsValidObj( pArtCrew ) || !pArtCrew->IsFree() || pArtillery->GetPlayer() != pTransport->GetPlayer() )
				{
					//not possible, cannot take nither other player's artillery nor free artillery
					pArtillery->SetBeingHooked( 0 );
					pTransport->SetCommandFinished();
				}
				else 
				{
					const bool trTurn = pTransport->TurnToDir( wDesiredTransportDir, false, true );
					const bool arTurn = pArtillery->TurnToDir( wDesiredTransportDir - 65535/2, false );

					if ( trTurn && arTurn )
					{
						if ( fabs( pTransport->GetHookPoint() - pArtillery->GetHookPoint() ) > SConsts::TILE_SIZE )
						{
							eState = TTGS_APPROACH_BY_CHEAT_PATH;
						}
						else if ( pArtillery->HasServeCrew() )
						{
							eState = TTGS_SEND_CREW_TO_TRANSPORT;
							pTransport->SetTowedArtillery( pArtillery );
							pTransport->SetTowedArtilleryCrew( pArtCrew );
							pArtillery->SetBeingHooked( 0 );
							theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_BEING_TOWED, pTransport ), pArtillery, false );
						}
						else
						{
							eState = TTGS_WAIT_FOR_CREW;
							pTransport->SetTowedArtillery( pArtillery );
							pTransport->SetTowedArtilleryCrew( pArtCrew );
							pArtillery->SetBeingHooked( 0 );
							theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_BEING_TOWED, pTransport ), pArtillery, false );
						}
					}
					else
						timeLast = curTime;
				}
			}

			break;
		case TTGS_SEND_CREW_TO_TRANSPORT:
			if ( EUSN_BEING_TOWED == pArtillery->GetState()->GetName() )
			{
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_ENTER_TRANSPORT_CHEAT_PATH, pTransport ), pTransport->GetTowedArtilleryCrew(), false );
				eState = TTGS_WAIT_FOR_CREW;
			}
			
			break;
		case TTGS_WAIT_FOR_CREW:
			if ( !pTransport->HasTowedArtilleryCrew() || 
						pTransport->GetTowedArtilleryCrew()->IsEveryUnitInTransport() )
			{
				pArtillery->SetBeingHooked( 0 );
				pTransport->SetCommandFinished();
			}

			break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CTransportHookArtilleryState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand )
	{
		pTransport->SetCommandFinished();
		pArtillery->SetBeingHooked( 0 );
		return TSIR_YES_IMMIDIATELY;
	}

	// не по смерти разрещается прерывать только если не начали крутить пушку
	if ( !pTransport->IsAlive () )
	{
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_STOP_THIS_ACTION), pArtillery, false );
		if ( pArtillery->HasServeCrew() )
		{
 			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_STOP_THIS_ACTION), pArtillery->GetCrew(), false );
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_CATCH_ARTILLERY, pArtillery), pArtillery->GetCrew(), false );
		}
		pTransport->SetCommandFinished();
		bInterrupted = true;
		pArtillery->SetBeingHooked( 0 );
		return TSIR_YES_IMMIDIATELY ;
	}
	else if ( !bInterrupted || !CanInterrupt() )
		return TSIR_NO_COMMAND_INCOMPATIBLE;
	else 
	{
		pTransport->SetCommandFinished();
		pTransport->SetRightDir( true );
		bInterrupted = true;
		pArtillery->SetBeingHooked( 0 );
		return TSIR_YES_IMMIDIATELY ;
	}
	return TSIR_YES_WAIT;
	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CTransportHookArtilleryState::GetPurposePoint() const
{
	return pTransport->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CTransportUnhookArtilleryState							*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CTransportUnhookArtilleryState::Instance( CAITransportUnit *pTransport, const CVec2 &vDestPoint, const bool _bNow )
{
	return new CTransportUnhookArtilleryState( pTransport, vDestPoint, _bNow );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTransportUnhookArtilleryState::CTransportUnhookArtilleryState ( CAITransportUnit *_pTransport, const CVec2 &_vDestPoint, const bool _bNow )
: pTransport( _pTransport ), eState( TUAS_START_APPROACH ),
	vDestPoint( _vDestPoint ), bInterrupted( false ), nAttempt( 0 ), bNow( _bNow )
{
	NI_ASSERT_T( pTransport->IsTowing(), "wrong towed artillery");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTransportUnhookArtilleryState::CanPlaceUnit( const class CAIUnit * pUnit ) const
{
	pTransport->UnlockTiles();
	SRect rect( pUnit->GetUnitRect() );
	const BYTE aiClass = pUnit->GetAIClass();

	CTilesSet tiles;
	GetTilesCoveredByRect( rect, &tiles );
	for ( CTilesSet::iterator i = tiles.begin(); i !=tiles.end(); ++i )
	{
		if ( theStaticMap.IsLocked( (*i), aiClass ) )
			return false;
	}
	//return theStaticMap.CanUnitGoToPoint( pUnit->GetBoundTileRadius(), pUnit->GetCenter(), )
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransportUnhookArtilleryState::Segment()
{
	CArtillery *pArt = pTransport->GetTowedArtillery();
			
	if ( !pArt || !pArt->IsAlive() )
	{
		TryInterruptState( 0 );
		return;
	}

	switch ( eState )
	{
	case TUAS_START_APPROACH:
		{
			if ( bNow )
				eState = TUAS_ESTIMATING;
			else
			{
				CPtr<IStaticPath> pPath = pTransport->GetCurCmd()->CreateStaticPath( pTransport );
				if ( !pPath )
					pPath = CreateStaticPathToPoint( vDestPoint, pTransport->GetGroupShift(), pTransport, true );

				if ( pPath )
				{
					pTransport->SendAlongPath( pPath, pTransport->GetGroupShift() );
					vDestPoint += pTransport->GetGroupShift();
					eState = TUAS_APPROACHING;
				}
				else
				{
					pTransport->SendAcknowledgement( ACK_NEGATIVE );
					TryInterruptState( 0 );
				}
			}
		}

		break;
	case TUAS_APPROACHING:
		if ( IsUnitNearPoint( pTransport, vDestPoint, SConsts::TILE_SIZE ) ||
				 pTransport->IsIdle() && IsUnitNearPoint( pTransport, vDestPoint, 6 * SConsts::TILE_SIZE ) )
		{
			const SMechUnitRPGStats *pArtStats = static_cast<const SMechUnitRPGStats *>(pArt->GetStats());
			const SMechUnitRPGStats *pTranspStats = static_cast<const SMechUnitRPGStats *>(pTransport->GetStats());
			float fLenght = pArtStats->vAABBHalfSize.y + pTranspStats->vAABBHalfSize.y;
			CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vDestPoint+pTransport->GetDirVector()*fLenght, 
																												 VNULL2, pTransport, true );
			if ( pPath )
			{
				pTransport->SendAlongPath( pPath, VNULL2 );
				eState = TUAS_MOVE_ARTILLERY_TO_THIS_POINT;
			}
			else
				eState = TUAS_ESTIMATING;
		}
		else if ( pTransport->IsIdle() )
			TryInterruptState( 0 );

		break;
	case TUAS_ESTIMATING:
		{
			nAttempt++;
			pTransport->UnlockTiles();
			if ( CanPlaceUnit( pArt ) )
			{
				eState = TUAS_START_UNHOOK;
				pArt->LockTiles();
			}
			else
			{
				eState = TUAS_ADVANCE_A_LITTLE;
			}
		}

		break;
	case TUAS_MOVE_ARTILLERY_TO_THIS_POINT:
		if ( pTransport->IsIdle() )
			eState = TUAS_ESTIMATING;

		break;
	case TUAS_ADVANCE_A_LITTLE:
		if ( nAttempt > SConsts::TRIES_TO_UNHOOK_ARTILLERY )
		{
			pTransport->SendAcknowledgement( ACK_NO_CANNOT_UNHOOK_ARTILLERY_HERE );
			TryInterruptState( 0 );
		}
		else
		{
			const SMechUnitRPGStats *pArtStats = static_cast<const SMechUnitRPGStats *>(pArt->GetStats());
			const SMechUnitRPGStats *pTranspStats = static_cast<const SMechUnitRPGStats *>(pTransport->GetStats());
			float fLenght = pArtStats->vAABBHalfSize.y + pTranspStats->vAABBHalfSize.y;
			CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vDestPoint+pTransport->GetDirVector()*fLenght, 
																												 VNULL2, pTransport, true );
			if ( pPath )
			{
				pTransport->SendAlongPath( pPath, VNULL2 );
				eState = TUAS_MOVE_A_LITTLE;
			}
			else
				eState = TUAS_ESTIMATING;
		}
		
		break;
	case TUAS_MOVE_A_LITTLE:
		pTransport->UnlockTiles();
		pTransport->GetTowedArtillery()->UnlockTiles();
		if ( CanPlaceUnit( pTransport->GetTowedArtillery() ) )
		{
			pTransport->StopUnit();
			eState = TUAS_START_UNHOOK;
			pArt->LockTiles();
		}
		else if ( pTransport->IsIdle() )
		{
			nAttempt++;
			eState = TUAS_ADVANCE_A_LITTLE;
		}

		break;
	case TUAS_START_UNHOOK:
		{
			CArtillery *pArt = pTransport->GetTowedArtillery();

			//отпустить пушку
			pTransport->SetTowedArtillery( 0 );
			pArt->GetState()->TryInterruptState( 0 );
			
			if ( !pTransport->HasTowedArtilleryCrew() )
				TryInterruptState( 0 );
			else
			{
				CFormation *pCrew = pTransport->GetTowedArtilleryCrew();
				//выгнать артиллеристов из транспорта
				CSoldier *pSold = 0;
				CVec2 point2D( pTransport->GetEntrancePoint() );
				CVec3 point3D( point2D.x, point2D.y, theStaticMap.GetZ( AICellsTiles::GetTile(point2D) ) );

				pCrew->StopUnit();
				pCrew->GetState()->TryInterruptState( 0 );

				for ( int i = 0; i < pCrew->Size(); ++i )
				{
					pSold = (*pCrew)[i];
					pTransport->DelPassenger( pSold );
					pSold->SetFree();
					pSold->GetState()->TryInterruptState( 0 );
					pSold->RestoreDefaultPath();
					pSold->StopUnit();

					if ( pSold->IsInSolidPlace() )
						pSold->SetCoordWOUpdate( point3D );
					else
						pSold->SetNewCoordinates( point3D );
				}
				pCrew->SetNewCoordinates( point3D );

				// пусть пушку могут теперь селектить
				pArt->SetCrew( pCrew );

				// дать команду пушке инталлироваться
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_INSTALL ), pArt, false );

				// все , можем уезжать, справятся без нас
				pTransport->SetCommandFinished();
			}
		}

		break;

	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CTransportUnhookArtilleryState::TryInterruptState( class CAICommand *pCommand )
{
	bInterrupted = true;
	pTransport->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										  CMoveToPointNotPresize											*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitState* CMoveToPointNotPresize::Instance( class CAIUnit *_pTransport, const CVec2 &_vGeneralCell, const float _fRadius )
{
	return new CMoveToPointNotPresize( _pTransport, _vGeneralCell, _fRadius );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMoveToPointNotPresize::CMoveToPointNotPresize( class CAIUnit *_pTransport, const CVec2 &vGeneralCell, const float _fRadius )
: vPurposePoint( vGeneralCell ), fRadius( _fRadius ), pTransport( _pTransport )
{
	SendToPurposePoint();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMoveToPointNotPresize::SendToPurposePoint()
{
	CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vPurposePoint, VNULL2, pTransport, true );
	if ( pPath )
		pTransport->SendAlongPath( pPath, VNULL2 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMoveToPointNotPresize::Segment()
{
	if ( fabs2( pTransport->GetCenter() - vPurposePoint ) < sqr( fRadius ) )
	{
		if ( pTransport->GetNextCommand() )
		{
			//hack! 
			pTransport->GetNextCommand()->ToUnitCmd().vPos = pTransport->GetCenter();
		}
		pTransport->StopUnit();
		pTransport->SetCommandFinished();
	}
	else if ( pTransport->IsIdle() )
	{
		SendToPurposePoint();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ETryStateInterruptResult CMoveToPointNotPresize::TryInterruptState( class CAICommand *pCommand )
{
	pTransport->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
