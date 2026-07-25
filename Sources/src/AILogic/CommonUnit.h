#ifndef __COMMON_UNIT_H__
#define __COMMON_UNIT_H__
#pragma ONCE
enum EUnitAckType : unsigned int;		// must match the fixed base in Main/RPGStats.h
#include "LinkObject.h"
#include "BasePathUnit.h"
#include "GroupUnit.h"
#include "QueueUnit.h"
enum ECamouflageRemoveReason
{
	ECRR_SELF_MOVE,
	ECRR_SELF_SHOOT,
	ECRR_GOOD_VISIBILITY,
	ECRR_USER_COMMAND,
};
class CBasicGun;
struct SBehaviour
{
	enum EMoving
	{
		EMRoaming = 0,
		EMFollow = 1,
		EMHoldPos = 2
	};
	enum EFire
	{
		EFAtWill = 0,
		EFReturn = 1,
		EFNoFire = 2
	};

	DECLARE_SERIALIZE;

public:
	EMoving moving;
	EFire fire;

	SBehaviour() : moving( EMRoaming ), fire( EFAtWill ) { }
	SBehaviour( const EMoving _moving, const EFire _fire ) : moving( _moving ), fire( _fire ) { }
};
interface IShootEstimator;
interface IScenarioUnit;
class CAIUnit;
class CCommonUnit : public CLinkObject, public IBasePathUnit, public CGroupUnit, public CQueueUnit
{
	DECLARE_SERIALIZE;

	int dbID;

	CPtr<IScenarioUnit> pScenarioUnit;
	
	SBehaviour beh;
	NTimer::STime lastBehTime;

	CPtr<CBasicGun> pLockingGun;
	CVec2 vBattlePos;
	WORD wReserveDir;
	CPtr<CAIUnit> pTruck;

	bool bSelectable;

	float fDesirableSpeed;
	CPtr<CCommonUnit> pFollowedUnit;
	float fMinFollowingSpeed;
	CVec2 vFollowShift;

	CPtr<IShootEstimator> pShootEstimator;
	
	bool bCanBeFrozenByState;
	bool bCanBeFrozenByScan;
	NTimer::STime nextFreezeScan;
public:
	CCommonUnit() { }

	virtual int GetMovingType() const { return 0; }
	virtual bool IsSelectable() const { return bSelectable; }
	virtual void Init( const int dbID );
	
	void SetScenarioUnit( interface IScenarioUnit *pScenarioUnit );
	IScenarioUnit *GetScenarioUnit() const;

	virtual int GetNGuns() const = 0;
	virtual CBasicGun* GetGun( const int n ) const = 0;

