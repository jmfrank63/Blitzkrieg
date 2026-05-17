#ifndef __GS_QUERY_REPORTING_DRIVER__
#define __GS_QUERY_REPORTING_DRIVER__
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "NetDriver.h"
#include "..\GameSpy\QueryReporting\gqueryreporting.h"

#include "..\Misc\Thread.h"
#include "..\Misc\Win32Helper.h"
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGSQueryReportingDriver : public INetDriver, public CThread
{
	OBJECT_NORMAL_METHODS( CGSQueryReportingDriver );

	// thread
	NWin32Helper::CCriticalSection criticalSection;
	bool bInitialized;

	// callbacks
	static void qr_basic_callback( char *pszOutBuf, int nMaxLen, void *pUserData );
	static void qr_info_callback( char *pszOutBuf, int nMaxLen, void *pUserData );
	static void qr_rules_callback( char *pszOutBuf, int nMaxLen, void *pUserData );
	static void qr_players_callback( char *pszOutBuf, int nMaxLen, void *pUserData );
	//
	// callback helpers
	void QRBasicCallBack( char *pszOutBuf, int nMaxLen );
	void QRInfoCallBack( char *pszOutBuf, int nMaxLen );
	void QRRulesCallBack( char *pszOutBuf, int nMaxLen );
	void QRPlayersCallBack( char *pszOutBuf, int nMaxLen );
	
	//
	SGameInfo gameInfo;
	int nGamePort;
	
	//
	qr_t gsHandler;

protected:
	virtual void Step();
public:
	CGSQueryReportingDriver();
	virtual ~CGSQueryReportingDriver();
	//
	virtual bool STDCALL Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly );
	// get current state (active/inactive/connecting)
	virtual EState STDCALL GetState() const;
	// get reject reason (then )
	virtual EReject STDCALL GetRejectReason() const;
	// start game (server)
	virtual void STDCALL StartGame();
	// start sending game info (for server)
	virtual void STDCALL StartGameInfoSend( const SGameInfo &gameInfo );
	// stop sending game info (for server)
	virtual void STDCALL StopGameInfoSend();
	// start accepting new players (for server)
	virtual void STDCALL StartNewPlayerAccept();
	// stop accepting new players (for server)
	virtual void STDCALL StopNewPlayerAccept();

	//???
	virtual SOCKET STDCALL GetSocket() { NI_ASSERT_T( false, "wrong call" ); return 0; }
	virtual sockaddr* STDCALL GetSockAddr() { NI_ASSERT_T( false, "wrong call" ); return 0; }

	// unnecessary functions (related with messages sending)
	// get game info (for client)
	virtual bool STDCALL GetGameInfo( int nIdx, INetNodeAddress *pAddr, bool *pWrongVersion, float *pPing, SGameInfo *pGameInfo ) { NI_ASSERT_T( false, "wrong call" ); return false; }
	virtual void STDCALL RefreshServersList() { NI_ASSERT_T( false, "wrong call" ); }

	// connect to the game with particular address
	virtual void STDCALL ConnectGame( const INetNodeAddress *pAddr, IDataStream *pPwd ) { NI_ASSERT_T( false, "wrong call" ); }

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

	// auxiliary multichannel functions
	virtual void STDCALL AddChannel( const int nChannelID, const std::unordered_set<BYTE> &channelMessages ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual void STDCALL RemoveChannel( const int nChannelID ) { NI_ASSERT_T( false, "wrong call" ); }
	virtual bool STDCALL GetChannelMessage( EMessage *pMsg, int *pClientID, int *received, IDataStream *pPkt, const int nChannel ) { NI_ASSERT_T( false, "wrong call" ); return false; }
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif __GS_QUERY_REPORTING_DRIVER__
