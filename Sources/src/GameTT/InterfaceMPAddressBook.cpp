#include "StdAfx.h"

#include "InterfaceMPAddressBook.h"
#include "InterfaceStartDialog.h"
#include "CommonId.h"
#include "../UI/UIMessages.h"
#include "MainMenu.h"
#include "WorldClient.h"
#include "MPConnectionError.h"
const std::string szFileName = "Data\\AddressBook.xml";
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{	"inter_ok", 				IMC_OK				},
#if !defined(_FINALRELEASE) || defined(_DEVVERSION)
	{ "show_console"		, MC_SHOW_CONSOLE		},
#endif // !defined(_FINALRELEASE) || defined(_DEVVERSION)
	{ 0									,	0							}
};
enum 
{
	E_BUTTON_OK														= 10002,
	E_BUTTON_BACK													= 10001,
	E_BUTTON_DELETE_ADDRESS								= 10003,
	E_BUTTON_ADD_ADDRESS									= 10004,
	E_BUTTON_CREATE												= 10005,

	E_LIST																= 1000,
	E_DIALOG_ASK_ADDRESS									= 3010,
	E_ADDRESS_EDIT_BOX										= 3011,
	E_DIALOG_WAIT_CONNECTION							= 3013,
};
void CInterfaceMPAddressBook::InitServersList()
{
	bChanged = false;
	Serialize( true );
	for ( CServersList::iterator it = szServers.begin(); it != szServers.end(); ++it )
	{
		AddServerInternal( *it );
	}
}
void CInterfaceMPAddressBook::Serialize( const bool bRead )
{
	CPtr<IDataStream> pStream = OpenFileStream(szFileName.c_str(), bRead ? STREAM_ACCESS_READ : STREAM_ACCESS_WRITE);
	if ( pStream )
	{
		CPtr<IDataTree> pTree = CreateDataTreeSaver( pStream, bRead ? IDataTree::READ : IDataTree::WRITE );
		CTreeAccessor saver = pTree;
		saver.Add( "Servers", &szServers );
	}
}
void CInterfaceMPAddressBook::SaveServersList()
{
	bChanged = false;
	Serialize( false );
}
void CInterfaceMPAddressBook::DeleteServer()
{
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) );
	const int nSelItem = pList->GetSelectionItem();
	if ( -1 != nSelItem )
	{
		IUIListRow *pRow = pList->GetItem( nSelItem );
		IUIElement *pEl = pRow->GetElement( 0 );
		const std::string szAddress = NStr::ToAscii( MakeWideStringFromWordString( pEl->GetWindowText(0) ) );
		pList->RemoveItem( pList->GetSelectionItem() );

		bChanged = true;
		szServers.erase( szAddress );
		CheckEnableButtons();
		pList->InitialUpdate();
	}
}
void CInterfaceMPAddressBook::AddServerInternal( const std::string &szServer )
{
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) );
	pList->AddItem();
	IUIListRow *pRow = pList->GetItem( pList->GetNumberOfItems() -1 );
	const std::wstring wszServer = NStr::ToUnicode( szServer );
	pRow->GetElement( 0 )->SetWindowText( 0, reinterpret_cast<const WORD*>( wszServer.c_str() ) );
	pList->InitialUpdate();
}
void CInterfaceMPAddressBook::AddServer( const std::string &szServer )
{
	CServersList::iterator it = szServers.find( szServer );
	if ( !szServer.empty() && it == szServers.end() )
	{
		bChanged = true;
		szServers.insert( szServer );
		AddServerInternal( szServer );
		IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) );
		pList->SetSelectionItem( pList->GetNumberOfItems() - 1 );
		CheckEnableButtons();
	}
}
void CInterfaceMPAddressBook::Done()
{
	CInterfaceMultiplayerScreen::Done();
	SaveServersList();
}
std::string CInterfaceMPAddressBook::GetServer()
{
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) );
	const int nSelItem = pList->GetSelectionItem();
	if ( -1 != nSelItem )
	{
		IUIElement *pEl = pList->GetItem( nSelItem )->GetElement( 0 );
		if ( pEl )
			return NStr::ToAscii( MakeWideStringFromWordString( pEl->GetWindowText(0) ) );		
	}

	return "";
}
void CInterfaceMPAddressBook::OnGetFocus( bool bFocus )
{
	CInterfaceScreenBase::OnGetFocus( bFocus );

	if ( bFocus && GetGlobalVar( "EraseAddres.OK", 0 ) )
	{
		RemoveGlobalVar( "EraseAddres.OK" );
		DeleteServer();
	}
}
bool CInterfaceMPAddressBook::ProcessMPCommand( const SToUICommand &cmd )
{
	if ( pDialogWaitForConnection && CMPConnectionError::DisplayError( cmd.eCommandID ) )
	{
		pDialogWaitForConnection->ShowWindow( UI_SW_HIDE_MODAL );
		pDialogWaitForConnection->ShowWindow( UI_SW_HIDE );
		pDialogWaitForConnection= 0;
	}
	return true;
}
bool CInterfaceMPAddressBook::ProcessMessage( const SGameMessage &msg )
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

	switch( msg.nEventID )
	{
	case E_BUTTON_CREATE:
		FinishInterface( MISSION_COMMAND_MULTYPLAYER_CREATEGAME, 0 );
		
		break;
	case 7777:
	case E_BUTTON_OK:
		if ( pDialogAskAddress )
		{
			IUIElement *pText = pDialogAskAddress->GetChildByID( E_ADDRESS_EDIT_BOX );
			const std::string szServer = NStr::ToAscii( MakeWideStringFromWordString( pText->GetWindowText( 0 ) ) );
			pDialogAskAddress->ShowWindow( UI_SW_HIDE_MODAL );
			pDialogAskAddress->ShowWindow( UI_SW_HIDE );
			AddServer( szServer );
			pDialogAskAddress = 0;
		}
		else if ( UI_NOTIFY_EDIT_BOX_RETURN != msg.nEventID )
		{
			SFromUINotification notify( EUTMN_CONNECT_TO_SERVER, new SNotificationStringParam( GetServer() ) );
			GetSingleton<IMPToUICommandManager>()->AddNotificationFromUI( notify );
			pDialogWaitForConnection = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_DIALOG_WAIT_CONNECTION ) );
			pDialogWaitForConnection->ShowWindow( UI_SW_SHOW_MODAL );
		}
		
		break;
	case E_BUTTON_BACK:
		if ( pDialogWaitForConnection )
		{
			SFromUINotification notify( EUTMN_CANCEL_CONNECT_TO_SERVER, 0 );
			pCommandManager->AddNotificationFromUI( notify );
			
			pDialogWaitForConnection->ShowWindow( UI_SW_HIDE_MODAL );
			pDialogWaitForConnection->ShowWindow( UI_SW_HIDE );
			pDialogWaitForConnection= 0;
		}
		else if ( pDialogAskAddress )
		{
			pDialogAskAddress->ShowWindow( UI_SW_HIDE_MODAL );
			pDialogAskAddress->ShowWindow( UI_SW_HIDE );
			pDialogAskAddress = 0;
		}
		else
		{
			FinishInterface( MISSION_COMMAND_MAIN_MENU, NStr::Format( "%d", CInterfaceMainMenu::E_MULTIPLAYER ) );
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d 0", MAIN_SP_TRANSCEIVER) );
		}

		break;
	case E_BUTTON_DELETE_ADDRESS:
		{
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX, 
									NStr::Format( "%s;%s;1;EraseAddres.OK", "Textes\\UI\\Intermission\\Multiplayer\\AddressBook\\caption_erase_address",
													 "Textes\\UI\\Intermission\\Multiplayer\\AddressBook\\message_erase_address" ) );

			return true;
		}

		break;
	case E_BUTTON_ADD_ADDRESS:
		{
			pDialogAskAddress = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_DIALOG_ASK_ADDRESS ) );
			IUIElement *pText = pDialogAskAddress->GetChildByID( E_ADDRESS_EDIT_BOX );
			pText->SetWindowText( 0, reinterpret_cast<const WORD*>( L"" ) );
			pText->SetFocus( true );
			pDialogAskAddress->ShowWindow( UI_SW_SHOW_MODAL );
		}

		break;
	case MC_SET_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );

		break;
	case MC_CANCEL_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
		break;

	case UI_NOTIFY_SELECTION_CHANGED:
		CheckEnableButtons();

		break;
	default:
		return false;
	}
	
	return true;
}
void CInterfaceMPAddressBook::CheckEnableButtons()
{
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) );
	const int nIndex = pList->GetSelectionItem();
	IUIElement *pEl = pUIScreen->GetChildByID( E_BUTTON_DELETE_ADDRESS );
	if ( pEl )
		pEl->EnableWindow( nIndex != -1 );
	pEl = pUIScreen->GetChildByID( E_BUTTON_OK );
	if ( pEl )
		pEl->EnableWindow( nIndex != -1 );
}
bool CInterfaceMPAddressBook::Init()
{
	CInterfaceMultiplayerScreen::Init();
	msgs.Init( pInput, commands );

	return true;
}
void CInterfaceMPAddressBook::StartInterface()
{
	CInterfaceScreenBase::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\AddressBook" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_LIST ) );
	pList->InitialUpdate();
	InitServersList();
	CheckEnableButtons();
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
	SFromUINotification notify( EUTMN_ADDRESS_BOOK_MODE, 0 );
	pCommandManager->AddNotificationFromUI( notify );

}
