#include "StdAfx.h"

#include "SoldierStates.h"
#include "Commands.h"
#include "GroupLogic.h"
#include "PathFinder.h"
#include "TransportStates.h"
#include "Entrenchment.h"
#include "Building.h"
#include "Behaviour.h"
#include "Technics.h"
#include "InTransportStates.h"
#include "InEntrenchmentStates.h"
#include "InBuildingStates.h"
#include "GroupLogic.h"
#include "Updater.h"
#include "Guns.h"
#include "StaticObject.h"
#include "StaticObjects.h"
#include "AISTaticMap.h"
#include "AntiArtilleryManager.h"
#include "Diplomacy.h"
#include "Mine.h"
#include "Soldier.h"
#include "Formation.h"
#include "Turret.h"
#include "Aviation.h"
#include "UnitCreation.h"
#include "Artillery.h"
#include "UnitGuns.h"
#include "StandartPath.h"
#include "ParatrooperPath.h"
#include "General.h"
#include "ArtilleryBulletStorage.h"
#include "MPLog.h"
#include "StaticObjectsIters.h"

#include "TimeCounter.h"
#include "AAFeedBacks.h"
extern CAAFeedBacks theAAFeedBacks;
extern CSupremeBeing theSupremeBeing;
extern CUnitCreation theUnitCreation;
extern CStaticMap theStaticMap;
extern CUpdater updater;
extern NTimer::STime curTime;
extern CStaticObjects theStatObjs;
extern CAntiArtilleryManager theAAManager;
extern CGroupLogic theGroupLogic;
extern CDiplomacy theDipl;

extern CTimeCounter timeCounter;
CPtr<CSoldierStatesFactory> CSoldierStatesFactory::pFactory = 0;

