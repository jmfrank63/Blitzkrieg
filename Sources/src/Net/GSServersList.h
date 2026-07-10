#ifndef __GS_SERVERS_LIST_H__
#define __GS_SERVERS_LIST_H__
#pragma ONCE
#include "NetDriver.h"

class CGSServersListDriver : public INetDriver
{
	OBJECT_NORMAL_METHODS( CGSServersListDriver );

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
