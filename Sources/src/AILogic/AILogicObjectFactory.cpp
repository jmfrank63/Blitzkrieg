#include "StdAfx.h"

#include "AILogicObjectFactory.h"
#include "AILogicInternal.h"
#include "Shell.h"
#include "AIEditorInternal.h"
#include "GunsInternal.h"
#include "StaticObjects.h"
#include "StaticObject.h"
#include "Building.h"
#include "Entrenchment.h"
#include "Soldier.h"
#include "Technics.h"
#include "CollisionInternal.h"
#include "SoldierStates.h"
#include "InBuildingStates.h"
#include "InEntrenchmentStates.h"
#include "InTransportStates.h"
#include "PlaneStates.h"
#include "TechnicsStates.h"
#include "TankStates.h"
#include "TransportStates.h"
#include "Commands.h"
#include "Formation.h"
#include "AIClassesID.h"
#include "FormationStates.h"
#include "PathUnit.h"
#include "BasePathUnit.h"
#include "Bridge.h"
#include "ArtilleryStates.h"
#include "ArtRocketStates.h"
#include "Artillery.h"
#include "Mine.h"
#include "Units.h"
#include "MountedGun.h"
#include "UnitCreation.h"
#include "ArtilleryBulletStorage.h"
#include "AntiArtillery.h"
#include "UnitGuns.h"
#include "Turret.h"
#include "PathFinderInternal.h"
#include "Aviation.h"
#include "ShootEstimatorInternal.h"
#include "Fence.h"
#include "StandartPath.h"
#include "PlanePath.h"
#include "ParatrooperPath.h"
#include "TankPitPath.h"
#include "PresizePath.h"
#include "ArtilleryPaths.h"
#include "TrainPath.h"
#include "RailroadGraph.h"
#include "TrainPathUnit.h"
#include "TrainPathFinder.h"
#include "SmokeScreen.h"
#include "GeneralInternal.h"
#include "GeneralTasks.h"
#include "ObstacleInternal.h"
#include "GeneralArtillery.h"
#include "GeneralAirForce.h"
#include "AIUnitInfoForGeneral.h"
#include "EntrenchmentCreation.h"
#include "EnemyRememberer.h"
#include "GeneralIntendant.h"
#include "StandartSmoothMechPath.h"
#include "StandartSmoothSoldierPath.h"
#include "Flag.h"
#include "Graveyard.h"
#include "AnimUnitSoldier.h"
#include "AnimUnitMech.h"
static CAILogicObjectFactory theAILogicObjectFactory;
CAILogicObjectFactory::CAILogicObjectFactory()
{
	REGISTER_CLASS( this, AI_UNITS, CUnits );
	REGISTER_CLASS( this, AI_LOGIC, CAILogic );
	REGISTER_CLASS( this, AI_INFANTRY, CInfantry );
	REGISTER_CLASS( this, AI_AVIATION, CAviation );
	REGISTER_CLASS( this, AI_TANK, CTank );
	REGISTER_CLASS( this, AI_TRANSPORT_UNIT, CAITransportUnit );
	
	REGISTER_CLASS( this, AI_FAKE_BALLISTIC_TRAJ, CFakeBallisticTraj );
	REGISTER_CLASS( this, AI_BOMB_BALLISTIC_TRAJ, CBombBallisticTraj );
	REGISTER_CLASS( this, AI_BALLISTIC_TRAJ, CBallisticTraj );
	REGISTER_CLASS( this, AI_VIS_SHELL, CVisShell );
	REGISTER_CLASS( this, AI_INVIS_SHELL, CInvisShell );
	REGISTER_CLASS( this, AI_BURST_EXPLOSION, CBurstExpl );
	REGISTER_CLASS( this, AI_CUMULATIVE_EXPL, CCumulativeExpl );
	
	REGISTER_CLASS( this, AI_COMMAND, CAICommand );
	REGISTER_CLASS( this, AI_EDITOR, CAIEditor );

	REGISTER_CLASS( this, AI_SIMPLE_STATIC_OBJECT, CSimpleStaticObject );
	REGISTER_CLASS( this, AI_BUILDING, CBuildingSimple );
	REGISTER_CLASS( this, AI_STORAGE, CBuildingStorage);
	
	REGISTER_CLASS( this, AI_ENTRENCHMENT_PART, CEntrenchmentPart );
	REGISTER_CLASS( this, AI_STANDART_PATH_FINDER, CStandartPathFinder );
	REGISTER_CLASS( this, AI_PLANE_PATH_FINDER, CPlanePathFinder );

	REGISTER_CLASS( this, AI_FREE_OF_COLLISIONS, CFreeOfCollisions );
	REGISTER_CLASS( this, AI_GIVING_PLACE_COLLISIONS, CGivingPlaceCollision );
	REGISTER_CLASS( this, AI_WAITING_COLLISION, CWaitingCollision );
	REGISTER_CLASS( this, AI_PLANE_COLLISION, CPlaneCollision );

	REGISTER_CLASS( this, AI_STATIC_PATH, CCommonStaticPath );
	REGISTER_CLASS( this, AI_STANDART_PATH, CStandartPath );
	REGISTER_CLASS( this, AI_STADART_DIR_PATH, CStandartDirPath );
	REGISTER_CLASS( this, AI_PLANE_PATH, CPlanePath );
	REGISTER_CLASS( this, AI_PLANE_SMOOTH_PATH, CPlaneSmoothPath );

	REGISTER_CLASS( this, AI_SOLDIER_STATES_FACTORY, CSoldierStatesFactory );

	REGISTER_CLASS( this, AI_SOLDIER_REST_STATE, CSoldierRestState );
	REGISTER_CLASS( this, AI_SOLDIER_ATTACK_STATE, CSoldierAttackState );
	REGISTER_CLASS( this, AI_SOLDIER_MOVE_TO_STATE, CSoldierMoveToState );
	REGISTER_CLASS( this, AI_SOLDIER_TURN_TO_POINT_STATE, CSoldierTurnToPointState );
	REGISTER_CLASS( this, AI_SOLDIER_MOVE_BY_DIR_STATE, CSoldierMoveByDirState );
	REGISTER_CLASS( this, AI_SOLDIER_ENTER_STATE, CSoldierEnterState );
	REGISTER_CLASS( this, AI_SOLDIER_ENTER_ENTRENCHMENT_STATE, CSoldierEnterEntrenchmentState );

	REGISTER_CLASS( this, AI_IN_BUILDING_STATES_FACTORY, CInBuildingStatesFactory );
	REGISTER_CLASS( this, AI_SOLDIER_REST_IN_BUILDING_STATE, CSoldierRestInBuildingState );
	REGISTER_CLASS( this, AI_IN_ENTRENCHMENT_STATES_FACTORY, CInEntrenchmentStatesFactory );

	REGISTER_CLASS( this, AI_SOLDIER_REST_IN_ENTRENCHMENT_STATE, CSoldierRestInEntrenchmentState );
	REGISTER_CLASS( this, AI_SOLDIER_ATTACK_IN_ENTRENCH_STATE, CSoldierAttackInEtrenchState );
	REGISTER_CLASS( this, AI_IN_TRANSPORT_STATES_FACTORY,	CInTransportStatesFactory );
	REGISTER_CLASS( this, AI_SOLDIER_REST_ON_BOARD_STATE, CSoldierRestOnBoardState );

	REGISTER_CLASS( this, AI_PLANE_STATES_FACTORY, CPlaneStatesFactory );
	REGISTER_CLASS( this, AI_PLANE_REST_STATE, CPlaneRestState );
	REGISTER_CLASS( this, AI_PLANE_BOMB_STATE, CPlaneBombState );
	REGISTER_CLASS( this, AI_TANK_STATES_FACTORY, CTankStatesFactory );

	REGISTER_CLASS( this, AI_TRANSPORT_STATES_FACTORY, CTransportStatesFactory );
	REGISTER_CLASS( this, AI_TRANSPORT_WAIT_PASSENGER_STATE, CTransportWaitPassengerState );
	REGISTER_CLASS( this, AI_TRANSPORT_LAND_STATE, CTransportLandState );

	REGISTER_CLASS( this, AI_ENTRENCHMENT, CEntrenchment );

	REGISTER_CLASS( this, AI_FORMATION, CFormation );
	REGISTER_CLASS( this, AI_FORMATION_STATES_FACTORY, CFormationStatesFactory );
	REGISTER_CLASS( this, AI_FORMATION_REST_STATE, CFormationRestState );
	REGISTER_CLASS( this, AI_FORMATION_MOVE_TO_STATE, CFormationMoveToState );
	REGISTER_CLASS( this, AI_FORMATION_ENTER_BUILDING_STATE, CFormationEnterBuildingState );
	REGISTER_CLASS( this, AI_FORMATION_ENTER_ENTRENCHMENT_STATE, CFormationEnterEntrenchmentState );
	REGISTER_CLASS( this, AI_FORMATION_IDLE_BUILDING_STATE, CFormationIdleBuildingState );
	REGISTER_CLASS( this, AI_FORMATION_IDLE_ENTRENCHMENT_STATE, CFormationIdleEntrenchmentState );
	REGISTER_CLASS( this, AI_FORMATION_LEAVE_BUILDING_STATE, CFormationLeaveBuildingState );
	REGISTER_CLASS( this, AI_FORMATION_LEAVE_ENTRENCHMNENT_STATE, CFormationLeaveEntrenchmentState );
	REGISTER_CLASS( this, AI_FORMATION_PLACE_MINE, CFormationPlaceMine );
	REGISTER_CLASS( this, AI_FORMATION_CLEAR_MINE, CFormationClearMine );
	REGISTER_CLASS( this, AI_FORMATION_ATTACK_UNIT_STATE, CFormationAttackUnitState );
	REGISTER_CLASS( this, AI_FORMATION_ATTACK_COMMON_STAT_OBJ_STATE, CFormationAttackCommonStatObjState );

	REGISTER_CLASS( this, AI_SIMPLE_PATH_UNIT, CSimplePathUnit );

	REGISTER_CLASS( this, AI_SOLDIER_PARADE_STATE, CSoldierParadeState );
	REGISTER_CLASS( this, AI_SOLDIER_PLACE_MINE_NOW_STATE, CSoldierPlaceMineNowState );
	REGISTER_CLASS( this, AI_SOLDIER_CLEAR_MINE_RADIUS_STATE, CSoldierClearMineRadiusState );
	REGISTER_CLASS( this, AI_SOLDIER_ATTACK_COMMON_STAT_OBJ_STATE, CSoldierAttackCommonStatObjState );
	
	REGISTER_CLASS( this, AI_COMMON_SWARM_STATE, CCommonSwarmState );
	
	REGISTER_CLASS( this, AI_BRIDGE_SPAN, CBridgeSpan );
	REGISTER_CLASS( this, AI_ARTILLERY_BULLET_STORAGE, CArtilleryBulletStorage );
	REGISTER_CLASS( this, AI_ANTI_ARTILLERY, CAntiArtillery );
	REGISTER_CLASS( this, AI_INSTALL_STATE, CArtilleryInstallTransportState );
	REGISTER_CLASS( this, AI_UNINSTALL_STATE, CArtilleryUninstallTransportState );
	REGISTER_CLASS( this, AI_ARTILLERY_BEING_TOWED_STATE, CArtilleryBeingTowedState );

	REGISTER_CLASS( this, AI_ARTILLERY_STATES_FACTORY, CArtilleryStatesFactory );
	REGISTER_CLASS( this, AI_ARTILLERY_MOVE_TO_STATE, CArtilleryMoveToState );
	REGISTER_CLASS( this, AI_ARTILLERY_TURN_TO_POINT_STATE, CArtilleryTurnToPointState );
	REGISTER_CLASS( this, AI_ARTILLERY_BOMBARDMENT_STATE, CArtilleryBombardmentState );
	REGISTER_CLASS( this, AI_ARTILLERY_RANGE_AREA_STATE, CArtilleryRangeAreaState );
	REGISTER_CLASS( this, AI_ARTILLERY, CArtillery );

	REGISTER_CLASS( this, AI_ART_ROCKET_STATES_FACTORY, CArtRocketStatesFactory );
	REGISTER_CLASS( this, AI_ART_ROCKET_ATTACK_GROUND_STATE, CArtRocketAttackGroundState );

	REGISTER_CLASS( this, AI_SOLDIER_ATTACK_UNIT_IN_BUILDING_STATE, CSoldierAttackUnitInBuildingState );
	
	REGISTER_CLASS( this, AI_COMMON_AMBUSH_STATE, CCommonAmbushState );
	REGISTER_CLASS( this, AI_MINE, CMineStaticObject );
	REGISTER_CLASS( this, AI_SMOOTH_PATH_MEMENTO, CStandartSmoothPathMemento );
	
	REGISTER_CLASS( this, AI_FORMATION_ROTATE_STATE, CFormationRotateState );

	REGISTER_CLASS( this, AI_UNIT_TURRET, CUnitTurret );
	REGISTER_CLASS( this, AI_MOUNTED_TURRET, CMountedTurret );

	REGISTER_CLASS( this, AI_BASE_GUN, CBaseGun );
	REGISTER_CLASS( this, AI_MOUNTED_GUN, CMountedGun<CBaseGun> );
	REGISTER_CLASS( this, AI_TURRET_GUN, CTurretGun );

	REGISTER_CLASS( this, AI_SOLDIER_ATTACK_IN_BUILDING_STATE, CSoldierAttackInBuildingState ); 
	
	REGISTER_CLASS( this, AI_FORMATION_ENTER_TRANSPORT_STATE, CFormationEnterTransportState );
	REGISTER_CLASS( this, AI_FORMATION_ENTER_TRANSPORT_NOW_STATE, CFormationEnterTransportNowState );
	REGISTER_CLASS( this, AI_FORMATION_IDLE_TRANSPORT_STATE, CFormationIdleTransportState );

	REGISTER_CLASS( this, AI_SOLDIER_ENTER_TRANSPORT_NOW_STATE, CSoldierEnterTransportNowState );
	REGISTER_CLASS( this, AI_SOLDIER_PARADROP_STATE, CSoldierParaDroppingState );
	REGISTER_CLASS( this, AI_SOLDIER_USE_SPYGLASS_STATE, CSoldierUseSpyglassState );
	REGISTER_CLASS( this, AI_SOLDIER_ATTACK_FORMATION_STATE, CSoldierAttackFormationState );
	REGISTER_CLASS( this, AI_SOLDIER_IDLE_STATE, CSoldierIdleState );
	REGISTER_CLASS( this, AI_SOLDIER_ATTACK_AVIATION_STATE, CSoldierAttackAviationState );

	REGISTER_CLASS( this, AI_PLANE_PARADRP_STATE, CPlaneParaDropState );
	REGISTER_CLASS( this, AI_PLANE_FIGHTER_PATROL_STATE, CPlaneFighterPatrolState );
	REGISTER_CLASS( this, AI_PLANE_SHTURMOVIK_PATROL_STATE, CPlaneShturmovikPatrolState );
	REGISTER_CLASS( this, AI_PLANE_LEAVE_STATE, CPlaneLeaveState );
	REGISTER_CLASS( this, AI_PLANE_SCOUT_STATE, CPlaneScoutState );

	REGISTER_CLASS( this, AI_PARATROOPER_PATH, CParatrooperPath );
	REGISTER_CLASS( this, AI_ARTILLERY_CREW_PATH, CArtilleryCrewPath );
	REGISTER_CLASS( this, AI_ARTILLERY_BEING_TOWED_PATH, CArtilleryBeingTowedPath );

	REGISTER_CLASS( this, AI_FORMATION_CATCH_TRANSPORT_STATE, CFormationCatchTransportState );
	REGISTER_CLASS( this, AI_FORMATION_PARADROP_STATE, CFormationParaDropState );

	REGISTER_CLASS( this, AI_TRANSPORT_REPAIR_STATE, CTransportRepairState );
	REGISTER_CLASS( this, AI_TRANSPORT_RESUPPLY_STATE, CTransportResupplyState );
	REGISTER_CLASS( this, AI_TRANSPORT_HOOK_ARTILLERY_STATE, CTransportHookArtilleryState );
	REGISTER_CLASS( this, AI_TRANSPORT_UNHOOK_ARTILLERY_STATE, CTransportUnhookArtilleryState );
	
	REGISTER_CLASS( this, AI_FORMATION_REPAIR_UNIT_STATE, CFormationRepairUnitState );
	REGISTER_CLASS( this, AI_FORMATION_LOAD_RU_STATE, CFormationLoadRuState );
	REGISTER_CLASS( this, AI_FORMATION_RESUPPLY_UNIT_STATE, CFormationResupplyUnitState );
	REGISTER_CLASS( this, AI_FORMATION_PLACE_ANTITANK_STATE, CFormationPlaceAntitankState );

	
	REGISTER_CLASS( this, AI_FORMATION_BUILD_LONGOBJECT_STATE, CFormationBuildLongObjectState );
	REGISTER_CLASS( this, AI_FORMATION_GUNSERVE_STATE,  CFormationGunCrewState );
	REGISTER_CLASS( this, AI_FORMATION_CAPTURE_ARTILLERY_STATE, CFormationCaptureArtilleryState );
	REGISTER_CLASS( this, AI_ENTRENCHMENT_TANKPIT, CEntrenchmentTankPit );
	REGISTER_CLASS( this, AI_TANK_LEAVE_TANKPIT_STATE, CTankPitLeaveState );
	REGISTER_CLASS( this, AI_TANK_TANKPIT_PATH, CTankPitPath );
	REGISTER_CLASS( this, AI_TANK_PRESIZE_PATH, CPresizePath );

	REGISTER_CLASS( this, AI_FORMATION_ATTACK_FORMATION_STATE, CFormationAttackFormationState );
	REGISTER_CLASS( this, AI_FORMATION_USE_SPYGLASS_STATE, CFormationUseSpyglassState );
	
	REGISTER_CLASS( this, AI_MECH_UNIT_GUNS, CMechUnitGuns );
	REGISTER_CLASS( this, AI_INFANTRY_GUNS, CInfantryGuns );
	REGISTER_CLASS( this, AI_COMMON_GUN_INFO, SCommonGunInfo );
	
	REGISTER_CLASS( this, AI_FORMATION_PARADE_STATE, CFormationParadeState );
	
	REGISTER_CLASS( this, AI_DEAD_UNIT, CDeadUnit );
	REGISTER_CLASS( this, AI_FULL_ENTRENCHMENT, CFullEntrenchment );
	
	REGISTER_CLASS( this, AI_GIVING_PLACE_ROTATE_COLLISION, CGivingPlaceRotateCollision );
	REGISTER_CLASS( this, AI_STOP_COLLISION, CStopCollision );
	REGISTER_CLASS( this, AI_FOLLOW_STATE, CFollowState );
	
	REGISTER_CLASS( this, AI_FULL_BRIDGE, CFullBridge );
	REGISTER_CLASS( this, AI_SPAN_LOCK, CFullBridge::SSpanLock );

	REGISTER_CLASS( this, AI_FORMATION_DISBAND_STATE, CFormationDisbandState );
	REGISTER_CLASS( this, AI_FORMATION_FORM_STATE, CFormationFormState );
	REGISTER_CLASS( this, AI_FORMATION_WAIT_TO_FORM_STATE, CFormationWaitToFormState );
	
	REGISTER_CLASS( this, AI_ARTILLERY_ATTACK_STATE, CArtilleryAttackState );
	REGISTER_CLASS( this, AI_ARTILLERY_ATTACK_COMMON_STAT_OBJ_STATE, CArtilleryAttackCommonStatObjState );
	REGISTER_CLASS( this, AI_ARTILLERY_REST_STATE,	CArtilleryRestState );
	REGISTER_CLASS( this, AI_ARTILLERY_ATTACK_AVIATION_STATE, CArtilleryAttackAviationState );
	REGISTER_CLASS( this, AI_TARNSPORT_RESUPPLY_HUMAN_RESOURCES_STATE, CTransportResupplyHumanResourcesState );
	REGISTER_CLASS( this, AI_TRANSPORT_LOAD_RU_STATE, CTransportLoadRuState );

	REGISTER_CLASS( this, AI_CATCH_FORMATION_STATE, CCatchFormationState );

	REGISTER_CLASS( this, AI_SNIPER, CSniper);

	REGISTER_CLASS( this, AI_FORMATION_SWARM_STATE, CFormationSwarmState );
	REGISTER_CLASS( this, AI_MECH_ATTACK_UNIT_STATE, CMechAttackUnitState );

	REGISTER_CLASS( this, AI_TANK_SHOOT_ESTIMATOR, CTankShootEstimator );
	REGISTER_CLASS( this, AI_SOLDIER_SHOOT_ESTIMATOR, CSoldierShootEstimator );
	REGISTER_CLASS( this, AI_PLANE_DEFFENSIVE_SHOOT_ESTIMATOR, CPlaneDeffensiveFireShootEstimator );
	REGISTER_CLASS( this, AI_SHTURMOVIK_SHOOT_ESTIMATOR, CPlaneShturmovikShootEstimator );
	
	REGISTER_CLASS( this, AI_HIT_INFO, CHitInfo );

	REGISTER_CLASS( this, AI_FENCE, CFence );
	REGISTER_CLASS( this, AI_SOLDIER_HULLDOWN_STATE, CSoldierEntrenchSelfState );
	REGISTER_CLASS( this, AI_SOLDIER_FIREMORALESHELL_STATE, CSoldierFireMoraleShellState );
	
	REGISTER_CLASS( this, AI_TRAIN_PATH, CTrainPath );
	REGISTER_CLASS( this, AI_TRAIN_SMOOTH_PATH, CTrainSmoothPath );									

	REGISTER_CLASS( this, AI_TRAIN_PATH_UNIT, CTrainPathUnit );

	REGISTER_CLASS( this, AI_EDGE_POINT, CEdgePoint );
	REGISTER_CLASS( this, AI_SPLINE_EDGE, CSimpleSplineEdge );
	REGISTER_CLASS( this, AI_RAILROAD, CRailroad );

	REGISTER_CLASS( this, AI_TRAIN_PATH_FINDER, CTrainPathFinder );

	REGISTER_CLASS( this, AI_ZERO_EDGE, CZeroEdge );
	REGISTER_CLASS( this, AI_CARRIAGE_PATH_UNIT, CCarriagePathUnit );
	
	REGISTER_CLASS( this, AI_TRAIN_SMOOTH_PATH_MEMENTO, CTrainSmoothPathMemento );

	REGISTER_CLASS( this, AI_SMOKE_SCREEN, CSmokeScreen );
	
	REGISTER_CLASS( this, AI_GENERAL, CGeneral );
	REGISTER_CLASS( this, AI_GENERAL_DEFEND_PATCH_TASK, CGeneralTaskToDefendPatch );
	REGISTER_CLASS( this, AI_GENERAL_HOLD_REINFORCEMENT_TASK, CGeneralTaskToHoldReinforcement );
	
	REGISTER_CLASS( this, AI_OBSTACLE_STATIC_OBJECT, CObstacleStaticObject );

	REGISTER_CLASS( this, AI_GENERAL_ARTILLERY, CGeneralArtillery );
	REGISTER_CLASS( this, AI_GENERAL_AIR_FORCE, CGeneralAirForce );
	REGISTER_CLASS( this, AI_GENERAL_AIR_FORCE_LAUNCH_FIGHTERS, CGeneralAirForceLaunchFighters );
	REGISTER_CLASS( this, AI_UNIT_INFO_FOR_GENERAL, CAIUnitInfoForGeneral );
	
	REGISTER_CLASS( this, AI_GENERAL_ARTILLERY_GO_TO_POSITION, CGeneralArtilleryGoToPosition );
	REGISTER_CLASS( this, AI_ENTRENCHMENT_CREATION, CEntrenchmentCreation );
	REGISTER_CLASS( this, AI_FENCE_CREATION, CFenceCreation );
	REGISTER_CLASS( this, AI_TRANSPORT_BUILDFENCE_STATE, CTransportBuildFenceState );
	REGISTER_CLASS( this, AI_TRANSPORT_ENTRENCHMENT_STATE, CTransportBuildEntrenchmentState );
	REGISTER_CLASS( this, AI_TRANSPORT_CLEARMINE_STATE, CTransportClearMineState );
	REGISTER_CLASS( this, AI_TRANSPORT_PLACEMINE_STATE, CTransportPlaceMineState );
	REGISTER_CLASS( this, AI_TRANSPORT_PLACEANTITANK_STATE, CTransportPlaceAntitankState );
	REGISTER_CLASS( this, AI_FORMATION_REPAIRBRIDGE_STATE, CFormationRepairBridgeState );
	REGISTER_CLASS( this, AI_PLANE_FLY_DEAD_STATE, CPlaneFlyDeadState );
	REGISTER_CLASS( this, AI_TRANSPORT_REPAIRBRIDGE_STATE, CTransportRepairBridgeState );
	REGISTER_CLASS( this, AI_ENEMYREMEMBERER, CEnemyRememberer );
	REGISTER_CLASS( this, AI_TRANSPORT_BUILDBRIDGE_STATE, CTransportBuildBridgeState );
	REGISTER_CLASS( this, AI_BRIDGE_CREATION, CBridgeCreation );
	REGISTER_CLASS( this, AI_FORMATION_REPAIRBUILSING_STATE, CFormationRepairBuildingState );
	REGISTER_CLASS( this, AI_GENERAL_INTEDANT, CGeneralIntendant );
	REGISTER_CLASS( this, AI_GENERAL_TASK_DEFENDSTORAGE, CGeneralTaskToDefendStorage );
	REGISTER_CLASS( this, AI_TRANSPORT_REPAIRBUILDING_STATE, CTransportRepairBuildingState );
	REGISTER_CLASS( this, AI_GENERAL_TASK_RECAPTURESTORAGE, CGeneralTaskRecaptureStorage );
	REGISTER_CLASS( this, AI_RESUPPLY_CELL_INFO, CResupplyCellInfo );
	REGISTER_CLASS( this, AI_WAIT_FOR_RECAPTURE_STORAGE, CGeneralTaskToDefendStorage::CWaitForChangePlayer );
	REGISTER_CLASS( this, AI_GENERAL_TASK_RESUPPLYCELL, CGeneralTaskToResupplyCell );
	REGISTER_CLASS( this, AI_GENERAL_TASK_CHEKCDANGER, CGeneralTaskCheckCellDanger );

	REGISTER_CLASS( this, AI_MECH_UNIT_REST_STATE, CMechUnitRestState );

	REGISTER_CLASS( this, AI_STANDART_SMOOTH_MECH_PATH, CStandartSmoothMechPath );
	REGISTER_CLASS( this, AI_STANDART_SMOOTH_SOLDIER_PATH, CStandartSmoothSoldierPath );
	
	REGISTER_CLASS( this, AI_FLAG, CFlag );
	REGISTER_CLASS( this, AI_PLANES_FORMATION, CPlanesFormation );
	REGISTER_CLASS( this, AI_PLAN_IN_FORMATION_SMOOTHPATH, CPlaneInFormationSmoothPath );
	REGISTER_CLASS( this, AI_LINE_PATH_FRACTION, CPlaneSmoothPath::CLinePathFraction );
	REGISTER_CLASS( this, AI_ARC_PATH_FRACTION, CPlaneSmoothPath::CArcPathFraction );
	REGISTER_CLASS( this, AI_FORMATION_INSTALL_MORTAR_STATE, CFormationInstallMortarState );

	REGISTER_CLASS( this, AI_TRANSPORT_MOVE_TO_RESUPPLYCELL_STATE, CMoveToPointNotPresize );
	REGISTER_CLASS( this, AI_GENERAL_TASK_SWARM_TO_POINT, CGeneralTaskToSwarmToPoint );
	REGISTER_CLASS( this, AI_GENERAL_DELAYED_TASK_WAIT_FORREADY, CGeneralSwarmWaitForReady );

	REGISTER_CLASS( this, AI_ANIM_UNIT_SOLDIER, CAnimUnitSoldier );
	REGISTER_CLASS( this, AI_ANIM_UNIT_MECH, CAnimUnitMech );
	
	REGISTER_CLASS( this, AI_SOLDIER_USE_STATE, CSoldierUseState );
	REGISTER_CLASS( this, AI_FORMATION_STATE_ENTER_TRANSPORT_CHEAT_PATH, CFormationEnterTransportByCheatPathState );
	
	REGISTER_CLASS( this, AI_FORMATION_ENTER_BUILDING_NOW_STATE, CFormationEnterBuildingNowState );
	REGISTER_CLASS( this, AI_FORMATION_ENTER_ENTRENCHMENT_NOW_STATE, CFormationEnterEntrenchmentNowState );
	
	REGISTER_CLASS( this, AI_MOVE_TO_GRID_STATE, CCommonMoveToGridState );
}
static SModuleDescriptor theModuleDescriptor( "AI Logic (base)", AI_AI, 0x0100, &theAILogicObjectFactory, 0 );
extern "C" BK_EXPORT const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
