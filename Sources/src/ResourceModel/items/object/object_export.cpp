// The object exporter: CObjectFrame::ExportFrameData and SaveRPGStats
// (Sources/src/editor/ObjectFrm.cpp:436-737) and CObjectTreeRootItem::
// ComposeAnimations (ObjTreeItem.cpp:40-72). The stats are SObjectRPGStats
// written as tree.Add( "desc", &stats ); the graphics are the summer, winter
// and Africa pictures, each packed with its shadow into 1, 1w and 1a (a
// sprite set .san beside _c/_l/_h.dds), plus icon.tga and name.txt.
//
// MFC computed the passability and visibility grids from the frame's tile
// lists and the camera at save time and kept the result in the project's desc.
// The port keeps that desc as the home of the grids (the bridge and the later
// grid tools edit it), so the export copies the grids and their origins from
// it and takes everything else from the tree. What it still needs the camera
// for is what MFC asked IScene for: the zero cross and sprite position on
// screen, and the grid origin on screen. ComputeSpriteNewZeroPos of a sprite
// just composed from one picture (no pre-processing, zero shift) has the
// picture's top-left at the sprite position and the texel at 0.5, so it
// reduces to ceil( zero cross + 15.4 - sprite position ) on screen.
//
// As in MFC a picture that cannot be composed does not fail the export: it is
// a warning (MFC's message box) and the stats are written all the same.
//
// The up-to-date check ports FindMaximalSourceTime and FindMinimalExportFileTime
// (ObjTreeItem.cpp:74, ObjectFrm.cpp:1488) with MFC's slip corrected: it looked
// for 1.tga, which the export never writes, so an object was never up to date;
// the port looks for the .san files and the _c/_l/_h.dds textures.
#include "StdAfx.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include "object_export.h"
#include "../stats_export.h"
#include "../../compose.h"
#include "../../image_export.h"
#include "../../mfc_value.h"
#include "../tree_item_types.h"
#include "../stats_item.h"
#include "../../../Main/RPGStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;
namespace fs = std::filesystem;

const char kObjectAddDir[] = "objects\\";
// ObjectFrm.cpp:28: the shift of the zero cross.
const float kZeroShift = 15.4f;

// NResourceXml::Node helpers of the chunks.
std::vector<const NResourceXml::Node *> Items( const NResourceXml::Node &list )
{
	std::vector<const NResourceXml::Node *> out;
	for ( const auto &child : list.children )
		if ( child.kind == NResourceXml::Node::Element && child.name == "item" )
			out.push_back( &child );
	return out;
}

std::string AttrText( const NResourceXml::Node &node, const char *pszName )
{
	const std::string *pValue = FindAttr( node, pszName );
	return pValue != nullptr ? *pValue : std::string();
}

float AttrFloat( const NResourceXml::Node &node, const char *pszName, float fDefault )
{
	const std::string *pValue = FindAttr( node, pszName );
	return pValue != nullptr ? float( std::strtod( pValue->c_str(), nullptr ) ) : fDefault;
}

std::string BodyText( const NResourceXml::Node &node )
{
	for ( const auto &child : node.children )
		if ( child.kind == NResourceXml::Node::Text || child.kind == NResourceXml::Node::CData )
			return child.text;
	return std::string();
}

bool HexDecode( const std::string &szHex, std::vector<unsigned char> &out )
{
	if ( szHex.size() % 2 != 0 )
		return false;
	out.resize( szHex.size() / 2 );
	auto Nibble = []( char c, int &v ) -> bool
	{
		if ( c >= '0' && c <= '9' ) { v = c - '0'; return true; }
		if ( c >= 'a' && c <= 'f' ) { v = 10 + ( c - 'a' ); return true; }
		if ( c >= 'A' && c <= 'F' ) { v = 10 + ( c - 'A' ); return true; }
		return false;
	};
	for ( std::size_t i = 0; i < out.size(); ++i )
	{
		int hi = 0, lo = 0;
		if ( !Nibble( szHex[2 * i], hi ) || !Nibble( szHex[2 * i + 1], lo ) )
			return false;
		out[i] = (unsigned char) ( ( hi << 4 ) | lo );
	}
	return true;
}

