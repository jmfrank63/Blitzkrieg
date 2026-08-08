#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "SaveMission.h"

#include "../Main/iMainCommands.h"
#include "MultiplayerCommandManager.h"
#include "SaveLoadCommon.h"
#include "CommonId.h"
#include "../UI/UIMessages.h"
#include "../Main/ScenarioTracker.h"
static const NInput::SRegisterCommandEntry savemissionCommands[] = 
{
	{ "cancel_save"	,	IMC_CANCEL					},
	{ "save_mission", IMC_OK							},
	{ "key_up",				MESSAGE_KEY_UP			},
	{ "key_down",			MESSAGE_KEY_DOWN		},
	{ "key_left",			MESSAGE_KEY_LEFT		},
	{ "key_right",		MESSAGE_KEY_RIGHT		},
	{ 0							,	0										}
};
CInterfaceSaveMission::~CInterfaceSaveMission()
{
}
void CInterfaceSaveMission::OnGetFocus( bool bFocus )
{
	CInterfaceScreenBase::OnGetFocus( bFocus );
	if ( bFocus ) 
	{
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
		if ( GetGlobalVar( "SaveGame.OkOverrite", 0 ) )
		{
			RemoveGlobalVar( "SaveGame.OkOverrite" );
			OnSave();
		}
		else
		{
			pUIScreen->ShowWindow( UI_SW_SHOW );
		}
	}
}
void CInterfaceSaveMission::Configure( const int _nMode )
{
}
bool CInterfaceSaveMission::Init()
{
	CInterfaceScreenBase::Init();
	pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
	SetBindSection( "savemission" );
	savemissionMsgs.Init( pInput, savemissionCommands );

	return true;
}
void CInterfaceSaveMission::StartInterface()
{
	CInterfaceScreenBase::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\savemission" );
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		// should be List Control
	if ( !pElement )
		return;			// не нашелся list control
	IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
	
	szSaves.clear();
	std::string szMask = "*.sav";
	std::string szBaseDir = std::string( GetSingleton<IDataStorage>()->GetName() );
	szBaseDir = szBaseDir.substr( 0, szBaseDir.rfind('\\') );
	szBaseDir = szBaseDir.substr( 0, szBaseDir.rfind('\\') );
	const std::string szModname = GetSingleton<IUserProfile>()->GetMOD();
	szBaseDir += "\\";
	if ( !szModname.empty() )
	{
		szBaseDir += "mods\\";
		szBaseDir += szModname;
	}
	szBaseDir += "saves\\";
	
	std::vector<SLoadFileDesc> files;
	NFile::EnumerateFiles( szBaseDir.c_str(), szMask.c_str(), CGetFiles2Load(files, szBaseDir), true );
	std::sort( files.begin(), files.end(), SLoadFileLessFunctional() );
	const DWORD dwTextColor = GetGlobalVar( "Scene.Colors.Summer.Text.Default.Color", int(0xffd8bd3e) );
	for ( int i=0; i<files.size(); i++ )
	{
		pList->AddItem();
		IUIListRow *pRow = pList->GetItem( i );
		IUIStatic *pStatic = checked_cast<IUIStatic*> ( pRow->GetElement( 0 ) );
		
		szSaves.push_back( files[i].szFileName );
		std::wstring wszTemp;
		NStr::ToUnicode( &wszTemp, files[i].szFileName.substr( 0, files[i].szFileName.rfind( '.' ) ) );
		pStatic->SetWindowText( pStatic->GetState(), NPlatform::WordStringData( NPlatform::WordStringFromWide( wszTemp.c_str() ) ) );
		pStatic->SetTextColor( dwTextColor );
	}
	if ( szSaves.size() > 0 )
	{
		std::string szEdit = szSaves[0];
		szEdit = szEdit.substr( 0, szEdit.rfind( '.' ) );
		IUIEditBox *pEdit = checked_cast<IUIEditBox*>( pUIScreen->GetChildByID( 2000 ) );
		const std::wstring wszEdit = NStr::ToUnicode( szEdit );
		pEdit->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszEdit.c_str() ) ) );
		pEdit->SetCursor( szEdit.size() );
		pEdit->SetSel( 0, -1 );
		pList->SetSelectionItem( 0 );
	}
	pList->InitialUpdate();
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
	IUIElement *pEdit = pUIScreen->GetChildByID( 2000 );		//should be Edit Control
	pEdit->ShowWindow( UI_SW_SHOW );
	pEdit->SetFocus( true );
	pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
}
bool CInterfaceSaveMission::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case IMC_SELECTION_CHANGED:
			{
				IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		//should be List Control
				IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
				if ( !pList )
					return true;			//не нашелся list control
				int nSave = pList->GetSelectionItem();
				if ( nSave == -1 )
					return true;
				
				std::string szEdit = szSaves[nSave];
				szEdit = szEdit.substr( 0, szEdit.rfind( '.' ) );
				IUIEditBox *pEdit = checked_cast<IUIEditBox*>( pUIScreen->GetChildByID( 2000 ) );
				const std::wstring wszEditUnicode = NStr::ToUnicode( szEdit );
				pEdit->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszEditUnicode.c_str() ) ) );
				pEdit->SetCursor( wszEditUnicode.length() );
				pEdit->SetSel( 0, -1 );
				pEdit->ShowWindow( UI_SW_SHOW );
				pEdit->SetFocus( true );
				
				return true;
			}
			
		case IMC_CANCEL:
			{
				pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
				IMainLoop *pML = GetSingleton<IMainLoop>();
				CloseInterface();
				pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_GAME_UNPAUSE_MENU) );	//уберем паузу
				return true;
			}

		case IMC_OK:
			{
				IUIEditBox *pEdit = checked_cast<IUIEditBox *>( pUIScreen->GetChildByID( 2000 ) );		//should be EditBox
				NI_ASSERT_T( pEdit != 0, "Can't find editbox control with ID 2000" );
				const std::wstring wszEdit = MakeWideStringFromWordString( pEdit->GetWindowText( pEdit->GetState() ) );
				std::string szEdit = NStr::ToAscii( wszEdit );
				NStr::TrimBoth( szEdit );
				if ( szEdit.size() == 0 )
				{
					IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		//should be List Control
					IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
					if ( !pList )
						return true;			//не нашелся list control
					int nSave = pList->GetSelectionItem();
					if ( nSave == -1 )
						return true;
					szEdit = szSaves[nSave];
				}
				
				szProspecitveSave = szEdit;
				std::string szFileName = GetSingleton<IMainLoop>()->GetBaseDir();
				szFileName += "saves\\";
				szFileName += szEdit + ".sav";

				if ( NFile::IsFileExist( szFileName.c_str() ) )
				{
					pUIScreen->ShowWindow( UI_SW_HIDE );
					
					GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX, 
															NStr::Format( "%s;%s;1;SaveGame.OkOverrite", "Textes\\UI\\MessageBox\\overwrite_save_caption",
														 "Textes\\UI\\MessageBox\\overwrite_save_message" ) );

				}
				else
					OnSave();
				return true;
			}
			break;

		case MC_SET_TEXT_MODE:
			pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
			break;
		case MC_CANCEL_TEXT_MODE:
			pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
			break;
		case UI_NOTIFY_EDIT_BOX_TEXT_CHANGED:
			{
				
			}
			return true;
	}
	return false;
}
void CInterfaceSaveMission::OnSave()
{
	IMainLoop *pML = GetSingleton<IMainLoop>();
	CloseInterface();
	pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
	const std::string szSave = szProspecitveSave + ".sav";
	pML->Command( MAIN_COMMAND_SAVE, szSave.c_str() );
	pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_GAME_UNPAUSE_MENU) );	//уберем паузу
}
bool CInterfaceSaveMission::StepLocal( bool bAppActive )
{
	const CVec2 vPos = pCursor->GetPos();
	CInterfaceScreenBase::OnCursorMove( vPos );
	pUIScreen->Update( pTimer->GetAbsTime() );
	return true;
}
void CInterfaceSaveMission::DrawAdd()
{
}
