// The fence exporter: CFenceFrame::ExportFrameData (FenceFrm.cpp:416-472),
// FillSegmentProps (956-1076) and CFenceTreeRootItem::ComposeFences
// (FenceTreeItem.cpp:53-258). The stats are SFenceRPGStats written as
// tree.Add( "RPG", &stats ): per segment the passability and visibility
// grids with their origins, per direction the segment indices of the four
// insert types. The graphics are every segment picture packed into one sprite
// set 1 (sprite set .san, _c/_l/_h.dds) and the shadows into 1s, plus icon.tga.
//
// The batch export first ran CFenceFrame::LoadRPGStats (346-381), which
// numbers the segments that have no SegmentIndex yet, in tree order, and
// collects the indices nobody uses. More than one free index means a segment
// was deleted and the packed animation would have a hole, which MFC refused
// with a message box; the port refuses the same way, before anything is
// written, and names the missing indices.
//
// Two departures from the MFC order, both because a picture that cannot be
// composed must not decide the stats. MFC filled a segment's stats only when
// its sprite file was readable, so a missing picture left a segment without
// grids and out of its direction's lists; the port fills every segment from
// the tree (FillSegmentProps never looked at the picture) and reports each
// missing or unusable picture as a warning, as for the object. And BuildAnimations
// needs a picture in every slot, so with a picture missing the graphics are
// not composed at all, where MFC's loop went on with an empty file name.
// MFC's ComposeFences never called ComposeSingleSprite, whatever its name
// suggests: it packs all segments with BuildAnimations.
//
// MFC had no up-to-date check for a fence (FindMaximalSourceTime is the base
// class's, which is never older than an export), so every export writes.
#include "StdAfx.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>

#include "fence.h"
#include "../stats_export.h"
#include "../../compose.h"
#include "../../image_export.h"
#include "../tree_item_types.h"
#include "../stats_item.h"
#include "../../mfc_value.h"
#include "../../xml.h"
#include "../../../Main/RPGStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;
namespace fs = std::filesystem;

const char kFenceAddDir[] = "fences\\";

// One FENCE_PROPS item in the order ComposeFences visits them.
struct SSegment
{
	const CFencePropsItem *pProps;
	int nType;      // 0 safe, 1 destroyed left, 2 destroyed right, 3 full destroyed
	int nIndex;     // the item's SegmentIndex, or LoadRPGStats' number for -1
};

struct SDirection
{
	std::vector<SSegment> segments;
};

// The segments of the four directions. A direction or insert item the tree
// lacks fails the export naming it; MFC indexed them unchecked.
bool CollectSegments( const CTreeItem &root, std::vector<SDirection> &directions, SExportOutcome &outcome )
{
	static const char *const kDirections[] = { "North-east", "North-west", "South-west", "South-east" };
	static const char *const kTypes[] = { "Safe", "Destroyed left", "Destroyed right", "Full destroyed" };
	int nIndex = 0;
	directions.assign( 4, SDirection() );
	for ( int nDirection = 0; nDirection < 4; ++nDirection )
	{
		const CTreeItem *pDirection = RequireChild( root, ETIT_FENCE_DIRECTION_ITEM, nDirection, kDirections[nDirection], outcome );
		if ( pDirection == nullptr )
			return false;
		for ( int nType = 0; nType < 4; ++nType )
		{
			const CTreeItem *pInsert = RequireChild( *pDirection, ETIT_FENCE_INSERT_ITEM, nType, kTypes[nType], outcome );
			if ( pInsert == nullptr )
				return false;
			for ( const auto &pChild : pInsert->GetChildren() )
			{
				if ( pChild->GetItemType() != ETIT_FENCE_PROPS_ITEM )
					continue;
				const CFencePropsItem *pProps = static_cast<const CFencePropsItem *>( pChild.get() );
				if ( pProps->nSegmentIndex < -1 )
				{
					outcome.szError = "the segment " + pProps->GetDisplayName() + " has the invalid SegmentIndex " + std::to_string( pProps->nSegmentIndex );
					return false;
				}
				const int nSegmentIndex = pProps->nSegmentIndex == -1 ? nIndex : pProps->nSegmentIndex;
				directions[nDirection].segments.push_back( SSegment{ pProps, nType, nSegmentIndex } );
				++nIndex;
			}
		}
	}
	return true;
}

