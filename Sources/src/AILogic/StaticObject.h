#ifndef __STATIC_OBJECT_H__
#define __STATIC_OBJECT_H__

#pragma ONCE
#include "LinkObject.h"
#include "RectTiles.h"
enum EStaticObjType { ESOT_COMMON, 
											ESOT_BUILDING, 
											ESOT_MINE, 
											ESOT_ENTR_PART, 
											ESOT_ENTRENCHMENT, 
											ESOT_TERRA, 
											ESOT_BRIDGE_SPAN,
											ESOT_TANKPIT,
											ESOT_FENCE,
											ESOT_SMOKE_SCREEN,
											ESOT_FLAG,
											ESOT_ARTILLERY_BULLET_STORAGE,
										};
class CStaticObject : public CLinkObject
{
	DECLARE_SERIALIZE;
public:
	virtual const SHPObjectRPGStats* GetStats() const = 0;
	
	virtual const CVec2& GetCenter() const = 0;
	virtual const CVec2 GetAttackCenter( const CVec2 &vPoint ) const = 0;
	virtual void GetBoundRect( SRect *pRect ) const = 0;
	virtual void GetCoveredTiles( CTilesSet *pTiles ) const = 0;
	virtual bool IsPointInside( const CVec2 &point ) const = 0;
	virtual const WORD GetDir() const = 0;

	virtual const float GetHitPoints() const = 0;
	virtual void SetHitPoints( const float fNewHP ) { NI_ASSERT_T(false, "wrong call,CStaticObject::SetHitPoints"); }
	virtual bool IsAlive() const { return GetHitPoints() > 0.0f; }
	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit ) = 0;
	virtual void TakeEditorDamage( const float fDamage ) = 0;

	virtual void Segment() = 0;
	virtual const NTimer::STime GetNextSegmentTime() const { return 0; }

	virtual EStaticObjType GetObjectType() const = 0;
	virtual const BYTE GetPlayer() const;
	virtual void SetPlayerForEditor( const int nPlayer ) { }
	
	virtual bool IsContainer() const = 0;
	virtual const int GetNDefenders() const = 0;
	virtual class CSoldier* GetUnit( const int n ) const = 0;
	
	virtual const bool IsVisible( const BYTE cParty ) const;
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const;
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const;

  static bool CheckStaticObject( const SObjectBaseRPGStats * pStats, const CVec2 & vPos, const int nFrameIndex );

	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const = 0;
};
interface ILoadableObject
{
	virtual void StartIterate() = 0;
	virtual void Iterate() = 0;
	virtual bool IsIterateFinished() = 0;
	virtual class CAIUnit* GetIteratedUnit() = 0;
};
class CExistingObject : public CStaticObject
{
	DECLARE_SERIALIZE;
	
	unsigned long mark;

	NTimer::STime burningEnd;
protected:
	static unsigned long globalMark;

	int dbID;
	int nFrameIndex;
	float fHP;

	virtual void SetNewPlaceWithoutMapUpdate( const CVec2 &center, const WORD dir = 0 ) = 0;
	const int GetRandArmorByDir( const int nArmorDir, const WORD wAttackDir );
public:
	CExistingObject() { }
	CExistingObject( const int _nFrameIndex, const int _dbID, const float _fHP ) 
		: dbID( _dbID ), nFrameIndex( _nFrameIndex ), fHP( _fHP ), mark( 0 ) { SetUniqueId(); }

	virtual const int GetDBID() const { return dbID; }
	const int GetFrameIndex() const { return nFrameIndex; }

	virtual void TakeEditorDamage( const float fDamage );
	const float GetHitPoints() const { return fHP; }
	virtual void Die( const float fDamage ) = 0;

	virtual void SetNewPlacement( const CVec2 &center, const WORD dir );

	virtual void LockTiles( bool bInitialization = false ) = 0;
	virtual void UnlockTiles( bool bInitialization = false ) = 0;
	virtual void SetTransparencies() = 0;
	virtual void RemoveTransparencies() = 0;
	virtual void RestoreTransparencies() = 0;
	
