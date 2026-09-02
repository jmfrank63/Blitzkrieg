#ifndef __AI_LOGIC_H__
#define __AI_LOGIC_H__
#include "../Formats/fmtMap.h"
#include "../AILogic/AIGeometry.h"
#include "AIClassesID.h"
#include "../StreamIO/ProgressHook.h"
#define AI_INIT_PROGRESS_STEPS 5
interface IAIEditor : public IRefCount
{
	enum { tidTypeID = AI_EDITOR };
	virtual void STDCALL Init( const struct STerrainInfo &terrainInfo ) = 0;
	virtual void STDCALL Clear() = 0;
	
	virtual bool STDCALL IsFormation( IRefCount *pObject ) const= 0;
	virtual void STDCALL GetUnitsInFormation( IRefCount *pObject, IRefCount ***pUnits, int *pnLen ) = 0;
	virtual IRefCount* STDCALL GetFormationOfUnit( IRefCount *pObject ) = 0;
	
	virtual bool STDCALL AddNewObject( const struct SMapObjectInfo &object, IRefCount **pObject ) = 0;
	virtual bool STDCALL AddNewEntrencment( IRefCount** segments, const int nLen, IRefCount **pObject ) = 0;
	virtual void STDCALL LoadEntrenchments( const std::vector<SEntrenchmentInfo> &entrenchments ) = 0;
	virtual bool STDCALL MoveObject( IRefCount *pObject, short x, short y ) = 0;
	virtual void STDCALL DeleteObject( IRefCount *pObject ) = 0;
	virtual void STDCALL DamageObject( IRefCount *pObject, const float fHP ) = 0;
	
	virtual bool STDCALL TurnObject( IRefCount *pObject, const WORD wDir ) = 0;

	virtual float STDCALL GetObjectHP( IRefCount *pObject ) = 0;
	virtual int STDCALL GetObjectScriptID( IRefCount *pObject ) = 0;
	
	virtual void STDCALL HandOutLinks() = 0;
	virtual IRefCount* STDCALL LinkToAI( const int ID ) = 0;
	virtual int STDCALL AIToLink( IRefCount *pObj ) = 0;
	
	virtual const CVec2& STDCALL GetCenter( IRefCount *pObj ) const = 0;
	virtual const WORD STDCALL GetDir( IRefCount *pObj ) const = 0;
	
	virtual const int STDCALL GetUnitDBID( IRefCount *pObj ) const = 0;
	
	virtual bool STDCALL IsObjectInsideOfMap( const struct SMapObjectInfo &object ) const = 0;
	virtual bool STDCALL CanAddObject( const struct SMapObjectInfo &object ) const = 0;
	
	virtual void STDCALL ApplyPattern( const struct SVAPattern &rPattern ) = 0;
	virtual void STDCALL UpdateAllHeights() = 0;
	
	virtual bool STDCALL ToggleShow( const int nShowType ) = 0;
	
	virtual void STDCALL UpdateTerrain( const CTRect<int> &rect, const struct STerrainInfo &terrainInfo ) = 0;

	virtual void STDCALL RecalcPassabilityForPlayer( CArray2D<BYTE> *array, const int nPlayer ) = 0;
	
	virtual void STDCALL SetPlayer( IRefCount *pObj, const int nPlayer ) = 0;
	virtual void STDCALL SetDiplomacies( const std::vector<BYTE> &playerParty ) = 0;
	
	virtual void STDCALL DeleteRiver( const SVectorStripeObject &river ) = 0;
	virtual void STDCALL AddRiver( const SVectorStripeObject &river ) = 0;
};
interface IAILogic : public IRefCount
{
	enum { tidTypeID = AI_LOGIC };

	virtual void STDCALL Suspend() = 0;
	virtual void STDCALL Resume() = 0;
	virtual bool STDCALL IsSuspended() const = 0;
	virtual void STDCALL Init(  const struct SLoadMapInfo &mapInfo, IProgressHook *pProgress = 0 ) = 0;
	virtual void STDCALL Clear() = 0;

