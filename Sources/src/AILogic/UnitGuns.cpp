#include "StdAfx.h"

#include "Technics.h"
#include "Soldier.h"
#include "GunsInternal.h"
#include "Guns.h"
#include "UnitGuns.h"
#include "Turret.h"
#include "StaticObject.h"
#include "Path.h"
#include "Path.h"
#include "PathFinder.h"
#include "Weather.h"
extern NTimer::STime curTime;
extern CWeather theWeather;
BASIC_REGISTER_CLASS( CUnitGuns );
BASIC_REGISTER_CLASS( CMechUnitGuns );
void CUnitGuns::AddGun( const interface IGunsFactory &gunsFactory, const SWeaponRPGStats *pWeapon, int *nGuns, const int nAmmo )
{
	const int nCommonGun = gunsFactory.GetNCommonGun();
	if ( commonGunsInfo.size() <= nCommonGun )
		commonGunsInfo.resize( nCommonGun + 1 );
	if ( gunsBegins.size() <= nCommonGun + 1 )
		gunsBegins.resize( nCommonGun + 2 );

	commonGunsInfo[nCommonGun] = new SCommonGunInfo( false, nAmmo, nCommonGun );

	gunsBegins[nCommonGun] = *nGuns;
	gunsBegins[nCommonGun+1] = *nGuns + pWeapon->shells.size();

	if ( nCommonGuns < nCommonGun + 1 )
		nCommonGuns = nCommonGun + 1;

	if ( guns.size() < *nGuns + pWeapon->shells.size() )
		guns.resize( *nGuns + pWeapon->shells.size() );
	for ( int i = 0; i < pWeapon->shells.size(); ++i )
	{
		CBasicGun *pGun;
		
		if ( pWeapon->shells[i].trajectory == SWeaponRPGStats::SShell::TRAJECTORY_BOMB )
			pGun = gunsFactory.CreateGun( IGunsFactory::PLANE_GUN, i, commonGunsInfo[nCommonGun] );//when create bomb
		else if ( pWeapon->shells[i].trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE )
		{
			if ( pWeapon->shells[i].fArea2 == 0 )
				pGun = gunsFactory.CreateGun( IGunsFactory::MOMENT_CML_GUN, i, commonGunsInfo[nCommonGun] );
			else
				pGun = gunsFactory.CreateGun( IGunsFactory::MOMENT_BURST_GUN, i, commonGunsInfo[nCommonGun] );
		}
		else
		{
			if ( pWeapon->shells[i].fArea2 == 0 )
				pGun = gunsFactory.CreateGun( IGunsFactory::VIS_CML_BALLIST_GUN, i, commonGunsInfo[nCommonGun] );
			else
				pGun = gunsFactory.CreateGun( IGunsFactory::VIS_BURST_BALLIST_GUN, i, commonGunsInfo[nCommonGun] );
		}

		guns[(*nGuns)++] = pGun;
		if ( pGun->GetFireRangeMax() > fMaxFireRange )
			fMaxFireRange = pGun->GetFireRangeMax();

		if ( pGun->GetWeapon()->nCeiling > 0 )
			bCanShootToPlanes = true;

		if ( i == 0 && pGun->GetGun().nPriority == 0 )
			nMainGun = (*nGuns) - 1;
	}
}
void CUnitGuns::Segment()
{
	for ( int i = 0; i < guns.size(); ++i )
		guns[i]->Segment();
}
bool CUnitGuns::FindTimeToGo( CAIUnit *pUnit, CAIUnit *pEnemy, std::list< CUnitGuns::SWeaponPathInfo > *pPathInfo, const SWeaponRPGStats *pStats, CUnitGuns::SWeaponPathInfo *pInfo ) const
{
	const float fFireRangeMax = GetFireRangeMax( pStats, pUnit );
	for ( std::list<SWeaponPathInfo>::iterator iter = pPathInfo->begin(); iter != pPathInfo->end(); ++iter )
	{
		if ( fabs( iter->fRadius - fFireRangeMax ) < 0.01 )
		{
			*pInfo = *iter;
			return true;
		}
	}
	
	CPtr<IStaticPath> pPath = CreateStaticPathForAttack( pUnit, pEnemy, pStats->fRangeMin, fFireRangeMax );

	if ( !pPath || pPath->GetLength() == -1 )
		return false;
	else
	{
		pPathInfo->push_back( SWeaponPathInfo() );
		pPathInfo->back().fRadius = fFireRangeMax;
		pPathInfo->back().time = pPath->GetLength() * SConsts::TILE_SIZE * pUnit->GetStats()->fSpeed;
		pPathInfo->back().pStaticPath = pPath;
		*pInfo = pPathInfo->back();

		return true;
	}
}
void CUnitGuns::FindTimeToTurn( CAIUnit *pOwner, const WORD wWillPower, CTurret *pTurret, CAIUnit *pEnemy, const SVector &finishTile, const bool bIsEnemyInFireRange, NTimer::STime *pTimeToTurn ) const
{
	*pTimeToTurn = 0;
	if ( pOwner->CanRotate() && ( !bIsEnemyInFireRange || pTurret == 0 ) )
	{
		const WORD finishToEnemyDir = GetDirectionByVector( (pEnemy->GetTile() - finishTile).ToCVec2() );
		const WORD dirsDiff( DirsDifference( pOwner->GetDir(), finishToEnemyDir ) );

		if ( dirsDiff > wWillPower )
			*pTimeToTurn = dirsDiff / pOwner->GetRotateSpeed();
	}

	if ( pTurret != 0 )
	{
		WORD startAngle = pTurret->GetHorCurAngle() + pOwner->GetFrontDir();

		WORD finalAngle;
		if ( !bIsEnemyInFireRange )
			finalAngle = GetDirectionByVector( (pEnemy->GetTile() - finishTile).ToCVec2() );
		else
			finalAngle = GetDirectionByVector( (pEnemy->GetTile() - pOwner->GetTile()).ToCVec2() );

		const WORD dirsDiff = DirsDifference( finalAngle, startAngle );

		if ( dirsDiff > wWillPower )
		{
			const WORD wHorizontalRotationSpeed = pTurret->GetHorRotateSpeed();
			NI_ASSERT_SLOW_T( wHorizontalRotationSpeed != 0, NStr::Format("horizontal rotation speed == 0 for \"%s\"", pOwner->GetStats()->szKeyName.c_str()) );
			*pTimeToTurn += DirsDifference( startAngle, finalAngle ) / wHorizontalRotationSpeed;
		}
	}
}
bool CUnitGuns::FindTimeToStatObjGo( CAIUnit *pUnit, CStaticObject *pObj, std::list< SWeaponPathInfo > *pPathInfo, const SWeaponRPGStats *pStats, CUnitGuns::SWeaponPathInfo *pInfo ) const
{
	const float fFireRangeMax = GetFireRangeMax( pStats, pUnit );
	for ( std::list<SWeaponPathInfo>::iterator iter = pPathInfo->begin(); iter != pPathInfo->end(); ++iter )
	{
		if ( fabs( iter->fRadius - fFireRangeMax ) < 0.01 )
		{
			*pInfo = *iter;
			return true;
		}
	}
	
	CPtr<IStaticPath> pPath = CreateStaticPathForStObjAttack( pUnit, pObj, pStats->fRangeMin, fFireRangeMax );

	if ( !pPath.IsValid() || pPath->GetLength() == -1 )
		return false;
	else
	{
		pPathInfo->push_back( SWeaponPathInfo() );
		pPathInfo->back().fRadius = fFireRangeMax;
		pPathInfo->back().time = pPath->GetLength() * SConsts::TILE_SIZE * pUnit->GetStats()->fSpeed;
		pPathInfo->back().pStaticPath = pPath;
		*pInfo = pPathInfo->back();

		return true;
	}
}
CBasicGun* CUnitGuns::ChooseGunForStatObj( CAIUnit *pOwner, CStaticObject *pObj, NTimer::STime *pTime )
{
	if ( pOwner->GetNGuns() == 0 )
		return 0;
	
	*pTime = 0;
	int nGun = -1;
	
	std::list< SWeaponPathInfo > pathInfo( 0 );
	const SWeaponRPGStats *pWStats = pOwner->GetGun(0)->GetWeapon();
	int i = 0;
	do
	{
		CBasicGun *pGun = pOwner->GetGun(i);
		const SWeaponRPGStats::SShell &shell = pGun->GetShell();
		if ( shell.eDamageType != SWeaponRPGStats::SShell::DAMAGE_HEALTH || shell.fArea2 <= 0 )
			pGun->SetRejectReason( ACK_NEGATIVE );
		else if ( pGun->CanShootToObject( pObj ) )
		{
			SWeaponPathInfo info;			
			if ( !pOwner->CanMove() || FindTimeToStatObjGo( pOwner, pObj, &pathInfo, pWStats, &info ) )
			{
				if ( pOwner->CanMove() && !pOwner->CanGoToPoint( info.pStaticPath->GetFinishPoint() ) )
					continue;

				NTimer::STime time;
				if ( pOwner->CanMove() )
					time = info.time;
				else
					time = 0;

				const float fShotsToKill = pObj->GetHitPoints() / pGun->GetDamage();
				const int nBursts = ceil( fShotsToKill / pWStats->nAmmoPerBurst );

				time += nBursts * ( pWStats->nAimingTime + ( pWStats->nAmmoPerBurst - 1 ) * pGun->GetFireRate() );

				if ( time < *pTime || nGun == -1 )
				{
					*pTime = time;
					nGun = i;
				}
			}
		}
	
		++i;
		if ( i < GetNTotalGuns() )
			pWStats = pOwner->GetGun(i)->GetWeapon();
	} while ( i < pOwner->GetNGuns() );

	if ( nGun == -1 )
		return 0;
	else
		return pOwner->GetGun( nGun );
}
void CUnitGuns::SetOwner( CAIUnit *pUnit )
{
	for ( int i = 0; i < guns.size(); ++i )
		guns[i]->SetOwner( pUnit );
}
const SBaseGunRPGStats& CUnitGuns::GetCommonGunStats( const int nCommonGun ) const
{
	NI_ASSERT_T( nCommonGun < nCommonGuns, NStr::Format( "Wrong number of gun (%d), total number of guns (%d)", nCommonGun, nCommonGuns ) );
	return guns[gunsBegins[nCommonGun]]->GetGun();
}
int CUnitGuns::GetNAmmo( const int nCommonGun ) const
{
	NI_ASSERT_T( nCommonGun < nCommonGuns, NStr::Format( "Wrong number of gun (%d), total number of guns (%d)", nCommonGun, nCommonGuns ) );
	return commonGunsInfo[nCommonGun]->nAmmo;
}
void CUnitGuns::ChangeAmmo( const int nCommonGun, const int nAmmo )
{
	NI_ASSERT_T( nCommonGun < nCommonGuns, NStr::Format( "Wrong number of gun (%d), total number of guns (%d)", nCommonGun, nCommonGuns ) );
	commonGunsInfo[nCommonGun]->nAmmo += nAmmo;
	commonGunsInfo[nCommonGun]->nAmmo = Clamp( commonGunsInfo[nCommonGun]->nAmmo, 0, GetNAmmo( nCommonGun ) );
}
const EUnitAckType CUnitGuns::GetRejectReason() const
{
	int nMaxPriority = 10000;
	EUnitAckType eReason = ACK_NONE;
	float fMaxFireRange = -1.0f;

	for ( int i = 0; i < guns.size(); ++i )
	{
		const EUnitAckType &eGunReason = guns[i]->GetRejectReason();
		if ( eGunReason != ACK_NONE )
		{
			const int nPriority = guns[i]->GetGun().nPriority;
			if ( nPriority < nMaxPriority )
			{
				nMaxPriority = guns[i]->GetGun().nPriority;
				fMaxFireRange = guns[i]->GetFireRange( 0.0f );

				eReason = eGunReason;
			}
			else if ( nPriority == nMaxPriority )
			{
				const float fFireRange = guns[i]->GetFireRange( 0.0f );
				if ( fFireRange > fMaxFireRange )
				{
					fMaxFireRange = fFireRange;
					eReason = eGunReason;
				}
			}
		}
	}

	return eReason;
}
bool CUnitGuns::DoesExistRejectReason( const EUnitAckType &ackType ) const
{
	for ( int i = 0; i < guns.size(); ++i )
	{
		if ( guns[i]->GetRejectReason() == ackType )
			return true;
	}

	return false;
}
CBasicGun* CUnitGuns::GetMainGun() const
{
	if ( guns.size() != 0 )
		return guns[nMainGun];
	else
		return 0;
}
float CUnitGuns::GetMaxFireRange( const CAIUnit *pOwner ) const
{
	const float fExpCoeff = pOwner ? pOwner->GetExpLevel().fBonusFireRange : 1.0f;
	return fMaxFireRange * ( 1 + (SConsts::BAD_WEATHER_FIRE_RANGE_COEFFICIENT - 1) * (int)theWeather.IsActive() ) * fExpCoeff;
}
void CMechUnitGuns::Init( CCommonUnit *pCommonUnit )
{
	CAIUnit *pUnit = static_cast<CAIUnit*>(pCommonUnit);
	const SMechUnitRPGStats *pStats = static_cast<const SMechUnitRPGStats*>( pUnit->GetStats() );

	int nGuns = 0;
	float fMaxRevealRadius = 0;

	for ( int i = 0; i < pStats->platforms.size(); ++i )
	{
		for ( int j = pStats->platforms[i].nFirstGun; j < pStats->platforms[i].nFirstGun + pStats->platforms[i].nNumGuns; ++j )
		{
			AddGun( CUnitsGunsFactory( pUnit, j, i-1 ), pStats->guns[j].pWeapon, &nGuns, pStats->guns[j].nAmmo );

			if ( pStats->guns[j].pWeapon->fRevealRadius > fMaxRevealRadius )
				fMaxRevealRadius = pStats->guns[j].pWeapon->fRevealRadius;
		}
	}

	if ( fMaxRevealRadius > 0 )
		pUnit->CreateAntiArtillery( fMaxRevealRadius );

	int i = 0;
	while ( i < GetNGuns() && 
					(
						SWeaponRPGStats::SShell::TRAJECTORY_LINE == GetGun(i)->GetShell().trajectory ||
						GetGun(i)->GetShell().eDamageType == SWeaponRPGStats::SShell::DAMAGE_MORALE ||
						GetGun(i)->GetShell().eDamageType == SWeaponRPGStats::SShell::DAMAGE_FOG
					)
				)
		++i;

	nFirstArtGun = ( i < GetNGuns() ) ? i : -1;
	
	for ( int i = 1; i < GetNGuns(); ++i )
	{
		if ( GetGun( i )->GetGun().nPriority == 0 )
		{
			int j = 0;
			while ( j < i && 
							( GetGun( j )->GetGun().nPriority != 0 || 
							  GetGun( j )->GetCommonGunNumber() == GetGun( i )->GetCommonGunNumber() ) )
				++j;

			if ( j < i )
			{
				GetGun( j )->AddParallelGun( GetGun( i ) );
				GetGun( i )->SetToParallelGun();
			}
		}
	}
}
bool CMechUnitGuns::SetActiveShellType( const enum SWeaponRPGStats::SShell::EDamageType eShellType )
{
	int i = 0;
	while ( i < GetNGuns() && 
					(
					SWeaponRPGStats::SShell::TRAJECTORY_LINE == GetGun(i)->GetShell().trajectory ||
					GetGun(i)->GetShell().eDamageType != eShellType
					)
				)
	{
		++i;
	}
	if ( i < GetNGuns() && nFirstArtGun != i )
	{
		nFirstArtGun = i ;
		return true;
	}
	return false;
}
const int CMechUnitGuns::GetActiveShellType() const
{
	if ( nFirstArtGun == -1 )
		return SWeaponRPGStats::SShell::DAMAGE_HEALTH;
	else
		return GetGun(nFirstArtGun)->GetShell().eDamageType;
}
class CBasicGun* CMechUnitGuns::GetFirstArtilleryGun() const
{ 
	if ( nFirstArtGun >= 0 ) 
		return GetGun( nFirstArtGun ); 
	else return 0; 
}
void CInfantryGuns::Init( CCommonUnit *pCommonUnit )
{
	CSoldier *pUnit = static_cast<CSoldier*>(pCommonUnit);
	const SInfantryRPGStats *pStats = static_cast<const SInfantryRPGStats*>( pUnit->GetStats() );

	int nGuns = 0;
	for ( int i = 0; i < pStats->guns.size(); ++i )
		AddGun( CUnitsGunsFactory( pUnit, i, -1 ), pStats->guns[i].pWeapon, &nGuns, pStats->guns[i].nAmmo );
}
