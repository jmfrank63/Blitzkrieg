#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "InterfaceMPChat.h"
#include "WorldClient.h"
#include "MuliplayerToUIConsts.h"
#include "../UI/UIMessages.h"
#include "CommonId.h"

static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{ "inter_ok",					IMC_OK				},
	{ 0									,	0							}
};
enum EInterfaceElements
{
	E_PLAYER_RELATION_ICON			= 113,
	E_PLAYER_STATE_ICON					= 113,

	E_PLAYER_LIST								= 1000,
	E_CHAT_WINDOW								= 2001,
	E_CHAT_ENTRY_FEILD					= 2002,
	E_BUTTON_BACK								= 10001,
	E_BUTTON_INFO								= 10004,
	E_BUTTON_GAMES							= 10005,
	E_BUTTON_AWAY								= 10006,
	E_BUTTON_WHISPER						= 10003,
	E_DIALOG_PLAYER_INFO				= 3000,

	E_DIALOG_BUTTON_RELATION		= 3002,
	
	E_DIALOG_STATIC_AWAYCAUSE		= 3005,
	E_DIALOG_BUTTON_OK					= 3006,
	E_DIALOG_BUTTON_CANCEL			= 3007,
	E_DIALOG_CAPTION						= 20000,

	IMC_SHOW_PLAYER_INFO				= 8888,
	IMC_HIDE_PLAYER_INFO				= 8889,
};
bool CInterfaceMPChat::Init()
{
	CInterfaceMultiplayerScreen::Init();
	commandMsgs.Init( pInput, commands );

	return true;
}
void CInterfaceMPChat::StartInterface()
{
	CInterfaceMultiplayerScreen::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\MuptiplayerChat" );
	
	playerList.SetListControl( checked_cast< IUIListControl *> ( pUIScreen->GetChildByID( E_PLAYER_LIST ) ) );
	pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_DIALOG_PLAYER_INFO ) );
	
	chat.Init( checked_cast<IUIColorTextScroll*>( pUIScreen->GetChildByID( E_CHAT_WINDOW ) ),
		checked_cast<IUIEditBox*>( pUIScreen->GetChildByID( E_CHAT_ENTRY_FEILD) ),
		E_BUTTON_WHISPER,
		this );
	
	SFromUINotification notify( EUTMN_CHAT_MODE, 0 );
	pCommandManager->AddNotificationFromUI( notify );
	
	UpdateButtons();
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );

	if ( GetSingleton<IMPToUICommandManager>()->GetConnectionType() == EMCT_GAMESPY )
	{
		IUIElement * pGameSpyLogo = pUIScreen->GetChildByID( E_GAMESY_LOGO );
		if ( pGameSpyLogo )
			pGameSpyLogo->ShowWindow( UI_SW_SHOW );
	}
}
const WORD * CInterfaceMPChat::GetDestinationName()
{
	// szName is wide, and wchar_t is 32 bits off Windows, so convert instead of
	// reinterpreting; the member keeps the returned pointer alive.
	szDestinationName = NPlatform::WordStringFromWide( playerList.GetCurInfo()->szName );
	return NPlatform::WordStringData( szDestinationName );
}
bool CInterfaceMPChat::ProcessMPCommand( SToUICommand & cmd )
{
	switch ( cmd.eCommandID )
	{
	case EMTUC_UPDATE_CHAT_PLAYER_INFO:
		AddPlayer( checked_cast_ptr<SUIChatPlayerInfo*>( cmd.pCommandParams ) );
	
		return true;
	case EMTUC_PLAYER_LEFT_GAMESPY:
		playerList.Delete( checked_cast_ptr<SUIChatPlayerInfo*>( cmd.pCommandParams ) );

		return true;
	case EMTUC_CONNECTION_FAILED:
		FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );
		return false;
	}
	return true;
	
}
void CInterfaceMPChat::AddPlayer( SUIChatPlayerInfo * pInfo )
{
	IUIListRow *pRow = playerList.Add( pInfo );

	IUIDialog *pStateDialog = checked_cast<IUIDialog*>( pRow->GetElement( 0 ) );
	IUIElement *pStateIcon = checked_cast<IUIElement*>( pStateDialog->GetChildByID( E_PLAYER_STATE_ICON ) );
	if ( pInfo->eState != EPCS_ISNT_CHANGED )
		pStateIcon->SetState( pInfo->eState );
	std::wstring wszState = NStr::ToUnicode( NStr::Format( "%i", pInfo->eState ) );
	pStateDialog->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszState.c_str() ) ) );

	pStateIcon->EnableWindow( false );

	IUIDialog *pRelationDialog = checked_cast<IUIDialog*>( pRow->GetElement( 1 ) );
	IUIElement *pRelationIcon= checked_cast<IUIElement*>( pRelationDialog->GetChildByID( E_PLAYER_RELATION_ICON ) );
	pRelationIcon->SetState( pInfo->eRelation );
	pRelationIcon->EnableWindow( false );
	std::wstring wszRelation = NStr::ToUnicode( NStr::Format( "%i", pInfo->eRelation ) );
	pRelationDialog->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszRelation.c_str() ) ) );

	IUIStatic * pPlayerName = checked_cast<IUIStatic*>( pRow->GetElement( 2 ) );
	pPlayerName->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( pInfo->szName.c_str() ) ) );
}
bool CInterfaceMPChat::ProcessMessage( const SGameMessage &msg ) 
{ 
	if ( CInterfaceMultiplayerScreen::ProcessMessage( msg ) )
		return true;

	if ( WCC_MULTIPLAYER_TO_UI_UPDATE == msg.nEventID )
	{
		SToUICommand cmd;

		if ( pCommandManager->PeekCommandToUI( &cmd ) &&
				 cmd.eCommandID == EMTUC_CONNECTION_FAILED )
		{
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
			chat.AddMessageToChat( pChatMessage );
		}
		return true;
	}

	chat.ProcessMessage( msg );
	switch( msg.nEventID )
	{
	case UI_NOTIFY_EDIT_BOX_RETURN:
		{
			SendTextFromEditBox();
		}

		return true;
	case UI_NOTIFY_EDIT_BOX_ESCAPE:
		{
			IUIEditBox * pEdit = checked_cast<IUIEditBox*> ( pUIScreen->GetChildByID( E_CHAT_ENTRY_FEILD ) );
			pEdit->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( L"" ) ) );
		}

		return true;
	case E_BUTTON_BACK:
		if ( pDialog->IsVisible() )
		{
			GetSingleton<IInput>()->AddMessage( SGameMessage( IMC_HIDE_PLAYER_INFO ) );
		}
		else
		{		
			SFromUINotification notify( EUTMN_LEFT_GAME, 0 );
			pCommandManager->AddNotificationFromUI( notify );
			FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );
			
			notify.eNotifyID = EUTMN_LEAVE_CHAT_MOVE;
			notify.pCommandParams = 0;
			pCommandManager->AddNotificationFromUI( notify );
		}
		
		return true;
	case E_DIALOG_BUTTON_OK:
		OnPlayerInfoOk();
		return true;
		
	case E_BUTTON_INFO:
		ShowPlayerInfo( playerList.GetCurInfo() );

		return true;
	case E_BUTTON_AWAY:
		{
			bAwayPressed = true;
			timeLastAwayPressed = GetSingleton<IGameTimer>()->GetAbsTime();
			IUIButton * pButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_AWAY ) );
			SFromUINotification notify( EUTMN_AWAY, new SNotificationSimpleParam( pButton->GetState() ) );
			pCommandManager->AddNotificationFromUI( notify );
			pButton->EnableWindow( false );
		}

		return true;
	case UI_NOTIFY_SELECTION_CHANGED:
		UpdateButtons();

		return true;

	}
	
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	return false; 
}
void CInterfaceMPChat::OnPlayerInfoOk()
{
	IUIButton * pButtonRelation = checked_cast<IUIButton*>( pDialog->GetChildByID( E_DIALOG_BUTTON_RELATION ) );
	if ( pCurEdittedInfo )
	{
		CPtr<SUIRelationNotify> pParam = new SUIRelationNotify( NPlatform::WordStringData( NPlatform::WordStringFromWide( pCurEdittedInfo->szName.c_str() ) ), static_cast<EPlayerRelation>(pButtonRelation->GetState()) );

		SFromUINotification notify( EUTMN_PLAYER_RELATION_CHANGED, pParam );
		pCommandManager->AddNotificationFromUI( notify );
	}
}
void CInterfaceMPChat::ShowPlayerInfo( SUIChatPlayerInfo *pInfo )
{
	pCurEdittedInfo = pInfo;
	IUIButton * pButtonFriend = checked_cast<IUIButton*>( pDialog->GetChildByID( E_DIALOG_BUTTON_RELATION ) );
	pButtonFriend->SetState( pInfo->eRelation );
	
	if ( pInfo->eState != EPCS_ISNT_CHANGED )
	{
		IUIStatic *pAwayReason = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_DIALOG_STATIC_AWAYCAUSE ) );

		if ( pInfo->eState == EPCS_IN_CHAT  )
			pAwayReason->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( pInfo->szAwayReason.c_str() ) ) );
		else
			pAwayReason->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( L"" ) ) );
	}

	IUIStatic *pCaption = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_DIALOG_CAPTION ) );
	std::wstring szCaption = pInfo->szName;
	szCaption += L" ";
	IText *pText = GetSingleton<ITextManager>()->GetString( "Textes\\UI\\Intermission\\Multiplayer\\Chat\\playerinfo_caption" );
	szCaption += MakeWideStringFromWordString( pText->GetString() );
	pCaption->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( szCaption.c_str() ) ) );

	GetSingleton<IInput>()->AddMessage( SGameMessage( IMC_SHOW_PLAYER_INFO ) );
}
void CInterfaceMPChat::UpdateButtons()
{
	SUIChatPlayerInfo * pInfo = playerList.GetCurInfo();

	IUIButton * pButtonInfo = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_INFO ) );
	pButtonInfo->EnableWindow( pInfo != 0 );

	IUIButton * pButtonWhisper = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_WHISPER ) );
	pButtonWhisper->EnableWindow( pInfo != 0 );
}
void CInterfaceMPChat::SendTextFromEditBox( const bool bWhisper )
{
	IUIEditBox * pEdit = checked_cast<IUIEditBox*> ( pUIScreen->GetChildByID( E_CHAT_ENTRY_FEILD ) );
	std::wstring wszText = MakeWideStringFromWordString( pEdit->GetWindowText( 0 ) );

	CPtr<SChatMessage> pMessage = new SChatMessage( NPlatform::WordStringData( NPlatform::WordStringFromWide( wszText.c_str() ) ), bWhisper );
	pCommandManager->AddChatMessageFromUI( pMessage );
	pEdit->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( L"" ) ) );
}
bool CInterfaceMPChat::StepLocal( bool bAppActive )
{
	if ( bAwayPressed && GetSingleton<IGameTimer>()->GetAbsTime() > timeLastAwayPressed + 5000 )
	{
		IUIButton * pButton = checked_cast<IUIButton*>( pUIScreen->GetChildByID( E_BUTTON_AWAY ) );
		pButton->EnableWindow( true );
		bAwayPressed = false;
	}
	return CInterfaceMultiplayerScreen::StepLocal( bAppActive );
}