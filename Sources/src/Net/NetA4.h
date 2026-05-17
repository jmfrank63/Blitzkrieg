#ifndef __NETA4_H_
#define __NETA4_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Streams.h"
#include "NetDriver.h"
#include "NetLowest.h"
#include "NetAcks.h"
#include "NetStream.h"
#include "NetServerInfo.h"
#include "NetLogin.h"
#include "NetPeer2Peer.h"

#include "..\Misc\HPTimer.h"
#include "..\Misc\Win32Helper.h"

#if !defined(_FINALRELEASE) || defined(_DEVVERSION)
#define __TEST_LAGS__
#endif // !defined(_FINALRELEASE) || defined(_DEVVERSION)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NNet
{
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CNetDriver : public INetDriver
{
	OBJECT_NORMAL_METHODS( CNetDriver );
public:
	struct SServer
	{
		INetNodeAddress *pAddr;
		bool bWrongVersion;
		float fPing;
		CMemoryStream info;
	};
	struct SMessage
	{
		EMessage msg;
		int nClientID;
		std::vector<int> received;
		CMemoryStream pkt;
	};
private:
	//
	struct SClientAddressInfo
	{
		CNodeAddress inetAddress;
		CNodeAddressSet localAddress;
		
		SClientAddressInfo() {}
		SClientAddressInfo( const CNodeAddress &_inetAddress, const CNodeAddressSet &_localAddress ) 
			: inetAddress(_inetAddress), localAddress(_localAddress) {}
	};
	//
	struct SPeer
	{
		CP2PTracker::UCID clientID;
		CNodeAddress currentAddr;
		SClientAddressInfo addrInfo;
		CAckTracker acks;
		CStreamTracker data;
		bool bTryShortcut;
	};

	struct SGameInfoWrapper : public INetDriver::SGameInfo
	{
		SGameInfoWrapper() { }
		public:
			SGameInfoWrapper( const SGameInfo &gameInfo ) : SGameInfo( gameInfo ) { }

			void Pack( IDataStream *pDataStream );
			void Unpack( IDataStream *pDataStream );
	};
	//
	typedef std::list<SPeer> CPeerList;
	CPeerList clients;
	EState state;
	EReject lastReject;
	NHPTimer::STime lastTime;
	CServerInfoSupport serverInfo;
	CLoginSupport login;
	CP2PTracker p2p;
	std::list<SMessage> msgQueue;
	bool bAcceptNewClients;
	CLinksManager links;
	int nGamePort;
	CNodeAddress addr;
	CNodeAddress gameHostAddress;

	SPeer* GetClientByAddr( const CNodeAddress &addr );
	SPeer* GetClient( CP2PTracker::UCID nID );
	void AddClient( const SClientAddressInfo &addr, CP2PTracker::UCID clientID );
	void AddNewP2PClient( const SClientAddressInfo &addr, CP2PTracker::UCID clientID );
	void RemoveClient( CP2PTracker::UCID nID );
	void StepInactive();
	void StepConnecting();
	void StepActive( float fDeltaTime );
	// step net driver
	void Step();

	void ProcessIncomingMessages();
	void ProcessLogin( const CNodeAddress &addr, CBitStream &bits );
	void ProcessNormal( const CNodeAddress &addr, CBitStream &bits );
	void AddOutputMessage( EMessage msg, const CP2PTracker::UCID &_from, 
		CMemoryStream &data, const std::vector<CP2PTracker::UCID> &received );

	void PollMessages( SPeer *pPeer );

	//
	// multichannel
	std::vector< std::unordered_set<BYTE> > channelMsgTypes;

	struct SChannelMessage
	{
		EMessage msg;
		int nClientID;
		CPtr<IDataStream> pPkt;
	};

	std::vector< std::list<SChannelMessage> > channelMsgs;
	std::vector<int> existingChannels;

	bool bMultiChannel;

	//
	// thread
	HANDLE hThread;
	HANDLE hFinishReport;
	HANDLE hStopCommand;
	NWin32Helper::CCriticalSection criticalSection;
	
	// CRAP{ for traffic to winsock measurement
	NTimer::STime lastTrafficCheckTime;
	int nSent;
	// CRAP}

#ifdef __TEST_LAGS__
	NTimer::STime lastSendTime;
	NTimer::STime lastReceiveTime;
	int nMsgCanReceive;
	NTimer::STime lagPeriod;
	bool bPaused;
	bool bSendNow;
	bool bReceiveNow;

	std::list< CPtr<IDataStream> > msgToSendBroadcast;
	std::list< std::pair< int, CPtr<IDataStream> > > msgToSendDirect;

	bool AnalyzeLags();
#endif __TEST_LAGS__

	//
	void ResizeArrays( const int nNewSize );
	void StepMultiChannel();

	virtual void STDCALL StartGameInfoSend( IDataStream *pData );
public:
	CNetDriver();
	~CNetDriver();
	//
	virtual bool STDCALL Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly );
	// get current state (active/inactive/connecting)
	virtual EState STDCALL GetState() const { return state; }
	// get reject reason (then )
	virtual EReject STDCALL GetRejectReason() const { return lastReject; }
	// connect to the game with particular address
	virtual void STDCALL ConnectGame( const INetNodeAddress *pAddr, IDataStream *pPwd );
	// start game (server)
	virtual void STDCALL StartGame();
	// start sending game info (for server), should be compatible with gamespy
	virtual void STDCALL StartGameInfoSend( const SGameInfo &gameInfo );
	// stop sending game info (for server)
	virtual void STDCALL StopGameInfoSend();
	// start accepting new players (for server)
	virtual void STDCALL StartNewPlayerAccept();
	// stop accepting new players (for server)
	virtual void STDCALL StopNewPlayerAccept();
	// get game info (for client)
	virtual bool STDCALL GetGameInfo( int nIdx, INetNodeAddress *pAddr, bool *pWrongVersion, float *pPing, SGameInfo *pGameInfo );
	// refresh servers list ( for client )
	virtual void STDCALL RefreshServersList() { }
	// send broadcast message for all
	virtual bool STDCALL SendBroadcast( IDataStream *pPkt );
	// send direct message for client 'nClient'
	virtual bool STDCALL SendDirect( int nClient, IDataStream *pPkt );
	// kick player 'nClient'
	virtual void STDCALL Kick( int nClient );
	// get next message
	virtual bool STDCALL GetMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt );
	// ping of the client, -1 if client doesn't exist
	virtual const float STDCALL GetPing( const int nClientID );
	// time since last message was received from this client
	virtual const float STDCALL GetTimeSinceLastRecv( const int nClientID );
	// 
	// CRAP functions to work with GameSpy
	//
	virtual SOCKET STDCALL GetSocket();
	virtual sockaddr* STDCALL GetSockAddr();

	//
	virtual void STDCALL AddChannel( const int nChannelID, const std::unordered_set<BYTE> &channelMessages );
	virtual void STDCALL RemoveChannel( const int nChannelID );
	// received �� �����������!
	virtual bool STDCALL GetChannelMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt, const int nChannel );
	// for debug
	virtual const char* STDCALL GetAddressByClientID( const int nClientID ) const;

	virtual void STDCALL PauseNet();
	virtual void STDCALL UnpauseNet();
	virtual void STDCALL SetLag( const NTimer::STime period );
	
	// thread functions
	friend DWORD WINAPI TheThreadProc( LPVOID lpParameter );

	void StartThread();
	bool CanWork();
	void FinishThread();
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INetDriver* MakeDriver( int nGamePort, bool bClientOnly = false );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
