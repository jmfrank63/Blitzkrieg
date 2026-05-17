#include "stdafx.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ����������� ���� ������
const WORD SConsts::STANDART_VIS_ANGLE = 32768;

// ����������, � ������� ����������� ����� ��� ������ ��� �������� ����. �������
const int SConsts::MAX_DIST_TO_RECALC_FOG = 55 * SConsts::TILE_SIZE;

const int SConsts::TURN_TOLERANCE = 0;
// ��� ����� ������� � ���� ����� ������ ��������������
const WORD SConsts::DIR_DIFF_TO_SMOOTH_TURNING = 2000;
// ��������� forward iteration �������� ����� ��� �������� ����� �������
const int SConsts::NUMBER_ITERS_TO_LOOK_AHEAD = 7;

// ������������ ����� ���� � ������, ��� ������� ����� ����� �����
const int SConsts::MAX_LEN_TO_GO_BACKWARD = 8;

const int SConsts::SPEED_FACTOR = 800;

const short int SConsts::SPLINE_STEP = 6;

// size of "neighbours scan" cell
const int SConsts::CELL_COEFF = 4;
const int SConsts::CELL_SIZE = SConsts::CELL_COEFF * SConsts::TILE_SIZE;			// must be divisible by TILE_SIZE

const int SConsts::BIG_CELL_COEFF = 8;
const int SConsts::BIG_CELL_SIZE = SConsts::BIG_CELL_COEFF * SConsts::TILE_SIZE;

// ������ ������ ��� ������ ������������� ��������
const int SConsts::HIT_CELL_COEFF = 8;
const int SConsts::HIT_CELL_SIZE = SConsts::HIT_CELL_COEFF * SConsts::TILE_SIZE;

// max number of tiles, occupied by a unit
const int SConsts::MAX_UNIT_TILE_RADIUS = 5;
const int SConsts::MAX_UNIT_RADIUS = 160;

const int SConsts::BIG_PATH_SHIFT = 10;

// starting sizes of vectors
const int SConsts::AI_START_VECTOR_SIZE = 10;

//
// ������������ ��������� ��� ����, ����� ����� ���� � ����� "�������" ������
const int SConsts::GROUP_DISTANCE = 40 * SConsts::TILE_SIZE;

// ������� ������������ ��� ������ ���������� ����� ��� antiartillery ������
const float SConsts::ANTI_ARTILLERY_SCAN_TIME = 5000;

// ����������� ��� ���������/��������� boundRect
const float SConsts::BOUND_RECT_FACTOR = 1.0f;
// ����������� ��� ���������/��������� boundRect ��� ������� ������
const float SConsts::COEFF_FOR_LOCK = 1.0f;
	// ���������� �� ����� ������� ���������� �� �����, ���� ������ ����� ����� �� ������
const float SConsts::DIST_FOR_LAND = 1.2f * SConsts::TILE_SIZE;
// ���������� �� ����� ������� ���������� �� �����, ���� ������ ����� ����� �� ������ ( ��� �������� "�����������" ����� )
float SConsts::GOOD_LAND_DIST = 1.4f * SConsts::TILE_SIZE;

// ������� ����� �� ������ ��� ����������� ��������
const int SConsts::STATIC_OBJ_CELL = 8;
// ������� ����� �� ������ ��� ����������� container ��������
const int SConsts::STATIC_CONTAINER_OBJ_CELL = 32;

// �������� ������� � ������� HP / tick
float SConsts::CURE_SPEED_IN_BUILDING = 0.001f;
// ����� ����� ������ ������� � ������� ����������� turret � default position
int SConsts::TIME_TO_RETURN_GUN = 5000;
// ���������� ����������� �� �������
int SConsts::NUM_TO_SCAN_IN_SEGM = 50;
// ����� ����� updates ���������
NTimer::STime SConsts::BEH_UPDATE_DURATION = 2000;
// ����� ����� updates ��������� ��� ������
NTimer::STime SConsts::SOLDIER_BEH_UPDATE_DURATION = 3000;
NTimer::STime SConsts::AA_BEH_UPDATE_DURATION = 200;
NTimer::STime SConsts::LONG_RANGE_ARTILLERY_UPDATE_DURATION = 5000;
// �����, ������� �������� �����
NTimer::STime SConsts::DEAD_SEE_TIME = 2000;
// �����, � ������� �������� ������� � ������
int SConsts::TIME_OF_BUILDING_ALARM = 8000;
// �����, ����� ������� ����� ��������������� � ��������� idle
int SConsts::TIME_BEFORE_CAMOUFLAGE = 2000;
// �����, ����� ������� �������� ����� ��������������� � ��������� idle
int SConsts::TIME_BEFORE_SNIPER_CAMOUFLAGE = 1000;
// ����� ������� ��� ���������
int SConsts::TIME_OF_LYING_UNDER_FIRE = 2000;
// cover ��� ������� - �����������, ��� �������
float SConsts::LYING_SOLDIER_COVER = 0.7f;
// ������ � ������ , ������� ������������, ����� ������, ��� ��������� ��� ���
int SConsts::RADIUS_OF_HIT_NOTIFY = 5 * SConsts::TILE_SIZE;
// ������������� ��������, ��� ��������� ��� ���
int SConsts::TIME_OF_HIT_NOTIFY = 1000;

// ������ ( � ������ ), � ������� ������� ����� ����
int SConsts::MINE_VIS_RADIUS = 3 * SConsts::TILE_SIZE;
// ������ ( � ������ ), � ������� ������� ������� ����
int SConsts::MINE_CLEAR_RADIUS = 7 * SConsts::TILE_SIZE;
// ������������ ���������� �������� �� ������ ��������
int SConsts::RADIUS_OF_FORMATION = 10 * SConsts::TILE_SIZE;

// ������, � ������� ����� ������ ��� guard state
float SConsts::GUARD_STATE_RADIUS = 10 * SConsts::TILE_SIZE;

// ��������� �� �������� ��� ��������
float SConsts::LYING_SPEED_FACTOR = 0.5f;