std::string HexEncode( const unsigned char *p, std::size_t n )
{
	static const char kDigits[] = "0123456789ABCDEF";
	std::string out( n * 2, '0' );
	for ( std::size_t i = 0; i < n; ++i )
	{
		out[2 * i] = kDigits[( p[i] >> 4 ) & 0xF];
		out[2 * i + 1] = kDigits[p[i] & 0xF];
	}
	return out;
}

// CTreeAccessor::Do2DArrayData: <item size_x size_y/> then one <item> of hex
// bytes per row; one item or none is an empty grid.
bool ReadGrid( const NResourceXml::Node *pGrid, const char *pszWhat, STileGrid &grid, std::string &szError )
{
	grid = STileGrid();
	if ( pGrid == nullptr )
		return true;
	const std::vector<const NResourceXml::Node *> rows = Items( *pGrid );
	if ( rows.size() <= 1 )
		return true;
	const int nWidth = std::atoi( AttrText( *rows[0], "size_x" ).c_str() );
	const int nHeight = std::atoi( AttrText( *rows[0], "size_y" ).c_str() );
	if ( nWidth < 0 || nHeight < 0 || rows.size() != std::size_t( nHeight ) + 1 )
	{
		szError = std::string( pszWhat ) + ": the row count does not match size_y";
		return false;
	}
	for ( int y = 0; y < nHeight; ++y )
	{
		std::vector<unsigned char> row;
		if ( !HexDecode( BodyText( *rows[y + 1] ), row ) || row.size() != std::size_t( nWidth ) )
		{
			szError = std::string( pszWhat ) + ": a row is not size_x hex bytes";
			return false;
		}
		grid.data.insert( grid.data.end(), row.begin(), row.end() );
	}
	grid.sizeX = nWidth;
	grid.sizeY = nHeight;
	return true;
}

void ReadVec( const NResourceXml::Node *pNode, SVec3 &v )
{
	if ( pNode == nullptr )
		return;
	v.x = AttrFloat( *pNode, "x", v.x );
	v.y = AttrFloat( *pNode, "y", v.y );
	v.z = AttrFloat( *pNode, "z", v.z );
}

void ReadVec( const NResourceXml::Node *pNode, SVec2 &v )
{
	if ( pNode == nullptr )
		return;
	v.x = AttrFloat( *pNode, "x", v.x );
	v.y = AttrFloat( *pNode, "y", v.y );
}

NResourceXml::Node NewElement( const std::string &szName )
{
	NResourceXml::Node node;
	node.kind = NResourceXml::Node::Element;
	node.name = szName;
	return node;
}

NResourceXml::Node &ChildOrNew( NResourceXml::Node &parent, const std::string &szName )
{
	for ( auto &child : parent.children )
		if ( child.kind == NResourceXml::Node::Element && child.name == szName )
			return child;
	parent.children.push_back( NewElement( szName ) );
	return parent.children.back();
}

void WriteVec( NResourceXml::Node &node, float x, float y, const float *pz )
{
	SetAttr( node, "x", MfcFloat( x ) );
	SetAttr( node, "y", MfcFloat( y ) );
	if ( pz != nullptr )
		SetAttr( node, "z", MfcFloat( *pz ) );
}

void WriteGrid( NResourceXml::Node &node, const STileGrid &grid )
{
	node.children.clear();
	const int nWidth = grid.data.empty() ? 0 : grid.sizeX;
	const int nHeight = grid.data.empty() ? 0 : grid.sizeY;
	NResourceXml::Node size = NewElement( "item" );
	SetAttr( size, "size_x", MfcInt( nWidth ) );
	SetAttr( size, "size_y", MfcInt( nHeight ) );
	node.children.push_back( std::move( size ) );
	for ( int y = 0; y < nHeight; ++y )
		node.children.push_back( StringElement( "item", HexEncode( grid.data.data() + std::size_t( y ) * nWidth, std::size_t( nWidth ) ) ) );
}

// One tile grid as the SObjectRPGStats array.
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

