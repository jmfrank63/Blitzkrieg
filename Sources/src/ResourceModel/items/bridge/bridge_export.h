#pragma once
// The Bridge exporter and importer: CBridgeFrame::ExportFrameData,
// FillRPGStats, SaveSegmentInformation and AddSpriteAndShadow (Sources/src/
// editor/BridgeFrm.cpp:652-1358, 2416-2591) and the tree half of GetRPGStats /
// LoadRPGStats (553-621, 1149-1270). ExportBridge is declared in
// stats_export.h with the other exporters; the two functions here are the
// importer's halves, which the bridge's BkResImportFromGame calls with the
// stats the engine's reader found.
//
// MFC kept a bridge's fire and smoke points in the frame, as sprites on the
// view, and its project file holds their positions in the "RPG" chunk that
// SaveRPGStats writes beside the tree (and the Begin, End, Front and Back marks
// in own_data). No tree item owns them, so the exporter takes each point's
// position, picture position and world position from that chunk and its
// direction and effect from the tree, as GetRPGStats gave the tree the values
// and FillRPGStats read both. A tree point that has no entry in the chunk is
// skipped, as MFC skipped a point whose sprite was gone.
//
// A bridge's source pictures are not in its stats: the packs hold them. The
// importer therefore gives every part it creates a placeholder picture name
// under imported\<stage>\<frame index>.tga (nothing for a girder the span does
// not have), which a stats-only export reads as "this part exists" and a full
// export refuses by naming the file until the author supplies the art.

#include "../../project.h"
#include "../../tree_item.h"
#include "../../grid_projection.h"

struct SBridgeRPGStats;

namespace NResourceModel
{

// LoadRPGStats' tree half: the basic properties (direction, health, repair cost
// and the four passability flags from the AI classes), the six defences, the
// three damage stages with one span item per entry of their begin, line and end
// lists (each with the part pictures it names and, for the first begin, line
// and end span of the whole stage, the locked, unlocked and transparency tiles
// of the slab's grids), and one tree child per fire and smoke point.
// projection places the grids' tiles from the default marks that
// WriteBridgeFrameData writes. root is a default tree (CreateDefaultChilds has
// run). name is the project's name.
void BridgeStatsToTree( const SBridgeRPGStats &stats, CTreeItem &root, const GridProjection &projection, const std::string &szName );

// LoadRPGStats' frame half: own_data with the default Begin and End marks and
// the Front and Back offsets of the girders' relative positions, and the "RPG"
// chunk with the stats' fire, smoke and directed-explosion points.
void WriteBridgeFrameData( NResourceXml::Node &root, const SBridgeRPGStats &stats );

}
