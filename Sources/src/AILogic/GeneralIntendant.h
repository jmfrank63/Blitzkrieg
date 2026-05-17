#ifndef __GENERALINTENDANT_H__
#define __GENERALINTENDANT_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
#include "Commander.h"
#include "StaticObjects.h"
#include "EnemyRememberer.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CResupplyCellInfo : public IRefCount
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CResupplyCellInfo);

	typedef std::unordered_map< int/*Unique ID*/, BYTE > CResupplyInfo;
	CResupplyInfo resupplyInfo;
	std::vector<float> resupplyCount; 
	float fCount;													// whole weight
	BYTE cMarkedUnderSupply;							// indicates that this cell is under supply
	SVector vCell;													// coordinates of the cell ( in GetGeneralCell )
	NTimer::STime timeLastDanger;					// time when last truck was killed near this cell

	void RemoveUnitResupplyInternal( class CCommonUnit *pUnit, const enum EResupplyType eType );
public:
	CResupplyCellInfo();
	void Init( const SVector &_vPos ) { vCell = _vPos; }
	SVector GetCenter() const { return vCell; }
	void AddUnitResupply( class CCommonUnit * pUnit, const enum EResupplyType eType );
	void RemoveUnitResupply( class CCommonUnit * pUnit, const enum EResupplyType eType );
	
	byte RemoveUnit( class CCommonUnit * pUnit );
	void AddUnit( class CCommonUnit * pUnit, const byte cRes );
	const bool IsUnitRegistered( CCommonUnit * pUnit ) const;

	//for markink that this cell is under supply.
	void MarkUnderSupply( const enum EResupplyType eType, const bool bSupply = true );
	const bool IsMarkedUnderSupply( const enum EResupplyType eType ) const { return cMarkedUnderSupply & (1<<eType); }
	// dander
	void SetDanger( const NTimer::STime timeDanger ) ;
	const bool IsDangerous() const ;

	float GetNNeeded( const byte cTypeMask ) const;
	bool IsEmpty() const;
	float GetNNeeded( const enum EResupplyType eType ) const { return resupplyCount[eType]; }

	static bool IsUnitSuitable( const class CCommonUnit *pUnit, const enum EResupplyType eType );
	static void MoveUnitToCell( const class CCommonUnit *pUnit, const enum EResupplyType eType );
	static void IssueCommand( class CCommonUnit * pUnit, const enum EResupplyType eType, const CVec2 &vResupplyCenter );

	struct SSortByResupplyMaskPredicate
	{
		BYTE cMask;
		SSortByResupplyMaskPredicate( const BYTE cMask ) : cMask( cMask ) {  }
		bool operator()( const CPtr<CResupplyCellInfo> &s1, const CPtr<CResupplyCellInfo> &s2 ) const
		{ return s1->GetNNeeded( cMask ) > s2->GetNNeeded( cMask ); }
	};

	// were transport must be to cover all units with resupply.
	CVec2 CalcResupplyPos( const enum EResupplyType eType ) const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuildingStorage;
typedef std::list< CPtr<CBuildingStorage> > Storages;

class CGeneralTaskToDefendStorage : public IGeneralTask, public IWorkerEnumerator
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CGeneralTaskToDefendStorage);

public:
	class CWaitForChangePlayer : public IGeneralDelayedTask 
	{
		DECLARE_SERIALIZE;
		OBJECT_COMPLETE_METHODS(CWaitForChangePlayer);
		CPtr<CGeneralTaskToDefendStorage> pMainTask;
		CPtr<CBuildingStorage> pStorage;
		int nParty;
	public:
		CWaitForChangePlayer() {  }
		CWaitForChangePlayer( CBuildingStorage * pStorage, CGeneralTaskToDefendStorage * pMainTask, const int nParty );
		virtual bool IsTimeToRun() const;
		virtual void Run();
	};
private:
	enum ETaskState
	{
		TS_OPERATE,
		TS_START_RECAPTURE,
		TS_RECAPTURE,
		TS_FINISH_RECAPTURE,
		TS_START_REPAIR,
		TS_REPAIR,
		TS_FINISHED,
	};
	ETaskState eState;

	float fSeverity;
	int nParty;
	CPtr<CBuildingStorage> pStorage;
	CPtr<CCommonUnit> pRepairTransport;
	WORD wRequestID;

	void Recaptured();
public:
	CGeneralTaskToDefendStorage () { }
	CGeneralTaskToDefendStorage ( CBuildingStorage * pStorage, const int nParty );

	virtual ETaskName GetName() const { return ENT_DEFEND_ESTORAGE; }
	virtual void AskForWorker( ICommander *pManager, const float fMaxSeverity, const bool bInit = false );
	virtual void ReleaseWorker( ICommander *pManager, const float fMinSeverity );
	virtual void CancelTask( ICommander *pManager ) ;
	virtual void Segment() ;
	

	virtual float GetSeverity() const { return fSeverity; }
	virtual bool IsFinished() const { return eState == TS_FINISHED; } 

	//IWorkerEnumerator
	virtual bool EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType );
	virtual bool EvaluateWorker( CCommonUnit * pUnit, const enum EForceType eType ) const;
	virtual int NeedNBest( const enum EForceType eType ) const ;
	virtual float EvaluateWorkerRating( CCommonUnit * pUnit, const enum EForceType eType ) const ;

	friend class CWaitForChangePlayer;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGeneralTaskToResupplyCell : public IGeneralTask, public IWorkerEnumerator
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CGeneralTaskToResupplyCell);

	CPtr<CResupplyCellInfo> pCell;
	CVec2 vResupplyCenter;
	int nParty;
	enum EResupplyType eResupplyType;
	CPtr<CCommonUnit> pResupplyTransport;
	bool bFinished;
	float fSeverity;
	NTimer::STime timeNextCheck;
	class CGeneralIntendant *pCells;
