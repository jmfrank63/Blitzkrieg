#include "StdAfx.h"

#include <float.h>

#include "FormationStates.h"
#include "Formation.h"
#include "Commands.h"
#include "Soldier.h"
#include "Guns.h"
#include "GroupLogic.h"
#include "Entrenchment.h"
#include "Updater.h"
#include "Diplomacy.h"
#include "Technics.h"
#include "TransportStates.h"
#include "EntrenchmentCreation.h"
#include "..\Common\Actions.h"
#include "AIStaticMap.h"
#include "StaticObjects.h"
#include "Building.h"
#include "Artillery.h"
#include "Turret.h"
#include "Soldier.h"
#include "PathFinder.h"
#include "UnitsIterators2.h"
#include "UnitCreation.h"
#include "ArtilleryPaths.h"
#include "ObstacleInternal.h"
#include "Bridge.h"
#include "General.h"
#include "Statistics.h"

#include "TimeCounter.h"
extern CSupremeBeing theSupremeBeing;
extern CUnitCreation theUnitCreation;
extern CStaticMap theStaticMap;
extern CStaticObjects theStatObjs;
extern NTimer::STime curTime;
extern CGroupLogic theGroupLogic;
extern CUpdater updater;
extern CDiplomacy theDipl;
extern CUnits units;
extern CStatistics theStatistics;
extern CTimeCounter timeCounter;
float Heal( const float fMaxHP, const float fCurHP, const float fRepCost,
																			 float *pfWorkAccumulator, float *pfWorkLeft, class CAITransportUnit *pHomeTransport )
{
	if (  fMaxHP !=  fCurHP )
	{
		const float hpRepeared = *pfWorkAccumulator / fRepCost;
		const float dh =  Min( fMaxHP - fCurHP, hpRepeared );
		{
			*pfWorkAccumulator -= dh * fRepCost;
			*pfWorkLeft -= dh * fRepCost ;
			pHomeTransport->DecResursUnitsLeft( dh * fRepCost );
		}
		return fCurHP + dh;
	}
	return fCurHP;
}
IUnitState* CFormationPlaceMine::Instance( CFormation *pFormation, const CVec2 &point, const enum SMineRPGStats::EType eType )
{
	return new CFormationPlaceMine( pFormation, point, eType );
}
CFormationPlaceMine::CFormationPlaceMine( CFormation *_pFormation, const CVec2 &_point, const enum SMineRPGStats::EType _eType )
: pFormation( _pFormation ), point( _point ), eType( _eType ), eState( EPM_WAIT_FOR_HOMETRANSPORT )
{
}
void CFormationPlaceMine::SetHomeTransport( class CAITransportUnit *pTransport )
{
	NI_ASSERT_T( eState == EPM_WAIT_FOR_HOMETRANSPORT, "wrong state" );
	eState = EPM_START;
	pHomeTransport = pTransport;
}
void CFormationPlaceMine::Segment()
{
	if ( !IsValidObj( pHomeTransport ) )
		TryInterruptState( 0 );

	switch ( eState )
	{
		case EPM_WAIT_FOR_HOMETRANSPORT:
			if ( pFormation->GetNextCommand() )
				pFormation->SetCommandFinished();

			break;
		case EPM_START:
			if ( CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( point, VNULL2, pFormation, true ) )
			{
				pFormation->SendAlongPath( pStaticPath, pFormation->GetGroupShift() );
				eState = EPM_MOVE;
			}
			else
			{
				pFormation->SendAcknowledgement( ACK_NEGATIVE );
				pFormation->SetCommandFinished();
			}
	
			break;
		case EPM_MOVE:
			{
				const float fDist = fabs2( pFormation->GetCenter() - point - pFormation->GetGroupShift() );

				if ( pFormation->IsIdle() )
				{
					pFormation->StopFormationCenter();
					if ( pFormation->IsIdle() )
					{
						if ( pFormation->Size() == 1 && pFormation->GetGroupShift() == VNULL2 )
						{
							if ( SConsts::MINE_RU_PRICE[eType] <= pHomeTransport->GetResursUnitsLeft() )
							{
								pHomeTransport->DecResursUnitsLeft( SConsts::MINE_RU_PRICE[eType] );
								theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_PLACEMINE_NOW, point.x, point.y, eType ), (*pFormation)[0], false );
							}
						}
						else
						{
							for ( int i = 0; i < pFormation->Size(); ++i )
							{
								if ( SConsts::MINE_RU_PRICE[eType] <= pHomeTransport->GetResursUnitsLeft() )
								{
									pHomeTransport->DecResursUnitsLeft( SConsts::MINE_RU_PRICE[eType] );
									const CVec2 point = (*pFormation)[i]->GetCenter() + (*pFormation)[i]->GetDirVector() * SConsts::TILE_SIZE;
									theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_PLACEMINE_NOW, point.x, point.y, eType ), (*pFormation)[i], false );
								}
							}
						}
					}

					pFormation->SetToWaitingState();
					eState = EPM_WAITING;
				}
			}

			break;
		case EPM_WAITING:
			if ( pFormation->IsEveryUnitResting() )
				pFormation->SetCommandFinished();

			pFormation->UnsetFromWaitingState();

			break;
	}
}
ETryStateInterruptResult CFormationPlaceMine::TryInterruptState( class CAICommand *pCommand )
{
	pFormation->UnsetFromWaitingState();
	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
IUnitState* CFormationClearMine::Instance( CFormation *pFormation, const CVec2 &point )
{
	return new CFormationClearMine( pFormation, point );
}
CFormationClearMine::CFormationClearMine( CFormation *_pFormation, const CVec2 &_point )
: pFormation( _pFormation ), point( _point ), eState( EPM_START )
{
}
void CFormationClearMine::Segment()
{
	switch ( eState )
	{
		case EPM_START:
			if ( CPtr<IStaticPath> pStaticPath = pFormation->GetCurCmd()->CreateStaticPath( pFormation ) )
			{
				pFormation->SendAlongPath( pStaticPath, pFormation->GetGroupShift() );
				eState = EPM_MOVE;
			}
			else
			{
				pFormation->SendAcknowledgement( ACK_NEGATIVE );
				pFormation->SetCommandFinished();
			}

			break;
		case EPM_MOVE:
			if ( pFormation->IsIdle() )
			{
				pFormation->StopFormationCenter();
				for ( int i = 0; i < pFormation->Size(); ++i )
					theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_CLEARMINE_RADIUS, (*pFormation)[i]->GetCenter().x, (*pFormation)[i]->GetCenter().y ), (*pFormation)[i], false );

				pFormation->SetToWaitingState();
				eState = EPM_WAIT;
			}

			break;

		case EPM_WAIT:
			if ( pFormation->IsEveryUnitResting() )
				pFormation->SetCommandFinished();

			pFormation->UnsetFromWaitingState();

			break;
	}
}
ETryStateInterruptResult CFormationClearMine::TryInterruptState( class CAICommand *pCommand )
{
	pFormation->UnsetFromWaitingState();
	pFormation->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
bool CFormationRepairUnitState::CFindFirstStorageToRepearPredicate::AddStorage( class CBuildingStorage * pStorage, const float fPathLenght )
{
	const SBuildingRPGStats * pStats = static_cast<const SBuildingRPGStats *>(pStorage->GetStats());
	if ( SBuildingRPGStats::TYPE_TEMP_RU_STORAGE == pStats->eType &&
			 pStorage->GetHitPoints() != pStats->fMaxHP ) // нужна починка
	{
		if ( pStats->fRepairCost <= fMaxRu )						// можем починить хоть немного
			bHasStor = true;
		else
			bNotEnoughRu = true;
	}
	return bHasStor;
}
IUnitState* CFormationRepairUnitState::Instance( class CFormation *_pUnit, CAIUnit *_pPreferredUnit )
{
	return new CFormationRepairUnitState( _pUnit, _pPreferredUnit );
}
CFormationRepairUnitState::CFormationRepairUnitState( class CFormation *pUnit, CAIUnit *_pPreferredUnit )
: pUnit( pUnit ), 
	lastRepearTime( curTime ),
	vPointInQuestion( VNULL2 ),
	fRepCost( 0 ),
	CFormationServeUnitState( _pPreferredUnit ), bNearTruck( true )
{
}
bool CFormationRepairUnitState::CheckUnit( CAIUnit *pU, CFormationServeUnitState::CFindUnitPredicate * pPred, const float fResurs, const CVec2 &vCenter )
{
	if ( pU && pU->IsValid() && pU->IsAlive() )
	{
		const EUnitRPGType type = pU->GetStats()->type;
		const SHPObjectRPGStats * pStats = pU->GetStats();
		
		bool bCannotReachUnit = false; // до юнита нельзя дойти
		float fTrackHP = 0;

		if ( ::IsArmor( type ) || ::IsSPG( type ) || ::IsTrain( type ) )
		{
			/*CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( pU->GetCenter(), VNULL2, pLoaderSquad, true );
			SRect unitRect = pU->GetUnitRect();
			unitRect.Compress( 1.2f );
			if ( !pStaticPath || !unitRect.IsPointInside( pStaticPath->GetFinishPoint() ) )
			{
				bCannotReachUnit = true;
			}*/
			if ( !bCannotReachUnit )
			{
				CTank * pTank = static_cast<CTank*>( pU );
				if ( pTank->IsTrackDamaged() )
				{
					if ( pStats->fRepairCost * SConsts::TANK_TRACK_HIT_POINTS <= fResurs )
						fTrackHP = SConsts::TANK_TRACK_HIT_POINTS;
					else
					{
						pPred->SetNotEnoughRu();
						return false;
					}
				}
			}
		}

		if ( (::IsArmor(type) || ::IsArtillery(type) || ::IsTrain(type) || ::IsTransport(type) || ::IsSPG(type) ) && 
				pU->IsIdle() )
		{
			/*CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( pU->GetCenter(), VNULL2, pLoaderSquad, true );
			SRect unitRect = pU->GetUnitRect();
			unitRect.Compress( 1.2f );
			if ( pStaticPath && unitRect.IsPointInside( pStaticPath->GetFinishPoint() ) )*/

			{
				const float curHP = pU->GetStats()->fMaxHP + fTrackHP - pU->GetHitPoints();
				if ( curHP >  0.0f )
				{
					if ( pStats->fRepairCost > fResurs )
						pPred->SetNotEnoughRu();
					else
					{
						/*if ( pPred->SetUnit( pU, curHP, pStaticPath->GetLength() * SAIConsts::TILE_SIZE ) )
							return;*/
						if ( pPred->SetUnit( pU, curHP, fabs( pU->GetCenter() - vCenter )) )
							return true;
					}
				}
			}
		}
	}
	return false;
}
void CFormationRepairUnitState::FindUnitToServe( const CVec2 &vCenter, 
																									int nPlayer, 
																									const float fResurs, 
																									CCommonUnit * pLoaderSquad, 
																									CFormationServeUnitState::CFindUnitPredicate * pPred,
																									CAIUnit *_pPreferredUnit )
{
	if ( CheckUnit( _pPreferredUnit, pPred, fResurs, vCenter ) || pPred->HasUnit() )
		return;

	CUnitsIter<0,2> iter( theDipl.GetNParty(nPlayer), EDI_FRIEND, vCenter, SConsts::RESUPPLY_RADIUS );

	while ( !iter.IsFinished() )
	{
		if ( CheckUnit( (*iter), pPred, fResurs, vCenter ) )
			return;
		
		iter.Iterate();
	}
}
void CFormationRepairUnitState::Segment()
{
	if ( EFRUS_WAIT_FOR_HOME_TRANSPORT != eState )
	{
		if ( !IsValidObj( pHomeTransport ) )
			TryInterruptState( 0 );
		if ( ( !IsValidObj( pUnitInQuiestion ) || 	//реакция на смерть юнита
				pUnitInQuiestion->GetCenter() != vPointInQuestion ) ) // да пошел! еще бегать за ним.
		{
			eState = EFRUS_FIND_UNIT_TO_SERVE;
		}
		else if ( pUnitInQuiestion->GetHitPoints() == pUnitInQuiestion->GetStats()->fMaxHP )
		{
			const EUnitRPGType type = pUnitInQuiestion->GetStats()->type;
			if ( !::IsArmor( type ) && !::IsSPG( type ) && !static_cast_ptr<CTank*>( pUnitInQuiestion )->IsTrackDamaged() )
			{
				eState = EFRUS_FIND_UNIT_TO_SERVE;
			}
		}
	}

	switch ( eState )
	{
	case EFRUS_WAIT_FOR_HOME_TRANSPORT:
		if ( pUnit->GetNextCommand() )
			pUnit->SetCommandFinished();		

		break;
	case EFRUS_WAIT_FOR_UNIT_TO_SERVE:
		if ( !bNearTruck )
		{
			Interrupt();
			break;
		}
		if ( curTime - lastRepearTime > 3000 )
			eState = EFRUS_FIND_UNIT_TO_SERVE;

		break;
	case EFRUS_FIND_UNIT_TO_SERVE:
		{
			CFormationServeUnitState::CFindBestUnitPredicate pred;
			FindUnitToServe( pHomeTransport->GetCenter(), pHomeTransport->GetPlayer(), fWorkLeft, pUnit, &pred, pPreferredUnit );
			if ( pred.HasUnit() )
			{
				pUnitInQuiestion = pred.GetUnit();
				vPointInQuestion = pUnitInQuiestion->GetCenter();
				EUnitRPGType type = pUnitInQuiestion->GetStats()->type;
				if (::IsArmor(type) || ::IsSPG(type) || ::IsTrain(type) )
					pTank = static_cast_ptr< CTank* >( pUnitInQuiestion );
				eState = EFRUS_START_APPROACH;
				fRepCost = pUnitInQuiestion->GetStats()->fRepairCost;
				break;
			}
			else if ( pred.IsNotEnoughRu() || !bNearTruck ||
								fWorkLeft != Min( SConsts::ENGINEER_RU_CARRY_WEIGHT, pHomeTransport->GetResursUnitsLeft() ) )
				Interrupt();
			else
			{
				lastRepearTime = curTime;
				eState = EFRUS_WAIT_FOR_UNIT_TO_SERVE;
			}
		}

		break;
	case EFRUS_START_APPROACH:
		{
			CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( pUnitInQuiestion->GetCenter(), VNULL2, pUnit, true );
			if( pStaticPath )
			{
				SRect unitRect;
				unitRect = pUnitInQuiestion->GetUnitRect();
				unitRect.Compress( 1.2f );
				const CVec2 finishPoint ( pStaticPath->GetFinishPoint().x, pStaticPath->GetFinishPoint().y );
				{
					bNearTruck = false;
					pUnit->SendAlongPath( pStaticPath, VNULL2 );
					eState = EFRUS_APPROACHING;
				}
			}
			eState = EFRUS_APPROACHING;
		}

		break;
	case EFRUS_APPROACHING:
		if ( pUnit->IsIdle() )
			eState = EFRUS_START_SERVICE;

		break;
	case EFRUS_START_SERVICE:
		for ( int i = 0; i < pUnit->Size(); ++i )
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_USE, (float)ACTION_NOTIFY_USE_UP ), (*pUnit)[i] );

		eState = EFRUS_SERVICING;
		lastRepearTime = curTime;
		fWorkAccumulator = 0;

		break;
	case EFRUS_SERVICING:
		if ( curTime - lastRepearTime > SConsts::TIME_QUANT )
		{
			fWorkAccumulator += pUnit->Size() * 
													SConsts::ENGINEER_REPEAR_HP_PER_QUANT *
													( curTime - lastRepearTime ) / SConsts::TIME_QUANT;

			fWorkAccumulator = Min( fWorkAccumulator, fWorkLeft );

			if ( pTank && pTank->IsTrackDamaged() && // если повреждена и достаточно ресурсов
					fWorkLeft >= fRepCost * SConsts::TANK_TRACK_HIT_POINTS ) 
			{
				if ( fRepCost * SConsts::TANK_TRACK_HIT_POINTS <= fWorkAccumulator )
				{
					fWorkAccumulator -= fRepCost * SConsts::TANK_TRACK_HIT_POINTS;
					fWorkLeft -= fRepCost * SConsts::TANK_TRACK_HIT_POINTS ;
					pHomeTransport->DecResursUnitsLeft( fRepCost * SConsts::TANK_TRACK_HIT_POINTS );
					pTank->RepairTrack();
				}
			}
			else
			{
				const float maxHP = pUnitInQuiestion->GetStats()->fMaxHP;
				const float curHP = pUnitInQuiestion->GetHitPoints();
				const float &fRepCost = pUnitInQuiestion->GetStats()->fRepairCost;
			
				const float fNewHP = Heal( maxHP, curHP, fRepCost, &fWorkAccumulator, &fWorkLeft, pHomeTransport );
				pUnitInQuiestion->IncreaseHitPoints( fNewHP - curHP );
				if ( pUnitInQuiestion->GetStats()->fMaxHP == pUnitInQuiestion->GetHitPoints() )//починили
				{
					theSupremeBeing.UnitAskedForResupply( pUnitInQuiestion, ERT_REPAIR, false );
					eState = EFRUS_FIND_UNIT_TO_SERVE;
				}
				else if ( fWorkLeft + fWorkAccumulator < fRepCost )
					eState = EFRUS_FIND_UNIT_TO_SERVE;
			}

			lastRepearTime = curTime;
		}

		break;
	}
}
void CFormationRepairUnitState::Interrupt()
{
	if ( !pUnit->IsIdle() )
		pUnit->StopUnit();

	pUnit->SetCommandFinished();
}
ETryStateInterruptResult CFormationRepairUnitState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand || pCommand->ToUnitCmd().cmdType == ACTION_MOVE_CATCH_TRANSPORT )
	{
		Interrupt();
		return TSIR_YES_IMMIDIATELY;
	}
	else if ( pCommand->ToUnitCmd().cmdType == ACTION_MOVE_SET_HOME_TRANSPORT ||
						pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_LOAD )
	{
		return TSIR_YES_WAIT;
	}
	return TSIR_NO_COMMAND_INCOMPATIBLE;
}

