// The squad exporter: CSquadFrame::SaveRPGStats (Sources/src/editor/
// SquadFrm.cpp:216-285) for the stats, and ExportFrameData's copy of the squad
// picture beside the stats. The squad frame did not override the up-to-date
// check, so MFC exported every time and so does the port.
//
// Two things MFC took from the running editor:
//   - IObjectsDB, to turn a member given as a path ("USSR\Mosin") into the
//     key of the sprite unit at units\humans\<path>. The port asks
//     SExportContext::findUnitKey (D015).
//   - the scene's screen transform, to move the formation's zero point by the
//     half size of its cross icon on screen (zeroShiftX/Y) before the soldier
//     positions are made relative to it. The port computes the same shift
//     with the squad frame's camera; see ShiftedZero.
#include "StdAfx.h"

#include <cctype>
#include <cmath>

#include "../stats_export.h"
#include "../../image_export.h"
#include "../tree_item_types.h"
#include "squad.h"
#include "../../../Main/RPGStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

static const float zeroShiftX = 15.4f;
static const float zeroShiftY = 15.4f;

// CSquadCommonPropsItem::GetSquadType; MFC asserted and returned -1.
int SquadType( const CTreeItem &commonProps, SExportOutcome &outcome )
{
	const std::string szName = ValueStr( commonProps, 2 );
	if ( szName == "riflemans" || szName == "Riflemans" )
		return SSquadRPGStats::RIFLEMANS;
	if ( szName == "infantry" || szName == "Infantry" )
		return SSquadRPGStats::INFANTRY;
	if ( szName == "submachine gunners" || szName == "SubMachineGunners" )
		return SSquadRPGStats::SUBMACHINEGUNNERS;
	if ( szName == "machine gunners" || szName == "MachineGunners" )
		return SSquadRPGStats::MACHINEGUNNERS;
	if ( szName == "AT team" || szName == "At team" )
		return SSquadRPGStats::AT_TEAM;
	if ( szName == "mortar team" || szName == "Mortar team" )
		return SSquadRPGStats::MORTAR_TEAM;
	if ( szName == "snipers" || szName == "Snipers" )
		return SSquadRPGStats::SNIPERS;
	if ( szName == "gunners" || szName == "Gunners" )
		return SSquadRPGStats::GUNNERS;
	if ( szName == "engineers" || szName == "Engineers" )
		return SSquadRPGStats::ENGINEERS;
	outcome.warnings.push_back( "Unknown squad type \"" + szName + "\"" );
	return -1;
}

// CSquadFormationPropsItem::GetFormationType; MFC asserted and returned 0.
int FormationType( const CTreeItem &formProps, SExportOutcome &outcome )
{
	const std::string szVal = ValueStr( formProps, 0 );
	if ( szVal == "default" || szVal == "Default" )
		return SSquadRPGStats::SFormation::DEFAULT;
	if ( szVal == "movement" || szVal == "Movement" )
		return SSquadRPGStats::SFormation::MOVEMENT;
	if ( szVal == "defensive" || szVal == "Defensive" )
		return SSquadRPGStats::SFormation::DEFENSIVE;
	if ( szVal == "offensive" || szVal == "Offensive" )
		return SSquadRPGStats::SFormation::OFFENSIVE;
	if ( szVal == "sneak" || szVal == "Sneak" )
		return SSquadRPGStats::SFormation::SNEAK;
	outcome.warnings.push_back( "Unknown formation type \"" + szVal + "\" in " + formProps.GetDisplayName() );
	return 0;
}

// CSquadFormationPropsItem::GetLieState; MFC asserted and returned -1.
int LieState( const CTreeItem &formProps, SExportOutcome &outcome )
{
	const std::string szVal = ValueStr( formProps, 2 );
	if ( szVal == "standart" || szVal == "Standart" )
		return 0;
	if ( szVal == "always stand" || szVal == "Always stand" )
		return 1;
	if ( szVal == "always lie" || szVal == "Always lie" )
		return 2;
	outcome.warnings.push_back( "Unknown lie state \"" + szVal + "\" in " + formProps.GetDisplayName() );
	return -1;
}

// MakeName: a member given as a path is the unit stored at units\humans\
// <path>, named by its key; any other name is the key already. MFC asserted
// "Can't find stats" and wrote an empty name; the port fails the export.
bool MakeName( const std::string &szName, const SExportContext &context, std::string &szKey, SExportOutcome &outcome )
{
	if ( szName.find( '\\' ) == std::string::npos )
	{
		szKey = szName;
		return true;
	}
	std::string szNewName = "units\\humans\\" + szName;
	for ( char &c : szNewName )
		c = char( std::tolower( (unsigned char)c ) );
	if ( !context.findUnitKey )
	{
		outcome.szError = "squad member \"" + szName + "\" cannot be resolved: the export has no objects database to find " + szNewName + " in";
		return false;
	}
	if ( !context.findUnitKey( szNewName, szKey ) )
	{
		outcome.szError = "Can't find stats for \"" + szNewName + "\": squad member \"" + szName + "\" is not a unit of the objects database";
		return false;
	}
	return true;
}

