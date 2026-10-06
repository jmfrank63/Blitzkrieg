#pragma once
// The 3D Road exporter and importer: C3DRoadFrame::FillRPGStats,
// SaveRPGStats, ExportFrameData and GetRPGStats (Sources/src/editor/
// 3dRoadFrm.cpp:86-190). ExportRoad3D is declared in stats_export.h with the
// other exporters; Road3DStatsToTree is the importer's half, which the
// bridge's BkResImportFromGame calls with the descriptor the engine's reader
// found.
//
// MFC's GetRPGStats put back only what the frame needed to redraw: the
// border width, priority, passability, road type, minimap colours, soil
// parameters and the border layer's own props, and it neither set the bottom
// width, the "Has borders?" flag nor the central layer. The importer fills
// every slot the export reads, so a shipped road opens as a project and
// exports the same descriptor. That is the port's addition; the exporter is
// MFC's.

#include <string>

#include "../../tree_item.h"

struct SVectorStripeObjectDesc;

namespace NResourceModel
{

// FillRPGStats: the descriptor the project's tree says. False with szError
// naming what is missing (a road with borders needs its "Border layer").
// root is a project root with CreateDefaultChilds run.
bool FillRoad3DDesc( const CTreeItem &root, SVectorStripeObjectDesc &desc, std::string &szError );

// GetRPGStats for a road: root is a default tree (CreateDefaultChilds has
// run). The border layer child is added when the descriptor has borders.
void Road3DStatsToTree( const SVectorStripeObjectDesc &desc, CTreeItem &root );

}
