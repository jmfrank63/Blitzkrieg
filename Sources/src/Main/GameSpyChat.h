#ifndef __GAME_SPY_CHAT_H__
#define __GAME_SPY_CHAT_H__
#pragma ONCE
#include "GameCreationInterfaces.h"
#include "MessagesStore.h"

#include "..\Misc\Thread.h"
#include "..\GameSpy\Chat\Chat.h"
class CGameSpyChat : public IChat, public CThread
{
	OBJECT_NORMAL_METHODS( CGameSpyChat );

	CHAT chat;
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

	static chatGlobalCallbacks globalCallbacks;
	static void rawCallBack( CHAT chat, const char *pRaw, void *pParam );
	static void disconnectedCallBack( CHAT chat, const char *pReason, void *pParam );
	static void privateMessageCallBack( CHAT chat, const char *pUser, const char *pMessage, int nType, void *pParam );
	static void invitedCallBack( CHAT chat, const char *pChannel, const char *pUser, void *pParam );

	void RawCallBack( const char *pRaw );
	void DisconnectedCallBack( const char *pReason );
	void PrivateMessageCallBack( const char *pUser, const char *pMessage, int nType );
	void InvitedCallBack( const char *pChannel, const char *pUser );

	static chatChannelCallbacks channelCallbacks;
	static void channelMessage( CHAT chat, const char *pChannel, const char *pUser, const char *pMessage, int nType, void *pParam );
	static void kicked( CHAT chat, const char *pChannel, const char *pUser, const char *pReason, void *pParam );
	static void userJoined( CHAT chat, const char *pChannel, const char *pUser, int mode, void *pParam );
	static void userParted( CHAT chat, const char *pChannel, const char *pUser, int why, const char *pReason, const char *pKicker, void *pParam );
	static void userChangedNick( CHAT chat, const char *pChannel, const char * pOldNick, const char *pNewNick, void *pParam );
	static void topicChanged( CHAT chat, const char *pChannel, const char *pTopic, void * pParam );
	static void channelModeChanged( CHAT chat, const char *pChannel, CHATChannelMode *pMode, void *pParam );
	static void userModeChanged( CHAT chat, const char *pChannel, const char *pUser, int mode, void * pParam );
	static void userListUpdated( CHAT chat, const char *pChannel, void *pParam );
	static void newUserList( CHAT chat, const char *pChannel, int num, const char **ppUsers, int *pModes, void *pParam );
	static void broadcastKeyChanged( CHAT chat, const char *pChannel, const char *pUser, const char *pKey, const char *pValue, void *pParam );

	void ChannelMessage( const char *pChannel, const char *pUser, const char *pMessage, int nType );
	void Kicked( const char *pUser, const char *pReason );
	void UserJoined( const char *pChannel, const char *pUser, int nMode );
	void UserParted( const char *pChannel, const char *pUser, int nWhy, const char *pReason, const char *pKicker );
	void UserChangedNick( const char *pChannel, const char *pOldNick, const char *pNewNick );
	void TopicChanged( const char *pChannel, const char *pTopic );
	void ChannelModeChanged( const char *pChannel, CHATChannelMode *pMode );
	void UserModeChanged( const char *pChannel, const char *pUser, int nMode );
	void UserListUpdated( const char *pChannel );
	void NewUserList( const char *pChannel, int nNum, const char **ppUsers, int *pModes );
	void BroadcastKeyChanged( const char *pChannel, const char *pUser, const char *pKey, const char *pValue );

	static void enterChannelCallback( CHAT chat, CHATBool success, CHATEnterResult result, const char *pChannel, void *pParam );
	void EnterChannelCallBack( CHATBool success, CHATEnterResult result, const char *pChannel );

	static void chatEnumUsersCallback( CHAT chat, CHATBool success, const char *pChannel, int numUsers, const char **ppUsers, int *pNModes, void *pParam );
	void ChatEnumUsersCallback( CHATBool success, const char *pChannel, int numUsers, const char **ppUsers, int *pNModes );

	static void nickErrorCallback( CHAT chat, int nType, const char *pszNick, void *pParam );
	static void fillInUserCallback( CHAT chat, unsigned int nIP, char user[128], void *pParam );
	static void connectCallback( CHAT chat, CHATBool success, void *pParam );

	void NickErrorCallback( CHAT chat, int nType, const char *pszNick );
	void FillInUserCallback( unsigned int nIP, char user[128] );
	void ConnectCallback( CHATBool success );
	
	void InitGSChat( const char *pszRealUserName, const char *pszNick );
	void DisconnectFromChat( bool bShutDown );
	bool IsDisconnected() const;
	bool IsInChatRoom() const { return eMode == EUM_IN_GS_CHAT || eMode == EUM_AWAY; }
protected:
	virtual void Step();
public:
	CGameSpyChat();
	virtual ~CGameSpyChat();

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
#endif // __GAME_SPY_CHAT_H__
