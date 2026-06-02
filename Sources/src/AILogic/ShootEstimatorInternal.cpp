#include "stdafx.h"

#include "ShootEstimatorInternal.h"
#include "AIUnit.h"
#include "Turret.h"
#include "Obstacle.h"
#include "Diplomacy.h"
#include "Units.h"
#include "ScanLimiter.h"
#include "Soldier.h"
#include "Building.h"
#include "GunsInternal.h"
#include "Artillery.h"
#include "MPLog.h"
#include "General.h"

#include "TimeCounter.h"
extern CDiplomacy theDipl;
extern CUnits units;
extern CScanLimiter theScanLimiter;

extern CTimeCounter timeCounter;
extern CSupremeBeing theSupremeBeing;
BASIC_REGISTER_CLASS( CTankShootEstimator );
BASIC_REGISTER_CLASS( IShootEstimator );
BASIC_REGISTER_CLASS( CPlaneDeffensiveFireShootEstimator );
BASIC_REGISTER_CLASS( CPlaneShturmovikShootEstimator );
const float F( const float fHPPercent, const float fT0, const float fT1, const float fT2, const float fPrice )
{
	static const float F_LIMIT_TIME = 1000.0f;	

	static const float fAlphaAttack1 = 1.0f;
	static const float fAlphaAttack2 = 0.3f;
	static const float fAlphaGo = 0.005f;
	static const float fAlphaKill= 1.0f;
	static const float fAlphaPrice = 1.0f;

	return
		( 0.8f + fHPPercent * 0.2f ) * fAlphaAttack1 * Min( 0.0f, fT2 - F_LIMIT_TIME ) -
		( 0.8f + fHPPercent * 0.2f ) * fAlphaAttack2 * Max( 0.0f, fT2 - F_LIMIT_TIME ) -
		fAlphaGo * fT0 - 
		fAlphaKill * fT1 +
		fPrice * fAlphaPrice;
}
const float FGunplane( const float fHPPercent, const float fT0, const float fT1, const float fT2, const float fPrice )
{
	static const float F_LIMIT_TIME = SConsts::TR_GUNPLANE_LIMIT_TIME;	

	static const float fAlphaAttack1 =	SConsts::TR_GUNPLANE_ALPHA_ATTACK_1;//1.0f;
	static const float fAlphaAttack2 =	SConsts::TR_GUNPLANE_ALPHA_ATTACK_2;//0.3f;
	static const float fAlphaGo =				SConsts::TR_GUNPLANE_ALPHA_GO;//0.005f;
	static const float fAlphaKill=			SConsts::TR_GUNPLANE_ALPHA_KILL;//1.0f;
	static const float fAlphaPrice =		SConsts::TR_GUNPLANE_ALPHA_PRICE;//1.0f;

	return
		( 0.8f + fHPPercent * 0.2f ) * fAlphaAttack1 * Min( 0.0f, fT2 - F_LIMIT_TIME ) -
		( 0.8f + fHPPercent * 0.2f ) * fAlphaAttack2 * Max( 0.0f, fT2 - F_LIMIT_TIME ) -
		fAlphaGo * fT0 - 
		fAlphaKill * fT1 +
		fPrice * fAlphaPrice;
}
CTankShootEstimator::CTankShootEstimator( CAIUnit *_pOwner ) 
: pOwner( _pOwner )
{
	dwForbidden = 0;
	dwDefaultForbidden = 0;
	for ( int i = 1; i < pOwner->GetNGuns(); ++i )
	{
		if ( pOwner->GetGun( i )->GetGun().nPriority == 0 )
		{
			int j = 0;
			while ( j < i && 
							( pOwner->GetGun( j )->GetGun().nPriority != 0 || 
							  pOwner->GetGun( j )->GetCommonGunNumber() == pOwner->GetGun( i )->GetCommonGunNumber() ) )
				++j;

			if ( j < i )
				dwDefaultForbidden |= ( 1UL << i );
		}
	}
	
	pMosinStats = NGDB::GetRPGStats<SUnitBaseRPGStats>( "USSR_Mosin" );
}
const float FindTimeToTurnToPoint( const CVec2 &vPoint, class CCommonUnit *pOwner, CBasicGun *pGun )
{
	const WORD wUnitDir = pOwner->GetDir();		
	const WORD wDirToEnemy = GetDirectionByVector( vPoint - pOwner->GetCenter() );
	const bool bMovingOwner = pOwner->CanRotate() && !pOwner->NeedDeinstall();
	const WORD wDeltaAngle = pGun->GetWeapon()->wDeltaAngle;
	CTurret *pTurret = pGun->GetTurret();

	if ( pTurret != 0 )
	{
		const WORD wGunDir = pGun->GetTurret()->GetHorCurAngle() + pGun->GetGun().wDirection + wUnitDir;
		const WORD wTurnAngle = DirsDifference( wGunDir, wDirToEnemy );

		if ( wTurnAngle < wDeltaAngle )
			return 0.0f;
		else
		{
			float fTurnSpeed = pTurret->GetHorRotationSpeed();
			if ( bMovingOwner )
				fTurnSpeed = Min( fTurnSpeed, pOwner->GetRotateSpeed() );

			return float( wTurnAngle ) / fTurnSpeed;
		}
	}
	else
	{
		if ( !bMovingOwner )
			return 0.0f;
		else
		{
			const WORD wGunDir = pGun->GetGun().wDirection + wUnitDir;
			const WORD wTurnAngle = DirsDifference( wGunDir, wDirToEnemy );
			if ( wTurnAngle < wDeltaAngle )
				return 0.0f;
			else
				return float( wTurnAngle ) / pOwner->GetRotateSpeed();
		}
	}
}