	virtual void STDCALL UpdatePlacements( struct SAINotifyPlacement **pUnitPosBuffer, int *pnLen ) = 0;
	virtual void STDCALL UpdateActions( struct SAINotifyAction **pActionsBuffer, int *pnLen ) = 0;
	virtual void STDCALL UpdateRPGParams( struct SAINotifyRPGStats **pUnitRPGBuffer, int *pnLen ) = 0;
	virtual void STDCALL UpdateTurretTurn( struct SAINotifyTurretTurn **pTurretsBuffer, int *pnLen ) = 0;
	virtual void STDCALL UpdateEntranceStates( struct SAINotifyEntranceState **pUnits, int *pnLen ) = 0;
	
	virtual void STDCALL UpdateFeedbacks( struct SAIFeedBack **pFeedBacksBuffer, int *pnLen ) = 0;

	virtual void STDCALL UpdateStObjPlacements( struct SAINotifyPlacement **pObjPosBuffer, int *pnLen ) = 0;
	virtual void STDCALL UpdateDiplomacies( struct SAINotifyDiplomacy **pDiplomaciesBuffer, int *pnLen ) = 0;
	
	virtual void STDCALL EndUpdates() = 0;

	virtual void STDCALL UpdateShots( struct SAINotifyMechShot **pShots, int *pnLen ) = 0;	
	virtual void STDCALL UpdateShots( struct SAINotifyInfantryShot **pShots, int *pnLen ) = 0;
	virtual void STDCALL UpdateHits( struct SAINotifyHitInfo **pHits, int *pnLen ) = 0;
	virtual void STDCALL GetNewProjectiles( struct SAINotifyNewProjectile **pProjectiles, int *pnLen ) = 0;
	virtual void STDCALL GetDeadProjectiles( IRefCount ***pProjectilesBuf, int *pnLen ) = 0;

	virtual void STDCALL GetNewUnits( struct SNewUnitInfo **pNewUnitBuffer, int *pnNumNewUnits ) = 0;
	virtual void STDCALL GetNewStaticObjects( struct SNewUnitInfo **pObjects, int *pnLen ) = 0;
	virtual void STDCALL GetEntrenchments( struct SSegment2Trench **pEntrenchemnts, int *pnLen ) = 0;
	virtual void STDCALL GetFormations( struct SSoldier2Formation **pFormations, int *pnLen ) = 0;
	virtual void STDCALL GetNewBridgeSpans( struct SNewUnitInfo **pObjects, int *pnLen ) = 0;
	virtual bool STDCALL GetNewBridge( IRefCount ***pSpans, int *pnLen ) = 0;
	
	virtual void STDCALL GetDeadUnits( struct SAINotifyDeadAtAll **pDeadUnitsBuffer, int *pnLen ) = 0;
	virtual void STDCALL GetDisappearedUnits( IRefCount ***pUnitsBuffer, int *pnLen ) = 0;
	virtual void STDCALL GetDeletedStaticObjects( IRefCount ***pObjBuffer, int *pnLen ) = 0;
	virtual void STDCALL GetRevealCircles( CCircle **pCircleBuffer, int *pnLen ) = 0;

	virtual void STDCALL UnitCommand( const struct SAIUnitCmd *pCommand, const WORD wGroupID, const int nPlayer ) = 0;

	virtual void STDCALL GetVisibilities( const class CVec2 &upLeft, const class CVec2 &downLeft, 
																				const class CVec2 &downRight, const class CVec2 &upRight,
																				struct SAIVisInfo **pVisBuffer, int *pnLen ) const = 0;
	
	virtual const	WORD STDCALL GenerateGroupNumber() = 0;
	virtual void STDCALL RegisterGroup( IRefCount **pUnitsBuffer, const int nLen, const WORD wGroup ) = 0;
	virtual void STDCALL UnregisterGroup( const WORD wGroup ) = 0;
	virtual void STDCALL GroupCommand( const SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue ) = 0;
	
	virtual void STDCALL CheckDiplomacy( const IRefCount **pUnitsBuffer, BYTE **pResults, const int nLen ) = 0;
	
