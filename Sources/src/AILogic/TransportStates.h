#ifndef __TRANSPORT_STATES__
#define __TRANSPORT_STATES__

#pragma ONCE
#include "UnitStates.h"
#include "StatesFactory.h"
#include "CLockWithUnlockPossibilities.h"
class CFormation;
class CAIUnit;
class CBuildingStorage;
class CArtillery;
interface IStaticPath;
class CTransportStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CTransportStatesFactory );
	
	static CPtr<CTransportStatesFactory> pFactory;
public:
	static IStatesFactory* Instance();
	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );

	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	friend class CStaticMembers;
};
class CTransportWaitPassengerState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CTransportWaitPassengerState );
	DECLARE_SERIALIZE;

	class CMilitaryCar *pTransport;
	std::list< CPtr<CFormation> > formationsToWait;
public:
	static IUnitState* Instance( class CMilitaryCar *pTransport, class CFormation *pFormation );

	CTransportWaitPassengerState() : pTransport( 0 ) { }
	CTransportWaitPassengerState( class CMilitaryCar *pTransport, class CFormation *pFormation );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_WAIT_FOR_PASSENGER; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	void AddFormationToWait( class CFormation *pFormation );
};
class CTransportLandState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CTransportLandState );
	DECLARE_SERIALIZE;

	enum ELandStates { ELS_STARTING, ELS_MOVING, ELS_LANDING };
	ELandStates state;

	class CMilitaryCar *pTransport;

	CVec2 vLandPoint;

	void LandPassenger( class CSoldier *pLandUnit );
	const SVector GetLandingPoint();
public:
	static IUnitState* Instance( class CMilitaryCar *pTransport, const CVec2 &vLandPoint );

	CTransportLandState() : pTransport( 0 ) { }
	CTransportLandState( class CMilitaryCar *pTransport, const CVec2 &vLandPoint );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_LAND; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CTransportLoadRuState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CTransportLoadRuState );
	DECLARE_SERIALIZE;
	enum ETransportLoadRuState
	{
		ETLRS_SEARCH_FOR_STORAGE,
		ETLRS_APPROACHING_STORAGE,
		ETLRS_START_LOADING_RU,
		ETLRS_LOADING_RU,
		ETLRS_WAIT_FOR_LOADERS,

		ETLRS_SUBSTATE_FINISHED,
	};

	ETransportLoadRuState eState;

	CPtr<CBuildingStorage> pStorage;
	CPtr<CFormation> pLoaderSquad; // толпа грузчиков
	class CAITransportUnit *pTransport;
	int nEntrance;
	bool bSubState;												// является ли этот стейт сабстейтом

	void CreateSquad();

	CBuildingStorage * FindNearestSource();
	void Interrupt();
public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const bool bSubState = false, CBuildingStorage *_pPreferredStorage = 0 );

	CTransportLoadRuState () : pTransport( 0 ) { }
	CTransportLoadRuState ( class CAITransportUnit *pTransport, const bool bSubState, CBuildingStorage *_pPreferredStorage );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
	
	bool IsSubStateFinished() const { return eState == ETLRS_SUBSTATE_FINISHED; }
};
class CTransportServeState : public IUnitState
{
	DECLARE_SERIALIZE;
	enum ETransportServeState
	{
		ETRS_WAIT_FOR_UNLOCK,
		ETRS_INIT,

		ETRS_START_APPROACH,
		ETRS_APPROACHING,
		ETRS_CREATE_SQUAD,
		ETRS_FINDING_UNIT_TO_SERVE,
		ETRS_LOADERS_INROUTE,
		
		ETRS_GOING_TO_STORAGE,
		
		ETRS_WAIT_FOR_LOADERS,
		ETRS_WAIT_FOR_UNIT_TO_SERVE,
	};
	
	ETransportServeState eState;
	CVec2 vServePoint; //senter of serving circle
	CPtr<CAIUnit> pResupplyUnit;	//юнит, который перезаряжают
	NTimer::STime timeLastUpdate ;//время последнего апдейта поведения.

