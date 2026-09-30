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

// The drag that plans a bridge of nSpans spans (2 or more; fewer is taken as
// 2) about vCentreVis (world units), along rInput's direction: from
// centre - n L / 2 to that plus ( n + 0.5 ) L, n = nSpans - 2, so PlanBridge
// counts n middle spans and the planned bridge is centred on vCentreVis up to
// its grid fit. A rotate (D-11) plans the partner type through this, about
// the old bridge's centre with its span count.
void RotatedBridgeDrag( const SBridgePlanInput &rInput, const CVec2 &vCentreVis, int nSpans, CVec2 *pFirstVis, CVec2 *pLastVis );

// The rotated variant of a bridge type: a trailing "_01" and "_02" swapped
// (every shipped family ships both, _01 horizontal and _02 vertical), the rest
// of the name kept as it is. Empty for a name with neither suffix.
std::string BridgePartnerName( const std::string &szName );

// ---------------------------------------------------------------------------
// Fences (04-07, D-14): the MFC Fences tab, RoadDrawState.cpp:122-182 (a_dirLine),
// 519-622 (the ghost), 841-898 (the single-fence ghost) and 1034-1107 (the
// commit). A fence run is a drag along one axis that places one fence every
// second AI tile; its direction is in the packed frame type.
//
// Tiles are AI tiles (half a world cell, ITerrainEditor::GetAITileIndex), the
// map's extent in them is 32 per patch side; a planned piece's position is in
// map (AI) units, one AI tile being nAITileSize of them (SAIConsts::TILE_SIZE,
// which the engine tier checks this agrees with).
const int nAITileSize = 32;
// FENCE_TYPE_NORMAL and the direction bit of SFenceRPGStats (Main/RPGStats.h),
// repeated so this unit needs no stats header; the engine tier checks them.
const int nFenceTypeNormal = 0x00010000;

// The MFC editor's a_dirLine, unchanged: the tiles of the integer line from
// `from` to `to` inclusive, in order (a Bresenham that steps the longer axis
// and, at equal length, the x axis).
void RasterizeLine( const CTPoint<int> &from, const CTPoint<int> &to, std::vector< CTPoint<int> > *pTiles );

// What a fence plan needs of a fence type and a map: the AI-unit origin of the
// centre segment of each of the four directions (SFenceRPGStats::GetOrigin of
// GetCenterIndex( dir ), the seeded first one) and the map's extent in AI
// tiles.
struct SFencePlanInput
{
	CVec2 vOrigin[4];
	int nTilesX, nTilesY;
	SFencePlanInput() : nTilesX( 0 ), nTilesY( 0 )
	{
		for ( int i = 0; i < 4; ++i )
			vOrigin[i] = VNULL2;
	}
};

// The fences of a drag from firstTile to lastTile (AI tiles):
//  - refused (false, the reason in pWhy) for a map with no extent, a
//    non-finite origin, an end tile whose >> 1 cell is outside the map (the
//    MFC refusal box, patches * 16 - 1), and a planned position outside the
//    map's AI units; refusal is of the whole run;
//  - a drag whose tiles are the same is one fence, direction 0, or 1 when
//    bCtrl (the ghost's single fence); bCtrl is ignored for a longer drag;
//  - otherwise the axis is the one with the longer tile delta (a tie is
//    horizontal, GetCurrentDirection), the last tile is locked to the first's
//    other coordinate, and RasterizeLine's tiles give one fence at every
//    second tile from the first (the MFC loop advances twice);
//  - direction: horizontal 1 when the drag goes left, else 3 and the tile
//    moved two to the right; vertical 0 when it goes up (smaller y) and the
//    tile moved two up, else 2;
//  - position: the tile in AI units, AI2Vis, FitVisOrigin2AIGrid with the
//    direction's origin, Vis2AI - the commit's own chain;
//  - packed type ( 1 << dir ) | nFenceTypeNormal, nDir 0 (the fence's own
//    direction is in its frame index).
bool PlanFences( const SFencePlanInput &rInput, const CTPoint<int> &firstTile, const CTPoint<int> &lastTile, bool bCtrl,
                 std::vector<SPlannedPiece> *pFences, std::string *pWhy );

