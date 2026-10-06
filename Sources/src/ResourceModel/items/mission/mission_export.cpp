// The mission exporter: CMissionFrame::FillRPGStats and ExportFrameData
// (Sources/src/editor/MissionFrm.cpp:73-422). SMissionStats is written under RPG
// through the engine's serializer; the map pictures map_{h,c,l}.dds, the header,
// subheader and description .txt and each objective's header and text .txt are
// copied beside it, and the final map's .bzm is made when it is missing.
//
// MFC's validation ran first and showed the last failing item's message in a
// box (each check overwrote szErrorMsg); the export fails with that message. Two
// quirks of it are kept as behaviour: both music checks look at the first music
// list (the combat one, GetChildItem( E_MISSION_MUSICS_ITEM, 0 )), so the
// exploration list is never checked, and the message of an earlier failing item
// is lost to a later one.
//
// What MFC did silently the port reports. A source that is not there was
// MyCopyFile's silent failure and is a warning naming it. The map pictures come
// from the project folder; when map_h.dds is missing they are made straight into
// the staged export through the bridge's createMinimap (MFC made them in the
// project folder on load), and the .bzm through convertMapToBzm. An export that
// needs either and finds the callback empty fails naming it, instead of writing
// a mission whose map the game cannot load.
#include "StdAfx.h"

#include "mission_export.h"
#include "../../image_export.h"
#include "../tree_item_types.h"
#include "../../../Main/GameStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kMissionAddDir[] = "scenarios\\";
const char kMapName[] = "map";

// The "You should specify ..." check of ExportFrameData. MFC kept the last
// failure; an empty string is a project that passes.
std::string Validate( const CTreeItem &commonProps, const CTreeItem &combatMusics, const CTreeItem &objectives )
{
	std::string szErrorMsg;
	if ( ValueStr( commonProps, 0 ).empty() )
		szErrorMsg = "You should specify header text reference before exporting.\n";
	if ( ValueStr( commonProps, 1 ).empty() )
		szErrorMsg = "You should specify subheader text reference before exporting.\n";
	if ( ValueStr( commonProps, 2 ).empty() )
		szErrorMsg = "You should specify description text reference before exporting.\n";
	if ( ValueStr( commonProps, 4 ).empty() )
	{
		if ( ValueStr( commonProps, 3 ).empty() )
			szErrorMsg = "You should specify either template or final map reference before exporting.\n";
		else if ( ValueStr( commonProps, 5 ).empty() )
			szErrorMsg = "You should specify setting reference before exporting.\n";
	}
	// Both music checks read the first list, as MFC's did.
	for ( const char *pszKind : { "combat", "exploration" } )
	{
		if ( combatMusics.GetChildren().empty() )
			szErrorMsg = std::string( "You should specify some " ) + pszKind + " music references before exporting.\n";
		else
			for ( const auto &pMusic : combatMusics.GetChildren() )
				if ( ValueStr( *pMusic, 0 ).empty() )
					szErrorMsg = std::string( "You should specify all " ) + pszKind + " music references before exporting.\n";
	}
	if ( objectives.GetChildren().empty() )
		szErrorMsg = "You should specify some objectives before exporting.\n";
	for ( const auto &pObjective : objectives.GetChildren() )
	{
		if ( ValueStr( *pObjective, 0 ).empty() )
			szErrorMsg = "You should specify header text for all objectives before exporting.\n";
		if ( ValueStr( *pObjective, 1 ).empty() )
			szErrorMsg = "You should specify description text for all objectives before exporting.\n";
	}
	return szErrorMsg;
}

// A file of the maps folder in any of the roots the export knows, ignoring
// case; the roots are the export root's data folder, the shipped Data and the
// staged export itself.
bool MapFileExists( const SExportContext &context, const std::string &szMap, const char *pszExtension )
{
	std::error_code ec;
	for ( const std::string &szRoot : { context.szDataRoot, context.szEditorDataDir, context.szStagingRoot } )
		if ( !szRoot.empty() && std::filesystem::is_regular_file( FoldedFile( szRoot + "/maps/" + szMap + pszExtension ), ec ) )
			return true;
	return false;
}

}