	CPtr<IStaticPath> pStaticPath ;
	bool bWaitForPath;
	void CreateSquad();
protected:
	bool bUpdatedActionsBegin;
	CPtr<CFormation> pLoaderSquad; // толпа грузчиков
	CPtr<CAIUnit> pPreferredUnit;			// unit that is served first
	class CAITransportUnit *pTransport;

	virtual bool FindUnitToServe( bool *pIsNotEnoughRU ) = 0;

	virtual void SendLoaders() = 0;
	
	virtual void UpdateActionBegin() = 0;

public:
	CTransportServeState() : pTransport( 0 ), bUpdatedActionsBegin( false ) { }
	CTransportServeState( class CAITransportUnit *pTransport, const CVec2 &_vServePoint, CAIUnit *_pPreferredUnit );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return vServePoint; }
};
class CTransportResupplyState: public CTransportServeState
{
	OBJECT_COMPLETE_METHODS( CTransportResupplyState );
	DECLARE_SERIALIZE;

protected:
	bool FindUnitToServe( bool *pIsNotEnoughRU );
	virtual void SendLoaders();
	virtual void UpdateActionBegin();

public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const CVec2 &_vServePoint, CAIUnit *_pPreferredUnit );

	CTransportResupplyState() { }
	CTransportResupplyState( class CAITransportUnit *pTransport, const CVec2 &_vServePoint, CAIUnit *_pPreferredUnit );

	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }

	virtual EUnitStateNames GetName() { return EUSN_RESUPPLY_UNIT; }
};
class CTransportRepairState: public CTransportServeState
{
	OBJECT_COMPLETE_METHODS( CTransportRepairState );
	DECLARE_SERIALIZE;

protected:
	virtual bool FindUnitToServe( bool *pIsNotEnoughRU );
	void SendLoaders();
	void UpdateActionBegin();
public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const CVec2 &_vServePoint, CAIUnit *_pPreferredUnit );

	CTransportRepairState() { }
	CTransportRepairState( class CAITransportUnit *pTransport, const CVec2 &_vServePoint, CAIUnit *_pPreferredUnit );

	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }

	virtual EUnitStateNames GetName() { return EUSN_REPAIR_UNIT; }
};
class CTransportResupplyHumanResourcesState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CTransportResupplyHumanResourcesState );
	DECLARE_SERIALIZE;

	enum ETransportResupplyHumanResourcesState
	{
		ETSHR_GOTO_STORAGE,

		ETSHR_ESTIMATING,
		ETSHR_APPROACHNIG,
		ETSHR_SERVING,

		ETSHR_START_SERVE_SQUAD,
		ETSHR_SERVING_SQUAD,

		ETSHR_START_SERVE_ARTILLERY,
		ETSHR_SERVING_ARTILLERY,

		ETSHR_WAIT_FOR_UNITS,
	};
	ETransportResupplyHumanResourcesState eState;
	class CAITransportUnit *pTransport;
	CVec2 vServePoint; //senter of serving circle
	std::list< CPtr<CFormation> > notCompleteSquads;
	std::list< CPtr<CArtillery> > emptyArtillery;
	NTimer::STime timeLastUpdate;
	CPtr<CArtillery> pPreferredUnit;

	bool bWaitForPath;

	bool ServeArtillery( CArtillery *pArtillery );
	bool ServeSquad( CFormation *pSquad );

	void FindNotCompleteSquads( std::list< CPtr<CFormation> > *pSquads ) const;
	void FindEmptyArtillery( std::list< CPtr<CArtillery> > *pArtillerys, CArtillery *_pPreferredUnit ) const;
	bool CheckArtillery( CAIUnit *pU ) const;

