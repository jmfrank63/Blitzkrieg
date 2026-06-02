#include "stdafx.h"

#include "GameSpyPeerChat.h"
#include "LanChat.h"
#include "ChatMessages.h"
PEERCallbacks CGameSpyPeerChat::callBacks;
const std::string CGameSpyPeerChat::szRoomKeyName = "b_player_flag";
CGameSpyPeerChat::CGameSpyPeerChat()
: CThread( 500 ), peer( 0 ), eInitState( EIS_NONE ), szRealUserName( "" ), lastTimeToTryToReconnect( 0 ),
	eMode( EUM_NONE )
{
}
CGameSpyPeerChat::~CGameSpyPeerChat()
{
	StopThread();
	if ( peer )
		DisconnectFromChat( true );
}
void CGameSpyPeerChat::Step()
{
	if ( peer )
		peerThink( peer );

	if ( eInitState == EIS_INITIALIZED_NOT_ENTERED )
	{
		eInitState = EIS_INITIALIZED_ENTERING;
		peerJoinTitleRoom( peer, joinRoomCallback, this, PEERFalse );
	}
}
void CGameSpyPeerChat::InitGSChat( const char *pszRealUserName, const char *pszNick )
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

	memset( &callBacks, 0, sizeof(callBacks) );
	callBacks.crossPing = crossPingCallBack;
	callBacks.disconnected = disconnectedCallBack;
	callBacks.gameStarted = gameStartedCallBack;
	callBacks.globalKeyChanged = globalKeyChangedCallBack;
	callBacks.GOABasic = GOABasicCallBack;
	callBacks.GOAInfo = GOAInfoCallBack;
	callBacks.GOAPlayers = GOAPlayersCallBack;
	callBacks.GOARules = GOARulesCallBack;
	callBacks.kicked = kickedCallBack;
	callBacks.newPlayerList = newPlayerListCallBack;
	callBacks.ping = pingCallBack;
	callBacks.playerChangedNick = playerChangedNickCallBack;
	callBacks.playerFlagsChanged = playerFlagsChangedCallBack;
	callBacks.playerInfo = playerInfoCallBack;
	callBacks.playerJoined = playerJoinedCallBack;
	callBacks.playerLeft = playerLeftCallBack;
	callBacks.playerMessage = playerMessageCallBack;
	callBacks.playerUTM = playerUTMCallBack;
	callBacks.readyChanged = readyChangedCallBack;
	callBacks.roomKeyChanged = roomKeyChangedCallBack;
	callBacks.roomMessage = roomMessageCallBack;
	callBacks.roomModeChanged = roomModeChangedCallBack;
	callBacks.roomUTM = roomUTMCallBack;
	callBacks.param = this;

	eInitState = EIS_INITIALIZING;	
	peer = peerInitialize( &callBacks );

	if ( !peer )
		DisconnectFromChat( false );
	else
	{
		PEERBool pingRooms[3];
		pingRooms[TitleRoom] = PEERTrue;
		pingRooms[GroupRoom] = PEERTrue;
		pingRooms[StagingRoom] = PEERTrue;

		PEERBool crossPingRooms[3];
		crossPingRooms[TitleRoom] = PEERFalse;
		crossPingRooms[GroupRoom] = PEERFalse;
		crossPingRooms[StagingRoom] = PEERTrue;

		const PEERBool bpeerSetTitleResult = 
			peerSetTitle(
				peer, GetGlobalVar("GameSpyGameName"), szSecretKey.c_str(), GetGlobalVar("GameSpyEngineName"), 
				szSecretKey.c_str(), 10, pingRooms, crossPingRooms );

		if ( bpeerSetTitleResult == PEERFalse )
			DisconnectFromChat( false );
		else
			peerConnect( peer, szNick.c_str(), 0, nickErrorCallback, connectCallback, this, PEERFalse );
	}
}
void CGameSpyPeerChat::InitGSChat( const WORD *pszUserName )
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
void CGameSpyPeerChat::InitInGameChat( INetDriver *pNetDriver )
{
	pInGameChat = new CLanChat();
	static_cast_ptr<CLanChat*>(pInGameChat)->InitInGameChat( pNetDriver );
}
void CGameSpyPeerChat::DisconnectFromChat( bool bShutDown )
{
	peerShutdown( peer );

	peer = 0;
	eInitState = EIS_NONE;
	if ( !bShutDown )
		lastTimeToTryToReconnect = GetSingleton<IGameTimer>()->GetAbsTime();

	if ( IsInChatRoom() )
		messages.AddMessage( new CSimpleChatMessage( CSimpleChatMessage::EP_DISCONNECTED ) );
}
void CGameSpyPeerChat::DestroyInGameChat()
{
	pInGameChat = 0;
}
bool CGameSpyPeerChat::IsDisconnected() const
{
	return eInitState == EIS_NONE || eInitState == EIS_CHANGED_NICK || eInitState == EIS_DISCONNECTED;
}
void CGameSpyPeerChat::SendMessage( const WORD *pszMessage, const SPlayerInfo &ourPlayer )
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
			peerMessageRoom( peer, TitleRoom, NStr::ToAscii( wszMessage ).c_str(), NormalMessage );
		}
	}
}
void CGameSpyPeerChat::SendWhisperMessage( const WORD *pszMessage, const SPlayerInfo &toPlayer, const SPlayerInfo &ourPlayer )
{
	if ( !IsInChatRoom() )
	{
		if ( pInGameChat )
			pInGameChat->SendWhisperMessage( pszMessage, toPlayer, ourPlayer );
	}
}
void CGameSpyPeerChat::SendMessage( const WORD *pszMessage, const WORD *wszToPlayer, const bool bWhisper )
{
	if ( eInitState == EIS_INITIALIZED && IsInChatRoom() )
	{
		NStr::SetCodePage( GetACP() );
		if ( bWhisper )
		{
			const std::wstring wszToPlayerName = MakeWideStringFromWordString( wszToPlayer );
			const std::wstring wszMessage = MakeWideStringFromWordString( pszMessage );
			const std::wstring wszNick = NStr::ToUnicode( szNick );
			peerMessagePlayer( peer, NStr::ToAscii( wszToPlayerName ).c_str(), NStr::ToAscii( wszMessage ).c_str(), NormalMessage );
			messages.AddMessage( new CChatMessage( pszMessage, reinterpret_cast<const WORD*>( wszNick.c_str() ), true ) );
		}
		else
		{
			const std::wstring wszMessage = MakeWideStringFromWordString( pszMessage );
			peerMessageRoom( peer, TitleRoom, NStr::ToAscii( wszMessage ).c_str(), NormalMessage );
		}
	}
}
void CGameSpyPeerChat::Segment()
{
	if ( pInGameChat )
		pInGameChat->Segment();

	if ( eInitState == EIS_DISCONNECTED )
		DisconnectFromChat( false );
	else if ( eInitState == EIS_CHANGED_NICK )
	{
		eInitState = EIS_INITIALIZING;		
		peerRetryWithNick( peer, szNick.c_str() );
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
IMultiplayerMessage* CGameSpyPeerChat::GetMessage()
{
	IMultiplayerMessage *pMessage = 0;
	if ( pInGameChat )
		pMessage = pInGameChat->GetMessage();

	if ( !pMessage )
		pMessage = messages.GetMessage();

	return pMessage;
}
void CGameSpyPeerChat::SetRoomKeys()
{
	std::string value;
	switch ( eMode )
	{
		case EUM_NONE: return;
		case EUM_AWAY: value = "AWAY"; break;
		case EUM_NOT_AWAY: value = "NOT_AWAY"; break;
		case EUM_IN_GS_CHAT: value = "IN_GS_CHAT"; break;
		case EUM_IN_SERVERS_LIST: value = "IN_SERVERS_LIST"; break;
		case EUM_IN_STAGING_ROOM: value = "IN_STAGING_ROOM"; break;
		case EUM_IN_GAME_PLAYING: value = "IN_GAME"; break;
		default: NI_ASSERT_T( false, NStr::Format( "Unknown chat mode (%d)", (int)eMode ) );
	}

	const char* pszKey = szRoomKeyName.c_str();
	const char* pszValue = value.c_str();
/*
	const char* pszValue = new char[value.size()+1];
	memcpy( pszValue, value.c_str(), value.size()+1 );
*/
/*
	const char* pszKey = new char[szKeyName.size()+1];
	memcpy( pszKey, szKeyName.c_str(), szKeyName.size()+1 );
*/
	peerSetRoomKeys( peer, TitleRoom, szNick.c_str(), 1, &pszKey, &pszValue );
}
bool CGameSpyPeerChat::AnalyzeRoomKeys( const char *pszKeyName, const char *pszKeyValue, EUserMode *pMode )
{
	if ( strcmp( pszKeyName, szRoomKeyName.c_str() ) == 0 )
	{
		if ( strcmp( pszKeyName, "AWAY" ) == 0 )
			*pMode = EUM_AWAY;
		else if ( strcmp( pszKeyValue, "NOT_AWAY" ) == 0 )
			*pMode = EUM_IN_GS_CHAT;
		else if ( strcmp( pszKeyValue, "IN_GS_CHAT" ) == 0 )
			*pMode = EUM_IN_GS_CHAT;
		else if ( strcmp( pszKeyValue, "IN_SERVERS_LIST" ) == 0 )
			*pMode = EUM_IN_SERVERS_LIST;
		else if ( strcmp( pszKeyValue, "IN_STAGING_ROOM" ) == 0 )
			*pMode = EUM_IN_STAGING_ROOM;
		else if ( strcmp( pszKeyValue, "IN_GAME" ) == 0 )
			*pMode = EUM_IN_GAME_PLAYING;
		else
			return false;
	}
	else
		return false;

	return true;
}
void CGameSpyPeerChat::AskForRoomKeys()
{
	const char* pszKeyName = szRoomKeyName.c_str();

	peerGetRoomKeys( peer, TitleRoom, "*", 1, &pszKeyName, getRoomKeysCallBack, this, PEERFalse );
}
void CGameSpyPeerChat::UserModeChanged( const EUserMode _eMode )
{
	if ( eInitState == EIS_INITIALIZED && eMode != _eMode )
	{
		if ( eMode == EUM_IN_GAME_PLAYING )
			peerStopGame( peer );
		if ( eMode == EUM_AWAY )
			peerSetAwayMode( peer, 0 );
	}
	
	eMode = _eMode;
	if ( eMode == EUM_NOT_AWAY )
		eMode = EUM_IN_GS_CHAT;

	if ( eInitState == EIS_INITIALIZED )
	{
		if ( IsInChatRoom() )
		{
			if ( eMode == EUM_AWAY )
				peerSetAwayMode( peer, "away" );
			else
			{
				peerEnumPlayers( peer, TitleRoom, enumPlayersCallback, this );
				AskForRoomKeys();
			}

			peerSetQuietMode( peer, PEERFalse );
		}
		else
		{
			peerSetQuietMode( peer, PEERTrue );

		  if ( eMode == EUM_IN_GAME_PLAYING )
				peerStartPlaying( peer );
			else if ( eMode == EUM_IN_STAGING_ROOM )
				peerSetReady( peer, PEERTrue );
		}

		SetRoomKeys();

		messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_MODE, szNick.c_str(), eMode ) );
	}
}
void CGameSpyPeerChat::AnalyzeMode( int nFlags, EUserMode *pMode )
{
	if ( nFlags & PEER_FLAG_PLAYING )
		*pMode = EUM_IN_GAME_PLAYING;
	else if ( nFlags & PEER_FLAG_AWAY )
		*pMode = EUM_AWAY;
	else if ( (nFlags & PEER_FLAG_STAGING) || !nFlags )
		*pMode = EUM_IN_GS_CHAT;
	else if ( nFlags & PEER_FLAG_READY )
		*pMode = EUM_IN_STAGING_ROOM;
	else
		*pMode = EUM_NONE;
}
void CGameSpyPeerChat::Disconnected( const char *pszReason )
{
	if ( !IsDisconnected() )
	{
		if ( IsInChatRoom() )
			messages.AddMessage( new CSimpleChatMessage( CSimpleChatMessage::EP_DISCONNECTED ) );
		eInitState = EIS_DISCONNECTED;
	}
}
void CGameSpyPeerChat::RoomMessage( RoomType roomType, const char *pszNick, const char *pszMessage, MessageType messageType )
{
	if ( IsInChatRoom() )
		messages.AddMessage( new CChatMessage( pszMessage, pszNick, false ) );
}
void CGameSpyPeerChat::RoomUTM( RoomType roomType, const char *pszNick, const char *pszCommand,	const char *pszParameters, PEERBool authenticated )
{
}
void CGameSpyPeerChat::RoomNameChanged( RoomType roomType )
{
}
void CGameSpyPeerChat::RoomModeChanged( RoomType roomType, CHATChannelMode *pMode )
{
}
void CGameSpyPeerChat::PlayerMessage( const char *pszNick, const char *pszMessage, MessageType messageType )
{
	messages.AddMessage( new CChatMessage( pszMessage, pszNick, true ) );
}
void CGameSpyPeerChat::PlayerUTM( const char *pszNick, const char *pszCommand, const char *pszParameters, PEERBool authenticated )
{
}
void CGameSpyPeerChat::ReadyChanged( const char *nick, PEERBool ready )
{
}
void CGameSpyPeerChat::GameStarted( unsigned int IP, const char *message )
{
}
void CGameSpyPeerChat::PlayerJoined( RoomType roomType, const char *pszNick )
{
	if ( IsInChatRoom() )	
	{
		int flags;
		peerGetPlayerFlags( peer, pszNick, TitleRoom, &flags );

		EUserMode eUserMode;
		AnalyzeMode( flags, &eUserMode );

		messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_JOINED, pszNick, eUserMode ) );
	}
}
void CGameSpyPeerChat::PlayerLeft( RoomType roomType, const char *pszNick, const char *pszReason )
{
	if ( IsInChatRoom() )
		messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_PARTED, pszNick, EUM_NONE ) );
}
void CGameSpyPeerChat::Kicked( RoomType roomType, const char *pszNick, const char *pszReason )
{
	if ( IsInChatRoom() )
		messages.AddMessage( new CSimpleChatMessage( CSimpleChatMessage::EP_KICKED ) );
	eInitState = EIS_DISCONNECTED;
}
void CGameSpyPeerChat::NewPlayerList( RoomType roomType )
{
	peerEnumPlayers( peer, TitleRoom, enumPlayersCallback, this );
}
void CGameSpyPeerChat::PlayerChangedNick( RoomType roomType, const char *pszOldNick, const char *pszNewNick )
{
	if ( IsInChatRoom() )
		messages.AddMessage( new CChatUserChangedNick( pszOldNick, pszNewNick ) );
}
void CGameSpyPeerChat::PlayerInfo( RoomType roomType, const char *pszNick, unsigned int IP, int profileID )
{
}
void CGameSpyPeerChat::PlayerFlagsChanged( RoomType roomType, const char *pszNick, int oldFlags, int newFlags )
{
	EUserMode eUserMode;
	AnalyzeMode( newFlags, &eUserMode );
	
	if ( eUserMode != EUM_NONE )
		messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_MODE, pszNick, eUserMode ) );
}
void CGameSpyPeerChat::Ping( const char *pszNick, int ping )
{
}
void CGameSpyPeerChat::CrossPing( const char *pszNick1, const char *pszNick2, int crossPing )
{
}
void CGameSpyPeerChat::GlobalKeyChanged( const char *pszNick, const char *pszKey, const char *pszValue )
{
}
void CGameSpyPeerChat::RoomKeyChanged( RoomType roomType, const char *pszNick, const char *pszKey, const char *pszValue )
{
	EUserMode eUserMode;
	if ( AnalyzeRoomKeys( pszKey, pszValue, &eUserMode ) )
		messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_MODE, pszNick, eUserMode ) );
}
void CGameSpyPeerChat::GOABasic( PEERBool playing, char *outbuf, int maxlen )
{
}
void CGameSpyPeerChat::GOAInfo( PEERBool playing, char *pOutbuf, int maxlen )
{
}
void CGameSpyPeerChat::GOARules( PEERBool playing, char *pOutbuf, int maxlen )
{
}
void CGameSpyPeerChat::GOAPlayers( PEERBool playing, char *pOutbuf, int maxlen )
{
}
void CGameSpyPeerChat::NickError( int nType, const char *pszNick )
{
	if ( nType == PEER_IN_USE )
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
	else
	{
		if ( IsInChatRoom() )			
			messages.AddMessage( new CSimpleChatMessage( CSimpleChatMessage::EP_FAILED_TO_CONNECT ) );
		eInitState = EIS_DISCONNECTED;
	}
}
void CGameSpyPeerChat::Connect( PEERBool success )
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
void CGameSpyPeerChat::EnumPlayers( PEERBool success, RoomType roomType, int index, const char *pszNick, int flags )
{
	if ( IsInChatRoom() && success == PEERTrue && index >= 0 )
	{
		EUserMode eUserMode;
		AnalyzeMode( flags, &eUserMode );

		if ( eUserMode == EUM_NONE )
			eUserMode = EUM_IN_GS_CHAT;
		messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_JOINED, pszNick, eUserMode ) );
	}
}
void CGameSpyPeerChat::JoinRoom( PEERBool success, PEERJoinResult result, RoomType roomType )
{
	if ( success == PEERTrue && result == PEERJoinSuccess )
	{
		if ( eInitState == EIS_INITIALIZED_ENTERING )
		{
			eInitState = EIS_INITIALIZED;
			UserModeChanged( eMode );
			
			if ( IsInChatRoom() )
			{
				peerEnumPlayers( peer, TitleRoom, enumPlayersCallback, this );
				AskForRoomKeys();
			}
		}
	}
	else
	{
		if ( IsInChatRoom() )
			messages.AddMessage( new CSimpleChatMessage( CSimpleChatMessage::EP_FAILED_TO_CONNECT ) );
		eInitState = EIS_DISCONNECTED;
	}
}
void CGameSpyPeerChat::GetRoomKeys( PEERBool success, const char *pszNick, int num, char **ppszKeys, char **ppszValues )
{
	if ( success == PEERTrue && pszNick )
	{
		for ( int i = 0; i < num; ++i )
		{
			EUserMode eUserMode;
			if ( AnalyzeRoomKeys( ppszKeys[i], ppszValues[i], &eUserMode ) )
				messages.AddMessage( new CChatUserChanged( CChatUserChanged::EUS_MODE, pszNick, eUserMode ) );
		}
	}
}
void CGameSpyPeerChat::disconnectedCallBack( PEER peer, const char *reason, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->Disconnected( reason );
}
void CGameSpyPeerChat::roomMessageCallBack( PEER peer, RoomType roomType, const char *nick, const char *message,	MessageType messageType, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->RoomMessage( roomType, nick, message, messageType );
}
void CGameSpyPeerChat::roomUTMCallBack( PEER peer, RoomType roomType, const char *nick, const char *command,	const char *parameters, PEERBool authenticated, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->RoomUTM( roomType, nick, command, parameters, authenticated );
}
void CGameSpyPeerChat::peerRoomNameChangedCallback( PEER peer, RoomType roomType, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->RoomNameChanged( roomType );
}
void CGameSpyPeerChat::roomModeChangedCallBack( PEER peer, RoomType roomType, CHATChannelMode *mode, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->RoomModeChanged( roomType, mode );
}
void CGameSpyPeerChat::playerMessageCallBack( PEER peer, const char *nick, const char *message, MessageType messageType, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->PlayerMessage( nick, message, messageType );
}
void CGameSpyPeerChat::playerUTMCallBack( PEER peer, const char *nick, const char *command, const char *parameters, PEERBool authenticated, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->PlayerUTM( nick, command, parameters, authenticated );
}
void CGameSpyPeerChat::readyChangedCallBack( PEER peer, const char *nick, PEERBool ready, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->ReadyChanged( nick, ready );
}
void CGameSpyPeerChat::gameStartedCallBack( PEER peer, unsigned int IP, const char *message, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->GameStarted( IP, message );
}
void CGameSpyPeerChat::playerJoinedCallBack( PEER peer, RoomType roomType, const char *nick, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->PlayerJoined( roomType, nick );
}
void CGameSpyPeerChat::playerLeftCallBack( PEER peer, RoomType roomType, const char *nick, const char *reason, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->PlayerLeft( roomType, nick, reason );
}
void CGameSpyPeerChat::kickedCallBack( PEER peer, RoomType roomType, const char *nick, const char *reason, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->Kicked( roomType, nick, reason );
}
void CGameSpyPeerChat::newPlayerListCallBack( PEER peer, RoomType roomType, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->NewPlayerList( roomType );
}
void CGameSpyPeerChat::playerChangedNickCallBack( PEER peer, RoomType roomType, const char *oldNick, const char *newNick, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->PlayerChangedNick( roomType, oldNick, newNick );
}
void CGameSpyPeerChat::playerInfoCallBack( PEER peer, RoomType roomType, const char *nick, unsigned int IP, int profileID, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->PlayerInfo( roomType, nick, IP, profileID );
}
void CGameSpyPeerChat::playerFlagsChangedCallBack( PEER peer, RoomType roomType, const char *nick, int oldFlags, int newFlags, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->PlayerFlagsChanged( roomType, nick, oldFlags, newFlags );
}
void CGameSpyPeerChat::pingCallBack( PEER peer, const char *nick, int ping, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->Ping( nick, ping );
}
void CGameSpyPeerChat::crossPingCallBack( PEER peer, const char *nick1, const char *nick2, int crossPing, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->CrossPing( nick1, nick2, crossPing );
}
void CGameSpyPeerChat::globalKeyChangedCallBack( PEER peer, const char *nick, const char *key, const char *value, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->GlobalKeyChanged( nick, key, value );
}
void CGameSpyPeerChat::roomKeyChangedCallBack( PEER peer, RoomType roomType, const char *nick, const char *key, const char *value, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->RoomKeyChanged( roomType, nick, key, value );
}
void CGameSpyPeerChat::GOABasicCallBack( PEER peer, PEERBool playing, char *outbuf, int maxlen, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->GOABasic( playing, outbuf, maxlen );
}
void CGameSpyPeerChat::GOAInfoCallBack( PEER peer, PEERBool playing, char *outbuf, int maxlen, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->GOAInfo( playing, outbuf, maxlen );
}
void CGameSpyPeerChat::GOARulesCallBack( PEER peer, PEERBool playing, char *outbuf, int maxlen, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->GOARules( playing, outbuf, maxlen );
}
void CGameSpyPeerChat::GOAPlayersCallBack( PEER peer, PEERBool playing, char *outbuf, int maxlen, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->GOAPlayers( playing, outbuf, maxlen );
}
void CGameSpyPeerChat::nickErrorCallback( PEER peer, int type, const char *nick, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->NickError( type, nick );
}
void CGameSpyPeerChat::connectCallback( PEER peer, PEERBool success, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->Connect( success );
}
void CGameSpyPeerChat::enumPlayersCallback( PEER peer, PEERBool success, RoomType roomType, int index, const char *nick, int flags, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->EnumPlayers( success, roomType, index, nick, flags );
}
void CGameSpyPeerChat::joinRoomCallback( PEER peer, PEERBool success, PEERJoinResult result, RoomType roomType, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->JoinRoom( success, result, roomType );
}
void CGameSpyPeerChat::getRoomKeysCallBack( PEER peer, PEERBool success, RoomType roomType, const char *nick, int num, char **keys, char **values, void *param )
{
	reinterpret_cast<CGameSpyPeerChat*>(param)->GetRoomKeys( success, nick, num, keys, values );
}
