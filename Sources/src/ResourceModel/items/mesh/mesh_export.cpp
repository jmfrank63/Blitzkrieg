// The unit exporter: CMeshFrame::FillRPGStats and SaveRPGStats (Sources/src/
// editor/MeshFrm.cpp:300-1134), ported line for line into SMechUnitRPGStats and
// written to 1.xml as tree.Add( "RPG", &stats ).
//
// MFC asked the preview objects it had built from the three .mod files
// (pCombatObject, pInstallObject, pTransObject) for node names and for the
// bind-pose matrices of the combat model. The port reads the same data from
// the .mod files through the engine's structure loader (D019): the skeleton
// of chunk 1 gives the names, and the matrices are the skeleton's base pose
// walked with the engine's own matrix stack, which is what
// CMeshAnimation::GetMatrices( MONE ) returned for an object with no
// animation playing. Export needs no window and no GPU.
//
// What MFC's message boxes said goes into the outcome: a combat model that
// cannot be loaded fails the export, a platform or gun part that is no node of
// the model is a warning naming it (MFC stored -1 and went on).
//
// The graphics half is ExportFrameData (MeshFrm.cpp:1313-1545): every .mod of
// the combat model's folder copied beside the stats, the six alive and dead
// season textures, the icons and the localisation texts. As in MFC a texture
// that cannot be converted does not fail the export: it is a warning (MFC's
// message box) and the stats are written all the same, so a stats-only export
// and the full one write the same 1.xml.
//
// The up-to-date check ports FindMaximalSourceTime and FindMinimalExportFileTime
// (MeshFrm.cpp:2137-2306) with two corrections of MFC's own slips. It looked
// for 1.tga ... 2a.tga in the export, which the export never writes (it
// writes .dds), so a mesh was never up to date; the port looks for the
// textures' _c/_l/_h.dds. And the icon's time was read from the previous
// texture's path; the port reads icon.tga itself.
#include "StdAfx.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "mesh.h"
#include "../stats_export.h"
#include "../../image_export.h"
#include "../tree_item_types.h"
#include "../../../Main/RPGStats.h"
#include "../../../Formats/fmtMesh.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;
namespace fs = std::filesystem;

const char kMeshAddDir[] = "units\\technics\\";

// A value as MFC's int64 conversion (CUnitActionsItem::GetActions).
long long ValueInt64( const CTreeItem &item, int nIndex )
{
	const CPropVector &values = item.GetValues();
	if ( nIndex < 0 || nIndex >= int( values.size() ) )
		return 0;
	const CVariant &value = values[nIndex].value;
	switch ( value.GetKind() )
	{
		case CVariant::VK_INT64: return value.AsInt64();
		case CVariant::VK_STR:   return (long long) std::strtoull( value.AsStr().c_str(), nullptr, 16 );
		default:                 return ValueInt( item, nIndex );
	}
}

