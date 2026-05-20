#include "stdafx.h"

#include "Shell.h"
#include "AIStaticMap.h"
#include "AIUnit.h"
#include "Randomize.h"
#include "UnitsIterators2.h"
#include "Updater.h"
#include "StaticObjects.h"
#include "Guns.h"
#include "HitsStore.h"
#include "StaticObject.h"
#include "CombatEstimator.h"
#include "Diplomacy.h"
#include "AIWarFog.h"
#include "Weather.h"
#include "DifficultyLevel.h"
#include "Cheats.h"
#include "MPLog.h"
#include "StaticObjectsIters.h"

#include "..\Scene\Scene.h"
#include "..\Misc\CheckSums.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CCombatEstimator theCombatEstimator;
extern CStaticMap theStaticMap;
extern CUpdater updater;
extern NTimer::STime curTime;
extern CStaticObjects theStatObjs;
extern CHitsStore theHitsStore;
CShellsStore theShellsStore;
extern CDiplomacy theDipl;
extern CGlobalWarFog theWarFog;
extern CWeather theWeather;
extern CDifficultyLevel theDifficultyLevel;
extern SCheats theCheats;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CHitInfo															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CHitInfo::CHitInfo( const class CExplosion *pExpl, IRefCount *_pVictim, const enum SAINotifyHitInfo::EHitType &_eHitType, const CVec3 &_explCoord )
: pWeapon( pExpl->GetWeapon() ), wShell( pExpl->GetShellType() ), wDir( pExpl->GetAttackDir() ), 
	pVictim( _pVictim ), eHitType( _eHitType ), explCoord( _explCoord )
{
	SetUniqueId();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CHitInfo::GetHitInfo( SAINotifyHitInfo *pHitInfo ) const 
{ 
	pHitInfo->explCoord = explCoord;
	pHitInfo->pVictim = pVictim;
	pHitInfo->pWeapon = pWeapon;
	pHitInfo->wDir = wDir;
	pHitInfo->wShell = wShell;
	pHitInfo->eHitType = eHitType;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												CExplosion																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplosion::Init(	CAIUnit *_pUnit, 
												const SWeaponRPGStats *_pWeapon, 
												const float fDispersion, 
												const float fDispRatio,
												const CVec2 &_explCoord, 
												const float _z, 
												const CVec2 &attackerPos, 
												const BYTE _nShellType, 
												const bool bRandomize, 
												const int _nPlayerOfShoot )
{
	pUnit = _pUnit;
	pWeapon = _pWeapon;
	nShellType = _nShellType;
	z = _z;
	nPlayerOfShoot = _nPlayerOfShoot;

	CVec2 vRand( VNULL2 );
	if ( bRandomize )
	{
		const float fFireRangeMax = GetFireRangeMax( pWeapon, pUnit );
		const float fDispRadius = GetDispByRadius( fDispersion, fFireRangeMax, fabs( attackerPos - _explCoord ) );
		RandQuadrInCircle( fDispRadius, &vRand, fDispRatio, _explCoord-attackerPos );
	}

	explCoord = _explCoord + vRand;
	attackDir = GetDirectionByVector( attackerPos - explCoord );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CExplosion::CExplosion( CAIUnit *pUnit, const SWeaponRPGStats *pWeapon, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize )
{
	if ( pUnit != 0 )
		Init( pUnit, pWeapon, pWeapon->fDispersion, 1, explCoord, z, attackerPos, nShellType, bRandomize, pUnit->GetPlayer() );
	else
		Init( pUnit, pWeapon, pWeapon->fDispersion, 1, explCoord, z, attackerPos, nShellType, bRandomize, theDipl.GetNeutralPlayer() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CExplosion::CExplosion( CAIUnit *pUnit, const CBasicGun *pGun, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize )
{
	float fDispRatio = pGun->GetDispRatio( nShellType, fabs(explCoord-attackerPos) );
	if ( pUnit != 0 )
		Init( pUnit, pGun->GetWeapon(), pGun->GetDispersion(), fDispRatio, explCoord, z, attackerPos, nShellType, bRandomize, pUnit->GetPlayer() );
	else
		Init( pUnit, pGun->GetWeapon(), pGun->GetDispersion(), fDispRatio, explCoord, z, attackerPos, nShellType, bRandomize, theDipl.GetNeutralPlayer() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SAINotifyHitInfo::EHitType CExplosion::ProcessExactHit( CAIUnit *pTarget, const SRect &combatRect, const CVec2 &explCoord, const int nRandPiercing, const int nRandArmor ) const
{
	// попали по комбат системе
	if ( combatRect.IsPointInside( explCoord ) )
	{
		// пробили
		if ( nRandPiercing >= nRandArmor && !pTarget->IsSavedByCover() )
			return SAINotifyHitInfo::EHT_HIT;
		else
			return SAINotifyHitInfo::EHT_REFLECT;
	}
	else
		return SAINotifyHitInfo::EHT_MISS;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CExplosion::GetRandomPiercing() const
{
	return
		pWeapon->shells[nShellType].GetRandomPiercing() *
		theDifficultyLevel.GetPiercingCoeff( theDipl.GetNParty( nPlayerOfShoot ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CExplosion::GetMaxDamage() const
{
	const float fMaxDamage = pWeapon->shells[nShellType].fDamagePower + pWeapon->shells[nShellType].nDamageRandom;
	return
		fMaxDamage * theDifficultyLevel.GetDamageCoeff( theDipl.GetNParty( nPlayerOfShoot ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CExplosion::GetRandomDamage() const
{
	const float fRandomDamage = pWeapon->shells[nShellType].GetRandomDamage();
	return
		fRandomDamage * theDifficultyLevel.GetDamageCoeff( theDipl.GetNParty( nPlayerOfShoot ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CExplosion::GetPartyOfShoot() const 
{
	NI_ASSERT_T( nPlayerOfShoot != -1, "Invalid shooting player" );
	return theDipl.GetNParty( nPlayerOfShoot );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CExplosion::GetPlayerOfShoot() const
{
	NI_ASSERT_T( nPlayerOfShoot != -1, "Invalid shooting player" );
	return nPlayerOfShoot; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExplosion::ProcessMoraleExplosion() const
{
	const SWeaponRPGStats::SShell &rShell = pWeapon->shells[nShellType];
	if ( rShell.eDamageType == SWeaponRPGStats::SShell::DAMAGE_MORALE )
	{
		// взрыв моральный
		updater.Update( ACTION_NOTIFY_HIT, new CHitInfo( pWeapon, nShellType, attackDir, CVec3( GetExplCoordinates(), GetExplZ() ), SAINotifyHitInfo::EHT_GROUND ) );
		// всем врагам в радиусе понизить мораль
		for ( CUnitsIter<0,0> iter( pUnit->GetParty(), EDI_ENEMY, GetExplCoordinates(), rShell.fArea ); !iter.IsFinished(); iter.Iterate() )
		{
			CPtr<CAIUnit> curUnit = *iter;
			curUnit->SetMorale( curUnit->GetMorale() - pWeapon->shells[nShellType].GetRandomDamage() / 100.0f );
		}

		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExplosion::ProcessSmokeScreenExplosion() const
{
	const SWeaponRPGStats::SShell &rShell = pWeapon->shells[nShellType];
	if ( rShell.eDamageType == SWeaponRPGStats::SShell::DAMAGE_FOG )
	{
		// большой радиус взрыва - радиус завесы,
		// nPiercing - прозрачность,
		// fDamage - время существования
		theStatObjs.AddNewSmokeScreen(
			GetExplCoordinates(),
			pWeapon->shells[nShellType].fArea,
			pWeapon->shells[nShellType].nPiercing,
			pWeapon->shells[nShellType].fDamagePower );

		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplosion::AddHitToSend( CHitInfo *pHit )
{
	//чтобы удалилось
	CPtr<CHitInfo> memHit = pHit;
	if ( pHitToSend == 0 || pHitToSend->eHitType != SAINotifyHitInfo::EHT_HIT )
		pHitToSend = pHit;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												CCumulativeExpl														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CCumulativeExpl::CCumulativeExpl( CAIUnit *pUnit, const CBasicGun *pGun, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize )
: CExplosion( pUnit, pGun, explCoord, z, attackerPos, nShellType, bRandomize )
{
	if ( pUnit && pUnit->GetZ() > GetExplZ() )
		nArmorDir = 2;
	else
		nArmorDir = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SAINotifyHitInfo::EHitType GetHitType( const CVec2 &vPoint )
{
	const SVector hitTile( AICellsTiles::GetTile( vPoint ) );
	const ETerrainTypes eType = theStaticMap.GetTerrainType( hitTile.x, hitTile.y );

	switch ( eType )
	{
		case ETT_EARTH_TERRAIN:			return SAINotifyHitInfo::EHT_GROUND;
		case ETT_EARTH_SEA_TERRAIN: return SAINotifyHitInfo::EHT_NONE;
		case ETT_RIVER_TERRAIN:			return SAINotifyHitInfo::EHT_WATER;
	}

	return SAINotifyHitInfo::EHT_NONE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCumulativeExpl::Explode()
{
	if ( ProcessMoraleExplosion() ) return;
	if ( ProcessSmokeScreenExplosion() ) 
	{
		updater.Update( ACTION_NOTIFY_HIT, new CHitInfo( pWeapon, nShellType, attackDir, CVec3( GetExplCoordinates(), GetExplZ() ),	GetHitType( explCoord ) ) );
		return;
	}

	theHitsStore.AddHit( explCoord, CHitsStore::EHT_OPEN_SIGHT );

	bool bHit = false;
	bool bSoldierHit = false;
	
	// по юнитам
	for ( CUnitsIter<0,0> iter( 0, ANY_PARTY, explCoord, 0.0f ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pTarget = *iter;
		if ( IsValidObj( pTarget ) && pUnit.GetPtr() != pTarget )
		{
			if ( nShellType == SWeaponRPGStats::SShell::TRAJECTORY_LINE || nShellType == SWeaponRPGStats::SShell::TRAJECTORY_GRENADE )
				pTarget->Grazed( pUnit );
			
			// target жив, target не тот, кто стрелял и по высоте совпадает с высотой взрыва
			if ( !bSoldierHit || !pTarget->GetStats()->IsInfantry() )
			{
				// чтобы не пропускался вызов функции из-за оптимизации вычисления bool выражений
				const bool bExplResult = pTarget->ProcessCumulativeExpl( this, nArmorDir, false );
				bHit = bHit || bExplResult;

				bSoldierHit = bSoldierHit || bExplResult && pTarget->GetStats()->IsInfantry();
				
				if ( nShellType == SWeaponRPGStats::SShell::TRAJECTORY_LINE || nShellType == SWeaponRPGStats::SShell::TRAJECTORY_GRENADE )
				{
					CAIUnit *pWhoFire = GetWhoFire();
					if ( IsValidObj( pWhoFire ) )
						pWhoFire->WantedToReveal( pTarget );
				}
			}
		}
	}

	if ( GetExplZ() == 0.0f )
	{
		// нельзя создавать 2 итератора по статическим объектам, внутри ProcessCumulativeExpl
		// итератор нужен, значит здесь нельзя заводить итератор.
		std::list<CExistingObject*> hitObjects;
		
		// по статическим объектам
		for ( CStObjCircleIter<false> iter( explCoord, 0 ); !iter.IsFinished(); iter.Iterate() )
		{
			CExistingObject *pObj = *iter;
			if ( pObj->IsAlive() )
				hitObjects.push_back( pObj );
		}
		
		for ( std::list<CExistingObject*>::iterator it = hitObjects.begin(); it != hitObjects.end(); ++it )
		{
			// чтобы не пропускался вызов функции из-за оптимизации вычисления bool выражений			
			const bool bExplResult = (*it)->ProcessCumulativeExpl( this, nArmorDir, false );
			bHit = bHit || bExplResult;
		}
	}
	
	// ни в кого не попало
	if ( !bHit )
	{
		if ( GetExplZ() <= 0 ) 
			updater.Update( ACTION_NOTIFY_HIT, new CHitInfo( pWeapon, nShellType, attackDir, CVec3( GetExplCoordinates(), GetExplZ() ),	GetHitType( explCoord ) ) );
		else
			updater.Update( ACTION_NOTIFY_HIT, new CHitInfo( pWeapon, nShellType, attackDir, CVec3( GetExplCoordinates(), GetExplZ() ),	SAINotifyHitInfo::EHT_AIR ) );
	}
	else if ( pHitToSend != 0 )
		updater.Update( ACTION_NOTIFY_HIT, pHitToSend );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												CBurstExpl																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBurstExpl::CBurstExpl( CAIUnit *pUnit, const CBasicGun *pGun, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize, int ArmorDir )
: CExplosion( pUnit, pGun, explCoord, z, attackerPos, nShellType, bRandomize ), nArmorDir( ArmorDir )
{
	if ( pWeapon->shells[nShellType].trajectory != SWeaponRPGStats::SShell::TRAJECTORY_LINE || (pUnit && pUnit->GetZ() > GetExplZ()) )
		nArmorDir = 2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBurstExpl::CBurstExpl( CAIUnit *pUnit, const SWeaponRPGStats *pWeapon, const CVec2 &explCoord, const float z, const CVec2 &attackerPos, const BYTE nShellType, const bool bRandomize, int ArmorDir )
: CExplosion( pUnit, pWeapon, explCoord, z, attackerPos, nShellType, bRandomize ), nArmorDir( ArmorDir )
{ 
	if ( pWeapon->shells[nShellType].trajectory != SWeaponRPGStats::SShell::TRAJECTORY_LINE || (pUnit && pUnit->GetZ() > GetExplZ()) )
		nArmorDir = 2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBurstExpl::Explode()
{
	if ( ProcessMoraleExplosion() ) return;
	if ( ProcessSmokeScreenExplosion() ) 
	{
		updater.Update( ACTION_NOTIFY_HIT, new CHitInfo( pWeapon, nShellType, attackDir, CVec3( GetExplCoordinates(), GetExplZ() ), GetHitType( explCoord ) ) );
		return;
	}
		
	if ( nArmorDir != 2 )
		theHitsStore.AddHit( explCoord, CHitsStore::EHT_OPEN_SIGHT );
	else
		theHitsStore.AddHit( explCoord, CHitsStore::EHT_OVER_SIGHT );
	
	const float &fRadius = pWeapon->shells[nShellType].fArea2;
	const float &fSmallRadius = pWeapon->shells[nShellType].fArea;
	NI_ASSERT_T( fRadius != 0, "Неверный тип взрыва" );

	bool bHit = false;
	// по юнитам
	for ( CUnitsIter<0,0> iter( 0, ANY_PARTY, explCoord, fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pTarget = *iter;
		if ( IsValidObj( pTarget ) )
		{
			if ( pTarget != pUnit &&
					 ( nShellType == SWeaponRPGStats::SShell::TRAJECTORY_LINE || nShellType == SWeaponRPGStats::SShell::TRAJECTORY_GRENADE ) )
				pTarget->Grazed( pUnit );

			// чтобы не пропускался вызов функции из-за оптимизации вычисления bool выражений
			const bool bExplResult = pTarget->ProcessBurstExpl( this, nArmorDir, fRadius, fSmallRadius );
			bHit = bHit || bExplResult;

			if ( nShellType == SWeaponRPGStats::SShell::TRAJECTORY_LINE || nShellType == SWeaponRPGStats::SShell::TRAJECTORY_GRENADE )
			{
				CAIUnit *pWhoFire = GetWhoFire();
				if ( IsValidObj( pWhoFire ) )
					pWhoFire->WantedToReveal( pTarget );
			}
		}
	}

	if ( GetExplZ() == 0.0f )
	{	
		// по статическим объектам
		// нельзя создавать 2 итератора по статическим объектам, внутри ProcessCumulativeExpl
		// итератор нужен, значит здесь нельзя заводить итератор.
		std::list<CExistingObject*> hitObjects;
		
		// по статическим объектам
		for ( CStObjCircleIter<false> iter( explCoord, fSmallRadius ); !iter.IsFinished(); iter.Iterate() )
		{
			CExistingObject *pObj = *iter;
			if ( pObj->IsAlive() )
				hitObjects.push_back( pObj );
		}
		for ( std::list<CExistingObject*>::iterator it = hitObjects.begin(); it != hitObjects.end(); ++it )
		{
			// чтобы не пропускался вызов функции из-за оптимизации вычисления bool выражений			
			const bool bExplResult = (*it)->ProcessBurstExpl( this, nArmorDir, fRadius, fSmallRadius );
			bHit = bHit || bExplResult;
		}
	}

	// так никуда и не попали
	if ( !bHit )
	{
		if ( GetExplZ() <= 0 ) 
			updater.Update( ACTION_NOTIFY_HIT, new CHitInfo( pWeapon, nShellType, attackDir, CVec3( GetExplCoordinates(), GetExplZ() ), GetHitType( explCoord ) ) );
		else
			updater.Update( ACTION_NOTIFY_HIT, new CHitInfo( pWeapon, nShellType, attackDir, CVec3( GetExplCoordinates(), GetExplZ() ), SAINotifyHitInfo::EHT_AIR ) );
	}
	else if ( pHitToSend != 0 )
		updater.Update( ACTION_NOTIFY_HIT, pHitToSend );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CShell																	*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CShell::CShell( const NTimer::STime &_explTime, CExplosion *_expl, const int _nGun )
: explTime( _explTime ), expl( _expl ), nGun( _nGun )
{
	CVec2 vOwnerCenter;
	CAIUnit *pWhoFire = expl->GetWhoFire();
	if ( pWhoFire == 0 )
		vOwnerCenter = expl->GetExplCoordinates();
	else
		vOwnerCenter = pWhoFire->GetCenter();
	vStartVisZ = theStaticMap.GetVisZ( vOwnerCenter.x, vOwnerCenter.y );

	const CVec2 vExplCoord( expl->GetExplCoordinates() );
	vFinishVisZ = theStaticMap.GetVisZ( vExplCoord.x, vExplCoord.y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IRefCount* CShell::GetWhoFired() const 
{ 
	return expl->GetWhoFire(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												CVisShell																	*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisShell::GetPlacement(  SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff )
{
	pPlacement->pObj = this;
	pPlacement->dir = pTraj->GetStart2DDir();

	CVec3 pos( center );
	CVec3 vSpeed3;
	GetSpeed3( &vSpeed3 );
	pos -= timeDiff * vSpeed3;

	pPlacement->center.x = pos.x;
	pPlacement->center.y = pos.y;
	pPlacement->z = pos.z;

	pPlacement->fSpeed = fabs( vSpeed3 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CVisShell::IsVisibleByPlayer() const
{
	return theCheats.IsHistoryPlaying() || bVisible;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisShell::CalcVisibility()
{
	const bool bVisibleByPlayer = theWarFog.IsTileVisible( AICellsTiles::GetTile( center.x, center.y ), theDipl.GetMyParty() );
	if ( bVisible != bVisibleByPlayer )
	{
		bVisible = bVisibleByPlayer;
		updater.Update( ACTION_NOTIFY_CHANGE_VISIBILITY, this, IsVisibleByPlayer() );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisShell::Segment()
{
	const CVec3 oldCenter( center );
	center = pTraj->GetCoordinates();
	speed = ( center - oldCenter ) / SConsts::AI_SEGMENT_DURATION;

	updater.Update( ACTION_NOTIFY_PLACEMENT, this );

	CalcVisibility();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisShell::GetProjectileInfo( struct SAINotifyNewProjectile *pProjectileInfo )
{
	pProjectileInfo->pObj = this;
	pProjectileInfo->pSource = GetWhoFired();
	pProjectileInfo->nGun = GetNGun();
	pProjectileInfo->nShell = GetShellType();
	pProjectileInfo->flyingTime = GetExplTime() - GetStartTime();
	pProjectileInfo->startTime = GetStartTime();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CVisShell::GetTerrainHeight( const float x, const float y, const NTimer::STime timeDiff ) const
{
	float fRatio;
	if ( curTime - timeDiff < GetStartTime() )
		fRatio = 0;
	else
		fRatio = float( curTime - timeDiff - GetStartTime() ) / float( GetExplTime() - GetStartTime() );
	
	if ( pTraj->GetTrajType() == SWeaponRPGStats::SShell::TRAJECTORY_BOMB )
		return GetFinishVisZ() * fRatio;
	else 
		return GetStartVisZ() * ( 1 - fRatio ) + GetFinishVisZ() * fRatio;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*								  CShellsStore																		*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CShellsStore::AddShell( CMomentShell &shell )
{
	shell.Explode();
	theCombatEstimator.AddShell( curTime, shell.GetMaxDamage() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CShellsStore::AddShell( CInvisShell *pShell )
{ 
	invisShells.push( pShell );
	theCombatEstimator.AddShell( curTime, pShell->GetMaxDamage() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CShellsStore::AddShell( CVisShell *pShell )
{
	visShells.push_back( pShell );
	updater.Update( ACTION_NOTIFY_NEW_PROJECTILE, pShell );

	if ( pShell->GetTrajectoryType() == SWeaponRPGStats::SShell::TRAJECTORY_LINE ||
			 pShell->GetTrajectoryType() == SWeaponRPGStats::SShell::TRAJECTORY_GRENADE )
		theCombatEstimator.AddShell( curTime, pShell->GetMaxDamage() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CShellsStore::Segment()
{
	// взорвать невидимые снаряды
	while ( !invisShells.empty() && invisShells.top()->GetExplTime() <= curTime + SConsts::AI_SEGMENT_DURATION / 2 )
	{
		invisShells.top()->Explode();
		invisShells.pop();
	}

	// обновить видимые
	std::list< CPtr<CVisShell> >::iterator iter = visShells.begin();
	while ( iter != visShells.end() )
	{
		CVisShell *shell = *iter;
		// долетел
		if ( shell->GetExplTime() <= curTime )
		{
			shell->Explode();
			updater.Update( ACTION_NOTIFY_DEAD_PROJECTILE, shell );
			iter = visShells.erase( iter );
		}
		else
		{
			shell->Segment();
			++iter;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CShellsStore::Clear()
{
	while ( !invisShells.empty() )
		invisShells.pop();

	visShells.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CShellsStore::UpdateCheckSum( uLong *pCheckSum )
{
	using namespace NCheckSums;

	static SCheckSumBufferStorage checkSumBuf( 10000 );
	checkSumBuf.nCnt = 0;

	std::priority_queue< CPtr<CInvisShell>, std::vector< CPtr<CInvisShell> >, SInvisShellCmp > copyQueue = invisShells;
	while ( !copyQueue.empty() )
	{
		CInvisShell *pShell = copyQueue.top();
		copyQueue.pop();
		
		const CVec2 vExplCenter = pShell->GetExplCoordinates();
		const NTimer::STime explTime = pShell->GetExplTime();

		CopyToBuf( &checkSumBuf, vExplCenter );
		CopyToBuf( &checkSumBuf, explTime );
	}

	for ( std::list< CPtr<CVisShell> >::iterator iter = visShells.begin(); iter != visShells.end(); ++iter )
	{
		CVisShell *pShell = *iter;
		const CVec2 vExplCenter = pShell->GetExplCoordinates();
		const CVec3 vCurCenter = pShell->GetCoordinates();
		const NTimer::STime explTime = pShell->GetExplTime();

		CopyToBuf( &checkSumBuf, vExplCenter );
		CopyToBuf( &checkSumBuf, vCurCenter );
		CopyToBuf( &checkSumBuf, explTime );
	}

	*pCheckSum = crc32( *pCheckSum, &(checkSumBuf.buf[0]), checkSumBuf.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CBombBallisticTraj													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBombBallisticTraj::CBombBallisticTraj( const CVec3 &_point, const CVec3 &_v, const NTimer::STime &_explTime, const CVec2 &_vRandAcc )
: point( _point ), v( _v ), wDir( GetDirectionByVector( CVec2( _v.x, _v.y ) ) ), 
	startTime( curTime ), explTime( _explTime ), vRandAcc( _vRandAcc )
{ 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CBombBallisticTraj::CalcTrajectoryFinish( const CVec3 &vSourcePoint, const CVec3 &vInitialSpeed, const CVec2 &vRandAcc )
{
	const float fTimeOfFly = GetTimeOfFly( vSourcePoint.z, vInitialSpeed.z );
	const float fTimeOfFly2 = sqr( fTimeOfFly );

	const float fCoeff = GetCoeff( fTimeOfFly );
	return CVec3( vSourcePoint.x + vInitialSpeed.x * fCoeff + vRandAcc.x * fTimeOfFly2 / 2.0f, vSourcePoint.y + vInitialSpeed.y * fCoeff + vRandAcc.y * fTimeOfFly2 / 2.0f, 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 CBombBallisticTraj::GetCoordinates() const
{
	const float timeDiff = curTime - startTime;
	const float timeDiff2 = sqr( timeDiff );
	const float fCoeff = GetCoeff( timeDiff );
	const float vPointX = v.x * fCoeff;
	const float vPointY = v.y * fCoeff;
	const float vPointZ = v.z * timeDiff - SConsts::TRAJECTORY_BOMB_G * timeDiff2 / 2;

	return CVec3( point.x + vPointX + vRandAcc.x * timeDiff2 / 2.0f, point.y + vPointY + vRandAcc.y * timeDiff2 / 2.0f, point.z + vPointZ );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CBombBallisticTraj::GetCoeff( const float &timeDiff )
{
	return ( 1 - exp( -1.0f * SConsts::TRAJ_BOMB_ALPHA * timeDiff ) ) / SConsts::TRAJ_BOMB_ALPHA;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CBombBallisticTraj::GetTimeOfFly( const float fZ, const float fZSpeed )
{
	return ( sqrt( sqr(fZSpeed) + 2 * SConsts::TRAJECTORY_BOMB_G * fZ ) + fZSpeed ) / SConsts::TRAJECTORY_BOMB_G;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										CFakeBallisticTraj														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFakeBallisticTraj::CFakeBallisticTraj( const CVec3 &_point, const CVec3 &_v, const NTimer::STime &_explTime, const float _A1, const float _A2 )
: point( _point ), v( _v ), wDir( GetDirectionByVector( CVec2( _v.x, _v.y ) ) ), 
	startTime( curTime ), explTime( _explTime ), A1( _A1 ), A2( _A2 ) 
{ 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 CFakeBallisticTraj::GetCoordinates() const
{
	const NTimer::STime timeDiff = curTime - startTime;
	const CVec3 firstPoint = v * timeDiff;
	const float r = fabs( CVec2( firstPoint.x, firstPoint.y ) );

	return CVec3 ( point.x + firstPoint.x, point.y + firstPoint.y, 
								 point.z + firstPoint.z + A1 * sqr( r ) + A2 * r );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CBallisticTraj													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBallisticTraj::CBallisticTraj( const CVec3 &_vStart, const CVec2 &vFinish, float fV, const SWeaponRPGStats::SShell::ETrajectoryType _eType, WORD wMaxAngle, float fMaxRange )
: startTime( curTime ), vStart3D( _vStart ), eType( _eType )
{
	const CVec2 vStart( vStart3D.x, vStart3D.y );
	vDir = vFinish - vStart;
	const float x0 = fabs( vDir );
	Normalize( &vDir );
	wDir = GetDirectionByVector( vDir );

	if ( eType == SWeaponRPGStats::SShell::TRAJECTORY_HOWITZER )
	{
		wAngle = wMaxAngle + 65535/4*3;
		const CVec2 vSin = GetVectorByDirection( wAngle );
		fG = 2.0f * sqr( fV ) * vSin.x * vSin.y / x0;
		fVx = vSin.x * fV;
		fVy = vSin.y * fV;
	}
	else
	{	
		fV = sqr( fV );
		fG = fV / fMaxRange / 2;
		const float fCoeff = fG * x0;
		// добавить скорости, если не хватает
		if ( fV < fCoeff + 0.001f )
			fV = fCoeff + 0.001f;

		const float fSqrt1 = sqrt( fV + fCoeff );
		const float fSqrt2 = sqrt( fV - fCoeff );

		// крутая траектория
		/*if ( eType == SWeaponRPGStats::SShell::TRAJECTORY_GRENADE )
		{
 			fVx = 0.5f * ( fSqrt1 - fSqrt2 );
			fVy = 0.5f * ( fSqrt1 + fSqrt2 );
		}
		// пологая траектория
		else*/
		{
			fVx = 0.5f * ( fSqrt1 + fSqrt2 );
			fVy = 0.5f * ( fSqrt1 - fSqrt2 );
		}
		wAngle = GetDirectionByVector( fVx, fVy );
	}

	
	explTime = startTime + x0 / fVx;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 CBallisticTraj::GetCoordinates() const
{
	const float fT = curTime - startTime;
	const CVec3 vRet = vStart3D + CVec3( vDir * fVx * fT, fVy * fT - fG * sqr( fT ) / 2 );
	return vRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CBallisticTraj::GetTrajectoryZAngle( const CVec2 &vToAim, const float z, float fV, const SWeaponRPGStats::SShell::ETrajectoryType eType, WORD wMaxAngle, float fMaxRange )
{
	const CBallisticTraj traj( VNULL3, vToAim, fV, eType, wMaxAngle, fMaxRange );
	return traj.wAngle;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
