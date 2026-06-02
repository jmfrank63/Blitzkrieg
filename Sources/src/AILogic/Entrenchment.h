#ifndef __ENTRENCHMENT_H__
#define __ENTRENCHMENT_H__
#pragma ONCE
#include "StaticObject.h"
#include "StormableObject.h"
#include "RotatingFireplacesObject.h"
class CSoldier;
class CEntrenchment : public CStaticObject, public ILoadableObject, public CStormableObject, public CRotatingFireplacesObject
{
	OBJECT_COMPLETE_METHODS( CEntrenchment );
	DECLARE_SERIALIZE;

	struct SFireplaceInfo
	{
		DECLARE_SERIALIZE;
		
	public:
		CVec2 center;
		CPtr<CSoldier> pUnit;
		int nFrameIndex;

		SFireplaceInfo() : nFrameIndex( -1 ) { }
		SFireplaceInfo( const CVec2 &_center, CSoldier *_pUnit, const int _nFrameIndex ) : pUnit( _pUnit ), center( _center ), nFrameIndex( _nFrameIndex ) { }
	};

	struct SInsiderInfo
	{
		DECLARE_SERIALIZE;
		public:

		CPtr<CSoldier> pUnit;
		int nFireplace;

		SInsiderInfo() { }
		SInsiderInfo( CSoldier *_pUnit, const int _nFireplace ) : pUnit( _pUnit ), nFireplace( _nFireplace ) { }
	};

	SRect rect;
	int z;

	int nBusyFireplaces;
	std::vector<SFireplaceInfo> fireplaces;	
	
	std::list<SInsiderInfo> insiders;
	std::list<SInsiderInfo>::iterator iter;

	CGDBPtr<SEntrenchmentRPGStats> pStats;

	NTimer::STime nextSegmTime;

	static CVec2 GetShift( const CVec2 &vPoint, const CVec2 &vDir );
	void ProcessEmptyFireplace( const int nFireplace );
protected:
	virtual void AddSoldier( CSoldier *pUnit );
	void AddSoldierToFirePlace( CSoldier *pUnit, const int nFirePlace );
	virtual void DelSoldier( CSoldier *pUnit, const bool bFillEmptyFireplace );
	virtual void SoldierDamaged( class CSoldier *pUnit ) { }
public:
	CEntrenchment() { }
	CEntrenchment( IRefCount** segments, const int nLen, class CFullEntrenchment *pFullEntrenchment );

	const SHPObjectRPGStats* GetStats() const { return pStats; }

	virtual const CVec2& GetCenter() const { return rect.center; }
	virtual const CVec2 GetAttackCenter( const CVec2 &vPoint ) const { return rect.center; }
	virtual void GetCoveredTiles( CTilesSet *pTiles ) const;
	virtual void GetBoundRect( SRect *pRect ) const { *pRect = rect; }
	virtual bool IsPointInside( const CVec2 &point ) const { return rect.IsPointInside( point ); }
	virtual const WORD GetDir() const { return GetDirectionByVector( rect.dir ); }

	virtual const float GetHitPoints() const { return pStats->fMaxHP; }
	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	virtual void TakeEditorDamage( const float fDamage ) { }

	virtual void Segment();
	virtual const NTimer::STime GetNextSegmentTime() const { return nextSegmTime; }
	
	virtual EStaticObjType GetObjectType() const { return ESOT_ENTRENCHMENT; }
	
	virtual void StartIterate() { iter = insiders.begin(); }
	virtual void Iterate();
	virtual bool IsIterateFinished() { return iter == insiders.end(); }
	virtual class CAIUnit* GetIteratedUnit();

	virtual bool IsContainer() const { return true; }
	virtual const int GetNDefenders() const;
	virtual class CSoldier* GetUnit( const int n ) const;
	virtual const BYTE GetPlayer() const;

	void Delete();
	
	void GetClosestPoint( const CVec2 &vPoint, CVec2 *pvResult ) const;
	virtual const bool IsVisibleForDiplomacyUpdate() { return IsAnyInsiderVisible(); }
	
	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const { return true; }
	
	virtual bool CanRotateSoldier( class CSoldier *pSoldier ) const;
	virtual void ExchangeUnitToFireplace( class CSoldier *pSoldier, int nFirePlace );
	const int GetNFirePlaces() const;
	class CSoldier* GetSoldierInFireplace( const int nFireplace) const;
	const CVec2 GetFirePlaceCoord( const int nFirePlace );
};
class CFullEntrenchment;
class CEntrenchmentPart : public CExistingObject
{
	OBJECT_COMPLETE_METHODS( CEntrenchmentPart );
	DECLARE_SERIALIZE;

	CGDBPtr<SEntrenchmentRPGStats> pStats;
	CPtr<CEntrenchment> pOwner; // окоп, которому он принадлежит

	CVec2 center;
	WORD dir;

	SRect boundRect;

	bool bVisible;
	CTilesSet coveredTiles;	
	CObj<CFullEntrenchment> pFullEntrenchment;

	NTimer::STime nextSegmTime;
	