// CMeshCommonPropsItem::GetMeshType; a name MFC asserted on is a carrier.
EUnitRPGType MeshType( const std::string &szName )
{
	static const struct { const char *pszName; EUnitRPGType type; } kTypes[] =
	{
		{ "transport carrier", RPG_TYPE_TRN_CARRIER }, { "transport support", RPG_TYPE_TRN_SUPPORT },
		{ "transport medicine", RPG_TYPE_TRN_MEDICINE }, { "transport tractor", RPG_TYPE_TRN_TRACTOR },
		{ "transport military auto", RPG_TYPE_TRN_MILITARY_AUTO }, { "transport civilian auto", RPG_TYPE_TRN_CIVILIAN_AUTO },
		{ "artillery gun", RPG_TYPE_ART_GUN }, { "artillery howitzer", RPG_TYPE_ART_HOWITZER },
		{ "artillery heavy gun", RPG_TYPE_ART_HEAVY_GUN }, { "artillery heavy machine gun", RPG_TYPE_ART_HEAVY_MG },
		{ "artillery antiair gun", RPG_TYPE_ART_AAGUN }, { "artillery rocket", RPG_TYPE_ART_ROCKET },
		{ "artillery super", RPG_TYPE_ART_SUPER }, { "artillery mortar", RPG_TYPE_ART_MORTAR },
		{ "SPG assault", RPG_TYPE_SPG_ASSAULT }, { "SPG antitank", RPG_TYPE_SPG_ANTITANK },
		{ "SPG super", RPG_TYPE_SPG_SUPER }, { "SPG antiair", RPG_TYPE_SPG_AAGUN },
		{ "armor light", RPG_TYPE_ARM_LIGHT }, { "armor medium", RPG_TYPE_ARM_MEDIUM },
		{ "armor super", RPG_TYPE_ARM_SUPER }, { "armor heavy", RPG_TYPE_ARM_HEAVY },
		{ "avia scout", RPG_TYPE_AVIA_SCOUT }, { "avia bomber", RPG_TYPE_AVIA_BOMBER },
		{ "avia attack", RPG_TYPE_AVIA_ATTACK }, { "avia fighter", RPG_TYPE_AVIA_FIGHTER },
		{ "avia super", RPG_TYPE_AVIA_SUPER }, { "avia lander", RPG_TYPE_AVIA_LANDER },
		{ "train locomotive", RPG_TYPE_TRAIN_LOCOMOTIVE }, { "train cargo", RPG_TYPE_TRAIN_CARGO },
		{ "train carrier", RPG_TYPE_TRAIN_CARRIER }, { "train super", RPG_TYPE_TRAIN_SUPER },
		{ "train armor", RPG_TYPE_TRAIN_ARMOR },
	};
	for ( const auto &type : kTypes )
		if ( szName == type.pszName )
			return type.type;
	return RPG_TYPE_TRN_CARRIER;
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

struct SMyGunner
{
	int nIndex;
	std::string szName;

	bool operator<( const SMyGunner &a ) const { return szName < a.szName; }
};

// What one .mod file gives the export: the node names and the base pose.
struct SModel
{
	SSkeletonFormat skeleton;
	std::vector<std::string> names;      // IMeshAnimationEdit::GetAllNodeNames, in skeleton order
	std::vector<SHMatrix> matrices;      // CMeshAnimation::GetMatrices( MONE ) of an object at rest, by node index
	int NumNodes() const { return int( names.size() ); }
};

// CMeshSkeleton's walk with no animation: every node pushes its base
// (bone, rotation) on the stack and keeps the product.
void WalkBase( const SSkeletonFormat &skeleton, int nNode, CMatrixStack<32> &mstack, std::vector<SHMatrix> &matrices, int nDepth )
{
	const SSkeletonFormat::SNodeFormat *pNode = nullptr;
	for ( const auto &node : skeleton.nodes )
		if ( node.nIndex == nNode )
		{
			pNode = &node;
			break;
		}
	if ( pNode == nullptr || nDepth > 30 )
		return;
	SHMatrix matBase;
	matBase.Set( pNode->bone, CQuat( pNode->quat ) );
	mstack.Push43( matBase );
	if ( nNode >= 0 && nNode < int( matrices.size() ) )
		matrices[nNode] = mstack();
	for ( int nChild : pNode->children )
		WalkBase( skeleton, nChild, mstack, matrices, nDepth + 1 );
	mstack.Pop();
}

void BuildModel( SModel &model )
{
	model.names.clear();
	for ( const auto &node : model.skeleton.nodes )
		model.names.push_back( node.szName );
	model.matrices.assign( model.names.size(), SHMatrix() );
	for ( SHMatrix &matrix : model.matrices )
		Identity( &matrix );
	if ( model.names.empty() )
		return;
	CMatrixStack<32> mstack;
	mstack.Set( MONE );
	WalkBase( model.skeleton, model.skeleton.nTopNode, mstack, model.matrices, 0 );
}

// MFC's MakeFullPath( GetDirectory( project ), rel ) for a project-relative
// name, the name itself otherwise, folded to this file system.
fs::path ModelFile( const SExportContext &context, const std::string &szRelName )
{
	std::string szName = szRelName;
	std::replace( szName.begin(), szName.end(), '/', '\\' );
	if ( IsRelatedPath( szName ) )
	{
		std::string szDir = ProjectDirectory( context );
		std::replace( szDir.begin(), szDir.end(), '/', '\\' );
		szName = MakeFullPath( szDir, szName );
	}
	return FoldedFile( szName );
}

// OpenFileStream( file, STREAM_ACCESS_READ ): null when the file is not there.
CPtr<IDataStream> OpenModel( const fs::path &file )
{
	std::string szDir = file.parent_path().string();
	if ( szDir.empty() || szDir.back() != '/' )
		szDir += '/';
	CPtr<IDataStorage> pStorage = OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	if ( pStorage == 0 )
		return 0;
	return pStorage->OpenStream( file.filename().string().c_str(), STREAM_ACCESS_READ );
}

void AddAnimations( SMechUnitRPGStats &rpgStats, const std::vector<SAnimationFormat> &animations )
{
	for ( int i = 0; i < int( animations.size() ); i++ )
	{
		SUnitBaseRPGStats::SAnimDesc desc;
		desc.nIndex = i;
		desc.nLength = animations[i].GetLength();
		desc.nAction = animations[i].GetActionTime();
		desc.nAABB_A = animations[i].nAABB_AIndex;
		desc.nAABB_D = animations[i].nAABB_DIndex;
		rpgStats.animdescs[animations[i].nType].push_back( desc );
	}
}

// The model file of an install or transportable variant: skipped, as MFC
// skipped a preview object that did not build, when the file is not there.
// Reads its animations (and the boxes when the combat model had none) into
// the stats, and its skeleton into model.
bool ReadVariant( const SExportContext &context, const std::string &szRelName, SMechUnitRPGStats &rpgStats,
                  std::vector<SAnimationFormat> &animations, std::vector<SAABBFormat> &aabb_as, std::vector<SAABBFormat> &aabb_ds, SModel &model )
{
	if ( szRelName.empty() )
		return false;
	CPtr<IDataStream> pStream = OpenModel( ModelFile( context, szRelName ) );
	if ( pStream == 0 )
		return false;
	CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
	CSaverAccessor saver = pSaver;
	saver.Add( 1, &model.skeleton );
	saver.Add( 3, &animations );
	if ( aabb_as.size() == 0 )
		saver.Add( 5, &aabb_as );
	if ( aabb_ds.size() == 0 )
		saver.Add( 6, &aabb_ds );
	AddAnimations( rpgStats, animations );
	BuildModel( model );
	return true;
}

// The position in names of the node called szName, names.size() when none.
int FindNode( const std::vector<std::string> &names, const std::string &szName )
{
	int i = 0;
	for ( ; i < int( names.size() ); i++ )
		if ( names[i] == szName )
			break;
	return i;
}

bool StartsWith( const std::string &sz, const char *pszPrefix )
{
	return sz.compare( 0, std::strlen( pszPrefix ), pszPrefix ) == 0;
}

// CMeshFrame::FillRPGStats.
bool FillRPGStats( SMechUnitRPGStats &rpgStats, const CTreeItem &rootItem, const SExportContext &context, SExportOutcome &outcome )
{
	SSkeletonFormat skeleton;
	SAABBFormat aabb;
	std::vector<SAABBFormat> aabb_as;
	std::vector<SAABBFormat> aabb_ds;
	rpgStats.animdescs.resize( ANIMATION_LAST_ANIMATION );

	const CTreeItem *pGraphicsItem = RequireChild( rootItem, ETIT_MESH_GRAPHICS_ITEM, 0, "Graphics Info", outcome );
	const CTreeItem *pCommonProps = RequireChild( rootItem, ETIT_MESH_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	const CTreeItem *pAcks = RequireChild( rootItem, ETIT_UNIT_ACKS_ITEM, 0, "Acknowledgments", outcome );
	const CTreeItem *pEffects = RequireChild( rootItem, ETIT_MESH_EFFECTS_ITEM, 0, "Effects", outcome );
	const CTreeItem *pActions = RequireChild( rootItem, ETIT_UNIT_ACTIONS_ITEM, 0, "Actions", outcome );
	const CTreeItem *pExposures = RequireChild( rootItem, ETIT_UNIT_EXPOSURES_ITEM, 0, "Exposures", outcome );
	const CTreeItem *pDefencesItem = RequireChild( rootItem, ETIT_MESH_DEFENCES_ITEM, 0, "Defences", outcome );
	const CTreeItem *pJoggingsItem = RequireChild( rootItem, ETIT_MESH_JOGGINGS_ITEM, 0, "Joggings", outcome );
	const CTreeItem *pPlatforms = RequireChild( rootItem, ETIT_MESH_PLATFORMS_ITEM, 0, "Platforms", outcome );
	if ( !pGraphicsItem || !pCommonProps || !pAcks || !pEffects || !pActions || !pExposures || !pDefencesItem || !pJoggingsItem || !pPlatforms )
		return false;
	const CTreeItem *pDeathCratersItem = RequireChild( *pGraphicsItem, ETIT_MESH_DEATH_CRATERS_ITEM, 0, "Death craters", outcome );
	if ( pDeathCratersItem == nullptr )
		return false;
	for ( const auto &pProps : pDeathCratersItem->GetChildren() )
		rpgStats.deathCraters.push_back( ValueStr( *pProps, 0 ) );

	SModel combat;
	SModel install;
	SModel trans;
	bool bInstall = false;
	bool bTrans = false;
	{
		const std::string szCombat = ValueStr( *pGraphicsItem, 0 );
		const fs::path combatFile = ModelFile( context, szCombat );
		CPtr<IDataStream> pStream = szCombat.empty() ? CPtr<IDataStream>( 0 ) : OpenModel( combatFile );
		if ( pStream == 0 )
		{
			outcome.szError = "Error: Can not load combat mechanics file, aborting (" + combatFile.string() + ")";
			return false;
		}
		std::vector<SAnimationFormat> animations;
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
		CSaverAccessor saver = pSaver;
		saver.Add( 1, &skeleton );
		saver.Add( 3, &animations );
		saver.Add( 4, &aabb );
		saver.Add( 5, &aabb_as );
		saver.Add( 6, &aabb_ds );
		AddAnimations( rpgStats, animations );
		combat.skeleton = skeleton;
		BuildModel( combat );
		NStr::DebugTrace( "msh export: combat model %s, %d nodes", combatFile.string().c_str(), combat.NumNodes() );

		bInstall = ReadVariant( context, ValueStr( *pGraphicsItem, 1 ), rpgStats, animations, aabb_as, aabb_ds, install );
		bTrans = ReadVariant( context, ValueStr( *pGraphicsItem, 2 ), rpgStats, animations, aabb_as, aabb_ds, trans );
	}

	rpgStats.vAABBCenter.x = aabb.vCenter.x;
	rpgStats.vAABBCenter.y = aabb.vCenter.y;
	rpgStats.vAABBHalfSize.x = aabb.vHalfSize.x;
	rpgStats.vAABBHalfSize.y = aabb.vHalfSize.y;
	for ( const SAABBFormat &box : aabb_as )
	{
		SUnitBaseRPGStats::SAABBDesc desc;
		desc.vCenter.x = box.vCenter.x;
		desc.vCenter.y = box.vCenter.y;
		desc.vHalfSize.x = box.vHalfSize.x;
		desc.vHalfSize.y = box.vHalfSize.y;
		rpgStats.aabb_as.push_back( desc );
	}
	for ( const SAABBFormat &box : aabb_ds )
	{
		SUnitBaseRPGStats::SAABBDesc desc;
		desc.vCenter.x = box.vCenter.x;
		desc.vCenter.y = box.vCenter.y;
		desc.vHalfSize.x = box.vHalfSize.x;
		desc.vHalfSize.y = box.vHalfSize.y;
		rpgStats.aabb_ds.push_back( desc );
	}

	const std::vector<SHMatrix> &modelMatrix = combat.matrices;

	rpgStats.szKeyName = ValueStr( *pCommonProps, 0 );
	rpgStats.type = MeshType( ValueStr( *pCommonProps, 1 ) );
	rpgStats.aiClass = (EAIClass) AIClass( ValueStr( *pCommonProps, 2 ) );
	rpgStats.fMaxHP = ValueFloat( *pCommonProps, 4 );
	rpgStats.fRepairCost = ValueFloat( *pCommonProps, 5 );
	rpgStats.fSight = ValueFloat( *pCommonProps, 21 );
	rpgStats.fCamouflage = ValueFloat( *pCommonProps, 6 );
	rpgStats.fSpeed = ValueFloat( *pCommonProps, 7 );
	rpgStats.fPassability = ValueFloat( *pCommonProps, 8 );
	rpgStats.fTowingForce = ValueFloat( *pCommonProps, 9 );
	rpgStats.fUninstallRotate = ValueFloat( *pCommonProps, 10 );
	rpgStats.fUninstallTransport = ValueFloat( *pCommonProps, 11 );
	rpgStats.fWeight = ValueFloat( *pCommonProps, 12 );
	rpgStats.nCrew = ValueInt( *pCommonProps, 13 );
	rpgStats.nPassangers = ValueInt( *pCommonProps, 14 );
	rpgStats.nPriority = ValueInt( *pCommonProps, 15 );
	rpgStats.fRotateSpeed = ValueFloat( *pCommonProps, 16 );
	rpgStats.nBoundTileRadius = ValueInt( *pCommonProps, 17 );
	rpgStats.fTurnRadius = ValueFloat( *pCommonProps, 18 );
	rpgStats.fSmallAABBCoeff = ValueFloat( *pCommonProps, 19 );
	rpgStats.fPrice = ValueFloat( *pCommonProps, 20 );
	rpgStats.fSightPower = ValueFloat( *pCommonProps, 22 );

	rpgStats.szAcksNames.resize( 1 );
	rpgStats.szAcksNames[0] = ValueStr( *pAcks, 0 );

	rpgStats.szEffectDiesel = ValueStr( *pEffects, 0 );
	rpgStats.szEffectSmoke = ValueStr( *pEffects, 1 );
	rpgStats.szEffectWheelDust = ValueStr( *pEffects, 2 );
	rpgStats.szEffectShootDust = ValueStr( *pEffects, 3 );
	rpgStats.szEffectFatality = ValueStr( *pEffects, 4 );
	rpgStats.szEffectDisappear = ValueStr( *pEffects, 5 );
	rpgStats.szSoundMoveStart = ValueStr( *pEffects, 6 );
	rpgStats.szSoundMoveStop = ValueStr( *pEffects, 8 );
	rpgStats.szSoundMoveCycle = ValueStr( *pEffects, 7 );

	if ( const CTreeItem *pAviaProps = ChildItem( *pCommonProps, ETIT_MESH_AVIA_ITEM ) )
	{
		rpgStats.fMaxHeight = ValueFloat( *pAviaProps, 0 );
		rpgStats.fDivingAngle = ValueFloat( *pAviaProps, 1 );
		rpgStats.fClimbAngle = ValueFloat( *pAviaProps, 2 );
		rpgStats.fTiltAngle = ValueFloat( *pAviaProps, 3 );
		rpgStats.fTiltRatio = ValueFloat( *pAviaProps, 4 );
	}
	if ( const CTreeItem *pTrackProps = ChildItem( *pCommonProps, ETIT_MESH_TRACK_ITEM ) )
	{
		rpgStats.bLeavesTracks = ValueBool( *pTrackProps, 0 );
		rpgStats.fTrackWidth = ValueFloat( *pTrackProps, 1 );
		rpgStats.fTrackOffset = ValueFloat( *pTrackProps, 2 );
		rpgStats.fTrackStart = ValueFloat( *pTrackProps, 3 );
		rpgStats.fTrackEnd = ValueFloat( *pTrackProps, 4 );
		rpgStats.fTrackIntensity = ValueFloat( *pTrackProps, 5 );
		rpgStats.nTrackLifetime = ValueInt( *pTrackProps, 6 );
	}

	const long long nActions = ValueInt64( *pActions, 0 );
	rpgStats.availCommands.Clear();
	for ( int i = 0; i < 64; i++ )
		if ( nActions & ( (long long) 1 << i ) )
			rpgStats.AddCommand( i );
	const long long nExposures = ValueInt64( *pExposures, 0 );
	rpgStats.availExposures.Clear();
	for ( int i = 0; i < 64; i++ )
		if ( nExposures & ( (long long) 1 << i ) )
		{
			if ( rpgStats.availExposures.GetSize() <= i )
				rpgStats.availExposures.SetSize( i + 1 );
			rpgStats.availExposures.SetData( i );
		}

	for ( int i = 0; i < 6; i++ )
	{
		const CTreeItem *pDefProps = RequireChild( *pDefencesItem, ETIT_MESH_DEFENCE_PROPS_ITEM, i, "Defence", outcome );
		if ( pDefProps == nullptr )
			return false;
		const std::string &szSide = pDefProps->GetDisplayName();
		int nIndex = 0;
		if ( szSide == "Left" )
			nIndex = RPG_LEFT;
		else if ( szSide == "Right" )
			nIndex = RPG_RIGHT;
		else if ( szSide == "Top" )
			nIndex = RPG_TOP;
		else if ( szSide == "Bottom" )
			nIndex = RPG_BOTTOM;
		else if ( szSide == "Front" )
			nIndex = RPG_FRONT;
		else if ( szSide == "Back" )
			nIndex = RPG_BACK;
		rpgStats.armors[nIndex].fMin = ValueInt( *pDefProps, 0 );
		rpgStats.armors[nIndex].fMax = ValueInt( *pDefProps, 1 );
	}

	for ( int i = 0; i < 3; i++ )
	{
		const CTreeItem *pJogProps = RequireChild( *pJoggingsItem, ETIT_MESH_JOGGING_PROPS_ITEM, i, "Jogging", outcome );
		if ( pJogProps == nullptr )
			return false;
		SMechUnitRPGStats::SJoggingParams *pJog = i == 0 ? &rpgStats.jx : ( i == 1 ? &rpgStats.jy : &rpgStats.jz );
		pJog->fPeriod1 = ValueFloat( *pJogProps, 0 );
		pJog->fPeriod2 = ValueFloat( *pJogProps, 1 );
		pJog->fAmp1 = ValueFloat( *pJogProps, 2 );
		pJog->fAmp2 = ValueFloat( *pJogProps, 3 );
		pJog->fPhase1 = ValueFloat( *pJogProps, 4 );
		pJog->fPhase2 = ValueFloat( *pJogProps, 5 );
	}

	int nMyAmmoIndex = -1;
	// The install and transportable models only gave their exhaust points.
	for ( const SModel *pModel : { bInstall ? &install : nullptr, bTrans ? &trans : nullptr } )
		if ( pModel != nullptr )
			for ( int i = 0; i < pModel->NumNodes(); i++ )
				if ( StartsWith( pModel->names[i], "LExhaust" ) )
					rpgStats.exhaustPoints.push_back( i );

	const std::vector<std::string> &allNames = combat.names;
	const int nNumNodes = combat.NumNodes();
	for ( int i = 0; i < nNumNodes; i++ )
	{
		const std::string &szNode = allNames[i];
		if ( StartsWith( szNode, "LExhaust" ) )
		{
			rpgStats.exhaustPoints.push_back( i );
			continue;
		}
		if ( StartsWith( szNode, "LPeople" ) )
		{
			rpgStats.nEntrancePoint = i;
			continue;
		}
		if ( StartsWith( szNode, "LGunner" ) )
			rpgStats.peoplePointIndices.push_back( i );
		if ( StartsWith( szNode, "LAmmo" ) )
		{
			nMyAmmoIndex = i;
			continue;
		}
		if ( StartsWith( szNode, "LShootDust" ) )
		{
			rpgStats.nShootDustPoint = i;
			continue;
		}
	}

	rpgStats.guns.clear();
	rpgStats.platforms.resize( pPlatforms->GetChildren().size() );
	int nPlatformIndex = 0;
	for ( const auto &pPlatformProps : pPlatforms->GetChildren() )
	{
		SMechUnitRPGStats::SPlatform &platform = rpgStats.platforms[nPlatformIndex];
		platform.fVerticalRotationSpeed = ValueFloat( *pPlatformProps, 3 );
		platform.fHorizontalRotationSpeed = ValueFloat( *pPlatformProps, 4 );
		if ( nNumNodes != 0 )
		{
			std::string szPartName = ValueStr( *pPlatformProps, 0 );
			int i = FindNode( allNames, szPartName );
			if ( i == nNumNodes )
			{
				platform.constraint.fMin = 0;
				platform.constraint.fMax = 0;
				platform.nModelPart = -1;
				outcome.warnings.push_back( "platform \"" + pPlatformProps->GetDisplayName() + "\": locator \"" + szPartName + "\" is no node of the combat model" );
			}
			else
			{
				platform.nModelPart = i;
				for ( const auto &node : skeleton.nodes )
					if ( node.nIndex == platform.nModelPart )
					{
						platform.constraint.fMin = node.constraint.fMin;
						platform.constraint.fMax = node.constraint.fMax;
					}
			}

			platform.dwGunCarriageParts = 0xffff0000;
			platform.constraintVertical.fMin = 0;
			platform.constraintVertical.fMax = 0;
			for ( int k = 0; k < 2; k++ )
			{
				szPartName = ValueStr( *pPlatformProps, 1 + k );
				i = FindNode( allNames, szPartName );
				if ( i == nNumNodes )
					platform.dwGunCarriageParts |= 255 << ( 8 * k );
				else
				{
					platform.dwGunCarriageParts |= i << ( 8 * k );
					for ( const auto &node : skeleton.nodes )
						if ( node.nIndex == i )
						{
							if ( platform.constraintVertical.fMin == 0 )
								platform.constraintVertical.fMin = node.constraint.fMin;
							if ( platform.constraintVertical.fMax == 0 )
								platform.constraintVertical.fMax = node.constraint.fMax;
						}
				}
			}
		}

		if ( const CTreeItem *pGuns = ChildItem( *pPlatformProps, ETIT_MESH_GUNS_ITEM ) )
		{
			for ( const auto &pGunProps : pGuns->GetChildren() )
			{
				SMechUnitRPGStats::SGun gun;
				if ( nNumNodes != 0 )
				{
					const std::string szPointName = ValueStr( *pGunProps, 0 );
					int i = FindNode( allNames, szPointName );
					if ( i == nNumNodes )
					{
						gun.nShootPoint = -1;
						outcome.warnings.push_back( "gun \"" + pGunProps->GetDisplayName() + "\": shoot point locator \"" + szPointName + "\" is no node of the combat model" );
					}
					else
					{
						gun.nShootPoint = i;
						CVec3 vRes;
						modelMatrix[gun.nShootPoint].RotateVector( &vRes, CVec3( 0, 0, 1 ) );
						float alpha = atan2( vRes.y, vRes.x );
						alpha += (float) 3 * FP_PI2;
						if ( alpha > FP_2PI )
							alpha -= FP_2PI;
						gun.wDirection = alpha * 65535 / FP_2PI;
					}

					const std::string szGunPartName = ValueStr( *pGunProps, 1 );
					i = FindNode( allNames, szGunPartName );
					if ( i == nNumNodes )
					{
						gun.fRecoilLength = 0;
						gun.nModelPart = -1;
					}
					else
					{
						gun.nModelPart = i;
						for ( const auto &node : skeleton.nodes )
							if ( node.nIndex == gun.nModelPart )
								gun.fRecoilLength = fabs( node.constraint.fMax - node.constraint.fMin );
					}
				}
				else
					gun.nShootPoint = -1;

				gun.szWeapon = ValueStr( *pGunProps, 2 );
				gun.nPriority = ValueInt( *pGunProps, 3 );
				gun.bRecoil = ValueBool( *pGunProps, 4 );
				gun.recoilTime = ValueInt( *pGunProps, 5 );
				gun.nRecoilShakeTime = ValueInt( *pGunProps, 6 );
				gun.fRecoilShakeAngle = ValueFloat( *pGunProps, 7 );
				gun.nAmmo = ValueInt( *pGunProps, 8 );
				gun.fReloadCost = ValueFloat( *pGunProps, 9 );
				rpgStats.guns.push_back( gun );
			}
		}
		if ( nPlatformIndex == 0 )
			platform.nFirstGun = 0;
		else
			platform.nFirstGun = rpgStats.platforms[nPlatformIndex - 1].nFirstGun + rpgStats.platforms[nPlatformIndex - 1].nNumGuns;
		platform.nNumGuns = rpgStats.guns.size() - platform.nFirstGun;

		nPlatformIndex++;
	}

	if ( rpgStats.nEntrancePoint != -1 )
	{
		const CVec3 vEntranceTrans = modelMatrix[rpgStats.nEntrancePoint].GetTrans3();
		rpgStats.vEntrancePoint.x = vEntranceTrans.x;
		rpgStats.vEntrancePoint.y = vEntranceTrans.y;
	}
	for ( int i = 0; i < int( rpgStats.peoplePointIndices.size() ); ++i )
	{
		const CVec3 v3 = modelMatrix[rpgStats.peoplePointIndices[i]].GetTrans3();
		rpgStats.vPeoplePoints.push_back( CVec2( v3.x, v3.y ) );
	}
	if ( nMyAmmoIndex != -1 )
	{
		const CVec3 v3 = modelMatrix[nMyAmmoIndex].GetTrans3();
		rpgStats.vAmmoPoint.x = v3.x;
		rpgStats.vAmmoPoint.y = v3.y;
	}

	rpgStats.vGunners.resize( 3 );
	for ( int k = 0; k < 3; k++ )
	{
		const SModel *pModel = k == 0 ? &combat : ( k == 1 ? ( bInstall ? &install : nullptr ) : ( bTrans ? &trans : nullptr ) );
		if ( pModel == nullptr )
			continue;
		const SHMatrix *pLocalMatrix = pModel->matrices.data();
		std::vector<SMyGunner> gunners;
		for ( int i = 0; i < pModel->NumNodes(); i++ )
		{
			const std::string &szNode = pModel->names[i];
			if ( StartsWith( szNode, "LGunner" ) )
			{
				SMyGunner oneGunner;
				oneGunner.szName = szNode;
				oneGunner.nIndex = i;
				gunners.push_back( oneGunner );
				continue;
			}
			if ( StartsWith( szNode, "LTowingPoint" ) )
			{
				rpgStats.nTowPoint = i;
				const CVec3 vTowTrans = pLocalMatrix[rpgStats.nTowPoint].GetTrans3();
				rpgStats.vTowPoint.x = vTowTrans.x;
				rpgStats.vTowPoint.y = vTowTrans.y;
				continue;
			}
			if ( StartsWith( szNode, "LFrontWheel" ) )
			{
				const CVec3 vWheelTrans = pLocalMatrix[i].GetTrans3();
				rpgStats.vFrontWheel.x = vWheelTrans.x;
				rpgStats.vFrontWheel.y = vWheelTrans.y;
				continue;
			}
			if ( StartsWith( szNode, "LBackWheel" ) )
			{
				const CVec3 vWheelTrans = pLocalMatrix[i].GetTrans3();
				rpgStats.vBackWheel.x = vWheelTrans.x;
				rpgStats.vBackWheel.y = vWheelTrans.y;
				continue;
			}
			if ( StartsWith( szNode, "LHookPoint" ) )
			{
				const CVec3 vHookTrans = pLocalMatrix[i].GetTrans3();
				rpgStats.vHookPoint.x = vHookTrans.x;
				rpgStats.vHookPoint.y = vHookTrans.y;
				continue;
			}
			if ( StartsWith( szNode, "LFatalitySmoke" ) )
			{
				rpgStats.nFatalitySmokePoint = i;
				continue;
			}
			if ( StartsWith( szNode, "LSmoke" ) )
			{
				rpgStats.damagePoints.push_back( i );
				continue;
			}
		}

		std::sort( gunners.begin(), gunners.end() );
		for ( const SMyGunner &gunner : gunners )
		{
			const CVec3 v3 = pLocalMatrix[gunner.nIndex].GetTrans3();
			const CVec2 v2( v3.x, v3.y );
			// 7 == sizeof "LGunner"; the digit after it says the mode.
			const char cMode = gunner.szName.size() > 7 ? gunner.szName[7] : 0;
			if ( cMode == '0' )
				rpgStats.vGunners[0].push_back( v2 );
			else if ( cMode == '1' )
				rpgStats.vGunners[1].push_back( v2 );
			else if ( cMode == '2' )
				rpgStats.vGunners[2].push_back( v2 );
		}
	}

	// A platform with no vertical limit of its own takes the elevation of
	// its first ballistic gun's shoot point (IObjectsDB tells what a weapon
	// shoots; the bridge fills SExportContext::isBallisticWeapon from it).
	for ( SMechUnitRPGStats::SPlatform &platform : rpgStats.platforms )
	{
		if ( platform.nModelPart == -1 )
			continue;
		if ( platform.constraintVertical.fMax != 0 )
			continue;

		bool bBalisticType = false;
		int nShootPointIndex = 0;
		for ( int g = platform.nFirstGun; g < platform.nFirstGun + platform.nNumGuns; g++ )
		{
			const SMechUnitRPGStats::SGun &gun = rpgStats.guns[g];
			bool bBallistic = false;
			if ( !context.isBallisticWeapon || !context.isBallisticWeapon( gun.szWeapon, bBallistic ) )
				outcome.warnings.push_back( "weapon \"" + gun.szWeapon + "\" is not known to the objects database; its elevation is not taken from the shoot point" );
			if ( bBallistic )
			{
				bBalisticType = true;
				nShootPointIndex = gun.nShootPoint;
				break;
			}
		}

		if ( bBalisticType && nShootPointIndex >= 0 )
		{
			CVec3 v;
			modelMatrix[nShootPointIndex].RotateVector( &v, CVec3( 0, 0, 1.0f ) );
			const double d = sqrt( v.x * v.x + v.y * v.y );
			const double alpha = atan2( fabs( v.z ), d );
			platform.constraintVertical.fMin = alpha;
			platform.constraintVertical.fMax = alpha;
		}
	}
	return true;
}

// The warning for a picture or model the export could not take: outcome's
// error moved to the warnings, as MFC's message box did not stop the export.
void WarnLastError( SExportOutcome &outcome, const std::string &szPrefix )
{
	outcome.warnings.push_back( szPrefix + outcome.szError );
	outcome.szError.clear();
}

// The alive and dead textures of the Graphics Info item and the file each is
// written as, in MFC's order.
struct STexture
{
	int nValue;
	const char *pszName;
	const char *pszWhat;
};
const STexture kTextures[] =
{
	{ 3, "1", "alive summer texture" }, { 4, "1w", "alive winter texture" }, { 5, "1a", "alive africa texture" },
	{ 6, "2", "dead summer texture" }, { 7, "2w", "dead winter texture" }, { 8, "2a", "dead africa texture" },
};

// The pictures of the project folder MFC converted without asking whether
// they exist: an absent one was no error.
const char *const kOptionalPictures[][2] =
{
	{ "icon512", "icon512.tga" }, { "1p", "1p.tga" }, { "1pw", "1pw.tga" }, { "1pa", "1pa.tga" },
};

bool IsFile( const fs::path &file )
{
	std::error_code ec;
	return fs::is_regular_file( file, ec );
}

// ConvertAndSaveImage( source, dest ) with a warning for what MFC's version
// returned false for.
void ConvertPicture( const SExportContext &context, const fs::path &source, const std::string &szName, const std::string &szWhat,
                     const NImageExport::SGamma &gamma, SExportOutcome &outcome )
{
	if ( !NImageExport::ConvertAndSaveImage( context, source.string(), szName, gamma, outcome ) )
		WarnLastError( outcome, szWhat + ": " );
}

// The two icons of ExportFrameData from <project folder>\icon.tga: icon.tga
// at 64 x 64 on grey, and icon at 128 x 128 holding the 90 x 90 picture.
void ExportIcons( const SExportContext &context, const std::string &szResultDir, const NImageExport::SGamma &gamma, SExportOutcome &outcome )
{
	const fs::path source = FoldedFile( ProjectDirectory( context ) + "icon.tga" );
	if ( !IsFile( source ) )
		return;
	CPtr<IImage> pImage = NImageExport::LoadPicture( source.string(), outcome );
	if ( pImage == 0 )
	{
		WarnLastError( outcome, "icon: " );
		return;
	}
	IImageProcessor *pIP = GetImageProcessor();
	{
		CPtr<IImage> pSmallImage = pIP->CreateScaleBySize( pImage, 64, 64, ISM_LANCZOS3 );
		CPtr<IImage> p64Image = pIP->CreateImage( 64, 64 );
		SColor col;
		col.r = col.g = col.b = 146;
		col.a = 0;
		p64Image->Set( col );
		RECT rc = { 0, 0, 64, 64 };
		p64Image->CopyFromAB( pSmallImage, &rc, 0, 0 );
		if ( !NImageExport::SaveTga( context, p64Image, szResultDir + "icon.tga", outcome ) )
			WarnLastError( outcome, "icon.tga: " );
	}
	{
		if ( pImage->GetSizeX() != 90 || pImage->GetSizeY() != 90 )
			pImage = pIP->CreateScaleBySize( pImage, 90, 90, ISM_LANCZOS3 );
		CPtr<IImage> p128Image = pIP->CreateImage( 128, 128 );
		p128Image->Set( SColor( 0 ) );
		RECT rc = { 0, 0, 90, 90 };
		p128Image->CopyFrom( pImage, &rc, 0, 0 );
		if ( !NImageExport::SaveCompressedTexture( context, p128Image, szResultDir + "icon", gamma, outcome ) )
			WarnLastError( outcome, "icon: " );
	}
}

// MyCopyFile of one localisation source beside the exported unit; a source
// that is not there is a warning.
void CopyLocalization( const SExportContext &context, const std::string &szSource, const std::string &szName, SExportOutcome &outcome )
{
	if ( !NImageExport::CopyFileInto( context, ModelFile( context, szSource ).string(), szName, outcome ) )
		WarnLastError( outcome, "" );
}

// The *.mod files of one folder, as NFile::EnumerateFiles found them, by name.
std::vector<fs::path> ModFiles( const fs::path &dir )
{
	std::vector<fs::path> files;
	std::error_code ec;
	for ( fs::directory_iterator it( dir, ec ), end; !ec && it != end; it.increment( ec ) )
	{
		std::string szExtension = it->path().extension().string();
		std::transform( szExtension.begin(), szExtension.end(), szExtension.begin(), []( unsigned char c ) { return char( std::tolower( c ) ); } );
		if ( szExtension == ".mod" && it->is_regular_file( ec ) )
			files.push_back( it->path() );
	}
	std::sort( files.begin(), files.end() );
	return files;
}

// FindMaximalSourceTime: the project and everything the export is made from.
fs::file_time_type MaximalSourceTime( const SExportContext &context, const CTreeItem &graphics )
{
	fs::file_time_type newest = ChangeTime( fs::path( context.szProjectPath ) );
	for ( int nValue : { 0, 1, 2, 3, 4, 5, 6, 7, 8 } )
		if ( !ValueStr( graphics, nValue ).empty() )
			newest = std::max( newest, ChangeTime( ModelFile( context, ValueStr( graphics, nValue ) ) ) );
	newest = std::max( newest, ChangeTime( FoldedFile( ProjectDirectory( context ) + "icon.tga" ) ) );
	return newest;
}

// FindMinimalExportFileTime: the oldest file this export writes. A texture
// the project does not name, a model it does not have and an icon it does not
// ship are not written, so they do not hold the export back.
fs::file_time_type MinimalExportTime( const SExportContext &context, const CTreeItem &graphics, const std::string &szResultDir )
{
	const fs::path exported = FoldedFile( ( fs::path( context.szDataRoot ) / ToSlashes( szResultDir ) ).string() );
	fs::file_time_type oldest = ChangeTime( FoldedChild( exported, "1.xml" ) );
	for ( int nModel = 0; nModel < 3; nModel++ )
		if ( nModel == 0 || IsFile( ModelFile( context, ValueStr( graphics, nModel ) ) ) )
			oldest = std::min( oldest, ChangeTime( FoldedChild( exported, std::to_string( nModel + 1 ) + ".mod" ) ) );
	for ( const STexture &texture : kTextures )
		if ( !ValueStr( graphics, texture.nValue ).empty() )
			for ( const char *pszSuffix : { "_c.dds", "_l.dds", "_h.dds" } )
				oldest = std::min( oldest, ChangeTime( FoldedChild( exported, std::string( texture.pszName ) + pszSuffix ) ) );
	if ( IsFile( FoldedFile( ProjectDirectory( context ) + "icon.tga" ) ) )
		oldest = std::min( oldest, ChangeTime( FoldedChild( exported, "icon.tga" ) ) );
	return oldest;
}

// ExportFrameData after the stats, up to the localisation texts.
void ExportGraphics( const SExportContext &context, const CTreeItem &graphics, const std::string &szResultDir, SExportOutcome &outcome )
{
	const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( ProjectDirectory( context ) );

	const fs::path combatFile = ModelFile( context, ValueStr( graphics, 0 ) );
	for ( const fs::path &mod : ModFiles( combatFile.parent_path() ) )
		if ( !NImageExport::CopyFileInto( context, mod.string(), szResultDir + mod.filename().string(), outcome ) )
			WarnLastError( outcome, "model: " );
	// The install and transportable models are copied only when they sit in
	// the combat model's folder; one that sits elsewhere is not exported.
	for ( int nModel = 1; nModel < 3; nModel++ )
	{
		const std::string szRel = ValueStr( graphics, nModel );
		if ( szRel.empty() )
			continue;
		const fs::path file = ModelFile( context, szRel );
		if ( !IsFile( file ) )
			outcome.warnings.push_back( std::string( nModel == 1 ? "install" : "transportable" ) + " model " + file.string() + " is not there" );
		else if ( file.parent_path() != combatFile.parent_path() )
			outcome.warnings.push_back( std::string( nModel == 1 ? "install" : "transportable" ) + " model " + file.string() + " is not in the combat model's folder and is not exported" );
	}

	for ( const STexture &texture : kTextures )
	{
		const std::string szRel = ValueStr( graphics, texture.nValue );
		if ( !szRel.empty() )
			ConvertPicture( context, ModelFile( context, szRel ), szResultDir + texture.pszName, std::string( texture.pszWhat ) + " " + szRel, gamma, outcome );
	}

	ExportIcons( context, szResultDir, gamma, outcome );
	for ( const auto &picture : kOptionalPictures )
	{
		const fs::path source = FoldedFile( ProjectDirectory( context ) + picture[1] );
		if ( IsFile( source ) )
			ConvertPicture( context, source, szResultDir + picture[0], picture[1], gamma, outcome );
	}
}

// The localisation texts, copied by a stats-only export too: they are not
// graphics.
void CopyLocalizations( const SExportContext &context, const CTreeItem &localization, const std::string &szResultDir, SExportOutcome &outcome )
{
	CopyLocalization( context, ValueStr( localization, 0 ), szResultDir + "name.txt", outcome );
	CopyLocalization( context, ValueStr( localization, 1 ), szResultDir + "desc.txt", outcome );
	CopyLocalization( context, ValueStr( localization, 2 ), szResultDir + "stats.txt", outcome );
}

}

bool ExportMesh( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_MESH_ROOT_ITEM, "mesh", outcome );
	if ( !pProject )
		return false;
	SMechUnitRPGStats rpgStats;
	if ( !FillRPGStats( rpgStats, *pProject->root, context, outcome ) )
		return false;

	const CTreeItem *pGraphics = ChildItem( *pProject->root, ETIT_MESH_GRAPHICS_ITEM );
	const CTreeItem *pLocalization = RequireChild( *pProject->root, ETIT_LOCALIZATION_ITEM, 0, "Localization", outcome );
	if ( pGraphics == nullptr || pLocalization == nullptr )
		return false;

	const std::string szFile = StatsFileName( project, context, kMeshAddDir, false );
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
		tree.Add( "RPG", &rpgStats );
	}, outcome ) )
		return false;

	if ( !context.bStatsOnly )
		ExportGraphics( context, *pGraphics, szResultDir, outcome );
	CopyLocalizations( context, *pLocalization, szResultDir, outcome );
	return true;
}


