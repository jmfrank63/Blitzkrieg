#include "StdAfx.h"

#include "MultiplayerStartingGame.h"
#include "CommonId.h"
#include "WorldClient.h"
#include "..\UI\UIMessages.h"
#include "UIConsts.h"
#include "MapSettingsWrapper.h"
#include "UIMapINfo.h"
#include "MinimapCreation.h"

bool bServerconfiguration = false;
SMultiplayerGameSettings configuration ;	// server configuration (to display in MP settings )

static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_cancel"		, IMC_CANCEL		},
#if !defined(_FINALRELEASE) || defined(_DEVVERSION)
	{ "show_console"		, MC_SHOW_CONSOLE		},
#endif // !defined(_FINALRELEASE) || defined(_DEVVERSION)
	{ 0									,	0							}
};

enum EButtonsInMPStartingGame
{
	E_PLAYER_LIST				= 1000,
	E_CHAT_TEXT					= 2001,
	E_CHAT_ENTRY_FEILD	= 2002,

	E_BUTTONS_VERY_BEGIN = 10000,

	E_BUTTONS_BEGIN			= 10002,
	E_BUTTON_JOIN				= E_BUTTONS_BEGIN,
	E_BUTTON_KICK				= 10003,
	E_BUTTON_READY			= 10004,
	E_BUTTON_LAUNCH			= 10005,
	E_BUTTON_CHANGESIDE	= 10006,
	E_BUTTON_SETTINGS		= 10008,
	E_BUTTON_END				= E_BUTTON_SETTINGS,

	E_OPTIONS_LIST			= 1005,
	E_STATIC_GAME_TYPE	= 20002,

	E_PLAYER_READY_ICON						= 113,
	E_DOWNLOAD_COUNT							= 114,

};
int CICMultyplayerStartingGame::operator&( IStructureSaver & ss )
{
	CSaverAccessor saver = &ss;
	return 0;
}
int CICGameSpyClientConnect::operator&( IStructureSaver & ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &szIPAdress );
	saver.Add( 2, &bPasswordRequired );
	saver.Add( 3, &szPassword );

	return 0;
}

