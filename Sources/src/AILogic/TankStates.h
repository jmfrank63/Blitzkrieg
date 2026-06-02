#ifndef __TANK_STATES_H__
#define __TANK_STATES_H__

#pragma ONCE
#include "StatesFactory.h"
#include "RectTiles.h"
#include "UnitStates.h"
#include "CLockWithUnlockPossibilities.h"
class CTankStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CTankStatesFactory );
	
	static CPtr<CTankStatesFactory> pFactory;
public:
	static IStatesFactory* Instance();
	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );

	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	friend class CStaticMembers;
};
#endif // __TANK_STATES_H__
