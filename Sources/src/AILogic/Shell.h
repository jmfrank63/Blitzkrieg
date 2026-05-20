#ifndef __SHELL_H__
#define __SHELL_H__

#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <queue>
#include "UpdatableObject.h"
#include "LinkObject.h"
#include "..\zlib\zlib.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIUnit;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														Hits																	*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CHitInfo : public CLinkObject
{
	OBJECT_COMPLETE_METHODS( CHitInfo );
	DECLARE_SERIALIZE;
public:
	CGDBPtr<SWeaponRPGStats> pWeapon;
	WORD wShell;
	WORD wDir;

	CPtr<IRefCount> pVictim;  // для попадания по юниту
	CVec3 explCoord;					// для попадания по земле

	SAINotifyHitInfo::EHitType eHitType;
	
	CHitInfo() { }
	CHitInfo( const SWeaponRPGStats *_pWeapon, const WORD _wShell, const WORD &_wDir, IRefCount *_pVictim, const SAINotifyHitInfo::EHitType _eHitType, const CVec3 &_explCoord )
		: pWeapon( _pWeapon ), wShell( _wShell ), wDir( _wDir ), pVictim( _pVictim ), eHitType( _eHitType ), explCoord( _explCoord ) { SetUniqueId(); }

	CHitInfo( const SWeaponRPGStats *_pWeapon, const WORD _wShell, const WORD &_wDir, const CVec3 &_explCoord, const SAINotifyHitInfo::EHitType _eHitType )
		: pWeapon( _pWeapon ), wShell( _wShell ), wDir( _wDir ), pVictim( 0 ), explCoord( _explCoord ), eHitType( _eHitType ) { SetUniqueId(); }

	CHitInfo( const class CExplosion *pExpl, IRefCount *pVictim, const enum SAINotifyHitInfo::EHitType &eHitType, const CVec3 &explCoord );

	virtual void GetHitInfo( struct SAINotifyHitInfo *pHitInfo ) const;
	
	virtual const bool IsVisible( const BYTE party ) const { return true; }
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const { pTiles->clear(); }
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const { return false; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												Траектории																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IBallisticTraj : public IRefCount
{
public:
	virtual const NTimer::STime& GetExplTime() const = 0;
	virtual const NTimer::STime& GetStartTime() const = 0;

	virtual const CVec3& GetStartPoint() const = 0;
	virtual const WORD GetStart2DDir() const = 0;

	virtual const CVec3 GetCoordinates() const = 0;

	virtual const SWeaponRPGStats::SShell::ETrajectoryType GetTrajType() const = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBallisticTraj: public IBallisticTraj
{
	OBJECT_COMPLETE_METHODS( CBallisticTraj	);
	DECLARE_SERIALIZE;

	CVec3 vStart3D;
	// скорости
	float fVx, fVy;
	// ускорения свободного падения

	WORD wAngle; //вертикальнй угол

	WORD wDir;
	CVec2 vDir;

	float fG; // для данной траектории ускорение свободного падения

	NTimer::STime startTime, explTime;

	SWeaponRPGStats::SShell::ETrajectoryType eType;
public:
	CBallisticTraj() { }
	CBallisticTraj( const CVec3 &vStart, const CVec2 &vFinish, float fV, const SWeaponRPGStats::SShell::ETrajectoryType eType, WORD wMaxAngle, float fMaxRange );
	
	virtual const NTimer::STime& GetExplTime() const { return explTime; }
	virtual const NTimer::STime& GetStartTime() const { return startTime; }
	virtual const CVec3& GetStartPoint() const { return vStart3D; }
	virtual const WORD GetStart2DDir() const { return wDir; }

	virtual const CVec3 GetCoordinates() const;

	static WORD GetTrajectoryZAngle( const CVec2 &vToAim, const float z, float fV, const SWeaponRPGStats::SShell::ETrajectoryType eType, WORD wMaxAngle, float fMaxRange );

	virtual const SWeaponRPGStats::SShell::ETrajectoryType GetTrajType() const { return eType; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFakeBallisticTraj : public IBallisticTraj
{
	OBJECT_COMPLETE_METHODS( CFakeBallisticTraj );
	DECLARE_SERIALIZE;

	NTimer::STime startTime, explTime;

	CVec3 point;
	CVec3 v;
	float A1, A2;
	WORD wDir;
public:
	CFakeBallisticTraj() { }
	CFakeBallisticTraj( const CVec3 &point, const CVec3 &v, const NTimer::STime &explTime, const float A1, const float A2 );

	virtual const NTimer::STime& GetExplTime() const { return explTime; }
	virtual const NTimer::STime& GetStartTime() const { return startTime; }

	virtual const CVec3& GetStartPoint() const { return point; }
	virtual const WORD GetStart2DDir() const { return wDir; }

	virtual const CVec3 GetCoordinates() const;
	static WORD GetTrajectoryZAngle( const CVec2 &vToAim, const float z, float fV, const SWeaponRPGStats::SShell::ETrajectoryType eType ) { return 0; }

	virtual const SWeaponRPGStats::SShell::ETrajectoryType GetTrajType() const { return SWeaponRPGStats::SShell::TRAJECTORY_CANNON; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBombBallisticTraj : public IBallisticTraj
{
	OBJECT_COMPLETE_METHODS( CBombBallisticTraj );

	DECLARE_SERIALIZE;

	CVec3 point;
	CVec3 v;
	CVec2 vRandAcc;
	WORD wDir;
	NTimer::STime startTime, explTime;
public:
	CBombBallisticTraj() { };
	CBombBallisticTraj( const CVec3 &point, const CVec3 &v, const NTimer::STime &explTime, const CVec2 &vRandAcc );

	virtual const NTimer::STime& GetExplTime() const { return explTime; }
	virtual const NTimer::STime& GetStartTime() const { return startTime; }

	virtual const CVec3& GetStartPoint() const { return point; }
	virtual const WORD GetStart2DDir() const { return wDir; }

	virtual const CVec3 GetCoordinates() const;

	const SWeaponRPGStats::SShell::ETrajectoryType GetTrajType() const { return SWeaponRPGStats::SShell::TRAJECTORY_BOMB; }

	static float GetCoeff( const float &timeDiff );
	static WORD GetTrajectoryZAngle( const CVec2 &vToAim, const float z, float fV, const SWeaponRPGStats::SShell::ETrajectoryType eType ) { return 16384 * 3; }
	static CVec3 CalcTrajectoryFinish( const CVec3 &vSourcePoint, const CVec3 &vInitialSpeed, const CVec2 &vRandAcc );
	static float GetTimeOfFly( const float fZ, const float fZSpeed );
};
//*******************************************************************
//*								  Взрывы																					*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosion : public IRefCount
{
	DECLARE_SERIALIZE;
protected:
	BYTE nShellType;
	CGDBPtr<SWeaponRPGStats> pWeapon;
	CPtr<CAIUnit> pUnit;
	
	CVec2 explCoord;
	float z;
	WORD attackDir;
	int nPlayerOfShoot;

	CPtr<CHitInfo> pHitToSend;

	//
	const SAINotifyHitInfo::EHitType ProcessExactHit( class CAIUnit *pTarget, const SRect &combatRect, const CVec2 &explCoord, const int nRandPiercing, const int nRandArmor ) const;
	void Init( class CAIUnit *pUnit, const struct SWeaponRPGStats *pWeapon, const float fDispersion, const float fDispRatio, const CVec2 &_explCoord, const float _z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize, const int nPlayerOfShoot );
public:
	CExplosion() : nPlayerOfShoot( -1 ) { }
	CExplosion( CAIUnit *pUnit, const class CBasicGun *pGun, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize = true );
	CExplosion( CAIUnit *pUnit, const struct SWeaponRPGStats *pWeapon, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize = true );

	const CVec2& GetExplCoordinates() const { return explCoord; }
	const float& GetExplZ() const { return z; }
	CAIUnit* GetWhoFire() const { return pUnit; }

	const SWeaponRPGStats *GetWeapon() const { return pWeapon; }
	const BYTE GetShellType() const { return nShellType; }
	const SWeaponRPGStats::SShell GetShellStats() const { return pWeapon->shells[nShellType]; }
	const WORD GetAttackDir() const { return attackDir; }

	const int GetRandomPiercing() const;
	const float GetRandomDamage() const;
	
	const int GetPartyOfShoot() const;
	const int GetPlayerOfShoot() const;

	virtual void Explode() = 0;
	float GetMaxDamage() const ;
	SWeaponRPGStats::SShell::ETrajectoryType GetTrajectoryType() const { return pWeapon->shells[nShellType].trajectory; }
	
	// если explosion моральный, то вернется true.
	bool ProcessMoraleExplosion() const;
	// если explosion дымовой, то вернется true.
	bool ProcessSmokeScreenExplosion() const;

	void AddHitToSend( CHitInfo *pHit );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBurstExpl : public CExplosion
{
	OBJECT_COMPLETE_METHODS( CBurstExpl );
	DECLARE_SERIALIZE;
	int nArmorDir;
public:
	CBurstExpl() { }
	// nArmorDir == 0  -  просто по плоскому направлению ( это для снарядов )
	// nArmorDir == 1  -  взрыв под днищем ( для мин )
	// nArmorDir == 2  -  взрыв над крышей

	CBurstExpl( CAIUnit *pUnit, const class CBasicGun *pGun, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize = true, int ArmorDir = 0 );
	CBurstExpl( CAIUnit *pUnit, const SWeaponRPGStats *pWeapon, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize = true, int ArmorDir = 0 );

	virtual void Explode();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCumulativeExpl : public CExplosion
{
	OBJECT_COMPLETE_METHODS( CCumulativeExpl );
	DECLARE_SERIALIZE;
	int nArmorDir;
public:
	CCumulativeExpl() { }
	CCumulativeExpl( CAIUnit *pUnit, const class CBasicGun *pGun, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize = true );

	virtual void Explode();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*								  Снаряды																					*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// снаряд, попадающй мгновенно
class CMomentShell
{
	DECLARE_SERIALIZE;
	
	CPtr<CExplosion> expl;
public:
	CMomentShell( CExplosion *_expl ) : expl( _expl ) { }

	const CVec2& GetExplCoordinates() const { return expl->GetExplCoordinates(); }
	const float GetExplZ() const { return expl->GetExplZ(); }
	void Explode() { expl->Explode(); }

	float GetMaxDamage() const { return expl->GetMaxDamage(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// снаряд
class CShell
{
	DECLARE_SERIALIZE;

	NTimer::STime explTime;
	CPtr<CExplosion> expl;
	int nGun;

	float vStartVisZ, vFinishVisZ;
protected:
	const float GetStartVisZ() const { return vStartVisZ; }
	const float GetFinishVisZ() const { return vFinishVisZ; }
public:
	CShell() { }
	CShell( const NTimer::STime &explTime, CExplosion *expl, const int nGun );

	const NTimer::STime GetExplTime() const { return explTime; }

	const CVec2& GetExplCoordinates() const { return expl->GetExplCoordinates(); }	
	const float GetExplZ() const { return expl->GetExplZ(); }
	const SWeaponRPGStats *GetWeapon() const { return expl->GetWeapon(); }
	const BYTE GetShellType() const { return expl->GetShellType(); }

	IRefCount* GetWhoFired() const;
	const int GetNGun() const { return nGun; }

	void Explode() { expl->Explode(); }
	float GetMaxDamage() const { return expl->GetMaxDamage(); }
	SWeaponRPGStats::SShell::ETrajectoryType GetTrajectoryType() const { return expl->GetTrajectoryType(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// невидимый снаряд
class CInvisShell : public IRefCount, public CShell
{
	OBJECT_COMPLETE_METHODS( CInvisShell );
	DECLARE_SERIALIZE;
public:
	CInvisShell() { }
	CInvisShell( const NTimer::STime &explTime, CExplosion *expl, const int nGun )
		: CShell( explTime, expl, nGun ) { }

	bool operator < ( const CInvisShell &shell ) { return GetExplTime() > shell.GetExplTime(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SInvisShellCmp
{
	bool operator()( const CPtr<CInvisShell> &shell1, const CPtr<CInvisShell> &shell2 ) const
	{
		return shell1->GetExplTime() > shell2->GetExplTime();
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// видимый снаряд
class CVisShell : public CLinkObject, public CShell
{
	OBJECT_COMPLETE_METHODS( CVisShell );		

	DECLARE_SERIALIZE;
	CPtr<IBallisticTraj> pTraj;
	CVec3 center;
	CVec3 speed;
	bool bVisible;
	
	void CalcVisibility();
public:
	CVisShell() { }
	CVisShell( CExplosion *_expl, IBallisticTraj *_pTraj, const int nGun )
		: CShell( _pTraj->GetExplTime(), _expl, nGun ), pTraj( _pTraj ), center( _pTraj->GetStartPoint() ),  speed( VNULL3 ), bVisible( true ) { SetUniqueId(); }

	const NTimer::STime GetStartTime() const { return pTraj->GetStartTime(); }

	virtual void GetPlacement( struct SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff );
	virtual void GetSpeed3( CVec3 *pSpeed ) const { *pSpeed = speed; }
	virtual void GetProjectileInfo( struct SAINotifyNewProjectile *pProjectileInfo );

	void Segment();

	const CVec3 GetCoordinates() const { return pTraj->GetCoordinates(); }

	virtual float GetTerrainHeight( const float x, const float y, const NTimer::STime timeDiff ) const;
	virtual const bool IsVisible( const BYTE party ) const { return true; }
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const { pTiles->clear(); }
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const { return false; }
	
	virtual const bool IsVisibleByPlayer() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*								  Склад снарядов																	*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CShellsStore
{
	DECLARE_SERIALIZE;
	// все невидимые снаряды
	std::priority_queue< CPtr<CInvisShell>, std::vector< CPtr<CInvisShell> >, SInvisShellCmp > invisShells;
	// все видимые снаряды
	std::list< CPtr<CVisShell> > visShells;
public:
	CShellsStore() { };
	void Clear();

	void AddShell( CMomentShell	&shell ); 
	void AddShell( CInvisShell *pShell );
	void AddShell( CVisShell *pShell );

	void Segment();
	
	// для тестирования multiplayer
	void UpdateCheckSum( uLong *pCheckSum );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif	// __SHELL_H__
