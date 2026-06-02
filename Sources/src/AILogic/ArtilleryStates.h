#ifndef __ARTILLERY_STATES_H__
#define __ARTILLERY_STATES_H__

#pragma ONCE
#include "StatesFactory.h"
#include "UnitStates.h"
#include "Behaviour.h"
#include "FreeFireManager.h"
#include "SoldierStates.h"
class CAIUnit;
class CStaticObject;
class CAITransportUnit;
class CArtilleryStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CArtilleryStatesFactory );

	static CPtr<CArtilleryStatesFactory> pFactory;
public:
	static IStatesFactory* Instance();

	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );

	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	friend class CStaticMembers;
};
class CArtilleryMoveToState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CArtilleryMoveToState );
	DECLARE_SERIALIZE;

	enum { TIME_OF_WAITING = 200 };
	enum EArtilleryMoveToState { EAMTS_UNINSTALLING, EAMTS_START_MOVING, EAMTS_MOVING, EAMTS_WAIT_FOR_PATH };
	EArtilleryMoveToState eState;

	bool bToFinish;
	class CArtillery *pArtillery;

	NTimer::STime startTime;
	CPtr<IStaticPath> pStaticPath;
public:
	static IUnitState* Instance( class CArtillery *pArtillery, const CVec2 &point );

	CArtilleryMoveToState() : pArtillery( 0 ) { }
	CArtilleryMoveToState( class CArtillery *pArtillery, const CVec2 &point );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState(class CAICommand *pCommand);
	
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
};
class CArtilleryTurnToPointState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CArtilleryTurnToPointState );
	DECLARE_SERIALIZE;

	enum EArtilleryTurnToPointStates { EATRS_ESTIMATING, EATPS_UNINSTALLING, EATPS_TURNING };
	EArtilleryTurnToPointStates eState;

	class CArtillery *pArtillery;

	NTimer::STime lastCheck;
	CVec2 targCenter;
public:
	static IUnitState* Instance( class CArtillery *pArtillery, const CVec2 &targCenter );

	CArtilleryTurnToPointState() : pArtillery( 0 ) { }
	CArtilleryTurnToPointState( class CArtillery *pArtillery, const CVec2 &targCenter );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState(class CAICommand *pCommand);

	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
	
	virtual EUnitStateNames GetName() { return EUSN_TURN_TO_POINT; }
};
class CArtilleryBombardmentState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CArtilleryBombardmentState );
	DECLARE_SERIALIZE;

	enum EArtilleryBombarmentStates { EABS_START, EABS_TURNING, EABS_FIRING };
	EArtilleryBombarmentStates eState;

	class CAIUnit *pUnit;

	CVec2 point;
	bool bStop;
public:
	static IUnitState* Instance( class CAIUnit *pUnit, const CVec2 &point );

	CArtilleryBombardmentState() : pUnit( 0 ) { }
	CArtilleryBombardmentState( class CAIUnit *pUnit, const CVec2 &point );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const { return point; }

	virtual bool IsAttacksUnit() const { return false; }
	virtual class CAIUnit* GetTargetUnit() const { return 0; }
	
	virtual EUnitStateNames GetName() { return EUSN_BOMBARDMANET; }
};
class CArtilleryRangeAreaState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CArtilleryRangeAreaState );
	DECLARE_SERIALIZE;

	enum ERangeAreaStates { ERAS_TURNING, ERAS_RANGING, ERAS_WAITING, ERAS_SHOOT_UNIT, ERAS_SHOOT_OBJECT };
	ERangeAreaStates eState;
	enum { CHECK_TIME = 2000 };

	class CAIUnit *pUnit;

	CPtr<CAIUnit> pEnemy;
	CPtr<CStaticObject> pObj;
	CPtr<CBasicGun> pGun;

	CVec2 point;
	int nShootsLast;
	NTimer::STime lastCheck;
	bool bFinish;
	bool bFired;

	void CheckArea();
	void FinishCommand();

public:
	static IUnitState* Instance( class CAIUnit *pUnit, const CVec2 &point );

	CArtilleryRangeAreaState() : pUnit( 0 ) { }
	CArtilleryRangeAreaState( class CAIUnit *pUnit, const CVec2 &point );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState(class CAICommand *pCommand);
	
	virtual EUnitStateNames GetName() { return EUSN_RANGING; }
	void GetRangeCircle( CCircle *pCircle ) const;

	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const { return point; }

	virtual bool IsAttacksUnit() const;
	virtual class CAIUnit* GetTargetUnit() const;
};
class CArtilleryInstallTransportState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CArtilleryInstallTransportState );
	DECLARE_SERIALIZE;

	enum EArtilleryInstallTransportState 
	{
		AITS_WAITING_FOR_CREW,
		AITS_INSTALLING,
	};
	EArtilleryInstallTransportState eState;

	class CArtillery *pArtillery;
