#ifndef __FENCE_H__
#define __FENCE_H__
#pragma ONCE
#include "StaticObject.h"
class CFence : public CCommonStaticObject
{
	OBJECT_COMPLETE_METHODS( CFence );
	DECLARE_SERIALIZE;

	CGDBPtr<SFenceRPGStats> pStats;

	int nCreator;													// diplomacy of creator
	
	int nDir;
	SVector leftTile, rightTile;
	bool bSuspendAppear;

	enum ETypesOfLife { ETOL_SAFE, ETOL_LEFT, ETOL_RIGHT, ETOL_DESTROYED };
	ETypesOfLife eLifeType;

	std::list< CPtr<CFence> > neighFences;
	std::list<ETypesOfLife> dirToBreak;

	void InitDirectionInfo();
	void AnalyzeConnection( CFence *pFence );
	void DamagePartially( const ETypesOfLife eType );
public:
	CFence() { }
	CFence( const SFenceRPGStats *pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, const int nDiplomacy = -1, bool IsEditor = false );
	virtual void Init();

	virtual const BYTE GetPlayer() const { return nCreator; }

	const struct SHPObjectRPGStats *GetStats() const { return pStats; }

	virtual EStaticObjType GetObjectType() const { return ESOT_FENCE; }

	virtual void Die( const float fDamage );
	virtual void Delete();

	virtual bool CanUnitGoThrough( const EAIClass &eClass ) const;
	virtual bool IsAlive() const { return eLifeType != ETOL_DESTROYED && CCommonStaticObject::IsAlive(); }
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const;
};
#endif // __FENCE_H__
