#ifndef __PLANE_STATES_H__
#define __PLANE_STATES_H__

#pragma ONCE
#include "StatesFactory.h"
#include "UnitStates.h"
#include "DamageToEnemyUpdater.h"
class CAviation;
class CFormation;
class CPlaneStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CPlaneStatesFactory );
	
	static CPtr<CPlaneStatesFactory> pFactory;
public:
	static IStatesFactory* Instance();

	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );

	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );
	
	friend class CStaticMembers;
};
class CPlaneRestState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CPlaneRestState );
	DECLARE_SERIALIZE;

	class CAviation *pPlane;

	CVec3 vertices[3];
	int curVertex;
	float fHeight;

	void InitTriangle( class CAviation *pPlane, const CVec3 &startVertex );
public:
	static IUnitState* Instance( class CAviation *pPlane, float fHeight=-1 );

	CPlaneRestState() : pPlane( 0 ) { }
	CPlaneRestState( class CAviation *_pPlane, float fHeight );

	virtual void Segment();
	
	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_REST; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CPlaneDeffensiveFireShootEstimator;
class CPlaneDeffensiveFire
{
	DECLARE_SERIALIZE;
	
	class CAviation *pOwner;
	NTimer::STime timeLastBSUUpdate;			// для поведения бортовых стрелковых установок

	CPtr<CPlaneDeffensiveFireShootEstimator> pShootEstimator;
	CDamageToEnemyUpdater damageUpdater;
protected:
	void AnalyzeBSU();
	CPlaneDeffensiveFire() : pOwner( 0 ), timeLastBSUUpdate( 0 ) {}
	CPlaneDeffensiveFire( class CAviation *pPlane );
};
class CPlanePatrolState : public IUnitAttackingState
{
	DECLARE_SERIALIZE;
	
protected:
  std::vector<CVec2> vPatrolPoints;			// набор точек патрулирования
	int								 nCurPointIndex;		// текущая точка патрулирования
	class CAviation *pPlane;

	const CVec2 &GetPoint() const { return vPatrolPoints[nCurPointIndex%vPatrolPoints.size()]; }
	const void ToNextPoint() { ++nCurPointIndex; }

	void InitPathByCurDir( const float fDesiredHeight );

	virtual void ToTakeOffState() = 0;

	CAviation* FindBetterEnemiyPlane( CAviation * pEnemie, const float fRadius ) const;
	CAviation* FindNewEnemyPlane( const float fRadius ) const;
public:
	CPlanePatrolState() : nCurPointIndex( 0 ), pPlane( 0 ) {  }
	CPlanePatrolState( CAviation *pPlane, const CVec2 &point );
	int GetNPoints() const { return vPatrolPoints.size(); }
	void AddPoint( const CVec2 &vAddPoint );
	void TakeOff() { ToTakeOffState(); } 
	void Escape( const int /*SUCAviation::AIRCRAFT_TYPE*/ nAviaType );

	virtual bool IsAttacksUnit() const { return false; }
	virtual class CAIUnit* GetTargetUnit() const { return 0; }
	void RegisterPoints( const int /*SUCAviation::AIRCRAFT_TYPE*/ nPlaneType );
	void UnRegisterPoints();
};
class CPlaneBombState : public CPlanePatrolState, public CPlaneDeffensiveFire
{
	OBJECT_COMPLETE_METHODS( CPlaneBombState );
	DECLARE_SERIALIZE;

	enum ECurBombState
	{
		_WAIT_FOR_TAKEOFF,
		ECBS_ESTIMATE,

		ECBS_GAIN_HEIGHT,
		ECBS_GAIN_DISTANCE,

		ECBS_APPROACH,
		ECBS_WAIT_BOMBPOINT_REACH,
		ECBS_ATTACK_DIVE,
		ECBS_ATTACK_HORISONTAL,