IStatesFactory* CSoldierStatesFactory::Instance()
{
	if ( pFactory == 0 )
		pFactory = new CSoldierStatesFactory();

	return pFactory;
}
bool CSoldierStatesFactory::CanCommandBeExecuted( CAICommand *pCommand )
{
	const EActionCommand &cmdType = pCommand->ToUnitCmd().cmdType;
	return 
		( cmdType == ACTION_COMMAND_DIE								||
			cmdType == ACTION_COMMAND_MOVE_TO						||
			cmdType == ACTION_COMMAND_ATTACK_UNIT				||
			cmdType == ACTION_COMMAND_ATTACK_OBJECT			||
			cmdType == ACTION_COMMAND_ROTATE_TO					||
			cmdType == ACTION_MOVE_BY_DIR								||
			cmdType == ACTION_COMMAND_ENTER							||
			cmdType == ACTION_COMMAND_IDLE_BUILDING			||
			cmdType == ACTION_COMMAND_IDLE_TRENCH				||
			cmdType == ACTION_COMMAND_SWARM_TO					||
			cmdType == ACTION_COMMAND_PARADE						||
			cmdType == ACTION_COMMAND_PLACEMINE_NOW			||
			cmdType == ACTION_COMMAND_CLEARMINE_RADIUS	||
			cmdType == ACTION_COMMAND_GUARD							||
			cmdType == ACTION_COMMAND_AMBUSH						||
			cmdType == ACTION_COMMAND_ENTER_TRANSPORT_NOW ||
			cmdType == ACTION_COMMAND_IDLE_TRANSPORT		||
			cmdType == ACTION_COMMAND_DISAPPEAR ||
			cmdType == ACTION_MOVE_PARACHUTE ||
			cmdType == ACTION_COMMAND_USE_SPYGLASS ||
			cmdType == ACTION_MOVE_IDLE ||
			cmdType == ACTION_COMMAND_FOLLOW ||
			cmdType == ACTION_COMMAND_FOLLOW_NOW ||
			cmdType == ACTION_COMMAND_SNEAK_ON ||
			cmdType == ACTION_COMMAND_SNEAK_OFF ||
			cmdType == ACTION_COMMAND_FORM_FORMATION ||
			cmdType == ACTION_COMMAND_SWARM_ATTACK_UNIT ||
			cmdType == ACTION_COMMAND_SWARM_ATTACK_OBJECT ||
			cmdType == ACTION_COMMAND_ROTATE_TO_DIR ||
			cmdType == ACTION_COMMAND_USE ||
			cmdType == ACTION_COMMAND_STAND_GROUND ||
			cmdType == ACTION_COMMAND_MOVE_TO_GRID
		);
}
IUnitState* CSoldierStatesFactory::ProduceState( class CQueueUnit *pObj, CAICommand *pCommand )
{
	NI_ASSERT_T( dynamic_cast<CAIUnit*>(pObj) != 0, "Wrong unit passed" );
	CAIUnit *pUnit = static_cast<CAIUnit*>(pObj);
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	IUnitState* pResult = 0;

	bool bSwarmAttack = false;
	
	switch ( cmd.cmdType )
	{
		case ACTION_COMMAND_DIE:
			NI_ASSERT_T( false, "Command to die in the queue" );

			break;
		case ACTION_COMMAND_SNEAK_ON:
			{
				CONVERT_OBJECT( CSniper, pSniper, pUnit, "Not sniper passed" );				

				pSniper->SetCamoulfage();
				pSniper->SetSneak( true );
			}
			
			break;
		case ACTION_COMMAND_SNEAK_OFF:
			{
				CONVERT_OBJECT( CSniper, pSniper, pUnit, "Not sniper passed" );				
				pSniper->SetSneak( false );
			}

			break;
		case ACTION_COMMAND_AMBUSH:
			pResult = CCommonAmbushState::Instance( pUnit );
			

			break;
		case ACTION_MOVE_IDLE:
			pResult = CSoldierIdleState::Instance( pUnit );

			break;
		case ACTION_COMMAND_USE_SPYGLASS:
			{
				CONVERT_OBJECT( CSoldier, pSoldier, pUnit, "ACTION_COMMAND_USE_SPYGLASS: not soldier passed" );
				pResult = CSoldierUseSpyglassState::Instance( pSoldier, cmd.vPos );
			}

			break;
		case ACTION_MOVE_PARACHUTE:
			{
				CONVERT_OBJECT( CSoldier, pSoldier, pUnit, "ACTION_MOVE_PARACHUTE: not soldier passed" );
				pResult = CSoldierParaDroppingState::Instance( pSoldier );
			}

			break;
		case ACTION_COMMAND_MOVE_TO:
			pUnit->UnsetFollowState();
			pResult = CSoldierMoveToState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_SWARM_ATTACK_UNIT:
			bSwarmAttack = true;
		case ACTION_COMMAND_ATTACK_UNIT:
			{
				CONVERT_OBJECT_PTR( CAIUnit, pTarget, cmd.pObject, "Wrong unit to attack" );

				if ( IsValidObj( pTarget ) )
				{
					if ( pTarget->GetStats()->IsInfantry() && static_cast<CSoldier*>(pTarget)->IsInBuilding() )
						pResult = CSoldierAttackUnitInBuildingState::Instance( pUnit, static_cast<CSoldier*>(pTarget), cmd.fNumber == 0, bSwarmAttack );
					else
					{
						if ( pTarget->GetStats()->IsInfantry() && static_cast<CSoldier*>(pUnit)->IsInEntrenchment() )
							pResult = CSoldierAttackInEtrenchState::Instance( static_cast<CSoldier*>(pUnit), static_cast<CAIUnit*>( pTarget ), bSwarmAttack );
						else
							pResult = CSoldierAttackState::Instance( pUnit, static_cast<CAIUnit*>(pTarget), cmd.fNumber == 0, bSwarmAttack );
					}
				}
				else
					pUnit->SendAcknowledgement( ACK_INVALID_TARGET );
			}

			break;
		case ACTION_COMMAND_SWARM_ATTACK_OBJECT:
			bSwarmAttack = true;
		case ACTION_COMMAND_ATTACK_OBJECT:
			{
				CONVERT_OBJECT_PTR( CStaticObject, pStaticObj, cmd.pObject, "Wrong static object to attack" );

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
		case ACTION_COMMAND_ENTER:
			if ( cmd.fNumber == 0 )
			{
				CONVERT_OBJECT_PTR( CBuilding, pBuilding, cmd.pObject, "Wrong static object is passed, command enter to building" );
				pResult = CSoldierEnterState::Instance( pUnit, pBuilding );
			}
			else if ( cmd.fNumber == 1 )
			{
				CONVERT_OBJECT_PTR( CEntrenchment, pEntrenchment, cmd.pObject, "Wrong static object is passed, command enter to entrenchment" );
				pResult = CSoldierEnterEntrenchmentState::Instance( pUnit, pEntrenchment );
			}
			else if  ( cmd.fNumber == 2 )
			{
				CONVERT_OBJECT_PTR( CEntrenchmentPart, pEntrenchmentPart, cmd.pObject, "Wrong static object is passed, command enter to entrenchment part" );
				pResult = CSoldierEnterEntrenchmentState::Instance( pUnit, pEntrenchmentPart->GetOwner() );
			}
			else
				NI_ASSERT_T( false, NStr::Format( "Wrong number %g in command Enter", cmd.fNumber ) );
			
			break;
		case ACTION_COMMAND_IDLE_BUILDING:
			{
				CONVERT_OBJECT_PTR( CBuilding, pBuilding, cmd.pObject, "Wrong object to enter" );
				CONVERT_OBJECT( CSoldier, pSoldier, pUnit, "Wrong unit passed: soldier expected" );
				pResult = CSoldierRestInBuildingState::Instance( pSoldier, pBuilding );
			}

			break;
		case ACTION_COMMAND_IDLE_TRENCH:
			{
				CONVERT_OBJECT_PTR( CEntrenchment, pEntrenchment, cmd.pObject, "Wrong static object is passed, command enter to entrenchment" );
				CONVERT_OBJECT( CSoldier, pSoldier, pUnit, "Wrong unit passed: soldier expected" );
				pResult = CSoldierRestInEntrenchmentState::Instance( pSoldier, pEntrenchment );
			}

			break;
		case ACTION_COMMAND_SWARM_TO:
			pResult = CCommonSwarmState::Instance( pUnit, cmd.vPos, cmd.fNumber );

			break;
		case ACTION_COMMAND_PARADE:
			pResult = CSoldierParadeState::Instance( pUnit );

			break;
		case ACTION_COMMAND_PLACEMINE_NOW:
			pResult = CSoldierPlaceMineNowState::Instance( pUnit, cmd.vPos, static_cast<SMineRPGStats::EType>(int(cmd.fNumber)) );

			break;
		case ACTION_COMMAND_CLEARMINE_RADIUS:
			pResult = CSoldierClearMineRadiusState::Instance( pUnit, cmd.vPos );

			break;
		case ACTION_COMMAND_GUARD:
			pResult = CSoldierRestState::Instance( pUnit );

			break;
		case ACTION_COMMAND_ENTER_TRANSPORT_NOW:
			{
				CONVERT_OBJECT_PTR( CMilitaryCar, pCar, cmd.pObject, "Not transport passed to enter to" );
				pResult = CSoldierEnterTransportNowState::Instance( pUnit, pCar );
			}

			break;
		case ACTION_COMMAND_IDLE_TRANSPORT:
			{
				CONVERT_OBJECT_PTR( CMilitaryCar, pCar, cmd.pObject, "Not transport passed to idle" );
				CONVERT_OBJECT( CSoldier, pSoldier, pUnit, "Wrong unit passed: soldier expected" );
				pResult = CSoldierRestOnBoardState::Instance( pSoldier, pCar );
			}

			break;
		case ACTION_COMMAND_FOLLOW:
			NI_ASSERT_T( dynamic_cast_ptr<CCommonUnit*>(cmd.pObject) != 0, "Not common unit in follow command" );
			pUnit->SetFollowState( static_cast_ptr<CCommonUnit*>(cmd.pObject) );

			break;
		case ACTION_COMMAND_FOLLOW_NOW:
			{
				CONVERT_OBJECT_PTR( CCommonUnit, pUnitToFollowTo, cmd.pObject, "Not common unit in follow command" );
				pResult = CFollowState::Instance( pUnit, pUnitToFollowTo );
			}

			break;
		case ACTION_COMMAND_FORM_FORMATION:
			if ( pUnit->IsInFormation() )
			{
				CONVERT_OBJECT( CSoldier, pSoldier, pUnit, "Wrong unit for FORM_FORMATION command" );
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_FORM_FORMATION ), pSoldier, false );
			}

			break;
		case ACTION_COMMAND_USE:
			NI_ASSERT_T( cmd.fNumber >= 0, NStr::Format( "Wrong number of use animation (%d)", (int)cmd.fNumber ) );
			pResult = CSoldierUseState::Instance( pUnit, EActionNotify((int)cmd.fNumber) );

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
IUnitState* CSoldierStatesFactory::ProduceRestState( class CQueueUnit *pUnit )
{
	CSoldier *pSoldier = checked_cast<CSoldier*>( pUnit );
	if ( pSoldier->IsInEntrenchment() )
		return CSoldierRestInEntrenchmentState::Instance( pSoldier, pSoldier->GetEntrenchment() );
	else
		return CSoldierRestState::Instance( pSoldier );
}
IUnitState* CSoldierRestState::Instance( CAIUnit *pUnit )
{
	return new CSoldierRestState( pUnit );
}
CSoldierRestState::CSoldierRestState( CAIUnit *_pUnit )
: pUnit( _pUnit ), nextMove( 0 ), bScanned( false ), fDistToGuardPoint( -1.0f )
{
	if ( pUnit->GetStats()->type != RPG_TYPE_SNIPER )
		pUnit->StartCamouflating();
	pUnit->ResetTargetScan();

	if ( pUnit->GetFormation() )
		guardPoint = pUnit->GetUnitPointInFormation();

	pUnit->StopUnit();
}
void CSoldierRestState::Segment()
{
	if ( !pUnit->GetArtilleryIfCrew() )
	{
		bool bTargetFound = false;
		
		if ( pUnit->GetFormation()->IsInWaitingState() )
		{
			const BYTE cResult = pUnit->AnalyzeTargetScan( 0, false, false );

			if ( cResult & 2 )
				bScanned = true;

			bTargetFound = cResult & 1;
		}
		else
			bScanned = true;

		if ( !bTargetFound )
		{
			if ( bScanned && curTime >= nextMove && pUnit->CanMoveForGuard() && pUnit->IsIdle() )
			{
				const CVec2 vUnitCenter = pUnit->GetCenter();

				bool bSent = false;
				guardPoint = pUnit->GetUnitPointInFormation();
				if ( fabs2( guardPoint - vUnitCenter ) > 200.0f )
				{
					CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( guardPoint, VNULL2, pUnit ); 

					if ( pStaticPath )
					{
						const float fNewDistance = fabs2( vUnitCenter - pStaticPath->GetFinishPoint() );
						if ( fDistToGuardPoint == -1.0f ||
								 fNewDistance < fDistToGuardPoint - 2.0f * SAIConsts::TILE_SIZE &&
								 fNewDistance > sqr( 2.0f * SAIConsts::TILE_SIZE ) )
						{
							fDistToGuardPoint = fNewDistance;
							
							pUnit->SendAlongPath( pStaticPath, VNULL2 );
							pUnit->UnRegisterAsBored( ACK_BORED_IDLE );
							bSent = true;
						}
					}
				}

				if ( !bSent )
					pUnit->RegisterAsBored( ACK_BORED_IDLE );

				nextMove = curTime + Random( 0, 4000 );

				pUnit->FreezeByState( true );
			}
		}
	}
}
ETryStateInterruptResult CSoldierRestState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->UnRegisterAsBored( ACK_BORED_IDLE );
	pUnit->SetCommandFinished();

	return TSIR_YES_IMMIDIATELY;
}
IUnitState* CSoldierAttackState::Instance( CAIUnit *pUnit, CAIUnit *pEnemy, bool bAim, const bool bSwarmAttack )
{
	return new CSoldierAttackState( pUnit, pEnemy, bAim, bSwarmAttack );
}
CSoldierAttackState::CSoldierAttackState( CAIUnit *_pUnit, CAIUnit *_pEnemy, bool _bAim, const bool _bSwarmAttack )
: pUnit( _pUnit ), pEnemy( _pEnemy ),
	nextShootCheck( 0 ), lastEnemyTile( -1, -1 ), bAim( _bAim ), wLastEnemyDir( 0 ), pGun( 0 ), bFinish( false ),
	lastEnemyCenter( -1, -1 ), bSwarmAttack( _bSwarmAttack ),
	runUpToEnemy( _pUnit, _pEnemy ), nEnemyParty( _pEnemy->GetParty() )
{
	ResetTime( pEnemy );
}
void CSoldierAttackState::FireNow()
{
	NI_ASSERT_T( pGun != 0, "Wrong gun descriptor" );

	pUnit->RegisterAsBored( ACK_BORED_ATTACK );
	pUnit->StopUnit();
	pGun->StartEnemyBurst( pEnemy, bAim );
	bAim = false;

	NI_ASSERT_T( runUpToEnemy.IsRunningToEnemy() == false, "Wrong state" );
}
void CSoldierAttackState::StopFire()
{
	if ( pGun )
		pGun->StopFire();

	pUnit->UnRegisterAsBored( ACK_BORED_ATTACK );
	damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
	runUpToEnemy.Finish();

	bFinish = true;
}
ETryStateInterruptResult CSoldierAttackState::TryInterruptState( CAICommand *pCommand )
{
	if ( !pCommand )
	{
		StopFire();
		pUnit->SetCommandFinished();

		return TSIR_YES_IMMIDIATELY;
	}
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	if ( cmd.cmdType == ACTION_COMMAND_ATTACK_UNIT && cmd.pObject.GetPtr() == pEnemy )
		return TSIR_NO_COMMAND_INCOMPATIBLE;

	if ( !pGun || !pGun->IsBursting() )
	{
		StopFire();
		pUnit->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else
	{
		bFinish = true;
		return TSIR_YES_WAIT;
	}
}
void CSoldierAttackState::StartAgain()
{
	if ( pGun )
	{
		pGun->StopFire();
		pGun = 0;
	}

	damageToEnemyUpdater.UnsetDamageFromEnemy( pEnemy );
}
void CSoldierAttackState::AnalyzeBruteMovingPosition()
{
	const bool bIdle = pUnit->IsIdle();
	const bool bEnemyVisible = pEnemy->IsVisible( pUnit->GetParty() );
	if ( !bEnemyVisible && !pEnemy->IsRevealed() )
	{
		if ( bIdle )
			StopFire();
	}
	else
	{
		if ( ( bIdle || curTime >= nextShootCheck ) && bEnemyVisible && pGun->CanShootToUnitWOMove( pEnemy ) )
			FireNow();
		else if ( curTime >= nextShootCheck && pGun->TooCloseToFire( pEnemy ) )
		{
			pUnit->SendAcknowledgement( ACK_ENEMY_IS_TO_CLOSE );
			StopFire();
		}
		else if ( bIdle || curTime >= nextShootCheck && lastEnemyTile != pEnemy->GetTile() )
		{
			const float fRandomDist = 5.0f * SConsts::TILE_SIZE;

			CPtr<IStaticPath> pStaticPath = 
				bEnemyVisible ? 
				CreateStaticPathForAttack( pUnit, pEnemy, pGun->GetWeapon()->fRangeMin, pGun->GetFireRange( 0 ), fRandomDist ) :
				CreateStaticPathToPoint( pEnemy->GetCenter(), VNULL2, pUnit );

			if ( pStaticPath )
			{
				if ( pUnit->GetTile() != pStaticPath->GetFinishTile() )
					bAim = true;
				pUnit->SendAlongPath( pStaticPath, VNULL2 );
			}
			else
			{
				pUnit->SendAcknowledgement( ACK_CANNOT_FIND_PATH_TO_TARGET );
				StopFire();
			}

			lastEnemyTile = pEnemy->GetTile();
			nextShootCheck = curTime + SHOOTING_CHECK + Random( 0, 500 );
		}
	}
}
void CSoldierAttackState::AnalyzeMovingPosition()
{
	const bool bIdle = pUnit->IsIdle();
	const bool bEnemyVisible=  pEnemy->IsVisible( pUnit->GetParty() );

	if ( !bEnemyVisible && !pEnemy->IsRevealed() )
	{
		if ( bIdle )
			StopFire();
	}
	else if ( ( curTime >= nextShootCheck || bIdle ) && bEnemyVisible && pGun->CanShootToUnitWOMove( pEnemy ) )
		FireNow();
	else if ( bIdle || curTime >= nextShootCheck && lastEnemyTile != pEnemy->GetTile() )
	{
		if ( pGun->InGoToSideRange( pEnemy ) )
		{
			lastEnemyTile = SVector( -1, -1 );
			state = ESAS_MOVING_TO_SIDE;
		}
		else if ( pGun->TooCloseToFire( pEnemy ) )
		{
			pUnit->SendAcknowledgement( ACK_ENEMY_IS_TO_CLOSE );
			StopFire();
		}
		else if ( bIdle || lastEnemyTile != pEnemy->GetTile() )
		{
			CPtr<IStaticPath> pStaticPath = 
				CreateStaticPathForAttack( pUnit, pEnemy, pGun->GetWeapon()->fRangeMin, 2 * pGun->GetFireRange( 0 ) );

			if ( pStaticPath )
			{
				if ( pUnit->GetTile() != pStaticPath->GetFinishTile() )
					bAim = true;
				pUnit->SendAlongPath( pStaticPath, VNULL2 );
			}
			else
			{
				pUnit->SendAcknowledgement( ACK_CANNOT_FIND_PATH_TO_TARGET );
				StopFire();
			}

			lastEnemyTile = pEnemy->GetTile();			
			nextShootCheck = curTime + SHOOTING_CHECK + Random( 0, 500 );
		}
	}
}
IStaticPath* CSoldierAttackState::BestSidePath()
{
	const CVec2 vUnitCenter = pUnit->GetCenter();
	const CVec2 vEnemyCenter = pEnemy->GetCenter();
	CVec2 vEnemyDir = pEnemy->GetDirVector();
	const float fEnemyLength = pEnemy->GetStats()->vAABBHalfSize.y;
	const float fEnemyWidth = pEnemy->GetStats()->vAABBHalfSize.x;

	CVec2 vSides[4];

	CVec2 vShift = vEnemyDir * fEnemyLength;
	vSides[0] = vEnemyCenter + vShift;
	vSides[2] = vEnemyCenter - vShift;

	vShift = vEnemyDir * fEnemyWidth;
	std::swap( vShift.x, vShift.y );
	vShift.x = -vShift.x;
	
	vSides[1] = vEnemyCenter + vShift;
	vSides[3] = vEnemyCenter - vShift;
	
	float fMinLength = 1e10;
	int nBestSide = -1;
	for ( int i = 0; i < 4; ++i )
	{
		if ( pGun->CanBreach( pEnemy, i ) )
		{
			const float fDist = fabs2( vUnitCenter - vSides[i] );
			if ( fDist < fMinLength )
			{
				fMinLength = fDist;
				nBestSide = i;
			}
		}
	}

	IStaticPath *pBestPath = 
		CreateStaticPathForSideAttack( pUnit, pEnemy, 
																	 vSides[nBestSide] - vEnemyCenter,
																	 pGun->GetWeapon()->fRangeMin, pGun->GetFireRange( 0 ),
																	 8.0f * SConsts::TILE_SIZE );

	return pBestPath;
}
void CSoldierAttackState::AnalyzeMovingToSidePosition()
{
	const bool bIdle = pUnit->IsIdle();
	const bool bEnemyVisible = pEnemy->IsVisible( pUnit->GetParty() );

	if ( !bEnemyVisible && !pEnemy->IsRevealed() )
	{
		if ( bIdle )
			StopFire();
	}
	else if ( ( curTime >= nextShootCheck || bIdle ) && bEnemyVisible && pGun->CanShootToUnitWOMove( pEnemy ) )
		FireNow();
	else if ( curTime >= nextShootCheck )
	{
		const SVector curEnemyTile = pEnemy->GetTile();
		const WORD wCurEnemyDir = pEnemy->GetDir();
		if ( bIdle || lastEnemyTile != curEnemyTile || DirsDifference( wCurEnemyDir, wLastEnemyDir ) >= ENEMY_DIR_TOLERANCE )
		{
			if ( !pGun->InGoToSideRange( pEnemy ) )
				state = ESAS_MOVING;
			else 
			{
				if ( IStaticPath *pStaticPath = BestSidePath() )
				{
					if ( pUnit->GetTile() != pStaticPath->GetFinishTile() )
						bAim = true;
					pUnit->SendAlongPath( pStaticPath, VNULL2 );
				}
				else
				{
					pUnit->SendAcknowledgement( ACK_CANNOT_FIND_PATH_TO_TARGET );
					StopFire();
				}
			}

			nextShootCheck = curTime + SHOOTING_CHECK + Random( 0, 500 );
			lastEnemyTile = curEnemyTile;
			wLastEnemyDir = wCurEnemyDir;
		}
	}
}
bool CSoldierAttackState::IsBruteMoving()
{
	const int nMinPossiblePiercing = pGun->GetMinPossiblePiercing();
	return
		!pUnit->CanMove() ||
		pEnemy->GetArmor(0) <= nMinPossiblePiercing &&
		pEnemy->GetArmor(1) <= nMinPossiblePiercing &&
		pEnemy->GetArmor(2) <= nMinPossiblePiercing &&
		pEnemy->GetArmor(3) <= nMinPossiblePiercing;
}
void CSoldierAttackState::Segment()
{
	if ( bFinish )
	{
		StopFire();
		pUnit->SetCommandFinished();
	}
	else if ( pGun == 0 )
	{
		if ( IsValidObj( pEnemy ) )
		{
			pUnit->ResetShootEstimator( pEnemy, false );
			pGun = pUnit->GetBestShootEstimatedGun();
			if ( pGun == 0 )
				pUnit->SendAcknowledgement( pUnit->GetGunsRejectReason() );
		}

		if ( pGun == 0 )
			StopFire();
		else
		{
			if ( IsBruteMoving() )
				state = ESAS_BRUTE_MOVING;
			else
				state = ESAS_MOVING;
		}
	}
	else if ( !pGun->IsFiring() && !pGun->CanShootToUnit( pEnemy ) )
		StartAgain();
	else if ( pGun->GetNAmmo() == 0 )
		StopFire();
	else
	{
		damageToEnemyUpdater.SetDamageToEnemy( pUnit, pEnemy, pGun );
		
		if ( !pGun->IsFiring() )
		{
			runUpToEnemy.Segment();			


			if ( !bFinish )
			{
				if ( !runUpToEnemy.IsRunningToEnemy() )
				{
					if ( !IsValidObj( pEnemy ) || pEnemy.GetPtr() == pUnit || pEnemy->GetParty() != nEnemyParty )
					{
						if ( IsValidObj( pEnemy ) && !pEnemy->IsVisible( pUnit->GetParty() ) && !pEnemy->IsRevealed() )
							pUnit->SendAcknowledgement( ACK_DONT_SEE_THE_ENEMY );
						
						StopFire();
						pUnit->SetCommandFinished();
					}
					else switch ( state )
					{
						case ESAS_BRUTE_MOVING:
							AnalyzeBruteMovingPosition();

							break;
						case ESAS_MOVING:
							AnalyzeMovingPosition();

							break;
						case ESAS_MOVING_TO_SIDE:
							AnalyzeMovingToSidePosition();

							break;
					}
				}
			}
		}
	}
}
const CVec2 CSoldierAttackState::GetPurposePoint() const
{
	if ( IsValidObj( pEnemy ) )
		return pEnemy->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierMoveToState::Instance( CAIUnit *pUnit, const CVec2 &point )
{
	return new CSoldierMoveToState( pUnit, point );
}
CSoldierMoveToState::CSoldierMoveToState( CAIUnit *_pUnit, const CVec2 &_point )
: pUnit( _pUnit ), startTime( curTime ), bWaiting( true ), CFreeFireManager( _pUnit ),
	point( _point ), wDirToPoint( _pUnit->GetFrontDir() )
{
	pUnit->UnlockTiles();
	pUnit->FixUnlocking();
}
void CSoldierMoveToState::Segment()
{
	if ( bWaiting )
	{
		if ( curTime - startTime >= TIME_OF_WAITING )
		{
			bWaiting = false;

			if ( CPtr<IStaticPath> pStaticPath = pUnit->GetCurCmd()->CreateStaticPath( pUnit ) )
			{
				point += pUnit->GetGroupShift();
				wDirToPoint = GetDirectionByVector( point - pUnit->GetCenter() );

				CBasicGun *pMainGun = pUnit->GetGuns()->GetMainGun();
				if ( pMainGun )
					pUnit->Lock( pMainGun );
				pUnit->SendAlongPath( pStaticPath, pUnit->GetGroupShift() );
			}
			else
			{
				pUnit->SendAcknowledgement( ACK_NEGATIVE );
				pUnit->SetCommandFinished();
			}

			pUnit->UnfixUnlocking();
		}
	}
	else
	{
		if ( !pUnit->GetStats()->IsInfantry() )
			CFreeFireManager::Analyze( pUnit, 0 );
		if ( pUnit->IsIdle() || pUnit->GetNextCommand() != 0 && fabs2( pUnit->GetCenter() - point ) <= sqr( 2.5f * (float)SConsts::TILE_SIZE ) )
		{
			CBasicGun *pMainGun = pUnit->GetGuns()->GetMainGun();
			if ( pMainGun )
				pUnit->Unlock( pMainGun );
			
			if ( pUnit->GetNextCommand() == 0 )
			{
				const WORD wDir = pUnit->GetFrontDir() == pUnit->GetDir() ? wDirToPoint : wDirToPoint + 32768;
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_GUARD, point, wDir ), pUnit, false );
			}

			pUnit->SetCommandFinished();
		}
	}
}
ETryStateInterruptResult CSoldierMoveToState::TryInterruptState( class CAICommand *pCommand )
{ 
	pUnit->UnfixUnlocking();

	CBasicGun *pMainGun = pUnit->GetGuns()->GetMainGun();
	if ( pMainGun )
		pUnit->Unlock( pMainGun );

	pUnit->SetCommandFinished();

	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierMoveToState::GetPurposePoint() const
{
	return point;
}
IUnitState* CSoldierTurnToPointState::Instance( CAIUnit *pUnit, const CVec2 &targCenter )
{
	return new CSoldierTurnToPointState( pUnit, targCenter );
}
CSoldierTurnToPointState::CSoldierTurnToPointState( CAIUnit *_pUnit, const CVec2 &_targCenter )
: pUnit( _pUnit ), lastCheck( curTime ), targCenter( _targCenter )
{
	pUnit->StopUnit();
}
void CSoldierTurnToPointState::Segment()
{
	if ( !pUnit->CanRotateTo( pUnit->GetUnitRect(), targCenter - pUnit->GetCenter(), false, false ) )
	{
		pUnit->SendAcknowledgement( ACK_NEGATIVE );
		pUnit->SetCommandFinished();
	}
	else
	{
		if ( pUnit->TurnToUnit( targCenter ) )
			TryInterruptState( 0 );
		else
			lastCheck = curTime;
	}
}
ETryStateInterruptResult CSoldierTurnToPointState::TryInterruptState( class CAICommand *pCommand )
{ 
	pUnit->SetCommandFinished(); 
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierTurnToPointState::GetPurposePoint() const
{
	if ( pUnit && pUnit->IsValid() && pUnit->IsAlive() )
		return pUnit->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierMoveByDirState::Instance( CAIUnit *pUnit, const CVec2 &vTarget )
{
	return new CSoldierMoveByDirState( pUnit, vTarget );
}
CSoldierMoveByDirState::CSoldierMoveByDirState( CAIUnit *_pUnit, const CVec2 &vTarget )
: pUnit( _pUnit )
{
	IPath *pPath = new CStandartDirPath( pUnit->GetCenter(), Norm( vTarget - pUnit->GetCenter() ), vTarget );
	pUnit->SendAlongPath( pPath );
}
void CSoldierMoveByDirState::Segment()
{
	if ( pUnit->IsIdle() )
		pUnit->SetCommandFinished();
}
ETryStateInterruptResult CSoldierMoveByDirState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierMoveByDirState::GetPurposePoint() const
{
	return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierEnterState::Instance( CAIUnit *pUnit, CBuilding *pBuilding )
{
	return new CSoldierEnterState( pUnit, pBuilding );
}
CSoldierEnterState::CSoldierEnterState( CAIUnit *_pUnit, CBuilding *_pBuilding )
:	pUnit( _pUnit ), pBuilding( _pBuilding ), state( EES_START ), nEfforts( 0 )
{
	int i = 0;
	while ( i < pBuilding->GetNEntrancePoints() )
	{
		if ( pBuilding->IsGoodPointForRunIn( pUnit->GetCenter(), i ) )
		{
			nEntrance = i;
			state = EES_RUN_UP;
			break;
		}
		else
			++i;
	}
}
bool CSoldierEnterState::SetPathForRunUp()
{
	CPtr<IStaticPath> pBestPath = pUnit->GetPathToBuilding( pBuilding, &nEntrance );
	
	if ( pBestPath == 0 )
		return false;
	else
	{
		pUnit->SendAlongPath( pBestPath, VNULL2 );
		return true;
	}
}
void CSoldierEnterState::Segment()
{
	if ( !IsValidObj( pBuilding ) || pBuilding->GetNFreePlaces() == 0 )
		TryInterruptState( 0 );
	else
	{
		switch ( state )
		{
			case EES_START:
				if ( SetPathForRunUp() )
					state = EES_RUN_UP;
				else
					pUnit->SetCommandFinished();

				break;
			case EES_RUN_UP:
				if ( pUnit->IsIdle() )
				{
					if ( nEfforts < 3 && !pBuilding->IsGoodPointForRunIn( pUnit->GetCenter(), nEntrance, SConsts::TILE_SIZE ) )
					{
						++nEfforts;
						state = EES_START;
					}
					else
					{
						pUnit->TurnToDir( GetDirectionByVector( pBuilding->GetCenter() - pUnit->GetCenter() ), false );
						theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_BUILDING, pBuilding ), pUnit, false );
					}
				}

				break;
		}
	}
}
ETryStateInterruptResult CSoldierEnterState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierEnterState::GetPurposePoint() const
{
	if ( IsValidObj( pBuilding ) )
		return pBuilding->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierEnterEntrenchmentState::Instance( CAIUnit *pUnit, CEntrenchment *pEntrenchment )
{
	return new CSoldierEnterEntrenchmentState( pUnit, pEntrenchment );
}
CSoldierEnterEntrenchmentState::CSoldierEnterEntrenchmentState( CAIUnit *_pUnit, CEntrenchment *_pEntrenchment )
: pUnit( _pUnit ), state( EES_START ), pEntrenchment( _pEntrenchment )
{
	if ( pEntrenchment->IsPointInside( pUnit->GetCenter() ) )
		state = EES_RUN;
}
bool CSoldierEnterEntrenchmentState::SetPathForRunIn()
{
	CPtr<IStaticPath> pPath = pUnit->GetPathToEntrenchment( pEntrenchment );

	if ( pPath )
	{
		pUnit->SendAlongPath( pPath, VNULL2 );
		return true;
	}
	else
		return false;
}
void CSoldierEnterEntrenchmentState::EnterToEntrenchment()
{
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRENCH, pEntrenchment ), pUnit, false );
}
void CSoldierEnterEntrenchmentState::Segment()
{
	switch ( state )
	{
		case EES_START:
			if ( SetPathForRunIn() )
				state = EES_RUN;
			else
				pUnit->SetCommandFinished();

			break;
		case EES_RUN:
			if ( !IsValidObj( pEntrenchment ) )
				pUnit->SetCommandFinished();
			else if ( pEntrenchment->IsPointInside( pUnit->GetCenter() ) )
			{
				EnterToEntrenchment();
				state = EES_FINISHED;
			}
			else if ( pUnit->IsIdle() )
				state = EES_START;

			break;
	}
}
ETryStateInterruptResult CSoldierEnterEntrenchmentState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand || 
				pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_IDLE_TRENCH ||
				state != EES_FINISHED )
	{
		pUnit->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else
		return TSIR_YES_WAIT;
}
const CVec2 CSoldierEnterEntrenchmentState::GetPurposePoint() const
{
	return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierAttackCommonStatObjState::Instance( CAIUnit *pUnit, CStaticObject *pObj, bool bSwarmAttack )
{
	return new CSoldierAttackCommonStatObjState( pUnit, pObj, bSwarmAttack );
}
CSoldierAttackCommonStatObjState::CSoldierAttackCommonStatObjState( CAIUnit *_pUnit, CStaticObject *pObj, bool _bSwarmAttack )
: pUnit( _pUnit ), CCommonAttackCommonStatObjState( _pUnit, pObj, _bSwarmAttack )
{
}
void CSoldierAttackCommonStatObjState::FireNow()
{
	NI_ASSERT_T( pGun != 0, "Wrong gun descriptor" );	
	
	pUnit->StopUnit();
	if ( pGun->IsOnTurret() )
		pGun->GetTurret()->Lock( pGun );
	pGun->StartPointBurst( pObj->GetAttackCenter( pUnit->GetCenter() ), bAim );
}
ETryStateInterruptResult CSoldierAttackCommonStatObjState::TryInterruptState( class CAICommand *pCommand )
{
	if ( pCommand == 0 || !pGun.IsValid() || !pGun->IsBursting() )
	{
		FinishState();
		pUnit->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}

	if ( pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_ATTACK_UNIT ||
				pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_SWARM_ATTACK_UNIT )
	{
		if ( checked_cast_ptr<CAIUnit*>(pCommand->ToUnitCmd().pObject)->GetStats()->IsInfantry() )
		{
			CSoldier *pSoldier = checked_cast_ptr<CSoldier*>(pCommand->ToUnitCmd().pObject);
			if ( pSoldier->GetBuilding() == pObj )
				return TSIR_NO_COMMAND_INCOMPATIBLE;
		}
	}

	bFinish = true;
	return TSIR_YES_WAIT;
}
void CSoldierAttackCommonStatObjState::Segment()
{
	CCommonAttackCommonStatObjState::Segment();

	if ( bSwarmAttack )
		pUnit->AnalyzeTargetScan( 0, false, false, pObj );
}
CAIUnit* CSoldierAttackCommonStatObjState::GetUnit() const
{ 
	return pUnit; 
}
const CVec2 CSoldierAttackCommonStatObjState::GetPurposePoint() const
{
	if ( GetTarget() && GetTarget()->IsValid() && GetTarget()->IsAlive() && pUnit && pUnit->IsValid() && pUnit->IsAlive() )
		return GetTarget()->GetAttackCenter( pUnit->GetCenter() );
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierParadeState::Instance( class CAIUnit *pUnit )
{
	if ( pUnit->GetStats()->IsInfantry() && static_cast<CSoldier*>(pUnit)->IsInFormation() )
	{
		if ( CPtr<IStaticPath> pPath = CreateStaticPathToPoint( static_cast<CSoldier*>(pUnit)->GetUnitPointInFormation(), VNULL2, pUnit ) )
		{
			pUnit->SendAlongPath( pPath, VNULL2 );
			return new CSoldierParadeState( pUnit );
		}
		else
			return 0;
	}
	else
		return 0;
}
CSoldierParadeState::CSoldierParadeState( class CAIUnit *_pUnit )
: pUnit( _pUnit )
{
}
void CSoldierParadeState::Segment()
{
	if ( pUnit->IsIdle() )
	{
		pUnit->SetCommandFinished();
		pUnit->UpdateDirection( pUnit->GetFormation()->GetUnitDir( pUnit->GetFormationSlot() ) );
	}
}
ETryStateInterruptResult CSoldierParadeState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierParadeState::GetPurposePoint() const
{
	if ( pUnit && pUnit->IsValid() && pUnit->IsAlive() )
		return pUnit->GetFormation()->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierPlaceMineNowState::Instance( class CAIUnit *pUnit, const CVec2 &point, const enum SMineRPGStats::EType nType )
{
	return new CSoldierPlaceMineNowState( pUnit, point, nType );
}
CSoldierPlaceMineNowState::CSoldierPlaceMineNowState( class CAIUnit *_pUnit, const CVec2 &_point, const enum SMineRPGStats::EType _nType )
: pUnit( _pUnit ), point( _point ), nType( _nType )
{
	updater.Update( ACTION_NOTIFY_USE_DOWN, pUnit );
	beginAnimTime = curTime;
}
void CSoldierPlaceMineNowState::Segment()
{
	if ( pUnit->GetPlayer() == theDipl.GetNeutralPlayer() )
		pUnit->SetCommandFinished();
	else if ( curTime - beginAnimTime >= pUnit->GetStats()->GetAnimTime( GetAnimationFromAction( ACTION_NOTIFY_USE_DOWN ) ) )
	{	
		theUnitCreation.CreateMine( static_cast<SMineRPGStats::EType>(nType), point, pUnit->GetPlayer() );
		pUnit->SetCommandFinished();		
	}
}
ETryStateInterruptResult CSoldierPlaceMineNowState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand )
	{
		pUnit->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}

	return TSIR_YES_WAIT;
}
const CVec2 CSoldierPlaceMineNowState::GetPurposePoint() const
{
	return point;
}
IUnitState* CSoldierClearMineRadiusState::Instance( CAIUnit *pUnit, const CVec2 &clearCenter )
{
	return new CSoldierClearMineRadiusState( pUnit, clearCenter );
}
CSoldierClearMineRadiusState::CSoldierClearMineRadiusState( CAIUnit *_pUnit, const CVec2 &_clearCenter )
: pUnit( _pUnit ), clearCenter( _clearCenter ), eState( EPM_START )
{
}
bool CSoldierClearMineRadiusState::FindMineToClear()
{
	float fBestDistance2 = 1e10;
	pMine = 0;
	
	CStObjCircleIter<false> iter( clearCenter, SConsts::MINE_CLEAR_RADIUS );
	while ( !iter.IsFinished() )
	{
		if ( (*iter)->GetObjectType() == ESOT_MINE && 
				!static_cast<CMineStaticObject*>(*iter)->IsBeingDisarmed() && 
				fabs2( (*iter)->GetCenter() - clearCenter ) <= sqr( float(SConsts::MINE_CLEAR_RADIUS) ) )
		{
			const float fDistanceToUnit2 = fabs2( (*iter)->GetCenter() - pUnit->GetCenter() );			
			if ( fDistanceToUnit2 < fBestDistance2 )
			{
				if ( CPtr<IStaticPath> pPath = CreateStaticPathToPoint( (*iter)->GetCenter(), VNULL2, pUnit, true ) )
				{
					pMine = static_cast<CMineStaticObject*>(*iter);
					fBestDistance2 = fDistanceToUnit2;
				}
			}
		}

		iter.Iterate();
	}

	if ( pMine != 0 )
	{
		pMine->SetBeingDisarmed( true );
		pUnit->SendAlongPath( CreateStaticPathToPoint( pMine->GetCenter(), VNULL2, pUnit, true ), VNULL2 );

		return true;
	}
	else
		return false;
}
void CSoldierClearMineRadiusState::Segment()
{
	if ( pMine != 0 && !IsValidObj( pMine ) )
	{
		pMine = 0;
		eState = EPM_START;
	}
	else switch ( eState )
	{
		case EPM_START:
			if ( FindMineToClear() )
				eState = EPM_MOVE;
			else
				pUnit->SetCommandFinished();

			break;
		case EPM_MOVE:
			if ( !pMine->IsValid() || !pMine->IsAlive() )
			{
				pMine = 0;
				eState = EPM_START;
			}
			else if ( mDistance( pUnit->GetCenter(), pMine->GetCenter() ) <= SConsts::TILE_SIZE )
			{
				pUnit->StopUnit();
				
				eState = EPM_WAITING; 
				updater.Update( ACTION_NOTIFY_USE_DOWN, pUnit );
				beginAnimTime = curTime;			
			}
			else if ( pUnit->IsIdle() )
			{
				if( IsValidObj( pMine ) )
					pMine->SetBeingDisarmed( false );
				eState = EPM_START;
			}

			break;		
		case EPM_WAITING:
			if ( curTime - beginAnimTime >= pUnit->GetStats()->GetAnimTime( GetAnimationFromAction( ACTION_NOTIFY_USE_DOWN ) ) )
			{
				pMine->Delete();
				pMine = 0;
				eState = EPM_START;
			}

			break;
	}
}
ETryStateInterruptResult CSoldierClearMineRadiusState::TryInterruptState( class CAICommand *pCommand )
{
	if ( eState != EPM_WAITING || !pUnit->IsAlive() || !pCommand || pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_DISAPPEAR )
	{
		pUnit->SetCommandFinished();
		if( IsValidObj( pMine ) )
			pMine->SetBeingDisarmed( false );

		return TSIR_YES_IMMIDIATELY;
	}
	else
		return TSIR_YES_WAIT;
}
void CSoldierAttackUnitInBuildingState::FireNow()
{
	pUnit->StopUnit();

	if ( pGun->IsOnTurret() )
		pGun->GetTurret()->Lock( pGun );
	pGun->StartEnemyBurst( pTarget, bAim );
	bAim = false;
}
IUnitState* CSoldierAttackUnitInBuildingState::Instance( CAIUnit *pUnit, CSoldier *pTarget, bool bAim, const bool bSwarmAttack )
{
	return new CSoldierAttackUnitInBuildingState( pUnit, pTarget, bAim, bSwarmAttack );
}
CSoldierAttackUnitInBuildingState::CSoldierAttackUnitInBuildingState( CAIUnit *_pUnit, CSoldier *pTarget, bool bAim, const bool bSwarmAttack )
: pUnit( _pUnit ), bTriedToShootBuilding( false ), CCommonAttackUnitInBuildingState( _pUnit, pTarget, bAim, bSwarmAttack )
{
}
void CSoldierAttackUnitInBuildingState::Segment()
{
	if ( IsValidObj( pTarget ) && !pTarget->IsInBuilding() )
		pUnit->SetCommandFinished();
	else if ( !bTriedToShootBuilding && IsValidObj( pTarget ) )
	{
		bTriedToShootBuilding = true;

		if ( !pUnit->GetStats()->IsInfantry() && pUnit->ChooseGunForStatObjWOTime( pTarget->GetBuilding() ) )
		{
			const EActionCommand cmd = bSwarmAttack ? ACTION_COMMAND_SWARM_ATTACK_OBJECT : ACTION_COMMAND_ATTACK_OBJECT;
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( cmd, pTarget->GetBuilding() ), pUnit );
			pUnit->SetCommandFinished();
		}
	}
	else
		CCommonAttackUnitInBuildingState::Segment();
}
ETryStateInterruptResult CSoldierAttackUnitInBuildingState::TryInterruptState( class CAICommand *pCommand )
{
	FinishState();
	return TSIR_YES_IMMIDIATELY;
}
CAIUnit* CSoldierAttackUnitInBuildingState::GetUnit() const
{  
	return pUnit; 
}
const CVec2 CSoldierAttackUnitInBuildingState::GetPurposePoint() const
{
	if ( GetTarget() && GetTarget()->IsValid() && GetTarget()->IsAlive() )
		return GetTarget()->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierEnterTransportNowState::Instance( CAIUnit *pUnit, CMilitaryCar *pTransport )
{
	return new CSoldierEnterTransportNowState( pUnit, pTransport );
}
CSoldierEnterTransportNowState::CSoldierEnterTransportNowState( CAIUnit *_pUnit, CMilitaryCar *_pTransport )
: pUnit( _pUnit ), pTransport( _pTransport ), eState( EETS_START ), 
	vLastTransportCenter( _pTransport->GetCenter() ), wLastTransportDir( _pTransport->GetFrontDir() )
{
}
void CSoldierEnterTransportNowState::Segment()
{
	if ( !IsValidObj( pTransport ) )
		pUnit->SetCommandFinished();
	else
	{
		if ( curTime - timeLastTrajectoryUpdate > pUnit->GetBehUpdateDuration() )
		{
			if ( vLastTransportCenter != pTransport->GetCenter() || wLastTransportDir != pTransport->GetFrontDir() )
				eState = EETS_START;

			timeLastTrajectoryUpdate = curTime;
		}

		switch ( eState )
		{
			case EETS_START:
				{
					if ( CPtr<IStaticPath> pPath = CreateStaticPathToPoint( pTransport->GetEntrancePoint(), VNULL2, pUnit, true ) )
					{
						pUnit->SendAlongPath( pPath, VNULL2 );
						eState = EETS_MOVING;
						timeLastTrajectoryUpdate = curTime;
					}
					else
						pUnit->SetCommandFinished();
				}

				break;
			case EETS_MOVING:
				if ( pUnit->IsIdle() )
				{
					pUnit->TurnToDir( GetDirectionByVector( pTransport->GetCenter() - pUnit->GetCenter() ), false );

					theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRANSPORT, pTransport ), pUnit, false );
					eState = EETS_FINISHED;
				}

				break;
		}
	}
}
ETryStateInterruptResult CSoldierEnterTransportNowState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierEnterTransportNowState::GetPurposePoint() const
{
	if ( IsValidObj( pTransport ) )
		return pTransport->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierParaDroppingState::Instance( class CSoldier *_pUnit )
{
	return new CSoldierParaDroppingState( _pUnit );
}
CSoldierParaDroppingState::CSoldierParaDroppingState( class CSoldier *_pUnit )
: pUnit( _pUnit ), eState( ESPDS_OPEN_PARASHUTE )
{
	pUnit->AllowLieDown( false );
	pUnit->SetCurPath( new CParatrooperPath( CVec3(pUnit->GetCenter().x, pUnit->GetCenter().y,pUnit->GetZ() )) );
	
	updater.Update( ACTION_NOTIFY_NEW_UNIT, pUnit );
	updater.Update( ACTION_NOTIFY_NEW_FORMATION, pUnit );

	updater.Update( ACTION_NOTIFY_CHANGE_DBID, pUnit, theUnitCreation.GetParadropperDBID( pUnit->GetPlayer() ) );
	updater.Update( ACTION_NOTIFY_RPG_CHANGED, pUnit );
	updater.Update( ACTION_NOTIFY_CHANGE_VISIBILITY, pUnit, pUnit->IsVisibleByPlayer() );
	
	updater.Update( ACTION_NOTIFY_FALLING, pUnit );
	eState = ESPDS_FALLING;

	const SInfantryRPGStats *pStats = NGDB::GetRPGStats<SInfantryRPGStats>( theUnitCreation.GetObjectDB(), 
		                                                                       theUnitCreation.GetParadropperDBID(pUnit->GetPlayer()) );

	timeToOpenParashute = curTime + 2000;
	timeToFallWithParashute  =  timeToOpenParashute + pStats->GetAnimTime( GetAnimationFromAction(ACTION_NOTIFY_OPEN_PARASHUTE) );
	timeToCloseParashute = curTime + CParatrooperPath::CalcFallTime( pUnit->GetZ() ) + 
		pStats->GetAnimTime( GetAnimationFromAction(ACTION_NOTIFY_CLOSE_PARASHUTE) );
}
void CSoldierParaDroppingState::Segment()
{
	bool bAgain = false;
	do
	{
		bAgain = false;

		switch( eState )
		{
		case ESPDS_FALLING:
			if ( timeToOpenParashute <= curTime )
			{
				updater.Update( ACTION_NOTIFY_OPEN_PARASHUTE, pUnit );
				eState = ESPDS_WAIT_FOR_END_UPDATES;
				eStateToSwitch = ESPDS_OPEN_PARASHUTE;
			}
			
			break;
		case ESPDS_OPEN_PARASHUTE:
			if ( timeToFallWithParashute <= curTime )
			{
				updater.Update( ACTION_NOTIFY_PARASHUTE, pUnit );
				eState = ESPDS_WAIT_FOR_END_UPDATES;
				eStateToSwitch = ESPDS_FALLING_W_PARASHUTE;
			}

			break;
		case ESPDS_FALLING_W_PARASHUTE:
			if ( pUnit->GetCurPath()->IsFinished() )
			{
				updater.Update( ACTION_NOTIFY_CLOSE_PARASHUTE, pUnit );
				eState = ESPDS_WAIT_FOR_END_UPDATES;
				eStateToSwitch = ESPDS_CLOSING_PARASHUTE;
			}

			break;
		case ESPDS_CLOSING_PARASHUTE:
			if ( timeToCloseParashute <= curTime )
			{
				updater.Update( ACTION_NOTIFY_CHANGE_DBID, pUnit, pUnit->GetDBID() );
				const CVec2 vCenter = pUnit->GetCenter();
				if ( vCenter.x < 0 || vCenter.y < 0 || 
						!theStaticMap.IsTileInside( AICellsTiles::GetTile(vCenter) ) ||
						theStaticMap.IsLocked( AICellsTiles::GetTile(pUnit->GetCenter()), AI_CLASS_HUMAN ) )
					pUnit->Die( false, 0 ); // при падении на технику - смерть.
				else
				{
					eState = ESPDS_WAIT_FOR_END_UPDATES;
					eStateToSwitch = ESPDS_FINISH_STATE;
				}
			}
			
			break;
		case ESPDS_FINISH_STATE:
			pUnit->RestoreDefaultPath();
			pUnit->AllowLieDown( true );
			updater.Update( ACTION_NOTIFY_RPG_CHANGED, pUnit );

			pUnit->SetCommandFinished();

			break;
		case ESPDS_WAIT_FOR_END_UPDATES:
			{
				eState = eStateToSwitch;
				bAgain = true;
			}
			break;
		}
	} while( bAgain );
}
ETryStateInterruptResult CSoldierParaDroppingState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand )
	{
		updater.Update( ACTION_NOTIFY_CHANGE_DBID, pUnit, pUnit->GetDBID() );
		eState = ESPDS_WAIT_FOR_END_UPDATES;
	}

	return TSIR_YES_WAIT;
}
const CVec2 CSoldierParaDroppingState::GetPurposePoint() const
{
	if ( pUnit && pUnit->IsValid() && pUnit->IsAlive() )
		return pUnit->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierUseSpyglassState::Instance( CSoldier *pSoldier, const CVec2 &point )
{
	return new CSoldierUseSpyglassState( pSoldier, point );
}
CSoldierUseSpyglassState::CSoldierUseSpyglassState( CSoldier *_pSoldier, const CVec2 &point )
: pSoldier( _pSoldier ), vPoint2Look( point )
{
	pSoldier->SendAcknowledgement( ACK_POSITIVE );
	pSoldier->StopUnit();
	const	WORD wDir2Look = GetDirectionByVector( vPoint2Look - pSoldier->GetCenter() );
	pSoldier->TurnToDir( wDir2Look, false );
	pSoldier->SetOwnSightRadius( SConsts::SPY_GLASS_RADIUS );
	pSoldier->SetVisionAngle( SConsts::SPY_GLASS_ANGLE );
	pSoldier->ChangeWarFogState();

	pSoldier->StartCamouflating();

	SetLookAnimation();
}
void CSoldierUseSpyglassState::SetLookAnimation()
{
	if ( pSoldier->GetStats()->type == RPG_TYPE_SNIPER && pSoldier->IsLying() )
		updater.Update( ACTION_NOTIFY_AIM_LYING, pSoldier );
	else
		updater.Update( ACTION_NOTIFY_USE_SPYGLASS, pSoldier );
}
void CSoldierUseSpyglassState::Segment()
{
	if ( pSoldier->IsIdle() )
	{
		const	WORD wDir2Look = GetDirectionByVector( vPoint2Look - pSoldier->GetCenter() );
		if ( pSoldier->GetDir() != wDir2Look )
		{
			pSoldier->TurnToDir( wDir2Look, false );
			SetLookAnimation();
		}
	}
}
ETryStateInterruptResult CSoldierUseSpyglassState::TryInterruptState( CAICommand *pCommand )
{
	pSoldier->RemoveOwnSightRadius();
	pSoldier->SetAngles( 0, 65535 );
	pSoldier->SetVisionAngle( 32768 );
	pSoldier->ChangeWarFogState();

	pSoldier->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierUseSpyglassState::GetPurposePoint() const
{
	if ( pSoldier && pSoldier->IsValid() && pSoldier->IsAlive() )	
		return pSoldier->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierAttackFormationState::Instance( CAIUnit *pUnit, CFormation *pTarget, const bool bSwarmAttack )
{
	return new CSoldierAttackFormationState( pUnit, pTarget, bSwarmAttack );
}
CSoldierAttackFormationState::CSoldierAttackFormationState( CAIUnit *_pUnit, CFormation *_pTarget, const bool _bSwarmAttack )
: pUnit ( _pUnit ), pTarget ( _pTarget ), bSwarmAttack( _bSwarmAttack )
{
}
void CSoldierAttackFormationState::Segment()
{
	if ( !IsValidObj( pTarget ) || pTarget->Size() == 0 )
	{
	}
	else
	{
		if ( bSwarmAttack )
		{
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_MOVE_SWARM_ATTACK_FORMATION, pTarget ), pUnit );
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_SWARM_ATTACK_UNIT, (*pTarget)[0] ), pUnit );
		}
		else
		{
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_MOVE_ATTACK_FORMATION, pTarget ), pUnit );
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_ATTACK_UNIT, (*pTarget)[0] ), pUnit );
		}
	}

	TryInterruptState ( 0 ) ;

}
ETryStateInterruptResult CSoldierAttackFormationState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierAttackFormationState::GetPurposePoint() const
{
	if ( IsValidObj( pTarget ) )
		return pTarget->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CSoldierIdleState::Instance( class CAIUnit *pUnit )
{
	return new CSoldierIdleState( pUnit );
}
CSoldierIdleState::CSoldierIdleState( class CAIUnit *pUnit )
: pUnit( pUnit )
{
}
void CSoldierIdleState::Segment()
{
}
ETryStateInterruptResult CSoldierIdleState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CSoldierIdleState::GetPurposePoint() const
{
	if ( pUnit && pUnit->IsValid() && pUnit->IsAlive() )	
		return pUnit->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
CSoldierAttackAviationState::SPredict::SPredict( const CVec3 &pt, const float _fRange, const NTimer::STime _timeToFire, CAIUnit *pOwner )
	: vPt( pt ), fRange( _fRange ), timeToFire ( _timeToFire )
{
	const CVec2 vCenter( pOwner->GetCenter() );
	const CVec2 vDir( pt.x - vCenter.x, pt.y - vCenter.y );
	
	wHor = GetDirectionByVector( vDir );
	wVer = GetDirectionByVector( fabs(vDir), pt.z );
}
IUnitState* CSoldierAttackAviationState::Instance( class CAIUnit *pUnit, class CAviation *pPlane )
{
	return new CSoldierAttackAviationState( pUnit, pPlane );
}
CSoldierAttackAviationState::CSoldierAttackAviationState ( class CAIUnit *pUnit, class CAviation *pPlane )
: pUnit(pUnit), pPlane(pPlane), eState( SAAS_ESITMATING ), bAttacking( false )
{
	theSupremeBeing.SetAAVisible( pUnit, pPlane->GetParty(), true );
	const CVec2 vCenter = pUnit->GetCenter();
}
void CSoldierAttackAviationState::StopFire()
{
	for ( int i = 0; i < pUnit->GetNGuns(); ++i )
	{
		pUnit->GetGun( i )->StopFire();
		pUnit->GetGun( i )->CanShoot();
	}
}
bool CSoldierAttackAviationState::IsFinishedFire()
{
	int nGuns = pUnit->GetNGuns();
	for ( int i = 0; i < nGuns; ++i )
	{
		if ( pUnit->GetGun( i )->IsFiring() )
			return false;
	}
	return true;
}
bool CSoldierAttackAviationState::CanFireNow() const
{
	int nGuns = pUnit->GetNGuns();
	for ( int i=0; i < nGuns; ++i )
	{
		if ( pUnit->GetGun( i )->CanShootToUnitWOMove( pPlane ) )
			return true;
	}
	return false;
}
void CSoldierAttackAviationState::FireNow()
{
	const int nGuns = pUnit->GetNGuns();
	for ( int i = 0; i < nGuns; ++i )
	{
		CBasicGun *pGun = pUnit->GetGun( i );
		pGun->CanShoot();
	}
}
void CSoldierAttackAviationState::Segment()
{
	if (	eState != SAAS_WAIT_FOR_END_OF_BURST && 
				eState != SAAS_FINISH && 
				( !IsValidObj( pPlane ) || EDI_ENEMY != theDipl.GetDiplStatus( pPlane->GetPlayer(), pUnit->GetPlayer() ) ) )
	{
		eState = SAAS_FINISH;
		StopFire();
	}
	else if ( pUnit->GetStats()->IsArtillery() )
	{
		NI_ASSERT_T( dynamic_cast<CArtillery*>(pUnit) != 0, "Wrong type of unit" );
		CArtillery *pArt = static_cast<CArtillery*>(pUnit);
		if ( !pArt->IsInstalled() )
		{
			pArt->ForceInstallAction();
			return;
		}
	}

	switch ( eState )
	{
	case SAAS_ESITMATING:
		{
			CalcAimPoint();
			eState = SAAS_START_AIMING_TO_PREDICTED_POINT;
			timeLastAimUpdate = curTime;
		}

		break;
	case SAAS_FINISH:
		if ( IsFinishedFire() )
		{
			StopFire();
			pUnit->SetCommandFinished();
		}
		else
		{
			StopFire();
			eState = SAAS_WAIT_FOR_END_OF_BURST;
		}

		break;
	case SAAS_WAIT_FOR_END_OF_BURST:
		if ( IsFinishedFire() )
		{
			StopFire();
			pUnit->SetCommandFinished();
		}
		
		break;
	case SAAS_START_AIMING_TO_PREDICTED_POINT:
		{
			if ( curTime - timeLastAimUpdate > SConsts::AA_BEH_UPDATE_DURATION )
				CalcAimPoint();

			if ( fabs( CVec2( aimPoint.GetPt().x, aimPoint.GetPt().y) - pUnit->GetCenter() ) <= aimPoint.GetRange() )
			{
				pUnit->SendAcknowledgement( ACK_ATTACKING_AVIATION, true );
				bAttacking = true;
				
				eState = SAAS_AIM_TO_PREDICTED_POINT;
				timeOfStartBurst = curTime;
			}
			else
				eState = SAAS_FINISH;
		}

		break;
	case SAAS_AIM_TO_PREDICTED_POINT:
		
		if ( aimPoint.GetPt().z <= 0 )			// don't want to shoot to the ground.
			eState = SAAS_ESITMATING;
		else
		{
			Aim();
			if ( curTime >= aimPoint.GetFireTime() )
			{
				FireNow();
				theAAFeedBacks.Fired( pUnit, pPlane );
				eState = SAAS_FIRING_TO_PREDICTED_POINT;
			}
		}
		break;
	case SAAS_FIRING_TO_PREDICTED_POINT:
		if ( IsFinishedFire() )
		{
			eState = SAAS_ESITMATING;
		}
		break;
	}
}
void CSoldierAttackAviationState::Aim()
{
	const int nGuns = pUnit->GetNGuns();
	for ( int i = 0; i < nGuns; ++i )
	{
		CBasicGun *pGun = pUnit->GetGun( i );
		if ( CTurret *pTurret = pGun->GetTurret() )
		{
			pGun->DontShoot();
			pGun->StartPointBurst( aimPoint.GetPt(), false );
		}
	}	
}
bool CSoldierAttackAviationState::CalcAimPoint()
{
	if ( !IsValidObj( pPlane ) ) return false;
	timeLastAimUpdate = curTime;

	const int nGuns = pUnit->GetNGuns();
	float fShellSpeed =0;
	float fRange = 0;
	CTurret * pTurret = 0;
	CBasicGun * pGunAbleToShoot = 0;
	for ( int i = 0; i < nGuns ; ++i )
	{
		CBasicGun *pGun = pUnit->GetGun( i );
		if ( pGun->CanShootByHeight( pPlane ) )
		{
			pTurret = pGun->GetTurret();
			fRange = pGun->GetFireRangeMax();
			pGunAbleToShoot = pGun;
		}
	}
	
	if ( !pGunAbleToShoot ) 
	{
		StopFire();
		pUnit->SetCommandFinished();

		return false;
	}
	
	CVec3 vPlaneSpeed ;
	pPlane->GetSpeed3( &vPlaneSpeed );
	const float fCircleRadius = pPlane->GetCurPath()->GetCurvatureRadius();
	const CVec2 vCircleCenter = pPlane->GetCurPath()->GetCurvatureCenter();

	float fAngleSpeed = 0;
	if ( 0 != fCircleRadius )
	{
		fAngleSpeed = fabs( vPlaneSpeed ) / fCircleRadius * 65535 / ( 2 * PI );
	}

	const CVec3 vPlaneCenter ( pPlane->GetCenter(),pPlane->GetZ() );
	const CVec3 vUnitCenter  ( pUnit->GetCenter(), pUnit->GetZ() );
	
	CVec3 vPredict = vPlaneCenter;
	float fTime;													// time before shoot, will calcualte iteratively

	for ( int i = 0; i < SConsts::AA_AIM_ITERATIONS ; ++i )
	{
		fTime = pGunAbleToShoot->GetTimeToShootToPoint( vPredict ) + 100;

		if ( 0 == fAngleSpeed )							// straight line motion
			vPredict = vPlaneCenter + vPlaneSpeed * fTime;
		else																// circle motion
		{
			const float fDeltaAngle = fAngleSpeed * fTime;
			const WORD wDeltaAngle = fDeltaAngle >= 0 ? int( fDeltaAngle ) % 65535 : ( 65535 + int( fDeltaAngle ) % 65535 );
			const WORD wResultAnlge = GetDirectionByVector( vPlaneCenter.x - vCircleCenter.x, vPlaneCenter.y - vCircleCenter.y ) + wDeltaAngle;
			vPredict = CVec3( GetVectorByDirection( wResultAnlge ) * fabs(fCircleRadius) + vCircleCenter, vPlaneCenter.z + vPlaneSpeed.z * fTime );
		}
	}
	const int nTimeToShoot = int(fTime) / SConsts::AI_SEGMENT_DURATION * SConsts::AI_SEGMENT_DURATION;

	aimPoint = SPredict( vPredict, fRange, curTime + nTimeToShoot, pUnit );
	return true;
}
ETryStateInterruptResult CSoldierAttackAviationState::TryInterruptState(class CAICommand *pCommand)
{
	if ( !pCommand )
	{
		theSupremeBeing.SetAAVisible( pUnit, pUnit->GetParty() == 0 ? 1 : 0, false );
		StopFire();
		pUnit->SetCommandFinished();

		return TSIR_YES_IMMIDIATELY;
	}
	
	const SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	if ( cmd.cmdType == ACTION_COMMAND_ATTACK_UNIT && cmd.pObject.GetPtr() == pPlane )
		return TSIR_NO_COMMAND_INCOMPATIBLE;

	theSupremeBeing.SetAAVisible( pUnit, pUnit->GetParty() == 0 ? 1 : 0, false );
	eState = SAAS_FINISH;

	return TSIR_YES_WAIT;
}
CAIUnit* CSoldierAttackAviationState::GetTargetUnit() const
{
	return pPlane;
}
IUnitState* CSoldierFireMoraleShellState::Instance( CAIUnit * pUnit, const class CVec2 &vTarget )
{
	return new CSoldierFireMoraleShellState( pUnit, vTarget );
}
CSoldierFireMoraleShellState::CSoldierFireMoraleShellState( CAIUnit * pUnit, const class CVec2 &vTarget )
: pUnit( pUnit ), nMoraleGun( -1 ), vTarget( vTarget )
{
	const int nGuns = pUnit->GetNGuns();
	for ( int i = 0; i < nGuns; ++i )
	{
		CBasicGun * pGun = pUnit->GetGun( i );
		if ( pGun->GetShell().eDamageType == SWeaponRPGStats::SShell::DAMAGE_MORALE )
		{
			if ( !pGun->CanShootToPoint( vTarget, 0.0f ) )
				pUnit->SendAcknowledgement( pGun->GetRejectReason(), true );
			else
			{
				pGun->StartPointBurst( vTarget, true );
				nMoraleGun = i;
				break;
			}
		}
	}
	
	if ( -1 != nMoraleGun )
	{
		pUnit->SendAcknowledgement( ACK_NEGATIVE, true );
		TryInterruptState( 0 );
	}
}
void CSoldierFireMoraleShellState::Segment()
{
	CBasicGun *pGun = pUnit->GetGun( nMoraleGun );
	if ( !pGun->IsFiring() )
		pGun->StartPointBurst( vTarget, false );
}
ETryStateInterruptResult CSoldierFireMoraleShellState::TryInterruptState( CAICommand *pCommand )
{
	if ( -1 != nMoraleGun && pUnit && pUnit->IsAlive() )
		pUnit->GetGun( nMoraleGun )->StopFire();
	return TSIR_YES_IMMIDIATELY;
}
IUnitState* CSoldierUseState::Instance( CAIUnit *pUnit, const EActionNotify &eState )
{
	return new CSoldierUseState( pUnit, eState );
}
CSoldierUseState::CSoldierUseState( CAIUnit *_pUnit, const EActionNotify &_eState )
: pUnit( _pUnit ), eState( _eState )
{
	pUnit->StopUnit();
	updater.Update( eState, pUnit );
	
	NI_ASSERT_T( eState == ACTION_NOTIFY_USE_UP || eState == ACTION_NOTIFY_USE_DOWN, NStr::Format( "Wrong action (%d) in UseState", (int)eState ) );
}
void CSoldierUseState::Segment()
{
}
ETryStateInterruptResult CSoldierUseState::TryInterruptState( CAICommand *pCommand )
{
	pUnit->StopCurAnimation();
	pUnit->SetCommandFinished();
	
	return TSIR_YES_IMMIDIATELY;
}
