#include "StdAfx.h"

#include "MissionObjectFactory.h"

#include "iMissionInternal.h"
#include "WorldClient.h"
#include "SaveMission.h"
#include "LoadMission.h"
#include "QuitMission.h"
#include "Campaign.h"
#include "Chapter.h"
#include "Mission.h"
#include "MainMenu.h"
#include "Stats.h"
#include "encyclopedia.h"
#include "CustomMission.h"
#include "CustomChapter.h"
#include "CustomCampaign.h"
#include "AddUnitToMission.h"
#include "UnitsPool.h"
#include "UpgradeUnit.h"
#include "PlayMovieInterface.h"
#include "SingleMedal.h"
#include "SwitchToNextChapter.h"
#include "InterfaceStartDialog.h"
#include "InterfaceCloudCredentials.h"
#include "IMLoadMission.h"
#include "TutorialList.h"
#include "PlayerGainLevel.h"
#include "SaveReplay.h"
#include "ReplayList.h"
#include "CutsceneList.h"
#include "TotalEncyclopedia.h"
#include "IMTutorial.h"

#include "PlayersInterface.h"
#include "MultiplayerGamesList.h"
#include "InterfaceMPChat.h"
#include "MultiplayerStartingGame.h"
#include "MultiplayerCommandManagerInternal.h"
#include "InterfaceMPCreateGame.h"
#include "InterfaceOptionsSettings.h"
#include "InterfaceMPMapSettings.h"
#include "IMSaveMission.h"
#include "../Common/MOObject.h"
#include "../Common/MOUnitMechanical.h"
#include "../Common/MOUnitInfantry.h"
#include "../Common/MOProjectile.h"
#include "../Common/MOBuilding.h"
#include "../Common/MOBridge.h"
#include "../Common/MOSquad.h"
#include "../Common/MOEntrenchment.h"
#include "../Common/UISquadElement.h"
#include "../GameTT/AckManager.h"
#include "../GameTT/MessageReactionINternal.h"
#include "../GameTT/InterfaceUnitPerformance.h"
#include "../GameTT/InterfaceAfterMissionPopups.h"
#include "../GameTT/InterfaceIMModsList.h"
#include "../GameTT/InterfaceSwitchModeTo.h"
#include "../GameTT/InterfaceMessageBox.h"
#include "../GameTT/InterfaceNewDepotUpgrades.h"
#include "../GameTT/InterfaceMPAddressBook.h"
CMissionObjectFactory theMissionObjectFactory;
IObjectFactory* STDCALL GetMissionObjectFactory()
{
	return &theMissionObjectFactory;
}
CMissionObjectFactory::CMissionObjectFactory()
{
	REGISTER_CLASS( this, MISSION_MO_OBJECT, CMOObject );
	REGISTER_CLASS( this, MISSION_MO_UNIT_MECHANICAL, CMOUnitMechanical );
	REGISTER_CLASS( this, MISSION_MO_UNIT_INFANTRY, CMOUnitInfantry );
	REGISTER_CLASS( this, MISSION_MO_PROJECTILE, CMOProjectile );
	REGISTER_CLASS( this, MISSION_MO_BUILDING, CMOBuilding );
	REGISTER_CLASS( this, MISSION_MO_SQUAD, CMOSquad );
	REGISTER_CLASS( this, MISSION_MO_ENTRENCHMENT_SEGMENT, CMOEntrenchmentSegment );
	REGISTER_CLASS( this, MISSION_MO_BRIDGE_SPAN, CMOBridgeSpan );
	REGISTER_CLASS( this, MISSION_UI_WHO_IN_CONTAINER, CUISquadElement );
	REGISTER_CLASS( this, MISSION_UI_UNIT_OBSERVER, CUIUnitObserver );
	REGISTER_CLASS( this, MISSION_INTERFACE_MISSION, CInterfaceMission );
	REGISTER_CLASS( this, MISSION_COMMAND_MISSION, CICMission );
	REGISTER_CLASS( this, MISSION_WORLD, CWorldClientBridge );
	REGISTER_CLASS( this, MISSION_INTERFACE_SAVE_MISSION, CInterfaceSaveMission );
	REGISTER_CLASS( this, MISSION_COMMAND_SAVE_MISSION, CICSaveMission );
	REGISTER_CLASS( this, MISSION_INTERFACE_LOAD_MISSION, CInterfaceLoadMission );
	REGISTER_CLASS( this, MISSION_COMMAND_LOAD_MISSION, CICLoadMission );
	REGISTER_CLASS( this, MISSION_INTERFACE_QUIT_MISSION, CInterfaceQuitMission );
	REGISTER_CLASS( this, MISSION_COMMAND_QUIT_MISSION, CICQuitMission );
	REGISTER_CLASS( this, MISSION_INTERFACE_CAMPAIGN, CInterfaceCampaign );
	REGISTER_CLASS( this, MISSION_COMMAND_CAMPAIGN, CICCampaign );
	REGISTER_CLASS( this, MISSION_INTERFACE_CHAPTER, CInterfaceChapter );
	REGISTER_CLASS( this, MISSION_COMMAND_CHAPTER, CICChapter );
	REGISTER_CLASS( this, MISSION_INTERFACE_ABOUT_MISSION, CInterfaceAboutMission );
	REGISTER_CLASS( this, MISSION_COMMAND_ABOUT_MISSION, CICAboutMission );
	REGISTER_CLASS( this, GAMETT_CLIENT_ACK_MANAGER, CClientAckManager );
	
	REGISTER_CLASS( this, MISSION_INTERFACE_MAIN_MENU, CInterfaceMainMenu );
	REGISTER_CLASS( this, MISSION_COMMAND_MAIN_MENU, CICMainMenu );
	REGISTER_CLASS( this, MISSION_INTERFACE_STATS, CInterfaceStats );
	REGISTER_CLASS( this, MISSION_COMMAND_STATS, CICStats );
	REGISTER_CLASS( this, MISSION_INTERFACE_ENCYCLOPEDIA, CInterfaceEncyclopedia );
	REGISTER_CLASS( this, MISSION_COMMAND_ENCYCLOPEDIA, CICEncyclopedia );
	REGISTER_CLASS( this, MISSION_INTERFACE_CUSTOM_MISSION, CInterfaceCustomMission );
	REGISTER_CLASS( this, MISSION_COMMAND_CUSTOM_MISSION, CICCustomMission );
	REGISTER_CLASS( this, MISSION_INTERFACE_CUSTOM_CHAPTER, CInterfaceCustomChapter );
	REGISTER_CLASS( this, MISSION_COMMAND_CUSTOM_CHAPTER, CICCustomChapter );
	REGISTER_CLASS( this, MISSION_INTERFACE_CUSTOM_CAMPAIGN, CInterfaceCustomCampaign );
	REGISTER_CLASS( this, MISSION_COMMAND_CUSTOM_CAMPAIGN, CICCustomCampaign );
	REGISTER_CLASS( this, MISSION_INTERFACE_UNITS_POOL, CInterfaceUnitsPool );
	REGISTER_CLASS( this, MISSION_COMMAND_UNITS_POOL, CICUnitsPool );
	REGISTER_CLASS( this, MISSION_INTERFACE_UPGRADE_UNIT, CInterfaceUpgradeUnit );
	REGISTER_CLASS( this, MISSION_COMMAND_UPGRADE_UNIT, CICUpgradeUnit );
	REGISTER_CLASS( this, MISSION_INTERFACE_ADD_UNIT_TO_MISSION, CInterfaceAddUnitToMission );
	REGISTER_CLASS( this, MISSION_COMMAND_ADD_UNIT_TO_MISSION, CICAddUnitToMission );
	REGISTER_CLASS( this, MISSION_INTERFACE_SINGLE_MEDAL, CInterfaceSingleMedal );
	REGISTER_CLASS( this, MISSION_COMMAND_SINGLE_MEDAL, CICSingleMedal );
	REGISTER_CLASS( this, MISSION_INTERFACE_NEXT_CHAPTER, CInterfaceNextChapter );
	REGISTER_CLASS( this, MISSION_COMMAND_NEXT_CHAPTER, CICNextChapter );
	
	REGISTER_CLASS( this, MISSION_INTERFACE_IM_LOAD_MISSION, CInterfaceIMLoadMission );
	REGISTER_CLASS( this, MISSION_COMMAND_IM_LOAD_MISSION, CICIMLoadMission );
	REGISTER_CLASS( this, MISSION_INTERFACE_TUTORIAL_LIST, CInterfaceTutorialList );
	REGISTER_CLASS( this, MISSION_COMMAND_TUTORIAL_LIST, CICTutorialList );
	REGISTER_CLASS( this, MISSION_INTERFACE_PLAYER_GAIN_LEVEL, CInterfacePlayerGainLevel );
	REGISTER_CLASS( this, MISSION_COMMAND_PLAYER_GAIN_LEVEL, CICPlayerGainLevel );
	REGISTER_CLASS( this, MISSION_INTERFACE_SAVE_REPLAY, CInterfaceSaveReplay );
	REGISTER_CLASS( this, MISSION_COMMAND_SAVE_REPLAY, CICSaveReplay );
	REGISTER_CLASS( this, MISSION_INTERFACE_REPLAY_LIST, CInterfaceReplayList );
	REGISTER_CLASS( this, MISSION_COMMAND_REPLAY_LIST, CReplayList );
	REGISTER_CLASS( this, MISSION_INTERFACE_CUTSCENE_LIST, CInterfaceCutsceneList );
	REGISTER_CLASS( this, MISSION_COMMAND_CUTSCENE_LIST, CCutsceneList );
	REGISTER_CLASS( this, MISSION_INTERFACE_TOTAL_ENCYCLOPEDIA, CInterfaceTotalEncyclopedia );
	REGISTER_CLASS( this, MISSION_COMMAND_TOTAL_ENCYCLOPEDIA, CICTotalEncyclopedia );
	REGISTER_CLASS( this, MISSION_INTERFACE_IM_TUTORIAL, CInterfaceIMTutorial );
	REGISTER_CLASS( this, MISSION_COMMAND_IM_TUTORIAL, CICIMTutorial );
	
	REGISTER_CLASS( this, MISSION_INTERFACE_VIDEO, CPlayMovieInterface );
	REGISTER_CLASS( this, MISSION_COMMAND_VIDEO, CICPlayMovie );

	REGISTER_CLASS( this, MISSION_COMMAND_PLAYERS_STATS, CICPlayersInterface );
	REGISTER_CLASS( this, MISSION_INTERFACE_PLAYERS_STATS, CPlayersInterface );

	REGISTER_CLASS( this, GAMETT_MULTIPLAYER_TO_UI_COMMANDS, CMPToUICommandManager );
	REGISTER_CLASS( this, MISSION_COMMAND_MULTIPLAYER_GAMESLIST, CICMultyplayerGamesList );
	REGISTER_CLASS( this, MISSION_INTERFACE_MULTIPLAYER_GAMESLIST, CInterfaceMPGamesList );
	REGISTER_CLASS( this, GAMETT_UI_SERVERINFO, SUIServerInfo );
	
	REGISTER_CLASS( this, MISSION_COMMAND_MULTIPLAYER_STARTINGGAME, CICMultyplayerStartingGame );
	REGISTER_CLASS( this, MISSION_INTERFACE_MULTIPLAYER_STARTINGGAME, CInterfaceMPStartingGame );
	REGISTER_CLASS( this, MISSION_COMMAND_GAMESPY_CLIENT, CICGameSpyClientConnect );
	REGISTER_CLASS( this, MISSION_COMMAND_GAMESPY_HOST, CICGameSpyCreateHost ) ;
	
	REGISTER_CLASS( this, MISSION_INTERFACE_MULTYPLAYER_CHAT, CInterfaceMPChat );
	REGISTER_CLASS( this, MISSION_COMMAND_MULTYPLAYER_CHAT, CICMultyplayerChat );

	REGISTER_CLASS( this, MISSION_COMMAND_MULTYPLAYER_CREATEGAME, CICMultyplayerCreateGame );
	REGISTER_CLASS( this, MISSION_INTERFACE_MULTYPLAYER_CREATEGAME, CInterfaceMPCreateGame  );

	REGISTER_CLASS( this, MISSION_INTERFACE_OPTIONSSETTINGS, CInterfaceOptionsSettings );
	REGISTER_CLASS( this, MISSION_COMMAND_OPTIONSSETTINGS, CICOptionsSettings );
	
	REGISTER_CLASS( this, MISSION_INTERFACE_PLAYER_PROFILE, CInterfacePlayerProfile );
	REGISTER_CLASS( this, MISSION_COMMAND_PLAYER_PROFILE, CICPlayerProfile );

	REGISTER_CLASS( this, MISSION_INTERFACE_CLOUD_CREDENTIALS, CInterfaceCloudCredentials );
	REGISTER_CLASS( this, MISSION_COMMAND_CLOUD_CREDENTIALS, CICCloudCredentials );

	REGISTER_CLASS( this, MISSION_INTERFACE_MP_MAP_SETTINGS, CInterfaceMPMapSettings );
	REGISTER_CLASS( this, MISSION_COMMAND_MP_MAP_SETTINGS, CICMPMapSettings );
	REGISTER_CLASS( this, MISSION_PLAYER_LAGGED_STATE, CInterfaceMission::CPlayerLaggedDialog::CDialogStateLagged );
	REGISTER_CLASS( this, MISSION_PLAYER_LOADING_STATE, CInterfaceMission::CPlayerLaggedDialog::CDialogStateLoading );
	
	REGISTER_CLASS( this, MISSION_INTERFACE_WAREHOUSE, CInterfaceWarehouse );
	REGISTER_CLASS( this, MISSION_COMMAND_WAREHOUSE, CICWarehouse );
	
	REGISTER_CLASS( this, MISSION_COMMAND_ADDRESS_BOOK, CICMPAddressBook );
	REGISTER_CLASS( this, MISSION_INTERFACE_ADDRESS_BOOK, CInterfaceMPAddressBook );

	
	REGISTER_CLASS( this, MISSION_INTERFACE_IM_SAVE_MISSION, CInterfaceIMSaveMission );
	REGISTER_CLASS( this, MISSION_COMMAND_IM_SAVE_MISSION, CICIMSaveMission );
	
	REGISTER_CLASS( this, GAMETT_LOAD_HELPER, CLoadHelper );
	REGISTER_CLASS( this, GAMETT_MESSAGE_ATOM_REACTION_PAUSE, CMessageAtomReactionPause );
	REGISTER_CLASS( this, GAMETT_MESSAGE_ATOM_REACTION_MESSAGE_TO_INPUT, CMessageAtomReactionMessageToInput );
	REGISTER_CLASS( this, GAMETT_MESSAGE_ATOM_REACTION_MESSAGE_TO_MAINLOOP, CMessageAtomReactionMessageToMainLoop );
	REGISTER_CLASS( this, GAMETT_MESSAGE_REACTION, CMessageReaction );
	REGISTER_CLASS( this, GAMETT_MESSAGELINK, CMessageLink );
	REGISTER_CLASS( this, GAMETT_MESSAGELINK_CONTAINER, CMessageLinkContainer );

	REGISTER_CLASS( this, GAMETT_MESSAGE_ATOM_REACTION_SETGLOBALVAR, CMessageAtomReactionSetGlobalVar );
	REGISTER_CLASS( this, GAMETT_MESSAGE_ATOM_REACTION_REMOVEGLOBALVAR, CMessageAtomReactionRemoveGlobalVar );
	REGISTER_CLASS( this, GAMETT_MESSAGE_ATOM_REACTION_CUSTOM, CMessageAtomReactionCustom )
	REGISTER_CLASS( this, GAMETT_MESSAGE_ATOM_REACTION_SET_WINDOWTEXT, CMessageAtomReactionSetWindowText )
	REGISTER_CLASS( this, GAMETT_MESSAGE_ATOM_REACTION_NOP, CMessageAtomReactionNOP );
	REGISTER_CLASS( this, GAMETT_MESSAGE_ATOM_REACTION_SET_WINDOWTEXT_FROM_GLOBALVAR, CMessageAtomReactionSetWindowTextFromGlobalVar );
	
	REGISTER_CLASS( this, GAMETT_STATS_SORTER, CInterfaceStats::CSorter );
	REGISTER_CLASS( this, MISSION_INTERFACE_UNIT_PERFORMANCE, CInterfaceUnitPerformance );
	REGISTER_CLASS( this, MISSION_COMMAND_UNIT_PERFORMANCE, CICUnitPerformance );
	
	REGISTER_CLASS( this, MISSION_INTERFACE_AFTERMISSION_POPUPS, CAfterMissionPopups );

	REGISTER_CLASS( this, MISSION_COMMAND_MODS_LIST, CICIMModsList );
	REGISTER_CLASS( this, MISSION_INTERFACE_MODS_LIST, CInterfaceIMModsList );
	
	REGISTER_CLASS( this, MISSION_INTERFACE_SWITCH_MODE_TO, CInterfaceSwitchModeTo );
	REGISTER_CLASS( this, MISSION_COMMAND_SWITCH_MODE_TO, CICSwitchModeTo );
	
	REGISTER_CLASS( this, MISSION_INTERFACE_MESSAGE_BOX, CInterfaceMessageBox );
	REGISTER_CLASS( this, MISSION_COMMAND_MESSAGE_BOX, CICMessageBox );

	REGISTER_CLASS( this, MISSION_INTERFACE_NEW_DEPOTUPGRADES, CInterfaceNewDepotUpgrades );
	REGISTER_CLASS( this, MISSION_COMMAND_NEW_DEPOTUPGRADES, CICNewDepotUpgrades );
	REGISTER_CLASS( this, MISSION_SCORES_STATE_GAME, CInterfaceMission::CMultiplayerScoresSmall::CGameScoresState );
	REGISTER_CLASS( this, MISSION_SCORES_STATE_REPLAY, CInterfaceMission::CMultiplayerScoresSmall::CReplayScoresState )
}
static SModuleDescriptor theModuleDescriptor( "Main game logic", MISSION_BASE_VALUE, 0x0100, &theMissionObjectFactory, 0 );
extern "C" BK_EXPORT const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