		ECBS_AIM_TO_NEXT_POINT,
		ECBS_AIM_TO_NEXT_POINT_2,
		ECBS_START_ESACPE,
	};
	ECurBombState eState;
	bool bDive;														//dive bomber or not
	bool bDiveInProgress;

	float fInitialHeight ;
	float fStartAttackDist;

	float fFormerVerticalSpeed;						//to determine exit from diving
	bool bExitDive;
	NTimer::STime timeOfStart;						// time of start patrolling
protected:
	virtual void ToTakeOffState();
public:
	static IUnitState* Instance( CAviation *pPlane, const CVec2 &point );

	CPlaneBombState() { }
	CPlaneBombState( CAviation *pUnit, const CVec2 &point );

	virtual void Segment();

	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const { return GetPoint(); }
	virtual bool IsDiving() const { return bDiveInProgress; }
	virtual EUnitStateNames GetName() { return EUSN_DIVE_BOMBING; }
};
class CPlaneParaDropState : public CPlanePatrolState, public CPlaneDeffensiveFire
{
	OBJECT_COMPLETE_METHODS( CPlaneParaDropState );
	DECLARE_SERIALIZE;	
	enum EPlaneParaDropState
	{
		_WAIT_FOR_TAKEOFF,
		PPDS_ESTIMATE,
		PPDS_APPROACHNIG,
		PPDS_PREPARE_TO_DROP,
		PPDS_DROPPING,
		PPDS_TO_NEXT_POINT,
	};
	EPlaneParaDropState eState;

	int nSquadNumber;
	CPtr<CFormation> pSquad; // взвод паращютистов
	int nDroppingSoldier;									// current soldier to drop

	CVec2 vLastDrop;// точка, в которой выброшен последний парашютист
	bool CanDrop( const CVec2 & point );
protected:
	virtual void ToTakeOffState();
	
public:
	static IUnitState* Instance( CAviation *pPlane, const CVec2 &vDestPoint );

	CPlaneParaDropState () { }
	CPlaneParaDropState ( CAviation *pPlane, const CVec2 &vDestPoint );

	virtual void Segment();

	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	friend class CStaticMembers;
};
class CPlaneFighterPatrolState : public CPlanePatrolState, public CPlaneDeffensiveFire
{
	OBJECT_COMPLETE_METHODS( CPlaneFighterPatrolState );
	DECLARE_SERIALIZE;	

  enum ECurFighterOnEnemieState
	{
		_WAIT_FOR_TAKEOFF,
		ECFS_ESCAPE,
		ECFS_GOTO_GUARDPOINT,
		ECFS_GOING_TO_GUARDPOINT,
		ECFS_ENGAGE_TARGET,
		ECFS_AIM_TO_NEXT_POINT,
		ECFS_FIND_ENEMY_OR_NEXT_POINT,
	};
	ECurFighterOnEnemieState eState;

	float fPartolRadius;									// patrol radius of this state
	float fPatrolHeight;									// height of patrolling
	CPtr<CAviation> pEnemie;							//enemie that we attack (plane)

	NTimer::STime timeOfStart;						//time of start patrolling
	NTimer::STime timeOfLastPathUpdate;		//last update of path
	NTimer::STime timeLastCheck;					// последняя проверка на наличие патронов

	void TryInitPathToEnemie( bool isEnemieNew = false );
	void TryInitPathToPoint( const CVec3 & v, bool isNewPoint=false );
protected:
	virtual void ToTakeOffState();
	bool IsEnemyAlive( const class CAviation * pEnemie ) const;
public:
	static IUnitState* Instance( CAviation *pPlane, const CVec2 &point );

	CPlaneFighterPatrolState () { }
	CPlaneFighterPatrolState ( CAviation *_pPlane, const CVec2 &point ) ;

