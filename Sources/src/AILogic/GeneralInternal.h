#ifndef __GENERAL_INTERNAL__
#define __GENERAL_INTERNAL__
#include "General.h"
#include "GeneralInternalInterfaces.h"
#include "Commander.h"
#include "AIHashFuncs.h"
#include "Resistance.h"
#include "../Misc/FreeIDs.h"
#pragma ONCE
class CArtillery;
class CAIUnit;
class CFormation;
class CCommonUnit;
class CGeneralAirForce;
class CGeneralArtillery;
class CGeneralIntendant;
class CGeneral : public CCommander, public IEnemyContainer
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CGeneral);
	CFreeIds requestIDs;
	int nParty;													// general is for this player
	
	typedef std::pair< CPtr<CAIUnit>, NTimer::STime> CUnitTimeSeen;
	typedef std::unordered_map< int/* unit unique ID*/, CUnitTimeSeen > CEnemyVisibility;

	CEnemyVisibility::iterator curProcessed;	// cannot be saved, so there will be some tricks
	std::list<int> erased;

	CEnemyVisibility enemys;
	CEnemyVisibility antiAviation;

	CommonUnits infantryInTrenches;				//commander strores only unasigned units
	CommonUnits infantryFree;
	CommonUnits tanksFree;
	CommonUnits stationaryTanks;
	CommonUnits transportsFree;

	NTimer::STime timeNextUpdate;					// next update of this general
	std::unordered_set<int> mobileReinforcementGroupIDs;

	CPtr<CGeneralAirForce> pAirForce;
	CPtr<CGeneralArtillery> pGeneralArtillery;
	CPtr<CGeneralIntendant> pIntendant;

	typedef std::unordered_map< int/*request ID*/, CPtr<IGeneralTask> > RequestedTasks;
	RequestedTasks requestedTasks;
	
	CResistancesContainer resContainer;

	NTimer::STime lastBombardmentCheck;
	BYTE cBombardmentType;
	bool bSendReserves;										// send tanks to swarm

	void BombardmentSegment();
	void GiveCommandToBombardment();

	void EraseLastSeen();
public:
	CGeneral() 
		: lastBombardmentCheck( 0 ), timeNextUpdate( 0 ) 
	{ 
		curProcessed = enemys.end();
	}
	CGeneral( const int nParty ) : nParty( nParty ), lastBombardmentCheck( 0 ), timeNextUpdate( 0 ) { }

	void Init( const struct SAIGeneralSideInfo &mapInfo );
	void Init();
	void GiveNewUnits( const std::list<CCommonUnit*> &pUnits );

	bool IsMobileReinforcement( int nGroupID ) const;

	void SetUnitVisible( class CAIUnit *pUnit, const bool bVisible );
	void SetAAVisible( class CAIUnit *pUnit, const bool bVisible );

	void GiveEnemies( IEnemyEnumerator *pEnumerator );
	virtual void AddResistance( const CVec2 &vCenter, const float fRadius );
	virtual void RemoveResistance( const CVec2 &vCenter );

	virtual float GetMeanSeverity() const { return 0; }
	virtual void EnumWorkers( const EForceType eType, IWorkerEnumerator *pEnumerator );
	virtual void GiveResistances( IEnemyEnumerator *pEnmumerator );

	void Give( CCommonUnit *pWorker );

	void Segment();

	virtual void CancelRequest( int nRequestID, enum EForceType eType  );
	virtual int /*request ID*/CGeneral::RequestForSupport( const CVec2 &vSupportCenter, enum EForceType eType );

	
	void UpdateEnemyUnitInfo( class CAIUnitInfoForGeneral *pInfo,
		const NTimer::STime lastVisibleTimeDelta, const CVec2 &vLastVisiblePos,
		const NTimer::STime lastAntiArtTimeDelta, const CVec2 &vLastVisibleAntiArtCenter, const float fDistToLastVisibleAntiArt );
	void UnitDied( class CAIUnitInfoForGeneral *pInfo );
	void UnitDied( class CCommonUnit * pUnit );
	void UnitChangedParty( CAIUnit *pUnit, const int nNewParty );

	void UnitChangedPosition( class CCommonUnit * pUnit, const CVec2 &vNewPos );
	void UnitAskedForResupply( class CCommonUnit * pUnit, const EResupplyType eType, const bool bSet );

	void ReserveAviationForTimes( const std::vector<NTimer::STime> &times );

	void SetCellInUse( const int nResistanceCellNumber, bool bInUse );
	bool IsInResistanceCircle( const CVec2 &vPoint ) const;
};
#endif // __GENERAL_INTERNAL__