// LoadRPGStats' hole check: the segment indices nobody uses below the largest
// one (MFC's freeIndexes without its trailing "next free" entry), and that
// largest index plus one (GetMaxFenceIndex).
std::vector<int> FreeIndexes( const std::vector<SDirection> &directions, int &nMaxIndex )
{
	std::set<int> indexSet;
	for ( const SDirection &direction : directions )
		for ( const SSegment &segment : direction.segments )
			indexSet.insert( segment.nIndex );
	std::vector<int> free;
	int nPrev = -1;
	for ( int nIndex : indexSet )
	{
		for ( int i = nPrev + 1; i < nIndex; ++i )
			free.push_back( i );
		nPrev = nIndex;
	}
	nMaxIndex = nPrev + 1;
	return free;
}

void FillArray( CArray2D<BYTE> &array, const STileGrid &grid )
{
	if ( grid.empty() || grid.data.size() != std::size_t( grid.sizeX ) * grid.sizeY )
	{
		array.SetSizes( 0, 0 );
		return;
	}
	array.SetSizes( grid.sizeX, grid.sizeY );
	std::copy( grid.data.begin(), grid.data.end(), array.GetBuffer() );
}

// One tile list as a grid and where it lies: the sprite position minus the
// world position of the grid's leftmost corner, as FillSegmentProps computes
// both the passability and the visibility origin. An empty list is no grid and
// the origin zero.
void FillGrid( const CListOfTiles &tiles, const SVec3 &vSpritePos, const GridProjection &projection, CArray2D<BYTE> &array, CVec2 &vOrigin )
{
	const STileGrid grid = TilesToGrid( tiles );
	FillArray( array, grid );
	if ( grid.empty() )
	{
		vOrigin = CVec2( 0, 0 );
		return;
	}
	const SVec3 origin = projection.OriginOfGrid( vSpritePos, grid.minTileX, grid.minTileY );
	vOrigin = CVec2( origin.x, origin.y );
}

const char kFrameData[] = "desc";

// A grid's size and cells as one text, which tells whether the cut-out of a grid is still the imported one.
std::string GridFingerprint( const CArray2D<BYTE> &grid )
{
	unsigned long long nHash = 1469598103934665603ull;
	const BYTE *pCells = const_cast<CArray2D<BYTE> &>( grid ).GetBuffer();
	for ( int i = 0; i < grid.GetSizeX() * grid.GetSizeY(); ++i )
		nHash = ( nHash ^ pCells[i] ) * 1099511628211ull;
	return std::to_string( grid.GetSizeX() ) + "x" + std::to_string( grid.GetSizeY() ) + ":" + std::to_string( nHash );
}

const NResourceXml::Node *KeptSegment( const NResourceXml::Node *pProjectElement, int nIndex )
{
	const NResourceXml::Node *pFrame = pProjectElement != nullptr ? NResourceXml::FindChild( *pProjectElement, kFrameData ) : nullptr;
	const NResourceXml::Node *pSegments = pFrame != nullptr ? NResourceXml::FindChild( *pFrame, "Segments" ) : nullptr;
	if ( pSegments == nullptr )
		return nullptr;
	for ( const NResourceXml::Node &item : pSegments->children )
		if ( item.kind == NResourceXml::Node::Element )
			if ( const std::string *pIndex = FindAttr( item, "Index" ) )
				if ( std::atoi( pIndex->c_str() ) == nIndex )
					return &item;
	return nullptr;
}