// ������ ��� call for help
int SConsts::CALL_FOR_HELP_RADIUS = 20 * SConsts::TILE_SIZE;
int SConsts::AI_CALL_FOR_HELP_RADIUS = 20 * SConsts::TILE_SIZE;

// �����, ������� ������� ���������� � ������ ����� �������� � ������
NTimer::STime SConsts::CAMPING_TIME = 2000;

// ��������� �� weapon range ��� �������� ���������� ������ �������
float SConsts::INSIDE_OBJ_WEAPON_FACTOR = 0.5f;
// ������ �������, � ������� ���������� ��� ������� ������ �������
NTimer::STime SConsts::INSIDE_OBJ_COMBAT_PERIOD = 800;
// �����, ����� ������� ������� �������� ����� ������
NTimer::STime SConsts::TIME_TO_DISAPPEAR = 5000;

// ��������� ����� ������� install/uninstall ��� ����, ����� ����� install/uninstall ��������������
NTimer::STime SConsts::THRESHOLD_INSTALL_TIME = 2000;

// ���������� ��������� ��� ���������� ���������� �� �������
int SConsts::SHOOTS_TO_RANGE = 4;
// ����������� �� dispersion, ���� ������ �������� �� ������������� �������
float SConsts::RANDGED_DISPERSION_RADIUS_BONUS = 0.5f;
// ������ ������� ����������
float SConsts::RANGED_AREA_RADIUS = 5 * SConsts::TILE_SIZE;

// ����������, �� ������� ����� ���������� ���������� ��� ����, ����� �������� info � � ���������������
float SConsts::RELOCATION_RADIUS = 5 * SConsts::TILE_SIZE;
// ����. ������ �������� ������ ���������� ����������
float SConsts::MAX_ANTI_ARTILLERY_RADIUS = 10 * SConsts::TILE_SIZE;
// ���. ������ �������� ������ ���������� ����������
float SConsts::MIN_ANTI_ARTILLERY_RADIUS = SConsts::TILE_SIZE;
// ���������� ���������, ����� ������ MAX_ANTI_ARTILLERY_RADIUS � MIN_ANTI_ARTILLERY_RADIUS
int SConsts::SHOTS_TO_MINIMIZE_LOCATION_RADIUS = 5;

// �����, ������� �������� ����� ����� �������� �����
NTimer::STime SConsts::AUDIBILITY_TIME = 20000;
// ������������� ��������� ������ ������������������ ������
NTimer::STime SConsts::REVEAL_CIRCLE_PERIOD = 2000;

// ����������� ���������� �����, ��� ������� �� ����� �������������
float SConsts::GOOD_ATTACK_RPOBABILITY = 0.6f;

NTimer::STime SConsts::FIGHTER_PATROL_TIME = 180000;
NTimer::STime SConsts::FIGHTER_PATH_UPDATE_TIME = 7000;
NTimer::STime SConsts::SHTURMOVIK_PATH_UPDATE_TIME = 3000;
float SConsts::PARATROOPER_FALL_SPEED = 0.05f;
int SConsts::PARADROP_SPRED = 4;
int SConsts::RESUPPLY_RADIUS = 1000;
int SConsts::RESUPPLY_RADIUS_MORALE = 300;

NTimer::STime SConsts::TIME_QUANT = 160;
float SConsts::ENGINEER_LOAD_RU_PER_QUANT = 1.1f;
float SConsts::ENGINEER_REPEAR_HP_PER_QUANT = 1.1f; 
float SConsts::ENGINEER_FENCE_LENGHT_PER_QUANT = 1.1f;
float SConsts::ENGINEER_ENTRENCH_LENGHT_PER_QUANT = 1.0f;
float SConsts::ENGINEER_RESUPPLY_PER_QUANT = 1.0f;
float SConsts::ENGINEER_ANTITANK_HALTH_PER_QUANT = 1.0f;
float SConsts::ENGINEER_RU_CARRY_WEIGHT = 100.0f;

// ������ ������ �������
float SConsts::SPY_GLASS_RADIUS = 1920.f;
// ���� ������ �������
WORD SConsts::SPY_GLASS_ANGLE = 5000;

// ����������� �� area damage
float SConsts::AREA_DAMAGE_COEFF = 0.2f;
// ����������� ����, �� ������� ����� ��������� ���� �� ����� ����� turret-��, ����� �������� ���� �������
WORD SConsts::MIN_ROTATE_ANGLE = 6000;

float SConsts::RADIUS_TO_START_ANTIARTILLERY_FIRE = 320.0f;

float SConsts::TRANSPORT_RU_CAPACITY = 500.0f;

// �����, ������� �������� alarm ��� �������� ����� ��������� �� �����������
float SConsts::TIME_OF_ALARM_UNDER_FIRE = 15000.0f;

float SConsts::STORAGE_RESUPPLY_RADIUS = 1000.0f;

// ������������� ������� ��� ����
float SConsts::TRAJ_BOMB_ALPHA = 0.0005f;
float SConsts::TRAJECTORY_BOMB_G = 0.001f;

float SConsts::GUN_CREW_TELEPORT_RADIUS = 5.0f;

float SConsts::PLANE_PARADROP_INTERVAL = 50.0f;
float SConsts::PLANE_PARADROP_INTERVAL_PERP_MIN = 50.0f;
float SConsts::PLANE_PARADROP_INTERVAL_PERP_MAX = 50.0f;

NTimer::STime SConsts::PARATROOPER_GROUND_SCAN_PERIOD = 200;

float SConsts::MORALE_ADDITION_PER_TICK = 0.00005f;
float SConsts::MORALE_DECREASE_PER_TICK = 0.00000001f;
float SConsts::PROBABILITY_TO_DECREASE_MORALE = 0.8f;
float SConsts::MORALE_MIN_VALUE = 0.1f;

float SConsts::TRANSPORT_MOVE_BACK_DISTANCE = 700.0f*700.0f;

int SConsts::TRIES_TO_UNHOOK_ARTILLERY = 2;
NTimer::STime SConsts::ENGINEER_MINE_CHECK_PERIOD; 
float SConsts::PLANE_TILT_PER_SECOND = 1.0f;


