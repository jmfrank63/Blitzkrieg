#include "StdAfx.h"

#include "iMission.h"

#include <mmsystem.h>

#include "iMissionInternal.h"
#include "UIConsts.h"
#include "MultiplayerCommandManager.h"
#include "..\Main\ServerInfo.h"
#include "..\main\gamestats.h"
#include "..\Common\Actions.h"
#include "..\Misc\HPTimer.h"
#include "..\Anim\Animation.h"
#include "..\Scene\Terrain.h"
#include "..\Main\GameDB.h"
#include "..\Main\CommandsHistoryInterface.h"
#include "..\AILogic\AILogic.h"
#include "..\AILogic\AITypes.h"
#include "..\AILogic\AIConsts.h"
#include "..\Formats\fmtTerrain.h"
#include "..\Main\TextSystem.h"

void SetChildWindowText( IUIElement *pParent, const int nID, const wchar_t *pszText );
void SetChildWindowText( IUIElement *pParent, const int nID, const std::wstring &szText );
void SetUIWindowText( IUIElement *pElement, const wchar_t *pszText );
void SetUIWindowText( IUIElement *pElement, const std::wstring &szText );
void SetHelpContext( IUIElement *pElement, const wchar_t *pszText );
void SetHelpContext( IUIElement *pElement, const std::wstring &szText );
void OutputString( IUIStatusBar *pBar, const int nIndex, const wchar_t *pszText );
void OutputString( IUIStatusBar *pBar, const int nIndex, const std::wstring &szText );
void SetUnitProperty( IUIStatusBar *pBar, const int nIndex, const int nLevel, const wchar_t *pszText );
void SetUnitProperty( IUIStatusBar *pBar, const int nIndex, const int nLevel, const std::wstring &szText );
#include "..\Main\Transceiver.h"
#include "..\Main\ScenarioTracker.h"
#include "..\Main\ScenarioTrackerTypes.h"
#include "..\Main\iMainCommands.h"

#include "..\Image\Image.h"
#include "..\RandomMapGen\IB_Types.h"
#include "..\RandomMapGen\MiniMap_Types.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\StreamIO\ProgressHook.h"
#include "..\Misc\Win32Helper.h"

#include "..\Common\PauseGame.h"
#include "..\UI\UIMessages.h"
#include "MessageReaction.h"
#include "MissionInterfaceEscapeMenu.h"
#include "..\GameTT\WorldClient.h"
enum
{
	E_MULTIPLAYER_PLAYER_LAGGED_DIALOG								= 100001,
	E_MULTIPLAYER_PLAYER_LAGGED_LIST									= 3000,
	E_MULTIPLAYER_PLAYER_LAGGED_BUTTON_BASE						= 10010,
	E_MULTIPLAYER_PLAYER_LAGGED_DROPBUTTON						= 10001,

	E_SINGLE_OBJECTIVE_BUTTON													= 6001,

	E_DIALOG_MULTIPLAYER_SCORES_SMALL									= 30000,

	E_MULTIPLAYER_SCORES_SMALL_TEAMFRAGS							= 20001,
	E_MULTIPLAYER_SCORES_SMALL_TEAMFLAGS							= 20002,
	E_MULTIPLAYER_SCORES_SMALL_TIME										= 20003,
	E_MULTIPLAYER_SCORES_SMALL_TIMEBEFORECAPTURE			= 20004,

	E_MULTIPLAYER_SCORES_SMALL_TEAMFRAGS_VAL					= 20011,
	E_MULTIPLAYER_SCORES_SMALL_TEAMFLAGS_VAL					= 20012,
	E_MULTIPLAYER_SCORES_SMALL_TIME_VAL								= 20013,
	E_MULTIPLAYER_SCORES_SMALL_TIMEBEFORECAPTURE_VAL	= 20014,
	E_MULTIPLAYER_SCORES_SMALL_MOD_NAME							  = 20015,
	E_MULTIPLAYER_SCORES_SMALL_MOD_VERSION						= 20016,

	E_DIALOG_REPLAY_SS																= 30001,
	
	E_REPLAY_SS_TIMEBEFORECAPTURE											= 20001,
	E_REPLAY_SS_TIMEBEFORECAPTURE_VAL									= 20002,

	E_REPLAY_SS_TIME																	= 20003,
	E_REPLAY_SS_TIME_VAL															= 20004,

	E_REPLAY_SS_TEAM_1_PARTYNAME											= 20016,
	E_REPLAY_SS_TEAM_1_FLAGS													= 20005,
	E_REPLAY_SS_TEAM_1_FLAGS_VAL											= 20006,
	E_REPLAY_SS_TEAM_1_FRAGS													= 20007,
	E_REPLAY_SS_TEAM_1_FRAGS_VAL											= 20008,
	
	E_REPLAY_SS_TEAM_2_PARTYNAME											= 20015,
	E_REPLAY_SS_TEAM_2_FLAGS													= 20009,
	E_REPLAY_SS_TEAM_2_FLAGS_VAL											= 20010,
	E_REPLAY_SS_TEAM_2_FRAGS													= 20011,
	E_REPLAY_SS_TEAM_2_FRAGS_VAL											= 20012,

	E_REPLAY_SS_MOD_NAME															= 20013,
	E_REPLAY_SS_MOD_VERSION														= 20014,



	E_MULTIPLAYER_TIMEOUT_DIALOG											= 100002,
	E_MULTIPLAYER_TIMEOUT_CAPTION											= 1999,
	E_MULTIPLAYER_TIMEOUT_COUNTER											= 2000,
	E_MULTIPLAYER_BUTTON_CANCEL												= 2001,
	E_MULTIPLAYER_TIMEOUT_MESSAGE											= 2002,


