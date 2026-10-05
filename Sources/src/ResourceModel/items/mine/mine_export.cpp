// The mine exporter: CMineFrame::ExportFrameData (Sources/src/editor/
// MineFrm.cpp:70-125). SaveRPGStats writes the stats; the composed sprite
// "1" (ComposeSingleObject of 1.tga and 1s.tga) is the graphics half, which
// a stats-only export leaves alone.
//
// As for the weapon, MFC wrote the defaults while bNewProjectJustCreated was
// set; the bridge exports saved projects only, so the tree is always used.
#include "StdAfx.h"

#include "../stats_export.h"
#include "../tree_item_types.h"
#include "../../../Main/RPGStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kMineAddDir[] = "objects\\simpleobjects\\common\\summer\\mine\\";

// CMineFrame::FillRPGStats. GetMineName is values[0], the "Weapon" prop:
// MFC names the mine after its weapon and uses it for both fields.
bool FillRPGStats( SMineRPGStats &rpgStats, const CTreeItem &rootItem, SExportOutcome &outcome )
{
	const CTreeItem *pCommonProps = RequireChild( rootItem, ETIT_MINE_COMMON_PROPS_ITEM, 0, "Basic info", outcome );
	if ( pCommonProps == nullptr )
		return false;
	rpgStats.szKeyName = ValueStr( *pCommonProps, 0 );
	rpgStats.fWeight = float( ValueInt( *pCommonProps, 1 ) );
	rpgStats.szFlagModel = "1";
	rpgStats.szWeapon = ValueStr( *pCommonProps, 0 );
	return true;
}

}

bool ExportMine( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_MINE_ROOT_ITEM, "mine", outcome );
	if ( !pProject )
		return false;
	SMineRPGStats rpgStats;
	if ( !FillRPGStats( rpgStats, *pProject->root, outcome ) )
		return false;
	const std::string szFile = StatsFileName( project, context, kMineAddDir, false );
	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
	}, outcome ) )
		return false;
	// The sprite the game builds for the mine, "1" beside the stats.
	outcome.szObjectName = DirectoryOf( szFile ) + "1";
	return true;
}

}