float SConsts::FIGHTER_VERTICAL_SPEED_UP = 150.0f;
float SConsts::FIGHTER_VERTICAL_SPEED_DOWN = 300.0f;
float SConsts::PLANE_GUARD_STATE_RADIUS = 40 * SConsts::TILE_SIZE;

float SConsts::TANK_TRACK_HIT_POINTS;

float SConsts::TRAJECTORY_LOW_LINE_RATIO = 0.6f;

const int SConsts::NUMBER_SOLDIER_DIRS = 8;

float SConsts::SHTURMOVIK_APPROACH_RADIUS_SQR = 1000.0f*1000.0f;
float SConsts::SHTURMOVIK_APPROACH_RADIUS = 1000.0f;
float SConsts::PLANE_MIN_HEIGHT = 200.0f;
float SConsts::PLANE_DIVE_FINISH_DISTANCE_SQR	= 100.0f*100.0f;

float SConsts::DIVEBOMBER_VERT_MANEUR_RATIO = 8.0f;
float SConsts::GUNPLANES_VERT_MANEUR_RATIO = 8.0f;
float SConsts::PLANES_HEAVY_FORMATION_SIZE = 2.0f;
float SConsts::PLANES_SMALL_FORMATION_SIZE = 1.2f;
float SConsts::PLANES_START_RANDOM;

float SConsts::SNIPER_CAMOUFLAGE_DECREASE_PER_SHOOT = 0.1f;
float SConsts::SNIPER_CAMOUFLAGE_INCREASE = 0.0004f;
float SConsts::AMBUSH_ATTACK_BEGIN_CIRTERIA = 0.5f;
float SConsts::ARTILLERY_REVEAL_COEEFICIENT = 200.0f;
float SConsts::dispersionRatio[6][2] =
{
/*				TRAJECTORY_LINE				= 0,
			TRAJECTORY_HOWITZER		= 1,
			TRAJECTORY_BOMB				= 2,
			TRAJECTORY_CANNON			= 3,
			TRAJECTORY_ROCKET			= 4,
			TRAJECTORY_GRENADE		= 5*/

	{ 1.0f,		1.0f },
	{ 1.0f,		2.0f },
	{ 1.0f,		3.0f },
	{ 1.0f,		4.0f },
	{ 1.0f,		4.0f },
	{ 1.0f,		1.0f },
};

float SConsts::COEFF_FOR_RANDOM_DELAY = 1.21f;

float SConsts::HEIGHT_FOR_VIS_RADIUS_INC = 1.0f;

float SConsts::BURNING_SPEED = 0.002f;

// follow ���������
float SConsts::FOLLOW_STOP_RADIUS = SConsts::TILE_SIZE * 9;
float SConsts::FOLLOW_EQUALIZE_SPEED_RADIUS = SConsts::TILE_SIZE * 12;
float SConsts::FOLLOW_GO_RADIUS = SConsts::TILE_SIZE * 11;
float SConsts::FOLLOW_WAIT_RADIUS = SConsts::TILE_SIZE * 24;

float SConsts::TRANSPORT_LOAD_RU_DISTANCE = 100.0f;
int SConsts::RESUPPLY_MAX_PATH = 30;
// fatality ���������
float SConsts::FATALITY_PROBABILITY = 0.1f;
float SConsts::DAMAGE_FOR_MASSIVE_DAMAGE_FATALITY = 0.7f;
float SConsts::MASSIVE_DAMAGE_FATALITY_PROBABILITY = 0.8f;

float SConsts::BOMB_START_HEIGHT ;
float SConsts::STAND_LIE_RANDOM_DELAY = 500;
int SConsts::MIN_MECH_TO_DROP_BOMBS = 1;
int SConsts::MIN_INFANTRY_TO_DROP_BOMBS = 20;
float SConsts::TRANSPORT_RESUPPLY_OFFSET = 100;
float SConsts::HP_BALANCE_COEFF = 500.0f;
NTimer::STime SConsts::SQUAD_MEMBER_LEAVE_INTERVAL = 200;
float SConsts::SOLDIER_RU_PRICE = 50;
float SConsts::LOW_HP_PERCENTAGE = 0.2f;
float SConsts::DIRECT_HIT_DAMAGE_COMBAT_SITUATION = 100;
NTimer::STime SConsts::DIRECT_HIT_TIME_COMBAT_SITUATION = 1000;
int SConsts::NUMBER_ENEMY_MECH_MOVING_TO_COMBAT_SITUATION = 3;
int SConsts::NUMBER_ENEMY_INFANTRY_MOVING_TO_COMBAT_SITUATION = 20;

float SConsts::OFFICER_COEFFICIENT_FOR_SCAN = 1.5f;
float SConsts::MAIN_STORAGE_HEALING_SPEED = 10.0f;
float SConsts::RADIUS_TO_TAKE_STORAGE_OWNERSHIP = 100.0f;
float SConsts::TANKPIT_COVER  = 0.5f;
const float SConsts::CLOSEST_TO_RAILROAD_POINT_TOLERANCE = 100.0f;

float SConsts::FENCE_SEGMENT_RU_PRICE = 100;
float SConsts::ENTRENCHMENT_SEGMENT_RU_PRICE = 100;
float SConsts::MINE_RU_PRICE[] = { 1000, 2000 };
float SConsts::ANTITANK_RU_PRICE;

NTimer::STime SConsts::RESIDUAL_VISIBILITY_TIME = 700;
NTimer::STime SConsts::MED_TRUCK_HEAL_RADIUS = 1000;
float SConsts::MED_TRUCK_HEAL_PER_UPDATEDURATION = 0.5f;

NTimer::STime SConsts::PERIOD_OF_PATH_TO_FORMATION_SEARCH = 3000;
NTimer::STime SConsts::ENTRENCH_SELF_TIME = 1000;

int SConsts::N_SCANNING_UNITS_IN_SEGMENT = 30;
int SConsts::GENERAL_CELL_SIZE = 0;

float SConsts::FLAG_RADIUS = 320.0f;
float SConsts::FLAG_POINTS_SPEED = 10.0f;
// "�������� ����", ������� ������ ����, ���������� �� ����������� ������
float SConsts::PLAYER_POINTS_SPEED = 10.0f;