// ---------------------------------------------------------------------------
// Entrenchments (04-08, D-13): the MFC trench builder, RoadDrawState.cpp:184-237
// (CAngle), 238-290 (GetLineAngle, GetTrenchWidth, SplitLineToSegrments),
// 293-364 (CConnector), 1135-1216 (a click extends the path) and 1403-1585 (the
// double click's commit), ported with its arithmetic: the path is integer
// points (GPoint truncates), a length is the project's two-argument fabs (a
// Euclidean length, Misc/Tools.h:600), an angle wraps into [0, 2 pi) with fmod
// only where the MFC's CAngle operators wrap it.
//
// SEntrenchmentRPGStats' packed piece types (ENTRENCHMENT_LINE / FIREPLACE /
// TERMINATOR / ARC, Main/RPGStats.h), repeated so this unit needs no stats
// header; the engine tier checks them.
enum ETrenchPieceType
{
	TRENCH_LINE = 0x00000001,
	TRENCH_FIREPLACE = 0x00000002,
	TRENCH_TERMINATOR = 0x00000004,
	TRENCH_ARC = 0x00000008,
};
// What the builder reads of the "Entrenchment" stats (GetTrenchWidth): the
// length of a line piece and of an arc piece along the trench, world units -
// the segment's GetVisAABBHalfSize().x * 2 (a line: the first of `lines`, every
// shipped one is as long; an arc: the first of `arcs`). And the map's extent
// in map (AI) units, 0 for none: the engine places a trench piece anywhere,
// even off the map (CAIEditor::IsObjectInsideOfMap passes every
// SGVOGT_ENTRENCHMENT), and the MFC tool never checked, so a piece whose
// centre the map does not hold is refused here (an addition, T-04-08-02).
struct STrenchPlanInput
{
	float fLineWidth;
	float fArcWidth;
	float fMapWidth, fMapHeight;
	STrenchPlanInput() : fLineWidth( 0.0f ), fArcWidth( 0.0f ), fMapWidth( 0.0f ), fMapHeight( 0.0f ) {  }
};
// A planned entrenchment: the builder's path (m_pointForTrench, integer world
// points), the pieces in the order the MFC commit makes them - [0] the begin
// terminator, [1] the end terminator, then one piece per step of the path -
// and the sections, each a list of indices into `pieces` in trench order.
struct STrenchPlan
{
	std::vector< CTPoint<int> > path;
	std::vector<SPlannedPiece> pieces;
	std::vector< std::vector<int> > sections;
};
// The path the clicks build, one click at a time as OnLButtonDown extends
// m_pointForTrench: the first click is the first point; the second adds the
// straight line to it cut into line pieces; a later one either carries the
// last direction on (a turn under 30 degrees, for the click's distance) or
// adds an arc of arc pieces turning the shorter way round (15 degrees first,
// then 30 each) and, when the click is nearer the arc's end than its start,
// a straight run on from it. Refused (false, the reason in pWhy) for widths
// that are not finite or below 4 world units, a click that is not finite or
// farther than a million world units out, and a path of more than 2048
// points.
bool TrenchPath( const STrenchPlanInput &rInput, const std::vector<CVec2> &rClicksVis, std::vector< CTPoint<int> > *pPath, std::string *pWhy );
// The entrenchment the clicks (world units) commit, OnLButtonDblClk's rules:
//  - terminators at the first path point (the first step's angle + pi) and at
//    the last (the last step's angle);
//  - one piece per path step, at the step's integer midpoint: a step longer
//    than 0.9 line widths is straight, a fireplace and a line alternating
//    through the whole trench starting with a fireplace; a shorter one is an
//    arc, turned + pi unless the previous step's angle less its own wraps to
//    more than pi (the previous step is taken from the third step on, as the
//    MFC's `i > 1` does);
//  - direction int( angle / 2 pi * 65535 ), positions Vis2AI (map units);
//  - the first section starts with the begin terminator; a straight piece
//    after an arc closes the section before it and opens the next; the end
//    terminator joins the last section.
// Refused as TrenchPath is, for a path of fewer than two points ("the trench
// is shorter than one piece"), a step whose angle is not a number, and - with
// an extent given - a piece outside [0, fMapWidth) x [0, fMapHeight) ("the
// trench leaves the map").
bool PlanEntrenchment( const STrenchPlanInput &rInput, const std::vector<CVec2> &rPointsVis, STrenchPlan *pPlan, std::string *pWhy );

// ---------------------------------------------------------------------------
// Script areas (04-10, D-21): the MFC editor's area tool, MapToolState.cpp
// (OnLButtonUp: 188-274 the rectangle and circle a drag makes) and
// TemplateEditorFrame1.cpp (CalculateAreasToAI, 3535-3555: the conversion the
// save applied). A drag is world (Vis) units; an area is stored in map (AI) units
// and the game reads it raw (CScripts::InitAreas, GetScriptAreaParams).
//
// The conversion rule, applied ONCE to a new or edited area and never to one the
// map already holds: Vis2AI - times fAITileXCoeff1, plus 0.3, truncated - for the
// centre, the half size and, through x alone, the radius.

// A new area from a drag: a rectangle's centre is the middle of the two points and
// its half size half the distance along each axis; a circle's centre is the first
// point and its radius the (Euclidean) distance to the last. eType is
// SScriptArea::EAT_RECTANGLE or EAT_CIRCLE. Every value comes back in AI units;
// the unused one (a circle's half size, a rectangle's radius) is 0, as the MFC
// editor's own record.
SScriptArea AreaFromVis( int eType, const CVec2 &vFirstVis, const CVec2 &vLastVis, const std::string &szName );
// The area moved so its centre is at vNewCentreVis (world units): the new centre
// converted with the rule, size and name kept.
SScriptArea MoveArea( const SScriptArea &rArea, const CVec2 &vNewCentreVis );
// The area resized by dragging its corner (a rectangle) or edge (a circle) handle
// to vHandleVis (world units): a rectangle's half size is the distance from its
// centre to the handle along each axis, a circle's radius the distance to the
// handle, converted with the rule; centre and name kept.
SScriptArea ResizeArea( const SScriptArea &rArea, const CVec2 &vHandleVis );
}
#endif // __MAP_GEOMETRY_H__
