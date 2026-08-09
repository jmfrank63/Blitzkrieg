#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "LoadMission.h"
#include "MultiplayerCommandManager.h"

#include "../Main/iMainCommands.h"
#include "SaveLoadCommon.h"
#include "CommonId.h"
#include "../Main/ScenarioTracker.h"
static const NInput::SRegisterCommandEntry loadmissionCommands[] = 
{
	{ "cancel_load"	,	IMC_CANCEL					},
	{ "load_mission", IMC_OK							},
	{ "key_up",				MESSAGE_KEY_UP			},
	{ "key_down",			MESSAGE_KEY_DOWN		},
	{ "key_left",			MESSAGE_KEY_LEFT		},
	{ "key_right",		MESSAGE_KEY_RIGHT		},
	{ 0							,	0										}
};
void CICLoadMission::PostCreate( IMainLoop *pML, CInterfaceLoadMission *pILM )
{
	pML->PushInterface( pILM );
}
CInterfaceLoadMission::~CInterfaceLoadMission()
{
}
bool CInterfaceLoadMission::Init()
{
	NStr::SetCodePage( GetACP() );
	CInterfaceScreenBase::Init();
	SetBindSection( "loadmission" );
	loadmissionMsgs.Init( pInput, loadmissionCommands );
	
	return true;
}
void CInterfaceLoadMission::StartInterface()
{
	CInterfaceScreenBase::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\LoadMission" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		//should be List Control
	IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
	if ( !pList )
		return;			//не нашелся list control
	
	szSaves.clear();
	std::string szMask = "*.sav";
	// Was derived by stripping two components off the data storage name with
	// rfind('\\'). There is no backslash in a path here, so rfind returned npos,
	// substr(0, npos) handed back the whole string, and the dialog looked for
	// saves inside Data instead of beside the executable -- an empty list.
	// GetBaseDir is where quicksave and the intermission dialogs already write.
	std::string szBaseDir = GetSingleton<IMainLoop>()->GetBaseDir();
	const std::string szModname = GetSingleton<IUserProfile>()->GetMOD();
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
		pElement = pUIScreen->GetChildByID( 2000 );
		const std::wstring wszEdit = NStr::ToUnicode( szEdit );
		pElement->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszEdit.c_str() ) ) );
		pList->SetSelectionItem( 0 );
	}
	
	pList->InitialUpdate();
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceLoadMission::ProcessMessage( const SGameMessage &msg )
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
				pElement = pUIScreen->GetChildByID( 2000 );
				const std::wstring wszEdit = NStr::ToUnicode( szEdit );
				pElement->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszEdit.c_str() ) ) );
			}
			return true;
			
		case IMC_CANCEL:
			{
				IMainLoop *pML = GetSingleton<IMainLoop>();
				CloseInterface();
				pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_GAME_UNPAUSE_MENU) );	//уберем паузу
				return true;
			}

		case IMC_OK:
			{
				IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		//should be List Control
				IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
				if ( !pList )
					return true;			//не нашелся list control
				int nSave = pList->GetSelectionItem();
				if ( nSave == -1 )
					return true;

				std::string szEdit = szSaves[nSave];
				IMainLoop *pML = GetSingleton<IMainLoop>();
				CloseInterface();
				pML->Command( MAIN_COMMAND_LOAD, szEdit.c_str() );
				pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_GAME_UNPAUSE_MENU) );	//уберем паузу
				return true;
			}
	}
	return false;
}
bool CInterfaceLoadMission::StepLocal( bool bAppActive )
{
	const CVec2 vPos = pCursor->GetPos();
	CInterfaceScreenBase::OnCursorMove( vPos );
	pUIScreen->Update( pTimer->GetAbsTime() );
	return true;
}
void CInterfaceLoadMission::DrawAdd()
{
}
