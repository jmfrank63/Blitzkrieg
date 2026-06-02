#ifndef __PEER_CHAT_H__
#define __PEER_CHAT_H__
#pragma ONCE
#include "GameCreationInterfaces.h"
#include "MessagesStore.h"

#include "..\Misc\Thread.h"
#include "..\GameSpy\peer\peer.h"
class CGameSpyPeerChat : public IChat, public CThread
{
	OBJECT_NORMAL_METHODS( CGameSpyPeerChat );

	const static std::string szRoomKeyName;

	PEER peer;

	enum EInitState
	{
		EIS_NONE,
		EIS_INITIALIZING,
		EIS_INITIALIZED_NOT_ENTERED,
		EIS_INITIALIZED_ENTERING,
		EIS_INITIALIZED,
		EIS_CHANGED_NICK,
		EIS_DISCONNECTED,
	};
	EInitState eInitState;
	NTimer::STime lastTimeToTryToReconnect;

	EUserMode eMode;

	CMessagesStore messages;

	std::string szRealUserName;	
	std::string szNick;
	int nNamePostfix;

	CPtr<IChat> pInGameChat;

	static PEERCallbacks callBacks;

	static void disconnectedCallBack( PEER peer, const char *reason, void *param );
	static void roomMessageCallBack( PEER peer, RoomType roomType, const char *nick, const char *message,	MessageType messageType, void *param );
	static void roomUTMCallBack( PEER peer, RoomType roomType, const char *nick, const char *command,	const char *parameters, PEERBool authenticated, void *param );
	static void peerRoomNameChangedCallback( PEER peer, RoomType roomType, void *param );
	static void roomModeChangedCallBack( PEER peer, RoomType roomType, CHATChannelMode *mode, void *param );
	static void playerMessageCallBack( PEER peer, const char *nick, const char *message, MessageType messageType, void *param );
	static void playerUTMCallBack( PEER peer, const char *nick, const char *command, const char *parameters, PEERBool authenticated, void *param );
	static void readyChangedCallBack( PEER peer, const char *nick, PEERBool ready, void *param );
	static void gameStartedCallBack( PEER peer, unsigned int IP, const char *message, void *param );
	static void playerJoinedCallBack( PEER peer, RoomType roomType, const char *nick, void *param );
	static void playerLeftCallBack( PEER peer, RoomType roomType, const char *nick, const char *reason, void *param );
	static void kickedCallBack( PEER peer, RoomType roomType, const char *nick, const char *reason, void *param );
	static void newPlayerListCallBack( PEER peer, RoomType roomType, void *param );
	static void playerChangedNickCallBack( PEER peer, RoomType roomType, const char *oldNick, const char *newNick, void *param );
	static void playerInfoCallBack( PEER peer, RoomType roomType, const char *nick, unsigned int IP, int profileID, void *param );
	static void playerFlagsChangedCallBack( PEER peer, RoomType roomType, const char *nick, int oldFlags, int newFlags, void *param );
	static void pingCallBack( PEER peer, const char *nick, int ping, void *param );
	static void crossPingCallBack( PEER peer, const char *nick1, const char *nick2, int crossPing, void *param );
	static void globalKeyChangedCallBack( PEER peer, const char *nick, const char *key, const char *value, void *param );
	static void roomKeyChangedCallBack( PEER peer, RoomType roomType, const char *nick, const char *key, const char *value, void *param );
	static void GOABasicCallBack( PEER peer, PEERBool playing, char *outbuf, int maxlen, void *param );
	static void GOAInfoCallBack( PEER peer, PEERBool playing, char *outbuf, int maxlen, void *param );
	static void GOARulesCallBack( PEER peer, PEERBool playing, char *outbuf, int maxlen, void *param );
	static void GOAPlayersCallBack( PEER peer, PEERBool playing, char *outbuf, int maxlen, void *param );

