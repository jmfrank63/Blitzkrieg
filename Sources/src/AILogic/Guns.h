#ifndef __GUNS_H__
#define __GUNS_H__
#pragma ONCE
#include "UpdatableObject.h"
#include "LinkObject.h"
struct SCommonGunInfo : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SCommonGunInfo );
	DECLARE_SERIALIZE;
public:
	bool bFiring;
	int nAmmo;
	NTimer::STime lastShoot;
	int nGun;

	SCommonGunInfo() { }
	SCommonGunInfo( bool _bFiring, const int _nAmmo, const int _nGun ) : bFiring( _bFiring ), nAmmo( _nAmmo ), lastShoot( 0 ), nGun( _nGun ) { }
};
interface IGunsFactory
{
	enum EGunTypes { MOMENT_CML_GUN, MOMENT_BURST_GUN, VIS_CML_BALLIST_GUN, VIS_BURST_BALLIST_GUN, PLANE_GUN, MORALE_GUN };

	virtual class CBasicGun* CreateGun( const EGunTypes eType, const int nShell, SCommonGunInfo *pCommonGunInfo ) const = 0;
	virtual int GetNCommonGun() const = 0;
};
class CBasicGun : public CLinkObject
{
	DECLARE_SERIALIZE;
public:
	virtual const float GetAimTime( bool bRandomize = true ) const = 0;
	virtual const float GetRelaxTime( bool bRandomize = true ) const = 0;
	virtual class CAIUnit* GetOwner() const = 0;
	virtual void SetOwner( class CAIUnit *pUnit ) = 0;
	virtual bool IsAlive() const = 0;
	virtual bool IsGrenade() const = 0;
	
	virtual void GetMechShotInfo( SAINotifyMechShot *pMechShotInfo, const NTimer::STime &time ) const = 0;
	virtual void GetInfantryShotInfo( SAINotifyInfantryShot *pInfantryShotInfo, const NTimer::STime &time ) const = 0;

	virtual bool InFireRange( class CAIUnit *pTarget ) const = 0;
	virtual bool InFireRange( const CVec3 &vPoint ) const = 0;
	virtual float GetFireRange( float z ) const = 0;
	virtual float GetFireRangeMax() const = 0;
	virtual bool InGoToSideRange( const class CAIUnit *pTarget ) const = 0;
	virtual bool TooCloseToFire( const class CAIUnit *pTarget ) const = 0;
	virtual bool TooCloseToFire( const CVec3 &vPoint ) const = 0;

	virtual void StartPointBurst( const CVec3 &_target, bool bReAim ) = 0;
	virtual void StartPointBurst( const CVec2 &target, bool bReAim ) = 0;
	virtual void StartEnemyBurst( class CAIUnit *pEnemy, bool bReAim ) = 0;
	virtual void Segment() = 0;

	virtual bool IsFiring() const = 0;
	virtual bool IsBursting() const = 0;
	virtual void StopFire() = 0;

	virtual bool IsWaitForReload() const =0;
	virtual void ClearWaitForReload() =0;

	virtual void Fire( const CVec2 &target, const float z = 0 ) = 0;
	virtual interface IBallisticTraj* CreateTraj( const CVec2 &vTarget ) const = 0;

	virtual const SBaseGunRPGStats& GetGun() const = 0;
	virtual const SWeaponRPGStats* GetWeapon() const = 0;
	virtual const SWeaponRPGStats::SShell& GetShell() const = 0;
	virtual bool IsOnTurret() const = 0;
	
	virtual void TraceAim( class CAIUnit *pUnit ) = 0;
	virtual void StopTracing() = 0;
	virtual bool IsRelaxing() const = 0;
	virtual bool CanShootWOGunTurn( class CAIUnit *pEnemy, const BYTE cDeltaAngle ) = 0;
	virtual const WORD GetGlobalDir() const = 0;
	virtual void TurnToRelativeDir( const WORD wAngle ) = 0;

	virtual const NTimer::STime GetRestTimeOfRelax() const = 0;
	virtual const float GetRotateSpeed() const = 0;