public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const CVec2 &vServePoint, CArtillery *_pPreferredUnit );

	CTransportResupplyHumanResourcesState() { }
	CTransportResupplyHumanResourcesState( class CAITransportUnit *pTransport, const CVec2 &vServePoint, CArtillery *_pPreferredUnit );

	virtual void Segment();

	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
	
	virtual EUnitStateNames GetName() { return EUSN_HUMAN_RESUPPLY; }
};
class CTransportHookArtilleryState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CTransportHookArtilleryState );
	DECLARE_SERIALIZE;

	enum ETransportTakeGunState
	{
		TTGS_ESTIMATING,
		TTGS_APPROACHING,
		TTGS_START_UNINSTALL,
		TTGS_START_APPROACH_BY_MOVE_BACK, 
		TTGS_APPROACH_BY_MOVE_BACK,				
		TTGS_START_APPROACH_BY_CHEAT_PATH,
		TTGS_APPROACH_BY_CHEAT_PATH,
		TTGS_WAIT_FOR_UNINSTALL,
		TTGS_WAIT_FOR_TURN,
		TTGS_WAIT_FOR_CREW,
		TTGS_SEND_CREW_TO_TRANSPORT,
		TTGS_WAIT_FOR_LEAVE_TANKPIT,
	};
	ETransportTakeGunState eState;

	CPtr<CArtillery> pArtillery;
	class CAITransportUnit *pTransport;
	CVec2 vArtilleryPoint;
	
	NTimer::STime timeLast;

	WORD wDesiredTransportDir; // куда бдет направлен транспорт при погрузке
	bool bInterrupted;
	bool CanInterrupt();
	void InterruptBecauseOfPath();
public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, class CArtillery *pArtillery );

	CTransportHookArtilleryState() { }
	CTransportHookArtilleryState( class CAITransportUnit *pTransport, class CArtillery *pArtillery );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
	
	virtual EUnitStateNames GetName() { return EUSN_HOOK_ARTILLERY; }
};

class CTransportUnhookArtilleryState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CTransportUnhookArtilleryState );
	DECLARE_SERIALIZE;

	enum ETransportUnhookGunState
	{
		TUAS_START_APPROACH,
		TUAS_APPROACHING,
		TUAS_ESTIMATING,
		TUAS_ADVANCE_A_LITTLE,
		TUAS_MOVE_A_LITTLE,
		TUAS_MOVE_ARTILLERY_TO_THIS_POINT,
		TUAS_START_UNHOOK,
	};
	
	ETransportUnhookGunState eState;
	class CAITransportUnit *pTransport;
	CVec2 vDestPoint;
	int nAttempt; // количество попыток поставить артиллерию
	bool bInterrupted;
	bool bNow;														// unhook gun right at the current place

	bool CanPlaceUnit( const class CAIUnit * pUnit ) const;
public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const class CVec2 &vDestPoint, const bool bNow );

	CTransportUnhookArtilleryState () { }
	CTransportUnhookArtilleryState ( class CAITransportUnit *pTransport, const class CVec2 &vDestPoint, const bool bNow );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return vDestPoint; }
};
class CTransportBuildState : public IUnitState
{
	DECLARE_SERIALIZE;

	enum ETransportBuildState
	{
		ETBS_ESTIMATE,

		ETBS_WAIT_FOR_ENDPOINT,
		ETBS_END_POINT_READY,
		
		ETBS_APROACHING_BUILDPOINT,
		
		ETBS_CREATE_SQUAD,
		ETBS_WAIT_FINISH_BUILD,
		ETBS_START_APPROACH,

		ETBS_LOADING_RESOURCES,

		ETBS_WAIT_FOR_LOADERS,
	};

	ETransportBuildState eState;
	CPtr<CTransportLoadRuState> pLoadRuSubState;

protected:
	CAITransportUnit * pUnit;
	CVec2 vStartPoint;
	CVec2 vEndPoint;
	CPtr<CFormation> pEngineers;

	virtual void SendTransportToBuildPoint() = 0;
	virtual bool HaveToSendEngeneersNow()  = 0;
	virtual void SendEngineers() = 0;
	virtual bool IsEndPointNeeded() const = 0;
	virtual bool IsEnoughResources() const = 0;
	virtual bool IsWorkDone() const = 0;
	virtual bool MustSayNegative() const { return true; }
	virtual void NotifyGoToStorage() { }

