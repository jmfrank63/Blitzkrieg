#ifndef __BUILDING_H__
#define __BUILDING_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "StaticObject.h"
#include "Heap.h"
#include "StormableObject.h"
#include "RotatingFireplacesObject.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoldier;
class CCommonUnit;
class CTurret;
class CUnitGuns;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CBuilding															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuilding : public CGivenPassabilityStObject, public ILoadableObject, public CStormableObject
{
	DECLARE_SERIALIZE;

	// ������ ���������, ����� ��� � ������. ������� ��������
	NTimer::STime startOfRest;
	// �������
	bool bAlarm;
	
	struct SHealthySort{ bool operator()( const CPtr<CSoldier> &a, const CPtr<CSoldier> &b ); };
	struct SIllSort{ bool operator()( const CPtr<CSoldier> &a, const CPtr<CSoldier> &b ); };

	struct SSwapAction
	{
		void operator()( CPtr<CSoldier> pSoldier1, CPtr<CSoldier> pSoldier2, const int nSoldier1Index, const int nSoldier2Index );
	};

	CHeap< CPtr<CSoldier>, SHealthySort, SSwapAction > medical;
	CHeap< CPtr<CSoldier>, SIllSort, SSwapAction > fire;
	CHeap< CPtr<CSoldier>, SIllSort, SSwapAction > rest;
	int nOveralPlaces;

	int nIterator;

	CCommonUnit *pLockingUnit;

	NTimer::STime nextSegmTime;
	
	std::vector< CObj<CTurret> > turrets;
	std::vector< CPtr<CUnitGuns> > guns;

	NTimer::STime lastDistibution;

	// ��� ������ �� ������ 3 �������������� fireplace
	CArray2D<int> observationPlaces;
	struct SSideInfo
	{
		// ���������� fireSlots �� �������
		int nFireSlots;
		// ���������� observation points �� �������
		int nObservationPoints;
		// ���������� ������ � observation points �������
		int nSoldiersInObservationPoints;

		SSideInfo() : nFireSlots( 0 ), nObservationPoints( 0 ), nSoldiersInObservationPoints( 0 ) { }
	};
	std::vector<SSideInfo> sides;
	// �� fire place - <����� ����� << 2> | <�������>
	std::vector<int> firePlace2Observation;
	// �� fireplace - ������ � ���
	std::vector< CPtr<CSoldier> > firePlace2Soldier;
	int nLastFreeFireSoldierChoice;

	// player ���������� �� ����������, ����������� � ������
	int nLastPlayer;

	int nScriptID;
	
	// ������ �� ����� ������� �� ������, ����� � ���� ��������� ���� ��������
	bool bShouldEscape;
	// units escaped
	bool bEscaped;
	NTimer::STime timeOfDeath;
	
	std::vector<NTimer::STime> lastLeave;

	//
	bool IsIllInFire();
	bool IsIllInRest();

	void SwapFireMed();
	void SwapRestMed();

	const BYTE GetFreeFireSlot();

	// �������� ����������� ����� � ����
	void PopFromFire();

	// ���� ��������� ����, �������� ���� ���������
	void SeatSoldierToMedicalSlot();
	// ���� ��������� ����, �������� ���� �����������
	void SeatSoldierToFireSlot();

	// ���������������� �������� (��������/������� �� medical places )
	void DistributeAll();
	// ���������������� �� ���������� �������� ( ��������/������� �� medical places )
	void DistributeNonFires();

	void SetFiringUnitProperties( class CSoldier *pUnit, const int nSlot, const int nIndex );
	void DistributeFiringSoldiers();

	void InitObservationPlaces();

	//
	void DelSoldierFromFirePlace( CSoldier *pSoldier );
	void DelSoldierFromMedicalPlace( CSoldier *pSoldier );
	void DelSoldierFromRestPlace( CSoldier *pSoldier );

	void PushSoldierToFirePlace( CSoldier *pUnit, const int nFirePlace );

	// �������� pUnit � ������ ���������� fireplace
	void PushToFire( class CSoldier *pUnit );
	void PushToMedical( class CSoldier *pUnit );
	void PushToRest( class CSoldier *pUnit );

	// ��������� ������ � ����� ����������
	void SetSoldiersToObservationPoints();
	// ����������� ����������� ������� pSoldier ��������� � ����� ����������; true - ����������, false - ���
	bool TryToPushRestSoldierToObservation( CSoldier *pSoldier );
	// ����������� ������� � ���������� ������ ��������� � ����� ����������; true - ����������, false - ���
	bool TryToPushFireSoldierToObservation( CSoldier *pSoldier );

	// ��������� �� pSoldier � ����� ����������
	bool IsSoldierInObservationPoint( CSoldier *pSoldier ) const;
	// ��������� pSoldier � observation point �� ������� nSide
	void PushSoldierToObservationPoint( CSoldier *pSoldier, const int nSide );

	// ������� ������� ����� ���������� ������� nSide
	void GetSidesObservationPoints( const int nSide, int *pnLeftPoint, int *pnRightPoint ) const;
	// ������� ����������� ����� ���������� ������� nSide
	const int GetMiddleObservationPoint( const int nSide ) const;
	// ������� ������� �� ������ �� ������� nSize
	CSoldier* GetSoldierOnSide( const int nSide );
	// true, ���� pSoldierInPoint � observation point ����� ������� �� pSoldier
	bool IsBetterChangeObservationSoldier( CSoldier *pSoldier, CSoldier *pSoldierInPoint );
	// ������� �������, ����� �������� ������� � ����� ����������,
	// ���� � ������ �� ����� ��� ����� ������, ��������� -1
	const int ChooseSideToSetSoldier( class CSoldier *pSoldier ) const;
	void CentreSoldiersInObservationPoints();
	// �������� ������ �� ���������� �����
	void ExchangeSoldiersToTurrets();

