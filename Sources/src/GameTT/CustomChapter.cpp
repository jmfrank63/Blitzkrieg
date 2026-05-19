#include "StdAfx.h"

#include "..\Main\GameStats.h"
#include "..\Main\ScenarioTracker.h"
#include "CommonId.h"
#include "CustomChapter.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceCustomChapter::~CInterfaceCustomChapter()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceCustomChapter::Init()
{
	//инициализируем имена
	fileMasks.clear();
	fileMasks.push_back( "*.xml" );
	//	szTopDir = std::string( GetSingleton<IDataStorage>()->GetName() );
	szTopDir = "";
	szTopDir += "scenarios\\custom\\chapters\\";
	szCurrentDir = szTopDir;
	szInterfaceName = "ui\\Lists\\IMCustomChapter";
	nSortType = E_SORT_BY_NAME;
	bStorageFiles = true;
	szCollectorName = "custom_chapters";
	//
	CInterfaceCustomList::Init();
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceCustomChapter::FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem )
{
	std::string szChapterName = szFullFileName.substr( 0, szFullFileName.rfind('.') );
	const SChapterStats *pStats = NGDB::GetGameStats<SChapterStats>( szChapterName.c_str(), IObjectsDB::CHAPTER );
	if ( (pStats == 0) || pStats->szSideName.empty() ) 
		return false;
	IUIElement *pElement = pRow->GetElement( 1 );
	if ( !pElement )
		return false;
	ITextManager *pTextM = GetSingleton<ITextManager>();
	std::string szKey = "textes\\opponents\\";
	szKey += pStats->szSideName;
	CPtr<IText> pText = pTextM->GetDialog( szKey.c_str() );
	NI_ASSERT_T( pText != 0, NStr::Format("Can not get text by key %s", szKey.c_str()) );
	if ( pText )
		pElement->SetWindowText( 0, pText->GetString() );
	
	pElement = pRow->GetElement( 2 );
	if ( !pElement )
		return false;
	const std::wstring wszMissions = NStr::ToUnicode( NStr::Format( "%d", pStats->missions.size() ) );
	pElement->SetWindowText( 0, reinterpret_cast<const WORD*>( wszMissions.c_str() ) );

/*
	std::string szVal = GetFileChangeTimeString( szFullFileName.c_str() );
	pElement->SetWindowText( 0, NStr::ToUnicode( szVal ).c_str() );
*/

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceCustomChapter::OnOk( const std::string &szFullFileName )
{
	std::string szBase = GetSingleton<IDataStorage>()->GetName();
	std::string szShortName = szFullFileName.substr( szBase.size() );
	szShortName = szShortName.substr( 0, szShortName.rfind('.') );
	
	const SChapterStats *pStats = NGDB::GetGameStats<SChapterStats>( szShortName.c_str(), IObjectsDB::CHAPTER );
	NI_ASSERT_T( pStats != 0, (std::string("Invalid custom chapter ") + szFullFileName).c_str() );
	if ( !pStats )
		return true;

	SetGlobalVar( "Custom.Chapter", 1 );
	SetGlobalVar( "Chapter.Current.Name", szShortName.c_str() );

	IScenarioTracker *pST = GetSingleton<IScenarioTracker>();
	pST->StartCampaign( "custom_chapter", CAMPAIGN_TYPE_CUSTOM_CHAPTER );
	pST->GetUserPlayer()->SetSide( pStats->szSideName.c_str() );

	GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_SWITCH_MODE_TO,
				NStr::Format( "%s;%s;%d;%d;0",	// 0 - not silent switch
												pStats->szMODName.c_str(),
												pStats->szMODVersion.c_str(),
												MISSION_COMMAND_CHAPTER,
												0 ) );
	
	//FinishInterface( MISSION_COMMAND_CHAPTER, 0 );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