	CTransportBuildState() { }
	CTransportBuildState( class CAITransportUnit *pTransport, const class CVec2 & vDestPoint );

public:
	virtual void Segment();

	void SetStartPoint( const CVec2 &_vStartPoint ) { vStartPoint = _vStartPoint; }

	virtual void SetEndPoint( const CVec2& _vEndPoint ) ;
	
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return vStartPoint; }
};
class CLongObjectCreation;
class CTransportBuildLongObjectState : public CTransportBuildState
{
	DECLARE_SERIALIZE;
protected:

	CPtr<CLongObjectCreation> pCreation;

	virtual void SendTransportToBuildPoint();
	virtual bool HaveToSendEngeneersNow() ;
	virtual bool IsEnoughResources() const;
	virtual bool IsWorkDone() const;
	void SendEngineers();

	virtual bool IsEndPointNeeded() const { return true; }

	CTransportBuildLongObjectState () {  }
	CTransportBuildLongObjectState ( class CAITransportUnit *pTransport, const class CVec2 & vDestPoint, class CLongObjectCreation *pCreation )
		: pCreation( pCreation ), CTransportBuildState( pTransport, vDestPoint ) {  }
public:

	virtual void SetEndPoint( const CVec2& _vEndPoint );
	
};
class CTransportBuildFenceState : public CTransportBuildLongObjectState 
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CTransportBuildFenceState );
public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const class CVec2 &vStartPoint );

	CTransportBuildFenceState() {  }
	CTransportBuildFenceState ( class CAITransportUnit *pTransport, const class CVec2 &vStartPoint );

	virtual EUnitStateNames GetName() { return EUSN_BUILD_FENCE; }
};
class CTransportBuildEntrenchmentState : public CTransportBuildLongObjectState 
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CTransportBuildEntrenchmentState );
public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint );

	CTransportBuildEntrenchmentState() {  }
	CTransportBuildEntrenchmentState( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint );

	virtual EUnitStateNames GetName() { return EUSN_BUILD_ENTRENCHMENT; }
};
class CTransportClearMineState : public CTransportBuildState 
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CTransportClearMineState );

	NTimer::STime timeCheckPeriod, timeLastCheck;
	bool bWorkDone;
protected:	
	virtual void SendTransportToBuildPoint();
	virtual bool HaveToSendEngeneersNow() ;
	virtual bool IsEnoughResources() const;
	virtual bool IsWorkDone() const;
	void SendEngineers();
	virtual bool MustSayNegative() const { return false; }
	virtual bool IsEndPointNeeded() const ;

public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint );

	CTransportClearMineState() {  }
	CTransportClearMineState( class CAITransportUnit *pTransport, const class CVec2 & vDestPoint );

	virtual EUnitStateNames GetName() { return EUSN_CLEAR_MINE; }
};
class CTransportPlaceMineState : public CTransportBuildState 
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CTransportPlaceMineState );

	int nNumber;
	bool bWorkDone;
	bool bTransportSent;
protected:	
	virtual void SendTransportToBuildPoint();
	virtual bool HaveToSendEngeneersNow() ;
	virtual bool IsEnoughResources() const;
	virtual bool IsWorkDone() const;
	void SendEngineers();

	virtual bool IsEndPointNeeded() const ;

public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint, const float fNumer );

	CTransportPlaceMineState() {  }
	CTransportPlaceMineState( class CAITransportUnit *pTransport, const class CVec2 & vDestPoint, const float fNumber )
		: CTransportBuildState( pTransport, vDestPoint ), nNumber( fNumber ), bWorkDone( false ), bTransportSent( false ) {  }

	virtual EUnitStateNames GetName() { return EUSN_PLACE_MINE; }
};
class CTransportPlaceAntitankState : public CTransportBuildState 
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CTransportPlaceAntitankState );

	bool bWorkFinished;
	bool bSent;
