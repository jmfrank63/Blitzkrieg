// The infantry exporter: CAnimationFrame::ExportFrameData (Sources/src/editor/
// AnimationFrm.cpp:570-600), FillRPGStats (282-388) and
// CAnimationTreeRootItem::ComposeAnimations (AnimTreeItem.cpp:649-856). The
// stats go to 1.xml as tree.Add( "RPG", &stats ); the frames of three seasons
// and two blood passes are packed into 1[b][w|a].san with a 1[b][w|a]_c/_l/_h
// .dds each; name.txt, desc.txt and stats.txt are copied beside them.
//
// What MFC reported through message boxes (the batch export switched them
// off) goes into the outcome as warnings, and as in MFC a failed compose
// does not fail the export: ExportFrameData ignored its result and went on
// to write the stats.
//
// The compose keeps MFC's frame bookkeeping as it is, quirks included. A
// frame file that is missing is announced and stood in for with
// editor\invalid.tga, but nCurrentFrame advances only for files that exist,
// so the next frame is written into the same slot and a missing frame shows
// the one after it. The slots past the last written one stay empty, and
// MFC's image loop stopped at the first empty name; the port cuts the list
// there for the same effect.
#include "StdAfx.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include "../stats_export.h"
#include "../../compose.h"
#include "../../image_export.h"
#include "../tree_item_types.h"
#include "../../../Main/RPGStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;
namespace fs = std::filesystem;

const char kInfantryAddDir[] = "units\\humans\\";

// A value as MFC's int64 conversion: the default of the actions and
// exposures is an empty string, which is 0.
long long ValueInt64( const CTreeItem &item, int nIndex )
{
	const CPropVector &values = item.GetValues();
	if ( nIndex < 0 || nIndex >= int( values.size() ) )
		return 0;
	const CVariant &value = values[nIndex].value;
	switch ( value.GetKind() )
	{
		case CVariant::VK_INT64: return value.AsInt64();
		case CVariant::VK_STR:   return std::strtoll( value.AsStr().c_str(), nullptr, 10 );
		default:                 return ValueInt( item, nIndex );
	}
}

// CUnitAnimationPropsItem::GetAnimationType: the animation slot of the stats
// for an item name, -1 for a name MFC asserted on.
int AnimationType( const std::string &szName )
{
	static const struct { const char *pszName; int nType; } kTypes[] =
	{
		{ "Run", ANIMATION_MOVE }, { "Crawl", ANIMATION_CRAWL }, { "Shoot", ANIMATION_SHOOT }, { "Shoot down", ANIMATION_SHOOT_DOWN },
		{ "Shoot trench", ANIMATION_SHOOT_TRENCH }, { "Aiming", ANIMATION_AIMING }, { "Aiming down", ANIMATION_AIMING_DOWN },
		{ "Aiming trench", ANIMATION_AIMING_TRENCH }, { "Throw", ANIMATION_THROW }, { "Throw trench", ANIMATION_THROW_TRENCH },
		{ "Death", ANIMATION_DEATH }, { "Death down", ANIMATION_DEATH_DOWN }, { "Idle", ANIMATION_IDLE }, { "Idle down", ANIMATION_IDLE_DOWN },
		{ "Use down", ANIMATION_USE_DOWN }, { "Use up", ANIMATION_USE }, { "Pointing", ANIMATION_POINTING }, { "Binoculars", ANIMATION_BINOCULARS },
		{ "Radio", ANIMATION_RADIO }, { "Lie to stand cross", ANIMATION_LIE }, { "Stand to lie cross", ANIMATION_STAND },
		{ "Throw down", ANIMATION_THROW_DOWN }, { "Idle2", ANIMATION_IDLE2 }, { "Prisoning", ANIMATION_PRISONING },
	};
	for ( const auto &type : kTypes )
		if ( szName == type.pszName )
			return type.nType;
	return -1;
}

// CUnitCommonPropsItem::GetUnitType.
int UnitType( const std::string &szVal )
{
	if ( szVal == "engineer" || szVal == "Engineer" )
		return RPG_TYPE_ENGINEER;
	if ( szVal == "sniper" || szVal == "Sniper" )
		return RPG_TYPE_SNIPER;
	if ( szVal == "officer" || szVal == "Officer" )
		return RPG_TYPE_OFFICER;
	return RPG_TYPE_SOLDIER;
}