public:
	static IUnitState* Instance( class CArtillery *pArtillery );

	CArtilleryInstallTransportState() : pArtillery( 0 ) { }
	CArtilleryInstallTransportState( class CArtillery *pArtillery );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState(class CAICommand *pCommand);

	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CArtilleryUninstallTransportState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CArtilleryUninstallTransportState );
	DECLARE_SERIALIZE;
	
	enum EArtilleryUninstallTransportState 
	{
		AUTS_WAIT_FOR_UNINSTALL,
		AUTS_INSTALLING,

		AUTS_WAIT_FOR_UNINSTALL_TRANSPORT,
	};

	EArtilleryUninstallTransportState eState;

	class CArtillery *pArtillery;
public:
	static IUnitState* Instance( class CArtillery *pArtillery );

	CArtilleryUninstallTransportState() : pArtillery( 0 ) { }
	CArtilleryUninstallTransportState( class CArtillery *pArtillery );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CArtilleryBeingTowedPath;
class CArtilleryBeingTowedState: public IUnitState
{
	OBJECT_COMPLETE_METHODS( CArtilleryBeingTowedState );
	DECLARE_SERIALIZE;

	class CArtillery *pArtillery;

	CPtr<CAITransportUnit> pTransport;
	WORD wLastTagDir;
	CVec2 vLastTagCenter;

	bool bInterrupted;
	CPtr<CArtilleryBeingTowedPath> pPath;
	NTimer::STime timeLastUpdate;
public:
	static IUnitState* Instance( class CArtillery *pArtillery, class CAITransportUnit * pTransport );

	CArtilleryBeingTowedState() : pArtillery( 0 ) { }
	CArtilleryBeingTowedState( class CArtillery *pArtillery, class CAITransportUnit * pTransport );
	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual EUnitStateNames GetName() { return EUSN_BEING_TOWED; }

	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
	
	class CAITransportUnit* GetTowingTransport() const;
};
class CArtilleryAttackState : public IUnitAttackingState, public CFreeFireManager
{
	OBJECT_COMPLETE_METHODS( CArtilleryAttackState );
	DECLARE_SERIALIZE;

	enum EAttackStates { EAS_NONE, EAS_ROTATING, EAS_FIRING };
	EAttackStates eState;
	WORD wDirToRotate;
	
	class CArtillery *pArtillery;
	CPtr<CAIUnit> pEnemy;
	bool bAim;
	bool bFinish;

	CPtr<CBasicGun> pGun;
	bool bSwarmAttack;

	CDamageToEnemyUpdater damageToEnemyUpdater;
	int nEnemyParty;

	void FinishState();
public:
	static IUnitState* Instance( class CArtillery *pArtillery, class CAIUnit *pEnemy, bool bAim, const bool bSwarmAttack );

	CArtilleryAttackState() : pArtillery( 0 ) { }
	CArtilleryAttackState( class CArtillery *pArtillery, class CAIUnit *pEnemy, bool bAim, const bool bSwarmAttack );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_ATTACK_UNIT; }
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;

	virtual bool IsAttacksUnit() const { return true; }
	virtual class CAIUnit* GetTargetUnit() const;
};
class CArtilleryAttackCommonStatObjState : public IUnitAttackingState, public CFreeFireManager
{
	OBJECT_COMPLETE_METHODS( CArtilleryAttackCommonStatObjState );
	DECLARE_SERIALIZE;

	class CArtillery *pArtillery;
	CPtr<CStaticObject> pObj;
	CPtr<CBasicGun> pGun;

	enum EAttackStates { EAS_NONE, EAS_ROTATING, EAS_FIRING };
	EAttackStates eState;
	WORD wDirToRotate;

	bool bAim;
	bool bFinish;

	void FinishState();
public:
	static IUnitState* Instance( class CArtillery *pArtillery, class CStaticObject *pObj );

	CArtilleryAttackCommonStatObjState() : pArtillery( 0 ) { }
	CArtilleryAttackCommonStatObjState( class CArtillery *pArtillery, class CStaticObject *pObj );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_ATTACK_STAT_OBJECT; }
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;

	virtual bool IsAttacksUnit() const { return false; }
	virtual class CAIUnit* GetTargetUnit() const { return 0; }
};
class CArtilleryRestState : public CMechUnitRestState
{
	OBJECT_COMPLETE_METHODS( CArtilleryRestState );
	DECLARE_SERIALIZE;

	CArtillery *pArtillery;
public:
	static IUnitState* Instance( class CArtillery *pArtillery, const CVec2 &guardPoint, const WORD wDir );
	
	CArtilleryRestState() : pArtillery( 0 ) { }
	CArtilleryRestState( class CArtillery *pArtillery, const CVec2 &guardPoint, const WORD wDir );

	virtual void Segment();
};
class CArtilleryAttackAviationState : public CSoldierAttackAviationState
{
	OBJECT_COMPLETE_METHODS( CArtilleryAttackAviationState );
	DECLARE_SERIALIZE;

	CArtillery *pArtillery;
public:
	static IUnitState* Instance( class CArtillery *pArtillery, class CAviation *pPlane );
	
	CArtilleryAttackAviationState() : pArtillery( 0 ) { }
	CArtilleryAttackAviationState( class CArtillery *pArtillery, class CAviation *pPlane );

	virtual void Segment();
};
#endif // __ARTILLERY_STATES_H__
