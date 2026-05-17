#ifndef __AI_LOGIC_INTERNAL_H__
#define __AI_LOGIC_INTERNAL_H__

#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "AILogic.h"
#include "Scripts\Scripts.h"
#include "..\zlib\zlib.h"
#include "LinkObject.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCommonUnit;
class CAIUnit;
class CBridgeSpan;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::unordered_map<CLinkObject*, SMapObjectInfo::SLinkInfo, SUniqueIdHash> LinkInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogic : public IAILogic
{
	OBJECT_NORMAL_METHODS( CAILogic );
	DECLARE_SERIALIZE;

	bool bSuspended;
	bool bFirstTime;
	
	// �����
	typedef std::list< std::list<CPtr<CBridgeSpan> > > Bridges;
	Bridges bridges;

	//
	std::list< CObj<CCommonUnit> > garbage;

	// �������
	CScripts scripts;
	CPtr<ISegmentTimer> pGameSegment;
	
	EActionNotify eTypeOfAreasToShow;
	SLoadMapInfo::TStartCommandsList startCmds;
	SLoadMapInfo::TReservePositionsList reservePositions;

	NTimer::STime nextCheckSumTime;
	NTimer::STime periodToCheckSum;
	uLong checkSum;

	bool bSegment;
	bool bNetGameStarted;
	
	typedef std::unordered_set<CGDBPtr<SMechUnitRPGStats>, SDefaultPtrHash> CAvailTrucks;
	CAvailTrucks availableTrucks;
	// ���������, �� �������� �� object ����������, ����������� � ����������� ����������
	// ���� ��, ���� ���������� � ���������� �������� (� pNewStats) � ���������� true, ���� ���������� �� ������� - ���������� false
	// ���� ��� �� �������� ����� ����������, ���������� true
	bool CheckForScenarioTruck( const SMapObjectInfo &object, IObjectsDB *pIDB, const SGDBObjectDesc *pDesc, const int nDBIndex, LinkInfo *linksInfo, const SMechUnitRPGStats **pNewStats ) const;

	// Loading
	// ����� �������������, ����� ��� ���� � ���������
	void CommonInit( const STerrainInfo &terrainInfo );

	void LoadUnits( const struct SLoadMapInfo &mapInfo, LinkInfo *linksInfo );
	void LoadScenarioUnits( const struct SLoadMapInfo &mapInfo, LinkInfo *linksInfo );
	void InitReservePositions();
	void InitStartCommands();
	void LaunchStartCommand( const SAIStartCommand &startCommand, IRefCount **pUnitsBuffer, const int nSize );
	// bSend - whether to send checksum
	void UpdateCheckSum( bool bSend );

	void LoadAvailableTrucks();
	bool CanShowVisibilities() const;
public:
	CAILogic();

	int GetScriptID( interface IUpdatableObj *pObj ) const { return scripts.GetScriptID( pObj ); }

	void ToGarbage( class CCommonUnit *pUnit );
	
	virtual void STDCALL Suspend();
	virtual void STDCALL Resume();
	virtual bool STDCALL IsSuspended() const { return bSuspended; }

	ISegmentTimer* GetGameSegment() const { return pGameSegment; }

	virtual void STDCALL Init( const struct SLoadMapInfo &mapInfo, IProgressHook * pProgress = 0 );
	virtual void STDCALL InitEditor( const struct STerrainInfo &terrainInfo );
	virtual void STDCALL Clear();

	// Note: These functions use the temp buffer
	virtual void STDCALL UpdatePlacements( SAINotifyPlacement **pObjPosBuffer, int *pnLen );
	virtual void STDCALL UpdateActions( SAINotifyAction **pActionsBuffer, int *pnLen );
	virtual void STDCALL UpdateRPGParams( SAINotifyRPGStats **pUnitRPGBuffer, int *pnLen );
	virtual void STDCALL UpdateTurretTurn( struct SAINotifyTurretTurn **pTurretsBuffer, int *pnLen );
	virtual void STDCALL UpdateEntranceStates( struct SAINotifyEntranceState **pUnits, int *pnLen );
	
