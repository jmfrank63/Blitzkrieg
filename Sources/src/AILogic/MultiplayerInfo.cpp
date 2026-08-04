#include "StdAfx.h"

#include "MultiplayerInfo.h"
#include "Diplomacy.h"
#include "Updater.h"
#include "Scripts/Scripts.h"
#include "Statistics.h"
#include "MPLog.h"
#include "Units.h"
#include "Cheats.h"
CMultiplayerInfo theMPInfo;
extern CDiplomacy theDipl;
extern CScripts *pScripts;
extern CUpdater updater;
extern NTimer::STime curTime;
extern CStatistics theStatistics;
extern CUnits units;
extern SCheats theCheats;
void CMultiplayerInfo::Init()
{
	if ( theDipl.IsNetGame() )
	{
		bNoWin = false;
		
		eGameType = CMapInfo::GAME_TYPE( GetGlobalVar( "Multiplayer.GameType", CMapInfo::TYPE_SINGLE_PLAYER ) );

		NI_ASSERT_T( eGameType == CMapInfo::TYPE_FLAGCONTROL || eGameType == CMapInfo::TYPE_SABOTAGE, NStr::Format( "Unknown game type (%d)", eGameType ) );

		winConditions.nFlagScoreLimit = GetGlobalVar( "Multiplayer.GameSettings.FlagScoreLimit", 0 );
		winConditions.nKillScoreLimit = GetGlobalVar( "Multiplayer.GameSettings.KillScoreLimit", 0 );
		winConditions.nTimeToCapture = GetGlobalVar( "Multiplayer.GameSettings.TimeToCapture", 0 ) * 1000;
		winConditions.nTimeLimit = GetGlobalVar( "Multiplayer.GameSettings.TimeLimit", 0 ) * 60 * 1000;
		nAttackingParty = GetGlobalVar( "Multiplayer.AttackingParty", -1 );

		bCapturedByAttackingParty = false;

		if ( winConditions.nFlagScoreLimit != 0 && eGameType == CMapInfo::TYPE_FLAGCONTROL )
		{
			updater.AddFeedBack( SAIFeedBack( EFB_UPDATE_TEAM_F_L_AGS, 0 ) );
			if ( !theCheats.IsHistoryPlaying() )
				updater.AddFeedBack( SAIFeedBack( EFB_UPDATE_TEAM_F_L_AGS, 1 ) );
		}
		if ( winConditions.nKillScoreLimit != 0 )
		{
			updater.AddFeedBack( SAIFeedBack( EFB_UPDATE_TEAM_F_R_AGS, 0 ) );
			if ( !theCheats.IsHistoryPlaying() )
				updater.AddFeedBack( SAIFeedBack( EFB_UPDATE_TEAM_F_R_AGS, 1 ) );
		}

		capturedByPartyFlags.clear();
		capturedByPartyFlags.resize( 3 );
		nFlagsAtTheMap = 0;
		timeOfFlagCaptured = 0;
	}
}
void CMultiplayerInfo::GameFinished( const EFeedBack eGameResult )
{
	if ( !bNoWin )
	{
		bNoWin = true;

		if ( !theCheats.IsHistoryPlaying() )
			updater.AddFeedBack( eGameResult );
		else
		{
			if ( eGameResult == EFB_WIN )
				SetGlobalVar( "temp.Multiplayer.Win.Partyname", theDipl.GetMyParty() );
			else if ( eGameResult == EFB_LOOSE )
				SetGlobalVar( "temp.Multiplayer.Win.Partyname", 1 - theDipl.GetMyParty() );
			else
				SetGlobalVar( "temp.Multiplayer.Win.Partyname", 2 );

			updater.AddFeedBack( EFB_WIN );
		}
	}
}
void CMultiplayerInfo::CheckFlagPoints()
{
	const int nMyParty = theDipl.GetMyParty();
	const int nEnemyParty = 1 - theDipl.GetMyParty();

	if ( (int)flagPoints[nMyParty] > (int)flagPoints[nEnemyParty] )
		GameFinished( EFB_WIN );
	else if ( (int)flagPoints[nMyParty] < (int)flagPoints[nEnemyParty] )
		GameFinished( EFB_LOOSE );
}
void CMultiplayerInfo::CheckTroopPoints()
{
	const int nMyParty = theDipl.GetMyParty();
	const int nEnemyParty = 1 - theDipl.GetMyParty();

	if ( (int)troopsPoints[nMyParty] > (int)troopsPoints[nEnemyParty] )
		GameFinished( EFB_WIN );
	else if ( (int)troopsPoints[nMyParty] < (int)troopsPoints[nEnemyParty] )
		GameFinished( EFB_LOOSE );
}
void CMultiplayerInfo::CheckSabotageWinConditions()
{
	const bool bFinishedByFlagCapture = bCapturedByAttackingParty && timeOfFlagCaptured >= winConditions.nTimeToCapture;
	const bool bFinishedByTimeLimit = winConditions.nTimeLimit != 0 && curTime >= winConditions.nTimeLimit;

	if ( bFinishedByFlagCapture && bFinishedByTimeLimit )
		GameFinished( EFB_DRAW );
	else if ( bFinishedByFlagCapture )
		GameFinished( nAttackingParty == theDipl.GetMyParty() ? EFB_WIN : EFB_LOOSE );
	else if ( bFinishedByTimeLimit )
		GameFinished( nAttackingParty == theDipl.GetMyParty() ? EFB_LOOSE : EFB_WIN );
}
void CMultiplayerInfo::CheckFlagControlWinConditions()
{
	if ( winConditions.nTimeLimit != 0 && curTime >= winConditions.nTimeLimit )
	{
		CheckFlagPoints();
		CheckTroopPoints();
		GameFinished( EFB_DRAW );
	}
	else if ( winConditions.nFlagScoreLimit != 0 &&
				    ( flagPoints[0] >= winConditions.nFlagScoreLimit || flagPoints[1] >= winConditions.nFlagScoreLimit ) )
	{
		const bool bWeWin = flagPoints[theDipl.GetMyParty()] >= winConditions.nFlagScoreLimit;
		const bool bEnemyWin = flagPoints[1 - theDipl.GetMyParty()] >= winConditions.nFlagScoreLimit;

		if ( bWeWin && bEnemyWin )
		{
			CheckFlagPoints();
			CheckTroopPoints();
			GameFinished( EFB_DRAW );
		}
		else if ( bWeWin )
			GameFinished( EFB_WIN );
		else
			GameFinished( EFB_LOOSE );
	}
	else if ( winConditions.nKillScoreLimit != 0 &&
						( troopsPoints[0] >= winConditions.nKillScoreLimit || troopsPoints[1] >= winConditions.nKillScoreLimit ) )
	{
		const bool bWeWin = troopsPoints[theDipl.GetMyParty()] >= winConditions.nKillScoreLimit;
		const bool bEnemyWin = troopsPoints[1 - theDipl.GetMyParty()] >= winConditions.nKillScoreLimit;

		if ( bWeWin && bEnemyWin )
		{
			CheckTroopPoints();
			CheckFlagPoints();
			GameFinished( EFB_DRAW );
		}
		else if ( bWeWin )
			GameFinished( EFB_WIN );
		else
			GameFinished( EFB_LOOSE );
	}
	if ( nFlagsAtTheMap != 0 )
	{
		if ( capturedByPartyFlags[theDipl.GetMyParty()].size() == nFlagsAtTheMap )
			GameFinished( EFB_WIN );
		else if ( capturedByPartyFlags[1 - theDipl.GetMyParty()].size() == nFlagsAtTheMap )
			GameFinished( EFB_LOOSE );
	}
}
void CMultiplayerInfo::CheckWinConditions()
{
	if ( eGameType == CMapInfo::TYPE_SABOTAGE )
		CheckSabotageWinConditions();
	else
		CheckFlagControlWinConditions();
	
	if ( eGameType == CMapInfo::TYPE_SABOTAGE )
	{
		if ( units.Size( 0 ) == 0 && units.Size( 1 ) == 0 )
			GameFinished( EFB_DRAW );
		if ( units.Size( 1 - theDipl.GetMyParty() ) == 0 )
			GameFinished( EFB_WIN );
		if ( units.Size( theDipl.GetMyParty() ) == 0 )
			GameFinished( EFB_LOOSE );
	}

	for ( int i = 0; i < theDipl.GetNPlayers(); ++i )
	{
		if ( theDipl.IsPlayerExist( i ) && theDipl.GetDiplStatus( i, theDipl.GetMyNumber() ) == EDI_ENEMY )
			return;
	}

	GameFinished( EFB_WIN );
}
void CMultiplayerInfo::Segment()
{
	if ( theDipl.IsNetGame() && !bNoWin )
	{
		CheckWinConditions();
		
		const float fPoints = SConsts::PLAYER_POINTS_SPEED * (float)SConsts::AI_SEGMENT_DURATION / 1000.0f;
		AddFlagPoints( 0, fPoints );
		AddFlagPoints( 1, fPoints );

		if ( eGameType == CMapInfo::TYPE_FLAGCONTROL )
		{
			if ( flagPoints4Script[0] >= SConsts::FLAG_POINTS_TO_REINFORCEMENT )
			{
				flagPoints4Script[0] = 0.0f;
				pScripts->CallScriptFunction( NStr::Format( "FlagReinforcement( %d );", 0 ) );
			}
			if ( flagPoints4Script[1] >= SConsts::FLAG_POINTS_TO_REINFORCEMENT )
			{
				flagPoints4Script[1] = 0.0f;
				pScripts->CallScriptFunction( NStr::Format( "FlagReinforcement( %d );", 1 ) );
			}
		}

		if ( eGameType == CMapInfo::TYPE_SABOTAGE )
		{
			if ( bCapturedByAttackingParty )
				timeOfFlagCaptured += SConsts::AI_SEGMENT_DURATION;
		}
	}
}
void CMultiplayerInfo::AddFlagPoints( const int nParty, const float fPoints )
{
	if ( theDipl.IsNetGame() && eGameType == CMapInfo::TYPE_FLAGCONTROL && nParty != theDipl.GetNeutralParty() && !bNoWin )
	{
		flagPoints[nParty] += fPoints;
		flagPoints4Script[nParty] += fPoints;
		
		if ( eGameType == CMapInfo::TYPE_FLAGCONTROL )
			updater.AddFeedBack( SAIFeedBack( EFB_UPDATE_TEAM_F_L_AGS, (int(flagPoints[nParty]) << 2) | nParty ) );
		
		theStatistics.SetFlagPoints( nParty, flagPoints[nParty] );
	}
}
void CMultiplayerInfo::FlagCaptured( const int nParty, const int nFlagID )
{
	if ( theDipl.IsNetGame() && !bNoWin )
	{
		capturedByPartyFlags[0].erase( nFlagID );
		capturedByPartyFlags[1].erase( nFlagID );
		capturedByPartyFlags[2].erase( nFlagID );
		capturedByPartyFlags[nParty].insert( nFlagID );

		theStatistics.SetCapturedFlags( 0, capturedByPartyFlags[0].size() );
		theStatistics.SetCapturedFlags( 1, capturedByPartyFlags[1].size() );

		if ( eGameType == CMapInfo::TYPE_SABOTAGE )
		{
			if ( nParty != nAttackingParty )
			{
				if ( capturedByPartyFlags[nAttackingParty].empty() )
				{
					timeOfFlagCaptured = 0;
					bCapturedByAttackingParty = false;

					updater.AddFeedBack( SAIFeedBack( EFB_UPDATE_TIME_BEFORE_CAPTURE, 0 ) );
				}
			}
			else if ( !bCapturedByAttackingParty )
			{
				bCapturedByAttackingParty = true;
				updater.AddFeedBack( SAIFeedBack( EFB_UPDATE_TIME_BEFORE_CAPTURE, winConditions.nTimeToCapture / 1000 ) );
			}
		}
	}
}
void CMultiplayerInfo::UnitsKilled( const int nKillerPlayer, const float fUnitsPrice, const int nKilledUnitsPlayer )
{
	if ( theDipl.IsNetGame() && theDipl.GetDiplStatus( nKillerPlayer, nKilledUnitsPlayer ) == EDI_ENEMY )
	{
		const int nKillerParty = theDipl.GetNParty(nKillerPlayer);
		troopsPoints[nKillerParty] += fUnitsPrice;

		updater.AddFeedBack( SAIFeedBack( EFB_UPDATE_TEAM_F_R_AGS, (int(troopsPoints[nKillerParty]) << 2) | nKillerParty ) );
	}
}
void CMultiplayerInfo::Clear()
{
	bNoWin = false;

	capturedByPartyFlags.clear();

	flagPoints.clear();
	flagPoints.resize( 3, 0.0f );

	troopsPoints.clear();
	troopsPoints.resize( 3, 0.0f );

	flagPoints4Script.clear();
	flagPoints4Script.resize( 3, 0.0f );
}
const NTimer::STime CMultiplayerInfo::GetTimeToCaptureObject() const 
{ 
	if ( eGameType == CMapInfo::TYPE_SABOTAGE )
		return 0;
	else
		return winConditions.nTimeToCapture;
}
void CMultiplayerInfo::NoWin()
{
	bNoWin = true;
}
