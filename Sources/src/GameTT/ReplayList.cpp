#include "StdAfx.h"
#include "../StreamIO/ProfilePaths.h"
#include "../Platform/LegacyText.h"

#include "CommonId.h"
#include "ReplayList.h"
#include "MultiplayerCommandManager.h"

#include "../Main/CommandsHistoryInterface.h"
#include "../Main/GameStats.h"
#include "../Main/Transceiver.h"

#include "../GameTT/MultiplayerCommandManager.h"

#include "../StreamIO/RandomGen.h"
#include "../Main/ScenarioTracker.h"
#include "../Common/PauseGame.h"
void CReplayList::PostCreate( IMainLoop *pML, CInterfaceReplayList *pILM )
{
	if ( !szFileNameToLoad.empty() )
		pILM->SetFileName( szFileNameToLoad.c_str() );
	pML->PushInterface( pILM );
}
CInterfaceReplayList::~CInterfaceReplayList()
{
}
void CInterfaceReplayList::SetFileName( const char *pszFullFileName ) 
{ 
	if ( pszFullFileName )
	{
		ICommandsHistory *pCommandsHistory = GetSingleton<ICommandsHistory>();
		pCommandsHistory->Load( pszFullFileName );

		const std::string szMapName = GetGlobalVar( "Map.Current.Name", "" );
		NI_ASSERT_T( !szMapName.empty(), "Error in CInterfaceReplayList::OnOk(): MapName is empty" );
		if ( !szMapName.empty() )
		{
			UnRegisterSingleton( ITransceiver::tidTypeID );
			ITransceiver *pTrans = 0;
			if ( std::string( GetGlobalVar( "MultiplayerGame" ) ).size() != 0 )
			{
				pTrans = CreateObject<ITransceiver>( MAIN_MP_TRANSCEIVER );
				pTrans->Init( GetSingletonGlobal(), EMCT_LAN );
			}
			else
			{
				pTrans = CreateObject<ITransceiver>( MAIN_SP_TRANSCEIVER );
				pTrans->Init( GetSingletonGlobal(), EMCT_NONE );
			}

			RegisterSingleton( ITransceiver::tidTypeID, pTrans );
			pTrans->LoadAllGameParameters();
			GetSingleton<IGameTimer>()->PauseGame( true, PAUSE_TYPE_PREMISSION );
			FinishInterface( MISSION_COMMAND_MISSION, szMapName.c_str() );
		}
	}
}
void CInterfaceReplayList::StartInterface()
{
	CInterfaceBaseList::StartInterface();
	DisplayError();
}
void CInterfaceReplayList::DisplayError()
{
	const int nReplayError = GetGlobalVar( "ReplayError", 0 );
	switch( nReplayError )
	{
	case ERR_NO_ERROR:
		break;
	case ERR_BAD_RESOURCES:
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX,
			NStr::Format( "%s;%s;0;", "Textes\\UI\\Intermission\\LoadReplay\\message_box_error_caption",
														 "Textes\\UI\\Intermission\\LoadReplay\\message_box_error_badresources" ) );
		break;
	case ERR_BAD_MAP:
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX,
			NStr::Format( "%s;%s;0;", "Textes\\UI\\Intermission\\LoadReplay\\message_box_error_caption",
														 "Textes\\UI\\Intermission\\LoadReplay\\message_box_error_badmap" ) );
		break;
	}

	RemoveGlobalVar( "ReplayError" );
}
bool CInterfaceReplayList::Init()
{
	fileMasks.clear();
	fileMasks.push_back( "*.rpl" );
	szTopDir = std::string( GetSingleton<IMainLoop>()->GetBaseDir() ) + NProfile::Segment();
	const std::string szModname = GetSingleton<IUserProfile>()->GetMOD();
	if ( !szModname.empty() )
	{
		szTopDir += "mods\\";
		szTopDir += szModname;
	}
	szTopDir += "replays\\";
	szCurrentDir = szTopDir;
	szInterfaceName = "ui\\Lists\\IMReplayList";
	nSortType = E_SORT_BY_TIME;
	CInterfaceBaseList::Init();
	return true;
}
bool CInterfaceReplayList::FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem )
{
	IUIElement *pElement = pRow->GetElement( 1 );
	if ( !pElement )
		return false;
	
	std::string szVal = GetFileChangeTimeString( szFullFileName.c_str() );
	const std::wstring wszVal = NStr::ToUnicode( szVal );
	pElement->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszVal.c_str() ) ) );
	return true;
}
bool CInterfaceReplayList::OnOk( const std::string &szFullFileName )
{
	ICommandsHistory *pCommandsHistory = GetSingleton<ICommandsHistory>();
	if ( pCommandsHistory->Load( szFullFileName.c_str() ) )
	{
		GetSingleton<IMainLoop>()->Command( 
			MISSION_COMMAND_SWITCH_MODE_TO,
			NStr::Format( "%s;%s;%d;%s;0", // 0 -not silent switch
			pCommandsHistory->GetModName(),
			pCommandsHistory->GetModVersion(),
			MISSION_COMMAND_REPLAY_LIST,
			szFullFileName.c_str() )
		);
		pCommandsHistory->Clear();
	}
	else
	{
		pCommandsHistory->Clear();
		DisplayError();
	}
	

	return true;
}
bool CInterfaceReplayList::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceBaseList::ProcessMessage( msg ) )
		return true;

	switch ( msg.nEventID )
	{
	case IMC_CANCEL:
		{
			CloseInterface();
			return true;
		}
	}

	return false;
}
