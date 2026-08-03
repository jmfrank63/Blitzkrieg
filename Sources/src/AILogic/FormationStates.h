#ifndef __FORMATION_STATES_H__
#define __FORMATION_STATES_H__

#pragma ONCE
#include "UnitStates.h"
#include "StatesFactory.h"
#include "CommonStates.h"
#include "../Common/Actions.h"
#include "StaticObjects.h"
class CBuilding;
class CEntrenchment;
class CAIUnit;
class CStaticObject;
class CMilitaryCar;
class CAITransportUnit;
class CTank;
class CCommonStaticObject;
class CArtillery;
class CSoldier;
class CFormation;
class CEntrenchmentPart;
interface IEngineerFormationState : public IUnitState
{
	virtual void SetHomeTransport( class CAITransportUnit *pTransport ) = 0;
};
class CFormationStatesFactory : public IStatesFactory
{
	OBJECT_COMPLETE_METHODS( CFormationStatesFactory );
	
	static CPtr<CFormationStatesFactory> pFactory;
public:
	static CFormationStatesFactory* Instance();

	virtual interface IUnitState* ProduceState( class CQueueUnit *pUnit, class CAICommand *pCommand );
	virtual interface IUnitState* ProduceRestState( class CQueueUnit *pUnit );

	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	friend class CStaticMembers;
};
class CFormationRestState : public CCommonRestState
{
 	OBJECT_COMPLETE_METHODS( CFormationRestState );
	DECLARE_SERIALIZE;

	class CFormation *pFormation;
protected:
	virtual class CCommonUnit* GetUnit() const;
	
public:
	static IUnitState* Instance( class CFormation *pFormation, const CVec2 &guardPoint, const WORD wDir );
	
	CFormationRestState() : pFormation( 0 ) { }
	CFormationRestState( class CFormation *pFormation, const CVec2 &guardPoint, const WORD wDir );

	void Segment();
	
	virtual EUnitStateNames GetName() { return EUSN_REST; }
	virtual bool IsAttackingState() const { return false; }

	ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
};
class CFormationMoveToState : public IUnitState
{
  OBJECT_COMPLETE_METHODS( CFormationMoveToState );
	DECLARE_SERIALIZE;

	enum { TIME_OF_WAITING = 200 };
	enum EMoveToStates { EMTS_FORMATION_MOVING, EMTS_UNITS_MOVING_TO_FORMATION_POINTS };
	EMoveToStates eMoveToState;

	class CFormation *pFormation;

	NTimer::STime startTime;
	bool bWaiting;

	void FormationMovingState();
	void UnitsMovingToFormationPoints();
public:
	static IUnitState* Instance( class CFormation *pFormation, const CVec2 &point );

	CFormationMoveToState() : pFormation( 0 ) { }
	CFormationMoveToState( class CFormation *pFormation, const CVec2 &point );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }

	virtual EUnitStateNames GetName() { return EUSN_MOVE; }
};
class CFormationParaDropState : public IUnitState
{
  OBJECT_COMPLETE_METHODS( CFormationParaDropState );
	DECLARE_SERIALIZE;
	enum EParadropState
	{
		EPS_WAIT_FOR_PARADROP_BEGIN,
		EPS_WAIT_FOR_PARADROP_END,
	};
	EParadropState eState;

	class CFormation *pFormation;
public:
	static IUnitState* Instance( class CFormation *pFormation );

	CFormationParaDropState() : pFormation( 0 ) { }
	CFormationParaDropState( class CFormation *pFormation );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
	virtual EUnitStateNames GetName() { return EUSN_PARTROOP; }
};
class CFormationEnterBuildingState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationEnterBuildingState );
	DECLARE_SERIALIZE;

	enum EEnterBuildingStates { EES_START, EES_RUN_UP, EES_FINISHED, EES_WAIT_FOR_UNLOCK, EES_WAITINIG_TO_ENTER };
	EEnterBuildingStates state;

	class CFormation *pFormation;
	CPtr<CBuilding> pBuilding;
	int nEntrance;

	bool SetPathForRunUp();
	void SendUnitsToBuilding();
	bool IsNotEnoughSpace();
public:
	static IUnitState* Instance( class CFormation *pFormation, class CBuilding *pBuilding );

	CFormationEnterBuildingState() : pFormation( 0 ) { }
	CFormationEnterBuildingState( class CFormation *pFormation, class CBuilding *pBuilding );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	virtual EUnitStateNames GetName() { return EUSN_ENTER; }
};
class CFormationEnterEntrenchmentState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationEnterEntrenchmentState );
	DECLARE_SERIALIZE;

	enum EEnterState { EES_START, EES_RUN, EES_WAIT_TO_ENTER, EES_FINISHED };
	EEnterState state;

	class CFormation *pFormation;
	CPtr<CEntrenchment> pEntrenchment;

	bool IsAnyPartCloseToEntrenchment() const;
	bool SetPathForRunIn();
	void EnterToEntrenchment();
