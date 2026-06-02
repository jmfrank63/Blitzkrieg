
#include "stdafx.h"
#include "MultiplayerGamesList.h"
#include "WorldClient.h"
#include "MuliplayerToUIConsts.h"
#include "..\UI\UIMessages.h"
#include "CommonId.h"
#include "MainMenu.h"
#include "UIOptions.h"
#include "UIConsts.h"
#include "..\Main\ScenarioTracker.h"
#include "UIMapInfo.h"
#include "MPConnectionError.h"
extern SMultiplayerGameSettings configuration;	
extern bool bServerconfiguration;

static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{ "inter_ok"		,			IMC_OK				},
#if !defined(_FINALRELEASE) || defined(_DEVVERSION)
	{ "show_console"		, MC_SHOW_CONSOLE		},
#endif // !defined(_FINALRELEASE) || defined(_DEVVERSION)
	{ 0									,	0							}
};
enum EButtonsInMPGamesList
{
	E_CAPTION						= 20000,
	E_SERVERS_LIST			= 1000,
	E_BUTTON_CHAT				= 10003,
	E_BUTTON_JOIN				= 10004,
	E_BUTTON_CREATE			= 10005,
	E_BUTTON_INFO				= 10006,
	E_BUTTON_SETTINGS   = 10007,

	E_DIALOG_SETTINGS				= 3000,
	E_SETTINGS_DIALOG_BUTTON_OK	= 3006,
	IMC_SHOW_SETTINGS				= 8888,
	IMC_HIDE_SETTINGS				= 8889,
	E_SETTINGS_DIALOG_OPTIONS_LIST	= 1000,

	E_SERVER_ICON						= 113,

	E_SERVER_STATE_NORMAL		= 0,
	E_SERVER_STATE_PASSWORD = 1,
	E_SERVER_STATE_INGAME		= 2,
	E_SERVER_STATE_WRONGVERSION = 3,

