#include "StdAfx.h"

#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\Main\GameStats.h"
#include "..\Main\ScenarioTracker.h"
#include "CommonId.h"
#include "CustomMission.h"

CInterfaceCustomMission::~CInterfaceCustomMission()
{
}
bool CInterfaceCustomMission::Init()
{
	fileMasks.clear();
	fileMasks.push_back( "*.xml" );
	szTopDir = "";
	szTopDir += "scenarios\\custom\\missions\\";
	szCurrentDir = szTopDir;
	szInterfaceName = "ui\\Lists\\IMCustomMission";
	nSortType = E_SORT_BY_NAME;
	bStorageFiles = true;
	szCollectorName = "custom_missions";
	CInterfaceCustomList::Init();
	return true;
}
bool CInterfaceCustomMission::FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem )
{
	std::string szMissionName = szFullFileName.substr( 0, szFullFileName.rfind('.') );
	const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( szMissionName.c_str(), IObjectsDB::MISSION );
	IUIElement *pElement = 0;
	
	SQuickLoadMapInfo mapInfo;
	std::string szMapName = "maps\\";
	szMapName += pStats->szFinalMap;
	bool bRes = LoadLatestDataResource( szMapName, ".bzm", RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER, RMGC_QUICK_LOAD_MAP_INFO_NAME, mapInfo );
	if ( bRes )
	{
		pElement = pRow->GetElement( 1 );
		if ( !pElement )
			return false;
		const std::wstring wszSizeX = NStr::ToUnicode( NStr::Format( "%d", mapInfo.size.x ) );
		pElement->SetWindowText( 0, reinterpret_cast<const WORD*>( wszSizeX.c_str() ) );
	}

	pElement = pRow->GetElement( 2 );
	if ( !pElement )
		return false;
	const std::wstring wszPlayers = NStr::ToUnicode( NStr::Format( "%d", mapInfo.playerParties.size() ) );
	pElement->SetWindowText( 0, reinterpret_cast<const WORD*>( wszPlayers.c_str() ) );

	return true;
}
bool CInterfaceCustomMission::OnOk( const std::string &szFullFileName )
{
	std::string szBase = GetSingleton<IDataStorage>()->GetName();
	std::string szShortName = szFullFileName.substr( szBase.size() );
	szShortName = szShortName.substr( 0, szShortName.rfind('.') );
	
	const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( szShortName.c_str(), IObjectsDB::MISSION );
	NI_ASSERT_T( pStats != 0, (std::string("Invalid custom mission ") + szFullFileName).c_str() );
	if ( !pStats )
		return true;
	
	SetGlobalVar( "Chapter.Current.Name", "custom_mission" );
	SetGlobalVar( "Mission.Current.Name", szShortName.c_str() );

	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	pST->StartCampaign( "custom_mission", CAMPAIGN_TYPE_CUSTOM_MISSION );
	pST->StartChapter( "custom_mission" );

	GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_SWITCH_MODE_TO,
				NStr::Format( "%s;%s;%d;%s;0",	// 0 - not silent switch
												pStats->szMODName.c_str(),
												pStats->szMODVersion.c_str(),
												MISSION_COMMAND_MISSION,
												(pStats->szFinalMap + ".xml").c_str() ) );

	return true;
}
