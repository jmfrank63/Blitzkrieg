// The sprite exporter: CSpriteFrame::ExportFrameData (Sources/src/editor/
// SpriteFrm.cpp:266-272) and CSpriteTreeRootItem::ComposeAnimations
// (SpriteTreeItem.cpp:27-130). The frames of the "Sprites" item, each a
// <directory><frame name>.tga, are packed into one picture with one animation
// "effect" of one direction, written as 1.san beside 1_c.dds (DXT5), 1_l.dds
// (ARGB4444) and 1_h.dds (ARGB8888).
//
// A sprite has no stats: SaveRPGStats of the base class writes nothing the
// game reads, so a stats-only export has nothing to write and says so.
//
// MFC's up-to-date check compares the sources with the older of the export's
// 1.san and 1.tga (FindMinimalExportFileTime). The export never writes a
// 1.tga, so on a clean export folder the check never skips; the port keeps
// the check as it is, so a 1.tga that is there (an older export left one)
// counts, and nothing else does.
//
// What MFC reported through message boxes goes into the outcome: frames that
// cannot be found (with the invalid.tga stand-in when the data folder has
// one), "no valid animations" with nothing written, and the compose result.
#include "StdAfx.h"

#include <algorithm>
#include <filesystem>

#include "../stats_export.h"
#include "../../compose.h"
#include "../../image_export.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;
namespace fs = std::filesystem;

const char kSpriteAddDir[] = "effects\\sprites\\";

// The frame folder the Sprites item names, with backslashes and no
// guarantee of a trailing one (MFC concatenated the frame name directly).
std::string FrameDirectory( const CTreeItem &spritesItem, const SExportContext &context )
{
	std::string szDirName = ValueStr( spritesItem, 0 );
	std::replace( szDirName.begin(), szDirName.end(), '/', '\\' );
	if ( IsRelatedPath( szDirName ) )
	{
		std::string szProjectDir = ProjectDirectory( context );
		std::replace( szProjectDir.begin(), szProjectDir.end(), '/', '\\' );
		szDirName = MakeFullPath( szProjectDir, szDirName );
	}
	return szDirName;
}

// FindMaximalSourceTime: the newest of the frame files that exist; min() when
// none does.
fs::file_time_type MaximalSourceTime( const std::vector<fs::path> &frames )
{
	fs::file_time_type newest = (fs::file_time_type::min)();
	for ( const fs::path &frame : frames )
		newest = (std::max)( newest, ChangeTime( frame ) );
	return newest;
}

}

bool ExportSprite( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_SPRITE_ROOT_ITEM, "sprite", outcome );
	if ( !pProject )
		return false;
	const std::string szFile = StatsFileName( project, context, kSpriteAddDir, false );
	const std::string szResultDir = DirectoryOf( szFile );
	// The sprite the game builds, "1" beside the .san.
	outcome.szObjectName = szResultDir + "1";
	if ( context.bStatsOnly )
	{
		outcome.warnings.push_back( "a sprite has no stats: a stats-only export writes nothing" );
		return true;
	}

	const CTreeItem *pSpritesItem = ChildItem( *pProject->root, ETIT_SPRITES_ITEM );
	if ( pSpritesItem == nullptr || pSpritesItem->GetChildren().empty() )
	{
		outcome.warnings.push_back( "Error: no valid animations" );
		return true;
	}

	// ComposeAnimations: one animation, one direction, frame k at slot k.
	std::vector<NCompose::SAnimationDesc> animDescVector( 1 );
	NCompose::SAnimationDesc &animDesc = animDescVector[0];
	animDesc.bCycled = false;
	animDesc.fSpeed = 0;
	animDesc.nFrameTime = ValueInt( *pSpritesItem, 1 );
	const CVec2 vPosition( float( ValueInt( *pSpritesItem, 2 ) ), float( ValueInt( *pSpritesItem, 3 ) ) );
	animDesc.ptFrameShift = vPosition;
	animDesc.szName = "effect";
	animDesc.dirs.resize( 1 );
	NCompose::SAnimationDesc::SDirDesc &dirDesc = animDesc.dirs[0];
	dirDesc.ptFrameShift = vPosition;

	const std::string szDirName = FrameDirectory( *pSpritesItem, context );
	std::vector<fs::path> existing;      // frames found, for the up-to-date check
	std::vector<std::string> fileNames;
	std::vector<std::string> invalidNames;
	const fs::path invalid = InvalidPicture( context );
	for ( const auto &pSprite : pSpritesItem->GetChildren() )
	{
		const std::string szWanted = ToSlashes( szDirName + pSprite->GetDisplayName() + ".tga" );
		const fs::path wantedPath( szWanted );
		const fs::path found = FoldedChild( wantedPath.parent_path(), wantedPath.filename().string() );
		std::error_code ec;
		if ( fs::is_regular_file( found, ec ) )
		{
			existing.push_back( found );
			fileNames.push_back( found.string() );
		}
		else
		{
			invalidNames.push_back( szWanted );
			if ( invalid.empty() )
				continue;       // nothing to stand in: the frame is left out
			fileNames.push_back( invalid.string() );
		}
		const int k = int( fileNames.size() ) - 1;
		dirDesc.frames.push_back( short( k ) );
		animDesc.frames[k] = vPosition;
	}
	if ( !invalidNames.empty() )
	{
		std::string szWarning = "Can not find files total count " + std::to_string( invalidNames.size() ) + ":";
		for ( const std::string &szName : invalidNames )
			szWarning += " " + szName;
		szWarning += invalid.empty() ? "; editor\\invalid.tga is not available here, the frames are left out" : "; editor\\invalid.tga stands in";
		outcome.warnings.push_back( szWarning );
	}
	if ( fileNames.empty() )
	{
		outcome.warnings.push_back( "Error: no valid animations" );
		return true;
	}

	// The up-to-date check of CParentFrame::ExportProject: a source time of
	// zero (no frame file at all) ended the batch export without writing.
	if ( !context.bForce && !context.szDataRoot.empty() )
	{
		const fs::file_time_type sourceTime = MaximalSourceTime( existing );
		if ( sourceTime == (fs::file_time_type::min)() )
		{
			outcome.warnings.push_back( "no frame file exists: nothing to compose" );
			return true;
		}
		const fs::path exported = fs::path( context.szDataRoot ) / ToSlashes( szResultDir );
		const fs::file_time_type exportTime = (std::min)( ChangeTime( FoldedChild( exported, "1.san" ) ), ChangeTime( FoldedChild( exported, "1.tga" ) ) );
		if ( exportTime >= (std::max)( sourceTime, ChangeTime( fs::path( context.szProjectPath ) ) ) )
		{
			++outcome.nSkipped;
			return true;
		}
	}

	SSpriteAnimationFormat spriteAnimFmt;
	CPtr<IImage> pImage = NCompose::BuildAnimations( &animDescVector, &spriteAnimFmt, fileNames, true, 0, outcome );
	if ( pImage == 0 )
	{
		if ( outcome.szError.empty() )
			outcome.szError = "Composing images failed!";
		return false;
	}
	const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( ProjectDirectory( context ) );
	return NImageExport::SaveCompressedTexture( context, pImage, szResultDir + "1", gamma, GFXPF_ARGB4444, outcome ) &&
	       NImageExport::SaveAnimation( context, spriteAnimFmt, szResultDir + "1.san", outcome );
}

}