public:
	static IUnitState* Instance( class CFormation *pFormation, class CEntrenchment *pEntrenchment );

	CFormationEnterEntrenchmentState() : pFormation( 0 ) { }
	CFormationEnterEntrenchmentState( class CFormation *pFormation, class CEntrenchment *pEntrenchment );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	virtual EUnitStateNames GetName() { return EUSN_ENTER_ENTRENCHMENT; }
};
class CFormationIdleBuildingState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationIdleBuildingState );
	DECLARE_SERIALIZE;

	class CFormation *pFormation;
	CPtr<CBuilding> pBuilding;
public:
	static IUnitState* Instance( class CFormation *pFormation, class CBuilding *pBuilding );

	CFormationIdleBuildingState() : pFormation( 0 ) { }
	CFormationIdleBuildingState( class CFormation *pFormation, class CBuilding *pBuilding );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_REST_IN_BUILDING; }

	class CBuilding* GetBuilding() const { return pBuilding; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CFormationIdleEntrenchmentState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationIdleEntrenchmentState );
	DECLARE_SERIALIZE;

	class CFormation *pFormation;
	CPtr<CEntrenchment> pEntrenchment;
public:
	static IUnitState* Instance( class CFormation *pFormation, class CEntrenchment *pEntrenchment );

	CFormationIdleEntrenchmentState() : pFormation( 0 ) { }
	CFormationIdleEntrenchmentState( class CFormation *pFormation, class CEntrenchment *pEntrenchment );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_REST_ENTRENCHMENT; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	class CEntrenchment* GetEntrenchment() const { return pEntrenchment; }
};
class CFormationLeaveBuildingState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationLeaveBuildingState );
	DECLARE_SERIALIZE;

	class CFormation *pFormation;
	CPtr<CBuilding> pBuilding;
	CVec2 point;
public:
	static IUnitState* Instance( class CFormation *pFormation, class CBuilding *pBuilding, const CVec2 &point );

	CFormationLeaveBuildingState() : pFormation( 0 ) { }
	CFormationLeaveBuildingState( class CFormation *pFormation, class CBuilding *pBuilding, const CVec2 &point );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return point; }

	virtual EUnitStateNames GetName() { return EUSN_GO_OUT; }
};
class CFormationLeaveEntrenchmentState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationLeaveEntrenchmentState );
	DECLARE_SERIALIZE;

	class CFormation *pFormation;
	CPtr<CEntrenchment> pEntrenchment;
	CVec2 point;

public:
	static IUnitState* Instance( class CFormation *pFormation, class CEntrenchment *pEntrenchment, const CVec2 &point );

	CFormationLeaveEntrenchmentState() : pFormation( 0 ) { }
	CFormationLeaveEntrenchmentState( class CFormation *pFormation, class CEntrenchment *pEntrenchment, const CVec2 &point );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return point; }

	virtual EUnitStateNames GetName() { return EUSN_GO_OUT_ENTRENCHMENT; }
};
class CFormationPlaceMine : public IEngineerFormationState
{
	OBJECT_COMPLETE_METHODS( CFormationPlaceMine );
	DECLARE_SERIALIZE;

	enum EPlaceMineStates 
	{ 
		EPM_WAIT_FOR_HOMETRANSPORT,
		EPM_START, 
		EPM_MOVE, 
		EPM_WAITING 
	};
	EPlaceMineStates eState;
	CPtr<CAITransportUnit> pHomeTransport;

	class CFormation *pFormation;

	CVec2 point;
	int /*enum EMineType */ eType;
public:
	static IUnitState* Instance( class CFormation *pFormation, const CVec2 &point, const enum SMineRPGStats::EType nType );

	CFormationPlaceMine() : pFormation( 0 ) { }
	CFormationPlaceMine( class CFormation *pFormation, const CVec2 &point, const enum SMineRPGStats::EType nType );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return point; }

	virtual void SetHomeTransport( class CAITransportUnit *pTransport );
};
class CFormationClearMine : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationClearMine );
	DECLARE_SERIALIZE;

	enum EClearMineStates { EPM_START, EPM_MOVE, EPM_WAIT };
	EClearMineStates eState;

	class CFormation *pFormation;

	CVec2 point;
public:
	static IUnitState* Instance( class CFormation *pFormation, const CVec2 &point );

	CFormationClearMine() : pFormation( 0 ) { }
	CFormationClearMine( class CFormation *pFormation, const CVec2 &point );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual EUnitStateNames GetName() { return EUSN_CLEAR_MINE; }

	virtual const CVec2 GetPurposePoint() const { return point; }
};
class CFormationAttackUnitState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CFormationAttackUnitState );
	DECLARE_SERIALIZE;
	
	enum EAttackUnitStates { EPM_MOVING, EPM_WAITING };
	EAttackUnitStates eState;

	class CFormation *pFormation;
	CPtr<CAIUnit> pEnemy;
	bool bSwarmAttack;
	int nEnemyParty;

	void SetToMovingState();
	void SetToWaitingState();
