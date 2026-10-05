// The building exporter: CBuildingFrame::ExportFrameData and SaveRPGStats
// (Sources/src/editor/BuildFrm.cpp:454-920) and CBuildingTreeRootItem::
// ComposeAnimations (BuildTreeItem.cpp:68). The stats are SBuildingRPGStats
// written as tree.Add( "desc", &stats ); the graphics are the summer and winter
// pictures, whole, damaged and destroyed (1, 2, 3 and 1w, 2w, 3w), each packed
// with its shadow, the damaged and destroyed ones also with their noise picture
// (2g, 3g, 2wg, 3wg), plus icon.tga from the summer whole picture.
//
// MFC computed the passability and visibility grids from the frame's tile lists
// and the points from the sprites the editor had placed. The port keeps desc as
// their home, so the export copies the grids, origins and each point's
// position, picture position and world position from it, and takes everything
// the property inspector edits (a point's direction, cone, weapon, ammo, effect;
// the basic properties) from the tree, as MFC's SaveRPGStats read the tree
// children. A point that has a tree child but no entry in desc is skipped, as
// MFC skipped a point whose horizontal sprite was gone. The picture and world
// positions are stored values: the editor ABI edits one position per point, and
// MFC computed the world position from the engine's matrices and the sprite's
// alpha, which the exporter has no scene for.
//
// What needs the camera is what MFC asked IScene for: the zero cross on screen
// (ceil( zero cross + 15.4 - sprite position ), as in the object exporter) and
// the grid origin on screen.
//
// As in MFC a picture that cannot be composed does not fail the export: it is a
// warning and the stats are written all the same. The one exception is MFC's
// own: the summer whole picture is the default sprite, and without it the export
// stops ("Can not continue export data") before any stats are written.
//
// The up-to-date check ports FindMaximalSourceTime (BuildTreeItem.cpp:194) and
// FindMinimalExportFileTime (BuildFrm.cpp:2574).
#include "StdAfx.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include "building_export.h"
#include "../object/object_export.h"
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

const char kBuildingAddDir[] = "buildings\\";
// BuildFrm.cpp:26: the shift of the zero cross.
const float kZeroShift = 15.4f;

// The six defence children by name, as SaveRPGStats' if-chain maps them; a
// name that is none of them is index 0, as there.
int DefenceIndex( const std::string &szName )
{
	if ( szName == "Left" )
		return RPG_LEFT;
	if ( szName == "Right" )
		return RPG_RIGHT;
	if ( szName == "Top" )
		return RPG_TOP;
	if ( szName == "Bottom" )
		return RPG_BOTTOM;
	if ( szName == "Front" )
		return RPG_FRONT;
	if ( szName == "Back" )
		return RPG_BACK;
	return 0;
}

// The building types of the combo, GetBuildingType / SetBuildingType.
const char *const kBuildingTypes[] = { "building", "main storage", "temporary storage", "dot" };

