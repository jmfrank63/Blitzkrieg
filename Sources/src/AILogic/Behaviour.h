#ifndef __BEHAVIOUR_H__
#define __BEHAVIOUR_H__

#pragma ONCE
class CCommonUnit;
class CAIUnit;
class CBasicGun;
class CShootEstimatorLighAA
{
	BYTE party;
	
	CPtr<CCommonUnit> pUnit;
	CPtr<CAIUnit> pResult;
	CPtr<CBasicGun> pGun;
	float fWorstDamage;
	NTimer::STime bestTime;
	float fMinDistance;
	bool bCanShootNow;

public:
	CShootEstimatorLighAA() 
	: fWorstDamage( -1 ),
		bestTime( 100000 ),
		fMinDistance( 1e10 ),
		bCanShootNow( false ) { }

	void Init( class CCommonUnit *pUnit );
	void Init( class CCommonUnit *pUnit, CBasicGun *pGun );
	void AddUnit( class CAIUnit *pTarget );
	class CAIUnit* GetBestUnit();
};
class CStandartBehaviour
{
	DECLARE_SERIALIZE;
	class CAIUnit* LookForTargetInFireRange( class CCommonUnit *pUnit );

	NTimer::STime camouflateTime;

	NTimer::STime underFireAnalyzeTime;
	NTimer::STime lastTimeOfRotate;
	NTimer::STime fleeTraceEnemyTime;
	int nLastSign;

	bool TryToTraceEnemy( class CAIUnit *pUnit );
public:
	CStandartBehaviour() : camouflateTime( 0 ), underFireAnalyzeTime( 0 ), nLastSign( 1 ), lastTimeOfRotate( -1 ), fleeTraceEnemyTime( 0 ) { }

	void ResetTime( class CCommonUnit *pUnit );
	void UponFire( class CCommonUnit *pUnit, class CAIUnit *pWho, class CAICommand *pCommand );

	void AnalyzeUnderFire( class CAIUnit *pUnit );
	
};
#endif // __BEHAVIOUR_H__
