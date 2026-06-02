#include "stdafx.h"

#include "ServersList.h"
#include "MultiplayerConsts.h"
#include "GameCreation.h"
#include "ServersListMessages.h"
#include "LanChat.h"

#include "..\StreamIO\StreamIOHelper.h"
#include "..\StreamIO\StreamIOTypes.h"

#include "..\Net\NetDriver.h"
#include "GameSpyPeerChat.h"
BASIC_REGISTER_CLASS( IServersList );
void CServersList::Init( INetDriver *_pNetDriver )
{
	pNetDriver = _pNetDriver;
	servers.clear();
	wCurUniqueId = 0;
	lastServersCheck = 0;
}
IMultiplayerMessage* CServersList::GetMessage()
{
	return messages.GetMessage();
}
void CServersList::AddServer( INetNodeAddress *pAddress, const float fPing, const INetDriver::SGameInfo &gameInfo, const bool bSameVersion )
{
	servers.push_back( SServerInfo() );
	SServerInfo &info = servers.back();

	info.bUpdated = true;
	info.pAddress = pAddress;
	info.fPing = fPing;
	info.wUniqueServerId = wCurUniqueId++;
	info.Unpack( gameInfo );

	messages.AddMessage
	(
		new CServerInfoRefreshed
		( 
			info.wUniqueServerId, info.szGameName, info.fPing, info.eState, info.nCurPlayers, info.nMaxPlayers, 
			info.szMapName, info.bPasswordRequired, info.szModName, info.szModVersion, bSameVersion, info.eGameType,
			info.gameSettings
		)
	);
}
void CServersList::RefreshServerInfo( const SServerInfo &info, const bool bSameVersion )
{
	CServers::iterator iter = servers.begin();		
	while ( iter != servers.end() && iter->wUniqueServerId != info.wUniqueServerId )
		++iter;

	messages.AddMessage
	(
		new CServerInfoRefreshed( iter->wUniqueServerId, iter->szGameName, iter->fPing, iter->eState,
															iter->nCurPlayers, iter->nMaxPlayers, iter->szMapName, iter->bPasswordRequired,
															iter->szModName, iter->szModVersion, bSameVersion, iter->eGameType, iter->gameSettings )
	);
}
void CServersList::RemoveServer( const SServerInfo &info )
{
	messages.AddMessage( new CServerRemoved( info.wUniqueServerId ) );
}
void CServersList::RefreshServersList()
{
	INetDriver::SGameInfo gameInfo;
	CPtr<INetNodeAddress> pAddress = CreateObject<INetNodeAddress>( NET_NODE_ADDRESS );
	
	const int nDebugTemp = servers.size();

	bool bWrongVersion = true;
	float fPing = 0;
	int i = 0;
	while ( pNetDriver->GetGameInfo( i++, pAddress, &bWrongVersion, &fPing, &gameInfo ) )
	{
		CServers::iterator iter = servers.begin();
		while ( iter != servers.end() && !pAddress->IsSameIP( iter->pAddress ) )
			++iter;

		if ( iter == servers.end() )
			AddServer( pAddress, fPing, gameInfo, !bWrongVersion );
		else
		{
			iter->bUpdated = true;

			SServerInfo info;
			info.pAddress = pAddress;
			info.fPing = fPing;
			info.wUniqueServerId = iter->wUniqueServerId;

			info.Unpack( gameInfo );

			if ( info != *iter )
			{
				*iter = info;
				RefreshServerInfo( *iter, !bWrongVersion );
			}
		}

		pAddress = CreateObject<INetNodeAddress>( NET_NODE_ADDRESS );
	}

	const NTimer::STime curAbsTime = GetSingleton<IGameTimer>()->GetAbsTime();
	if ( lastServersCheck + 2000 < curAbsTime )
	{
		lastServersCheck = curAbsTime;		
		
		CServers::iterator iter = servers.begin();
		while ( iter != servers.end() )
		{
			if ( !iter->bUpdated )
			{
				RemoveServer( *iter );
				iter = servers.erase( iter );
			}
			else
			{
				iter->bUpdated = false;
				++iter;
			}
		}
	}
}
void CServersList::Segment()
{
	if ( pNetDriver )
		RefreshServersList();
}
void CServersList::DestroyNetDriver()
{
	pNetDriver = 0;
}
const SServerInfo* CServersList::FindServerByID( const WORD wServerID ) const
{
	CServers::const_iterator iter = servers.begin();
	while ( iter != servers.end() && iter->wUniqueServerId != wServerID )
		++iter;

	if ( iter == servers.end() )
		return 0;
	else
		return &(*iter);
}
bool CServersList::CanJoinToServerByID( const WORD wServerID )
{
	const SServerInfo *pInfo = FindServerByID( wServerID );

	if ( pInfo )
	{
		std::string szIPAdderss = pInfo->pAddress->GetFastName();
		const int n = szIPAdderss.find( ':' );
		if ( n >= 0 )
			szIPAdderss.resize( n );

		pInfo->pAddress->Clear();
		if ( !pInfo->pAddress->SetInetName( szIPAdderss.c_str(), SMultiplayerConsts::NET_PORT ) )
			return false;

		if ( pInfo && pInfo->eState == SServerInfo::ESS_OPEN && pInfo->nCurPlayers < pInfo->nMaxPlayers )
			return true;
	}
	
	return false;
}
bool CServersList::IsNeedPassword( const WORD wServerID ) const
{
	const SServerInfo *pInfo = FindServerByID( wServerID );
	return pInfo ? pInfo->bPasswordRequired : false;
}
IGameCreation* CServersList::JoinToServerByID( const WORD wServerID, CPtr<IChat> *pChat, bool bPasswordRequired, const std::string &szPassword )
{
	const SServerInfo *pInfo = FindServerByID( wServerID );

	NI_ASSERT_T( pInfo != 0, NStr::Format( "Server %d not found", wServerID ) );

	return JoinToServerByAddress( pInfo->pAddress, pChat, pInfo->nHostPort, bPasswordRequired, szPassword );
}
IGameCreation* CServersList::JoinToServerByAddress( INetNodeAddress *pAddress, CPtr<IChat> *pChat, const int nPort, bool bPasswordRequired, const std::string &szPassword )
{
	CPtr<IDataStream> pPwd = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	CPtr<INetDriver> pNetDriver = CreateInGameNetDriver( SMultiplayerConsts::NET_PORT );
	
	std::string szIPAdderss = pAddress->GetFastName();
	const int n = szIPAdderss.find( ':' );
	if ( n >= 0 )
		szIPAdderss.resize( n );

	pAddress->Clear();
	if ( pAddress->SetInetName( szIPAdderss.c_str(), SMultiplayerConsts::NET_PORT ) )
	{
		pNetDriver->ConnectGame( pAddress, pPwd );
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "request for connect is sent" ), 0xffffff00, true );

		CClientGameCreation *pGameCreation = new CClientGameCreation();
		pGameCreation->Init( pNetDriver, bPasswordRequired, szPassword );

		CreateInGameChat( pChat, pNetDriver );

		return pGameCreation;
	}
	else
		return 0;
}
void CServersList::Refresh()
{
	pNetDriver->RefreshServersList();
}
void CLanServersList::Init()
{
	INetDriver *pNetDriver = CreateObject<INetDriver>( INetDriver::tidTypeID );
	pNetDriver->Init( GetGlobalVar("NetGameVersion", 1), SMultiplayerConsts::NET_PORT, true );

	CServersList::Init( pNetDriver );
}
INetDriver* CLanServersList::CreateInGameNetDriver( const int nPort )
{
	INetDriver *pNetDriver = GetNetDriver();

	if ( !pNetDriver )
	{
		pNetDriver = CreateObject<INetDriver>( INetDriver::tidTypeID );
		if ( nPort == -1 )
			pNetDriver->Init( GetGlobalVar("NetGameVersion", 1), SMultiplayerConsts::NET_PORT, true );
		else
			pNetDriver->Init( GetGlobalVar("NetGameVersion", 1), nPort, true );
	}

	return pNetDriver;
}
IGameCreation* CLanServersList::CreateServer( const SGameInfo &gameInfo, const SQuickLoadMapInfo &mapInfo, CPtr<IChat> *pChat )
{
	DestroyNetDriver();

	INetDriver *pInGameNetDriver = CreateObject<INetDriver>( INetDriver::tidTypeID );
	pInGameNetDriver->Init( GetGlobalVar("NetGameVersion", 1), SMultiplayerConsts::NET_PORT, false );

	CServerGameCreation *pGameCreation = new CServerGameCreation();
	pGameCreation->Init( pInGameNetDriver, pInGameNetDriver, gameInfo, mapInfo );

	CLanChat *pCreatedChat = new CLanChat();
	pCreatedChat->InitInGameChat( pInGameNetDriver );
	*pChat = pCreatedChat;

	return pGameCreation;
}
void CLanServersList::CreateInGameChat( CPtr<IChat> *pChat, INetDriver *pNetDriver )
{
	CLanChat *pCreatedChat = new CLanChat();
	pCreatedChat->InitInGameChat( pNetDriver );
	*pChat = pCreatedChat;
}
void CGameSpyServersList::Init()
{
	INetDriver *pNetDriver = CreateObject<INetDriver>( NET_GS_SERVERS_LIST_DIRVER );
	pNetDriver->Init( GetGlobalVar("NetGameVersion", 1), SMultiplayerConsts::GS_NET_PORT, true );

	CServersList::Init( pNetDriver );
}
void CGameSpyServersList::CreateInGameChat( CPtr<IChat> *pChat, INetDriver *pInGameNetDriver )
{
	if ( (*pChat) == 0 )
	{
		*pChat = new CGameSpyPeerChat();
		
		std::wstring szPlayerName = MakeWideStringFromWordString( GetGlobalWVar( "Options.Multiplayer.GameSpyPlayerName", reinterpret_cast<const WORD*>( L"Noname" ) ) );
		if ( szPlayerName == L"Noname" )
			szPlayerName = NStr::ToUnicode( GetGlobalVar( "Options.Multiplayer.PlayerName", "Noname" ) );

		(*pChat)->InitGSChat( ToWordString( szPlayerName ) );
	}

	(*pChat)->InitInGameChat( pInGameNetDriver );
}
IGameCreation* CGameSpyServersList::CreateServer( const struct SGameInfo &gameInfo, const struct SQuickLoadMapInfo &mapInfo, CPtr<IChat> *pChat )
{
	DestroyNetDriver();

	INetDriver *pInGameNetDriver = CreateObject<INetDriver>( INetDriver::tidTypeID );
	pInGameNetDriver->Init( GetGlobalVar("NetGameVersion", 1), SMultiplayerConsts::NET_PORT, false );

	INetDriver *pOutGameNetDriver = CreateObject<INetDriver>( NET_GS_QUERY_REPORTING_DRIVER );
	pOutGameNetDriver->Init( GetGlobalVar("NetGameVersion", 1), SMultiplayerConsts::GS_NET_PORT, false );

	CServerGameCreation *pGameCreation = new CServerGameCreation();
	pGameCreation->Init( pInGameNetDriver, pOutGameNetDriver, gameInfo, mapInfo );

	CreateInGameChat( pChat, pInGameNetDriver );

	return pGameCreation;
}
INetDriver* CGameSpyServersList::CreateInGameNetDriver( const int nPort )
{
	INetDriver *pNetDriver = CreateObject<INetDriver>( INetDriver::tidTypeID );
	if ( nPort == -1 )
		pNetDriver->Init( GetGlobalVar("NetGameVersion", 1), SMultiplayerConsts::NET_PORT, true );
	else
		pNetDriver->Init( GetGlobalVar("NetGameVersion", 1), nPort, true );

	return pNetDriver;
}
void CInternetServersList::Init()
{
	CServersList::Init( 0 );
}
INetDriver* CInternetServersList::CreateInGameNetDriver( const int nPort )
{
	INetDriver *pNetDriver = GetNetDriver();

	if ( !pNetDriver )
	{
		pNetDriver = CreateObject<INetDriver>( INetDriver::tidTypeID );
		if ( nPort == -1 )
			pNetDriver->Init( GetGlobalVar("NetGameVersion", 1), SMultiplayerConsts::NET_PORT, true );
		else
			pNetDriver->Init( GetGlobalVar("NetGameVersion", 1), nPort, true );
	}

	return pNetDriver;
}
IGameCreation* CInternetServersList::CreateServer( const SGameInfo &gameInfo, const SQuickLoadMapInfo &mapInfo, CPtr<IChat> *pChat )
{
	DestroyNetDriver();

	INetDriver *pInGameNetDriver = CreateObject<INetDriver>( INetDriver::tidTypeID );
	pInGameNetDriver->Init( GetGlobalVar("NetGameVersion", 1), SMultiplayerConsts::NET_PORT, false );

	CServerGameCreation *pGameCreation = new CServerGameCreation();
	pGameCreation->Init( pInGameNetDriver, pInGameNetDriver, gameInfo, mapInfo );

	CLanChat *pCreatedChat = new CLanChat();
	pCreatedChat->InitInGameChat( pInGameNetDriver );
	*pChat = pCreatedChat;

	return pGameCreation;
}
void CInternetServersList::CreateInGameChat( CPtr<IChat> *pChat, INetDriver *pNetDriver )
{
	CLanChat *pCreatedChat = new CLanChat();
	pCreatedChat->InitInGameChat( pNetDriver );
	*pChat = pCreatedChat;
}
