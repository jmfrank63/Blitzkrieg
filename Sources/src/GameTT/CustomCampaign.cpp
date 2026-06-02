#include "StdAfx.h"

#include "..\Main\GameStats.h"
#include "..\Main\ScenarioTracker.h"
#include "CommonId.h"
#include "CustomCampaign.h"

CInterfaceCustomCampaign::~CInterfaceCustomCampaign()
{
}
bool CInterfaceCustomCampaign::Init()
{
	fileMasks.clear();
	fileMasks.push_back( "*.xml" );
	szTopDir = "";
	szTopDir += "scenarios\\custom\\campaigns\\";
	szCurrentDir = szTopDir;
	szInterfaceName = "ui\\Lists\\IMCustomCampaign";
	nSortType = E_SORT_BY_NAME;
	bStorageFiles = true;
	szCollectorName = "custom_campaigns";
	CInterfaceCustomList::Init();
	return true;
}
bool CInterfaceCustomCampaign::FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem )
{
	std::string szCampaignName = szFullFileName.substr( 0, szFullFileName.rfind('.') );
	const SCampaignStats *pStats = NGDB::GetGameStats<SCampaignStats>( szCampaignName.c_str(), IObjectsDB::CAMPAIGN );
	if ( (pStats == 0) || pStats->szSideName.empty() ) 
		return false;
	IUIElement *pElement = pRow->GetElement( 1 );
	if ( !pElement )
		return false;
	ITextManager *pTextM = GetSingleton<ITextManager>();
	std::string szKey = "textes\\opponents\\";
	szKey += pStats->szSideName;
	CPtr<IText> pText = pTextM->GetDialog( szKey.c_str() );
	NI_ASSERT_T( pText != 0, NStr::Format("Can not get text by key %s", szKey.c_str() ) );
	if ( pText )
		pElement->SetWindowText( 0, pText->GetString() );
	
	pElement = pRow->GetElement( 2 );
	if ( !pElement )
		return false;
	const std::wstring wszChapters = NStr::ToUnicode( NStr::Format( "%d", pStats->chapters.size() ) );
	pElement->SetWindowText( 0, reinterpret_cast<const WORD*>( wszChapters.c_str() ) );

	return true;
}
bool CInterfaceCustomCampaign::OnOk( const std::string &szFullFileName )
{
	std::string szBase = GetSingleton<IDataStorage>()->GetName();
	std::string szShortName = szFullFileName.substr( szBase.size() );
	szShortName = szShortName.substr( 0, szShortName.rfind('.') );
	
	const SCampaignStats *pStats = NGDB::GetGameStats<SCampaignStats>( szShortName.c_str(), IObjectsDB::CAMPAIGN );
	NI_ASSERT_T( pStats != 0, (std::string("Invalid custom campaign ") + szFullFileName).c_str() );
	if ( !pStats )
		return true;
	
	SetGlobalVar( "Campaign.Current.Name", szShortName.c_str() );
	
	GetSingleton<IScenarioTracker>()->StartCampaign( szShortName, CAMPAIGN_TYPE_CUSTOM_CAMPAIGN );

	GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_SWITCH_MODE_TO,
				NStr::Format( "%s;%s;%d;%d;0",	// 0 - not silent switch
												pStats->szMODName.c_str(),
												pStats->szMODVersion.c_str(),
												MISSION_COMMAND_CAMPAIGN,
												0 ) );

	return true;
}
