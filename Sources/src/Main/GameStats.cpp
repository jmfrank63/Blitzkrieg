#include "StdAfx.h"

#include "GameStats.h"
int SBasicGameStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "KeyName", &szKeyName );
	saver.Add( "StatsType", &szStatsType );
	saver.Add( "HeaderText", &szHeaderText );
	saver.Add( "SubheaderText", &szSubheaderText );
	saver.Add( "DescriptionText", &szDescriptionText );
	if ( saver.IsReading() ) 
	{
		NStr::ToLower( szHeaderText );
		NStr::ToLower( szSubheaderText );
		NStr::ToLower( szDescriptionText );
	}
	return 0;
}
int SCommonGameStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SBasicGameStats*>(this) );
	saver.Add( "MapImage", &szMapImage );
	saver.Add( "MapImageRect", &mapImageRect );
	if ( saver.IsReading() ) 
		NStr::ToLower( szMapImage );
	return 0;
}
int SMissionStats::SObjective::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Header", &szHeader );
	saver.Add( "DescriptionText", &szDescriptionText );
	saver.Add( "PosOnMap", &vPosOnMap );
	saver.Add( "Secret", &bSecret );
	saver.Add( "AnchorScriptID", &nAnchorScriptID );
	if ( saver.IsReading() ) 
	{
		NStr::ToLower( szHeader );
		NStr::ToLower( szDescriptionText );
	}
	return 0;
}
int SMissionStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SCommonGameStats*>(this) );
	saver.Add( "TemplateMap", &szTemplateMap );
	saver.Add( "FinalMap", &szFinalMap );
	saver.Add( "CombatMusics", &combatMusics );
	saver.Add( "ExplorMusics", &explorMusics );
	saver.Add( "Objectives", &objectives );
	saver.Add( "SettingName", &szSettingName );
	saver.Add( "MODName", &szMODName );
	saver.Add( "MODVersion", &szMODVersion );
	if ( saver.IsReading() ) 
	{
		NStr::ToLower( szTemplateMap );
		NStr::ToLower( szFinalMap );
	}
	return 0;
}
void SChapterStats::SMission::RetrieveShortcuts( IObjectsDB *pGDB )
{
	pMission = static_cast<const SMissionStats*>( pGDB->GetGameStats( szMission.c_str(), IObjectsDB::MISSION ) );
}
void SChapterStats::RetrieveShortcuts( IObjectsDB *pGDB )
{
	for ( std::vector<SMission>::iterator it = missions.begin(); it != missions.end(); ++it )
		it->RetrieveShortcuts( pGDB );
}
void SChapterStats::RemoveTemplateMissions()
{
	for ( std::vector<SMission>::iterator it = missions.begin(); it != missions.end(); )
	{
		NI_ASSERT_T( it->pMission != 0, NStr::Format( "Invalid mission stats: \"%s\"", it->szMission.c_str() ) );
		if ( it->pMission->IsTemplate() )
		{
			/**
			const std::string szVarName = "Mission." + it->pMission->szParentName + ".Random.Generated";
			RemoveGlobalVar( szVarName.c_str() );
			NStr::DebugTrace( "SChapterStats::RemoveTemplateMissions(), remove Template Mission: %s\n", it->pMission->szParentName.c_str() );
			/**/
			it = missions.erase( it );
		}
		else
			++it;
	}
}
void SChapterStats::AddMission( const SChapterStats::SMission &mission )
{
	missions.push_back( mission );
}
int SChapterStats::SMission::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Mission", &szMission );
	saver.Add( "PosOnMap", &vPosOnMap );
	saver.Add( "Difficulty", &nMissionDifficulty );
	saver.Add( "MissionBonus", &szMissionBonus );
	saver.Add( "AllBonuses", &szAllBonuses );
	if ( saver.IsReading() ) 
		NStr::ToLower( szMission );
	return 0;
}
int SChapterStats::SMission::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szMission );
	saver.Add( 2, &vPosOnMap );
	saver.Add( 3, &nMissionDifficulty );
	saver.Add( 4, &szMissionBonus );
	saver.Add( 5, &szAllBonuses );
	if ( saver.IsReading() ) 
		pMission = static_cast<const SMissionStats*>( GetSingleton<IObjectsDB>()->GetGameStats( szMission.c_str(), IObjectsDB::MISSION ) );
	return 0;
}
int SChapterStats::SPlaceHolder::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Position", &vPosOnMap );
	return 0;
}
int SChapterStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SCommonGameStats*>(this) );
	saver.Add( "Season", &nSeason );
	saver.Add( "InterfaceMusic", &szInterfaceMusic );
	saver.Add( "Missions", &missions );
	saver.Add( "PlaceHolders", &placeHolders );
	saver.Add( "Script", &szScript );
	saver.Add( "SettingName", &szSettingName );
	saver.Add( "ContextName", &szContextName );
	saver.Add( "PlayerSide", &szSideName );
	saver.Add( "MODName", &szMODName );
	saver.Add( "MODVersion", &szMODVersion );
	if ( saver.IsReading() ) 
	{
		NStr::ToLower( szInterfaceMusic );
		NStr::ToLower( szScript );
		NStr::ToLower( szContextName );
	}
	return 0;
}
void SCampaignStats::SChapter::RetrieveShortcuts( IObjectsDB *pGDB )
{
	pChapter = static_cast<const SChapterStats*>( pGDB->GetGameStats( szChapter.c_str(), IObjectsDB::CHAPTER ) );
}
void SCampaignStats::RetrieveShortcuts( IObjectsDB *pGDB )
{
	for ( std::vector<SChapter>::iterator it = chapters.begin(); it != chapters.end(); ++it )
		it->RetrieveShortcuts( pGDB );
}
int SCampaignStats::SChapter::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Chapter", &szChapter );
	saver.Add( "PosOnMap", &vPosOnMap );
	saver.Add( "Visible", &bVisible );
	saver.Add( "Secret", &bSecret );
	if ( saver.IsReading() ) 
		NStr::ToLower( szChapter );
	return 0;
}
int SCampaignStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SCommonGameStats*>(this) );
	saver.Add( "IntroMovie", &szIntroMovie );
	saver.Add( "OutroMovie", &szOutroMovie );
	saver.Add( "InterfaceMusic", &szInterfaceMusic );
	saver.Add( "AllChapters", &chapters );
	saver.Add( "Templates", &templateMissions );
	saver.Add( "PlayerAllianceSide", &szSideName );
	saver.Add( "MODName", &szMODName );
	saver.Add( "MODVersion", &szMODVersion );
	if ( saver.IsReading() )
	{
		NStr::ToLower( szIntroMovie );
		NStr::ToLower( szOutroMovie );
		NStr::ToLower( szInterfaceMusic );
		for ( std::vector<std::string>::iterator it = templateMissions.begin(); it != templateMissions.end(); ++it )
			NStr::ToLower( *it );
	}
	return 0;
}
int SMedalStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SBasicGameStats*>(this) );
	saver.Add( "Texture", &szTexture );
	saver.Add( "ImageRect", &mapImageRect );
	saver.Add( "PicturePos", &vPicturePos );
	saver.Add( "TextPos", &vTextCenterPos );
	if ( saver.IsReading() ) 
		NStr::ToLower( szTexture );
	return 0;
}
