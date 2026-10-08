// The chapter exporter: CChapterFrame::FillRPGStats and ExportFrameData
// (Sources/src/editor/ChapterFrm.cpp:70-340). SChapterStats is written under RPG
// through the engine's serializer; the map image goes through
// ComposeImageToTexture (DXT3 and ARGB4444, the frame's formats) beside the stats
// and the header, subheader and description .txt, the chapter script .lua and the
// context .xml are copied there.
//
// MFC's validation ran first and showed the last failing item's message in a
// box (each check overwrote szErrorMsg); the export fails with that message.
// As for the medal, the map image is checked before anything is written, so a
// missing picture leaves no half export and the error names the file, and a
// missing text, script or context is MFC's silent MyCopyFile failure that
// becomes a warning naming it.
#include "StdAfx.h"

#include "chapter_export.h"
#include "../../image_export.h"
#include "../tree_item_types.h"
#include "../../../Common/World.h"
#include "../../../Main/GameStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kChapterAddDir[] = "scenarios\\";

// The "You should specify ..." check of ExportFrameData. MFC kept the last
// failure; an empty string is a project that passes.
std::string Validate( const CTreeItem &commonProps, const CTreeItem &missions, const CTreeItem &places )
{
	std::string szErrorMsg;
	if ( ValueStr( commonProps, 0 ).empty() )
		szErrorMsg = "You should specify header text reference before exporting.\n";
	if ( ValueStr( commonProps, 1 ).empty() )
		szErrorMsg = "You should specify subheader text reference before exporting.\n";
	if ( ValueStr( commonProps, 2 ).empty() )
		szErrorMsg = "You should specify description text reference before exporting.\n";
	if ( ValueStr( commonProps, 3 ).empty() )
		szErrorMsg = "You should specify map image reference before exporting.\n";
	if ( ValueStr( commonProps, 4 ).empty() )
		szErrorMsg = "You should specify script reference before exporting.\n";
	if ( ValueStr( commonProps, 5 ).empty() )
		szErrorMsg = "You should specify interface music reference before exporting.\n";
	if ( ValueStr( commonProps, 7 ).empty() )
		szErrorMsg = "You should specify setting reference before exporting.\n";
	if ( ValueStr( commonProps, 6 ).empty() )
		szErrorMsg = "You should specify season before exporting.\n";
	if ( ValueStr( commonProps, 8 ).empty() )
		szErrorMsg = "You should specify context reference before exporting.\n";
	if ( ValueStr( commonProps, 9 ).empty() )
		szErrorMsg = "You should specify player side before exporting.\n";
	if ( missions.GetChildren().empty() )
		szErrorMsg = "You should specify some mission references before exporting.\n";
	for ( const auto &pMission : missions.GetChildren() )
		if ( ValueStr( *pMission, 0 ).empty() )
			szErrorMsg = "You should specify all mission references before exporting.\n";
	if ( places.GetChildren().empty() )
		szErrorMsg = "You should specify some placeholders before exporting.\n";
	return szErrorMsg;
}

// CChapterCommonPropsItem::GetSeason. MFC asserted on any other name; the
// export refuses it instead of writing a season the game would misread.
bool SeasonOf( const std::string &szSeason, int &nSeason )
{
	if ( szSeason == "summer" )
		nSeason = SEASON_SUMMER;
	else if ( szSeason == "winter" )
		nSeason = SEASON_WINTER;
	else if ( szSeason == "africa" )
		nSeason = SEASON_AFRIKA;
	else
		return false;
	return true;
}

}

