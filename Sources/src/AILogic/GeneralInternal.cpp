#include "stdafx.h"

#include "General.h"
#include "GeneralHelper.h"
#include "GeneralInternal.h"
#include "AIUnit.h"
#include "Formation.h"
#include "Diplomacy.h"
#include "UnitsIterators2.h"
#include "GroupLogic.h"
#include "Guns.h"
#include "Soldier.h"
#include "Formation.h"
#include "GeneralTasks.h"
#include "UnitStates.h"
#include "Commands.h"
#include "AILogicInternal.h"
#include "GeneralAirForce.h"
#include "GeneralArtillery.h"
#include "Aviation.h"
#include "AIUnitInfoForGeneral.h"
#include "Technics.h"
#include "GeneralIntendant.h"
#include "GeneralConsts.h"
#include "TimeCounter.h"
#include "UnitCreation.h"

#include "..\Scene\Scene.h"
#include "..\Formats\fmtMap.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CTimeCounter timeCounter;
extern CAILogic *pAILogic;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CGroupLogic theGroupLogic;
extern CUnitCreation theUnitCreation;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CGeneral );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CGeneral															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::EnumWorkers( const enum EForceType eType, IWorkerEnumerator *pEnumerator )
{
	//search trough reserve to give it to the task
	switch( eType )
	{
	case FT_INFANTRY_IN_TRENCHES:
		EnumWorkersInternal( eType, pEnumerator, &infantryInTrenches );

		break;
	case FT_FREE_INFANTRY:
		EnumWorkersInternal( eType, pEnumerator, &infantryFree );

		break;
	case FT_SWARMING_TANKS:
		EnumWorkersInternal( eType, pEnumerator, &tanksFree );

		break;
	case FT_MOBILE_TANKS:
		EnumWorkersInternal( eType, pEnumerator, &tanksFree );

		break;
	case FT_STATIONARY_MECH_UNITS:
		EnumWorkersInternal( eType, pEnumerator, &stationaryTanks );

		break;
	case FT_TRUCK_REPAIR_BUILDING:
		EnumWorkersInternal( eType, pEnumerator, &transportsFree );
		break;

	default:
		NI_ASSERT_T( false, NStr::Format( "wrong type asked from commander %d", eType ) );
		break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::CancelRequest( int nRequestID, enum EForceType eType ) 
{  
	switch( eType )
	{
	case FT_AIR_SCOUT:
	case FT_AIR_GUNPLANE:
		pAirForce->CancelRequest( nRequestID, eType );
		break;
	case FT_RECAPTURE_STORAGE:
		{
			//find desired task
			RequestedTasks::iterator it = requestedTasks.find( nRequestID );
			if ( it != requestedTasks.end() )
			{
				IGeneralTask * pTask = requestedTasks[nRequestID];
				pTask->CancelTask( this );
				requestIDs.AddToFreeId( nRequestID );
			}
		}
		break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int /*request ID*/CGeneral::RequestForSupport( const CVec2 &vSupportCenter, enum EForceType eType ) 
{ 
	switch( eType )
	{
	case FT_AIR_SCOUT:
	case FT_AIR_GUNPLANE:
		return pAirForce->RequestForSupport( vSupportCenter, eType );
		break;
	case FT_RECAPTURE_STORAGE:
		{
			const int nID = requestIDs.GetFreeId();
			CGeneralTaskRecaptureStorage * pTask = new CGeneralTaskRecaptureStorage( vSupportCenter );
			requestedTasks[nID] = pTask;
			tasks.push_back( pTask );

			return nID;
		}
		break;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::Give( CCommonUnit *pWorker )
{
	if ( !pWorker || !pWorker->IsValid() || !pWorker->IsAlive() ) 
		return;

	if ( pWorker->IsFormation() )
	{
		CFormation * pFormation = static_cast<CFormation*>( pWorker );
		if ( pFormation->GetState()->GetName() == EUSN_GUN_CAPTURE )
		{
			// don't consider gunners as infantry. they are pictures.
		}
		else if ( pFormation->IsInEntrenchment() || 
					( pFormation->GetNextCommand() && 
						( ACTION_COMMAND_IDLE_TRENCH == pFormation->GetNextCommand()->ToUnitCmd().cmdType ||
							ACTION_COMMAND_IDLE_BUILDING == pFormation->GetNextCommand()->ToUnitCmd().cmdType ||
							ACTION_COMMAND_ENTER == pFormation->GetNextCommand()->ToUnitCmd().cmdType
						)
					)
				)
		{
			infantryInTrenches.push_back( static_cast<CFormation*>(pWorker) );
		}
		else
			infantryFree.push_back( static_cast<CFormation*>(pWorker) );
	}
	else
	{
		NI_ASSERT_T( dynamic_cast<CAIUnit*>(pWorker) != 0, "Wrong unit passed" );
		CAIUnit *pUnit = static_cast<CAIUnit*>(pWorker);

		// ������������ ������
		if ( pUnit->GetFirstArtilleryGun() != 0 )
		{
			if ( pUnit->GetStats()->IsArtillery() || pUnit->GetStats()->IsSPG() || pUnit->GetStats()->type == RPG_TYPE_TRAIN_SUPER )
				pGeneralArtillery->TakeArtillery( pUnit );
		}
		else
		{
			const SUnitBaseRPGStats *pStats = pUnit->GetStats();
			if ( pStats->IsTransport() )
			{
				if ( IsMobileReinforcement( pAILogic->GetScriptID( pWorker ) ) )
				{
					if ( pStats->type == RPG_TYPE_TRN_CIVILIAN_AUTO ) 
					{
						//����� ����� ����� �� ����� ��������
					}
					else if ( pStats->type == RPG_TYPE_TRN_MILITARY_AUTO )
					{
						pIntendant->AddReiforcePosition( pUnit->GetCenter(), pUnit->GetDir() );
						pIntendant->Give( pUnit );
					}
					else
					{
						CAITransportUnit * pTransport = static_cast<CAITransportUnit*>( pUnit );
						if ( !pTransport->IsMustTow() )
						{
							pIntendant->AddReiforcePosition( pUnit->GetCenter(), pUnit->GetDir() );
							pIntendant->Give( pWorker );
						}
						else
						{
							pGeneralArtillery->TakeTruck( pUnit );
						}
					}
				}
			}
			else if ( IsMobileReinforcement( pAILogic->GetScriptID( pWorker ) ) )
				tanksFree.push_back( pWorker );
			else 
				stationaryTanks.push_back( pWorker );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::EraseLastSeen()
{
	int nStep = 10; // optimisation parameter;

	// if finished, start again
	if ( curProcessed == enemys.end() ) 
		curProcessed = enemys.begin();

	// ������ ���� ������, ������� ������ �����.
	for ( ; curProcessed != enemys.end() && nStep > 0; ++curProcessed )
	{
		--nStep;
		const CUnitTimeSeen &unitTimeSeen = curProcessed->second;
		if ( unitTimeSeen.second != -1 && curTime - unitTimeSeen.second > SGeneralConsts::TIME_DONT_SEE_ENEMY_BEFORE_FORGET )
			erased.push_back( curProcessed->first );
	}
	while ( !erased.empty() )
	{
		enemys.erase( *erased.begin() );
		erased.pop_front();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::Segment()
{
	/*RegisterCounter( 64, "general_removedead" );
	RegisterCounter( 65, "general_removeenemies" );
	RegisterCounter( 66, "general_commander" );
	RegisterCounter( 67, "general_artillery" );
	RegisterCounter( 68, "general_intendant" );*/

	if ( curTime > timeNextUpdate )
	{
		timeCounter.Count( 64, true );
		SGeneralHelper::RemoveDead( &infantryInTrenches );
		SGeneralHelper::RemoveDead( &infantryFree );
		SGeneralHelper::RemoveDead( &tanksFree );
		SGeneralHelper::RemoveDead( &stationaryTanks );
		SGeneralHelper::RemoveDead( &transportsFree );
		timeCounter.Count( 64, false );

		//CRAP{ �������������, ���� ����� ���������, �� �������� �� ���������
		timeCounter.Count( 65, true );
		EraseLastSeen();
		timeCounter.Count( 65, false );

		timeCounter.Count( 66, true );
		CCommander::Segment();
		timeCounter.Count( 66, false );
		
		pAirForce->Segment();

		timeCounter.Count( 67, true );
		// �������������� ��������
		BombardmentSegment();
		pGeneralArtillery->Segment();
		timeCounter.Count( 67, false );

		timeCounter.Count( 68, true );
		if ( pIntendant )
			pIntendant->Segment();
		timeCounter.Count( 68, false );
		//CRAP}

		timeNextUpdate = curTime + SGeneralConsts::GENERAL_UPDATE_PERIOD + Random( 1000 );
		
		GetSingleton<IScene>()->GetStatSystem()->UpdateEntry( "General: visible enemies", "" );
		GetSingleton<IScene>()->GetStatSystem()->UpdateEntry( "General: antiartillery circles", "" );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::GiveCommandToBombardment()
{
	if ( 2 == cBombardmentType ) return;
	
	const float fComparativeWeight = 
		cBombardmentType == 1 ? SGeneralConsts::MIN_WEIGHT_TO_SEND_BOMBERS : SGeneralConsts::MIN_WEIGHT_TO_ARTILLERY_FIRE;
	// ���������� ��������, ����� ������� ������ ������
	int cnt = 0;
	CResistancesContainer::iterator iter = resContainer.begin();
	while ( cnt < 10 && !iter.IsFinished() && (*iter).GetWeight() >= fComparativeWeight )
	{
		const CVec2 vCellCenter = SResistance::GetResistanceCellCenter( (*iter).GetCellNumber() );
		if ( cBombardmentType == 1 || pGeneralArtillery->CanBombardRegion( vCellCenter ) )
			++cnt;

		iter.Iterate();
	}

	// ����� �������
	if ( cnt > 0 )
	{
		const int nRegion = Random( 1, cnt );
		
		cnt = 0;
		iter = resContainer.begin();
		while ( cnt < nRegion )
		{
			const CVec2 vCellCenter = SResistance::GetResistanceCellCenter( (*iter).GetCellNumber() );
			if ( cBombardmentType == 1 || pGeneralArtillery->CanBombardRegion( vCellCenter ) )
				++cnt;
			if ( cnt < nRegion )
				iter.Iterate();
		}

		const CVec2 vCellCenter = SResistance::GetResistanceCellCenter( (*iter).GetCellNumber() );
		const float fRadius = SGeneralConsts::RESISTANCE_CELL_SIZE * SConsts::TILE_SIZE + Random( 0, 2 ) * SConsts::TILE_SIZE;

		CVec2 vCenter( VNULL2 );
		int nUnits = 0;
		bool bIsAntiArtilleryFight = false;
		for ( CUnitsIter<0,3> unitsIter( nParty, EDI_ENEMY, vCellCenter, fRadius ); !unitsIter.IsFinished(); unitsIter.Iterate() )
		{
			CAIUnit *pUnit = *unitsIter;
			CAIUnitInfoForGeneral* pInfo = pUnit->GetUnitInfoForGeneral();

			// ���� ������� �������� ��� ���� ��� ���� � ��� ������ �� ��� �����
			if ( cBombardmentType == 1 || curTime - pInfo->GetLastTimeOfVisibility() < pUnit->GetTimeToForget() )
			{
				if ( pInfo->IsLastVisibleAntiArt() )
					bIsAntiArtilleryFight = true;
				
				vCenter += pUnit->GetCenter();
				++nUnits;
			}
		}

		if ( nUnits > 0 )
		{
			vCenter /= float( nUnits );

			if ( cBombardmentType == 1 )
				pAirForce->RequestForSupport( vCenter, FT_AIR_BOMBER, (*iter).GetCellNumber() );
			else	
			{
				float fMaxDistance = 0;
				for ( CUnitsIter<0,3> unitsIter( nParty, EDI_ENEMY, vCellCenter, fRadius ); !unitsIter.IsFinished(); unitsIter.Iterate() )
				{
					CAIUnit *pUnit = *unitsIter;
					CAIUnitInfoForGeneral* pInfo = pUnit->GetUnitInfoForGeneral();

					// ���� ��� ���� � ��� ������ �� ��� �����		
					if ( curTime - pInfo->GetLastTimeOfVisibility() < pUnit->GetTimeToForget() )
					{
						const float fDist = fabs( pUnit->GetCenter() - vCenter );
						if ( fDist > fMaxDistance )
							fMaxDistance = fDist;

						// "������" � ��������� ������ � ���� �������
						if ( !pUnit->IsVisible( nParty ) )
							pUnit->SetLastVisibleTime( 0 );
					}
				}

				pGeneralArtillery->RequestForSupport( vCenter, fMaxDistance, bIsAntiArtilleryFight, (*iter).GetCellNumber() );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::GiveResistances( IEnemyEnumerator *pEnumerator )
{
	if ( bSendReserves )
	{
		CResistancesContainer::iterator iter = resContainer.begin();
		while ( !iter.IsFinished() && (*iter).GetWeight() > SGeneralConsts::MIN_WEIGHT_TO_SEND_SWARM && pEnumerator->EnumResistances( *iter ) )
			iter.Iterate();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float Func( const float fX, const float fBound )
{
	//	return ( 1 - exp( -fX * 0.3 ) ) * fBound;

	// x <= f  -->  exp( k * ( x - f ) )
	// x > f   -->  2 - exp( -k * ( x - f ) )

	// ����� ����� - �������� �����������
	const float f = 5.0f;
	// ���� ����������� � ����� f
	const float k = 2.0f;

	float func;
	if ( fX <= f )
		func = exp( k * ( fX - f ) );
	else
		func = 2 - exp( -k * ( fX - f ) );

	// ����� ������� � ���� � ���� � � fBound � �������������
	func = ( func - exp( -k*f ) ) / ( 2 - exp( -k*f ) ) * fBound;

	return func;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::BombardmentSegment()
{
	CResistancesContainer::iterator iter = resContainer.begin();
	if ( !iter.IsFinished() )
	{
		if ( curTime >= lastBombardmentCheck + 1000 )
		{
			const float fComparativeWeight = SGeneralConsts::MIN_WEIGHT_TO_ARTILLERY_FIRE;

			const SResistance &maxCell = *iter;

			const float fRatio = maxCell.GetWeight() / fComparativeWeight;
			bSendReserves = maxCell.GetWeight() >= SGeneralConsts::MIN_WEIGHT_TO_SEND_SWARM;
			// ����� ���������� �� ��������� �� ��������
			if ( fRatio >= 1.0f )
			{
				// ����������� ���������� �� ���������� �������� ������� ( TIME_TO_ARTILLERY_FIRE )
				const float fProbability = 
					Func( fRatio, 1 - SGeneralConsts::PROBABILITY_TO_SHOOT_AFTER_ARTILLERY_FIRE )
					+ 
					SGeneralConsts::PROBABILITY_TO_SHOOT_AFTER_ARTILLERY_FIRE;

				// ������� �� 1000, �.�. �������� ��� � �������
				const float fProbNow = 1 - exp( 1.0f/((float)SGeneralConsts::TIME_TO_ARTILLERY_FIRE / 1000.0f) * log( 1 - fProbability ) );
				
				// ����� ��������
				if ( Random( 0.0f, 1.0f ) < fProbNow )
				{
					lastBombardmentCheck = curTime;
					
					// ������� ����������������
					const bool bSendBombers = 
						theUnitCreation.IsAviaEnabled( nParty, SUCAviation::AT_BOMBER ) &&
						maxCell.GetWeight() >= SGeneralConsts::MIN_WEIGHT_TO_SEND_BOMBERS;

					if ( bSendBombers )
						cBombardmentType = 1;
					else
						cBombardmentType = 0;

					lastBombardmentCheck = curTime + 1000;
					GiveCommandToBombardment();

					if ( cBombardmentType == 1 )
					{
						cBombardmentType = 0;
						GiveCommandToBombardment();
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGeneral::IsMobileReinforcement( int nGroupID ) const
{
	return mobileReinforcementGroupIDs.find( nGroupID ) != mobileReinforcementGroupIDs.end();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::GiveNewUnits( const std::list<CCommonUnit*> &pUnits )
{
	typedef std::unordered_map<int, bool> Formations;
	Formations formations; // ����������� ��������
	
	// ������� ��� �����, ����� ������. ������ �������� ����������	
	for ( std::list<CCommonUnit*>::const_iterator iter = pUnits.begin(); iter != pUnits.end(); ++iter )
	{
		CCommonUnit *pUnit = *iter;
		if ( theDipl.GetDiplStatusForParties( pUnit->GetParty(), nParty ) == EDI_FRIEND )
		{
			if ( pUnit->IsInFormation() &&
					 pUnit->GetFormation()->GetState()->GetName() != EUSN_GUN_CREW_STATE )
				formations[pUnit->GetFormation()->GetUniqueId()] = true;
			else
				Give( pUnit );
		}
	}

	// ������� ��������
	for( Formations::iterator it = formations.begin(); it != formations.end(); ++it )
	{
		if ( CCommonUnit *pUnit = GetObjectByUniqueIdSafe<CCommonUnit>( it->first ) )
			Give( pUnit );
	}
	
	// another oppotunity to ask for worker for tasks
	for ( Tasks::iterator it = tasks.begin(); it != tasks.end(); ++it )
		(*it)->AskForWorker( this, 0, true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::Init()
{
	bSendReserves = false;
	SGeneralConsts::Init();
	pAirForce = new CGeneralAirForce( nParty, this );
	pGeneralArtillery = new CGeneralArtillery( nParty, this );
	pIntendant = new CGeneralIntendant( nParty, this );
	pIntendant->Init();

	tasks.push_back( new CGeneralTaskToSwarmToPoint( this, this ) );

	curProcessed = enemys.begin();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::Init( const SAIGeneralSideInfo &mapInfo )
{
	// ������� ������ ��������� �������
	for ( int i = 0; i < mapInfo.mobileScriptIDs.size(); ++i )
		mobileReinforcementGroupIDs.insert( mapInfo.mobileScriptIDs[i] );

	Init();
	
	bool bReinforceCreated = false;

	for ( int i = 0; i < mapInfo.parcels.size(); ++i )
	{
		switch( mapInfo.parcels[i].eType )
		{
		case SAIGeneralParcelInfo::EPATCH_DEFENCE:								// ������ ������ 
			{
				CGeneralTaskToDefendPatch * pTask  = new CGeneralTaskToDefendPatch;
				pTask->Init( mapInfo.parcels[i], this );
				pTask->AskForWorker( this, 0, true );
				tasks.push_back( pTask );
				pTask->SetEnemyConatiner( this );
			}
			break;
		case SAIGeneralParcelInfo::EPATCH_REINFORCE:
			{
				bReinforceCreated = true;
				// ��������������� ���������
				CGeneralTaskToHoldReinforcement * pTask = new CGeneralTaskToHoldReinforcement;
				pTask->Init( mapInfo.parcels[i] );
				// notify Intendant about points to hold reinforcement.
				pIntendant->AddReiforcePositions( mapInfo.parcels[i] );
				pTask->AskForWorker( this, 0, true );
				tasks.push_back( pTask );
				pTask->SetEnemyConatiner( this );
			}
			break;
		}
	}
	// there is no reinforcement positions. create one, MUST HAVE at leas one
	if ( !bReinforceCreated )
	{
		// ��������������� ���������
		CGeneralTaskToHoldReinforcement * pTask = new CGeneralTaskToHoldReinforcement;
		pTask->Init();
		pTask->AskForWorker( this, 0, true );
		tasks.push_back( pTask );
		pTask->SetEnemyConatiner( this );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::SetAAVisible( class CAIUnit *pUnit, const bool bVisible )
{
	pAirForce->SetAAVisible( pUnit, bVisible );
	enemys[pUnit->GetUniqueId()] = CUnitTimeSeen( pUnit, bVisible ? -1 : curTime );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::SetUnitVisible( class CAIUnit *pUnit, const bool bVisible )
{
	const SUnitBaseRPGStats * pStats = pUnit->GetStats();
	if ( EDI_ENEMY == theDipl.GetDiplStatusForParties( pUnit->GetParty(), nParty ) )
	{	
		if ( !pStats->IsAviation() )
		{
			if ( pStats->IsArtillery() )
			{
				// invisible artillery is cannot be supplied

			}
			if ( pUnit->CanShootToPlanes() )
				SetAAVisible( pUnit, bVisible );
			else
			{
				NI_ASSERT_T( pUnit->GetUniqueId(), "unit has zero unique ID" );
				enemys[pUnit->GetUniqueId()] = CUnitTimeSeen(pUnit, bVisible ? -1 : curTime );
			}
		}
	}
	if ( pUnit->GetParty() == theDipl.GetNeutralParty() && pStats->IsArtillery() )
	{
		pIntendant->SetArtilleryVisible( pUnit, bVisible );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::RemoveResistance( const CVec2 &vCenter )
{
	resContainer.RemoveExcluded( vCenter );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::AddResistance( const CVec2 &vCenter, const float fRadius )
{
	resContainer.AddExcluded( vCenter, fRadius );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::GiveEnemies( IEnemyEnumerator *pEnumerator )
{
	for ( CEnemyVisibility::iterator it = enemys.begin();
				it != enemys.end() && pEnumerator->EnumEnemy( it->second.first ); ++it );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::UpdateEnemyUnitInfo( CAIUnitInfoForGeneral *pInfo,
	const NTimer::STime lastVisibleTimeDelta, const CVec2 &vLastVisiblePos,
	const NTimer::STime lastAntiArtTimeDelta, const CVec2 &vLastVisibleAntiArtCenter, const float fDistToLastVisibleAntiArt )
{
	resContainer.UpdateEnemyUnitInfo(
		pInfo, lastVisibleTimeDelta, vLastVisiblePos,	lastAntiArtTimeDelta, vLastVisibleAntiArtCenter, fDistToLastVisibleAntiArt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::UnitDied( class CCommonUnit * pUnit )
{
	pIntendant->UnitDead( pUnit );
	CEnemyVisibility::iterator it = enemys.find( pUnit->GetUniqueId() );
	if ( it != enemys.end() )
	{
		if ( curProcessed == it )
			++curProcessed;
		enemys.erase( it );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::UnitDied( CAIUnitInfoForGeneral *pInfo )
{
	UnitDied( pInfo->GetOwner() );

	if ( EDI_ENEMY == theDipl.GetDiplStatusForParties( pInfo->GetOwner()->GetParty(), nParty ) )
	{
		resContainer.UnitDied( pInfo );
		pAirForce->DeleteAA( pInfo->GetOwner() );
	}
	else
	{
		CAIUnit *pUnit = pInfo->GetOwner();
		CAIUnit *pTruck = pUnit->GetTruck();
		if ( pTruck && pTruck->IsValid() && pTruck->IsAlive() )
			Give( pTruck );			
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::UnitChangedPosition( class CCommonUnit * pUnit, const CVec2 &vNewPos )
{
	if ( pUnit->IsFormation() || !static_cast<CAIUnit*>( pUnit )->GetStats()->IsAviation() )
	{
		pIntendant->UnitChangedPosition( pUnit, vNewPos );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::UnitAskedForResupply( class CCommonUnit * pUnit, const EResupplyType eType, const bool bSet )
{
	pIntendant->UnitAskedForResupply( pUnit, eType, bSet );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::UnitChangedParty( CAIUnit *pUnit, const int nNewParty )
{
	switch ( theDipl.GetDiplStatusForParties( nNewParty, nParty ) )
	{
	case EDI_ENEMY:
		{
			resContainer.UnitChangedParty( pUnit->GetUnitInfoForGeneral() );

			pAirForce->DeleteAA( pUnit );
			if ( !pUnit->GetStats()->IsAviation() )
				pIntendant->UnitDead( pUnit );
		}

		break;
	case EDI_NEUTRAL:
		{
			const SUnitBaseRPGStats * pStats = pUnit->GetStats();
			if ( !pStats->IsAviation() && !pStats->IsArtillery() )
				pIntendant->UnitDead( pUnit );
		}

		break;
	case EDI_FRIEND:
		if ( !pUnit->GetStats()->IsAviation() )
			pIntendant->UnitDead( pUnit );
		
		break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::ReserveAviationForTimes( const std::vector<NTimer::STime> &times )
{
	pAirForce->ReserveAviationForTimes( times );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeneral::SetCellInUse( const int nResistanceCellNumber, bool bInUse )
{
	resContainer.SetCellInUse( nResistanceCellNumber, bInUse );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGeneral::IsInResistanceCircle( const CVec2 &vPoint ) const
{
	return resContainer.IsInResistanceCircle( vPoint );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