float SConsts::FLAG_POINTS_TO_REINFORCEMENT = 30.0f;
float SConsts::FLAG_TIME_TO_CAPTURE = 5000.0f;

NTimer::STime SConsts::TIME_OF_PRE_DISAPPEAR_NOTIFY = 100;
WORD SConsts::ANGLE_DIVEBOMBER_MIN_DIVE = 65535/8;
// ������������ �����, ����������� area damage
int SConsts::ARMOR_FOR_AREA_DAMAGE = 10;
float SConsts::BUILDING_FIREPLACE_DEFAULT_COVER = 0.5f;

NTimer::STime SConsts::DIVE_BEFORE_EXPLODE_TIME = 5000;
NTimer::STime SConsts::DIVE_AFTER_EXPLODE_TIME = 150;

NTimer::STime SConsts::WEATHER_TIME = 10000;
NTimer::STime SConsts::WEATHER_TIME_RANDOM = 20000;
NTimer::STime SConsts::WEATHER_TURN_PERIOD = 30000;
NTimer::STime SConsts::WEATHER_TURN_PERIOD_RANDOM = 30000;

float SConsts::BAD_WEATHER_FIRE_RANGE_COEFFICIENT = 0.5f;
int SConsts::TIME_TO_WEATHER_FADE_OFF = 5;
int SConsts::AA_AIM_ITERATIONS = 3;

float SConsts::COEFF_TO_LOW_MORALE_WITHOUT_OFFICER = 2.0f;

float SConsts::MAX_DISTANCE_TO_THROW_GRENADE = 320.0f;

float SConsts::MORALE_DISPERSION_COEFF = 1.0f;
float SConsts::MORALE_RELAX_COEFF = 1.0f;
float SConsts::MORALE_AIMING_COEFF = 1.0f;

float SConsts::TR_DISTANCE_TO_CENTER_FACTOR = 100;
float SConsts::TR_GUNPLANE_ALPHA_ATTACK_1 = 1.0f;
float SConsts::TR_GUNPLANE_ALPHA_ATTACK_2 = 0.3f;
float SConsts::TR_GUNPLANE_ALPHA_GO = 0.005f;
float SConsts::TR_GUNPLANE_ALPHA_KILL = 1.0f;
float SConsts::TR_GUNPLANE_ALPHA_PRICE = 1.0f;
float SConsts::TR_GUNPLANE_LIMIT_TIME = 1000.0f;

float SConsts::MAX_FIRE_RANGE_TO_SHOOT_BY_LINE = 2000.0f;

float SConsts::HP_PERCENT_TO_ESCAPE_FROM_BUILDING = 0.1f;

int SConsts::SHOW_ALL_TIME_COEFF = 5;

std::unordered_map<int, SConsts::SRevealInfo> SConsts::REVEAL_INFO;

