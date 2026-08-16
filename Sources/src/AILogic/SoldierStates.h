#ifndef __SOLDIER_STATES_H__
#define __SOLDIER_STATES_H__

#pragma ONCE
#include "UnitStates.h"
#include "StatesFactory.h"
#include "CommonStates.h"
#include "DamageToEnemyUpdater.h"
#include "RectTiles.h"
#include "../Common/Actions.h"
class CBuilding;
class CEntrenchment;
class CMineStaticObject;
class CMilitaryCar;
class CFormation;
class CSoldierStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CSoldierStatesFactory );
	
	static CPtr<CSoldierStatesFactory> pFactory;
public:
	static IStatesFactory* Instance();
	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );

	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	friend class CStaticMembers;
};
class CSoldierRestState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierRestState );	
	DECLARE_SERIALIZE;

	CAIUnit *pUnit;

	NTimer::STime startTime;
	NTimer::STime nextMove;
	bool bScanned;
	CVec2 guardPoint;
	float fDistToGuardPoint;
public:
	static IUnitState* Instance( class CAIUnit *pUnit );
	
	CSoldierRestState() : pUnit( 0 ) { }
	CSoldierRestState( class CAIUnit *pUnit );
	
	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_REST; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return guardPoint; }

	class CAIUnit* GetTarget() const { return 0; }

	void SetNullLastMoveTime() { nextMove = 1; }
};
class CSoldierAttackState : public IUnitAttackingState, public CStandartBehaviour
{
	OBJECT_COMPLETE_METHODS( CSoldierAttackState );
	DECLARE_SERIALIZE;
	
	enum { SHOOTING_CHECK = 3000, ENEMY_DIR_TOLERANCE = 10000 };

	enum ESoldierAttackStates { ESAS_BRUTE_MOVING, ESAS_MOVING, ESAS_MOVING_TO_SIDE };
	ESoldierAttackStates state;	

	NTimer::STime nextShootCheck;
	SVector lastEnemyTile;
	WORD wLastEnemyDir;

	CVec2 lastEnemyCenter;

	class CAIUnit *pUnit;
	CPtr<CAIUnit> pEnemy;

	CPtr<CBasicGun> pGun;
	bool bAim;
	bool bFinish;
	bool bSwarmAttack;
	int nEnemyParty;

	CDamageToEnemyUpdater damageToEnemyUpdater;
	CRndRunUpToEnemy runUpToEnemy;

	bool IsBruteMoving();
	interface IStaticPath* BestSidePath();

	void AnalyzeBruteMovingPosition();
	void AnalyzeMovingPosition();
	void AnalyzeMovingToSidePosition();

	void FireNow();
	void StopFire();
	void StartAgain();
public:
	static IUnitState* Instance( class CAIUnit *pUnit, class CAIUnit *pEnemy, bool bAim, const bool bSwarmAttack );

	CSoldierAttackState() : pUnit( 0 ) { }
	CSoldierAttackState( class CAIUnit *pUnit, class CAIUnit *pEnemy, bool bAim, const bool bSwarmAttack );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_ATTACK_UNIT; }
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;

	class CAIUnit* GetTarget() const { return pEnemy; }

	virtual bool IsAttacksUnit() const { return true; }
	virtual class CAIUnit* GetTargetUnit() const { return GetTarget(); }
};
class CSoldierMoveToState : public IUnitState, public CFreeFireManager
{
	OBJECT_COMPLETE_METHODS( CSoldierMoveToState );
	DECLARE_SERIALIZE;

	enum { TIME_OF_WAITING = 200 };

	CAIUnit *pUnit;

	NTimer::STime startTime;
	bool bWaiting;
	CVec2 point;
	WORD wDirToPoint;
public:
	static IUnitState* Instance( CAIUnit *pUnit, const CVec2 &point );

	CSoldierMoveToState() : pUnit( 0 ) { }
	CSoldierMoveToState( class CAIUnit *pUnit, const CVec2 &point );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
	
	virtual EUnitStateNames GetName() { return EUSN_MOVE; }
};
class CSoldierTurnToPointState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierTurnToPointState );
	DECLARE_SERIALIZE;

	CAIUnit *pUnit;

	NTimer::STime lastCheck;
	CVec2 targCenter;
