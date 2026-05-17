#include "StdAfx.h"

#include "InterfaceOptionsSettings.h"
#include "..\StreamIO\OptionSystem.h"
#include "CommonID.h"
#include "OptionEntryWrapper.h"
#include "MainMenu.h"
#include "..\Main\iMainCommands.h"
#include <mmsystem.h>
#include "..\Common\actions.h"
#include "..\Main\ScenarioTracker.h"
//////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////
enum EButtonsInOptionsSettings
{
	_E_BUTTON_CHANGE_DIVISION_BEGIN					= 10007,
	// .....
	_E_BUTTON_CHANGE_DIVISION_END						= 10009,

	
	_E_LIST_BEGIN						= 1000,
	// .... 
	_E_LIST_END							= 1002,

	
	E_BUTTON_DEFAULT				= 10003,
};
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//**		CInterfaceMPGamesList
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool CInterfaceOptionsSettings::OpenCurtains()
{
	if ( !GetGlobalVar( "AreWeInMission", 0 ) )
		OpenCurtainsForced();
	return true;
}
//////////////////////////////////////////////////////////////////////
void CInterfaceOptionsSettings::SuspendAILogic( bool bSuspend )
{
	if ( GetGlobalVar( "AreWeInMission", 0 ) && GetGlobalVar( "MultiplayerGame", 0 ) == 1 )
	{
		// no action, cannot suspend AILogic
	}
	else
	{
		CInterfaceInterMission::SuspendAILogic( bSuspend );
	}
}
//////////////////////////////////////////////////////////////////////
bool CInterfaceOptionsSettings::Init()
{
	CInterfaceInterMission::Init();
	SetBindSection( "loadmission" );
	commandMsgs.Init( pInput, commonCommands );
	if ( GetGlobalVar( "AreWeInMission", 0 ) && GetGlobalVar( "MultiplayerGame", 0 ) == 1 )
		SuspendAILogic( false );
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceOptionsSettings::Done()
{
	CInterfaceInterMission::Done();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceOptionsSettings::Create()
{
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	const bool bInMission = GetGlobalVar( "AreWeInMission", 0 );
	if ( bInMission )
		pUIScreen->Load( "ui\\MissionOptionsSettings" );
	else
		pUIScreen->Load( "ui\\OptionsSettings" );
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	if ( !bInMission )
		StoreScreen();
	pScene->AddUIScreen( pUIScreen );
	
	std::unordered_map< std::string, OptionDescs > sections;
	
	IOptionSystem * pOptionSystem = GetSingleton<IOptionSystem>();

	// determine the set of options
	const int nOptionFlag = GetGlobalVar("MultiplayerGame", 0) == 1 ? 
													OPTION_FLAG_IN_MP_MISSION  : 
													(bInMission ? OPTION_FLAG_IN_MISSION : OPTION_FLAG_MAIN_OPTIONS);
	
	for ( CPtr<IOptionSystemIterator> pIter = pOptionSystem->CreateIterator( nOptionFlag );
				!pIter->IsEnd(); pIter->Next() )
	{
		const SOptionDesc * pDesc = pIter->GetDesc();
		sections[pDesc->szDivision].push_back( *pDesc );
	}
	
	ITextManager * pTM = GetSingleton<ITextManager>();
	
	nMaxDivision = 0;
	const std::string szKeyOption = "Textes\\Options\\";
	for ( std::unordered_map< std::string, OptionDescs >::iterator it = sections.begin(); it != sections.end(); ++it )
	{
		const std::string szKeyName = szKeyOption + it->first + ".name";
		const std::string szKeyTooltip = szKeyOption + it->first + ".tooltip";

		IText *pT = pTM->GetString( szKeyName.c_str() );
		
		NI_ASSERT_T( pT != 0, NStr::Format( "no local name for section %s", szKeyName.c_str() ) );
		
		IUIListControl * pList = checked_cast<IUIListControl*>(pUIScreen->GetChildByID( _E_LIST_BEGIN + nMaxDivision ));
		IUIStatic * pCaption = checked_cast<IUIStatic*>( pList->GetChildByID( 10 ) );
		
		optionsLists.push_back( new COptionsListWrapper( pList, it->second, 100 ) );
		IUIButton * pButton = checked_cast<IUIButton *>( pUIScreen->GetChildByID( _E_BUTTON_CHANGE_DIVISION_BEGIN + nMaxDivision ) );
		
		pButton->SetWindowText( -1, pT->GetString() );
		pCaption->SetWindowText( -1, pT->GetString() );

		pT = pTM->GetString( szKeyTooltip.c_str() );
		if ( pT )
			pButton->SetHelpContext( 0, pT->GetString() );
		
		pButton->ShowWindow( UI_SW_SHOW );
		++nMaxDivision;
	}
	
	nActive = -1;
	OnChangeDivision( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceOptionsSettings::OnChangeDivision( const int nDivision )
{
	if ( nActive != -1 )
	{
		optionsLists[nActive]->ShowWindow( UI_SW_HIDE );
		
		//enable ���������� ������
		IUIElement *pElement = pUIScreen->GetChildByID( nActive + _E_BUTTON_CHANGE_DIVISION_BEGIN );
		NI_ASSERT_T( pElement != 0, NStr::Format("There is no button with id %d") );
		pElement->EnableWindow( true );
	}
	nActive = nDivision;
	
	optionsLists[nActive]->ShowWindow( UI_SW_SHOW );
	optionsLists[nActive]->ShowWindow( UI_SW_MAXIMIZE );

	//disable ������� ������
	IUIElement *pElement = pUIScreen->GetChildByID( nActive + _E_BUTTON_CHANGE_DIVISION_BEGIN );
	NI_ASSERT_T( pElement != 0, NStr::Format("There is no button with id %d") );
	pElement->EnableWindow( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceOptionsSettings::Close()
{
	if ( GetGlobalVar( "AreWeInMission", 0 ) )
	{
		IMainLoop *pML = GetSingleton<IMainLoop>();
		CloseInterface();
		pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_GAME_UNPAUSE_MENU) );	//������ �����
		pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_LOAD_FINISHED) );
	}
	else
		FinishInterface( MISSION_COMMAND_MAIN_MENU, NStr::Format( "%d", CInterfaceMainMenu::E_OPTIONS ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceOptionsSettings::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;

	// changin division
	if ( msg.nEventID >= _E_BUTTON_CHANGE_DIVISION_BEGIN && msg.nEventID < nMaxDivision + _E_BUTTON_CHANGE_DIVISION_BEGIN )
	{
		OnChangeDivision( msg.nEventID - _E_BUTTON_CHANGE_DIVISION_BEGIN );
	}
	//

	//process message to options.
	if ( nActive >= 0 &&
			 optionsLists.size() > nActive && 
			 optionsLists[nActive]->ProcessMessage( msg ) ) 
	{
		return true;
	}

	if ( CInterfaceInterMission::ProcessMessage( msg ) ) return true;

	//process buttons pressings
	switch( msg.nEventID )
	{
	
	case IMC_OK:
		{
			for ( int i = 0; i < optionsLists.size(); ++i )
				optionsLists[i]->Apply();
			// save options 
			GetSingleton<IMainLoop>()->SerializeConfig( false, 0xffffffff );
			Close();
		}
		return true;

	case IMC_CANCEL:
		for ( int i = 0; i < optionsLists.size(); ++i )
			optionsLists[i]->CancelChanges();
		Close();
		
		return true;
	case E_BUTTON_DEFAULT:
		for ( int i = 0; i < optionsLists.size(); ++i )
			optionsLists[i]->ToDefault();
		
		return true;
	}
	return false;
}