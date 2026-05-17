#ifndef __GS_SERVERS_LIST_H__
#define __GS_SERVERS_LIST_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "NetDriver.h"
#include "..\GameSpy\cengine\goaceng.h"

#include "..\Misc\Thread.h"
#include "..\Misc\Win32Helper.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGSServersListDriver : public INetDriver, public CThread
{
	OBJECT_NORMAL_METHODS( CGSServersListDriver );

	// thread
	NWin32Helper::CCriticalSection criticalSection;
	
	GServerList serverList;
	static const int SERVER_LIST_UPDATE_PERIOD;
	NTimer::STime endOfLastUpdate;
	bool bUpdating;

	struct SServerInfo
	{
		INetDriver::SGameInfo gameInfo;
		CPtr<INetNodeAddress> pAddr;
		float fPing;
		std::string szIP;
		int nNetVersion;

		SServerInfo() : fPing( 0.0f ) { }

		bool operator==( const SServerInfo &serverInfo )
		{
			return 
				gameInfo == serverInfo.gameInfo && pAddr->IsSameIP( serverInfo.pAddr ) && fPing == serverInfo.fPing;
		}
	};

	std::list<SServerInfo> servers;
	int nNetVersion;

	//
	static void ListCallBack( GServerList serverList, int nMsg, void *pInstance, void *pParam1, void *pParam2 );
	void List( GServerList ServerList, int nMsg, void *pParam1, void *pParam2 );

	void AddServer( GServer server );
protected:
	virtual void Step();
public:
	CGSServersListDriver();
	virtual ~CGSServersListDriver();
	//
	virtual bool STDCALL Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly );
	// get current state (active/inactive/connecting)
	virtual EState STDCALL GetState() const;
	// get reject reason (then )
	virtual EReject STDCALL GetRejectReason() const;
	// get game info (for client)
	virtual bool STDCALL GetGameInfo( int nIdx, INetNodeAddress *pAddr, bool *pWrongVersion, float *pPing, SGameInfo *pGameInfo );
		// refresh servers list ( for client )
	virtual void STDCALL RefreshServersList();

	// unnecessary functions
	// connect to the game with particular address
	virtual void STDCALL ConnectGame( const INetNodeAddress *pAddr, IDataStream *pPwd ) { NI_ASSERT_T( false, "wrong call" ); }
	// start game (server)
	virtual void STDCALL StartGame() { NI_ASSERT_T( false, "wrong call" ); }
	// start sending game info (for server)
	virtual void STDCALL StartGameInfoSend( const SGameInfo &gameInfo ) { NI_ASSERT_T( false, "wrong call" ); }
	// stop sending game info (for server)
	virtual void STDCALL StopGameInfoSend() { NI_ASSERT_T( false, "wrong call" ); }
	// start accepting new players (for server)
	virtual void STDCALL StartNewPlayerAccept() { NI_ASSERT_T( false, "wrong call" ); }
	// stop accepting new players (for server)
	virtual void STDCALL StopNewPlayerAccept() { NI_ASSERT_T( false, "wrong call" ); }
	// send broadcast message for all
	virtual bool STDCALL SendBroadcast( IDataStream *pPkt ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	// send direct message for client 'nClient'
	virtual bool STDCALL SendDirect( int nClient, IDataStream *pPkt ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	// kick player 'nClient'
	virtual void STDCALL Kick( int nClient ) { NI_ASSERT_T( false, "wrong call" ); }
	// get next message. 'received' must be a buffer of, at least, 128 elements length
	virtual bool STDCALL GetMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	// ping of the client, -1 if client doesn't exist
	virtual const float STDCALL GetPing( const int nClientID ) { NI_ASSERT_T( false, "wrong call" ); return 0.0f; }
	// time since last message was received from this client
	virtual const float STDCALL GetTimeSinceLastRecv( const int nClientID ) { NI_ASSERT_T( false, "wrong call" ); return 0.0f; }
	// 
	// ???
	virtual SOCKET STDCALL GetSocket() { NI_ASSERT_T( false, "wrong call" ); return 0; }
	virtual sockaddr* STDCALL GetSockAddr() { NI_ASSERT_T( false, "wrong call" ); return 0; }

	// auxiliary multichannel functions
	virtual void STDCALL AddChannel( const int nChannelID, const std::unordered_set<BYTE> &channelMessages ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual void STDCALL RemoveChannel( const int nChannelID ) { NI_ASSERT_T( false, "wrong call" ); }

	virtual bool STDCALL GetChannelMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt, const int nChannel ) { NI_ASSERT_T( false, "wrong call" ); return false; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GS_SERVERS_LIST_H__