int BuildingTypeOf( const std::string &szVal )
{
	if ( szVal == "main storage" || szVal == "Main storage" )
		return SBuildingRPGStats::TYPE_MAIN_RU_STORAGE;
	if ( szVal == "temporary storage" || szVal == "Temporary storage" )
		return SBuildingRPGStats::TYPE_TEMP_RU_STORAGE;
	if ( szVal == "dot" || szVal == "DOT" )
		return SBuildingRPGStats::TYPE_DOT;
	return SBuildingRPGStats::TYPE_BULDING;
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

// NResourceXml helpers of the desc chunk.
std::vector<const NResourceXml::Node *> Items( const NResourceXml::Node *pList )
{
	std::vector<const NResourceXml::Node *> out;
	if ( pList == nullptr )
		return out;
	for ( const auto &child : pList->children )
		if ( child.kind == NResourceXml::Node::Element && child.name == "item" )
			out.push_back( &child );
	return out;
}

float AttrFloat( const NResourceXml::Node &node, const char *pszName, float fDefault )
{
	const std::string *pValue = FindAttr( node, pszName );
	return pValue != nullptr ? float( std::strtod( pValue->c_str(), nullptr ) ) : fDefault;
}

CVec3 ReadPos3( const NResourceXml::Node &item, const char *pszName )
{
	CVec3 v( 0, 0, 0 );
	if ( const NResourceXml::Node *pNode = NResourceXml::FindChild( item, pszName ) )
	{
		v.x = AttrFloat( *pNode, "x", 0 );
		v.y = AttrFloat( *pNode, "y", 0 );
		v.z = AttrFloat( *pNode, "z", 0 );
	}
	return v;
}

CVec2 ReadPos2( const NResourceXml::Node &item, const char *pszName )
{
	CVec2 v( 0, 0 );
	if ( const NResourceXml::Node *pNode = NResourceXml::FindChild( item, pszName ) )
	{
		v.x = AttrFloat( *pNode, "x", 0 );
		v.y = AttrFloat( *pNode, "y", 0 );
	}
	return v;
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

NResourceXml::Node Vec2Node( const char *pszName, const CVec2 &v )
{
	NResourceXml::Node node = NewElement( pszName );
	SetAttr( node, "x", MfcFloat( v.x ) );
	SetAttr( node, "y", MfcFloat( v.y ) );
	return node;
}

NResourceXml::Node Vec3Node( const char *pszName, const CVec3 &v )
{
	NResourceXml::Node node = Vec2Node( pszName, CVec2( v.x, v.y ) );
	SetAttr( node, "z", MfcFloat( v.z ) );
	return node;
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

STileGrid GridOf( CArray2D<BYTE> &array )
{
	STileGrid grid;
	grid.sizeX = array.GetSizeX();
	grid.sizeY = array.GetSizeY();
	if ( !grid.empty() )
		grid.data.assign( array.GetBuffer(), array.GetBuffer() + std::size_t( grid.sizeX ) * grid.sizeY );
	else
		grid.sizeX = grid.sizeY = 0;
	return grid;
}

const CTreeItem *NthChild( const CTreeItem &container, std::size_t nIndex )
{
	const auto &children = container.GetChildren();
	return nIndex < children.size() ? children[nIndex].get() : nullptr;
}

// A value of a point's tree child, or the desc attribute it was synced from
// when the point has no child.
float PointFloat( const CTreeItem *pChild, int nValue, const NResourceXml::Node &item, const char *pszAttr )
{
	return pChild != nullptr ? ValueFloat( *pChild, nValue ) : AttrFloat( item, pszAttr, 0 );
}

void WarnLastError( SExportOutcome &outcome, const std::string &szPrefix )
{
	if ( outcome.szError.empty() )
		return;
	outcome.warnings.push_back( szPrefix + outcome.szError );
	outcome.szError.clear();
}

// MakeFullPath( project folder, name ) of one picture, with the case of the
// folders on disk (the projects were authored on Windows). A name that is
// already absolute stays as it is, as ComposeAnimations kept it.
std::string SourceFile( const SExportContext &context, const std::string &szName )
{
	std::string szRel = szName;
	std::replace( szRel.begin(), szRel.end(), '/', '\\' );
	std::string szDir = ProjectDirectory( context );
	std::replace( szDir.begin(), szDir.end(), '/', '\\' );
	const std::string szFull = IsRelatedPath( szRel ) ? MakeFullPath( szDir, szRel ) : szRel;
	return FoldedFile( szFull ).string();
}

// SaveRPGStats' tree half: Basic Info, the AI classes to pass, the defences,
// and then the points, whose positions come from desc.
bool FillRPGStats( SBuildingRPGStats &stats, const CTreeItem &root, const SObjectFrameData &frame, const NResourceXml::Node &projectElement, SExportOutcome &outcome )
{
	const CTreeItem *pCommonProps = RequireChild( root, ETIT_BUILDING_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	const CTreeItem *pPasses = RequireChild( root, ETIT_BUILDING_PASSES_ITEM, 0, "AI classes to pass", outcome );
	const CTreeItem *pDefences = RequireChild( root, ETIT_BUILDING_DEFENCES_ITEM, 0, "Defence", outcome );
	const CTreeItem *pSlots = RequireChild( root, ETIT_BUILDING_SLOTS_ITEM, 0, "Shoot slots", outcome );
	const CTreeItem *pFirePoints = RequireChild( root, ETIT_BUILDING_FIRE_POINTS_ITEM, 0, "Fire points", outcome );
	const CTreeItem *pDirExplosions = RequireChild( root, ETIT_BUILDING_DIR_EXPLOSIONS_ITEM, 0, "Direction explosions", outcome );
	const CTreeItem *pSmokes = RequireChild( root, ETIT_BUILDING_SMOKES_ITEM, 0, "Smoke points", outcome );
	if ( pCommonProps == nullptr || pPasses == nullptr || pDefences == nullptr || pSlots == nullptr || pFirePoints == nullptr || pDirExplosions == nullptr || pSmokes == nullptr )
		return false;

	stats.szKeyName = ValueStr( *pCommonProps, 0 );
	stats.eType = SBuildingRPGStats::EType( BuildingTypeOf( ValueStr( *pCommonProps, 1 ) ) );
	stats.fMaxHP = float( ValueInt( *pCommonProps, 2 ) );
	stats.fRepairCost = ValueFloat( *pCommonProps, 3 );
	stats.nRestSlots = ValueInt( *pCommonProps, 4 );
	stats.nMedicalSlots = ValueInt( *pCommonProps, 5 );
	stats.szAmbientSound = ValueStr( *pCommonProps, 6 );
	stats.szCycledSound = ValueStr( *pCommonProps, 7 );
	for ( const auto &pPass : pPasses->GetChildren() )
		stats.dwAIClasses |= AIClass( ValueStr( *pPass, 0 ) );
	for ( int i = 0; i < 6; ++i )
	{
		const CTreeItem *pDefProps = ChildItem( *pDefences, ETIT_BUILDING_DEFENCE_PROPS_ITEM, i );
		if ( pDefProps == nullptr )
		{
			outcome.szError = "the Defence item has no defence number " + std::to_string( i + 1 );
			return false;
		}
		const int nIndex = DefenceIndex( pDefProps->GetDisplayName() );
		stats.defences[nIndex].nArmorMin = ValueInt( *pDefProps, 0 );
		stats.defences[nIndex].nArmorMax = ValueInt( *pDefProps, 1 );
		stats.defences[nIndex].fSilhouette = ValueFloat( *pDefProps, 2 );
	}

	FillArray( stats.passability, frame.passability );
	stats.vOrigin = CVec2( frame.vOrigin.x, frame.vOrigin.y );
	FillArray( stats.visibility, frame.visibility );
	stats.vVisOrigin = CVec2( frame.vVisOrigin.x, frame.vVisOrigin.y );

	const NResourceXml::Node *pDesc = NResourceXml::FindChild( projectElement, "desc" );
	auto List = [&]( const char *pszName ) { return pDesc != nullptr ? Items( NResourceXml::FindChild( *pDesc, pszName ) ) : std::vector<const NResourceXml::Node *>(); };

	for ( const NResourceXml::Node *pItem : List( "Entrances" ) )
	{
		SBuildingRPGStats::SEntrance entrance;
		entrance.bStormable = false;
		entrance.vPos = ReadPos3( *pItem, "Position" );
		entrance.vPos.z = 0;
		stats.entrances.push_back( entrance );
	}

	std::size_t nPoint = 0;
	for ( const NResourceXml::Node *pItem : List( "FireSlots" ) )
	{
		const CTreeItem *pChild = NthChild( *pSlots, nPoint++ );
		SBuildingRPGStats::SSlot slot;
		slot.vPos = ReadPos3( *pItem, "Position" );
		slot.vPos.z = 0;
		slot.vPicturePosition = ReadPos2( *pItem, "PicturePosition" );
		slot.vWorldPosition = ReadPos3( *pItem, "WorldPosition" );
		slot.bShowFlashes = false;
		slot.fDirection = PointFloat( pChild, 0, *pItem, "Direction" );
		slot.fAngle = PointFloat( pChild, 1, *pItem, "Angle" );
		if ( pChild != nullptr )
		{
			slot.fSightMultiplier = ValueFloat( *pChild, 2 );
			slot.fCoverage = ValueFloat( *pChild, 3 );
			slot.gun.szWeapon = ValueStr( *pChild, 4 );
			slot.gun.nAmmo = ValueInt( *pChild, 5 );
			slot.fRotationSpeed = ValueFloat( *pChild, 6 );
			slot.gun.nPriority = ValueInt( *pChild, 7 );
		}
		slot.bBeforeSprite = slot.fDirection > 135.0f && slot.fDirection < 315.0f;
		stats.slots.push_back( slot );
	}

	nPoint = 0;
	for ( const NResourceXml::Node *pItem : List( "FirePoints" ) )
	{
		const CTreeItem *pChild = NthChild( *pFirePoints, nPoint++ );
		SBuildingRPGStats::SFirePoint fire;
		fire.vPos = ReadPos3( *pItem, "Position" );
		fire.vPos.z = 0;
		fire.vPicturePosition = ReadPos2( *pItem, "PicturePosition" );
		fire.vWorldPosition = ReadPos3( *pItem, "WorldPosition" );
		fire.fDirection = PointFloat( pChild, 0, *pItem, "Direction" );
		fire.fVerticalAngle = PointFloat( pChild, 2, *pItem, "VerticalAngle" );
		if ( pChild != nullptr )
			fire.szFireEffect = ValueStr( *pChild, 1 );
		stats.firePoints.push_back( fire );
	}

	// The five explosions are fixed: a missing one keeps the constructor's.
	stats.szDirExplosionEffect = ValueStr( *pDirExplosions, 0 );
	nPoint = 0;
	for ( const NResourceXml::Node *pItem : List( "DirExplosions" ) )
	{
		const CTreeItem *pChild = NthChild( *pDirExplosions, nPoint );
		if ( nPoint >= stats.dirExplosions.size() )
			stats.dirExplosions.resize( nPoint + 1 );
		SBuildingRPGStats::SDirectionExplosion &explosion = stats.dirExplosions[nPoint++];
		explosion.vPos = ReadPos3( *pItem, "Position" );
		explosion.vPos.z = 0;
		explosion.vPicturePosition = ReadPos2( *pItem, "PicturePosition" );
		explosion.vWorldPosition = ReadPos3( *pItem, "WorldPosition" );
		explosion.fDirection = PointFloat( pChild, 0, *pItem, "Direction" );
		explosion.fVerticalAngle = PointFloat( pChild, 1, *pItem, "VerticalAngle" );
	}

	stats.szSmokeEffect = ValueStr( *pSmokes, 0 );
	nPoint = 0;
	for ( const NResourceXml::Node *pItem : List( "SmokePoints" ) )
	{
		const CTreeItem *pChild = NthChild( *pSmokes, nPoint++ );
		SBuildingRPGStats::SFirePoint smoke;
		smoke.vPos = ReadPos3( *pItem, "Position" );
		smoke.vPos.z = 0;
		smoke.vPicturePosition = ReadPos2( *pItem, "PicturePosition" );
		smoke.vWorldPosition = ReadPos3( *pItem, "WorldPosition" );
		smoke.fDirection = PointFloat( pChild, 0, *pItem, "Direction" );
		smoke.fVerticalAngle = PointFloat( pChild, 1, *pItem, "VerticalAngle" );
		stats.smokePoints.push_back( smoke );
	}
	return true;
}

// The six pictures of the Graphics Info item as ComposeAnimations walks them:
// season k (summer, winter), picture i (whole, damaged, destroyed). The result
// name is the picture's number, 'w' for winter, and 'g' after it for a noise
// picture.
struct SPicture
{
	std::string szName;       // "1", "2w", ...
	std::string szSprite;     // the project's name for the picture
	std::string szShadow;
	std::string szNoise;      // empty for the whole picture
	bool bNoise = false;
};

std::vector<SPicture> PicturesOf( const CTreeItem &graphics )
{
	std::vector<SPicture> pictures;
	int k = 0;
	for ( const auto &pSeason : graphics.GetChildren() )
	{
		int i = 0;
		for ( const auto &pProps : pSeason->GetChildren() )
		{
			SPicture picture;
			picture.szName = std::to_string( i + 1 ) + ( k == 1 ? "w" : "" );
			picture.szSprite = ValueStr( *pProps, 0 );
			picture.szShadow = ValueStr( *pProps, 1 );
			picture.bNoise = i == 1 || i == 2;
			if ( picture.bNoise )
				picture.szNoise = ValueStr( *pProps, 2 );
			pictures.push_back( picture );
			++i;
		}
		++k;
	}
	return pictures;
}

// FindMaximalSourceTime: the project and every sprite and shadow it names.
fs::file_time_type MaximalSourceTime( const SExportContext &context, const std::vector<SPicture> &pictures )
{
	fs::file_time_type newest = ChangeTime( fs::path( context.szProjectPath ) );
	for ( const SPicture &picture : pictures )
	{
		newest = std::max( newest, ChangeTime( SourceFile( context, picture.szSprite ) ) );
		newest = std::max( newest, ChangeTime( SourceFile( context, picture.szShadow ) ) );
	}
	return newest;
}

// FindMinimalExportFileTime: the oldest of the three summer pictures' files and
// the stats.
fs::file_time_type MinimalExportTime( const SExportContext &context, const std::string &szResultDir )
{
	const fs::path exported = FoldedFile( ( fs::path( context.szDataRoot ) / ToSlashes( szResultDir ) ).string() );
	fs::file_time_type oldest = ChangeTime( FoldedChild( exported, "1.xml" ) );
	for ( int i = 1; i <= 3; ++i )
	{
		const std::string szName = std::to_string( i );
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

// ComposeAnimations: every picture packed with its shadow, the damaged and
// destroyed ones with their noise as well. A picture that cannot be composed is
// a warning that names it.
void ComposePictures( const SExportContext &context, const std::vector<SPicture> &pictures, const std::string &szResultDir,
                      const CVec2 &zeroPos2, const CArray2D<BYTE> &pass, const CVec2 &vLockedTilesCenter, SExportOutcome &outcome )
{
	const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( ProjectDirectory( context ) );
	for ( const SPicture &picture : pictures )
	{
		const std::string szSprite = SourceFile( context, picture.szSprite );
		if ( !NCompose::ComposeSingleObjectPack( context, gamma, GFXPF_ARGB1555, szSprite, SourceFile( context, picture.szShadow ), szResultDir + picture.szName,
		                                         zeroPos2, pass, vLockedTilesCenter, outcome ) )
			WarnLastError( outcome, "picture " + picture.szName + ": " );
		if ( picture.bNoise && !NCompose::ComposeNoisePack( context, gamma, GFXPF_ARGB1555, szSprite, SourceFile( context, picture.szNoise ),
		                                                    szResultDir + picture.szName + "g", zeroPos2, outcome ) )
			WarnLastError( outcome, "noise picture " + picture.szName + "g: " );
	}
}

// What a point item of the tree is called when the importer adds one.
const char *const kShootPointName = "Shoot point";
const char *const kFirePointName = "Fire point";
const char *const kSmokePointName = "Smoke point";

template <class T>
void SetValue( CTreeItem *pItem, std::size_t nSlot, const T &value )
{
	if ( pItem != nullptr && nSlot < pItem->MutableValues().size() )
		pItem->MutableValues()[nSlot].value = value;
}

CTreeItem *MutableChildOfType( CTreeItem &item, int nType, int nIndex = 0 )
{
	for ( const auto &pChild : item.GetChildren() )
		if ( pChild->GetItemType() == nType && nIndex-- == 0 )
			return pChild.get();
	return nullptr;
}

// The container's children replaced by count new ones of nType, named szName.
void ReplaceChildren( CTreeItem &container, int nType, const char *pszName, std::size_t nCount )
{
	container.MutableChildren().clear();
	for ( std::size_t i = 0; i < nCount; ++i )
	{
		auto pChild = CTreeItemFactory::Instance().Create( nType );
		if ( !pChild )
			return;
		pChild->SetItemName( pszName );
		container.AddChild( std::move( pChild ) );
	}
}

// One aimed point as desc stores it, without its picture and world positions.
NResourceXml::Node AimedItem( float fDirection, const char *pszConeAttr, float fCone, const CVec3 &vPos )
{
	NResourceXml::Node item = NewElement( "item" );
	SetAttr( item, "Direction", MfcFloat( fDirection ) );
	if ( pszConeAttr != nullptr )
		SetAttr( item, pszConeAttr, MfcFloat( fCone ) );
	item.children.push_back( Vec3Node( "Position", vPos ) );
	return item;
}

// The two positions every aimed point ends with.
void FinishItem( NResourceXml::Node &item, const CVec2 &vPicture, const CVec3 &vWorld )
{
	item.children.push_back( Vec2Node( "PicturePosition", vPicture ) );
	item.children.push_back( Vec3Node( "WorldPosition", vWorld ) );
}

void ReplaceList( NResourceXml::Node &desc, const char *pszName, std::vector<NResourceXml::Node> &&items )
{
	NResourceXml::Node &list = ChildOrNew( desc, pszName );
	list.children = std::move( items );
}

}

bool ExportBuilding( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_BUILDING_ROOT_ITEM, "building", outcome );
	if ( !pProject )
		return false;
	SObjectFrameData frame;
	if ( !ReadObjectFrameData( *pProject, frame, outcome.szError ) )
		return false;
	SBuildingRPGStats rpgStats;
	if ( !FillRPGStats( rpgStats, *pProject->root, frame, pProject->document.root, outcome ) )
		return false;
	const CTreeItem *pGraphics = RequireChild( *pProject->root, ETIT_BUILDING_GRAPHICS_ITEM, 0, "Graphics Info", outcome );
	if ( pGraphics == nullptr )
		return false;
	const std::vector<SPicture> pictures = PicturesOf( *pGraphics );

	const std::string szFile = StatsFileName( project, context, kBuildingAddDir, false );
	const std::string szResultDir = DirectoryOf( szFile );
	outcome.szObjectName = szResultDir + "1";

	if ( !context.bForce && !context.bStatsOnly && !context.szDataRoot.empty() &&
	     MinimalExportTime( context, szResultDir ) >= MaximalSourceTime( context, pictures ) )
	{
		++outcome.nSkipped;
		return true;
	}

	// ExportFrameData: without the default sprite nothing is exported.
	std::string szDefaultSprite;
	if ( !context.bStatsOnly )
	{
		const SPicture *pDefault = pictures.empty() ? nullptr : &pictures.front();
		szDefaultSprite = pDefault != nullptr ? SourceFile( context, pDefault->szSprite ) : std::string();
		if ( NImageExport::LoadPicture( szDefaultSprite, outcome ) == 0 )
		{
			outcome.szError = "Error: Can not load default sprite: " + outcome.szError + "; Can not continue export data";
			return false;
		}
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

		ComposePictures( context, pictures, szResultDir, zeroPos2, rpgStats.passability, vLockedTilesCenter, outcome );
		if ( !NCompose::SaveIconFile( context, szDefaultSprite, szResultDir + "icon.tga", outcome ) )
			WarnLastError( outcome, "icon: " );
	}
	return true;
}

void BuildingStatsToTree( const SBuildingRPGStats &stats, CTreeItem &root )
{
	CTreeItem *pCommonProps = MutableChildOfType( root, ETIT_BUILDING_COMMON_PROPS_ITEM );
	SetValue( pCommonProps, 0, stats.szKeyName );
	SetValue( pCommonProps, 1, std::string( kBuildingTypes[std::max( 0, std::min( int( stats.eType ), 3 ) )] ) );
	SetValue( pCommonProps, 2, int( stats.fMaxHP ) );
	SetValue( pCommonProps, 3, stats.fRepairCost );
	SetValue( pCommonProps, 4, stats.nRestSlots );
	SetValue( pCommonProps, 5, stats.nMedicalSlots );
	SetValue( pCommonProps, 6, stats.szAmbientSound );
	SetValue( pCommonProps, 7, stats.szCycledSound );

	// MFC's AI classes to pass item is filled by hand in the editor; the
	// importer adds one pass item per class bit the stats block.
	static const struct { DWORD dwClass; const char *pszName; } kClasses[] =
	{
		{ AI_CLASS_WHEEL, "wheel" }, { AI_CLASS_HALFTRACK, "halftrack" }, { AI_CLASS_TRACK, "track" }, { AI_CLASS_HUMAN, "human" },
	};
	if ( CTreeItem *pPasses = MutableChildOfType( root, ETIT_BUILDING_PASSES_ITEM ) )
	{
		pPasses->MutableChildren().clear();
		for ( const auto &entry : kClasses )
			if ( ( stats.dwAIClasses & entry.dwClass ) != 0 )
			{
				auto pPass = CTreeItemFactory::Instance().Create( ETIT_BUILDING_PASS_PROPS_ITEM );
				if ( !pPass )
					continue;
				pPass->SetItemName( entry.pszName );
				CTreeItem *pItem = pPass.get();
				pPasses->AddChild( std::move( pPass ) );
				SetValue( pItem, 0, std::string( entry.pszName ) );
			}
	}

	if ( CTreeItem *pDefences = MutableChildOfType( root, ETIT_BUILDING_DEFENCES_ITEM ) )
		for ( int i = 0; i < 6; ++i )
		{
			CTreeItem *pDefProps = MutableChildOfType( *pDefences, ETIT_BUILDING_DEFENCE_PROPS_ITEM, i );
			if ( pDefProps == nullptr )
				continue;
			const SDefenseRPGStats &defence = stats.defences[DefenceIndex( pDefProps->GetDisplayName() )];
			SetValue( pDefProps, 0, defence.nArmorMin );
			SetValue( pDefProps, 1, defence.nArmorMax );
			// MFC's LoadRPGStats replaced a silhouette outside 0..1 by 1.
			SetValue( pDefProps, 2, defence.fSilhouette < 0 || defence.fSilhouette > 1 ? 1.0f : defence.fSilhouette );
		}

	if ( CTreeItem *pSlots = MutableChildOfType( root, ETIT_BUILDING_SLOTS_ITEM ) )
	{
		ReplaceChildren( *pSlots, ETIT_BUILDING_SLOT_PROPS_ITEM, kShootPointName, stats.slots.size() );
		std::size_t i = 0;
		for ( const auto &pChild : pSlots->GetChildren() )
		{
			const SBuildingRPGStats::SSlot &slot = stats.slots[i++];
			SetValue( pChild.get(), 0, slot.fDirection );
			SetValue( pChild.get(), 1, slot.fAngle );
			SetValue( pChild.get(), 2, slot.fSightMultiplier );
			SetValue( pChild.get(), 3, slot.fCoverage );
			SetValue( pChild.get(), 4, slot.gun.szWeapon );
			SetValue( pChild.get(), 5, slot.gun.nAmmo );
			SetValue( pChild.get(), 6, slot.fRotationSpeed );
			SetValue( pChild.get(), 7, slot.gun.nPriority );
		}
	}

	if ( CTreeItem *pFires = MutableChildOfType( root, ETIT_BUILDING_FIRE_POINTS_ITEM ) )
	{
		ReplaceChildren( *pFires, ETIT_BUILDING_FIRE_POINT_PROPS_ITEM, kFirePointName, stats.firePoints.size() );
		std::size_t i = 0;
		for ( const auto &pChild : pFires->GetChildren() )
		{
			const SBuildingRPGStats::SFirePoint &fire = stats.firePoints[i++];
			SetValue( pChild.get(), 0, fire.fDirection );
			SetValue( pChild.get(), 1, fire.szFireEffect );
			SetValue( pChild.get(), 2, fire.fVerticalAngle );
		}
	}

	if ( CTreeItem *pDirExplosions = MutableChildOfType( root, ETIT_BUILDING_DIR_EXPLOSIONS_ITEM ) )
	{
		SetValue( pDirExplosions, 0, stats.szDirExplosionEffect );
		std::size_t i = 0;
		for ( const auto &pChild : pDirExplosions->GetChildren() )
		{
			if ( i >= stats.dirExplosions.size() )
				break;
			SetValue( pChild.get(), 0, stats.dirExplosions[i].fDirection );
			SetValue( pChild.get(), 1, stats.dirExplosions[i].fVerticalAngle );
			++i;
		}
	}

	if ( CTreeItem *pSmokes = MutableChildOfType( root, ETIT_BUILDING_SMOKES_ITEM ) )
	{
		SetValue( pSmokes, 0, stats.szSmokeEffect );
		ReplaceChildren( *pSmokes, ETIT_BUILDING_SMOKE_PROPS_ITEM, kSmokePointName, stats.smokePoints.size() );
		std::size_t i = 0;
		for ( const auto &pChild : pSmokes->GetChildren() )
		{
			const SBuildingRPGStats::SFirePoint &smoke = stats.smokePoints[i++];
			SetValue( pChild.get(), 0, smoke.fDirection );
			SetValue( pChild.get(), 1, smoke.fVerticalAngle );
		}
	}
}

void WriteBuildingFrameData( NResourceXml::Node &root, const SBuildingRPGStats &stats )
{
	// The frame's constructor values: both crosses at 16 world cells.
	SObjectFrameData frame;
	SBuildingRPGStats &mutableStats = const_cast<SBuildingRPGStats &>( stats );
	frame.passability = GridOf( mutableStats.passability );
	frame.vOrigin = SVec2{ stats.vOrigin.x, stats.vOrigin.y };
	frame.visibility = GridOf( mutableStats.visibility );
	frame.vVisOrigin = SVec2{ stats.vVisOrigin.x, stats.vVisOrigin.y };

	NResourceXml::Node &ownData = ChildOrNew( root, "own_data" );
	for ( const char *pszCross : { "sprite_pos", "krest_pos" } )
	{
		const SVec3 &v = pszCross[0] == 's' ? frame.vSpritePos : frame.vZeroPos;
		NResourceXml::Node &node = ChildOrNew( ownData, pszCross );
		SetAttr( node, "x", MfcFloat( v.x ) );
		SetAttr( node, "y", MfcFloat( v.y ) );
		SetAttr( node, "z", MfcFloat( v.z ) );
	}
	WriteObjectGrids( root, frame );

	NResourceXml::Node &desc = ChildOrNew( root, "desc" );

	std::vector<NResourceXml::Node> entrances;
	for ( const auto &entrance : stats.entrances )
	{
		NResourceXml::Node item = NewElement( "item" );
		SetAttr( item, "Stormable", entrance.bStormable ? "1" : "0" );
		item.children.push_back( Vec3Node( "Position", entrance.vPos ) );
		entrances.push_back( std::move( item ) );
	}
	ReplaceList( desc, "Entrances", std::move( entrances ) );

	std::vector<NResourceXml::Node> slots;
	for ( const auto &slot : stats.slots )
	{
		NResourceXml::Node item = AimedItem( slot.fDirection, "Angle", slot.fAngle, slot.vPos );
		SetAttr( item, "SightMultiplier", MfcFloat( slot.fSightMultiplier ) );
		SetAttr( item, "Coverage", MfcFloat( slot.fCoverage ) );
		SetAttr( item, "Ammo", MfcInt( slot.gun.nAmmo ) );
		SetAttr( item, "GunPriority", MfcInt( slot.gun.nPriority ) );
		SetAttr( item, "RotationSpeed", MfcFloat( slot.fRotationSpeed ) );
		SetAttr( item, "BeforeSprite", slot.bBeforeSprite ? "1" : "0" );
		SetAttr( item, "ShowFlashes", slot.bShowFlashes ? "1" : "0" );
		item.children.push_back( StringElement( "Weapon", slot.gun.szWeapon ) );
		FinishItem( item, slot.vPicturePosition, slot.vWorldPosition );
		slots.push_back( std::move( item ) );
	}
	ReplaceList( desc, "FireSlots", std::move( slots ) );

	auto WritePoints = [&]( const char *pszList, const std::vector<SBuildingRPGStats::SFirePoint> &points, bool bEffect )
	{
		std::vector<NResourceXml::Node> items;
		for ( const auto &point : points )
		{
			NResourceXml::Node item = AimedItem( point.fDirection, "VerticalAngle", point.fVerticalAngle, point.vPos );
			if ( bEffect )
				item.children.push_back( StringElement( "FireEffect", point.szFireEffect ) );
			FinishItem( item, point.vPicturePosition, point.vWorldPosition );
			items.push_back( std::move( item ) );
		}
		ReplaceList( desc, pszList, std::move( items ) );
	};
	WritePoints( "FirePoints", stats.firePoints, true );
	WritePoints( "SmokePoints", stats.smokePoints, true );

	std::vector<NResourceXml::Node> explosions;
	for ( const auto &explosion : stats.dirExplosions )
	{
		NResourceXml::Node item = AimedItem( explosion.fDirection, "VerticalAngle", explosion.fVerticalAngle, explosion.vPos );
		FinishItem( item, explosion.vPicturePosition, explosion.vWorldPosition );
		explosions.push_back( std::move( item ) );
	}
	ReplaceList( desc, "DirExplosions", std::move( explosions ) );
}

}