void CTankShootEstimator::Reset( CAIUnit *pCurEnemy, const bool bDamageUpdated, const DWORD _dwForbidden )
{
	pCurTarget = pCurEnemy;
	bDamageToCurTargetUpdated = bDamageUpdated;
	pBestUnit = 0;
	pBestGun = 0;
	nBestGun = 0;
	dwForbidden = _dwForbidden;

	if ( IsValidObj( pCurTarget ) )
		AddUnit( pCurTarget );
}
const float CTankShootEstimator::GetRating( const SUnitBaseRPGStats *pStats, const CVec2 &vCenter, CBasicGun *pGun ) const
{
	float fTimeToGo = 0;

	if ( pOwner->CanMove() && !pOwner->NeedDeinstall() )
		fTimeToGo = fabs( pOwner->GetCenter() - vCenter ) / pOwner->GetStats()->fSpeed;
	fTimeToGo += FindTimeToTurnToPoint( vCenter, pOwner, pGun );

	const float fEnemyKillUsSpeed = 0.0f;
	const float fEnemyKillUsTime = 0.0f;

	const float fKillEnemySpeed = pOwner->GetKillSpeed( pStats, vCenter, pGun );
	const float fKillEnemyTime = pStats->fMaxHP / fKillEnemySpeed;
	const float fEnemyHPPercent = 1.0f;

	return
		F( fEnemyHPPercent, fTimeToGo, fEnemyKillUsTime, fKillEnemyTime, pStats->fPrice );
}
const float CTankShootEstimator::GetRating( CAIUnit *pEnemy, CBasicGun *pGun ) const
{
	float fTimeToGo = 0;

	if ( pOwner->CanMove() && !pOwner->NeedDeinstall() )
		fTimeToGo = fabs( pOwner->GetCenter() - pEnemy->GetCenter() ) / pOwner->GetStats()->fSpeed;
	fTimeToGo += FindTimeToTurnToPoint( pEnemy->GetCenter(), pOwner, pGun );

	const float fEnemyKillUsSpeed = pEnemy->GetKillSpeed( pOwner );
	float fEnemyKillUsTime;
	if ( fEnemyKillUsSpeed == 0.0f )
		fEnemyKillUsTime = 0.0f;
	else
		fEnemyKillUsTime = pOwner->GetStats()->fMaxHP / fEnemyKillUsSpeed;

	float fKillEnemySpeed;
	if ( !bDamageToCurTargetUpdated )
		fKillEnemySpeed = pEnemy->GetTakenDamagePower() + pOwner->GetKillSpeed( pEnemy, pGun );
	else
	{
		if ( pEnemy != pCurTarget )
			fKillEnemySpeed = pEnemy->GetTakenDamagePower() + pOwner->GetKillSpeed( pEnemy, pGun );
		else
			fKillEnemySpeed = pEnemy->GetTakenDamagePower();
	}

	const float fKillEnemyTime = pEnemy->GetStats()->fMaxHP / fKillEnemySpeed;
	const float fEnemyHPPercent = pEnemy->GetHitPoints() / pEnemy->GetStats()->fMaxHP;

	return
		F( fEnemyHPPercent, fTimeToGo, fEnemyKillUsTime, fKillEnemyTime, pEnemy->GetStats()->fPrice );
}
void CTankShootEstimator::ChooseGun( CBasicGun **pBestGun, int *nBestGun, CAIUnit *pEnemy )
{
	float fBestTime = 0;
	*pBestGun = 0;

	const float fDist = fabs( pOwner->GetCenter() - pEnemy->GetCenter() );
	const bool bOverHorizont = fDist > pOwner->GetSightRadius();
	const bool bMovingOwner = pOwner->CanRotate() && pOwner->CanMove() && !pOwner->NeedDeinstall();

	const bool bArtilleryMech = pOwner->GetFirstArtilleryGun() != 0;

	const SRect enemyRect = pEnemy->GetUnitRect();
	const float fSEnemyRect = 
		2 * enemyRect.width * ( enemyRect.lengthAhead + enemyRect.lengthBack ) / sqr( pEnemy->GetRemissiveCoeff() );

	dwForbidden |= dwDefaultForbidden;
	for ( int i = 0; i < pOwner->GetNGuns(); ++i )
	{
		CBasicGun *pGun = pOwner->GetGun( i );
		if ( ( dwForbidden & ( 1UL << i ) ) == 0 && pGun->GetNAmmo() > 0 && pGun->GetShell().eDamageType == SWeaponRPGStats::SShell::DAMAGE_HEALTH )
		{
/*
			if ( bOverHorizont ||
					 pOwner->GetStats()->type == RPG_TYPE_ART_MORTAR ||
 					 !bOverHorizont && pGun->GetShell().trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
*/
			{
				if ( !pGun->TooCloseToFire( pEnemy ) &&
						 (
							!bArtilleryMech && pGun->CanShootToUnit( pEnemy ) ||
							bArtilleryMech && ( !pGun->IsBallisticTrajectory() && pGun->CanShootToUnit( pEnemy ) && !bOverHorizont || pGun->CanShootToUnitWOMove( pEnemy ) ) 
						  )
						)
				{
					if ( bArtilleryMech && pGun->IsBallisticTrajectory() && bOverHorizont )
					{
						*pBestGun = pGun;
						*nBestGun = i;

						return;
					}
					
					if ( theDipl.IsAIPlayer( pOwner->GetPlayer() ) && pGun->IsBallisticTrajectory() && 
							 theSupremeBeing.IsInResistanceCircle( pEnemy->GetCenter(), pOwner->GetParty() ) )
						continue;
					
					const float fFireRange = pGun->GetFireRange( pEnemy->GetZ() );
					const float fDistToGo = Max( 0.0f, fDist - fFireRange );
					CVec2 vDirToEnemy = pEnemy->GetCenter() - pOwner->GetCenter();
					Normalize( &vDirToEnemy );
					const CVec2 vPoint = pOwner->GetCenter() + vDirToEnemy * fDistToGo;
					if ( !bMovingOwner || fDistToGo == 0 || pOwner->CanGoToPoint( vPoint ) )
					{
						float fTime = 0;
						if ( bMovingOwner )
							fTime = fDistToGo / pOwner->GetStats()->fSpeed;

						fTime += FindTimeToTurnToPoint( pEnemy->GetCenter(), pOwner, pGun );

						if ( pGun->GetMaxPossiblePiercing() < pEnemy->GetMaxArmor() )
							fTime += 5461.0f * pOwner->GetRotateSpeed();

						const float fDispRadius = GetDispByRadius( pGun, pOwner->GetCenter(), pEnemy->GetCenter() );
						float fR;
						if ( pEnemy->GetMaxArmor() == 0 )
						{
							if ( !pEnemy->GetStats()->IsInfantry() )
								fR = 0.56 * fDispRadius - pGun->GetShell().fArea;
							else if ( pEnemy->IsFree() )
								fR = 0.56 * fDispRadius - pGun->GetShell().fArea2;
							else
								fR = 0.56 * fDispRadius - pGun->GetShell().fArea;
						}
						else
							fR = 0.56 * fDispRadius;

						float fProbToHit;
						if ( fR <= 0 )
							fProbToHit = 1;
						else
							fProbToHit = Min( 1.0f, fSEnemyRect / ( FP_PI * sqr( fR ) ) );

						const float fShotsToKill = 0.8 * pEnemy->GetHitPoints() / pGun->GetDamage();
						const float fProbShotsToKill = fShotsToKill / fProbToHit;

						const int nAmmoPerBurst = pGun->GetWeapon()->nAmmoPerBurst;
						const int nBursts = ceil( fProbShotsToKill / nAmmoPerBurst );
						
						fTime += pGun->GetAimTime( false ) +
										 ( nBursts - 1 ) * pGun->GetRelaxTime( false ) +
										 nBursts * ( ( nAmmoPerBurst - 1 ) * pGun->GetFireRate() );
						
						if ( *pBestGun == 0 || fTime < fBestTime )
						{
							*pBestGun = pGun;
							*nBestGun = i;
							fBestTime = fTime;
						}
					}
				}
			}
		}
	}
}
void CTankShootEstimator::AddUnit( CAIUnit *pEnemy )
{
	theScanLimiter.TargetScanning( pOwner->GetStats()->type );
	
	CBasicGun *pBestLocalGun = 0;
	int nBestLocalGun = 0;
	const CVec2 vEnemyCenter = pEnemy->GetCenter();

	bool bChoose = true;
	if ( pOwner->IsFreeEnemySearch() && pOwner->GetFirstArtilleryGun() != 0 )
	{
		const float fDistToEnemy2 = fabs2( pOwner->GetCenter() - vEnemyCenter );
		if ( fDistToEnemy2 >= sqr( pOwner->GetSightRadius() ) )
		{
			const float fDispersion = pOwner->GetFirstArtilleryGun()->GetDispersion();

			if ( pEnemy->GetStats()->IsInfantry() && 
					 units.GetNSoldiers( vEnemyCenter, fDispersion * 1.2f, 1 - pOwner->GetParty() ) < 10 )
				bChoose = false;

			if ( !pEnemy->GetStats()->IsInfantry() &&
					 units.GetNUnits( vEnemyCenter, fDispersion, 1 - pOwner->GetParty() ) < 4 )
				bChoose = false;
		}
	}

	if ( bChoose )
		ChooseGun( &pBestLocalGun, &nBestLocalGun, pEnemy );

	if ( pBestLocalGun != 0 )
	{
		const float fSoldierRating = GetRating( pMosinStats, vEnemyCenter, pBestLocalGun );
		const int nSoldiers = units.GetNSoldiers( vEnemyCenter, pBestLocalGun->GetDispersion(), 1 - pOwner->GetParty() );
		const float fRating = GetRating( pEnemy, pBestLocalGun ) + nSoldiers * fSoldierRating;

		if ( pBestUnit == 0 )
		{
			pBestUnit = pEnemy;
			pBestGun = pBestLocalGun;
			nBestGun = nBestLocalGun;
			fBestRating = fRating;
		}
		else
		{
			const float fNewEnemyKillUsTime = pEnemy->GetKillSpeed( pOwner );
			const float fEnemyKillUsTime = pBestUnit->GetKillSpeed( pOwner );

			if ( fEnemyKillUsTime == 0.0f && fNewEnemyKillUsTime != 0.0f ||
					 fRating > fBestRating && 
					 (	
							fEnemyKillUsTime != 0.0f && fNewEnemyKillUsTime != 0.0f ||
							fEnemyKillUsTime == 0.0f && fNewEnemyKillUsTime == 0.0f 
					 )
				 )
			{
				pBestUnit = pEnemy;
				pBestGun = pBestLocalGun;
				nBestGun = nBestLocalGun;
				fBestRating = fRating;
			}
		}
	}
}
CAIUnit* CTankShootEstimator::GetBestUnit() const
{
	return pBestUnit;
}
CBasicGun* CTankShootEstimator::GetBestGun() const
{
	return pBestGun;
}
const int CTankShootEstimator::GetNumberOfBestGun() const
{
	return nBestGun;
}
const int CSoldierShootEstimator::N_GOOD_NUMBER_ATTACKING_GRENADES = 6;

