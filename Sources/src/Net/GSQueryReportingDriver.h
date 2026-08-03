#ifndef __GS_QUERY_REPORTING_DRIVER__
#define __GS_QUERY_REPORTING_DRIVER__
#pragma ONCE
#include "NetDriver.h"

class CGSQueryReportingDriver : public INetDriver
{
	OBJECT_NORMAL_METHODS( CGSQueryReportingDriver );

	SGameInfo gameInfo;

public:
	CGSQueryReportingDriver();
	virtual ~CGSQueryReportingDriver();
	virtual bool STDCALL Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly );
	virtual EState STDCALL GetState() const;
	virtual EReject STDCALL GetRejectReason() const;
	virtual void STDCALL StartGame();
	virtual void STDCALL StartGameInfoSend( const SGameInfo &gameInfo );
	virtual void STDCALL StopGameInfoSend();
	virtual void STDCALL StartNewPlayerAccept();
	virtual void STDCALL StopNewPlayerAccept();

	virtual NPlatform::SocketHandle STDCALL GetSocket() { NI_ASSERT_T( false, "wrong call" ); return 0; }
	virtual NPlatform::SocketAddress* STDCALL GetSockAddr() { NI_ASSERT_T( false, "wrong call" ); return 0; }

	virtual bool STDCALL GetGameInfo( int nIdx, INetNodeAddress *pAddr, bool *pWrongVersion, float *pPing, SGameInfo *pGameInfo ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	virtual void STDCALL RefreshServersList() { NI_ASSERT_T( false, "wrong call" ); }

	virtual void STDCALL ConnectGame( const INetNodeAddress *pAddr, IDataStream *pPwd ) { NI_ASSERT_T( false, "wrong call" ); }

	virtual bool STDCALL SendBroadcast( IDataStream *pPkt ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	virtual bool STDCALL SendDirect( int nClient, IDataStream *pPkt ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	virtual void STDCALL Kick( int nClient ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual bool STDCALL GetMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	virtual const float STDCALL GetPing( const int nClientID ) { NI_ASSERT_T( false, "wrong call" ); return 0.0f; }
	virtual const float STDCALL GetTimeSinceLastRecv( const int nClientID ) { NI_ASSERT_T( false, "wrong call" ); return 0.0f; }

	virtual void STDCALL AddChannel( const int nChannelID, const std::unordered_set<BYTE> &channelMessages ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual void STDCALL RemoveChannel( const int nChannelID ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual bool STDCALL GetChannelMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt, const int nChannel ) { NI_ASSERT_T( false, "wrong call" ); return false; }
};
#endif // __GS_QUERY_REPORTING_DRIVER__
