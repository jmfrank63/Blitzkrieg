#ifndef __GENERAL_TASKS__
#define __GENERAL_TASKS__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GeneralInternalInterfaces.h"
#include "..\Formats\fmtMap.h"
#include "AIHashFuncs.h"
#include "Resistance.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ****
// ������ �����.
// ****
class CGeneralTaskToDefendPatch : public IGeneralTask, public IWorkerEnumerator, public IEnemyEnumerator 
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CGeneralTaskToDefendPatch);
	
	IEnemyContainer *pEnemyConatainer;
	class CGeneral *pOwner;

	SAIGeneralParcelInfo  patchInfo;
	int nCurReinforcePoint;
			
	float fSeverity;										// current severity of this task
	float fEnemyForce, fFriendlyForce, fFriendlyMobileForce;	// 
	float fMaxSeverity;

	bool bFinished;
	bool bWaitForFinish;								// wait for finish the task
	NTimer::STime timeLastUpdate;				

	CommonUnits infantryInTrenches;				// ������ ������ �� �����
	CommonUnits infantryFree;
	CommonUnits tanksMobile;
	CommonUnits stationaryUnits;
	CommonUnits enemyForces;

	int nRequestForGunPlaneID;

	void CalcSeverity( const bool bEnemyUpdated, const bool bFriendlyUpdated );
	void InitTanks( class CCommonUnit *pUnit );
	void InitInfantryInTrenches( class CCommonUnit *pUnit );
public:
	CGeneralTaskToDefendPatch();
	void Init( const struct SAIGeneralParcelInfo &_patchInfo, class CGeneral *pOwner );

	virtual void SetEnemyConatiner( IEnemyContainer * _pEnemyConatainer ) { pEnemyConatainer = _pEnemyConatainer; }

	//ITask
	virtual ETaskName GetName() const ;
	virtual void AskForWorker( ICommander *pManager, const float _fMaxSeverity, const bool bInit = false );
	virtual void ReleaseWorker( ICommander *pManager, const float _fMinSeverity );
	virtual float GetSeverity() const;
	virtual bool IsFinished() const;
	virtual void CancelTask( ICommander *pManager ) ;
	virtual void Segment() ;

	//IEnumerator
	virtual bool EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType );
	virtual bool EvaluateWorker( CCommonUnit * pUnit, const enum EForceType eType ) const;
	
	//IEnemyEnumerator
	virtual bool EnumEnemy( class CAIUnit *pEnemy );

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ****
// hold reinforcemen
// ****
class CGeneralTaskToHoldReinforcement : public IGeneralTask, public IWorkerEnumerator
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CGeneralTaskToHoldReinforcement);
	CommonUnits tanksFree;
	SAIGeneralParcelInfo patchInfo;
	typedef std::unordered_map<int, CVec2> UnitsPositions;
	UnitsPositions unitsPositions;

	float fSeverity;										// current severity of this task
	int nCurReinforcePoint;	
public:
	CGeneralTaskToHoldReinforcement();
	void Init( const struct SAIGeneralParcelInfo &_patchInfo );
	//empty reinforcement parcell. uses unit's initial positions as reinforcements positions.
	void Init(){ 	fSeverity = 0; }

	//ITask
	virtual ETaskName GetName() const { return ETN_HOLD_REINFORCEMENT; }
	virtual void AskForWorker( ICommander *pManager, const float fMaxSeverity, const bool bInit = false );
	virtual void ReleaseWorker( ICommander *pManager, const float fMinSeverity );
	virtual float GetSeverity() const ;
	virtual bool IsFinished() const { return false; } 
	virtual void CancelTask( ICommander *pManager ) ;
	virtual void Segment() ;
	virtual int NeedNBest( const enum EForceType eType ) const { return true; }


	//IEnumerator
	virtual bool EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType );
	virtual bool EvaluateWorker( CCommonUnit * pUnit, const enum EForceType eType ) const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGeneralTaskRecaptureStorage : public IGeneralTask, public IWorkerEnumerator
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CGeneralTaskRecaptureStorage);
	CommonUnits tanksFree;
	CVec2 vReinforcePoint;
	float fSeverity;										// current severity of this task

	bool bFinished;
