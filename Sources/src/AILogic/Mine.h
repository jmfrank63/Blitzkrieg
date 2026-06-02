#ifndef __MINE_H__
#define __MINE_H__
#pragma ONCE
#include "StaticObject.h"
class CMineStaticObject : public CGivenPassabilityStObject
{
	OBJECT_COMPLETE_METHODS( CMineStaticObject );
	DECLARE_SERIALIZE;

	CGDBPtr<SMineRPGStats> pStats;
	int	player;

	DWORD mVisibleStatus;
	NTimer::STime nextSegmTime;

	bool bIfWillBeDeleted; // кто-то из солдат направл€етс€ к этой мине
	bool bIfRegisteredInCWorld; // мина видима

	bool bAlive;

	void Detonate();

	bool IsRegisteredInWorld() const;
public: 
	CMineStaticObject();
	CMineStaticObject( const SMineRPGStats *_pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, int player );
	virtual void Init();

	virtual const SHPObjectRPGStats* GetStats() const { return pStats; }

	virtual void Segment();
	virtual const NTimer::STime GetNextSegmentTime() const { return nextSegmTime; }

	bool WillExplodeUnder( CAIUnit *pUnit );

	bool CheckToDetonate( class CAIUnit *pUnit );
	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	virtual void Die( const float fDamage );
	virtual EStaticObjType GetObjectType() const { return ESOT_MINE; }

	virtual const bool IsVisible( const BYTE nParty ) const;
	void SetVisible( int nParty, bool bVis = true );

	bool IsBeingDisarmed() const {return bIfWillBeDeleted; }
	void SetBeingDisarmed( bool bStartDisarm );

	void RegisterInWorld();

	void ClearVisibleStatus();
	
	virtual bool IsContainer() const { return false; }
	virtual const int GetNDefenders() const { return 0; }
	virtual class CSoldier* GetUnit( const int n ) const { return 0; }
	
	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const { return true; }
	
	virtual bool IsAlive() const { return bAlive; }
};
#endif // __MINE_H__