public:
	static IUnitState* Instance( class CFormation *pFormation, class CAIUnit *pEnemy, const bool bSwarmAttack );

	CFormationAttackUnitState() { }
	CFormationAttackUnitState( class CFormation *pFormation, class CAIUnit *pEnemy, const bool bSwarmAttack );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;

	virtual bool IsAttacksUnit() const { return true; }
	virtual class CAIUnit* GetTargetUnit() const { return 0; }
	
	virtual EUnitStateNames GetName();
};
class CFormationAttackCommonStatObjState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CFormationAttackCommonStatObjState );
	DECLARE_SERIALIZE;
	
	enum EAttackUnitStates { EPM_START, EPM_MOVING, EPM_WAITING };
	EAttackUnitStates eState;

	class CFormation *pFormation;
	CPtr<CStaticObject> pObj;

	void SetToWaitingState();
public:
	static IUnitState* Instance( class CFormation *pFormation, class CStaticObject *pObj );

	CFormationAttackCommonStatObjState() { }
	CFormationAttackCommonStatObjState( class CFormation *pFormation, class CStaticObject *pObj );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;

	virtual bool IsAttacksUnit() const { return false; }
	virtual class CAIUnit* GetTargetUnit() const { return 0; }

	virtual EUnitStateNames GetName() { return EUSN_ATTACK_STAT_OBJECT; }
};
class CFormationRotateState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationRotateState );
	DECLARE_SERIALIZE;

	class CFormation *pFormation;

public:
	static IUnitState* Instance( class CFormation *pFormation, const WORD wDir );

	CFormationRotateState() { }
	CFormationRotateState( class CFormation *pFormation, const WORD wDir );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CFormationEnterTransportState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationEnterTransportState );
	DECLARE_SERIALIZE;

	enum { CHECK_PERIOD = 500 };
	enum EEnterTransportStates { EETS_START, EETS_MOVING, EETS_WAIT_FOR_TURRETS_RETURN, EETS_WAITING, EETS_FINISHED, EETS_WAIT_TO_UNLOCK_TRANSPORT };
	EEnterTransportStates eState;

	CFormation *pFormation;
	CPtr<CMilitaryCar> pTransport;
	NTimer::STime lastCheck;
	CVec2 lastTransportPos;
	WORD lastTransportDir;

	bool SetPathToRunUp();
	void SendUnitsToTransport();
	bool IsAllUnitsInside();
	void SetTransportToWaitState();
	bool IsAllTransportTurretsReturned() const;
public:
  static IUnitState* Instance( class CFormation *pFormation, class CMilitaryCar *pTransport );
	
	CFormationEnterTransportState() { }
	CFormationEnterTransportState( class CFormation *pFormation, class CMilitaryCar *pTransport );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_ENTER_TRANSPORT; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CFormationIdleTransportState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationIdleTransportState );
	DECLARE_SERIALIZE;

	CFormation *pFormation;
	CPtr<CMilitaryCar> pTransport;

public:
	static IUnitState* Instance( class CFormation *pFormation, class CMilitaryCar *pTransport );

	CFormationIdleTransportState() : pFormation( 0 ) { }
	CFormationIdleTransportState( class CFormation *pFormation, class CMilitaryCar *pTransport );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_REST_ON_BOARD; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CFormationEnterTransportNowState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationEnterTransportNowState );
	DECLARE_SERIALIZE;

	CFormation *pFormation;
	CPtr<CMilitaryCar> pTransport;

public:
	static IUnitState* Instance( class CFormation *pFormation, class CMilitaryCar *pTransport );

	CFormationEnterTransportNowState() : pFormation( 0 ) { }
	CFormationEnterTransportNowState( class CFormation *pFormation, class CMilitaryCar *pTransport );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CFormationEnterTransportByCheatPathState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationEnterTransportByCheatPathState );
	DECLARE_SERIALIZE;

	CFormation *pFormation;
	CPtr<CMilitaryCar> pTransport;

