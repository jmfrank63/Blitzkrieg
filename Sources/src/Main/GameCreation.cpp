#include "stdafx.h"

#include "GameCreation.h"
#include "ServerInfo.h"
#include "GameCreationMessages.h"
#include "GamePlaying.h"
#include "NetMessages.h"
#include "MultiplayerConsts.h"
#include "CommandsHistory.h"

#include "..\RandomMapGen\Resource_Types.h"
#include "..\RandomMapGen\MapInfo_Types.h"

#include "..\StreamIO\StreamIOHelper.h"
#include "..\StreamIO\StreamIOTypes.h"

#include "..\Net\NetDriver.h"
#include "..\StreamIO\OptionsConvert.h"

#include "..\zlib\zlib.h"
#include "..\zlib\zconf.h"

// for debug
#if !defined(_FINALRELEASE) || defined(_DEVVERSION)
#define DEBUG_NET_MESSAGES
#endif // !defined(_FINALRELEASE) || defined(_DEVVERSION)

#ifdef DEBUG_NET_MESSAGES
#include "..\StreamIO\Globals.h"
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( IGameCreation );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const NTimer::STime TIME_TO_SEND_PING_MESSAGE = 200;
//const NTimer::STime PERIOD_OF_TIME_TO_SEND_PING_MESSAGES = 15000;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WriteDebugMessage( const char *pszMessage )
{
#ifdef DEBUG_NET_MESSAGES
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, pszMessage, 0xffffff00, true );
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*		 								   CCommonGameCreationInfo										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCommonGameCreationInfo::Init()
{
	lastPingMessageTime = 0;
	startSendMessagesTime = 0;

	std::unordered_set<BYTE> channelMessages;

	channelMessages.insert( BYTE( NGM_SEND_ME_MAP ) );
	channelMessages.insert( BYTE( NGM_TOTAL_PACKED_SIZE ) );
	channelMessages.insert( BYTE( NGM_PACKED_FILE_INFO ) );
	channelMessages.insert( BYTE( NGM_FILE_INFO ) );
	channelMessages.insert( BYTE( NGM_PACKET ) );
	channelMessages.insert( BYTE( NGM_FINISHED ) );
	channelMessages.insert( BYTE( NGM_STREAM_FINISHED ) );


	pInGameNetDriver->AddChannel( 2, channelMessages );
	packedInfo.bPacked = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CCommonGameCreationInfo::GetPlayerInfo( const WORD *pszPlayerName, SPlayerInfo *pInfo ) const
{
	std::wstring szPlayerName = pszPlayerName;
	
	int i = 0;
	while ( i < 16 && ( players[i].eState != SPlayerInfo::EPS_VALID || players[i].szName != szPlayerName ) )
		++i;

	if ( i < 16 )
	{
		*pInfo = players[i];
		return true;
	}
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CCommonGameCreationInfo::GetOurPlayerInfo( SPlayerInfo *pInfo, const int nOurLogicID ) const
{
	if ( nOurLogicID == -1 )
		return false;
	else
	{
		*pInfo = players[nOurLogicID];
		return true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCommonGameCreationInfo::CanStartGame() const
{
	std::vector<int> playersInSide( sides.size(), 0 );
	int nPlayers = 0;
	for ( int i = 0; i < 16; ++i )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID )
		{ 
			if ( !players[i].bReady )
				return false;

			++playersInSide[players[i].nSide];
			++nPlayers;
		}
	}

	for ( int i = 0; i < sides.size() - 1; ++i )
	{
		if ( playersInSide[i] > sides[i].nMaxPlayers )
			return false;
	}

	if ( nPlayers > gameInfo.nMaxPlayers )
		return false;

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCommonGameCreationInfo::IsAllPlayersInOneParty() const
{
	int nParty = -1;
	int nPlayers = 0;
	bool bIsRandomPlayers = false;
	for ( int i = 0; i < 16; ++i )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID )
		{
			++nPlayers;
			if ( players[i].nSide == sides.size() - 1 )
				bIsRandomPlayers = true;
			else
			{
				if ( nParty == -1 )
					nParty = players[i].nSide;
				else if ( players[i].nSide != nParty )
					return false;
			}
		}
	}

	return nPlayers <= 1 || !bIsRandomPlayers;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCommonGameCreationInfo::LoadSidesInformation()
{
	sides.clear();
	for ( int i = 0; i < mapInfo.playerParties.size(); ++i )
	{
		int j = 0;
		while ( j < sides.size() && sides[j].szName != mapInfo.playerParties[i] )
			++j;

		if ( j >= sides.size() )
			sides.push_back( SSideInfo( mapInfo.playerParties[i].c_str(), 1 ) );
		else
			++sides[j].nMaxPlayers;
	}

	if ( sides.size() == 1 )
		sides.push_back( sides[0] );

	sides.push_back( SSideInfo( "Random", 100000 ) );

	gameInfo.nMaxPlayers = Min( (int)gameInfo.nMaxPlayers, (int)mapInfo.diplomacies.size() - 1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCommonGameCreationInfo::LoadMapInfo( const bool bServer, const bool bNeedCheckSums )
{
	if ( !bServer )
	{
		LoadLatestDataResource(
			"maps\\" + gameInfo.szMapName, ".bzm", 
			RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER, RMGC_QUICK_LOAD_MAP_INFO_NAME, 
			mapInfo
		);
	}

	bool bMapLoaded = false;
	{
		CMapInfo fullMapInfo;
		bMapLoaded = LoadTypedSuperLatestDataResource( "maps\\" + gameInfo.szMapName, ".bzm", 1, fullMapInfo, &packedInfo.szMapFileName );
		if ( bMapLoaded )
		{
			if ( bNeedCheckSums )
				fullMapInfo.GetCheckSums( &gameInfo.checkSumRes, &gameInfo.checkSumMap );

			const int nPos = packedInfo.szMapFileName.find_last_of( '\\' );

			if ( fullMapInfo.szScriptFile.empty() )
				packedInfo.szScriptFileName = "";
			else
			{
				packedInfo.szScriptFileName.clear();
				packedInfo.szScriptFileName.assign( packedInfo.szMapFileName.begin(), packedInfo.szMapFileName.begin() + nPos );
				
				int nScriptPos = fullMapInfo.szScriptFile.find_last_of( '\\' );
				if ( nScriptPos > fullMapInfo.szScriptFile.size() || nScriptPos < 0 )
				{
					nScriptPos = 0;
					packedInfo.szScriptFileName += '\\';
				}
				packedInfo.szScriptFileName.append( fullMapInfo.szScriptFile.begin() + nScriptPos, fullMapInfo.szScriptFile.end() );
				packedInfo.szScriptFileName += ".lua";
			}

			packedInfo.szTXTFileName = packedInfo.szMapFileName;
			packedInfo.szTXTFileName[packedInfo.szTXTFileName.size() - 3] = 't';
			packedInfo.szTXTFileName[packedInfo.szTXTFileName.size() - 2] = 'x';
			packedInfo.szTXTFileName[packedInfo.szTXTFileName.size() - 1] = 't';
		}
	}
	
	if ( bMapLoaded )
		LoadSidesInformation();		

	return bMapLoaded;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCommonGameCreationInfo::DistributePlayersNumbers()
{
	std::vector<int> playersInSide( sides.size(), 0 );
	for ( int i = 0; i < 16; ++i )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID )
			++playersInSide[players[i].nSide];
	}

	// distribution by sides
	int nPlayer = 0;
	while ( true )
	{
		// look for first random-side player
		while ( nPlayer < 16 && ( players[nPlayer].eState != SPlayerInfo::EPS_VALID || players[nPlayer].nSide < sides.size() - 1 ) )
			++nPlayer;

		if ( nPlayer >= 16 )
			break;

		// find weakest sides
		std::list<int> weakestSides;
		int nPlayersInWeakestSide = 16 * 2;
		for ( int i = 0; i < playersInSide.size(); ++i )
		{
			if ( playersInSide[i] < sides[i].nMaxPlayers )
			{
				if ( playersInSide[i] < nPlayersInWeakestSide )
				{
					nPlayersInWeakestSide = playersInSide[i];
					weakestSides.clear();
					weakestSides.push_back( i );
				}
				else if ( playersInSide[i] == nPlayersInWeakestSide )
					weakestSides.push_back( i );
			}
		}
		NI_ASSERT_T( !weakestSides.empty(), "Cant find side to push the player" );

		// choose a random weakest side
		int n = ( float(rand()) / float(RAND_MAX) ) * float(weakestSides.size());
		if ( n >= weakestSides.size() )
			n = weakestSides.size() - 1;

		std::list<int>::iterator iter = weakestSides.begin();
		std::advance( iter, n );
		const int nSide = *iter;

		// set player to the chosen side
		players[nPlayer].nSide = nSide;
		++playersInSide[nSide];
	}

	// distribution by players
	for ( int i = 0; i < sides.size() - 1; ++i )
	{
		// players of side i
		std::vector<int> allPlayers;
		for ( int j = 0; j < 16; ++j )
		{
			if ( players[j].eState == SPlayerInfo::EPS_VALID && players[j].nSide == i )
				allPlayers.push_back( j );
		}

		// dump players
		while ( allPlayers.size() < sides[i].nMaxPlayers )
			allPlayers.push_back( -1 );

		std::random_shuffle( allPlayers.begin(), allPlayers.end() );

		std::vector<int>::const_iterator iter = allPlayers.begin();
		for ( int nPlayer = mapInfo.playerParties.size() - 1; nPlayer >= 0; --nPlayer )
		{
			if ( mapInfo.playerParties[nPlayer] == sides[i].szName )
			{
				if ( *iter != -1 )
					players[*iter].nLogicID = nPlayer;

				++iter;
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCommonGameCreationInfo::SetGlobalVars( const int nOurLogicID )
{
	// write all players info to global vars
	int i = 0;
	std::string szValueName;
	int nPlayers = 0;
	for ( int i = 0; i < 16; ++i )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID )
		{
			szValueName = NStr::Format( "Multiplayer.Player%d.Name", nPlayers );
			SetGlobalVar( szValueName.c_str(), players[i].szName.c_str() );

			szValueName = NStr::Format( "Multiplayer.Player%d.Side", nPlayers );
			SetGlobalVar( szValueName.c_str(), int(players[i].nSide) );
			
			szValueName = NStr::Format( "Multiplayer.Player%d.LogicID", nPlayers );
			SetGlobalVar( szValueName.c_str(), int(players[i].nLogicID) );

			++nPlayers;
		}
	}
	
	for ( int i = 0; i < sides.size(); ++i )
	{
		szValueName = NStr::Format( "Multiplayer.Side%d.Name", i );
		SetGlobalVar( szValueName.c_str(), sides[i].szName.c_str() );
	}

	SetGlobalVar( "Multiplayer.OurPlayerID", nOurLogicID );
	SetGlobalVar( "Multiplayer.NumPlayersInMap", int(mapInfo.diplomacies.size() - 1) );
	SetGlobalVar( "Multiplayer.NumGamingPlayers", nPlayers );

	SetGlobalVar( "Multiplayer.MapName", gameInfo.szMapName.c_str() );

	SetGlobalVar( "Multiplayer.GameType", int(gameInfo.eGameType) );
	SetGlobalVar( "Multiplayer.AttackingParty", int( mapInfo.nAttackingSide ) );
	SetGlobalVar( "Multiplayer.GameSettings.FlagScoreLimit", gameInfo.gameSettings.nFlagScoreLimit );
	SetGlobalVar( "Multiplayer.GameSettings.KillScoreLimit", gameInfo.gameSettings.nKillScoreLimit );
	SetGlobalVar( "Multiplayer.GameSettings.TimeToCapture", gameInfo.gameSettings.nTimeToCapture );
	SetGlobalVar( "Multiplayer.GameSettings.TimeLimit", gameInfo.gameSettings.nTimeLimit );
	SetGlobalVar( "Multiplayer.CheckSumMap", gameInfo.checkSumMap );
	SetGlobalVar( "Multiplayer.CheckSumRes", gameInfo.checkSumRes );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCommonGameCreationInfo::UpdatePlayersInfo()
{
	const NTimer::STime time = GetSingleton<IGameTimer>()->GetAbsTime();

	for ( CPlayers::iterator iter = players.begin(); iter != players.end(); ++iter )
	{
		if ( iter->eState == SPlayerInfo::EPS_VALID && time > iter->lastTimeInfoAsked + SMultiplayerConsts::TIME_TO_ASK_PLAYER_INFO )
		{
			iter->lastTimeInfoAsked = time;
			const float fPing = pInGameNetDriver->GetPing( iter->nClientID );
			if ( fPing != -1.0f )
			{
				iter->fPing = fPing;
				messages.AddMessage( new CPlayerInfoRefreshed( *iter, sides[iter->nSide].szName.c_str() ) );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCommonGameCreationInfo::SendPingMessage()
{
	const NTimer::STime time = GetSingleton<IGameTimer>()->GetAbsTime();
	if ( startSendMessagesTime == 0 )
		startSendMessagesTime = time;

	// ������, ����� �� ���������� ping � ���� ����� ping message
	if ( /*time < startSendMessagesTime + PERIOD_OF_TIME_TO_SEND_PING_MESSAGES &&*/
			 time > lastPingMessageTime + TIME_TO_SEND_PING_MESSAGE )
	{
		lastPingMessageTime = time;

		CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
		BYTE msgID = NGM_PING;
		pkt << msgID;
		pInGameNetDriver->SendBroadcast( pkt );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*		 					CCommonGameCreationInfo::SPackedInfo								*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCommonGameCreationInfo::SPackedInfo::PackFile( const std::string szFileName, std::vector<BYTE> &packedFile, int &nRealSize )
{
	packedFile.clear();
	nRealSize = 0;
	
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szFileName.c_str(), STREAM_ACCESS_READ );

	if ( pStream )
	{
		nRealSize = pStream->GetSize();
		std::vector<BYTE> realFile( nRealSize );
		pStream->Read( &(realFile[0]), nRealSize );

		packedFile.resize( realFile.size() + 100000 );
		
		// inflate map
		z_stream stream;
		stream.next_in = (Bytef*)(&(realFile[0]));
		stream.avail_in = realFile.size();

		stream.next_out = (Bytef*)(&(packedFile[0]));
		stream.avail_out = packedFile.size();
		stream.zalloc = (alloc_func)0;
		stream.zfree = (free_func)0;

		// Perform inflation. wbits < 0 indicates no zlib header inside the data.
		int err = deflateInit2( &stream, 9, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY );
		if ( err == Z_OK )
		{
			err = deflate( &stream, Z_FINISH );
			deflateEnd( &stream );
			// CRAP{ ������-�� ������ ��� ���������� ������������ "buffer error" ������ "stream end"...
			if ( (err == Z_STREAM_END) || (err == Z_BUF_ERROR) )
				err = Z_OK;
			// CRAP}
			deflateEnd( &stream );

			packedFile.resize( stream.next_out - &(packedFile[0]) );

			WriteDebugMessage( NStr::Format( "Map packed, real size %d, packed size %d", nRealSize, packedMap.size() ) );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCommonGameCreationInfo::SPackedInfo::LoadAllFiles()
{
	PackFile( szMapFileName, packedMap, nRealMapSize );
	if ( !szScriptFileName.empty() )
		PackFile( szScriptFileName, packedScript, nRealScriptSize );

	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szTXTFileName.c_str(), STREAM_ACCESS_READ );
	txtFile.clear();
	if ( pStream )
	{
		txtFile.resize( pStream->GetSize() );
		pStream->Read( &(txtFile[0]), txtFile.size() );
	}

	bPacked = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*		 								   CServerGameCreation												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::Init( INetDriver *_pInGameNetDriver, INetDriver *_pOutGameNetDriver,
																const SGameInfo &_gameInfo, const SQuickLoadMapInfo &_mapInfo )
{
	gameInfo = _gameInfo;
	gameInfo.nCurPlayers = 1;
	mapInfo = _mapInfo;

	pInGameNetDriver = _pInGameNetDriver;
	pOutGameNetDriver = _pOutGameNetDriver;

	CCommonGameCreationInfo::Init();

	const bool bLoaded = LoadMapInfo( true, true );
	NI_ASSERT_T( bLoaded == true, "failed to load map on server" );

	pOutGameNetDriver->StartGame();
	SendGameInfoOutside();

	pInGameNetDriver->StartGame();
	pInGameNetDriver->StartNewPlayerAccept();

	//
	players.clear();
	players.resize( 17 );
	players[0].nClientID = -1;
	players[0].nLogicID = 0;
	players[0].nSide = sides.size() - 1;
	players[0].bReady = true;
	players[0].fPing = 0;
	players[0].eState = SPlayerInfo::EPS_VALID;
	NStr::SetCodePage( GetACP() );
	
	players[0].szName = GetGlobalWVar( "Options.Multiplayer.GameSpyPlayerName", L"Noname" );
	if ( players[0].szName == L"Noname" )
		players[0].szName = NStr::ToUnicode( GetGlobalVar( "Options.Multiplayer.PlayerName", "Noname" ) );

	messages.AddMessage( new CGameInfoReceived( gameInfo, true, 0 ) );
	messages.AddMessage( new CPlayerInfoRefreshed( players[0], sides[players[0].nSide].szName.c_str() ) );

	bCanStartGame = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::SendGameInfoOutside()
{
	SServerInfo info( gameInfo );
	INetDriver::SGameInfo driverGameInfo;

	info.Pack( &driverGameInfo );
	pOutGameNetDriver->StartGameInfoSend( driverGameInfo );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::LeftGame()
{
	if ( pOutGameNetDriver )
		pOutGameNetDriver->StopGameInfoSend();
	pInGameNetDriver->StopGameInfoSend();
	
	pInGameNetDriver = 0;
	pOutGameNetDriver = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::SendConnectionFailed()
{
	INetDriver::EReject eReason;
	if ( pInGameNetDriver->GetState() == INetDriver::INACTIVE )
		eReason = pInGameNetDriver->GetRejectReason();
	else
		eReason = pOutGameNetDriver->GetRejectReason();
	
#ifdef DEBUG_NET_MESSAGES
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, "__ConnectionFailed", 0xffffff00, true );
#endif

 	messages.AddMessage( new CConnectionFailed( eReason ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CServerGameCreation::CheckConnection()
{
	if ( pInGameNetDriver )
	{
		if ( pInGameNetDriver->GetState() == INetDriver::INACTIVE ||
				 pOutGameNetDriver && pOutGameNetDriver->GetState() == INetDriver::INACTIVE )
		{
			SendConnectionFailed();

			pInGameNetDriver = 0;
			pOutGameNetDriver = 0;

			return false;
		}

		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CServerGameCreation::CreateNewLogicID()
{
	int i = 0;
	while ( i < 16 && players[i].eState != SPlayerInfo::EPS_INVALID )
		++i;

	return ( i < 16 ) ? i : -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::ReadPlayerInfo( int nClientID, CStreamAccessor &pkt )
{
	SPlayerInfo info;
	info.Unpack( pkt );

	players[info.nLogicID] = info;
	players[info.nLogicID].nClientID = nClientID;

	if ( info.nSide >= 0 )
		players[info.nLogicID].eState = SPlayerInfo::EPS_VALID;

	std::string szSideName = info.nSide >= 0 ? sides[info.nSide].szName.c_str() : "Random";
	messages.AddMessage( new CPlayerInfoRefreshed( info, szSideName.c_str() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::ConstructGameInfoPacket( CStreamAccessor &pkt )
{
	const BYTE msgID = NGM_GAME_INFO;
	pkt->SetSize( 0 );
	pkt << msgID;
	gameInfo.Pack( pkt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::ProcessNewClient( int nClientID, CStreamAccessor &pkt )
{
	// ������������ ������ ������� � ����� ��� logicID
	const int nLogicID = CreateNewLogicID();
	if ( nLogicID < 0 || nLogicID >= 16 )
	{
		pInGameNetDriver->Kick( nClientID );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "__client %d kicked", nClientID ), 0xffffff00, true );
#endif
	}
	else
	{
		players[nLogicID].nClientID = nClientID;
		players[nLogicID].nLogicID = nLogicID;
		players[nLogicID].szName = NStr::ToUnicode( "NewPlayer" );
		players[nLogicID].nSide = sides.size() - 1;
		players[nLogicID].eState = SPlayerInfo::EPS_CONNECTED;

		// �������� game info
		++gameInfo.nCurPlayers;

		ConstructGameInfoPacket( pkt );
		pInGameNetDriver->SendDirect( nClientID, pkt );

		// �������� ����� ������� ��� ����� logic ID ��� 'DIRECT'
		const BYTE msgID = NGM_LOGIC_ID;
		pkt->SetSize( 0 );
		pkt << msgID << nLogicID;
		pInGameNetDriver->SendDirect( nClientID, pkt );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_GAME_INFO to client %d sent", nClientID ), 0xffffff00, true );
#endif

		// ������� ������, ��� ���������� ������� ����������
		SendGameInfoOutside();

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_LOGIC_ID sent, client %d added, id %d", nClientID, nLogicID ), 0xffffff00, true );
#endif
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::ChoosePlayerName( int nClientID, CStreamAccessor &pkt )
{
	int nLogicID;
	std::wstring szSentName;
	pkt >> nLogicID >> szSentName;

	std::vector<int> usedIndexes( 17, 0 );
	bool bUsed = false;
	for ( int i = 0; i < 16; ++i )
	{
		if ( players[i].nLogicID != nLogicID && players[i].eState != SPlayerInfo::EPS_INVALID )
		{
			std::wstring szPlayerName = players[i].szName;

			if ( szSentName == szPlayerName )
				bUsed = true;
			else
			{
				const int nLen = szPlayerName.size();
				int nNumber = -1;
				if ( nLen >= 1 && szPlayerName[nLen-1] == ')' )
				{
					int nIndex = nLen - 2;
					std::wstring szSubString = L")";
					while ( nIndex >= 0 && szPlayerName[nIndex] >= '0' && szPlayerName[nIndex] <= '9' )
					{
						nNumber = ( nNumber < 0 ) ? 
											( szPlayerName[nIndex] - '0' ) : ( szPlayerName[nIndex] - '0' ) * 10 + nNumber;

						szSubString = szPlayerName[nIndex] + szSubString;
						--nIndex;
					}

					if ( nIndex >= 1 && szPlayerName[nIndex] == '(' && nNumber >= 1 && nNumber <= 16 )
					{
						szSubString = L"(" + szSubString;
						if ( szSentName + szSubString == szPlayerName )
							usedIndexes[nNumber] = 1;
					}
				}
			}
		}
	}

	if ( bUsed )
	{
		int i = 1;
		while ( i <= 16 && usedIndexes[i] == 1 )
			++i;
		NI_ASSERT_T( i <= 16, "Can't find index for player new name" );

		std::wstring szPostfix;
		NStr::ToUnicode( &szPostfix, NStr::Format( "(%d)", i ) );

		szSentName = szSentName + szPostfix;
	}

	players[nLogicID].szName = szSentName;

	pkt->SetSize( 0 );
	BYTE msgID = NGM_ANSWER_PLAYER_NAME;
	pkt << msgID << szSentName;
	pInGameNetDriver->SendDirect( nClientID, pkt );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_ANSWER_PLAYER_NAME sent to client %d", nClientID, szSentName.c_str() ), 0xffffff00, true );
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::ProcessDirectMessage( int nClientID, CStreamAccessor &pkt )
{
	BYTE msgID = -1;
	pkt >> msgID;

	switch ( msgID )
	{
		case NGM_DIRECT_PLAYER_INFO:
			break;
		case NGM_ASK_FOR_NAME:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_ASK_FOR_NAME from %d received", nClientID ), 0xffffff00, true );
#endif

			ChoosePlayerName( nClientID, pkt );
			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::ProcessBroadcastMessage( int nClientID, CStreamAccessor &pkt )
{
	BYTE msgID = -1;
	pkt >> msgID;

	switch ( msgID )
	{
		case NGM_BROADCAST_PLAYER_INFO:
			{
				ReadPlayerInfo( nClientID, pkt );
				// ����� �� 'nClientID' ��� ���� � ���� ��� 'DIRECT'
				pkt->SetSize( 0 );
				BYTE msgID = NGM_DIRECT_PLAYER_INFO;
				pkt << msgID;
				players[0].Pack( pkt );
				pInGameNetDriver->SendDirect( nClientID, pkt );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_BROADCAST_PLAYER_INFO from  %d received", nClientID ), 0xffffff00, true );
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_DIRECT_PLAYER_INFO to %d sent", nClientID ), 0xffffff00, true );
#endif
			}

			break;
		case NGM_PLAYER_LEFT:
			break;
		case NGM_PLAYER_KICKED:
			break;
		case NGM_PING:
			break;
		default:
			NI_ASSERT_T( false, NStr::Format( "Unknown message (%d) received by the server", msgID ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::ProcessRemoveClient( int nClientID, CStreamAccessor &pkt )
{
	int i = 0;
	while ( i < 16 && players[i].nClientID != nClientID )
		++i;

	if ( i < 16 )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID )
			messages.AddMessage( new CPlayerDeleted( i, players[i].szName, CPlayerDeleted::ER_LEFT ) );

		players[i].eState = SPlayerInfo::EPS_INVALID;
		players[i].nClientID = -1;

		// ������� ����, ��� player �����
		pkt->SetSize( 0 );
		BYTE msgID = NGM_PLAYER_LEFT;
		pkt << msgID << players[i].nLogicID;
		pInGameNetDriver->SendBroadcast( pkt );

		// �������� gameInfo
		--gameInfo.nCurPlayers;
		NI_ASSERT_T( gameInfo.nCurPlayers > 0, "Wrong number of players in the game" );
		// ������� ������, ��� ���������� ������� ����������
		SendGameInfoOutside();

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_PLAYER_LEFT, client %d sent", nClientID ), 0xffffff00, true );
#endif
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::ProcessMessages()
{
	INetDriver::EMessage eMsgID;
	int nClientID;
	int received[128];
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	//
	while ( pInGameNetDriver && pInGameNetDriver->GetMessage(&eMsgID, &nClientID, received, pkt) )
	{
		switch ( eMsgID ) 
		{
			case INetDriver::DIRECT:
				ProcessDirectMessage( nClientID, pkt );
				break;
			case INetDriver::BROADCAST:
				ProcessBroadcastMessage( nClientID, pkt );
				break;
			case INetDriver::NEW_CLIENT:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NEW_CLIENT %d received", nClientID ), 0xffffff00, true );
#endif
				ProcessNewClient( nClientID, pkt );

				break;
			case INetDriver::REMOVE_CLIENT:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "REMOVE_CLIENT %d received", nClientID ), 0xffffff00, true );
#endif

				ProcessRemoveClient( nClientID, pkt );
				
				break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::SendPacketStream( const int nClientID, const std::vector<BYTE> &stream )
{
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	
	int cnt = 0;
	while ( cnt < stream.size() )
	{
		pkt->SetSize( 0 );
		BYTE cMsg = NGM_PACKET;
		pkt << cMsg;

		const int nSizeToSend = Min( (int)7000, (int)stream.size() - cnt );
		pkt->Write( &(stream[cnt]), nSizeToSend );
		pInGameNetDriver->SendDirect( nClientID, pkt );
		cnt += nSizeToSend;
	}

	pkt->SetSize( 0 );
	BYTE cMsg = NGM_STREAM_FINISHED;
	pkt << cMsg;
	pInGameNetDriver->SendDirect( nClientID, pkt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::UpdateLoadMap()
{
	INetDriver::EMessage eMsgID;
	int nClientID;
	int received[128];
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	
	while ( pInGameNetDriver && pInGameNetDriver->GetChannelMessage( &eMsgID, &nClientID, received, pkt, 2 ) )
	{
		if ( eMsgID == INetDriver::DIRECT )
		{
			BYTE msg;
			pkt >> msg;
			if ( msg == NGM_SEND_ME_MAP )
			{
				WriteDebugMessage( NStr::Format( "Send map query" ) );
				if ( !packedInfo.bPacked )
					packedInfo.LoadAllFiles();

				if ( packedInfo.bPacked )
				{
					{
						pkt->SetSize( 0 );
						BYTE cMsg = NGM_TOTAL_PACKED_SIZE;
						pkt << cMsg << packedInfo.packedMap.size() + packedInfo.packedScript.size() + packedInfo.txtFile.size();
						pInGameNetDriver->SendDirect( nClientID, pkt );
					}
					
					{
						WriteDebugMessage( NStr::Format( "Send real map size %d, packed size %d", packedInfo.nRealMapSize, packedInfo.packedMap.size() ) );

						pkt->SetSize( 0 );
						BYTE cMsg = NGM_PACKED_FILE_INFO;
						pkt << cMsg << packedInfo.szMapFileName << packedInfo.packedMap.size() << packedInfo.nRealMapSize;
						pInGameNetDriver->SendDirect( nClientID, pkt );

						SendPacketStream( nClientID, packedInfo.packedMap );
					}

					if ( !packedInfo.szScriptFileName.empty() )
					{
						WriteDebugMessage( NStr::Format( "Send real script file size %d, packed size %d", packedInfo.nRealScriptSize, packedInfo.packedScript.size() ) );

						pkt->SetSize( 0 );
						BYTE cMsg = NGM_PACKED_FILE_INFO;
						pkt << cMsg << packedInfo.szScriptFileName << packedInfo.packedScript.size() << packedInfo.nRealScriptSize;
						pInGameNetDriver->SendDirect( nClientID, pkt );

						SendPacketStream( nClientID, packedInfo.packedScript );
					}

					{
						WriteDebugMessage( NStr::Format( "Send txt file size %d", packedInfo.txtFile.size() ) );

						pkt->SetSize( 0 );
						BYTE cMsg = NGM_FILE_INFO;
						pkt << cMsg << packedInfo.szTXTFileName << packedInfo.txtFile.size();
						pInGameNetDriver->SendDirect( nClientID, pkt );

						SendPacketStream( nClientID, packedInfo.txtFile );
					}

					{
						pkt->SetSize( 0 );
						BYTE cMsg = NGM_FINISHED;
						pkt << cMsg;
						pInGameNetDriver->SendDirect( nClientID, pkt );
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::Segment()
{
	if ( CheckConnection() )
	{
		SendPingMessage();
		ProcessMessages();
		if ( pInGameNetDriver )
		{
			UpdatePlayersInfo();

			if ( bCanStartGame ^ CanStartGame() )
			{
				bCanStartGame = !bCanStartGame;
				messages.AddMessage( new CCanStartGameState( bCanStartGame ) );
			}

			UpdateLoadMap();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::KickPlayer( const int nLogicID )
{
	if ( players[nLogicID].eState != SPlayerInfo::EPS_INVALID )
	{
		if ( players[nLogicID].eState == SPlayerInfo::EPS_VALID )
			messages.AddMessage( new CPlayerDeleted( nLogicID, players[nLogicID].szName, CPlayerDeleted::ER_KICKED ) );

		const int nClientID = players[nLogicID].nClientID;

		players[nLogicID].eState = SPlayerInfo::EPS_INVALID;
		players[nLogicID].nClientID = -1;

		// ������� ����, ��� player �����
		CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
		BYTE msgID = NGM_PLAYER_KICKED;
		pkt << msgID << nLogicID;
		pInGameNetDriver->SendBroadcast( pkt );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_PLAYER_KICKED, client %d", nLogicID ), 0xffffff00, true );
#endif

		// �������� gameInfo
		--gameInfo.nCurPlayers;
		NI_ASSERT_T( gameInfo.nCurPlayers > 0, "Wrong number of players in the game" );
		// ������� ������, ��� ���������� ������� ����������
		SendGameInfoOutside();

		pInGameNetDriver->Kick( nClientID );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::ChangePlayerSettings( const SPlayerInfo &info, const EPlayerSettings &eSettingsType )
{
	switch ( eSettingsType )
	{
		case EPS_READY: 
			players[0].bReady = info.bReady;
			break;
		case EPS_SIDE:	
			if ( sides.size() > 0 )
				players[0].nSide = ( players[0].nSide + 1 ) % sides.size();

			break;
		case EPS_NAME:
			players[0].szName = info.szName;
			break;
	}

	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );

	BYTE msgID = NGM_BROADCAST_PLAYER_INFO;
	pkt << msgID;
	players[0].Pack( pkt );
	pInGameNetDriver->SendBroadcast( pkt );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_BROADCAST_PLAYER_INFO sent, settings" ), 0xffffff00, true );
#endif
	
	//
	const std::string szSide = sides.size() > 0 ? sides[players[0].nSide].szName : "Random";
	messages.AddMessage( new CPlayerInfoRefreshed( players[0], szSide.c_str() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IGamePlaying* CServerGameCreation::CreateGamePlaying()
{
	pInGameNetDriver->StopNewPlayerAccept();
	
	gameInfo.eState = SServerInfo::ESS_IN_GAME;
	SendGameInfoOutside();

	const int nGameSpeed = NOptionsConvert::GetSpeed( gameInfo.gameSettings.szGameSpeed );
	gameInfo.gameSettings.nTimeLimit *= NTimer::GetCoeffFromSpeed( nGameSpeed );
	gameInfo.gameSettings.nTimeToCapture *= NTimer::GetCoeffFromSpeed( nGameSpeed );

	int nPlayers = 0;
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	BYTE msgID = NGM_GAME_STARTED;
	pkt << msgID;
	for ( int i = 0; i < 16; ++i )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID )
		{
			pkt << players[i].nLogicID;
			++nPlayers;
		}
		else if ( players[i].eState == SPlayerInfo::EPS_CONNECTED )
		{
			CStreamAccessor pkt1 = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
			const BYTE msgID = NGM_GAME_IS_ALREADY_STARTED;
			pkt1 << msgID;
			pInGameNetDriver->SendDirect( players[i].nClientID, pkt1 );
		}
	}
	pkt << -1;
	// distribute player numbers to players
	DistributePlayersNumbers();

	// load randomseed and commands history
	// ���� history ���������, �� ���������� ������������� �������, ��� �������� � history
	ICommandsHistory *pHistory = GetSingleton<ICommandsHistory>();
	if ( pHistory->LoadCommandLineHistory() )
	{
		// for compatibility with legacy histories
		if ( pHistory->GetNumPlayersInMPGame() > 0 )
		{
//			NI_ASSERT_T( nPlayers == pHistory->GetNumPlayersInMPGame(), NStr::Format("Wrong number of players %d, %d expected", nPlayers, pHistory->GetNumPlayersInMPGame() ) );

			int cnt = 0;
			for ( int i = 0; i < 16; ++i )
			{
				if ( players[i].eState == SPlayerInfo::EPS_VALID )
				{
					players[i].nLogicID = pHistory->GetMPPlayerLogicID( cnt );
					players[i].nSide = pHistory->GetMPPlayerSide( cnt );
					++cnt;
				}
			}
		}

		SetGlobalVar( "LoadCommandLineHistory", 1 );
	}
	else
		RemoveGlobalVar( "LoadCommandLineHistory" );

	RemoveGlobalVar( "History.Playing" );

	for ( int i = 0; i < 16; ++i )
	{
		if ( players[i].eState == SPlayerInfo::EPS_VALID )
		{
			pkt << players[i].nLogicID;
			pkt << players[i].nSide;
		}
	}
	pInGameNetDriver->SendBroadcast( pkt );

	SetGlobalVars( players[0].nLogicID );

	IGamePlaying *pGamePlaying = new CGamePlaying();
	pGamePlaying->Init( pInGameNetDriver, pOutGameNetDriver, players, true, players[0].nLogicID, mapInfo.diplomacies );

	//
	pGamePlaying->GameSpeed( nGameSpeed );

	return pGamePlaying;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerGameCreation::SetNewGameSettings( const SMultiplayerGameSettings &settings )
{
	gameInfo.gameSettings = settings;
	SendGameInfoOutside();

	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	const BYTE msgID = NGM_GAME_SETTINGS_CHANGED;
	pkt << msgID;
	settings.Pack( pkt );
	pInGameNetDriver->SendBroadcast( pkt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*		 								   CClientGameCreation												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::Init( INetDriver *_pInGameNetDriver, bool bPasswordRequired, const std::string &szPassword )
{
	gameInfo.bPasswordRequired = bPasswordRequired;
	gameInfo.szPassword = szPassword;
	
	pInGameNetDriver = _pInGameNetDriver;
	nOurLogicID = -1;
	players.resize( 17 );
	players[16].nSide = 2;
	players[0].nClientID = -1;
	bGameStarted = false;

	bModChanged = false;

	CCommonGameCreationInfo::Init();

	sides.resize( 3 );
	sides[0].szName = sides[1].szName = sides[2].szName = "Random";
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::SendConnectionFailed()
{
	INetDriver::EReject eReason;
	if ( pInGameNetDriver->GetState() == INetDriver::INACTIVE )
		eReason = pInGameNetDriver->GetRejectReason();

	messages.AddMessage( new CConnectionFailed( eReason ) );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "connection failed" ), 0xffffff00, true );
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClientGameCreation::CheckConnection()
{
	if ( pInGameNetDriver )
	{
		if ( pInGameNetDriver->GetState() == INetDriver::INACTIVE )
		{
			SendConnectionFailed();
			pInGameNetDriver = 0;

			return false;
		}

		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessLogicIDSet( int nClientID, CStreamAccessor &pkt )
{
	int nLogicID;
	pkt >> nLogicID;

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_LOGIC_ID %d received", nOurLogicID ), 0xffffff00, true );
#endif

	NI_ASSERT_T( nOurLogicID == -1, NStr::Format( "Double logic id received ( %d, %d )", nOurLogicID, nLogicID ) );
	players[nLogicID] = players[16];

	std::construct( &(players[16]) );

	nOurLogicID = nLogicID;
	players[nLogicID].nLogicID = nLogicID;
	players[nLogicID].nSide = sides.size() - 1;
	if ( players[nLogicID].nSide < 0 )
		players[nLogicID].nSide = 2;

	players[nLogicID].fPing = 0.0f;
	players[nLogicID].eState = SPlayerInfo::EPS_CONNECTED;
	NStr::SetCodePage( GetACP() );

	players[nLogicID].szName = GetGlobalWVar( "Options.Multiplayer.GameSpyPlayerName", L"Noname" );
	if ( players[nLogicID].szName == L"Noname" )
		players[nLogicID].szName = NStr::ToUnicode( GetGlobalVar( "Options.Multiplayer.PlayerName", "Noname" ) );

	players[nLogicID].bReady = false;
	
	// ������ ��� ���
	pkt->SetSize( 0 );
	BYTE msgID = NGM_ASK_FOR_NAME;
	pkt << msgID << nOurLogicID << players[nOurLogicID].szName;

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_ASK_FOR_NAME sent" ), 0xffffff00, true );
#endif

	if ( !pInGameNetDriver->SendDirect( nClientID, pkt ) )
	{
		SendConnectionFailed();
		LeftGame();
	}

	loadMap.SetServer( nClientID );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessReceivedOwnName( const int nClientID, CStreamAccessor &pkt )
{
	std::wstring wszOwnName;
	pkt >> wszOwnName;

	NI_ASSERT_T( nOurLogicID != -1, "Own name is received before player initialization" );
	players[nOurLogicID].szName = wszOwnName;
	players[nOurLogicID].eState = SPlayerInfo::EPS_VALID;

	// ������� ���� ���������� � ����
	pkt->SetSize( 0 );
	BYTE msgID = NGM_BROADCAST_PLAYER_INFO;
	pkt << msgID;
	players[nOurLogicID].Pack( pkt );
	pInGameNetDriver->SendBroadcast( pkt );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_BROADCAST_PLAYER_INFO sent, name" ), 0xffffff00, true );
#endif

	//
	messages.AddMessage( new CGameInfoReceived( gameInfo, false, nOurLogicID ) );
	messages.AddMessage( new CPlayerInfoRefreshed( players[nOurLogicID], sides[players[nOurLogicID].nSide].szName.c_str() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessNewPlayerInfo( int nClientID, CStreamAccessor &pkt )
{
	SPlayerInfo info;
	info.Unpack( pkt );
	info.nClientID = nClientID;

	players[info.nLogicID] = info;
	players[info.nLogicID].nClientID = nClientID;
	players[info.nLogicID].eState = SPlayerInfo::EPS_VALID;
	
	messages.AddMessage( new CPlayerInfoRefreshed( info, sides[info.nSide].szName.c_str() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessNewPlayerJoinedInfo( int nClientID, CStreamAccessor &pkt )
{
	ProcessNewPlayerInfo( nClientID, pkt );

	// ������� ���������� � ����
	pkt->SetSize( 0 );
	BYTE msgID = NGM_DIRECT_PLAYER_INFO;
	pkt << msgID;
	players[nOurLogicID].Pack( pkt );
	pInGameNetDriver->SendDirect( nClientID, pkt );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_DIRECT_PLAYER_INFO sent to %d", nClientID ), 0xffffff00, true );
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessGameInfo( int nClientID, CStreamAccessor &pkt )
{
	bool bOurPasswordRequired = gameInfo.bPasswordRequired;
	const std::string	szOurPassword = gameInfo.szPassword;
	
	gameInfo.Unpack( pkt );

	if ( gameInfo.bPasswordRequired && ( !bOurPasswordRequired || gameInfo.szPassword != szOurPassword ) )
	{
		messages.AddMessage( new CWrongPassword() );
		pInGameNetDriver = 0;
	}
	else
	{
		uLong serverCheckSumRes = gameInfo.checkSumRes;
		uLong serverCheckSumMap = gameInfo.checkSumMap;
		
		const bool bLoaded = LoadMapInfo( false, true );
		if ( gameInfo.checkSumRes != serverCheckSumRes )
		{
			messages.AddMessage( new CWrongResources() );
			pInGameNetDriver = 0;

			WriteDebugMessage( NStr::Format( "wrong resources" ) );
		}
		else
		{
			if ( !bLoaded || gameInfo.checkSumMap != serverCheckSumMap )
			{
				gameInfo.bMapLoaded = false;
				loadMap.Init( pInGameNetDriver, this );
			}

			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_SWITCH_MODE_TO,
				NStr::Format( "%s;%s;%d;%d;1",	// 1 silent switch
												gameInfo.szModName.c_str(), gameInfo.szModVersion.c_str(),
												MISSION_COMMAND_MULTIPLAYER_STARTINGGAME, 1 ) );
		}
	}		
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ContinueLoadMapInfo()
{
	NStr::Format( NStr::Format( "Map info loaded" ) );

//	messages.AddMessage( new CCreateStagingRoom() );
	messages.AddMessage( new CGameInfoReceived( gameInfo, false, nOurLogicID ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::MapLoaded()
{
	gameInfo.bMapLoaded = true;
	LoadMapInfo( false, false );
	messages.AddMessage( new CGameInfoReceived( gameInfo, false, nOurLogicID ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessGameIsAlreadyStarted()
{
	messages.AddMessage( new CGameIsAlreadyStarted() );
	pInGameNetDriver = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessDirectMessage( int nClientID, CStreamAccessor &pkt )
{
	BYTE msgID = -1;
	pkt >> msgID;

	switch ( msgID )
	{
		case NGM_LOGIC_ID:
			ProcessLogicIDSet( nClientID, pkt );
			break;
		case NGM_DIRECT_PLAYER_INFO:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_DIRECT_PLAYER_INFO from %d received", nClientID ), 0xffffff00, true );
#endif

			ProcessNewPlayerInfo( nClientID, pkt );
			break;
		case NGM_GAME_INFO:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_GAME_INFO from %d received, addr %s", nClientID, pInGameNetDriver->GetAddressByClientID( nClientID ) ), 0xffffff00, true );
#endif

			ProcessGameInfo( nClientID, pkt );
			break;
		case NGM_ANSWER_PLAYER_NAME:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_ANSWER_PLAYER_NAME from %d received", nClientID ), 0xffffff00, true );
#endif

			ProcessReceivedOwnName( nClientID, pkt );
			break;
		case NGM_GAME_IS_ALREADY_STARTED:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_GAME_IS_ALREADY_STARTED from %d received", nClientID ), 0xffffff00, true );
#endif

			ProcessGameIsAlreadyStarted();
			break;
		default:
			NI_ASSERT_T( false, NStr::Format( "Unknown message (%d) received by client", msgID ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessPlayerLeft( int nClientID, CStreamAccessor &pkt )
{
	int nLogicID;
	pkt >> nLogicID;

	if ( nLogicID == nOurLogicID )
	{
		SendConnectionFailed();
		pInGameNetDriver = 0;
	}
	else
	{
		NI_ASSERT_T( nLogicID != nOurLogicID, "Somebody make us left" );

		if ( players[nLogicID].eState != SPlayerInfo::EPS_INVALID )
		{
			if ( players[nLogicID].eState == SPlayerInfo::EPS_VALID )
				messages.AddMessage( new CPlayerDeleted( nLogicID, players[nLogicID].szName, CPlayerDeleted::ER_LEFT ) );

			players[nLogicID].eState = SPlayerInfo::EPS_INVALID;
			players[nLogicID].nClientID = -1;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessKickedPlayer( int nClientID, CStreamAccessor &pkt )
{
	int nLogicID;
	pkt >> nLogicID;

	if ( nLogicID == nOurLogicID )
	{
		messages.AddMessage( new CAIMKicked() );
		pInGameNetDriver = 0;
	}
	else if ( players[nLogicID].eState != SPlayerInfo::EPS_INVALID )
	{
		if ( players[nLogicID].eState == SPlayerInfo::EPS_VALID )
			messages.AddMessage( new CPlayerDeleted( nLogicID, players[nLogicID].szName, CPlayerDeleted::ER_KICKED ) );

		players[nLogicID].eState = SPlayerInfo::EPS_INVALID;
		players[nLogicID].nClientID = -1;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessGameStarted( CStreamAccessor &pkt )
{
	if ( nOurLogicID == -1 )
	{
		messages.AddMessage( new CConnectionFailed( INetDriver::FORBIDDEN ) );
		pInGameNetDriver = 0;

		return;
	}
	
	std::list<int> oldLogicIDs;

	int nPlayerID;
	pkt >> nPlayerID;
	while ( nPlayerID != -1 )
	{
		oldLogicIDs.push_back( nPlayerID );
		pkt >> nPlayerID;
	}

	std::vector<int> curPlayersIDs( 16, -1 );
	for ( int i = 0; i < 16; ++i )
		curPlayersIDs[i] = players[i].nLogicID;
	
	std::list<int>::iterator iter = oldLogicIDs.begin();
	while ( iter != oldLogicIDs.end() )
	{
		int i = 0;
		while ( i < 16 && curPlayersIDs[i] != *iter )
			++i;

		// �� �������� ���������� ��� ���� �������
		if ( i >= 16 )
		{
			messages.AddMessage( new CConnectionFailed( INetDriver::MAXPLAYERS_REACHED ) );
			pInGameNetDriver = 0;

			break;
		}

		pkt >> players[i].nLogicID;
		pkt >> players[i].nSide;

		++iter;
	}

	if ( GetSingleton<ICommandsHistory>()->LoadCommandLineHistory() )
		SetGlobalVar( "LoadCommandLineHistory", 1 );
	else
		RemoveGlobalVar( "LoadCommandLineHistory" );

	messages.AddMessage( new CGameStarted() );
	bGameStarted = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessGameSettingsChanged( CStreamAccessor &pkt )
{
	gameInfo.gameSettings.Unpack( pkt );
	messages.AddMessage( new CGameSettingsChanged( gameInfo.gameSettings ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessBroadcastMessage( int nClientID, CStreamAccessor &pkt )
{
	BYTE msgID = -1;
	pkt >> msgID;

	switch ( msgID )
	{
		case NGM_BROADCAST_PLAYER_INFO:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_BROADCAST_PLAYER_INFO from %d received", nClientID ), 0xffffff00, true );
#endif

			ProcessNewPlayerJoinedInfo( nClientID, pkt );
			break;
		case NGM_PLAYER_LEFT:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_PLAYER_LEFT from %d received", nClientID ), 0xffffff00, true );
#endif

			ProcessPlayerLeft( nClientID, pkt );
			break;
		case NGM_PLAYER_KICKED:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_PLAYER_KICKED from %d received", nClientID ), 0xffffff00, true );
#endif

			ProcessKickedPlayer( nClientID, pkt );
			break;
		case NGM_GAME_STARTED:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_GAME_STARTED from %d received", nClientID ), 0xffffff00, true );
#endif

			ProcessGameStarted( pkt );
			break;
		case NGM_PING:
			break;
		case NGM_GAME_SETTINGS_CHANGED:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_GAME_SETTINGS_CHANGED from %d received", nClientID ), 0xffffff00, true );
#endif

			ProcessGameSettingsChanged( pkt );
			break;
		default:
			NI_ASSERT_T( false, NStr::Format( "Unknown message (%d) received by client", msgID ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessRemoveClient( int nClientID, CStreamAccessor &pkt )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ProcessMessages()
{
	INetDriver::EMessage eMsgID;
	int nClientID;
	int received[128];
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	//
	while ( !bGameStarted && pInGameNetDriver && pInGameNetDriver->GetMessage(&eMsgID, &nClientID, received, pkt) )
	{
		switch ( eMsgID ) 
		{
			case INetDriver::DIRECT:
				ProcessDirectMessage( nClientID, pkt );
				break;
			case INetDriver::BROADCAST:
				ProcessBroadcastMessage( nClientID, pkt );
				break;
			case INetDriver::NEW_CLIENT:
				break;
			case INetDriver::SERVER_DEAD:
				{
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "SERVER_DEAD" ), 0xffffff00, true );
#endif
					SendConnectionFailed();
					pInGameNetDriver = 0;
				}
				break;
			case INetDriver::REMOVE_CLIENT:
#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "REMOVE_CLIENT from %d received", nClientID ), 0xffffff00, true );
#endif

				ProcessRemoveClient( nClientID, pkt );
				return;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::Segment()
{
	if ( bModChanged )
	{
		ContinueLoadMapInfo();
		bModChanged = false;
	}

	if ( CheckConnection() )
	{
		SendPingMessage();
		ProcessMessages();

		if ( pInGameNetDriver )
		{
			UpdatePlayersInfo();
			loadMap.Segment();
		}
	}	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::KickPlayer( const int nLogicID )
{
	NI_ASSERT_T( false, "Client can't kick a player" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::ChangePlayerSettings( const SPlayerInfo &info, const EPlayerSettings &eSettingsType )
{
	const int nID = nOurLogicID == -1 ? 16 : nOurLogicID;
	switch ( eSettingsType )
	{
		case EPS_READY:
			players[nID].bReady = info.bReady;
			break;
		case EPS_SIDE:
			if ( sides.size() > 0 )
				players[nID].nSide = ( players[nID].nSide + 1 ) % sides.size();

			break;
		case EPS_NAME:
			players[nID].szName = info.szName;
			break;
		case EPS_MAP_LOAD_PROGRESS:
			players[nID].cMapLoadProgress = info.cMapLoadProgress;

			break;
	}

	if ( nOurLogicID != -1 && players[nOurLogicID].eState == SPlayerInfo::EPS_VALID )
	{
		CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );

		BYTE msgID = NGM_BROADCAST_PLAYER_INFO;
		pkt << msgID;
		players[nOurLogicID].Pack( pkt );
		pInGameNetDriver->SendBroadcast( pkt );

#ifdef DEBUG_NET_MESSAGES
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "NGM_BROADCAST_PLAYER_INFO sent, settings" ), 0xffffff00, true );
#endif
	}

	//
	const std::string szSide = sides.size() > 0 ? sides[players[nID].nSide].szName : "Random";
	messages.AddMessage( new CPlayerInfoRefreshed( players[nID], szSide.c_str() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IGamePlaying* CClientGameCreation::CreateGamePlaying()
{
	const int nGameSpeed = NOptionsConvert::GetSpeed( gameInfo.gameSettings.szGameSpeed );
	gameInfo.gameSettings.nTimeLimit *= NTimer::GetCoeffFromSpeed( nGameSpeed );
	gameInfo.gameSettings.nTimeToCapture *= NTimer::GetCoeffFromSpeed( nGameSpeed );
	
	SetGlobalVars( players[nOurLogicID].nLogicID );

	//{CRAP ��������� ���� ����
	IGamePlaying *pGamePlaying = new CGamePlaying();
	//CRAP}

	pGamePlaying->Init( pInGameNetDriver, 0, players, false, players[nOurLogicID].nLogicID, mapInfo.diplomacies );

	return pGamePlaying;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*		 							CClientGameCreation::CLoadMap										*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::CLoadMap::Init( INetDriver *_pNetDriver, CClientGameCreation *_pClientGameCreation )
{
	eState = ELS_WAIT_FOR_SERVER_ID;

	pNetDriver = _pNetDriver;
	pClientGameCreation = _pClientGameCreation;

	WriteDebugMessage( NStr::Format( "Need to load map" ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::CLoadMap::ProcessWaitForServerID()
{
	if ( nServer != -1 )
	{
		CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
		BYTE msgID = NGM_SEND_ME_MAP;
		pkt << msgID;

		pNetDriver->SendDirect( nServer, pkt );

		SPlayerInfo playerInfo;
		playerInfo.cMapLoadProgress = 0;
		pClientGameCreation->ChangePlayerSettings( playerInfo, IGameCreation::EPS_MAP_LOAD_PROGRESS );

		WriteDebugMessage( NStr::Format( "Send load query" ) );

		eState = ELS_LOADING;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::CLoadMap::ProcessMapPacket( CStreamAccessor &pkt )
{
	const int nPacketSize = pkt->GetSize() - pkt->GetPos();
	NI_ASSERT_T( nReceived + nPacketSize < stream.size(), "Wrong map size" );
	if ( nReceived + nPacketSize >= stream.size() )
		stream.resize( nReceived + nPacketSize + 100 );

	pkt->Read( &(stream[nReceived]), nPacketSize );

	nReceived += nPacketSize;
	NI_ASSERT_T( nReceived <= nCompressedSize, NStr::Format( "Received (%d) more than map size (%d)", nReceived, nCompressedSize ) );
	nTotalReceived += nPacketSize;

	SPlayerInfo playerInfo;
	playerInfo.cMapLoadProgress = Min( int( (float)nTotalReceived/(float)nTotalSize * 100 ), 99 );
	pClientGameCreation->ChangePlayerSettings( playerInfo, IGameCreation::EPS_MAP_LOAD_PROGRESS );

	WriteDebugMessage( NStr::Format( "Packed receive: size %d, total received %d, sizeToReceive %d", nPacketSize, nReceived, nCompressedSize ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::CLoadMap::ProcessMapLoadFinished()
{
	NI_ASSERT_T( nReceived == nCompressedSize, NStr::Format( "Received size (%d) isn't equal to map size (%d)", nReceived, nCompressedSize ) );

	WriteDebugMessage( NStr::Format( "Load finished" ) );

	if ( nRealSize >= 0 )
	{
		// compressed file
		// inflate map
		z_stream zstream;
		zstream.next_in = (Bytef*)(&(stream[0]));
		zstream.avail_in = nReceived;

		std::vector<BYTE> inflatedStream( nRealSize + 10000 );
		zstream.next_out = (Bytef*)(&(inflatedStream[0]));
		zstream.avail_out = inflatedStream.size();
		zstream.zalloc = (alloc_func)0;
		zstream.zfree = (free_func)0;

		// Perform inflation. wbits < 0 indicates no zlib header inside the data.
		int err = inflateInit2( &zstream, -MAX_WBITS );
		if ( err == Z_OK )
		{
			err = inflate( &zstream, Z_FINISH );
			inflateEnd( &zstream );
			// CRAP{ ������-�� ������ ��� ���������� ������������ "buffer error" ������ "stream end"...
			if ( (err == Z_STREAM_END) || (err == Z_BUF_ERROR) )
				err = Z_OK;
			// CRAP}
			inflateEnd( &zstream );
		}

		{
			const int nInflatedSize = zstream.next_out - &(inflatedStream[0]);
			NI_ASSERT_T( nInflatedSize == nRealSize, NStr::Format( "Wrong inflated size, %d instead of %d", nInflatedSize, nRealSize ) );

			const std::string szPathName = GetSingleton<IDataStorage>()->GetName() + szFileName;
			CPtr<IDataStream> pStream = CreateFileStream( szPathName.c_str(), STREAM_ACCESS_WRITE );
			pStream->Write( &(inflatedStream[0]), nRealSize );

			WriteDebugMessage( NStr::Format( "Inflating OK" ) );
		}
	}
	// not deflated file
	else if ( nRealSize == -1 && nCompressedSize > 0 )
	{
		const std::string szPathName = GetSingleton<IDataStorage>()->GetName() + szFileName;
		CPtr<IDataStream> pStream = CreateFileStream( szPathName.c_str(), STREAM_ACCESS_WRITE );
		pStream->Write( &(stream[0]), nCompressedSize );

		WriteDebugMessage( NStr::Format( "Loading OK" ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::CLoadMap::AllLoadFinished()
{
	pClientGameCreation->MapLoaded();

	SPlayerInfo playerInfo;
	playerInfo.cMapLoadProgress = 100;
	pClientGameCreation->ChangePlayerSettings( playerInfo, IGameCreation::EPS_MAP_LOAD_PROGRESS );

	eState = ELS_NONE;
	pNetDriver = 0;

	WriteDebugMessage( NStr::Format( "Finished" ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::CLoadMap::ProcessReceivePackedFileInfo( CStreamAccessor &pkt )
{
	pkt >> szFileName >> nCompressedSize >> nRealSize;
	nReceived = 0;

	stream.resize( nCompressedSize + 100 );

	WriteDebugMessage( NStr::Format( "Packed file info received: file %s, realSize %d, compressedSize %d", szFileName.c_str(), nRealSize, nCompressedSize ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::CLoadMap::ProcessReceiveFileInfo( CStreamAccessor &pkt )
{
	pkt >> szFileName >> nCompressedSize;
	nReceived = 0;
	nRealSize = -1;

	stream.resize( nCompressedSize + 100 );

	WriteDebugMessage( NStr::Format( "File info received: file %s, realSize %d, compressedSize %d", szFileName.c_str(), nRealSize, nCompressedSize ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::CLoadMap::ProcessLoading()
{
	INetDriver::EMessage eMsgID;
	int nClientID;
	int received[128];
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	
	while ( pNetDriver && pNetDriver->GetChannelMessage( &eMsgID, &nClientID, received, pkt, 2 ) )
	{
		if ( eMsgID == INetDriver::DIRECT )
		{
			BYTE msg;
			pkt >> msg;
			switch ( msg )
			{
				case NGM_TOTAL_PACKED_SIZE:
					pkt >> nTotalSize;
					nTotalReceived = 0;

					break;
				case NGM_PACKED_FILE_INFO:
					ProcessReceivePackedFileInfo( pkt );

					break;
				case NGM_FILE_INFO:
					ProcessReceiveFileInfo( pkt );

					break;
				case NGM_PACKET:
					ProcessMapPacket( pkt );

					break;
				case NGM_STREAM_FINISHED:
					ProcessMapLoadFinished();

					break;
				case NGM_FINISHED:
					AllLoadFinished();					

					break;
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientGameCreation::CLoadMap::Segment()
{
	if ( pNetDriver && eState != ELS_NONE )
	{
		switch ( eState )
		{
			case ELS_WAIT_FOR_SERVER_ID:
				ProcessWaitForServerID();

				break;
			case ELS_LOADING:
				ProcessLoading();

				break;
			default: NI_ASSERT_T( false, NStr::Format( "Unknown state of map loading (%d)", int(eState) ) );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
