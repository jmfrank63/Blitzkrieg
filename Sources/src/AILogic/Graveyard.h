#ifndef __GRAVEYARD_H__
#define __GRAVEYARD_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "LinkObject.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIUnit;
class CCommonUnit;
interface IUpdatableObj;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SKilledUnit
{ 
	DECLARE_SERIALIZE;
public:
	CObj<CAIUnit> pUnit; 
	NTimer::STime endFogTime; 
	NTimer::STime endSceneTime;
	NTimer::STime timeToEndDieAnimation;
	// ������� ���, ��� ������
	bool bSentDead;
	// ���������� �������� ������ � ��������������������� endSceneTime � endFogTime
	bool bAnimFinished;
	//
	bool bDisappearUpdateSent;
	
	bool bFatality;						// ������ �� ��������
	NTimer::STime actionTime;	// action time ��� ��������
	CTilesSet lockedTiles;		// ���������� ����� ��� ��������

	bool bFogDeleted;

	SKilledUnit() : bFogDeleted( false ) { }
	SKilledUnit( CAIUnit *_pUnit, const NTimer::STime _timeToEndDieAnimation ) : pUnit( _pUnit ), timeToEndDieAnimation( _timeToEndDieAnimation ), endFogTime( 0 ), endSceneTime( 0 ), bSentDead( false ), bAnimFinished( false ), actionTime( 0 ), bFatality( false ), bDisappearUpdateSent( false ), bFogDeleted( false ) {}
	SKilledUnit( CAIUnit *_pUnit, const NTimer::STime _timeToEndDieAnimation, const NTimer::STime _endSceneTime, const NTimer::STime _endFogTime ) : pUnit( _pUnit ), timeToEndDieAnimation( _timeToEndDieAnimation ), endSceneTime( _endSceneTime ), endFogTime( _endFogTime ), bSentDead( false ), bAnimFinished( false ), actionTime( 0 ), bFatality( false ), bDisappearUpdateSent( false ), bFogDeleted( false ) {}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDeadUnit : public CLinkObject
{
	OBJECT_COMPLETE_METHODS( CDeadUnit );
	DECLARE_SERIALIZE;

	CPtr<IUpdatableObj> pDieObj;
	NTimer::STime dieTime;
	EActionNotify dieAction;
	int nFatality;
	bool bPutMud;

	SVector tileCenter;
public:
	CDeadUnit() { }
	CDeadUnit( class CCommonUnit *_pDieObj, const NTimer::STime _dieTime, const EActionNotify _dieAction, bool bPutMud );
	CDeadUnit( class CCommonUnit *_pDieObj, const NTimer::STime _dieTime, const EActionNotify _dieAction, const int _nFatality, bool bPutMud );

	virtual void GetDyingInfo( struct SAINotifyAction *pDyingInfo );

	virtual const bool IsVisible( const BYTE cParty ) const;
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const;
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const;

	virtual IUpdatableObj* GetDieObject() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGraveyard
{
	DECLARE_SERIALIZE;

	std::list<SKilledUnit> killed;
	typedef std::unordered_map< CObj<CAIUnit>, float, SUnitObjHash > UpdateObjSet;
	UpdateObjSet soonBeDead;

	typedef std::unordered_map<int, std::list< CPtr<CDeadUnit> > > CBridgeDeadSoldiers;
	std::unordered_set<IUpdatableObj*, SDefaultPtrHash> bridgeSoldiersSet;
	CBridgeDeadSoldiers bridgeDeadSoldiers;

	//
	void CheckSoonBeDead();
public:
	void Segment();

	void GetDeadUnits( SAINotifyDeadAtAll **pDeadUnitsBuffer, int *pnLen );

	void AddKilledUnit( class CAIUnit *pUnit, const NTimer::STime &timeOfVisDeath, const int nFatality );
	void PushToKilled( const SKilledUnit &killedUnit, CAIUnit *pUnit );
	void AddToSoonBeDead( class CAIUnit *pUnit, const float fDamage );
	void DelKilledUnitsFromRect( const SRect &rect, CAIUnit *pShotUnit );

	void AddBridgeKilledSoldier( const SVector &tile, CAIUnit *pSoldier );
	void FreeBridgeTile( const SVector &tile );
	
	void UpdateFog4RemovedObject( class CExistingObject *pObj );
	void UpdateFog4AddedObject( class CExistingObject *pObj );

	void Clear();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GRAVEYARD_H__