void UseKeptOrigin( const NResourceXml::Node &kept, const char *pszGrid, const char *pszOrigin, const CArray2D<BYTE> &grid, CVec2 &vOrigin )
{
	const std::string *pPrint = FindAttr( kept, std::string( pszGrid ) + "Grid" );
	const std::string *pX = FindAttr( kept, std::string( pszOrigin ) + "X" ), *pY = FindAttr( kept, std::string( pszOrigin ) + "Y" );
	if ( pPrint == nullptr || pX == nullptr || pY == nullptr || *pPrint != GridFingerprint( grid ) )
		return;
	vOrigin = CVec2( float( std::strtod( pX->c_str(), nullptr ) ), float( std::strtod( pY->c_str(), nullptr ) ) );
}

// CFenceFrame::FillSegmentProps.
void FillSegmentProps( const SSegment &segment, const GridProjection &projection, SFenceRPGStats::SSegmentRPGStats &stats, const NResourceXml::Node *pProjectElement )
{
	stats.nIndex = segment.nIndex;
	const SVec3 vSpritePos{ segment.pProps->vSpritePos.x, segment.pProps->vSpritePos.y, segment.pProps->vSpritePos.z };
	FillGrid( segment.pProps->lockedTiles, vSpritePos, projection, stats.passability, stats.vOrigin );
	FillGrid( segment.pProps->transeparences, vSpritePos, projection, stats.visibility, stats.vVisOrigin );
	if ( const NResourceXml::Node *pKept = KeptSegment( pProjectElement, segment.nIndex ) )
	{
		UseKeptOrigin( *pKept, "Pass", "Origin", stats.passability, stats.vOrigin );
		UseKeptOrigin( *pKept, "Vis", "VisOrigin", stats.visibility, stats.vVisOrigin );
	}
}

// MakeFullPath( project folder, name ) of the fences directory, with the case
// of the folders on disk and a trailing separator, which ComposeFences relied
// on the directory value carrying.
std::string SourceDirectory( const SExportContext &context, const std::string &szDirName )
{
	std::string szDir = szDirName;
	std::replace( szDir.begin(), szDir.end(), '/', '\\' );
	if ( IsRelatedPath( szDir ) )
	{
		std::string szProjectDir = ProjectDirectory( context );
		std::replace( szProjectDir.begin(), szProjectDir.end(), '/', '\\' );
		szDir = MakeFullPath( szProjectDir, szDir );
	}
	if ( !szDir.empty() && szDir.back() != '\\' )
		szDir += '\\';
	return szDir;
}

bool IsReadable( const std::string &szFile )
{
	std::error_code ec;
	return fs::is_regular_file( szFile, ec ) && std::ifstream( szFile, std::ios::binary ).good();
}