	// ���������� HP, ����� ���� ������� �� ������ ( �������, ���� �� ������ ������ )
	const float GetEscapeHitPoints() const;
	// ������� �������� ������� �� ����, pFormations - ������ ��� ��������� ��������
	void DriveOut( CSoldier *pSoldier, std::unordered_set<int> *pFormations );
	void KillAllInsiders();
protected:
	CGDBPtr<SBuildingRPGStats> pStats;

	virtual void AddSoldier( class CSoldier *pUnit );
	virtual void DelSoldier( class CSoldier *pUnit, const bool bFillEmptyFireplace );
	void SoldierDamaged( class CSoldier *pUnit );

	CBuilding() : pLockingUnit( 0 ), nextSegmTime ( 0 ), nScriptID( -1 ) { }
	CBuilding( const SBuildingRPGStats *pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex );
public:
	const int GetNFreePlaces() const;
	const int GetNOverallPlaces() const { return nOveralPlaces; }

	virtual const SHPObjectRPGStats* GetStats() const { return pStats; }
	virtual void SetHitPoints( const float fNewHP );
	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	virtual void Die( const float fDamage );

	const int GetNEntrancePoints() const { return pStats->entrances.size(); }
	const CVec2 GetEntrancePoint( const int nEntrance ) const;
	// ����� �����, ��������� ����� ���� � ����� point
	bool ChooseEntrance( class CCommonUnit *pUnit, const CVec2 &vPoint, int *pnEntrance ) const;

	void GoOutFromEntrance( const int nEntrance, class CSoldier *pUnit );
	bool IsGoodPointForRunIn( const SVector &point, const int nEntrance, const float fMinDist = 0 ) const;

	virtual void Segment();
	virtual const NTimer::STime GetNextSegmentTime() const { return nextSegmTime; }
	
	virtual EStaticObjType GetObjectType() const { return ESOT_BUILDING; }

	// ������������ �� fire slots
	virtual void StartIterate() { nIterator = 0; }
	virtual void Iterate() { if ( nIterator < fire.Size() ) ++nIterator; }
	virtual bool IsIterateFinished() { return nIterator == fire.Size(); }
	virtual class CAIUnit* GetIteratedUnit();

	virtual bool IsContainer() const { return true; }
	virtual const int GetNDefenders() const;
	virtual class CSoldier* GetUnit( const int n ) const;
	virtual const BYTE GetPlayer() const;
	
	void Lock( class CCommonUnit *pUnit );
	bool IsLocked( const int nPlayer ) const;
	void Unlock( class CCommonUnit *pUnit );

	void Alarm();
	
	const int GetNGunsInFireSlot( const int nSlot );
	CBasicGun* GetGunInFireSlot( const int nSlot, const int nGun );
	CTurret* GetTurretInFireSlot( const int nSlot );
	float GetMaxFireRangeInSlot( const int nSlot ) const;
	
	bool IsSoldierVisible( const int nParty, const CVec2 &center, bool bCamouflated, const float fCamouflage ) const;
	
	virtual bool IsSelectable() const;
	virtual const bool IsVisibleForDiplomacyUpdate();
	
	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const;
	
	// ����� �� ������ ���� � ����� �������
	virtual bool CanRotateSoldier( class CSoldier *pSoldier ) const;
	// ��������� ������� � place ������ �������� ���
	virtual void ExchangeUnitToFireplace( class CSoldier *pSoldier, int nFirePlace );
	// ���������� fireplaces
	const int GetNFirePlaces() const;
	// ������, ������� � fireplace, ���� fireplace ����, �� ���������� 0
	class CSoldier* GetSoldierInFireplace( const int nFireplace) const;
	
	virtual void SetScriptID( const int _nScriptID ) { nScriptID = _nScriptID; }

	const NTimer::STime& GetLastLeaveTime( const int nPlayer ) const { return lastLeave[nPlayer]; }
	void SetLastLeaveTime( const int nPlayer );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ������� ������
class CBuildingSimple : public CBuilding
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CBuildingSimple );
public:
	CBuildingSimple() { }
	CBuildingSimple( const SBuildingRPGStats *pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex )
	: CBuilding( pStats, center, dbID, fHP, nFrameIndex )
	{
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ������, ������� �������� ������� (������� ��� �������������)
class CBuildingStorage : public CBuilding
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CBuildingStorage );
	
	int nPlayer; // ���� ����������� ��� ������
	bool bConnected;
	NTimer::STime timeLastBuildingRepair;		// ��� ����������� �������� ���������
protected:
	virtual void AddSoldier( CSoldier *pUnit );
public:
	CBuildingStorage () : timeLastBuildingRepair( 0 ), bConnected( true ) {  }
	CBuildingStorage( const SBuildingRPGStats *pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, int player );

	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	
	virtual const BYTE GetPlayer() const;
	virtual void SetPlayerForEditor( const int _nPlayer ) { nPlayer = _nPlayer; }

	virtual void Segment();
	virtual void SetHitPoints( const float fNewHP );

	void SetConnected( bool bConnected );
	bool IsConnected() const { return bConnected; }
	void ChangePlayer( const int nPlayer );
	
	void GetNewUnitInfo( SNewUnitInfo *pNewUnitInfo );

	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BUILDING_H__
