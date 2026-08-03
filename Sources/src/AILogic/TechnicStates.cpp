#include "StdAfx.h"

#include "TechnicsStates.h"
#include "AIUnit.h"
#include "Entrenchment.h"
#include "Updater.h"
#include "AIStaticMap.h"
#include "GroupLogic.h"
#include "UnitCreation.h"
#include "StaticObjects.h"
#include "EntrenchmentCreation.h"
#include "UnitsIterators2.h"
#include "Artillery.h"
extern CStaticObjects theStatObjs;
extern NTimer::STime curTime;
extern CUnitCreation theUnitCreation;
extern CGroupLogic theGroupLogic;
extern CStaticMap theStaticMap;
extern CUpdater updater;
IUnitState* CTankPitLeaveState::Instance( class CAIUnit *pTank )
{
	return new CTankPitLeaveState( pTank );
}
CTankPitLeaveState::CTankPitLeaveState( class CAIUnit  *pTank )
: eState( TLTPS_ESTIMATING ), pUnit( pTank ), timeStartLeave( curTime )
{
}
void CTankPitLeaveState::Segment()
{
	switch( eState )
	{
		
	case TLTPS_ESTIMATING:
		updater.Update( ACTION_NOTIFY_ENTRENCHMENT_STARTED, pUnit, 2 );
		if ( curTime - timeStartLeave > SConsts::AA_BEH_UPDATE_DURATION )
		{
			updater.Update( ACTION_NOTIFY_START_LEAVE_PIT, pUnit );
			eState = TLTPS_MOVING;
		}

		break;
	case TLTPS_MOVING:
		pUnit->SetOffTankPit();
		pUnit->SetCommandFinished();
		break;
	}
}
ETryStateInterruptResult CTankPitLeaveState::TryInterruptState( class CAICommand *pCommand )
{
	return TSIR_YES_WAIT;
}
IUnitState* CSoldierEntrenchSelfState::Instance( class CAIUnit * pUnit )
{
	return new CSoldierEntrenchSelfState( pUnit );
}
CSoldierEntrenchSelfState::CSoldierEntrenchSelfState( class CAIUnit * pUnit ) 
: pUnit( pUnit ), eState( ESHD_ESTIMATE )
{  
	if ( pUnit->IsInTankPit() || pUnit->GetStats()->IsArtillery() && pUnit->NeedDeinstall() && pUnit->IsUninstalled() )
		pUnit->SetCommandFinished();
	else
		pUnit->StopUnit();
}
bool CSoldierEntrenchSelfState::CheckInfantry( const CAIUnit * pUnit, const SRect &rect ) const
{
	const CFormation *pFormation = 0;
	if ( pUnit->GetStats()->IsArtillery() )
		pFormation = static_cast<const CArtillery*>(pUnit)->GetCrew();

	for ( CUnitsIter<0,1> iter( 0, ANY_PARTY, pUnit->GetCenter(), rect.width + rect.lengthAhead + rect.lengthBack );
				!iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pSoldier = *iter;
	
		if ( pSoldier->IsFree() && pSoldier->GetStats()->IsInfantry() && rect.IsPointInside( pSoldier->GetCenter() ) )
		{
			if ( pFormation != pSoldier->GetFormation() )
				return false;
		}
	}
	return true;
}
bool CSoldierEntrenchSelfState::CheckTrenches( const CAIUnit * pUnit, const SRect &rectToTest ) const
{
	return !CEntrenchmentCreation::SearchTrenches( pUnit->GetCenter(), rectToTest );
}
void CSoldierEntrenchSelfState::Segment()
{
	
	switch( eState )
	{
	case ESHD_ESTIMATE:
		{
			pUnit->UnlockTiles();
			const SRect unitRect = pUnit->GetUnitRect();
			CTilesSet unitTiles;
			GetTilesCoveredByRect( unitRect, &unitTiles );

			bool bCanDig = true;
			for ( CTilesSet::iterator i = unitTiles.begin(); i != unitTiles.end(); ++i )
			{
				if ( !theStaticMap.CanDigEntrenchment( i->x, i->y ) )
				{
					bCanDig = false;
					break;
				}
			}

			const SMechUnitRPGStats * pUnitStats = static_cast<const SMechUnitRPGStats *>( pUnit->GetStats() );
			CPtr<IObjectsDB> pIDB = theUnitCreation.GetObjectDB();
			float fResize;
			std::string szTankPit = theUnitCreation.GetRandomTankPit( pUnitStats->vAABBHalfSize, bCanDig, &fResize );
			CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( szTankPit.c_str() );
			nDBIndex = pIDB->GetIndex( szTankPit.c_str() );
			pStats = static_cast<const SMechUnitRPGStats*>( pIDB->GetRPGStats( pDesc ) );

			const CVec2 vRelativePosUnit(  pUnitStats->vAABBHalfSize.y + pUnitStats->vAABBCenter.y, pUnitStats->vAABBCenter.x );
			const CVec2 vRelativePosPit( fResize * ( pStats->vAABBHalfSize.y + pStats->vAABBCenter.y), fResize * (pStats->vAABBCenter.x) );
			vTankPitCenter = pUnit->GetCenter() + ( ( vRelativePosUnit - vRelativePosPit )^pUnit->GetDirVector() );

			vHalfSize.x = pUnitStats->vAABBHalfSize.x ;
			vHalfSize.y = pUnitStats->vAABBHalfSize.y ;

			SRect rect;
			SRect rectToCheck;
			const CVec2 vFrontDir( GetVectorByDirection(pUnit->GetFrontDir()) );
			rect.InitRect( pUnit->GetCenter(), vFrontDir, vHalfSize.y, vHalfSize.x );
			rectToCheck.InitRect( pUnit->GetCenter(), vFrontDir, vHalfSize.y + SConsts::TILE_SIZE, vHalfSize.x + SConsts::TILE_SIZE );
			
			GetTilesNextToRect( rect, &tiles, 65535/2 + pUnit->GetFrontDir() );
			
			bool bCanAdd = true;
			for ( CTilesSet::iterator i = tiles.begin(); i != tiles.end(); ++i )
			{
				if ( theStaticMap.IsLocked( (*i), AI_CLASS_ANY ) )
				{
					bCanAdd = false;
					break;
				}
			}
			
			if ( bCanAdd ) // проверить, нет ли под TankPit окопов
				bCanAdd = CheckTrenches( pUnit, rectToCheck );
			if ( bCanAdd )
				bCanAdd = CheckInfantry( pUnit, rectToCheck ); 

			if ( bCanAdd )
			{
				theStaticMap.UpdateMaxesByTiles( tiles, AI_CLASS_ANY, true );
				eState = ESHD_START_BUILD;
				updater.Update( ACTION_NOTIFY_ENTRENCHMENT_STARTED, pUnit );
			}
			else
			{
				pUnit->SendAcknowledgement( ACK_CANNOT_START_BUILD );
				pUnit->SetCommandFinished();
			}
			timeStartBuild = curTime;
		}

		break;
	case ESHD_START_BUILD:
		if ( curTime - timeStartBuild > vHalfSize.x * vHalfSize.y * SConsts::ENTRENCH_SELF_TIME / sqr(static_cast<int>(SConsts::TILE_SIZE) ) )
		{
			updater.Update( ACTION_NOTIFY_START_BUILD_PIT, pUnit );
			eState = ESHD_BUILD_PIT;
		}

		break;
	case ESHD_BUILD_PIT:
		{
			theStaticMap.UpdateMaxesByTiles( tiles, AI_CLASS_ANY, false );

			pUnit->SetInTankPit( theStatObjs.AddNewTankPit( pStats, vTankPitCenter, pUnit->GetFrontDir(), 0, nDBIndex, vHalfSize, tiles, pUnit ) );
			pUnit->SendAcknowledgement( ACK_BUILDING_FINISHED, true );
		
			pUnit->SetCommandFinished();
			updater.Update( ACTION_NOTIFY_ENTRENCHMENT_STARTED, pUnit, 1 );
		}

		break;
	}
}
ETryStateInterruptResult CSoldierEntrenchSelfState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pUnit->IsValid() || !pUnit->IsAlive() )
	{
		if ( eState != ESHD_ESTIMATE )		//  UNLOCK LoCKED TILES
		{
			theStaticMap.UpdateMaxesByTiles( tiles, AI_CLASS_ANY, false );
		}
		return TSIR_YES_IMMIDIATELY;
	}
	return	TSIR_YES_WAIT;
}