	virtual void Segment();

	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1,-1 ); }

	virtual bool IsAttacksUnit() const ;
	virtual class CAIUnit* GetTargetUnit() const ;

	friend class CStaticMembers;
};
class CAIUnit;
class CBuilding;
class CPlaneShturmovikShootEstimator;
class CPlaneShturmovikPatrolState : public CPlanePatrolState, public CPlaneDeffensiveFire
{
	OBJECT_COMPLETE_METHODS( CPlaneShturmovikPatrolState );
	DECLARE_SERIALIZE;	

	class CBombEstimator
	{
		bool bFire;
		int nMechUnits, nInfantry;
		float fDamage;											// повреждения от одной бомбы
		float fDisp;												// круг в котором должны оказаться юниты
		const CVec2 vCenter;								
		const float fFlyTime;
	public:
		CBombEstimator( class CAviation *pAttacker, 
										const float fDamage,
										const CVec2 &vCenter,
										const float fDisp,
										const float fFlyTime );

		bool Collect( class CAIUnit *pTry );
		bool NeedDrop() const { return bFire;	}
	};
  enum EPlaneShturmovikPatrolState
	{
		_WAIT_FOR_TAKEOFF,
		PSPS_ESCAPE,
		PSPS_GOTO_GUARDPOINT,
		PSPS_GOING_TO_GUARDPOINT,
		PSPS_AIM_TO_NEXT_POINT,
		PSPS_FIND_ENEMY_OR_NEXT_POINT,

		PSPS_APPROACH_TARGET,
		PSPS_APPROACHING_TARGET,
		PSPS_ENGAGING_TARGET,
		PSPS_APPROACHING_TARGET_TOWARS_IT,
		PSPS_FIRE_TO_WORLD,
		PSPS_GAIN_HEIGHT,
	};

	class CEnemyContainer
	{
		DECLARE_SERIALIZE;
		CAIUnit *pOwner;
		
		float fTakenDamage;
		CPtr<CAIUnit> pEnemy;
		CPtr<CBuilding> pBuilding;
		
		CEnemyContainer( const CEnemyContainer & r );
		CEnemyContainer operator=( const CEnemyContainer &r );
	public:
		CEnemyContainer() : pOwner( 0 ), fTakenDamage( 0.0f ) { }
		CEnemyContainer( CAIUnit *pOwner ) : pOwner( pOwner ), fTakenDamage( 0.0f ) { }

		CVec2 GetCenter() const;
		float GetZ() const;
		bool CanShootToTarget( class CBasicGun * pGun ) const;
		void StartBurst( class CBasicGun *pGun );

		void SetEnemy( CAIUnit * pNewEnemy );
		void SetEnemy( CBuilding * pBuilding );

		bool IsValidBuilding() const;
		bool IsValidUnit() const;
		bool IsValid() const ; // is alive and is valid
		
		CAIUnit *GetEnemy();
		CBuilding *GetBuilding();
	};

	EPlaneShturmovikPatrolState eState;
	CVec3 vCurTargetPoint;								// точка, куда направляется самолет

	CPtr<CPlaneShturmovikShootEstimator> pShootEstimator;

	class CAviation *pPlane;

	CEnemyContainer enemie;								// enemie that we attack (ground target)
	
	float fPatrolHeight;
	
	NTimer::STime timeOfStart;						// time of start patrolling
	NTimer::STime timeOfLastPathUpdate;		// last update of path
	NTimer::STime timeLastCheck ;					// проверка на наличие патронов

	float fStartAttackDist;								// дистанция для начала пикирования
	float fFinishAttckDist;								// дистанция выхода из атаки
	float fTurnRadius;										// радиус поворота штурмовика
	enum EGunplaneCalledAs eCalledAs;					// 

	void TryInitPathToEnemie();
	void TryInitPathToPoint( const CVec3 & v, bool isNewPoint = false );
	bool FindNewEnemie();

	CAIUnit* FindEnemyInPossibleDiveSector();
	CAIUnit* FindEnemyInFiringSector();

