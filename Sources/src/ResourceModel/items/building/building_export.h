#pragma once
// The Building exporter and importer: CBuildingFrame::ExportFrameData and
// SaveRPGStats (Sources/src/editor/BuildFrm.cpp:454-920), CBuildingTreeRootItem::
// ComposeAnimations (BuildTreeItem.cpp:68) and CBuildingFrame::LoadRPGStats
// (BuildFrm.cpp:922-1274). ExportBuilding is declared in stats_export.h with
// the other exporters; the two functions here are the importer's halves, which
// the bridge's BkResImportFromGame calls with the stats the engine's reader
// found.
//
// MFC kept a building's grids and points in the frame, as lists of tiles and
// sprites, and SaveRPGStats wrote them into desc. The port keeps desc as their
// home (the bridge's geometry channels edit it), so the exporter takes the
// grids, origins and positions from it and everything else from the tree, and
// the importer is the inverse: the tree half from the stats, and desc from the
// stats' grids and points.

#include "../../project.h"
#include "../../tree_item.h"

struct SBuildingRPGStats;

namespace NResourceModel
{

// LoadRPGStats' tree half: the basic properties, the AI classes the building
// blocks, the six defences, the effect names, and one tree child per shoot,
// fire and smoke point and the values of the five fixed explosion children.
// root is a default tree (CreateDefaultChilds has run).
void BuildingStatsToTree( const SBuildingRPGStats &stats, CTreeItem &root );

// LoadRPGStats' frame half: the stats' passability and visibility grids with
// their origins, the entrance, and every point's position, picture position and
// world position, written into the project element's desc, and the two crosses
// where a frame starts them in own_data (no TransLines: a building has none).
void WriteBuildingFrameData( NResourceXml::Node &root, const SBuildingRPGStats &stats );

}
