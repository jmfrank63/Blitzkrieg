#include "stdafx.h"

#include "ChatMessages.h"
#include "ScenarioTracker.h"

#include "..\GameTT\MultiplayerCommandManager.h"
#include "..\GameTT\MuliplayerToUIConsts.h"
CChatMessage::CChatMessage( const char *pszMessage, const char *pszPlayerName, bool _bWhisper ) : bWhisper( _bWhisper )
{
	NStr::SetCodePage( GetACP() );
	szMessage = NStr::ToUnicode( pszMessage );
	szPlayerName = NStr::ToUnicode( pszPlayerName );
}
void CChatMessage::SendToUI()
{
	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();
	pCommandManager->AddChatMessageToUI( new SChatMessage( reinterpret_cast<const WORD*>( szMessage.c_str() ), reinterpret_cast<const WORD*>( szPlayerName.c_str() ), bWhisper ) );
}
void CSimpleChatMessage::SendToUI()
{
	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();	
	switch ( eParam )
	{
		case EP_FAILED_TO_CONNECT:
			pCommandManager->AddCommandToUI( SToUICommand( EMTUC_CONNECTION_FAILED, new SNotificationSimpleParam() ) );
			break;
		case EP_DISCONNECTED:
			pCommandManager->AddCommandToUI( SToUICommand( EMTUC_CONNECTION_FAILED, new SNotificationSimpleParam() ) );
			break;
		case EP_KICKED:
			pCommandManager->AddCommandToUI( SToUICommand( EMTUC_CONNECTION_FAILED, new SNotificationSimpleParam() ) );
			break;
	}
}
CChatUserChanged::CChatUserChanged( const EUserState &_eState, const char *pszUserNick, const IChat::EUserMode &_eMode )
: eState( _eState ), eMode( _eMode )
{
	NStr::SetCodePage( GetACP() );
	wszUserNick = NStr::ToUnicode( pszUserNick );
}
void CChatUserChanged::SendToUI()
{
	EPlayerChatState eUIState;
	switch ( eMode )
	{
		case IChat::EUM_NONE:						 eUIState = EPCS_NONE; break;
		case IChat::EUM_AWAY:						 eUIState = EPCS_AWAY; break;
		case IChat::EUM_NOT_AWAY:				 eUIState = EPCS_IN_CHAT; break;
		case IChat::EUM_IN_GS_CHAT:			 eUIState = EPCS_IN_CHAT; break;
		case IChat::EUM_IN_SERVERS_LIST: eUIState = EPCS_IN_SERVERSLIST; break;
		case IChat::EUM_IN_STAGING_ROOM: eUIState = EPCS_IN_STAGINGROOM; break;
		case IChat::EUM_IN_GAME_PLAYING: eUIState = EPCS_IN_GAME;
	}

	SUIChatPlayerInfo *pInfo = new SUIChatPlayerInfo( reinterpret_cast<const WORD*>( wszUserNick.c_str() ) );
	pInfo->eState = eUIState;
	pInfo->eRelation = GetSingleton<IUserProfile>()->GetChatRelation( wszUserNick.c_str() );
	
	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();
	switch ( eState )
	{
		case EPCS_NONE:
			pCommandManager->AddCommandToUI( SToUICommand( EMTUC_UPDATE_CHAT_PLAYER_INFO, pInfo ) );
			break;
		case EUS_JOINED:
			pCommandManager->AddCommandToUI( SToUICommand( EMTUC_UPDATE_CHAT_PLAYER_INFO, pInfo ) );
			break;
		case EUS_PARTED:
			pCommandManager->AddCommandToUI( SToUICommand( EMTUC_PLAYER_LEFT_GAMESPY, pInfo ) );
			break;
		case EUS_MODE:
			pCommandManager->AddCommandToUI( SToUICommand( EMTUC_UPDATE_CHAT_PLAYER_INFO, pInfo )	);
			break;
	}
}
CChatUserChangedNick::CChatUserChangedNick( const char *pszOldNick, const char *pszNewNick )
{
	NStr::SetCodePage( GetACP() );
	wszOldNick = NStr::ToUnicode( pszOldNick );
	wszNewNick = NStr::ToUnicode( pszNewNick );
}
void CChatUserChangedNick::SendToUI()
{
	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();
	pCommandManager->AddCommandToUI( 
		SToUICommand( EMTUC_PLAYER_CHANGED_NICK, new SUIChatPlayerChangedNick( reinterpret_cast<const WORD*>( wszOldNick.c_str() ), reinterpret_cast<const WORD*>( wszNewNick.c_str() ) ) )
	);
}
