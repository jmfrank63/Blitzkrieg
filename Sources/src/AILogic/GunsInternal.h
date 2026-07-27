#ifndef __GUNS_INTERNAL_H__
#define __GUNS_INTERNAL_H__
#pragma ONCE
#include "Guns.h"
class CAIUnit;
class CGun : public CBasicGun
{
	DECLARE_SERIALIZE;

	enum EShootState { EST_TURNING, EST_AIMING, WAIT_FOR_ACTION_POINT, EST_SHOOTING, EST_REST };
	EShootState shootState;

	bool bWaitForReload; //specific for artillery

	bool bCanShoot;
	int nShotsLast;

	CPtr<SCommonGunInfo> pCommonGunInfo;

	IGunsFactory::EGunTypes eType;
	EUnitAckType eRejectReason;
	
	CVec3 vLastShotPoint;
	
	float fRandom4Aim, fRandom4Relax;


	void Aiming();
	void WaitForActionPoint();
	void Shooting();
	const CVec2 GetShootingPoint() const;
	WORD GetVisAngleOfAim() const;
	
	void OnWaitForActionPointState();
	void OnTurningState();
	void OnAimState();
protected:	
	BYTE nShellType;
	CAIUnit *pOwner;
	int nOwnerParty;

	CPtr<CAIUnit> pEnemy;
	CVec2 target;
	NTimer::STime lastCheck;
	CVec2 lastEnemyPos;
	bool bAngleLocked;

	bool bAim;
	bool bGrenade;
	float z;

	typedef std::list< CPtr<CBasicGun> > CParallelGuns;
	CParallelGuns parallelGuns;
	bool bParallelGun;
	NTimer::STime lastCheckTurnTime;

	const NTimer::STime GetActionPoint() const;
	virtual bool TurnGunToEnemy( const CVec2 &vEnemyCenter, const float zDiff ) = 0;
	virtual bool IsGoodAngle( const CVec2 &point, const WORD addAngle, const float z, const BYTE cDeltaAngle ) const = 0;
	virtual void ToRestState();
	virtual void Rest() = 0;
	virtual bool AnalyzeTurning() = 0;
	bool CanShootWOGunTurn( const BYTE cDeltaAngle, const float fZ );
	bool AnalyzeLimitedAngle( class CCommonUnit *pUnit, const CVec2 &point ) const;
	void Turning();
	bool CanShootToTargetWOMove();

	void InitRandoms();
public:
	CGun() : pOwner( 0 ), bParallelGun( false ), vLastShotPoint( VNULL3	), lastCheckTurnTime( 0 ) { }
	CGun( class CAIUnit *pOwner, const BYTE nShellType, SCommonGunInfo *pCommonGunInfo, const IGunsFactory::EGunTypes eType );

	virtual const float GetAimTime( bool bRandomize = true ) const;
	virtual const float GetRelaxTime( bool bRandomize = true ) const;
	virtual class CAIUnit* GetOwner() const { return pOwner; }	
	virtual void SetOwner( CAIUnit *pOwner );
	virtual bool IsAlive() const;
	virtual bool IsGrenade() const { return bGrenade; }
	
	virtual void GetMechShotInfo( SAINotifyMechShot *pMechShotInfo, const NTimer::STime &time ) const;
	virtual void GetInfantryShotInfo( SAINotifyInfantryShot *pInfantryShotInfo, const NTimer::STime &time ) const;

	virtual bool InFireRange( class CAIUnit *pTarget ) const;
	virtual bool InFireRange( const CVec3 &vPoint ) const;
	virtual float GetFireRange( float z ) const;
	virtual float GetFireRangeMax() const;
	virtual bool InGoToSideRange( const class CAIUnit *pTarget ) const;
	virtual bool TooCloseToFire( const class CAIUnit *pTarget ) const;
	virtual bool TooCloseToFire( const CVec3 &vPoint ) const;
	
	virtual void StartPointBurst( const CVec3 &target, bool bReAim );
	virtual void StartPointBurst( const CVec2 &target, bool bReAim );
	virtual void StartEnemyBurst( class CAIUnit *pEnemy, bool bReAim );
	virtual void Segment();

	virtual bool IsWaitForReload() const { return bWaitForReload; }
	virtual void ClearWaitForReload() { bWaitForReload = false; }