CSoldierShootEstimator::CSoldierShootEstimator( CAIUnit *_pOwner ) 
: pOwner( _pOwner ), bHasGrenades ( pOwner->GetNGuns() >= 2 )
{ 
	pMosinStats = NGDB::GetRPGStats<SUnitBaseRPGStats>( "USSR_Mosin" );
}
void CSoldierShootEstimator::Reset( CAIUnit *pCurEnemy, const bool bDamageUpdated, const DWORD _dwForbidden )
{
	pCurTarget = pCurEnemy;
	bDamageToCurTargetUpdated = bDamageUpdated;
	bThrowGrenade = false;
	pBestUnit = 0;
	pBestGun = 0;
	nBestGun = 0;

	dwForbidden = _dwForbidden;

	if ( IsValidObj( pCurTarget ) )
		AddUnit( pCurTarget );
}
const float CSoldierShootEstimator::GetRating( CAIUnit *pEnemy, CBasicGun *pGun ) const
{
	float fTimeToGo = fabs( pOwner->GetCenter() - pEnemy->GetCenter() ) / pOwner->GetStats()->fSpeed;

	const float fEnemyKillUsSpeed = pEnemy->GetKillSpeed( pOwner );
	float fEnemyKillUsTime;
	if ( fEnemyKillUsSpeed == 0.0f )
		fEnemyKillUsTime = 0.0f;
	else
		fEnemyKillUsTime = pOwner->GetStats()->fMaxHP / fEnemyKillUsSpeed;

	float fKillEnemySpeed;
	if ( !bDamageToCurTargetUpdated )
		fKillEnemySpeed = pEnemy->GetTakenDamagePower() + pOwner->GetKillSpeed( pEnemy, pGun );
	else
	{
		if ( pEnemy != pCurTarget )
			fKillEnemySpeed = pEnemy->GetTakenDamagePower() + pOwner->GetKillSpeed( pEnemy, pGun );
		else
			fKillEnemySpeed = pEnemy->GetTakenDamagePower();
	}

	const float fKillEnemyTime = pEnemy->GetStats()->fMaxHP / fKillEnemySpeed;
	const float fEnemyHPPercent = pEnemy->GetHitPoints() / pEnemy->GetStats()->fMaxHP;

	return
		F( fEnemyHPPercent, fTimeToGo, fEnemyKillUsTime, fKillEnemyTime, pEnemy->GetStats()->fPrice );
}
const float CSoldierShootEstimator::GetRating( const SUnitBaseRPGStats *pStats, const CVec2 &vCenter, CBasicGun *pGun ) const
{
	float fTimeToGo = 0;

	if ( pOwner->CanMove() && !pOwner->NeedDeinstall() )
		fTimeToGo = fabs( pOwner->GetCenter() - vCenter ) / pOwner->GetStats()->fSpeed;
	fTimeToGo += FindTimeToTurnToPoint( vCenter, pOwner, pGun );

	const float fEnemyKillUsSpeed = 0.0f;
	const float fEnemyKillUsTime = 0.0f;

	const float fKillEnemySpeed = pOwner->GetKillSpeed( pStats, vCenter, pGun );
	const float fKillEnemyTime = pStats->fMaxHP / fKillEnemySpeed;
	const float fEnemyHPPercent = 1.0f;

	return
		F( fEnemyHPPercent, fTimeToGo, fEnemyKillUsTime, fKillEnemyTime, pStats->fPrice );
}
void CSoldierShootEstimator::ChooseGun( CBasicGun **pBestGun, int *nBestGun, CAIUnit *pEnemy )
{
	float fBestTime = 0;
	*pBestGun = 0;

	const float fDist = fabs( pOwner->GetCenter() - pEnemy->GetCenter() );

	if ( pOwner->IsFree() && 
			( dwForbidden & 1 ) == 0 &&
			( pOwner->GetNGuns() == 1 || pEnemy->GetStats()->IsInfantry() ) )
	{
		CBasicGun *pGun = pOwner->GetGun( 0 );
		if ( pGun->GetNAmmo() > 0 && pGun->GetShell().eDamageType == SWeaponRPGStats::SShell::DAMAGE_HEALTH && pGun->CanShootToUnit( pEnemy ) )
		{
			*pBestGun = pGun;
			*nBestGun = 0;
		}
	}
	else
	{
		const SRect enemyRect = pEnemy->GetUnitRect();
		const float fSEnemyRect = 
			2 * enemyRect.width * ( enemyRect.lengthAhead + enemyRect.lengthBack ) / sqr( pEnemy->GetRemissiveCoeff() );

		for ( int i = 0; i < pOwner->GetNGuns(); ++i )
		{
			CBasicGun *pGun = pOwner->GetGun( i );
			const bool bTooFar = 
				( i == 1 ) && sqr(fDist) > sqr( SConsts::MAX_DISTANCE_TO_THROW_GRENADE ) &&
				pEnemy->GetStats()->IsArtillery() &&
				checked_cast<CArtillery*>(pEnemy)->GetCrew() != 0;
			
			if ( !bTooFar && ( dwForbidden & ( 1 << i ) ) == 0 && pGun->GetShell().eDamageType == SWeaponRPGStats::SShell::DAMAGE_HEALTH &&
					 pGun->GetNAmmo() > 0 && pGun->CanShootToUnit( pEnemy ) )
			{
				const float fDistToGo = Max( 0.0f, fDist - pGun->GetFireRange( pEnemy->GetZ() ) );

				CVec2 vDirToEnemy = pEnemy->GetCenter() - pOwner->GetCenter();
				Normalize( &vDirToEnemy );
				const CVec2 vPoint = pOwner->GetCenter() + vDirToEnemy * fDistToGo;

				if ( pOwner->CanGoToPoint( vPoint ) || fDistToGo == 0 )
				{
					float fTime = fDistToGo / pOwner->GetStats()->fSpeed;

					const float fDispRadius = GetDispByRadius( pGun, pOwner->GetCenter(), pEnemy->GetCenter() );
					float fR;
					if ( pEnemy->GetMaxArmor() == 0 )
					{
						if ( !pEnemy->GetStats()->IsInfantry() )
							fR = 0.56 * fDispRadius - pGun->GetShell().fArea;
						else if ( pEnemy->IsFree() )
							fR = 0.56 * fDispRadius - pGun->GetShell().fArea2;
						else
							fR = 0.56 * fDispRadius - pGun->GetShell().fArea;
					}
					else
						fR = 0.56 * fDispRadius;

					float fProbToHit;
					if ( fR <= 0 )
						fProbToHit = 1;
					else
						fProbToHit = Min( 1.0f, fSEnemyRect / ( FP_PI * sqr( fR ) ) );

					const float fShotsToKill = 0.8 * pEnemy->GetHitPoints() / pGun->GetDamage();
					const float fProbShotsToKill = fShotsToKill / fProbToHit;

					const int nAmmoPerBurst = pGun->GetWeapon()->nAmmoPerBurst;
					const int nBursts = ceil( fProbShotsToKill / nAmmoPerBurst );

					fTime += pGun->GetAimTime( false ) +
									 ( nBursts - 1 ) * pGun->GetRelaxTime( false ) +
									 nBursts * ( ( nAmmoPerBurst - 1 ) * pGun->GetFireRate() );

					if ( *pBestGun == 0 || fTime < fBestTime )
					{
						*pBestGun = pGun;
						*nBestGun = i;
						fBestTime = fTime;
					}
				}
			}
		}
	}
}
void CSoldierShootEstimator::AddUnit( CAIUnit *pEnemy )
{
	theScanLimiter.TargetScanning( pOwner->GetStats()->type );	
	
	CBasicGun *pBestLocalGun = 0;
	if ( bHasGrenades )
	{
		if ( !pEnemy->GetStats()->IsInfantry() && pEnemy->CanMove() && !pEnemy->NeedDeinstall() )
		{
			int nAttackingGrenades = pEnemy->GetNAttackingGrenages();
			if ( bDamageToCurTargetUpdated && pEnemy == pCurTarget )
				--nAttackingGrenades;

			if ( nAttackingGrenades < N_GOOD_NUMBER_ATTACKING_GRENADES && pOwner->GetGun( 1 )->CanShootToUnit( pEnemy ) )
			{
				const float fDistToEnemy2 = fabs2( pOwner->GetCenter() - pEnemy->GetCenter() );

				if ( fDistToEnemy2 <= SConsts::MAX_DISTANCE_TO_THROW_GRENADE )
				{
					if ( bThrowGrenade )
					{
						if ( pBestUnit == 0 || fDistToEnemy2 < fabs2( pBestUnit->GetCenter() - pOwner->GetCenter() ) )
						{
							pBestUnit = pEnemy;
							pBestGun = pOwner->GetGun( 1 );
							nBestGun = 1;
						}
					}
					else
					{
						bThrowGrenade = true;
						pBestUnit = pEnemy;
						pBestGun = pOwner->GetGun( 1 );
						nBestGun = 1;
					}
				}
			}
		}
	}
	
	if ( !bThrowGrenade )
	{
		int nBestLocalGun = 0;
		ChooseGun( &pBestLocalGun, &nBestLocalGun, pEnemy );

		if ( pBestLocalGun != 0 )
		{
			const float fSoldierRating = GetRating( pMosinStats, pEnemy->GetCenter(), pBestLocalGun );
			const int nSoldiers = units.GetNSoldiers( pEnemy->GetCenter(), pBestLocalGun->GetDispersion(), 1 - pOwner->GetParty() );
			const float fRating = GetRating( pEnemy, pBestLocalGun ) + nSoldiers * fSoldierRating;

			if ( pBestUnit == 0 )
			{
				pBestUnit = pEnemy;
				pBestGun = pBestLocalGun;
				nBestGun = nBestLocalGun;
				fBestRating = fRating;
			}
			else
			{
				const float fNewEnemyKillUsTime = pEnemy->GetKillSpeed( pOwner );
				const float fEnemyKillUsTime = pBestUnit->GetKillSpeed( pOwner );

				if ( fEnemyKillUsTime == 0.0f && fNewEnemyKillUsTime != 0.0f ||
						 fRating > fBestRating && 
						 (	
								fEnemyKillUsTime != 0.0f && fNewEnemyKillUsTime != 0.0f ||
								fEnemyKillUsTime == 0.0f && fNewEnemyKillUsTime == 0.0f 
						 )
					 )
				{
					pBestUnit = pEnemy;
					pBestGun = pBestLocalGun;
					nBestGun = nBestLocalGun;
					fBestRating = fRating;
				}
			}
		}
	}
}
CAIUnit* CSoldierShootEstimator::GetBestUnit() const
{
	return pBestUnit;
}
CBasicGun* CSoldierShootEstimator::GetBestGun() const
{
	return pBestGun;
}
const int CSoldierShootEstimator::GetNumberOfBestGun() const
{
	return nBestGun;
}
const float CPlaneDeffensiveFireShootEstimator::CalcTimeToOpenFire( class CAIUnit *pEnemy, CBasicGun *pGun ) const
{
	if ( pGun->IsInShootCone( pEnemy->GetCenter() ) )
	{
		return ( pGun->InFireRange( pEnemy ) ? 0 : 10000 );
	}
	else
	{
		return 100000;
	}
}
const float CPlaneDeffensiveFireShootEstimator::CalcRating( CAIUnit *pEnemy, CBasicGun *pGun ) const
{
	const float fTimeToGo = CalcTimeToOpenFire( pEnemy, pGun );
	const float fEnemyKillUsSpeed = pEnemy->GetKillSpeed( pOwner );
	float fEnemyKillUsTime;
	if ( fEnemyKillUsSpeed == 0.0f )
		fEnemyKillUsTime = 0.0f;
	else
		fEnemyKillUsTime = pOwner->GetStats()->fMaxHP / fEnemyKillUsSpeed;

	float fKillEnemySpeed;
	if ( !bDamageToCurTargetUpdated )
		fKillEnemySpeed = pEnemy->GetTakenDamagePower() + pOwner->GetKillSpeed( pEnemy, pGun );
	else
	{
		if ( pEnemy != pCurTarget )
			fKillEnemySpeed = pEnemy->GetTakenDamagePower() + pOwner->GetKillSpeed( pEnemy, pGun );
		else
			fKillEnemySpeed = pEnemy->GetTakenDamagePower();
	}

	const float fKillEnemyTime = pEnemy->GetStats()->fMaxHP / fKillEnemySpeed;
	const float fEnemyHPPercent = pEnemy->GetHitPoints() / pEnemy->GetStats()->fMaxHP;

	return
		F( fEnemyHPPercent, fTimeToGo, fEnemyKillUsTime, fKillEnemyTime, pEnemy->GetStats()->fPrice );
}
void CPlaneDeffensiveFireShootEstimator::SetGun( CBasicGun *_pGun )
{
	pGun = _pGun;
}
CPlaneDeffensiveFireShootEstimator::CPlaneDeffensiveFireShootEstimator( class CAIUnit *pOwner )
: pOwner( pOwner )
{
	
}
void CPlaneDeffensiveFireShootEstimator::Reset( class CAIUnit *pCurEnemy, const bool bDamageUpdated, const DWORD dwForbidden )
{
	pCurTarget = pCurEnemy;
	bDamageToCurTargetUpdated = bDamageUpdated;
	pBestUnit = 0;

	if ( IsValidObj( pCurTarget ) )
		AddUnit( pCurTarget );
}
void CPlaneDeffensiveFireShootEstimator::AddUnit( class CAIUnit *pEnemy )
{

	const int nGuns = pOwner->GetNGuns();
	bool bCanShootByHeight = false;

	for ( int i = 0; i < nGuns; ++i )
	{
		if ( pOwner->GetGun( i )->CanShootByHeight( pEnemy ) )
		{
			bCanShootByHeight = true;
			break;
		}
	}
	
	if ( !bCanShootByHeight )
		return;
		
	const float fTempRating = CalcRating( pEnemy, pGun );

	if ( !pCurTarget || fTempRating > fBestRating )
	{
		pCurTarget = pEnemy;	
		fBestRating = fTempRating;
	}
}
class CAIUnit* CPlaneDeffensiveFireShootEstimator::GetBestUnit() const
{
	return pCurTarget;
};
CBasicGun* CPlaneDeffensiveFireShootEstimator::GetBestGun() const
{
	NI_ASSERT_T( false, "wrong call");
	return 0;
}
const int CPlaneDeffensiveFireShootEstimator::GetNumberOfBestGun() const
{
	NI_ASSERT_T( false, "wrong call");
	return 0;
}
CPlaneShturmovikShootEstimator::CPlaneShturmovikShootEstimator( class CAIUnit *pOwner )
: pOwner( pOwner )
{
}
void CPlaneShturmovikShootEstimator::Reset( class CAIUnit *_pCurEnemy, const bool bDamageUpdated, const DWORD dwForbidden )
{
	bestAviation.Reset();
	bestForBombs.Reset();
	bestForGuns.Reset();
	if ( _pCurEnemy && _pCurEnemy->IsValid() && _pCurEnemy->IsAlive() )
	{
		pCurEnemy = _pCurEnemy;
		AddUnit( pCurEnemy );
	}
	buildings.clear();
}
void CPlaneShturmovikShootEstimator::AddUnit( CAIUnit *pTry )
{
	if ( !pTry || !pTry->IsValid() || !pTry->IsAlive() ) return;

	if ( !pTry->IsFree() && pTry->GetStats()->IsInfantry() )
	{
		NI_ASSERT_T( dynamic_cast<CSoldier*>( pTry ) != 0, "not soldier, but isn't free" );
		CSoldier * pSoldier = static_cast<CSoldier*>( pTry );
		if ( pSoldier->IsInBuilding() )
		{
			CBuilding * pBuilding = pSoldier->GetBuilding();
			if ( pBuilding && pBuilding->GetStats() )
			{
				const SBuildingRPGStats * pStats = static_cast<const SBuildingRPGStats*>( pBuilding->GetStats() );
				if ( pStats->eType != SBuildingRPGStats::TYPE_MAIN_RU_STORAGE )
					buildings.insert( pBuilding->GetUniqueId() );
			}
		}
		return;
	}
	
	bool bCanBreak = false;
	bool bOnlyBombs = true;
	int nGuns = pOwner->GetNGuns();
	DWORD dwPossibleGuns = 0;
	for ( int i = 0; i < nGuns; ++i )
	{
		CBasicGun *pGun = pOwner->GetGun( i );
		if ( 0 != pGun->GetNAmmo() && pGun->CanBreakArmor( pTry ) )
		{
			if ( SWeaponRPGStats::SShell::TRAJECTORY_BOMB != pGun->GetShell().trajectory )
			{
				if( DirsDifference( pGun->GetGlobalDir(), pOwner->GetDir() ) < 500 )
				{
					dwPossibleGuns |= 1<<i;
					bCanBreak = true;
					bOnlyBombs = false;
				}
			}
			else
			{
				bCanBreak = true;
				dwPossibleGuns |= 1<<i;
			}
		}
	}

	if ( !bCanBreak )
		return;

	if ( pTry->GetStats()->IsAviation() )
	{
		if ( !bOnlyBombs )
			CollectTarget( &bestAviation, pTry, dwPossibleGuns );	
	}
	else if ( bOnlyBombs )
		CollectTarget( &bestForBombs, pTry, dwPossibleGuns );		
	else
		CollectTarget( &bestForGuns, pTry, dwPossibleGuns );
}
class CAIUnit* CPlaneShturmovikShootEstimator::GetBestUnit() const
{
	if ( bestForGuns.pTarget )
		return bestForGuns.pTarget ;
	else if ( bestForBombs.pTarget )
		return bestForBombs.pTarget;
	else
		return bestAviation.pTarget;
};
void CPlaneShturmovikShootEstimator::CalcBestBuilding()
{
	float fRating = 0;

	for ( CBuildings::iterator it = buildings.begin(); it != buildings.end(); ++it )
	{
		CBuilding *pBuilding = static_cast<CBuilding*>( CLinkObject::GetObjectByUniqueIdSafe( *it ) );
		const SBuildingRPGStats *pBuildingStats = static_cast<const SBuildingRPGStats*>( pBuilding->GetStats() );

		int nGuns = pOwner->GetNGuns();
		DWORD dwPossibleGuns = 0;
		bool bCanBreak = false;
		for ( int i = 0; i < nGuns; ++i )
		{
			CGun *pGun = static_cast<CGun*>( pOwner->GetGun( i ) );
			if ( 0 != pGun->GetNAmmo() && pGun->CanBreach( pBuildingStats, RPG_TOP ) )
			{
				if ( SWeaponRPGStats::SShell::TRAJECTORY_BOMB != pGun->GetShell().trajectory ||
							DirsDifference( pGun->GetGlobalDir(), pOwner->GetDir() ) < 500 )
				{
					dwPossibleGuns |= 1<<i;
					bCanBreak = true;
				}
			}
		}

		float fCurrentRating = 0.0f;
		if ( bCanBreak )
		{
			const float fKillBuildingSpeed = pOwner->GetKillSpeed( pBuildingStats, pBuilding->GetAttackCenter( pOwner->GetCenter() ), dwPossibleGuns );
			const float fKillBuildingTime = fKillBuildingSpeed == 0.0f ? 0.0f : 1.0f / fKillBuildingSpeed;
			
			float fTotalPrice = 0;
			const int nDefenders = pBuilding->GetNDefenders();
			for ( int i = 0; i < nDefenders; ++i )
				fTotalPrice += pBuilding->GetUnit( i )->GetPriceMax();
			fCurrentRating = F( pBuilding->GetHitPoints() / pBuildingStats->fMaxHP, 0, 0, fKillBuildingTime, fTotalPrice );
		}
		if ( !pBestBuilding || fCurrentRating > fRating )
		{
			fRating = fCurrentRating;
			pBestBuilding = pBuilding;
		}
	}
}
const float CPlaneShturmovikShootEstimator::CalcTimeToOpenFire( CAIUnit *pEnemy ) const
{
	const WORD wDir = pOwner->GetDir();
	const WORD wDirToEnemy = GetDirectionByVector( pEnemy->GetCenter() - pOwner->GetCenter() );
	return float( DirsDifference( wDir, wDirToEnemy ) ) / 65535.0f;
	return 0;
}
const float CPlaneShturmovikShootEstimator::CalcRating( CAIUnit *pEnemy, const DWORD dwPossibleGuns ) const
{
	const float fTimeToGo = CalcTimeToOpenFire( pEnemy );

	const float fEnemyKillUsSpeed = pEnemy->GetKillSpeed( pOwner );
	float fEnemyKillUsTime;
	if ( fEnemyKillUsSpeed == 0.0f )
		fEnemyKillUsTime = 0.0f;
	else
		fEnemyKillUsTime = pOwner->GetStats()->fMaxHP / fEnemyKillUsSpeed;

	float fKillEnemySpeed;
	if ( pEnemy == pCurEnemy )
		fKillEnemySpeed = pEnemy->GetTakenDamagePower() - pOwner->GetKillSpeed( pEnemy, dwPossibleGuns );
	else
		fKillEnemySpeed = pEnemy->GetTakenDamagePower();

	float fKillEnemyTime;
	if ( fKillEnemySpeed == 0 )
		fKillEnemyTime = 0;
	else
		fKillEnemyTime = pEnemy->GetStats()->fMaxHP / fKillEnemySpeed;

	const float fEnemyHPPercent = pEnemy->GetHitPoints() / pEnemy->GetStats()->fMaxHP;

	return SConsts::TR_DISTANCE_TO_CENTER_FACTOR / fabs( pEnemy->GetCenter() - vCenter ) + 
		FGunplane( fEnemyHPPercent, fTimeToGo, fEnemyKillUsTime, fKillEnemyTime, pEnemy->GetStats()->fPrice );
}
void CPlaneShturmovikShootEstimator::CollectTarget( CPlaneShturmovikShootEstimator::STargetInfo * pInfo, class CAIUnit *pTarget, const DWORD dwPossibleGuns )
{
/*	const WORD wSpeedDiff = pTarget->GetSpeed() == VNULL2 ? 655535/4 : DirsDifference( GetDirectionByVector( pTarget->GetSpeed() ),
																	GetDirectionByVector( pOwner->GetSpeed() ) );
	
	const WORD wCurDirToTarget = GetDirectionByVector( pTarget->GetCenter() - pOwner->GetCenter() ) - pOwner->GetDir();
*/
	const float fRating = CalcRating( pTarget, dwPossibleGuns );

	if (	!pInfo->pTarget || //первая цель
				(
					fRating > pInfo->fRating  
				) // при прочих равных условиях рейтинг больше
			)
	{
		pInfo->pTarget = pTarget;
		pInfo->fRating = fRating;
	}
}
bool CShootEstimatorForObstacles::AddObstacle( interface IObstacle *pObstacle )
{
	if ( theDipl.GetDiplStatus( pObstacle->GetPlayer(), pOwner->GetPlayer() ) != EDI_ENEMY )
		return false;


	NTimer::STime timeToKill = 0;
	CBasicGun *pGun = pObstacle->ChooseGunToShootToSelf( pOwner, &timeToKill );
	if ( pGun )
	{
		float fTimeToGo = 0;
		/*if ( pOwner->CanMove() && !pOwner->NeedDeinstall() )
			fTimeToGo = fabs( pOwner->GetCenter() - pObstacle->GetCenter() );*/
		fTimeToGo += FindTimeToTurnToPoint( pObstacle->GetCenter(), pOwner, pGun );

		float fRating = F( pObstacle->GetHPPercent(), fTimeToGo, 0, timeToKill, 0 );
		
		if ( !pBest || fCurRating < fRating )
		{
			pBest = pObstacle;
			fCurRating = fRating;
		}
	}
	return false;
}
interface IObstacle * CShootEstimatorForObstacles::GetBest() const
{
	return pBest;
}