	void TryBurstAllGuns();
	void TryDropBombs();
	void TryBurstAllGunsToPoints();
	bool IsTargetBehind( const CVec2 &vTarget ) const;
protected:
	virtual void ToTakeOffState();
public:
	static IUnitState* Instance( CAviation *pPlane, const CVec2 &point, const int eCalledAs );

	CPlaneShturmovikPatrolState () : pPlane( 0 ) { }
	CPlaneShturmovikPatrolState ( CAviation *_pPlane, const CVec2 &point, const int eCalledAs );

	virtual void Segment();

	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return GetPoint(); }
};
class CPlaneScoutState : public CPlanePatrolState, public CPlaneDeffensiveFire
{
	OBJECT_COMPLETE_METHODS( CPlaneScoutState );
	DECLARE_SERIALIZE;	

  enum EPlaneScoutState 
	{
		_WAIT_FOR_TAKEOFF,
		EPSS_GOTO_GUARDPOINT,
		EPSS_GOING_TO_GUARDPOINT,
		EPSS_AIM_TO_NEXT_POINT,
		EPSS_ESCAPE,
	};
	EPlaneScoutState eState;

	float fPatrolHeight;									// height of patrolling
	NTimer::STime timeOfStart;

protected:
	virtual void ToTakeOffState();
public:

	static IUnitState* Instance( CAviation *pPlane, const CVec2 &point );

	CPlaneScoutState () { }
	CPlaneScoutState ( CAviation *_pPlane, const CVec2 &point ) ;

	virtual void Segment();

	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return GetPoint(); }
};
class CPlaneLeaveState : public IUnitState, public CPlaneDeffensiveFire
{
	
	OBJECT_COMPLETE_METHODS( CPlaneLeaveState );
	DECLARE_SERIALIZE;	

	class CAviation *pPlane;
	CPtr<IUnitState> pMoveToExitPoint; 
	int nAviaType;

	enum EPlaneLeaveState
	{
		EPLS_STARTING,
		EPLS_IN_ROUTE,
	};
	EPlaneLeaveState eState;
public:
	static IUnitState* Instance( CAviation *pPlane, const int nAviaType );

	CPlaneLeaveState() : pPlane( 0 ){ }
	CPlaneLeaveState( CAviation *pPlane, const int nAviaType );

	virtual void Segment();

	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
	virtual EUnitStateNames GetName() { return EUSN_REST; }

	friend class CStaticMembers;
};
class CPlaneFlyDeadState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CPlaneFlyDeadState );
	DECLARE_SERIALIZE;	
	enum EPlaneDeadState 
	{
		EPDS_START_DIVE,
		EPDS_DIVE,
		EPDS_ESTIMATE,
		EPDS_WAIT_FINISH_PATH,
	};
	EPlaneDeadState eState;

	class CDeadZone
	{
		float fMaxX, fMinX;
		float fMinY, fMaxY;
	public:
		CDeadZone() : fMaxX( 0.0f ), fMinX( 0.0f ), fMaxY( 0.0f ), fMinY( 0.0f ) { }
		void Init();
		bool IsInZone( const CVec2 &vPoint );
		void AdjustEscapePoint( CVec2 * pPoint );
	};
	CDeadZone deadZone;											// вне зтого rect умирают самолеты

	class CAviation *pPlane;
	float fHeight;
	bool bFatality;
	bool bExplodeInstantly;									// if false plane will explode after finish dive.
	NTimer::STime timeStart;								// start death time

	void InitPathToNearestPoint();
	float CalcPath( const WORD wCurDir, const byte nDesiredDir, const bool bRight, const float fTurnRadius, CVec2 *vDestPoint );
public:
	static IUnitState* Instance( CAviation *pPlane );

	CPlaneFlyDeadState () : pPlane( 0 ){ }
	CPlaneFlyDeadState ( CAviation *_pPlane );

	virtual void Segment();

	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const ;
	virtual EUnitStateNames GetName() { return EUSN_FLY_DEAD; }
};
#endif // __PLANE_STATES_H__