public:
	static IUnitState* Instance( class CAIUnit *pUnit, const CVec2 &targCenter );

	CSoldierTurnToPointState() : pUnit( 0 ) { }
	CSoldierTurnToPointState( class CAIUnit *pUnit, const CVec2 &targCenter );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
	
	virtual EUnitStateNames GetName() { return EUSN_TURN_TO_POINT; }
};
class CSoldierMoveByDirState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierMoveByDirState );
	DECLARE_SERIALIZE;

	CAIUnit *pUnit;

	void Init( class CAIUnit *pUnit, const CVec2 &vTarget );
public:
	static IUnitState* Instance( class CAIUnit *pUnit, const CVec2 &vTarget );

	CSoldierMoveByDirState() : pUnit( 0 ) { }
	CSoldierMoveByDirState( class CAIUnit *pUnit, const CVec2 &vTarget );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	friend class CStaticMembers;
};
class CSoldierEnterState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierEnterState );
	DECLARE_SERIALIZE;

	enum EEnterStates { EES_START, EES_RUN_UP };
	EEnterStates state;

	CAIUnit *pUnit;

	int nEntrance;
	CPtr<CBuilding> pBuilding;
	
	int nEfforts;

	bool SetPathForRunUp();
public:
	static IUnitState* Instance( class CAIUnit *pUnit, class CBuilding *pBuilding );

	CSoldierEnterState() : pUnit( 0 ) { }
	CSoldierEnterState( class CAIUnit *pUnit, class CBuilding *pBuilding );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_ENTER; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CSoldierEnterEntrenchmentState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierEnterEntrenchmentState );
	DECLARE_SERIALIZE;
	
	enum EEnterState { EES_START, EES_RUN, EES_FINISHED };
	EEnterState state;

	CAIUnit *pUnit;
	CPtr<CEntrenchment> pEntrenchment;

	bool SetPathForRunIn();
	void EnterToEntrenchment();
public:
	static IUnitState* Instance( class CAIUnit *pUnit, class CEntrenchment *pEntrenchment );

	CSoldierEnterEntrenchmentState() : pUnit( 0 ) { }
	CSoldierEnterEntrenchmentState( class CAIUnit *pUnit, class CEntrenchment *pEntrenchment );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_ENTER_ENTRENCHMENT; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CSoldierAttackCommonStatObjState : public CCommonAttackCommonStatObjState
{
	OBJECT_COMPLETE_METHODS( CSoldierAttackCommonStatObjState );
	DECLARE_SERIALIZE;

	class CAIUnit *pUnit;

protected:
	virtual class CAIUnit* GetUnit() const;
	virtual void FireNow();
public:
	static IUnitState* Instance( class CAIUnit *pUnit, class CStaticObject *pObj, bool bSwarmAttack );

	CSoldierAttackCommonStatObjState() : pUnit( 0 ) { }
	CSoldierAttackCommonStatObjState( class CAIUnit *pUnit, class CStaticObject *pObj, bool bSwarmAttack );

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;
	virtual void Segment();
};
class CSoldierParadeState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierParadeState );
	DECLARE_SERIALIZE;

	CAIUnit *pUnit;
public:
	static IUnitState* Instance( class CAIUnit *pUnit );

	CSoldierParadeState() : pUnit( 0 ) { }
	CSoldierParadeState( CAIUnit *pUnit );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_PARADE; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	friend class CStaticMembers;
};
class CSoldierPlaceMineNowState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierPlaceMineNowState );
	DECLARE_SERIALIZE;

	CAIUnit *pUnit;

	CVec2 point;
	int/*SMineRPGStats::EType*/ nType;
	NTimer::STime beginAnimTime; 
public:
	static IUnitState* Instance( class CAIUnit *pUnit, const CVec2 &point, const enum SMineRPGStats::EType nType );

	CSoldierPlaceMineNowState() : pUnit( 0 ) { }
	CSoldierPlaceMineNowState( class CAIUnit *pUnit, const CVec2 &point, const enum SMineRPGStats::EType nType );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CSoldierClearMineRadiusState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierClearMineRadiusState );
	DECLARE_SERIALIZE;

	enum EPutMineStates { EPM_START, EPM_MOVE,	EPM_WAITING };
	EPutMineStates eState;

	CAIUnit *pUnit;
	CPtr<CMineStaticObject> pMine;
	// Releases pMine's disarm claim when this state dies for ANY reason - the
	// class destructor is pinned empty by OBJECT_COMPLETE_METHODS, so the
	// release rides on a member whose destructor runs on both the real
	// destructor and DestroyContents(). See the .cpp for why this matters.
	struct SMineClaimRelease
	{
		CPtr<CMineStaticObject> *ppMine;
		SMineClaimRelease() : ppMine( 0 ) {  }
		~SMineClaimRelease();
	} claimRelease;

	CVec2 clearCenter;
	NTimer::STime beginAnimTime;
	bool bReassertClaim;									// set on load: re-flag pMine as being disarmed on the first Segment (see serialize)

	bool FindMineToClear();