protected:	
	virtual void SendTransportToBuildPoint();
	virtual bool HaveToSendEngeneersNow() ;
	virtual bool IsEnoughResources() const;
	virtual bool IsWorkDone() const;
	void SendEngineers();
	virtual bool IsEndPointNeeded() const ;

public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint);

	CTransportPlaceAntitankState() {  }
	CTransportPlaceAntitankState( class CAITransportUnit *pTransport, const class CVec2 & vDestPoint );

	virtual EUnitStateNames GetName() { return EUSN_PLACE_ANTITANK; }
};
class CFullBridge;
class CTransportRepairBridgeState : public CTransportBuildState
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CTransportRepairBridgeState );

	CPtr<CFullBridge> pBridgeToRepair;
	bool bSentToBuildPoint;
protected:	
	virtual void SendTransportToBuildPoint();
	virtual bool HaveToSendEngeneersNow() ;
	virtual bool IsEnoughResources() const;
	virtual bool IsWorkDone() const;
	void SendEngineers();
	virtual bool IsEndPointNeeded() const ;
	virtual void NotifyGoToStorage() { bSentToBuildPoint = false; }
public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, class CFullBridge *pFullBridge );

	CTransportRepairBridgeState() {  }
	CTransportRepairBridgeState( class CAITransportUnit *pTransport, class CFullBridge *pFullBridge );
	
	virtual EUnitStateNames GetName() { return EUSN_REPAIR_BRIDGE; }
};
class CBridgeCreation;
class CTransportBuildBridgeState : public CTransportBuildState
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CTransportBuildBridgeState );

	CPtr<CFullBridge> pFullBridge;
	CPtr<CBridgeCreation> pCreation;
	bool bTransportSent;									// transport saw sent to build point
protected:
	virtual void SendTransportToBuildPoint();
	virtual bool HaveToSendEngeneersNow() ;
	virtual bool IsEnoughResources() const;
	virtual bool IsWorkDone() const;
	void SendEngineers();
	virtual bool IsEndPointNeeded() const ;
	virtual void NotifyGoToStorage() { bTransportSent = false; }
public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, class CFullBridge *pFullBridge );

	CTransportBuildBridgeState() {  }
	CTransportBuildBridgeState( class CAITransportUnit *pTransport, class CFullBridge *pFullBridge );

	virtual EUnitStateNames GetName() { return EUSN_BUILD_BRIDGE; }
};
class CBuilding;
class CTransportRepairBuildingState : public CTransportBuildState 
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CTransportRepairBuildingState );

	CPtr<CBuilding> pBuilding;
	bool bSentToBuildPoint;
protected:
	virtual void SendTransportToBuildPoint();
	virtual bool HaveToSendEngeneersNow() ;
	virtual bool IsEnoughResources() const;
	virtual bool IsWorkDone() const;
	void SendEngineers();
	virtual bool IsEndPointNeeded() const ;
	virtual void NotifyGoToStorage() { bSentToBuildPoint = false; }
public:
	static IUnitState* Instance( class CAITransportUnit *pTransport, class CBuilding *pBuilding );

	CTransportRepairBuildingState() {  }
	CTransportRepairBuildingState( class CAITransportUnit *pTransport, class CBuilding *pBuilding );

	virtual EUnitStateNames GetName() { return EUSN_REPAIR_BUILDING; }
};
class CMoveToPointNotPresize : public IUnitState
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CMoveToPointNotPresize );
	
	CAIUnit * pTransport;
	CVec2 vPurposePoint;
	float fRadius;

	void SendToPurposePoint();
public:
	static IUnitState* Instance( class CAIUnit *pTransport, const CVec2 &vGeneralCell, const float fRadius );

	CMoveToPointNotPresize() {  }
	CMoveToPointNotPresize( class CAIUnit *pTransport, const CVec2 &vGeneralCell, const float fRadius );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return vPurposePoint; }
	virtual EUnitStateNames GetName() { return EUSN_MOVE_TO_RESUPPLY_CELL; }
};
#endif // __TRANSPORT_STATES__
