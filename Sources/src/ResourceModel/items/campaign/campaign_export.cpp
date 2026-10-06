// The campaign exporter: CCampaignFrame::FillRPGStats and ExportFrameData
// (Sources/src/editor/CampaignFrm.cpp:60-290). SCampaignStats is written under
// RPG through the engine's serializer; the map image goes through
// ComposeImageToTexture (DXT3 and ARGB4444, the frame's formats) beside the stats
// and the header and subheader .txt are copied there.
//
// MFC's validation ran first and showed the last failing item's message in a
// box (each check overwrote szErrorMsg); the export fails with that message.
// As for the medal, the map image is checked before anything is written, so a
// missing picture leaves no half export and the error names the file, and a
// missing .txt is MFC's silent MyCopyFile failure that becomes a warning.
#include "StdAfx.h"

#include "campaign_export.h"
#include "../../image_export.h"
#include "../tree_item_types.h"
#include "../../../Main/GameStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kCampaignAddDir[] = "scenarios\\campaigns\\";

// The "You should specify ..." check of ExportFrameData. MFC kept the last
// failure; an empty string is a project that passes.
std::string Validate( const CTreeItem &commonProps, const CTreeItem &chapters, const CTreeItem &templates )
{
	std::string szErrorMsg;
	if ( ValueStr( commonProps, 0 ).empty() )
		szErrorMsg = "You should specify header text reference before exporting.\n";
	if ( ValueStr( commonProps, 1 ).empty() )
		szErrorMsg = "You should specify subheader text reference before exporting.\n";
	if ( ValueStr( commonProps, 2 ).empty() )
		szErrorMsg = "You should specify map image reference before exporting.\n";
	if ( ValueStr( commonProps, 3 ).empty() )
		szErrorMsg = "You should specify intro movie reference before exporting.\n";
	if ( ValueStr( commonProps, 4 ).empty() )
		szErrorMsg = "You should specify outro movie reference before exporting.\n";
	if ( ValueStr( commonProps, 5 ).empty() )
		szErrorMsg = "You should specify interface music reference before exporting.\n";
	if ( ValueStr( commonProps, 6 ).empty() )
		szErrorMsg = "You should specify player side before exporting.\n";
	if ( chapters.GetChildren().empty() )
		szErrorMsg = "You should specify some chapter references before exporting.\n";
	for ( const auto &pChapter : chapters.GetChildren() )
		if ( ValueStr( *pChapter, 0 ).empty() )
			szErrorMsg = "You should specify all chapter references before exporting.\n";
	if ( templates.GetChildren().empty() )
		szErrorMsg = "You should specify some template references before exporting.\n";
	for ( const auto &pTemplate : templates.GetChildren() )
		if ( ValueStr( *pTemplate, 0 ).empty() )
			szErrorMsg = "You should specify all template references before exporting.\n";
	return szErrorMsg;
}

}

// Whether a name starts with the data root's scenarios folder, compared case folded as the engine's file names are.
static bool NameStartsWithDataFolder( const std::string &szName )
{
	static const char szFolder[] = "scenarios\\";
	const std::size_t nLength = sizeof( szFolder ) - 1;
	if ( szName.size() <= nLength )
		return false;
	for ( std::size_t i = 0; i < nLength; ++i )
		if ( std::tolower( (unsigned char)szName[i] ) != szFolder[i] )
			return false;
	return true;
}

bool ExportCampaign( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_CAMPAIGN_ROOT_ITEM, "campaign", outcome );
	if ( !pProject )
		return false;
	const CTreeItem *pCommonProps = RequireChild( *pProject->root, ETIT_CAMPAIGN_COMMON_PROPS_ITEM, 0, "Basic info", outcome );
	const CTreeItem *pChapters = pCommonProps != nullptr ? RequireChild( *pProject->root, ETIT_CAMPAIGN_CHAPTERS_ITEM, 0, "Chapters", outcome ) : nullptr;
	const CTreeItem *pTemplates = pChapters != nullptr ? RequireChild( *pProject->root, ETIT_CAMPAIGN_TEMPLATES_ITEM, 0, "Templates", outcome ) : nullptr;
	if ( pTemplates == nullptr )
		return false;
	const std::string szInvalid = Validate( *pCommonProps, *pChapters, *pTemplates );
	if ( !szInvalid.empty() )
	{
		outcome.szError = "campaign export refused: " + szInvalid.substr( 0, szInvalid.find_last_not_of( '\n' ) + 1 );
		return false;
	}
	const std::string szHeader = ValueStr( *pCommonProps, 0 );
	const std::string szSubHeader = ValueStr( *pCommonProps, 1 );
	const std::string szMapImage = ValueStr( *pCommonProps, 2 );

	const std::string szFile = StatsFileName( project, context, kCampaignAddDir, false );
	const std::string szPrefix = DirectoryOf( szFile );
	const std::string szProjectDir = ProjectDirectory( context );
	// A save writes the cache with the frame's prefix still empty (SaveRPGStats runs outside
	// ExportFrameData, which sets it), so the cached paths are the tree's own values; the
	// export prefixes them. A cache that held the prefixed paths would reload into the tree.
	const std::string szStatsPrefix = context.bSaveCache ? std::string() : szPrefix;
	// A map picture the project names from the data root (a mod's campaign keeps it under scenarios\custom, not beside
	// the stats) is the path the game reads: prefixing it again would point the game at a file that does not exist.
	// MFC's relative names never start with the data folder, so none of them changes.
	const std::string szImagePrefix = NameStartsWithDataFolder( szMapImage ) ? std::string() : szPrefix;

	SCampaignStats rpgStats;
	rpgStats.szHeaderText = szStatsPrefix + szHeader;
	rpgStats.szSubheaderText = szStatsPrefix + szSubHeader;
	rpgStats.szMapImage = ( szImagePrefix.empty() ? std::string() : szStatsPrefix ) + szMapImage;
	rpgStats.szIntroMovie = ValueStr( *pCommonProps, 3 );
	rpgStats.szOutroMovie = ValueStr( *pCommonProps, 4 );
	rpgStats.szInterfaceMusic = ValueStr( *pCommonProps, 5 );
	rpgStats.szSideName = ValueStr( *pCommonProps, 6 );
	rpgStats.szMODName = context.szModName;
	rpgStats.szMODVersion = context.szModVersion;
	for ( const auto &pChapter : pChapters->GetChildren() )
	{
		SCampaignStats::SChapter chapter;
		chapter.szChapter = ValueStr( *pChapter, 0 );
		chapter.vPosOnMap = CVec2( ValueFloat( *pChapter, 1 ), ValueFloat( *pChapter, 2 ) );
		chapter.bVisible = ValueBool( *pChapter, 3 );
		chapter.bSecret = ValueBool( *pChapter, 4 );
		rpgStats.chapters.push_back( chapter );
	}
	for ( const auto &pTemplate : pTemplates->GetChildren() )
		rpgStats.templateMissions.push_back( ValueStr( *pTemplate, 0 ) );

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

	for ( const std::string &szText : { szHeader, szSubHeader } )
		if ( !NImageExport::CopyFileInto( context, szProjectDir + szText + ".txt", szPrefix + szText + ".txt", outcome ) )
		{
			outcome.warnings.push_back( outcome.szError );
			outcome.szError.clear();
		}

	outcome.szObjectName = szImagePrefix + szMapImage;
	return NImageExport::ComposeImageToTexture( context, szSourcePicture, szImagePrefix + szMapImage, NImageExport::ReadGammaConfig( szProjectDir ), GFXPF_DXT3, GFXPF_ARGB4444, true, outcome );
}

}