// pSG->GetPos2( &vRealZero2, vZeroPos ); vRealZero2 += zeroShift;
// pSG->GetPos3( &vRealZero3, vRealZero2 ) in the squad frame's scene, which
// has no terrain, so GetPos3 takes its z=0 plane answer. The squad frame's
// camera is SetDefaultCamera's (pitch -(90+30) degrees, yaw 45 degrees) over
// an orthographic projection of one world unit per pixel. On the ground
// plane that maps a world point to screen x = (x + y) cos45 and screen y
// (downwards) = (x - y) cos45 sin30, so the camera's anchor and the window
// size only translate both, and the shift on screen is this one in the world.
CVec3 ShiftedZero( const Vec3 &vZeroPos )
{
	const float fCos45 = std::cos( ToRadian( 45.0f ) );
	const float fSin30 = std::sin( ToRadian( 30.0f ) );
	const float fSum = zeroShiftX / fCos45;                 // dx + dy
	const float fDiff = zeroShiftY / ( fCos45 * fSin30 );   // dx - dy
	return CVec3( vZeroPos.x + ( fSum + fDiff ) / 2, vZeroPos.y + ( fSum - fDiff ) / 2, 0 );
}

// CSquadFrame::SaveRPGStats up to tree.Add.
bool FillRPGStats( SSquadRPGStats &rpgStats, const CTreeItem &rootItem, const SExportContext &context, SExportOutcome &outcome )
{
	const CTreeItem *pCommonPropsItem = RequireChild( rootItem, ETIT_SQUAD_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	if ( pCommonPropsItem == nullptr )
		return false;
	rpgStats.szIcon = ValueStr( *pCommonPropsItem, 1 );
	rpgStats.type = ( SSquadRPGStats::ESquadType ) SquadType( *pCommonPropsItem, outcome );

	const CTreeItem *pMembersItem = RequireChild( rootItem, ETIT_SQUAD_MEMBERS_ITEM, 0, "Members", outcome );
	if ( pMembersItem == nullptr )
		return false;
	std::vector<const CTreeItem *> members;
	for ( const auto &pMember : pMembersItem->GetChildren() )
	{
		std::string szName;
		if ( !MakeName( pMember->GetDisplayName(), context, szName, outcome ) )
			return false;
		rpgStats.memberNames.push_back( szName );
		members.push_back( pMember.get() );
	}

	const CTreeItem *pFormations = RequireChild( rootItem, ETIT_SQUAD_FORMATIONS_ITEM, 0, "Formations", outcome );
	if ( pFormations == nullptr )
		return false;
	for ( const auto &pFormation : pFormations->GetChildren() )
	{
		const auto *pFormProps = dynamic_cast<const CSquadFormationPropsItem *>( pFormation.get() );
		if ( pFormProps == nullptr )
			continue;
		SSquadRPGStats::SFormation form;
		form.type = ( SSquadRPGStats::SFormation::EType ) FormationType( *pFormProps, outcome );
		form.changesByEvent.resize( 1 );
		form.changesByEvent[0] = ValueInt( *pFormProps, 1 );
		form.cLieFlag = BYTE( LieState( *pFormProps, outcome ) );
		form.fSpeedBonus = ValueFloat( *pFormProps, 3 );
		form.fDispersionBonus = ValueFloat( *pFormProps, 4 );
		form.fFireRateBonus = ValueFloat( *pFormProps, 5 );
		form.fRelaxTimeBonus = ValueFloat( *pFormProps, 6 );
		form.fCoverBonus = ValueFloat( *pFormProps, 7 );

		const CVec3 vRealZero3 = ShiftedZero( pFormProps->vZeroPos );

		// CSquadTreeRootItem::CallMeAfterSerialize pairs the formation's
		// units with the members in order.
		std::size_t nUnit = 0;
		for ( const CSquadFormationPropsItem::SUnit &unit : pFormProps->units )
		{
			if ( nUnit >= members.size() )
			{
				outcome.szError = "formation \"" + pFormProps->GetDisplayName() + "\" has " + std::to_string( pFormProps->units.size() ) +
				                  " soldiers and the squad only " + std::to_string( members.size() ) + " members";
				return false;
			}
			SSquadRPGStats::SFormation::SEntry entry;
			if ( !MakeName( members[nUnit]->GetDisplayName(), context, entry.szSoldier, outcome ) )
				return false;
			entry.vPos.x = unit.vPos.x - vRealZero3.x;
			entry.vPos.y = unit.vPos.y - vRealZero3.y;
			entry.fDir = ToDegree( unit.fDir );
			form.order.push_back( entry );
			++nUnit;
		}
		rpgStats.formations.push_back( form );
	}
	return true;
}

}

bool ExportSquad( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_SQUAD_ROOT_ITEM, "squad", outcome );
	if ( !pProject )
		return false;
	SSquadRPGStats rpgStats;
	if ( !FillRPGStats( rpgStats, *pProject->root, context, outcome ) )
		return false;
	const std::string szFile = StatsFileName( project, context, "squads\\", false );
	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
	}, outcome ) )
		return false;
	if ( context.bStatsOnly || rpgStats.szIcon.empty() )
		return true;
	// A picture given by a backslash path is taken as it stands, any other is
	// beside the project. MFC ignored a failed copy; the port reports it.
	std::string szSource = rpgStats.szIcon;
	std::string szShort = szSource;
	const std::string::size_type nPos = szSource.rfind( '\\' );
	if ( nPos != std::string::npos )
		szShort = szSource.substr( nPos + 1 );
	else
		szSource = ProjectDirectory( context ) + szSource;
	if ( !NImageExport::CopyFileInto( context, szSource, DirectoryOf( szFile ) + szShort, outcome ) )
	{
		outcome.warnings.push_back( outcome.szError );
		outcome.szError.clear();
	}
	return true;
}

}
