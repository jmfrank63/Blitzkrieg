#ifndef __MAP_GEOMETRY_H__
#define __MAP_GEOMETRY_H__
#include <string>
#include <vector>
#include "../Formats/fmtMap.h"
// The geometry of the M2 group tools, as plain numbers in and plain numbers
// out (research Pattern 4, C5): no engine, no object database, no renderer.
// The bridge fills the inputs from the RPG stats and the map-file tier fills
// them from literals, so both build the same records with the same function
// (the spec's "same function" rule for expected values).
//
// Units: a drag is world (Vis) units, as the pointer and the camera are; a
// planned piece's position is map (AI) units, as an object's is. The
// conversions are Formats/fmtTerrain.h's own (Vis2AI truncates with +0.3,
// FitVisOrigin2AIGrid), never re-derived here.
namespace NMapGeometry
{
// SBridgeRPGStats::EDirection (Main/RPGStats.h), repeated so this unit needs
// no stats header: the engine tier checks the two agree.
enum EBridgeDirection
{
	BRIDGE_VERTICAL = 0,
	BRIDGE_HORIZONTAL = 1,
};
// The packed frame types a saved bridge span holds (SBridgeRPGStats'
// BRIDGE_SPAN_TYPE_BEGIN/CENTER/END), what CMapInfo::PackFrameIndices writes
// before every MFC save (research C6).
enum EBridgeSpanType
{
	BRIDGE_SPAN_BEGIN = 0x00000001,
	BRIDGE_SPAN_CENTER = 0x00000002,
	BRIDGE_SPAN_END = 0x00000004,
};

// One piece a tool will place: its position in map (AI) units, z 0, its packed
// frame type and its direction (a WORD angle, 0 for a bridge span).
struct SPlannedPiece
{
	CVec3 vPos;
	int nPackedType;
	int nDir;
	SPlannedPiece() : vPos( VNULL3 ), nPackedType( 0 ), nDir( 0 ) {  }
};

// What a bridge plan needs of a bridge type: its direction (EBridgeDirection),
// the length of its first line span in world units (SSpan::fLength *
// fWorldCellSize / 2, as RoadDrawState.cpp's GetPointsForBridge takes it) and
// the AI-unit origin of the begin span the plan starts with (GetOrigin of the
// begin index the bridge chose).
struct SBridgePlanInput
{
	int nDirection;
	float fSpanLength;
	CVec2 vBeginOrigin;
	SBridgePlanInput() : nDirection( BRIDGE_HORIZONTAL ), fSpanLength( 0.0f ), vBeginOrigin( VNULL2 ) {  }
};

// The spans of a bridge dragged from vFirstVis to vLastVis (world units), the
// MFC editor's GetPointsForBridge and its commit (RoadDrawState.cpp:71-104,
// 969-1031) with the save-time nudge (TemplateEditorFrame1.cpp:3168-3179)
// applied once:
//  - refused (false, the reason in pWhy) for a length that is not finite and
//    above 0, a non-finite point, and a drag whose longer axis is not the
//    type's direction ("this bridge runs horizontally/vertically"); a drag
//    with no length at all matches either direction;
//  - the drag is locked to the direction's axis (horizontal keeps the first
//    point's y, vertical its x) and its ends ordered so the first is the lower;
//  - the start is fitted with FitVisOrigin2AIGrid( vBeginOrigin ), and
//    n = int( ( last - first ) / L ) middle spans follow, 0 allowed (a begin
//    and an end span only);
//  - world positions: begin at the fitted start, middle i at start + ( i + 0.5 )
//    * L, end at start + n * L, along the axis; each converted with Vis2AI;
//  - one nudge, in map units after the truncation: the first span of a
//    horizontal bridge x - 0.1, the last span of a vertical one y + 0.1;
//  - types BEGIN first, END last, CENTER between; direction 0.
// The MFC editor's own world-unit nudges before the conversion are left out:
// the saved value is the truncated position with the one nudge (Pitfall 6).
bool PlanBridge( const SBridgePlanInput &rInput, const CVec2 &vFirstVis, const CVec2 &vLastVis,
                 std::vector<SPlannedPiece> *pSpans, std::string *pWhy );

// The rotated variant of a bridge type: a trailing "_01" and "_02" swapped
// (every shipped family ships both, _01 horizontal and _02 vertical), the rest
// of the name kept as it is. Empty for a name with neither suffix.
std::string BridgePartnerName( const std::string &szName );
}
#endif // __MAP_GEOMETRY_H__