	virtual bool CanShootToUnitWOMove( class CAIUnit *pEnemy ) = 0;
	virtual bool CanShootToObjectWOMove( class CStaticObject *pObj ) = 0;
	virtual bool CanShootToPointWOMove( const CVec2 &point, const float fZ, const WORD wHorAddAngle = 0, const WORD wVertAddAngle = 0, CAIUnit *pEnemy = 0 ) = 0;
	
	virtual bool CanShootToUnit( class CAIUnit *pEnemy ) = 0;
	virtual bool CanShootToObject( class CStaticObject *pObj ) = 0;
	virtual bool CanShootToPoint( const CVec2 &point, const float fZ, const WORD wHorAddAngle = 0, const WORD wVertAddAngle = 0 ) = 0;
	virtual bool CanShootByHeight( CAIUnit *pTarget ) const = 0;
	virtual bool CanShootByHeight( const float fZ ) const = 0;
	
	virtual void StartPlaneBurst( class CAIUnit *pEnemy, bool bReAim ) = 0;

	virtual bool IsInShootCone( const CVec2 &point, const WORD wAddAngle = 0 ) const = 0;

	virtual const float GetDispersion() const = 0;
	virtual const float GetDispRatio( byte nShellType, const float fDist ) const =0; 
	virtual const int GetFireRate() const = 0;
	virtual void LockInCurAngle() = 0;
	virtual void UnlockCurAngle() = 0;
	
	virtual WORD GetHorTurnConstraint() const = 0;
	virtual WORD GetVerTurnConstraint() const = 0;
	virtual class CTurret* GetTurret() const = 0;

	virtual bool CanBreakArmor( class CAIUnit *pTarget ) const = 0;
	virtual bool CanBreach( const class CCommonUnit *pTarget ) const = 0;
	virtual bool CanBreach( const class CCommonUnit *pTarget, const int nSide ) const = 0;
	virtual bool CanBreach( const SHPObjectRPGStats *pStats, const int nSide ) const = 0;
	
	virtual void DontShoot() = 0;
	virtual void CanShoot() = 0;
	virtual bool IsShootAllowed()=0;

	virtual bool IsCommonGunFiring() const = 0;
	virtual bool IsCommonEqual( const CBasicGun *pGun ) const = 0;

	virtual int GetCommonGunNumber() const = 0;

	virtual int GetNAmmo() const = 0;
	
	virtual WORD GetTrajectoryZAngle( const CVec2 &vToAim, const float z ) const = 0;
	
	virtual const EUnitAckType& GetRejectReason() const = 0;
	virtual void SetRejectReason( const EUnitAckType &eRejectReason ) = 0;
	
	virtual void SetCircularAttack( const bool bCanAttack ) = 0;
	
	virtual void AddParallelGun( CBasicGun *pGun ) = 0;
	virtual void SetToParallelGun() = 0;
	
	virtual const int GetPiercing() const = 0;
	virtual const int GetPiercingRandom() const = 0;
	virtual const int GetRandomPiercing() const = 0;
	virtual const int GetMaxPossiblePiercing() const = 0;
	virtual const int GetMinPossiblePiercing() const = 0;

	virtual const float GetDamage() const = 0;
	virtual const float GetDamageRandom() const = 0;
	virtual const float GetRandomDamage() const = 0;
	
	virtual const NTimer::STime GetTimeToShootToPoint( const CVec3 &vPoint ) const { NI_ASSERT_T(false, "wrong call"); return 0; }
	virtual const NTimer::STime GetTimeToShoot( const CVec3 &vPoint ) const { NI_ASSERT_T( false, "wrong call" ); return 0; }
	
	virtual bool IsBallisticTrajectory() const = 0;
};
float GetDispByRadius( const class CBasicGun *pGun, const CVec2 &attackerPos, const CVec2 &explCoord );
float GetDispByRadius( const class CBasicGun *pGun, const float fDist );
float GetDispByRadius( const float fDispRadius, const float fRangeMax, const float fDist );
const float GetFireRangeMax( const SWeaponRPGStats *pStats, CAIUnit *pOwner );
#endif // __GUNS_H__
