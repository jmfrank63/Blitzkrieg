#ifndef __AI_INTERNAL_CONSTS_H__
#define __AI_INTERNAL_CONSTS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "AIConsts.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SConsts : public SAIConsts
{
private:
	static void LoadRevealInfo( class CTableAccessor &constsTbl );
public:
	// ����������, � ������� ����������� ����� ��� ������ ��� �������� ����. �������
	static const int MAX_DIST_TO_RECALC_FOG;
	
	// �� ����� ������������ ���� �������������� ���������
	static const int TURN_TOLERANCE;
	
	// ����������� ���� ������
	static const WORD STANDART_VIS_ANGLE;
	
	// ��� ����� ������� � ���� ����� ������������ � ��������������
	static const WORD DIR_DIFF_TO_SMOOTH_TURNING;

	// ������������ ����� ���� � ������, ��� ������� ����� ����� �����
	static const int MAX_LEN_TO_GO_BACKWARD;
	
	// ��������� forward iteration �������� ����� ��� �������� ����� �������
	static const int NUMBER_ITERS_TO_LOOK_AHEAD;
	
	// ��� �������������� ������� ����������� �����
	static const int SPEED_FACTOR;

	// ������� �������� ������ ���� ��� ���������� �������
	static const short int SPLINE_STEP;

	// size of "neighbours scan" cell
	static const int CELL_COEFF;
	static const int CELL_SIZE;			// must be divisible by TILE_SIZE
	
	static const int BIG_CELL_COEFF;
	static const int BIG_CELL_SIZE;
	
	// max number of tiles, occupied by a unit
	static const int MAX_UNIT_TILE_RADIUS;
	// ������������ ������ ����� � ������
	static const int MAX_UNIT_RADIUS;
	
	static const int BIG_PATH_SHIFT;

	// starting sizes of vectors
	static const int AI_START_VECTOR_SIZE;
	
	//
	// ������������ ��������� ��� ����, ����� ����� ���� � ����� "�������" ������
	static const int GROUP_DISTANCE;

	// ������� ������������ ��� ������ ���������� ����� ��� antiartillery ������
	static const float ANTI_ARTILLERY_SCAN_TIME;

	// ����������� ��� ���������/��������� boundRect
	static const float BOUND_RECT_FACTOR;
	// ����������� ��� ���������/��������� boundRect ��� ������� ������
	static const float COEFF_FOR_LOCK;
	// ���������� �� ����� ������� ���������� �� �����, ���� ������ ����� ����� �� ������ ( ��� ������ ���� )
	static const float DIST_FOR_LAND;
	// ���������� �� ����� ������� ���������� �� �����, ���� ������ ����� ����� �� ������ ( ��� �������� "�����������" ����� )
	static float GOOD_LAND_DIST;
	
	//
	// ������� ����� �� ������ ��� ����������� ��������
	static const int STATIC_OBJ_CELL;
	static const int STATIC_CONTAINER_OBJ_CELL;
	
	// ������ ������ ��� ������ ������������� ��������
	static const int HIT_CELL_COEFF;
	static const int HIT_CELL_SIZE;

	// ���������� ����������� � ����������
	static const int NUMBER_SOLDIER_DIRS;

	// ���������, ������������ �� �������� �����
	//
	// �������� ������� � ������� HP / tick
	static float CURE_SPEED_IN_BUILDING;
	// ����� ����� ������ ������� � ������� ����������� turret � default position
	static int TIME_TO_RETURN_GUN;
	// ���������� ����������� �� �������
	static int NUM_TO_SCAN_IN_SEGM;
	// ����� ����� updates ���������
	static NTimer::STime BEH_UPDATE_DURATION;
	// ����� ����� updates ��������� ��� ������
	static NTimer::STime SOLDIER_BEH_UPDATE_DURATION;
	// �� �� ��� AA 
	static NTimer::STime AA_BEH_UPDATE_DURATION;
	// ��� ������������ ����������
	static NTimer::STime LONG_RANGE_ARTILLERY_UPDATE_DURATION;
	// �����, ������� �������� �����
	static NTimer::STime DEAD_SEE_TIME;
	// �����, � ������� �������� ������� � ������
	static int TIME_OF_BUILDING_ALARM;
	// �����, ����� ������� ����� ��������������� � ��������� idle
	static int TIME_BEFORE_CAMOUFLAGE;
	// �����, ����� ������� �������� ����� ��������������� � ��������� idle
	static int TIME_BEFORE_SNIPER_CAMOUFLAGE;
	// ����� ������� ��� ���������
	static int TIME_OF_LYING_UNDER_FIRE;
	// cover ��� ������� - �����������, ��� �������
	static float LYING_SOLDIER_COVER;
	// ������ � ������ , ������� ������������, ����� ������, ��� ��������� ��� ���
	static int RADIUS_OF_HIT_NOTIFY;
	// ������������� ��������, ��� ��������� ��� ���
	static int TIME_OF_HIT_NOTIFY;
	
	// ����� � ������������ ����� 2 �������������� ��� ��� ���������
	static NTimer::STime ENGINEER_MINE_CHECK_PERIOD; 
	// ������ ( � ������ ), � ������� ������� ����� ����
	static int MINE_VIS_RADIUS;
	// ������ ( � ������ ), � ������� ������� ������� ����
	static int MINE_CLEAR_RADIUS;
	// ������������ ���������� �������� �� ������ ��������
	static int RADIUS_OF_FORMATION;
	
	// ������, � ������� ����� ������ ��� guard state
	static float GUARD_STATE_RADIUS;
	
	// ��������� �� �������� ��� ��������
	static float LYING_SPEED_FACTOR;
	
	// ������ ��� call for help
	static int CALL_FOR_HELP_RADIUS;
	static int AI_CALL_FOR_HELP_RADIUS;
	
	// �����, ������� ������� ���������� � ������ ����� �������� � ������
	static NTimer::STime CAMPING_TIME;
	// ��������� �� weapon range ��� �������� ���������� ������ �������
	static float INSIDE_OBJ_WEAPON_FACTOR;
	// ������ �������, � ������� ���������� ��� ������� ������ �������
	static NTimer::STime INSIDE_OBJ_COMBAT_PERIOD;
	// �����, ����� ������� ������� �������� ����� ������
	static NTimer::STime TIME_TO_DISAPPEAR;

	// ��������� ����� ������� install/uninstall ��� ����, ����� ����� install/uninstall ��������������
	static NTimer::STime THRESHOLD_INSTALL_TIME;
	
	// ���������� ��������� ��� ���������� ���������� �� �������
	static int SHOOTS_TO_RANGE;
	// ����������� �� dispersion ���� ������ �������� �� ������������� �������
	static float RANDGED_DISPERSION_RADIUS_BONUS;
	// ������ ������� ����������
	static float RANGED_AREA_RADIUS;

	// ����������, �� ������� ����� ���������� ���������� ��� ����, ����� �������� info � � ���������������
	static float RELOCATION_RADIUS;
	// ����. ������ �������� ������ ���������� ����������
	static float MAX_ANTI_ARTILLERY_RADIUS;
	// ���. ������ �������� ������ ���������� ����������
	static float MIN_ANTI_ARTILLERY_RADIUS;
	// ���������� ���������, ����� ������ MAX_ANTI_ARTILLERY_RADIUS � MIN_ANTI_ARTILLERY_RADIUS
	static int SHOTS_TO_MINIMIZE_LOCATION_RADIUS;
	// �����, ������� �������� ����� ����� �������� �����
	static NTimer::STime AUDIBILITY_TIME;
	// ������������� ��������� ������ ������������������ ������
	static NTimer::STime REVEAL_CIRCLE_PERIOD;
	
	// ����������� ���������� �����, ��� ������� �� ����� �������������
	static float GOOD_ATTACK_RPOBABILITY;
	
	//for fighter
	static NTimer::STime FIGHTER_PATROL_TIME;//after that time fighter will cancel patrolling 
	static NTimer::STime FIGHTER_PATH_UPDATE_TIME;//path is updated once per this time
	static NTimer::STime SHTURMOVIK_PATH_UPDATE_TIME;
	static float FIGHTER_VERTICAL_SPEED_UP;		// �������� ������ ������
	static float FIGHTER_VERTICAL_SPEED_DOWN;		// �������� ������ ������
	
	//for paratrooper
	static float PARATROOPER_FALL_SPEED;
	static int PARADROP_SPRED ; //��� ������ ���������� �����, �� ������� ����������.

	//������ � ������� �������� ���� �����, ������� ����� ���������� ��������� �� ����������
	static int RESUPPLY_RADIUS;
	// ��� ������
	static int RESUPPLY_RADIUS_MORALE;

	static NTimer::STime TIME_QUANT;//time of quant repear operation
	static float ENGINEER_LOAD_RU_PER_QUANT;					// ��� ���������� ����������
	static float ENGINEER_REPEAR_HP_PER_QUANT;				//additional health
	static float ENGINEER_FENCE_LENGHT_PER_QUANT;			//for building fence.
	static float ENGINEER_ENTRENCH_LENGHT_PER_QUANT;	// ��� ������ ��������
	static float ENGINEER_RESUPPLY_PER_QUANT;					//��� �����������
	static float ENGINEER_ANTITANK_HALTH_PER_QUANT;		// ��� ������������� ���
	static float ENGINEER_RU_CARRY_WEIGHT;						// ��� �������� ���������� �������� ����� ������� RU

	// ������ ������ �������
	static float SPY_GLASS_RADIUS;
	// ���� ������ �������
	static WORD SPY_GLASS_ANGLE;

	// ����������� �� area damage
	static float AREA_DAMAGE_COEFF;
	// ����������� ����, �� ������� ����� ��������� ���� �� ����� ����� turret-��, ����� �������� ���� �������
	static WORD MIN_ROTATE_ANGLE;
	
	// ������ ������ �� ���������, ������� � �������� ���������� �������� antiartillery ������
	static float RADIUS_TO_START_ANTIARTILLERY_FIRE;
	
	// ������� ��������� � RU
	static float TRANSPORT_RU_CAPACITY;
	// ��������� �� ������� ����� ��������� �������� � ������
	static float TRANSPORT_LOAD_RU_DISTANCE ;
	// ������������ ����� ���� (�� ������, � ������), �� ������� ���������� ���������
	static int RESUPPLY_MAX_PATH;
	
	// �����, ������� �������� alarm ��� �������� ����� ��������� �� �����������
	static float TIME_OF_ALARM_UNDER_FIRE;
	// ������ �������� ��� �������. �� ����� ���������� ������������ ������� ����� �������
	static float STORAGE_RESUPPLY_RADIUS;
	
	// ������������� ������� ��� ����
	static float TRAJ_BOMB_ALPHA;
	
	// ������� �� ��� ���������� � ������ ����� ������������ ������������ � ���.
	static float GUN_CREW_TELEPORT_RADIUS;

	//  ������� ���������� ����� ��������� ������������
	static float PLANE_PARADROP_INTERVAL;
	// ������������ �������� ����������� ��������������� ������� �������� ���������.
	static float PLANE_PARADROP_INTERVAL_PERP_MIN;
	static float PLANE_PARADROP_INTERVAL_PERP_MAX;

	// 1 ��� � ���� �������� ����������� ��������� �� ������ �� ��� �� ���������� ������
	static NTimer::STime PARATROOPER_GROUND_SCAN_PERIOD;

	//������� ������ ������ �� ������������ ������ � 1 BEH_UPDATE_DURATION
	static float MORALE_ADDITION_PER_TICK;
	// �������� ������ �� �������
	static float MORALE_DECREASE_PER_TICK;
	// ����������� �� ���������� ������
	static float PROBABILITY_TO_DECREASE_MORALE;
	static float MORALE_MIN_VALUE;
	
	// � ����� ���������� ���������� �� ������ ����� ���������� �����	
	static float TRANSPORT_MOVE_BACK_DISTANCE;

	// ������� ��� ��������� ����� �������� �������� ����������.
	static int TRIES_TO_UNHOOK_ARTILLERY;

	// ��� ����� ���������
	static float PLANE_TILT_PER_SECOND;
	
	static float PLANE_GUARD_STATE_RADIUS;
	static float PLANES_HEAVY_FORMATION_SIZE;
	static float PLANES_SMALL_FORMATION_SIZE;
	static float PLANES_START_RANDOM;
	
	// �������� ��������
	static float TANK_TRACK_HIT_POINTS;
	
	// ����� ���� ���������� ������ �� ������� ���������� ����� �����
	static float TRAJECTORY_LOW_LINE_RATIO;

	static float TRAJECTORY_BOMB_G;

	// ������� ��������� ��� ������ �����
	static float SHTURMOVIK_APPROACH_RADIUS_SQR;
	static float SHTURMOVIK_APPROACH_RADIUS; 

	// ����������� ������ ������ ���������
	static float PLANE_MIN_HEIGHT;

	// ������ ������ �� ����������� �� ����
	static float PLANE_DIVE_FINISH_DISTANCE_SQR	;
	static float DIVEBOMBER_VERT_MANEUR_RATIO;
	static float GUNPLANES_VERT_MANEUR_RATIO;

	
	static float SNIPER_CAMOUFLAGE_DECREASE_PER_SHOOT;
	static float SNIPER_CAMOUFLAGE_INCREASE;
	
	// ���� ������, ������� ������ ����� ����������� ��������
	// ���� �� ������ ��� ����, ����� ���� Ambush ����� ���������.
	static float AMBUSH_ATTACK_BEGIN_CIRTERIA;
	// ��� �������� ������� �� revealRadous � ���������� �� AntiArtilleryRadius
	static float ARTILLERY_REVEAL_COEEFICIENT;
	// ����������� � ������������ ������� ��� ���� ����� ����������
	static float dispersionRatio[6][2];
	
	static float COEFF_FOR_RANDOM_DELAY;
	
	// �� ����� ������ ����� ���������, ����� ������ ��������� ���������� �� ���� ����
	static float HEIGHT_FOR_VIS_RADIUS_INC;
	
	// �������� ������� ������ ( �������� � tick )
	static float BURNING_SPEED;
	
	// follow ���������
	// ������ ���������� ������ �� ��������
	static float FOLLOW_STOP_RADIUS;
	// ������ � ������� ����� ���������� �������� � �������� ��������
	static float FOLLOW_EQUALIZE_SPEED_RADIUS;
	// ������, ��-�� �������� ����� ����� �� �������
	static float FOLLOW_GO_RADIUS;
	// ������, �� ������� ������� ������ �����
	static float FOLLOW_WAIT_RADIUS;
	
	static float FATALITY_PROBABILITY;
	static float DAMAGE_FOR_MASSIVE_DAMAGE_FATALITY;
	static float MASSIVE_DAMAGE_FATALITY_PROBABILITY;

	static float BOMB_START_HEIGHT;
	
	static float STAND_LIE_RANDOM_DELAY;
	// ��� ������ ������� �� ���������� �����
	static int MIN_MECH_TO_DROP_BOMBS;
	static int MIN_INFANTRY_TO_DROP_BOMBS;
	
	static float TRANSPORT_RESUPPLY_OFFSET;
	static float HP_BALANCE_COEFF;
	
	// ��� ������ �� ���� ��������� � squad ����� �������� �� ������ ����� ���� ��������
	static NTimer::STime SQUAD_MEMBER_LEAVE_INTERVAL;

	// ��������� ������� � RU. ����� ��� �������������� 
	static float SOLDIER_RU_PRICE;
	static float LOW_HP_PERCENTAGE;
	
	// damage �� ������ ���������� � ������� �������, ����������� ��� ����, ����� �������� ���� ���������
	static float DIRECT_HIT_DAMAGE_COMBAT_SITUATION;
	static NTimer::STime DIRECT_HIT_TIME_COMBAT_SITUATION;
	static int NUMBER_ENEMY_MECH_MOVING_TO_COMBAT_SITUATION;
	static int NUMBER_ENEMY_INFANTRY_MOVING_TO_COMBAT_SITUATION;
	
	// ����������� �� firerange ������ �������, � ������� �� ��������� ( ������ ������������ firerange * OFFICER_COEFFICIENT_FOR_SCAN )
	static float OFFICER_COEFFICIENT_FOR_SCAN;
	
	static float MAIN_STORAGE_HEALING_SPEED;
	static float RADIUS_TO_TAKE_STORAGE_OWNERSHIP;
	static float TANKPIT_COVER;
	static const float CLOSEST_TO_RAILROAD_POINT_TOLERANCE;

	static float FENCE_SEGMENT_RU_PRICE;
	static float ENTRENCHMENT_SEGMENT_RU_PRICE;
	static float MINE_RU_PRICE[2];
	static float ANTITANK_RU_PRICE;
	
	// �����, ������� ���� ����� ����� ����, ��� �� ����� �� ������� ���������
	static NTimer::STime RESIDUAL_VISIBILITY_TIME;

	static NTimer::STime MED_TRUCK_HEAL_RADIUS;
	static float MED_TRUCK_HEAL_PER_UPDATEDURATION;
	
	//
	// �������������, � ������� ���� �������� ����� ���� �� ����� ��������, ���� �� ���-�� ����������
	static NTimer::STime PERIOD_OF_PATH_TO_FORMATION_SEARCH;
	
	static NTimer::STime ENTRENCH_SELF_TIME;
	
	// ������������ ���������� ������, ������� ����� ���� �������������� �� �������
	static int N_SCANNING_UNITS_IN_SEGMENT;

	// size of cell for general
	static int GENERAL_CELL_SIZE;
	
	// flags
	// ������ ���� ����� (� ������)
	static float FLAG_RADIUS;
	// �����, ����� ��������� ���� ( � �������� )
	static float FLAG_TIME_TO_CAPTURE;
	// ���� �� ���� � �������
	static float FLAG_POINTS_SPEED;
	// "�������� ����", ������� ������ ����, ���������� �� ����������� ������
	static float PLAYER_POINTS_SPEED;
	// ���������� �����, ����� ������ ������������
	static float FLAG_POINTS_TO_REINFORCEMENT;
	
	// �����, �� ������� ����� ���. ����� ��������� �������������� �� ������������
	static NTimer::STime TIME_OF_PRE_DISAPPEAR_NOTIFY;
	// ���� � ������� ���� ����������� ������ �����, �� �� - dive bomber
	static WORD ANGLE_DIVEBOMBER_MIN_DIVE;
	//
	// ������������ �����, ����������� area damage
	static int ARMOR_FOR_AREA_DAMAGE;
	
	// default cover for fireplace
	static float BUILDING_FIREPLACE_DEFAULT_COVER;

	static NTimer::STime DIVE_BEFORE_EXPLODE_TIME;
	static NTimer::STime DIVE_AFTER_EXPLODE_TIME;
	

	static NTimer::STime WEATHER_TIME;
	static NTimer::STime WEATHER_TIME_RANDOM;
	static NTimer::STime WEATHER_TURN_PERIOD;
	static NTimer::STime WEATHER_TURN_PERIOD_RANDOM;
	
	static float BAD_WEATHER_FIRE_RANGE_COEFFICIENT;
	static int TIME_TO_WEATHER_FADE_OFF;
	
	static int AA_AIM_ITERATIONS;
	
	// �� ������� ��� ������� �������� ������ ��� �������
	static float COEFF_TO_LOW_MORALE_WITHOUT_OFFICER;
	static float MORALE_DISPERSION_COEFF;
	static float MORALE_RELAX_COEFF;
	static float MORALE_AIMING_COEFF;

	// ������������ ����������, ����� ������ ������� ������� �������
	static float MAX_DISTANCE_TO_THROW_GRENADE;
	
	// target resolution constants
	static float TR_GUNPLANE_ALPHA_ATTACK_1;//1.0f;
	static float TR_GUNPLANE_ALPHA_ATTACK_2;//0.3f;
	static float TR_GUNPLANE_ALPHA_GO;//0.005f;
	static float TR_GUNPLANE_ALPHA_KILL;//1.0f;
	static float TR_GUNPLANE_ALPHA_PRICE;//1.0f;
	static float TR_GUNPLANE_LIMIT_TIME; //1000
	static float TR_DISTANCE_TO_CENTER_FACTOR;

	static float MAX_FIRE_RANGE_TO_SHOOT_BY_LINE;

	static int SHOW_ALL_TIME_COEFF;
	
	static float HP_PERCENT_TO_ESCAPE_FROM_BUILDING;

	static float REINFORCEMENT_GROUP_DISTANCE;

	struct SRevealInfo
	{
		// ����������� ��������� �����, ����� �� ����-�� ������������ (��������� ��� � ����������� �����, 1-2 ������)
		float fRevealByQuery;
		// ����������� ������������ ��������� �� ���������� ���������� ����� ��� ��������
		float fRevealByMovingOff;
		// ����������, �� ������� ��������� ������������ ��� �������� �� ����� ��������
		float fForgetRevealDistance;
		// �����, ����� ������� ��������� ������������ ��� ��������
		int nTimeOfReveal;

		SRevealInfo() : fRevealByQuery( 0.0f ), fRevealByMovingOff( 0.0f ), fForgetRevealDistance( 0.0f ), nTimeOfReveal( 0 ) { }
	};
	static std::unordered_map<int, SRevealInfo> REVEAL_INFO;
	
	//
	static void Load();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AI_INTERNAL_CONSTS_H__