	virtual void STDCALL UpdateFeedbacks( struct SAIFeedBack **pFeedBacksBuffer, int *pnLen );

	// Note: These functions use the temp buffer
	virtual void STDCALL UpdateShots( struct SAINotifyMechShot **pShots, int *pnLen );	
	virtual void STDCALL UpdateShots( struct SAINotifyInfantryShot **pShots, int *pnLen );

	virtual void STDCALL UpdateHits( struct SAINotifyHitInfo **pHits, int *pnLen );
	virtual void STDCALL GetNewProjectiles( struct SAINotifyNewProjectile **pProjectiles, int *pnLen );
	virtual void STDCALL GetDeadProjectiles( IRefCount ***pProjectilesBuf, int *pnLen );	
	
	virtual void STDCALL UpdateStObjPlacements( struct SAINotifyPlacement **pObjPosBuffer, int *pnLen );
	virtual void STDCALL UpdateDiplomacies( struct SAINotifyDiplomacy **pDiplomaciesBuffer, int *pnLen );
	
	virtual void STDCALL EndUpdates();

	// Note: This function uses the temp buffer
	virtual void STDCALL GetNewUnits( SNewUnitInfo **pNewUnitBuffer, int *pnLen );
	// Note: This function uses the temp buffer
	virtual void STDCALL GetNewStaticObjects( struct SNewUnitInfo **pObjects, int *pnLen );
	virtual void STDCALL GetEntrenchments( struct SSegment2Trench **pEntrenchemnts, int *pnLen );
	virtual void STDCALL GetFormations( struct SSoldier2Formation **pFormations, int *pnLen );
	virtual void STDCALL GetNewBridgeSpans( struct SNewUnitInfo **pObjects, int *pnLen );
	virtual bool STDCALL GetNewBridge( IRefCount ***pSpans, int *pnLen );

	// Note: This function uses the temp buffer
	virtual void STDCALL GetDeadUnits( SAINotifyDeadAtAll **pDeadUnitsBuffer, int *pnLen );
	// Note: This function uses the temp buffer
	virtual void STDCALL GetDisappearedUnits( IRefCount ***pUnitsBuffer, int *pnLen );
	// Note: This function uses the temp buffer
	virtual void STDCALL GetDeletedStaticObjects( IRefCount ***pObjBuffer, int *pnLen );
	virtual void STDCALL GetRevealCircles( CCircle **pCircleBuffer, int *pnLen );
	
	virtual void STDCALL UnitCommand( const SAIUnitCmd *pCommand, const WORD wGroupID, const int nPlayer );
	// Note: This function uses the temp buffer	
	virtual void STDCALL GetVisibilities( const class CVec2 &upLeft, const class CVec2 &downLeft, 
																				const class CVec2 &downRight, const class CVec2 &upRight,
																				struct SAIVisInfo **pVisBuffer, int *pnLen ) const;

	virtual const WORD STDCALL GenerateGroupNumber();
	virtual void STDCALL RegisterGroup( IRefCount **pUnitsBuffer, const int nLen, const WORD wGroup );
	virtual void STDCALL UnregisterGroup( const WORD wGroup );
	virtual void STDCALL GroupCommand( const SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue );
	
	virtual void STDCALL CheckDiplomacy( const IRefCount **pUnitsBuffer, BYTE **pResults, const int nLen );
	
	virtual void STDCALL GetGlobalPassability( BYTE **pMapBuffer, int *pnLen );
	virtual void STDCALL GetDisplayPassability( const class CVec2 &upLeft, const class CVec2 &downLeft, 
																							const class CVec2 &downRight, const class CVec2 &upRight,
																							SAIPassabilityInfo **pPassBuffer, int *pnLen );
	
	virtual void STDCALL ShowAreas( const int nGroup, const EActionNotify eType, bool bShow );
	virtual void STDCALL UpdateShootAreas( struct SShootAreas **pShootAreas, int *pnLen );
	
