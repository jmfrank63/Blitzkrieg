#ifndef __IN_TRANSPORT_STATES_H__
#define __IN_TRANSPORT_STATES_H__

#pragma ONCE
#include "UnitStates.h"
#include "StatesFactory.h"
class CInTransportStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CInTransportStatesFactory );
	
	static CPtr<CInTransportStatesFactory> pFactory;
public:
	static IStatesFactory* Instance();
	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );
	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );

	friend class CStaticMembers;
};
class CSoldierRestOnBoardState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CSoldierRestOnBoardState );
	DECLARE_SERIALIZE;

	class CSoldier *pSoldier;
public:
	static IUnitState* Instance( class CSoldier *pSoldier, class CMilitaryCar* pTransport );

	CSoldierRestOnBoardState() : pSoldier( 0 ) { }
	CSoldierRestOnBoardState( class CSoldier *pSoldier, class CMilitaryCar* pTransport );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_REST_ON_BOARD; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	friend class CStaticMembers;
};
#endif // __IN_TRANSPORT_STATES_H__
