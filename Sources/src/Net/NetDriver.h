#ifndef __NET_DRIVER_H__
#define __NET_DRIVER_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <winsock2.h>
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unsigned int APPLICATION_ID;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum
{
	NET_BASE_VALUE								= 0x100d0000,
	NET_NET												= NET_BASE_VALUE + 1,
	NET_NET_DRIVER								= NET_BASE_VALUE + 2,
	NET_NODE_ADDRESS							= NET_BASE_VALUE + 3,
	NET_GS_QUERY_REPORTING_DRIVER = NET_BASE_VALUE + 4,
	NET_GS_SERVERS_LIST_DIRVER		= NET_BASE_VALUE + 5,

	NET_FORCE_DWORD	= 0x7fffffff
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface INetNodeAddress : public IRefCount
{
	virtual void STDCALL Clear() = 0;
	//
	virtual bool STDCALL SetInetName( const char *pszHost, int nDefaultPort ) = 0;
	virtual const char* STDCALL GetName( bool bResolve = true ) const = 0;
	virtual const char* STDCALL GetFastName() const = 0;
	//
	virtual bool STDCALL IsSameIP( const INetNodeAddress *pAddress ) const = 0;
	virtual unsigned int STDCALL GetIP() const = 0;
	//
	virtual sockaddr* STDCALL GetSockAddr() = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface INetDriver : public IRefCount
{
	enum { tidTypeID = NET_NET_DRIVER };
	enum EState
	{
		INACTIVE,
		ACTIVE,
		CONNECTING
	};
	enum EReject
	{
		NONE,
		TIMEOUT,
		BANNED,
		WRONG_VERSION,
		MAXPLAYERS_REACHED,
		PASSWORD_FAILED,
		ALREADY_CONNECTED,
		FORBIDDEN
	};
	enum EMessage
	{
		NEW_CLIENT,
		REMOVE_CLIENT,
		DIRECT,
		BROADCAST,
		SERVER_DEAD,
	};
	enum EServerGameMode
	{
		ESGM_WAIT,						// waiting for players to join
		ESGM_SETTINGS,				// players are determining game parameters, no joining allowed
		ESGM_CLOSEDPLAYING,		// game is in progress, no joining allowed
		ESGM_OPENPLAYING,			// game is in progress, players may still join
		ESGM_DEBRIEFING,			// game is over, stats/info is being shown, no joining allowed
		ESGM_EXITING,					// server is shutting down, remove from server list
	};

	struct SGameInfo
	{
		std::wstring wszServerName;
		long nHostPort;
		std::wstring wszMapName;
		std::string szGameType;
		int nCurPlayers, nMaxPlayers;
		EServerGameMode eGameMode;

		bool bPasswordRequired;
		
		std::string szModName;
		std::string szModVersion;
		
		CPtr<IDataStream> pGameSettings;

		SGameInfo() 
			: wszServerName( L"" ), nHostPort( 0 ), wszMapName( L"" ), szGameType( "" ), 
				nCurPlayers( 0 ), nMaxPlayers( 0 ), eGameMode( ESGM_SETTINGS ), bPasswordRequired( false ),
				szModName( "" ), szModVersion( "" ) { }
		SGameInfo( const SGameInfo &gameInfo ) :
			wszServerName( gameInfo.wszServerName ), nHostPort( gameInfo.nHostPort ), wszMapName( gameInfo.wszMapName ),
			szGameType( gameInfo.szGameType ), nCurPlayers( gameInfo.nCurPlayers ), nMaxPlayers( gameInfo.nMaxPlayers ),	
			eGameMode( gameInfo.eGameMode ), bPasswordRequired( gameInfo.bPasswordRequired ),
			szModName( gameInfo.szModName ), szModVersion( gameInfo.szModVersion ), pGameSettings( gameInfo.pGameSettings ) { }

		bool operator==( const SGameInfo &gameInfo ) const
		{
			return 
				wszServerName == gameInfo.wszServerName &&
				nHostPort == gameInfo.nHostPort && 
				wszMapName == gameInfo.wszMapName &&
				szGameType == gameInfo.szGameType &&
				nCurPlayers == gameInfo.nCurPlayers &&
				nMaxPlayers == gameInfo.nMaxPlayers &&
				eGameMode == gameInfo.eGameMode &&
				bPasswordRequired == gameInfo.bPasswordRequired &&
				szModName == gameInfo.szModName &&
				szModVersion == gameInfo.szModVersion;
		}
	};
	//
	virtual bool STDCALL Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly ) = 0;
	// get current state (active/inactive/connecting)
	virtual EState STDCALL GetState() const = 0;
	// get reject reason (then )
	virtual EReject STDCALL GetRejectReason() const = 0;
	// connect to the game with particular address
	virtual void STDCALL ConnectGame( const INetNodeAddress *pAddr, IDataStream *pPwd ) = 0;
	// start game (server)
	virtual void STDCALL StartGame() = 0;
	// start sending game info (for server)
	virtual void STDCALL StartGameInfoSend( const SGameInfo &gameInfo ) = 0;
	// stop sending game info (for server)
	virtual void STDCALL StopGameInfoSend() = 0;
	// start accepting new players (for server)
	virtual void STDCALL StartNewPlayerAccept() = 0;
	// stop accepting new players (for server)
	virtual void STDCALL StopNewPlayerAccept() = 0;
	// get game info (for client)
	virtual bool STDCALL GetGameInfo( int nIdx, INetNodeAddress *pAddr, bool *pWrongVersion, float *pPing, SGameInfo *pGameInfo ) = 0;
	// refresh servers list ( for client )
	virtual void STDCALL RefreshServersList() = 0;
	// send broadcast message for all
	virtual bool STDCALL SendBroadcast( IDataStream *pPkt ) = 0;
	// send direct message for client 'nClient'
	virtual bool STDCALL SendDirect( int nClient, IDataStream *pPkt ) = 0;
	// kick player 'nClient'
	virtual void STDCALL Kick( int nClient ) = 0;
	// get next message. 'received' must be a buffer of, at least, 128 elements length
	virtual bool STDCALL GetMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt ) = 0;
	// ping of the client, -1 if client doesn't exist
	virtual const float STDCALL GetPing( const int nClientID ) = 0;
	// time since last message was received from this client
	virtual const float STDCALL GetTimeSinceLastRecv( const int nClientID ) = 0;
	// 
	// CRAP functions to work with GameSpy
	//
	virtual SOCKET STDCALL GetSocket() = 0;
	virtual sockaddr* STDCALL GetSockAddr() = 0;

	// auxiliary multichannel functions
	virtual void STDCALL AddChannel( const int nChannelID, const std::unordered_set<BYTE> &channelMessages ) = 0;
	virtual void STDCALL RemoveChannel( const int nChannelID ) = 0;

	virtual bool STDCALL GetChannelMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt, const int nChannel ) = 0;
	
	// for debug of lagging net
	virtual void STDCALL PauseNet() {}
	virtual void STDCALL UnpauseNet() {}
	virtual void STDCALL SetLag( const NTimer::STime period ) {}
	
	// for debug
	virtual const char* STDCALL GetAddressByClientID( const int nClientID ) const { return "Unknown"; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __NET_DRIVER_H__
