#pragma once
// The 3D River exporter and importer: C3DRiverFrame::FillRPGStats,
// SaveRPGStats and ExportFrameData (Sources/src/editor/3dRiverFrm.cpp:
// 107-160). ExportRiver3D is declared in stats_export.h with the other
// exporters.
//
// MFC's GetRPGStats for a river has an empty body, so MFC could not read a
// river back. The slice asks for rivers that import from shipped Data, so
// River3DStatsToTree is the port's addition: it fills the bottom layer props
// and one Layers child per descriptor layer, from what the export reads. The
// exporter is MFC's. What the export writes as constants (the bottom's
// stream speed, disturbance and relative width, every layer's relative
// width and cell count) cannot come back from a shipped file; the comparator
// lists those as round-trip losses.

#include <string>

#include "../../tree_item.h"

struct SVectorStripeObjectDesc;

namespace NResourceModel
{

// FillRPGStats: the descriptor the project's tree says. False with szError
// naming what is missing. root is a project root with CreateDefaultChilds run.
bool FillRiver3DDesc( const CTreeItem &root, SVectorStripeObjectDesc &desc, std::string &szError );

// The tree half of an import: root is a default tree (CreateDefaultChilds
// has run).
void River3DStatsToTree( const SVectorStripeObjectDesc &desc, CTreeItem &root );

}
