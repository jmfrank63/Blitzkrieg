#include "stdafx.h"

#include "Graveyard.h"
#include "Updater.h"
#include "AIStaticMap.h"
#include "AIUnit.h"
#include "StaticObjects.h"
#include "AIWarFog.h"
#include "Units.h"
#include "SuspendedUpdates.h"
#include "PathUnit.h"
#include "Diplomacy.h"
#include "StaticObject.h"
CGraveyard theGraveyard;

extern CUpdater updater;
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
extern CStaticObjects theStatObjs;
extern CGlobalWarFog theWarFog;
extern CUnits units;
extern CSuspendedUpdates theSuspendedUpdates;
extern CDiplomacy theDipl;
void CGraveyard::Segment()
{
	std::list<SKilledUnit>::iterator iter = killed.begin();
	while ( iter != killed.end() )
	{
		SKilledUnit unit = *iter;
		if ( iter->bFatality && iter->actionTime != 0 && curTime >= iter->actionTime )
		{
			iter->pUnit->UnlockTiles();
			theStaticMap.UpdateMaxesByTiles( iter->lockedTiles, AI_CLASS_ANY, true );
			iter->actionTime = 0;
		}

		if ( !iter->bAnimFinished && curTime >= iter->timeToEndDieAnimation	)
		{
			iter->bAnimFinished = true;
			iter->endFogTime = curTime + SConsts::DEAD_SEE_TIME;
			CAIUnit *pUnit = iter->pUnit;
			iter->endSceneTime = curTime + pUnit->GetDisappearInterval() + 2 * SConsts::AI_SEGMENT_DURATION;

			++iter;
		}
		else if ( iter->bAnimFinished )
		{
			if ( iter->endSceneTime != 0 && 
					 iter->endSceneTime >= curTime + SConsts::TIME_OF_PRE_DISAPPEAR_NOTIFY &&
					 iter->endSceneTime < curTime + SConsts::TIME_OF_PRE_DISAPPEAR_NOTIFY + SConsts::AI_SEGMENT_DURATION )
			{
				CAIUnit *pUnit = iter->pUnit;
				if ( !pUnit->GetStats()->IsInfantry() )
					updater.Update( ACTION_NOTIFY_PRE_DISAPPEAR, pUnit, SConsts::TIME_OF_PRE_DISAPPEAR_NOTIFY );
			}

			if ( iter->endSceneTime != 0 && curTime >= iter->endSceneTime )
			{
				iter->endSceneTime = 0;

				if ( iter->bFatality )
					theStaticMap.UpdateMaxesByTiles( iter->lockedTiles, AI_CLASS_ANY, false );
				else
				{
					iter->pUnit->UnlockTiles();
				}

				if ( !iter->pUnit->GetStats()->IsInfantry() )
				{
					const bool bPutMud = !theStaticMap.IsBridge( iter->pUnit->GetTile() );
					CDeadUnit *pDeadUnit = new CDeadUnit( iter->pUnit, 0, ACTION_NOTIFY_NONE, iter->pUnit->GetDBID(), bPutMud );

					updater.Update( ACTION_NOTIFY_DISSAPEAR_UNIT, pDeadUnit );
				}
			}

			if ( iter->endFogTime != 0 && curTime >= iter->endFogTime )
			{
				iter->endFogTime = 0;
				theWarFog.DeleteUnit( iter->pUnit->GetID() );
				iter->bFogDeleted = true;
			}

			if ( iter->endFogTime == 0 && iter->endSceneTime == 0 && iter->bSentDead )
			{
				units.FullUnitDelete( iter->pUnit );
				iter = killed.erase( iter );
			}
			else
				++iter;
		}
		else
			++iter;
	}

	CheckSoonBeDead();
}
void CGraveyard::GetDeadUnits( SAINotifyDeadAtAll **pDeadUnitsBuffer, int *pnLen )
{
	NTimer::STime curTime = GetAIGetSegmTime( GetSingleton<IGameTimer>()->GetGameSegmentTimer() );

	*pnLen = 0;
	const int nSize = killed.size() + theSuspendedUpdates.GetNRecalled( ACTION_NOTIFY_GET_DEAD_UNITS_UPDATE );
	*pDeadUnitsBuffer = GetTempBuffer<SAINotifyDeadAtAll>( nSize );

	for ( std::list<SKilledUnit>::iterator iter = killed.begin(); iter != killed.end(); ++iter )
	{
		if ( !iter->bSentDead && curTime >= iter->timeToEndDieAnimation && !iter->pUnit->GetStats()->IsAviation() )
		{
			::new ( &(*pDeadUnitsBuffer)[(*pnLen)] ) SAINotifyDeadAtAll();
			
			(*pDeadUnitsBuffer)[(*pnLen)].pObj = iter->pUnit;
			(*pDeadUnitsBuffer)[(*pnLen)].bRot = !iter->pUnit->GetStats()->IsInfantry() || bridgeSoldiersSet.find( iter->pUnit ) != bridgeSoldiersSet.end();

			if ( theDipl.GetDiplStatusForParties( iter->pUnit->GetParty(), theDipl.GetMyParty() ) != EDI_ENEMY ||
					 !theSuspendedUpdates.CheckToSuspend( ACTION_NOTIFY_GET_DEAD_UNITS_UPDATE, iter->pUnit, (*pDeadUnitsBuffer)[(*pnLen)] ) )
				++(*pnLen);

			iter->bSentDead = true;
		}
	}

	while ( !theSuspendedUpdates.IsRecalledEmpty( ACTION_NOTIFY_GET_DEAD_UNITS_UPDATE ) )
	{
		::new ( &(*pDeadUnitsBuffer)[(*pnLen)] ) SAINotifyDeadAtAll();
		theSuspendedUpdates.GetRecalled( ACTION_NOTIFY_GET_DEAD_UNITS_UPDATE, &(*pDeadUnitsBuffer)[(*pnLen)] );
		++(*pnLen);
	}
}
void CGraveyard::AddKilledUnit( CAIUnit *pUnit, const NTimer::STime &timeOfVisDeath, const int nFatality )
{
	pUnit->UnfixUnlocking();
	if ( pUnit->GetPathUnit()->CanLockTiles() )
		pUnit->ForceLockingTiles();
		
	SKilledUnit killInfo;
	const SUnitBaseRPGStats *pStats = pUnit->GetStats();
	if ( nFatality > -1 )
	{
		killInfo.actionTime = timeOfVisDeath + pStats->animdescs[ANIMATION_DEATH_FATALITY][nFatality].nAction;
		killInfo.bAnimFinished = false;
		killInfo.bFatality = true;
		killInfo.bSentDead = false;
		killInfo.endFogTime = 0;
		killInfo.endSceneTime = 0;

		const int nAABBD = pStats->animdescs[ANIMATION_DEATH_FATALITY][nFatality].nAABB_D;
		SRect finishRect;

		const CVec2 vFrontDir = GetVectorByDirection( pUnit->GetFrontDir() );
		const CVec2 vRectTurn( vFrontDir.y, -vFrontDir.x );
		
		finishRect.InitRect( pUnit->GetCenter() + ( (pStats->aabb_ds[nAABBD].vCenter) ^ vRectTurn ), vFrontDir,
												 pStats->aabb_ds[nAABBD].vHalfSize.y, pStats->aabb_ds[nAABBD].vHalfSize.x );
		GetTilesCoveredByRect( finishRect, &killInfo.lockedTiles );

		killInfo.pUnit = pUnit;
		killInfo.timeToEndDieAnimation = timeOfVisDeath + pStats->animdescs[ANIMATION_DEATH_FATALITY][nFatality].nLength + 2 * SConsts::AI_SEGMENT_DURATION;
	}
	else
	{
		killInfo.actionTime = 0;
		killInfo.bAnimFinished = false;
		killInfo.bFatality = false;
		killInfo.bSentDead = false;
		killInfo.endFogTime = 0;
		killInfo.endSceneTime = 0;
		killInfo.pUnit = pUnit;
		GetTilesCoveredByRect( pUnit->GetUnitRect(), &killInfo.lockedTiles );
		
		const int nAnimation = GetAnimationFromAction( pUnit->GetDieAction() );
		if ( nAnimation >= 0 && pStats->animdescs.size() > nAnimation && !pStats->animdescs[nAnimation].empty() )
			killInfo.timeToEndDieAnimation = timeOfVisDeath + pStats->animdescs[nAnimation][0].nLength + 2 * SConsts::AI_SEGMENT_DURATION;
		else
			killInfo.timeToEndDieAnimation = timeOfVisDeath + pUnit->GetStats()->GetAnimTime( nAnimation ) + 2 * SConsts::AI_SEGMENT_DURATION;
	}

	killed.push_back( killInfo );
	units.DeleteUnitFromMap( pUnit );
}
void CGraveyard::PushToKilled( const SKilledUnit &killedUnit, CAIUnit *pUnit )
{
	killed.push_back( killedUnit );
	killed.back().pUnit = pUnit;
}
void CGraveyard::DelKilledUnitsFromRect( const SRect &rect, CAIUnit *pShotUnit )
{
	CTilesSet rectTiles;
	GetTilesCoveredByRect( rect, &rectTiles );

	theStaticMap.MemMode();
	theStaticMap.SetMode( ELM_STATIC );
	
	for ( std::list<SKilledUnit>::iterator iter = killed.begin(); iter != killed.end(); ++iter )
	{
		SKilledUnit unit = *iter;

		bool bIntersect = false;
		for ( CTilesSet::iterator rectTilesIter = rectTiles.begin(); !bIntersect && rectTilesIter != rectTiles.end(); ++rectTilesIter )
		{
			for ( CTilesSet::iterator unitTilesIter = unit.lockedTiles.begin(); !bIntersect && unitTilesIter != unit.lockedTiles.end(); ++unitTilesIter )
			{
				if ( *rectTilesIter == *unitTilesIter )
					bIntersect = true;
			}
		}

		if ( bIntersect )
		{
			if ( pShotUnit )
				pShotUnit->EnemyKilled( iter->pUnit );
			
			iter->bSentDead = true;
			iter->bDisappearUpdateSent = true;
			CDeadUnit *pDeadUnit = new CDeadUnit( iter->pUnit, 0, ACTION_NOTIFY_NONE, iter->pUnit->GetDBID(), false );
			updater.Update( ACTION_NOTIFY_DISSAPEAR_UNIT, pDeadUnit );
		}
	}

	theStaticMap.RestoreMode();
}
void CGraveyard::CheckSoonBeDead()
{
	std::list<CAIUnit*> deadObjs;
	for ( UpdateObjSet::iterator iter = soonBeDead.begin(); iter != soonBeDead.end(); ++iter )
	{
		CAIUnit *pUnit = iter->first;
		if ( pUnit->GetTimeOfDeath() <= curTime )
		{
			const float fDamage = iter->second;			
			const int nFatality = pUnit->ChooseFatality( fDamage );
			const bool bPutMud = !theStaticMap.IsBridge( pUnit->GetTile() );
			pUnit->CalcVisibility();
			
			updater.Update( ACTION_NOTIFY_DEAD_UNIT, new CDeadUnit( pUnit, curTime, pUnit->GetDieAction(), nFatality, bPutMud ) );

			AddKilledUnit( pUnit, curTime, nFatality );
			deadObjs.push_back( pUnit );
		}
	}

	for ( std::list<CAIUnit*>::iterator iter = deadObjs.begin(); iter != deadObjs.end(); ++iter )
		soonBeDead.erase( *iter );
}
void CGraveyard::Clear()
{
	killed.clear();
	soonBeDead.clear();
	bridgeSoldiersSet.clear();
	bridgeDeadSoldiers.clear();
}
void CGraveyard::AddToSoonBeDead( CAIUnit *pUnit, const float fDamage )
{
	soonBeDead[pUnit] = fDamage;
}
const int GetTileNum( const SVector &tile )
{
	return (tile.x << 12) | tile.y;
}
void CGraveyard::AddBridgeKilledSoldier( const SVector &tile, CAIUnit *pSoldier )
{
	CDeadUnit *pDeadUnit = new CDeadUnit( pSoldier, 0, ACTION_NOTIFY_NONE, pSoldier->GetDBID(), false );
	bridgeDeadSoldiers[GetTileNum( tile )].push_back( pDeadUnit );

	bridgeSoldiersSet.insert( pSoldier );
}
void CGraveyard::FreeBridgeTile( const SVector &tile )
{
	const int nTileNum = GetTileNum( tile );
	for ( std::list< CPtr<CDeadUnit> >::iterator iter = bridgeDeadSoldiers[nTileNum].begin(); iter != bridgeDeadSoldiers[nTileNum].end(); ++iter )
	{
		CDeadUnit *pDeadUnit = *iter;
		updater.Update( ACTION_NOTIFY_DISSAPEAR_UNIT, pDeadUnit );
		bridgeSoldiersSet.erase( pDeadUnit->GetDieObject() );
	}
	
	bridgeDeadSoldiers.erase( nTileNum );
}
void CGraveyard::UpdateFog4RemovedObject( class CExistingObject *pObj )
{
	for ( std::list<SKilledUnit>::iterator iter = killed.begin(); iter != killed.end(); ++iter )
	{
		if ( !iter->bFogDeleted )
		{
			CAIUnit *pUnit = iter->pUnit;
			theWarFog.RecalculateForRemovedObject( pUnit->GetID(), fabs( pUnit->GetCenter() - pObj->GetCenter() ), pObj );
		}
	}
		
}
void CGraveyard::UpdateFog4AddedObject( class CExistingObject *pObj )
{
	for ( std::list<SKilledUnit>::iterator iter = killed.begin(); iter != killed.end(); ++iter )
	{
		if ( !iter->bFogDeleted )
		{
			CAIUnit *pUnit = iter->pUnit;
			theWarFog.RecalculateForAddedObject( pUnit->GetID(), fabs( pUnit->GetCenter() - pObj->GetCenter() ), pObj );
		}
	}
}
CDeadUnit::CDeadUnit( CCommonUnit *_pDieObj, const NTimer::STime _dieTime, const EActionNotify _dieAction, bool _bPutMud )
: pDieObj( _pDieObj ), dieTime( _dieTime ), dieAction( _dieAction ), nFatality( -1 ), tileCenter( _pDieObj->GetTile() ), bPutMud( _bPutMud )
{
	SetUniqueId();	
}
CDeadUnit::CDeadUnit( CCommonUnit *_pDieObj, const NTimer::STime _dieTime, const EActionNotify _dieAction, const int _nFatality, bool _bPutMud )
: pDieObj( _pDieObj ), dieTime( _dieTime ), dieAction( _dieAction ), nFatality( _nFatality ), tileCenter( _pDieObj->GetTile() ), bPutMud( _bPutMud )
{
	SetUniqueId();
}
const bool CDeadUnit::IsVisible( const BYTE cParty ) const
{
	return theWarFog.IsTileVisible( tileCenter, cParty );
}
void CDeadUnit::GetTilesForVisibility( CTilesSet *pTiles ) const
{
	pTiles->clear();
	if ( theStaticMap.IsTileInside( tileCenter ) )
		pTiles->push_back( tileCenter );
}
bool CDeadUnit::ShouldSuspendAction( const EActionNotify &eAction ) const
{
	return
		!theDipl.IsEditorMode() &&
		(	eAction == ACTION_NOTIFY_DEAD_UNIT || 
			eAction == ACTION_NOTIFY_GET_DEAD_UNITS_UPDATE ||
			eAction == ACTION_NOTIFY_NEW_ST_OBJ );
}
void CDeadUnit::GetDyingInfo( SAINotifyAction *pDyingInfo )
{
	pDyingInfo->pObj = pDieObj;
	pDyingInfo->time = dieTime;
	pDyingInfo->typeID = dieAction;
	if ( dieAction != ACTION_NOTIFY_NONE )
	{
		if ( nFatality >= 0 )
			pDyingInfo->nParam = ( ANIMATION_DEATH_FATALITY << 16 ) | nFatality ;
		else
			pDyingInfo->nParam = ( ANIMATION_DEATH << 16 ) | ( -nFatality - 1 );
	}
	else
	{
		if ( nFatality == -1 )
			nFatality =	WORD( -1 );
		
		pDyingInfo->nParam = nFatality;
	}

	if ( bPutMud )
		pDyingInfo->nParam |= 0x80000000;
}
IUpdatableObj* CDeadUnit::GetDieObject() const 
{ 
	return pDieObj; 
}
