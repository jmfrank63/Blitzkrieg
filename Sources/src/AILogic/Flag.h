#ifndef __FLAG_H__
#define __FLAG_H__
#pragma ONCE
#include "StaticObject.h"
class CFlag : public CCommonStaticObject
{
	OBJECT_COMPLETE_METHODS( CFlag );
	DECLARE_SERIALIZE;

	CGDBPtr<SStaticObjectRPGStats> pStats;

	int nParty;
	NTimer::STime nextSegmentTime;

	bool bGoingToCapture;
	int nPartyToCapture;
	int nPlayerToCapture;
	NTimer::STime timeOfStartCapturing;
	NTimer::STime lastSegmentTime;

	NTimer::STime timeOfStartNeutralPartyCapturing;
	bool bCapturingByNeutral;
public:
	CFlag() { }
	CFlag( const SStaticObjectRPGStats *pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, const int nPlayer, const EStaticObjType eType );

	virtual void Init();

	virtual void TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit ) { }
	virtual bool ProcessAreaDamage( const class CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius ) { return true; }
	
	virtual void Die( const float fDamage ) { }
	
	virtual void Segment();

	virtual const NTimer::STime GetNextSegmentTime() const { return nextSegmentTime; }

	virtual const BYTE GetPlayer() const;
	virtual const SHPObjectRPGStats* GetStats() const { return pStats; }
	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const { return true; }
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const { return false; }
};
#endif //__FLAG_H__
