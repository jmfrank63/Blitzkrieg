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
	SAddObject() : vPos( VNULL3 ), nDir( 0 ), nPlayer( 0 ), bScenario( false ), nLinkID( -1 ) {  }
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

// Everything that refers to nLinkID, named for the status bar. Empty means the
// object can be deleted.
void FindReferences( const SLoadMapInfo &rMap, int nLinkID, std::vector<std::string> *pReferences );

bool AddObject( SLoadMapInfo *pMap, const SAddObject &rAdd, int *pnLinkID );
bool MoveObject( SLoadMapInfo *pMap, const SMoveObject &rMove );
// A deleted object's record, the list it was in and its place in that list:
// what RestoreObject needs to put it back as it was.
struct SDeletedObject
{
	SMapObjectInfo object;
	bool bScenario;
	size_t nIndex;
	SDeletedObject() : bScenario( false ), nIndex( 0 ) {  }
};
// Refuses, filling pRefusal, when anything refers to the object. pDeleted, when
// given, receives the record that was taken out.
bool DeleteObject( SLoadMapInfo *pMap, int nLinkID, std::string *pRefusal, SDeletedObject *pDeleted = 0 );
// Puts the record back at its index (or the end of its list, if the list is
// now shorter). Refuses when the link ID is in use again.
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
}
#endif // __MAP_OVERLAY_H__
