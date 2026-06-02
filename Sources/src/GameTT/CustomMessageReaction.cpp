#include "StdAfx.h"

#include "CustomMessageReaction.h"
#include "..\Main\IMain.h"
#include "..\Main\ScenarioTracker.h"
#include "..\Main\GameStats.h"
#include "iMission.h"
#include "..\Scene\Scene.h"
#include "..\Main\Transceiver.h"
#include "..\Input\Input.h"
#include "..\Common\InterfaceScreenBase.h"
#include "..\Main\TextSystem.h"
#include "..\GameTT\UIConsts.h"
#include "..\GameTT\MultiplayerCommandManager.h"
#include "..\StreamIO\Globals.h"
void ConstructWhoWin( class CInterfaceScreenBase *_pInterface )
{
	ITextManager *pTM = GetSingleton<ITextManager>();
	
	std::wstring szMessage;
	const int nWinPartyName = GetGlobalVar( "temp.Multiplayer.Win.Partyname", 2 );
	
	IText *pBefore = pTM->GetString( "escape_mission_replay_win_beforeparty" );
	IText *pAfter = pTM->GetString( "escape_mission_replay_win_afterparty" );
	IText *pText = pTM->GetString( "escape_mission_fail_message_draw_mp" );

	if ( nWinPartyName != 2 )
	{
		if ( pBefore )
			szMessage += MakeWideStringFromWordString( pBefore->GetString() );
		
		if ( nWinPartyName == 0 )
			szMessage += MakeWideStringFromWordString( CUIConsts::GetLocalPartyName( GetGlobalVar( "Multiplayer.Side0.Name", "" ) ) );
		else
			szMessage += MakeWideStringFromWordString( CUIConsts::GetLocalPartyName( GetGlobalVar( "Multiplayer.Side1.Name", "" ) ) );

		if ( pAfter )
			szMessage += MakeWideStringFromWordString( pAfter->GetString() );
	}
	else
	{
		if ( pText )
			szMessage += MakeWideStringFromWordString( pText->GetString() );
	}

	SetGlobalVar( "temp.Replay.WinMessage", reinterpret_cast<const WORD*>( szMessage.c_str() ) );
}
void ReactionSetMultiplayerTimeout( class CInterfaceScreenBase *_pInterface )
{
	GetSingleton<ITransceiver>()->CommandTimeOut( true );
}
void ReactionRemoveMultiplayerTimeout( class CInterfaceScreenBase *_pInterface )
{
	GetSingleton<ITransceiver>()->CommandTimeOut( false );
}
bool OnDemoVersionEndMission( class CInterfaceScreenBase *pInterface, bool bWin = true )
{
	if ( GetGlobalVar( "demoversion", 0 ) )
	{
		SetGlobalVar( "demoversion.Win", bWin );
		pInterface->FinishInterface( MISSION_COMMAND_STATS, "2"/*STATS_COMPLEXITY_MISSION*/ );
		return true;
	}
	return false;
}
void ReactionRestartMisssion( class CInterfaceScreenBase *_pInterface )
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
}
void ReactionQuitMission( class CInterfaceScreenBase *pInterface )
{
	GetSingleton<IScenarioTracker>()->FinishMission( MISSION_FINISH_ABORT );
	GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d 0", MAIN_SP_TRANSCEIVER) );
	
	NI_ASSERT_T( pInterface != 0, "EMPTY MISSION INTERFACE" );
	pInterface->FinishInterface( MISSION_COMMAND_MAIN_MENU, 0 );
	
	GetSingleton<IGlobalVars>()->RemoveVarsByMatch( "temp." );
	if ( GetGlobalVar("History.Playing", 0) != 0 ) 
	{
		GetSingleton<IMainLoop>()->RestoreScenarioTracker();
		RemoveGlobalVar( "History.Playing" );
		RemoveGlobalVar( "MultiplayerGame" );

		UnRegisterSingleton( ITransceiver::tidTypeID );
		ITransceiver *pTrans = CreateObject<ITransceiver>( MAIN_SP_TRANSCEIVER );
		RegisterSingleton( ITransceiver::tidTypeID, pTrans );
	}
}
void ReactionExitToWindows( class CInterfaceScreenBase *_pInterface )
{
	GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_EXIT_GAME, 0 );
}
void ReactionAutosaveMissionEnd( class CInterfaceScreenBase *pInterface )
{
	std::string szSaveName;
	szSaveName += CUIConsts::GetCampaignNameAddition();
	szSaveName += " Mission End Auto";
	szSaveName += ".sav";
	GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_SAVE, NStr::Format( "%s;1", szSaveName.c_str() ) );
}
void ReactionWin( class CInterfaceScreenBase *pInterface )
{
	NI_ASSERT_T( pInterface != 0, "EMPTY MISSION INTERFACE" );

	if ( GetGlobalVar( "temp.Mission.Current.Aborted", 0 ) )
		GetSingleton<IScenarioTracker>()->FinishMission( MISSION_FINISH_ABORT );
	else
		GetSingleton<IScenarioTracker>()->FinishMission( MISSION_FINISH_WIN );	

	GetSingleton<IGlobalVars>()->RemoveVarsByMatch( "temp." );
	
	if ( OnDemoVersionEndMission( pInterface ) ) return;
	const std::string szChapterName = GetGlobalVar( "Chapter.Current.Name", "" );
	if ( GetGlobalVar( "MultiplayerGame", 0 ) || szChapterName == "custom_mission" )
	{	
		GetSingleton<ITransceiver>()->GameFinished();
		pInterface->FinishInterface( MISSION_COMMAND_STATS, "2"/*STATS_COMPLEXITY_MISSION*/ );
	}
	else
	{
		pInterface->FinishInterface( MISSION_COMMAND_PLAYERS_STATS, "1" );
	}
}
void ReactionLoose( class CInterfaceScreenBase *pInterface )
{
	GetSingleton<IGlobalVars>()->RemoveVarsByMatch( "temp." );
	GetSingleton<IScenarioTracker>()->FinishMission( MISSION_FINISH_LOSE );
	NI_ASSERT_T( pInterface != 0, "EMPTY MISSION INTERFACE" );

	if ( OnDemoVersionEndMission( pInterface, false ) ) return;

	if ( GetGlobalVar( "MultiplayerGame", 0 ) )
	{
		GetSingleton<ITransceiver>()->GameFinished();
		pInterface->FinishInterface( MISSION_COMMAND_STATS, "2"/*STATS_COMPLEXITY_MISSION*/ );
	}
	else
	{
		pInterface->FinishInterface( MISSION_COMMAND_PLAYERS_STATS, "1" );
	}
}
void CCustomMessageReaction::LaunchReaction( const std::string &szCutomReactionName, class CInterfaceScreenBase *_pInterface  )
{
	CReactions::iterator it = reactions.find( szCutomReactionName );
	if ( it != reactions.end() )
	{
		(*it->second.pfnReaction)( _pInterface );
		return;
	}
	NI_ASSERT_T( FALSE, NStr::Format( "not found custom reaction \"%s\"", szCutomReactionName.c_str() ) );

}
void CCustomMessageReaction::Clear()
{
	reactions.clear();
}
void CCustomMessageReaction::Init()
{
	reactions["ReactionRestartMisssion"].pfnReaction =	ReactionRestartMisssion;
	reactions["ReactionQuitMission"].pfnReaction = ReactionQuitMission;
	reactions["ReactionExitToWindows"].pfnReaction =	ReactionExitToWindows;
	reactions["ReactionWin"].pfnReaction =	ReactionWin;
	reactions["ReactionLoose"].pfnReaction =	ReactionLoose;
	reactions["ReactionSetMultiplayerTimeout"].pfnReaction =	ReactionSetMultiplayerTimeout;
	reactions["ReactionRemoveMultiplayerTimeout"].pfnReaction =	ReactionRemoveMultiplayerTimeout;
	reactions["ConstructWhoWin"].pfnReaction =	ConstructWhoWin;
	reactions["ReactionAutosaveMissionEnd"].pfnReaction =	ReactionAutosaveMissionEnd;
}