	E_MULTIPLAYER_PLAYER_LOADING_DIALOG								= 100003,
	E_MULTIPLAYER_PLAYER_LOADING_LIST									= 3000,

};
static const NInput::SRegisterCommandEntry missionCommands[] = 
{
	{ "show_warfog"					, MC_SHOW_WARFOG					},
	{ "center_camera"				, MC_CENTER_CAMERA				},
#if !defined(_FINALRELEASE) || defined(_PROFILER)
	{ "show_grid"						,	MC_SHOW_GRID						},
	{ "show_objects"				, MC_SHOW_OBJECTS					},
	{ "show_units"					, MC_SHOW_UNITS						},
	{ "show_bound_boxes"		, MC_SHOW_BOUND_BOXES			},
	{ "show_terrain"				, MC_SHOW_TERRAIN					},
	{ "show_shadows"				, MC_SHOW_SHADOWS					},
	{ "show_effects"				, MC_SHOW_EFFECTS					},
	{ "show_statistics"			,	MC_SHOW_STATISTICS			},
	{ "show_depth"					, MC_SHOW_DEPTH						},
	{ "show_ui"							,	MC_SHOW_UI							},
	{ "select_first_object"	, MC_SELECT_FIRST_OBJECT	},
	{ "select_next_object"	, MC_SELECT_NEXT_OBJECT		},
	{ "drop_object"					, MC_DROP_OBJECT					},
	{ "enter_ingame_editor"	, MC_ENTER_INGAME_EDITOR	},
	{ "leave_ingame_editor"	, MC_LEAVE_INGAME_EDITOR	},
	{ "save_scene"					, MC_SAVE_SCENE						},
#endif // !defined(_FINALRELEASE) || defined( _PROFILER )
	{ "show_avia_buttons"		,	MC_SHOW_AVIA_BUTTONS		},
	{ "add_action_on"				,	MC_ADD_ACTION_ON				},
	{ "add_action_off"			,	MC_ADD_ACTION_OFF				},
	{ "force_action_move_on",	MC_FORCE_ACTION_MOVE_ON	},
	{ "force_action_move_off",	MC_FORCE_ACTION_MOVE_OFF	},
	{ "force_action_attack_on",	MC_FORCE_ACTION_ATTACK_ON	},
	{ "force_action_attack_off",	MC_FORCE_ACTION_ATTACK_OFF	},
	{ "reset_selection"			,	MC_RESET_SELECTION			},
	{ "show_console"				, MC_SHOW_CONSOLE					},
	{ "enter_chat_mode"			, MC_ENTER_CHAT_MODE			},
	{ "enter_chat_mode_friends", MC_ENTER_CHAT_MODE_FRIENDS	},
	{ "clear_screen_acks"		,	MC_CLEAR_SCREEN_ACKS		},
	{ "show_escape_menu"		, MC_SHOW_ESCAPE_MENU			},
	{ "show_save_menu"			, MC_SHOW_SAVE_MENU				},
	{ "show_help_screen"		, MC_SHOW_HELP_SCREEN			},
	{ "show_status_bar"			, MC_TOGGLE_UNIT_INFO			},
	{ "show_objectives"			, MC_SHOW_OBJECTIVES			},
	{ "show_single_objective", WCC_SHOW_LAST_OBJECTIVE },
	{ "begin_timeout"				,	CMD_GAME_TIMEOUT_SEND		},
	{ "stop_timeout"				,	CMD_GAME_UNTIMEOUT_SEND	},
	{ "prop_lagged_player"  , MC_MP_DROP_LAGGED_PLAYER},
	{ "make_map_shot"				, MC_MAKE_MAP_SHOT				},
	{ 0											,	0												}
};
static DWORD timeFrameLastTime = 0;
static DWORD timePeriodTime = 0;
static NTimer::STime timeFrameLastGameTime = 0;
static int nPeriodCounter = 0;
inline void ResetGameSpeedAdjusting()
{
	timeFrameLastTime = 0;
	timeFrameLastGameTime = 0;
	timePeriodTime = 0;
	nPeriodCounter = 0;
}
int CICMission::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szMapName );
	return 0;
}
void CICMission::Configure( const char *pszConfig )
{
	if ( !pszConfig ) return;
	std::vector<std::string> szStrings;
	NStr::SplitString( pszConfig, szStrings, ';' );
	if ( szStrings.size() > 0 ) 
		szMapName = szStrings[0];
	if ( szStrings.size() > 1 ) 
		bCycledLaunch = NStr::ToInt( szStrings[1] ) != 0;
	else
		bCycledLaunch = false;
}
void GUID2String( std::string *pString, const GUID &guid )
{
	*pString = NStr::Format( "%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], 
														guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7] );
	NStr::ToUpper( *pString );
}
void CICMission::PreCreate( IMainLoop *pML ) 
{ 
	pML->ResetStack(); 
	pML->ClearResources(); 
}
void CICMission::PostCreate( IMainLoop *pML, CInterfaceMission *pInterface ) 
{ 
	NStr::ToLower( szMapName );
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	const std::string szTerrainName = std::string( "maps\\" ) + szMapName.substr( 0, szMapName.rfind( '.' ) );
	SStorageElementStats stats;
	Zero( stats );
	if ( pStorage->IsStreamExist( (szTerrainName + ".xml").c_str() ) == false &&
		   pStorage->IsStreamExist( (szTerrainName + ".bzm").c_str() ) == false ) 
	{
		pML->Command( MISSION_COMMAND_MAIN_MENU, 0 );
		return;
	}
	SetGlobalVar( "Map.Current.Name", szMapName.c_str() );
	GetSingleton<ICommandsHistory>()->LoadCommandLineHistory();

	GetSingleton<ICommandsHistory>()->PrepareToStartMission();
	GetSingleton<ITransceiver>()->PreMissionInit();
	pML->PushInterface( pInterface ); 

	if ( pInterface->NewMission(szMapName, bCycledLaunch) )
	{
		std::string szSaveName;
		szSaveName += CUIConsts::GetCampaignNameAddition();
		szSaveName += " Mission Start Auto";
		szSaveName += ".sav";
		pML->Command( MAIN_COMMAND_SAVE, NStr::Format( "%s;1", szSaveName.c_str() ) );
		pInterface->CheckResolution();
		pInterface->ConfigureInterfacePreferences();
	}
}
void CInterfaceMission::CTimeoutDialog::ProcessMessage( const SGameMessage &msg, IUIScreen * pUIScreen )
{
	switch( msg.nEventID )
	{
	case CMD_GAME_UNTIMEOUT:
		GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_MP_TIMEOUT );
		{
			IUIDialog *pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_MULTIPLAYER_TIMEOUT_DIALOG ) );
			pDialog->ShowWindow( UI_SW_HIDE );
		}

		break;
	case CMD_GAME_TIMEOUT_UPDATE:
		{
			IUIDialog *pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_MULTIPLAYER_TIMEOUT_DIALOG ) );
			IUIStatic *pCounter = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_TIMEOUT_COUNTER ) );
			SetUIWindowText( pCounter, NStr::ToUnicode( NStr::Format( "%d", msg.nParam ) ) );
		}
		break;

	case CMD_GAME_TIMEOUT:
		GetSingleton<IMainLoop>()->Pause( true, PAUSE_TYPE_MP_TIMEOUT );
		{
			IUIDialog *pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_MULTIPLAYER_TIMEOUT_DIALOG ) );
			IScenarioTracker * pScenarioTracker = GetSingleton<IScenarioTracker>();
			IUIElement *pMessage = checked_cast<IUIElement*>( pDialog->GetChildByID( E_MULTIPLAYER_TIMEOUT_MESSAGE ) );
			IUIButton *pButtonCancel = checked_cast<IUIButton*>( pDialog->GetChildByID( BUTTON_MP_TIMEOUT_CANCEL ) );
			IText *pText = GetSingleton<ITextManager>()->GetString( "Textes\\UI\\Mission\\TimeoutDialog\\message_timeout" );
			pButtonCancel->EnableWindow( pScenarioTracker->GetUserPlayerID() == msg.nParam );
			std::wstring szMessage = pScenarioTracker->GetPlayer( msg.nParam )->GetName();
			szMessage += L" ";
			szMessage += MakeWideStringFromWordString( pText->GetString() );
			SetUIWindowText( pMessage, szMessage );
			pDialog->ShowWindow( UI_SW_SHOW );
			IUIDialog *pPlayerLagged = checked_cast<IUIDialog*>( pUIScreen->GetChildByID(E_MULTIPLAYER_PLAYER_LAGGED_DIALOG) );
			if ( pPlayerLagged->IsVisible() )
				pPlayerLagged->ShowWindow( UI_SW_MAXIMIZE );
		}
		break;

	}		
}
void CInterfaceMission::CTimeoutDialog::StepLocal( IUIScreen * pUIScreen )
{
}
void CInterfaceMission::CPlayerLaggedDialog::CDialogStateLagged::RemovePlayer( const int nPlayer, IUIScreen *pUIScreen )
{
	players.Delete( nPlayer );
	if ( players.IsEmpty() )
		Hide( pUIScreen );
}
void CInterfaceMission::CPlayerLaggedDialog::CDialogStateLagged::Show( IUIScreen *pUIScreen )
{
	IUIDialog * pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_MULTIPLAYER_PLAYER_LAGGED_DIALOG ) );
	bShowedWindow = true;
	pDialog->EnableWindow( true );
	pDialog->ShowWindow( UI_SW_SHOW );
	GetSingleton<IMainLoop>()->Pause( true, PAUSE_TYPE_MP_LOADING );
	
	players.SetListControl( checked_cast<IUIListControl*>( pDialog->GetChildByID( E_MULTIPLAYER_PLAYER_LAGGED_LIST ) ) );
}
void CInterfaceMission::CPlayerLaggedDialog::CDialogStateLagged::Hide( IUIScreen *pUIScreen )
{
	bShowedWindow = false;
	IUIDialog * pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_MULTIPLAYER_PLAYER_LAGGED_DIALOG ) );
	GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_MP_LOADING );
	pDialog->ShowWindow( UI_SW_HIDE );
}
void CInterfaceMission::CPlayerLaggedDialog::CDialogStateLagged::AddPlayer( const int nParam, IUIScreen *pUIScreen )
{
	const int nPlayer = nParam & 0xff;
	const int nTime = (( nParam &0xffff0000 ) >> 16);

	if ( players.IsEmpty() )
	{
		IUIDialog * pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_MULTIPLAYER_PLAYER_LAGGED_DIALOG ) );
		Show( pUIScreen );
	}
	IUIListRow * pRow = players.Add( new SPlayerLaggedInfo( nPlayer, nTime ) );
	IUIElement * pEl = pRow->GetElement( 0 );
	IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetPlayer( nPlayer );
	SetUIWindowText( pEl, pPlayer->GetName() );
	IUIDialog *pDialogButton = checked_cast<IUIDialog*>( pRow->GetElement( 1 ) );
	
	IUIElement *pDropButtonFormer = pDialogButton->GetChildByID( E_MULTIPLAYER_PLAYER_LAGGED_DROPBUTTON );
	if ( !pDropButtonFormer )
		pDropButtonFormer = pDialogButton->GetChildByID( E_MULTIPLAYER_PLAYER_LAGGED_BUTTON_BASE + nPlayer );
	IUIButton *pDropButton = checked_cast<IUIButton*>( pDropButtonFormer );
	pDropButton->SetWindowID ( E_MULTIPLAYER_PLAYER_LAGGED_BUTTON_BASE + nPlayer );

	pDropButton->EnableWindow( !nTime );
	if ( nTime )
	{
		SetUIWindowText( pDropButton, NStr::ToUnicode( NStr::Format( "%i", nTime ) ) );
		pDropButton->EnableWindow( false );
	}
	else
	{
		ITextManager * pTM = GetSingleton<ITextManager>();
		IText *pText = pTM->GetString( "Textes\\UI\\Mission\\PlayerLaggedDialog\\button_drop_players" );
		pDropButton->SetWindowText( 0, pText->GetString() );
	}
}
void CInterfaceMission::CPlayerLaggedDialog::CDialogStateLoading::Show( IUIScreen *pUIScreen )
{
	IUIDialog * pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_MULTIPLAYER_PLAYER_LOADING_DIALOG ) );
	bShowedWindow = true;
	pDialog->EnableWindow( true );
	pDialog->ShowWindow( UI_SW_SHOW );
	GetSingleton<IMainLoop>()->Pause( true, PAUSE_TYPE_MP_LOADING );
	
	players.SetListControl( checked_cast<IUIListControl*>( pDialog->GetChildByID( E_MULTIPLAYER_PLAYER_LOADING_LIST ) ) );

}
void CInterfaceMission::CPlayerLaggedDialog::CDialogStateLoading::Hide( IUIScreen *pUIScreen )
{
	bShowedWindow = false;
	IUIDialog * pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_MULTIPLAYER_PLAYER_LOADING_DIALOG ) );
	GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_MP_LOADING );
	pDialog->ShowWindow( UI_SW_HIDE );
}
void CInterfaceMission::CPlayerLaggedDialog::CDialogStateLoading::AddPlayer( const int nParam, IUIScreen *pUIScreen )
{
	const int nPlayer = nParam;
	
	if ( players.IsEmpty() )
	{
		IUIDialog * pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_MULTIPLAYER_PLAYER_LOADING_DIALOG) );
		Show( pUIScreen );
	}
	IUIListRow * pRow = players.Add( new SPlayerLoadingInfo( nPlayer ) );
	IUIElement * pEl = pRow->GetElement( 0 );
	IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetPlayer( nPlayer );
	SetUIWindowText( pEl, pPlayer->GetName() );
}
void CInterfaceMission::CPlayerLaggedDialog::CDialogStateLoading::RemovePlayer( const int nPlayer, IUIScreen *pUIScreen )
{
	players.Delete( nPlayer );
	if ( players.IsEmpty() )
		Hide( pUIScreen );
}
void CInterfaceMission::CPlayerLaggedDialog::ProcessMessage( const SGameMessage &msg, IUIScreen * pUIScreen )
{
	const int nPlayer = msg.nParam & 0xff;
	const int nTime = (msg.nParam & 0xff0000) >> 16;
	
	switch( msg.nEventID )
	{
	case MC_MP_LAG_FINISHED:
		if ( pState && pState->GetName() == EWM_LAG )
			pState->RemovePlayer( msg.nParam, pUIScreen );
		
		break;
	case MC_MP_LAG_STARTED:
		if ( !pState || pState->GetName() != EWM_LAG )
		{
			if ( pState )
				pState->Hide( pUIScreen );
			pState = new CDialogStateLagged;
			GetSingleton<IInput>()->AddMessage( SGameMessage( MC_SHOW_ESCAPE_MENU, 2 ) );
		}
		if ( pState && pState->GetName() == EWM_LAG )
			pState->AddPlayer( msg.nParam, pUIScreen );

		break;
	case MC_MP_DROP_LAGGED_PLAYER:
		if ( pState && pState->GetName() == EWM_LAG )
		{
			pState->RemovePlayer( msg.nParam, pUIScreen );
			IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetPlayer( msg.nParam );
			GetSingleton<ITransceiver>()->CommandClientDropPlayer( ToWordString( pPlayer->GetName() ) );
		}

		break;
	case MC_MP_PLAYER_LOAD_FINISHED:
		if ( pState && pState->GetName() == EWM_LOADING )
			pState->RemovePlayer( msg.nParam, pUIScreen );

		break;
	case MC_MP_PLAYER_LOAD_STARTED:
		if ( !pState )
		{
			pState = new CDialogStateLoading;
		}
		if ( pState && pState->GetName() == EWM_LOADING )
			pState->AddPlayer( msg.nParam, pUIScreen );

		break;
	}
	if ( pState && !pState->IsActive() )
		pState = 0;

}
void CInterfaceMission::CPlayerLaggedDialog::StepLocal( IUIScreen * pUIScreen )
{
}
void CInterfaceMission::CMultiplayerScoresSmall::CReplayScoresState::Show( IUIScreen * pUIScreen )
{
	Init( pUIScreen );
	ITextManager *pTM = GetSingleton<ITextManager>();
	IUIElement *pSingleObjectiveButton = checked_cast<IUIElement*>( pUIScreen->GetChildByID( E_SINGLE_OBJECTIVE_BUTTON ) );
	pSingleObjectiveButton->ShowWindow( UI_SW_HIDE );

	pDialog->ShowWindow( UI_SW_SHOW );

	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_REPLAY_SS_TIME_VAL) );
	IUIStatic * pName = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_REPLAY_SS_TIME ) );
	pName->ShowWindow( UI_SW_SHOW );
	pVal->ShowWindow( UI_SW_SHOW );

	IUIStatic *pParty1Name = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_REPLAY_SS_TEAM_1_PARTYNAME ) );
	pParty1Name->SetWindowText( 0, CUIConsts::GetLocalPartyName( GetGlobalVar( "Multiplayer.Side0.Name", "" ) ) );

	IUIStatic *pParty2Name = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_REPLAY_SS_TEAM_2_PARTYNAME ) );
	pParty2Name->SetWindowText( 0, CUIConsts::GetLocalPartyName( GetGlobalVar( "Multiplayer.Side1.Name", "" ) ) );
	
	if ( GetGlobalVar( "MOD.Active", 0 ) )
	{
		IUIStatic *pModName = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_MOD_NAME ) );
		IUIStatic *pModVersion = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_MOD_VERSION ) );
		SetUIWindowText( pModName, NStr::ToUnicode( GetGlobalVar( "MOD.Name", "" ) ) );
		SetUIWindowText( pModVersion, NStr::ToUnicode( GetGlobalVar( "MOD.Version", "" ) ) );
		pModName->ShowWindow( UI_SW_SHOW );
		pModVersion->ShowWindow( UI_SW_SHOW );
	}
}
void CInterfaceMission::CMultiplayerScoresSmall::CReplayScoresState::OnFrags( const int nFrags, const int nParty )
{
	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( nParty ? E_REPLAY_SS_TEAM_2_FRAGS_VAL  : E_REPLAY_SS_TEAM_1_FRAGS_VAL ) );
	SetUIWindowText( pVal, NStr::ToUnicode( NStr::Format( "%d", nFrags ) ) );
	IUIStatic * pName = checked_cast<IUIStatic*>( pDialog->GetChildByID( nParty ? E_REPLAY_SS_TEAM_2_FRAGS  : E_REPLAY_SS_TEAM_1_FRAGS ) );
	
	pName->ShowWindow( UI_SW_SHOW );
	pVal->ShowWindow( UI_SW_SHOW );
}
void CInterfaceMission::CMultiplayerScoresSmall::CReplayScoresState::OnFlags( const int nFlags, const int nParty )
{
	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( nParty ? E_REPLAY_SS_TEAM_2_FLAGS_VAL  : E_REPLAY_SS_TEAM_1_FLAGS_VAL ) );
	SetUIWindowText( pVal, NStr::ToUnicode( NStr::Format( "%d", nFlags ) ) );
	IUIStatic * pName = checked_cast<IUIStatic*>( pDialog->GetChildByID( nParty ? E_REPLAY_SS_TEAM_2_FLAGS  : E_REPLAY_SS_TEAM_1_FLAGS ) );
	pName->ShowWindow( UI_SW_SHOW );
	pVal->ShowWindow( UI_SW_SHOW );
}
void CInterfaceMission::CMultiplayerScoresSmall::CReplayScoresState::OnTimeBeforeCapture( const int nTime )
{
	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_REPLAY_SS_TIMEBEFORECAPTURE_VAL ) );
	IUIStatic * pName = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_REPLAY_SS_TIMEBEFORECAPTURE ) );
	pName->ShowWindow( nTime != 0 ? UI_SW_SHOW : UI_SW_HIDE );
	pVal->ShowWindow( nTime != 0 ? UI_SW_SHOW : UI_SW_HIDE  );
	SetUIWindowText( pVal, NStr::ToUnicode( NStr::Format( "%d:%02d", nTime/60, nTime%60 ) ) );
}
void CInterfaceMission::CMultiplayerScoresSmall::CReplayScoresState::OnTime( const int nTime )
{
	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_REPLAY_SS_TIME_VAL ) );
	SetUIWindowText( pVal, NStr::ToUnicode( NStr::Format( "%d:%02d", nTime/60, nTime%60 ) ) );
}
void CInterfaceMission::CMultiplayerScoresSmall::CReplayScoresState::Init( IUIScreen *pUIScreen )
{
	if ( !bInitted )
	{
		pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_DIALOG_REPLAY_SS ) );
		pTimer = GetSingleton<IGameTimer>();
		bInitted = true;
	}
}
void CInterfaceMission::CMultiplayerScoresSmall::CGameScoresState::Show( IUIScreen * pUIScreen )
{
	Init( pUIScreen );
	IUIElement *pSingleObjectiveButton = checked_cast<IUIElement*>( pUIScreen->GetChildByID( E_SINGLE_OBJECTIVE_BUTTON ) );
	pSingleObjectiveButton->ShowWindow( UI_SW_HIDE );

	pDialog->ShowWindow( UI_SW_SHOW );

	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_TIME ) );
	IUIStatic * pName = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_TIME_VAL) );
	pName->ShowWindow( UI_SW_SHOW );
	pVal->ShowWindow( UI_SW_SHOW );
	
	if ( GetGlobalVar( "MOD.Active", 0 ) )
	{
		IUIStatic *pModName = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_MOD_NAME ) );
		IUIStatic *pModVersion = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_MOD_VERSION ) );
		SetUIWindowText( pModName, NStr::ToUnicode( GetGlobalVar( "MOD.Name", "" ) ) );
		SetUIWindowText( pModVersion, NStr::ToUnicode( GetGlobalVar( "MOD.Version", "" ) ) );
		pModName->ShowWindow( UI_SW_SHOW );
		pModVersion->ShowWindow( UI_SW_SHOW );
	}
}
void CInterfaceMission::CMultiplayerScoresSmall::CGameScoresState::OnFrags( const int nFrags, const int nPlayer )
{
	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_TEAMFRAGS_VAL ) );
	SetUIWindowText( pVal, NStr::ToUnicode( NStr::Format( "%d", nFrags ) ) );
	IUIStatic * pName = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_TEAMFRAGS ) );
	pName->ShowWindow( UI_SW_SHOW );
	pVal->ShowWindow( UI_SW_SHOW );
}
void CInterfaceMission::CMultiplayerScoresSmall::CGameScoresState::OnFlags( const int nFlags, const int nPlayer )
{
	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_TEAMFLAGS_VAL ) );
	SetUIWindowText( pVal, NStr::ToUnicode( NStr::Format( "%d", nFlags ) ) );
	IUIStatic * pName = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_TEAMFLAGS ) );
	pName->ShowWindow( UI_SW_SHOW );
	pVal->ShowWindow( UI_SW_SHOW );
}
void CInterfaceMission::CMultiplayerScoresSmall::CGameScoresState::Init( IUIScreen *pUIScreen )
{
	if ( !bInitted )
	{
		pDialog = checked_cast<IUIDialog*>( pUIScreen->GetChildByID( E_DIALOG_MULTIPLAYER_SCORES_SMALL ) );
		pTimer = GetSingleton<IGameTimer>();
		bInitted = true;
	}
}
void CInterfaceMission::CMultiplayerScoresSmall::CGameScoresState::OnTimeBeforeCapture( const int nTime )
{
	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_TIMEBEFORECAPTURE_VAL ) );
	IUIStatic * pName = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_TIMEBEFORECAPTURE ) );
	pName->ShowWindow( nTime != 0 ? UI_SW_SHOW : UI_SW_HIDE );
	pVal->ShowWindow( nTime != 0 ? UI_SW_SHOW : UI_SW_HIDE  );
	const std::wstring szTimeBeforeCapture = NStr::ToUnicode( NStr::Format( "%d:%02d", nTime/60, nTime%60 ) );
	SetUIWindowText( pVal, szTimeBeforeCapture );
}
void CInterfaceMission::CMultiplayerScoresSmall::CGameScoresState::OnTime( const int nTime )
{
	IUIStatic * pVal = checked_cast<IUIStatic*>( pDialog->GetChildByID( E_MULTIPLAYER_SCORES_SMALL_TIME_VAL ) );
	const std::wstring szTime = NStr::ToUnicode( NStr::Format( "%d:%02d", nTime/60, nTime%60 ) );
	SetUIWindowText( pVal, szTime );
}
void CInterfaceMission::CMultiplayerScoresSmall::CScoresState::ProcessMessage( const SGameMessage &msg, IUIScreen * pUIScreen )
{ 
	if ( !bVisible )
	{
		Show( pUIScreen );
		bVisible = true;
	}

	switch( msg.nEventID )
	{
	case MC_UPDATE_TEAM_F_R_AGS:
		{
			const int nPartyID = msg.nParam & ~(0xffffffff<<2);
			const int nPoints = msg.nParam >> 2;
			OnFrags( nPoints, nPartyID );
		}		

		break;
	case MC_UPDATE_TEAM_F_L_AGS:
		{
			const int nPartyID = msg.nParam & ~(0xffffffff<<2);
			const int nPoints = msg.nParam >> 2;
			OnFlags( nPoints, nPartyID );
		}

		break;
	case MC_UPDATE_TIME_BEFORE_CAPTURE:
		timeBeforeCapture = msg.nParam * 1000 + GetSingleton<IGameTimer>()->GetGameTime();
		OnTimeBeforeCapture( msg.nParam );

		break;
	}
}
void CInterfaceMission::CMultiplayerScoresSmall::CScoresState::StepLocal( IUIScreen * pUIScreen )
{
	if ( !bVisible ) return;

	const NTimer::STime curTime = GetSingleton<ITransceiver>()->GetMultiplayerTime();
	
	const NTimer::STime curGameTime = GetSingleton<IGameTimer>()->GetGameTime();
	const NTimer::STime timeToCapture = timeBeforeCapture >= curGameTime ? timeBeforeCapture - curGameTime : 0 ;
	
	const int nSeconds = timeToCapture / 1000;
	
	if ( timeBeforeCapture != -1 && nSeconds != nSecondsBeforeCapture )
	{
		nSecondsBeforeCapture = nSeconds;
		OnTimeBeforeCapture( nSecondsBeforeCapture );
	}
	
	const unsigned long int nCurGameTime = curTime / 1000;
	if ( nCurGameTime != nGameTime )
	{
		nGameTime = nCurGameTime;
		OnTime( nGameTime );
	}
}
void CInterfaceMission::CMultiplayerScoresSmall::ProcessMessage( const SGameMessage &msg, IUIScreen * pUIScreen )
{
	if ( pState )
		pState->ProcessMessage( msg, pUIScreen );
}
void CInterfaceMission::CMultiplayerScoresSmall::Done()
{
	pState = 0;
}
void CInterfaceMission::CMultiplayerScoresSmall::Init()
{
	if ( GetGlobalVar( "MultiplayerGame", 0 ) )
	{
			pState = new CReplayScoresState;
	}
}
void CInterfaceMission::CMultiplayerScoresSmall::StepLocal( IUIScreen * pUIScreen )
{
	if ( pState )
		pState->StepLocal( pUIScreen );
}
CInterfaceMission::CInterfaceMission()
: CInterfaceScreenBase( "Mission" ), 
  vCameraStartPos( VNULL3 ), pSelectedObject( 0 ), bEditMode( false ), 
  nDirection( 0 )
{
	nStartPauseCounter = 0;
	bCycledLaunch = false;
	nFPSAveragePeriod = 5000;
}
CInterfaceMission::~CInterfaceMission() 
{
	pFrameSelection = 0;
	pWorld = 0;
	if ( pScene )
	{
		pScene->RemoveUIScreen( pUIScreen );
		pScene->Clear();
	}
	if ( pAckManager )
	{
		pAckManager->Clear();
		pAckManager = 0;
	}
	if ( pAILogic )
	{
		pAILogic->Clear();
		pAILogic->Suspend();
	}
}
int CInterfaceMission::CMultiplayerScoresSmall::CReplayScoresState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CScoresState*>(this) );

	return 0;
}
int CInterfaceMission::CMultiplayerScoresSmall::CGameScoresState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CScoresState*>(this) );

	return 0;
}
int CInterfaceMission::CMultiplayerScoresSmall::CScoresState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &timeBeforeCapture );
	saver.Add( 2, &nSecondsBeforeCapture );
	saver.Add( 3, &nGameTime );
	saver.Add( 4, &bVisible );
	saver.Add( 5, &pDialog );
	if ( saver.IsReading() )
	{
		pTimer = GetSingleton<IGameTimer>();
	}
	return 0;
}
int CInterfaceMission::CMultiplayerScoresSmall::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 9, &pState );

	return 0;
}
int CInterfaceMission::CTimeoutDialog::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	return 0;
}
int CInterfaceMission::CPlayerLaggedDialog::CDialogState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &bShowedWindow );
	return 0;
}
int CInterfaceMission::CPlayerLaggedDialog::CDialogStateLagged::operator&( IStructureSaver &ss )
{	
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CDialogState*>(this) );
	saver.Add( 3, &players );
	return 0;
}
int CInterfaceMission::CPlayerLaggedDialog::CDialogStateLoading::operator&( IStructureSaver &ss )
{	
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CDialogState*>(this) );
	return 0;
}
int CInterfaceMission::CPlayerLaggedDialog::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 8, &pState);
	return 0;
}
int CInterfaceMission::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CInterfaceScreenBase*>(this) );
	saver.Add( 2, &pWorld );
	saver.Add( 3, &pUIScreen );
	saver.Add( 4, &vCameraStartPos );
	saver.Add( 5, &nDirection );
	saver.Add( 6, &vLastAnchor );
	saver.Add( 8, &pTextPause );
	saver.Add( 10, &szCurrMapName );
	saver.Add( 11, &bCycledLaunch );
	saver.Add( 12, &multiplayerScoresSmall );
	saver.Add( 13, &laggedDialog );
	saver.Add( 14, &timeoutDialog );
	
	if ( saver.IsReading() )
	{
		pWorld->Init( GetSingletonGlobal() );
		preselectedObjects.clear();
		selectedObjects.clear();
		itCurrSelected = selectedObjects.end();
		pSelectedObject = 0;
		nStartPauseCounter = 0;
		pAckManager = GetSingleton<IClientAckManager>();
		nFPSAveragePeriod = GetGlobalVar( "Word.FPSAveragePeriod", 5000 );
		ResetGameSpeedAdjusting();
	}
	return 0;
}
bool CInterfaceMission::Init()
{
	CInterfaceScreenBase::Init();

	GetSingleton<IMessageLinkContainer>()->SetInterface( this );

	while ( pScene->ToggleShow(SCENE_SHOW_HAZE) == false );

	multiplayerScoresSmall.Init();

	pAILogic = GetSingleton<IAILogic>();

	pAckManager = GetSingleton<IClientAckManager>();
	pFrameSelection = pScene->GetFrameSelection();
	missionMsgs.Init( pInput, missionCommands );
	CTRect<long> rcScreen = pGFX->GetScreenRect();
	const float fGameplayCameraHeight = float( rcScreen.Height() );
	pCamera->SetPlacement( CVec3(0, 0, 0), 1024*4 + fGameplayCameraHeight, -ToRadian(90.0f + 30.0f), ToRadian(45.0f) );
	nFPSAveragePeriod = GetGlobalVar( "Word.FPSAveragePeriod", 5000 );
	SetBindSection( "game_mission" );
	pAILogic->Resume();
	
  return true;
}
void CInterfaceMission::Done()
{
	GetSingleton<IMessageLinkContainer>()->SetInterface( 0 );
	GetSingleton<IGlobalVars>()->RemoveVarsByMatch( "temp." );	
	pFrameSelection = 0;
	pWorld = 0;
	NI_ASSERT_T( pScene != 0, "pScene is 0, information will not clear" );
	if ( pScene )
	{
		pScene->SetSoundSceneMode( ESSM_INTERMISSION_INTERFACE );
		pScene->RemoveUIScreen( pUIScreen );
		pScene->Clear();
		pScene->SetTerrain( 0 );
	}
	if ( pAckManager )
	{
		pAckManager->Clear();
		pAckManager = 0;
	}
	if ( pAILogic )
	{
		pAILogic->Clear();
		pAILogic->Suspend();
	}
	RemoveGlobalVar( "AreWeInMission" );
	GetSingleton<IMainLoop>()->ClearResources( true );
	multiplayerScoresSmall.Done();

/*
	int nMulty = GetGlobalVar( "MultiplayerGame", 0 );
	if ( !nMulty )		//���� �� � ������ multyplayer
		GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d 0", MAIN_SP_TRANSCEIVER) );
*/
	CInterfaceScreenBase::Done();
}
bool CInterfaceMission::OpenCurtains() 
{ 
	return true; 
}
void CInterfaceMission::ConfigureInterfacePreferences()
{
	if ( GetSingleton<IUserProfile>()->GetVar( "Mission.UnitExtendedInfo.opened", 1 ) )
	{
		if ( pUIScreen->GetChildByID( 110 )->IsWindowEnabled() )
			GetSingleton<IInput>()->AddMessage( SGameMessage( MC_TOGGLE_UNIT_INFO ) );
	}
}	
void CInterfaceMission::CheckResolution()
{
	if ( !pUIScreen ) return;

	const int nSizeXAfter = GetGlobalVar( "GFX.Mode.Current.SizeX", GFX_DEFAULT_SCREEN_WIDTH );
	
	const bool bEnoughToAllControls = nSizeXAfter >= 800;

	if ( bEnoughToAllControls )
	{
		pUIScreen->GetChildByID( 110 )->ShowWindow( UI_SW_SHOW_DONT_MOVE_UP );
	}
	else
	{
		pUIScreen->GetChildByID( 40000 )->EnableWindow( false );
		pUIScreen->GetChildByID( 40000 )->ShowWindow( UI_SW_HIDE );

		pUIScreen->GetChildByID( 110 )->ShowWindow( UI_SW_HIDE );
		pUIScreen->GetChildByID( 110 )->SetState( 0 );
	}
}
void CInterfaceMission::OnGetFocus( bool bFocus )
{
	if ( bFocus )
	{
		const int nInterMissionSizeX = GetGlobalVar( "GFX.Mode.InterMission.SizeX", GFX_DEFAULT_SCREEN_WIDTH );
		const int nInterMissionSizeY = GetGlobalVar( "GFX.Mode.InterMission.SizeY", GFX_DEFAULT_SCREEN_HEIGHT );
		const int nInterMissionBPP = GetGlobalVar( "GFX.Mode.InterMission.BPP", 16 );
		const int nInterMissionFullScreen = GetGlobalVar( "GFX.Mode.InterMission.FullScreen", int(GFXFS_WINDOWED) );
		const int nInterMissionFrequency = GetGlobalVar( "GFX.Mode.InterMission.Frequency", 0 );
		SetGlobalVar( "GFX.Mode.Mission.SizeX", nInterMissionSizeX );
		SetGlobalVar( "GFX.Mode.Mission.SizeY", nInterMissionSizeY );
		SetGlobalVar( "GFX.Mode.Mission.BPP", nInterMissionBPP );
		SetGlobalVar( "GFX.Mode.Mission.FullScreen", nInterMissionFullScreen );
		SetGlobalVar( "GFX.Mode.Mission.Frequency", nInterMissionFrequency );
	}
 	CInterfaceScreenBase::OnGetFocus( bFocus );
	
	if ( bFocus ) 
	{
		pAILogic->Resume();
		CheckResolution();
		while ( pScene->ToggleShow(SCENE_SHOW_HAZE) == false );
	}
	ResetGameSpeedAdjusting();
}
struct STestBorder
{
	std::vector<std::string> fronts;
	std::vector<std::string> backs;
	float fB2F;														// back-to-front change | b2f <= front <= f2b
	float fF2B;														// front to back change | 
	float fFrontWidth;
	float fBackWidth;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "FrontElements", &fronts );
		saver.Add( "BackElements", &backs );
		saver.Add( "B2FChange", &fB2F );
		saver.Add( "F2BChange", &fF2B );
		saver.Add( "BackWidth", &fBackWidth );
		saver.Add( "FrontWidth", &fFrontWidth );
		if ( saver.IsReading() ) 
		{
			fB2F = fB2F / 360.0f * 65535.0f;
			fF2B = fF2B / 360.0f * 65535.0f;
		}
		return 0;
	}
};

