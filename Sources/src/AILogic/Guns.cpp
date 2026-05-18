#include "stdafx.h"

#include "GunsInternal.h"
#include "Shell.h"
#include "Soldier.h"
#include "Randomize.h"
#include "AIStaticMap.h"
#include "Updater.h"
#include "Shell.h"
#include "Cheats.h"
#include "Turret.h"
#include "StaticObject.h"
#include "Technics.h"
#include "Aviation.h"
#include "UnitStates.h"
#include "Weather.h"
#include "DifficultyLevel.h"
#include "Diplomacy.h"
#include "Formation.h"
#include "Artillery.h"

#include "TimeCounter.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CStaticMap theStaticMap;
extern CUpdater updater;
extern NTimer::STime curTime;
extern CShellsStore theShellsStore;
extern SCheats theCheats;
extern CWeather theWeather;
extern CDifficultyLevel theDifficultyLevel;
extern CDiplomacy theDipl;

extern CTimeCounter timeCounter;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float GetDispByRadius( const float fDispRadius, const float fRangeMax, const float fDist )
{
	return fDispRadius / fRangeMax * fDist;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float GetDispByRadius( const CBasicGun *pGun, const float fDist )
{
	return GetDispByRadius( pGun->GetDispersion(), pGun->GetFireRangeMax(), fDist );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float GetDispByRadius( const CBasicGun *pGun, const CVec2 &attackerPos, const CVec2 &explCoord )
{
	return GetDispByRadius( pGun->GetDispersion(), pGun->GetFireRangeMax(), fabs( attackerPos - explCoord ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CBasicGun );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													  CGun																	*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CGun::CGun( class CAIUnit *_pOwner, const BYTE _nShellType, SCommonGunInfo *_pCommonGunInfo, const IGunsFactory::EGunTypes _eType )
: pOwner( _pOwner ), shootState( EST_REST ), nShellType( _nShellType ), bAngleLocked( false ), bCanShoot( true ), pCommonGunInfo( _pCommonGunInfo ), eType( _eType ),
	eRejectReason( ACK_NONE ), bWaitForReload( false ), lastCheck( 0 ), bParallelGun( false ), lastCheckTurnTime( 0 )
{
	SetUniqueId();
	bGrenade = pOwner && pOwner->GetStats()->IsInfantry() && pCommonGunInfo->nGun == 1;

	pEnemy = 0;
	z = 0;
	
	InitRandoms();

	nOwnerParty = pOwner ? pOwner->GetParty() : 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::InitRandoms()
{
	fRandom4Aim = Random( 1.0f, SConsts::COEFF_FOR_RANDOM_DELAY );
	fRandom4Relax = Random( 1.0f, SConsts::COEFF_FOR_RANDOM_DELAY );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::IsAlive() const 
{ 
	return GetOwner()->IsAlive(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::GetMechShotInfo( SAINotifyMechShot *pMechShotInfo, const NTimer::STime &time ) const
{
	pOwner->GetShotInfo( pMechShotInfo );

	pMechShotInfo->cGun = pCommonGunInfo->nGun;
	pMechShotInfo->cShell = nShellType;
	pMechShotInfo->time = time;
	pMechShotInfo->vDestPos = vLastShotPoint;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::GetInfantryShotInfo( SAINotifyInfantryShot *pInfantryShotInfo, const NTimer::STime &time ) const
{
	NI_ASSERT_T( pOwner->GetStats()->IsInfantry(), "Wrong unit type" );

	if ( pCommonGunInfo->nGun == 1 )
		static_cast<CSoldier*>(pOwner)->GetThrowInfo( pInfantryShotInfo );
	else
		pOwner->GetShotInfo( pInfantryShotInfo );

	pInfantryShotInfo->cShell = nShellType;
	pInfantryShotInfo->pWeapon = GetWeapon();
	pInfantryShotInfo->time = time;
	pInfantryShotInfo->vDestPos = vLastShotPoint;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const NTimer::STime CGun::GetActionPoint() const
{
	if ( pOwner->GetStats()->IsInfantry() && pCommonGunInfo->nGun == 1 )
		return pOwner->GetStats()->GetAnimActionTime( GetAnimationFromAction( static_cast<CSoldier*>(pOwner)->GetThrowAction() ) );
	else
		return pOwner->GetStats()->GetAnimActionTime( GetAnimationFromAction( pOwner->GetShootAction() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanBreakArmor( CAIUnit *pTarget ) const
{
	int nSide ;
	if ( pOwner->GetZ() > pTarget->GetZ() ) // стрельба из самолета по крышам 
	{
		nSide = RPG_TOP;
	}
	else
	{
		nSide = pTarget->GetUnitRect().GetSide( pOwner->GetCenter() );
	}
	return CanBreach( pTarget, nSide );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootByHeight( const float fZ ) const
{
	const bool bCeilingOK = fabs( GetOwner()->GetZ() - fZ ) <= GetWeapon()->nCeiling ||
												  !pOwner->GetStats()->IsAviation() && fZ <= 0.0f;
	return bCeilingOK;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootByHeight( CAIUnit *pTarget ) const
{
	const float fTargetZ = pTarget->GetZ();
	return CanShootByHeight( fTargetZ );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CGun::GetFireRangeMax() const
{
	const float fExpCoeff = pOwner ? pOwner->GetExpLevel().fBonusFireRange : 1.0f;
	return GetWeapon()->fRangeMax * ( 1 + (SConsts::BAD_WEATHER_FIRE_RANGE_COEFFICIENT - 1) * (int)theWeather.IsActive() ) * fExpCoeff;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CGun::GetFireRange( float fZ ) const
{
	if (	GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE &&
				!GetOwner()->GetStats()->IsAviation() &&
				( GetOwner()->GetStats()->type != RPG_TYPE_ART_AAGUN || fZ == 0.0f ) )
	{
		return Min( GetFireRangeMax(), SConsts::MAX_FIRE_RANGE_TO_SHOOT_BY_LINE );
	}
	else
		return GetFireRangeMax();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::InFireRange( const CVec3 &vPoint ) const
{
	const float fDist = fabs2( (pOwner->GetTile() - AICellsTiles::GetTile( vPoint.x, vPoint.y )).ToCVec2() );

	float fMaxRange = GetFireRange( vPoint.z );
	fMaxRange /= SConsts::TILE_SIZE;

	return 
		fDist <= sqr( fMaxRange ) &&
		( pOwner->GetZ() <= 0.0f && vPoint.z > 0.0f || fDist >= sqr( GetWeapon()->fRangeMin / SConsts::TILE_SIZE ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::InFireRange( CAIUnit *pTarget ) const
{
	const CVec2 vPoint = pTarget->GetCenter();
	float fDist = fabs( (pOwner->GetTile() - AICellsTiles::GetTile( vPoint )).ToCVec2() );

	float fMaxRange;
	if ( pTarget->GetStats()->IsAviation() || pOwner->GetStats()->IsAviation() )
		fMaxRange = GetFireRangeMax() / SConsts::TILE_SIZE;
	else
	{
		if ( GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
			fMaxRange = Min( GetFireRange( pTarget->GetZ() ), SConsts::MAX_FIRE_RANGE_TO_SHOOT_BY_LINE );
		// CRAP{ for gamedesing testing
//			fMaxRange = Min( GetFireRange( pTarget->GetZ() ), GetOwner()->GetSightRadius() );
		// CRAP}
		else
			fMaxRange = GetFireRange( pTarget->GetZ() );

		fMaxRange /= SConsts::TILE_SIZE;
	}

	float fDist4MaxRange;
	const bool bIsTargetInGunCrew = 
		pTarget->GetFormation() && pTarget->GetFormation()->GetState() && pTarget->GetFormation()->GetState()->GetName() == EUSN_GUN_CREW_STATE;

	if ( pOwner->IsMoving() && pTarget->CanMove() && !pTarget->NeedDeinstall() && !bIsTargetInGunCrew )
	{
		fDist4MaxRange = sqr( fDist + 3.0f );
		fDist *= fDist;
	}
	else
		fDist = fDist4MaxRange = sqr( fDist );

	return 
		fDist4MaxRange <= sqr( fMaxRange ) && 
		( pOwner->GetZ() <= 0.0f && pTarget->GetZ() > 0.0f || fDist >= sqr( GetWeapon()->fRangeMin / SConsts::TILE_SIZE ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::InGoToSideRange( const CAIUnit *pTarget ) const
{
	const float fDist = fabs2( pOwner->GetCenter() - pTarget->GetCenter() );
	return ( fDist <= sqr( 2 * GetFireRange(pTarget->GetZ()) ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::TooCloseToFire( const CAIUnit *pTarget ) const
{
	if ( pOwner->GetZ() <= 0.0f && pTarget->GetZ() > 0.0f )
		return false;
	else
	{
		const float fDist = SquareOfDistance( pOwner->GetTile(), pTarget->GetTile() );
		return ( fDist < fabs2( GetWeapon()->fRangeMin / SConsts::TILE_SIZE ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::TooCloseToFire( const CVec3 &vPoint ) const
{
	if ( pOwner->GetZ() <= 0.0f && vPoint.z > 0.0f )
		return false;
	else
	{
		const float fDist = SquareOfDistance( pOwner->GetTile(), AICellsTiles::GetTile( vPoint.x, vPoint.y ) );
		return ( fDist < fabs2( GetWeapon()->fRangeMin / SConsts::TILE_SIZE ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::ToRestState()
{
	shootState = EST_REST;
	lastCheck = curTime;
	pCommonGunInfo->bFiring = false;
	target = VNULL2;

	StopTracing();
	
	if ( CTurret *pTurret = GetTurret() )
	{
		if ( !pTurret->IsLocked( this ) )
			pTurret->StopTurning();
	}

	for ( CParallelGuns::iterator iter = parallelGuns.begin(); iter != parallelGuns.end(); ++iter )
		(*iter)->StopFire();
/*
	if ( IsValidObj( pEnemy ) )
		TraceAim( pEnemy );
*/
	pEnemy = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::Turning()
{
	bool bRightDir = false;
	const CVec2 pos2Turn = GetShootingPoint();

	if ( pEnemy != 0 )
		bRightDir = TurnGunToEnemy( pos2Turn, pEnemy->GetZ() - pOwner->GetZ() );
	else
		bRightDir = TurnGunToEnemy( pos2Turn, z );

	lastEnemyPos = pos2Turn;

	if ( bRightDir )
	{
		lastCheck = curTime;
		shootState = EST_AIMING;
		updater.Update( pOwner->GetAimAction(), pOwner );

		lastEnemyPos = VNULL2;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::IsFiring() const 
{ 
	return shootState != EST_REST; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::Aiming()
{
	// враг убежал из радиуса обстрела
	if ( pEnemy != 0 && !InFireRange( pEnemy ) )
		StopFire();
	// враг убежал из прицела
	else if ( !CanShootWOGunTurn( 1, z ) )
	{
		bAim = true;
		shootState = EST_TURNING;
	}
	// прицелились и перезарядились
	else 
	{
		// чтобы не сразу стрелять, а перезаряжаться после подвоза патронов
		if ( GetNAmmo() == 0 )
		{
			if ( pOwner->GetStats()->IsAviation() )			// but fuckin PLANES mustn't wait
				shootState = EST_REST;
			lastCheck = curTime;
		}

		const int nAimingTime = GetAimTime();
		const bool b1 = curTime - lastCheck >= nAimingTime * bAim ;
		const bool b2 = curTime - pCommonGunInfo->lastShoot >= GetRelaxTime() + GetWeapon()->nAimingTime * bAim;

		if (  b1 && b2 && bCanShoot && GetNAmmo() > 0 )
		{
			updater.Update( ACTION_NOTIFY_DELAYED_SHOOT, pOwner );

			NI_ASSERT_T( pEnemy == 0 || pEnemy->IsValid() && pEnemy->IsAlive(), "Dead enemy" );
			if ( pEnemy != 0 )
			{
				target = pEnemy->GetCenter();
				z = pEnemy->GetZ();
			}


			shootState = WAIT_FOR_ACTION_POINT;
			lastCheck = curTime;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::WaitForActionPoint()
{
	if ( curTime - lastCheck >= GetActionPoint() )
	{
		shootState = EST_SHOOTING;
		pOwner->RemoveCamouflage( ECRR_SELF_SHOOT );
		nShotsLast = GetWeapon()->nAmmoPerBurst;
		pCommonGunInfo->lastShoot = curTime - GetFireRate();

		vLastShotPoint = CVec3( target, z );

		if ( pOwner->GetStats()->IsInfantry() )
			updater.Update( ACTION_NOTIFY_INFANTRY_SHOOT, this );
		// для зениток выстрел нужно присылать на каждый снаряд, action point отсутствует
		else if ( pOwner->GetStats()->type != RPG_TYPE_ART_AAGUN && pOwner->GetStats()->type != RPG_TYPE_ART_ROCKET )
			updater.Update( ACTION_NOTIFY_MECH_SHOOT, this );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::Shooting()
{
	// время для выстрела и ещё есть патроны в очереди
	while ( curTime - pCommonGunInfo->lastShoot >= GetFireRate() && nShotsLast > 0 )
	{
		if ( pOwner->GetStats()->type == RPG_TYPE_ART_AAGUN || pOwner->GetStats()->type == RPG_TYPE_ART_ROCKET )
			updater.Update( ACTION_NOTIFY_MECH_SHOOT, this );
		
		--nShotsLast;
		Fire( target, z );
		pCommonGunInfo->lastShoot += GetFireRate();
		
		--pCommonGunInfo->nAmmo;
		updater.Update( ACTION_NOTIFY_RPG_CHANGED, GetOwner() );
	}
	if ( nShotsLast == 0 || pCommonGunInfo->nAmmo == 0 )
	{
		bWaitForReload = true;

		if ( !bParallelGun )
			ToRestState();
		else
		{
			shootState = EST_REST;
			lastCheck = curTime;
			pCommonGunInfo->bFiring = false;

			if ( pEnemy != 0 )
			{
				if ( IsValidObj( pEnemy ) && pEnemy->IsAlive() )
					StartEnemyBurst( pEnemy, false );
			}
			else
				StartPointBurst( CVec3( target, z ), false );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CGun::GetShootingPoint() const
{
	return ( pEnemy == 0 ) ? target : pEnemy->GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CGun::GetVisAngleOfAim() const
{
	if ( pEnemy == 0 )
		return 0;
	else
		return GetVisibleAngle( pOwner->GetCenter(), pEnemy->GetUnitRect() ) / 2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootWOGunTurn( const BYTE cDeltaAngle, const float fZ )
{
	return 
		pEnemy == 0 && IsGoodAngle( target, 0, fZ, cDeltaAngle ) && CanShootToPointWOMove( target, fZ ) ||
		pEnemy != 0 && CanShootWOGunTurn( pEnemy, cDeltaAngle );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootToTargetWOMove()
{
	return	
		( pEnemy != 0 && CanShootToUnitWOMove( pEnemy ) ) 
		||
		( pEnemy == 0 && CanShootToPointWOMove( target, z ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::SetOwner( CAIUnit *_pOwner )
{
	pOwner = _pOwner;
	nOwnerParty = pOwner ? pOwner->GetParty() : 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::Segment()
{
	NI_ASSERT_T( !pOwner || nOwnerParty == pOwner->GetParty(), "Wrong owner party" );

	// врага убили
	if ( shootState != EST_REST && pEnemy != 0 && !IsValidObj( pEnemy ) )
	{
		// момент выпускания очереди - стрельбу не прерывать 
		if ( shootState == EST_SHOOTING || shootState == WAIT_FOR_ACTION_POINT )
			pEnemy == 0;
		else
			ToRestState();
	}
	
	if ( shootState != EST_REST && shootState != EST_TURNING && shootState != EST_AIMING &&
			 !pOwner->GetStats()->IsAviation() && GetGun().nPriority == 0 && !pOwner->IsIdle() )
		ToRestState();

	bool bTryFastShot = false;
	switch ( shootState )
	{
		case EST_REST:
			Rest();

			break;
		case EST_TURNING:
			if ( pEnemy != 0 && !pEnemy->IsVisible( nOwnerParty ) )
				ToRestState();
			else
			{
				OnTurningState();
				if ( EST_TURNING != shootState )
					bTryFastShot = true;
			}

			break;
		case EST_AIMING:
			if ( pEnemy != 0 && !pEnemy->IsVisible( nOwnerParty ) )
				ToRestState();
			else
			{
				OnAimState();
				if ( EST_AIMING != shootState )
					bTryFastShot = true;
			}

			break;
		case WAIT_FOR_ACTION_POINT:
			OnWaitForActionPointState();
			if ( WAIT_FOR_ACTION_POINT != shootState )
				bTryFastShot = true;

			break;
		case EST_SHOOTING:
			Shooting();

			break;
	}

	if ( bTryFastShot )
	{
		if ( EST_AIMING == shootState )
			OnAimState();

		if ( WAIT_FOR_ACTION_POINT == shootState )
			OnWaitForActionPointState();

		if ( EST_SHOOTING == shootState )
			Shooting();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::OnWaitForActionPointState()
{
	WaitForActionPoint();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::OnTurningState()
{
	if ( AnalyzeTurning() )
		shootState = EST_AIMING;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::OnAimState()
{
	if ( !CanShootToTargetWOMove() )
		StopFire();
	else
		Aiming();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::StartPlaneBurst( CAIUnit *_pEnemy, bool bReAim )
{
	if ( _pEnemy && _pEnemy->IsValid() && _pEnemy->IsAlive() )
	{
		pEnemy = _pEnemy;
		lastCheck = curTime;
		bAim = bReAim;
		lastEnemyPos = VNULL2;
		target = pEnemy->GetCenter();
		z = pEnemy->GetZ();

		StopTracing();	
		
		if ( IsGoodAngle( pEnemy->GetCenter(), 
											GetVisibleAngle( pOwner->GetCenter(), pEnemy->GetUnitRect() ) / 2, 
											pEnemy->GetZ() - pOwner->GetZ(), 
											!bReAim || pOwner->GetStats()->IsAviation() ) )
		{
			shootState = EST_AIMING;
			if ( bAim || curTime - pCommonGunInfo->lastShoot < GetRelaxTime() )
				updater.Update( pOwner->GetAimAction(), pOwner );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::StartPointBurst( const CVec3 &_target, bool bReAim )
{
	if ( !(pCommonGunInfo->bFiring) && ( shootState == EST_REST || pEnemy != 0 || CVec3( target, z ) != _target ) )
	{
		target.x = _target.x;
		target.y = _target.y;
		z = _target.z;
		lastCheck = curTime;
		bAim = bReAim;
		lastEnemyPos = VNULL2;
		pCommonGunInfo->bFiring = true;

		StopTracing();

		if ( IsGoodAngle( target, 0, z, !bReAim || pOwner->GetStats()->IsAviation() ) )
		{
			shootState = EST_AIMING;

			if ( bAim || curTime - pCommonGunInfo->lastShoot < GetRelaxTime() )
				updater.Update( pOwner->GetAimAction(), pOwner );
		}
		else
		{
			bAim = true;
			shootState = EST_TURNING;
		}

		pEnemy = 0;
		
		for ( CParallelGuns::iterator iter = parallelGuns.begin(); iter != parallelGuns.end(); ++iter )
			(*iter)->StartPointBurst( _target, bReAim );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::StartPointBurst( const CVec2 &_target, bool bReAim )
{
	if ( !(pCommonGunInfo->bFiring) && ( shootState == EST_REST || pEnemy != 0 || target != _target ) )
	{
		target = _target;
		z = 0;
		lastCheck = curTime;
		bAim = bReAim;
		lastEnemyPos = VNULL2;
		pCommonGunInfo->bFiring = true;

		StopTracing();

		if ( IsGoodAngle( target, 0, 0, !bReAim || pOwner->GetStats()->IsAviation() ) )
		{
			shootState = EST_AIMING;

			if ( bAim || curTime - pCommonGunInfo->lastShoot < GetRelaxTime() )
				updater.Update( pOwner->GetAimAction(), pOwner );
		}
		else
		{
			bAim = true;
			shootState = EST_TURNING;
		}

		pEnemy = 0;

		for ( CParallelGuns::iterator iter = parallelGuns.begin(); iter != parallelGuns.end(); ++iter )
			(*iter)->StartPointBurst( _target, bReAim );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::StartEnemyBurst( CAIUnit *_pEnemy, bool bReAim )
{
	if ( IsValidObj( _pEnemy ) && !(pCommonGunInfo->bFiring) && ( shootState == EST_REST || pEnemy.GetPtr() != _pEnemy ) )
	{
		pEnemy = _pEnemy;
		lastCheck = curTime;
		bAim = bReAim;
		lastEnemyPos = VNULL2;
		pCommonGunInfo->bFiring = true;
		target = pEnemy->GetCenter();
		z = pEnemy->GetZ();

		StopTracing();

		if ( IsGoodAngle( pEnemy->GetCenter(), GetVisibleAngle( pOwner->GetCenter(), pEnemy->GetUnitRect() ) / 2, pEnemy->GetZ() - pOwner->GetZ(), !bReAim|| pOwner->GetStats()->IsAviation() ) )
		{
			shootState = EST_AIMING;

			if ( bAim || curTime - pCommonGunInfo->lastShoot < GetRelaxTime() )
				updater.Update( pOwner->GetAimAction(), pOwner );
		}
		else
		{
			bAim = true;
			shootState = EST_TURNING;
		}

		for ( CParallelGuns::iterator iter = parallelGuns.begin(); iter != parallelGuns.end(); ++iter )
			(*iter)->StartEnemyBurst( _pEnemy, bReAim );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SBaseGunRPGStats& CGun::GetGun() const
{
	NI_ASSERT_T( pOwner->IsValid(), "Wrong owner. Can't get gun info" );
	return pOwner->GetStats()->GetGun( pCommonGunInfo->nGun );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SWeaponRPGStats* CGun::GetWeapon() const 
{ 
	NI_ASSERT_T( pOwner->IsValid(), "Wrong owner. Can't get RPG stats" );
	return pOwner->GetStats()->GetGun( pCommonGunInfo->nGun ).pWeapon;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SWeaponRPGStats::SShell& CGun::GetShell() const 
{ 
	NI_ASSERT_T( pOwner->IsValid(), "Wrong owner. Can't get shell info" );	
	return GetWeapon()->shells[nShellType];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::IsRelaxing() const
{
	return curTime - pCommonGunInfo->lastShoot < GetRelaxTime();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootWOGunTurn( CAIUnit *pEnemy, const BYTE cDeltaAngle )
{
	return
		CanShootToUnitWOMove( pEnemy ) && 
		IsGoodAngle( pEnemy->GetCenter(), GetVisibleAngle( pOwner->GetCenter(), pEnemy->GetUnitRect() ) / 2, pEnemy->GetZ() - pOwner->GetZ(), cDeltaAngle );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const NTimer::STime CGun::GetRestTimeOfRelax() const
{
	return 
		Max( GetRelaxTime() - ( curTime - pCommonGunInfo->lastShoot ), 0.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::AnalyzeLimitedAngle( class CCommonUnit *pUnit, const CVec2 &point ) const
{
	NI_ASSERT_T( dynamic_cast<CSoldier*>(pUnit) != 0, "Wrong unit to analyze limited angle" );
	CSoldier *pSoldier = static_cast<CSoldier*>( pUnit );

	if ( pSoldier->IsAngleLimited() )
		return IsInTheAngle( GetDirectionByVector( point - pSoldier->GetCenter() ), pSoldier->GetMinAngle(), pSoldier->GetMaxAngle() );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootToUnitWOMove( CAIUnit *pEnemy )
{
	if ( !pEnemy || !pEnemy->IsAlive() || pEnemy == GetOwner() ||  pEnemy->GetState()->GetName() == EUSN_PARTROOP )
	{
		SetRejectReason( ACK_INVALID_TARGET );
		return false;
	}

	if ( !CanShootToPointWOMove( pEnemy->GetCenter(), pEnemy->GetZ(), GetVisibleAngle( pOwner->GetCenter(), pEnemy->GetUnitRect() ) / 2, pEnemy->GetZ() - pOwner->GetZ(), pEnemy ) )
		return false;

	if ( pEnemy->GetStats()->IsInfantry() && !AnalyzeLimitedAngle( pEnemy, pOwner->GetCenter() ) )
	{
		SetRejectReason( ACK_ENEMY_ISNT_IN_FIRE_SECTOR );
		return false;
	}

	if ( !CanShootByHeight( pEnemy ) )
	{
		SetRejectReason( ACK_NOT_IN_FIRE_RANGE );
		return false;
	}

	if ( !CanBreakArmor( pEnemy ) )
	{
		SetRejectReason( ACK_CANNOT_PIERCE );
		return false;
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootToUnit( CAIUnit *pEnemy )
{
	if ( !pEnemy || !pEnemy->IsAlive() || pEnemy == GetOwner() )
	{
		SetRejectReason( ACK_INVALID_TARGET );
		return false;
	}
	
	if ( !pOwner->CanMove() || pOwner->NeedDeinstall() || GetOwner()->IsLocked( this ) )
		return CanShootToUnitWOMove( pEnemy );
	
	if ( GetNAmmo() == 0 && pOwner->CanMove() && !pOwner->NeedDeinstall() && GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
	{
		SetRejectReason( ACK_NO_AMMO );
		return false;
	}

	if ( !CanShootByHeight( pEnemy ) )
	{
		SetRejectReason( ACK_NOT_IN_FIRE_RANGE );
		return false;
	}
	
	if ( !CanBreach( pEnemy ) )
	{
		SetRejectReason( ACK_CANNOT_PIERCE );
		return false;
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootToObjectWOMove( CStaticObject *pObj )
{
	if ( !pObj->IsValid() || !pObj->IsAlive() )
	{
		SetRejectReason( ACK_INVALID_TARGET );
		return false;
	}

	const CVec2 vOwnerCenter( pOwner->GetCenter() );
	if ( !CanShootToPointWOMove( pObj->GetAttackCenter( vOwnerCenter ), 0.0f ) )
		return false;

	// проверка на возможность пробивания брони
	{
		SRect boundRect;
		pObj->GetBoundRect( &boundRect );
		const WORD wDir2Obj( GetDirectionByVector( pObj->GetAttackCenter( vOwnerCenter ) - vOwnerCenter ) );
		
		const int nSide = IsBallisticTrajectory() ? RPG_TOP : boundRect.GetSide( wDir2Obj );
		if ( GetMaxPossiblePiercing() < pObj->GetStats()->GetMinPossibleArmor( nSide ) )
		{
			//SetRejectReason( ACK_CANNOT_PIERCE );
			// don't want to hear this ack on buildings
			SetRejectReason( ACK_NEGATIVE );
			return false;
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootToObject( CStaticObject *pObj )
{
	if ( !pObj->IsValid() || !pObj->IsAlive() )
	{
		SetRejectReason( ACK_INVALID_TARGET );
		return false;
	}

	if ( !pOwner->CanMove() || pOwner->NeedDeinstall() || pOwner->IsLocked( this ) )
		return CanShootToObjectWOMove( pObj );

	if ( GetNAmmo() == 0 && pOwner->CanMove() && !pOwner->NeedDeinstall() && GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
	{
		SetRejectReason( ACK_NO_AMMO );
		return false;
	}

	if ( GetShell().trajectory != SWeaponRPGStats::SShell::TRAJECTORY_LINE && GetMaxPossiblePiercing() < pObj->GetStats()->defences[RPG_TOP].nArmorMin ||
			 GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE && GetMaxPossiblePiercing() < pObj->GetStats()->GetMinPossibleArmor( RPG_FRONT ) )
	{
		//SetRejectReason( ACK_CANNOT_PIERCE );
		// don't want to hear this ack on buildings
		SetRejectReason( ACK_NEGATIVE );
		return false;
	}

	SetRejectReason( ACK_NONE );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootToPointWOMove( const CVec2 &point, const float fZ, const WORD wHorAddAngle, const WORD wVertAddAngle, CAIUnit *pEnemy )
{
	const CVec3 v3DTarget( point, fZ );
	if ( !pEnemy && !InFireRange( v3DTarget ) || pEnemy && !InFireRange( pEnemy ) )
	{
		if ( TooCloseToFire( v3DTarget ) )
			SetRejectReason( ACK_ENEMY_IS_TO_CLOSE );
		else
			SetRejectReason( ACK_NOT_IN_FIRE_RANGE );

		return false;
	}

	if ( !CanShootByHeight( fZ ) )
	{
		SetRejectReason( ACK_NOT_IN_FIRE_RANGE );
		return false;
	}

	// нельзя вращать базу
	if ( !pOwner->CanRotate() && !pOwner->CanMove() || pOwner->NeedDeinstall() || pOwner->IsLocked( this ) )
	{
		if ( !IsOnTurret() || IsOnTurret() && GetTurret()->IsLocked( this ) ) // нельзя вращать turret, или gun на базе
		{
			if ( !IsGoodAngle( point, wHorAddAngle, wVertAddAngle, 1 ) )
			{
				SetRejectReason( ACK_NOT_IN_ATTACK_ANGLE );
				return false;
			}
		}
		else if ( !IsInShootCone( point, wHorAddAngle ) ) 
		{
			SetRejectReason( ACK_NOT_IN_ATTACK_ANGLE );
			return false;
		}
	}

	if ( pOwner->GetStats()->IsInfantry() && !AnalyzeLimitedAngle( pOwner, point ) )
	{
		SetRejectReason( ACK_ENEMY_ISNT_IN_FIRE_SECTOR );
		return false;
	}

	if ( GetNAmmo() == 0 && pOwner->CanMove() && !pOwner->NeedDeinstall() && GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
	{
		SetRejectReason( ACK_NO_AMMO );
		return false;
	}

	SetRejectReason( ACK_NONE );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanShootToPoint( const CVec2 &point, const float fZ, const WORD wHorAddAngle, const WORD wVertAddAngle )
{
	if ( !pOwner->CanMove() || pOwner->NeedDeinstall() || pOwner->IsLocked( this ) )
		return CanShootToPointWOMove( point, fZ, wHorAddAngle, wVertAddAngle );

	if ( GetNAmmo() == 0 && pOwner->CanMove() && !pOwner->NeedDeinstall() && GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
	{
		SetRejectReason( ACK_NO_AMMO );
		return false;
	}

	SetRejectReason( ACK_NONE );	
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::IsInShootCone( const CVec2 &point, const WORD wAddAngle ) const
{
	if ( !pOwner->InVisCone( point ) )
		return false;
	else
	{
		const WORD dirToPoint = GetDirectionByVector( point - pOwner->GetCenter() );

		if ( IsOnTurret() && !GetTurret()->IsLocked( this ) )
			return DirsDifference( dirToPoint, pOwner->GetFrontDir() ) <= GetHorTurnConstraint() + (int)wAddAngle + (int)GetWeapon()->wDeltaAngle;
		else
			return DirsDifference( dirToPoint, pOwner->GetFrontDir() - GetGun().wDirection ) <= (int)wAddAngle + (int)GetWeapon()->wDeltaAngle;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CGun::GetDispRatio( byte nShellType, const float fDist ) const
{
	int eTraj = GetWeapon()->shells[nShellType].trajectory;
	float fMax = GetFireRangeMax();
	float fMin = GetWeapon()->fRangeMin;

	float fMaxDisp = SConsts::dispersionRatio[eTraj][0];
	float fMinDisp = SConsts::dispersionRatio[eTraj][1];

	return fMinDisp + (fDist-fMin)/(fMax-fMin)*(fMaxDisp-fMinDisp);

	return 4;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CalculateMoraleCoeff( const float fMorale, const float fCoeff )
{
	return ( fMorale + fCoeff * ( 1.0f - fMorale ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CGun::GetDispersion() const
{
	const float fDispWithoutMorale =
		theDifficultyLevel.GetDispersionCoeff( theDipl.GetNParty( pOwner->GetPlayer() ) ) *
		GetWeapon()->fDispersion * pOwner->GetExpLevel().fBonusDispersion * pOwner->GetDispersionBonus();

	if ( pOwner->GetStats()->type == RPG_TYPE_ART_ROCKET )
		return fDispWithoutMorale;
	else
		return fDispWithoutMorale / CalculateMoraleCoeff( pOwner->GetMorale(), SConsts::MORALE_DISPERSION_COEFF );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CGun::GetAimTime( bool bRandomize ) const 
{
	float fAimTime = float( GetWeapon()->nAimingTime ) * pOwner->GetAimTimeBonus();
	fAimTime /= CalculateMoraleCoeff( pOwner->GetMorale(), SConsts::MORALE_AIMING_COEFF );

	if ( bRandomize )
		fAimTime *= fRandom4Aim;

	return fAimTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CGun::GetRelaxTime( bool bRandomize ) const
{
	float fRelaxTime = 
		float(GetShell().nRelaxTime) * pOwner->GetRelaxTimeBonus() * pOwner->GetExpLevel().fBonusRelaxTime;

	fRelaxTime /= CalculateMoraleCoeff( pOwner->GetMorale(), SConsts::MORALE_RELAX_COEFF );
	
	if ( bRandomize )
		fRelaxTime *= fRandom4Relax;

	return fRelaxTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CGun::GetFireRate() const
{
	return GetShell().nFireRate * pOwner->GetFireRateBonus();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanBreach( const CCommonUnit *pTarget ) const
{
	if ( pOwner->GetZ() > pTarget->GetZ() ) // сирельба из самолета по крышам 
	{
		return GetMaxPossiblePiercing() >= pTarget->GetArmor( RPG_TOP );
	}
	else if ( GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
		return GetMaxPossiblePiercing() >= pTarget->GetMinArmor();
	else
		return GetMaxPossiblePiercing() >= pTarget->GetArmor( RPG_TOP );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanBreach( const SHPObjectRPGStats *pStats, const int nSide ) const
{
	if ( GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
		return GetMaxPossiblePiercing() >= pStats->GetMinPossibleArmor( nSide );
	else
		return GetMaxPossiblePiercing() >= pStats->GetMinPossibleArmor( RPG_TOP );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::CanBreach( const CCommonUnit *pTarget, const int nSide ) const
{
	if ( GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
		return GetMaxPossiblePiercing() >= pTarget->GetMinPossibleArmor( nSide );
	else
		return GetMaxPossiblePiercing() >= pTarget->GetMinPossibleArmor( RPG_TOP );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::IsCommonEqual( const CBasicGun *pGun ) const
{
	return pGun != 0 && pOwner == pGun->GetOwner() && GetCommonGunNumber() == pGun->GetCommonGunNumber();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IBallisticTraj* CGun::CreateTraj( const CVec2 &vTarget ) const
{
	switch ( eType )
	{
		case IGunsFactory::MOMENT_CML_GUN:
		case IGunsFactory::MOMENT_BURST_GUN:
			return 0;

		case IGunsFactory::VIS_CML_BALLIST_GUN:
		case IGunsFactory::VIS_BURST_BALLIST_GUN:
			return new CBallisticTraj( CVec3( pOwner->GetCenter(), 0 ), vTarget, GetShell().fSpeed, GetShell().trajectory, GetVerTurnConstraint(), GetFireRange(z) );
			break;
		case IGunsFactory::PLANE_GUN:
			NI_ASSERT_T( false, "wrong call " );
			break;
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::Fire( const CVec2 &target, const float z )
{
	const CVec2 vOwnerCenter = pOwner->GetCenter();
	switch ( eType )
	{
		case IGunsFactory::MOMENT_CML_GUN:
			theShellsStore.AddShell( 
				new CInvisShell( 
					curTime + fabs( target - vOwnerCenter ) / GetShell().fSpeed, 
					new CCumulativeExpl( pOwner, this, target, z, vOwnerCenter, nShellType ), 
					GetCommonGunNumber() 
				)
			);

			break;
		case IGunsFactory::MOMENT_BURST_GUN:
			theShellsStore.AddShell( 
				new CInvisShell( 
					curTime + fabs( target - vOwnerCenter ) / GetShell().fSpeed, 
					new CBurstExpl( pOwner, this, target, z, vOwnerCenter, nShellType ),
					GetCommonGunNumber() 
				)
			);

			break;
		case IGunsFactory::VIS_CML_BALLIST_GUN:
			{
				CPtr<CCumulativeExpl> pExpl = new CCumulativeExpl( pOwner, this, target, z, vOwnerCenter, nShellType );
				theShellsStore.AddShell( new CVisShell( pExpl, CreateTraj( pExpl->GetExplCoordinates() ), GetCommonGunNumber() ) );
			}

			break;
		case IGunsFactory::VIS_BURST_BALLIST_GUN:
			{
				CPtr<CBurstExpl> pExpl = new CBurstExpl( pOwner, this, target, z, vOwnerCenter, nShellType );
				theShellsStore.AddShell( new CVisShell( pExpl, CreateTraj( pExpl->GetExplCoordinates() ), GetCommonGunNumber() ) );
			}

			break;
		case IGunsFactory::PLANE_GUN:
			{
				NI_ASSERT_T( dynamic_cast<CAviation*>(pOwner)!=0, NStr::Format("unit %s weapon %s: only planes can shoot with bombs", pOwner->GetStats()->szParentName.c_str(), GetWeapon()->szParentName.c_str()) );
				CAviation *pAvia = static_cast<CAviation*>(pOwner);
				CVec2 vSpeed2( pAvia->GetSpeed() );
				CVec2 vSpeedHorVer( pAvia->GetSpeedHorVer() );
				Normalize( &vSpeed2 );
				vSpeed2 *= vSpeedHorVer.x;
				CVec3 vSpeed3( vSpeed2.x, vSpeed2.y, vSpeedHorVer.y );
				const CVec3 vOwnerCenter3D( vOwnerCenter, pOwner->GetZ() );

				const float fTimeToFly = CBombBallisticTraj::GetTimeOfFly( vOwnerCenter3D.z, vSpeed3.z );

				const float fDispRadius = GetDispersion() * pOwner->GetZ() / GetFireRangeMax();
				const float fAcceleration = fDispRadius * 2 / sqr( fTimeToFly );
				CVec2 vRandAcc = VNULL2;
				RandQuadrInCircle( fAcceleration, &vRandAcc );

				IBallisticTraj *pTraj = new CBombBallisticTraj( vOwnerCenter3D, vSpeed3, curTime + fTimeToFly, vRandAcc );
				const CVec3 vTrajFinish( CBombBallisticTraj::CalcTrajectoryFinish( vOwnerCenter3D, vSpeed3, vRandAcc ) );

				theShellsStore.AddShell
				(
					new CVisShell
					( 
						new CBurstExpl( pOwner, this, 
														CVec2( vTrajFinish.x, vTrajFinish.y ), z, vOwnerCenter, 
														nShellType, false, 2 
													),
						pTraj,
						GetCommonGunNumber()
					)
				);
			}

			break;
	}

	pOwner->Fired( GetWeapon()->fRevealRadius, pCommonGunInfo->nGun );

	InitRandoms();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CGun::GetTrajectoryZAngle( const CVec2 &vToAim, const float z ) const
{
	if ( eType == IGunsFactory::VIS_CML_BALLIST_GUN || eType == IGunsFactory::VIS_BURST_BALLIST_GUN )
		return CBallisticTraj::GetTrajectoryZAngle( vToAim, z, GetShell().fSpeed, GetShell().trajectory, GetVerTurnConstraint(), GetFireRange(z) );
	else
		return 16384 * 3;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::SetRejectReason( const EUnitAckType &eReason ) 
{ 
	eRejectReason = eReason;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CGun::GetPiercing() const
{
	return GetShell().nPiercing * theDifficultyLevel.GetPiercingCoeff( nOwnerParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CGun::GetPiercingRandom() const
{
	return GetShell().nPiercingRandom * theDifficultyLevel.GetPiercingCoeff( nOwnerParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CGun::GetMaxPossiblePiercing() const
{
	return GetShell().GetMaxPossiblePiercing() * theDifficultyLevel.GetPiercingCoeff( nOwnerParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CGun::GetMinPossiblePiercing() const
{
	return GetShell().GetMinPossiblePiercing() * theDifficultyLevel.GetPiercingCoeff( nOwnerParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CGun::GetRandomPiercing() const
{
	return GetShell().GetRandomPiercing() * theDifficultyLevel.GetPiercingCoeff( nOwnerParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CGun::GetDamage() const
{
	return GetShell().fDamagePower * theDifficultyLevel.GetDamageCoeff( nOwnerParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CGun::GetDamageRandom() const
{
	return GetShell().nDamageRandom * theDifficultyLevel.GetDamageCoeff( nOwnerParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CGun::GetRandomDamage() const
{
	return GetShell().GetRandomDamage() * theDifficultyLevel.GetDamageCoeff( nOwnerParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGun::IsBallisticTrajectory() const
{
	const SWeaponRPGStats::SShell::ETrajectoryType eTraj = GetShell().trajectory;
	return eTraj == SWeaponRPGStats::SShell::TRAJECTORY_HOWITZER || 
				 eTraj == SWeaponRPGStats::SShell::TRAJECTORY_ROCKET || 
				 eTraj == SWeaponRPGStats::SShell::TRAJECTORY_CANNON;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													 CTurretGun															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTurretGun::CTurretGun( CAIUnit *pOwner, const BYTE nShellType, SCommonGunInfo *pCommonGunInfo, const IGunsFactory::EGunTypes eType, const int nTurret )
: CGun( pOwner, nShellType, pCommonGunInfo, eType ), bCircularAttack( false ), bTurnByBestWay( false )
{ 
	pTurret = pOwner->GetTurret( nTurret );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTurretGun::TraceAim( CAIUnit *pUnit )
{
	GetTurret()->TraceAim( pUnit, this );

	for ( CParallelGuns::iterator iter = parallelGuns.begin(); iter != parallelGuns.end(); ++iter )
		(*iter)->TraceAim( pUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTurretGun::StopTracing()
{
	GetTurret()->StopTracing();
/*
	for ( CParallelGuns::iterator iter = parallelGuns.begin(); iter != parallelGuns.end(); ++iter )
		(*iter)->StopTracing();
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTurretGun::CanShootByHeight( CAIUnit *pTarget ) const
{
	if ( GetTurret()->DoesRotateVert() )
	{
		if ( GetZAngle( pTarget->GetCenter() - pOwner->GetCenter(), pTarget->GetZ() - pOwner->GetZ() ) > GetTurret()->GetVerTurnConstraint() )
			return false;
	}

	return CGun::CanShootByHeight( pTarget );
}	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CTurretGun::CalcVerticalAngle( const class CVec2 &pt, const float z ) const
{
	const WORD wZDesiredAngle = GetZAngle( pt, z ) + GetTrajectoryZAngle( pt, z );

	const WORD wConstraint = GetVerTurnConstraint() + 65535/4 * 3;
	return Min( wConstraint, wZDesiredAngle );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTurretGun::TurnByVer( const CVec2 &vEnemyCenter, const float zDiff )
{
	WORD wZAngle = CalcVerticalAngle( vEnemyCenter-pOwner->GetCenter(), zDiff );
	CTurret *pTurret = GetTurret();

	bool bTurned = false;
	if ( pTurret->IsVerFinished() && wZAngle != pTurret->GetVerCurAngle() || 
			 !pTurret->IsVerFinished() && wZAngle != pTurret->GetVerFinalAngle() )
	{
		pTurret->TurnVer( wZAngle );
		bTurned = ( pTurret->GetVerEndTime() <= curTime + SConsts::AI_SEGMENT_DURATION );
	}
	else
		bTurned = ( pTurret->GetVerCurAngle() == wZAngle );

	if ( bTurned )
		pTurret->StopVerTurning();

	return bTurned;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTurretGun::TurnArtilleryToEnemy( const CVec2 &vEnemyCenter )
{
	const WORD wToEnemy = GetDirectionByVector( vEnemyCenter - pOwner->GetCenter() ) - GetGun().wDirection;
	WORD wDesirableAngle = WORD( wToEnemy - pOwner->GetFrontDir() );
	CTurret *pTurret = GetTurret();

	bool bTurned = false;
	const WORD wHorConstraint = GetHorTurnConstraint();
	// желаемый угол вне contraints на поворот
	if ( wHorConstraint == 0 && wDesirableAngle != 0 ||
			 wDesirableAngle > wHorConstraint && wDesirableAngle < -wHorConstraint )
	{
		if ( DirsDifference( wDesirableAngle, wHorConstraint ) < DirsDifference( wDesirableAngle, -wHorConstraint ) )
			wDesirableAngle = wHorConstraint;
		else
			wDesirableAngle = -wHorConstraint;
	}

	if ( pTurret->IsHorFinished() && wDesirableAngle != pTurret->GetHorCurAngle() ||
 		  !pTurret->IsHorFinished() && wDesirableAngle != pTurret->GetHorFinalAngle() )
	{
		pTurret->TurnHor( wDesirableAngle );
		bTurned = ( pTurret->GetHorEndTime() <= curTime + SConsts::AI_SEGMENT_DURATION / 2 );
	}
	else
		bTurned = ( pTurret->GetHorCurAngle() == wDesirableAngle );

	if ( bTurned )
		pTurret->StopHorTurning();

	return bTurned;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTurretGun::TurnByBestWay( const WORD wDirToEnemy )
{
	bTurnByBestWay = true;	
	
	const WORD wFrontDir = pOwner->GetFrontDir();
	const WORD wBaseTurn = DirsDifference( wFrontDir, wDirToEnemy );
	const WORD wTurretCurAngle = GetTurret()->GetHorCurAngle() + GetGun().wDirection;
	const WORD wTurretGlobalAngle = wFrontDir + wTurretCurAngle;

	const float fBaseSpeed = pOwner->GetRotateSpeed();
	const float fTurretSpeed = GetTurret()->GetHorRotateSpeed();

	WORD wFinalTurretDir;
	if ( IsInTheMinAngle( wTurretGlobalAngle, wFrontDir, wDirToEnemy ) )
	{
		const WORD wCommonTurn = DirsDifference( wTurretGlobalAngle, wDirToEnemy );
		const WORD wResultBaseTurn = fBaseSpeed * wCommonTurn / ( fBaseSpeed + fTurretSpeed );
		const WORD wResultGunTurn = fTurretSpeed * wCommonTurn / ( fBaseSpeed + fTurretSpeed );

		if ( WORD(wDirToEnemy - wTurretGlobalAngle) == wCommonTurn )
		{
			wFinalTurretDir = wTurretCurAngle + wResultGunTurn - GetGun().wDirection;

			if ( DirsDifference( wFinalTurretDir, 0 ) > GetHorTurnConstraint() )
			{
				wBestWayDir = wDirToEnemy - GetHorTurnConstraint() - GetGun().wDirection;
				wFinalTurretDir = GetHorTurnConstraint();
			}
			else
				wBestWayDir = wFrontDir + wResultBaseTurn;
		}
		else
		{
			wFinalTurretDir = wTurretCurAngle - wResultGunTurn - GetGun().wDirection;

			if ( DirsDifference( wFinalTurretDir, 0 ) > GetHorTurnConstraint() )
			{
				wBestWayDir = wDirToEnemy + GetHorTurnConstraint() - GetGun().wDirection;
				wFinalTurretDir = -GetHorTurnConstraint();
			}
			else
				wBestWayDir = wFrontDir - wResultBaseTurn;
		}
	}
	else
	{
		const float fBaseTurnTime = (float)wBaseTurn / fBaseSpeed;
		const float fTurretTurnTime = (float)DirsDifference( wTurretCurAngle, 0 ) / fTurretSpeed;
		const float fTogetherTime = Max( fBaseTurnTime, fTurretTurnTime );

		const WORD wTurretTurn = DirsDifference( wTurretGlobalAngle, wDirToEnemy );
		const float fTurretOnlyTime = (float)wTurretTurn / fTurretSpeed;

		// поворачиваем вместе
		if ( fTogetherTime <= fTurretOnlyTime && DirsDifference( 0, -GetGun().wDirection ) <= GetHorTurnConstraint() )
		{
			wBestWayDir = wDirToEnemy;
			wFinalTurretDir = -GetGun().wDirection;
		}
		// только пушку
		else if ( DirsDifference( wDirToEnemy - wFrontDir - GetGun().wDirection, 0 ) <= GetHorTurnConstraint() )
		{
			wBestWayDir = wFrontDir;
			wFinalTurretDir = wDirToEnemy - wFrontDir - GetGun().wDirection;
		}
		else if ( DirsDifference( GetTurret()->GetHorCurAngle(), GetHorTurnConstraint() ) < DirsDifference( GetTurret()->GetHorCurAngle(), -GetHorTurnConstraint() ) )
		{
			wBestWayDir = wDirToEnemy - GetHorTurnConstraint() - GetGun().wDirection;
			wFinalTurretDir = GetHorTurnConstraint();
		}
		else
		{
			wBestWayDir = wDirToEnemy + GetHorTurnConstraint() - GetGun().wDirection;
			wFinalTurretDir = -GetHorTurnConstraint();
		}
	}

	const WORD wTurretTurnWOBase = wDirToEnemy - wFrontDir - GetGun().wDirection;
	if ( DirsDifference( wFrontDir, wBestWayDir ) <= SConsts::MIN_ROTATE_ANGLE && DirsDifference( wTurretTurnWOBase, 0 ) <= GetHorTurnConstraint() )
	{
		wBestWayDir = wFrontDir;
		wFinalTurretDir = wTurretTurnWOBase;
	}
	
	if ( !pOwner->CanTurnToFrontDir( wBestWayDir ) )
		wBestWayDir = wFrontDir;

	GetTurret()->TurnHor( wFinalTurretDir );

	pOwner->TurnToDir( wBestWayDir );
		
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTurretGun::TurnGunToEnemy( const CVec2 &vEnemyCenter, const float zDiff )
{
	// пушка
	if ( pOwner->NeedDeinstall() || pOwner->IsLocked( this ) || !pOwner->CanRotate() )
	{
		StopTracing();
		
		const bool bHor = TurnArtilleryToEnemy( vEnemyCenter );
		const bool bVer = TurnByVer( vEnemyCenter, zDiff );
		return bHor && bVer;
	}
	else if ( lastEnemyPos != vEnemyCenter || !bTurnByBestWay || lastCheckTurnTime + 3000 < curTime )
	{
		StopTracing();
		
		lastCheckTurnTime = curTime;
		lastEnemyPos = vEnemyCenter;

		const bool bHor = TurnByBestWay( GetDirectionByVector( vEnemyCenter - pOwner->GetCenter() ) );
		const bool bVer = TurnByVer( vEnemyCenter, zDiff );
		return bHor && bVer;
	}
	else
		return pOwner->TurnToDir( wBestWayDir, false ) && GetTurret()->IsFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTurretGun::IsGoodAngle( const CVec2 &point, const WORD addAngle, const float z, const BYTE cDeltaAngle ) const
{
	const WORD wDesirableAngle = WORD( GetDirectionByVector( point - pOwner->GetCenter() ) - GetGun().wDirection - pOwner->GetFrontDir() );
	const WORD wVerAngle = CalcVerticalAngle( point-pOwner->GetCenter(), z-pOwner->GetZ() );

	CTurret *pTurret = GetTurret();
	return 
		DirsDifference( wDesirableAngle, pTurret->GetHorCurAngle() ) <= 
				( (int)GetWeapon()->wDeltaAngle + (int)addAngle ) * cDeltaAngle + SConsts::AI_SEGMENT_DURATION * pTurret->GetHorRotateSpeed()
		&&
		DirsDifference( wVerAngle, pTurret->GetVerCurAngle() ) <= 
				GetWeapon()->wDeltaAngle * cDeltaAngle + SConsts::AI_SEGMENT_DURATION * pTurret->GetVerRotateSpeed();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTurretGun::Rest()
{
	if ( !bAngleLocked && lastCheck != 0 &&
			 curTime - lastCheck >= SConsts::TIME_TO_RETURN_GUN && curTime - lastCheck >= GetRelaxTime( false ) )
	{
		if ( !GetTurret()->IsLocked( this ) )
			GetTurret()->SetCanReturn();
		lastCheck = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const NTimer::STime CTurretGun::GetTimeToShoot( const CVec3 &vPoint ) const 
{ 
	const NTimer::STime nAimingTime = CGun::GetAimTime( false );
	const SWeaponRPGStats::SShell &shell = GetShell();

	const CVec3 vUnitCenter( pOwner->GetCenter(), pOwner->GetZ() );
	const float xDiff = vPoint.x - vUnitCenter.x;
	const float yDiff = vPoint.y - vUnitCenter.y;

	// time is rounded to segment duration
	return (nAimingTime +
					GetActionPoint() + 
	  			fabs( xDiff, yDiff ) / shell.fSpeed ) / SConsts::AI_SEGMENT_DURATION * SConsts::AI_SEGMENT_DURATION;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const NTimer::STime CTurretGun::GetTimeToShootToPoint( const CVec3 &vPoint ) const
{
	const float fVertRotSpeed = pTurret->GetVerRotateSpeed();
	const float fHorRotSpeed = pTurret->GetHorRotateSpeed();

	const WORD wCurVerAngle = pTurret->GetVerCurAngle();
	const WORD wCurHorAngle = pTurret->GetHorCurAngle() + pOwner->GetFrontDir();

	const CVec3 vUnitCenter( pOwner->GetCenter(), pOwner->GetZ() );

	const float xDiff = vPoint.x - vUnitCenter.x;
	const float yDiff = vPoint.y - vUnitCenter.y;
	const float zDiff = vPoint.z - vUnitCenter.z;

	const NTimer::STime timeHorTurn = 
		1.0f * ( DirsDifference( wCurHorAngle, GetDirectionByVector( xDiff,yDiff ) )) / fHorRotSpeed;
	const NTimer::STime timeVerTurn = 
		1.0f * ( DirsDifference( GetDirectionByVector( fabs(xDiff,yDiff), zDiff ), wCurVerAngle ) ) / fVertRotSpeed;
	const NTimer::STime timeToTurn = Max( timeVerTurn, timeHorTurn );

	const float fTime = 
					timeToTurn + GetTimeToShoot( vPoint );

	// time is rounded to segment duration
	return Max( fTime, GetRelaxTime( false ) + CGun::GetAimTime( false ) ) / SConsts::AI_SEGMENT_DURATION * SConsts::AI_SEGMENT_DURATION;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTurretGun::AnalyzeTurning()
{
	if ( !CanShootToTargetWOMove() )
	{
		if ( !GetTurret()->IsLocked( this ) )
			GetTurret()->StopTurning();

		StopFire();
	}
	else
	{
		if ( CanShootWOGunTurn( !bAim, z ) )
			return true;
		else if ( GetTurret()->IsLocked( this ) )
			StopFire();
		else
			Turning();
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTurretGun::StopFire()
{
	pOwner->Unlock( this );
	GetTurret()->Unlock( this );
	ToRestState();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const WORD CTurretGun::GetGlobalDir() const
{
	return pOwner->GetDir() + GetTurret()->GetHorCurAngle() + GetGun().wDirection;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTurretGun::TurnToRelativeDir( const WORD wAngle )
{
	GetTurret()->TurnHor( wAngle );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CTurretGun::GetRotateSpeed() const
{
	return GetTurret()->GetHorRotateSpeed();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CTurretGun::GetHorTurnConstraint() const
{
	if ( !bCircularAttack )
		return GetTurret()->GetHorTurnConstraint();
	else
		return 32768;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CTurretGun::GetVerTurnConstraint() const
{
	return GetTurret()->GetVerTurnConstraint();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTurretGun::SetCircularAttack( const bool bCanAttack )
{
	bCircularAttack = bCanAttack;	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTurretGun::StartPointBurst( const CVec3 &target, bool bReAim )
{
	bTurnByBestWay = false;
	CGun::StartPointBurst( target, bReAim );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTurretGun::StartPointBurst( const CVec2 &target, bool bReAim )
{
	bTurnByBestWay = false;
	CGun::StartPointBurst( target, bReAim );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTurretGun::StartEnemyBurst( class CAIUnit *pEnemy, bool bReAim )
{
	bTurnByBestWay = false;
	CGun::StartEnemyBurst( pEnemy, bReAim );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CBaseGun																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBaseGun::TurnGunToEnemy( const CVec2 &vEnemyCenter, const float zDiff )
{
	return pOwner->TurnToDir( GetDirectionByVector( vEnemyCenter - pOwner->GetCenter() ) - GetGun().wDirection, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBaseGun::IsGoodAngle( const CVec2 &point, const WORD addAngle, const float z, const BYTE cDeltaAngle  ) const
{
	const SUnitBaseRPGStats *pStats = pOwner->GetStats();
	
	if ( pStats->IsInfantry() && !AnalyzeLimitedAngle( pOwner, point ) )
		return false;

	const CVec2 vOwnerCenter( pOwner->GetCenter() );
	const int wWeaponDeltaAngle = (int)GetWeapon()->wDeltaAngle;

	if ( pStats->IsAviation() )
	{
		const CAviation * pPlane = static_cast<const CAviation*>( pOwner );
		// check vertical angle
		const WORD wDesiredVAngle = GetDirectionByVector( fabs(point - vOwnerCenter), -pOwner->GetZ() );
		const WORD wCurrentVAngle = GetDirectionByVector ( pPlane->GetSpeedHorVer() );
		if ( DirsDifference( wCurrentVAngle, wDesiredVAngle ) > 2 * ( wWeaponDeltaAngle + (int)addAngle ) * cDeltaAngle )
			return false;
	}

	const WORD wDesirableAngle = GetDirectionByVector( point - vOwnerCenter ) - GetGun().wDirection;
	return DirsDifference( wDesirableAngle, pOwner->GetFrontDir() ) <= 
				( wWeaponDeltaAngle + (int)addAngle ) * cDeltaAngle;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBaseGun::AnalyzeTurning()
{
	if ( !CanShootToTargetWOMove() )
		StopFire();

	if ( CanShootWOGunTurn( !bAim, z ) )
		return true;
	else if ( pOwner->IsLocked( this ) || pOwner->GetStats()->IsAviation() || !pOwner->CanRotate() )
		StopFire();
	else
		Turning();

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseGun::StopFire()
{
	pOwner->Unlock( this );
	ToRestState();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const WORD CBaseGun::GetGlobalDir() const
{
	return pOwner->GetDir() + GetGun().wDirection;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CBaseGun::GetRotateSpeed() const
{
	return pOwner->GetRotateSpeed();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float GetFireRangeMax( const SWeaponRPGStats *pStats, CAIUnit *pOwner )
{
	const float fExpCoeff = pOwner ? pOwner->GetExpLevel().fBonusFireRange : 1.0f;
	return pStats->fRangeMax * ( 1 + (SConsts::BAD_WEATHER_FIRE_RANGE_COEFFICIENT - 1) * (int)theWeather.IsActive() ) * fExpCoeff;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