void CFormationServeUnitState::SetHomeTransport( class CAITransportUnit *pTransport )
{
	NI_ASSERT_T( eState == EFRUS_WAIT_FOR_HOME_TRANSPORT, "wrong state" );
	eState = EFRUS_FIND_UNIT_TO_SERVE;
	pHomeTransport = pTransport;
	fWorkLeft = Min( SConsts::ENGINEER_RU_CARRY_WEIGHT, pTransport->GetResursUnitsLeft() );
}
IUnitState* CFormationResupplyUnitState::Instance( class CFormation *_pUnit, class CAIUnit *_pPreferredUnit )
{
	return new CFormationResupplyUnitState( _pUnit, _pPreferredUnit );
}
CFormationResupplyUnitState::CFormationResupplyUnitState( class CFormation *pUnit, class CAIUnit *_pPreferredUnit )
: pUnit( pUnit ), vPointInQuestion( VNULL2 ), CFormationServeUnitState( _pPreferredUnit ), bNearTruck ( true )
{
}
void CFormationResupplyUnitState::Segment()
{
	if( EFRUS_WAIT_FOR_HOME_TRANSPORT != eState )
	{
		if ( !IsValidObj( pUnitInQuiestion ) ||//реакция на смерть юнита
					vPointInQuestion != pUnitInQuiestion->GetCenter() // да пошел! еще бегать за ним.
				) 
		{
			eState = EFRUS_FIND_UNIT_TO_SERVE;
		}
		if ( !IsValidObj( pHomeTransport ) )
			TryInterruptState( 0 );
	}
	
	switch ( eState )
	{
	case EFRUS_WAIT_FOR_HOME_TRANSPORT:
		if ( pUnit->GetNextCommand() )
			pUnit->SetCommandFinished();		

		break;
	case EFRUS_WAIT_FOR_UNIT_TO_SERVE:
		if ( !bNearTruck )		// go back to truck
		{
			Interrupt();
			break;
		}
		if ( curTime - lastResupplyTime > 3000 )
			eState = EFRUS_FIND_UNIT_TO_SERVE;

		break;
	case EFRUS_FIND_UNIT_TO_SERVE:
		{
			CFormationServeUnitState::CFindBestUnitPredicate pred;
			FindUnitToServe( pHomeTransport->GetCenter(), pHomeTransport->GetPlayer(), fWorkLeft, pUnit, &pred, pPreferredUnit );
			if ( pred.HasUnit() )
			{
				pUnitInQuiestion = pred.GetUnit();
				vPointInQuestion = pUnitInQuiestion->GetCenter();
				EUnitRPGType type = pUnitInQuiestion->GetStats()->type;
				CPtr<CSoldier> pSold;
				if (	::IsInfantry( pUnitInQuiestion->GetStats()->type ) &&
							( pSold = static_cast_ptr<CSoldier*>(pUnitInQuiestion) ) &&
							( pSquadInQuestion = pSold->GetFormation()) )
				{
					pUnitInQuiestion = (*pSquadInQuestion)[0];
					vPointInQuestion = pUnitInQuiestion->GetCenter();
					iCurUnitInFormation = 0;
				}
				eState = EFRUS_START_APPROACH;
			}
			else if ( pred.IsNotEnoughRu() || !bNearTruck ||
								fWorkLeft != Min( SConsts::ENGINEER_RU_CARRY_WEIGHT, pHomeTransport->GetResursUnitsLeft() ) )
			{
				Interrupt();
			}
			else
			{
				lastResupplyTime = curTime;
				eState = EFRUS_WAIT_FOR_UNIT_TO_SERVE;
			}
		}

		break;
	case EFRUS_START_APPROACH:
		{
			CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( pUnitInQuiestion->GetCenter(), VNULL2, pUnit, true );
			if ( pStaticPath )
			{
				if ( IsUnitNearPoint( pUnitInQuiestion, pStaticPath->GetFinishPoint(), SConsts::TILE_SIZE * 5 ) )
				{
					bNearTruck = false;
					pUnit->SendAlongPath( pStaticPath, VNULL2 );
					eState = EFRUS_APPROACHING;
				}
			}
			eState = EFRUS_APPROACHING;
		}

		break;
	case EFRUS_APPROACHING:
		if ( pUnit->IsIdle() )
			eState = EFRUS_START_SERVICE;

		break;
	case EFRUS_START_SERVICE:
		for ( int i = 0; i < pUnit->Size(); ++i )
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_USE, (float)ACTION_NOTIFY_USE_UP ), (*pUnit)[i] );

		bSayAck = false;
		for ( int i = 0; i < pUnitInQuiestion->GetNCommonGuns(); ++ i )
		{
			if ( pUnitInQuiestion->GetNAmmo( i ) == 0 )
			{
				bSayAck = true;
				break;
			}
		}
		eState = EFRUS_SERVICING;
		fWorkAccumulator = 0;
		lastResupplyTime = curTime ;

		break;
	case EFRUS_SERVICING:
		if ( curTime - lastResupplyTime > SConsts::TIME_QUANT )
		{
			fWorkAccumulator += pUnit->Size() * 
													SConsts::ENGINEER_RESUPPLY_PER_QUANT * 
													(curTime - lastResupplyTime) / SConsts::TIME_QUANT;
			fWorkAccumulator = Min( fWorkAccumulator, fWorkLeft );
			lastResupplyTime = curTime ;
			float fWorkPerformed = 0;
			int nGunsResupplied = 0;
			float min1AmmoCost = 0;

			for ( int i = 0; i < pUnitInQuiestion->GetNCommonGuns(); ++i )
			{
				const SBaseGunRPGStats &rStats = pUnitInQuiestion->GetCommonGunStats( i );
				int iAmmoNeeded = rStats.nAmmo - pUnitInQuiestion->GetNAmmo( i );
				if ( 0 != iAmmoNeeded )
				{//need resupply
					min1AmmoCost = ( min1AmmoCost == 0 || rStats.fReloadCost < min1AmmoCost ) ? rStats.fReloadCost : min1AmmoCost;
					int nAmmoToAdd = Min( iAmmoNeeded, int( fWorkAccumulator / rStats.fReloadCost ) );
					float fWorkNeeded = Min( iAmmoNeeded * rStats.fReloadCost, nAmmoToAdd * rStats.fReloadCost );
					if ( fWorkNeeded == 0 )
					{
						continue;
					}
					pUnitInQuiestion->ChangeAmmo( i, nAmmoToAdd );
					fWorkPerformed += fWorkNeeded;
					fWorkAccumulator -= fWorkNeeded;
				}
				else
					++nGunsResupplied;
			}
			pHomeTransport->DecResursUnitsLeft( fWorkPerformed );
			fWorkLeft -= fWorkPerformed;

			if ( nGunsResupplied == pUnitInQuiestion->GetNCommonGuns() || // все перезарядили
					 min1AmmoCost > fWorkLeft )// в транспорте больше не осталось ресурсов
			{
				if ( nGunsResupplied == pUnitInQuiestion->GetNCommonGuns() )
				{
					theSupremeBeing.UnitAskedForResupply( pUnitInQuiestion, ERT_RESUPPLY, false );
					if ( bSayAck  )
						pUnitInQuiestion->SendAcknowledgement( ACK_GETTING_AMMO, true );
				}

				if ( pSquadInQuestion && ++iCurUnitInFormation < pSquadInQuestion->Size() )
				{
					pUnitInQuiestion = (*pSquadInQuestion)[iCurUnitInFormation];
					vPointInQuestion = pUnitInQuiestion->GetCenter();
				}
				else
					eState = EFRUS_FIND_UNIT_TO_SERVE;
			}
		}

		break;
	}
}
bool CFormationResupplyUnitState::CheckUnit( CAIUnit *pU, CFormationServeUnitState::CFindUnitPredicate * pPred, const float fResurs, const CVec2 &vCenter )
{
	if ( pU && pU->IsValid() && pU->IsAlive() &&
			/*pU->GetPlayer() == pU->GetPlayer() &&*/ 
			pU->IsIdle() && pU->IsFree() )
	{
			const EUnitRPGType type = pU->GetStats()->type;

		{
			float fWorkPresent = 0; //в рублях стоимость снарядов у юнита
			float fWorkTotal = 0;		// в рублях стоимость максимального боезапаса
			float fMinReloadCost = 0;
			for ( int i = 0; i < pU->GetNCommonGuns(); ++i )
			{
				const SBaseGunRPGStats& rStats = pU->GetCommonGunStats( i );
				int iAmmoPresent = pU->GetNAmmo( i );
				fWorkPresent += iAmmoPresent * rStats.fReloadCost;
				fWorkTotal += rStats.nAmmo * rStats.fReloadCost;
				if ( iAmmoPresent != rStats.nAmmo )
					fMinReloadCost = ( (fMinReloadCost == 0 || fMinReloadCost > rStats.fReloadCost) ) ? rStats.fReloadCost : fMinReloadCost;
			}
			const float curAmmo = fWorkTotal - fWorkPresent;
			if (  curAmmo > 0.0f )  // юниту нужны патроны
			{
				if ( fMinReloadCost != 0  && fMinReloadCost > fResurs ) // ne достаточно ресурсов, чтобы дать хотя-бы 1 патрон
					pPred->SetNotEnoughRu();
				/*else if ( pPred->SetUnit( pU, curAmmo, pStaticPath->GetLength()) )
					return;*/
				else if ( pPred->SetUnit( pU, curAmmo, fabs( pU->GetCenter() - vCenter ) ) )
					return true;
			}
		}			
	}
	return false;
}
void CFormationResupplyUnitState::FindUnitToServe( const CVec2 &vCenter, 
																												int nPlayer, 
																												const float fResurs, 
																												CCommonUnit * pLoaderSquad,  
																												CFormationServeUnitState::CFindUnitPredicate * pPred, 
																												CAIUnit *_pPreferredUnit )
{
	if ( CheckUnit( _pPreferredUnit, pPred, fResurs, vCenter ) || pPred->HasUnit() )
		return;

	CUnitsIter<0,2> iter( theDipl.GetNParty(nPlayer), EDI_FRIEND, vCenter, SConsts::RESUPPLY_RADIUS );

	while ( !iter.IsFinished() )
	{
		if ( CheckUnit( (*iter), pPred, fResurs, vCenter ) )
			return;
		iter.Iterate();
	}
}
void CFormationResupplyUnitState::Interrupt()
{
	if ( !pUnit->IsIdle() )
		pUnit->StopUnit();
	for ( int i = 0; i < pUnit->Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP_THIS_ACTION), (*pUnit)[i], false );

	pUnit->SetCommandFinished();
}
ETryStateInterruptResult CFormationResupplyUnitState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand || pCommand->ToUnitCmd().cmdType == ACTION_MOVE_CATCH_TRANSPORT )
	{
		Interrupt();
		return TSIR_YES_IMMIDIATELY;
	}
	else if ( 
				pCommand->ToUnitCmd().cmdType == ACTION_MOVE_SET_HOME_TRANSPORT ||
				pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_LOAD )
	{
		Interrupt();
		return TSIR_YES_IMMIDIATELY;
	}
	return TSIR_NO_COMMAND_INCOMPATIBLE;
}

