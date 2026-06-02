#ifndef __RND_RUN_UP_TO_ENEMY__
#define __RND_RUN_UP_TO_ENEMY__
class CSoldier;
class CAIUnit;
class CRndRunUpToEnemy
{
	DECLARE_SERIALIZE;

	CSoldier *pOwner;
	CPtr<CAIUnit> pEnemy;

	CVec2 vLastOwnerPos;
	NTimer::STime checkTime;
	bool bRunningToEnemy;
	bool bForceStaying;
	bool bCheck;

	void SendOwnerToRandomRun();
public:
	CRndRunUpToEnemy() : pOwner( 0 ) { }
	CRndRunUpToEnemy( CAIUnit *pOwner, CAIUnit *pEnemy );
	void Init( CAIUnit *pOwner, CAIUnit *pEnemy );

	bool IsRunningToEnemy() const { return bRunningToEnemy; }
	void Segment();

	void Finish();
};
#endif __RND_RUN_UP_TO_ENEMY__