public:
	static IUnitState* Instance( class CFormation *pFormation, class CMilitaryCar *pTransport );

	CFormationEnterTransportByCheatPathState() : pFormation( 0 ) { }
	CFormationEnterTransportByCheatPathState( class CFormation *pFormation, class CMilitaryCar *pTransport );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CFormationServeUnitState: public IEngineerFormationState
{
	DECLARE_SERIALIZE;
public:
	class CFindUnitPredicate
	{
	public:
		virtual bool HasUnit() = 0;
		virtual bool SetUnit( class CAIUnit *pUnit, const float fMissedHP, const float fDist  ) = 0;
		virtual void SetNotEnoughRu() = 0;
		static float CalcWeight( const float fMissedHP, const float fDist )
		{ return fMissedHP * SConsts::HP_BALANCE_COEFF + 1000 / fDist;}
	};
	class CFindFirstUnitPredicate : public CFindUnitPredicate
	{
		CPtr<CAIUnit> pUnit;
		bool bNotEnoughtRu;
	public:
		CFindFirstUnitPredicate() : bNotEnoughtRu( false ) { }
		virtual bool HasUnit(){ return pUnit; }
		virtual bool SetUnit( class CAIUnit *_pUnit, const float fMissedHP, const float fDist )
			{ pUnit = _pUnit; return true; }
		virtual void SetNotEnoughRu() { bNotEnoughtRu = true; }
		bool IsNotEnoughRu() const { return bNotEnoughtRu; }
	};
	class CFindBestUnitPredicate : public CFindUnitPredicate
	{
		CPtr<CAIUnit> pUnit;
		bool bNotEnoughtRu;
		float fCurWeight;
	public:
		CFindBestUnitPredicate() : bNotEnoughtRu( false ), fCurWeight( 0.0f ) { }
		virtual bool HasUnit(){ return pUnit; }
		virtual bool SetUnit( class CAIUnit *_pUnit, const float fMissedHP, const float fDist )
		{ 
			const float fWeight = CalcWeight( fMissedHP, fDist) ;
			if ( fCurWeight < fWeight )
			{
				pUnit = _pUnit; 
				fCurWeight = fWeight;
			}
			return false; 
		}
		virtual void SetNotEnoughRu() { bNotEnoughtRu = true; }
		bool IsNotEnoughRu() const { return bNotEnoughtRu; }
		CAIUnit * GetUnit() { return pUnit; }
	};
protected:
	enum EFormationServiceUnitState
	{
		EFRUS_WAIT_FOR_HOME_TRANSPORT,
		EFRUS_FIND_UNIT_TO_SERVE,
		EFRUS_START_APPROACH,
		EFRUS_APPROACHING,
		EFRUS_START_SERVICE,
		EFRUS_SERVICING,
		EFRUS_WAIT_FOR_UNIT_TO_SERVE,
	};
	EFormationServiceUnitState eState;
	CPtr<CAITransportUnit> pHomeTransport; //��������� � �������� ������� ������� �� �������
	float fWorkAccumulator;								//���������� ������ � ���������
	float fWorkLeft;											// ������� �������� ����� � ����� �������
	CPtr<CAIUnit> pPreferredUnit;
public:
	
	CFormationServeUnitState( CAIUnit *_pPreferredUnit ) 
		: eState( EFRUS_WAIT_FOR_HOME_TRANSPORT ), 
		 fWorkLeft( 0 ), 
		 fWorkAccumulator( 0 ),
		 pPreferredUnit( _pPreferredUnit ){  }
	
	virtual void SetHomeTransport( class CAITransportUnit *pTransport );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
};
class CFormationRepairUnitState : public CFormationServeUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationRepairUnitState );
	DECLARE_SERIALIZE;
	
	CFormation *pUnit;
	CPtr<CAIUnit> pUnitInQuiestion;			//����, ������� ����� ���������
	CPtr<CTank> pTank;
	CVec2 vPointInQuestion;							//��� ����� ����

	NTimer::STime lastRepearTime;
	float fRepCost;
	bool bNearTruck;

	void Interrupt();
	
	static bool CheckUnit( CAIUnit *pU, CFormationServeUnitState::CFindUnitPredicate * pPred, const float fResurs, const CVec2 &vCenter );
public:
	class CFindFirstStorageToRepearPredicate : public CStaticObjects::IEnumStoragesPredicate
	{
		bool bHasStor;
		bool bNotEnoughRu;
		const float fMaxRu;									// ����� ����� ��������
	public:
		CFindFirstStorageToRepearPredicate( const float fMaxRu ) : fMaxRu( fMaxRu ), bNotEnoughRu( false ), bHasStor( false ) { }
		virtual bool OnlyConnected() const { return false; }
		virtual bool AddStorage( class CBuildingStorage * pStorage, const float fPathLenght );
		bool HasStorage() const { return bHasStor; }
		bool IsNotEnoughRU() const { return bNotEnoughRu; }
	};
	static void FindUnitToServe( const CVec2 &vCenter, 
																int nPlayer, 
																const float fResurs, 
																CCommonUnit * pLoaderSquad, 
																CFormationServeUnitState::CFindUnitPredicate * pPred,
																CAIUnit *_pPreferredUnit );

	static IUnitState* Instance( class CFormation *pUnit, CAIUnit *_pPreferredUnit );
	
	CFormationRepairUnitState() : CFormationServeUnitState( 0 ), pUnit( 0 ) { }
	CFormationRepairUnitState( class CFormation *pUnit, CAIUnit *_pPreferredUnit );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }

	virtual EUnitStateNames GetName() { return EUSN_REPAIR_UNIT; }
};
class CFormationResupplyUnitState : public CFormationServeUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationResupplyUnitState );
	DECLARE_SERIALIZE;
	CFormation *pUnit;
	CPtr<CAIUnit> pUnitInQuiestion;			//����, ������� ����� ���������
	CVec2 vPointInQuestion;							//��� ����� ����
	NTimer::STime lastResupplyTime;

	CPtr<CFormation> pSquadInQuestion; // ���� ����, ������� ����� ��������� - ��������, �� ��� ���
	int iCurUnitInFormation; // � ������ ������ ����������� ����� �������
	bool bSayAck;							// unit being resupplied must say ack when being resupplied
	bool bNearTruck;

	void Interrupt();
	static bool CheckUnit( CAIUnit *pU, CFormationServeUnitState::CFindUnitPredicate * pPred, const float fResurs, const CVec2 &vCenter );