	bool CanUnregister() const;
	static CVec2 GetShift( const CVec2 &vPoint, const CVec2 &vDir );
protected:
	virtual void SetNewPlaceWithoutMapUpdate( const CVec2 &_center, const WORD _dir = 0 );
public:
	CEntrenchmentPart() { }
	void Init();
	CEntrenchmentPart( const SEntrenchmentRPGStats *pStats, const CVec2& center, const WORD dir, const int nFrameIndex, const int dbID, float fHP );
	static SRect CalcBoundRect( const CVec2 & center, const WORD _dir, const SEntrenchmentRPGStats::SSegmentRPGStats& stats);

	const SEntrenchmentRPGStats::SSegmentRPGStats& GetSegmStats() const { return pStats->segments[nFrameIndex]; }
	const SEntrenchmentRPGStats::EEntrenchSegmType GetType() const { return pStats->segments[nFrameIndex].eType; }
	CEntrenchment *GetOwner() const { return pOwner; }
	void SetOwner( CEntrenchment *_pOwner ) { pOwner = _pOwner; }

	virtual const CVec2& GetCenter() const { return center; }
	virtual const CVec2 GetAttackCenter( const CVec2 &vPoint ) const { return boundRect.center; }
	virtual const WORD GetDir() const { return dir; }

	virtual void GetRPGStats( struct SAINotifyRPGStats *pStats );

	void GetBoundRect( SRect *pRect ) const { *pRect = boundRect; }
	virtual bool IsPointInside( const CVec2 &point ) const;
	virtual void GetCoveredTiles( CTilesSet *pTiles ) const;

	virtual void Segment();

	virtual void LockTiles( bool bInitialization = false ) { }
	virtual void UnlockTiles( bool bInitialization = false )  { }
	virtual void SetTransparencies() { }
	virtual void RemoveTransparencies() { }
	virtual void RestoreTransparencies() { }
	virtual bool CanBeMovedTo( const CVec2 &newCenter ) const { return true; }

	virtual const SHPObjectRPGStats* GetStats() const { return pStats; }

	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	virtual void Die( const float fDamage ) { }
	
	virtual EStaticObjType GetObjectType() const { return ESOT_ENTR_PART; }

	virtual bool IsContainer() const { return true; }
	virtual const int GetNDefenders() const { return pOwner->GetNDefenders(); }
	virtual class CSoldier* GetUnit( const int n ) const { return pOwner->GetUnit( n ); }

	void SetVisible();
	void SetFullEntrench( class CFullEntrenchment *_pFullEntrenchment ) { pFullEntrenchment = _pFullEntrenchment; }

	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const { return true; }
};
class CFullEntrenchment : public IRefCount
{
	OBJECT_COMPLETE_METHODS( CFullEntrenchment );
	DECLARE_SERIALIZE;
	
	std::list<CEntrenchmentPart*> entrenchParts;
public:
	CFullEntrenchment() { }
	
	void AddEntrenchmentPart( class CEntrenchmentPart *pEntrenchmentPart );
	void SetVisible();
};
class CEntrenchmentTankPit : public CGivenPassabilityStObject
{
	OBJECT_COMPLETE_METHODS( CEntrenchmentTankPit );
	DECLARE_SERIALIZE;

	CGDBPtr<SMechUnitRPGStats> pStats;
	WORD wDir;
	
	CVec2 vHalfSize; 
	SRect boundRect;
	CTilesSet tilesToLock;

	CAIUnit *pOwner;
protected:
	virtual void SetNewPlaceWithoutMapUpdate( const CVec2 &_center, const WORD _dir = 0 ) { }
public:
	CEntrenchmentTankPit() { }
	CEntrenchmentTankPit( const SMechUnitRPGStats *pStats, const CVec2& center, const WORD dir,const int nFrameIndex, const int dbID, const class CVec2 &vResizeFactor, const CTilesSet &tilesToLock, class CAIUnit *_pOwner );

	virtual const WORD GetDir() const { return wDir; }

	virtual void GetRPGStats( struct SAINotifyRPGStats *pStats ) { }
	void GetBoundRect( SRect *pRect ) const { *pRect = boundRect; }
	virtual bool IsPointInside( const CVec2 &point ) const { return false; }
	virtual void GetCoveredTiles( CTilesSet *pTiles ) const ;
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const;
	virtual void Segment() { }

	virtual void LockTiles( bool bInitialization = false );
	virtual void UnlockTiles( bool bInitialization = false ) ;
	virtual void SetTransparencies() { }
	virtual void RemoveTransparencies() { }
	virtual bool CanBeMovedTo( const CVec2 &newCenter ) const { return true; }
	virtual const SHPObjectRPGStats* GetStats() const { return pStats; }

	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	virtual void Die( const float fDamage );

	virtual EStaticObjType GetObjectType() const { return ESOT_TANKPIT; }
	
	virtual void GetNewUnitInfo( struct SNewUnitInfo *pNewUnitInfo );

	virtual bool IsContainer() const { return false; }
	virtual const int GetNDefenders() const { return 0; }
	virtual class CSoldier* GetUnit( const int n ) const { return 0; }

	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const { return false; }
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const;
};
#endif // __ENTRENCHMENT_H__