	virtual bool IsFiring() const ;
	virtual bool IsBursting() const { return shootState == WAIT_FOR_ACTION_POINT || shootState == EST_SHOOTING; }

	const SBaseGunRPGStats& GetGun() const;
	virtual const SWeaponRPGStats* GetWeapon() const;
	virtual const SWeaponRPGStats::SShell& GetShell() const;
	
	virtual bool IsRelaxing() const;
	virtual bool CanShootWOGunTurn( class CAIUnit *pEnemy, const BYTE cDeltaAngle );
	virtual const NTimer::STime GetRestTimeOfRelax() const;

	virtual bool CanShootToUnitWOMove( class CAIUnit *pEnemy );
	virtual bool CanShootToObjectWOMove( class CStaticObject *pObj );
	virtual bool CanShootToPointWOMove( const CVec2 &point, const float fZ, const WORD wHorAddAngle = 0, const WORD wVertAddAngle = 0, CAIUnit *pEnemy = 0 );
	
	virtual bool CanShootByHeight( class CAIUnit *pTarget ) const;	
	virtual bool CanShootByHeight( const float fZ ) const;

	virtual bool CanShootToUnit( class CAIUnit *pEnemy );
	virtual bool CanShootToObject( class CStaticObject *pObj );
	virtual bool CanShootToPoint( const CVec2 &point, const float fZ, const WORD wHorAddAngle = 0, const WORD wVertAddAngle = 0 );

	virtual bool IsInShootCone( const CVec2 &point, const WORD wAddAngle = 0 ) const;

	virtual const float GetDispersion() const;
	virtual const float GetDispRatio( byte nShellType, const float fDist ) const; 
	virtual const int GetFireRate() const;
	virtual void LockInCurAngle() { bAngleLocked = true; }
	virtual void UnlockCurAngle() { bAngleLocked = false; }
	
	virtual void StartPlaneBurst( class CAIUnit *pEnemy, bool bReAim );

	virtual bool CanBreakArmor( class CAIUnit *pTarget ) const;
	virtual bool CanBreach( const class CCommonUnit *pTarget ) const;
	virtual bool CanBreach( const class CCommonUnit *pTarget, const int nSide ) const;
	virtual bool CanBreach( const SHPObjectRPGStats *pStats, const int nSide ) const;
	
	virtual void DontShoot() { bCanShoot = false; }
	virtual void CanShoot() { bCanShoot = true; }
	virtual bool IsShootAllowed(){ return bCanShoot; }

	virtual bool IsCommonGunFiring() const { return pCommonGunInfo->bFiring; }
	virtual bool IsCommonEqual( const CBasicGun *pGun ) const;

	virtual int GetCommonGunNumber() const { return pCommonGunInfo->nGun; }

	virtual int GetNAmmo() const { return pCommonGunInfo->nAmmo; }

	virtual interface IBallisticTraj* CreateTraj( const CVec2 &vTarget ) const;
	virtual void Fire( const CVec2 &target, const float z = 0 );
	virtual WORD GetTrajectoryZAngle( const CVec2 &vToAim, const float z) const;

	virtual const EUnitAckType& GetRejectReason() const { return eRejectReason; }
	virtual void SetRejectReason( const EUnitAckType &eReason );

	virtual const bool IsVisible( const BYTE party ) const { return true; }
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const { pTiles->clear(); }
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const { return false; }

	virtual void AddParallelGun( CBasicGun *pGun ) { parallelGuns.push_back( pGun ); }
	virtual void SetToParallelGun() { bParallelGun = true; }
	
	virtual const int GetPiercing() const;
	virtual const int GetPiercingRandom() const;
	virtual const int GetRandomPiercing() const;
	virtual const int GetMaxPossiblePiercing() const;
	virtual const int GetMinPossiblePiercing() const;

	virtual const float GetDamage() const;
	virtual const float GetDamageRandom() const;
	virtual const float GetRandomDamage() const;

	virtual bool IsBallisticTrajectory() const;
};
class CTurretGun : public CGun
{
	OBJECT_COMPLETE_METHODS( CTurretGun );
	DECLARE_SERIALIZE;

	WORD wBestWayDir;
	bool bTurnByBestWay;
	CPtr<CTurret> pTurret;
	bool bCircularAttack;

