#ifndef __SMOKE_SCREEN_H__
#define __SMOKE_SCREEN_H__
#pragma ONCE
#include "StaticObject.h"
class CSmokeScreen : public CExistingObject
{
	OBJECT_COMPLETE_METHODS( CSmokeScreen );
	DECLARE_SERIALIZE;

	CVec2 vCenter;
	SVector tileCenter;
	float fRadius;
	int nTransparency;
	NTimer::STime timeOfDissapear;

	NTimer::STime nextSegmTime;
	bool bTransparencySet;

	void OctupleTrace( const int x, const int y, const bool bAdd );
	void TraceToPoint( const int x, const int y, const bool bAdd );
	void Trace( const bool bAdd );
protected:
	virtual void SetNewPlaceWithoutMapUpdate( const CVec2 &center, const WORD dir = 0 ) { }
public:
	CSmokeScreen() : bTransparencySet( false ) { }
	CSmokeScreen( const CVec2 &vCenter, const float fRadius, const int nTransparency, const int nTime );
	virtual void Init();

	virtual const SHPObjectRPGStats* GetStats() const { return 0; }

	virtual void Segment();
	virtual const NTimer::STime GetNextSegmentTime() const { return nextSegmTime; }

	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit ) { }
	virtual void Die( const float fDamage ) { }
	virtual EStaticObjType GetObjectType() const { return ESOT_SMOKE_SCREEN; }

	virtual bool IsContainer() const { return false; }
	virtual const int GetNDefenders() const { return 0; }
	virtual class CSoldier* GetUnit( const int n ) const { return 0; }
	
	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const { return true; }

	virtual void LockTiles( bool bInitialization = false ) { }
	virtual void UnlockTiles( bool bInitialization = false )  { }
	virtual void SetTransparencies();
	virtual void RemoveTransparencies();
	virtual void RestoreTransparencies();

	virtual void GetCoveredTiles( CTilesSet *pTiles ) const;

	virtual void GetRPGStats( struct SAINotifyRPGStats *pStats ) {}
	virtual const CVec2& GetCenter() const { return vCenter; }
	virtual const CVec2 GetAttackCenter( const CVec2 &vPoint ) const { return vCenter; }
	virtual void GetBoundRect( SRect *pRect ) const { pRect->InitRect( GetCenter(), CVec2( 1.0f, 1.0f ), 0.0f, 0.0f ); }
	virtual bool IsPointInside( const CVec2 &point ) const { return false; }
	virtual const WORD GetDir() const { return 0; }
	virtual bool CanBeMovedTo( const CVec2 &newCenter ) const { return false; }
};
#endif // __SMOKE_SCREEN_H__
