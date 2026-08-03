#include "StdAfx.h"

#include "GamePlaying.h"
#include "Multiplayer.h"
#include "NetMessages.h"
#include "AILogicCommandInternal.h"
#include "MultiplayerConsts.h"
#include "CommandsHistoryInterface.h"

#include "../StreamIO/StreamIOHelper.h"
#include "../StreamIO/RandomGen.h"
#include "../StreamIO/StreamIOTypes.h"

#include "../Net/NetDriver.h"

#include <float.h>
BASIC_REGISTER_CLASS( IGamePlaying );
void CGamePlaying::Init( INetDriver *_pInGameNetDriver, INetDriver *_pOutGameNetDriver, 
												const CPlayers &_players, bool bServer, const int _nOurID, 
												const std::vector<BYTE> &_diplomacies )
{
	pInGameNetDriver = _pInGameNetDriver;
	pOutGameNetDriver = _pOutGameNetDriver;
	nOurID = _nOurID;
	diplomacies = _diplomacies;
	bStartGameReceived = false;

	if ( bServer )
	{
		CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
		pkt << BYTE( NGM_ID_START_GAME );

		pInGameNetDriver->SendBroadcast( pkt );
		commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_START_GAME, -1, -1, 0 ) );

		CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
		pSeed->InitByZeroSeed();
		GetSingleton<IRandomGen>()->SetSeed( pSeed );

		bStartGameReceived = true;
	}

	players = _players;
	for ( int i = 0; i < 16; ++i )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID && players[i].nClientID != -1 )
		{
			clientID2LogicID[players[i].nClientID] = players[i].nLogicID;
			players[i].lastTimeInfoAsked = 0;
		}
	}

	lags.resize( 16, false );
}
IMultiplayer::CCommand* CGamePlaying::GetCommand()
{
	if ( commands.empty() )
		return 0;
	else
	{
		pTakenCommand = commands.front();
		commands.pop_front();

		return pTakenCommand;
	}
}
void CGamePlaying::SendClientCommands( IDataStream *pPacket )
{
	pInGameNetDriver->SendBroadcast( pPacket );
}
void CGamePlaying::LeftGame()
{
	pInGameNetDriver = 0;
}
void CGamePlaying::RemoveClient( const int nClientID )
{
	if ( clientID2LogicID.find(nClientID) != clientID2LogicID.end() )
	{
		commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_PLAYER_REMOVED, clientID2LogicID[nClientID], -1, 0 ) );

		int i = 0;
		while ( i < players.size() && players[i].nClientID != nClientID )
			++i;

		if ( i < players.size() )
			players[i] = SPlayerInfo();
		
		clientID2LogicID.erase( nClientID );
	}
}
void CGamePlaying::ProcessPacket( const int nClientID, IDataStream *pPkt )
{
#if defined(_M_IX86)
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );
#else
	// x64 SSE has no x87 precision control; only DN/EM/RC bits are valid here
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _RC_NEAR | _DN_SAVE, _MCW_EM | _MCW_RC | _MCW_DN );
#endif
	if ( clientID2LogicID.find( nClientID ) != clientID2LogicID.end() )
	{
		const int nPlayer = clientID2LogicID[nClientID];
		CStreamAccessor pkt = pPkt;
		BYTE msgID = 0;
		while ( pkt->Read( &msgID, sizeof(msgID) ) == sizeof(msgID) ) 
		{
			IAILogicCommand *pAICmd = 0;
			switch ( msgID ) 
			{
				case NGM_ID_START_GAME:
					{
						CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
						pSeed->InitByZeroSeed();
						GetSingleton<IRandomGen>()->SetSeed( pSeed );

						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_START_GAME, -1, -1, 0 ) );

						bStartGameReceived = true;
					}

					break;
				case NGM_ID_SEGMENT:
					if ( bStartGameReceived )
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_SEGMENT_FINISHED, nPlayer, 0, 0 ) );

					break;
				case NGM_ID_COMMAND_REGISTER_GROUP:
					if ( bStartGameReceived )
					{
						pAICmd = new CRegisterGroupCommand();
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_AI_COMMAND, nPlayer, -1, pAICmd ) );
					}
					
					break;
				case NGM_ID_COMMAND_UNREGISTER_GROUP:
					if ( bStartGameReceived )
					{
						pAICmd = new CUnregisterGroupCommand();
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_AI_COMMAND, nPlayer, -1, pAICmd ) );
					}

					break;
				case NGM_ID_COMMAND_GROUP_COMMAND:
					if ( bStartGameReceived )
					{
						pAICmd = new CGroupCommand();
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_AI_COMMAND, nPlayer, -1, pAICmd ) );
					}

					break;
				case NGM_ID_COMMAND_UNIT_COMMAND:
					if ( bStartGameReceived )
					{
						pAICmd = new CUnitCommand();
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_AI_COMMAND, nPlayer, -1, pAICmd ) );
					}

					break;
				case NGM_ID_COMMAND_CHECK_SUM:
					if ( bStartGameReceived )
					{
						pAICmd = new CControlSumCheckCommand();
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_AI_COMMAND, nPlayer, -1, pAICmd ) );
					}

					break;
				case NGM_ID_COMMAND_DROP_PLAYER:
					if ( bStartGameReceived )
					{
						pAICmd = new CDropPlayerCommand();
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_AI_COMMAND, nPlayer, -1, pAICmd ) );
					}

					break;
				case NGM_PAUSE:
					if ( bStartGameReceived )
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_PAUSE, nPlayer, -1, 0 ) );

					break;
				case NGM_GAME_SPEED:
					if ( bStartGameReceived )
					{
						short int nChange;
						pkt >> nChange;
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_GAME_SPEED, nPlayer, nChange, 0 ) );
					}

					break;
				case NGM_TIMEOUT:
					if ( bStartGameReceived )
					{
						int nPlayerID, nSet;
						pkt >> nSet >> nPlayerID;
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_TIMEOUT, nPlayerID, nSet, 0 ) );
					}

					break;
				case NGM_IAM_ALIVE:
					if ( bStartGameReceived )
					{
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_PLAYER_ALIVE, nPlayer, 0, 0 ) );
					}

					break;
				case NGM_LEFT_GAME:
					if ( bStartGameReceived )
					{
						commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_PLAYER_REMOVED, nPlayer, 0, 0 ) );
					}

					break;
				default:
					return;
			}
			if ( pAICmd ) 
				pAICmd->Restore( pkt );
		}
	}
	else
		pInGameNetDriver->Kick( nClientID );
}
void CGamePlaying::UpdatePlayersInfo()
{
	const NTimer::STime time = GetSingleton<IGameTimer>()->GetAbsTime();

	for ( int i = 0; i < 16; ++i )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID && players[i].nLogicID != nOurID &&
				 ( time > players[i].lastTimeInfoAsked + SMultiplayerConsts::TIME_TO_ASK_PLAYER_INFO ||
					 time < players[i].lastTimeInfoAsked )
			 )
		{
			players[i].lastTimeInfoAsked = time;
			const float fTime = pInGameNetDriver->GetTimeSinceLastRecv( players[i].nClientID ) * 1000.0f;
			if ( fTime >= SMultiplayerConsts::TIME_TO_LAG_PLAYER && !lags[i] )
			{
				commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_PLAYER_LAG, players[i].nLogicID, 1, 0 ) );
				lags[i] = true;
			}
			else if ( fTime < SMultiplayerConsts::TIME_TO_LAG_PLAYER && lags[i] )
			{
				commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_PLAYER_LAG, players[i].nLogicID, 0, 0 ) );
				lags[i] = false;
			}
		}
	}
}
void CGamePlaying::ProcessNewClient( const int nClientID )
{
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	BYTE msgID = NGM_GAME_IS_ALREADY_STARTED;
	pkt << msgID;

	pInGameNetDriver->SendDirect( nClientID, pkt );
}
void CGamePlaying::Segment()
{
	if ( pInGameNetDriver )
	{
		INetDriver::EMessage eMsgID;
		int nClientID;
		int received[128];
		CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );

		while ( pInGameNetDriver->GetMessage(&eMsgID, &nClientID, received, pkt) )
		{
			switch ( eMsgID ) 
			{
				case INetDriver::NEW_CLIENT:
					ProcessNewClient( nClientID );
					break;
				case INetDriver::REMOVE_CLIENT:
					RemoveClient( nClientID );
					break;
				case INetDriver::BROADCAST:
				case INetDriver::DIRECT:
					ProcessPacket( nClientID, pkt );
					break;
			}
			pkt->SetSize( 0 );
		}
		
		UpdatePlayersInfo();
	}
}
const bool CGamePlaying::GetPlayerInfo( const WORD *pszPlayerName, SPlayerInfo *pInfo ) const
{
	std::wstring szPlayerName = MakeWideStringFromWordString( pszPlayerName );
	
	int i = 0;
	while ( i < 16 && ( players[i].eState == SPlayerInfo::EPS_VALID || players[i].szName != szPlayerName ) )
		++i;

	if ( i < 16 )
	{
		*pInfo = players[i];
		return true;
	}
	else
		return false;
}
const bool CGamePlaying::GetOurPlayerInfo( SPlayerInfo *pInfo ) const
{
	int i = 0;
	while ( i < 16 && players[i].nLogicID != nOurID )
		++i;

	NI_ASSERT_T( i < 16, "Can't find our player" );

	*pInfo = players[i];

	return true;
}
const int CGamePlaying::GetNAllies() const
{
	int cnt = 0;
	for ( int i = 0; i < players.size(); ++i )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID && players[i].nLogicID != -1 && /*players[i].nLogicID != nOurID &&*/
				 diplomacies[players[i].nLogicID] == diplomacies[nOurID] )
			++cnt;
	}

	return cnt;
}
const SPlayerInfo& CGamePlaying::GetAlly( const int n ) const
{
	int cnt = -1;
	int i = 0;
	while ( i < players.size() && cnt < n )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID && players[i].nLogicID != -1 && /*players[i].nLogicID != nOurID &&*/
				 diplomacies[players[i].nLogicID] == diplomacies[nOurID] )
			++cnt;

		if ( cnt < n )
			++i;
	}

	NI_ASSERT_T( cnt == n, NStr::Format( "Can't find an ally %d", n ) );

	return players[i];
}
void CGamePlaying::TogglePause()
{
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	pkt << BYTE( NGM_PAUSE );
	pInGameNetDriver->SendBroadcast( pkt );

	commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_PAUSE, nOurID, -1, 0 ) );
}
void CGamePlaying::GameSpeed( const int nChange )
{
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	pkt << BYTE( NGM_GAME_SPEED ) << static_cast<short>( nChange );
	pInGameNetDriver->SendBroadcast( pkt );

	commands.push_back( new IMultiplayer::CCommand( IMultiplayer::GPC_GAME_SPEED, nOurID, nChange, 0 ) );
}
void CGamePlaying::DropPlayer( const int nLogicID )
{
	CPlayers::iterator iter = players.begin();
	while ( iter != players.end() && ( iter->eState != SPlayerInfo::EPS_VALID || iter->nLogicID != nLogicID ) )
		++iter;

	if ( iter != players.end() )
	{
		const int nClientID = iter->nClientID;

		RemoveClient( nClientID );
		pInGameNetDriver->Kick( nClientID );
	}
}
int CGamePlaying::GetNumberOfPlayers() const
{
	int cnt = 0;
	for ( CPlayers::const_iterator iter = players.begin(); iter != players.end(); ++iter )
	{
		if ( iter->eState == SPlayerInfo::EPS_VALID )
			++cnt;
	}

	return cnt;
}
void CGamePlaying::CommandTimeOut( const bool bSet )
{
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	pkt << BYTE( NGM_TIMEOUT ) << int(bSet) << nOurID;

	pInGameNetDriver->SendBroadcast( pkt );
}
void CGamePlaying::SendAliveMessage()
{
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	pkt << BYTE( NGM_IAM_ALIVE );

	pInGameNetDriver->SendBroadcast( pkt );
}
void CGamePlaying::FinishGame()
{
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	pkt << BYTE( NGM_LEFT_GAME );

	pInGameNetDriver->SendBroadcast( pkt );
}
