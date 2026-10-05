// The weapon exporter: CWeaponFrame::ExportFrameData, which only calls
// SaveRPGStats (Sources/src/editor/WeaponFrm.cpp:64-171). Weapons have no
// graphics, so a stats-only export writes the same file.
//
// MFC's SaveRPGStats wrote the struct's defaults instead (GetRPGStats) while
// bNewProjectJustCreated was set, i.e. for a project created and exported
// before it was ever saved. The bridge refuses to export an unsaved project,
// so the port always fills the struct from the tree.
#include "StdAfx.h"

#include "../stats_export.h"
#include "../tree_item_types.h"
#include "../../../Main/RPGStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

// CWeaponDamagePropsItem::GetTrajectoryType: MFC asserted on an unknown
// name and used a line trajectory.
SWeaponRPGStats::SShell::ETrajectoryType TrajectoryType( const CTreeItem &damage, SExportOutcome &outcome )
{
	const std::string szVal = ValueStr( damage, 0 );
	if ( szVal == "line" || szVal == "Trajectory line" )
		return SWeaponRPGStats::SShell::TRAJECTORY_LINE;
	else if ( szVal == "howitzer" || szVal == "Trajectory howitzer" )
		return SWeaponRPGStats::SShell::TRAJECTORY_HOWITZER;
	else if ( szVal == "bomb" || szVal == "Trajectory bomb" )
		return SWeaponRPGStats::SShell::TRAJECTORY_BOMB;
	else if ( szVal == "cannon" || szVal == "Trajectory cannon" )
		return SWeaponRPGStats::SShell::TRAJECTORY_CANNON;
	else if ( szVal == "rocket" || szVal == "Trajectory rocket" )
		return SWeaponRPGStats::SShell::TRAJECTORY_ROCKET;
	else if ( szVal == "grenade" || szVal == "Trajectory grenade" )
		return SWeaponRPGStats::SShell::TRAJECTORY_GRENADE;
	outcome.warnings.push_back( "Unknown trajectory type \"" + szVal + "\" in " + damage.GetDisplayName() + ": exported as line" );
	return SWeaponRPGStats::SShell::TRAJECTORY_LINE;
}

// CWeaponDamagePropsItem::GetDamageType.
SWeaponRPGStats::SShell::EDamageType DamageType( const CTreeItem &damage, SExportOutcome &outcome )
{
	const std::string szVal = ValueStr( damage, 12 );
	if ( szVal == "damage" )
		return SWeaponRPGStats::SShell::DAMAGE_HEALTH;
	else if ( szVal == "morale" )
		return SWeaponRPGStats::SShell::DAMAGE_MORALE;
	else if ( szVal == "smoke" )
		return SWeaponRPGStats::SShell::DAMAGE_FOG;
	outcome.warnings.push_back( "Unknown damage type \"" + szVal + "\" in " + damage.GetDisplayName() + ": exported as damage" );
	return SWeaponRPGStats::SShell::DAMAGE_HEALTH;
}

