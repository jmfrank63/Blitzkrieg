#ifndef __IN_ENTRENCHMENT_STATES_H__
#define __IN_ENTRENCHMENT_STATES_H__

#pragma ONCE
#include "UnitStates.h"
#include "StatesFactory.h"
#include "Behaviour.h"
#include "CommonStates.h"
class CAIUnit;
class CInEntrenchmentStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CInEntrenchmentStatesFactory );
	
	static CPtr<CInEntrenchmentStatesFactory> pFactory;
public:
	static IStatesFactory* Instance();
	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );
	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	
	friend class CStaticMembers;
};
class CSoldierRestInEntrenchmentState : public IUnitState, public CStandartBehaviour
{
	OBJECT_COMPLETE_METHODS( CSoldierRestInEntrenchmentState );
	DECLARE_SERIALIZE;

	class CSoldier *pSoldier;	
	NTimer::STime startTime;
public:
	static IUnitState* Instance( class CSoldier *pSoldier, class CEntrenchment *pEntrenchment );

	CSoldierRestInEntrenchmentState() : pSoldier( 0 ) { }
	CSoldierRestInEntrenchmentState( class CSoldier *pSoldier );

	void SetUnitTo( class CEntrenchment *pEntrenchment );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_REST_ENTRENCHMENT; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	friend class CStaticMembers;
};
class CSoldierAttackInEtrenchState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CSoldierAttackInEtrenchState );
	DECLARE_SERIALIZE;
	
	class CSoldier *pSoldier;
	CPtr<CAIUnit> pEnemy;

	CPtr<CBasicGun> pGun;
	bool bFinish;
	bool bAim;
	bool bSwarmAttack;
	int nEnemyParty;

	CDamageToEnemyUpdater damageToEnemyUpdater;

	void AnalyzeCurrentState();
	void FinishState();
public:
	static IUnitState* Instance( class CSoldier *pSoldier, class CAIUnit *pEnemy, const bool bSwarmAttack );

	CSoldierAttackInEtrenchState() : pSoldier( 0 ) { }
	CSoldierAttackInEtrenchState( class CSoldier *pSoldier, class CAIUnit *pEnemy, const bool bSwarmAttack );
	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;

	virtual bool IsAttacksUnit() const { return true; }
	virtual class CAIUnit* GetTargetUnit() const;
};
#endif // __IN_ENTRENCHMENT_STATES_H__