	virtual bool CanBeMovedTo( const CVec2 &newCenter ) const = 0;

	virtual void GetPlacement( struct SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff );
	virtual void GetNewUnitInfo( struct SNewUnitInfo *pNewUnitInfo );
	virtual void GetRPGStats( struct SAINotifyRPGStats *pStats ) = 0;

	virtual void Delete();
	void DeleteForEditor();

	virtual bool IsGlobalUpdated() const { return mark == globalMark; }
	virtual void SetGlobalUpdated() { mark = globalMark; }
	static void UpdateGlobalMark() { ++globalMark; }
	
	virtual bool ProcessCumulativeExpl( class CExplosion *pExpl, const int nArmorDir, const bool bFromExpl );
	virtual bool ProcessBurstExpl( class CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius );
	virtual bool ProcessAreaDamage( const class CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius ) { return false; }

	virtual void BurnSegment();
	virtual void WasHit();

	friend class CStaticMembers;
};
class CGivenPassabilityStObject : public CExistingObject
{
	DECLARE_SERIALIZE;
	CVec2 center;
	SRect boundRect;
	CArray2D<BYTE> lockInfo;
	BYTE lockTypes;
	bool bPartially;
	bool bTransparencySet;
	
	CArray2D<BYTE> canSetTransparency;
	// Its map Direction, passed on to the client, which draws a building's or a
	// prop of four sides' side by it (Remastered and the Map Editor,
	// NCameraTurn::Facing); 0 for every 2003 map object.
	WORD wMapDirection;
	
	const int GetVisDownX() const;
	const int GetVisUpX() const;
	const int GetVisDownY() const;
	const int GetVisUpY() const;
protected:
	virtual void SetNewPlaceWithoutMapUpdate( const CVec2 &_center, const WORD dir = 0 ) { center = _center; }
	void InitTransparenciesPossibility();
	// Takes stats turned by NStatsTurn instead of its own (a subclass that
	// holds its stats); false where it holds none to turn.
	virtual bool SetStats( const struct SObjectBaseRPGStats *pStats ) { return false; }

public:
	CGivenPassabilityStObject() : bTransparencySet( false ), wMapDirection( 0 ) { }
	CGivenPassabilityStObject( const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex );
	virtual void Init();

	virtual const CVec2& GetCenter() const { return center; }
	virtual const CVec2 GetAttackCenter( const CVec2 &vPoint ) const;
	// 0 as in 2003, unless it stands turned (NStatsTurn): then its turn, so a
	// hit's side is taken from its turned front.
	virtual const WORD GetDir() const;
	void SetMapDirection( const WORD wDirection ) { wMapDirection = wDirection; }
	// The Map Editor's turn (CAIEditor::TurnObject): the object takes the map
	// Direction and stands turned as NCameraTurn::StandingTurn says, its tiles
	// laid again; refused (nothing changes) where the turned footprint leaves
	// the map.
	void TurnForEditor( const WORD wDirection );
	virtual void GetPlacement( struct SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff );

	virtual void GetRPGStats( struct SAINotifyRPGStats *pStats );

	virtual const int GetDownX() const;
	virtual const int GetUpX() const;
	virtual const int GetDownY() const;
	virtual const int GetUpY() const;

	virtual void GetCoveredTiles( CTilesSet *pTiles ) const;
	virtual void GetBoundRect( SRect *pRect ) const { *pRect = boundRect; }
	virtual bool IsPointInside( const CVec2 &point ) const;

	virtual void LockTiles( bool bInitialization = false );
	virtual void UnlockTiles( bool bInitialization = false );
	virtual void SetTransparencies();
	virtual void RemoveTransparencies();
	virtual void RestoreTransparencies();
	virtual bool CanBeMovedTo( const CVec2 &newCenter ) const;
};
class CCommonStaticObject : public CGivenPassabilityStObject
{
	DECLARE_SERIALIZE;