// A scratch folder for the shadow pictures, removed with it.
class CTempShadows
{
public:
	CTempShadows()
	{
		std::error_code ec;
		m_dir = fs::temp_directory_path( ec ) / ( "bk-fence-shadows-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
		m_bValid = fs::create_directories( m_dir, ec ) && !ec;
	}
	~CTempShadows()
	{
		std::error_code ec;
		fs::remove_all( m_dir, ec );
	}
	bool Valid() const { return m_bValid; }
	std::string File( int nIndex ) const { return ( m_dir / ( "s" + std::to_string( nIndex ) + ".tga" ) ).string(); }

private:
	fs::path m_dir;
	bool m_bValid = false;
};

// An image's half size, the frame shift ComposeFences gives each segment;
// false when the file is not a picture.
bool HalfSize( const std::string &szFile, CVec2 &vHalf )
{
	SExportOutcome probe;
	CPtr<IImage> pImage = NImageExport::LoadPicture( szFile, probe );
	if ( pImage == 0 )
		return false;
	vHalf = CVec2( float( pImage->GetSizeX() / 2 ), float( pImage->GetSizeY() / 2 ) );
	return true;
}

void MoveErrorToWarning( SExportOutcome &outcome, const std::string &szPrefix )
{
	outcome.warnings.push_back( szPrefix + outcome.szError );
	outcome.szError.clear();
}

// CFenceTreeRootItem::ComposeFences' graphics half: every segment's picture
// packed into 1, the shadows into 1s. False when nothing was composed; the
// reason is in outcome.warnings.
bool ComposeFences( const std::vector<SDirection> &directions, int nMaxIndex, const std::string &szSourceDir, const SExportContext &context,
                    const std::string &szResultDir, SExportOutcome &outcome )
{
	const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( ProjectDirectory( context ) );
	std::vector<std::string> fileNameVector( nMaxIndex ), shadowFileNameVector( nMaxIndex );
	std::vector<std::string> invalidNameVector;
	std::vector<NCompose::SAnimationDesc> animDescVector( 1 ), shadowDescVector( 1 );
	for ( NCompose::SAnimationDesc *pDesc : { &animDescVector[0], &shadowDescVector[0] } )
	{
		pDesc->bCycled = false;
		pDesc->fSpeed = 0;
		pDesc->nFrameTime = 0;
		pDesc->ptFrameShift = CVec2( 0, 0 );
		pDesc->szName = "default";
		pDesc->dirs.resize( 1 );
		pDesc->dirs[0].ptFrameShift = CVec2( 0, 0 );
	}
	NCompose::SAnimationDesc &animDesc = animDescVector[0];
	NCompose::SAnimationDesc &shadowDesc = shadowDescVector[0];

	CTempShadows shadows;
	if ( !shadows.Valid() )
	{
		outcome.warnings.push_back( "cannot create a folder for the shadow pictures: the fences were not composed" );
		return false;
	}

	int nActiveFenceIndex = 0;
	for ( const SDirection &direction : directions )
		for ( const SSegment &segment : direction.segments )
		{
			if ( segment.nIndex < 0 || segment.nIndex >= nMaxIndex )
				continue;
			const std::string szName = segment.pProps->GetDisplayName();
			const std::string szSprite = FoldedFile( szSourceDir + szName + ".tga" ).string();
			if ( !IsReadable( szSprite ) )
			{
				invalidNameVector.push_back( ToSlashes( szSourceDir + szName + ".tga" ) );
				continue;
			}

			const std::string szShadow = FoldedFile( szSourceDir + szName + "s.tga" ).string();
			SExportOutcome shadowOutcome;
			if ( !NCompose::SaveShadowFile( szSprite, szShadow, shadows.File( segment.nIndex ), shadowOutcome ) )
				invalidNameVector.push_back( ToSlashes( szSourceDir + szName + "s.tga" ) + " (" + shadowOutcome.szError + ")" );
			else
			{
				CVec2 vHalf;
				if ( !HalfSize( szShadow, vHalf ) )
				{
					invalidNameVector.push_back( ToSlashes( szSourceDir + szName + "s.tga" ) );
					continue;
				}
				shadowDesc.dirs[0].frames.push_back( nActiveFenceIndex );
				shadowDesc.frames[segment.nIndex] = vHalf;
				shadowFileNameVector[segment.nIndex] = shadows.File( segment.nIndex );
			}

			CVec2 vHalf;
			if ( !HalfSize( szSprite, vHalf ) )
			{
				invalidNameVector.push_back( ToSlashes( szSourceDir + szName + ".tga" ) );
				continue;
			}
			animDesc.dirs[0].frames.push_back( nActiveFenceIndex );
			animDesc.frames[segment.nIndex] = vHalf;
			fileNameVector[segment.nIndex] = szSprite;
			nActiveFenceIndex++;
		}

	if ( !invalidNameVector.empty() )
	{
		std::string szWarning = "Can not find files total count " + std::to_string( invalidNameVector.size() ) + ":";
		for ( const std::string &szFile : invalidNameVector )
			szWarning += " " + szFile + ";";
		outcome.warnings.push_back( szWarning );
	}
	if ( nActiveFenceIndex == 0 )
	{
		outcome.warnings.push_back( "Error: no valid pictures" );
		return false;
	}
	if ( std::find( fileNameVector.begin(), fileNameVector.end(), std::string() ) != fileNameVector.end() )
	{
		outcome.warnings.push_back( "a segment has no picture: the fences were not composed" );
		return false;
	}

	SSpriteAnimationFormat spriteAnimFmt;
	CPtr<IImage> pImage = NCompose::BuildAnimations( &animDescVector, &spriteAnimFmt, fileNameVector, true, 1, outcome );
	if ( pImage == 0 )
	{
		MoveErrorToWarning( outcome, "Composing fences failed: " );
		return false;
	}
	if ( !NImageExport::SaveCompressedTexture( context, pImage, szResultDir + "1", gamma, GFXPF_ARGB4444, outcome ) ||
	     !NImageExport::SaveAnimation( context, spriteAnimFmt, szResultDir + "1.san", outcome ) )
	{
		MoveErrorToWarning( outcome, "Composing fences failed: " );
		return false;
	}

	if ( std::find( shadowFileNameVector.begin(), shadowFileNameVector.end(), std::string() ) != shadowFileNameVector.end() )
	{
		outcome.warnings.push_back( "Composing shadows failed, a segment has no valid shadow" );
		return false;
	}
	SSpriteAnimationFormat shadowAnimFmt;
	pImage = NCompose::BuildAnimations( &shadowDescVector, &shadowAnimFmt, shadowFileNameVector, true, 0, outcome );
	if ( pImage == 0 )
	{
		MoveErrorToWarning( outcome, "Composing shadows failed: " );
		return false;
	}
	if ( !NImageExport::SaveShadowTexture( context, pImage, szResultDir + "1s", outcome ) ||
	     !NImageExport::SaveAnimation( context, shadowAnimFmt, szResultDir + "1s.san", outcome ) )
	{
		MoveErrorToWarning( outcome, "Composing shadows failed: " );
		return false;
	}
	return true;
}

}

