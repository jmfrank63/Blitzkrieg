#include "StdAfx.h"

#include "Statistics.h"
#include "../Main/ScenarioTracker.h"
#include "../Main/ScenarioTrackerTypes.h"
#include "Diplomacy.h"
#include "CommonUnit.h"
extern CDiplomacy theDipl;
CStatistics theStatistics;
BASIC_REGISTER_CLASS( IScenarioUnit );
void CStatistics::Init()
{
	pScenarioTracker = GetSingleton<IScenarioTracker>();
	bEnablePlayerExp = GetGlobalVar( "TutorialMode", 0 ) == 0;
}
void CStatistics::UnitCaptured( const int nPlayer )
{
	if ( pScenarioTracker )
		pScenarioTracker->GetPlayer( nPlayer )->GetMissionStats()->AddValue( STMT_ENEMY_MACHINERY_CAPTURED, 1.0f );
}
IMissionStatistics* CStatistics::GetPlayerStats( const int nPlayer )
{
	if ( pScenarioTracker )
	{
		if ( pScenarioTracker->GetPlayer( nPlayer ) )
			return pScenarioTracker->GetPlayer( nPlayer )->GetMissionStats();
	}

	return 0;
}
void CStatistics::UnitKilled( const int nPlayer, const int nKilledUnitsPlayer, const int nUnits, const float fTotalAIPrice )
{
	if ( pScenarioTracker )
	{
		EDiplomacyInfo diplInfo = theDipl.GetDiplStatus( nPlayer, nKilledUnitsPlayer );
		if ( diplInfo == EDI_ENEMY )
		{
			IMissionStatistics *pPlayerStats = GetPlayerStats( nPlayer );
			if ( pPlayerStats )
			{
				pPlayerStats->AddValue( STMT_ENEMY_KILLED, nUnits );
				pPlayerStats->AddValue( STMT_ENEMY_KILLED_AI_PRICE, fTotalAIPrice );
			}
			
			IMissionStatistics *pKilledPlayerStats = GetPlayerStats( nKilledUnitsPlayer );
			if ( pKilledPlayerStats )
			{
				pKilledPlayerStats->AddValue( STMT_FRIENDLY_KILLED, nUnits );
				pKilledPlayerStats->AddValue( STMT_FRIENDLY_KILLED_AI_PRICE, fTotalAIPrice );
			}
		}
	}
}
void CStatistics::ObjectDestroyed( const int nPlayer )
{
	if ( pScenarioTracker )
	{
		if ( IMissionStatistics *pPlayerStats = GetPlayerStats( nPlayer ) )
				pPlayerStats->AddValue( STMT_HOUSES_DESTROYED, 1.0f );
	}
}
void CStatistics::AviationCalled( const int nPlayer )
{
	if ( pScenarioTracker )
	{
		if ( IMissionStatistics *pPlayerStats = GetPlayerStats( nPlayer ) )
			pPlayerStats->AddValue( STMT_AVIATION_CALLED, 1.0f );
	}
}
void CStatistics::ReinforcementUsed( const int nPlayer )
{
	if ( pScenarioTracker ) 
	{
		if ( IMissionStatistics *pPlayerStats = GetPlayerStats( nPlayer ) )
			pPlayerStats->AddValue( STMT_REINFORCEMENT_USED, 1.0f );
	}
}
void CStatistics::ResourceUsed( const int nPlayer, const float fResources )
{
	if ( pScenarioTracker )
	{
		if ( IMissionStatistics *pPlayerStats = GetPlayerStats( nPlayer ) )
			pPlayerStats->AddValue( STMT_RESOURCES_USED, fResources );
	}
}
void CStatistics::UnitDead( CCommonUnit *pUnit )
{
	if ( pScenarioTracker )
	{
		if ( !pUnit->IsFormation() )
		{
			if ( IMissionStatistics *pPlayerStats = GetPlayerStats( pUnit->GetPlayer() ) )
				pPlayerStats->AddValue( STMT_UNITS_LOST_UNRECOVERABLY, 1.0f );
		}

		if ( IScenarioUnit *pScenarioUnit = pUnit->GetScenarioUnit() )
			pScenarioUnit->Kill();
	}
}
void CStatistics::IncreasePlayerExperience( const int nPlayer, const float fPrice ) 
{
	if ( bEnablePlayerExp && pScenarioTracker )
	{
		if ( IMissionStatistics *pPlayerStats = GetPlayerStats( nPlayer ) )
			pPlayerStats->AddValue( STMT_PLAYER_EXPERIENCE, fPrice );
	}
}
void CStatistics::UnitLeveledUp( CCommonUnit *pUnit )
{
	IScenarioUnit *pScenarioUnit = pUnit->GetScenarioUnit();
	if ( pScenarioUnit && pScenarioTracker )
	{
		if ( IMissionStatistics *pPlayerStats = GetPlayerStats( pUnit->GetPlayer() ) )
			pPlayerStats->AddValue( STMT_UNITS_LEVELED_UP, 1.0f );
	}
}
void CStatistics::SetFlagPoints( const int nParty, const float fPoints )
{
	if ( pScenarioTracker )
	{
		for ( IPlayerScenarioInfoIterator *pIter = pScenarioTracker->CreatePlayerScenarioInfoIterator(); !pIter->IsEnd(); pIter->Next() )
		{
			IPlayerScenarioInfo *pInfo = pIter->Get();
			if ( pInfo->GetDiplomacySide() == nParty )
				pInfo->GetMissionStats()->SetValue( STMT_FLAGPOINTS, fPoints );
		}
	}
}
void CStatistics::SetCapturedFlags( const int nParty, const int nFlags )
{
	if ( pScenarioTracker )
	{
		for ( IPlayerScenarioInfoIterator *pIter = pScenarioTracker->CreatePlayerScenarioInfoIterator(); !pIter->IsEnd(); pIter->Next() )
		{
			IPlayerScenarioInfo *pInfo = pIter->Get();
			if ( pInfo->GetDiplomacySide() == nParty )
				pInfo->GetMissionStats()->SetValue( STMT_FLAGS_CAPTURED, nFlags );
		}
	}
}
int CStatistics::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &bEnablePlayerExp );
	if ( (&ss)->IsReading() ) 
		pScenarioTracker = GetSingleton<IScenarioTracker>();

	return 0;
}