public:
	CGeneralTaskToResupplyCell() : pCells( 0 ) { }
	CGeneralTaskToResupplyCell( CResupplyCellInfo * pCell, const int nParty, const enum EResupplyType eType, class CGeneralIntendant *pCells  );

	virtual ETaskName GetName() const { return ETN_RESUPPLYCELL; }
	virtual void AskForWorker( ICommander *pManager, const float fMaxSeverity, const bool bInit = false );
	virtual void ReleaseWorker( ICommander *pManager, const float fMinSeverity );
	virtual void CancelTask( ICommander *pManager ) ;
	virtual void Segment() ;
	

	virtual float GetSeverity() const { return fSeverity; }
	virtual bool IsFinished() const { return bFinished; }

	//IWorkerEnumerator
	virtual bool EnumWorker( CCommonUnit *pUnit, const enum EForceType eType );
	virtual bool EvaluateWorker( CCommonUnit * pUnit, const enum EForceType eType ) const;
	virtual int NeedNBest( const enum EForceType eType ) const ;
	virtual float EvaluateWorkerRating( CCommonUnit * pUnit, const enum EForceType eType ) const ;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGeneralTaskCheckCellDanger : public IGeneralDelayedTask
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CGeneralTaskCheckCellDanger );
	CPtr<IGeneralTask> pTask;
	CPtr<CResupplyCellInfo> pCell;
	CPtr<ICommander> pCommander;
	enum EResupplyType eResupplyType;
public:
	CGeneralTaskCheckCellDanger() {  }
	CGeneralTaskCheckCellDanger( interface IGeneralTask *_pTask, class CResupplyCellInfo *_pCell, enum EResupplyType _eResupplyType, interface ICommander *_pCommander )
		: pTask( _pTask ), pCell( _pCell ), eResupplyType( _eResupplyType ), pCommander( _pCommander )
	{
	}
	
	virtual bool IsTimeToRun() const
	{
		return pTask->IsFinished() || pCell->IsDangerous() || pCell->GetNNeeded( eResupplyType ) == 0;
	}
	virtual void Run()
	{
		if ( !pTask->IsFinished() )
			pTask->CancelTask( pCommander );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// manipulate with storages and resupply trucks
class CGeneralIntendant : public CCommander
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CGeneralIntendant);
public:
	struct SVectorHash
	{
		int operator()( const SVector & v ) const { return (v.x<<16) && v.y; }
	};
private:
	
	CArray2D< CPtr<CResupplyCellInfo> > cells;

	typedef std::unordered_map< SVector, CPtr<CResupplyCellInfo>, SVectorHash > ResupplyCells;
	ResupplyCells cellsWithRequests;

	// artillery without crew.
	typedef std::unordered_map< /*Unique ID*/ int, CPtr<CEnemyRememberer> > CFreeArtillery;
	CFreeArtillery freeArtillery;

	// storages (tasks to defend storages)
	CommonUnits resupplyTrucks;
	typedef std::pair<CVec2, WORD> CPosition;
	std::vector<CPosition> vPositions;
	int nCurPosition;
	bool bInitedByParcel;

	int nParty;
	class CCommander * pGeneral;

	const bool IsUnitRegistered( CCommonUnit * pUnit ) const;
	CGeneralIntendant::ResupplyCells::iterator GetCell( const CVec2 &vPos );
	void DeleteForgottenArtillery();
public:
	CGeneralIntendant() : pGeneral( 0 ) {  }
	CGeneralIntendant( const int nPlayer, CCommander * pGeneral );

	void Init();
	void AddReiforcePositions( const struct SAIGeneralParcelInfo &_patchInfo );
	void AddReiforcePosition( const CVec2 & vPos, const WORD wDirection );
	
	virtual void Give( CCommonUnit *pWorker );
	virtual float GetMeanSeverity() const { return 0; }
	virtual void EnumWorkers( const EForceType eType, IWorkerEnumerator *pEnumerator );
	virtual void Segment() ;
	
	virtual int RequestForSupport( const CVec2 &vSupportCenter, enum EForceType eType );
	virtual void CancelRequest( int nRequestID, enum EForceType eType );

	void UnitDead( class CCommonUnit * pUnit );
	void UnitChangedPosition( class CCommonUnit * pUnit, const CVec2 &vNewPos );
	void UnitAskedForResupply( class CCommonUnit * pUnit, const enum EResupplyType eType, const bool bSet );
	void SetArtilleryVisible( const CAIUnit *pArtillery, const bool bVisible );

	void MarkCellsDangerous( const SVector &vCell );		
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GENERALINTENDANT_H__