public:
	static IUnitState* Instance( class CAIUnit *pUnit, const CVec2 &clearCenter );

	CSoldierClearMineRadiusState() : pUnit( 0 ), bReassertClaim( false ) { claimRelease.ppMine = &pMine; }
	CSoldierClearMineRadiusState( class CAIUnit *pUnit, const CVec2 &clearCenter );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return clearCenter; }
};
class CSoldierAttackUnitInBuildingState : public CCommonAttackUnitInBuildingState
{
	OBJECT_COMPLETE_METHODS( CSoldierAttackUnitInBuildingState );
	DECLARE_SERIALIZE;
	
	CAIUnit *pUnit;
	bool bTriedToShootBuilding;
protected:	
	virtual class CAIUnit* GetUnit() const;
	virtual void FireNow();
public:
	static IUnitState* Instance( class CAIUnit *pUnit, class CSoldier *pTarget, bool bAim, const bool bSwarmAttack );

	CSoldierAttackUnitInBuildingState() : pUnit( 0 ) { }
	CSoldierAttackUnitInBuildingState( class CAIUnit *pUnit, class CSoldier *pTarget, bool bAim, const bool bSwarmAttack );
	
	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_ATTACK_UNIT_IN_BUILDING; }
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;
};
class CSoldierEnterTransportNowState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierEnterTransportNowState );
	DECLARE_SERIALIZE;

	enum EEnterTransportStates { EETS_START, EETS_MOVING, EETS_FINISHED };
	EEnterTransportStates eState;

	CAIUnit *pUnit;
	CPtr<CMilitaryCar> pTransport;
	NTimer::STime timeLastTrajectoryUpdate;
	CVec2 vLastTransportCenter;
	WORD wLastTransportDir;
public:
	static IUnitState* Instance( class CAIUnit *pUnit, class CMilitaryCar *pTransport );
	
	CSoldierEnterTransportNowState() : pUnit( 0 ) { }
	CSoldierEnterTransportNowState( class CAIUnit *pUnit, class CMilitaryCar *pTransport );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CSoldierParaDroppingState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierParaDroppingState );
	DECLARE_SERIALIZE;

	enum ESoldierParaDroppingState
	{
		ESPDS_FALLING,
		ESPDS_OPEN_PARASHUTE,
		ESPDS_FALLING_W_PARASHUTE,
		ESPDS_CLOSING_PARASHUTE,
		ESPDS_WAIT_FOR_END_UPDATES,
		ESPDS_FINISH_STATE,
	};
	ESoldierParaDroppingState eState;
	ESoldierParaDroppingState eStateToSwitch;
	
	
	NTimer::STime timeToCloseParashute, timeToOpenParashute, timeToFallWithParashute ;
	
	CSoldier *pUnit;
public:
	static IUnitState* Instance( class CSoldier *pUnit );
	
	CSoldierParaDroppingState() : pUnit( 0 ) { }
	CSoldierParaDroppingState( class CSoldier *pUnit );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
		virtual EUnitStateNames GetName() { return EUSN_PARTROOP; }

};
class CSoldierUseSpyglassState : public IUnitState, public CStandartBehaviour
{
	OBJECT_COMPLETE_METHODS( CSoldierUseSpyglassState );
	DECLARE_SERIALIZE;

	CSoldier *pSoldier;
	CVec2 vPoint2Look;

	void SetLookAnimation();
public:
	static IUnitState* Instance( class CSoldier *pSoldier, const CVec2 &point );

	CSoldierUseSpyglassState() : pSoldier( 0 ), vPoint2Look( VNULL2 ) { }
	CSoldierUseSpyglassState( class CSoldier *pSoldier, const CVec2 &point );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CSoldierAttackFormationState: public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CSoldierAttackFormationState );
	DECLARE_SERIALIZE;

	CAIUnit *pUnit;
	CPtr<CFormation> pTarget;
	bool bSwarmAttack;