	void Disconnected( const char *pszReason );
	void RoomMessage( RoomType roomType, const char *pszNick, const char *pszMessage, MessageType messageType );
	void RoomUTM( RoomType roomType, const char *pszNick, const char *pszCommand,	const char *pszParameters, PEERBool authenticated );
	void RoomNameChanged( RoomType roomType );
	void RoomModeChanged( RoomType roomType, CHATChannelMode *pMode );
	void PlayerMessage( const char *pszNick, const char *pszMessage, MessageType messageType );
	void PlayerUTM( const char *pszNick, const char *pszCommand, const char *pszParameters, PEERBool authenticated );
	void ReadyChanged( const char *nick, PEERBool ready );
	void GameStarted( unsigned int IP, const char *message );
	void PlayerJoined( RoomType roomType, const char *pszNick );
	void PlayerLeft( RoomType roomType, const char *pszNick, const char *pszReason );
	void Kicked( RoomType roomType, const char *pszNick, const char *pszReason );
	void NewPlayerList( RoomType roomType );
	void PlayerChangedNick( RoomType roomType, const char *pszOldNick, const char *pszNewNick );
	void PlayerInfo( RoomType roomType, const char *pszNick, unsigned int IP, int profileID );
	void PlayerFlagsChanged( RoomType roomType, const char *pszNick, int oldFlags, int newFlags );
	void Ping( const char *pszNick, int ping );
	void CrossPing( const char *pszNick1, const char *pszNick2, int crossPing );
	void GlobalKeyChanged( const char *pszNick, const char *pszKey, const char *pszValue );
	void RoomKeyChanged( RoomType roomType, const char *pszNick, const char *pszKey, const char *pszValue );
	void GOABasic( PEERBool playing, char *outbuf, int maxlen );
	void GOAInfo( PEERBool playing, char *pOutbuf, int maxlen );
	void GOARules( PEERBool playing, char *pOutbuf, int maxlen );
	void GOAPlayers( PEERBool playing, char *pOutbuf, int maxlen );

	static void nickErrorCallback( PEER peer, int type, const char *nick, void *param );
	static void connectCallback( PEER peer, PEERBool success, void *param );
	static void enumPlayersCallback( PEER peer, PEERBool success, RoomType roomType, int index, const char *nick, int flags, void *param );
	static void joinRoomCallback( PEER peer, PEERBool success, PEERJoinResult result, RoomType roomType, void *param );
	static void getRoomKeysCallBack( PEER peer, PEERBool success, RoomType roomType, const char *nick, int num, char **keys, char **values, void *param );
		
	void NickError( int nType, const char *pszNick );
	void Connect( PEERBool success );
	void EnumPlayers( PEERBool success, RoomType roomType, int index, const char *pszNick, int flags );
	void JoinRoom( PEERBool success, PEERJoinResult result, RoomType roomType );
	void GetRoomKeys( PEERBool success, const char *pszNick, int num, char **ppszKeys, char **ppszValues );

	void InitGSChat( const char *pszRealUserName, const char *pszNick );
	void DisconnectFromChat( bool bShutDown );
	bool IsDisconnected() const;
	void AnalyzeMode( int nFlags, EUserMode *pMode );
	void SetRoomKeys();
	bool AnalyzeRoomKeys( const char *pszKeyName, const char *pszKeyValue, EUserMode *pMode );
	void AskForRoomKeys();

	bool IsInChatRoom() const { return eMode == EUM_IN_GS_CHAT || eMode == EUM_AWAY; }
protected:
	virtual void Step();
public:
	CGameSpyPeerChat();
	virtual ~CGameSpyPeerChat();

	virtual void STDCALL InitGSChat( const WORD *pszUserName );
	virtual void STDCALL InitInGameChat( INetDriver *pNetDriver );
	virtual void STDCALL DestroyInGameChat();
	
	virtual void STDCALL SendMessage( const WORD *pszMessage, const SPlayerInfo &ourPlayer );
	virtual void STDCALL SendWhisperMessage( const WORD *pszMessage, const SPlayerInfo &toPlayer, const SPlayerInfo &ourPlayer );
	virtual void STDCALL SendMessage( const WORD *pszMessage, const WORD *wszToPlayer, const bool bWhisper );

	virtual void STDCALL Segment();
	
	virtual interface IMultiplayerMessage* STDCALL GetMessage();

	virtual void STDCALL UserModeChanged( const EUserMode eMode );
};
#endif // __PEER_CHAT_H__
