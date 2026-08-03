#include "StdAfx.h"

#include <float.h>

#include "TransportStates.h"
#include "SoldierStates.h"
#include "Units.h"
#include "UnitsIterators2.h"
#include "Technics.h"
#include "GroupLogic.h"
#include "PointChecking.h"
#include "AIStaticMap.h"
#include "Entrenchment.h"
#include "Commands.h"
#include "Guns.h"
#include "UnitCreation.h"
#include "StaticObjects.h"
#include "..\Formats\fmtTerrain.h"
#include "Updater.h"
#include "Building.h"
#include "Formation.h"
#include "Soldier.h"
#include "Artillery.h"
#include "FormationStates.h"
#include "Diplomacy.h"
#include "TechnicsStates.h"
#include "PresizePath.h"
#include "EntrenchmentCreation.h"
#include "Mine.h"
#include "Bridge.h"
#include "StaticObjectsIters.h"
extern CDiplomacy theDipl;
extern CUpdater updater;
extern CUnitCreation theUnitCreation;
extern CGroupLogic theGroupLogic;
extern CUnits units;
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
extern CStaticObjects theStatObjs;
IUnitState* CTransportResupplyState::Instance( CAITransportUnit *pTransport, const CVec2 &vServePoint, CAIUnit *_pPreferredUnit )
{
	return new CTransportResupplyState( pTransport, vServePoint, _pPreferredUnit );
}
CTransportResupplyState::CTransportResupplyState( CAITransportUnit *_pTransport, const CVec2 &vServePoint, CAIUnit *_pPreferredUnit )
: CTransportServeState( _pTransport, vServePoint, _pPreferredUnit )
{
}
bool CTransportResupplyState::FindUnitToServe( bool *pIsNotEnoughRU )
{
	CFormationServeUnitState::CFindFirstUnitPredicate pred;
	CFormationResupplyUnitState::FindUnitToServe( pTransport->GetCenter(), 
																								pTransport->GetPlayer(), 
																								Min(pTransport->GetResursUnitsLeft(), SConsts::ENGINEER_RU_CARRY_WEIGHT),
																								pLoaderSquad, &pred, pPreferredUnit );
	*pIsNotEnoughRU = pred.IsNotEnoughRu();
	return pred.HasUnit();
}
void CTransportResupplyState::UpdateActionBegin()
{
	if ( !bUpdatedActionsBegin )
		pTransport->SendAcknowledgement( ACK_START_SERVICE_RESUPPLY );
	bUpdatedActionsBegin = true;
}
void CTransportResupplyState::SendLoaders()
{
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_RESUPPLY_UNIT, pPreferredUnit ), pLoaderSquad, false );
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_SET_HOME_TRANSPORT, pTransport), pLoaderSquad, true );
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_CATCH_TRANSPORT, pTransport), pLoaderSquad, true );
}
IUnitState* CTransportResupplyHumanResourcesState::Instance( CAITransportUnit *pTransport, const CVec2 &vServePoint, CArtillery *_pPreferredUnit )
{
	return new CTransportResupplyHumanResourcesState( pTransport, vServePoint, _pPreferredUnit );
}
CTransportResupplyHumanResourcesState::CTransportResupplyHumanResourcesState( CAITransportUnit *_pTransport, const CVec2 &_vServePoint, CArtillery *_pPreferredUnit )
: pTransport( _pTransport ), vServePoint( _vServePoint + _pTransport->GetGroupShift() ), timeLastUpdate( 0 ), eState( ETSHR_ESTIMATING ), pPreferredUnit( _pPreferredUnit ), bWaitForPath( false )
{
	if ( pPreferredUnit )
		vServePoint = pPreferredUnit->GetCenter();
}
void CTransportResupplyHumanResourcesState::Segment()
{
	if ( pTransport->GetResursUnitsLeft() < SConsts::SOLDIER_RU_PRICE )
		eState = ETSHR_GOTO_STORAGE;

	switch ( eState )
	{
	case ETSHR_GOTO_STORAGE:
		theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LOAD_RU), pTransport );

		break;
	case ETSHR_ESTIMATING:
		{
			if ( pTransport->GetResursUnitsLeft() < SConsts::SOLDIER_RU_PRICE )
				eState = ETSHR_GOTO_STORAGE;
			else
			{
				CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( vServePoint, pPreferredUnit ? VNULL2 : pTransport->GetGroupShift(), pTransport, true );
				if ( pStaticPath )
				{
					bWaitForPath = fabs2( pStaticPath->GetFinishPoint() - vServePoint ) < sqr( int(SConsts::TILE_SIZE) );
					pTransport->SendAlongPath( pStaticPath, pPreferredUnit ? VNULL2 : pTransport->GetGroupShift() );
					eState = ETSHR_APPROACHNIG;
				}
				else
				{
					pTransport->SendAcknowledgement( ACK_CANNOT_SUPPLY_NOT_PATH );
					TryInterruptState( 0 );
				}
			}
		}

		break;
	case ETSHR_APPROACHNIG:
		if ( bWaitForPath ? pTransport->IsIdle() : IsUnitNearPoint( pTransport, vServePoint, SConsts::TRANSPORT_RESUPPLY_OFFSET ) )
		{
			pTransport->StopUnit();
			eState = ETSHR_START_SERVE_ARTILLERY;
		}
		else if ( pTransport->IsIdle() )
		{
			pTransport->SendAcknowledgement( ACK_CANNOT_SUPPLY_NOT_PATH );
			TryInterruptState( 0 );
		}
		
		break;
	case ETSHR_WAIT_FOR_UNITS:
		if ( curTime - timeLastUpdate > pTransport->GetBehUpdateDuration() )
			eState = ETSHR_START_SERVE_ARTILLERY;

		break;
	case ETSHR_START_SERVE_SQUAD:
		{
			timeLastUpdate = curTime;
			FindNotCompleteSquads( &notCompleteSquads );
			if ( notCompleteSquads.empty() )
				eState = ETSHR_WAIT_FOR_UNITS;
			else
				eState = ETSHR_SERVING_SQUAD;
		}

		break;
	case ETSHR_START_SERVE_ARTILLERY:
		{
			timeLastUpdate = curTime;
			FindEmptyArtillery( &emptyArtillery, pPreferredUnit );
			if ( !emptyArtillery.empty() )
				eState = ETSHR_SERVING_ARTILLERY;
			else
				eState = ETSHR_START_SERVE_SQUAD;
		}

		break;
	case ETSHR_SERVING_SQUAD:
		if ( curTime - timeLastUpdate > SConsts::SQUAD_MEMBER_LEAVE_INTERVAL )
		{
			timeLastUpdate = curTime;
			if ( !notCompleteSquads.empty() )
			{
				if ( ServeSquad( *notCompleteSquads.begin() ) )
					notCompleteSquads.pop_front();
			}
			else
				eState = ETSHR_START_SERVE_SQUAD;
		}

		break;
	case ETSHR_SERVING_ARTILLERY:
		if ( curTime - timeLastUpdate > SConsts::SQUAD_MEMBER_LEAVE_INTERVAL )
		{
			timeLastUpdate = curTime;
			if ( !emptyArtillery.empty() )
			{
				if ( ServeArtillery( *emptyArtillery.begin() ) )
					emptyArtillery.pop_front();
			}
			else
				eState = ETSHR_START_SERVE_ARTILLERY;
		}

		break;
	}
}
bool CTransportResupplyHumanResourcesState::ServeSquad( CFormation *pSquad )
{
	if ( !pSquad || !pSquad->IsValid() || !pSquad->IsAlive() )
		return true;
	const SSquadRPGStats::SFormation &rFormation = pSquad->GetStats()->formations[pSquad->GetCurGeometry()];
	
	if ( rFormation.order.size() != pSquad->Size() + pSquad->VirtualUnitsSize() )
	{
		std::vector<bool> present( rFormation.order.size(), false );
		for ( int nSold = 0; nSold < pSquad->Size(); ++nSold )
		{
			const BYTE cSlotInStats = pSquad->GetUnitSlotInStats( nSold );
			present[cSlotInStats] = true;
		}
		for ( int nSold = 0; nSold < pSquad->VirtualUnitsSize(); ++nSold )
		{
			const BYTE cSlotInStats = pSquad->GetVirtualUnitSlotInStats( nSold );
			present[cSlotInStats] = true;
		}
		int nSlot = 0;
		while ( nSlot < present.size() && present[nSlot] )
			++nSlot;

		CVec3 vEntrancePoint( pTransport->GetEntrancePoint(), pTransport->GetZ() );	

		const int id = theUnitCreation.AddNewUnit( rFormation.order[nSlot].pSoldier,
																								1.0f, 
																								vEntrancePoint.x, vEntrancePoint.y, vEntrancePoint.z,
																								theUnitCreation.GetObjectDB()->GetIndex( rFormation.order[nSlot].szSoldier.c_str() ),
																								pTransport->GetDir() + 65535/2,
																								pTransport->GetPlayer(),
																								theUnitCreation.GetObjectDB()->GetDesc( rFormation.order[nSlot].szSoldier.c_str() )->eVisType,
																								false, true );

		units[id]->SetSelectable( pSquad->IsSelectable() );
		NI_ASSERT_T( dynamic_cast<CSoldier*>(units[id]) != 0, "Wrong type of unit created" );
		CSoldier *pSoldier = static_cast<CSoldier*>(units[id]);
		units[id]->SetSelectable( false );
		pTransport->SetResursUnitsLeft( pTransport->GetResursUnitsLeft() - SConsts::SOLDIER_RU_PRICE );

		CCommonUnit *pSingleUnitFormation = theUnitCreation.CreateSingleUnitFormation( pSoldier );
		pSoldier->SetFree();
		pSquad->AddVirtualUnit( pSoldier, nSlot );
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_CATCH_FORMATION, pSquad ), pSingleUnitFormation, false );
	}

	if ( rFormation.order.size() == pSquad->Size() + pSquad->VirtualUnitsSize() )
		return true;

	return false;
}
bool CTransportResupplyHumanResourcesState::ServeArtillery( CArtillery *pArtillery )
{
	if ( !pArtillery || !pArtillery->IsValid() || !pArtillery->IsAlive() || 
				pArtillery->IsBeingCaptured() )
		return true;

	const CVec3 vEntrancePoint( pTransport->GetEntrancePoint(), pTransport->GetZ() );

	if ( pTransport->GetPlayer() == theDipl.GetNeutralPlayer() )
		pTransport->SetCommandFinished();
	else
		theUnitCreation.CreateCrew( pArtillery, 0, 1, vEntrancePoint, pTransport->GetPlayer(), false );

	return true;
}
bool CTransportResupplyHumanResourcesState::CheckArtillery( CAIUnit *pU ) const
{
	if ( IsValidObj( pU ) && pU->GetStats()->IsArtillery() &&
			 ( pU->GetParty() == pTransport->GetParty() || pU->GetPlayer() == theDipl.GetNeutralPlayer() ) &&
			 pU->IsVisible( pTransport->GetParty()) && EUSN_BEING_TOWED != pU->GetState()->GetName() )
	{
		CArtillery * pArtillery = static_cast<CArtillery*>(pU);
		if ( pArtillery && !pArtillery->IsBeingCaptured() && !pArtillery->HasServeCrew() && pArtillery->MustHaveCrewToOperate() )
		{
			return true;
		}
	}
	return false;
}
void CTransportResupplyHumanResourcesState::FindEmptyArtillery( std::list< CPtr<CArtillery> > *pArtillerys, CArtillery *_pPreferredUnit ) const
{
	if ( CheckArtillery( _pPreferredUnit ) ) 
		pArtillerys->push_back( _pPreferredUnit );

	for( CUnitsIter<0,2> iter( pTransport->GetParty(), ANY_PARTY, vServePoint, SConsts::RESUPPLY_RADIUS );
				!iter.IsFinished(); iter.Iterate() )
	{
		if ( CheckArtillery( *iter ) )
			pArtillerys->push_back( static_cast<CArtillery*>(*iter));
	}	
}
void CTransportResupplyHumanResourcesState::FindNotCompleteSquads( std::list< CPtr<CFormation> > *pSquads ) const
{
	for ( CUnitsIter<0,2> iter( pTransport->GetParty(), EDI_FRIEND, vServePoint, SConsts::RESUPPLY_RADIUS );
				!iter.IsFinished(); iter.Iterate() )
	{
		CPtr<CAIUnit> pU = (*iter);
		if ( IsValidObj( pU ) && pU->GetStats()->IsInfantry() )
		{
			CSoldier * pSold = static_cast_ptr<CSoldier*>( pU );
			CFormation * pForm = pSold->GetFormation();
			if (  pForm && 
						pForm->IsResupplyable() && 
						pForm->Size() != pForm->GetStats()->members.size() )
			{
				pSquads->push_back( pForm );
			}
		}
	}
	pSquads->unique();
}
ETryStateInterruptResult CTransportResupplyHumanResourcesState::TryInterruptState( class CAICommand *pCommand )
{
	pTransport->SetCommandFinished();
	return TSIR_YES_IMMIDIATELY;
}
IUnitState* CTransportRepairState::Instance( CAITransportUnit *pTransport, const CVec2 &vServePoint, CAIUnit *_pPreferredUnit )
{
	return new CTransportRepairState( pTransport, vServePoint, _pPreferredUnit );
}
CTransportRepairState::CTransportRepairState( CAITransportUnit *pTransport, const CVec2 &vServePoint, CAIUnit *_pPreferredUnit )
: CTransportServeState( pTransport, vServePoint, _pPreferredUnit )
{
}
void CTransportRepairState::UpdateActionBegin()
{
	if ( !bUpdatedActionsBegin )
		pTransport->SendAcknowledgement( ACK_START_SERVICE_REPAIR );
	bUpdatedActionsBegin = true;
}
bool CTransportRepairState::FindUnitToServe( bool *pIsNotEnoughRU )
{
	CFormationServeUnitState::CFindFirstUnitPredicate pred;
	CFormationRepairUnitState::FindUnitToServe( pTransport->GetCenter(), 
																							pTransport->GetPlayer(), 
																							pTransport->GetResursUnitsLeft(), 
																							pLoaderSquad, &pred,
																							pPreferredUnit );
	*pIsNotEnoughRU |= pred.IsNotEnoughRu();
	return pred.HasUnit();
}
void CTransportRepairState::SendLoaders()
{
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_REPAIR_UNIT ), pLoaderSquad, false );
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_SET_HOME_TRANSPORT, pTransport), pLoaderSquad, true );
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_CATCH_TRANSPORT, pTransport), pLoaderSquad, true );
}
IUnitState* CTransportLoadRuState::Instance( CAITransportUnit *pTransport, const bool bSubState, CBuildingStorage *_pPreferredStorage )
{
	return new CTransportLoadRuState( pTransport, bSubState, _pPreferredStorage );
}
CTransportLoadRuState::CTransportLoadRuState ( CAITransportUnit *_pTransport, const bool bSubState, CBuildingStorage *_pPreferredStorage )
: pTransport( _pTransport ), eState( ETLRS_SEARCH_FOR_STORAGE ), nEntrance( -1 ), bSubState( bSubState ),
	pStorage( _pPreferredStorage )
{
	if ( pTransport->GetResursUnitsLeft() == SConsts::TRANSPORT_RU_CAPACITY )
		pTransport->SetCommandFinished();
}
CBuildingStorage* CTransportLoadRuState::FindNearestSource()
{
	class CFindNearestConnected : public CStaticObjects::IEnumStoragesPredicate 
	{
		CPtr<CBuildingStorage> pNearest;
		float fPathLength;
	public:
		CFindNearestConnected() : fPathLength( 0 ) {  }
		virtual bool OnlyConnected() const { return true; }
		virtual bool AddStorage( class CBuildingStorage * pStorage, const float _fPathLength )
		{
			if ( pStorage->IsAlive() && ( !pNearest || fPathLength > _fPathLength ) )
			{
				fPathLength = _fPathLength;
				pNearest = pStorage;
			}
			return false;
		}
		class CBuildingStorage * GetNearest() { return pNearest; };
	};
	
	CFindNearestConnected pred;
	theStatObjs.EnumStoragesInRange( pTransport->GetCenter(), 
																	 pTransport->GetParty(),
																	 SConsts::RESUPPLY_MAX_PATH,
																	 SConsts::TRANSPORT_LOAD_RU_DISTANCE,
																	 pTransport,
																	 &pred );
	return pred.GetNearest();
}
void CTransportLoadRuState::Segment()
{
	if ( ETLRS_SEARCH_FOR_STORAGE != eState && ( !IsValidObj( pStorage ) || theDipl.GetNParty(pStorage->GetPlayer()) != pTransport->GetParty() ) )
	{
		Interrupt();
		return;
	}

	switch( eState )
	{
	case ETLRS_SEARCH_FOR_STORAGE:
		{
			if ( (
							IsValidObj( pStorage ) &&	// preferred storage exists
							theDipl.GetNParty(pStorage->GetPlayer()) == pTransport->GetParty()	// and of our party
					 ) ||
					 ( 
							(pStorage = FindNearestSource()) 
					 )
				 )
			{
				if ( -1 != (nEntrance = CFormationRepairBuildingState::SendToNearestEntrance( pTransport, pStorage ))  )
				{
					if ( bSubState )
						pTransport->SendAcknowledgement( ACK_GOING_TO_STORAGE, true );
					eState = ETLRS_APPROACHING_STORAGE;
				}
				else //if ( bSubState )
					pTransport->SendAcknowledgement( ACK_NO_RESOURCES_CANT_FIND_PATH_TO_DEPOT, true );
			}
			else //if ( bSubState )
				pTransport->SendAcknowledgement( ACK_NO_RESOURCES_CANT_FIND_DEPOT, true );

			if ( ETLRS_APPROACHING_STORAGE != eState )
			{
				theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_GUARD), pTransport, false );
				Interrupt();
			}
		}	

		break;
	case ETLRS_APPROACHING_STORAGE:
		if ( !IsValidObj( pStorage ) )
			eState = ETLRS_SEARCH_FOR_STORAGE;
		else if ( fabs2( pTransport->GetCenter()-pStorage->GetEntrancePoint(nEntrance)) < sqr(SConsts::TRANSPORT_LOAD_RU_DISTANCE) )
		{
			pTransport->StopUnit();
			eState = ETLRS_START_LOADING_RU;
		}
		else if ( pTransport->IsIdle() )
			eState = ETLRS_START_LOADING_RU;

		break;
	case ETLRS_START_LOADING_RU:
		if ( IsValidObj( pStorage ) )
		{
			CreateSquad();

			if ( pLoaderSquad )
			{
				CAITransportUnit::PrepareLoaders( pLoaderSquad, pTransport );
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_LOAD_RU, pStorage ), pLoaderSquad, false );
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_SET_HOME_TRANSPORT, pTransport), pLoaderSquad, true );
				theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_CATCH_TRANSPORT, pTransport, SConsts::ENGINEER_RU_CARRY_WEIGHT/pLoaderSquad->Size()),
																	 pLoaderSquad, true );
				eState = ETLRS_LOADING_RU;
			}
		}
		else
			eState = ETLRS_SEARCH_FOR_STORAGE;

		break;
	case ETLRS_LOADING_RU:
		if ( !IsValidObj( pLoaderSquad ) )
		{
			const float fRes = pTransport->GetResursUnitsLeft();
			if ( fRes < SConsts::TRANSPORT_RU_CAPACITY )
			{
				eState = ETLRS_START_LOADING_RU;
			}
			else 
				Interrupt();
		}

		break;
	case ETLRS_WAIT_FOR_LOADERS:
		if ( IsValidObj( pLoaderSquad ) )
		{
			if ( pLoaderSquad->IsInTransport() )
			{
				pTransport->Unlock();
				pLoaderSquad->Disappear();
				pTransport->SetCommandFinished();
			}
		}
		else
			pTransport->SetCommandFinished();
		
		break;

	}
}
void CTransportLoadRuState::Interrupt()
{
	if ( bSubState )
		eState  = ETLRS_SUBSTATE_FINISHED;
	else 
		TryInterruptState( 0 );
}
void CTransportLoadRuState::CreateSquad()
{
	pTransport->Unlock();
	if ( IsValidObj( pLoaderSquad ) )
		pLoaderSquad->Disappear();
	
	if ( pTransport->GetPlayer() == theDipl.GetNeutralPlayer() )
	{
		pLoaderSquad = 0;
		pTransport->SetCommandFinished();
	}
	else
	{
		pLoaderSquad = theUnitCreation.CreateResupplyEngineers( pTransport );
		pTransport->Lock( pLoaderSquad );
	}
}
ETryStateInterruptResult CTransportLoadRuState::TryInterruptState( class CAICommand *pCommand )
{
	if ( pCommand && pTransport->IsAlive() )
	{
		if ( IsValidObj( pLoaderSquad ) && !pLoaderSquad->IsInTransport() )
		{
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pTransport), pLoaderSquad, false );
		}

		pTransport->AddExternLoaders( pLoaderSquad );
		pTransport->Unlock();
		pTransport->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
	else
	{
		CAITransportUnit::FreeLoaders( pLoaderSquad, pTransport );
		pTransport->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}

}
CTransportServeState::CTransportServeState( class CAITransportUnit *_pTransport, const CVec2 & _vServePoint, CAIUnit *_pPreferredUnit )
: pTransport( _pTransport ),eState( ETRS_WAIT_FOR_UNLOCK ),timeLastUpdate ( curTime ),
	vServePoint( _vServePoint + _pTransport->GetGroupShift() ), bUpdatedActionsBegin( false ),
	pPreferredUnit( _pPreferredUnit ), bWaitForPath( false )
{
	if ( IsValidObj( pPreferredUnit ) )
		vServePoint = pPreferredUnit->GetCenter();

	if ( IsValidObj( pPreferredUnit ) )
	{
		const SUnitBaseRPGStats *pStats = pPreferredUnit->GetStats();
		if ( pStats->IsInfantry() )
		{
			IUnitState *pCurrentState = static_cast_ptr<CSoldier*>(pPreferredUnit)->GetFormation()->GetState();
			if ( pCurrentState && pCurrentState->GetName() == EUSN_GUN_CREW_STATE )
			{
				CFormationGunCrewState *pGunCrewState = static_cast<CFormationGunCrewState*>( pCurrentState );
				pPreferredUnit = pGunCrewState->GetArtillery();
			}
		}
	}
}
void CTransportServeState::Segment()
{
	switch (eState)
	{
	case ETRS_WAIT_FOR_UNLOCK:
		if ( !pTransport->IsLocked() )
		{
			if ( pTransport->GetPlayer() == theDipl.GetNeutralPlayer() )
			{
				pLoaderSquad = 0;
				pTransport->SetCommandFinished();
			}
			else
			{
				pLoaderSquad = theUnitCreation.CreateResupplyEngineers( pTransport );
				pTransport->Lock( pLoaderSquad );
				eState = ETRS_INIT;
			}
		}

		break;
	case ETRS_INIT:
		eState = ETRS_START_APPROACH;

		break;
	case ETRS_GOING_TO_STORAGE:
		theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_MOVE_LOAD_RU), pTransport );

		break;
	case ETRS_START_APPROACH:
		{
			if ( pTransport->GetResursUnitsLeft() == 0.0f )
				eState = ETRS_GOING_TO_STORAGE;
			else
			{
				if ( fabs2( pTransport->GetCenter() - vServePoint ) < sqr( int(SConsts::TILE_SIZE) ) )
				{
					eState = ETRS_APPROACHING;
				}
				else
				{
					CPtr<IStaticPath> pStaticPath = CreateStaticPathToPoint( vServePoint, pTransport->GetGroupShift(), pTransport, true );

					if ( pStaticPath )
					{
						bWaitForPath = fabs2( pStaticPath->GetFinishPoint() - vServePoint ) < sqr( int(SConsts::TILE_SIZE) );
						pTransport->SendAlongPath( pStaticPath, pTransport->GetGroupShift() );
						eState = ETRS_APPROACHING;
					}
					else
					{
						pTransport->SendAcknowledgement( ACK_CANNOT_SUPPLY_NOT_PATH );
						TryInterruptState( 0 );
					}
				}
			}
		}

		break;
	case ETRS_APPROACHING:
		{
			if ( bWaitForPath ? pTransport->IsIdle() : IsUnitNearPoint( pTransport, vServePoint, SConsts::TRANSPORT_RESUPPLY_OFFSET ) )
			{
				pTransport->StopUnit();
				eState = ETRS_CREATE_SQUAD;
			}
			else if ( pTransport->IsIdle() )
			{
				pTransport->SendAcknowledgement( ACK_CANNOT_SUPPLY_NOT_PATH );
				TryInterruptState( 0 );
			}
		}			
		break;
	case ETRS_CREATE_SQUAD:
		CreateSquad();

		if ( pLoaderSquad )
			eState = ETRS_FINDING_UNIT_TO_SERVE;
		
		break;
	case ETRS_FINDING_UNIT_TO_SERVE:
		{
 			bool isNotEnoughRU = false;
			bool bFound = FindUnitToServe( &isNotEnoughRU );
			if ( !isNotEnoughRU )
			{
				UpdateActionBegin();
				CAITransportUnit::PrepareLoaders( pLoaderSquad, pTransport );
				SendLoaders();
				eState = ETRS_LOADERS_INROUTE;
			}
			else if ( isNotEnoughRU && pTransport->GetResursUnitsLeft() != SConsts::TRANSPORT_RU_CAPACITY )
			{
				eState = ETRS_GOING_TO_STORAGE;
			}
			/*else
			{
				UpdateActionBegin();
				timeLastUpdate = curTime;
				eState = ETRS_WAIT_FOR_UNIT_TO_SERVE;
			}*/
		}

		break;
	case ETRS_WAIT_FOR_UNIT_TO_SERVE:
		if ( curTime - timeLastUpdate > pTransport->GetBehUpdateDuration() )
			eState = ETRS_FINDING_UNIT_TO_SERVE;

		break;
	case ETRS_LOADERS_INROUTE:
		if ( curTime - timeLastUpdate < pTransport->GetBehUpdateDuration() )
		{
		}
		else 
		{
			timeLastUpdate = curTime;
			if ( !IsValidObj( pLoaderSquad ) || pLoaderSquad->IsInTransport() )
				eState = ETRS_CREATE_SQUAD;
		}			

		break;
	case ETRS_WAIT_FOR_LOADERS:
		if ( IsValidObj( pLoaderSquad ) )
		{
			if ( pLoaderSquad->IsInTransport() )
			{
				pTransport->Unlock();
				pLoaderSquad->Disappear();
				pTransport->SetCommandFinished();
			}
		}
		else
			pTransport->SetCommandFinished();
		
		break;
	}
}
void CTransportServeState::CreateSquad()
{
	if ( !pTransport || !pTransport->IsValid() || !pTransport->IsAlive() )
	{
		pLoaderSquad = 0;
		return;
	}

	pTransport->Unlock();
	if ( IsValidObj( pLoaderSquad ) )
		pLoaderSquad->Disappear();

	if ( pTransport->GetPlayer() == theDipl.GetNeutralPlayer() )
	{
		pLoaderSquad = 0;
		pTransport->SetCommandFinished();
	}
	else
	{
		pLoaderSquad = theUnitCreation.CreateResupplyEngineers( pTransport );
		pTransport->Lock( pLoaderSquad );
	}
}
ETryStateInterruptResult CTransportServeState::TryInterruptState( class CAICommand *pCommand )
{
	if ( pCommand && pTransport->IsValid() && pTransport->IsAlive() )
	{
		if ( IsValidObj( pLoaderSquad ) && !pLoaderSquad->IsInTransport() )
		{
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pTransport), pLoaderSquad, false );
		}

		pTransport->AddExternLoaders( pLoaderSquad );
		pTransport->Unlock();
		pTransport->SetCommandFinished();

		return TSIR_YES_IMMIDIATELY;
	}
	else
	{
		CAITransportUnit::FreeLoaders( pLoaderSquad, pTransport );
		pTransport->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
}
CTransportBuildState::CTransportBuildState ( class CAITransportUnit *pTransport, const class CVec2 & vDestPoint )
: eState( ETBS_ESTIMATE ), pUnit( pTransport ), vStartPoint( vDestPoint )
{
}
void CTransportBuildState::Segment()
{
	switch( eState )
	{
	case ETBS_ESTIMATE:
		if ( IsEndPointNeeded() )
			eState = ETBS_WAIT_FOR_ENDPOINT;
		else
			eState = ETBS_END_POINT_READY;

		break;
	case ETBS_END_POINT_READY:
		if ( IsWorkDone() )
		{
			pUnit->SendAcknowledgement( ACK_CANNOT_START_BUILD );
			pUnit->SetCommandFinished();
		}
		else if ( IsEnoughResources() )
			eState = ETBS_START_APPROACH;
		else
			eState = ETBS_LOADING_RESOURCES;

		break;
	case ETBS_WAIT_FOR_ENDPOINT:
		if ( pUnit->GetNextCommand() )
			pUnit->SetCommandFinished();

		break;
	case ETBS_START_APPROACH:
		if ( !HaveToSendEngeneersNow() )
		{
			SendTransportToBuildPoint();
			eState = ETBS_APROACHING_BUILDPOINT;
		}
		else
			eState = ETBS_CREATE_SQUAD;

		break;
	case ETBS_LOADING_RESOURCES:
		if ( !pLoadRuSubState )
			pLoadRuSubState = static_cast<CTransportLoadRuState*>(CTransportLoadRuState::Instance( pUnit, true ));
		pLoadRuSubState->Segment();
		if ( pLoadRuSubState->IsSubStateFinished() )
		{
			pLoadRuSubState = 0;
			if ( IsEnoughResources() )
				eState = ETBS_START_APPROACH;
			else
				TryInterruptState( 0 );
		}

		break;
	case ETBS_APROACHING_BUILDPOINT:
		if ( HaveToSendEngeneersNow() )
		{
			pUnit->StopUnit();
			eState = ETBS_CREATE_SQUAD;
		}
		else if ( pUnit->IsIdle() )
		{
			if ( MustSayNegative() )
				pUnit->SendAcknowledgement( ACK_NO_ENGINEERS_CANNOT_REACH_BUILDPOINT );
			pUnit->SetCommandFinished();
		}
		
		break;
	case ETBS_CREATE_SQUAD:
		{	
			pUnit->Unlock();
			if ( IsValidObj( pEngineers ) )
				pEngineers->Disappear();

			if ( pUnit->GetPlayer() == theDipl.GetNeutralPlayer() )
			{
				pEngineers = 0;
				pUnit->SetCommandFinished();
			}
			else
			{
				pEngineers = theUnitCreation.CreateResupplyEngineers( pUnit );
				pUnit->Lock( pEngineers );
			}
		}

		if ( pEngineers )
		{
			CAITransportUnit::PrepareLoaders( pEngineers, pUnit );
			SendEngineers();
			eState = ETBS_WAIT_FINISH_BUILD;
		}

		break;
	case ETBS_WAIT_FINISH_BUILD:
		if ( !IsValidObj( pEngineers ) || pEngineers->IsInTransport() )
		{
			pUnit->Unlock();
			if ( IsValidObj( pEngineers ) )
				pEngineers->Disappear();
			
			if ( IsWorkDone() )
				TryInterruptState( 0 );
			else if ( !IsEnoughResources() )
			{
				NotifyGoToStorage();
				eState = ETBS_LOADING_RESOURCES;
			}
			else
				eState = ETBS_CREATE_SQUAD;
		}
			
		break;
	case ETBS_WAIT_FOR_LOADERS:
		if ( IsValidObj( pEngineers ) )
		{
			if ( pEngineers->IsInTransport() )
			{
				pUnit->Unlock();
				pEngineers->Disappear();
				pUnit->SetCommandFinished();
			}
		}
		else
			pUnit->SetCommandFinished();

		break;
	}
}
void CTransportBuildState::SetEndPoint( const CVec2& _vEndPoint )
{
	NI_ASSERT_T( eState == ETBS_WAIT_FOR_ENDPOINT, "wrong states sequence" );
	vEndPoint = _vEndPoint;
	eState = ETBS_END_POINT_READY;
}
ETryStateInterruptResult CTransportBuildState::TryInterruptState( class CAICommand *pCommand )
{
	if ( pCommand && pUnit->IsValid() && pUnit->IsAlive() )
	{
		if ( IsValidObj( pEngineers ) && !pEngineers->IsInTransport() )
		{
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pUnit), pEngineers, false );
		}

		pUnit->AddExternLoaders( pEngineers );
		pUnit->Unlock();
		pUnit->SetCommandFinished();

		return TSIR_YES_IMMIDIATELY;
	}
	else
	{
		CAITransportUnit::FreeLoaders( pEngineers, pUnit );
		pUnit->SetCommandFinished();
		return TSIR_YES_IMMIDIATELY;
	}
}
void CTransportBuildLongObjectState::SetEndPoint( const CVec2& _vEndPoint )
{
	CTransportBuildState::SetEndPoint( _vEndPoint );
	pUnit->UnlockTiles();
	pCreation->PreCreate( vStartPoint, vEndPoint );
}
void CTransportBuildLongObjectState::SendTransportToBuildPoint()
{
	const float fDist = GetDistanceToSegment( vStartPoint, vEndPoint, pUnit->GetCenter() );
	const float fRadius = pUnit->GetStats()->vAABBHalfSize.x + pUnit->GetStats()->vAABBHalfSize.y;
	
	const float fMaxRadius = SConsts::TRANSPORT_RESUPPLY_OFFSET > fRadius ? SConsts::TRANSPORT_RESUPPLY_OFFSET : fRadius * 2;

	if ( fDist > fMaxRadius )
	{
		CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vStartPoint, VNULL2, pUnit, true );
		if ( pPath )
			pUnit->SendAlongPath( pPath, VNULL2 );
	}
	else if ( fDist <= fRadius )
	{
		CLine2 line( vStartPoint, vEndPoint );
		CVec2 vAway( line.a, line.b );
		Normalize( &vAway );
		int nSign = - line.GetSign( pUnit->GetCenter() );
		nSign = nSign != 0 ? nSign : 1;

		const CVec2 vTo = pUnit->GetCenter() - fMaxRadius * nSign * vAway;
		
		CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vTo, VNULL2, pUnit, true );
		if ( pPath )
			pUnit->SendAlongPath( pPath, VNULL2 );
	}
	else
	{
	}
}
bool CTransportBuildLongObjectState::HaveToSendEngeneersNow() 
{
	const float fDist = GetDistanceToSegment( vStartPoint, vEndPoint, pUnit->GetCenter() );
	const float fRadius = pUnit->GetStats()->vAABBHalfSize.x + pUnit->GetStats()->vAABBHalfSize.y;

	const float fMaxRadius = SConsts::TRANSPORT_RESUPPLY_OFFSET > fRadius ? SConsts::TRANSPORT_RESUPPLY_OFFSET : fRadius * 2;

	const CVec2 vCenter = pUnit->GetCenter();
	const bool bRes = fDist > fRadius && fDist <= fMaxRadius;
	if ( bRes )
	{
		const float fDist = GetDistanceToSegment( vStartPoint, vEndPoint, pUnit->GetCenter() );
	}
	return bRes;
}
bool CTransportBuildLongObjectState::IsEnoughResources() const
{
	return pUnit->GetResursUnitsLeft() > 0.0f; //pUnit->GetResursUnitsLeft() >= pCreation->GetPrice();
}
bool CTransportBuildLongObjectState::IsWorkDone() const
{
	return pCreation->GetCurIndex() == pCreation->GetMaxIndex() ||
				 !pCreation->CanBuildNext();
}
void CTransportBuildLongObjectState::SendEngineers()
{
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_BUILD_LONGOBJECT, pCreation), pEngineers, false );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_SET_HOME_TRANSPORT, pUnit ), pEngineers, true );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pUnit), pEngineers, true );
}
IUnitState* CTransportBuildFenceState::Instance( class CAITransportUnit *pTransport, const class CVec2 &vStartPoint )
{
	return new CTransportBuildFenceState( pTransport, vStartPoint );
}
CTransportBuildFenceState::CTransportBuildFenceState( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint )
: CTransportBuildLongObjectState( pTransport, vStartPoint, new CFenceCreation(pTransport->GetPlayer()) )
{
}
CTransportBuildEntrenchmentState::CTransportBuildEntrenchmentState( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint )
: CTransportBuildLongObjectState( pTransport, vStartPoint, new CEntrenchmentCreation(pTransport->GetPlayer()) )
{
}
IUnitState* CTransportBuildEntrenchmentState::Instance( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint )
{
	return new CTransportBuildEntrenchmentState( pTransport, vStartPoint );
}
CTransportClearMineState::CTransportClearMineState( class CAITransportUnit *pTransport, const class CVec2 & vDestPoint )
	: CTransportBuildState( pTransport, vDestPoint ), timeLastCheck( curTime ), bWorkDone( false )
{  
	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats *>(pTransport->GetStats());
	timeCheckPeriod = SConsts::MINE_VIS_RADIUS / pStats->fSpeed;
}
void CTransportClearMineState::SendTransportToBuildPoint()
{
	CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vStartPoint, VNULL2, pUnit, true );
	if ( pPath )
		pUnit->SendAlongPath( pPath, VNULL2 );
}
bool CTransportClearMineState::HaveToSendEngeneersNow() 
{
	if ( pUnit->IsIdle() ) // а когда останжовились, то найти хоть какую-нибудь мину
	{
		const CVec2 vClearCenter( pUnit->GetCenter() );
		for ( CStObjCircleIter<false> iter( vClearCenter, SConsts::MINE_CLEAR_RADIUS );
					!iter.IsFinished(); iter.Iterate() )
		{
			if ( (*iter)->GetObjectType() == ESOT_MINE &&
					!static_cast<CMineStaticObject*>(*iter)->IsBeingDisarmed() && 
					fabs2( (*iter)->GetCenter() - vClearCenter ) <= sqr( float(SConsts::MINE_CLEAR_RADIUS) ) )
				return true;
		}
	}
	else if ( curTime >= timeLastCheck  + timeCheckPeriod )
	{
		timeLastCheck = curTime;
		const int nParty = pUnit->GetParty();
 		for ( CMinesIter iter( pUnit->GetCenter(), SConsts::MINE_VIS_RADIUS, nParty, true ); !iter.IsFinished(); iter.Iterate() )
		{
			if ( (*iter)->WillExplodeUnder( pUnit ) ) 
				return true;
		}
	}
	return false;
}
bool CTransportClearMineState::IsEnoughResources() const
{
	return true;
}
bool CTransportClearMineState::IsWorkDone() const
{
	return bWorkDone;
}
void CTransportClearMineState::SendEngineers()
{
	bWorkDone = true;
	const CVec2 vUnitCenter( pUnit->GetCenter() );
	CVec2 vDir = vStartPoint - vUnitCenter;
	const float fMaxDist = fabs2( vDir );
	Normalize( &vDir );

	bool bToQueue = false;
	for ( CVec2 vCurPoint = vUnitCenter + vDir * SConsts::MINE_CLEAR_RADIUS;
				fabs2( vCurPoint - vUnitCenter ) < fMaxDist; 
				vCurPoint += vDir * SConsts::MINE_CLEAR_RADIUS )
	{
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CLEARMINE, vCurPoint), pEngineers, bToQueue );	
		bToQueue = true;
	}

	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CLEARMINE, vStartPoint ), pEngineers, bToQueue );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pUnit), pEngineers, true );
}
bool CTransportClearMineState::IsEndPointNeeded() const
{
	return false;
}
IUnitState* CTransportClearMineState::Instance( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint )
{
	return new CTransportClearMineState( pTransport, vStartPoint );
}
void CTransportPlaceMineState::SendTransportToBuildPoint()
{
	CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vStartPoint, VNULL2, pUnit, true );
	if ( pPath )
		pUnit->SendAlongPath( pPath, VNULL2 );
	bTransportSent = true;
}
bool CTransportPlaceMineState::HaveToSendEngeneersNow() 
{
	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>( pUnit->GetStats() );

	if ( pUnit->IsIdle() && fabs2(pUnit->GetCenter() - vStartPoint) <= sqr(SConsts::TRANSPORT_RESUPPLY_OFFSET * 2) )
		return true;

	if ( !pUnit->IsIdle() && fabs2(pUnit->GetCenter() - vStartPoint) <= sqr(SConsts::TRANSPORT_RESUPPLY_OFFSET) )
		return true;

	if ( bTransportSent && pUnit->IsIdle() )
		return true;
	return false;
}
bool CTransportPlaceMineState::IsEnoughResources() const
{
	return pUnit->GetResursUnitsLeft() >= SConsts::MINE_RU_PRICE[nNumber];
}
bool CTransportPlaceMineState::IsWorkDone() const
{
	return bWorkDone;
}
void CTransportPlaceMineState::SendEngineers()
{
	bWorkDone = true;
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_PLACEMINE, vStartPoint, nNumber), pEngineers, false );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_SET_HOME_TRANSPORT, pUnit), pEngineers, true );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pUnit), pEngineers, true );
}
bool CTransportPlaceMineState::IsEndPointNeeded() const
{
	return false;
}
IUnitState* CTransportPlaceMineState::Instance( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint, const float fNumber )
{
	return new CTransportPlaceMineState( pTransport, vStartPoint, fNumber );
}
IUnitState* CTransportPlaceAntitankState::Instance( class CAITransportUnit *pTransport, const class CVec2 & vStartPoint )
{
	return new CTransportPlaceAntitankState( pTransport, vStartPoint );
}
CTransportPlaceAntitankState::CTransportPlaceAntitankState( class CAITransportUnit *pTransport, const class CVec2 & vDestPoint )
	: CTransportBuildState( pTransport, vDestPoint ), bWorkFinished ( false ), bSent( false )
{
}
void CTransportPlaceAntitankState::SendTransportToBuildPoint()
{
	CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vStartPoint, VNULL2, pUnit, true );
	if ( pPath )
		pUnit->SendAlongPath( pPath, VNULL2 );
	bSent = true;
}
bool CTransportPlaceAntitankState::HaveToSendEngeneersNow() 
{
	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>( pUnit->GetStats() );
	if ( pUnit->IsIdle() && fabs2(pUnit->GetCenter() - vStartPoint) <= sqr(SConsts::TRANSPORT_RESUPPLY_OFFSET * 2) )
		return true;

	if ( !pUnit->IsIdle() && fabs2(pUnit->GetCenter() - vStartPoint) <= sqr(SConsts::TRANSPORT_RESUPPLY_OFFSET) )
		return true;

	if ( bSent && pUnit->IsIdle() )
		return true;
	return false;
}
bool CTransportPlaceAntitankState::IsEnoughResources() const
{
	return pUnit->GetResursUnitsLeft() >= SConsts::ANTITANK_RU_PRICE;
}
bool CTransportPlaceAntitankState::IsWorkDone() const
{
	return bWorkFinished;
}
void CTransportPlaceAntitankState::SendEngineers()
{
	bWorkFinished = true;
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_PLACE_ANTITANK, vStartPoint), pEngineers, false );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_SET_HOME_TRANSPORT, pUnit), pEngineers, true );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pUnit), pEngineers, true );
}
bool CTransportPlaceAntitankState::IsEndPointNeeded() const
{
	return false;
}
IUnitState* CTransportRepairBridgeState::Instance( class CAITransportUnit *pTransport, class CFullBridge *pFullBridge )
{
	return new CTransportRepairBridgeState( pTransport, pFullBridge );
}
CTransportRepairBridgeState::CTransportRepairBridgeState( class CAITransportUnit *pTransport, class CFullBridge *pFullBridge )
: CTransportBuildState( pTransport, VNULL2 ), pBridgeToRepair( pFullBridge ), bSentToBuildPoint( false )
{
	std::vector< CObj<CBridgeSpan> > spans;
	pBridgeToRepair->EnumSpans( &spans );
	SetStartPoint( CBridgeCreation::SortBridgeSpans( &spans, pTransport ) );
}
void CTransportRepairBridgeState::SendTransportToBuildPoint()
{
	CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vStartPoint, VNULL2, pUnit, true );
	bSentToBuildPoint = true;
	if ( pPath )
		pUnit->SendAlongPath( pPath, VNULL2 );
}
bool CTransportRepairBridgeState::HaveToSendEngeneersNow() 
{
	return bSentToBuildPoint && pUnit->IsIdle();
}
bool CTransportRepairBridgeState::IsEnoughResources() const
{
	return 1.0f < pUnit->GetResursUnitsLeft();
}
bool CTransportRepairBridgeState::IsWorkDone() const
{
	return pBridgeToRepair->GetHPPercent() == 1.0f;
}
void CTransportRepairBridgeState::SendEngineers()
{
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_REPAIR_BRIDGE, pBridgeToRepair ), pEngineers, false );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_SET_HOME_TRANSPORT, pUnit), pEngineers, true );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pUnit), pEngineers, true );
}
bool CTransportRepairBridgeState::IsEndPointNeeded() const 
{
	return false;
}
IUnitState* CTransportBuildBridgeState::Instance( class CAITransportUnit *pTransport, class CFullBridge *pFullBridge )
{
	return new CTransportBuildBridgeState( pTransport, pFullBridge );
}
CTransportBuildBridgeState::CTransportBuildBridgeState( class CAITransportUnit *_pTransport, class CFullBridge *_pFullBridge )
: CTransportBuildState( _pTransport, VNULL2 ), pFullBridge( _pFullBridge ), pCreation( new CBridgeCreation(_pFullBridge, _pTransport ) ),
	bTransportSent( false )
{
	SetStartPoint( pCreation->GetStartPoint() );
}
void CTransportBuildBridgeState::SendTransportToBuildPoint()
{
	CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vStartPoint, VNULL2, pUnit, true );

	bTransportSent = true;
	if ( pPath )
		pUnit->SendAlongPath( pPath, VNULL2 );
}
bool CTransportBuildBridgeState::HaveToSendEngeneersNow() 
{
	return bTransportSent && pUnit->IsIdle();
}
bool CTransportBuildBridgeState::IsEnoughResources() const
{
	return pUnit->GetResursUnitsLeft() > 0.0f;//return pCreation->GetPrice() <= pUnit->GetResursUnitsLeft();
}
bool CTransportBuildBridgeState::IsWorkDone() const
{
	return pCreation->GetCurIndex() >= pCreation->GetMaxIndex();
}
void CTransportBuildBridgeState::SendEngineers()
{
	bool bToQueue = false;
	if ( pCreation->IsFirstSegmentBuilt() )
	{
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_REPAIR_BRIDGE, pFullBridge ), pEngineers, false );
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_SET_HOME_TRANSPORT, pUnit), pEngineers, true );
		bToQueue = true;
	}
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_BUILD_LONGOBJECT, pCreation ), pEngineers, bToQueue );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_SET_HOME_TRANSPORT, pUnit), pEngineers, true );
	
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pUnit), pEngineers, true );
}
bool CTransportBuildBridgeState::IsEndPointNeeded() const 
{
	return false;
}
IUnitState* CTransportRepairBuildingState::Instance( class CAITransportUnit *pTransport, class CBuilding *pStaticObject )
{
	return new CTransportRepairBuildingState( pTransport, pStaticObject );
}
CTransportRepairBuildingState::CTransportRepairBuildingState( class CAITransportUnit *pTransport, class CBuilding *pStaticObject )
: CTransportBuildState( pTransport, pStaticObject->GetCenter() ), pBuilding( pStaticObject ), bSentToBuildPoint( false )
{
}
void CTransportRepairBuildingState::SendTransportToBuildPoint()
{
	CPtr<IStaticPath> pPath = CreateStaticPathToPoint( vStartPoint, VNULL2, pUnit, true );
	bSentToBuildPoint = true;
	if ( pPath )
		pUnit->SendAlongPath( pPath, VNULL2 );
}
bool CTransportRepairBuildingState::HaveToSendEngeneersNow() 
{
	const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>( pUnit->GetStats() );
	if ( bSentToBuildPoint && pUnit->IsIdle() || fabs2(pUnit->GetCenter() - vStartPoint) <= sqr(pStats->vAABBHalfSize.y * 2) )
		return true;
	return false;
}
bool CTransportRepairBuildingState::IsEnoughResources() const
{
	return pBuilding->GetStats()->fRepairCost <= pUnit->GetResursUnitsLeft();
}
bool CTransportRepairBuildingState::IsWorkDone() const
{
	return pBuilding->GetHitPoints() == pBuilding->GetStats()->fMaxHP;
}
void CTransportRepairBuildingState::SendEngineers()
{
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_REPAIR_BUILDING, pBuilding ), pEngineers, false );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_SET_HOME_TRANSPORT, pUnit), pEngineers, true );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_CATCH_TRANSPORT, pUnit), pEngineers, true );
}
bool CTransportRepairBuildingState::IsEndPointNeeded() const 
{
	return false;
}