// GetAIClassInfo( const char * ).
int AIClass( const std::string &szVal )
{
	if ( szVal == "wheel" )
		return AI_CLASS_WHEEL;
	if ( szVal == "halftrack" )
		return AI_CLASS_HALFTRACK;
	if ( szVal == "track" )
		return AI_CLASS_TRACK;
	if ( szVal == "human" )
		return AI_CLASS_HUMAN;
	return 0;
}

void WarnLastError( SExportOutcome &outcome, const std::string &szPrefix )
{
	if ( outcome.szError.empty() )
		return;
	outcome.warnings.push_back( szPrefix + outcome.szError );
	outcome.szError.clear();
}

// MakeFullPath( project folder, name ) of one picture, with the case of the
// folders on disk (the projects were authored on Windows).
std::string SourceFile( const SExportContext &context, const std::string &szName )
{
	std::string szRel = szName;
	std::replace( szRel.begin(), szRel.end(), '/', '\\' );
	std::string szDir = ProjectDirectory( context );
	std::replace( szDir.begin(), szDir.end(), '/', '\\' );
	const std::string szFull = IsRelatedPath( szRel ) ? MakeFullPath( szDir, szRel ) : szRel;
	return FoldedFile( szFull ).string();
}

// FillRPGStats of SaveRPGStats: the tree half of the stats.
bool FillRPGStats( SObjectRPGStats &stats, const CTreeItem &root, const SObjectFrameData &frame, SExportOutcome &outcome )
{
	const CTreeItem *pCommonProps = RequireChild( root, ETIT_OBJECT_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	const CTreeItem *pPasses = RequireChild( root, ETIT_OBJECT_PASSES_ITEM, 0, "AI classes to pass", outcome );
	const CTreeItem *pEffects = RequireChild( root, ETIT_OBJECT_EFFECTS_ITEM, 0, "Effects", outcome );
	if ( pCommonProps == nullptr || pPasses == nullptr || pEffects == nullptr )
		return false;

	stats.fMaxHP = float( ValueInt( *pCommonProps, 0 ) );
	for ( int i = 0; i < 6; ++i )
	{
		stats.defences[i].nArmorMin = ValueInt( *pCommonProps, 1 );
		stats.defences[i].nArmorMax = ValueInt( *pCommonProps, 1 );
		stats.defences[i].fSilhouette = float( ValueInt( *pCommonProps, 2 ) );
	}
	stats.szAmbientSound = ValueStr( *pCommonProps, 3 );
	stats.szCycledSound = ValueStr( *pCommonProps, 4 );
	for ( const auto &pPass : pPasses->GetChildren() )
		stats.dwAIClasses |= AIClass( ValueStr( *pPass, 0 ) );
	stats.szEffectExplosion = ValueStr( *pEffects, 0 );
	stats.szEffectDeath = ValueStr( *pEffects, 1 );

	FillArray( stats.passability, frame.passability );
	stats.vOrigin = CVec2( frame.vOrigin.x, frame.vOrigin.y );
	FillArray( stats.visibility, frame.visibility );
	stats.vVisOrigin = CVec2( frame.vVisOrigin.x, frame.vVisOrigin.y );
	return true;
}

// The three pictures of the Graphics Info item and the name each is exported
// under (ComposeAnimations: summer 1, winter 1w, Africa 1a).
const char *const kSeasonNames[] = { "1", "1w", "1a" };

// FindMaximalSourceTime: the project and every picture it names.
fs::file_time_type MaximalSourceTime( const SExportContext &context, const CTreeItem &graphics )
{
	fs::file_time_type newest = ChangeTime( fs::path( context.szProjectPath ) );
	for ( const auto &pProps : graphics.GetChildren() )
		for ( int nValue : { 0, 1 } )
			newest = std::max( newest, ChangeTime( SourceFile( context, ValueStr( *pProps, nValue ) ) ) );
	return newest;
}

// FindMinimalExportFileTime: the oldest file the export writes. The winter
// and Africa pictures count only when the project names them.
fs::file_time_type MinimalExportTime( const SExportContext &context, const CTreeItem &graphics, const std::string &szResultDir )
{
	const fs::path exported = FoldedFile( ( fs::path( context.szDataRoot ) / ToSlashes( szResultDir ) ).string() );
	fs::file_time_type oldest = ChangeTime( FoldedChild( exported, "1.xml" ) );
	int nSeason = 0;
	for ( const auto &pProps : graphics.GetChildren() )
	{
		if ( nSeason >= 3 )
			break;
		const std::string szName = kSeasonNames[nSeason++];
		if ( nSeason > 1 && ValueStr( *pProps, 0 ).empty() )
			continue;
		oldest = std::min( oldest, ChangeTime( FoldedChild( exported, szName + ".san" ) ) );
		oldest = std::min( oldest, ChangeTime( FoldedChild( exported, szName + "s.san" ) ) );
		for ( const char *pszSuffix : { "_c.dds", "_l.dds", "_h.dds" } )
		{
			oldest = std::min( oldest, ChangeTime( FoldedChild( exported, szName + pszSuffix ) ) );
			oldest = std::min( oldest, ChangeTime( FoldedChild( exported, szName + "s" + pszSuffix ) ) );
		}
	}
	return oldest;
}

}

