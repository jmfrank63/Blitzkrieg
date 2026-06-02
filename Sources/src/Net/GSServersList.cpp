#include "stdafx.h"

#include "GSServersList.h"
#include "GSConsts.h"

#include "..\Main\MultiplayerConsts.h"
#include "..\GameTT\MultiplayerCommandManager.h"
#include "..\StreamIO\StreamIOTypes.h"
using namespace NWin32Helper;
const int CGSServersListDriver::SERVER_LIST_UPDATE_PERIOD = 2000;
CGSServersListDriver::CGSServersListDriver()
: CThread( 50 ), serverList( 0 )
{
}
CGSServersListDriver::~CGSServersListDriver()
{
	StopThread();
	ServerListFree( serverList );
}
bool CGSServersListDriver::Init( const APPLICATION_ID nApplicationID, int _nGamePort, bool _bClientOnly )
{
	nNetVersion = nApplicationID;
	
	std::string szSecretKey;
	szSecretKey.resize( 6 );

	szSecretKey[0] = 'f';
	szSecretKey[1] = 'Y';
	szSecretKey[2] = 'D';
	szSecretKey[3] = 'X';
	szSecretKey[4] = 'B';
	szSecretKey[5] = 'N';
	
	serverList = ServerListNew( 
		GetGlobalVar("GameSpyGameName"), GetGlobalVar("GameSpyEngineName"),
		szSecretKey.c_str(), 10, ListCallBack, GCALLBACK_FUNCTION, this
	);

	endOfLastUpdate = 0;
	bUpdating = false;
	servers.clear();

	RunThread();

	return true;
}
INetDriver::EState CGSServersListDriver::GetState() const
{
	return ACTIVE;
}
INetDriver::EReject CGSServersListDriver::GetRejectReason() const
{
	return NONE;
}
bool CGSServersListDriver::GetGameInfo( int nIdx, INetNodeAddress *pAddr, bool *pWrongVersion, float *pPing, SGameInfo *pGameInfo )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	if ( nIdx >= servers.size() )
		return false;

	int nCnt = 0;
	std::list<SServerInfo>::iterator iter = servers.begin();
	while ( iter != servers.end() && nCnt < nIdx )
	{
		++iter;
		++nCnt;
	}

	NI_ASSERT_T( iter != servers.end(), "Wrong size of servers" );

	pAddr->SetInetName( iter->szIP.c_str(), iter->gameInfo.nHostPort );
	*pPing = iter->fPing;
	*pGameInfo = iter->gameInfo;
	*pWrongVersion = iter->nNetVersion != nNetVersion;

	return true;
}
void CGSServersListDriver::RefreshServersList()
{
	bUpdating = true;	
	ServerListClear( serverList );
	ServerListUpdate( serverList, true );

	servers.clear();
}
void CGSServersListDriver::Step()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	ServerListThink( serverList );

	IGameTimer *pGameTimer = GetSingleton<IGameTimer>();
	if ( pGameTimer )
	{
		const NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();
		if ( !bUpdating && ( endOfLastUpdate == 0 || curTime > endOfLastUpdate + SERVER_LIST_UPDATE_PERIOD ) )
			RefreshServersList();
	}
}
void CGSServersListDriver::AddServer( GServer server )
{
	SServerInfo serverInfo;
	
	serverInfo.gameInfo.wszServerName = NStr::ToUnicode( ServerGetStringValue( server, "hostname", "Noname" ) );
	serverInfo.gameInfo.nHostPort = ServerGetIntValue( server, "hostport", SMultiplayerConsts::NET_PORT );

	std::string szMapName = ServerGetStringValue( server, "mapname", "" );
	for ( int i = 0; i < szMapName.size(); ++i )
	{
		if ( szMapName[i] == '/' )
			szMapName = '\\';
	}
	serverInfo.gameInfo.wszMapName = NStr::ToUnicode( ServerGetStringValue( server, "mapname", "" ) );
	serverInfo.gameInfo.nCurPlayers = ServerGetIntValue( server, "numplayers", 0 );
	serverInfo.gameInfo.nMaxPlayers = ServerGetIntValue( server, "maxplayers", 0 );	
	serverInfo.gameInfo.bPasswordRequired = ( ServerGetIntValue( server, "password", 0 ) != 0 );
	serverInfo.nNetVersion = NStr::ToInt( ServerGetStringValue( server, "gamever", "1" ) );

	std::string szGameType = ServerGetStringValue( server, "gametype", "Flag control" );

	std::vector<std::string> szGameTypeParams;
	NStr::SplitStringWithMultipleBrackets( szGameType, szGameTypeParams, '/' );

	if ( szGameTypeParams.size() == 1 )
	{
		serverInfo.gameInfo.szModName = "";
		serverInfo.gameInfo.szModVersion = "";
		serverInfo.gameInfo.szGameType = szGameTypeParams[0];
	}
	else if ( szGameTypeParams.size() == 3 )
	{
		serverInfo.gameInfo.szModName = szGameTypeParams[0];
		serverInfo.gameInfo.szModVersion = szGameTypeParams[1];
		serverInfo.gameInfo.szGameType = szGameTypeParams[2];
	}
	else
	{
		NI_ASSERT_T( szGameTypeParams.size() == 0, NStr::Format( "Unknown game type %s", szGameType.c_str() ) );
	}

	std::string szGameMode = ServerGetStringValue( server, "gamemode", "closedplaying" );
	serverInfo.gameInfo.eGameMode = GetMode( szGameMode.c_str() );

	serverInfo.fPing = (float)ServerGetPing( server ) / 1000.0f;

	serverInfo.pAddr = CreateObject<INetNodeAddress>( NET_NODE_ADDRESS );
	serverInfo.szIP = ServerGetAddress( server );
	serverInfo.pAddr->SetInetName( serverInfo.szIP.c_str(), serverInfo.gameInfo.nHostPort );

	SMultiplayerGameSettings settings;
	settings.nFlagScoreLimit = ServerGetIntValue( server, "flagscorelimit", 0 );
	settings.nKillScoreLimit = ServerGetIntValue( server, "killscorelimit", 0 );
	settings.nTimeLimit = ServerGetIntValue( server, "timelimit", 0 );
	settings.nTimeToCapture = ServerGetIntValue( server, "timetocapture", 0 );
	settings.szGameSpeed = ServerGetStringValue( server, "gamespeed", "Normal" );

	serverInfo.gameInfo.pGameSettings = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	settings.Pack( serverInfo.gameInfo.pGameSettings );

	std::list<SServerInfo>::iterator iter = servers.begin();
	while ( iter != servers.end() && !iter->pAddr->IsSameIP( serverInfo.pAddr ) )
		++iter;

	if ( iter != servers.end() )
		servers.erase( iter );

	servers.push_back( serverInfo );
}
void CGSServersListDriver::List( GServerList serverList, int nMsg, void *pParam1, void *pParam2 )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	switch ( nMsg )
	{
		case LIST_PROGRESS:
			AddServer( reinterpret_cast<GServer>( pParam1 ) );
			break;
		case LIST_STATECHANGED:
			{
				const GServerListState eState = ServerListState( serverList );
				switch ( eState )
				{
					case sl_idle:
						{
							bUpdating = false;
							if ( IGameTimer *pGameTimer = GetSingleton<IGameTimer>() )
							endOfLastUpdate = pGameTimer->GetAbsTime();
						}
						
						break;
					case sl_listxfer:
						break;
					case sl_lanlist:
						break;
					case sl_querying:
						break;
					default:
						NI_ASSERT_T( false, "Unknown state" );
				}
			}
			break;
		default:
			NI_ASSERT_T( false, "Unknown message" );
	}
}
void CGSServersListDriver::ListCallBack( GServerList serverList, int nMsg, void *pInstance, void *pParam1, void *pParam2 )
{
	reinterpret_cast<CGSServersListDriver*>(pInstance)->List( serverList, nMsg, pParam1, pParam2 );
}