public:
	CGeneralTaskRecaptureStorage() {  }
	CGeneralTaskRecaptureStorage( const CVec2 & vReinforcePoint );

	//ITask
	virtual ETaskName GetName() const { return ETN_HOLD_REINFORCEMENT; }
	virtual void AskForWorker( ICommander *pManager, const float fMaxSeverity, const bool bInit = false );
	virtual void ReleaseWorker( ICommander *pManager, const float fMinSeverity );
	virtual float GetSeverity() const { return fSeverity; }
	virtual bool IsFinished() const ; 
	virtual void CancelTask( ICommander *pManager ) ;
	virtual void Segment() ;

	//IEnumerator
	virtual bool EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType );
	virtual bool EvaluateWorker( CCommonUnit * pUnit, const enum EForceType eType ) const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGeneralTaskToSwarmToPoint : public IGeneralTask, public IWorkerEnumerator, public IEnemyEnumerator
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CGeneralTaskToSwarmToPoint);
	enum ESwarmState
	{
		ESS_PREPEARING,
		ESS_ATTACKING,
		ESS_REST,
	};
	ESwarmState eState;
	CVec2 vPrepearCenter;

	SResistance curResistanceToAttack;
	float fSeverity;											// severity of this task
	float fMaxSeverity;
	float fMinSeverity;
	int nAdditionalIterations;						// number of additional iterations to try swarm to
	bool bFinished;
	bool bReleaseWorkers;									// this task does not always release workers
	NTimer::STime timeNextCheck;

	IEnemyContainer *pEnemyConatainer;
	class CGeneral *pOwner;

	typedef std::vector< CPtr<CCommonUnit> > CTanks;
	CTanks swarmingTanks;
	bool bResistanesBusyByUs;
	
	CVec2 vTanksPosition;									// center of tanks formation
	float fCurDistance;										// distance to nearest resistansce ( during enumeration )

	void ClearResistanceToAcceptNewTask();
	bool IsTimeToRun() const;
	void Run();
	void SendToGroupPoint();
public:
	CGeneralTaskToSwarmToPoint();
	CGeneralTaskToSwarmToPoint( IEnemyContainer *_pEnemyConatainer, class CGeneral *pOwner );
	
	virtual void SetEnemyConatiner( IEnemyContainer * _pEnemyConatainer ) { pEnemyConatainer = _pEnemyConatainer; }

	//ITask
	virtual ETaskName GetName() const { return ETN_SWARM_TO_POINT; }
	virtual void AskForWorker( ICommander *pManager, const float _fMaxSeverity, const bool bInit = false );
	virtual void ReleaseWorker( ICommander *pManager, const float fMinSeverity );
	virtual float GetSeverity() const ;
	virtual bool IsFinished() const ; 
	virtual void CancelTask( ICommander *pManager ) ;
	virtual void Segment() ;

	//IEnumerator
	virtual bool EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType );
	virtual bool EvaluateWorker( CCommonUnit * pUnit, const enum EForceType eType ) const;
	virtual int NeedNBest( const enum EForceType eType ) const { return 10000; }
	virtual float EvaluateWorkerRating( CCommonUnit * pUnit, const enum EForceType eType ) const;

	virtual bool EnumResistances( const struct SResistance &resistance );
	virtual bool EnumEnemy( class CAIUnit *pEnemy ) { NI_ASSERT_T( false, "didn't asked to" ); return false; }

	friend class CGeneralSwarmWaitForReady;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGeneralSwarmWaitForReady : public IGeneralDelayedTask 
{
	OBJECT_COMPLETE_METHODS( CGeneralSwarmWaitForReady );
	DECLARE_SERIALIZE;

	CPtr<CGeneralTaskToSwarmToPoint>  pGeneralTask;
public:
	CGeneralSwarmWaitForReady() {  }
	CGeneralSwarmWaitForReady( class CGeneralTaskToSwarmToPoint *pTask )
		:pGeneralTask( pTask )	
	{
	}
	virtual bool IsTimeToRun() const { return pGeneralTask->IsTimeToRun(); }
	virtual void Run() { pGeneralTask->Run(); }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GENERAL_TASKS__
