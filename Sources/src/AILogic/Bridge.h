#ifndef __BRIDGE_H__
#define __BRIDGE_H__

#pragma ONCE
#include "StaticObject.h"
class CFullBridge;

class CBridgeSpan : public CGivenPassabilityStObject
{
	OBJECT_COMPLETE_METHODS( CBridgeSpan );
	DECLARE_SERIALIZE;

	CGDBPtr<SBridgeRPGStats> pStats;

	CArray2D<BYTE> unlockTypes;	// разлоканные типы террэйна, 0 - если нечего было разлокивать
	CObj<CFullBridge> pFullBridge;
	bool bNewBuilt;												// этот мост построили во время тгры
	bool bLocked;													// залочены ли тайл

	bool bDeletingAround;

	int nScriptID;

	void GetTilesForVisibilityInternal( CTilesSet *pTiles ) const;

	virtual const int GetDownX() const;
	virtual const int GetUpX() const;
	virtual const int GetDownY() const;
	virtual const int GetUpY() const;
public:
	CBridgeSpan() : nScriptID( -1 ) { }
	CBridgeSpan( const SBridgeRPGStats *pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex );
	
	void Build();													// построить сегмент моста, залокать как положено, послать в мир.

	const SBridgeRPGStats * GetBridgeStats() const { return pStats; }
	virtual const SHPObjectRPGStats* GetStats() const { return pStats; }

	virtual void LockTiles( bool bInitialization = false );
	virtual void UnlockTiles( bool bInitialization = false ) ;

	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	virtual void TakeEditorDamage( const float fDamage );
	virtual bool IsPointInside( const CVec2 &point ) const;
	virtual void Die( const float fDamage );
	virtual void SetHitPoints( const float fNewHP );

	
	virtual void Segment() { }

	virtual EStaticObjType GetObjectType() const { return ESOT_BRIDGE_SPAN; }
	
	virtual bool IsContainer() const { return false; }
	virtual const int GetNDefenders() const { return 0; }
	virtual class CSoldier* GetUnit( const int n ) const { return 0; }

	void SetFullBrige( CFullBridge *pFullBridge );
	CFullBridge * GetFullBridge() { return pFullBridge; }
	
	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const { return true; }
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const;
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const;
	virtual void GetCoveredTiles( CTilesSet *pTiles ) const;
	
	virtual void SetScriptID( const int _nScriptID ) { nScriptID = _nScriptID; }

	friend class CFullBridge;
};
class CFullBridge : public IRefCount
{
	OBJECT_COMPLETE_METHODS( CFullBridge );
	DECLARE_SERIALIZE;
	
	std::list<CBridgeSpan*> spans;					// построенные части моста
	std::list<CBridgeSpan*> projectedSpans;	// части моста, которые находятся в проекте

	bool bGivingDamage;
public:
	struct SSpanLock : public IRefCount
	{
		OBJECT_COMPLETE_METHODS( SSpanLock );
		DECLARE_SERIALIZE;

		CTilesSet tiles;
		std::list<BYTE> formerTiles;
		CBridgeSpan * pSpan;
	public:
		SSpanLock(): pSpan( 0 ) {  }
		SSpanLock( CBridgeSpan * pSpan, const WORD wDir );
		void Unlock();
		const CBridgeSpan * GetSpan() const { return pSpan; }
	};
private:
	typedef std::list< CPtr<SSpanLock> > LockedSpans;
	LockedSpans lockedSpans;
	int nSpans;														//full number of bridge spans

public:
	CFullBridge() : bGivingDamage( false ), nSpans( 0 ) {  }

	const float GetHPPercent() const;

	void SpanBuilt( CBridgeSpan * pSpan );

	void AddSpan( CBridgeSpan *pSpan );
	void DamageTaken( CBridgeSpan *pDamagedSpan, const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit );

	void EnumSpans( std::vector< CObj<CBridgeSpan> > *pSpans );
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const;
	const bool IsVisible( const BYTE cParty ) const;

	void LockSpan( CBridgeSpan * pSpan, const WORD wDir );
	void UnlockSpan( CBridgeSpan * pSpan );
	void UnlockAllSpans();

	bool CanTakeDamage() const;
	const int GetNSpans() const;

	friend class CBridgeCreation;
};
#endif // __BRIDGE_H__
