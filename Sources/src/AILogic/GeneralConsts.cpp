#include "StdAfx.h"

#include "GeneralConsts.h"
int SGeneralConsts::TIME_DONT_SEE_ENEMY_BEFORE_FORGET = 5000;
int SGeneralConsts::SCOUT_FREE_POINT = 300;						// скаут шлется в точку, если в этом радиусе от нее нет наших
int SGeneralConsts::SCOUT_POINTS = 4;
NTimer::STime SGeneralConsts::TIME_SONT_SEE_AA_BEFORE_FORGET = 180000;			// связать с временем регенерации самолетов

int SGeneralConsts::AVIATION_PERIOD_MAX = 60000;
int SGeneralConsts::AVIATION_PERIOD_MIN = 10000;

int SGeneralConsts::FIGHTER_PERIOD_MAX = 20000;
int SGeneralConsts::FIGHTER_PERIOD_MIN = 10000;
float SGeneralConsts::FIGHTER_INTERCEPT_OFFSET = 1000;

float SGeneralConsts::AVATION_LONG_PERIOD_PROBABILITY = 0.5f;
float SGeneralConsts::AVATION_PERIOD_MULTIPLY = 5;
float SGeneralConsts::PLAYER_FORCE_COEFFICIENT = 2.0;

const int SGeneralConsts::RESISTANCE_CELL_SIZE = 32;
float SGeneralConsts::TIME_DONT_SEE_EMPTY_ARTILLERY_BEFORE_FORGET = 20000;
float SGeneralConsts::TIME_TO_FORGET_ANTI_ARTILLERY = 240000;
float SGeneralConsts::TIME_TO_FORGET_UNIT = 180000;
float SGeneralConsts::TIME_TO_ARTILLERY_FIRE = 3000;
float SGeneralConsts::PROBABILITY_TO_SHOOT_AFTER_ARTILLERY_FIRE = 1.0f;
float SGeneralConsts::SHOOTS_OF_ARTILLERY_FIRE = 10.0f;
int SGeneralConsts::GENERAL_UPDATE_PERIOD = 1000;
float SGeneralConsts::REPAIR_STORAGE_PROBABILITY = 0.1f;
float SGeneralConsts::RECAPTURE_STORAGE_PROBALITY = 0.2f;
float SGeneralConsts::RECAPTURE_STORAGE_MAX_DISTANCE = 2000.0f;
int SGeneralConsts::RECAPTURE_ARTILLERY_TANKS_NUMBER = 2; 
int SGeneralConsts::RESUPPLY_CELL_AFTER_TRANSPORT_DEATH;
int SGeneralConsts::RESUPPLY_CELL_AFTER_TRANSPORT_DEATH_RAND;
float SGeneralConsts::INTENDANT_DANGEROUS_CELL_RADIUS = 1000;
int SGeneralConsts::SWARM_ADDITIONAL_ITERATIONS = 3;
float SGeneralConsts::MIN_WEIGHT_TO_SEND_SWARM = 30.0f;

float SGeneralConsts::MIN_WEIGHT_TO_ARTILLERY_FIRE = 50.0f;
float SGeneralConsts::MIN_WEIGHT_TO_SEND_BOMBERS = 100.0f;
float SGeneralConsts::SWARM_WEIGHT_COEFFICIENT = 1.0f;

int SGeneralConsts::TIME_TO_WAIT_SWARM_READY = 10; 
int SGeneralConsts::TIME_TO_WAIT_SWARM_READY_RANDOM = 10;

int SGeneralConsts::TIME_SWARM_DURATION;
int SGeneralConsts::TIME_SWARM_DURATION_RANDOM;