	E_DIALOG_PASSWORD				= 3010,
	E_PASSWORD_ENTRY_FEILD	= 3011,
	E_PASSWORD_OK						= 10002,	
	E_DIALOG_WAIT_CONNECTION	= 3013,
};
CInterfaceMPGamesList::CInterfaceMPGamesList() 
: CInterfaceMultiplayerScreen ( "InterMission" ) 
{ 
}
void CInterfaceMPGamesList::Done()
{
	CInterfaceMultiplayerScreen::Done();
	bServerconfiguration = false;	
}
bool CInterfaceMPGamesList::Init()
{
	CInterfaceMultiplayerScreen::Init();

	commandMsgs.Init( pInput, commands );
	return true;
}
void CInterfaceMPGamesList::StartInterface()
{
	CInterfaceMultiplayerScreen::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\MuptiplayerGamesList" );
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	const EMultiplayerConnectionType eConnType = GetSingleton<IMPToUICommandManager>()->GetConnectionType();
	switch( eConnType )
	{
	case EMCT_LAN:
		SetTutorialNumber( 0 );
		break;
	case EMCT_INTERNET:
		SetTutorialNumber( 1 );
		break;
	case EMCT_GAMESPY:
		SetTutorialNumber( 2 );
		break;
	}
	
	
	IUIStatic *pCaption = checked_cast<IUIStatic *> ( pUIScreen->GetChildByID( E_CAPTION ) );
	ITextManager *pTextM = GetSingleton<ITextManager>();
	
	pCaption->SetWindowText( 0, CUIConsts::GetGamesListTitle( pCommandManager->GetConnectionType() ) );
	
	serversList.SetListControl( checked_cast< IUIListControl *> ( pUIScreen->GetChildByID( E_SERVERS_LIST ) ) );
	pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_DIALOG_SETTINGS ) );
	EnableButtonsByServerInfo();
	
	SFromUINotification notify( EUTMN_GAMES_LIST_MODE, 0 );
	pCommandManager->AddNotificationFromUI( notify );

	IUIButton * pButtonChat = checked_cast<IUIButton*> ( pUIScreen->GetChildByID( E_BUTTON_CHAT ) );
	switch( eConnType )
	{
	case EMCT_GAMESPY:
		{
			IUIElement * pGameSpyLogo = pUIScreen->GetChildByID( E_GAMESY_LOGO );
			if ( pGameSpyLogo )
				pGameSpyLogo->ShowWindow( UI_SW_SHOW );
			pButtonChat->ShowWindow(  UI_SW_SHOW );
		}

		break;
	case EMCT_INTERNET:
		pButtonChat->ShowWindow( UI_SW_HIDE );

		break;
	case EMCT_LAN:
		pButtonChat->ShowWindow( UI_SW_HIDE );

		break;
	}
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
}
void CInterfaceMPGamesList::EnableButtonsByServerInfo( const SUIServerInfo *pInfo )
{
	if ( !pInfo )
	{
		EnableButton ( static_cast<int>(E_BUTTON_INFO), false );
		EnableButton ( static_cast<int>(E_BUTTON_JOIN), false );
	}
	else
	{
		if ( pInfo->bCanJoin && pInfo->nPlayers < pInfo->nPlayersMax )
			EnableButton ( static_cast<int>(E_BUTTON_JOIN), true );
		else
			EnableButton ( static_cast<int>(E_BUTTON_JOIN), false );

		EnableButton ( static_cast<int>(E_BUTTON_INFO), true );
	}
}
void CInterfaceMPGamesList::EnableButton( const int nButtonID, bool bEnable )
{
	IUIElement * pEl = pUIScreen->GetChildByID( nButtonID );
	if ( pEl )
	{
		IUIButton * pButton = checked_cast<IUIButton*> ( pEl );
		pButton->EnableWindow( bEnable );
	}
}
bool CInterfaceMPGamesList::ProcessMessage( const SGameMessage &msg ) 
{ 
	if ( CInterfaceMultiplayerScreen::ProcessMessage( msg ) )
		return true;

	if ( WCC_MULTIPLAYER_TO_UI_UPDATE == msg.nEventID )
	{
		SToUICommand cmd;
		while ( pCommandManager->GetCommandToUI( &cmd ) && ProcessMPCommand( cmd ) )
		{
		}
		
		return true;
	}

	if ( pOptions && pOptions->ProcessMessage( msg ) ) return true;

	switch( msg.nEventID )
	{
	case IMC_OK:
		if ( pDialogEnterPassword )
		{
			IUIElement * pPasswordText = pDialogEnterPassword->GetChildByID( E_PASSWORD_ENTRY_FEILD );
			const std::string szPassword = NStr::ToAscii( std::wstring( reinterpret_cast<const wchar_t*>( pPasswordText->GetWindowText( 0 ) ) ) );
			pDialogEnterPassword->ShowWindow( UI_SW_HIDE_MODAL );
			pDialogEnterPassword->ShowWindow( UI_SW_HIDE );

			pDialogWaitForConnection = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_DIALOG_WAIT_CONNECTION ) );
			pDialogWaitForConnection->ShowWindow( UI_SW_SHOW_MODAL );
			
			pCommandManager->AddNotificationFromUI( 
					SFromUINotification( EUTMN_PASSWORD , new SPassword( szPassword ) ));	
		}
		else
		{
			TryJoin();
		}
		return true;

	case E_SETTINGS_DIALOG_BUTTON_OK:
		if ( pDialogError )
		{
			pDialogError->ShowWindow( UI_SW_HIDE );
		}
		else if ( pDialog )
		{
			/*	OnLocalSettingsOK();
			pOptions = 0;
			GetSingleton<IInput>()->AddMessage( SGameMessage( IMC_HIDE_SETTINGS ) );*/
		}
		return true;

	case IMC_CANCEL:
		if ( pDialogWaitForConnection )
		{
			SFromUINotification notify( EUTMN_CANCEL_CONNECT_TO_SERVER, 0 );
			pCommandManager->AddNotificationFromUI( notify );
			
			pDialogWaitForConnection->ShowWindow( UI_SW_HIDE_MODAL );
			pDialogWaitForConnection->ShowWindow( UI_SW_HIDE );
			pDialogWaitForConnection = 0;
			
			serversList.Clear();
		}
		else if ( pDialogEnterPassword && pDialogEnterPassword->IsVisible() )
		{
			pDialogEnterPassword->ShowWindow( UI_SW_HIDE );
			pCommandManager->AddNotificationFromUI( 
					SFromUINotification( EUTMN_PASSWORD, new SPassword ));				
		}
		else if ( pDialogError && pDialogError->IsVisible() )
			pDialogError->ShowWindow( UI_SW_HIDE );
		else if ( pDialog && pDialog->IsVisible() )
			GetSingleton<IInput>()->AddMessage( SGameMessage( IMC_HIDE_SETTINGS ) );
		else
		{
			FinishInterface( MISSION_COMMAND_MAIN_MENU, NStr::Format( "%d", CInterfaceMainMenu::E_MULTIPLAYER ) );
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d 0", MAIN_SP_TRANSCEIVER) );
		}

		return true;
	case E_BUTTON_CHAT:
		FinishInterface( MISSION_COMMAND_MULTYPLAYER_CHAT, 0 );
		
		return true;
	case E_BUTTON_JOIN:
		TryJoin();

		return true;
	case E_BUTTON_CREATE:
		FinishInterface( MISSION_COMMAND_MULTYPLAYER_CREATEGAME, 0 );
		
		return true;
	case E_BUTTON_INFO:
		{
			ShowServerInfo( serversList.GetCurInfo() );
		}

		return true;
	case E_BUTTON_SETTINGS:
		ShowLocalSettings();
		
		return true;
	case UI_NOTIFY_SELECTION_CHANGED:
		EnableButtonsByServerInfo( serversList.GetCurInfo() );
		return true;
		break;
	}
	
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	return false; 
}
void CInterfaceMPGamesList::TryJoin()
{
	IUIButton *pJoinButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_JOIN ) );
	if ( !pJoinButton || !pJoinButton->IsWindowEnabled() )
		return;

	const SUIServerInfo *pInfo = serversList.GetCurInfo();

	if ( pInfo )
	{

		if ( !pInfo->bSamePatch )
		{
			SToUICommand cmd( EMTUC_WRONG_GAMEEXE_VERSION,0 );
			pCommandManager->AddCommandToUI( cmd );
			return;
		}

		const std::string szModName = GetGlobalVar( "MOD.Name", "" );
		const std::string szModVersion = GetGlobalVar( "MOD.Version", "" );
		
		bool bNeedChangeMod = true;
		if ( szModName == pInfo->szModName && pInfo->szModName == "" ) // original game, connects to original game
		{
			bNeedChangeMod = false;
		}
		else
		{

			if ( szModName == pInfo->szModName && szModVersion == pInfo->szModVersion )
				bNeedChangeMod =  false;
		}

		if ( bNeedChangeMod )
		{
			const int nID = pInfo->GetID();
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_SWITCH_MODE_TO, 
				NStr::Format( "%s;%s;%d;%d;0",	// 0 - not silent switch
												pInfo->szModName.c_str(),
												pInfo->szModVersion.c_str(),
												MISSION_COMMAND_MULTIPLAYER_GAMESLIST,
												nID ) );
				
		}
		else
		{
			pCommandManager->AddNotificationFromUI( 
					SFromUINotification( EUTMN_JOIN, new SNotificationSimpleParam( pInfo->GetID() ) ) );

			pDialogWaitForConnection = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_DIALOG_WAIT_CONNECTION ) );
			pDialogWaitForConnection->ShowWindow( UI_SW_SHOW_MODAL );
		}
	}
}
void CInterfaceMPGamesList::Configure( const WORD wServerID )
{
	pCommandManager->AddNotificationFromUI( 
					SFromUINotification( EUTMN_JOIN, new SNotificationSimpleParam( wServerID ) ) );
}
void CInterfaceMPGamesList::OnLocalSettingsOK()
{
	pOptions->Apply();
}
void CInterfaceMPGamesList::ShowServerInfo( SUIServerInfo * pServerInfo )
{
	if ( pServerInfo )
	{
		::configuration = pServerInfo->settings;
		bServerconfiguration = true;
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MP_MAP_SETTINGS, NStr::Format( "%d;1", true ) );
	}
}
void CInterfaceMPGamesList::ShowLocalSettings()
{
	IUIDialog * pDialogOptions = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_DIALOG_SETTINGS ) );

	IUIListControl * pList = checked_cast<IUIListControl*>( pDialogOptions->GetChildByID( E_SETTINGS_DIALOG_OPTIONS_LIST ) );
	pOptions = new COptionsListWrapper(	OPTION_FLAG_MULTIPLAYER_SCREEN, pList, 100 );

	GetSingleton<IInput>()->AddMessage( SGameMessage( IMC_SHOW_SETTINGS ) );
}
void CInterfaceMPGamesList::AskPassword()
{
	if ( pDialogWaitForConnection )
	{
		pDialogWaitForConnection->ShowWindow( UI_SW_HIDE_MODAL );
		pDialogWaitForConnection->ShowWindow( UI_SW_HIDE );
		pDialogWaitForConnection = 0;
	}

	pDialogEnterPassword = checked_cast<IUIDialog *>( pUIScreen->GetChildByID( E_DIALOG_PASSWORD ) );
	IUIElement *pElement = pDialogEnterPassword->GetChildByID( E_PASSWORD_ENTRY_FEILD );
	pElement->SetWindowText( 0, L"" );
	pElement->SetFocus( true );
	pDialogEnterPassword->ShowWindow( UI_SW_SHOW_MODAL );
}
bool CInterfaceMPGamesList::ProcessMPCommand( const SToUICommand &cmd )
{
	switch( cmd.eCommandID )
	{
	case EMTUC_WRONG_RESOURCES:
	case EMTUC_WRONG_MAP:
	case EMTUC_NO_MAP:
	case EMTUC_CONNECTION_FAILED:
	case EMTUC_WRONG_PASSWORD:
	case EMTUC_GAME_IS_ALREADY_STARTED:
	case EMTUC_WRONG_GAMEEXE_VERSION:
	case EMTUC_AIM_KICKED:
		
		CMPConnectionError::DisplayError( cmd.eCommandID );
		if ( pDialogWaitForConnection )
		{
			pDialogWaitForConnection->ShowWindow( UI_SW_HIDE_MODAL );
			pDialogWaitForConnection->ShowWindow( UI_SW_HIDE );
			pDialogWaitForConnection= 0;
		}
		serversList.Clear();

		return true;
	
	case EMTUC_GIVE_PASSWORD:
		{
			AskPassword();
		}
		return true;
	case EMTUC_UPDATE_SERVER_INFO:
		UpdateServerInfo( static_cast_ptr<SUIServerInfo*>( cmd.pCommandParams ) );
		
		return true;
	case EMTUC_DELETE_SERVER:
		DeleteServer( static_cast_ptr<SUIServerInfo*>( cmd.pCommandParams ) );

		return true;
	case EMTUC_CREATE_STAGING_ROOM:
		{
			FinishInterface( MISSION_COMMAND_MULTIPLAYER_STARTINGGAME, "0"  );
		}
		return false;
	}
	return true;
}
void CInterfaceMPGamesList::DeleteServer( SUIServerInfo * pServerInfo )
{
	serversList.Delete( pServerInfo );
}
void CInterfaceMPGamesList::UpdateServerInfo( SUIServerInfo * pServerInfo )
{
	IUIListRow * pRow = serversList.Add( pServerInfo );

	IUIDialog * pPasswordDialog = checked_cast<IUIDialog*> ( pRow->GetElement( 0 ) );
	IUIStatic * pPasswordIcon = checked_cast<IUIStatic*>( pPasswordDialog->GetChildByID( E_SERVER_ICON ) );

	int nState = E_SERVER_STATE_NORMAL;
	if ( !pServerInfo->bSamePatch )			// wrong game version
		nState = E_SERVER_STATE_WRONGVERSION;
	else if ( !pServerInfo->bCanJoin ) // in game
		nState = E_SERVER_STATE_INGAME;
	else if ( pServerInfo->bPassword )
		nState = E_SERVER_STATE_PASSWORD;
	
	pPasswordDialog->SetWindowText( 0, NStr::ToUnicode( NStr::Format( "%i", nState ) ).c_str() );
	pPasswordIcon->SetState( nState );
	pPasswordIcon->EnableWindow( false );

	IUIStatic * pElementServerName = checked_cast<IUIStatic*>( pRow->GetElement( 1 ) );
	pElementServerName->SetWindowText( 0, pServerInfo->szName.c_str() );

	IUIStatic * pElementModName = checked_cast<IUIStatic*>( pRow->GetElement( 2 ) );
	pElementModName->SetWindowText( 0, NStr::ToUnicode( pServerInfo->szModName ).c_str() );
	
	IUIStatic * pElementModVersion = checked_cast<IUIStatic*>( pRow->GetElement( 3 ) );
	pElementModVersion->SetWindowText( 0, NStr::ToUnicode( pServerInfo->szModVersion ).c_str()  );

	IUIStatic *pMapName = checked_cast<IUIStatic*>( pRow->GetElement( 4 ) );
	pMapName->SetWindowText( 0, SUIMapInfo::GetVisualName( pServerInfo->szMapName ) );
	
	IUIStatic *pGameType = checked_cast<IUIStatic*>( pRow->GetElement( 5 ) );
	pGameType->SetWindowText( 0, CUIConsts::GetMapTypeString( pServerInfo->eGameType ) );

	IUIStatic *pPlayersNum = checked_cast<IUIStatic*>( pRow->GetElement( 6 ) );
	pPlayersNum->SetWindowText( 0, NStr::ToUnicode( NStr::Format( "%d/%d", pServerInfo->nPlayers, pServerInfo->nPlayersMax ) ).c_str() );

	IUIStatic * pPing = checked_cast<IUIStatic*>( pRow->GetElement( 7 ) );
	pPing->SetWindowText( 0, NStr::ToUnicode( NStr::Format( "%d", int(pServerInfo->fPing * 1000) ) ).c_str() );

	const SUIServerInfo * pInfo = serversList.GetCurInfo();
	if ( pInfo && pInfo->GetID() == pServerInfo->GetID() )
	{
		EnableButtonsByServerInfo( serversList.GetCurInfo() );
	}
	serversList.Resort();
}
