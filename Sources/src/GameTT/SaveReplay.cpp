#include "StdAfx.h"
#include "../StreamIO/ProfilePaths.h"

#include "SaveReplay.h"

#include "CommonId.h"
#include "MultiplayerCommandManager.h"
#include "../Main/CommandsHistoryInterface.h"
#include "../UI/UIMessages.h"
#include "../Main/ScenarioTracker.h"
enum EControls
{
	E_REPLAY_EDIT_BOX											= 2000,
	E_OK_BUTTON														= 10002,
};
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
CInterfaceSaveReplay::~CInterfaceSaveReplay()
{
}
bool CInterfaceSaveReplay::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );
	
	return true;
}
void CInterfaceSaveReplay::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\Popup\\SaveReplay" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	IUIDialog *pDialog = checked_cast<IUIDialog *>( pUIScreen->GetChildByID( 1000 ) );
	IUIElement *pSaveReplayEditBox = pDialog->GetChildByID( 2000 );
	pSaveReplayEditBox->SetFocus( true );

	pScene->AddUIScreen( pUIScreen );
	CheckEnableOk();
}
void CInterfaceSaveReplay::CheckEnableOk() const
{
	IUIElement * pEdit = pUIScreen->GetChildByID( E_REPLAY_EDIT_BOX );
	const std::wstring szName = MakeWideStringFromWordString( pEdit->GetWindowText( 0 ) );
	IUIElement *pButtonOK = pUIScreen->GetChildByID( E_OK_BUTTON );
	pButtonOK->EnableWindow( !szName.empty() );
}
void CInterfaceSaveReplay::OnGetFocus( bool bFocus )
{
	CInterfaceInterMission::OnGetFocus( bFocus );
	/*if ( bFocus && GetGlobalVar( "SaveGame.OkOverrite", 0 ) )
	{
		RemoveGlobalVar( "SaveGame.OkOverrite" );
		Save();
	}*/
}
void CInterfaceSaveReplay::OnSave()
{
	GetSingleton<ICommandsHistory>()->Save( szSaveReplayFile.c_str() );

	SetGlobalVar( "ReplaySaved", 1 );
	CloseInterface();
}
bool CInterfaceSaveReplay::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case UI_NOTIFY_EDIT_BOX_TEXT_CHANGED:
			{
				CheckEnableOk();
				
			}
			return true;
		case IMC_CANCEL:
			SetGlobalVar( "ReplaySaved", 0 );
			CloseInterface();
			return true;
			
		case IMC_OK:
			{
				IMainLoop *pML = GetSingleton<IMainLoop>();
				szSaveReplayFile = std::string( pML->GetBaseDir() ) + NProfile::Segment();
				const std::string szModname = GetSingleton<IUserProfile>()->GetMOD();
				if ( !szModname.empty() )
				{
					szSaveReplayFile += "mods\\";
					szSaveReplayFile += szModname;
				}
				szSaveReplayFile += "replays\\";
				IUIDialog *pDialog = checked_cast<IUIDialog *>( pUIScreen->GetChildByID( 1000 ) );
				IUIElement *pSaveReplayEditBox = pDialog->GetChildByID( 2000 );
				szSaveReplayFile += NStr::ToAscii( MakeWideStringFromWordString( pSaveReplayEditBox->GetWindowText( 0 ) ) );
				szSaveReplayFile += ".rpl";

				/*if ( NFile::IsFileExist( szFileName.c_str() ) )
				{
					GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX, 
															NStr::Format( "%s;%s;1;SaveGame.OkOverrite", "Textes\\UI\\MessageBox\\overwrite_save_caption_im",
														 "Textes\\UI\\MessageBox\\overwrite_save_message_im" ) );
					return true;
				}
				else*/
					OnSave();
				return true;
			}
	}
	
	return false;
}
