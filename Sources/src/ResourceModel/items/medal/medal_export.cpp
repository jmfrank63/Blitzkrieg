// The medal exporter: CMedalFrame::SaveRPGStats and ExportFrameData
// (Sources/src/editor/MedalFrm.cpp:71-190). SMedalStats is written under RPG
// through the engine's serializer, the medal picture goes through
// ComposeImageToTexture into <Texture>_c/_l/_h.dds beside the stats and the
// name and description .txt are copied there.
//
// MFC wrote the stats first and reported a failed ComposeImageToTexture in a
// message box afterwards. The port checks the picture before it writes
// anything, so a missing texture leaves no half export and the error names the
// file. A missing .txt is MFC's silent MyCopyFile failure: it becomes a warning
// naming the file and the export goes on. The up-to-date check of MFC's
// frame (FindMaximalSourceTime answers "always newer") never skipped anything,
// so bForce changes nothing here.
#include "StdAfx.h"

#include "medal_export.h"
#include "../../image_export.h"
#include "../tree_item_types.h"
#include "../../../Main/GameStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kMedalAddDir[] = "medals\\";

}

bool ExportMedal( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_MEDAL_ROOT_ITEM, "medal", outcome );
	if ( !pProject )
		return false;
	const CTreeItem *pCommonProps = RequireChild( *pProject->root, ETIT_MEDAL_COMMON_PROPS_ITEM, 0, "Basic info", outcome );
	if ( pCommonProps == nullptr )
		return false;
	const std::string szName = ValueStr( *pCommonProps, 0 );
	const std::string szDescription = ValueStr( *pCommonProps, 1 );
	const std::string szTexture = ValueStr( *pCommonProps, 2 );

	const std::string szFile = StatsFileName( project, context, kMedalAddDir, false );
	const std::string szResultDir = DirectoryOf( szFile );
	const std::string szProjectDir = ProjectDirectory( context );

	// CMedalFrame::FillRPGStats: every name carries the folder the stats go to.
	SMedalStats rpgStats;
	rpgStats.szHeaderText = szResultDir + szName;
	rpgStats.szDescriptionText = szResultDir + szDescription;
	rpgStats.szTexture = szResultDir + szTexture;
	const std::string szSourcePicture = szProjectDir + szTexture + ".tga";
	const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( szProjectDir );
	CPtr<IImage> pPicture;
	if ( !szTexture.empty() )
	{
		// The size of the source picture is the stats' ImageRect. A picture
		// the engine cannot read fails the export before any file; a
		// stats-only export (no graphics wanted) keeps MFC's all-zero
		// rectangle and says why.
		pPicture = NImageExport::LoadPicture( szSourcePicture, outcome );
		if ( pPicture == 0 )
		{
			if ( !context.bStatsOnly )
				return false;
			outcome.warnings.push_back( outcome.szError );
			outcome.szError.clear();
		}
	}
	if ( pPicture != 0 )
		rpgStats.mapImageRect = NImageExport::GetImageSize( szSourcePicture, outcome );

	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
	}, outcome ) )
		return false;
	if ( context.bStatsOnly )
		return true;

	outcome.szObjectName = szResultDir + szTexture;
	if ( pPicture != 0 && !NImageExport::ComposeImageToTexture( context, szSourcePicture, szResultDir + szTexture, gamma, GFXPF_DXT5, GFXPF_ARGB1555, true, outcome ) )
		return false;

	for ( const std::string &szText : { szName, szDescription } )
	{
		if ( szText.empty() )
			continue;
		if ( !NImageExport::CopyFileInto( context, szProjectDir + szText + ".txt", szResultDir + szText + ".txt", outcome ) )
		{
			outcome.warnings.push_back( outcome.szError );
			outcome.szError.clear();
		}
	}
	return true;
}

}