bool RebuildMeshLocators( CTreeItem &root, const SExportContext &context, int &nNodes, std::string &szModFile, std::string &szMessage )
{
	nNodes = 0;
	CTreeItem *pLocators = nullptr;
	for ( const auto &pChild : root.GetChildren() )
		if ( pChild->GetItemType() == ETIT_MESH_LOCATORS_ITEM )
			pLocators = pChild.get();
	CTreeItem *pGraphics = nullptr;
	for ( const auto &pChild : root.GetChildren() )
		if ( pChild->GetItemType() == ETIT_MESH_GRAPHICS_ITEM )
			pGraphics = pChild.get();
	if ( pLocators == nullptr || pGraphics == nullptr )
	{
		szMessage = "the unit project has no Graphics Info or Locators item";
		return false;
	}
	pLocators->MutableChildren().clear();

	const std::string szCombat = ValueStr( *pGraphics, 0 );
	const fs::path combatFile = ModelFile( context, szCombat );
	szModFile = combatFile.string();
	CPtr<IDataStream> pStream = szCombat.empty() ? CPtr<IDataStream>( 0 ) : OpenModel( combatFile );
	if ( pStream == 0 )
	{
		szMessage = "cannot load the combat model " + szModFile + ", so there are no locators";
		NStr::DebugTrace( "msh locators: no combat model %s", szModFile.c_str() );
		return false;
	}
	SModel model;
	CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
	CSaverAccessor saver = pSaver;
	saver.Add( 1, &model.skeleton );
	BuildModel( model );
	for ( int i = 0; i < model.NumNodes(); ++i )
	{
		auto pLocator = std::make_unique<CMeshLocatorPropsItem>();
		pLocator->nLocatorID = i;
		pLocator->bLocator = std::find( model.skeleton.locators.begin(), model.skeleton.locators.end(), model.skeleton.nodes[i].nIndex ) != model.skeleton.locators.end();
		pLocator->SetItemName( model.names[i] );
		pLocators->AddChild( std::move( pLocator ) );
	}
	nNodes = model.NumNodes();
	NStr::DebugTrace( "msh locators: %d nodes from %s", nNodes, szModFile.c_str() );
	return true;
}

}