	bool TurnByVer( const CVec2 &vEnemyCenter, const float zDiff );
	bool TurnArtilleryToEnemy( const CVec2 &vEnemyCenter );
	bool TurnByBestWay( const WORD wDirToEnemy );
	
	WORD CalcVerticalAngle( const class CVec2 &pt, const float z ) const;
protected:
	virtual bool TurnGunToEnemy( const CVec2 &vEnemyCenter, const float zDiff );
	virtual bool IsGoodAngle( const CVec2 &point, const WORD addAngle, const float z, const BYTE cDeltaAngle ) const;
	virtual void Rest();
	virtual bool AnalyzeTurning();
public:
	CTurretGun() : bCircularAttack( false ) { }
	CTurretGun( class CAIUnit *pOwner, const BYTE nShellType, SCommonGunInfo *pCommonGunInfo, const IGunsFactory::EGunTypes eType, const int nTurret );

	virtual bool IsOnTurret() const { return true; }
	virtual class CTurret* GetTurret() const { return pTurret; }
	virtual void TraceAim( class CAIUnit *pUnit );
	virtual void StopTracing();

	virtual void StopFire();

	virtual bool CanShootByHeight( class CAIUnit *pTarget ) const;

	virtual const WORD GetGlobalDir() const;
	virtual void TurnToRelativeDir( const WORD wAngle );

	virtual const float GetRotateSpeed() const;

	virtual WORD GetHorTurnConstraint() const;
	virtual WORD GetVerTurnConstraint() const;
	
	void SetCircularAttack( const bool bCanAttack );

	virtual void StartPointBurst( const CVec3 &target, bool bReAim );
	virtual void StartPointBurst( const CVec2 &target, bool bReAim );
	virtual void StartEnemyBurst( class CAIUnit *pEnemy, bool bReAim );
	
	virtual const NTimer::STime GetTimeToShootToPoint( const CVec3 &vPoint ) const;
	virtual const NTimer::STime GetTimeToShoot( const CVec3 &vPoint ) const;
};
class CBaseGun : public CGun
{
	OBJECT_COMPLETE_METHODS( CBaseGun );
	DECLARE_SERIALIZE;

protected:
	virtual bool TurnGunToEnemy( const CVec2 &vEnemyCenter, const float zDiff );
	virtual bool IsGoodAngle( const CVec2 &point, const WORD addAngle, const float z, const BYTE cDeltaAngle ) const;
	virtual void Rest() { }
	virtual bool AnalyzeTurning();
public:
	CBaseGun() { }
	CBaseGun( class CAIUnit *pOwner, const BYTE nShellType, SCommonGunInfo *pCommonGunInfo, const IGunsFactory::EGunTypes eType )
	: CGun( pOwner, nShellType, pCommonGunInfo, eType ) { }

	virtual bool IsOnTurret() const { return false; }
	virtual class CTurret* GetTurret() const { return 0; }
	virtual void TraceAim( class CAIUnit *pUnit ) { }
	virtual void StopTracing() { }

	virtual void StopFire();

	virtual const WORD GetGlobalDir() const;
	virtual void TurnToRelativeDir( const WORD wAngle ) { }

	virtual const float GetRotateSpeed() const;

	virtual WORD GetHorTurnConstraint() const { return 32768; }
	virtual WORD GetVerTurnConstraint() const { return 32768; }

	void SetCircularAttack( const bool bCanAttack ) { }
};
class CUnitsGunsFactory : public IGunsFactory
{
	class CAIUnit *pUnit;
	const int nCommonGun;
	int nTurret;
public:
	CUnitsGunsFactory( class CAIUnit *_pUnit, const int _nCommonGun, const int _nTurret )
		: pUnit( _pUnit ), nCommonGun( _nCommonGun ), nTurret( _nTurret ) { }

	virtual int GetNCommonGun() const { return nCommonGun; }
	virtual CBasicGun* CreateGun( const EGunTypes eType, const int nShell, SCommonGunInfo *pCommonGunInfo ) const 
	{ 
		CBasicGun *pGun = 0;
		if ( nTurret != -1 )
			pGun = new CTurretGun( pUnit, nShell, pCommonGunInfo, eType, nTurret );
		else
			pGun = new CBaseGun( pUnit, nShell, pCommonGunInfo, eType );

		return pGun;
	}
};
#endif // __GUNS_INTERNAL_H__