public:
	static IUnitState* Instance( class CFormation *pUnit, CAIUnit *pPreferredUnit );
	
	CFormationResupplyUnitState() : CFormationServeUnitState( 0 ), pUnit( 0 ) { }
	CFormationResupplyUnitState( class CFormation *pUnit, CAIUnit *pPreferredUnit );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }

	static void FindUnitToServe( const CVec2 &vCenter, int nPlayer, const float fResurs, 
		CCommonUnit * pLoaderSquad, CFormationServeUnitState::CFindUnitPredicate * pPred, CAIUnit *_pPreferredUnit );

	virtual EUnitStateNames GetName() { return EUSN_RESUPPLY_UNIT; }
};
class CBuildingStorage;
class CFormationLoadRuState: public CFormationServeUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationLoadRuState );
	DECLARE_SERIALIZE;
	
	CFormation *pUnit;
	CPtr<CBuildingStorage> pStorage;			//�� ����� ��������� ����� �������
	NTimer::STime lastResupplyTime;
	int nEntrance;
	void Interrupt();
public:
	static IUnitState* Instance( class CFormation *pUnit, class CBuildingStorage *pStorage);
	
	CFormationLoadRuState() : CFormationServeUnitState( 0 ),pUnit( 0 ) { }
	CFormationLoadRuState( class CFormation *pUnit, class CBuildingStorage *pStorage);

	virtual void Segment();
	
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
};
class CFormationCatchTransportState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationCatchTransportState );
	DECLARE_SERIALIZE;

	enum ECatchState 
	{
		E_SENDING,
		E_CHECKING,
	};

	CFormation *pUnit;
	CPtr<CAITransportUnit> pTransportToCatch; //c��� ����� �����������

	std::list< CPtr<CSoldier> > deleted; // ��� �� �������������, ����������� � �������� �� 1 ��������.

	NTimer::STime timeLastUpdate;
	CVec2 vEnterPoint;
	float fResursPerSoldier;							// �������, ������� � ��������� ����� �������� �������
	ECatchState eState;

	void UpdatePath( CSoldier * pSold, const bool bForce = false );
	void Interrupt();
public:
	static IUnitState* Instance( class CFormation *pUnit, class CAITransportUnit *pTransport, float fResursPerSoldier );
	
	CFormationCatchTransportState() : pUnit( 0 ), fResursPerSoldier( 0 ) {  }
	CFormationCatchTransportState( class CFormation *pUnit, class CAITransportUnit *pTransport, float fResursPerSoldier );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CFormationPlaceAntitankState : public IEngineerFormationState
{
	OBJECT_COMPLETE_METHODS( CFormationPlaceAntitankState );
	DECLARE_SERIALIZE;

	enum EFormationPlaceAntitankState
	{
		FPAS_WAIT_FOR_HOMETRANSPORT,
		FPAS_ESITMATING,
		FPAS_APPROACHING,
		FPAS_APPROACHING_2,
		FPAS_START_BUILD,
		FPAS_START_BUILD_2,
		FPAS_BUILDING,
	};
	EFormationPlaceAntitankState eState;

	CPtr<CCommonStaticObject> pAntitank;
	CPtr<CAITransportUnit> pHomeTransport;
	CVec2 vDesiredPoint; //here antitank is going to be built
	float fWorkAccumulator;
	CFormation *pUnit;
	NTimer::STime timeBuild;
public:
	static IUnitState* Instance( class CFormation *pUnit, const CVec2 &vDesiredPoint );
	
	CFormationPlaceAntitankState() : pUnit( 0 ) { }
	CFormationPlaceAntitankState( class CFormation *pUnit, const CVec2 &vDesiredPoint );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return vDesiredPoint; }
	virtual void SetHomeTransport( class CAITransportUnit *pTransport );
};
class CLongObjectCreation;
class CFormationBuildLongObjectState : public IEngineerFormationState
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CFormationBuildLongObjectState );
	
	enum EFormationBuildEntrenchState
	{
		ETBS_WAITING_FOR_HOMETRANSPORT,


		FBFS_READY_TO_START,
		FBFS_APPROACHING_STARTPOINT,
		FBFS_BUILD_SEGMENT,

		FBFS_START_APPROACH_SEGMENT,
		FBFS_NEXT_SEGMENT,
		FBFS_CANNOT_BUILD_ANYMORE,
		FBFS_CHECK_FOR_UNITS_PREVENTING,
		FBFS_WAIT_FOR_UNITS,
		FBFS_APPROACH_SEGMENT,
	};
	EFormationBuildEntrenchState eState;
	CFormation *pUnit;

	NTimer::STime lastTime;
	std::list<CPtr<CAIUnit> > unitsPreventing;
	float fWorkLeft;
	CPtr<CAITransportUnit> pHomeTransport;
				
	CPtr<CLongObjectCreation> pCreation;
	float fCompletion;										// ������� ���������� ������� ��������
	void SendUnitsAway( std::list<CPtr<CAIUnit> > *pUnitsPreventing );

