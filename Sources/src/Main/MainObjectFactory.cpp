#include "StdAfx.h"

#include "MainObjectFactory.h"

#include "iMainClassIDs.h"
#include "GameTimerInternal.h"
#include "iMainInternal.h"
#include "MainLoopCommands.h"
#include "TextObject.h"
#include "TextManager.h"
#include "SinglePlayerTransceiver.h"
#include "MultiPlayerTransceiver.h"
#include "AILogicCommandInternal.h"
#include "MultiplayerInternal.h"
#include "CommandsHistory.h"
#include "FilesInspector.h"

#include "UserProfile.h"
#include "PlayerScenarioInfo.h"
#include "ScenarioStatistics.h"
#include "ScenarioTracker2Internal.h"
CMainObjectFactory theMainObjectFactory;
IObjectFactory* STDCALL GetMainObjectFactory()
{
	return &theMainObjectFactory;
}
CMainObjectFactory::CMainObjectFactory()
{
	REGISTER_CLASS( this, MAIN_SP_TRANSCEIVER, CSinglePlayerTransceiver );
	REGISTER_CLASS( this, MAIN_MP_TRANSCEIVER, CMultiPlayerTransceiver );
	REGISTER_CLASS( this, MAIN_COMMAND_CHANGE_TRANSCEIVER, CChangeTransceiverCommand );

	REGISTER_CLASS( this, MAIN_TRANS_COMMAND_REGISTER_GROUP, CRegisterGroupCommand );
	REGISTER_CLASS( this, MAIN_TRANS_COMMAND_UNREGISTER_GROUP, CUnregisterGroupCommand );
	REGISTER_CLASS( this, MAIN_TRANS_COMMAND_GROUP_COMMAND, CGroupCommand );
	REGISTER_CLASS( this, MAIN_TRANS_COMMAND_UNIT_COMMAND, CUnitCommand );
	REGISTER_CLASS( this, MAIN_TRANS_COMMAND_SHOW_AREAS, CShowAreasCommand );
	REGISTER_CLASS( this, MAIN_CONTROL_CHECK_SUM_COMMAND, CControlSumCheckCommand );
	REGISTER_CLASS( this, MAIN_DROP_PLAYER_COMMAND, CDropPlayerCommand );
	
	REGISTER_CLASS( this, MAIN_COMMAND_SAVE, CICSave );
	REGISTER_CLASS( this, MAIN_COMMAND_LOAD, CICLoad );
	REGISTER_CLASS( this, MAIN_COMMAND_POP, CICPopInterface );
	REGISTER_CLASS( this, MAIN_COMMAND_CMD, CICSendCommand );
	REGISTER_CLASS( this, MAIN_COMMAND_ENABLE_MESSAGE_PROCESSING, CICEnableMessageProcessing );
	REGISTER_CLASS( this, MAIN_COMMAND_EXIT_GAME, CICExitGame );
	REGISTER_CLASS( this, MAIN_COMMAND_CHANGE_MOD, CICChangeMOD );
	REGISTER_CLASS( this, MAIN_COMMAND_PAUSE, CICPauseGame );
	
	REGISTER_CLASS( this, MAIN_SINGLE_TIMER, CSingleTimer );
	REGISTER_CLASS( this, MAIN_SEGMENT_TIMER, CSegmentTimer );
	REGISTER_CLASS( this, MAIN_GAME_TIMER, CGameTimer );
	REGISTER_CLASS( this, MAIN_TIME_SLIDER, CTimeSlider );

	REGISTER_CLASS( this, TEXT_MANAGER, CTextManager );
	REGISTER_CLASS( this, TEXT_STRING, CTextString );
	REGISTER_CLASS( this, TEXT_DIALOG, CTextDialog );
	REGISTER_CLASS( this, MAIN_SCENARIO_TRACKER, CScenarioTracker2 );
	REGISTER_CLASS( this, MAIN_USER_PROFILE, CUserProfile );
	REGISTER_CLASS( this, MAIN_SCENARIO_UNIT, CScenarioUnit );
	REGISTER_CLASS( this, MAIN_PLAYER_SCENARIO_INFO, CPlayerScenarioInfo );
	REGISTER_CLASS( this, MAIN_MISSION_STATISTICS, CMissionStatistics );
	REGISTER_CLASS( this, MAIN_CHAPTER_STATISTICS, CChapterStatistics );
	REGISTER_CLASS( this, MAIN_CAMPAIGN_STATISTICS, CCampaignStatistics );

	REGISTER_CLASS( this, LAN_MULTIPLAYER, CLanMultiplayer );
	REGISTER_CLASS( this, GAMESPY_MULTIPLAYER, CGameSpyMultiplayer );
	REGISTER_CLASS( this, INTERNET_MULTIPLAYER, CInternetMultiplayer );
	
	REGISTER_CLASS( this, MAIN_PROGRESS_INDICATOR, CProgressScreen );

	REGISTER_CLASS( this, MAIN_COMMANDS_HISTORY_INTERNAL, CCommandsHistory );

	REGISTER_CLASS( this, MAIN_FILES_INSPECTOR, CFilesInspector );
	REGISTER_CLASS( this, MAIN_FILES_INSPECTOR_ENTRY_GDB, CFilesInspectorEntryGDB );
	REGISTER_CLASS( this, MAIN_FILES_INSPECTOR_ENTRY_COLLECTOR, CFilesInspectorEntryCollector );
	
	REGISTER_CLASS( this, MAIN_AUTOMAGIC, CRPGStatsAutomagic );
}
