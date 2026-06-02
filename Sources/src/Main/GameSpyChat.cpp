#include "stdafx.h"

#include "GameSpyChat.h"
#include "LanChat.h"
#include "ChatMessages.h"

#include "..\Main\GameTimer.h"
chatGlobalCallbacks CGameSpyChat::globalCallbacks;
chatChannelCallbacks CGameSpyChat::channelCallbacks;
CGameSpyChat::CGameSpyChat()
: CThread( 500 ), chat( 0 ), eInitState( EIS_NONE ), szRealUserName( "" ), lastTimeToTryToReconnect( 0 ),
	eMode( EUM_NONE )
{
}
CGameSpyChat::~CGameSpyChat()
{
	StopThread();
	if ( chat /*&& eInitState != EIS_NONE && eInitState != EIS_INITIALIZING && eInitState != EIS_DISCONNECTED*/ )
		DisconnectFromChat( true );
}
void CGameSpyChat::InitGSChat( const char *pszRealUserName, const char *pszNick )
{
	szRealUserName = pszRealUserName;
	szNick = pszNick;
	
	std::string szSecretKey;
	szSecretKey.resize( 6 );

	szSecretKey[0] = 'f';
	szSecretKey[1] = 'Y';
	szSecretKey[2] = 'D';
	szSecretKey[3] = 'X';
	szSecretKey[4] = 'B';
	szSecretKey[5] = 'N';

	memset(&globalCallbacks, 0, sizeof(chatGlobalCallbacks));
	globalCallbacks.raw = rawCallBack;
	globalCallbacks.disconnected = disconnectedCallBack;
	globalCallbacks.privateMessage = privateMessageCallBack;
	globalCallbacks.invited = invitedCallBack;
	globalCallbacks.param = this;
	
	eInitState = EIS_INITIALIZING;	
	chat = chatConnectSecure(
		"peerchat.gamespy.com", 6667, szNick.c_str(), szNick.c_str(),
		GetGlobalVar("GameSpyGameName"), szSecretKey.c_str(), 
		&globalCallbacks,
		nickErrorCallback, fillInUserCallback, connectCallback,
		this, CHATFalse
	);
}
void CGameSpyChat::InitGSChat( const WORD *pszUserName )
{
	NStr::SetCodePage( GetACP() );
	const std::wstring wszUserName = MakeWideStringFromWordString( pszUserName );
	szRealUserName = NStr::ToAscii( wszUserName );
	szNick = NStr::ToAscii( wszUserName );
	nNamePostfix = -1;

	eMode = EUM_NONE;	
	InitGSChat( szRealUserName.c_str(), szNick.c_str() );

	RunThread();
}
void CGameSpyChat::InitInGameChat( INetDriver *pNetDriver )
{
	pInGameChat = new CLanChat();
	static_cast_ptr<CLanChat*>(pInGameChat)->InitInGameChat( pNetDriver );
}
void CGameSpyChat::DisconnectFromChat( bool bShutDown )
{
	chatDisconnect( chat );
	chat = 0;
	eInitState = EIS_NONE;
	if ( !bShutDown )
		lastTimeToTryToReconnect = GetSingleton<IGameTimer>()->GetAbsTime();
}
void CGameSpyChat::SendMessage( const WORD *pszMessage, const SPlayerInfo &ourPlayer )
{
	if ( !IsInChatRoom() )
	{
		if ( pInGameChat )
			pInGameChat->SendMessage( pszMessage, ourPlayer );
	}
	else
	{
		if ( eInitState == EIS_INITIALIZED )
		{
			NStr::SetCodePage( GetACP() );
			const std::wstring wszMessage = MakeWideStringFromWordString( pszMessage );
			chatSendChannelMessage( chat, GetGlobalVar("GameSpyChatName"), NStr::ToAscii( wszMessage ).c_str(), CHAT_MESSAGE );
		}
	}
}
void CGameSpyChat::SendWhisperMessage( const WORD *pszMessage, const SPlayerInfo &toPlayer, const SPlayerInfo &ourPlayer )
{
	if ( !IsInChatRoom() )
	{
		if ( pInGameChat )
			pInGameChat->SendWhisperMessage( pszMessage, toPlayer, ourPlayer );
	}
}
void CGameSpyChat::SendMessage( const WORD *pszMessage, const WORD *wszToPlayer, const bool bWhisper )
{
	if ( eInitState == EIS_INITIALIZED && IsInChatRoom() )
	{
		NStr::SetCodePage( GetACP() );
		if ( bWhisper )
		{
			const std::wstring wszToPlayerName = MakeWideStringFromWordString( wszToPlayer );
			const std::wstring wszMessage = MakeWideStringFromWordString( pszMessage );
			const std::wstring wszNick = NStr::ToUnicode( szNick );
			chatSendUserMessage( chat, NStr::ToAscii( wszToPlayerName ).c_str(), NStr::ToAscii( wszMessage ).c_str(), CHAT_MESSAGE );
			messages.AddMessage( new CChatMessage( pszMessage, reinterpret_cast<const WORD*>( wszNick.c_str() ), true ) );
		}
		else
		{
			const std::wstring wszMessage = MakeWideStringFromWordString( pszMessage );
			chatSendChannelMessage( chat, GetGlobalVar("GameSpyChatName"), NStr::ToAscii( wszMessage ).c_str(), CHAT_MESSAGE );
		}
	}
}
bool CGameSpyChat::IsDisconnected() const
{
	return eInitState == EIS_NONE || eInitState == EIS_CHANGED_NICK || eInitState == EIS_DISCONNECTED;
}
void CGameSpyChat::Segment()
{
	if ( pInGameChat )
		pInGameChat->Segment();

	if ( eInitState == EIS_DISCONNECTED )
		DisconnectFromChat( false );
	else if ( eInitState == EIS_CHANGED_NICK )
	{
		eInitState = EIS_INITIALIZING;		
		chatRetryWithNick( chat, szNick.c_str() );
	}
	else if ( eInitState == EIS_NONE && szRealUserName != "" )
	{
		NTimer::STime curTime = GetSingleton<IGameTimer>()->GetAbsTime();
		if ( lastTimeToTryToReconnect + 5000 < curTime )
		{
			NStr::SetCodePage( GetACP() );
			InitGSChat( reinterpret_cast<const WORD*>( NStr::ToUnicode( szRealUserName.c_str() ).c_str() ) );
			lastTimeToTryToReconnect = curTime;
		}
	}
}
IMultiplayerMessage* CGameSpyChat::GetMessage()
{
	IMultiplayerMessage *pMessage = 0;
	if ( pInGameChat )
		pMessage = pInGameChat->GetMessage();

	if ( !pMessage )
		pMessage = messages.GetMessage();

	return pMessage;
}
void CGameSpyChat::Step()
{
	if ( chat /*&& eState !IsDisconnected() */)
		chatThink( chat );

	if ( eInitState == EIS_INITIALIZED_NOT_ENTERED )
	{
		eInitState = EIS_INITIALIZED_ENTERING;

		memset( &channelCallbacks, 0, sizeof(chatChannelCallbacks) );
		channelCallbacks.channelMessage = channelMessage;
		channelCallbacks.kicked = kicked;
		channelCallbacks.userJoined = userJoined;
		channelCallbacks.userParted = userParted;
		channelCallbacks.userChangedNick = userChangedNick;
		channelCallbacks.topicChanged = topicChanged;
		channelCallbacks.channelModeChanged = channelModeChanged;
		channelCallbacks.userModeChanged = userModeChanged;
		channelCallbacks.userListUpdated = userListUpdated;
		channelCallbacks.newUserList = newUserList;
		channelCallbacks.broadcastKeyChanged = broadcastKeyChanged;
		channelCallbacks.param = this;
		
		chatEnterChannel( 
			chat, GetGlobalVar("GameSpyChatName"), 0,
			&channelCallbacks, enterChannelCallback,
			this, CHATFalse
		);
	}
}
void CGameSpyChat::RawCallBack( const char *pRaw )
{
}
void CGameSpyChat::DisconnectedCallBack( const char *pReason )
{
	if ( !IsDisconnected() )
	{
		if ( IsInChatRoom() )
			messages.AddMessage( new CSimpleChatMessage( CSimpleChatMessage::EP_DISCONNECTED ) );
		eInitState = EIS_DISCONNECTED;
	}
}
void CGameSpyChat::PrivateMessageCallBack( const char *pUser, const char *pMessage, int nType )
{
	messages.AddMessage( new CChatMessage( pMessage, pUser, true ) );
}
void CGameSpyChat::InvitedCallBack( const char *pChannel, const char *pUser )
{
}
void CGameSpyChat::NickErrorCallback( CHAT chat, int nType, const char *pszNick )
{
	std::string szNamePostfix;
	if ( nNamePostfix >= 0 )
	{
		szNamePostfix = NStr::Format( "{%d}", nNamePostfix );
		const int nNamePostfixSize = szNamePostfix.size();
		const int nNickSize = szNick.size();
		NI_ASSERT_T( nNamePostfixSize <= nNickSize, "Wrong size of name postfix" );
		NI_ASSERT_T( szNick.substr( nNickSize - nNamePostfixSize, nNamePostfixSize ) == szNamePostfix, "Wrong postfix" );

		szNick.erase( nNickSize - nNamePostfixSize, nNamePostfixSize );
		++nNamePostfix;
	}
	else
		nNamePostfix = 0;

	szNamePostfix = NStr::Format( "{%d}", nNamePostfix );
	szNick += szNamePostfix;

	eInitState = EIS_CHANGED_NICK;
}
void CGameSpyChat::FillInUserCallback( unsigned int nIP, char user[128] )
{
}
void CGameSpyChat::ConnectCallback( CHATBool success )
{
	switch ( success )
	{
		case CHATTrue:
			if ( eInitState == EIS_INITIALIZING )
				eInitState = EIS_INITIALIZED_NOT_ENTERED;

			break;
		case CHATFalse:
		if ( IsInChatRoom() )			
				messages.AddMessage( new CSimpleChatMessage( CSimpleChatMessage::EP_FAILED_TO_CONNECT ) );
			eInitState = EIS_DISCONNECTED;
			break;
	}
}
void CGameSpyChat::ChannelMessage( const char *pChannel, const char *pUser, const char *pMessage, int nType )
{
	if ( IsInChatRoom() )
		messages.AddMessage( new CChatMessage( pMessage, pUser, false ) );
}
void CGameSpyChat::Kicked( const char *pUser, const char *pReason )
{
	if ( IsInChatRoom() )
		messages.AddMessage( new CSimpleChatMessage( CSimpleChatMessage::EP_KICKED ) );
	eInitState = EIS_DISCONNECTED;
}
void CGameSpyChat::UserJoined( const char *pChannel, const char *pUser, int nMode )
{
	if ( IsInChatRoom() )	
		messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_JOINED, pUser, IChat::EUserMode( nMode ) ) );
}
void CGameSpyChat::UserParted( const char *pChannel, const char *pUser, int nWhy, const char *pReason, const char *pKicker )
{
	if ( IsInChatRoom() )	
		messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_PARTED, pUser, EUM_NONE ) );
}
void CGameSpyChat::UserChangedNick( const char *pChannel, const char *pOldNick, const char *pNewNick )
{
	if ( IsInChatRoom() )	
		messages.AddMessage( new CChatUserChangedNick( pOldNick, pNewNick ) );
}
void CGameSpyChat::TopicChanged( const char *pChannel, const char *pTopic )
{
}
void CGameSpyChat::ChannelModeChanged( const char *pChannel, CHATChannelMode *pMode )
{
}
void CGameSpyChat::UserModeChanged( const char *pChannel, const char *pUser, int nMode )
{
}
void CGameSpyChat::UserListUpdated( const char *pChannel )
{
}
void CGameSpyChat::NewUserList( const char *pChannel, int nNum, const char **ppUsers, int *pModes )
{
}
void CGameSpyChat::BroadcastKeyChanged( const char *pChannel, const char *pUser, const char *pKey, const char *pValue )
{
}
void CGameSpyChat::EnterChannelCallBack( CHATBool success, CHATEnterResult result, const char *pChannel )
{
	switch ( success )
	{
		case CHATTrue:
			if ( eInitState == EIS_INITIALIZED_ENTERING )
			{
				UserModeChanged( eMode );
				
				eInitState = EIS_INITIALIZED;
				if ( IsInChatRoom() )
					chatEnumUsers( chat, GetGlobalVar("GameSpyChatName"), chatEnumUsersCallback, this, CHATFalse );
			}

			break;
		case CHATFalse:
			if ( IsInChatRoom() )
				messages.AddMessage( new CSimpleChatMessage( CSimpleChatMessage::EP_FAILED_TO_CONNECT ) );
			eInitState = EIS_DISCONNECTED;

			break;
	}
}
void CGameSpyChat::ChatEnumUsersCallback( CHATBool success, const char *pChannel, int numUsers, const char **ppUsers, int *pNModes )
{
	if ( IsInChatRoom() )
	{
		for ( int i = 0; i < numUsers; ++i )
			messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_JOINED, ppUsers[i], IChat::EUserMode( pNModes[i] ) ) );
	}
}
void CGameSpyChat::UserModeChanged( const EUserMode _eMode )
{
	eMode = _eMode;
	if ( eMode == EUM_NOT_AWAY )
		eMode = EUM_IN_GS_CHAT;

	if ( eInitState == EIS_INITIALIZED )
	{

		if ( IsInChatRoom() )
			chatEnumUsers( chat, GetGlobalVar("GameSpyChatName"), chatEnumUsersCallback, this, CHATFalse );
	}
}
void CGameSpyChat::DestroyInGameChat()
{
	pInGameChat = 0;
}
void CGameSpyChat::rawCallBack( CHAT chat, const char *pRaw, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->RawCallBack( pRaw );
}
void CGameSpyChat::disconnectedCallBack( CHAT chat, const char *pReason, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->DisconnectedCallBack( pReason );
}
void CGameSpyChat::privateMessageCallBack( CHAT chat, const char *pUser, const char *pMessage, int nType, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->PrivateMessageCallBack( pUser, pMessage, nType );
}
void CGameSpyChat::invitedCallBack( CHAT chat, const char *pChannel, const char *pUser, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->InvitedCallBack( pChannel, pUser );
}
void CGameSpyChat::nickErrorCallback( CHAT chat, int nType, const char *pszNick, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->NickErrorCallback( chat, nType, pszNick );
}
void CGameSpyChat::fillInUserCallback( CHAT chat, unsigned int nIP, char user[128], void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->FillInUserCallback( nIP, user );
}
void CGameSpyChat::connectCallback( CHAT chat, CHATBool success, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->ConnectCallback( success );
}
void CGameSpyChat::channelMessage( CHAT chat, const char *pChannel, const char *pUser, const char *pMessage, int nType, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->ChannelMessage( pChannel, pUser, pMessage, nType );
}
void CGameSpyChat::kicked( CHAT chat, const char *pChannel, const char *pUser, const char *pReason, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->Kicked( pUser, pReason );
}
void CGameSpyChat::userJoined( CHAT chat, const char *pChannel, const char *pUser, int mode, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->UserJoined( pChannel, pUser, mode );
}
void CGameSpyChat::userParted( CHAT chat, const char *pChannel, const char *pUser, int why, const char *pReason, const char *pKicker, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->UserParted( pChannel, pUser, why, pReason, pKicker );
}
void CGameSpyChat::userChangedNick( CHAT chat, const char *pChannel, const char *pOldNick, const char *pNewNick, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->UserChangedNick( pChannel, pOldNick, pNewNick );
}
void CGameSpyChat::topicChanged( CHAT chat, const char *pChannel, const char *pTopic, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->TopicChanged( pChannel, pTopic );
}
void CGameSpyChat::channelModeChanged( CHAT chat, const char *pChannel, CHATChannelMode *pMode, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->ChannelModeChanged( pChannel, pMode );
}
void CGameSpyChat::userModeChanged( CHAT chat, const char *pChannel, const char *pUser, int mode, void *pParam )
{
}
void CGameSpyChat::userListUpdated( CHAT chat, const char *pChannel, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->UserListUpdated( pChannel );
}
void CGameSpyChat::newUserList( CHAT chat, const char *pChannel, int num, const char **ppUsers, int *pModes, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->NewUserList( pChannel, num, ppUsers, pModes );
}
void CGameSpyChat::broadcastKeyChanged( CHAT chat, const char *pChannel, const char *pUser, const char *pKey, const char *pValue, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->BroadcastKeyChanged( pChannel, pUser, pKey, pValue );
}
void CGameSpyChat::enterChannelCallback( CHAT chat, CHATBool success, CHATEnterResult result, const char *pChannel, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->EnterChannelCallBack( success, result, pChannel );
}
void CGameSpyChat::chatEnumUsersCallback( CHAT chat, CHATBool success, const char *pChannel, int numUsers, const char **ppUsers, int *pNModes, void *pParam )
{
	reinterpret_cast<CGameSpyChat*>(pParam)->ChatEnumUsersCallback( success, pChannel, numUsers, ppUsers, pNModes );
}
