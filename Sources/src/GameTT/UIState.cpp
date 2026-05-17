#include "StdAfx.h"

#include "..\Main\GameStats.h"
#include "..\Main\ScenarioTracker.h"
#include "MainMenu.h"
#include "UIState.h"
#include "CommonId.h"
#include "Campaign.h"
#include "MultiplayerCommandManager.h"
#include "..\AILogic\AILogic.h"
#include "..\StreamIO\OptionSystem.h"
#include "CutScenesHelper.h"
#include "UIConsts.h"
#include "..\Misc\FileUtils.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IUIState::Show()
{
	GetSingleton<IScene>()->SetToolTip( 0, CVec2(0, 0), CTRect<float>(0, 0, 0, 0) );
	
	IUIElement *pMenu = GetMainInterface()->GetUIScreen()->GetChildByID( nMenuID );
	pMenu->ShowWindow( UI_SW_SHOW );
	
	CPtr<IText> pHeaderText = GetSingleton<ITextManager>()->GetString( szTitleKey.c_str() );
	NI_ASSERT_T( pHeaderText != 0, NStr::Format( "There is no text with key %s", szTitleKey.c_str() ) );
	IUIElement *pHeader = GetMainInterface()->GetUIScreen()->GetChildByID( 20001 );
	pHeader->SetWindowText( 0, pHeaderText->GetString() );
	GetMainInterface()->RefreshCursor();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IUIState::Hide()
{
	IUIElement *pMenu = GetMainInterface()->GetUIScreen()->GetChildByID( nMenuID );
	pMenu->ShowWindow( UI_SW_HIDE );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMainMenuState::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			GetSingleton<IInput>()->AddMessage( SGameMessage( IMC_SHOW_EXIT_GAME ) );
			return true;
		case IMC_OK:
			GetSingleton<IAILogic>()->SetNetGame( false );
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_NEW_GAME );
			return true;
		case IMC_MULTIPLAYER:
			// check if strings exists
			// if it is, then display message, else switch to multiplayer directly
			{
				const bool bFileExists = NFile::IsFileExist( "Data\\DisplayESRBMessage.txt" );
				if ( bFileExists )
					GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_MULTIPLAYER );
				else
				{
					ITextManager *pTM = GetSingleton<ITextManager>();
					GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX, 
												NStr::Format( "%s;%s;1;EnterMultiplauer.Confirm", "Textes\\UI\\MessageBox\\enter_multiplayer_caption",
																 "Textes\\UI\\MessageBox\\enter_multiplayer_message" ) );
				}
			}
			return true;
		case IMC_LOAD_GAME:
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_LOAD_GAME );
			return true;
		case IMC_SETTINGS:
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_OPTIONS );
			return true;
		case IMC_EXIT_GAME:
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_EXIT_GAME, 0 );
			return true;
	}
	
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUINewGameState::Show()
{
	IUIState::Show();

	IScenarioTracker * pTracker = GetSingleton<IScenarioTracker>();

	//check if player's name differ from default
	IOptionSystem * pOptions =  GetSingleton<IOptionSystem>();
	const SOptionDesc * pDesc = pOptions->GetDesc( "GamePlay.PlayerName" );
	variant_t varPlayerName;
	pOptions->Get( "GamePlay.PlayerName", &varPlayerName );
	const std::wstring szNameFromOptions = (wchar_t*)(bstr_t)varPlayerName;
	IText * pT = GetSingleton<ITextManager>()->GetDialog( "Textes\\PlayerName" );
	const std::wstring szDefault = pT ? reinterpret_cast<const wchar_t*>(pT->GetString()) : L"";

	if (  !GetGlobalVar( "ProfileShown", 0 ) && 
				!GetGlobalVar( "TutorialMode", 0 ) &&
				szNameFromOptions == szDefault || szNameFromOptions.empty() )
	{
		SetGlobalVar( "ProfileShown", 1 );
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_PLAYER_PROFILE, 0 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUINewGameState::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_MAIN_MENU );
			return true;
		case IMC_OK:
			if ( GetGlobalVar( "MOD.Active", 0 ) != 1 )
			{
				GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_SELECT_CAMPAIGN );
			}
			else
			{
				GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_SWITCH_MODE_TO, 
																											NStr::Format( ";;%d;%d;0",		
																																		MISSION_COMMAND_MAIN_MENU, 
																																		CInterfaceMainMenu::E_SELECT_CAMPAIGN 
																																	)
																					);
			}
			RemoveGlobalVar( "TutorialMode" );
			return true;
		case IMC_TUTORIAL:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_TUTORIAL_LIST, 0 );
			return true;
		case IMC_SINGLE_MISSION:
			RemoveGlobalVar( "TutorialMode" );
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_CUSTOM_GAME );
			return true;
		case IMC_PLAYER_PROFILE:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_PLAYER_PROFILE, 0 );
			return true;
	}
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUISelectCampaignState::Show()
{
	IUIState::Show();

	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	IUIScreen *pUIScreen = GetMainInterface()->GetUIScreen();

	// CRAP{ криво у нас кампании на уровне интерфейса сделаны...
	const int indices[] = { 10004, 10003, 10002 };
	// 10004 - german (0), 10003 - russian (1), 10002 - allies (2)
	// CRAP}
	for ( int i = 0; i < 3; ++i )
	{
		const std::string szPartyName = CUIConsts::GetPartyNameByNumber( i );
		const std::string szCampaignName = "scenarios\\campaigns\\" + szPartyName + "\\" + szPartyName;

		if ( pStorage->IsStreamExist((szCampaignName + ".xml").c_str()) == false )
		{
			if ( IUIElement *pElement = pUIScreen->GetChildByID(indices[i]) )
				pElement->EnableWindow( false );
		}
		else
		{
			if ( IUIElement *pElement = pUIScreen->GetChildByID(indices[i]) )
				pElement->EnableWindow( true );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUISelectCampaignState::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_NEW_GAME );
			return true;
		case IMC_GERMAN_CAMPAIGN:
		case IMC_RUSSIAN_CAMPAIGN:
		case IMC_ALLIES_CAMPAIGN:
		{
			
			IMainLoop *pML = GetSingleton<IMainLoop>();
			SetGlobalVar( "Chapter.Current", 0 );
			int nNewCampaign;
			
			switch ( msg.nEventID ) 
			{
				case IMC_GERMAN_CAMPAIGN:
					nNewCampaign = 0;
					break;
				case IMC_RUSSIAN_CAMPAIGN:
					nNewCampaign = 1;
					break;
				case IMC_ALLIES_CAMPAIGN:
					nNewCampaign = 2;
					break;
			}

			SetGlobalVar( "Campaign.Current", nNewCampaign );
			const SCampaignStats *pCampaignStats = CInterfaceCampaign::ReadCampaignStats();
			// don't start campaign in the case of empty stats
			if ( pCampaignStats == 0 ) 
			{
				SetGlobalVar( "Campaign.Current", -1 );
				break;
			}
			// проиграем видео
			SetGlobalVar( "Campaign.Current.Name", pCampaignStats->szParentName.c_str() );
			GetSingleton<IScenarioTracker>()->StartCampaign( pCampaignStats->szParentName.c_str(), CAMPAIGN_TYPE_SINGLE );

			NCutScenes::AddCutScene( pCampaignStats->szIntroMovie );
			GetSingleton<ISFX>()->StopStream( GetGlobalVar( "Sound.TimeToFade", 5000 ) );
			GetMainInterface()->FinishInterface( MISSION_COMMAND_VIDEO, NStr::Format("%s;%d", pCampaignStats->szIntroMovie.c_str(), MISSION_COMMAND_CAMPAIGN) );
			return true;
		}
		break;
	}
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIOptionsState::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_MAIN_MENU );
			return true;
		case IMC_OPTIONS:
			GetMainInterface()->FinishInterface( MISSION_COMMAND_OPTIONSSETTINGS, 0 );
			return true;
		case IMC_MODS:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MODS_LIST, 0 );
			return true;
		case IMC_VIDEO:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_CUTSCENE_LIST, 0 );
			return true;
		case IMC_CREDITS:
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_CREDITS );
			return true;
	}
	
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMultiplayerState::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_MAIN_MENU );

			return true;
		case IMC_INTERNET:
			GetSingleton<IMPToUICommandManager>()->SetConnectionType( EMCT_INTERNET );
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d %d 1", MAIN_MP_TRANSCEIVER, GetSingleton<IMPToUICommandManager>()->GetConnectionType() ) );
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_ADDRESS_BOOK, 0 );
			
			return true;
		case IMC_LAN:
			GetSingleton<IMPToUICommandManager>()->SetConnectionType( EMCT_LAN );
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d %d 1", MAIN_MP_TRANSCEIVER, GetSingleton<IMPToUICommandManager>()->GetConnectionType() ) );
			GetMainInterface()->FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );

			return true;
		case IMC_GAMESPY:
			GetSingleton<IMPToUICommandManager>()->SetConnectionType( EMCT_GAMESPY );
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d %d 1", MAIN_MP_TRANSCEIVER, GetSingleton<IMPToUICommandManager>()->GetConnectionType() ) );
			GetMainInterface()->FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );

			return true;
	}
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICustomGameState::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_NEW_GAME );
			return true;
		case IMC_CUSTOM_CAMPAIGNS:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_CUSTOM_CAMPAIGN, 0 );
			return true;
		case IMC_CUSTOM_CHAPTERS:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_CUSTOM_CHAPTER, 0 );
			return true;
		case IMC_CUSTOM_MISSIONS:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_CUSTOM_MISSION, 0 );
			return true;
	}
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUILoadGameState::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_MAIN_MENU );
			return true;
		case IMC_LOAD_LOAD_GAME:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_IM_LOAD_MISSION, 0 );
			return true;
		case IMC_LOAD_LOAD_REPLAY:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_REPLAY_LIST, 0 );
			return true;
	}
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICreditsState::Show()
{
	IUIState::Show();
	//
	GetMainInterface()->GetUIScreen()->GetChildByID( 20001 )->ShowWindow( UI_SW_HIDE );
	GetMainInterface()->GetUIScreen()->GetChildByID( 2007 )->ShowWindow( UI_SW_HIDE );
	GetMainInterface()->GetUIScreen()->GetChildByID( 7777 )->ShowWindow( UI_SW_SHOW );
	const std::string szCreditsStreamSound = GetGlobalVar( "CreditsStreamSound", "" );
	int nTimeToFade = GetGlobalVar( "Sound.TimeToFade", 5000 );
	if ( !szCreditsStreamSound.empty() )
		GetSingleton<ISFX>()->PlayStream( szCreditsStreamSound.c_str(), true, nTimeToFade );
	
	bLeaveToMainMenu = GetGlobalVar( "FinishingCampaign", 0 ) == 1;
	RemoveGlobalVar( "FinishingCampaign" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICreditsState::Hide()
{
	GetMainInterface()->GetUIScreen()->GetChildByID( 20001 )->ShowWindow( UI_SW_SHOW );
	GetMainInterface()->GetUIScreen()->GetChildByID( 7777 )->ShowWindow( UI_SW_HIDE );
	GetMainInterface()->PlayIntermissionSound();
	//
	IUIState::Hide();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICreditsState::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
		case MC_CANCEL_CREDITS:
			if ( bLeaveToMainMenu )
				GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_MAIN_MENU );
			else
				GetMainInterface()->SetActiveState( CInterfaceMainMenu::E_OPTIONS );
			return true;
		default:
			break;
	}
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIDemoMainMenuState::ProcessMessage( const SGameMessage &msg )
{
	switch( msg.nEventID )
	{
	case IMC_CANCEL:
		GetSingleton<IInput>()->AddMessage( SGameMessage( IMC_SHOW_EXIT_GAME ) );
		return true;

	case IMC_DEMO_NEW_GAME:
		{
			SetGlobalVar( "Chapter.Current.Name", "custom_mission" );

			const std::string szMission = GetGlobalVar( "demomission.0", "" );
			SetGlobalVar( "Mission.Current.Name", szMission.c_str() );

			const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( szMission.c_str(), IObjectsDB::MISSION );
			NI_ASSERT_T( pStats != 0, (std::string("Invalid custom mission ") + szMission).c_str() );
			if ( !pStats )
				return true;

			IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
			pST->StartCampaign( "custom_mission", CAMPAIGN_TYPE_CUSTOM_MISSION );
			pST->StartChapter( "custom_mission" );
			GetMainInterface()->FinishInterface( MISSION_COMMAND_MISSION, (pStats->szFinalMap +".xml").c_str() );
		}			
		break;
	case IMC_DEMO_OPTIONS:
		GetMainInterface()->FinishInterface( MISSION_COMMAND_OPTIONSSETTINGS, 0 );

		break;
	case IMC_DEMO_LOAD_GAME:
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_IM_LOAD_MISSION, 0 );

		return true;
	case IMC_DEMO_EXIT_GAME:
		GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_EXIT_GAME, 0 );
		return true;
	}
	return false;
}