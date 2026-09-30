#ifndef __MAP_OVERLAY_H__
#define __MAP_OVERLAY_H__
#include <string>
#include <vector>
#include "../Formats/fmtMap.h"
struct SLoadMapInfo;
namespace NMapOverlay
{
// The edits the editor lays over a snapshot, as the spec's "Saving: the
// snapshot and the overlay" describes them. Nothing here needs the renderer or
// the object database: an added object's frame index stays 0 for the bridge to
// pack once it can look the type up.
struct SAddObject
{
	std::string szName;
	CVec3 vPos;
	int nDir;
	int nPlayer;
	bool bScenario;											// scenarioObjects rather than objects
	int nLinkID;												// -1: NextLinkID; otherwise this one, which must be free
	// The three fields an add used to hard-code. The defaults are those
	// values, so a caller that sets none of them behaves as before.
	int nFrameIndex;										// left 0 for the bridge to pack, unless the caller knows better
	float fHP;													// a fraction of the maximum; 1 is whole
	int nScriptID;											// -1: none
	// What the object is linked with: -1, as before, for the spans, fences and
	// trench pieces the group tools add; 0 - "nothing", SLinkInfo's own default
	// and what the MFC editor wrote for an object it had not linked - for an
	// object a person places. The game lands a reinforcement only when its
	// nLinkWith is 0 (CScripts::LandSuspendedReiforcements), so a unit placed
	// with -1 would wait in the queue for good.
	int nLinkWith;
	SAddObject() : vPos( VNULL3 ), nDir( 0 ), nPlayer( 0 ), bScenario( false ), nLinkID( -1 ), nFrameIndex( 0 ), fHP( 1.0f ), nScriptID( -1 ), nLinkWith( -1 ) {  }
};
struct SMoveObject
{
	int nLinkID;
	CVec3 vPos;
	int nDir;
	int nPlayer;
	SMoveObject() : nLinkID( -1 ), vPos( VNULL3 ), nDir( 0 ), nPlayer( 0 ) {  }
};

// One above every link ID in use, so a new object can never collide with a
// reference held elsewhere in the map.
int NextLinkID( const SLoadMapInfo &rMap );

// Everything that refers to nLinkID, named for the status bar: bridges, trench
// pieces, start commands (as a unit or as the target), reserve positions,
// reinforcement groups and the AI general's mobile reinforcements (both by the
// object's SCRIPT ID, never its link ID), and a passenger holding it as its
// vehicle. Link ID 0 is "no link ID" - hundreds of shipped objects carry it -
// and finds nothing. Not the same as what refuses a delete: see DeleteObject.
void FindReferences( const SLoadMapInfo &rMap, int nLinkID, std::vector<std::string> *pReferences );

bool AddObject( SLoadMapInfo *pMap, const SAddObject &rAdd, int *pnLinkID );
bool MoveObject( SLoadMapInfo *pMap, const SMoveObject &rMove );
// What deleting an object changed besides the object, in the order it was
// applied, so RestoreObject can undo it in reverse. Positions are indices in
// the list at the moment of the change (an earlier erase has already shifted
// the later ones); nothing is renumbered and the other records are untouched.
struct SStartCommandChange
{
	size_t nPosition;
	SAIStartCommand before;						// the record as it was, to put back
	bool bErased;											// erased (no unit left) rather than edited
	bool bUnitRemoved;								// the object was in unitLinkIDs
	bool bTargetCleared;							// the target linkID was set to 0
	SStartCommandChange() : nPosition( 0 ), bErased( false ), bUnitRemoved( false ), bTargetCleared( false ) {  }
};
struct SReservePositionChange
{
	size_t nPosition;
	SBattlePosition before;						// the erased record
	SReservePositionChange() : nPosition( 0 ) {  }
};
struct SCascade
{
	std::vector<SStartCommandChange> startCommands;
	std::vector<SReservePositionChange> reservePositions;
	// Things a delete leaves alone but the player should hear about: a script
	// ID a reinforcement group or the AI general still names.
	std::vector<std::string> notes;
};
// One short English line for the status bar; empty when nothing else changed.
void DescribeCascade( const SCascade &rCascade, std::string *pOut );

// A deleted object's record, the list it was in and its place in that list,
// and the cascade: what RestoreObject needs to put it all back as it was.
struct SDeletedObject
{
	SMapObjectInfo object;
	bool bScenario;
	size_t nIndex;
	SCascade cascade;
	SDeletedObject() : bScenario( false ), nIndex( 0 ) {  }
};
// Removes the object and, as the MFC editor's delete does, takes it out of the
// records that name it: start commands lose it from their units (a command left
// with no unit is erased) and are cleared of it as target, reserve positions
// naming it are erased. Refuses, filling pRefusal and changing nothing, only
// for what the game's loaders and the links of M3 depend on: a bridge span, a
// trench piece, and a vehicle that holds a passenger. pDeleted, when given,
// receives the record that was taken out and the cascade.
bool DeleteObject( SLoadMapInfo *pMap, int nLinkID, std::string *pRefusal, SDeletedObject *pDeleted = 0 );
// Puts the cascade back in reverse and the record at its index (or the end of
// its list, if the list is now shorter). Refuses, changing nothing, when the
// link ID is in use again.
bool RestoreObject( SLoadMapInfo *pMap, const SDeletedObject &rDeleted );
bool SetDiplomacy( SLoadMapInfo *pMap, int nPlayer, BYTE nDiplomacy );

// One painted cell: the two fields SMainTileInfo has.
struct SPaintCell
{
	int nX, nY;
	BYTE tile, noise;
	SPaintCell() : nX( 0 ), nY( 0 ), tile( 0 ), noise( 0 ) {  }
};

// Everything a paint command must remember to undo itself: the tiles and the
// patch crosses of all of the affected region, as they were before it ran.
// Undo restores exactly these rather than re-running the function, because the
// preprocessing pass makes the order of commands matter.
struct SPaintUndo
{
	CTRect<int> rPatches;								// the region, in patch coordinates
	std::vector<SMainTileInfo> tiles;		// row-major over the region's cell rectangle
	std::vector<STerrainPatchInfo> patches;	// row-major over rPatches
	SPaintUndo() : rPatches( 0, 0, 0, 0 ) {  }
};

// The affected region R of the spec's "Terrain edits": every patch holding a
// painted cell or a cell next to one, in PATCH coordinates - which is what
// CMapInfo::UpdateTerrainCrosses iterates.
CTRect<int> AffectedPatches( const struct STerrainInfo &rTerrain, const std::vector<SPaintCell> &rCells );

// Sets the cells, runs the preprocessing pass and regenerates the crosses of R
// - the deterministic function the bridge, the editor and the tests all use.
// Fills pUndo with the state of R beforehand.
bool Paint( SLoadMapInfo *pMap, const std::vector<SPaintCell> &rCells, SPaintUndo *pUndo );
void UndoPaint( SLoadMapInfo *pMap, const SPaintUndo &rUndo );
// The region's tiles and patches as they are now, in SPaintUndo's layout.
void CaptureRegion( const SLoadMapInfo &rMap, const CTRect<int> &rPatches, SPaintUndo *pOut );

// D-19 (M3): everything an altitude region edit must remember to undo itself.
// rVertices is in terrain-VERTEX coordinates - altitudes are indexed by
// terrain vertex, one more per axis than the tiles - and altitudes are
// row-major over that rectangle. The rectangle the record holds is the region
// GROWN BY THE SHADE KERNEL (GrowForShades): a vertex's shade depends on its
// neighbours' normals, so an edit inside R changes the shades of R's ring as
// well, and undo has to put those back too. As with SPaintUndo, undo restores
// exactly these rather than re-running any function, and the values must be
// captured from the map's own storage, never a copy of it: SVertexAltitude is
// written as a raw struct and its three padding bytes ride along with a
// capture, whatever a copy left there.
struct SAltitudeUndo
{
	CTRect<int> rVertices;								// the region, in terrain-vertex coordinates
	std::vector<SVertexAltitude> altitudes;	// row-major over rVertices
	SAltitudeUndo() : rVertices( 0, 0, 0, 0 ) {  }
};

// The shade kernel: a height edit changes every vertex whose normal reads it,
// which is one vertex on each side of the edit - the MFC editor grows its
// shade update rect the same way (DrawShadeState.cpp:210-213). Grown by one
// vertex per side and clamped to the map's vertex bounds; a rectangle already
// at the edge grows only the sides that have room.
CTRect<int> GrowForShades( const SLoadMapInfo &rMap, const CTRect<int> &rVertices );

// The region's altitudes as they are now, in SAltitudeUndo's layout.
void CaptureAltitudeRegion( const SLoadMapInfo &rMap, const CTRect<int> &rVertices, SAltitudeUndo *pOut );
// Writes the values row-major over rVertices, an in-place assignment of whole
// SVertexAltitude records (so a raw undo can put them back). False, with the
// map untouched, for a bad rectangle (empty, inverted or off the map) or a
// value count that does not match the rectangle. pBefore, when given, gets
// the region's state beforehand. The shade of each value is the caller's to
// fill: the deterministic function of D-19 sets the heights and then runs
// CMapInfo::UpdateTerrainShades over GrowForShades of the rectangle, which is
// the bridge's ApplyAltitudesInSession, not this write.
bool SetAltitudeRegion( SLoadMapInfo *pMap, const CTRect<int> &rVertices, const std::vector<SVertexAltitude> &rValues, SAltitudeUndo *pBefore );
// Puts the recorded region back raw - heights, shades and padding bytes, and
// nothing re-run, exactly SPaintUndo's own rule.
void UndoAltitudeRegion( SLoadMapInfo *pMap, const SAltitudeUndo &rUndo );

// The same three over a bare STerrainInfo: the engine keeps its own terrain
// and has no per-vertex altitude call, so the bridge writes its copy in place
// the way the MFC editor did through GetTerrainInfo (DrawShadeState.cpp:204).
// The map-level ones above are these over pMap->terrain.
void CaptureTerrainAltitudeRegion( const STerrainInfo &rTerrain, const CTRect<int> &rVertices, SAltitudeUndo *pOut );
bool SetTerrainAltitudeRegion( STerrainInfo *pTerrain, const CTRect<int> &rVertices, const std::vector<SVertexAltitude> &rValues, SAltitudeUndo *pBefore );
void UndoTerrainAltitudeRegion( STerrainInfo *pTerrain, const SAltitudeUndo &rUndo );
}
#endif // __MAP_OVERLAY_H__