	virtual CBasicGun* ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime ) = 0;
	virtual CBasicGun* ChooseGunForStatObjWOTime( class CStaticObject *pObj );

	bool CanShootToUnitWoMove( class CAIUnit *pTarget );

	virtual void SetSelectable(  bool bSelectable );

	virtual const BYTE GetPlayer() const = 0;
	const BYTE GetParty() const;
	virtual void ChangePlayer( const BYTE cPlayer ) = 0;
	virtual void SetPlayerForEditor( const int nPlayer ) = 0;

	virtual const float GetMaxDamage( class CCommonUnit *pTarget ) const;

	virtual const float GetSightRadius() const = 0;

	virtual bool CanGoToPoint( const CVec2 &point ) const;

	SBehaviour& GetBehaviour() { return beh; }
	const SBehaviour& GetBehaviour() const { return beh; }
	const NTimer::STime& GetLastBehTime() const { return lastBehTime; }
	NTimer::STime& GetLastBehTime() { return lastBehTime; }

	virtual const bool IsVisible( const BYTE party ) const = 0;

	virtual const int GetDBID() const { return dbID; }
	
	virtual const bool NeedDeinstall() const { return false; }
	virtual const bool CanShoot() const { return true; }
	virtual const bool CanShootToPlanes() const = 0;
	
	virtual void Fired( const float fGunRadius, const int nGun ) = 0;
	
	virtual CBasicGun* GetFirstArtilleryGun() const { return 0; }
	
	virtual void SetAmbush() = 0;
	virtual void RemoveAmbush() = 0;
	
	virtual const NTimer::STime GetTimeToCamouflage() const = 0;
	virtual void SetCamoulfage()=0;
	virtual void RemoveCamouflage( ECamouflageRemoveReason eReason )=0;

	
	virtual void UpdateArea( const EActionNotify eAction ) = 0;
	virtual BYTE GetAIClass() const { return AI_CLASS_ANY; }
	
	virtual void Lock( const CBasicGun *pGun );
	virtual void Unlock( const CBasicGun *pGun );
	virtual bool IsLocked( const CBasicGun *pGun ) const;
	
	virtual class CTurret* GetTurret( const int nTurret ) const = 0;
	virtual const int GetNTurrets() const = 0;
	virtual bool IsMech() const = 0;
	
	void SetBattlePos( const CVec2 &vPos, const WORD _wReserveDir = 0 ) { vBattlePos = vPos; wReserveDir = _wReserveDir; }
	bool DoesReservePosExist() const { return vBattlePos.x != -1.0f; }
	const CVec2& GetBattlePos() const { NI_ASSERT_T( DoesReservePosExist(), "Reserve pos doesn't exist" ); return vBattlePos; }
	const WORD GetReserveDir() const { NI_ASSERT_T( DoesReservePosExist(), "Reserve pos doesn't exist" ); return wReserveDir; }
	void SetTruck( class CAIUnit *pUnit );
	class CAIUnit* GetTruck() const;
	
	virtual void Disappear() = 0;
	virtual void Die( const bool fromExplosion, const float fDamage ) = 0;

	virtual bool IsFormation() const { return false; }
	virtual bool IsInfantry() const { return false; }

	virtual void SendAcknowledgement( EUnitAckType ack, bool bForce = false ) = 0;
	virtual void SendAcknowledgement( CAICommand *pCommand, EUnitAckType ack, bool bForce = false ) = 0;

	virtual const float GetMaxSpeedHere( const CVec2 &point, bool bAdjust = true ) const;
	virtual void SetDesirableSpeed( const float fDesirableSpeed );
	virtual void UnsetDesirableSpeed();
	virtual float GetDesirableSpeed() const;
	virtual void AdjustWithDesirableSpeed( float *pfMaxSpeed ) const;

	void SetFollowState( class CCommonUnit *pFollowedUnit );
	void UnsetFollowState();
	bool IsInFollowState();
	class CCommonUnit* GetFollowedUnit() const;
	const CVec2& GetFollowShift() const { return vFollowShift; }

	void FollowingByYou( class CCommonUnit *pFollowingUnit );

	virtual void Segment();
	virtual void FreezeSegment();
	
	virtual bool IsOperable() const { return true; }
	
	virtual const int GetMinArmor() const = 0;
	virtual const int GetMaxArmor() const = 0;
	virtual const int GetMinPossibleArmor( const int nSide ) const = 0;
	virtual const int GetMaxPossibleArmor( const int nSide ) const = 0;
	virtual const int GetArmor( const int nSide ) const = 0;
	virtual const int GetRandomArmor( const int nSide ) const = 0;
	
	virtual float GetMaxFireRange() const = 0;
	
	virtual void UnRegisterAsBored( const enum EUnitAckType eBoredType ) = 0;
	virtual void RegisterAsBored( const enum EUnitAckType eBoredType ) = 0;

	virtual EUnitAckType GetGunsRejectReason() const = 0;

	void SetShootEstimator( interface IShootEstimator *pShootEstimator );
	void ResetShootEstimator( class CAIUnit *pCurEnemy, const bool bDamageUpdated, const DWORD dwForbidden = 0 );
	void AddUnitToShootEstimator( class CAIUnit *pUnit );
	CAIUnit* GetBestShootEstimatedUnit() const;
	CBasicGun* GetBestShootEstimatedGun() const;
	const int GetNumOfBestShootEstimatedGun() const;

	virtual const float GetKillSpeed( class CAIUnit *pEnemy ) const { return 0; }
	virtual void ResetTargetScan() = 0;
	virtual BYTE AnalyzeTargetScan(	CAIUnit *pCurTarget, const bool bDamageUpdated, const bool bScanForObstacles, IRefCount *pCheckBuilding = 0 ) = 0;
	virtual void LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, CBasicGun **pGun ) = 0;
	virtual interface IObstacle * LookForObstacle() { return 0; };
	
	virtual bool CanMoveForGuard() const = 0;
	virtual bool CanRotate() const = 0;
	
	virtual bool CanCommandBeExecutedByStats( int nCmd ) const = 0;

	virtual const NTimer::STime GetNextPathSegmTime() const { return 0; }
	
	virtual float GetPriceMax() const = 0;
	virtual const NTimer::STime& GetBehUpdateDuration() const = 0;
	
	virtual void AnimationSegment() { }
	
	bool IsDangerousDirExist() const { return false; }
	const WORD GetDangerousDir() const { return 0; }

	virtual const float GetTargetScanRadius() { return 0.0f; }

	bool CanBeFrozen() const;
	bool IsFrozenByState() const;
	virtual void FreezeByState( const bool bFreeze );
	
	virtual bool CanHookUnit( class CAIUnit *pUnitToHook ) const { return false; }
	virtual bool IsTowing() const { return false; }
	
	virtual bool CanMoveAfterUserCommand() const = 0;
};
#endif // __COMMON_UNIT_H__