// CUnitAnimationPropsItem's getters, by the position of their property in
// the item's default order (frame time, action frame, speed, cycled, number of
// directions).
int FrameTime( const CTreeItem &anim ) { return ValueInt( anim, 0 ); }
int ActionFrame( const CTreeItem &anim ) { return ValueInt( anim, 1 ); }
float AnimationSpeed( const CTreeItem &anim ) { return ValueFloat( anim, 2 ); }
bool CycledFlag( const CTreeItem &anim ) { return ValueBool( anim, 3 ); }
int NumberOfDirections( const CTreeItem &anim ) { return ValueStr( anim, 4 ) == "8" ? 8 : 4; }
int FrameCount( const CTreeItem &anim ) { return int( anim.GetChildren().size() ); }

// CAnimationFrame::FillRPGStats.
bool FillRPGStats( SInfantryRPGStats &rpgStats, const CTreeItem &rootItem, SExportOutcome &outcome )
{
	const CTreeItem *pCommonProps = RequireChild( rootItem, ETIT_UNIT_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	const CTreeItem *pAcks = RequireChild( rootItem, ETIT_UNIT_ACKS_ITEM, 0, "Acknowledgments", outcome );
	const CTreeItem *pActions = RequireChild( rootItem, ETIT_UNIT_ACTIONS_ITEM, 0, "Actions", outcome );
	const CTreeItem *pExposures = RequireChild( rootItem, ETIT_UNIT_EXPOSURES_ITEM, 0, "Exposures", outcome );
	const CTreeItem *pWeaponProps = RequireChild( rootItem, ETIT_UNIT_WEAPON_PROPS_ITEM, 0, "Weapon", outcome );
	const CTreeItem *pGrenadeProps = RequireChild( rootItem, ETIT_UNIT_GRENADE_PROPS_ITEM, 0, "Grenade", outcome );
	const CTreeItem *pAnimsItem = RequireChild( rootItem, ETIT_UNIT_ANIMATIONS_ITEM, 0, "Animations", outcome );
	if ( !pCommonProps || !pAcks || !pActions || !pExposures || !pWeaponProps || !pGrenadeProps || !pAnimsItem )
		return false;

	rpgStats.szKeyName = ValueStr( *pCommonProps, 0 );
	rpgStats.type = (EUnitRPGType) UnitType( ValueStr( *pCommonProps, 1 ) );
	rpgStats.fMaxHP = ValueFloat( *pCommonProps, 3 );
	rpgStats.nMinArmor = rpgStats.nMaxArmor = ValueInt( *pCommonProps, 4 );
	rpgStats.fSight = ValueFloat( *pCommonProps, 11 );
	rpgStats.fCamouflage = ValueFloat( *pCommonProps, 5 );
	rpgStats.fSpeed = ValueFloat( *pCommonProps, 6 );
	rpgStats.fPassability = ValueFloat( *pCommonProps, 7 );
	rpgStats.bCanAttackUp = ValueBool( *pCommonProps, 8 );
	rpgStats.bCanAttackDown = ValueBool( *pCommonProps, 9 );
	rpgStats.fPrice = ValueFloat( *pCommonProps, 10 );
	rpgStats.fSightPower = ValueFloat( *pCommonProps, 12 );

	rpgStats.szAcksNames.resize( 2 );
	rpgStats.szAcksNames[0] = ValueStr( *pAcks, 0 );
	rpgStats.szAcksNames[1] = ValueStr( *pAcks, 1 );

	// CUnitActionsItem::GetActions, CUnitExposuresItem::GetExposures.
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

	rpgStats.fRotateSpeed = 0.0f;
	rpgStats.nPriority = 0;
	rpgStats.nUninstallRotate = 0;
	rpgStats.nUninstallTransport = 0;

	rpgStats.guns.resize( 2 );
	if ( ValueStr( *pWeaponProps, 0 ).empty() )
		rpgStats.guns[0].szWeapon = "generic";
	else
		rpgStats.guns[0].szWeapon = ValueStr( *pWeaponProps, 0 );
	rpgStats.guns[0].nAmmo = ValueInt( *pWeaponProps, 1 );
	rpgStats.guns[0].fReloadCost = ValueFloat( *pWeaponProps, 2 );

	const std::string szGrenade = ValueStr( *pGrenadeProps, 0 );
	if ( szGrenade == "generic" )
		rpgStats.guns.resize( 1 );
	else if ( !szGrenade.empty() && szGrenade != "_" )
	{
		rpgStats.guns[1].szWeapon = szGrenade;
		rpgStats.guns[1].nAmmo = ValueInt( *pGrenadeProps, 1 );
		rpgStats.guns[1].fReloadCost = ValueFloat( *pGrenadeProps, 2 );
	}
	else
		rpgStats.guns.resize( 1 );

	// The //CRAP +1 of MFC: one slot more than there are animations. An
	// animation whose slot lies past that was a write past the vector in MFC;
	// the port leaves it out and says so.
	const auto &anims = pAnimsItem->GetChildren();
	rpgStats.animtimes.resize( anims.size() + 1 );
	int nFind = 0;
	for ( const auto &pAnim : anims )
	{
		const int nAction = NCompose::GetActionFromName( pAnim->GetDisplayName() );
		if ( nAction >= 0 && nAction < int( rpgStats.animtimes.size() ) )
			rpgStats.animtimes[nAction] = FrameTime( *pAnim ) * FrameCount( *pAnim );
		else
			outcome.warnings.push_back( "animation \"" + pAnim->GetDisplayName() + "\" has no slot among the " + std::to_string( rpgStats.animtimes.size() ) + " animation times" );

		const std::string &szAnimName = pAnim->GetDisplayName();
		if ( szAnimName == "Run" )
		{
			rpgStats.fRunSpeed = AnimationSpeed( *pAnim );
			if ( nFind == 1 )
				break;
			nFind++;
		}
		if ( szAnimName == "Crawl" )
		{
			rpgStats.fCrawlSpeed = AnimationSpeed( *pAnim );
			if ( nFind == 1 )
				break;
			nFind++;
		}
	}

	int nIndex = 0;
	rpgStats.animdescs.resize( ANIMATION_LAST_ANIMATION );
	for ( const auto &pAnim : anims )
	{
		SUnitBaseRPGStats::SAnimDesc desc;
		desc.nIndex = nIndex;
		desc.nAction = ActionFrame( *pAnim ) * FrameTime( *pAnim );
		desc.nLength = FrameCount( *pAnim ) * FrameTime( *pAnim );
		desc.nAABB_A = -1;
		desc.nAABB_D = -1;
		const int nType = AnimationType( pAnim->GetDisplayName() );
		if ( nType >= 0 && nType < int( rpgStats.animdescs.size() ) )
			rpgStats.animdescs[nType].push_back( desc );
		else
			outcome.warnings.push_back( "Unknown animation \"" + pAnim->GetDisplayName() + "\": it has no animation description" );
		nIndex++;
	}
	return true;
}

// The frame folder of one direction of a season with a backslash at the end
// as the project stores it, resolved against the project folder when it is
// relative.
std::string DirectoryName( const CTreeItem &season, int nIndex, const SExportContext &context )
{
	const CTreeItem *pDir = ChildItem( season, ETIT_UNIT_DIRECTORY_PROPS_ITEM, nIndex );
	std::string szDirName = pDir != nullptr ? ValueStr( *pDir, 0 ) : std::string();
	std::replace( szDirName.begin(), szDirName.end(), '/', '\\' );
	if ( IsRelatedPath( szDirName ) )
	{
		std::string szProjectDir = ProjectDirectory( context );
		std::replace( szProjectDir.begin(), szProjectDir.end(), '/', '\\' );
		szDirName = MakeFullPath( szProjectDir, szDirName );
	}
	return szDirName;
}

// The folder name MFC took the directory from: GetDirName( i * 2 ) for a 4
// direction animation.
std::string ShortDirectoryName( const CTreeItem &season, int nDirs, int i )
{
	const CTreeItem *pDir = ChildItem( season, ETIT_UNIT_DIRECTORY_PROPS_ITEM, nDirs == 4 ? i * 2 : i );
	std::string szName = pDir != nullptr ? ValueStr( *pDir, 0 ) : std::string();
	std::replace( szName.begin(), szName.end(), '/', '\\' );
	return szName;
}

bool IsReadable( const std::string &szFile )
{
	std::error_code ec;
	return fs::is_regular_file( FoldedFile( szFile ), ec );
}

// FindMaximalSourceTime of the animation tree: the newest frame that exists.
fs::file_time_type MaximalFrameTime( const CTreeItem &dirsItem, const CTreeItem &animsItem, const SExportContext &context )
{
	fs::file_time_type newest = fs::file_time_type::min();
	for ( const auto &pSeason : dirsItem.GetChildren() )
		for ( const auto &pAnim : animsItem.GetChildren() )
		{
			if ( pAnim->GetChildren().empty() )
				continue;
			const int nDirs = NumberOfDirections( *pAnim );
			for ( int i = 0; i < nDirs; i++ )
			{
				const std::string szDirName = DirectoryName( *pSeason, nDirs == 4 ? i * 2 : i, context );
				for ( const auto &pFrame : pAnim->GetChildren() )
				{
					const fs::path file = FoldedFile( szDirName + pFrame->GetDisplayName() + ".tga" );
					std::error_code ec;
					if ( fs::is_regular_file( file, ec ) )
						newest = std::max( newest, ChangeTime( file ) );
				}
			}
		}
	return newest;
}

// GetTextureFileChangeTime: the oldest of the three .dds of a picture.
fs::file_time_type TextureChangeTime( const fs::path &dir, const std::string &szName )
{
	fs::file_time_type oldest = fs::file_time_type::max();
	for ( const char *pszSuffix : { "_c.dds", "_l.dds", "_h.dds" } )
		oldest = std::min( oldest, ChangeTime( FoldedChild( dir, szName + pszSuffix ) ) );
	return oldest;
}

// ComposeAnimations( project, resultDir, false, false ). False when nothing
// was composed; the reason is in outcome.warnings.
bool ComposeAnimations( const CTreeItem &root, const SExportContext &context, const std::string &szResultDir, SExportOutcome &outcome )
{
	if ( root.GetChildren().empty() )
		return false;
	const CTreeItem *pDirsItem = ChildItem( root, ETIT_UNIT_DIRECTORIES_ITEM );
	const CTreeItem *pAnimsItem = ChildItem( root, ETIT_UNIT_ANIMATIONS_ITEM );
	if ( pDirsItem == nullptr || pAnimsItem == nullptr )
	{
		outcome.warnings.push_back( "Error: no valid animations" );
		return false;
	}

	std::error_code ec;
	const bool bBloodDirExist = fs::is_directory( FoldedFile( ProjectDirectory( context ) + "blood" ), ec );
	const fs::path invalid = InvalidPicture( context );
	const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( ProjectDirectory( context ) );

	for ( int nBlood = 0; nBlood < 2; nBlood++ )
	{
		CVec2 vCommonFrameSize( 32, 32 );
		bool bCommonSizeComputed = false;
		int nSeason = 0;
		for ( const auto &pSeason : pDirsItem->GetChildren() )
		{
			int nCurrentFrame = 0;
			int nCurrentAnim = 0;
			int nCurrentSpriteNumber = 0;
			std::vector<std::string> fileNameVector;
			std::vector<std::string> invalidNameVector;
			std::vector<NCompose::SAnimationDesc> animDescVector( pAnimsItem->GetChildren().size() );

			for ( const auto &pAnim : pAnimsItem->GetChildren() )
			{
				const int nDirsCount = NumberOfDirections( *pAnim );
				if ( pAnim->GetChildren().empty() )
					continue;

				NCompose::SAnimationDesc &animDesc = animDescVector[nCurrentAnim];
				animDesc.bCycled = CycledFlag( *pAnim );
				animDesc.fSpeed = AnimationSpeed( *pAnim );
				const int nLastSprite = nCurrentSpriteNumber + nDirsCount * FrameCount( *pAnim );
				animDesc.nFrameTime = FrameTime( *pAnim );
				animDesc.ptFrameShift = CVec2( 0, 0 );
				animDesc.szName = pAnim->GetDisplayName();

				fileNameVector.resize( nLastSprite );
				animDesc.dirs.resize( nDirsCount );
				for ( int i = 0; i < nDirsCount; i++ )
				{
					const std::string szShortDirName = ShortDirectoryName( *pSeason, nDirsCount, i );
					const std::string szDirName = DirectoryName( *pSeason, nDirsCount == 4 ? i * 2 : i, context );

					NCompose::SAnimationDesc::SDirDesc &dirDesc = animDesc.dirs[i];
					dirDesc.ptFrameShift = CVec2( 0, 0 );
					dirDesc.frames.resize( FrameCount( *pAnim ) );

					if ( !bCommonSizeComputed )
					{
						const std::string szTempFileName = szDirName + pAnim->GetChildren().front()->GetDisplayName() + ".tga";
						if ( IsReadable( szTempFileName ) )
						{
							SExportOutcome probe;
							CPtr<IImage> pImage = NImageExport::LoadPicture( FoldedFile( szTempFileName ).string(), probe );
							if ( pImage != 0 )
							{
								vCommonFrameSize.x = float( pImage->GetSizeX() / 2 );
								vCommonFrameSize.y = float( pImage->GetSizeY() / 2 );
								bCommonSizeComputed = true;
							}
						}
					}

					int k = 0;
					for ( const auto &pFrame : pAnim->GetChildren() )
					{
						dirDesc.frames[k] = short( nCurrentFrame );
						animDesc.frames[nCurrentFrame] = vCommonFrameSize;

						std::string szTempFileName = szDirName;
						const std::string &szName = pAnim->GetDisplayName();
						if ( bBloodDirExist && nBlood == 1 && ( szName == "Death" || szName == "Death down" ) )
						{
							szTempFileName = szTempFileName.substr( 0, szTempFileName.size() - std::min( szTempFileName.size(), szShortDirName.size() ) );
							szTempFileName += "blood\\";
							szTempFileName += szShortDirName;
						}
						szTempFileName += pFrame->GetDisplayName();
						szTempFileName += ".tga";

						if ( IsReadable( szTempFileName ) )
						{
							fileNameVector[nCurrentFrame] = FoldedFile( szTempFileName ).string();
							nCurrentFrame++;
						}
						else
						{
							invalidNameVector.push_back( ToSlashes( szTempFileName ) );
							// The stand-in MFC named is read from the data folder; without
							// one the slot stays empty, where MFC's image loop stopped.
							fileNameVector[nCurrentFrame] = invalid.empty() ? std::string() : invalid.string();
						}
						k++;
					}
				}

				nCurrentSpriteNumber = nLastSprite;
				nCurrentAnim++;
			}
			animDescVector.resize( nCurrentAnim );

			if ( !invalidNameVector.empty() )
			{
				std::string szWarning = "Can not find files total count " + std::to_string( invalidNameVector.size() ) + ":";
				for ( const std::string &szName : invalidNameVector )
					szWarning += " " + szName;
				szWarning += invalid.empty() ? "; editor\\invalid.tga is not available here" : "; editor\\invalid.tga stands in";
				outcome.warnings.push_back( szWarning );
			}
			if ( nCurrentFrame == 0 )
			{
				outcome.warnings.push_back( "Error: no valid animations" );
				return false;
			}

			// MFC's loop over the names stopped at the first empty one.
			const auto firstEmpty = std::find( fileNameVector.begin(), fileNameVector.end(), std::string() );
			fileNameVector.erase( firstEmpty, fileNameVector.end() );

			SSpriteAnimationFormat spriteAnimFmt;
			CPtr<IImage> pImage = NCompose::BuildAnimations( &animDescVector, &spriteAnimFmt, fileNameVector, true, 0, outcome );
			if ( pImage == 0 )
			{
				outcome.warnings.push_back( outcome.szError.empty() ? std::string( "Composing images failed!" ) : "Composing images failed: " + outcome.szError );
				outcome.szError.clear();
				return false;
			}

			std::string szSuffix = nBlood ? "b" : "";
			if ( nSeason == 1 )
				szSuffix += "w";
			else if ( nSeason == 2 )
				szSuffix += "a";
			else if ( nSeason > 3 )
				outcome.warnings.push_back( "The number of seasons is above 3" );
			if ( !NImageExport::SaveAnimation( context, spriteAnimFmt, szResultDir + "1" + szSuffix + ".san", outcome ) ||
			     !NImageExport::SaveCompressedTexture( context, pImage, szResultDir + "1" + szSuffix, gamma, outcome ) )
			{
				outcome.warnings.push_back( "Composing images failed: " + outcome.szError );
				outcome.szError.clear();
				return false;
			}
			nSeason++;
		}
	}
	return true;
}

// MyCopyFile of one localisation source beside the exported unit; a source
// that is not there is a warning.
void CopyLocalization( const SExportContext &context, const std::string &szSource, const std::string &szName, SExportOutcome &outcome )
{
	std::string szSourcePath = szSource;
	std::replace( szSourcePath.begin(), szSourcePath.end(), '/', '\\' );
	if ( IsRelatedPath( szSourcePath ) )
	{
		std::string szProjectDir = ProjectDirectory( context );
		std::replace( szProjectDir.begin(), szProjectDir.end(), '/', '\\' );
		szSourcePath = MakeFullPath( szProjectDir, szSourcePath );
	}
	if ( !NImageExport::CopyFileInto( context, FoldedFile( szSourcePath ).string(), szName, outcome ) )
	{
		outcome.warnings.push_back( outcome.szError );
		outcome.szError.clear();
	}
}

}