	virtual void STDCALL GetGlobalPassability( BYTE **pMapBuffer, int *pnLen ) = 0;
	virtual void STDCALL GetDisplayPassability( const class CVec2 &upLeft, const class CVec2 &downLeft, 
																							const class CVec2 &downRight, const class CVec2 &upRight,
																							struct SAIPassabilityInfo **pPassBuffer, int *pnLen ) = 0;

	virtual void STDCALL ShowAreas( const int nGroup, const EActionNotify eType, bool bShow )	= 0;
	
	virtual void STDCALL UpdateShootAreas( struct SShootAreas **pShootAreas, int *pnLen ) = 0;
		
	virtual void STDCALL GetMiniMapInfo( struct SMiniMapUnitInfo **pUnitsBuffer, int *pnLen ) = 0;
	virtual void STDCALL GetMiniMapInfo( BYTE **pVisBuffer, int *pnLen ) = 0;
	
	virtual void STDCALL CallScriptFunction( const char *pszCommand ) = 0;
	
	virtual int STDCALL GetUniqueIDOfObject( IRefCount *pObj ) = 0;
	virtual IRefCount* STDCALL GetObjByUniqueID( const int id ) = 0;

	virtual void STDCALL Segment() = 0;
	
	virtual void STDCALL SetMyInfo( const int nParty, const int nNumber ) = 0;
	virtual void STDCALL SetNPlayers( const int nPlayers ) = 0;
	virtual void STDCALL SetNetGame( bool bNetGame ) = 0;
	
	virtual bool SubstituteUniqueIDs( IRefCount **pUnitsBuffer, const int nLen ) = 0;
	
	virtual void STDCALL UpdateAcknowledgments( SAIAcknowledgment **pAckBuffer, int *pnLen ) = 0;
	virtual void STDCALL UpdateAcknowledgments( SAIBoredAcknowledgement **pAckBuffer, int *pnLen ) = 0;
	
	virtual float STDCALL GetZ( const CVec2 &vPoint ) const = 0;
	virtual const DWORD STDCALL GetNormal( const CVec2 &vPoint ) const = 0;
	virtual const bool STDCALL GetIntersectionWithTerrain( CVec3 *pvResult, const CVec3 &vBegin, const CVec3 &vEnd ) const = 0;
	
	virtual bool STDCALL ToggleShow( const int nShowType ) = 0;
	
	virtual bool STDCALL IsCombatSituation() = 0;
	
	virtual CVec2 STDCALL LockAvitaionAppearPoint() = 0;
	virtual void STDCALL UnlockAviationAppearPoint() = 0;
	
	virtual void STDCALL SetDifficultyLevel( const int nLevel ) = 0;
	
	virtual int STDCALL GetUniqueID( IRefCount *pObj ) = 0;
	virtual IRefCount* STDCALL GetUnitState( IRefCount *pObj ) = 0;
	virtual bool STDCALL IsFrozen( IRefCount *pObj ) const = 0;
	virtual bool STDCALL IsFrozenByState( IRefCount *pObj ) const = 0;

	virtual void STDCALL SendAcknowlegdementForced( IRefCount *pObj, const EUnitAckType eAck ) = 0;
	
	virtual void STDCALL NetGameStarted() = 0;
	virtual bool STDCALL IsNetGameStarted() const = 0;

	virtual const class CDifficultyLevel* STDCALL GetDifficultyLevel() const = 0;
	
	virtual void STDCALL NeutralizePlayer( const int nPlayer ) = 0;
	virtual void STDCALL NoWin() = 0;
	virtual bool STDCALL IsNoWin() const = 0;
	
	virtual void STDCALL GetGridUnitsCoordinates( const int nGroup, const CVec2 &vGridCenter, CVec2 **pCoord, int *pnLen ) = 0;

	// Whether the whole fence (bEntrenchment false) or entrenchment line
	// from vFrom to vTo - both in AI world coordinates - would be accepted
	// by the corresponding build command. The build-preview line's colour
	// asks this every frame: the answer is pure, creates nothing, and
	// never advances the synced random sequence.
	virtual bool STDCALL CanBuildLongObjectLine( const bool bEntrenchment, const CVec2 &vFrom, const CVec2 &vTo ) = 0;
};
#endif // __AI_LOGIC_H__
