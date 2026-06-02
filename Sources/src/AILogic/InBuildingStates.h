#ifndef __IN_BULDING_STATES_H__
#define __IN_BULDING_STATES_H__

#pragma ONCE
#include "UnitStates.h"
#include "StatesFactory.h"
#include "Behaviour.h"
#include "CommonStates.h"
class CInBuildingStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CInBuildingStatesFactory );
	
	static CPtr<CInBuildingStatesFactory> pFactory;
public:
	static IStatesFactory* Instance();

	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );
	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	
	friend class CStaticMembers;
};
class CSoldierRestInBuildingState : public IUnitState, public CStandartBehaviour
{
	OBJECT_COMPLETE_METHODS( CSoldierRestInBuildingState );
	DECLARE_SERIALIZE;

	class CSoldier *pSoldier;

	NTimer::STime startTime;
public:
	static IUnitState* Instance( class CSoldier *pSoldier, class CBuilding *pBuilding );

	CSoldierRestInBuildingState() : pSoldier( 0 ) { }
	CSoldierRestInBuildingState( class CSoldier *pSoldier );
	
	void SendUnitTo( class CBuilding *pBuilding );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_REST_IN_BUILDING; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	friend class CStaticMembers;
};
class CSoldierAttackInBuildingState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CSoldierAttackInBuildingState );
	DECLARE_SERIALIZE;
	
	class CSoldier *pSoldier;
	CPtr<CAIUnit> pEnemy;

	CPtr<CBasicGun> pGun;
	bool bFinish;
	bool bAim;
	int nEnemyParty;

	CDamageToEnemyUpdater damageToEnemyUpdater;

	void AnalyzeCurrentState();
public:
	static IUnitState* Instance( class CSoldier *pSoldier, class CAIUnit *pEnemy );

	CSoldierAttackInBuildingState() : pSoldier( 0 ) { }
	CSoldierAttackInBuildingState( class CSoldier *pSoldier, class CAIUnit *pEnemy );
	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;

	virtual bool IsAttacksUnit() const { return true; }
	virtual class CAIUnit* GetTargetUnit() const;
};
#endif // __IN_BULDING_STATES_H__
