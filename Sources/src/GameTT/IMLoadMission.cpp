#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "CommonId.h"
#include "IMLoadMission.h"
#include "MultiplayerCommandManager.h"
#include "../Main/iMainCommands.h"
#include "../Main/ScenarioTracker.h"
void CICIMLoadMission::PostCreate( IMainLoop *pML, CInterfaceIMLoadMission *pILM )
{
	pML->PushInterface( pILM );
}
CInterfaceIMLoadMission::~CInterfaceIMLoadMission()
{
}
bool CInterfaceIMLoadMission::Init()
{
	fileMasks.clear();
	fileMasks.push_back( "*.sav" );
	szTopDir = GetSingleton<IMainLoop>()->GetBaseDir();
	const std::string szModname = GetSingleton<IUserProfile>()->GetMOD();
	if ( !szModname.empty() )
	{
		szTopDir += "mods\\";
		szTopDir += szModname;
	}
	szTopDir += "saves\\";
	szCurrentDir = szTopDir;
	szInterfaceName = "ui\\Lists\\IMLoadMission";
	nSortType = E_SORT_BY_TIME;
	nFirstSortColumn = 1;
	CInterfaceBaseList::Init();
	return true;
}
bool CInterfaceIMLoadMission::FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem )
{
	IUIElement *pElement = pRow->GetElement( 1 );
	if ( !pElement )
		return false;

	std::string szVal = GetFileChangeTimeString( szFullFileName.c_str() );
	const std::wstring wszVal = NStr::ToUnicode( szVal );
	pElement->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszVal.c_str() ) ) );
	return true;
}
bool CInterfaceIMLoadMission::OnOk( const std::string &szFullFileName )
{
	IMainLoop *pML = GetSingleton<IMainLoop>();
	std::string szShortName = szFullFileName.substr( szTopDir.size() );
	CloseInterface();
	pML->Command( MAIN_COMMAND_LOAD, szShortName.c_str() );
	pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_GAME_UNPAUSE_MENU) );	//уберем паузу
	return true;
}
bool CInterfaceIMLoadMission::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceBaseList::ProcessMessage( msg ) )
		return true;

	switch ( msg.nEventID )
	{
/*
		case IMC_SELECTION_CHANGED:
			if ( !bLoadGameIM )
			{
				IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		//should be List Control
				IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
				if ( !pList )
					return true;			//не нашелся list control
				int nSave = pList->GetSelectionItem();
				if ( nSave == -1 )
					return true;
				
				IUIListRow *pSelRow = pList->GetItem( nSave );
				std::string szEdit = szSaves[ pSelRow->GetUserData() ];
				pElement = pUIScreen->GetChildByID( 2000 );
				pElement->SetWindowText( 0, NStr::ToUnicode(szEdit).c_str() );
			}
			return true;
*/
		
		case IMC_CANCEL:
			{
				CloseInterface();
/*
				pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_GAME_UNPAUSE_MENU) );	//уберем паузу
				pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", MC_SHOW_ESCAPE_MENU) );	//покажем escape menu
*/
				return true;
			}

/*
		case IMC_OK:
			{
				IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		//should be List Control
				IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
				if ( !pList )
					return true;			//не нашелся list control
				int nSave = pList->GetSelectionItem();
				if ( nSave == -1 )
					return true;
				
				IUIListRow *pSelRow = pList->GetItem( nSave );
				std::string szEdit = szSaves[ pSelRow->GetUserData() ];
				IMainLoop *pML = GetSingleton<IMainLoop>();
				CloseInterface();
				szEdit += ".sav";
				pML->Command( MAIN_COMMAND_LOAD, szEdit.c_str() );
				pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_GAME_UNPAUSE_MENU) );	//уберем паузу
				return true;
			}
	*/
	}

	return false;
}
