#include "stdafx.h"

#include "MultiplayerInternal.h"
#include "GameCreationInterfaces.h"
#include "ServersList.h"
#include "Messages.h"
#include "ChatMessages.h"
#include "MultiplayerConsts.h"
#include "GameSpyPeerChat.h"
#include "ScenarioTracker.h"

#include "..\GameTT\MultiplayerCommandManager.h"
#include "..\GameTT\MuliplayerToUIConsts.h"
#include "..\GameTT\WorldClient.h"

#include "..\Net\NetDriver.h"
BASIC_REGISTER_CLASS( IChat );
BASIC_REGISTER_CLASS( IMultiplayer::CCommand );
void CMultiplayer::InitServersList()
{
	eState = EMS_SERVERS_LIST;
	pGameCreation = 0;
	pGamePlaying = 0;
	pChat = 0;
	pServersList = 0;
	finishGameTime = 0;

	pServersList = CreateServersList();
}
void CMultiplayer::CreateServer( const SGameInfo &gameInfo, const SQuickLoadMapInfo &mapInfo )
{
	pGameCreation = pServersList->CreateServer( gameInfo, mapInfo, &pChat );

	pServersList = 0;
	eState = EMS_GAME_CREATION;

	if ( pChat )
		pChat->UserModeChanged( IChat::EUM_IN_STAGING_ROOM );
}
void CMultiplayer::JoinToServer( const WORD wServerID, bool bPasswordRequired, const std::string &szPassword )
{
	pGameCreation = pServersList->JoinToServerByID( wServerID, &pChat, bPasswordRequired, szPassword );
	pServersList = 0;
	eState = EMS_GAME_CREATION;

	if ( pChat )
		pChat->UserModeChanged( IChat::EUM_IN_STAGING_ROOM );
}
void CMultiplayer::StartGame()
{
	pGamePlaying = pGameCreation->CreateGamePlaying();
	pGameCreation = 0;
	finishGameTime = 0;
	eState = EMS_PLAYING;

	if ( pChat )
		pChat->UserModeChanged( IChat::EUM_IN_GAME_PLAYING );
}
void CMultiplayer::ServersListSegment()
{
	pServersList->Segment();
	
	CPtr<IMultiplayerMessage> pMessage;
	while ( (pMessage = pServersList->GetMessage()) )
		pMessage->SendToUI();
	
	IMPToUICommandManager *pUIManager = GetSingleton<IMPToUICommandManager>();
	SFromUINotification notificationFromUI;
	while ( pUIManager->GetNotificationFromUI( &notificationFromUI ) )
	{
		switch ( notificationFromUI.eNotifyID )
		{
			case EUTMN_CREATE:
				{
					CPtr<SNewMapInfo> pMapInfo = checked_cast_ptr<SNewMapInfo*>( notificationFromUI.pCommandParams );

					bool bPasswordRequired = pMapInfo->bPasswordRequired;
					const std::string szPassword = pMapInfo->szPassword;
					const std::string szServerName = GetGlobalVar( "Options.Multiplayer.ServerName", "Unknown" );

					SGameInfo gameInfo;
					gameInfo.Init
					(
						ToWordString( NStr::ToUnicode( szServerName ) ),
						pMapInfo->szMapName.c_str(),
						GetGlobalVar( "Options.Multiplayer.PlayerLimit", int( pMapInfo->mapInfo.diplomacies.size() - 1 ) ),
						0,
						pMapInfo->settings,
						static_cast<CMapInfo::GAME_TYPE>( pMapInfo->mapInfo.nType ),
						bPasswordRequired, szPassword,
						GetGlobalVar( "MOD.Name", "" ), GetGlobalVar( "MOD.Version", "" ), true
					);
					
					CreateServer( gameInfo, pMapInfo->mapInfo );
					pUIManager->AddCommandToUI( SToUICommand( EMTUC_CREATE_STAGING_ROOM, 0 ) );
				
					break;
				}
			case EUTMN_JOIN:
				{
					CPtr<SNotificationSimpleParam> pServerInfo = checked_cast_ptr<SNotificationSimpleParam*>( notificationFromUI.pCommandParams );
					if ( pServersList->CanJoinToServerByID( pServerInfo->nParam ) )
					{
						if ( pServersList->IsNeedPassword( pServerInfo->nParam ) )
						{
							nParam = pServerInfo->nParam;
							pUIManager->AddCommandToUI( SToUICommand( EMTUC_GIVE_PASSWORD, 0 ) );
						}
						else
							JoinToServer( pServerInfo->nParam, false, "" );
					}
					else
						pUIManager->AddCommandToUI( SToUICommand( EMTUC_CONNECTION_FAILED, 0 ) );
				}
				
				break;
			case EUTMN_PASSWORD:
				{
					CPtr<SPassword> pPassword = checked_cast_ptr<SPassword*>( notificationFromUI.pCommandParams );
					if ( !pPassword->bCancelFlag )
					{ 
						if ( pServersList->CanJoinToServerByID( nParam ) )
							JoinToServer( nParam, true, pPassword->szPassword );
						else
							pUIManager->AddCommandToUI( SToUICommand( EMTUC_CONNECTION_FAILED, 0 ) );
					}
				}

				break;
			case EUTMN_CHAT_MODE:
				if ( pChat )
					pChat->UserModeChanged( IChat::EUM_IN_GS_CHAT );
				bInGSChat = true;

				break;
			case EUTMN_LEAVE_CHAT_MOVE:
				if ( pChat )
					pChat->UserModeChanged( IChat::EUM_IN_SERVERS_LIST );
				bInGSChat = false;
				
				pServersList->Refresh();

				break;
			case EUTMN_AWAY:
				if ( pChat )
				{
					CPtr<SNotificationSimpleParam> pNotification = checked_cast_ptr<SNotificationSimpleParam*>( notificationFromUI.pCommandParams );
					if ( pNotification->nParam == 0 )
						pChat->UserModeChanged( IChat::EUM_NOT_AWAY );
					else
						pChat->UserModeChanged( IChat::EUM_AWAY );
				}

				break;
			case EUTMN_PLAYER_RELATION_CHANGED:
				{
					CPtr<SUIRelationNotify> pNotification = checked_cast_ptr<SUIRelationNotify*>( notificationFromUI.pCommandParams );
					GetSingleton<IUserProfile>()->SetChatRelation( pNotification->szName.c_str(), pNotification->eRelation );

					SUIChatPlayerInfo *pInfo = new SUIChatPlayerInfo( ToWordString( pNotification->szName ) );
					pInfo->eState = EPCS_ISNT_CHANGED;
					pInfo->eRelation = GetSingleton<IUserProfile>()->GetChatRelation( pNotification->szName.c_str() );
					GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_UPDATE_CHAT_PLAYER_INFO, pInfo ) );
				}

				break;
			case EUTMN_CONNECT_TO_SERVER:
				{
					CPtr<SNotificationStringParam> pParam = checked_cast_ptr<SNotificationStringParam*>( notificationFromUI.pCommandParams );
					
					CPtr<INetNodeAddress> pAddress = CreateObject<INetNodeAddress>( NET_NODE_ADDRESS );
					if ( pAddress->SetInetName( pParam->szParam.c_str(), SMultiplayerConsts::NET_PORT ) )
					{
						pGameCreation = pServersList->JoinToServerByAddress( pAddress, &pChat, SMultiplayerConsts::NET_PORT, false, "" );
						if ( pGameCreation )
						{
							pServersList = 0;
							eState = EMS_GAME_CREATION;
						}
						else
							pUIManager->AddCommandToUI( SToUICommand( EMTUC_CONNECTION_FAILED, 0 ) );
					}
					else
						pUIManager->AddCommandToUI( SToUICommand( EMTUC_CANT_RESOLVE_ADDRESS, 0 ) );
				}

				break;
			case EUTMN_CANCEL_CONNECT_TO_SERVER:
				{
					pGameCreation = 0;
					InitServersList();
				}
		}
	}
}
void CMultiplayer::GameCreationSegment()
{
	pGameCreation->Segment();

	while ( IMultiplayerMessage *pMessage = pGameCreation->GetMessage() )
	{
		pMessage->SendToUI();
		const EMultiplayerMessages eMessageID = pMessage->GetMessageID();
		switch ( eMessageID )
		{
			case CONNECTION_FAILED:
				pGameCreation = 0;

				{
					static int nConn = 0;
					NStr::DebugTrace( "connection failed %d\n", nConn++ );
				}
				
				GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "connection failed received" ), 0, true );

				InitServersList();

				return;
			case GAME_STARTED:
				StartGame();

				return;
			case WRONG_RESOURCES:
			case WRONG_MAP:
			case NO_MAP:
			case WRONG_PASSWORD:
			case GAME_IS_ALREADY_STARTED:
			case AIM_KICKED:
				pGameCreation = 0;
				InitServersList();

				GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format( "message %d received", eMessageID ), 0, true );
				
				return;
		}
	}

	IMPToUICommandManager *pUIManager = GetSingleton<IMPToUICommandManager>();
	SFromUINotification notificationFromUI;
	while ( pUIManager->GetNotificationFromUI( &notificationFromUI ) )
	{
		switch ( notificationFromUI.eNotifyID )
		{
			case EUTMN_LEFT_GAME:
				pGameCreation->LeftGame();
				pGameCreation = 0;
				InitServersList();

				return;
			case EUTMN_KICK_PLAYER:
				pGameCreation->KickPlayer( 
					checked_cast_ptr<SNotificationSimpleParam*>( notificationFromUI.pCommandParams )->nParam
				);

				break;
			case EUTMN_PLAYER_READY:
				{
					SPlayerInfo info;
					info.bReady = 
						checked_cast_ptr<SNotificationSimpleParam*>( notificationFromUI.pCommandParams )->nParam;
					pGameCreation->ChangePlayerSettings( info, IGameCreation::EPS_READY );
				}

				break;
			case EUTMN_SIDE:
				{
					SPlayerInfo info;
					pGameCreation->ChangePlayerSettings( info, IGameCreation::EPS_SIDE );
				}

				break;
			case EUTMN_START_GAME:
				{
					if ( pGameCreation->CanStartGame() )
					{
						const bool bStartWOOnePartyCheck = 
							checked_cast_ptr<SNotificationSimpleParam*>( notificationFromUI.pCommandParams )->nParam;
						const bool bAllPlayersInOneParty = pGameCreation->IsAllPlayersInOneParty();
						
						if ( bStartWOOnePartyCheck || !bAllPlayersInOneParty )
						{
							StartGame();
							pUIManager->AddCommandToUI( SToUICommand( EMTUC_START_GAME, 0 ) );
						}
						else if ( !bStartWOOnePartyCheck && bAllPlayersInOneParty )
							pUIManager->AddCommandToUI( SToUICommand( EMTUC_START_WITH_SINGLE_PARTY, 0 ) );
					}
				}

				break;
			case EUTMN_SERVER_SETTINGS_CHANGED:
				{
					SServerNewSettings *pSettings = checked_cast_ptr<SServerNewSettings*>( notificationFromUI.pCommandParams );
					pGameCreation->SetNewGameSettings( pSettings->settings );
				}

				break;
			case EUTMN_CANCEL_CONNECT_TO_SERVER:
				{
					pGameCreation = 0;
					InitServersList();
				}

				break;
			case EUTMN_SWITCH_MOD_OK:
				pGameCreation->ModChanged();

				break;
		}
	}
}
void CMultiplayer::PlayingSegment()
{
	if ( finishGameTime > 0 && finishGameTime < GetSingleton<IGameTimer>()->GetAbsTime() )
	{
		pServersList = 0;
		pGameCreation = 0;
		pGamePlaying = 0;
		GetChat()->DestroyInGameChat();

		finishGameTime = 0;
	}
	else
	{
		if ( pGamePlaying )
			pGamePlaying->Segment();

		IMPToUICommandManager *pUIManager = GetSingleton<IMPToUICommandManager>();
		SFromUINotification notificationFromUI;
		while ( pUIManager->GetNotificationFromUI( &notificationFromUI ) )
		{
			switch ( notificationFromUI.eNotifyID )
			{
				case EUTMN_GAMES_LIST_MODE:
				case EUTMN_ADDRESS_BOOK_MODE:
					pGamePlaying = 0;
					InitServersList();

					return;

			}
		}
	}
}
void CMultiplayer::SendChatMessageToConsole( IMultiplayerMessage *_pMessage )
{
	if ( _pMessage->GetMessageID() == E_CHAT_MESSAGE )
	{
		CChatMessage *pMessage = checked_cast<CChatMessage*>( _pMessage );

		std::wstring szConsoleBufferMessage = L"(" + pMessage->szPlayerName + L"): " + pMessage->szMessage;

		const std::string szSeasonName = GetGlobalVar( "World.Season" );

		const DWORD dwColor = pMessage->bWhisper ?
			GetGlobalVar( ("Scene.Colors." + szSeasonName + ".Text.ChatAllies.Color").c_str(), int(0xff00ff00) )
			:
			GetGlobalVar( ("Scene.Colors." + szSeasonName + ".Text.Chat.Color").c_str(), int(0xffd8bd3e) );

		IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
		pBuffer->Write( CONSOLE_STREAM_CHAT, szConsoleBufferMessage.c_str(), dwColor, false );
	}
}
void CMultiplayer::ProcessChat()
{
	if ( pChat )
	{
		pChat->Segment();

		while ( IMultiplayerMessage *pMessage = pChat->GetMessage() )
		{
			if ( pMessage->GetMessageID() == E_CHAT_MESSAGE )
			{
				const WORD *wpszSentPlayer = checked_cast<CChatMessage*>(pMessage)->GetPlayerNick();
				if ( GetSingleton<IUserProfile>()->GetChatRelation( reinterpret_cast<const wchar_t*>( wpszSentPlayer ) ) == EPR_IGNORED )
					continue;
			}
			
			if ( eState != EMS_PLAYING )
				pMessage->SendToUI();
			else
				SendChatMessageToConsole( pMessage );
		}

		IMPToUICommandManager *pUIManager = GetSingleton<IMPToUICommandManager>();
		while ( SChatMessage *pMessage = pUIManager->GetChatMessageFromUI() )
		{
			
			SPlayerInfo ourPlayer;
			switch ( eState )
			{
				case EMS_SERVERS_LIST: break;
				case EMS_GAME_CREATION: pGameCreation->GetOurPlayerInfo( &ourPlayer ); break;
				case EMS_PLAYING: pGamePlaying->GetOurPlayerInfo( &ourPlayer ); break;
			}

			if ( bInGSChat )
				pChat->SendMessage( ToWordString( pMessage->szMessageText ), ToWordString( pMessage->szPlayerName ), pMessage->bWhisper );
			else if ( pMessage->bWhisper )
			{
				SPlayerInfo info;
				bool bValidPlayer = false;
				switch ( eState )
				{
					case EMS_SERVERS_LIST:
						break;
					case EMS_GAME_CREATION:
							bValidPlayer = pGameCreation->GetPlayerInfo( ToWordString( pMessage->szPlayerName ), &info );
						pGameCreation->GetOurPlayerInfo( &ourPlayer );
						break;
					case EMS_PLAYING:
							bValidPlayer = pGamePlaying->GetPlayerInfo( ToWordString( pMessage->szPlayerName ), &info );
						pGamePlaying->GetOurPlayerInfo( &ourPlayer );
						break;
				}

				if ( bValidPlayer )
					pChat->SendWhisperMessage( ToWordString( pMessage->szMessageText ), info, ourPlayer );
			}
			else
				pChat->SendMessage( ToWordString( pMessage->szMessageText ), ourPlayer );
		}
	}
}
void CMultiplayer::Segment()
{
	if ( eState != EMS_NONE )
	{
		SFromUINotification notificationFromUI;
		IMPToUICommandManager *pManager = GetSingleton<IMPToUICommandManager>();

		ProcessChat();
		
		switch ( eState )
		{
			case EMS_SERVERS_LIST:
				ServersListSegment();
				break;
			case EMS_GAME_CREATION:
				GameCreationSegment();
				break;
			case EMS_PLAYING:
				PlayingSegment();
				break;
		}
	}
}
IMultiplayer::CCommand* CMultiplayer::GetCommand()
{
	return pGamePlaying ? pGamePlaying->GetCommand() : 0;
}
void CMultiplayer::SendClientCommands( IDataStream *pPacket )
{
	NI_ASSERT_T( pGamePlaying != 0, "Cant send client commands" );
	pGamePlaying->SendClientCommands( pPacket );	
}
void CMultiplayer::SendInGameChatMessage( const WORD *pszType, const WORD *pszMessage )
{
	if ( eState == EMS_PLAYING )
	{
		SPlayerInfo ourPlayer;
		pGamePlaying->GetOurPlayerInfo( &ourPlayer );

		std::wstring szMessageType = MakeWideStringFromWordString( pszType );

		if ( szMessageType == L"All" )
		{
			pChat->SendMessage( pszMessage, ourPlayer );
		}
		else if ( szMessageType == L"Allies" )
		{
			for ( int i = 0; i < pGamePlaying->GetNAllies(); ++i )
			{
				const SPlayerInfo &player = pGamePlaying->GetAlly( i );
					pChat->SendWhisperMessage( pszMessage, player, ourPlayer );
			}
		}
		else
		{
			SPlayerInfo toPlayer;
			if ( pGamePlaying->GetPlayerInfo( pszType, &toPlayer ) )
				pChat->SendWhisperMessage( pszMessage, toPlayer, ourPlayer );
		}
	}
}
void CMultiplayer::TogglePause()
{
	if ( eState == EMS_PLAYING )
		pGamePlaying->TogglePause();
}
void CMultiplayer::GameSpeed( const int nChange )
{
	if ( eState == EMS_PLAYING )
		pGamePlaying->GameSpeed( nChange );
}
void CMultiplayer::DropPlayer( const int nLogicID )
{
	if ( eState == EMS_PLAYING )
		pGamePlaying->DropPlayer( nLogicID );
}
int CMultiplayer::GetNumberOfPlayers() const
{
	if ( eState == EMS_PLAYING )
		return pGamePlaying->GetNumberOfPlayers();
	else
		return 0;
}
void CMultiplayer::CommandTimeOut( const bool bSet )
{
	if ( eState == EMS_PLAYING )
		pGamePlaying->CommandTimeOut( bSet );
}
void CMultiplayer::SetGameCreation( IGameCreation* _pGameCreation )
{
	pGameCreation = _pGameCreation;
}
void CMultiplayer::SetServersList( IServersList *_pServersList )
{
	pServersList = _pServersList;
}
void CMultiplayer::SetState( const EMultiplayerStates _eState )
{
	eState = _eState;
}
void CMultiplayer::SetChat( IChat *_pChat )
{
	pChat = _pChat;
}
IChat* CMultiplayer::GetChat()
{
	return pChat;
}
void CMultiplayer::SendAliveMessage()
{
	if ( pGamePlaying )
		pGamePlaying->SendAliveMessage();
}
void CMultiplayer::FinishGame()
{
	if ( pGamePlaying )
	{
		finishGameTime = GetSingleton<IGameTimer>()->GetAbsTime() + 5000;
		pGamePlaying->FinishGame();
	}
}
INetDriver* CMultiplayer::GetInGameNetDriver() const
{
	switch ( eState )
	{
		case EMS_SERVERS_LIST:
			return pServersList->GetInGameNetDriver();
			break;
		case EMS_GAME_CREATION:
			return pGameCreation->GetInGameNetDriver();
			break;
		case EMS_PLAYING:
			return pGamePlaying->GetInGameNetDriver();
			break;
		default:
			return 0;
			break;
	}
}
IServersList* CLanMultiplayer::CreateServersList()
{
	CLanServersList *pServersList = new CLanServersList();
	pServersList->Init();

	return pServersList;
}
CGameSpyMultiplayer::CGameSpyMultiplayer()
{
}
void CGameSpyMultiplayer::Init()
{
	SetChat( new CGameSpyPeerChat() );

	std::wstring wszUserName = MakeWideStringFromWordString( GetGlobalWVar( "Options.Multiplayer.GameSpyPlayerName", reinterpret_cast<const WORD*>( L"Noname" ) ) );
	if ( wszUserName == L"Noname" )
		wszUserName = NStr::ToUnicode( GetGlobalVar( "Options.Multiplayer.PlayerName", "Noname" ) );
	
	GetChat()->InitGSChat( ToWordString( wszUserName ) );
	GetChat()->UserModeChanged( IChat::EUM_IN_SERVERS_LIST );
	SetState( EMS_NONE );
}
IServersList* CGameSpyMultiplayer::CreateServersList()
{
	CGameSpyServersList *pServersList = new CGameSpyServersList();
	pServersList->Init();

	return pServersList;
}
bool CGameSpyMultiplayer::InitJoinToServer( const char *pszIPAddress, const int nHostPort, bool bPasswordRequired, const char* pszPassword )
{
	CGameSpyServersList *pServersList = new CGameSpyServersList();

	INetNodeAddress *pAddress = CreateObject<INetNodeAddress>( NET_NODE_ADDRESS );
	const int nPort = nHostPort == -1 ? SMultiplayerConsts::NET_PORT : nHostPort;
	if ( pAddress->SetInetName( pszIPAddress, nPort ) )
	{
		CPtr<IChat> pChat;
		CPtr<IGameCreation> pGameCreation = pServersList->JoinToServerByAddress( pAddress, &pChat, nPort, bPasswordRequired, pszPassword );
		if ( pGameCreation )
		{
			SetGameCreation( pGameCreation );
			SetChat( pChat );

			pServersList = 0;
			SetState( EMS_GAME_CREATION );

			return true;
		}
		else
			return false;
	}
	else
		return false;
}
void CGameSpyMultiplayer::InitServersList()
{
	SetState( EMS_SERVERS_LIST );	
	SetServersList( CreateServersList() );
	GetChat()->DestroyInGameChat();
	GetChat()->UserModeChanged( IChat::EUM_IN_SERVERS_LIST );
}
IServersList* CInternetMultiplayer::CreateServersList()
{
	CInternetServersList *pServersList = new CInternetServersList();
	pServersList->Init();

	return pServersList;
}