void WriteFenceFrameData( NResourceXml::Node &root, const SFenceRPGStats &rpgStats )
{
	NResourceXml::Node frame, segments;
	frame.kind = segments.kind = NResourceXml::Node::Element;
	frame.name = kFrameData;
	segments.name = "Segments";
	for ( std::size_t i = 0; i < rpgStats.stats.size(); ++i )
	{
		const SFenceRPGStats::SSegmentRPGStats &segment = rpgStats.stats[i];
		NResourceXml::Node item;
		item.kind = NResourceXml::Node::Element;
		item.name = "item";
		SetAttr( item, "Index", MfcInt( int( i ) ) );
		SetAttr( item, "PassGrid", GridFingerprint( segment.passability ) );
		SetAttr( item, "OriginX", MfcFloat( segment.vOrigin.x ) );
		SetAttr( item, "OriginY", MfcFloat( segment.vOrigin.y ) );
		SetAttr( item, "VisGrid", GridFingerprint( segment.visibility ) );
		SetAttr( item, "VisOriginX", MfcFloat( segment.vVisOrigin.x ) );
		SetAttr( item, "VisOriginY", MfcFloat( segment.vVisOrigin.y ) );
		segments.children.push_back( std::move( item ) );
	}
	frame.children.push_back( std::move( segments ) );
	for ( NResourceXml::Node &child : root.children )
		if ( child.kind == NResourceXml::Node::Element && child.name == kFrameData )
		{
			child = std::move( frame );
			return;
		}
	root.children.push_back( std::move( frame ) );
}