public:
	static IUnitState* Instance( class CAIUnit *pUnit, class CFormation *pTarget, const bool bSwarmAttack );
	
	CSoldierAttackFormationState() : pUnit( 0 ) { }
	CSoldierAttackFormationState( class CAIUnit *pUnit, class CFormation *pTarget, const bool bSwarmAttack );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;

	virtual bool IsAttacksUnit() const { return true; }
	virtual class CAIUnit* GetTargetUnit() const { return 0; }
};
class CSoldierIdleState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierIdleState );
	DECLARE_SERIALIZE;

	CAIUnit *pUnit;
public:
	static IUnitState* Instance( class CAIUnit *pUnit );
	
	CSoldierIdleState() : pUnit( 0 ) { }
	CSoldierIdleState( class CAIUnit *pUnit );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CAviation;
class CSoldierAttackAviationState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CSoldierAttackAviationState );
	DECLARE_SERIALIZE;

	enum ESoldierAttackAviationState
	{
		SAAS_ESITMATING,
		
		SAAS_START_TRASING,										// ��� �������� ���������������� �����
		SAAS_TRASING,
		SAAS_FIRING,
		
		SAAS_START_AIMING_TO_PREDICTED_POINT,	// ��� �������� �������������� �����
		SAAS_AIM_TO_PREDICTED_POINT,					
		SAAS_START_FIRE_TO_PREDICTED_POINT,
		SAAS_FIRING_TO_PREDICTED_POINT,

		SAAS_FINISH,
		SAAS_WAIT_FOR_END_OF_BURST,
	};

	class SPredict
	{
		WORD wHor, wVer;
		CVec3 vPt;
		float fRange;
		NTimer::STime timeToFire;
	public:
		SPredict() {  }
		SPredict( const CVec3 &pt, const float _fRange, const NTimer::STime _timeToFire, CAIUnit *pOwner );
		WORD GetHor()const { return wHor; }
		WORD GetVer()const { return wVer; }
		float GetRange() const { return fRange; }
		const CVec3 GetPt() const { return vPt; }
		const NTimer::STime GetFireTime() const { return timeToFire; }
	};

	ESoldierAttackAviationState eState;

	class CAIUnit *pUnit;
	CPtr<CAviation> pPlane;
	bool bAttacking;											// true when desided to aim and shoot

	SPredict aimPoint;		// ����� ������������ ��� �������� �������������� �����
	NTimer::STime timeOfStartBurst;
	NTimer::STime timeLastAimUpdate;

	bool CanFireNow() const;
	void FireNow();
	void StopFire();
	bool IsFinishedFire();
	void Aim();
	bool CalcAimPoint();

public:
	static IUnitState* Instance( class CAIUnit *pUnit, class CAviation *pPlane );

	CSoldierAttackAviationState () : pUnit( 0 ) { }
	CSoldierAttackAviationState ( class CAIUnit *pUnit, class CAviation *pPlane );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return bAttacking; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }

	virtual bool IsAttacksUnit() const { return true; }
	virtual class CAIUnit* GetTargetUnit() const;
};
class CSoldierFireMoraleShellState : public IUnitState
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CSoldierFireMoraleShellState );

	class CAIUnit *pUnit;
	int nMoraleGun;
	CVec2 vTarget;
public:
	static IUnitState* Instance( class CAIUnit *pUnit, const class CVec2 &vTarget );

	CSoldierFireMoraleShellState() : pUnit( 0 ) { }
	CSoldierFireMoraleShellState( class CAIUnit *pUnit, const class CVec2 &vTarget );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
};
class CSoldierUseState : public IUnitState
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CSoldierUseState );

	EActionNotify eState;
	class CAIUnit *pUnit;
public:
	static IUnitState* Instance( class CAIUnit *pUnit, const EActionNotify &eState );

	CSoldierUseState() : eState( ACTION_NOTIFY_NONE ), pUnit( 0 ) { }
	CSoldierUseState( class CAIUnit *pUnit, const EActionNotify &eState );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }

	virtual EUnitStateNames GetName() { return EUSN_USE; }
};
#endif // __SOLDIER_STATES_H__
