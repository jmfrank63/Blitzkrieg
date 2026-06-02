#include "StdAfx.h"

#include "QuitMission.h"

#include "..\Main\gamestats.h"
#include "..\Main\ScenarioTracker.h"
#include "..\Main\Transceiver.h"
enum EInterMissionCommand
{
	MC_RESTART_MISSION	= 10001,
	MC_QUIT_MISSION			= 10002,
	MC_EXIT_PROGRAM			= 10003,
	MC_CANCEL_QUIT			= 10004,
};
static const NInput::SRegisterCommandEntry quitmissionCommands[] = 
{
	{ "restart_mission", MC_RESTART_MISSION	},
	{ "quit_mission", MC_QUIT_MISSION			},
	{ "exit_program", MC_EXIT_PROGRAM			},
	{ "cancel_quit"	,	MC_CANCEL_QUIT			},
	{ 0							,	0										}
};
CInterfaceQuitMission::~CInterfaceQuitMission()
{
}
bool CInterfaceQuitMission::Init()
{
	CInterfaceScreenBase::Init();
	SetBindSection( "quitmission" );
	quitmissionMsgs.Init( pInput, quitmissionCommands );

	return true;
}
void CInterfaceQuitMission::StartInterface()
{
	CInterfaceScreenBase::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\quitmission" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	int nMG = GetGlobalVar( "MultiplayerGame", -1 );
	if ( nMG > 0 )
	{
		IUIElement *pButton = pUIScreen->GetChildByID( 10001 );
		pButton->EnableWindow( false );
	}
	
	if ( GetGlobalVar("History.Playing", 0) != 0 )
	{
		IUIElement *pButton = pUIScreen->GetChildByID( 10001 );
		pButton->EnableWindow( false );
	}

	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceQuitMission::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
	case MC_CANCEL_QUIT:
		{
			IMainLoop *pML = GetSingleton<IMainLoop>();
			CloseInterface();
			pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", MC_SHOW_ESCAPE_MENU) );	//покажем escape menu
			return true;
		}
		
		case MC_QUIT_MISSION:
			GetSingleton<IScenarioTracker>()->FinishMission( MISSION_FINISH_ABORT );
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d 0", MAIN_SP_TRANSCEIVER) );
			FinishInterface( MISSION_COMMAND_MAIN_MENU, 0 );
			GetSingleton<IScene>()->SetSoundSceneMode( ESSM_INTERMISSION_INTERFACE );
			if ( GetGlobalVar("History.Playing", 0) != 0 ) 
			{
				GetSingleton<IMainLoop>()->RestoreScenarioTracker();
				RemoveGlobalVar( "History.Playing" );
				RemoveGlobalVar( "MultiplayerGame" );

				UnRegisterSingleton( ITransceiver::tidTypeID );
				ITransceiver *pTrans = CreateObject<ITransceiver>( MAIN_SP_TRANSCEIVER );
				RegisterSingleton( ITransceiver::tidTypeID, pTrans );
			}
			return true;
			
		case MC_RESTART_MISSION:
		{
			std::string szMapName;
			const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>(GetGlobalVar("Mission.Current.Name"), IObjectsDB::MISSION);
			if ( pStats != 0 )
				szMapName = pStats->szFinalMap + ".xml";
			else
				szMapName = GetGlobalVar( "Map.Current.Name" );
			
			GetSingleton<IGlobalVars>()->RemoveVarsByMatch( "temp." );
			GetSingleton<IScenarioTracker>()->FinishMission( MISSION_FINISH_RESTART );
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MISSION, szMapName.c_str() );
			return true;
		}

		case MC_EXIT_PROGRAM:
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d 0", MAIN_SP_TRANSCEIVER) );
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_EXIT_GAME, 0 );
			return true;
	}
	return false;
}
bool CInterfaceQuitMission::StepLocal( bool bAppActive )
{
	const CVec2 vPos = pCursor->GetPos();
	CInterfaceScreenBase::OnCursorMove( vPos );
	pUIScreen->Update( pTimer->GetAbsTime() );
	return true;
}
void CInterfaceQuitMission::DrawAdd()
{
}