bool ExportFence( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_FENCE_ROOT_ITEM, "fence", outcome );
	if ( !pProject )
		return false;
	const CTreeItem &root = *pProject->root;

	std::vector<SDirection> directions;
	if ( !CollectSegments( root, directions, outcome ) )
		return false;
	int nMaxIndex = 0;
	const std::vector<int> freeIndexes = FreeIndexes( directions, nMaxIndex );
	if ( !freeIndexes.empty() )
	{
		std::string szHoles;
		for ( int nIndex : freeIndexes )
			szHoles += ( szHoles.empty() ? "" : ", " ) + std::to_string( nIndex );
		outcome.szError = "Error: You have deleted some fence items. The resulting animations will include holes but they are not supported. "
		                  "You need to add some fence items before continue export files. The segment indices without an item: " + szHoles;
		return false;
	}

	const CTreeItem *pCommonProps = RequireChild( root, ETIT_FENCE_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	const CTreeItem *pEffects = RequireChild( root, ETIT_OBJECT_EFFECTS_ITEM, 0, "Effects", outcome );
	if ( pCommonProps == nullptr || pEffects == nullptr )
		return false;

	SFenceRPGStats rpgStats;
	rpgStats.szKeyName = ValueStr( *pCommonProps, 0 );
	rpgStats.fMaxHP = float( ValueInt( *pCommonProps, 2 ) );
	for ( int i = 0; i < 6; ++i )
	{
		rpgStats.defences[i].nArmorMin = ValueInt( *pCommonProps, 3 );
		rpgStats.defences[i].nArmorMax = ValueInt( *pCommonProps, 3 );
		rpgStats.defences[i].fSilhouette = 0;
	}
	if ( ValueBool( *pCommonProps, 4 ) )
		rpgStats.dwAIClasses |= AI_CLASS_HUMAN;
	if ( ValueBool( *pCommonProps, 5 ) )
		rpgStats.dwAIClasses |= AI_CLASS_WHEEL;
	if ( ValueBool( *pCommonProps, 6 ) )
		rpgStats.dwAIClasses |= AI_CLASS_HALFTRACK;
	if ( ValueBool( *pCommonProps, 7 ) )
		rpgStats.dwAIClasses |= AI_CLASS_TRACK;
	rpgStats.szEffectExplosion = ValueStr( *pEffects, 0 );
	rpgStats.szEffectDeath = ValueStr( *pEffects, 1 );

	// What MFC asked IScene for: the camera that puts a sprite position and a
	// tile grid in the same world.
	SGroundCamera camera;
	if ( !context.groundCamera || !context.groundCamera( camera ) )
	{
		camera = DefaultEditorCamera();
		outcome.warnings.push_back( "no engine camera: the segment origins use the default editor camera" );
	}
	const GridProjection projection( camera );

	rpgStats.stats.resize( nMaxIndex );
	std::set<int> used;
	for ( const SDirection &direction : directions )
	{
		SFenceRPGStats::SDir dir;
		for ( const SSegment &segment : direction.segments )
		{
			if ( !used.insert( segment.nIndex ).second )
				outcome.warnings.push_back( "segment index " + std::to_string( segment.nIndex ) + " is used by more than one item (" + segment.pProps->GetDisplayName() + ")" );
			FillSegmentProps( segment, projection, rpgStats.stats[segment.nIndex], &pProject->document.root );
			std::vector<int> &list = segment.nType == 0 ? dir.centers : segment.nType == 1 ? dir.ldamages : segment.nType == 2 ? dir.rdamages : dir.cdamages;
			list.push_back( segment.nIndex );
		}
		rpgStats.dirs.push_back( dir );
	}

	const std::string szFile = StatsFileName( project, context, kFenceAddDir, false );
	const std::string szResultDir = DirectoryOf( szFile );
	outcome.szObjectName = szResultDir + "1";

	const std::string szSourceDir = SourceDirectory( context, ValueStr( *pCommonProps, 1 ) );
	if ( !context.bStatsOnly )
	{
		if ( !ComposeFences( directions, nMaxIndex, szSourceDir, context, szResultDir, outcome ) )
			outcome.warnings.push_back( "the fences were not composed; the stats are written all the same" );
	}

	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
	}, outcome ) )
		return false;

	// The icon is the first safe segment of the first direction that has one.
	if ( !context.bStatsOnly )
	{
		const SSegment *pFirst = directions[0].segments.empty() || directions[0].segments.front().nType != 0 ? nullptr : &directions[0].segments.front();
		if ( pFirst == nullptr )
			outcome.warnings.push_back( "icon: the first safe item of the north-east direction is missing" );
		else
		{
			const std::string szPicture = FoldedFile( szSourceDir + pFirst->pProps->GetDisplayName() + ".tga" ).string();
			if ( !NCompose::SaveIconFile( context, szPicture, szResultDir + "icon.tga", outcome ) )
				MoveErrorToWarning( outcome, "icon: " );
		}
	}
	return true;
}

}