public:
	static IUnitState * Instance( class CFormation *pUnit, class CLongObjectCreation *pCreation  );

	CFormationBuildLongObjectState() : pUnit ( 0 ) {  }
	CFormationBuildLongObjectState( class CFormation *pUnit, class CLongObjectCreation *pCreation  );

	virtual void Segment();
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2(-1,-1); }

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual void SetHomeTransport( class CAITransportUnit *pTransport );
	virtual EUnitStateNames GetName() { return EUSN_BUILD_LONGOBJECT; }
	
};
class CFormationCaptureArtilleryState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationCaptureArtilleryState );
	DECLARE_SERIALIZE;

	enum EFormationCaptureArtilleryState
	{
		FCAS_ESTIMATING,
		FCAS_APPROACHING,
	};
	EFormationCaptureArtilleryState eState;

	class CFormation *pUnit;
	CPtr<CArtillery> pArtillery;
	std::vector< CPtr<CAIUnit> > usedSoldiers;
public:
	static IUnitState* Instance( class CFormation *_pUnit, CArtillery *pArtillery, const bool bUseFormationPart );
	
	CFormationCaptureArtilleryState() { }
	CFormationCaptureArtilleryState( class CFormation *_pUnit, CArtillery *pArtillery, const bool bUseFormationPart );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
	
	virtual EUnitStateNames GetName() { return EUSN_GUN_CAPTURE; }
};
class CFormationGunCrewState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationGunCrewState );
	DECLARE_SERIALIZE;

	struct SCrewAnimation
	{
		EActionNotify eAction;
		WORD wDirection;
		SCrewAnimation() : eAction( ACTION_NOTIFY_NONE ), wDirection( 0 ) {  }
		SCrewAnimation( EActionNotify eAction, WORD wDirection )
			:eAction( eAction ), wDirection ( wDirection )
		{
		}
	};

	struct SUnit
	{
		DECLARE_SERIALIZE;
		EActionNotify eAction;
		EActionNotify eNewAction;
		bool bForce;
		WORD wDirection;
		NTimer::STime timeNextUpdate;
	public:
		CPtr<CSoldier> pUnit;
		CVec2 vServePoint;

		void UpdateAction();
		void SetAction( const struct SCrewAnimation &rNewAnim, bool force = false );
		void ResetAnimation();
		inline bool IsAlive() const;
		SUnit();
		SUnit( class CSoldier *pUnit, const CVec2 &vServePoint, const EActionNotify eAction = ACTION_NOTIFY_IDLE );
	};
	struct SCrewMember : public SUnit
	{
		DECLARE_SERIALIZE;
	public:
		bool bOnPlace;
		
		SCrewMember();
		SCrewMember( const CVec2 &vServePoint, class CSoldier *pUnit = 0, const EActionNotify eAction = ACTION_NOTIFY_IDLE );
	};

	enum EGunServeState
	{
		EGSS_OPERATE = 0,
		EGSS_ROTATE = 1,
		EGSS_MOVE = 2,
	};
	
	enum EGunOperateSubState
	{
		EGOSS_AIM,
		EGOSS_AIM_VERTICAL,
		EGOSS_RELAX,
		EGOSS_RELOAD,
	};
	
	int nReloadPhaze;											// ������������ ��������� �� ��������� ���
	bool b360DegreesRotate;							// gun has no horisontal constraints

	EGunServeState eGunState;
	
	EGunOperateSubState eGunOperateSubState;

	std::vector< SCrewMember > crew; // ����� � ������� ������� ����� �����������

	CFormation *pUnit;
	CPtr<CArtillery> pArtillery;
	CGDBPtr<SMechUnitRPGStats> pStats;

	typedef std::list< SUnit > CFreeUnits;
	CFreeUnits freeUnits;
	NTimer::STime startTime;
	NTimer::STime timeLastUpdate;

	bool bReloadInProgress;

	float fReloadPrice; // ���� ����� �����������
	float fReloadProgress;	// ������� ��������� �����������
	bool bSegmPriorMove;

	WORD wGunTurretDir ; 
	WORD wGunBaseDir;
	WORD wTurretHorDir; //  ���������� ����������� ������
	WORD wTurretVerDir; //  ���������� ����������� ������
	int nFormationSize;
	CVec2 vGunPos;
	
	bool ClearState();

	SCrewAnimation CalcNeededAnimation( int iUnitNumber ) const;
	SCrewAnimation CalcAniamtionForMG( int iUnitNumber ) const;

	WORD CalcDirToAmmoBox( int nCrewNumber ) const;
	WORD CalcDirFromTo( int nCrewNumberFrom, int nCrewNumberTo ) const;

	void SetAllAnimation( EActionNotify action, bool force = false );
	void RecountPoints( const CVec2 &gunDir, const CVec2 &vTurretDir );
	void SendThatAreNotOnPlace( const bool bNoAnimation );
	int CheckThatAreOnPlace();
	void RefillCrew();
	void UpdateAnimations();
	void Interrupt();
	bool CanInterrupt() ;
	bool IsGunAttacking() const ;