SObjectFrameData::SObjectFrameData()
{
	const float fDefault = 16 * fWorldCellSize;
	vSpritePos = SVec3{ fDefault, fDefault, 0 };
	vZeroPos = SVec3{ fDefault, fDefault, 0 };
}

bool ReadObjectFrameData( const Project &project, SObjectFrameData &data, std::string &szError )
{
	return ReadObjectFrameData( project.document.root, data, szError );
}

bool ReadObjectFrameData( const NResourceXml::Node &root, SObjectFrameData &data, std::string &szError )
{
	data = SObjectFrameData();
	if ( const NResourceXml::Node *pOwnData = NResourceXml::FindChild( root, "own_data" ) )
	{
		ReadVec( NResourceXml::FindChild( *pOwnData, "sprite_pos" ), data.vSpritePos );
		ReadVec( NResourceXml::FindChild( *pOwnData, "krest_pos" ), data.vZeroPos );
		if ( const NResourceXml::Node *pLines = NResourceXml::FindChild( *pOwnData, "TransLines" ) )
			for ( const NResourceXml::Node *pLine : Items( *pLines ) )
			{
				STransLine line;
				ReadVec( NResourceXml::FindChild( *pLine, "Point1" ), line.p1 );
				ReadVec( NResourceXml::FindChild( *pLine, "Point2" ), line.p2 );
				data.transLines.push_back( line );
			}
	}
	if ( const NResourceXml::Node *pDesc = NResourceXml::FindChild( root, "desc" ) )
	{
		if ( !ReadGrid( NResourceXml::FindChild( *pDesc, "passability" ), "desc passability", data.passability, szError ) ||
		     !ReadGrid( NResourceXml::FindChild( *pDesc, "visibility" ), "desc visibility", data.visibility, szError ) )
			return false;
		ReadVec( NResourceXml::FindChild( *pDesc, "origin" ), data.vOrigin );
		ReadVec( NResourceXml::FindChild( *pDesc, "VisOrigin" ), data.vVisOrigin );
	}
	return true;
}

void WriteObjectFrameData( Project &project, const SObjectFrameData &data )
{
	WriteObjectFrameData( project.document.root, data );
}

void WriteObjectFrameData( NResourceXml::Node &root, const SObjectFrameData &data )
{
	NResourceXml::Node &ownData = ChildOrNew( root, "own_data" );
	WriteVec( ChildOrNew( ownData, "sprite_pos" ), data.vSpritePos.x, data.vSpritePos.y, &data.vSpritePos.z );
	WriteVec( ChildOrNew( ownData, "krest_pos" ), data.vZeroPos.x, data.vZeroPos.y, &data.vZeroPos.z );
	NResourceXml::Node &lines = ChildOrNew( ownData, "TransLines" );
	lines.children.clear();
	for ( const STransLine &line : data.transLines )
	{
		NResourceXml::Node item = NewElement( "item" );
		NResourceXml::Node p1 = NewElement( "Point1" ), p2 = NewElement( "Point2" );
		WriteVec( p1, line.p1.x, line.p1.y, nullptr );
		WriteVec( p2, line.p2.x, line.p2.y, nullptr );
		item.children.push_back( std::move( p1 ) );
		item.children.push_back( std::move( p2 ) );
		lines.children.push_back( std::move( item ) );
	}

	WriteObjectGrids( root, data );
}

