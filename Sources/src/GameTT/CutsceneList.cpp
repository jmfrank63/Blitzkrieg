#include "StdAfx.h"

#include "..\Main\Transceiver.h"
#include "..\Main\GameStats.h"
#include "..\Main\AILogicCommand.h"
#include "..\StreamIO\RandomGen.h"
#include "CommonId.h"
#include "CutScenesHelper.h"
#include "CutsceneList.h"

static const NInput::SRegisterCommandEntry commonCommands[] = 
{
	{ "cancel_load"	,	IMC_CANCEL					},
	{ "load_mission", IMC_OK							},
	{ "key_up",				MESSAGE_KEY_UP			},
	{ "key_down",			MESSAGE_KEY_DOWN		},
	{ "key_left",			MESSAGE_KEY_LEFT		},
	{ "key_right",		MESSAGE_KEY_RIGHT		},
	{ 0							,	0										}
};
void CCutsceneList::PostCreate( IMainLoop *pML, CInterfaceCutsceneList *pILM )
{
	pML->PushInterface( pILM );
}
CInterfaceCutsceneList::~CInterfaceCutsceneList()
{
}
bool CInterfaceCutsceneList::Init()
{
	NStr::SetCodePage( GetACP() );
	CInterfaceScreenBase::Init();
	SetBindSection( "loadmission" );
	commandMsgs.Init( pInput, commonCommands );
	
	return true;
}
void CInterfaceCutsceneList::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\lists\\IMCutsceneList" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( 1000 ) );
	NI_ASSERT( pList != 0 );
	
	ITextManager *pTextMan = GetSingleton<ITextManager>();
	std::list<std::string> cutscenes;
	NCutScenes::GetCutScenesList( cutscenes );
	int nSceneIndex = 0;
	for ( std::list<std::string>::const_iterator it = cutscenes.begin(); it != cutscenes.end(); ++it, ++nSceneIndex )
	{
		pList->AddItem();
		IUIListRow *pRow = pList->GetItem( nSceneIndex );
		pRow->SetUserData( nSceneIndex );
		
		IUIContainer *pContainer = checked_cast<IUIContainer*> ( pRow->GetElement( 0 ) );
		std::string szVideoName = *it;
		cutscenesList.push_back( szVideoName );

		CPtr<IText> pText = pTextMan->GetDialog( szVideoName.c_str() );
		if ( CPtr<IText> pText = pTextMan->GetDialog(szVideoName.c_str()) ) 
			pContainer->SetWindowText( 0, pText->GetString() );
		else
		{
			const int nPos = szVideoName.rfind('\\');
			if ( nPos != std::string::npos )
				szVideoName = szVideoName.substr( nPos + 1 );
			const std::wstring wszVideoName = NStr::ToUnicode( szVideoName );
			pContainer->SetWindowText( 0, reinterpret_cast<const WORD*>( wszVideoName.c_str() ) );
		}
		
		IUIElement *pElement = pContainer->GetChildByID( 1 );
		NI_ASSERT_T( pElement != 0, "Invalid list control name dialog, it should contain icon" );
		pElement->SetState( 1 );			//файл
	}
	
	int nSelItem = GetGlobalVar( "LastCutscene", -1 );
	if ( nSelItem >= 0 )
		pList->SetSelectionItem( nSelItem );

	pList->InitialUpdate();
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceCutsceneList::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			CloseInterface();
			RemoveGlobalVar( "LastCutscene" );
			return true;
			
		case IMC_OK:
			if ( IUIElement *pElement = pUIScreen->GetChildByID( 1000 ) ) 
			{
				IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
				if ( !pList )
					return true;			// не нашелся list control
				int nSelItem = pList->GetSelectionItem();			// индекс в списке
				if ( nSelItem == -1 )
					return true;

				IUIListRow *pSelRow = pList->GetItem( nSelItem );
				int nSel = pSelRow->GetUserData();						// индекс в массиве
				std::string szVideo = cutscenesList[ nSel ];
				szVideo += NStr::Format( ";%d;99", MISSION_COMMAND_MAIN_MENU );
				IMainLoop *pML = GetSingleton<IMainLoop>();
				CloseInterface();
				GetSingleton<ISFX>()->StopStream( GetGlobalVar( "Sound.TimeToFade", 5000 ) );

				pML->Command( MISSION_COMMAND_VIDEO, szVideo.c_str() );
				SetGlobalVar( "LastCutscene", nSelItem );
			}
			return true;
	}

	return false;
}