public:
	static IUnitState* Instance( class CFormation *pUnit, CArtillery *pArtillery);
	
	CFormationGunCrewState() : pUnit( 0 ) { }
	CFormationGunCrewState( class CFormation *pUnit, CArtillery *pArtillery);

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	virtual EUnitStateNames GetName() { return EUSN_GUN_CREW_STATE; }
	
	CArtillery* GetArtillery() const { return pArtillery; }
};
class CFormationInstallMortarState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationInstallMortarState );
	DECLARE_SERIALIZE;
	
	CFormation *pUnit;
	NTimer::STime timeInstall;
	CPtr<CArtillery> pArt;
	int nStage;
public:
	static IUnitState* Instance( class CFormation *pUnit );
	CFormationInstallMortarState() : pUnit( 0 ) { }
	CFormationInstallMortarState( class CFormation *pUnit );
	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2(-1,-1);}
};
class CFormationUseSpyglassState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationUseSpyglassState );
	DECLARE_SERIALIZE;
	
	CFormation *pFormation;
public:
	static IUnitState* Instance( class CFormation *pFormation, const CVec2 &point );
	
	CFormationUseSpyglassState() : pFormation( 0 ) { }
	CFormationUseSpyglassState( class CFormation *pFormation, const CVec2 &point );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
	
	virtual EUnitStateNames GetName() { return EUSN_USE_SPYGLASS; }
};
class CFormationAttackFormationState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CFormationAttackFormationState );
	DECLARE_SERIALIZE;
	
	CFormation *pUnit;
	CPtr<CFormation> pTarget;
	bool bSwarmAttack;
	int nEnemyParty;
public:
	static IUnitState* Instance( class CFormation *pFormation, class CFormation *pTarget, const bool bSwarmAttack );
	
	CFormationAttackFormationState() : pUnit( 0 ) { }
	CFormationAttackFormationState( class CFormation *pFormation, class CFormation *pTarget, const bool bSwarmAttack );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const;

	virtual bool IsAttacksUnit() const { return true; }
	virtual class CAIUnit* GetTargetUnit() const { return 0; }
};
class CFormationParadeState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationParadeState );
	DECLARE_SERIALIZE;

	CFormation *pFormation;
	NTimer::STime startTime;
public:
	static IUnitState* Instance( class CFormation *pFormation, const int nType );

	CFormationParadeState() : pFormation( 0 ) { }
	CFormationParadeState( class CFormation *pFormation, const int nType );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;

	virtual EUnitStateNames GetName() { return EUSN_PARADE; }
};
class CFormationDisbandState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationDisbandState );
	DECLARE_SERIALIZE;

	CFormation *pFormation;
public:
	static IUnitState* Instance( class CFormation *pFormation );

	CFormationDisbandState() : pFormation( 0 ) { }
	CFormationDisbandState( class CFormation *pFormation );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CFormationFormState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationFormState );
	DECLARE_SERIALIZE;

	CFormation *pFormation;
public:
	static IUnitState* Instance( class CFormation *pFormation );

	CFormationFormState() : pFormation( 0 ) { }
	CFormationFormState( class CFormation *pFormation );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CFormationWaitToFormState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationWaitToFormState );
	DECLARE_SERIALIZE;

	CFormation *pFormation;
	CPtr<CFormation> pFormFormation;
	CPtr<CSoldier> pMainSoldier;
	bool bMain;
	NTimer::STime startTime;

	void FinishState();
	void FormFormation();
public:
	static IUnitState* Instance( class CFormation *pFormation, const float fMain, class CSoldier *pMainSoldier );

	CFormationWaitToFormState() : pFormation( 0 ) { }
	CFormationWaitToFormState( class CFormation *pFormation, const float fMain, class CSoldier *pMainSoldier );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_WAIT_TO_FORM; }
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const;
};
class CCatchFormationState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CCatchFormationState );
	DECLARE_SERIALIZE;

	enum ECatchFormationState { ECFS_NONE, ECFS_FREE, ECFS_IN_BUILDING, ECFS_IN_ENTRENCHMENT, ECFS_IN_TRANSPORT };
	ECatchFormationState eState;

	CVec2 lastFormationPos;
	CPtr<IRefCount> pLastFormationObject;

	class CFormation *pCatchingFormation;
	CPtr<CFormation> pFormation;
	NTimer::STime lastUpdateTime;

	void LeaveCurStaticObject();
	void MoveSoldierToFormation();
	void JoinToFormation();

	void SetToDisbandedState();
	void AnalyzeFreeFormation();
	void AnalyzeFormationInTransport( class CMilitaryCar *pCar );
	void AnalyzeFormationInEntrenchment( class CEntrenchment *pEntrenchment );
	void AnalyzeFormationInBuilding( class CBuilding *pBuilding );
public:
	static IUnitState* Instance( class CFormation *pCatchingFormation, class CFormation *pFormation );

	CCatchFormationState() : pCatchingFormation( 0 ) { }
	CCatchFormationState( class CFormation *pCatchingFormation, class CFormation *pFormation );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2( -1.0f, -1.0f ); }
};
class CFormationSwarmState : public IUnitAttackingState
{
	OBJECT_COMPLETE_METHODS( CFormationSwarmState );
	DECLARE_SERIALIZE;