	EStaticObjType eType;
public:
	CCommonStaticObject() { }
	CCommonStaticObject( const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, EStaticObjType _eType )
		: CGivenPassabilityStObject( center, dbID, fHP, nFrameIndex ), eType( _eType ) {}

	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	virtual void Die( const float fDamage );
	
	virtual void Segment() { }

	virtual EStaticObjType GetObjectType() const { return eType; }

	virtual bool IsContainer() const { return false; }
	virtual const int GetNDefenders() const { return 0; }
	virtual class CSoldier* GetUnit( const int n ) const { return 0; }

	virtual bool ProcessAreaDamage( const class CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius );
};
class CSimpleStaticObject : public CCommonStaticObject
{
	OBJECT_COMPLETE_METHODS( CSimpleStaticObject );
	DECLARE_SERIALIZE;

	CGDBPtr<SStaticObjectRPGStats> pStats;
	int nPlayer;
	bool bDelayedUpdate;									// if true then update for new object is delayed
public:
	CSimpleStaticObject() { }
	CSimpleStaticObject( const SStaticObjectRPGStats *_pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, EStaticObjType eType, const int nPlayer = -1, const bool bDelayedUpdate = false )
		: pStats( _pStats ), CCommonStaticObject( center, dbID, fHP, nFrameIndex, eType ), nPlayer( nPlayer ), bDelayedUpdate( bDelayedUpdate ) { }

	virtual const BYTE GetPlayer() const
	{
		if ( -1 == nPlayer )
			return CCommonStaticObject::GetPlayer();
		return nPlayer;
	}

	virtual const SHPObjectRPGStats* GetStats() const { return pStats; }
	virtual bool SetStats( const SObjectBaseRPGStats *_pStats ) { pStats = _pStats; return true; }
	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const;
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const;
};
class CTerraMeshStaticObject : public CCommonStaticObject
{
	OBJECT_COMPLETE_METHODS( CTerraMeshStaticObject );
	DECLARE_SERIALIZE;

	CGDBPtr<SStaticObjectRPGStats> pStats;	
	WORD wDir;
public:
	CTerraMeshStaticObject() { }
	CTerraMeshStaticObject( const SStaticObjectRPGStats *_pStats, const CVec2 &center, const WORD _wDir, const int dbID, const float fHP, const int nFrameIndex, EStaticObjType eType )
		: pStats( _pStats ), wDir( _wDir ), CCommonStaticObject( center, dbID, fHP, nFrameIndex, eType ) { }
	
	virtual const SHPObjectRPGStats* GetStats() const { return pStats; }
	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const;

	virtual const WORD GetDir() const { return wDir; }

	virtual void SetNewPlacement( const CVec2 &center, const WORD dir );
	virtual void GetPlacement( struct SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff );
};
// Remastered and the Map Editor (Main/CameraTurn.h): a map object stands
// turned by its facing - its footprint, visibility, entrances and fire slots
// turned by (x, y) -> (-y, x) as its picture is, M005's side convention and
// bk-views' turnXml. The turned stats are copies the database's own stats
// make and hold (SObjectBaseRPGStats::GetTurned).
namespace NStatsTurn
{
	// The quarter turns a map object of this description and map Direction
	// stands turned by (NCameraTurn::StandingTurn).
	int TurnOf( const struct SGDBObjectDesc *pDesc, const int nDirection );
	// pStats turned nTurn times (pStats itself at 0, and for stats of kinds
	// other than a building's or a prop's).
	const struct SObjectBaseRPGStats* Turned( const struct SObjectBaseRPGStats *pStats, const int nTurn );
	// The turn of stats Turned made (0 for the database's own) and the stats
	// it was made from.
	int TurnOfStats( const struct SHPObjectRPGStats *pStats );
	const struct SObjectBaseRPGStats* Original( const struct SHPObjectRPGStats *pStats );
};
#endif // __STATIC_OBJECT_H__