// CWeaponFrame::FillRPGStats.
bool FillRPGStats( SWeaponRPGStats &rpgStats, const CTreeItem &rootItem, SExportOutcome &outcome )
{
	const CTreeItem *pCommonProps = RequireChild( rootItem, ETIT_WEAPON_COMMON_PROPS_ITEM, 0, "Name", outcome );
	if ( pCommonProps == nullptr )
		return false;
	rpgStats.szKeyName = ValueStr( *pCommonProps, 0 );
	rpgStats.wDeltaAngle = WORD( ValueInt( *pCommonProps, 1 ) );
	rpgStats.nAmmoPerBurst = ValueInt( *pCommonProps, 2 );
	rpgStats.fDispersion = ValueFloat( *pCommonProps, 4 );
	rpgStats.fRangeMin = ValueFloat( *pCommonProps, 5 );
	rpgStats.fRangeMax = ValueFloat( *pCommonProps, 6 );
	rpgStats.nCeiling = ValueInt( *pCommonProps, 7 );
	rpgStats.fAimingTime = ValueFloat( *pCommonProps, 3 );
	rpgStats.fRevealRadius = ValueFloat( *pCommonProps, 8 );

	const CTreeItem *pShootTypesItem = RequireChild( rootItem, ETIT_WEAPON_SHOOT_TYPES_ITEM, 0, "Shoot types", outcome );
	if ( pShootTypesItem == nullptr )
		return false;
	bool bFirst = true;
	for ( const auto &pChild : pShootTypesItem->GetChildren() )
	{
		SWeaponRPGStats::SShell damage;
		const CTreeItem &damageItem = *pChild;

		damage.trajectory = TrajectoryType( damageItem, outcome );
		damage.nPiercing = ValueInt( damageItem, 1 );
		damage.nPiercingRandom = ValueInt( damageItem, 2 );
		damage.fDamagePower = float( ValueInt( damageItem, 3 ) );
		damage.nDamageRandom = ValueInt( damageItem, 4 );
		damage.fArea = ValueFloat( damageItem, 5 );
		damage.fArea2 = ValueFloat( damageItem, 6 );
		damage.fSpeed = ValueFloat( damageItem, 7 );
		damage.fDetonationPower = ValueFloat( damageItem, 9 );
		damage.fFireRate = ValueFloat( damageItem, 10 );
		damage.fRelaxTime = ValueFloat( damageItem, 11 );
		damage.eDamageType = DamageType( damageItem, outcome );
		// GetTraceProbability: (float) values[13].value / 100.0f.
		damage.fTraceProbability = ValueFloat( damageItem, 13 ) / 100.0f;
		damage.fTraceSpeedCoeff = ValueFloat( damageItem, 14 );
		damage.fBrokeTrackProbability = ValueFloat( damageItem, 15 );

		const CTreeItem *pCratersItem = RequireChild( damageItem, ETIT_WEAPON_CRATERS_ITEM, 0, "Picture craters", outcome );
		if ( pCratersItem == nullptr )
			return false;
		for ( const auto &pCrater : pCratersItem->GetChildren() )
			damage.szCraters.push_back( ValueStr( *pCrater, 0 ) );

		if ( ValueBool( damageItem, 8 ) )
			damage.specials.SetData( 0 );
		else
			damage.specials.RemoveData( 0 );

		const CTreeItem *pEffects = RequireChild( damageItem, ETIT_WEAPON_EFFECTS_ITEM, 0, "Effects", outcome );
		if ( pEffects == nullptr )
			return false;
		damage.szFireSound = ValueStr( *pEffects, 0 );
		damage.szEffectGunFire = ValueStr( *pEffects, 1 );
		damage.szEffectTrajectory = ValueStr( *pEffects, 2 );
		damage.szEffectHitDirect = ValueStr( *pEffects, 3 );
		damage.szEffectHitMiss = ValueStr( *pEffects, 4 );
		damage.szEffectHitReflect = ValueStr( *pEffects, 5 );
		damage.szEffectHitGround = ValueStr( *pEffects, 6 );
		damage.szEffectHitWater = ValueStr( *pEffects, 7 );
		damage.szEffectHitAir = ValueStr( *pEffects, 8 );

		for ( int i = 0; i < 2; i++ )
		{
			SFlashEffect *pEffect = i == 0 ? &damage.flashFire : &damage.flashExplosion;
			const CTreeItem *pFlashProps = RequireChild( damageItem, ETIT_WEAPON_FLASH_PROPS_ITEM, i, i == 0 ? "Flash fire" : "Flash explosion", outcome );
			if ( pFlashProps == nullptr )
				return false;
			pEffect->nPower = ValueInt( *pFlashProps, 0 );
			pEffect->nDuration = ValueInt( *pFlashProps, 1 );
		}

		if ( bFirst )
			rpgStats.shells[0] = damage;
		else
			rpgStats.shells.push_back( damage );
		bFirst = false;
	}
	return true;
}

}

bool ExportWeapon( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_WEAPON_ROOT_ITEM, "weapon", outcome );
	if ( !pProject )
		return false;
	SWeaponRPGStats rpgStats;
	if ( !FillRPGStats( rpgStats, *pProject->root, outcome ) )
		return false;
	const std::string szFile = StatsFileName( project, context, "weapons\\", true );
	return WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
	}, outcome );
}

}
