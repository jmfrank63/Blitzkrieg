#ifndef __GS_SERVERS_LIST_H__
#define __GS_SERVERS_LIST_H__
#pragma ONCE
#include "NetDriver.h"
#include "..\GameSpy\cengine\goaceng.h"

#include "..\Misc\Thread.h"
#include "..\Misc\Win32Helper.h"
class CGSServersListDriver : public INetDriver, public CThread
{
	OBJECT_NORMAL_METHODS( CGSServersListDriver );

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

	static void ListCallBack( GServerList serverList, int nMsg, void *pInstance, void *pParam1, void *pParam2 );
	void List( GServerList ServerList, int nMsg, void *pParam1, void *pParam2 );

	void AddServer( GServer server );
protected:
	virtual void Step();
public:
	CGSServersListDriver();
	virtual ~CGSServersListDriver();
	virtual bool STDCALL Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly );
	virtual EState STDCALL GetState() const;
	virtual EReject STDCALL GetRejectReason() const;
	virtual bool STDCALL GetGameInfo( int nIdx, INetNodeAddress *pAddr, bool *pWrongVersion, float *pPing, SGameInfo *pGameInfo );
	virtual void STDCALL RefreshServersList();

	virtual void STDCALL ConnectGame( const INetNodeAddress *pAddr, IDataStream *pPwd ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual void STDCALL StartGame() { NI_ASSERT_T( false, "wrong call" ); }
	virtual void STDCALL StartGameInfoSend( const SGameInfo &gameInfo ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual void STDCALL StopGameInfoSend() { NI_ASSERT_T( false, "wrong call" ); }
	virtual void STDCALL StartNewPlayerAccept() { NI_ASSERT_T( false, "wrong call" ); }
	virtual void STDCALL StopNewPlayerAccept() { NI_ASSERT_T( false, "wrong call" ); }
	virtual bool STDCALL SendBroadcast( IDataStream *pPkt ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	virtual bool STDCALL SendDirect( int nClient, IDataStream *pPkt ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	virtual void STDCALL Kick( int nClient ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual bool STDCALL GetMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	virtual const float STDCALL GetPing( const int nClientID ) { NI_ASSERT_T( false, "wrong call" ); return 0.0f; }
	virtual const float STDCALL GetTimeSinceLastRecv( const int nClientID ) { NI_ASSERT_T( false, "wrong call" ); return 0.0f; }
	virtual SOCKET STDCALL GetSocket() { NI_ASSERT_T( false, "wrong call" ); return 0; }
	virtual sockaddr* STDCALL GetSockAddr() { NI_ASSERT_T( false, "wrong call" ); return 0; }

	virtual void STDCALL AddChannel( const int nChannelID, const std::unordered_set<BYTE> &channelMessages ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual void STDCALL RemoveChannel( const int nChannelID ) { NI_ASSERT_T( false, "wrong call" ); }

	virtual bool STDCALL GetChannelMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt, const int nChannel ) { NI_ASSERT_T( false, "wrong call" ); return false; }
};
#endif // __GS_SERVERS_LIST_H__