void WriteObjectGrids( NResourceXml::Node &root, const SObjectFrameData &data )
{
	NResourceXml::Node &desc = ChildOrNew( root, "desc" );
	WriteGrid( ChildOrNew( desc, "passability" ), data.passability );
	WriteVec( ChildOrNew( desc, "origin" ), data.vOrigin.x, data.vOrigin.y, nullptr );
	WriteVec( ChildOrNew( desc, "VisOrigin" ), data.vVisOrigin.x, data.vVisOrigin.y, nullptr );
	WriteGrid( ChildOrNew( desc, "visibility" ), data.visibility );
}

bool ExportObject( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_OBJECT_ROOT_ITEM, "object", outcome );
	if ( !pProject )
		return false;
	SObjectFrameData frame;
	if ( !ReadObjectFrameData( *pProject, frame, outcome.szError ) )
		return false;
	SObjectRPGStats rpgStats;
	if ( !FillRPGStats( rpgStats, *pProject->root, frame, outcome ) )
		return false;
	const CTreeItem *pGraphics = RequireChild( *pProject->root, ETIT_OBJECT_GRAPHICS_ITEM, 0, "Graphics Info", outcome );
	if ( pGraphics == nullptr )
		return false;

	const std::string szFile = StatsFileName( project, context, kObjectAddDir, false );
	const std::string szResultDir = DirectoryOf( szFile );
	outcome.szObjectName = szResultDir + "1";

	if ( !context.bForce && !context.bStatsOnly && !context.szDataRoot.empty() &&
	     MinimalExportTime( context, *pGraphics, szResultDir ) >= MaximalSourceTime( context, *pGraphics ) )
	{
		++outcome.nSkipped;
		return true;
	}

	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "desc", &rpgStats );
	}, outcome ) )
		return false;

	if ( !context.bStatsOnly )
	{
		SGroundCamera camera;
		if ( !context.groundCamera || !context.groundCamera( camera ) )
		{
			camera = DefaultEditorCamera();
			outcome.warnings.push_back( "no engine camera: the zero cross and grid origin use the default editor camera" );
		}
		const GridProjection projection( camera );
		const SVec2 vZero2 = projection.Pos3To2( frame.vZeroPos );
		const SVec2 vSprite2 = projection.Pos3To2( frame.vSpritePos );
		const CVec2 zeroPos2( std::ceil( vZero2.x + kZeroShift - vSprite2.x ), std::ceil( vZero2.y + kZeroShift - vSprite2.y ) );
		const CVec2 vLockedTilesCenter = NCompose::Origin2DPosition( camera, rpgStats.vOrigin );

		const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( ProjectDirectory( context ) );
		int nSeason = 0;
		std::string szSummerSprite;
		for ( const auto &pProps : pGraphics->GetChildren() )
		{
			if ( nSeason >= 3 )
				break;
			const std::string szName = kSeasonNames[nSeason];
			const std::string szSprite = SourceFile( context, ValueStr( *pProps, 0 ) );
			const std::string szShadow = SourceFile( context, ValueStr( *pProps, 1 ) );
			if ( nSeason++ == 0 )
				szSummerSprite = szSprite;
			if ( !NCompose::ComposeSingleObjectPack( context, gamma, GFXPF_ARGB1555, szSprite, szShadow, szResultDir + szName, zeroPos2, rpgStats.passability, vLockedTilesCenter, outcome ) )
				WarnLastError( outcome, "picture " + szName + ": " );
		}
		if ( !szSummerSprite.empty() && !NCompose::SaveIconFile( context, szSummerSprite, szResultDir + "icon.tga", outcome ) )
			WarnLastError( outcome, "icon: " );
	}

	const fs::path nameFile = FoldedFile( ProjectDirectory( context ) + "name.txt" );
	std::error_code ec;
	if ( fs::is_regular_file( nameFile, ec ) && !NImageExport::CopyFileInto( context, nameFile.string(), szResultDir + "name.txt", outcome ) )
		WarnLastError( outcome, "name.txt: " );
	return true;
}

}