struct SPolylineSample
{
	int nFirst, nLast;
	float fCoeff;
	SPolylineSample() : nFirst( 0 ), nLast( 1 ), fCoeff( 0 ) {  }
	const CVec3 GetPoint( const std::vector<CVec3> &points )
	{
		CVec3 vPos;
		vPos.Lerp( fCoeff, points[nFirst], points[nLast] );
		return vPos;
	}
};
struct SBorderSegment
{
	int nFrameIndex;
	CVec3 vPos;
	WORD wDir;
};
bool FindNextPoint( SPolylineSample *pRes, const SPolylineSample &curr, const float fLen, const std::vector<CVec3> &points )
{
	CVec3 vPos;
	vPos.Lerp( curr.fCoeff, points[curr.nFirst], points[curr.nLast] );
	int nNextIndex = curr.nLast;
	const float fLen2 = fabs2( fLen );
	while ( (fabs2(vPos - points[nNextIndex]) < fLen2) && (nNextIndex < points.size()) ) 
		++nNextIndex;
	if ( nNextIndex == points.size() ) 
		return false;
	const CVec3 vFrom = curr.nLast == nNextIndex ? vPos : points[nNextIndex - 1];
	const CVec3 vTo = points[nNextIndex];
	float fStartCoeff = curr.nLast == nNextIndex ? curr.fCoeff : 0;
	const CVec3 vVec = vTo - vFrom;
	float fCurrCoeff;
	for ( fCurrCoeff = fStartCoeff; fCurrCoeff < 1.0f; fCurrCoeff += 0.01f )
	{
		if ( fabs2(vFrom + fCurrCoeff*vVec) >= fLen2 )
			break;
	}
	pRes->nFirst = nNextIndex - 1;
	pRes->nLast = nNextIndex;
	pRes->fCoeff = fCurrCoeff;
	return true;
}
void AddBorder( const SVectorStripeObject &stripe, const int nSide, std::vector<SMapObjectInfo> &objects )
{
	if ( GetGlobalVar("border", 0) == 0 ) 
		return;
	STestBorder borders;
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "borders.xml", STREAM_ACCESS_READ );
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
		saver.AddTypedSuper( &borders );
	}
	std::vector<CVec3> points( stripe.points.size() );
	WORD wAngleAdd = 0;
	if ( nSide == 0 ) 
	{
		wAngleAdd = 32768;
		for ( int i = 0; i < stripe.points.size(); ++i )
			points[i] = stripe.points[i].vPos + stripe.points[i].vNorm*stripe.points[i].fWidth;
	}
	else
	{
		for ( int i = 0; i < stripe.points.size(); ++i )
			points[i] = stripe.points[i].vPos - stripe.points[i].vNorm*stripe.points[i].fWidth;
	}
	SPolylineSample curr, next;
	while ( FindNextPoint(&next, curr, borders.fFrontWidth*fWorldCellSize/2.0f, points) ) 
	{
		const CVec3 vFirst = curr.GetPoint( points );
		const CVec3 vLast = next.GetPoint( points );
		CVec3 vNorm( -(vLast.y - vFirst.y), vLast.x - vFirst.x, 0 );
		::Normalize( &vNorm );
		float fAngle = acos( Clamp( vNorm * CVec3( 0, 1, 0 ), -1.0f, 1.0f ) );
		if ( vNorm.x > 0 ) 
			fAngle = FP_2PI - fAngle;
		WORD wDir = fAngle / FP_2PI * 65535.0f;
		wDir += wAngleAdd;
		CVec3 vCenter = ( vFirst + vLast ) / 2.0f;
		Vis2AI( &vCenter );
		std::string szName;
		if ( (wDir > borders.fB2F) && (wDir < borders.fF2B) ) 
			szName = borders.fronts[rand() % borders.fronts.size()];
		else
			szName = borders.backs[rand() % borders.backs.size()];
		SMapObjectInfo obj;
		obj.szName = szName;
		obj.vPos = vCenter;
		obj.nDir = wDir;
		obj.fHP = 1;
		obj.link.nLinkID = 0;
		objects.push_back( obj );
		curr = next;
	}
}
bool CInterfaceMission::NewMission( const std::string &_szMapName, bool _bCycledLaunch )
{
	CPtr<IMovieProgressHook> pProgress = CreateObject<IMovieProgressHook>( MAIN_PROGRESS_INDICATOR );
	pProgress->Init( IMovieProgressHook::PT_NEWMISSION );
	pProgress->SetNumSteps( 7 + AI_INIT_PROGRESS_STEPS );
	pProgress->SetCurrPos( 1 );
	RemoveGlobalVar( "Mission.Current.StartTime" );
	RemoveGlobalVar( "Mission.Current.HasObjectivesToShow" );
	RemoveGlobalVar( "Mission.Current.HasActiveObjective" );
	const std::string szMissionName = GetGlobalVar("Mission.Current.Name", "None" );
	const std::string szVarName = "Mission." + szMissionName + ".Random.Generated";
	RemoveGlobalVar( szVarName.c_str() );
	NStr::DebugTrace( "CInterfaceMission::NewMission(), remove Template Mission: %s\n", szMissionName.c_str() );
	
	GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_MP_NO_SEGMENT_DATA );
	GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_MP_LAGG );
	GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_MP_TIMEOUT );
	GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_MP_LOADING );

	SetGlobalVar( "AreWeInMission", 1 );
	GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_MENU );
	GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_USER_PAUSE );
	GetSingleton<ISFX>()->Pause( false );
	GetSingleton<ISFX>()->PauseStreaming( false );
	pTimer->SetSpeed( 0 );
	pTimer->GetGameSegmentTimer()->Set( pTimer->GetGameTime() );
	GetSingleton<IMainLoop>()->Pause( true, PAUSE_TYPE_PREMISSION );
	szCurrMapName = _szMapName;
	bCycledLaunch = _bCycledLaunch;
	pTextPause = CreateObject<IGFXText>( GFX_TEXT );
	pTextPause->SetText( GetSingleton<ITextManager>()->GetString("pause") );
	pTextPause->SetFont( GetSingleton<IFontManager>()->GetFont("fonts\\large") );

	pProgress->Step(); //2
	
	{
		pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
		pUIScreen->Load( "ui\\mission" );
		pUIScreen->Reposition( pGFX->GetScreenRect() );
		pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );

		const int nTutorialMode = GetGlobalVar( "TutorialMode", 0 );
		if ( nTutorialMode )
		{
			IUIElement *pButton = pUIScreen->GetChildByID( 6001 );
			NI_ASSERT_T( pButton != 0, "There is no control with ID 6001 (show single objective button)" );
			pButton->ShowWindow( UI_SW_SHOW );
		}
		if ( !GetGlobalVar( "MultiplayerGame", 0 ) )
		{
			IUIElement *pElement = pUIScreen->GetChildByID( 12002 );
			if ( pElement )
				pElement->EnableWindow( GetGlobalVar( "MultiplayerGame", 0  ) );
		}
	}
	pProgress->Step(); //3
	
	const std::string szMapName = _szMapName.substr( 0, _szMapName.rfind( '.' ) );
	const std::string szTerrainName = std::string( "maps\\" ) + _szMapName.substr( 0, _szMapName.rfind( '.' ) );
	CMapInfo mapinfo;
	CPtr<IDataStream> pMapStream;
	RemoveGlobalVar( "nogeneral_script" );
	try
	{
		IDataStorage *pStorage = GetSingleton<IDataStorage>();
		SStorageElementStats statsXML, statsBZM;
		Zero( statsXML );
		pStorage->GetStreamStats( (szTerrainName + ".xml").c_str(), &statsXML );
		Zero( statsBZM );
		pStorage->GetStreamStats( (szTerrainName + ".bzm").c_str(), &statsBZM );
		NI_ASSERT_T( (statsXML.mtime != 0) || (statsBZM.mtime != 0), NStr::Format("Can't find neither XML nor BZM file with map \"%s\"", szTerrainName.c_str()) );
		
		if ( statsXML.mtime > statsBZM.mtime ) 
		{
			pMapStream = pStorage->OpenStream( (szTerrainName + ".xml").c_str(), STREAM_ACCESS_READ );
			NI_ASSERT_T( pMapStream != 0, NStr::Format("Can't open XML stream with map \"%s\"", szTerrainName.c_str()) );
			CTreeAccessor saver = CreateDataTreeSaver( pMapStream, IDataTree::READ );
			saver.AddTypedSuper( &mapinfo );
		}
		else
		{
			pMapStream = pStorage->OpenStream( (szTerrainName + ".bzm").c_str(), STREAM_ACCESS_READ );
			NI_ASSERT_T( pMapStream != 0, NStr::Format("Can't open BZM stream with map \"%s\"", szTerrainName.c_str()) );
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pMapStream, IStructureSaver::READ );
			CSaverAccessor saver = pSaver;
			saver.Add( 1, &mapinfo );
		}
		if ( !mapinfo.IsValid() ) 
			throw 0;
	}
	catch ( ... )
	{
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MAIN_MENU, 0 );
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("Invalid or corrupted map file \"%s\"", szTerrainName.c_str()), 0xffff0000, true );
		return false;
	}
	for ( int i = 0; i < mapinfo.diplomacies.size(); ++i )
	{
		NStr::DebugTrace( "CInterfaceMission::NewMission(), player: %d, diplomacy %d\n", i, mapinfo.diplomacies[i] );
	}
	mapinfo.UnpackFrameIndices();
	if ( GetGlobalVar("MultiplayerGame", 0) == 0 ) 
	{
		for ( CPtr<IPlayerScenarioInfoIterator> pIt = GetSingleton<IScenarioTracker>()->CreatePlayerScenarioInfoIterator(); !pIt->IsEnd(); pIt->Next() )
		{
			const int nPlayerID = pIt->GetID();
			std::string szPartyName = "Neutral";
			if ( nPlayerID < mapinfo.unitCreation.units.size() ) 
			{
				if ( !mapinfo.unitCreation.units[nPlayerID].szPartyName.empty() ) 
					szPartyName = mapinfo.unitCreation.units[nPlayerID].szPartyName;
				else
				{
					const std::string szError = NStr::Format( "Player party for player %d (total = %d players in UnitCreation) was not set for map \"%s\"", nPlayerID, mapinfo.unitCreation.units.size(), _szMapName.c_str() );
					GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, szError.c_str(), 0xffff0000, true );
				}
			}
			NStr::ToLower( szPartyName );
			pIt->Get()->SetSide( szPartyName );
		}
	}
	if ( GetGlobalVar("border", 0) != 0 ) 
	{
		for ( int i = 0; i < mapinfo.terrain.rivers.size(); ++i )
		{
			AddBorder( mapinfo.terrain.rivers[i], 0, mapinfo.objects );
			AddBorder( mapinfo.terrain.rivers[i], 1, mapinfo.objects );
		}
	}
	{
		std::string szScriptName;
		const int nMapNamePos = szTerrainName.rfind( '\\' );
		if ( nMapNamePos != std::string::npos ) 
			szScriptName = szTerrainName.substr( 0, nMapNamePos + 1 );
		const int nScriptNamePos = mapinfo.szScriptFile.rfind( '\\' );
		if ( nScriptNamePos != std::string::npos ) 
			szScriptName += mapinfo.szScriptFile.substr( nScriptNamePos + 1 );
		else
			szScriptName += mapinfo.szScriptFile;
		mapinfo.szScriptFile = szScriptName;
	}
	{
		const std::string szMissionName = GetGlobalVar( "Mission.Current.Name", "" );
		if ( !szMissionName.empty() )
		{
			if ( const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>(szMissionName.c_str(), IObjectsDB::MISSION) )
			{
				if ( pStats->IsTemplate() )
				{
					const std::string szVarName = NStr::Format( "Mission.Random.Usage.%s", szMissionName.c_str() );
					const int nNumUsage = GetGlobalVar( szVarName.c_str(), 0 );
					SetGlobalVar( szVarName.c_str(), nNumUsage + 1 );
				}
			}
		}
	}

	pProgress->Step(); //4

	GetSingleton<IScenarioTracker>()->StartMission( GetGlobalVar("Mission.Current.Name", "UNKNOWN") );

	mapinfo.AddSounds( &( mapinfo.soundsList ), CMapInfo::SOUND_TYPE_BITS_RIVERS );
	
	vCameraStartPos = mapinfo.vCameraAnchor;
	{
		const int nUserPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayerID();
		if ( nUserPlayer < mapinfo.playersCameraAnchors.size() ) 
			vCameraStartPos = mapinfo.playersCameraAnchors[nUserPlayer];
		if ( vCameraStartPos == VNULL3 ) 
			vCameraStartPos = mapinfo.vCameraAnchor;
	}
	pCamera->SetAnchor( vCameraStartPos );
	pCamera->SetBounds( 0, 0, mapinfo.terrain.tiles.GetSizeX() * fWorldCellSize, mapinfo.terrain.tiles.GetSizeY() * fWorldCellSize );

	pProgress->Step(); //5
	pAILogic->Init( mapinfo, pProgress );
	pProgress->Step(); //11
	ITerrain *pTerrain = CreateTerrain();
	pTerrain->Load( szTerrainName.c_str(), mapinfo.terrain );
	GetSingleton<IScene>()->SetTerrain( pTerrain );
	CTRect<long> rcScreen = pGFX->GetScreenRect();
	pCursor->SetPos( rcScreen.Width()/2, rcScreen.Height()/2 );
	pAILogic->Resume();
	pWorld = CreateObject<IWorldClient>( MISSION_WORLD );
	pWorld->Init( GetSingletonGlobal() );
	pWorld->Start();
	pWorld->SetSeason( mapinfo.nSeason );
	preselectedObjects.clear();
	selectedObjects.clear();
	itCurrSelected = selectedObjects.end();
	pSelectedObject = 0;
	SetGlobalVar( "Mission.Current.StartTime", int(pTimer->GetGameTime()) );

	if ( pUIScreen ) 
	{
    if ( IUIMiniMap *pUIMiniMap = checked_cast<IUIMiniMap*>( pUIScreen->GetChildByID( 20000 ) ) )
    {
			pUIMiniMap->SetTerrainSize( pTerrain->GetSizeX(), pTerrain->GetSizeY(), mapinfo.diplomacies.size() ); 
			const std::string szTextureName = "maps\\" + szMapName;
			{
				SStorageElementStats mapStats, miniMapStats;
				Zero( mapStats );
				Zero( miniMapStats );
				pMapStream->GetStats( &mapStats	);
				GetSingleton<IDataStorage>()->GetStreamStats( ( szTextureName + GetDDSImageExtention( COMPRESSION_DXT ) ).c_str(), &miniMapStats );
				if ( mapStats.mtime > miniMapStats.mtime )
				{
					CRMImageCreateParameterList imageCreateParameterList;
					imageCreateParameterList.push_back( SRMImageCreateParameter( szTextureName, CTPoint<int>( 0x100, 0x100 ), true ) ); 
					mapinfo.CreateMiniMapImage( imageCreateParameterList );
				}
			}

			pUIMiniMap->SetBackgroundTexture( GetSingleton<ITextureManager>()->GetTexture( szTextureName.c_str() ) );
			std::string szMissionName = GetGlobalVar( "Mission.Current.Name" );
			const SMissionStats *pMissionStats = NGDB::GetGameStats<SMissionStats>( szMissionName.c_str(), IObjectsDB::MISSION );
			if ( pMissionStats != 0 )
			{
				NTimer::STime currentAbsTime = pTimer->GetAbsTime();
				for ( int nMissionObjectiveIndex = 0; nMissionObjectiveIndex < pMissionStats->objectives.size(); ++nMissionObjectiveIndex )
				{
					const SMissionStats::SObjective &rObjective = pMissionStats->objectives[nMissionObjectiveIndex];
					if ( !rObjective.bSecret )
					{
						if ( rObjective.vPosOnMap != VNULL2 )
						{
							CVec2 objectivePosition( rObjective.vPosOnMap.x / 512.0f, ( 512 - rObjective.vPosOnMap.y - 1 ) / 512.0f );
							pUIMiniMap->AddMarker( "ObjectiveReceived", objectivePosition, true, nMissionObjectiveIndex, currentAbsTime, 0, true );
						}
					}
				}
			}
    }
	}
	if ( mapinfo.soundsList.size() )
		pScene->InitMapSounds( &mapinfo.soundsList.front(), mapinfo.soundsList.size() );

	pScene->InitMusic( GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetGeneralSide() );
	RemoveGlobalVar( "Mission.Current.Bonus" );
	const std::string szChapterName = GetGlobalVar( "Chapter.Current.Name", "" );
	if ( !szChapterName.empty() ) 
	{
		const SChapterStats *pChapter = NGDB::GetGameStats<SChapterStats>( szChapterName.c_str(), IObjectsDB::CHAPTER );
		if ( pChapter ) 
		{
			const std::string szMissionName = GetGlobalVar( "Mission.Current.Name", "" );
			if ( !szMissionName.empty() ) 
			{
				for ( std::vector<SChapterStats::SMission>::const_iterator it = pChapter->missions.begin(); it != pChapter->missions.end(); ++it )
				{
					if ( it->szMission == szMissionName ) 
					{
						if ( !it->szMissionBonus.empty() ) 
						{
							if ( GetSingleton<IObjectsDB>()->GetDesc(it->szMissionBonus.c_str()) != 0 )
								SetGlobalVar( "Mission.Current.Bonus", it->szMissionBonus.c_str() );
							break;
						}
					}
				}
			}
		}
	}
	if ( pUIScreen ) 
	{
		pScene->AddUIScreen( pUIScreen );
		pScene->SetMissionScreen( pUIScreen ); 
	}
	pScene->SetSoundSceneMode( ESSM_INGAME );
	pProgress->Step(); //12
	pProgress->Stop();
	if ( GetGlobalVar("notransition", 0) == 0 && GetGlobalVar( "MultiplayerGame", 0 ) == 0 )
	{
		ISceneObject *pObj = CreateObject<ISceneObject>( SCENE_GAMMA_FADER );
		GetSingleton<IScene>()->AddSceneObject( pObj );
	}
	ResetGameSpeedAdjusting();
	GetSingleton<IScenarioTracker>()->UpdateMinimumDifficulty();
	return true;
}
bool CInterfaceMission::StepLocal( bool bAppActive )
{
	if ( !bAppActive )
		return false;

	const bool bInterfaceActive = pScene->GetMissionScreen() == pScene->GetUIScreen();
	++nStartPauseCounter;
	if ( nStartPauseCounter == 2 )
		GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_PREMISSION );
	if ( const int nDelay = GetGlobalVar("delay", 0) ) 
		Sleep( nDelay );
	NTimer::STime currentGameTime = pTimer->GetGameTime();
	if ( bInterfaceActive )
	{
		RECT rcScreen = pGFX->GetScreenRect();
		CVec2 vCursor = pCursor->GetPos();
		CVec2 vScroll( 0, 0 );
		if ( vCursor.x < 1 )
			vScroll.x = -1;
		else if ( vCursor.x > rcScreen.right - rcScreen.left - 2 )
			vScroll.x = 1;
		if ( vCursor.y < 1 )
			vScroll.y = 1;
		else if ( vCursor.y > rcScreen.bottom - rcScreen.top - 2 )
			vScroll.y = -1;
		pCamera->SetScrollSpeedX( vScroll.x );
		pCamera->SetScrollSpeedY( vScroll.y );
	}
	if ( bInterfaceActive )
	{
		CVec3 pos3;
		pScene->GetPos3( &pos3, pCursor->GetPos() );
		pFrameSelection->Update( pos3 );
		if ( pFrameSelection->IsActive() ) 
		{
			CVec2 vBegin;
			pScene->GetPos2( &vBegin, pFrameSelection->GetBeginPoint() );
			CVec2 vEnd;
			pScene->GetPos2( &vEnd, pFrameSelection->GetEndPoint() );
			CTRect<float> rcRect( vBegin, vEnd );
			rcRect.Normalize();
			if ( !rcRect.IsEmpty() )
			{
				ResetPreSelection();
				PreSelectObjects( rcRect, SGVOGT_UNIT );
			}
		}
	}
	if ( bInterfaceActive )
		OnCursorMove( pCursor->GetPos() );
	if ( pUIScreen )
	{
		IUIElement *pElement = pUIScreen->GetChildByID( 20000 );
		if ( pElement )
		{
			IUIMiniMap *pMiniMap = checked_cast<IUIMiniMap*>( pElement );
			BYTE *pVisInfo = 0;
			int nNumObjects = 0;
			pAILogic->GetMiniMapInfo( &pVisInfo, &nNumObjects );
			pMiniMap->AddWarFogData( pVisInfo, nNumObjects );
			struct SMiniMapUnitInfo *pObjects = 0;
			pAILogic->GetMiniMapInfo( &pObjects, &nNumObjects );
			pMiniMap->AddUnitsData( pObjects, nNumObjects );
			{
				SShootAreas *pObjects = 0;
				int nNumObjects = 0;
				pScene->GetAreas( &pObjects, &nNumObjects );
				pMiniMap->AddFireRangeAreas( pObjects, nNumObjects );
			}
			NTimer::STime currentAbsTime = pTimer->GetAbsTime();
			
			{
				CCircle *pObjects = 0;
				int nNumObjects = 0;
				pAILogic->GetRevealCircles( &pObjects, &nNumObjects );
				for ( int i = 0; i != nNumObjects; ++i )
				{
					CVec3 vCenter;
					AI2Vis( &vCenter, pObjects[i].center.x, pObjects[i].center.y, 0 );
					const float fRadius = pObjects[i].r * fAITileXCoeff;
					pMiniMap->AddCircle( CVec2( vCenter.x, vCenter.y ), fRadius, MMC_STYLE_DIVERGENT, 0xFF80, currentAbsTime, 2000, false, 0 );
				}
			}
			pWorld->GetAviationCircles( pMiniMap, currentAbsTime );
		}
	}
	multiplayerScoresSmall.StepLocal( pUIScreen );

	laggedDialog.StepLocal( pUIScreen );
	timeoutDialog.StepLocal( pUIScreen );

	if ( pUIScreen ) 
		pUIScreen->Update( pTimer->GetAbsTime() );
	pWorld->Update( currentGameTime );
	if ( pSelectedObject && bEditMode )
	{
		CVec2 pos = pCursor->GetPos();
		pos += pSelectedObject->second;
		CVec3 vPos3;
		GetPos3( &vPos3, pos );
		pWorld->MoveObject( pSelectedObject->first, vPos3 );
	} 
	if ( GetGlobalVar("MultiplayerGame", 0) == 0 ) 
	{
		const DWORD timeFrameCurrTime = timeGetTime();
		const NTimer::STime timeGameTime = pTimer->GetGameTime();
		if ( timeFrameLastTime == 0 ) 
		{
			timeFrameLastTime = timeFrameCurrTime;
			timeFrameLastGameTime = timeGameTime;
		}
		if ( timeFrameLastTime < timeFrameCurrTime ) 
		{
			timePeriodTime += timeFrameCurrTime - timeFrameLastTime;
			++nPeriodCounter;
		}
		timeFrameLastTime = timeFrameCurrTime;
		if ( timePeriodTime >= nFPSAveragePeriod ) 
		{
			const float fAveFPS = 1000.0f * float( nPeriodCounter ) / float( timePeriodTime );
			timePeriodTime = 0;
			nPeriodCounter = 0;
			float fMinFPS = GetGlobalVar( "Options.GFX.MinFPS", 2.0f );
			if ( fMinFPS != 0 ) 
			{
				fMinFPS = Clamp( fMinFPS, 2.0f, 20.0f );
				const NTimer::STime timeDiff = nFPSAveragePeriod / 10;
				if ( (fAveFPS < fMinFPS) && (timeGameTime > timeFrameLastGameTime) && (timeGameTime - timeFrameLastGameTime > timeDiff) ) 
				{
					IGameTimer *pTimer = GetSingleton<IGameTimer>();
					const int nOldSpeed = pTimer->GetSpeed();
					pTimer->SetSpeed( pTimer->GetSpeed() - 1 );
					const int nNewSpeed = pTimer->GetSpeed();
					if ( nNewSpeed < nOldSpeed )
					{
						IText *pText = GetSingleton<ITextManager>()->GetDialog( "textes\\gamespeedlowered" );
						if ( pText )
						{
						const std::wstring szString = MakeWideStringFromWordString( pText->GetString() ) + L" " + NStr::ToUnicode( NStr::Format("%+d", nNewSpeed) );
							GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_CHAT, szString.c_str(), 0xffff0000 );
						}
					}
				}
			}
			timeFrameLastGameTime = timeGameTime;
		}
	}
	return true;
}
bool CInterfaceMission::OnCursorMove( const CVec2 &vPos )
{
	CInterfaceScreenBase::OnCursorMove( vPos );
	pWorld->OnMouseMove( pCursor->GetPos(), pUIScreen != 0 ? pUIScreen->PickElement(vPos, 1000000000) : 0 );
	return true;
}
void CInterfaceMission::DrawAdd()
{
	if ( pTimer->GetPauseReason() == 0 )
	{
		pGFX->SetupDirectTransform();
		CTRect<long> rcScreen = pGFX->GetScreenRect();
		const int nH = rcScreen.Height() / 2;
		const int nTextHalfHeight = pTextPause->GetLineSpace() / 2;
		rcScreen.Set( rcScreen.x1, nH - nTextHalfHeight - 4, rcScreen.x2, nH + nTextHalfHeight + 4 );
		rcScreen.Move( 2, 2 );
		pTextPause->SetColor( 0xff000000 );
		pGFX->DrawText( pTextPause, rcScreen, 1, FNT_FORMAT_CENTER );
		rcScreen.Move( -2, -2 );
		const std::string szSeasonName = GetGlobalVar( "World.Season" );
		const DWORD dwColor = GetGlobalVar( ("Scene.Colors." + szSeasonName + ".Text.Default.Color").c_str(), int(0xffffffff) );
		pTextPause->SetColor( dwColor );
		pGFX->DrawText( pTextPause, rcScreen, 1, FNT_FORMAT_CENTER );
		pGFX->RestoreTransform();
	}
}
bool CInterfaceMission::ProcessMessage( const SGameMessage &msg )
{
	pWorld->ProcessMessage( msg );

	SGameMessage localmsg;
	while ( pWorld->GetMessage(&localmsg) ) 
		ProcessMessageLocal( localmsg );
	return true;
}
void SetChildWindowText( IUIElement *pParent, const int nID, const wchar_t *pszText )
{
	IUIContainer *pContainer = checked_cast<IUIContainer*>( pParent );
	if ( IUIElement *pElement = pContainer->GetChildByID(nID) )
		pElement->SetWindowText( 0, reinterpret_cast<const WORD*>( pszText ) );
}
void SetChildWindowText( IUIElement *pParent, const int nID, const std::wstring &szText )
{
	SetChildWindowText( pParent, nID, szText.c_str() );
}
void SetUIWindowText( IUIElement *pElement, const wchar_t *pszText )
{
	pElement->SetWindowText( 0, reinterpret_cast<const WORD*>( pszText ) );
}
void SetUIWindowText( IUIElement *pElement, const std::wstring &szText )
{
	SetUIWindowText( pElement, szText.c_str() );
}
void SetHelpContext( IUIElement *pElement, const wchar_t *pszText )
{
	pElement->SetHelpContext( 0, reinterpret_cast<const WORD*>( pszText ) );
}
void SetHelpContext( IUIElement *pElement, const std::wstring &szText )
{
	SetHelpContext( pElement, szText.c_str() );
}
void OutputString( IUIStatusBar *pBar, const int nIndex, const wchar_t *pszText )
{
	pBar->OutputString( nIndex, reinterpret_cast<const WORD*>( pszText ) );
}
void OutputString( IUIStatusBar *pBar, const int nIndex, const std::wstring &szText )
{
	OutputString( pBar, nIndex, szText.c_str() );
}
void SetUnitProperty( IUIStatusBar *pBar, const int nIndex, const int nLevel, const wchar_t *pszText )
{
	pBar->SetUnitProperty( nIndex, nLevel, reinterpret_cast<const WORD*>( pszText ) );
}
void SetUnitProperty( IUIStatusBar *pBar, const int nIndex, const int nLevel, const std::wstring &szText )
{
	SetUnitProperty( pBar, nIndex, nLevel, szText.c_str() );
}
const SMissionStatusObject* GetMissionStatusObject();
void CInterfaceMission::SetMissionStatusObject( bool bStatus )
{
	const SMissionStatusObject *pStatus = bStatus ? GetMissionStatusObject() : 0;

	if ( pUIScreen == 0 )
		return;
	if ( IUIElement *pElement = pUIScreen->GetChildByID(40000) )
	{
		IUIStatusBar *pBar = checked_cast<IUIStatusBar*>( pElement );
		IUIDialog *pDialog = checked_cast<IUIDialog *>( pUIScreen->GetChildByID( 5000 ) );
		if ( pStatus )
		{
			pBar->SetUnitIcons( pStatus->dwIconsStatus );
			int nExp = 0;
			int nExpNextLevel = 0;
			float fExperience = 0.0f;
			if ( pStatus->nScenarioIndex < 0 ) 
			{
				pBar->SetUnitProperty( 0, -1, 0 );
				OutputString( pBar, 1, L"" );
			}
			else if ( IScenarioUnit *pUnit = GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetUnit(pStatus->nScenarioIndex) ) 
			{
				const int nLevel = pUnit->GetValue( STUT_LEVEL );
				nExp = pUnit->GetValue( STUT_EXP );
				nExpNextLevel = pUnit->GetValue(STUT_EXP_NEXT_LEVEL );
				const int nPrevLevel = pUnit->GetValue( STUT_EXP_CURR_LEVEL );
				IText *pText = GetSingleton<ITextManager>()->GetDialog( NStr::Format("textes\\ui\\mission\\status\\tt_unit_level%d", nLevel) );
				std::wstring wToolTip = pText != 0 ? MakeWideStringFromWordString( pText->GetString() ) : L"";
				wToolTip += NStr::ToUnicode( NStr::Format("(%d / %d)", nExp, nExpNextLevel) );
				SetUnitProperty( pBar, 0, nLevel, wToolTip );
				if ( IText *pText = pUnit->GetName() ) 
					pBar->OutputString( 1, pText->GetString() );
				else
					OutputString( pBar, 1, L"" );
				if ( nPrevLevel == nExpNextLevel )
				{
					if ( nLevel == 0 )
						fExperience = 0.0f;
					else
						fExperience = 1.0f;
				}
				else
					fExperience = 1.0f * ( nExp ) / ( nExpNextLevel );
			}
			else
			{
				NI_ASSERT_T( false, NStr::Format("Can't find scenario unit %d", pStatus->nScenarioIndex) );
			}
			OutputString( pBar, 0, std::wstring( pStatus->pszName ) );
			for ( int i = 0; i < 4; ++i )
			{
				const int nTempID = 10 + i;
				IUINumberIndicator *pNumIndicator = checked_cast<IUINumberIndicator *> ( pDialog->GetChildByID( nTempID ) );
				NI_ASSERT_TF( pNumIndicator != 0, NStr::Format("Can't find minimap child by ID %d", nTempID), return );
				pNumIndicator->SetValue( pStatus->GetRelative( i, pStatus->params ) );
				const int nIconID = 20 + i;
				IUIElement *pIcon = pDialog->GetChildByID( nIconID );
				std::wstring szValue = MakeWideStringFromWordString( pNumIndicator->GetWindowText( 0 ) );
				szValue += NStr::ToUnicode( NStr::Format(  " %i/%i", pStatus->GetLow( i, pStatus->params ), pStatus->GetHigh( i, pStatus->params ) ) );
				SetHelpContext( pNumIndicator, szValue );	
				SetHelpContext( pIcon, szValue );	
			}

			const int nTempID = 10 + 3;
			IUINumberIndicator *pNumIndicator = checked_cast<IUINumberIndicator *> ( pDialog->GetChildByID( nTempID ) );
			pNumIndicator->SetValue( fExperience );

			const int nIconID = 20 + 3;
			IUIElement *pIcon = pDialog->GetChildByID( nIconID );
			std::wstring szValue = MakeWideStringFromWordString( pNumIndicator->GetWindowText( 0 ) );
			szValue += NStr::ToUnicode( NStr::Format(  " %i/%i", nExp, nExpNextLevel ) );
			SetHelpContext( pNumIndicator, szValue );	
			SetHelpContext( pIcon, szValue );	
		}
		else
		{
			OutputString( pBar, 0, L"" );
			OutputString( pBar, 1, L"" );
			for ( int i = 0; i < 4; ++i )
			{
				const int nTempID = 10 + i;
				IUINumberIndicator *pNumIndicator = checked_cast<IUINumberIndicator *> ( pDialog->GetChildByID( nTempID ) );
				NI_ASSERT_TF( pNumIndicator != 0, NStr::Format("Can't find minimap child by ID %d", nTempID), return );
				pNumIndicator->SetValue( 0 );
				const int nIconID = 20 + i;
				IUIElement *pIcon = pDialog->GetChildByID( nIconID );
				std::wstring szValue = MakeWideStringFromWordString( pNumIndicator->GetWindowText( 0 ) );
				SetHelpContext( pNumIndicator, szValue );	
				SetHelpContext( pIcon, szValue );	
			}
			pBar->SetUnitProperty( 0, -1, 0 );
			pBar->SetUnitIcons( 0 );
		}
	}
	IUIContainer *pDialog = checked_cast<IUIContainer *> ( pUIScreen->GetChildByID( 40000 ) );
	if ( pStatus )
	{
		IUIElement *pElement = pDialog->GetChildByID( 300 );
		if ( pElement )
			pElement->ShowWindow( UI_SW_SHOW );
		pElement = pDialog->GetChildByID( 310 );
		if ( pElement )
			pElement->ShowWindow( UI_SW_SHOW );

		static std::wstring szText;
		for ( int i = 0; i < 4; ++i )
		{
			NStr::ToUnicode( &szText, NStr::Format("%d", pStatus->armors[i]) );
			if ( IUIElement *pElement = pDialog->GetChildByID(101 + i) )
			{
				pElement->ShowWindow( UI_SW_SHOW );
				SetUIWindowText( pElement, szText );
			}
		}
		for ( int i = 0; i < 2 /*3*/; ++i )
		{
			NStr::ToUnicode( &szText, NStr::Format("%d", pStatus->weaponstats[i]) );
			if ( IUIElement *pElement = pDialog->GetChildByID(111 + i) )
			{
				pElement->ShowWindow( UI_SW_SHOW );
				SetUIWindowText( pElement, szText );
			}
		}
	}
	else
	{
		IUIElement *pElement = pDialog->GetChildByID( 300 );
		if ( pElement )
			pElement->ShowWindow( UI_SW_HIDE );
		pElement = pDialog->GetChildByID( 310 );
		if ( pElement )
			pElement->ShowWindow( UI_SW_HIDE );

		for ( int i = 0; i < 4; ++i )
		{
			if ( IUIElement *pElement = pDialog->GetChildByID(101 + i) )
				pElement->ShowWindow( UI_SW_HIDE );
		}
		for ( int i = 0; i < 2 /*3*/; ++i )
		{
			if ( IUIElement *pElement = pDialog->GetChildByID(111 + i) )
				pElement->ShowWindow( UI_SW_HIDE );
		}
	}
}
bool CInterfaceMission::ProcessMessageLocal( const SGameMessage &msg )
{
	if ( pTimer->GetPauseReason() > PAUSE_TYPE_NO_CONTROL ) 
	{
		if ( (msg.nEventID == CMD_BEGIN_ACTION1) || (msg.nEventID == CMD_END_ACTION1) || 
			   (msg.nEventID == CMD_BEGIN_ACTION2) || (msg.nEventID == CMD_END_ACTION2) ||
				 (msg.nEventID < 512) ) 
		{
			return true;
		}
	}

	if ( GetSingleton<IMessageLinkContainer>()->ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case MC_SHOW_GRID:
			pScene->ToggleShow( SCENE_SHOW_GRID );
			break;
		case MC_SHOW_OBJECTS:
			pScene->ToggleShow( SCENE_SHOW_OBJECTS );
			break;
		case MC_SHOW_UNITS:
			pScene->ToggleShow( SCENE_SHOW_UNITS );
			break;
		case MC_SHOW_BOUND_BOXES:
			pScene->ToggleShow( SCENE_SHOW_BBS );
			break;
		case MC_SHOW_TERRAIN:
			pScene->ToggleShow( SCENE_SHOW_TERRAIN );
			break;
		case MC_SHOW_SHADOWS:
			pScene->ToggleShow( SCENE_SHOW_SHADOWS );
			break;
		case MC_SHOW_EFFECTS:
			pScene->ToggleShow( SCENE_SHOW_EFFECTS );
			break;
		case MC_SHOW_WARFOG:
			GetSingleton<IAILogic>()->ToggleShow( 0 );
			break;
		case MC_SHOW_STATISTICS:
			ToggleShowStats();
			break;
		case MC_SHOW_DEPTH:
			pScene->ToggleShow( SCENE_SHOW_DEPTH_COMPLEXITY );
			break;
		case MC_SHOW_UI:
			pScene->ToggleShow( SCENE_SHOW_UI );
			break;
		case MC_SHOW_AVIA_BUTTONS:
			GetSingleton<IInput>()->AddMessage( SGameMessage( UI_NEXT_STATE_MESSAGE, 12003 ) );
			break;
		case MC_TOGGLE_UNIT_INFO:
			{
				IUIElement *pUnitInfo = pUIScreen->GetChildByID( 40000 );
				IUIElement *pButton = pUIScreen->GetChildByID( 110 );
				if ( pUnitInfo->IsWindowEnabled() && pButton->IsWindowEnabled() )
				{
					GetSingleton<IInput>()->AddMessage( SGameMessage( UI_NEXT_STATE_MESSAGE, 110 ) );
				}
			}
			
			break;
		case MC_MAKE_MAP_SHOT:
			MakeMapShot();
			break;
			
		case MC_ENTER_INGAME_EDITOR:
			pInput->SetBindSection( "ingame_editor" );
			bEditMode = true;
			break;
		case MC_LEAVE_INGAME_EDITOR:
			pInput->SetBindSection( "game_mission" );
			bEditMode = false;
			break;
		case MC_SELECT_FIRST_OBJECT:
			SelectFirstObject( pCursor->GetPos() );
			break;
		case MC_SELECT_NEXT_OBJECT:
			SelectNextObject();
			break;
		case MC_DROP_OBJECT:
			DropObject( pCursor->GetPos() );
			break;
		case CMD_BEGIN_ACTION1:
			BeginSelection( pCursor->GetPos() );
			break;
		case CMD_END_ACTION1:
			EndSelection( pCursor->GetPos() );
			break;
		case CMD_BEGIN_ACTION2:
			BeginAction( msg );
			break;
		case CMD_END_ACTION2:
			DoAction( msg );
			break;
		case MC_RESET_SELECTION:
			ResetSelection();
			break;
		case MC_CENTER_CAMERA:
			GetSingleton<ICamera>()->SetAnchor( vCameraStartPos );
			break;
		case MC_SET_TEXT_MODE:
			pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
			break;
		case MC_CANCEL_TEXT_MODE:
			pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
			break;
		case MC_STATUS_OBJECT:							// change status bar text
			SetMissionStatusObject( msg.nParam != 0 );
			break;
		case CMD_LOAD_FINISHED:
			{
				CheckResolution();
				GetSingleton<IScenarioTracker>()->UpdateMinimumDifficulty();
			}
			return false;
		
		case MC_MP_LAG_FINISHED:
		case MC_MP_LAG_STARTED:
		case MC_MP_DROP_LAGGED_PLAYER:
		case MC_MP_PLAYER_LOAD_FINISHED:
		case MC_MP_PLAYER_LOAD_STARTED:

			laggedDialog.ProcessMessage( msg, pUIScreen );
			break;

		case MC_MP_FINISHED:
			GetSingleton<IScenarioTracker>()->FinishMission( MISSION_FINISH_WIN );
			FinishInterface( MISSION_COMMAND_STATS, "2"/*STATS_COMPLEXITY_MISSION*/ );
			break;

		case WCB_DRAW:
			GetSingleton<IScenarioTracker>()->FinishMission( MISSION_FINISH_WIN );
			FinishInterface( MISSION_COMMAND_STATS, "2"/*STATS_COMPLEXITY_MISSION*/ );

			break;
		case MC_SHOW_SINGLE_OBJECTIVE:
			GetSingleton<IInput>()->AddMessage( SGameMessage( MC_SHOW_SINGLE_OBJECTIVE ) );
			break;
		case MC_HIDE_ESCAPE_WITHOUT_UNPAUSE:
			GetSingleton<IInput>()->AddMessage( SGameMessage( MC_HIDE_ESCAPE_WITHOUT_UNPAUSE ) );
			break;
		case MC_SHOW_QUIT_MENU:
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_QUIT_MISSION, 0 );
			break;
		case CMD_GAME_PAUSE_MENU:
			if ( GetGlobalVar("MultiplayerGame", 0) != 1 ) 
				GetSingleton<IMainLoop>()->Pause( true, PAUSE_TYPE_MENU );
			break;
		case CMD_GAME_UNPAUSE_MENU:
			GetSingleton<IMainLoop>()->Pause( false, PAUSE_TYPE_MENU );
			break;
		case CMD_GAME_SPEED_INC_SEND:
		case CMD_GAME_SPEED_DEC_SEND:
			GetSingleton<ITransceiver>()->CommandClientSpeed( msg.nEventID == CMD_GAME_SPEED_INC_SEND ? +1 : -1 );
			break;
		case CMD_GAME_SPEED_INC:
		case CMD_GAME_SPEED_DEC:
			{
				const int nOldSpeed = GetSingleton<IGameTimer>()->GetSpeed();
				const int nNewSpeed = GetSingleton<IGameTimer>()->SetSpeed( nOldSpeed + (msg.nEventID == CMD_GAME_SPEED_INC ? +1 : -1) );
				if ( nOldSpeed != nNewSpeed )
				{
					const int nMaxSpeed = +GetGlobalVar( "maxspeed", 10 );
					const int nMinSpeed = -GetGlobalVar( "minspeed", 10 );

					CPtr<IText> pText = GetSingleton<ITextManager>()->GetString( "game_speed" );
					CPtr<IText> pAddition;
					
					if ( nMaxSpeed == nNewSpeed )
						pAddition = GetSingleton<ITextManager>()->GetString( "game_speed_maximum" );
					else if ( nMinSpeed == nNewSpeed )
						pAddition = GetSingleton<ITextManager>()->GetString( "game_speed_minimum" );
					
					if ( pText )
					{
					std::wstring szString = MakeWideStringFromWordString( pText->GetString() ) + L" " + NStr::ToUnicode( NStr::Format("%+d", nNewSpeed) );
						if ( pAddition )
						szString += MakeWideStringFromWordString( pAddition->GetString() );
						GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_CHAT, szString.c_str(), 0xff00ff00 );
					}
					else
						GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("%+d", nNewSpeed), 0xff00ff00 );
				}
			}
			break;
		case MC_CLEAR_SCREEN_ACKS:
			if ( pUIScreen )
				pUIScreen->ClearStrings();
			break;
			
		case UI_NOTIFY_SELECTION_CHANGED:
		case UI_NOTIFY_BAR_EXPAND:
			if ( msg.nParam == 10 )		//��� objectives Shortcut Bar
			{
				IUIMiniMap *pUIMiniMap = checked_cast<IUIMiniMap*>( pUIScreen->GetChildByID( 20000 ) );
				if ( pUIMiniMap )
				{
					int nObjective = msg.nParam;
					IUIContainer *pDialog = checked_cast<IUIContainer *> ( pUIScreen->GetChildByID( 4000 ) );
					IUIShortcutBar *pSB = checked_cast<IUIShortcutBar *> ( pDialog->GetChildByID( 10 ) );
					int nBar = 0;
					int nItem = 0;
					pSB->GetSelectionItem( &nBar, &nItem );
					if ( nBar != -1 && pSB->GetBarExpandState( nBar ) )
					{
						IUIElement *pBar = pSB->GetBar( nBar );
						NI_ASSERT_T( pBar != 0, "Error in UI_NOTIFY_BAR_EXPAND" );
						const int nObjectiveNumber = pBar->GetWindowID();
						const std::string szMissionName = GetGlobalVar( "Mission.Current.Name", "" );
						const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>(szMissionName.c_str(), IObjectsDB::MISSION);
						if ( pStats )
						{
							if ( ( nObjectiveNumber >= 0 ) && ( nObjectiveNumber < pStats->objectives.size() ) )
							{
								const SMissionStats::SObjective &rObjective = pStats->objectives[nObjectiveNumber];
								if( rObjective.vPosOnMap != VNULL2 )
								{
									IGameTimer *pGameTimer = GetSingleton<IGameTimer>();
									NTimer::STime currentAbsTime = pGameTimer->GetAbsTime();
									const int nCirclesCount = 1;
									const CVec2 objectivePosition( rObjective.vPosOnMap.x / 512.0f, ( 512 - rObjective.vPosOnMap.y - 1 ) / 512.0f );
									for ( int nCircleIndex = 0; nCircleIndex < nCirclesCount; ++nCircleIndex )
									{
										pUIMiniMap->AddCircle( objectivePosition, fWorldCellSize * 16.0f, MMC_STYLE_MIXED, 0xFF0F, currentAbsTime + ( 2000 * nCircleIndex ), 2050, true, 0 );
									}
								}
							}
						}
					}
				}
			}
			return true;

		case MC_SHOW_BUILD_BUTTONS:
		case MC_SHOW_BUILD_BUTTONS + 1:
		case MC_SHOW_BUILD_BUTTONS + 2:
		case MC_SHOW_BUILD_BUTTONS + 3:
		case MC_SHOW_BUILD_BUTTONS + 4:
		case MC_SHOW_BUILD_BUTTONS + 5:
		case MC_CLEAR_ACTIVE_BUTTON:
		case MC_CLEAR_ACTIVE_BUTTON + 1:
		case MC_CLEAR_ACTIVE_BUTTON + 2:
			GetSingleton<IInput>()->AddMessage( SGameMessage( msg.nEventID ) );
			break;
		case MC_UPDATE_TEAM_F_R_AGS:
		case MC_UPDATE_TEAM_F_L_AGS:
		case MC_UPDATE_TIME_BEFORE_CAPTURE:
			multiplayerScoresSmall.ProcessMessage( msg, pUIScreen );
			break;

		case MC_ENTER_CHAT_MODE:
		{
			int k = 0;
			break;
		}

		case MC_ENTER_CHAT_MODE_FRIENDS:
		{
			int k = 0;
			break;
		}

		case MC_VISUALIZE_FEEDBACK_ENEMY_ANTIARTILLERY:
		case MC_VISUALIZE_FEEDBACK_ENEMY_AVIATION:
		case MC_VISUALIZE_FEEDBACK_AVIATION_READY:
		case MC_VISUALIZE_FEEDBACK_PLACE_MARKER:
		case MC_VISUALIZE_FEEDBACK_BAD_WEATHER:
		case MC_VISUALIZE_FEEDBACK_AVIATION_KILLED:
		case MC_VISUALIZE_FEEDBACK_AA_STARTED:
		case MC_VISUALIZE_FEEDBACK_AA_NEWDETECTED:
		case MC_VISUALIZE_FEEDBACK_REINFORCEMENT_ARRIVAL:
		case MC_VISUALIZE_FEEDBACK_SCENARIOUNIT_DEAD:
		case MC_VISUALIZE_FEEDBACK_SNIPER_DEAD:
		case MC_VISUALIZE_FEEDBACK_UNITS_PASSED:
			VisualizeFeedback( msg.nEventID, msg.nParam );
			break;

	case CMD_GAME_TIMEOUT_SEND:						// set game timeout
		if ( GetGlobalVar( "temp.LocalPlayer.TimeOutEnable", 0 ) && !GetGlobalVar( "History.Playing", 0 ) )
			GetSingleton<ITransceiver>()->CommandTimeOut( true );
		break;
	case CMD_GAME_UNTIMEOUT_SEND:					// remove game timeout
		if ( GetGlobalVar( "temp.LocalPlayer.UntimeOutEnable", 0 ) && !GetGlobalVar( "History.Playing", 0 ) )
			GetSingleton<ITransceiver>()->CommandTimeOut( false );
		break;

		case CMD_GAME_TIMEOUT:
		case CMD_GAME_UNTIMEOUT:
		case CMD_GAME_TIMEOUT_UPDATE:
			timeoutDialog.ProcessMessage( msg, pUIScreen );			
			break;

		default:
			AddMessage( msg );
			return false;
	}

	return true;
}
void CInterfaceMission::VisualizeFeedback( const int /*EMissionCommands*/ nFeedBack, const int nParam )
{
	bool bCircle = false;
	WORD wCircleColor = 0xff00;
	MINIMAP_CIRCLE_STYLE eStyle = MMC_STYLE_MIXED;
	CVec3 vCenter;
	ITextManager * pTM = GetSingleton<ITextManager>();
	IText *pText = 0;
	std::string szSoundName;
	const std::string szSeason = GetGlobalVar( "World.Season", "Summer" );
	const DWORD dwTextColor = GetGlobalVar( (std::string("Scene.Colors.") + szSeason + ".Text.Information.Color").c_str(), int(0xffffffff) );

	switch( nFeedBack )
	{
	case MC_VISUALIZE_FEEDBACK_BAD_WEATHER:
		bCircle = false;
		pText = pTM->GetString( "Textes\\FeedBacks\\bad_weather_started_avia_disabled" );

		break;
	case MC_VISUALIZE_FEEDBACK_PLACE_MARKER:
		bCircle = true;
		wCircleColor = 0xf00f;
		szSoundName = "Int_information";
		pText = pTM->GetString( "Textes\\FeedBacks\\ally_placed_marker" );

		break;
	case MC_VISUALIZE_FEEDBACK_AVIATION_READY:
		bCircle = false;
		pText = pTM->GetString( "Textes\\FeedBacks\\aviation_is_ready" );

		break;
	case MC_VISUALIZE_FEEDBACK_ENEMY_ANTIARTILLERY:
		bCircle = true;
		wCircleColor = 0xff80;
		eStyle = MMC_STYLE_CONVERGENT;
		szSoundName = "Int_information";
		pText = pTM->GetString( "Textes\\FeedBacks\\enemy_antiartillery" );

		break;
	case MC_VISUALIZE_FEEDBACK_AA_STARTED:
		bCircle = true;
		wCircleColor = 0xff00;
		szSoundName = "Int_information";
		pText = pTM->GetString( "Textes\\FeedBacks\\enemy_aa_detected" ); 

		break;
	case MC_VISUALIZE_FEEDBACK_AA_NEWDETECTED:
		bCircle = true;
		wCircleColor = 0xff00;
		break;
	case MC_VISUALIZE_FEEDBACK_SCENARIOUNIT_DEAD:
		bCircle = true;
		wCircleColor = 0xff00;
		szSoundName = "Int_information";
		eStyle = MMC_STYLE_LPOLYGON_CONVERGENT;
		pText = pTM->GetString( "Textes\\FeedBacks\\scenario_unit_dead" ); 

		break;
	case MC_VISUALIZE_FEEDBACK_UNITS_PASSED:
		{
			bCircle = false;
			szSoundName = "Int_information";
			pText = CreateObject<IText>( TEXT_STRING );
			IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
			const std::wstring wszTrg = pST->GetPlayer( (nParam >> 16) & 0xffff )->GetName();
			const std::wstring wszSrc = pST->GetPlayer( nParam & 0xffff )->GetName();
			const std::wstring wszBase = 
				( pTM->GetString( "Textes\\FeedBacks\\units_passed" ) ) ? 
				MakeWideStringFromWordString( pTM->GetString( "Textes\\FeedBacks\\units_passed" )->GetString() ) : L"";
			pText->SetText( reinterpret_cast<const WORD*>( (wszBase + L": " + wszSrc + L" -> " + wszTrg).c_str() ) );
		}
		break;
	case MC_VISUALIZE_FEEDBACK_SNIPER_DEAD:
		bCircle = true;
		wCircleColor = 0xff00;
		szSoundName = "Int_information";
		eStyle = MMC_STYLE_LPOLYGON_CONVERGENT;
		pText = pTM->GetString( "Textes\\FeedBacks\\sniper_dead" ); 

		break;
	case MC_VISUALIZE_FEEDBACK_REINFORCEMENT_ARRIVAL:
		bCircle = true;
		wCircleColor = 0xf0f0;
		eStyle = MMC_STYLE_LPOLYGON_CONVERGENT;

		break;
	case MC_VISUALIZE_FEEDBACK_ENEMY_AVIATION:
		bCircle = true;
		wCircleColor = 0xff00;
		szSoundName = "Int_information";
		eStyle = MMC_STYLE_LPOLYGON_CONVERGENT;
		pText = pTM->GetString( "Textes\\FeedBacks\\enemy_aviation_called" );

		break;
	case MC_VISUALIZE_FEEDBACK_AVIATION_KILLED:
		{
			const int nFeedbackType = (nParam >> 16) & 0xffff;
			bCircle = false;
			std::string szKey = "Textes\\FeedBacks\\AviationKilled\\";
			szSoundName = "Int_information";
			switch( nFeedbackType )
			{
			case 1:
				szKey += "LocalPlayer\\";
				break;
			case 2:
				szKey += "LocalParty\\";
				break;
			case 3:
				szKey += "Enemy\\";
				break;
			}
			const int nAviaType = nParam & 0xffff;
			szKey += NStr::Format( "%d", nAviaType );
			pText = pTM->GetString( szKey.c_str() );
		}
		break;
	}

	if ( bCircle )
	{
		CVec2 vCenter;
		AI2Vis( &vCenter, LOWORD(nParam), HIWORD(nParam) );
		IUIMiniMap *pUIMiniMap = checked_cast<IUIMiniMap*>( pUIScreen->GetChildByID( 20000 ) );
		IGameTimer * pGameTimer = GetSingleton<IGameTimer>();
		float fRadius;
		switch ( eStyle )
		{
			case MMC_STYLE_LPOLYGON_CONVERGENT:
			case MMC_STYLE_LPOLYGON_DIVERGENT:
			case MMC_STYLE_LPOLYGON_MIXED:
				fRadius = 32.0f;
				break;
			default:
				fRadius = 500.0f;
		}
		pUIMiniMap->AddCircle( vCenter, fRadius, eStyle, wCircleColor, pGameTimer->GetAbsTime(), 5000, false, MAKELPARAM( 4, 2 ) );
	}

	if ( !szSoundName.empty() )
		GetSingleton<IScene>()->AddSound( szSoundName.c_str(), VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
	
	if ( pText )
	{
		IConsoleBuffer * pBuffer = GetSingleton<IConsoleBuffer>();
		pBuffer->Write( CONSOLE_STREAM_CHAT, MakeWideStringFromWordString( pText->GetString() ).c_str(), dwTextColor );
	}
}
bool CInterfaceMission::PickObjects( CPickVisObjList *pPickedObjects, const CVec2 &point, EObjGameType type, bool bVisible )
{
	pPickedObjects->clear();
	std::pair<IVisObj*, CVec2> *pObjects = 0;
	int nNumObjects = 0;
	pScene->Pick( point, &pObjects, &nNumObjects, type, bVisible );
	for ( int i=0; i<nNumObjects; ++i )
		pPickedObjects->push_back( pObjects[i] );
	return !pPickedObjects->empty();
}
bool CInterfaceMission::PickObjects( CPickVisObjList *pPickedObjects, const CTRect<float> &rcRect, EObjGameType type, bool bVisible )
{
	pPickedObjects->clear();
	std::pair<IVisObj*, CVec2> *pObjects = 0;
	int nNumObjects = 0;
	pScene->Pick( rcRect, &pObjects, &nNumObjects, type, bVisible );
	for ( int i=0; i<nNumObjects; ++i )
		pPickedObjects->push_back( pObjects[i] );
	return !pPickedObjects->empty();
}
void CInterfaceMission::ResetPreSelection() 
{ 
	pWorld->ResetPreSelection();
	preselectedObjects.clear();
}
void CInterfaceMission::ResetSelection( IVisObj *pObj ) { pWorld->ResetSelection( pObj ); }
void CInterfaceMission::ResetSelectionLocal()
{
	for ( CPickVisObjList::iterator it = selectedObjects.begin(); it != selectedObjects.end(); ++it )
		it->first->Select( SGVOSS_UNSELECTED );
	selectedObjects.clear();
	itCurrSelected = selectedObjects.end();
	pSelectedObject = 0;
}
bool CInterfaceMission::PreSelectObjects( const CVec2 &vPos, EObjGameType type )
{
	CPickVisObjList picked;
	if ( PickObjects(&picked, vPos, type, true) )
		PreSelectObjects( picked );
	return !picked.empty();
}
bool CInterfaceMission::PreSelectObjects( const CTRect<float> &rect, EObjGameType type )
{
	CPickVisObjList picked;
	if ( PickObjects(&picked, rect, type, true) )
		PreSelectObjects( picked );
	return !picked.empty();
}
void CInterfaceMission::PreSelectObjects( const CPickVisObjList &picked )
{
	preselectedObjects.resize( picked.size() );
	int i = 0;
	for ( CPickVisObjList::const_iterator it = picked.begin(); it != picked.end(); ++it, ++i )
		preselectedObjects[i] = it->first;
	pWorld->PreSelect( &(preselectedObjects[0]), preselectedObjects.size() );
}
void CInterfaceMission::AddPreSelectedObjects()
{
	if ( !preselectedObjects.empty() )
	{
		pWorld->Select( &(preselectedObjects[0]), preselectedObjects.size() );
		preselectedObjects.clear();
	}
}
bool CInterfaceMission::SelectFirstObject( const CVec2 &point )
{
	ResetSelectionLocal();
	{
		selectedObjects.clear();
		CPickVisObjList picked;
		if ( PickObjects( &picked, point, SGVOGT_UNKNOWN ) )
		{
			for ( CPickVisObjList::const_iterator it = picked.begin(); it != picked.end(); ++it )
				selectedObjects.push_back( *it );
		}
	}
	for ( CVisObjList::iterator it = preselectedObjects.begin(); it != preselectedObjects.end(); ++it )
		(*it)->Select( SGVOSS_UNSELECTED );
	if ( !selectedObjects.empty() )
	{
		itCurrSelected = selectedObjects.begin();
		pSelectedObject = &( *itCurrSelected );
		pSelectedObject->first->Select( SGVOSS_SELECTED );
	}
	
	return pSelectedObject != 0;
}
void CInterfaceMission::SelectNextObject()
{
	if ( pSelectedObject )
	{
		if ( itCurrSelected != selectedObjects.end() )
			++itCurrSelected;
		if ( itCurrSelected != selectedObjects.end() )
		{
			pSelectedObject->first->Select( SGVOSS_UNSELECTED );
			pSelectedObject = &( *itCurrSelected );
			pSelectedObject->first->Select( SGVOSS_SELECTED );
		}
	}
}
bool CInterfaceMission::DropObject( const CVec2 &pos )
{
	if ( !pSelectedObject )
		return false;
	ResetSelectionLocal();

	return true;
}
void CInterfaceMission::GetPos3( CVec3 *pPos, const CVec2 &pos )
{
	pScene->GetPos3( pPos, pos );
}
void CInterfaceMission::GetPos3( CVec3 *pPos, float x, float y )
{
	pScene->GetPos3( pPos, CVec2(x, y) );
}
void CInterfaceMission::DoAction( const SGameMessage &msg )
{
	pWorld->DoAction( msg );
}
void CInterfaceMission::BeginAction( const SGameMessage &msg )
{
	pWorld->BeginAction( msg );
}
void CInterfaceMission::BeginSelection( const CVec2 &vPos2 )
{
	CVec3 vPos3;
	pScene->GetPos3( &vPos3, vPos2 );
	pFrameSelection->Begin( vPos3 );
}
void CInterfaceMission::EndSelection( const CVec2 &vPos2 )
{
	CTRect<float> rcRect( 0, 0, 0, 0 );
	if ( pFrameSelection->IsActive() ) 
	{
		CVec2 vBegin;
		pScene->GetPos2( &vBegin, pFrameSelection->GetBeginPoint() );
		CVec2 vEnd;
		pScene->GetPos2( &vEnd, pFrameSelection->GetEndPoint() );
		rcRect.Set( vBegin, vEnd );
		rcRect.Normalize();
		pFrameSelection->End();
	}
	if ( !rcRect.IsEmpty() )							// do frame selection
		AddPreSelectedObjects();
	else			// do pick selection
	{
		pWorld->Select( vPos2 );
		ResetPreSelection();
	}
}
bool CInterfaceMission::MakeMapShot()
{
	ITerrain *pTerrain = pScene->GetTerrain();
	if ( pTerrain == 0 ) 
		return false;
	const CTRect<long> rcScreenRect = pGFX->GetScreenRect();
	const int nStepX = rcScreenRect.Width();
	const int nStepY = rcScreenRect.Height() * 2;
	const int nMaxY =  pTerrain->GetSizeY() * fCellSizeX;
	const int nMinY = -pTerrain->GetSizeY() * fCellSizeX;
	const int nMaxX = pTerrain->GetSizeX() * fCellSizeX * 2;
	const int nMinX = 0;
	while ( pScene->ToggleShow(SCENE_SHOW_HAZE) != false );
	while ( pScene->ToggleShow(SCENE_SHOW_UI) != false );
	while ( pScene->ToggleShow(SCENE_SHOW_WARFOG) != false );
	pCamera->SetBounds( 0, 0, 0, 0 );
	pCursor->Show( false );
	const CVec3 vOldAnchor = pCamera->GetAnchor();
	const int nLargeSizeX = ( (nMaxX - nMinX) / nStepX + 1 ) * rcScreenRect.Width();
	const int nLargeSizeY = ( (nMaxY - nMinY) / nStepY + 2 ) * rcScreenRect.Height();
	const int nImageStepX = rcScreenRect.Width();
	const int nImageStepY = rcScreenRect.Height();
	int nImageY = 0;

	IImageProcessor *pIP = GetImageProcessor();
	CPtr<IImage> pImage = pIP->CreateImage( nLargeSizeX, nLargeSizeY );

	for ( int i = nMaxY, nCountY = 0; i > nMinY - nStepY; i -= nStepY, nImageY += nImageStepY, ++nCountY )
	{
		int nImageX = 0;
		for ( int j = nMinX, nCountX = 0; j <= nMaxX; j += nStepX, nImageX += nImageStepX, ++nCountX )
		{
			const double fX = double(j - i) / SQRT_2;
			const double fY = double(j + i) / SQRT_2;
			pCamera->SetAnchor( CVec3(fX, fY, 0) );
			pGFX->Clear( 0, 0, GFXCLEAR_ALL, 0 );
			pGFX->BeginScene();
			pScene->Draw( pCamera );
			pGFX->EndScene();
			pGFX->Flip();
			CPtr<IImage> pShot = pIP->CreateImage( rcScreenRect.Width(), rcScreenRect.Height() );
			if ( pGFX->TakeScreenShot(pShot) )
				pImage->CopyFrom( pShot, 0, nImageX, nImageY );
		}
	}
	{
		CPtr<IDataStream> pStream = CreateFileStream( NStr::Format("%smapshot.tga", GetSingleton<IMainLoop>()->GetBaseDir()), STREAM_ACCESS_WRITE );
		pIP->SaveImageAsTGA( pStream, pImage );
	}
	pCamera->SetBounds( 0, 0, pTerrain->GetSizeX() * fWorldCellSize, pTerrain->GetSizeY() * fWorldCellSize );
	while ( pScene->ToggleShow(SCENE_SHOW_HAZE) == false );
	while ( pScene->ToggleShow(SCENE_SHOW_UI) == false );
	while ( pScene->ToggleShow(SCENE_SHOW_WARFOG) == false );
	pCamera->SetAnchor( vOldAnchor );
	pCursor->Show( true );
	return true;
}