IUnitState* CFormationLoadRuState::Instance( class CFormation *pUnit, class CBuildingStorage *pStorage)
{
	return new CFormationLoadRuState( pUnit, pStorage );
}
CFormationLoadRuState::CFormationLoadRuState( class CFormation *pUnit, class CBuildingStorage *pStorage)
: pUnit( pUnit ), pStorage( pStorage ), nEntrance( -1 ), CFormationServeUnitState( 0 )
{
}
void CFormationLoadRuState::Segment()
{
	if ( !IsValidObj( pStorage ) )
	{
		Interrupt();
		return;
	}

	switch ( eState )
	{
	case EFRUS_WAIT_FOR_HOME_TRANSPORT:
		if ( pUnit->GetNextCommand() )
			pUnit->SetCommandFinished();		

		break;
	case EFRUS_FIND_UNIT_TO_SERVE:
		eState = EFRUS_START_APPROACH;
		break;
	case EFRUS_START_APPROACH:
		{
			nEntrance = CFormationRepairBuildingState::SendToNearestEntrance( pUnit, pStorage );
			if ( -1 != nEntrance )
				eState = EFRUS_APPROACHING;
			else
				Interrupt();
		}

		break;
	case EFRUS_APPROACHING:
		if ( pUnit->IsIdle() )
			eState = EFRUS_START_SERVICE;

		break;
	case EFRUS_START_SERVICE:
		for ( int i = 0; i < pUnit->Size(); ++i )
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_USE, (float)ACTION_NOTIFY_USE_UP ), (*pUnit)[i] );

		eState = EFRUS_SERVICING;
		fWorkAccumulator =0;
		lastResupplyTime = curTime ;

		break;
	case EFRUS_SERVICING:
		if ( curTime - lastResupplyTime > SConsts::TIME_QUANT )
		{
			fWorkAccumulator += pUnit->Size() * 
													SConsts::ENGINEER_LOAD_RU_PER_QUANT * 
													(curTime - lastResupplyTime)/SConsts::TIME_QUANT;
			lastResupplyTime = curTime ;
			
			if ( fWorkAccumulator > SConsts::ENGINEER_RU_CARRY_WEIGHT )
				Interrupt();
		}
		break;
	}
}
void CFormationLoadRuState::Interrupt()
{
	pUnit->SetCommandFinished();
}
ETryStateInterruptResult CFormationLoadRuState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand )
	{
		Interrupt();
		return TSIR_YES_IMMIDIATELY;
	}
	else if ( pCommand->ToUnitCmd().cmdType == ACTION_MOVE_CATCH_TRANSPORT )
	{
		pCommand->ToUnitCmd().fNumber = fWorkAccumulator/pUnit->Size();
		Interrupt();
		return TSIR_YES_IMMIDIATELY;
	}
	else if ( pCommand->ToUnitCmd().cmdType == ACTION_MOVE_SET_HOME_TRANSPORT ||
						pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_LOAD )
	{
		return TSIR_YES_WAIT;
	}
	return TSIR_NO_COMMAND_INCOMPATIBLE;

}
IUnitState* CFormationPlaceAntitankState::Instance( class CFormation *_pUnit, const CVec2 &vDesiredPoint )
{
	return new CFormationPlaceAntitankState( _pUnit, vDesiredPoint );
}
CFormationPlaceAntitankState::CFormationPlaceAntitankState( class CFormation *_pUnit, const CVec2 &vDesiredPoint )
: pUnit(_pUnit), eState( FPAS_WAIT_FOR_HOMETRANSPORT), vDesiredPoint( vDesiredPoint )
{
}
void CFormationPlaceAntitankState::SetHomeTransport( class CAITransportUnit *pTransport )
{
	NI_ASSERT_T( eState == FPAS_WAIT_FOR_HOMETRANSPORT, "wrong state" );
	eState = FPAS_ESITMATING ;
	pHomeTransport = pTransport;
}
void CFormationPlaceAntitankState::Segment()
{

	if ( !IsValidObj( pHomeTransport ) )
		TryInterruptState( 0 );

	switch( eState )
	{
	case FPAS_WAIT_FOR_HOMETRANSPORT:
		if ( pUnit->GetNextCommand() )
			pUnit->SetCommandFinished();

		break;
	case FPAS_ESITMATING:
		if ( fabs2( pUnit->GetCenter() - vDesiredPoint) > SConsts::TILE_SIZE*SConsts::TILE_SIZE )
		{
			CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vDesiredPoint, VNULL2, pUnit, true );
			if ( pPath )
			{
				pUnit->SendAlongPath( pPath, VNULL2 );
				eState = FPAS_APPROACHING;
			}
			else
			{
				pUnit->SendAcknowledgement( ACK_CANNOT_SUPPLY_NOT_PATH );
				TryInterruptState( 0 );
			}
		}
		else
			eState = FPAS_START_BUILD;

		break;
	case FPAS_APPROACHING:
		if ( fabs2(pUnit->GetCenter() - vDesiredPoint) < SConsts::TILE_SIZE*SConsts::TILE_SIZE )
		{
			const int nSize = pUnit->Size();
			for ( int i = 0; i < nSize; ++i )
			{
				const CVec2 vShift( GetVectorByDirection( WORD(Random(0, 65535) ) * SConsts::TILE_SIZE ) );
				CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vDesiredPoint, vShift, (*pUnit)[i], true );
				if ( pPath )
					(*pUnit)[i]->SendAlongPath( pPath, vShift );
			}
			eState = FPAS_APPROACHING_2;
		}
		else if ( pUnit->IsIdle() )
		{
			pUnit->SendAcknowledgement( ACK_CANNOT_SUPPLY_NOT_PATH );
			TryInterruptState( 0 );
		}

		break;
	case FPAS_APPROACHING_2:
		{
			bool bEveryIsIdle = true;
			for ( int i = 0; i < pUnit->Size(); ++i )
			{
				if ( !(*pUnit)[i]->IsIdle() )
				{
					bEveryIsIdle = false;
					break;
				}
			}
			if ( bEveryIsIdle )
				eState = FPAS_START_BUILD;
		}

		break;
	case FPAS_START_BUILD:
		{
			pUnit->StopUnit();
			eState = FPAS_START_BUILD_2;
		}

		break;
	case FPAS_START_BUILD_2:
		{
			for ( int i=0; i < pUnit->Size(); ++i )
				theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_USE, (float)ACTION_NOTIFY_USE_UP ), (*pUnit)[i] );

			fWorkAccumulator = 0.0f;
			eState = FPAS_BUILDING;
			timeBuild = curTime;

			CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
			const char * pszAntitankName = theUnitCreation.GetRandomAntitankObjectName();
			CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( pszAntitankName );
			const int nDBIndex = pIDB->GetIndex( pszAntitankName );

			const SObjectBaseRPGStats * pStats = static_cast<const SObjectBaseRPGStats *>(pIDB->GetRPGStats( pDesc ));

			if ( CStaticObject::CheckStaticObject( pStats, vDesiredPoint, 0 ) )
			{
				pAntitank = new CSimpleStaticObject( pStats, vDesiredPoint, nDBIndex, pStats->fMaxHP, 0, ESOT_COMMON, pUnit->GetPlayer(), true );
				pAntitank->Mem2UniqueIdObjs();
				pAntitank->Init();
				pAntitank->LockTiles();
				eState = FPAS_BUILDING;
			}
			else
			{
				pUnit->SendAcknowledgement( ACK_CANNOT_START_BUILD );
				TryInterruptState( 0 );
			}
		}

		break;
	case FPAS_BUILDING:
		if ( curTime - timeBuild > SConsts::TIME_QUANT )
		{
			fWorkAccumulator += SConsts::ENGINEER_ANTITANK_HALTH_PER_QUANT * ( curTime - timeBuild ) / SConsts::TIME_QUANT;
			timeBuild = curTime;

			if ( fWorkAccumulator >= pAntitank->GetHitPoints() )
			{
				CObstacleStaticObject *pObstacle = new CObstacleStaticObject( pAntitank );
				theStatObjs.AddStaticObject( pAntitank, true );
				theStatObjs.AddObstacle( pObstacle );
				pUnit->SendAcknowledgement( ACK_BUILDING_FINISHED, true );

				pHomeTransport->DecResursUnitsLeft( SConsts::ANTITANK_RU_PRICE );

				pUnit->SetCommandFinished();
			}
		}

		break;
	}
}
ETryStateInterruptResult CFormationPlaceAntitankState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->SetCommandFinished();

	if ( pAntitank )
		pAntitank->UnlockTiles();
	return TSIR_YES_IMMIDIATELY;
}
IUnitState * CFormationBuildLongObjectState::Instance( class CFormation *pUnit, class CLongObjectCreation *pCreation  )
{
	return new CFormationBuildLongObjectState( pUnit, pCreation );
}
CFormationBuildLongObjectState::CFormationBuildLongObjectState( class CFormation *pUnit, class CLongObjectCreation *pCreation )
: pUnit(pUnit), 
	eState(ETBS_WAITING_FOR_HOMETRANSPORT), 
	lastTime(curTime), 
	pCreation( pCreation ),
	fCompletion( 0 )
{
}
void CFormationBuildLongObjectState::SetHomeTransport( class CAITransportUnit *pTransport )
{
	NI_ASSERT_T( eState == ETBS_WAITING_FOR_HOMETRANSPORT, "wrong state" );
	eState = FBFS_READY_TO_START;
	pHomeTransport = pTransport;
	fWorkLeft = Min( SConsts::ENGINEER_RU_CARRY_WEIGHT, pTransport->GetResursUnitsLeft() );
}
ETryStateInterruptResult CFormationBuildLongObjectState::TryInterruptState( CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	for ( int i = 0; i < pUnit->Size(); ++i )
		(*pUnit)[i]->RestoreDefaultPath();

	return TSIR_YES_IMMIDIATELY;
}
void CFormationBuildLongObjectState::Segment()
{
	switch ( eState )
	{
	case ETBS_WAITING_FOR_HOMETRANSPORT:
		if ( pUnit->GetNextCommand() )
			pUnit->SetCommandFinished();

		break;
	case FBFS_READY_TO_START:
		{
			if ( pCreation->GetCurIndex() != 0 )
			{
				CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( pCreation->GetNextPoint(0,1), VNULL2, pUnit, true );

				if ( pStaticPath )
				{
					pUnit->SendAlongPath( pStaticPath, VNULL2 );
					eState = FBFS_APPROACHING_STARTPOINT;
				}
				eState = FBFS_APPROACHING_STARTPOINT;
			}
			else if ( pCreation->GetMaxIndex() != 0 )
			{
				CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( pCreation->GetNextPoint(0,1), VNULL2, pUnit, true );

				if ( pStaticPath )
				{
					pUnit->SendAlongPath( pStaticPath, VNULL2 );
					eState = FBFS_APPROACHING_STARTPOINT;
				}
			}
			if ( FBFS_READY_TO_START == eState )
			{
				pUnit->SendAcknowledgement( ACK_NEGATIVE );
				TryInterruptState( 0 );
			}
		}
		
		break;
	case FBFS_APPROACHING_STARTPOINT:
		if ( pUnit->IsIdle() )
		{
			eState = FBFS_NEXT_SEGMENT;
			const int nSize = pUnit->Size();
			for ( int i = 0; i < nSize; ++i )
				theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_IDLE), (*pUnit)[i], false );
		}

		break;
	case FBFS_BUILD_SEGMENT:
		if ( curTime - lastTime > SConsts::TIME_QUANT )
		{
			const float fAddWork = Min( 1.0f * pUnit->Size() *
														pCreation->GetBuildSpeed() *
														float( curTime - lastTime ) / SConsts::TIME_QUANT,
														fWorkLeft );
			float fDecResource = fAddWork;
			if ( fAddWork + pCreation->GetWorkDone() >= pCreation->GetPrice() )
			{
				fDecResource = pCreation->GetPrice() - pCreation->GetWorkDone();
				pCreation->BuildNext();
				eState = FBFS_NEXT_SEGMENT;
			}
			else 
				pCreation->AddWork( fAddWork );

			pHomeTransport->DecResursUnitsLeft( fDecResource );
			fWorkLeft -= fDecResource;

			if ( 0.0f == fWorkLeft )
				pUnit->SetCommandFinished();
		}

		break;
	case FBFS_NEXT_SEGMENT:
		if ( pCreation->GetCurIndex() < pCreation->GetMaxIndex() )
		{
			if ( fWorkLeft == 0 )
			{
				pUnit->SetCommandFinished();
			}
			else if ( pCreation->CanBuildNext() )
			{
				eState = FBFS_CHECK_FOR_UNITS_PREVENTING;
				pCreation->LockNext();
			}
			else
			{
				pCreation->LockCannotBuild();
				eState = FBFS_CANNOT_BUILD_ANYMORE;
			}
		}
		else
		{
			if ( pCreation->CannotFinish() )
				pUnit->SendAcknowledgement( ACK_CANNOT_FINISH_BUILD, true );
			else
				pUnit->SendAcknowledgement( ACK_BUILDING_FINISHED, true );
			pUnit->SetCommandFinished();
		}

		break;
	case FBFS_CANNOT_BUILD_ANYMORE:
		pUnit->SetCommandFinished();
		pUnit->SendAcknowledgement( ACK_CANNOT_FINISH_BUILD, true );
		pUnit->SetNewCoordinates( CVec3( (*pUnit)[0]->GetCenter(), 0.0f ), false );

		break;
	case FBFS_CHECK_FOR_UNITS_PREVENTING:
		{
			pCreation->GetUnitsPreventing( &unitsPreventing );
			SendUnitsAway( &unitsPreventing );
			eState = FBFS_WAIT_FOR_UNITS;
			lastTime = curTime;
		}

		break;
	case FBFS_WAIT_FOR_UNITS:
		if ( !pCreation->IsAnyUnitPrevent() )
			eState = FBFS_START_APPROACH_SEGMENT;
		else if ( curTime - lastTime > 15000 ) // ждали достаточно
			eState = FBFS_CANNOT_BUILD_ANYMORE;

		break;
	case FBFS_START_APPROACH_SEGMENT:
		{
			timeCounter.Count( 3, true );
			const int nSize = pUnit->Size();
			CVec2 vNewFormationCenter;
			for ( int i = 0; i < nSize; ++i )
			{
				CAIUnit * pSoldier= (*pUnit)[i];
				vNewFormationCenter = pCreation->GetNextPoint( i, nSize );
				if ( pCreation->IsCheatPath() )
				{
					pSoldier->SetCurPath( new CArtilleryCrewPath( pSoldier, pSoldier->GetCenter(), vNewFormationCenter, pSoldier->GetMaxPossibleSpeed()/2 ) );
				}
				else
				{
					timeCounter.Count( 4, true );					
					CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( vNewFormationCenter, VNULL2, pSoldier, true );
					timeCounter.Count( 4, false );
					pSoldier->SendAlongPath( pStaticPath, VNULL2 );
				}
			}
			pUnit->SetNewCoordinates( CVec3( vNewFormationCenter, 0.0f ), false );
			lastTime = curTime;
			eState = FBFS_APPROACH_SEGMENT;
			timeCounter.Count( 3, false );
		}
		break;
	case FBFS_APPROACH_SEGMENT:
		{
			bool bEveryIsOnPlace = true;
			for ( int i = 0; i < pUnit->Size(); ++i )
			{
				CAIUnit * pSold = (*pUnit)[i];
				if ( !pSold->IsIdle() )
				{
					bEveryIsOnPlace = false;
					break;
				}
			}
			if ( bEveryIsOnPlace ) 
			{
				eState = FBFS_BUILD_SEGMENT;
				lastTime = curTime;
				fCompletion = 0;
				for ( int i = 0; i < pUnit->Size(); ++i )
				{
					CAIUnit *pSoldier = (*pUnit)[i];
					pSoldier->RestoreDefaultPath();
					theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_USE, (float)ACTION_NOTIFY_USE_UP ), pSoldier );
				}
			}
		}
		break;
	}
}
void CFormationBuildLongObjectState::SendUnitsAway( std::list<CPtr<CAIUnit> > *pUnitsPreventing )
{
	CLine2 line = pCreation->GetCurLine();
	CVec2 vAway( line.a, line.b );
	Normalize( &vAway );
	for ( std::list<CPtr<CAIUnit> >::iterator it = pUnitsPreventing->begin(); it != pUnitsPreventing->end(); ++it )
	{
		CAIUnit * pUnit = *it;
		int nSign = - line.GetSign( pUnit->GetCenter() );
		nSign = nSign != 0 ? nSign : 1;
		const CVec2 vTo = pUnit->GetCenter() + 
					pUnit->GetBoundTileRadius() * 10 * SConsts::TILE_SIZE * nSign * vAway;
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_MOVE_TO, vTo ), pUnit, false );
	}
	pUnitsPreventing->clear();
}
void CFormationGunCrewState::SUnit::UpdateAction()
{
	if ( bForce || eNewAction != eAction )
	{
		NI_ASSERT_T( IsAlive(), "wrong call" );

		bForce = false;
		eAction = eNewAction;

		if ( eAction != ACTION_NOTIFY_NONE  )
		{
			if ( eAction == ACTION_NOTIFY_USE_UP || eAction == ACTION_NOTIFY_USE_DOWN )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_USE, (float)eAction ), pUnit, false );
			else if ( eAction == ACTION_NOTIFY_IDLE )
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), pUnit, false );
		}
	}

	if ( timeNextUpdate < curTime && eAction != ACTION_NOTIFY_NONE )
	{
		pUnit->UpdateDirection( wDirection );
		timeNextUpdate = curTime + 500;
	}
}
void CFormationGunCrewState::SUnit::ResetAnimation()
{
	bForce = true;
}
void CFormationGunCrewState::SUnit::SetAction( const CFormationGunCrewState::SCrewAnimation &rNewAnim, bool force )
{
	eNewAction = rNewAnim.eAction;
	wDirection = rNewAnim.wDirection;
	bForce |= force;
}
bool CFormationGunCrewState::SUnit::IsAlive() const 
{
	return IsValidObj( pUnit );
}
CFormationGunCrewState::SUnit::SUnit()
: eAction( ACTION_NOTIFY_NONE ), eNewAction( ACTION_NOTIFY_NONE ), bForce( true ), timeNextUpdate( 0 )
{
}
CFormationGunCrewState::SUnit::SUnit( class CSoldier * pUnit, const CVec2 &vServePoint, const EActionNotify eAction)
: pUnit( pUnit ), eAction( eAction ), eNewAction( ACTION_NOTIFY_NONE ), bForce( true ), vServePoint( vServePoint ), wDirection(0)
{
}
int CFormationGunCrewState::SUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 3, &eAction );
	saver.Add( 2, &pUnit);
	saver.Add( 5, &eNewAction );
	saver.Add( 6, &bForce );
	saver.Add( 7, &wDirection );
	saver.Add( 8, &timeNextUpdate );
	return 0;
}
int CFormationGunCrewState::SCrewMember::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vServePoint );
	saver.Add( 2, &bOnPlace );
	saver.AddTypedSuper( 3, static_cast<SUnit*>(this) );

	return 0;
}
CFormationGunCrewState::SCrewMember::SCrewMember()
: bOnPlace(false)
{
}
CFormationGunCrewState::SCrewMember::SCrewMember( const CVec2 &vServePoint, CSoldier *pUnit, const EActionNotify eAction)
: SUnit( pUnit, vServePoint, eAction ), bOnPlace(false)
{
}
IUnitState* CFormationGunCrewState::Instance( class CFormation *pUnit, CArtillery *pArtillery)
{
	return new CFormationGunCrewState( pUnit, pArtillery );
}
CFormationGunCrewState::CFormationGunCrewState( class CFormation *_pUnit, CArtillery * _pArtillery)
: pUnit( _pUnit ), pArtillery( _pArtillery ), startTime(curTime),
	fReloadProgress( 0 ),
	eGunState( EGSS_OPERATE ),
	timeLastUpdate( curTime ),
	nFormationSize( 0 ),
	nReloadPhaze( 0 )
{
	pUnit->SetSelectable( false );
	const int nSize = pUnit->Size();
	for ( int i = 0; i < nSize; ++i )
		(*pUnit)[i]->AllowLieDown( false );

	pArtillery->SetSelectable( pArtillery->GetPlayer()==theDipl.GetMyNumber() );

	pStats = static_cast<const SMechUnitRPGStats*>( pArtillery->GetStats() );
	b360DegreesRotate = pStats->platforms[1].constraint.wMax >= 65535;
	ClearState();
	updater.Update( ACTION_NOTIFY_SERVED_ARTILLERY, pUnit, pArtillery->GetUniqueId() );
	
	if ( EDI_ENEMY == theDipl.GetDiplStatus( pArtillery->GetInitialPlayer(), pUnit->GetPlayer() ) )
	{
		theStatistics.UnitCaptured( pUnit->GetPlayer() );
		pArtillery->SetInitialPlayer( pUnit->GetPlayer() );
		updater.Update( ACTION_NOTIFY_CHANGE_SCENARIO_INDEX, pArtillery, -1 );
	}
}
bool CFormationGunCrewState::ClearState()
{
	bReloadInProgress = false;

	freeUnits.clear();
	nFormationSize = pUnit->Size();
	for ( int i = 0; i < nFormationSize; ++i )
	{
		CSoldier *pSold = (*pUnit)[i];
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_IDLE), pSold, false );
		pSold->SetFree();
		pSold->RestoreDefaultPath();
		pSold->StopUnit();
		freeUnits.push_back( SUnit( pSold, VNULL2 ) );
	}

	if ( (pStats->type == RPG_TYPE_ART_MORTAR ||pStats->type == RPG_TYPE_ART_HEAVY_MG)&& eGunState == EGSS_MOVE )
	{
		pUnit->SetCarryedMortar( pArtillery );
		pUnit->SetGroupShift( pArtillery->GetGroupShift() );
		pUnit->SetSubGroup( pArtillery->GetSubGroup() );
		pUnit->InitWCommands( pArtillery );	// очередь команд миномета скопировать в очередь SQUAD
		updater.Update( ACTION_SET_SELECTION_GROUP, pUnit, pArtillery->GetUniqueId() );
		updater.Update( ACTION_NOTIFY_SELECT_CHECKED, pUnit, pArtillery->GetUniqueId() );
		pArtillery->Disappear();
		pArtillery->SetOffTankPit();
		pUnit->SetSelectable( true );
		pUnit->SetCommandFinished();
		
		return true;
	}
	else
	{
		crew.clear();
		NI_ASSERT_T( pStats->vGunners.size() == EGSS_MOVE + 1, NStr::Format("gunners structure has wrong size (%d)", pStats->vGunners.size()) );
		crew.resize( pStats->vGunners[eGunState].size() );
		NI_ASSERT_T( !crew.empty(), NStr::Format( "locators for gunner places in artillery %s are not exist", pStats->GetParentName()) );

		pArtillery->SetOperable( 1.0f );
		wGunTurretDir =  pArtillery->GetFrontDir() + pArtillery->GetTurret( 0 )->GetHorCurAngle();
	}
	return false;
}
void CFormationGunCrewState::Segment()
{
	if ( !IsValidObj( pArtillery ) )
	{
		pArtillery->DelCrew();	
		TryInterruptState( 0 );
		return;
	}

	CTurret *pTurret = pArtillery->GetTurret( 0 );
	const WORD wCurTurretHorDir = pTurret->GetHorCurAngle();
	const WORD wCurTurretVerDir = pTurret->GetVerCurAngle();

	const bool bNoAnimation = EGSS_MOVE != eGunState && b360DegreesRotate;
	
	const WORD wCurTuttetDir = pArtillery->GetFrontDir() + wCurTurretHorDir;
	const CVec2 vTurretDir = GetVectorByDirection( wGunTurretDir );
	
	const bool bReloaderRotatesWithTurret = b360DegreesRotate && pStats->type != RPG_TYPE_ART_AAGUN;

	const CVec2 vGunDir =  bReloaderRotatesWithTurret ? vTurretDir : GetVectorByDirection( pArtillery->GetFrontDir() );

	const WORD wCurBaseDir = bReloaderRotatesWithTurret ? wGunTurretDir : pArtillery->GetFrontDir();
	const CVec2 vCurGunPos = pArtillery->GetCenter();
	bool bRecalcPoints = wCurTuttetDir != wGunTurretDir || wGunBaseDir != wCurBaseDir || vCurGunPos != vGunPos;
	vGunPos = vCurGunPos;
	wGunTurretDir = wCurTuttetDir;
	wGunBaseDir = wCurBaseDir;
	
	EGunServeState eDesiredState = EGSS_OPERATE;
	if ( pArtillery->IsUninstalled() && pArtillery->GetCurUninstallAction() == ACTION_NOTIFY_UNINSTALL_MOVE )
		eDesiredState = EGSS_MOVE;
	else if ( pArtillery->IsInInstallAction() || pArtillery->IsUninstalled() )
	{
		eDesiredState = EGSS_ROTATE;
	}
	bool bFinishState = false;
	if ( eGunState != eDesiredState || nFormationSize < pUnit->Size() ) // change state
	{
		bRecalcPoints = true;
		eGunState = eDesiredState;
		bFinishState = ClearState();
	}

	if ( bFinishState )
		return;


	if ( bRecalcPoints )
		RecountPoints( vGunDir, vTurretDir );
	RefillCrew();
	const int iUnitsOnPlace = CheckThatAreOnPlace();
	SendThatAreNotOnPlace( bNoAnimation );
	
	switch ( eGunState )
	{
	case EGSS_OPERATE:
		{
			bool bStartReload = false;
			int nGun = pArtillery->GetNGuns();
			bool bNoAmmo = true;
			if ( !bStartReload )
			{
				for ( int i = 0; i< nGun; ++i )
				{
					CBasicGun *pGun = pArtillery->GetGun( i );
					bStartReload = pGun->IsRelaxing(); //some gun is waiting for reload
					if ( bStartReload && !bReloadInProgress )
						fReloadPrice = pGun->GetRelaxTime( false );
					bNoAmmo &= ( 0==pGun->GetNAmmo() );
				}
			}
			if ( bStartReload )
			{
				if ( !bReloadInProgress ) // только начали перезарядку
				{
					bReloadInProgress  = true;
					fReloadProgress = 0;
				}
			}

			eGunOperateSubState = EGOSS_RELAX;
			if ( bNoAmmo )
				fReloadProgress = 0;
			else if ( bReloadInProgress )
			{
				eGunOperateSubState = EGOSS_RELOAD;
				fReloadProgress += 1.0f * (curTime - startTime) *
														iUnitsOnPlace / pStats->vGunners[eGunState].size();
				nReloadPhaze = int ( fReloadProgress / (fReloadPrice / 3.0f) );
				if ( fReloadProgress >= fReloadPrice )
				{
					pArtillery->ClearWaitForReload();
					eGunOperateSubState = EGOSS_RELAX;
					bReloadInProgress = false;
				}
			}
			else if ( abs( wCurTurretVerDir - wTurretVerDir ) > 0 )
			{
				if ( abs( wCurTurretHorDir - wTurretHorDir ) > 0 )
					eGunOperateSubState = EGOSS_AIM;
				else
					eGunOperateSubState = EGOSS_AIM_VERTICAL;
			}
		}
		break;
	case EGSS_ROTATE:
		bReloadInProgress = false;
		break;
	case EGSS_MOVE:
		bReloadInProgress = false;
		break;
	}

	UpdateAnimations();
	
	pArtillery->SetOperable( 1.0f );

	startTime = curTime ;
	wTurretHorDir = wCurTurretHorDir;
	wTurretVerDir = wCurTurretVerDir;
}
void CFormationGunCrewState::RefillCrew()
{
	for ( int i=0; i< crew.size(); ++i )
	{
		if ( !crew[i].IsAlive() ) //кого-то убили
		{
			nFormationSize = pUnit->Size();
			if ( !freeUnits.empty() ) // если есть запасные, взять из запасных
			{
				crew[i].pUnit = freeUnits.front().pUnit;
				freeUnits.pop_front();
				crew[i].bOnPlace = false;
				crew[i].ResetAnimation();
				crew[i].pUnit->SetCurPath( new CArtilleryCrewPath( crew[i].pUnit, crew[i].pUnit->GetCenter(), crew[i].vServePoint, 0 ) );
			}
			else	// перераспределить команду.
			{
				for ( int j = crew.size() - 1; j > i; --j ) // найти живого на менее приоритетном месте
				{
					if ( crew[j].IsAlive() )
					{
	  				crew[i].pUnit->SetCurPath( new CArtilleryCrewPath( crew[i].pUnit, crew[i].pUnit->GetCenter(), crew[i].vServePoint, 0 ) );
						crew[i].pUnit = crew[j].pUnit;
						crew[j].pUnit = 0;
						crew[i].bOnPlace = false;
						crew[i].ResetAnimation();
						break;
					}
				}
			}
		}
	}
}
void CFormationGunCrewState::RecountPoints( const CVec2 &vGunDir, const CVec2 &vTurretDir )
{
	const CVec2 vCenter ( pArtillery->GetCenter() );

	const int nCrew = crew.size();
	const int nDesiredSize = pStats->vGunners[eGunState].size();
	NI_ASSERT_T( nDesiredSize != 0, NStr::Format("%s in state %d has 0 gunners", pStats->szKeyName, eGunState) )

	for ( int i = 0; i < nCrew; ++i )
	{
		const CVec2 vCrew( pStats->vGunners[eGunState][i%nDesiredSize] );
		const int nOffs = i / nDesiredSize;
		const CVec2 pt( vCrew.y, - vCrew.x * ( 1 + 1.1f * nOffs ) );
		crew[i].vServePoint = vCenter + ( pt ^ ( i == 1 ? vGunDir: vTurretDir ) );
	}

	int i = 0;
	for ( std::list< SUnit >::iterator it = freeUnits.begin(); it != freeUnits.end(); ++it )
	{
		const CVec2 freePoint( -pStats->vAABBHalfSize.y + pStats->vAABBCenter.y - SConsts::TILE_SIZE, i * SConsts::TILE_SIZE/2 );	
		const CVec2 vServePoint = vCenter + ( freePoint ^ vGunDir );
		(*it).vServePoint = vServePoint;
		++i;
	}
}				
WORD CFormationGunCrewState::CalcDirToAmmoBox( int nCrewNumber ) const
{
	return GetDirectionByVector( pArtillery->GetAmmoBoxCoordinates() - crew[nCrewNumber].pUnit->GetCenter() );
}
WORD CFormationGunCrewState::CalcDirFromTo( int nCrewNumberFrom, int nCrewNumberTo ) const
{
	const CVec2 vCenter ( pArtillery->GetCenter() );
	const CVec2 ptTo( pStats->vGunners[eGunState][nCrewNumberTo].y, - pStats->vGunners[eGunState][nCrewNumberTo].x );
	const CVec2 ptFrom( pStats->vGunners[eGunState][nCrewNumberFrom].y, - pStats->vGunners[eGunState][nCrewNumberFrom].x );
	const CVec2 vGunDir( GetVectorByDirection( wGunTurretDir ) );

	return GetDirectionByVector( (ptTo ^ vGunDir)  - (ptFrom ^ vGunDir) );
}
CFormationGunCrewState::SCrewAnimation CFormationGunCrewState::CalcAniamtionForMG( int iUnitNumber ) const
{
	SCrewAnimation animation;

	animation.wDirection = wGunTurretDir;
	switch( iUnitNumber )
	{
		case 0: 
			if ( EGSS_OPERATE == eGunState && IsGunAttacking() )
				animation.eAction = ACTION_NOTIFY_USE_UP;
			else
				animation.eAction = ACTION_NOTIFY_USE_DOWN;

			break;
		case 1: 
			animation.eAction = ACTION_NOTIFY_USE_DOWN; 
			break;
		default:
			animation.eAction = ACTION_NOTIFY_NONE; 
			break;
	}

	return animation;
}
bool CFormationGunCrewState::IsGunAttacking() const 
{
	return 	pArtillery->IsInstalled() &&
					pArtillery->GetState() &&
					( 
						pArtillery->GetState()->IsAttackingState() || 
						(b360DegreesRotate ? false : pArtillery->GetTurret( 0 )->GetHorCurAngle() != 0 )
					) &&
					0 != pArtillery->GetGun( 0 )->GetNAmmo();
}
CFormationGunCrewState::SCrewAnimation CFormationGunCrewState::CalcNeededAnimation( int iUnitNumber ) const
{
	SCrewAnimation animation;

	if ( RPG_TYPE_ART_HEAVY_MG == pStats->type )
		return CalcAniamtionForMG( iUnitNumber );

	switch( eGunState )
	{
	case EGSS_OPERATE:
		{
			switch( eGunOperateSubState )
			{
			case EGOSS_AIM_VERTICAL:
				switch ( iUnitNumber )
				{
				case 0:
					animation.eAction = ACTION_NOTIFY_USE_UP;
					animation.wDirection = wGunTurretDir;
					break;
				case 1:
					animation.eAction = ACTION_NOTIFY_IDLE;
					animation.wDirection = wGunBaseDir;
					break;
				case 2:
					animation.eAction = ACTION_NOTIFY_IDLE;
					animation.wDirection = wGunTurretDir;
					break;
				}

				break;
			case EGOSS_AIM:
				switch( iUnitNumber )
				{
				case 0:
					animation.wDirection = wGunTurretDir;
					animation.eAction = ACTION_NOTIFY_USE_UP;
					break;
				case 1:
					animation.eAction = ACTION_NOTIFY_MOVE;
					animation.wDirection = wGunBaseDir;
					break;
				case 2:
					animation.eAction = ACTION_NOTIFY_MOVE;
					animation.wDirection = wGunTurretDir;
					break;
				}
				
				break;
			
			case EGOSS_RELOAD:
				{
					switch( nReloadPhaze )
					{
					case 0: // достать из ящика патрон
						switch ( iUnitNumber )
						{
						case 0: 
							animation.wDirection = wGunTurretDir; 
							if ( pStats->type == RPG_TYPE_ART_MORTAR ) // у миномета передают патрон сидя
								animation.eAction = ACTION_NOTIFY_USE_DOWN; 
							else
								animation.eAction = ACTION_NOTIFY_USE_DOWN; 
							break;
						case 2: 
							animation.wDirection = wGunTurretDir; 
							if ( pStats->type == RPG_TYPE_ART_MORTAR ) // у миномета передают патрон сидя
								animation.eAction = ACTION_NOTIFY_USE_DOWN;
							else 
								animation.eAction = ACTION_NOTIFY_IDLE; 

							break;
						case 1: 
							animation.wDirection = CalcDirToAmmoBox( 1 );
							animation.eAction = ACTION_NOTIFY_USE_DOWN; 
							
							break;
						default: 
							animation.eAction = ACTION_NOTIFY_IDLE; 
							animation.wDirection = wGunTurretDir; 
							
							break;
						}

						break;
					case 1:	// передать заряжающему
						switch ( iUnitNumber )
						{
						case 0: 
							animation.wDirection = wGunTurretDir;
							if ( pStats->type == RPG_TYPE_ART_MORTAR ) // у миномета передают патрон сидя
								animation.eAction = ACTION_NOTIFY_USE_DOWN; 
							else
								animation.eAction = ACTION_NOTIFY_USE_DOWN; 
							
							break;
						case 2: // заряжаюший
							animation.wDirection = CalcDirFromTo( 2, 1 );
							if ( pStats->type == RPG_TYPE_ART_MORTAR ) // у миномета передают патрон сидя
								animation.eAction = ACTION_NOTIFY_USE_DOWN;
							else
								animation.eAction = ACTION_NOTIFY_USE_UP;

							break;
						case 1: // у ящика
							animation.wDirection = CalcDirFromTo( 1, 2 );
							if ( pStats->type == RPG_TYPE_ART_MORTAR ) // у миномета передают патрон сидя
								animation.eAction = ACTION_NOTIFY_USE_DOWN;
							else
								animation.eAction = ACTION_NOTIFY_USE_UP; 

							break;
						default: 
							animation.wDirection = wGunTurretDir; 
							animation.eAction = ACTION_NOTIFY_IDLE; 
							
							break;
						}

						break;
					case 2:	// зарядить патрон в пушку
					default:
						switch ( iUnitNumber )
						{
						case 0: // при зарядке миномета встать
							animation.wDirection = wGunTurretDir; 
							if ( pStats->type == RPG_TYPE_ART_MORTAR ) 
								animation.eAction = ACTION_NOTIFY_USE_UP;
							else
								animation.eAction = ACTION_NOTIFY_USE_DOWN; 

							break;
						case 1: // подающий у миномета сидит
							animation.wDirection = CalcDirToAmmoBox( 1 ); 
							animation.eAction = ACTION_NOTIFY_USE_DOWN; 

							break;
						case 2: 
							animation.wDirection = wGunTurretDir; 
							if ( pStats->type == RPG_TYPE_ART_MORTAR ) 
								animation.eAction = ACTION_NOTIFY_USE_DOWN; 
							else
								animation.eAction = ACTION_NOTIFY_USE_UP; 

							break;
						default: 
							animation.wDirection = wGunTurretDir; 
							animation.eAction = ACTION_NOTIFY_IDLE; 
	
							break;
						}
						
						break;
					}
				}

				break;
			case EGOSS_RELAX:
			default:
				{
					if ( iUnitNumber == 1 )
						animation.wDirection = wGunBaseDir;
					else
						animation.wDirection = wGunTurretDir;
					NI_ASSERT_T( 1 == pArtillery->GetNCommonGuns(), NStr::Format("artillery with %d gins, error", pArtillery->GetNCommonGuns()) );
					const bool bAttackingNow =	IsGunAttacking();
					if ( !bAttackingNow )
						animation.eAction = ACTION_NOTIFY_IDLE;
					else
					{
						if ( pStats->type == RPG_TYPE_ART_MORTAR && iUnitNumber >= 0 && iUnitNumber < 3  )  
							animation.eAction = ACTION_NOTIFY_USE_DOWN; // у миномета рассчет сидит во время боя
						else
						{
							switch ( iUnitNumber )
							{
							case 0: 
								animation.wDirection = wGunTurretDir;
								if ( pStats->type == RPG_TYPE_ART_MORTAR ) 
									animation.eAction = ACTION_NOTIFY_USE_UP;
								else
									animation.eAction = ACTION_NOTIFY_USE_DOWN; 
								break;
							case 1: 
								animation.wDirection = CalcDirToAmmoBox( 1 );
								animation.eAction = ACTION_NOTIFY_USE_DOWN; 

								break;
							case 2: 
								animation.wDirection = wGunTurretDir; 
								if ( pStats->type == RPG_TYPE_ART_MORTAR ) 
									animation.eAction = ACTION_NOTIFY_USE_DOWN; 
								else
									animation.eAction = ACTION_NOTIFY_IDLE; 

								break;
							default: 
								animation.wDirection = wGunTurretDir; 
								animation.eAction = ACTION_NOTIFY_IDLE; 

								break;
							}
						}
					}
				}
				break;
			}
		}

		break;
	case EGSS_ROTATE:
		animation.eAction = ACTION_NOTIFY_NONE;

		break;
	case EGSS_MOVE:
		animation.eAction = ACTION_NOTIFY_MOVE;
		animation.wDirection = wGunTurretDir;

		break;
	default:
		animation.wDirection = wGunTurretDir;
		animation.eAction = ACTION_NOTIFY_IDLE;

		break;
	}
	return animation;
}
int CFormationGunCrewState::CheckThatAreOnPlace()
{
	int iOnPlace = 0;
	for ( int i = 0; i < crew.size(); ++i )
	{
		SCrewMember &crewMember = crew[i];
		if ( crewMember.IsAlive() )
		{
			const CVec2 vDiff = crewMember.vServePoint - crewMember.pUnit->GetCenter();
			if ( fabs2( vDiff ) > 0.000001f )
			{
				crewMember.bOnPlace = false;
				crewMember.SetAction( SCrewAnimation( ACTION_NOTIFY_NONE, GetDirectionByVector( vDiff ) ) );
			}
			else if ( crewMember.bOnPlace )
			{
				++iOnPlace;
				crewMember.SetAction( CalcNeededAnimation( i ) );
			}
			else 
			{
				++iOnPlace;
				crewMember.bOnPlace = true;
				crewMember.SetAction( CalcNeededAnimation( i ), true );
			}
		}
	}

	for ( std::list<SUnit>::iterator it = freeUnits.begin(); it != freeUnits.end(); )
	{
		SUnit crewUnit( *it );
		if ( crewUnit.IsAlive() )
		{
			if ( crewUnit.pUnit->IsIdle() )
			{
				crewUnit.pUnit->StopUnit();
				crewUnit.SetAction( CalcNeededAnimation( -1 ) );
			}
			++it;
		}
		else
			it = freeUnits.erase( it );
	}

	return iOnPlace;
}
void CFormationGunCrewState::SendThatAreNotOnPlace( const bool bNoAnimation )
{
	for ( int i = 0; i < crew.size() ; ++i )
	{
		SCrewMember & crewMember = crew[i];
		if ( crewMember.IsAlive() )
		{
			if ( bNoAnimation )
			{
				const float fDiff2 = fabs2( crewMember.vServePoint - crewMember.pUnit->GetCenter() );
				if ( fDiff2 > 0 )					// is not on place
				{
					CArtilleryCrewPath* pPath = static_cast<CArtilleryCrewPath*>( crewMember.pUnit->GetCurPath() );
					const float fSpeed = pArtillery->GetSpeedLen();
					
					/*CVec2 vSpeed2 = crewMember.vServePoint - crewMember.pUnit->GetCenter();
					const float fCurrentSpeed = Min( fabs(vSpeed2), Max( fSpeed, crewMember.pUnit->GetMaxPossibleSpeed() ) );
					Normalize( &vSpeed2 );
					pPath->SetParams( crewMember.vServePoint, fCurrentSpeed, fCurrentSpeed * vSpeed2  );
					*/


					const float fCurrentSpeed = Max( fSpeed, crewMember.pUnit->GetMaxPossibleSpeed() );
					pPath->SetParams( crewMember.vServePoint, fCurrentSpeed );
					
					if ( !crewMember.pUnit->IsInSolidPlace() )
						units.UnitChangedPosition( crewMember.pUnit, crewMember.vServePoint );
					crewMember.pUnit->SetCoordWOUpdate( CVec3(crewMember.vServePoint,0.0f) );
					crewMember.pUnit->UpdateDirection( wGunBaseDir );
					updater.Update( ACTION_NOTIFY_PLACEMENT, crewMember.pUnit );
				}
				else
					crewMember.SetAction( CalcNeededAnimation( i ) );
				continue;
			}
			else if ( !crewMember.bOnPlace )
			{
				const CVec2 vDiff = crewMember.vServePoint - crewMember.pUnit->GetCenter();
				const float fDiff2 = fabs2( vDiff );
				if ( fDiff2 >= 0.01f ) //far from desired place
				{
					if ( crewMember.pUnit->IsInFirePlace() )
						crewMember.pUnit->SetFree();
					NI_ASSERT_T( dynamic_cast<CArtilleryCrewPath*>(crewMember.pUnit->GetCurPath()) != 0, "wrong path" );
					CArtilleryCrewPath* pPath = static_cast<CArtilleryCrewPath*>( crewMember.pUnit->GetCurPath() );
					const float fSpeed = pArtillery->GetSpeedLen();
					pPath->SetParams( crewMember.vServePoint, Max( fSpeed, crewMember.pUnit->GetMaxPossibleSpeed() )  );
					crewMember.pUnit->UpdateDirection( GetDirectionByVector( vDiff ) );
					continue;
				}
				
				if ( fDiff2 < 0.01f ) // near to desired place
				{
					NI_ASSERT_T( dynamic_cast<CArtilleryCrewPath*>(crewMember.pUnit->GetCurPath()) != 0, "wrong path" );
					CArtilleryCrewPath* pPath = static_cast<CArtilleryCrewPath*>( crewMember.pUnit->GetCurPath() );
					pPath->SetParams( crewMember.vServePoint, sqrt(fDiff2) / SConsts::AI_SEGMENT_DURATION );

					if ( !crewMember.pUnit->IsInSolidPlace() )
						units.UnitChangedPosition( crewMember.pUnit, crewMember.vServePoint );
					crewMember.pUnit->SetCoordWOUpdate( CVec3(crewMember.vServePoint,0.0f) );
					crewMember.pUnit->UpdateDirection( GetDirectionByVector( vDiff ) );
					continue;
				}
			}

			if( !crewMember.pUnit->IsInFirePlace() )
			{
				crewMember.pUnit->SetToFirePlace();
				crewMember.SetAction( CalcNeededAnimation( i ), true );
				crewMember.pUnit->AllowLieDown( false );
			}
			else
				crewMember.SetAction( CalcNeededAnimation( i ) );
		}
	}
	
	for ( std::list< SUnit >::iterator it = freeUnits.begin(); it != freeUnits.end(); ++it )
	{
		const CVec2 &vServePoint =  (*it).vServePoint ;
		const CVec2 &vCenter =  (*it).pUnit->GetCenter() ;
		if (	(*it).pUnit->IsIdle() &&
					fabs2( vCenter - vServePoint ) > sqr( static_cast<int>(SConsts::TILE_SIZE) ) )
		{
			CPtr<IStaticPath> pPath = CreateStaticPathToPoint( (*it).vServePoint, VNULL2, (*it).pUnit, true );
			if ( pPath )
			{
				(*it).pUnit->RestoreDefaultPath();
				(*it).pUnit->SendAlongPath( pPath, VNULL2);
				(*it).SetAction( SCrewAnimation(ACTION_NOTIFY_MOVE, (*it).pUnit->GetDir()) );
			}
		}
	}
}
void CFormationGunCrewState::UpdateAnimations()
{
	for ( int i = 0; i < crew.size(); ++i )
	{
		if ( crew[i].IsAlive() )
		{
			if ( crew[i].bOnPlace )
				crew[i].SetAction( CalcNeededAnimation( i ) );
			crew[i].UpdateAction();
		}
	}
	for ( CFreeUnits::iterator it = freeUnits.begin(); it != freeUnits.end(); ++it )
	{
		if ( it->IsAlive() )
		{
			it->SetAction( CalcNeededAnimation( -1 ) );
			it->UpdateAction();
		}
	}
}
void CFormationGunCrewState::SetAllAnimation( EActionNotify action, bool force )
{
	for ( int i=0; i< crew.size(); ++i )
		crew[i].SetAction( SCrewAnimation(action, crew[i].pUnit->GetDir()), force );
}				
void CFormationGunCrewState::Interrupt()
{
	if	( pUnit->IsIdle() )
		pUnit->StopUnit();

	const int nSize = pUnit->Size();
	for	( int i = 0; i < nSize; ++i )
	{
		CSoldier *pSold = (*pUnit)[i];
		pSold->SetFree();
		pSold->AllowLieDown( true );
		pSold->RestoreDefaultPath();
	}

	pUnit->SetCommandFinished();
	pArtillery->SetOperable( false );
	pArtillery->DelCrew();
	
	updater.Update( ACTION_NOTIFY_SERVED_ARTILLERY, pUnit, -1 );
}
bool CFormationGunCrewState::CanInterrupt() 
{
	return false;
}
ETryStateInterruptResult CFormationGunCrewState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand || 
				pCommand->ToUnitCmd().cmdType == ACTION_MOVE_CATCH_TRANSPORT ||
				pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_LEAVE ||
				pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_LOAD_NOW ||
				pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_LOAD ||
				pCommand->ToUnitCmd().cmdType == ACTION_MOVE_ENTER_TRANSPORT_CHEAT_PATH )
	{
		Interrupt();
		return TSIR_YES_IMMIDIATELY;
	}
	else
		return TSIR_YES_WAIT;
}
const CVec2 CFormationGunCrewState::GetPurposePoint() const
{
	if ( IsValidObj( pArtillery ) )
		return pArtillery->GetCenter();
	else
		return CVec2( -1.0f, -1.0f );
}
IUnitState* CFormationInstallMortarState::Instance( class CFormation *pUnit )
{
	return new CFormationInstallMortarState( pUnit );
}
CFormationInstallMortarState::CFormationInstallMortarState( class CFormation *pUnit )
: pUnit( pUnit ), timeInstall( curTime + 200 ), nStage( 0 )
{
	const int id = pUnit->InstallCarryedMortar();
	if ( id )
	{
		pArt = static_cast<CArtillery *>(units[id]);
		pArt->InstallAction( ACTION_NOTIFY_INSTALL_MOVE, true );
	}
	else
		pUnit->SetCommandFinished();
}
void CFormationInstallMortarState::Segment()
{
	if ( curTime > timeInstall )
	{
		if ( nStage == 0 )
		{
			if ( pArt && pArt->IsValid() && pArt->IsAlive() )
			{
				pArt->SetCrew( pUnit, false );
				pArt->SetSelectable( pUnit->IsSelectable() );
				updater.Update( ACTION_SET_SELECTION_GROUP, pArt, pUnit->GetUniqueId() );
				updater.Update( ACTION_NOTIFY_SELECT_CHECKED, pArt, pUnit->GetUniqueId() );

				theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_INSTALL), pArt, false );
			}
			nStage = 1;
			timeInstall = curTime + 200;
		}
		else if ( nStage == 1 )
		{
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_CATCH_ARTILLERY, pArt), pUnit, false );
			pUnit->SetSelectable( false );
			pUnit->SetCommandFinished();
		}
	}
}
ETryStateInterruptResult CFormationInstallMortarState::TryInterruptState( class CAICommand *pCommand )
{
	return TSIR_YES_WAIT;
}
IUnitState* CFormationUseSpyglassState::Instance( CFormation *pFormation, const CVec2 &point )
{
	return new CFormationUseSpyglassState( pFormation, point );
}
CFormationUseSpyglassState::CFormationUseSpyglassState( CFormation *_pFormation, const CVec2 &point )
: pFormation( _pFormation )
{
	bool bHoldPos = false;
	for ( int i = 0; i < pFormation->Size(); ++i )
	{
		CSoldier *pSoldier = (*pFormation)[i];

		if ( pSoldier->GetStats()->HasCommand( ACTION_COMMAND_USE_SPYGLASS ) &&
				 ( pSoldier->IsFree() || pSoldier->IsInFirePlace() ) )
		{
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_USE_SPYGLASS, point ), pSoldier, false );
			bHoldPos = true;
		}
	}

	if ( bHoldPos )
	{
		pFormation->StopUnit();	
		pFormation->GetBehaviour().moving = SBehaviour::EMHoldPos;
		for ( int i = 0; i < pFormation->Size(); ++i )
		{
			CSoldier *pSoldier = (*pFormation)[i];

			pSoldier->StopUnit();
			pSoldier->GetBehaviour().moving = SBehaviour::EMHoldPos;
		}
	}

	pFormation->SetToWaitingState();
}
void CFormationUseSpyglassState::Segment()
{
}
ETryStateInterruptResult CFormationUseSpyglassState::TryInterruptState( class CAICommand *pCommand )
{
	pFormation->UnsetFromWaitingState();
	pFormation->SetCommandFinished();

	return TSIR_YES_IMMIDIATELY;
}
const CVec2 CFormationUseSpyglassState::GetPurposePoint() const
{
	return pFormation->GetCenter();
}
IUnitState* CFormationCaptureArtilleryState::Instance( CFormation *pUnit, CArtillery *pArtillery, const bool _bUseFormationPart )
{
	return new CFormationCaptureArtilleryState( pUnit, pArtillery, _bUseFormationPart );
}
CFormationCaptureArtilleryState::CFormationCaptureArtilleryState( class CFormation *_pUnit, CArtillery *_pArtillery, const bool bUseFormationPart )
: pUnit( _pUnit ), pArtillery( _pArtillery ), eState( FCAS_ESTIMATING )
{
	if ( (pArtillery->IsBeingCaptured() && pArtillery->GetCapturedUnit() != pUnit) ||
			 (pArtillery->HasServeCrew() && pArtillery->GetCrew() != pUnit ) ||
			 !pArtillery->MustHaveCrewToOperate() )
	{
		pUnit->SetCommandFinished();
	}
	else
	{
		if ( bUseFormationPart )
		{
			if ( pUnit->GetSightRadius() < fabs( pUnit->GetCenter() - pArtillery->GetCenter() ) )
			{
				pUnit->SendAcknowledgement( ACK_NEGATIVE, true );
				pUnit->SetCommandFinished();
				return;
			}
			CPtr<CFormation> pCrew = theUnitCreation.CreateCrew( pArtillery, GetSingleton<IObjectsDB>(), -1, CVec3( pUnit->GetCenter(), 0.0f ), pUnit->GetPlayer(), false );

			const bool bUseOfficer = pCrew->Size() >= pUnit->Size();

			std::vector< CPtr<CSoldier> > soldiersToUse;
			
			for ( int i = 0; i < pUnit->Size(); ++i )
			{
				if ( bUseOfficer || RPG_TYPE_OFFICER != (*pUnit)[i]->GetStats()->type )
					soldiersToUse.push_back( (*pUnit)[i] );
			}
			
			const int nSize = pCrew->Size();
			std::vector< CPtr<CSoldier> > soldiersToDelete;
			for ( int i = 0; i < pCrew->Size(); ++i )
			{
				if ( i >= soldiersToUse.size() )
					soldiersToDelete.push_back( (*pCrew)[i] );
				else
				{
					CSoldier *pOldSoldier = soldiersToUse[i];
					CSoldier *pNewSoldier = (*pCrew)[i];

					const WORD wDirection = pOldSoldier->GetDir();
					const CVec2 vPos = pOldSoldier->GetCenter();

					pNewSoldier->UpdateDirection( wDirection );
					pNewSoldier->NullCreationTime();
					pNewSoldier->SetNewCoordinates( CVec3( vPos, 0.0f ) );
					pNewSoldier->CalcVisibility();
					updater.Update( ACTION_NOTIFY_PLACEMENT, pNewSoldier );
					usedSoldiers.push_back( pOldSoldier );
				}
			}
			
			for ( int nSold = 0; nSold < soldiersToDelete.size(); ++nSold ) // crew has more memebers that is needed
				soldiersToDelete[nSold]->Disappear();
		}
		else
			pUnit = _pUnit;
	}
}
void CFormationCaptureArtilleryState::Segment()
{
	if ( !IsValidObj( pArtillery ) )
	{
		pUnit->SetCommandFinished();
		pUnit->SetSelectable( pUnit->GetPlayer() == theDipl.GetMyNumber() );
	}
	
	switch ( eState )
	{
	case FCAS_ESTIMATING:
		{
			if ( !usedSoldiers.empty() )
			{
				for ( int i = 0; i < usedSoldiers.size(); ++i )
					usedSoldiers[i]->Disappear();
				usedSoldiers.clear();
				pUnit->SetCommandFinished();
				return;
			}
			
			CPtr<IStaticPath> pPath = CreateStaticPathToPoint( pArtillery->GetCenter(), VNULL2, pUnit, true );
			if ( pPath )
			{
				pUnit->SendAlongPath( pPath, VNULL2 );
				eState = FCAS_APPROACHING;
			}
			else 
			{
				pUnit->SendAcknowledgement( ACK_NEGATIVE );
				pUnit->SetCommandFinished();
			}
		}

		break;
	case FCAS_APPROACHING:
		if ( pUnit->IsIdle() )
		{
			pUnit->SetCommandFinished();
			pArtillery->SetCrew( pUnit ); // отдались
		}
		
		break;
	}
}
ETryStateInterruptResult CFormationCaptureArtilleryState::TryInterruptState( class CAICommand *pCommand )
{
	pUnit->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
IUnitState* CFormationRepairBridgeState::Instance( class CFormation *pFormation, class CFullBridge *pBridge )
{
	return new CFormationRepairBridgeState( pFormation, pBridge );
}
CFormationRepairBridgeState::CFormationRepairBridgeState( class CFormation *pFormation, class CFullBridge *pBridge )
: pBridgeToRepair( pBridge ), pUnit( pFormation ), eState( FRBS_WAIT_FOR_HOMETRANSPORT )
{
	pBridge->EnumSpans( &bridgeSpans );
	for( int i = 0; i < bridgeSpans.size(); ++i )
	{
		if ( bridgeSpans[i]->GetHitPoints() < 0.0f )
		{
			bridgeSpans.resize( i );
			break;
		}
	}
	CBridgeCreation::SortBridgeSpans( &bridgeSpans, pUnit );
}
void CFormationRepairBridgeState::Segment()
{
	if ( !IsValidObj( pHomeTransport ) )
		TryInterruptState( 0 );

	switch( eState )
	{
	case FRBS_WAIT_FOR_HOMETRANSPORT:
		if ( pUnit->GetNextCommand() )
			pUnit->SetCommandFinished();
		
		break;
	case FRBS_START_APPROACH:
		{
			if ( bridgeSpans.empty() ) 
				pUnit->SetCommandFinished();
			else
			{
				CPtr<IStaticPath> pPath = CreateStaticPathToPoint( bridgeSpans[0]->GetAttackCenter( pUnit->GetCenter() ), VNULL2, pUnit, true );
				if ( pPath )
					pUnit->SendAlongPath( pPath, VNULL2 );
				eState = FRBS_APPROACH;
			}
		}
		
		break;
	case FRBS_APPROACH:
		if ( pUnit->IsIdle() )
		{
			eState = FRBS_REPEAR;
			fWorkDone = 0;
			timeLastCheck = curTime;
		}

		break;
	case FRBS_REPEAR:
		if ( curTime - timeLastCheck > SConsts::TIME_QUANT )
		{
			fWorkDone = 1.0f * ( curTime - timeLastCheck ) / SConsts::TIME_QUANT * SConsts::ENGINEER_REPEAR_HP_PER_QUANT;
			fWorkDone = Min( fWorkLeft, fWorkDone );
			float fMissedWork = 0;
			int nSpansWithMissedHP = 0;

			for ( int i = 0; i < bridgeSpans.size(); ++i )
			{
				const SHPObjectRPGStats * pStats = bridgeSpans[i]->GetStats();
				const float fDWork = (pStats->fMaxHP - bridgeSpans[i]->GetHitPoints()) * pStats->fRepairCost;
				fMissedWork += fDWork;
				nSpansWithMissedHP += (fDWork != 0);
			}

			if ( fWorkDone >= fMissedWork || 0 == nSpansWithMissedHP )
			{
				for ( int i = 0; i < bridgeSpans.size(); ++i )
				{
					const SHPObjectRPGStats * pStats = bridgeSpans[i]->GetStats();
					bridgeSpans[i]->SetHitPoints( pStats->fMaxHP );
				}
				fWorkLeft -= fMissedWork;
				pHomeTransport->DecResursUnitsLeft( fMissedWork );
				pUnit->SetCommandFinished();
			}
			else
			{
				const float fWorkPerSpan = fWorkDone / nSpansWithMissedHP;
				for ( int i = 0; i < bridgeSpans.size(); ++i )
				{
					const SHPObjectRPGStats * pStats = bridgeSpans[i]->GetStats();
					float fNewHP = Min( pStats->fMaxHP, bridgeSpans[i]->GetHitPoints() + fWorkPerSpan / pStats->fRepairCost );
					bridgeSpans[i]->SetHitPoints( bridgeSpans[i]->GetHitPoints() + fWorkPerSpan / pStats->fRepairCost );
					fWorkLeft -= fWorkPerSpan;
					pHomeTransport->DecResursUnitsLeft( fWorkPerSpan );	
				}
			}
			fWorkDone = 0;
			if ( 1.0f >= fWorkLeft )
				pUnit->SetCommandFinished();
		}

		break;
	}
}
ETryStateInterruptResult CFormationRepairBridgeState::TryInterruptState( class CAICommand *pCommand )
{
	return TSIR_YES_IMMIDIATELY;
}
void CFormationRepairBridgeState::SetHomeTransport( class CAITransportUnit *pTransport )
{
	NI_ASSERT_T( eState == FRBS_WAIT_FOR_HOMETRANSPORT, "wrong state sequence" );
	eState = FRBS_START_APPROACH;
	pHomeTransport = pTransport;
	fWorkLeft = Min( SConsts::ENGINEER_RU_CARRY_WEIGHT, pTransport->GetResursUnitsLeft() );
}
IUnitState* CFormationRepairBuildingState::Instance( class CFormation *pFormation, class CBuilding *pBuilding )
{
	return new CFormationRepairBuildingState( pFormation, pBuilding );
}
CFormationRepairBuildingState::CFormationRepairBuildingState( class CFormation *pFormation, class CBuilding *pBuilding )
: pUnit( pFormation ), eState( EFRBS_WAIT_FOR_HOME_TRANSPORT ), pBuilding( pBuilding )
{
}
int CFormationRepairBuildingState::SendToNearestEntrance( CCommonUnit *pTransport, CBuilding * pStorage )
{
	
	const int nEntrances = pStorage->GetNEntrancePoints();
	CPtr<IStaticPath> pShortestPath;
	float fPathLen = 1000000;
	int nNearestEntrance = -1;
	for ( int i = 0; i < nEntrances; ++i )
	{
		const CVec2 &vEntrance = pStorage->GetEntrancePoint(i);
		CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( vEntrance, VNULL2, pTransport, true );
		if ( fabs(pStaticPath->GetFinishPoint() - vEntrance) < sqr(SConsts::TRANSPORT_RESUPPLY_OFFSET) &&
				 pStaticPath->GetLength() < fPathLen )
		{
			fPathLen = pStaticPath->GetLength();
			pShortestPath = pStaticPath;
			nNearestEntrance = i;
		}
	}

	if ( pShortestPath )
	{
		pTransport->SendAlongPath( pShortestPath, VNULL2 );
		return nNearestEntrance;
	}

	return -1;
}
void CFormationRepairBuildingState::Segment()
{
	if ( EFRBS_WAIT_FOR_HOME_TRANSPORT != eState )
	{
		if ( !IsValidObj( pHomeTransport ) )
			TryInterruptState( 0 );
		if ( pBuilding->GetHitPoints() == pBuilding->GetStats()->fMaxHP )
			pUnit->SetCommandFinished();
	}

	switch ( eState )
	{
	case EFRBS_WAIT_FOR_HOME_TRANSPORT:
		if ( pUnit->GetNextCommand() )
			pUnit->SetCommandFinished();		

		break;
	case EFRBS_START_APPROACH:
		{
			const int nEntrance = SendToNearestEntrance( pUnit, pBuilding );
			if ( -1 != nEntrance )
			{
				eState = EFRBS_APPROACHING;
				break;
			}
			else
			{
				pUnit->SendAcknowledgement( ACK_NEGATIVE );
				Interrupt();
			}
		}

		break;
	case EFRBS_APPROACHING:
		if ( pUnit->IsIdle() )
			eState = EFRBS_START_SERVICE;

		break;
	case EFRBS_START_SERVICE:
		for ( int i = 0; i < pUnit->Size(); ++i )
			theGroupLogic.InsertUnitCommand( SAIUnitCmd( ACTION_COMMAND_USE, (float)ACTION_NOTIFY_USE_UP ), (*pUnit)[i] );

		eState = EFRBS_SERVICING;
		lastRepearTime = curTime;
		fWorkAccumulator =0;

		break;
	case EFRBS_SERVICING:
		if ( curTime - lastRepearTime > SConsts::TIME_QUANT )
		{
			fWorkAccumulator += pUnit->Size() * SConsts::ENGINEER_REPEAR_HP_PER_QUANT * ( curTime - lastRepearTime ) / SConsts::TIME_QUANT;
			fWorkAccumulator = Min( fWorkAccumulator, fWorkLeft );

			const float maxHP = pBuilding->GetStats()->fMaxHP;
			const float curHP = pBuilding->GetHitPoints();
			const float &fRepCost = pBuilding->GetStats()->fRepairCost;
			
			const float fNewHP = Heal( maxHP, curHP, fRepCost, &fWorkAccumulator, &fWorkLeft, pHomeTransport );
			pBuilding->SetHitPoints( fNewHP );
			if ( pBuilding->GetStats()->fMaxHP == pBuilding->GetHitPoints() ||  //починили
					 fWorkLeft + fWorkAccumulator < fRepCost )
			{
				pUnit->SetCommandFinished();
			}

			lastRepearTime = curTime;
		}

		break;
	}
}
void CFormationRepairBuildingState::Interrupt()
{
	if ( !pUnit->IsIdle() )
		pUnit->StopUnit();

	pUnit->SetCommandFinished();
}
ETryStateInterruptResult CFormationRepairBuildingState::TryInterruptState( class CAICommand *pCommand )
{
	if ( !pCommand || pCommand->ToUnitCmd().cmdType == ACTION_MOVE_CATCH_TRANSPORT )
	{
		Interrupt();
		return TSIR_YES_IMMIDIATELY;
	}
	else if ( pCommand->ToUnitCmd().cmdType == ACTION_MOVE_SET_HOME_TRANSPORT ||
						pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_LOAD )
	{
		return TSIR_YES_WAIT;
	}
	return TSIR_NO_COMMAND_INCOMPATIBLE;
}
void CFormationRepairBuildingState::SetHomeTransport( class CAITransportUnit *pTransport )
{
	NI_ASSERT_T( eState == EFRBS_WAIT_FOR_HOME_TRANSPORT, "wrong state" );
	eState = EFRBS_START_APPROACH;
	pHomeTransport = pTransport;
	fWorkLeft = Min( SConsts::ENGINEER_RU_CARRY_WEIGHT, pTransport->GetResursUnitsLeft() );
}