	virtual void STDCALL GetMiniMapInfo( struct SMiniMapUnitInfo **pUnitsBuffer, int *pnLen );
	virtual void STDCALL GetMiniMapInfo( BYTE **pVisBuffer, int *pnLen );

	virtual void STDCALL CallScriptFunction( const char *pszCommand );
	
	virtual int STDCALL GetUniqueIDOfObject( IRefCount *pObj );
	virtual IRefCount* STDCALL GetObjByUniqueID( const int id );

	virtual void STDCALL Segment();

	//CRAP{�� ���� ��������� ������������� � ���������
	IRefCount* AddObject( const SMapObjectInfo &object, IObjectsDB *pIDB, LinkInfo *linksInfo, bool bInitialization, bool IsEditor, const SHPObjectRPGStats *pPassedStats );
	//CRAP}�� ���� ��������� ������������� � ���������
	void InitLinks( LinkInfo &linksInfo );
	void LoadEntrenchments( const std::vector<struct SEntrenchmentInfo> &entrenchments );
	void LoadBridges( const std::vector< std::vector<int> > &bridgesInfo );

	virtual void STDCALL SetMyInfo( const int nParty, const int nNumber );
	virtual void STDCALL SetNPlayers( const int nPlayers );
	virtual void STDCALL SetNetGame( bool bNetGame );
	
	virtual bool SubstituteUniqueIDs( IRefCount **pUnitsBuffer, const int nLen );

	virtual void STDCALL UpdateAcknowledgments( SAIAcknowledgment **pAckBuffer, int *pnLen );
	virtual void STDCALL UpdateAcknowledgments( SAIBoredAcknowledgement **pAckBuffer, int *pnLen );
	
	virtual float STDCALL GetZ( const CVec2 &vPoint ) const;
	virtual const DWORD STDCALL GetNormal( const CVec2 &vPoint ) const;
	virtual const bool STDCALL GetIntersectionWithTerrain( CVec3 *pvResult, const CVec3 &vBegin, const CVec3 &vEnd ) const;
	
	virtual bool STDCALL ToggleShow( const int nShowType );

	virtual bool STDCALL IsCombatSituation();
	void InitStartCommands( const LinkInfo &linksInfo, std::unordered_map<int, int> &old2NewLinks );
	void InitReservePositions( std::unordered_map<int, int> &old2NewLinks );
	
	bool IsSegment() const { return bSegment; }
	
	virtual CVec2 STDCALL LockAvitaionAppearPoint();
	virtual void STDCALL UnlockAviationAppearPoint();
	
		// difficuly levels
	virtual void STDCALL SetDifficultyLevel( const int nLevel );
	virtual void STDCALL SetCheatDifficultyLevel( const int nCheatLevel );
	
	const bool IsFirstTime() const { return bFirstTime; }

	virtual void STDCALL SendAcknowlegdementForced( IRefCount *pObj, const EUnitAckType eAck );	

	// for debug
	virtual int STDCALL GetUniqueID( IRefCount *pObj ) 
	{ 
		if ( CLinkObject *pLinkObj = dynamic_cast<CLinkObject*>(pObj) ) 
			return pLinkObj->GetUniqueId();
		else
			return 0;
	}
	
	// ��� ���� � multiplayer: ��� ������ ����������� � ���� ����������
	virtual void STDCALL NetGameStarted();
	virtual bool STDCALL IsNetGameStarted() const;

	virtual const class CDifficultyLevel* STDCALL GetDifficultyLevel() const;
	
	virtual void STDCALL NeutralizePlayer( const int nPlayer );
	virtual void STDCALL NoWin();
	virtual bool STDCALL IsNoWin() const;

	virtual IRefCount* STDCALL GetUnitState( IRefCount *pObj );
	virtual bool STDCALL IsFrozen( IRefCount *pObj ) const;
	virtual bool STDCALL IsFrozenByState( IRefCount *pObj ) const;
	
	virtual void STDCALL GetGridUnitsCoordinates( const int nGroup, const CVec2 &vGridCenter, CVec2 **pCoord, int *pnLen );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AI_LOGIC_INTERNAL_H__