bool ExportMission( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_MISSION_ROOT_ITEM, "mission", outcome );
	if ( !pProject )
		return false;
	const CTreeItem *pCommonProps = RequireChild( *pProject->root, ETIT_MISSION_COMMON_PROPS_ITEM, 0, "Basic info", outcome );
	const CTreeItem *pCombatMusics = pCommonProps != nullptr ? RequireChild( *pProject->root, ETIT_MISSION_MUSICS_ITEM, 0, "Combat musics", outcome ) : nullptr;
	const CTreeItem *pExplorMusics = pCombatMusics != nullptr ? RequireChild( *pProject->root, ETIT_MISSION_MUSICS_ITEM, 1, "Exploration musics", outcome ) : nullptr;
	const CTreeItem *pObjectives = pExplorMusics != nullptr ? RequireChild( *pProject->root, ETIT_MISSION_OBJECTIVES_ITEM, 0, "Objectives", outcome ) : nullptr;
	if ( pObjectives == nullptr )
		return false;
	const std::string szInvalid = context.bSaveCache ? std::string() : Validate( *pCommonProps, *pCombatMusics, *pObjectives );
	if ( !szInvalid.empty() )
	{
		outcome.szError = "mission export refused: " + szInvalid.substr( 0, szInvalid.find_last_not_of( '\n' ) + 1 );
		return false;
	}
	const std::string szHeader = ValueStr( *pCommonProps, 0 );
	const std::string szSubHeader = ValueStr( *pCommonProps, 1 );
	const std::string szDescription = ValueStr( *pCommonProps, 2 );
	const std::string szFinalMap = ValueStr( *pCommonProps, 4 );

	const std::string szFile = StatsFileName( project, context, kMissionAddDir, false );
	const std::string szPrefix = DirectoryOf( szFile );
	const std::string szProjectDir = ProjectDirectory( context );

	SMissionStats rpgStats;
	rpgStats.szHeaderText = szPrefix + szHeader;
	rpgStats.szSubheaderText = szPrefix + szSubHeader;
	rpgStats.szDescriptionText = szPrefix + szDescription;
	rpgStats.szMapImage = szPrefix + kMapName;
	for ( const auto &pMusic : pCombatMusics->GetChildren() )
		rpgStats.combatMusics.push_back( ValueStr( *pMusic, 0 ) );
	for ( const auto &pMusic : pExplorMusics->GetChildren() )
		rpgStats.explorMusics.push_back( ValueStr( *pMusic, 0 ) );
	rpgStats.szTemplateMap = ValueStr( *pCommonProps, 3 );
	rpgStats.szFinalMap = szFinalMap;
	rpgStats.szSettingName = ValueStr( *pCommonProps, 5 );
	rpgStats.szMODName = context.szModName;
	rpgStats.szMODVersion = context.szModVersion;
	for ( const auto &pObjective : pObjectives->GetChildren() )
	{
		SMissionStats::SObjective objective;
		objective.szHeader = szPrefix + ValueStr( *pObjective, 0 );
		objective.szDescriptionText = szPrefix + ValueStr( *pObjective, 1 );
		objective.vPosOnMap = CVec2( ValueFloat( *pObjective, 2 ), ValueFloat( *pObjective, 3 ) );
		objective.bSecret = ValueBool( *pObjective, 4 );
		objective.nAnchorScriptID = ValueInt( *pObjective, 5 );
		rpgStats.objectives.push_back( objective );
	}

	// MFC read the size of the fixed map.tga of the project folder, and an
	// unreadable one left GetImageSize's all-zero rectangle: a warning here.
	const std::string szSourcePicture = szProjectDir + kMapName + ".tga";
	CPtr<IImage> pPicture = NImageExport::LoadPicture( szSourcePicture, outcome );
	if ( pPicture == 0 )
	{
		outcome.warnings.push_back( outcome.szError + "; the stats' ImageRect is left zero" );
		outcome.szError.clear();
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

	// The three pictures of the map: copied from the project folder, or made
	// from the final map when map_h.dds is not there (MFC's LoadRPGStats made
	// them in the project folder on opening).
	const std::string szPictureBase = szPrefix + kMapName;
	std::error_code ec;
	if ( !szFinalMap.empty() && !std::filesystem::is_regular_file( FoldedFile( szProjectDir + kMapName + "_h.dds" ), ec ) )
	{
		if ( !context.createMinimap )
		{
			outcome.szError = "mission export needs the map pictures of the final map " + szFinalMap + " (" + szProjectDir + kMapName + "_h.dds is missing) but the export has no createMinimap callback";
			return false;
		}
		std::filesystem::create_directories( std::filesystem::path( context.szStagingRoot ) / ToSlashes( szPrefix ), ec );
		std::string szError;
		if ( !context.createMinimap( szFinalMap, ToSlashes( context.szStagingRoot + "/" + szPictureBase ), szError ) )
		{
			outcome.szError = szError;
			return false;
		}
		outcome.nWritten += 3;
	}
	else
		for ( const char *pszPart : { "_h.dds", "_c.dds", "_l.dds" } )
			if ( !NImageExport::CopyFileInto( context, szProjectDir + kMapName + pszPart, szPictureBase + pszPart, outcome ) )
			{
				outcome.warnings.push_back( outcome.szError );
				outcome.szError.clear();
			}

	const std::string szCopies[] = { szHeader, szSubHeader, szDescription };
	for ( const std::string &szText : szCopies )
		if ( !NImageExport::CopyFileInto( context, szProjectDir + szText + ".txt", szPrefix + szText + ".txt", outcome ) )
		{
			outcome.warnings.push_back( outcome.szError );
			outcome.szError.clear();
		}

	// The map as a .bzm the game loads, made when no maps folder has one.
	if ( !szFinalMap.empty() && !MapFileExists( context, szFinalMap, ".bzm" ) )
	{
		if ( !context.convertMapToBzm )
		{
			outcome.szError = "mission export needs maps\\" + szFinalMap + ".bzm (no maps folder has it) but the export has no convertMapToBzm callback";
			return false;
		}
		const std::string szBzm = ToSlashes( context.szStagingRoot + "/maps/" + szFinalMap + ".bzm" );
		std::filesystem::create_directories( std::filesystem::path( szBzm ).parent_path(), ec );
		std::string szError;
		if ( !context.convertMapToBzm( szFinalMap, szBzm, szError ) )
		{
			outcome.szError = szError;
			return false;
		}
		++outcome.nWritten;
	}

	// The objective texts, in subfolders of the stats' folder when the name has one.
	for ( const auto &pObjective : pObjectives->GetChildren() )
		for ( int nValue = 0; nValue < 2; ++nValue )
		{
			const std::string szText = ValueStr( *pObjective, nValue );
			if ( !NImageExport::CopyFileInto( context, szProjectDir + szText + ".txt", szPrefix + szText + ".txt", outcome ) )
			{
				outcome.warnings.push_back( outcome.szError );
				outcome.szError.clear();
			}
		}

	outcome.szObjectName = szPictureBase;
	return true;
}

}