bool ExportChapter( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_CHAPTER_ROOT_ITEM, "chapter", outcome );
	if ( !pProject )
		return false;
	const CTreeItem *pCommonProps = RequireChild( *pProject->root, ETIT_CHAPTER_COMMON_PROPS_ITEM, 0, "Basic info", outcome );
	const CTreeItem *pMissions = pCommonProps != nullptr ? RequireChild( *pProject->root, ETIT_CHAPTER_MISSIONS_ITEM, 0, "Missions", outcome ) : nullptr;
	const CTreeItem *pPlaces = pMissions != nullptr ? RequireChild( *pProject->root, ETIT_CHAPTER_PLACES_ITEM, 0, "Place holders", outcome ) : nullptr;
	if ( pPlaces == nullptr )
		return false;
	const std::string szInvalid = Validate( *pCommonProps, *pMissions, *pPlaces );
	if ( !szInvalid.empty() )
	{
		outcome.szError = "chapter export refused: " + szInvalid.substr( 0, szInvalid.find_last_not_of( '\n' ) + 1 );
		return false;
	}
	int nSeason = 0;
	if ( !SeasonOf( ValueStr( *pCommonProps, 6 ), nSeason ) )
	{
		outcome.szError = "chapter export refused: the season \"" + ValueStr( *pCommonProps, 6 ) + "\" is not summer, winter or africa";
		return false;
	}
	const std::string szHeader = ValueStr( *pCommonProps, 0 );
	const std::string szSubHeader = ValueStr( *pCommonProps, 1 );
	const std::string szDescription = ValueStr( *pCommonProps, 2 );
	const std::string szMapImage = ValueStr( *pCommonProps, 3 );
	const std::string szScript = ValueStr( *pCommonProps, 4 );
	const std::string szContext = ValueStr( *pCommonProps, 8 );

	const std::string szFile = StatsFileName( project, context, kChapterAddDir, false );
	const std::string szPrefix = DirectoryOf( szFile );
	const std::string szProjectDir = ProjectDirectory( context );
	// A save writes the cache with the frame's prefix still empty (SaveRPGStats runs outside
	// ExportFrameData, which sets it), so the cached paths are the tree's own values; the
	// export prefixes them. A cache that held the prefixed paths would reload into the tree.
	const std::string szStatsPrefix = context.bSaveCache ? std::string() : szPrefix;

	SChapterStats rpgStats;
	rpgStats.szHeaderText = szStatsPrefix + szHeader;
	rpgStats.szSubheaderText = szStatsPrefix + szSubHeader;
	rpgStats.szDescriptionText = szStatsPrefix + szDescription;
	rpgStats.szMapImage = szStatsPrefix + szMapImage;
	rpgStats.szScript = szStatsPrefix + szScript;
	rpgStats.szInterfaceMusic = szStatsPrefix + ValueStr( *pCommonProps, 5 );
	rpgStats.nSeason = nSeason;
	rpgStats.szSettingName = ValueStr( *pCommonProps, 7 );
	rpgStats.szContextName = szStatsPrefix + szContext;
	rpgStats.szSideName = ValueStr( *pCommonProps, 9 );
	rpgStats.szMODName = context.szModName;
	rpgStats.szMODVersion = context.szModVersion;
	for ( const auto &pMissionItem : pMissions->GetChildren() )
	{
		SChapterStats::SMission mission;
		mission.szMission = ValueStr( *pMissionItem, 0 );
		mission.vPosOnMap = CVec2( ValueFloat( *pMissionItem, 1 ), ValueFloat( *pMissionItem, 2 ) );
		rpgStats.missions.push_back( mission );
	}
	for ( const auto &pPlace : pPlaces->GetChildren() )
	{
		SChapterStats::SPlaceHolder place;
		place.vPosOnMap = CVec2( ValueFloat( *pPlace, 0 ), ValueFloat( *pPlace, 1 ) );
		rpgStats.placeHolders.push_back( place );
	}

	// The size of the map image is the stats' ImageRect. A picture the engine
	// cannot read fails the export before any file; a stats-only export keeps
	// MFC's all-zero rectangle and says why.
	const std::string szSourcePicture = szProjectDir + szMapImage + ".tga";
	CPtr<IImage> pPicture = NImageExport::LoadPicture( szSourcePicture, outcome );
	if ( pPicture == 0 )
	{
		if ( !context.bStatsOnly )
			return false;
		outcome.warnings.push_back( outcome.szError );
		outcome.szError.clear();
		float keptRect[4];
		if ( KeptImageRect( pProject->document.root, keptRect ) )
			rpgStats.mapImageRect = CTRect<float>( keptRect[0], keptRect[1], keptRect[2], keptRect[3] );
	}
	else
		rpgStats.mapImageRect = NImageExport::GetImageSize( szSourcePicture, outcome );

	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
	}, outcome ) )
		return false;
	if ( context.bStatsOnly )
		return true;

	const std::string szCopies[][2] = { { szHeader, ".txt" }, { szSubHeader, ".txt" }, { szDescription, ".txt" }, { szScript, ".lua" }, { szContext, ".xml" } };
	for ( const auto &copy : szCopies )
		if ( !NImageExport::CopyFileInto( context, szProjectDir + copy[0] + copy[1], szPrefix + copy[0] + copy[1], outcome ) )
		{
			outcome.warnings.push_back( outcome.szError );
			outcome.szError.clear();
		}

	outcome.szObjectName = szPrefix + szMapImage;
	return NImageExport::ComposeImageToTexture( context, szSourcePicture, szPrefix + szMapImage, NImageExport::ReadGammaConfig( szProjectDir ), GFXPF_DXT3, GFXPF_ARGB4444, true, outcome );
}

}
