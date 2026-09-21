#ifndef __MAP_OVERLAY_H__
#define __MAP_OVERLAY_H__
#include <string>
#include <vector>
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
	SAddObject() : vPos( VNULL3 ), nDir( 0 ), nPlayer( 0 ), bScenario( false ) {  }
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
// Refuses, filling pRefusal, when anything refers to the object.
bool DeleteObject( SLoadMapInfo *pMap, int nLinkID, std::string *pRefusal );
bool SetDiplomacy( SLoadMapInfo *pMap, int nPlayer, BYTE nDiplomacy );
}
#endif // __MAP_OVERLAY_H__