bool CInterfaceMPStartingGame::Init()
{
	CInterfaceMultiplayerScreen::Init();
	commandMsgs.Init( pInput, commands );

	return true;
}
void CInterfaceMPStartingGame::StartInterface()
{
	RemoveGlobalVar( "MultiplayerOptions.Changed" );
	bFirstConfiguration = true;
	bStarted = false;
	
	CInterfaceMultiplayerScreen::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\MuptiplayerStartingGame" );
	
	playerList.SetListControl ( checked_cast< IUIListControl *> ( pUIScreen->GetChildByID( E_PLAYER_LIST ) ) );
	
	chat.Init( checked_cast< IUIColorTextScroll*> ( pUIScreen->GetChildByID( E_CHAT_TEXT ) ),
		checked_cast< IUIEditBox*> ( pUIScreen->GetChildByID( E_CHAT_ENTRY_FEILD ) ),
		0, this );
	
	SFromUINotification notify( EUTMN_STAGING_ROOM_MODE, 0 );
	pCommandManager->AddNotificationFromUI( notify );
	
	for ( int i = E_BUTTONS_BEGIN; i <= E_BUTTON_END; ++i )
	{
		IUIElement * pButton = pUIScreen->GetChildByID( i );
		if ( pButton )
			pButton->EnableWindow( false );
	}

	bServerconfiguration = false;
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );

	if ( GetSingleton<IMPToUICommandManager>()->GetConnectionType() == EMCT_GAMESPY )
	{
		IUIElement * pGameSpyLogo = pUIScreen->GetChildByID( E_GAMESY_LOGO );
		if ( pGameSpyLogo )
			pGameSpyLogo->ShowWindow( UI_SW_SHOW );
	}
}
void CInterfaceMPStartingGame::AddMessageToChat( const SChatMessage *pChatMessage )
{
	chat.AddMessageToChat( pChatMessage );
}
void CInterfaceMPStartingGame::OnStart( const bool bForced )
{
	SNotificationSimpleParam *pParam = new SNotificationSimpleParam( bForced );
	pCommandManager->AddNotificationFromUI( SFromUINotification( EUTMN_START_GAME, pParam ) );
	bStarted = true;
}	
void CInterfaceMPStartingGame::OnGetFocus( bool bFocus )
{
	CInterfaceMultiplayerScreen::OnGetFocus( bFocus );
	if ( bFocus )
	{
		if (  GetGlobalVar( "ForcedStart.Confirm", 0 ) )
		{
			RemoveGlobalVar( "ForcedStart.Confirm" );
			OnStart( true );
		}
		
		if ( GetGlobalVar( "MultiplayerOptions.Changed", 0 ) )
		{
			NotifyOptionsChanged();
			RemoveGlobalVar( "MultiplayerOptions.Changed" );
		}
	}
}
void CInterfaceMPStartingGame::NotifyOptionsChanged()
{
	CPtr<CMapSettingsWrapper> pMamSettings = new CMapSettingsWrapper( true, OPTION_FLAG_BEFORE_MULTIPLAYER_START );
	SFromUINotification notify( EUTMN_SERVER_SETTINGS_CHANGED, new SServerNewSettings( pMamSettings->GetSettingsWOApply() ) );
	pCommandManager->AddNotificationFromUI( notify );
}
bool CInterfaceMPStartingGame::ProcessMPCommand( const SToUICommand &cmd )
{
	switch( cmd.eCommandID )
	{	
	case EMTUC_SERVER_SETTINGS_CHANGED:
		OnNewServerSettings( checked_cast_ptr<SServerNewSettings*>( cmd.pCommandParams )->settings, true );
		
		break;
	case EMTUC_START_WITH_SINGLE_PARTY:
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX, 
										NStr::Format( "%s;%s;1;ForcedStart.Confirm", "Textes\\UI\\MessageBox\\confirm_play_1party_caption",
														 "Textes\\UI\\MessageBox\\confirm_play_1party_message" ) );
		 
		break;
	case EMTUC_START_GAME:
		{
			IText *pText = GetSingleton<ITextManager>()->GetString( "Textes\\UI\\Intermission\\Multiplayer\\StagingRoom\\immessage-game-started" );
			chat.AddImportantText( pText->GetString() );
			for ( int i = E_BUTTONS_VERY_BEGIN; i <= E_BUTTON_END; ++i )
			{
				IUIElement *pElement = pUIScreen->GetChildByID( i );
				if ( pElement )
					pElement->EnableWindow( false );
			}
		}
		
		return true;
	case EMTUC_ALLOW_START_GAME:
		if ( pConfiguration && pConfiguration->bServer )
		{
			IUIElement *pButtonLaunch = pUIScreen->GetChildByID( E_BUTTON_LAUNCH );
			pButtonLaunch->EnableWindow( static_cast_ptr<const SNotificationSimpleParam*>(cmd.pCommandParams)->nParam ); 
		}

		return true;
	case EMTUC_CONNECTION_FAILED:
		if ( pCommandManager->GetConnectionType() == EMCT_INTERNET )
			FinishInterface( MISSION_COMMAND_ADDRESS_BOOK, 0 );
		else
			FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );


		return false;
	case EMTUC_UPDATE_PLAYER_INFO:
		AddOrUpdatePlayer( checked_cast_ptr<SUIPlayerInfo*>( cmd.pCommandParams ) );

		UpdateButtons();

		return true;
	case EMTUC_CONFIGURE_STAGING_ROOM:
		ConfigureStagingRoom( checked_cast_ptr<SUIStagingRoomConfigure*>( cmd.pCommandParams ) );
		
		UpdateButtons();


		return true;
	case EMTUC_PLAYER_LEFT:
		PlayerLeft( checked_cast_ptr<SUIPlayerInfo*>( cmd.pCommandParams ) );
		UpdateButtons();

		return true;
	case EMTUC_PLAYER_KICKED:
		PlayerKicked( checked_cast_ptr<SUIPlayerInfo*>( cmd.pCommandParams ) );
		UpdateButtons();

		return true;
	}
	return true;
}
void CInterfaceMPStartingGame::PlayerLeft( const SUIPlayerInfo * pInfo )
{
	CPtr<ITextManager> pTextM = GetSingleton<ITextManager>();
	IText * pText = pTextM->GetString( "immessage-multiplayer-playerleft" );
	if ( pText )
	{
		std::wstring wszText = pInfo->szName + NStr::ToUnicode(" ") + reinterpret_cast<const wchar_t*>( pText->GetString() );
		chat.AddImportantText( reinterpret_cast<const WORD*>( wszText.c_str() ) );
	}
	DeletePlayer( pInfo );
}
void CInterfaceMPStartingGame::PlayerKicked( const SUIPlayerInfo * pInfo )
{
	CPtr<ITextManager> pTextM = GetSingleton<ITextManager>();
	IText * pText = pTextM->GetString( "immessage-multiplayer-playerkicked" );
	if ( pText )
	{
		std::wstring wszText = pInfo->szName + NStr::ToUnicode(" ")+ reinterpret_cast<const wchar_t*>( pText->GetString() );
		chat.AddImportantText( reinterpret_cast<const WORD*>( wszText.c_str() ) );
	}
	DeletePlayer( pInfo );
}
void CInterfaceMPStartingGame::UpdateButtons()
{
	IUIButton * pKickButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_KICK ) );
	pKickButton->EnableWindow( pConfiguration &&
														pConfiguration->bServer && 
														playerList.GetCurInfo() != 0 &&
														playerList.GetCurInfo()->nID != 0 );
	
	IUIButton * pJoinButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_JOIN ) );
	pJoinButton->EnableWindow( pConfiguration && playerList.GetSize() <= pConfiguration->nPlayersMax );
}
void CInterfaceMPStartingGame::OnNewServerSettings( const SMultiplayerGameSettings &serverSettings, const bool bVisialNotify )
{
	configuration = serverSettings;

	if ( bVisialNotify )
	{
		CPtr<ITextManager> pTextM = GetSingleton<ITextManager>();
		IText * pText = pTextM->GetString( "Textes\\UI\\Intermission\\Multiplayer\\StagingRoom\\immessage-settings-changed" );
		if ( pText )
			chat.AddImportantText( pText->GetString() );
		
		if ( !pConfiguration->bServer )
		{
			IUIButton * pReadyButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_READY ) );
			pReadyButton->SetState( 0, true );
		}
	}
}
void CInterfaceMPStartingGame::ConfigureStagingRoom( SUIStagingRoomConfigure *pInfo )
{
	pConfiguration = pInfo;
	OnNewServerSettings( pInfo->serverSettings, false );
	bServerconfiguration = !pConfiguration->bServer;

	IUIButton * pReadyButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_READY ) );
	pReadyButton->ShowWindow( !pInfo->bServer );

	IUIButton * pLaunchButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_LAUNCH ) );
	pLaunchButton->ShowWindow( pInfo->bServer );

	if ( !pInfo->szMapLocation.empty() )
	{
		const std::string szFullPathName = "maps\\" + pInfo->szMapLocation;
		CMinimapCreation::Create1Minimap( szFullPathName, szFullPathName );
	
		IUIObjMap *pMap = checked_cast<IUIObjMap *> ( pUIScreen->GetChildByID( 100 ) );
		IGFXTexture *pTexture = GetSingleton<ITextureManager>()->GetTexture(  CUIConsts::CreateTexturePathFromMapPath( pInfo->szMapLocation.c_str() ).c_str() );
		NI_ASSERT_T( pTexture != 0, "Mission map texture is invalid" );
		pMap->SetMapTexture( pTexture );
		pMap->Init();
	}
		
	pCommandManager->AddNotificationFromUI( 
					SFromUINotification( EUTMN_PLAYER_READY, new SNotificationSimpleParam( pInfo->bServer ) ) 
																				);

	pMapSettingsWrapper = new CMapSettingsWrapper( false, OPTION_FLAG_BEFORE_MULTIPLAYER_START );
	
	pMapSettingsWrapper->Init( pInfo->serverSettings );
	pMapSettingsWrapper->Init( checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_OPTIONS_LIST ) ),
														 checked_cast<IUIStatic*>( pUIScreen->GetChildByID( E_STATIC_GAME_TYPE ) ) );
	
	SUIMapInfo mapInfo;
	if ( mapInfo.LoadMapByPath( pInfo->szMapLocation.c_str() ) )
		pMapSettingsWrapper->SetGameType( mapInfo.mapInfo.nType ); 

	if ( bFirstConfiguration )
	{
		bFirstConfiguration = false;
		ITextManager *pTM = GetSingleton<ITextManager>();
		IText *pText = 0;
		if ( pConfiguration->bServer )
			pText = pTM->GetString( "Textes\\UI\\Intermission\\Multiplayer\\StagingRoom\\immessage-server" );
		else
			pText = pTM->GetString( "Textes\\UI\\Intermission\\Multiplayer\\StagingRoom\\immessage-client" );

		if ( pText )
			chat.AddImportantText( pText->GetString() );
	}
}
void CInterfaceMPStartingGame::Done()
{
	CInterfaceMultiplayerScreen::Done();
	bServerconfiguration = false;
}
const WORD * CInterfaceMPStartingGame::GetDestinationName()
{
	SUIPlayerInfo * pInfo = playerList.GetCurInfo();
	if ( !pInfo ) return 0;
	return reinterpret_cast<const WORD*>( pInfo->szName.c_str() );
}
void CInterfaceMPStartingGame::DeletePlayer( const SUIPlayerInfo *pPlayerInfo )
{
	playerList.Delete( pPlayerInfo );
}
void CInterfaceMPStartingGame::AddOrUpdatePlayer( SUIPlayerInfo *pPlayerInfo )
{
	IUIListRow * pRow = playerList.Add( pPlayerInfo );

	IUIDialog * pReadyDialog = checked_cast<IUIDialog*>( pRow->GetElement( 0 ) );
	IUIStatic * pReadyIcon = checked_cast<IUIStatic*>( pReadyDialog->GetChildByID( E_PLAYER_READY_ICON ) );
	IUIStatic * pDownload = checked_cast<IUIStatic*>( pReadyDialog->GetChildByID( E_DOWNLOAD_COUNT ) );
	if ( pPlayerInfo->nDownloadCount < 100 )
	{
		pReadyIcon->ShowWindow( UI_SW_HIDE );
		pDownload->ShowWindow( UI_SW_SHOW );
		pDownload->SetWindowText( 0, NStr::ToUnicode( NStr::Format( "%i%%", pPlayerInfo->nDownloadCount ) ).c_str() );
	}
	else // ready/not ready
	{
		pDownload->ShowWindow( UI_SW_HIDE );
		pReadyIcon->ShowWindow( UI_SW_SHOW );

		pReadyIcon->SetState( pPlayerInfo->bReady ? 1 :  0 );
		pReadyIcon->EnableWindow( false );
	}

	IUIStatic * pPlayerName = checked_cast<IUIStatic*>( pRow->GetElement( 1 ) );
	pPlayerName->SetWindowText( 0, pPlayerInfo->szName.c_str() );

	std::wstring szLocalParty = reinterpret_cast<const wchar_t*>( CUIConsts::GetLocalPartyName( pPlayerInfo->szSide.c_str() ) );
	
	IUIStatic * pTeam = checked_cast<IUIStatic*>( pRow->GetElement( 2 ) );
	pTeam->SetWindowText( 0, szLocalParty.c_str() );

	IUIStatic * pPing = checked_cast<IUIStatic*>( pRow->GetElement( 3 ) );
	pPing->SetWindowText( 0, NStr::ToUnicode( NStr::Format( "%d", int(pPlayerInfo->fPing * 1000) ) ).c_str() );

	if ( pConfiguration && pConfiguration->nLocalPlayerID == pPlayerInfo->nID  )
	{
		IUIElement * pButton = pUIScreen->GetChildByID( E_BUTTON_READY );
		pButton->EnableWindow( pPlayerInfo->nDownloadCount == 100 );
		pButton = pUIScreen->GetChildByID( E_BUTTON_CHANGESIDE );
		pButton->EnableWindow( true );
		pButton = pUIScreen->GetChildByID( E_BUTTON_SETTINGS );
		pButton->EnableWindow( true );
	}
}
bool CInterfaceMPStartingGame::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceMultiplayerScreen::ProcessMessage( msg ) )
		return true;

	if ( WCC_MULTIPLAYER_TO_UI_UPDATE == msg.nEventID )
	{
		SToUICommand cmd;
		
		if ( pCommandManager->PeekCommandToUI( &cmd ) &&
				 (  cmd.eCommandID == EMTUC_WRONG_RESOURCES ||
						cmd.eCommandID == EMTUC_WRONG_MAP ||
						cmd.eCommandID == EMTUC_WRONG_PASSWORD ||
						cmd.eCommandID == EMTUC_GAME_IS_ALREADY_STARTED ||
						cmd.eCommandID == EMTUC_NO_MAP ||
						cmd.eCommandID == EMTUC_AIM_KICKED ||
						cmd.eCommandID == EMTUC_CONNECTION_FAILED
						) )
		{
			if ( pCommandManager->GetConnectionType() == EMCT_INTERNET )
				FinishInterface( MISSION_COMMAND_ADDRESS_BOOK, 0 );
			else
				FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );
			return true;
		}

		while ( pCommandManager->GetCommandToUI( &cmd ) )
		{
			if ( !ProcessMPCommand( cmd ) ) 
				return true;
		}
		SChatMessage * pChatMessage;
		while ( pChatMessage = pCommandManager->GetChatMessageToUI() )
		{
			AddMessageToChat( pChatMessage );
		}
		return true;
	}

	if ( !bStarted && chat.ProcessMessage( msg ) ) return true;
	switch( msg.nEventID )
	{
	case UI_NOTIFY_EDIT_BOX_ESCAPE:
	case IMC_CANCEL:
		{
			SFromUINotification notify( EUTMN_LEFT_GAME, 0 );
			pCommandManager->AddNotificationFromUI( notify );
			if ( pCommandManager->GetConnectionType() == EMCT_INTERNET )
				FinishInterface( MISSION_COMMAND_ADDRESS_BOOK, 0 );
			else
				FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );
		}
		return true;

	case E_BUTTON_READY:
		if ( pConfiguration && !pConfiguration->bServer )
		{
				IUIButton * pReadyButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_READY ) );
				pCommandManager->AddNotificationFromUI( 
					SFromUINotification( EUTMN_PLAYER_READY, 
															new SNotificationSimpleParam( pReadyButton->GetState() != 0 ) ) );

			return true;
		}
	case E_BUTTON_JOIN:
	case E_BUTTON_LAUNCH:
		if ( pConfiguration && pConfiguration->bServer )
		{
			IUIElement *pButtonLaunch = pUIScreen->GetChildByID( E_BUTTON_LAUNCH );
			if ( pButtonLaunch->IsWindowEnabled() )
			{
				OnStart( false );
			}
			return true;
		}
		
		break;
	case E_BUTTON_CHANGESIDE:
		pCommandManager->AddNotificationFromUI( SFromUINotification( EUTMN_SIDE, 0 ) );
		{
			if ( !pConfiguration->bServer )
			{
				IUIButton * pReadyButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_READY ) );
				pReadyButton->SetState( 0, true );
			}
		}
		return true; 
	case E_BUTTON_KICK:
		{
			SUIPlayerInfo * pInfo = playerList.GetCurInfo();

			pCommandManager->AddNotificationFromUI( 
					SFromUINotification( EUTMN_KICK_PLAYER, 
															new SNotificationSimpleParam( pInfo->nID ) ) );
		}
		return true;
	case E_BUTTON_SETTINGS:
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MP_MAP_SETTINGS, NStr::Format( "%d;1", !(pConfiguration && pConfiguration->bServer) ) );

		return true;
	case UI_NOTIFY_SELECTION_CHANGED:
		UpdateButtons();

		return true;
	}
	
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	return false; 
}
void CInterfaceMPStartingGame::SetParams( const char * pszParams )
{
	std::string szParams = pszParams;
	
	const int nType = NStr::ToInt( szParams ); // nType == 0 => server

	IUIElement * pButtonJoin = checked_cast<IUIElement*> ( pUIScreen->GetChildByID( E_BUTTON_JOIN ) );
	pButtonJoin->ShowWindow( nType == 0 ? UI_SW_HIDE : UI_SW_SHOW );

	IUIElement * pB1 = checked_cast<IUIElement*> ( pUIScreen->GetChildByID( E_BUTTON_READY ) );
	pB1->ShowWindow( nType == 1 ? UI_SW_HIDE : UI_SW_SHOW );
	
	pB1 = checked_cast<IUIElement*> ( pUIScreen->GetChildByID( E_BUTTON_KICK ) );
	pB1->ShowWindow( UI_SW_SHOW );
	
	pB1 = checked_cast<IUIElement*> ( pUIScreen->GetChildByID( E_BUTTON_LAUNCH ) );
	pB1->ShowWindow( nType == 1 ? UI_SW_HIDE : UI_SW_SHOW );
	
	pB1 = checked_cast<IUIElement*> ( pUIScreen->GetChildByID( E_BUTTON_CHANGESIDE ) );
	pB1->ShowWindow( UI_SW_SHOW );
	
	pB1 = checked_cast<IUIElement*> ( pUIScreen->GetChildByID( E_CHAT_ENTRY_FEILD ) );
	pB1->ShowWindow( UI_SW_SHOW );
}