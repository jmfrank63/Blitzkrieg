#include "StdAfx.h"

#include <float.h>

#include "MultiPlayerTransceiver.h"

#include "iMainCommands.h"
#include "AILogicCommandInternal.h"
#include "Multiplayer.h"
#include "NetMessages.h"
#include "CommandsHistoryInterface.h"
#include "ScenarioTracker.h"
#include "..\GameTT\iMission.h"
#include "..\StreamIO\StreamIOTypes.h"
#include "..\AILogic\AILogic.h"
#include "..\Input\Input.h"
#include "..\GameTT\MultiplayerCommandManager.h"
#include "..\Common\PauseGame.h"
#include "..\Main\TextSystem.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												CMultiPlayerTransceiver										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CMultiPlayerTransceiver::MAX_LATENCY = 128;
const NTimer::STime CMultiPlayerTransceiver::TIME_TO_START_LAG_BY_NO_SEGMENT_DATA = 3000;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMultiPlayerTransceiver::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() ) 
	{
		pAILogic = GetSingleton<IAILogic>();
		pCmdsHistory = GetSingleton<ICommandsHistory>();
	}
	//
	saver.Add( 1, &nLatency );
	saver.Add( 2, &nNumPlayers );
	saver.Add( 3, &nSegment );
	saver.Add( 4, &nMyNumber );
	saver.Add( 5, &segmFinished );
	saver.Add( 6, &wMask );
	saver.Add( 7, &cmds );

	saver.Add( 9, &nCommonSegment );
	saver.Add( 11, &timeOfLastSegmFinished );
	saver.Add( 12, &lastTimeToCheckNoSegmDataLag );
	saver.Add( 13, &noSegmDataLags );
	saver.Add( 16, &nNumPlayersInMap );
	saver.Add( 17, &nSegmentsPackSize );
	saver.Add( 18, &lastFinishedSegment );
	saver.Add( 19, &bHistoryPlaying );
	saver.Add( 20, &nTimeToAllowDropByLag );

	saver.Add( 21, &loadingPlayers );
	saver.Add( 22, &nLoadingPlayers );
	saver.Add( 23, &bTotalOutOfSync );
	saver.Add( 24, &fLastSentMultiplayerTime );
	saver.Add( 25, &bSpeedSet );
	saver.Add( 26, &receivedCmds );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::Init( ISingleton *pSingleton, const int nMultiplayerType )
{
	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );
	nTimeToAllowDropByLag = constsTbl.GetInt( "Net", "TimeToAllowDropByLag", 30 );

	pCmdsHistory = GetSingleton<ICommandsHistory>();

	bGameStarted = false;
	bHistoryPlaying = false;
	bTotalOutOfSync = false;
	/*EMCT_LAN,
	EMCT_INTERNET,
	EMCT_GAMESPY,*/
	switch ( nMultiplayerType )
	{
		case EMCT_LAN: 
			pMultiplayer = CreateObject<IMultiplayer>( LAN_MULTIPLAYER );
			if ( GetGlobalVar( "History.Playing", 0 ) == 0 )
				RemoveGlobalVar( "Multiplayer.InternetGame" );

			break;
		case EMCT_GAMESPY:
			pMultiplayer = CreateObject<IMultiplayer>( GAMESPY_MULTIPLAYER );
			if ( GetGlobalVar( "History.Playing", 0 ) == 0 )
				SetGlobalVar( "Multiplayer.InternetGame", 1 );

			break;
		case EMCT_INTERNET:
			pMultiplayer = CreateObject<IMultiplayer>( INTERNET_MULTIPLAYER );
			if ( GetGlobalVar( "History.Playing", 0 ) == 0 )
				SetGlobalVar( "Multiplayer.InternetGame", 1 );

			break;
	}

	pMultiplayer->Init();
	timeOut.Init( pMultiplayer );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::LoadGameSettings()
{
	// my number info
	nMyNumber = GetGlobalVar( "Multiplayer.OurPlayerID", -1 );
	NI_ASSERT_T( nMyNumber != -1, "Wrong number of our player" );
	timeOut.InitGameStart( nMyNumber );
	
	nNumPlayersInMap = GetGlobalVar( "Multiplayer.NumPlayersInMap", -1 );
	pAILogic->SetNPlayers( nNumPlayersInMap );

	// number of players info
	nNumPlayers = GetGlobalVar( "Multiplayer.NumGamingPlayers", 0 );
	
	wMask = 0;
	IScenarioTracker *pScenarioTracker = GetSingleton<IScenarioTracker>();
	
	players.clear();

	// all players info
	std::string szValueName;
	std::vector<int> busyNumbers( 16, 0 );
	for ( int i = 0; i != nNumPlayers; ++i )
	{
		players.push_back( SPlayerInfo() );
		SPlayerInfo &player = players.back();

		player.totalLagTime = 0;
		player.lastLagUpdateTime = 0;

		szValueName = NStr::Format( "Multiplayer.Player%d.Name", i );
		player.szName = MakeWideStringFromWordString( GetGlobalWVar( szValueName.c_str(), L"Unknown Player" ) );

		szValueName = NStr::Format( "Multiplayer.Player%d.Side", i );
		player.nSide = GetGlobalVar( szValueName.c_str(), int(-1) );

		szValueName = NStr::Format( "Multiplayer.Player%d.LogicID", i );
		player.nLogicID = GetGlobalVar( szValueName.c_str(), int(-1) );

		szValueName = NStr::Format( "Multiplayer.Side%d.Name", player.nSide );
		const std::string szSideName = GetGlobalVar( szValueName.c_str(), "Unknown" );
		
		if ( player.nSide != -1 && szSideName != "Unknown" )
			SetGlobalVar( NStr::Format( "Multiplayer.Side%d.Name", player.nSide ), szSideName.c_str() );

		//
		IPlayerScenarioInfo *pScenarioTrackerPlayer = pScenarioTracker->AddPlayer( player.nLogicID );
		if ( player.nLogicID == nMyNumber )
		{
			pAILogic->SetMyInfo( player.nSide, player.nLogicID );
			pScenarioTracker->SetUserPlayer( player.nLogicID );
		}

		pScenarioTrackerPlayer->SetSide( szSideName.c_str() );
		pScenarioTrackerPlayer->SetName( player.szName.c_str() );
		pScenarioTrackerPlayer->SetDiplomacySide( player.nSide );

		//
		if ( (player.nSide == -1) || (player.nLogicID == -1) )
		{
			NI_ASSERT_T( false, NStr::Format("Player %d has wrong information. Skipping", i) );
			players.pop_back();
			continue;
		}
		else
			wMask = wMask | ( 1UL << player.nLogicID );

		busyNumbers[player.nLogicID] = 1;
	}
	
	if ( GetGlobalVar( "LoadCommandLineHistory", 0 ) == 1 )
	{
		const int nRealPlayers = pCmdsHistory->GetNumPlayersInMPGame();

		for ( int i = nNumPlayers; i < nRealPlayers; ++i )
		{
			const int nSide = pCmdsHistory->GetMPPlayerSide( i );
			const int nLogicID = pCmdsHistory->GetMPPlayerLogicID( i );

			const std::string szValueName = NStr::Format( "Multiplayer.Side%d.Name", nSide );
			const std::string szSideName = GetGlobalVar( szValueName.c_str(), "Unknown" );

			IPlayerScenarioInfo *pScenarioTrackerPlayer = pScenarioTracker->AddPlayer( nLogicID );

			pScenarioTrackerPlayer->SetSide( szSideName.c_str() );
			pScenarioTrackerPlayer->SetName( NStr::ToUnicode(NStr::Format( "AbsentPlayer%d", i - nNumPlayers )) );
			pScenarioTrackerPlayer->SetDiplomacySide( nSide );

			busyNumbers[nLogicID] = 1;
		}
	}
	
	// add neutral players
	for ( int i = 0; i < 16; ++i )
	{
		if ( busyNumbers[i] == 0 )
		{
			IPlayerScenarioInfo *pScenarioTrackerPlayer = pScenarioTracker->AddPlayer( i );
			pScenarioTrackerPlayer->SetDiplomacySide( 2 );
			pScenarioTrackerPlayer->SetSide( "Neutral" );
			CPtr<IText> pText = GetSingleton<ITextManager>()->GetDialog( "textes\\opponents\\neutral" );
			NI_ASSERT_T( pText != 0, "Text \"textes\\opponents\\neutral\" with neutral player name doesn't exist" );
			pScenarioTrackerPlayer->SetName( MakeWideStringFromWordString( pText->GetString() ) );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::InitVariables()
{
	// clear checksums stream
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	while ( const char *pszString = pBuffer->ReadASCII(CONSOLE_STREAM_MULTIPLAYER_CHECK) );

	pktOutgoing = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );

	bTotalOutOfSync = false;

	//
	receivedCmds.clear();
	segmFinished.clear(); segmFinished.resize( MAX_LATENCY, 0 );
	lastFinishedSegment.clear(); lastFinishedSegment.resize( MAX_LATENCY, 0 );
	timeOfLastSegmFinished.clear(); timeOfLastSegmFinished.resize( 16, -1 );
	noSegmDataLags.clear(); noSegmDataLags.resize( 16, 0 );
	loadingPlayers.clear(); loadingPlayers.resize( 16, 0 );
	nLoadingPlayers = 0;
	lastTimeToCheckNoSegmDataLag = 0;

	nLatency = 14;//GetGlobalVar( "Multiplayer.InternetGame", 0 ) == 1 ? 14 : 8;

	nSegmentsPackSize = nLatency / 2;
	for ( int i = MAX_LATENCY - nLatency; i < MAX_LATENCY; ++i )
	{
		if ( ( MAX_LATENCY - i ) % nSegmentsPackSize == 0 )
			segmFinished[i] = wMask;
	}
	
	for ( int i = 0; i < nNumPlayersInMap; ++i )
		lastFinishedSegment[i] = -nSegmentsPackSize;

	nSegment = 0;
	nCommonSegment = 0;

	cmds.Clear();
	cmds.SetSizes( nNumPlayersInMap, MAX_LATENCY );

	CControlSumCheckCommand::Init( wMask );

	pAILogic->SetNetGame( true );
	
	//
	SetGlobalVar( "MultiplayerGame", 1 );

	bGameStarted = true;
	fLastSentMultiplayerTime = 0;

	bHistoryPlaying = GetGlobalVar( "History.Playing", 0 ) == 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::LoadAllGameParameters()
{
	pAILogic = GetSingleton<IAILogic>();
	NI_ASSERT_T( pMultiplayer != 0, "Can't start game: doesn't have multiplayer object" );

	// enter campaign
	IScenarioTracker *pScenarioTracker = GetSingleton<IScenarioTracker>();
	pScenarioTracker->StartCampaign( "MULTIPLAYER", CAMPAIGN_TYPE_MULTIPLAYER );
	LoadGameSettings();

	pScenarioTracker->StartChapter( "MULTIPLAYER" );
	pScenarioTracker->StartMission( "MULTIPLAYER" );
	InitVariables();

	// reset timer
	GetSingleton<IGameTimer>()->GetGameTimer()->Reset();
	GetSingleton<IGameTimer>()->GetGameSegmentTimer()->Set( 0xffffffff );

	bSpeedSet = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::SendChatMessages()
{
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	while ( const wchar_t *pszString = pBuffer->Read( CONSOLE_STREAM_NET_CHAT ) )
	{
		// скопировать во временную переменную, чтобы не затёрлось следующим вызовом pBuffer->Read
		std::wstring szMessageType = pszString;
		const wchar_t *pszString1 = pBuffer->Read( CONSOLE_STREAM_NET_CHAT );
		pMultiplayer->SendInGameChatMessage( reinterpret_cast<const WORD*>( szMessageType.c_str() ), reinterpret_cast<const WORD*>( pszString1 ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::SegmentFinished()
{
	if ( !bTotalOutOfSync )
	{
		IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
		// checksums от AI
		while ( const char *pszString = pBuffer->ReadASCII(CONSOLE_STREAM_MULTIPLAYER_CHECK) )
		{
			unsigned long checkSum = 0;
			sscanf( pszString, "%ul", &checkSum );
			CControlSumCheckCommand *pCheckSumCommand = new CControlSumCheckCommand( nMyNumber, checkSum );

			AddCommandToSend( pCheckSumCommand );
		}

		// add segment data
		pktOutgoing << BYTE(NGM_ID_SEGMENT);
		// послать об окончании команд моего сегмента
		pMultiplayer->SendClientCommands( pktOutgoing );
		pktOutgoing->SetSize( 0 );
	}

	//
	segmFinished[nSegment] |= 1UL << nMyNumber;
	lastFinishedSegment[nMyNumber] = nCommonSegment;

	// segment finished sent, don't need to send "I am alive" message
	timeOfLastSegmFinished[nMyNumber] = GetSingleton<IGameTimer>()->GetAbsTime();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::SetLatency( int nSegments )
{
	// CRAP{ не всё так просто. для корректной работы надо послать команду на изменение latency для этого клиента
	// выполнить лишние сегменты
	nLatency = nSegments;
	// CRAP}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CMultiPlayerTransceiver::GetPastSegmentNum( const int nLatency ) const
{
	// прибавляется MAX_LATENCY, т.к. модуль от отрицательных чисел считается неправильно
	return ( nSegment - nLatency + MAX_LATENCY ) % MAX_LATENCY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CMultiPlayerTransceiver::GetCommonPastSegmentNum() const
{
	return ( nCommonSegment - nLatency );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::AddCommandToSend( IAILogicCommand *pCommand )
{
	const int nModule = nCommonSegment % nSegmentsPackSize;
	const int nSegmentToExecuteCommand = nModule == 0 ? nSegment : ( nSegment + nSegmentsPackSize - nModule ) % MAX_LATENCY;
	cmds[nSegmentToExecuteCommand][nMyNumber].push_back( pCommand );

	pCommand->Store( pktOutgoing );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::RemovePlayer( const int nLogicID )
{
	wMask &= ~(1UL << nLogicID);
	CControlSumCheckCommand::wMask = wMask;

	int nSegmentToChange = GetPastSegmentNum( nLatency );
	segmFinished[nSegmentToChange] &= ~(1UL << nLogicID);
	timeOfLastSegmFinished[nLogicID] = -1;

	if ( nSegmentToChange != nSegment )
	{
		do
		{
			nSegmentToChange = ( nSegmentToChange + 1 ) % MAX_LATENCY;						
			segmFinished[nSegmentToChange] &= ~(1UL << nLogicID);
		} 
		while ( nSegmentToChange != nSegment );
	}

	//
	CPlayersList::iterator iter = players.begin();
	while ( iter != players.end() && iter->nLogicID != nLogicID )
		++iter;

	if ( iter != players.end() )
	{
		GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_MP_PLAYER_STATE_CHANGED, (iter->nLogicID << 8) ) );

		IAILogicCommand *pAICmd = new CDropPlayerCommand( iter->nLogicID );
		AddCommandToSend( pAICmd );
	}

	if ( timeOut.GetTimeOutPlayer() == nLogicID )
		timeOut.UnsetTimeOut( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::SetPlayerAlive( const int nPlayer )
{
	// if nodata lag set
	if ( noSegmDataLags[nPlayer] == 1 )
	{
		// no segments received from this player
		if ( timeOfLastSegmFinished[nPlayer] == -1 )
			SetPlayerLoading( nPlayer, false );
		else						
			SetPlayerLag( nPlayer, false );

		noSegmDataLags[nPlayer] = 0;
	}

	timeOfLastSegmFinished[nPlayer] = GetSingleton<IGameTimer>()->GetAbsTime();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::ProcessMultiplayerCommands()
{
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );

	CPtr<IMultiplayer::CCommand> pCommand;
	while ( pCommand = pMultiplayer->GetCommand() )
	{
		switch ( pCommand->eCommandType )
		{
			case IMultiplayer::GPC_SEGMENT_FINISHED:
				{
					if ( bGameStarted )
					{
						lastFinishedSegment[pCommand->nPlayer] += nSegmentsPackSize;

						const WORD wPlayerSegment = lastFinishedSegment[pCommand->nPlayer] % MAX_LATENCY;
						// segment message received - now add all commands to 'cmds'
						while ( !receivedCmds.empty() ) 
						{
							cmds[wPlayerSegment][pCommand->nPlayer].push_back( receivedCmds.front() );
							receivedCmds.pop_front();
						}
						// signal, that 'nPlayer' has finished 'wPlayerSegment'
						segmFinished[wPlayerSegment] |= ( 1UL << pCommand->nPlayer );

						SetPlayerAlive( pCommand->nPlayer );
					}
				}

				break;
			case IMultiplayer::GPC_PLAYER_REMOVED:
				if ( bGameStarted && ( wMask & ( 1UL << pCommand->nPlayer ) ) && pCommand->nPlayer != nMyNumber )
					RemovePlayer( pCommand->nPlayer );

				break;
			case IMultiplayer::GPC_PLAYER_LAG:
				if ( bGameStarted && !bTotalOutOfSync )
				{
					CPlayersList::iterator iter = players.begin();
					while ( iter != players.end() && iter->nLogicID != pCommand->nPlayer )
						++iter;

					if ( iter != players.end() )
						SetPlayerLag( iter->nLogicID, pCommand->nParam == 1 );
				}

				break;
			case IMultiplayer::GPC_AI_COMMAND:
				if ( bGameStarted )
				{
					NI_ASSERT_T( bGameStarted, "Game wasn't started" );
					receivedCmds.push_back( pCommand->pAILogicCommand );
				}

				break;
			case IMultiplayer::GPC_START_GAME:
				LoadAllGameParameters();
				GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MISSION, GetGlobalVar( "Multiplayer.MapName" ) );

				break;
			case IMultiplayer::GPC_PAUSE:
				if ( bGameStarted )
					GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_GAME_PAUSE ) );

				break;
			case IMultiplayer::GPC_GAME_SPEED:
				if ( bGameStarted )
				{
					const int nChange = pCommand->nParam;
					IMainLoop *pMainLoop = GetSingleton<IMainLoop>();
					// посылаем через interface команду, чтобы после старта multiplayer можно было 
					// сразу давать команду на изменения скрости
					for ( int i = 0; i < abs( nChange ); ++i )
					{
						if ( nChange < 0 )
							pMainLoop->Command( MAIN_COMMAND_CMD, NStr::Format( "%d", CMD_GAME_SPEED_DEC ) );
						else
							pMainLoop->Command( MAIN_COMMAND_CMD, NStr::Format( "%d", CMD_GAME_SPEED_INC ) );
					}
				}

				break;
			case IMultiplayer::GPC_TIMEOUT:
				if ( bGameStarted )
				{
					if ( pCommand->nParam )
						timeOut.SetTimeOut( pCommand->nPlayer );
					else
						timeOut.UnsetTimeOut( false );
				}

				break;
			case IMultiplayer::GPC_PLAYER_ALIVE:
				SetPlayerAlive( pCommand->nPlayer );

				break;
			default:
				NI_ASSERT_T( false, "Unknown command came from multiplayer" );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::SetNoSegmentDataLags()
{
	NTimer::STime curAbsTime = GetSingleton<IGameTimer>()->GetAbsTime();
	// time to set lags
	if ( lastTimeToCheckNoSegmDataLag + 1000 < curAbsTime )
	{
		lastTimeToCheckNoSegmDataLag = curAbsTime;

		// find loading players
		for ( int i = 0; i < 16; ++i )
		{
			// player i exists, doesn't have a pause and started the game
			if ( i != nMyNumber && ( wMask & ( 1UL << i ) ) && noSegmDataLags[i] == 0 )
			{
				if ( timeOfLastSegmFinished[i] + TIME_TO_START_LAG_BY_NO_SEGMENT_DATA < curAbsTime && timeOfLastSegmFinished[i] == -1 )
				{
					noSegmDataLags[i] = 1;
					SetPlayerLoading( i, true );
				}
			}
		}

		// find lagging players
		if ( nLoadingPlayers == 0 )
		{
			for ( int i = 0; i < 16; ++i )
			{
				// player i exists, doesn't have a pause and started the game
				if ( i != nMyNumber && ( wMask & ( 1UL << i ) ) && noSegmDataLags[i] == 0 )
				{
					if ( timeOfLastSegmFinished[i] + TIME_TO_START_LAG_BY_NO_SEGMENT_DATA < curAbsTime && timeOfLastSegmFinished[i] >= 0 )
					{
						noSegmDataLags[i] = 1;
						SetPlayerLag( i, true );
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::SetPlayerLoading( const int nPlayer, const bool bSet )
{
	if ( loadingPlayers[nPlayer] == 1 && bSet || loadingPlayers[nPlayer] == 0 && !bSet )
		return;
	
	CPlayersList::iterator iter = players.begin();
	while ( iter != players.end() && iter->nLogicID != nPlayer )
		++iter;
	
	if ( iter != players.end() )
	{
		const int nParam = bSet ? 4 : 5;
		GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_MP_PLAYER_STATE_CHANGED, (iter->nLogicID << 8) | nParam ) );
	}

	if ( loadingPlayers[nPlayer] == 0 )
		++nLoadingPlayers;
	else
		--nLoadingPlayers;

	NI_ASSERT_T( nLoadingPlayers >= 0, "Wrong number of loading players" );

	loadingPlayers[nPlayer] = (int)bSet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::SetPlayerLag( const int nPlayer, const bool bSet )
{
	// if player is loading, don't process his lags
	if ( loadingPlayers[nPlayer] == 1 )
		return;
	
	CPlayersList::iterator iter = players.begin();
	while ( iter != players.end() && iter->nLogicID != nPlayer )
		++iter;
	
	if ( iter != players.end() )
	{
		if ( bSet )
			iter->lastLagUpdateTime = GetSingleton<IGameTimer>()->GetAbsTime();
		else
			iter->lastLagUpdateTime = 0;
		
		const int nParam = bSet ? 2 : 3;

		const int nTimeLast = Max( nTimeToAllowDropByLag - iter->totalLagTime, 0 );
		GetSingleton<IInput>()->AddMessage(
			SGameMessage( CMD_MP_PLAYER_STATE_CHANGED, (nTimeLast << 16) | (iter->nLogicID << 8) | nParam )
		);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::UpdateLags()
{
	for ( CPlayersList::iterator iter = players.begin(); iter != players.end(); ++iter )
	{
		if ( ( (1UL << iter->nLogicID) & wMask) && iter->lastLagUpdateTime != 0 )
		{
			NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();
			if ( iter->lastLagUpdateTime + 1000 < curTime )
			{
				while ( iter->lastLagUpdateTime + 1000 < curTime )
				{
					iter->lastLagUpdateTime += 1000;
					++(iter->totalLagTime);
				}

				const int nTimeLast = Max( nTimeToAllowDropByLag - iter->totalLagTime, 0 );
				GetSingleton<IInput>()->AddMessage(
					SGameMessage( CMD_MP_PLAYER_STATE_CHANGED, (nTimeLast << 16) | (iter->nLogicID << 8) | 2 )
				);
			}
		}
		
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::SendIAmAlive()
{
	IGameTimer *pGameTimer = GetSingleton<IGameTimer>();

	if ( !pGameTimer->HasPause( PAUSE_TYPE_INACTIVE ) && !pGameTimer->HasPause( PAUSE_TYPE_PREMISSION ) )
	{
		NTimer::STime curAbsTime = pGameTimer->GetAbsTime();	
		// waiting for segments from other players, send "I am alive" message
		if ( timeOfLastSegmFinished[nMyNumber] + TIME_TO_START_LAG_BY_NO_SEGMENT_DATA / 2 < curAbsTime && nCommonSegment > 0 )
		{
			timeOfLastSegmFinished[nMyNumber] = curAbsTime;
			pMultiplayer->SendAliveMessage();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// perform segments for AI
void CMultiPlayerTransceiver::DoSegments()
{
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );

	//
	if ( !bHistoryPlaying && pMultiplayer->GetState() != IMultiplayer::EMS_PLAYING )
		bGameStarted = false;

	const bool bOldGameStarted = bGameStarted;

	if ( !bHistoryPlaying )
	{
		pMultiplayer->Segment();
		ProcessMultiplayerCommands();
		SendChatMessages();
	}

	if ( bGameStarted && bOldGameStarted && ( bHistoryPlaying || pMultiplayer->GetState() == IMultiplayer::EMS_PLAYING ) )
	{
		IGameTimer *pGameTimer = GetSingleton<IGameTimer>();
		ISegmentTimer *pGameSegment = pGameTimer->GetGameSegmentTimer();

		// set game speed		
		if ( bHistoryPlaying && !bSpeedSet )
		{
			const int nGameSpeed = GetGlobalVar( "Multiplayer.GameSpeed", 0 );

			const std::string szCmd( nGameSpeed < 0 ? NStr::Format( "%d", CMD_GAME_SPEED_DEC ) : NStr::Format( "%d", CMD_GAME_SPEED_INC ) );
			for ( int i = 0; i < abs( nGameSpeed ); ++i )
				GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CMD, szCmd.c_str() );

			bSpeedSet = true;
		}

		UpdateLags();
		timeOut.Segment();
		// check for segment time
		while ( pGameSegment->BeginSegments(pGameTimer->GetGameTime()) )
		{
			const int nPastSegment = GetPastSegmentNum( nLatency );
			const int nCommonPastSegmentNum = GetCommonPastSegmentNum();
			// time to execute pack of commands
			if ( nCommonPastSegmentNum % nSegmentsPackSize == 0 )
			{
				// all commands received
				// играется history или побитовое >=
				if ( bHistoryPlaying || ( ~segmFinished[nPastSegment] & wMask ) == 0 )
				{
					// пришли сегменты от всех игроков, можно открыть туман и юниты
					if ( nCommonSegment >= nLatency && nCommonSegment <= 2 * nLatency )
						GetSingleton<IAILogic>()->NetGameStarted();
					
					// remove forced segment pause
					pGameTimer->PauseGame( false, PAUSE_TYPE_MP_NO_SEGMENT_DATA );
					// исполнить загруженную history команд
					if ( nCommonSegment >= nSegmentsPackSize )
					{
						// цикл для того, чтобы single player history можно было проигрывать в multiplayer
						for ( int i = nCommonSegment - nSegmentsPackSize + 1; i <= nCommonSegment; ++i )
							pCmdsHistory->ExecuteSegmentCommands( i, this );
					}
					// finish segment => send data
					if ( !bHistoryPlaying )
						SegmentFinished();
					// do all commands for 'past segment'
					_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );
					//
					for ( int i = 0; i < nNumPlayersInMap; ++i )
					{
						for ( CAILogicCommandsList::iterator it = cmds[nPastSegment][i].begin(); it != cmds[nPastSegment][i].end(); ++it )
						{
							(*it)->Execute( pAILogic );

							// команда пришла на сегменте GetCommonPastSegmentNum(), добавить в список команд
							pCmdsHistory->AddCommand( GetCommonPastSegmentNum(), *it );
						}
						cmds[nPastSegment][i].clear();
					}

					segmFinished[nPastSegment] = 0;
					// do 'NextSegment()' for segment timer and call 'Segment()' for AI logic
					pGameSegment->NextSegment();

					if ( !bHistoryPlaying && !bTotalOutOfSync )
						CControlSumCheckCommand::Check( nMyNumber );

					pAILogic->Segment();
					// increment internal segment-loop counter
					nSegment = ( nSegment + 1 ) % MAX_LATENCY;
					++nCommonSegment;
				}
				else
				{
					// set forced segment pause until we have all necessary data
					pGameTimer->PauseGame( true, PAUSE_TYPE_MP_NO_SEGMENT_DATA );
					SetNoSegmentDataLags();
					SendIAmAlive();
					
					return;
				}
			}
			else
			{
				pGameSegment->NextSegment();
				pAILogic->Segment();

				nSegment = ( nSegment + 1 ) % MAX_LATENCY;
				++nCommonSegment;
			}
		}

		SendIAmAlive();

		// check for remove forced segment pause
		if ( segmFinished[GetPastSegmentNum( nLatency )] == wMask )
			pGameTimer->PauseGame( false, PAUSE_TYPE_MP_NO_SEGMENT_DATA );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// register group of units to AI
int CMultiPlayerTransceiver::CommandRegisterGroup( IRefCount **pUnitsBuffer, const int nLen )
{
	if ( !bHistoryPlaying )
	{
		const WORD wID = pAILogic->GenerateGroupNumber();
		pAILogic->SubstituteUniqueIDs( pUnitsBuffer, nLen );
		//
		IAILogicCommand *pAICmd = new CRegisterGroupCommand( pUnitsBuffer, nLen, wID, pAILogic );
		AddCommandToSend( pAICmd );
		//
		return wID;
	}
	else
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// unregister group 
void CMultiPlayerTransceiver::CommandUnregisterGroup( const WORD wGroup )
{
	if ( !bHistoryPlaying )
	{
		IAILogicCommand *pAICmd = new CUnregisterGroupCommand( wGroup );
		AddCommandToSend( pAICmd );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// send command to group of units
void CMultiPlayerTransceiver::CommandGroupCommand( const SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue )
{
	if ( !bHistoryPlaying )
	{
		SAIUnitCmd cmd( *pCommand );
		cmd.bFromAI = false;
		
		IAILogicCommand *pAICmd = new CGroupCommand( &cmd, wGroup, bPlaceInQueue, pAILogic );
		AddCommandToSend( pAICmd );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set single command to call planes, reinforcements, etc. returns group number, which was created
int CMultiPlayerTransceiver::CommandUnitCommand( const struct SAIUnitCmd *pCommand )
{
	if ( !bHistoryPlaying )
	{
		const WORD wID = pAILogic->GenerateGroupNumber();
		SAIUnitCmd cmd( *pCommand );
		cmd.bFromAI = false;
		IAILogicCommand *pAICmd = new CUnitCommand( &cmd, wID, nMyNumber );
		AddCommandToSend( pAICmd );

		return wID;
	}
	else
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::CommandShowAreas( int nGroupID, int nAreaType, bool bShow )
{
	if ( !bHistoryPlaying )
	{
		IAILogicCommand *pAICmd = new CShowAreasCommand( nGroupID, nAreaType, bShow );
		AddCommandToSend( pAICmd );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::Done()
{
	players.clear();
	pMultiplayer = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::CommandClientTogglePause()
{
	//pMultiplayer->TogglePause();
/*
	if ( bHistoryPlaying )
		GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_GAME_PAUSE ) );
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::CommandClientSpeed( const int nChange )
{
/*	
	pMultiplayer->GameSpeed( nChange );

	if ( bHistoryPlaying )
	{
		for ( int i = 0; i < abs( nChange ); ++i )
		{
			if ( nChange < 0 )
				GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CMD, NStr::Format( "%d", CMD_GAME_SPEED_DEC ) );
			else
				GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CMD, NStr::Format( "%d", CMD_GAME_SPEED_INC ) );
		}
	}
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::CommandClientDropPlayer( const WORD *pszPlayerNick )
{
	CPlayersList::iterator iter = players.begin();
	const std::wstring szPlayerNick = MakeWideStringFromWordString( pszPlayerNick );
	while ( iter != players.end() && iter->szName != szPlayerNick )
		++iter;

	if ( iter != players.end() && nMyNumber != iter->nLogicID && ( wMask & (1UL << iter->nLogicID) ) )
	{
		if ( !bTotalOutOfSync )
			pMultiplayer->DropPlayer( iter->nLogicID );

		RemovePlayer( iter->nLogicID );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMultiPlayerTransceiver::GetNumberOfPlayers() const
{
	return pMultiplayer->GetNumberOfPlayers();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::CommandTimeOut( const bool bSet )
{
	if ( bSet )
		timeOut.SetTimeOut( nMyNumber );
	else
		timeOut.UnsetTimeOut( true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultiPlayerTransceiver::JoinToServer( const char *pszIPAddress, const int nPort, bool bPasswordRequired, const char* pszPassword )
{
	return pMultiplayer->InitJoinToServer( pszIPAddress, nPort, bPasswordRequired, pszPassword );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::CreateServer()
{
	pMultiplayer->InitServersList();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::InitByCreateServersList()
{
	if ( pMultiplayer->GetState() == IMultiplayer::EMS_NONE )
		pMultiplayer->InitServersList();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
NTimer::STime CMultiPlayerTransceiver::GetMultiplayerTime()
{
	if ( !GetSingleton<IAILogic>()->IsNoWin() )
	{
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		fLastSentMultiplayerTime = (float)pTimer->GetGameTime() / NTimer::GetCoeffFromSpeed( (float)pTimer->GetSpeed() );
	}

	return fLastSentMultiplayerTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::SetTotalOutOfSync()
{ 
	bTotalOutOfSync = true; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerTransceiver::GameFinished()
{
	if ( bGameStarted )
	{
		bGameStarted = false;
		pMultiplayer->FinishGame();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INetDriver* CMultiPlayerTransceiver::GetInGameNetDriver() const
{
	if ( pMultiplayer )
		return pMultiplayer->GetInGameNetDriver();
	else
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CTimeOut														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTimeOut::Init( IMultiplayer *_pMultiplayer )
{
	pMultiplayer = _pMultiplayer;
	nMyNumber = nTimeOutPlayer = -1;

	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );

	timeOutTime = constsTbl.GetInt( "Net", "GamePlayTimeOut", 30 ) * 1000;
	timeBWTimeOuts = constsTbl.GetInt( "Net", "TimeBWGamePlayTimeOuts", 30 ) * 1000;
	lastTimeOutTime = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTimeOut::InitGameStart( const int n )
{
	nMyNumber = n;
	lastTimeOutTime = 0;
	if ( GetGlobalVar( "History.Playing", 0 ) == 0 )
		SetGlobalVar( "temp.LocalPlayer.TimeOutEnable", 1 );

	SetGlobalVar( "temp.LocalPlayer.UntimeOutEnable", 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTimeOut::SetTimeOut( const int nPlayer )
{
	NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();
	// или команда пришла не от нашего клиента, или уже прошло время после предыдущего timeOut
	const bool bCanSetTimeOutNow = nPlayer != nMyNumber || lastTimeOutTime + timeBWTimeOuts < curTime;
	if ( bCanSetTimeOutNow && ( !IsActive() || nPlayer < nTimeOutPlayer ) )
	{
		nTimeOutPlayer = nPlayer;
		lastTimeOutTime = curTime + timeOutTime;

		if ( nPlayer == nMyNumber )
		{
			pMultiplayer->CommandTimeOut( true );
			SetGlobalVar( "temp.LocalPlayer.UntimeOutEnable", 1 );
		}

		GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_GAME_TIMEOUT, nPlayer ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTimeOut::UnsetTimeOut( const bool bByClientCommand )
{
	if ( IsActive() )
	{
		NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();
		lastTimeOutTime = curTime;

		if ( bByClientCommand )
			pMultiplayer->CommandTimeOut( false );

		GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_GAME_UNTIMEOUT, nTimeOutPlayer ) );

		nTimeOutPlayer = -1;

		SetGlobalVar( "temp.LocalPlayer.UntimeOutEnable", 0 );		
		SetGlobalVar( "temp.LocalPlayer.TimeOutEnable", 0 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTimeOut::Segment()
{
	if ( IsActive() )
	{
		const NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();

		if ( curTime > lastTimeOutTime )
			UnsetTimeOut( false );
		else
			GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_GAME_TIMEOUT_UPDATE, (lastTimeOutTime - curTime)/1000 ) );
	}
	else if ( GetGlobalVar( "temp.LocalPlayer.TimeOutEnable", 0 ) == 0 )
	{
		const NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();

		if ( lastTimeOutTime + timeBWTimeOuts < curTime && GetGlobalVar( "History.Playing", 0 ) == 0 )
			SetGlobalVar( "temp.LocalPlayer.TimeOutEnable", 1 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
