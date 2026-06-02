#include "StdAfx.h"

#include "..\Main\GameStats.h"
#include "CommonId.h"
#include "TutorialList.h"
#include "..\Main\ScenarioTracker.h"
bool CInterfaceTutorialList::Init()
{
	fileMasks.clear();
	fileMasks.push_back( "*.xml" );
	szTopDir = "";
	szTopDir += "scenarios\\tutorials\\";
	szCurrentDir = szTopDir;
	szInterfaceName = "ui\\Lists\\IMTutorialList";
	nSortType = E_SORT_BY_NAME;
	bStorageFiles = true;
	szCollectorName = "tutorial";
	CInterfaceCustomList::Init();
	return true;
}
bool CInterfaceTutorialList::FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem )
{
/*
IUIElement *pElement = pRow->GetElement( 1 );
if ( !pElement )
return false;

	std::string szVal = GetFileChangeTimeString( szFullFileName.c_str() );
	pElement->SetWindowText( 0, NStr::ToUnicode( szVal ).c_str() );
	*/
	return true;
}
bool CInterfaceTutorialList::OnOk( const std::string &szFullFileName )
{
	std::string szDir = GetSingleton<IDataStorage>()->GetName();
	std::string szPrefix = szFullFileName.substr( 0, szDir.size() );
	if ( szDir == szPrefix )
		szPrefix = szFullFileName.substr( szDir.size() );
	else
		szPrefix = szFullFileName;
	szPrefix = szPrefix.substr( 0, szPrefix.rfind('.') );
	const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( szPrefix.c_str(), IObjectsDB::MISSION );
	NI_ASSERT_T( pStats != 0, (std::string("Can not get tutorial mission stats by key ") + szPrefix).c_str() );

	SetGlobalVar( "Chapter.Current.Name", "tutorial" );
	SetGlobalVar( "Mission.Current.Name", szPrefix.c_str() );
	SetGlobalVar( "TutorialMode", 1 );

	GetSingleton<IScenarioTracker>()->StartCampaign( "tutorial", CAMPAIGN_TYPE_TUTORIAL );
	GetSingleton<IScenarioTracker>()->StartChapter( "tutorial" );
	
	FinishInterface( MISSION_COMMAND_MISSION, pStats->szFinalMap.c_str() );
	return true;
}
