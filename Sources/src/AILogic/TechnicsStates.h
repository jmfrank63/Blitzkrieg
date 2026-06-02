#ifndef __TechnicsStates_h__
#define __TechnicsStates_h__

#pragma ONCE
#include "UnitStates.h"
#include "CLockWithUnlockPossibilities.h"
class CTankPitLeaveState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CTankPitLeaveState );
	DECLARE_SERIALIZE;

	enum ETankLeaveTankPitState 
	{
		TLTPS_ESTIMATING,
		TLTPS_MOVING,
	};
	ETankLeaveTankPitState eState;
	class CAIUnit * pUnit;
	NTimer::STime timeStartLeave;
public:
	static IUnitState* Instance( class CAIUnit *pTank );

	CTankPitLeaveState() { }
	CTankPitLeaveState( class CAIUnit  *pTank );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
};
class CSoldierEntrenchSelfState : public IUnitState
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CSoldierEntrenchSelfState );

	enum ESoldierHullDownState 
	{
		ESHD_ESTIMATE,
		ESHD_START_BUILD,
		ESHD_BUILD_PIT,
	};

	CTilesSet tiles;											// locked tiles under tank pit that being built
	CVec2 vHalfSize;											// half size of tank pit
	CGDBPtr<SMechUnitRPGStats> pStats;		// stats of tank pit
	int nDBIndex;													// db index of tank pit

	ESoldierHullDownState eState;
	class CAIUnit * pUnit;
	NTimer::STime timeStartBuild;
	CVec2 vTankPitCenter;

	bool CheckTrenches( const CAIUnit * pUnit, const SRect &rectToTest ) const;
	bool CheckInfantry( const CAIUnit * pUnit, const SRect &rect ) const;
public:
	static IUnitState* Instance( class CAIUnit * pUnit );
	
	CSoldierEntrenchSelfState() : pUnit( 0 ) {  }
	CSoldierEntrenchSelfState( class CAIUnit * pUnit );
	
	


	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
	
};
#endif // __TechnicsStates_h__