bool ExportInfantry( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_ANIMATION_ROOT_ITEM, "infantry", outcome );
	if ( !pProject )
		return false;
	const CTreeItem &root = *pProject->root;
	SInfantryRPGStats rpgStats;
	if ( !FillRPGStats( rpgStats, root, outcome ) )
		return false;
	const CTreeItem *pLocItem = RequireChild( root, ETIT_LOCALIZATION_ITEM, 0, "Localization", outcome );
	if ( pLocItem == nullptr )
		return false;

	const std::string szFile = StatsFileName( project, context, kInfantryAddDir, false );
	const std::string szResultDir = DirectoryOf( szFile );
	// The sprite the game builds, "1" beside the .sans.
	outcome.szObjectName = szResultDir + "1";

	const CTreeItem *pDirsItem = ChildItem( root, ETIT_UNIT_DIRECTORIES_ITEM );
	const CTreeItem *pAnimsItem = ChildItem( root, ETIT_UNIT_ANIMATIONS_ITEM );
	// The up-to-date check of CParentFrame::ExportProject with the times of
	// CAnimationFrame: the project, the frames and name.txt against the oldest
	// of 1.san, the three textures, 1.xml and name.txt of the export.
	if ( !context.bForce && !context.bStatsOnly && !context.szDataRoot.empty() && pDirsItem != nullptr && pAnimsItem != nullptr )
	{
		fs::file_time_type sourceTime = std::max( ChangeTime( fs::path( context.szProjectPath ) ), MaximalFrameTime( *pDirsItem, *pAnimsItem, context ) );
		sourceTime = std::max( sourceTime, ChangeTime( FoldedFile( ProjectDirectory( context ) + "name.txt" ) ) );
		const fs::path exported = FoldedFile( ( fs::path( context.szDataRoot ) / ToSlashes( szResultDir ) ).string() );
		fs::file_time_type exportTime = ChangeTime( FoldedChild( exported, "1.san" ) );
		for ( const char *pszName : { "1", "1w", "1a" } )
			exportTime = std::min( exportTime, TextureChangeTime( exported, pszName ) );
		exportTime = std::min( exportTime, ChangeTime( FoldedChild( exported, "1.xml" ) ) );
		exportTime = std::min( exportTime, ChangeTime( FoldedChild( exported, "name.txt" ) ) );
		if ( exportTime >= sourceTime )
		{
			++outcome.nSkipped;
			return true;
		}
	}

	if ( !context.bStatsOnly && !ComposeAnimations( root, context, szResultDir, outcome ) )
		outcome.warnings.push_back( "the animations were not composed; the stats are written all the same" );

	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
	}, outcome ) )
		return false;

	CopyLocalization( context, ValueStr( *pLocItem, 0 ), szResultDir + "name.txt", outcome );
	CopyLocalization( context, ValueStr( *pLocItem, 1 ), szResultDir + "desc.txt", outcome );
	CopyLocalization( context, ValueStr( *pLocItem, 2 ), szResultDir + "stats.txt", outcome );
	return true;
}

}