	enum { TIME_OF_WAITING = 200 };
	enum EFormationSwarmStates { EFSS_START, EFSS_WAIT, EFSS_MOVING };
	EFormationSwarmStates state;

	class CFormation *pFormation;

	CVec2 point;
	NTimer::STime startTime;
	bool bContinue;

	void AnalyzeTargetScan();
public:
	static IUnitState* Instance( class CFormation *pFormation, const CVec2 &point, const float fContinue );

	CFormationSwarmState() : pFormation( 0 ) { }
	CFormationSwarmState( class CFormation *pFormation, const CVec2 &point, const float fContinue );

	virtual void Segment();
	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );

	virtual EUnitStateNames GetName() { return EUSN_SWARM; }
	virtual bool IsAttackingState() const { return true; }
	virtual const CVec2 GetPurposePoint() const { return point; }

	virtual bool IsAttacksUnit() const { return false; }
	virtual class CAIUnit* GetTargetUnit() const { return 0; }
};
class CFullBridge;
class CBridgeSpan;
class CFormationRepairBridgeState : public IEngineerFormationState
{
	OBJECT_COMPLETE_METHODS( CFormationRepairBridgeState );
	DECLARE_SERIALIZE;

	enum EFormationRepearBridgeState
	{
		FRBS_WAIT_FOR_HOMETRANSPORT,
		
		FRBS_START_APPROACH,
		FRBS_APPROACH,
		FRBS_REPEAR,
	};
	EFormationRepearBridgeState eState;
	class CFormation * pUnit;
	

	CPtr<CFullBridge> pBridgeToRepair;
	CPtr<CAITransportUnit> pHomeTransport ;
	std::vector< CObj<CBridgeSpan> > bridgeSpans;
	
	float fWorkLeft;											// RU that engineers have with them
	float fWorkDone;
	NTimer::STime timeLastCheck;
public:
	static IUnitState* Instance( class CFormation *pFormation, class CFullBridge *pBridge );

	CFormationRepairBridgeState() : pUnit( 0 ) { }
	CFormationRepairBridgeState ( class CFormation *pFormation, class CFullBridge *pBridge );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2(-1,-1); }

	virtual void SetHomeTransport( class CAITransportUnit *pTransport );
	virtual EUnitStateNames GetName() { return EUSN_REPAIR_BRIDGE; }

};
class CFormationRepairBuildingState : public IEngineerFormationState
{
	OBJECT_COMPLETE_METHODS( CFormationRepairBuildingState );
	DECLARE_SERIALIZE;

	enum EFormationRepairBuildingState
	{
		EFRBS_WAIT_FOR_HOME_TRANSPORT,
		EFRBS_START_APPROACH,
		EFRBS_APPROACHING,
		EFRBS_START_SERVICE,
		EFRBS_SERVICING,
	};

	EFormationRepairBuildingState eState;

	class CFormation * pUnit;
	CPtr<CAITransportUnit> pHomeTransport;
	CPtr<CBuilding> pBuilding;

	float fWorkAccumulator;
	float fWorkLeft;
	NTimer::STime lastRepearTime;
	void Interrupt();
public:
	static IUnitState* Instance( class CFormation *pFormation, class CBuilding *pBuilding );

	CFormationRepairBuildingState() : pUnit( 0 ) { }
	CFormationRepairBuildingState( class CFormation *pFormation, class CBuilding *pBuilding );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2(-1,-1); }

	virtual void SetHomeTransport( class CAITransportUnit *pTransport );
	
	static int SendToNearestEntrance( CCommonUnit *pTransport, CBuilding * pStorage );

	virtual EUnitStateNames GetName() { return EUSN_REPAIR_BUILDING; }
};
class CFormationEnterBuildingNowState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationEnterBuildingNowState );
	DECLARE_SERIALIZE;

	CFormation *pFormation;
public:
	static IUnitState* Instance( class CFormation *pFormation, class CBuilding *pBuilding );

	CFormationEnterBuildingNowState() : pFormation( 0 ) { }
	CFormationEnterBuildingNowState( class CFormation *pFormation, class CBuilding *pBuilding );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2(-1.0f,-1.0f); }
};
class CFormationEnterEntrenchmentNowState : public IUnitState
{
	OBJECT_COMPLETE_METHODS( CFormationEnterEntrenchmentNowState );
	DECLARE_SERIALIZE;

	CFormation *pFormation;
public:
	static IUnitState* Instance( class CFormation *pFormation, class CEntrenchment *pEntrenchment );

	CFormationEnterEntrenchmentNowState() : pFormation( 0 ) { }
	CFormationEnterEntrenchmentNowState( class CFormation *pFormation, class CEntrenchment *pEntrenchment );

	virtual void Segment();

	virtual ETryStateInterruptResult TryInterruptState( class CAICommand *pCommand );
	virtual bool IsAttackingState() const { return false; }
	virtual const CVec2 GetPurposePoint() const { return CVec2(-1.0f,-1.0f); }
};
#endif // __FORMATION_STATES_H__