void SGeneralConsts::Init()
{
	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );

	TIME_DONT_SEE_ENEMY_BEFORE_FORGET = constsTbl.GetInt( "AI", "General.TimeDontSeeTheEnemyBeforeForget", 10000 );
	
	SCOUT_FREE_POINT = constsTbl.GetInt( "AI", "General.Aviation.ScoutFreePoint", 300 );
	SCOUT_POINTS = constsTbl.GetInt( "AI", "General.Aviation.ScoutPointsPerMap", 4 );

	TIME_SONT_SEE_AA_BEFORE_FORGET = constsTbl.GetInt( "AI", "General.TimeDontSeeAABeforeForget", 10000 );
	FIGHTER_INTERCEPT_OFFSET = constsTbl.GetFloat( "AI", "General.Aviation.FighterInterceptOffset", 1000 );

	AVIATION_PERIOD_MAX = constsTbl.GetInt( "AI", "General.Aviation.PeriodMax", 30000 );
	AVIATION_PERIOD_MIN = constsTbl.GetInt( "AI", "General.Aviation.PeriodMin", 10000 );

	FIGHTER_PERIOD_MAX = constsTbl.GetInt( "AI", "General.Aviation.FighterPeriodMax", 10000 );
	FIGHTER_PERIOD_MIN = constsTbl.GetInt( "AI", "General.Aviation.FighterPeriodMin", 5000 );

	AVATION_LONG_PERIOD_PROBABILITY = constsTbl.GetFloat( "AI", "General.Aviation.LongPeriodProbability", 0.5f );
	AVATION_PERIOD_MULTIPLY = constsTbl.GetFloat( "AI", "General.Aviation.PeriodMultiply", 5.0f );
	PLAYER_FORCE_COEFFICIENT = constsTbl.GetFloat( "AI", "General.PlayerForceMultiply", 2.0f );

	TIME_TO_FORGET_ANTI_ARTILLERY = constsTbl.GetFloat( "AI", "General.Artillery.TimeToForgetAntiArtillery", 240000 );
	TIME_TO_FORGET_UNIT = constsTbl.GetFloat( "AI", "General.Artillery.TimeToForgetUnit", 120000 );
	TIME_TO_ARTILLERY_FIRE = constsTbl.GetFloat( "AI", "General.Artillery.TimeToArtilleryFire", 3000 );
	PROBABILITY_TO_SHOOT_AFTER_ARTILLERY_FIRE = constsTbl.GetFloat( "AI", "General.Artillery.ProbabilityToShootAfterArtilleryFire", 1.0f );
	SHOOTS_OF_ARTILLERY_FIRE = constsTbl.GetFloat( "AI", "General.Artillery.ShootsOfArtilleryFire", 10.0f );
	MIN_WEIGHT_TO_ARTILLERY_FIRE = constsTbl.GetFloat( "AI", "General.Artillery.MinWeightToArtilleryFire", 50.0f );
	MIN_WEIGHT_TO_SEND_BOMBERS = constsTbl.GetFloat( "AI", "General.Artillery.MinWeightToSendBombers", 100.0f );

	
	GENERAL_UPDATE_PERIOD = constsTbl.GetInt( "AI", "General.UpdatePeriod", 3000 );
	REPAIR_STORAGE_PROBABILITY = constsTbl.GetFloat( "AI", "General.Intendant.ProbabilityToRepairStorage", 0.1f );
	RECAPTURE_STORAGE_PROBALITY = constsTbl.GetFloat( "AI", "General.Intendant.ProbabilityToRecaptureStorage", 0.4f );

	SWARM_ADDITIONAL_ITERATIONS = constsTbl.GetInt( "AI", "General.Swarm.Iterations", 3 );
	MIN_WEIGHT_TO_SEND_SWARM = constsTbl.GetInt( "AI", "General.Swarm.MinWeight", 30.0f );
	SWARM_WEIGHT_COEFFICIENT = constsTbl.GetFloat( "AI", "General.Swarm.WeightCoefficient", 1.0f );

	RESUPPLY_CELL_AFTER_TRANSPORT_DEATH = constsTbl.GetInt( "AI", "General.Intendant.ResupplyCellPeriodAfterDeath", 10000 );
	RESUPPLY_CELL_AFTER_TRANSPORT_DEATH_RAND = constsTbl.GetInt( "AI", "General.Intendant.ResupplyCellPeriodAfterDeathRandom", 20000 );
	INTENDANT_DANGEROUS_CELL_RADIUS = constsTbl.GetFloat( "AI", "General.Intendant.DangerousCellRadius", 1000 );

	RECAPTURE_STORAGE_MAX_DISTANCE = constsTbl.GetFloat( "AI", "General.Intendant.RecaptureStorageMaxDistance", 2000.0f );
	RECAPTURE_ARTILLERY_TANKS_NUMBER = constsTbl.GetInt( "AI", "General.Intendant.RecaptureStorageMaxUnits", 2 );
	TIME_DONT_SEE_EMPTY_ARTILLERY_BEFORE_FORGET = constsTbl.GetInt( "AI", "General.Intendant.TimeDontSeeEmptyArtilleryBeforeForget", 2000 );

	TIME_TO_WAIT_SWARM_READY = constsTbl.GetInt( "AI", "General.Swarm.WaitTime", 10 );
	TIME_TO_WAIT_SWARM_READY_RANDOM = constsTbl.GetInt( "AI", "General.Swarm.WaitTimeRandom", 10 );

	TIME_SWARM_DURATION = constsTbl.GetInt( "AI", "General.Swarm.IterationDuration", 20 );
	TIME_SWARM_DURATION_RANDOM = constsTbl.GetInt( "AI", "General.Swarm.IterationDurationRandom", 20 );
}