float SConsts::REINFORCEMENT_GROUP_DISTANCE = 900.0f;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SConsts::Load()
{
	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );

	TIME_BEFORE_SNIPER_CAMOUFLAGE = constsTbl.GetInt( "AI", "Infantry.TimeBeforeSniperCamouflage", 1000 );
	TIME_OF_LYING_UNDER_FIRE = constsTbl.GetInt( "AI", "Infantry.TimeOfLyingUnderFire", 20000 );
	LYING_SOLDIER_COVER	= constsTbl.GetFloat( "AI", "Infantry.LyingSoldierCover", 0.7f );
	RADIUS_OF_FORMATION	= constsTbl.GetInt( "AI", "Infantry.RadiusOfFormation", 10 * TILE_SIZE );
	LYING_SPEED_FACTOR = constsTbl.GetFloat( "AI", "Infantry.LyingSpeedFactor", 0.5f );
	SPY_GLASS_RADIUS = constsTbl.GetFloat( "AI", "Infantry.SpyGlassRadius", 1920.f );

	const float fSpyGlassAngle = constsTbl.GetFloat( "AI", "Infantry.SpyGlassAngle",  float( SPY_GLASS_ANGLE ) / 65536 * 365 );
	const int nAngle = fSpyGlassAngle / 365 * 65536 / 2;
	SPY_GLASS_ANGLE = ( nAngle > 32768 ) ? 32768 : nAngle;

	MINE_VIS_RADIUS	= constsTbl.GetInt( "AI", "Engineers.MineVisRadius", 3 * TILE_SIZE );
	MINE_CLEAR_RADIUS = constsTbl.GetInt( "AI", "Engineers.MineClearRadius", 7 * TILE_SIZE );
	ENGINEER_LOAD_RU_PER_QUANT = constsTbl.GetFloat( "AI", "Engineers.EngineerLoadRuPerQuant", 1.1f );
	ENGINEER_REPEAR_HP_PER_QUANT = constsTbl.GetFloat( "AI", "Engineers.EngineerRepearPerQuant", 1.1f );
	ENGINEER_FENCE_LENGHT_PER_QUANT = constsTbl.GetFloat( "AI", "Engineers.EngineerFenceLenghtPerQuant", 1.1f );
	ENGINEER_ENTRENCH_LENGHT_PER_QUANT = constsTbl.GetFloat( "AI", "Engineers.EngineerEntrenchLenghtPerQuant", 1.1f );
	ENGINEER_RESUPPLY_PER_QUANT = constsTbl.GetFloat( "AI", "Engineers.EngineerResupplyPerQuant", 1.1f );
	ENGINEER_MINE_CHECK_PERIOD = constsTbl.GetULong( "AI", "Engineers.EngineerMineCheckPeriod", 1000 );
	TIME_QUANT = constsTbl.GetInt( "AI", "Engineers.TimeQuant", 160 );
	ENGINEER_RU_CARRY_WEIGHT = constsTbl.GetFloat( "AI", "Engineers.EngineerRuCarryWeight", 500 );

	FIGHTER_PATROL_TIME = 1000 * constsTbl.GetULong( "AI", "Aviation.FighterPatrolTime", 180 );
	FIGHTER_PATH_UPDATE_TIME = constsTbl.GetULong( "AI", "Aviation.FighterPathUpdateTime", 6000 );
	SHTURMOVIK_PATH_UPDATE_TIME = constsTbl.GetULong( "AI", "Aviation.ShturmovikPathUpdateTime", 6000 );
	PLANE_TILT_PER_SECOND = constsTbl.GetFloat( "AI", "Aviation.PlaneTiltPerSecond", 100.0f );
	PLANE_GUARD_STATE_RADIUS = constsTbl.GetFloat( "AI", "Aviation.PlaneGuardStateRadius", 1000.0f );
	// ������� ��������� ��� ������ �����
	SHTURMOVIK_APPROACH_RADIUS = constsTbl.GetFloat( "AI", "Aviation.ShturmovikApproachRadius", 1000.0f );
	SHTURMOVIK_APPROACH_RADIUS_SQR = SHTURMOVIK_APPROACH_RADIUS*SHTURMOVIK_APPROACH_RADIUS;
	// ����������� ������ ������ ���������
	PLANE_MIN_HEIGHT = constsTbl.GetFloat( "AI", "Aviation.PlaneMinHeight", 300.0f );

	CURE_SPEED_IN_BUILDING = constsTbl.GetFloat( "AI", "Buildings.CureSpeedInBuilding", 0.001f );
	TIME_OF_BUILDING_ALARM = constsTbl.GetInt( "AI", "Buildings.TimeOfBuildingAlarm", 8000 );
	CAMPING_TIME = constsTbl.GetULong( "AI", "Buildings.CampingTime", 2000 );
	INSIDE_OBJ_WEAPON_FACTOR = constsTbl.GetFloat( "AI", "Buildings.InsideObjWeaponFactor", 0.5f );
	INSIDE_OBJ_COMBAT_PERIOD = constsTbl.GetULong( "AI", "Buildings.InsideObjCombatPeriod", 800 );

	THRESHOLD_INSTALL_TIME = constsTbl.GetULong( "AI", "Artillery.ThresholdInstallTime", 2000 );
	SHOOTS_TO_RANGE = constsTbl.GetInt( "AI", "Artillery.ShootsToRange", 4 );
	RANDGED_DISPERSION_RADIUS_BONUS = constsTbl.GetFloat( "AI", "Artillery.RangedDispersionRadiusBonus", 0.5f );
	RANGED_AREA_RADIUS = constsTbl.GetFloat( "AI", "Artillery.RangedAreaRadius", 5 * TILE_SIZE );
	MAX_ANTI_ARTILLERY_RADIUS = constsTbl.GetFloat( "AI", "Artillery.MaxAntiArtilleryRadius", 10 * TILE_SIZE );
	MIN_ANTI_ARTILLERY_RADIUS = constsTbl.GetFloat( "AI", "Artillery.MinAntiArtilleryRadius", TILE_SIZE );
	SHOTS_TO_MINIMIZE_LOCATION_RADIUS = constsTbl.GetInt( "AI", "Artillery.ShotsToMinimizeLocationRadius", 5 );
	REVEAL_CIRCLE_PERIOD = constsTbl.GetULong( "AI", "Artillery.RevealCirclePeriod", 2000 );
	RADIUS_TO_START_ANTIARTILLERY_FIRE = constsTbl.GetFloat( "AI", "Artillery.RadiusToStartAntiartilleryFire", 320.0f );
	RELOCATION_RADIUS = constsTbl.GetFloat( "AI", "Artillery.RelocationRadius", 5 * TILE_SIZE );
	AUDIBILITY_TIME	= constsTbl.GetULong( "AI", "Artillery.AudibilityTime", 20000 );

	PARATROOPER_FALL_SPEED = constsTbl.GetFloat( "AI", "Paratroopers.ParatrooperFallSpeed", 0.05f );
	PARADROP_SPRED = constsTbl.GetInt( "AI", "Paratroopers.ParadropSpred", 4 );
	PLANE_PARADROP_INTERVAL = constsTbl.GetFloat( "AI", "Paratroopers.PlaneParadropInterval", 20.0f );
	PLANE_PARADROP_INTERVAL_PERP_MIN = constsTbl.GetFloat( "AI", "Paratroopers.PlaneParadropIntervalPerpMin", 20.0f );
	PLANE_PARADROP_INTERVAL_PERP_MAX = constsTbl.GetFloat( "AI", "Paratroopers.PlaneParadropIntervalPerpMax", 50.0f );

	if ( PLANE_PARADROP_INTERVAL_PERP_MAX <= PLANE_PARADROP_INTERVAL_PERP_MIN ) // to avoid zero division during random form min to max
		PLANE_PARADROP_INTERVAL_PERP_MAX += 2.0f;

	PARATROOPER_GROUND_SCAN_PERIOD = constsTbl.GetInt( "AI", "Paratroopers.ParatrooperGroundScanPeriod", 200 );
	DIVEBOMBER_VERT_MANEUR_RATIO = constsTbl.GetFloat( "AI", "Aviation.DivebomberVertManeurRatio", 1.0f );
	GUNPLANES_VERT_MANEUR_RATIO = constsTbl.GetFloat( "AI", "Aviation.GroundAttack.VertManeurRatio", 1.0f );

	MORALE_ADDITION_PER_TICK = constsTbl.GetFloat( "AI", "Morale.MoraleAdditionPerTick", 0.00005f );
	MORALE_DECREASE_PER_TICK = constsTbl.GetFloat( "AI", "Morale.MoraleDecreasePerTick", 0.00000001f );
	PROBABILITY_TO_DECREASE_MORALE = constsTbl.GetFloat( "AI", "Morale.ProbabilityToDecreaseMorale", 0.1f );
	MORALE_MIN_VALUE = constsTbl.GetFloat( "AI", "Morale.MinValue", 0.1f );
	
	RESUPPLY_RADIUS = constsTbl.GetInt( "AI", "TransportAndResupply.ResupplyRadius", 1000 );
	RESUPPLY_RADIUS_MORALE = constsTbl.GetInt( "AI", "Morale.ResupplyRadius", 300 );
	
	TRANSPORT_RU_CAPACITY = constsTbl.GetFloat( "AI", "TransportAndResupply.TransportRuCapacity", 500.0f );
	TRANSPORT_LOAD_RU_DISTANCE = constsTbl.GetFloat( "AI", "TransportAndResupply.TransportLoadRuDistance", 200.0f );
	GOOD_LAND_DIST = constsTbl.GetFloat( "AI", "TransportAndResupply.LandDistance", 50.0f );

	NUM_TO_SCAN_IN_SEGM				= constsTbl.GetInt("AI", "Common.NumToScanInSegm", 50 );
	BEH_UPDATE_DURATION				= constsTbl.GetULong( "AI", "Common.BehUpdateDuration", 2000 );
	SOLDIER_BEH_UPDATE_DURATION = constsTbl.GetULong( "AI", "Common.SoldierBehUpdateDuration", 3000 );
	AA_BEH_UPDATE_DURATION		= constsTbl.GetULong( "AI", "Common.AABehUpdateDuration", 200 );
	DEAD_SEE_TIME							= constsTbl.GetULong( "AI", "Common.DeadSeeTime", 2000 );
	TIME_TO_RETURN_GUN				= constsTbl.GetInt( "AI", "Common.TimeToReturnGun", 2000 );
	TIME_BEFORE_CAMOUFLAGE		= constsTbl.GetInt( "AI", "Common.TimeBeforeCamouflage", 2000 );
	RADIUS_OF_HIT_NOTIFY			= constsTbl.GetInt( "AI", "Common.RadiusOfHitNotify", 5 * TILE_SIZE );
	TIME_OF_HIT_NOTIFY				= constsTbl.GetInt( "AI", "Common.TimeOfHitNotify", 1000 );
	CALL_FOR_HELP_RADIUS			= constsTbl.GetFloat( "AI", "Common.CallForHelpRadius", 20 * TILE_SIZE );
	AI_CALL_FOR_HELP_RADIUS		= constsTbl.GetFloat( "AI", "Common.AICallForHelpRadius", 20 * TILE_SIZE );
	TIME_TO_DISAPPEAR					= constsTbl.GetULong( "AI", "Common.TimeToDisappear", 5000 );
	GUARD_STATE_RADIUS				= constsTbl.GetFloat( "AI", "Common.GuardStateRadius", 10 * TILE_SIZE );
	GOOD_ATTACK_RPOBABILITY		= constsTbl.GetFloat( "AI", "Common.GoodAttackProbability", 0.6f );
	AREA_DAMAGE_COEFF					= constsTbl.GetFloat( "AI", "Common.AreaDamageCoeff", 0.2f );
	const float fMinRotateAngle				  = constsTbl.GetFloat( "AI", "Common.MinRotateAngle",  float( MIN_ROTATE_ANGLE ) / 65536 * 365 );
	const int nMinRotateAngle = fMinRotateAngle / 365 * 65536;
	MIN_ROTATE_ANGLE = ( nMinRotateAngle > 65535 ) ? 65535 : nMinRotateAngle;
	TANK_TRACK_HIT_POINTS = constsTbl.GetFloat( "AI", "Common.TankTrackHitPoints", 0.1f );
	TRAJECTORY_LOW_LINE_RATIO = constsTbl.GetFloat( "AI", "Common.TrajectoryLineRatio", 0.7f );
	TRAJECTORY_BOMB_G = constsTbl.GetFloat( "AI", "Common.TrajectoryBombG", 0.7f );
	
	PLANES_HEAVY_FORMATION_SIZE = constsTbl.GetFloat( "AI", "Aviation.HeavyFormationDistance", 2.0f );
	PLANES_SMALL_FORMATION_SIZE = constsTbl.GetFloat( "AI", "Aviation.LightFormationDistance", 1.5f );
	PLANES_START_RANDOM = constsTbl.GetFloat( "AI", "Aviation.StartRandom", 3.1f );

	SNIPER_CAMOUFLAGE_DECREASE_PER_SHOOT	= constsTbl.GetFloat( "AI", "Infantry.SniperCamouflageDecreasePerShoot", 0.1f );
	SNIPER_CAMOUFLAGE_INCREASE						= constsTbl.GetFloat( "AI", "Infantry.SniperCamouflageIncrease", 0.014f );
	AMBUSH_ATTACK_BEGIN_CIRTERIA					= constsTbl.GetFloat( "AI", "Common.AmbushBeginAttackCriteria", 0.5f );
	ARTILLERY_REVEAL_COEEFICIENT					= constsTbl.GetFloat( "AI", "Artillery.ArtilleryRevealCoefficient", 200.0f );
	
	dispersionRatio[0][0] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.LineMin", 1.0f );
	dispersionRatio[0][1] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.LineMax", 1.0f );

	dispersionRatio[1][0] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.HowitserMin", 1.0f );
	dispersionRatio[1][1] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.HowitserMax", 2.0f );

	dispersionRatio[2][0] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.BombMin", 1.0f );
	dispersionRatio[2][1] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.BombMax", 3.0f );

	dispersionRatio[3][0] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.CannonMin", 1.0f );
	dispersionRatio[3][1] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.CannonMax", 4.0f );

	dispersionRatio[4][0] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.RocketMin", 1.0f );
	dispersionRatio[4][1] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.RocketMax", 4.0f );

	dispersionRatio[5][0] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.GrenadeMin", 1.0f );
	dispersionRatio[5][1] = constsTbl.GetFloat( "AI", "Artillery.DispersionRatio.GrenadeMax", 1.0f );
	
	COEFF_FOR_RANDOM_DELAY		= constsTbl.GetFloat( "AI", "Common.CoeffForRandomDelay", 1.2f );
	HEIGHT_FOR_VIS_RADIUS_INC = constsTbl.GetFloat( "AI", "Common.HeightForVisRadiusInc", 10.0f );
	BURNING_SPEED							= constsTbl.GetFloat( "AI", "Buildings.BurningSpeed", 0.002f );

	FOLLOW_STOP_RADIUS						= constsTbl.GetFloat( "AI", "Follow.StopRadius", 32.0f * 9 );
	FOLLOW_EQUALIZE_SPEED_RADIUS	= constsTbl.GetFloat( "AI", "Follow.EqualizeSpeedRadius", 32.0f * 12 );
	FOLLOW_GO_RADIUS							= constsTbl.GetFloat( "AI", "Follow.GoRadius", 32.0f * 11 );
	FOLLOW_WAIT_RADIUS						= constsTbl.GetFloat( "AI", "Follow.WaitRadius", 32.0f * 24 );

	FATALITY_PROBABILITY								= constsTbl.GetFloat( "AI", "Common.FatalityProbability", 0.1f );
	DAMAGE_FOR_MASSIVE_DAMAGE_FATALITY	= constsTbl.GetFloat( "AI", "Common.DamageForMassiveDamageFatality", 0.7f );
	MASSIVE_DAMAGE_FATALITY_PROBABILITY = constsTbl.GetFloat( "AI", "Common.MassiveDamageFatalityProbability", 0.8f );

	BOMB_START_HEIGHT = constsTbl.GetFloat( "AI", "Aviation.PlanesBombHeight", 0.5f ); 
	
	STAND_LIE_RANDOM_DELAY			= constsTbl.GetFloat( "AI", "Infantry.StandLieRandomDelay", 500.0f );
	MIN_MECH_TO_DROP_BOMBS			= constsTbl.GetInt( "AI", "Aviation.GroundAttack.MechNuberToDropBombs", 1 );
	MIN_INFANTRY_TO_DROP_BOMBS	= constsTbl.GetInt( "AI", "Aviation.GroundAttack.InfantryNuberToDropBombs", 20 );

	TRANSPORT_RESUPPLY_OFFSET		= constsTbl.GetFloat( "AI", "TransportAndResupply.ResupplyOffset", 100.0f );
	HP_BALANCE_COEFF						= constsTbl.GetFloat( "AI", "TransportAndResupply.ResupplyBalanceCoeff", 500.0f );
	SQUAD_MEMBER_LEAVE_INTERVAL = constsTbl.GetInt( "AI", "Infantry.SquadMemberLeaveInterval", 200 );
	SOLDIER_RU_PRICE						= constsTbl.GetFloat( "AI", "TransportAndResupply.SoldierRUPrice", 50.0f );
	LOW_HP_PERCENTAGE						= constsTbl.GetFloat( "AI", "Common.HpPercentageToWaitMedic", 0.5f );
	
	DIRECT_HIT_DAMAGE_COMBAT_SITUATION = constsTbl.GetFloat( "AI", "CombatSituation.Damage", 100.0f );
	DIRECT_HIT_TIME_COMBAT_SITUATION = constsTbl.GetULong( "AI", "CombatSituation.TimeDamage", 1000 );
	NUMBER_ENEMY_MECH_MOVING_TO_COMBAT_SITUATION = constsTbl.GetInt( "AI", "CombatSituation.MovingEnemyMechNumber", 3 );
	NUMBER_ENEMY_INFANTRY_MOVING_TO_COMBAT_SITUATION = constsTbl.GetInt( "AI", "CombatSituation.MovingEnemyInfantryNumber", 20 );
	MAIN_STORAGE_HEALING_SPEED = constsTbl.GetFloat( "AI", "TransportAndResupply.MainStorageHealingSpeed", 10.0f );

	RESUPPLY_MAX_PATH = constsTbl.GetInt( "AI", "TransportAndResupply.ResupplyMaxPathLenght", 60 );
	RADIUS_TO_TAKE_STORAGE_OWNERSHIP = constsTbl.GetFloat( "AI", "TransportAndResupply.TakeStorageOwnershipRadius", 100.0f);
	
	TANKPIT_COVER  = constsTbl.GetFloat( "AI", "Common.TankPitCover", 0.5f);
	FENCE_SEGMENT_RU_PRICE = constsTbl.GetFloat( "AI", "Common.FenceSegmentRuPrice", 100.0f);
	ENTRENCHMENT_SEGMENT_RU_PRICE = constsTbl.GetFloat( "AI", "Common.TrenchSegmentRuPrice", 100.0f);
	MINE_RU_PRICE[0] = constsTbl.GetFloat( "AI", "Engineers.MineAPersRuPrice", 10000 );
	MINE_RU_PRICE[1] = constsTbl.GetFloat( "AI", "Engineers.MineATankRuPrice", 10000 );
	ANTITANK_RU_PRICE = constsTbl.GetFloat( "AI", "Engineers.AntitankRuPrice", 10000 );
	
	MED_TRUCK_HEAL_RADIUS = constsTbl.GetFloat( "AI", "TransportAndResupply.MedicalTruckHealRadius", 1000 );
	MED_TRUCK_HEAL_PER_UPDATEDURATION = constsTbl.GetFloat( "AI", "TransportAndResupply.MedicalTruckHealPerUpdateDuration", 1 );

	ENTRENCH_SELF_TIME = constsTbl.GetInt( "AI", "Common.UnitEntrenchTime", 1000 );

	FLAG_RADIUS = constsTbl.GetFloat( "AI", "Flags.Radius", 320.0f );
	FLAG_POINTS_SPEED = constsTbl.GetFloat( "AI", "Flags.PointsSpeed", 10.0f );
	PLAYER_POINTS_SPEED = constsTbl.GetFloat( "AI", "Flags.PlayerPointsSpeed", 10.0f );

	FLAG_POINTS_TO_REINFORCEMENT = constsTbl.GetFloat( "AI", "Flags.PointsToReinforcement", 30.0f );
	FLAG_TIME_TO_CAPTURE = constsTbl.GetFloat( "AI", "Flags.TimeToCapture", 5000.0f );

	TIME_OF_PRE_DISAPPEAR_NOTIFY = constsTbl.GetInt( "AI", "Common.TimeOfPreDisappearNotify", 500 );
	ANGLE_DIVEBOMBER_MIN_DIVE = 65535 * constsTbl.GetFloat( "AI", "Aviation.MinDiveAngleForDiveBombers", 45 ) / 360;

	ARMOR_FOR_AREA_DAMAGE = constsTbl.GetFloat( "AI", "Common.ArmorForAreaDamage", 10 );

	BUILDING_FIREPLACE_DEFAULT_COVER = constsTbl.GetFloat( "AI", "Buildings.DefaultFireplaceCoverage", 0.5f );

	SConsts::GENERAL_CELL_SIZE = Min( SConsts::RESUPPLY_RADIUS, SConsts::RESUPPLY_RADIUS_MORALE ) / 2.5f;

	DIVE_BEFORE_EXPLODE_TIME = constsTbl.GetInt( "AI", "Aviation.DiveBeforeExplodeTime", 5000 );
	DIVE_AFTER_EXPLODE_TIME = constsTbl.GetInt( "AI", "Aviation.DiveAfterExplodeTime", 150 );

	WEATHER_TIME = constsTbl.GetInt( "AI", "Weather.Time", 20000 );
	WEATHER_TIME_RANDOM = constsTbl.GetInt( "AI", "Weather.TimeRandom", 20000 );
	
	WEATHER_TURN_PERIOD = constsTbl.GetInt( "AI", "Weather.Period", 20000 );
	WEATHER_TURN_PERIOD_RANDOM = constsTbl.GetInt( "AI", "Weather.PeriodRandom", 20000 );

	BAD_WEATHER_FIRE_RANGE_COEFFICIENT = constsTbl.GetFloat( "AI", "Weather.FireRangeCoefficient", 0.5f );
	TIME_TO_WEATHER_FADE_OFF = constsTbl.GetInt( "AI", "Weather.TimeToFadeOff", 0.5f );
	SetGlobalVar( "Weather.TimeToFadeOff", TIME_TO_WEATHER_FADE_OFF );

	AA_AIM_ITERATIONS = constsTbl.GetInt( "AI", "AntiAviationArtillery.AimIterations", 3 );

	
	COEFF_TO_LOW_MORALE_WITHOUT_OFFICER = constsTbl.GetFloat( "AI", "Morale.CoeffToLowMoraleWithoutOfficer", 2.0f );

	MAX_DISTANCE_TO_THROW_GRENADE = constsTbl.GetFloat( "AI", "Infantry.MaxDistanceToThrowGrenade", 320.0f );

	MORALE_DISPERSION_COEFF = constsTbl.GetFloat( "AI", "Morale.DispersionCoeff", 1.0f );
	MORALE_RELAX_COEFF = constsTbl.GetFloat( "AI", "Morale.RelaxCoeff", 1.0f );
	MORALE_AIMING_COEFF = constsTbl.GetFloat( "AI", "Morale.AimingCoeff", 1.0f );
	
	TR_DISTANCE_TO_CENTER_FACTOR = constsTbl.GetFloat( "AI", "TargetResolution.Gunplane.DistanceToCenter", 100.0f );
	TR_GUNPLANE_ALPHA_ATTACK_1	= constsTbl.GetFloat( "AI", "TargetResolution.Gunplane.AlphaAttack1", 1.0f );
	TR_GUNPLANE_ALPHA_ATTACK_2	= constsTbl.GetFloat( "AI", "TargetResolution.Gunplane.AlphaAttack2", 3.0f );
	TR_GUNPLANE_ALPHA_GO				= constsTbl.GetFloat( "AI", "TargetResolution.Gunplane.AlphaGo", 0.005f );
	TR_GUNPLANE_ALPHA_KILL			= constsTbl.GetFloat( "AI", "TargetResolution.Gunplane.AlphaKill", 1.0f );
	TR_GUNPLANE_ALPHA_PRICE			= constsTbl.GetFloat( "AI", "TargetResolution.Gunplane.AlphaPrice", 1.0f );
	TR_GUNPLANE_LIMIT_TIME			= constsTbl.GetFloat( "AI", "TargetResolution.Gunplane.LimitTime", 1000.0f );

	MAX_FIRE_RANGE_TO_SHOOT_BY_LINE = constsTbl.GetFloat( "AI", "Common.MaxFireRangeToShootByLine", 2000.0f );

	HP_PERCENT_TO_ESCAPE_FROM_BUILDING = constsTbl.GetFloat( "AI", "Buildings.HPPercentToEscapeFromBuilding", 0.1f );

	LoadRevealInfo( constsTbl );
	
	//
	if ( AI_CALL_FOR_HELP_RADIUS > 2000 )
	{
		AI_CALL_FOR_HELP_RADIUS = 2000;
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("AICallForHelpRadius is too big, reduced to %d", AI_CALL_FOR_HELP_RADIUS ), 0xffff0000, true );
	}
	if ( CALL_FOR_HELP_RADIUS > 2000 )
	{
		CALL_FOR_HELP_RADIUS = 2000;
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("CallForHelpRadius is too big, reduced to %d", CALL_FOR_HELP_RADIUS ), 0xffff0000, true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SConsts::LoadRevealInfo( CTableAccessor &constsTbl )
{
	CPtr<IRPGStatsAutomagic> pAutoMagic = CreateObject<IRPGStatsAutomagic>( MAIN_AUTOMAGIC );

	std::string szStatsIter = pAutoMagic->GetFirstStr();
	do
	{
		const int nStats = pAutoMagic->ToInt( szStatsIter.c_str() );

		std::string szName = "RevealInfo." + szStatsIter + ".Query";
		REVEAL_INFO[nStats].fRevealByQuery = constsTbl.GetFloat( "AI", szName.c_str(), 0.0f );

		szName = "RevealInfo." + szStatsIter + ".MovingOff";
		REVEAL_INFO[nStats].fRevealByMovingOff = constsTbl.GetFloat( "AI", szName.c_str(), 0.0f );

		szName = "RevealInfo." + szStatsIter + ".Distance";
		REVEAL_INFO[nStats].fForgetRevealDistance = constsTbl.GetFloat( "AI", szName.c_str(), 0.0f );

		szName = "RevealInfo." + szStatsIter + ".Time";
		REVEAL_INFO[nStats].nTimeOfReveal = constsTbl.GetInt( "AI", szName.c_str(), 0 );

		szStatsIter = pAutoMagic->GetNextStr( szStatsIter.c_str() );
	}
	while ( !pAutoMagic->IsLastStr( szStatsIter.c_str() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
