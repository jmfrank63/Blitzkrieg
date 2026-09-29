#ifndef __MAP_RECORDS_H__
#define __MAP_RECORDS_H__
#include <string>
#include <vector>
#include "../Formats/fmtMap.h"
struct SLoadMapInfo;
// Record-level edits of the map's M2 collections, laid over a snapshot the way
// NMapOverlay lays object edits over it (spec: "Saving: the snapshot and the
// overlay"). Every function edits the map in place: nothing renumbers, nothing
// rebuilds a collection, so a record the edit did not name stays byte for byte
// what the file held, and an edit followed by its inverse writes the file it
// started from. Nothing here needs the engine or the object database; the
// bridge applies the same calls to its own copies and the tests build their
// expected map with them.
//
// Units: object positions, areas, start-command targets, reserve positions
// and parcels are map (AI) units; the camera, the anchors, the sounds and the
// road and river points are world (Vis) units. A function never converts: the
// caller hands it what the record holds.
//
// Every function that can be given a bad index or value returns false and then
// leaves the map exactly as it was.
namespace NMapRecords
{
// The camera anchors of a map: the neutral one (vCameraAnchor) and one per
// player (playersCameraAnchors). World units; VNULL3 means "not set" - the
// game reads playersCameraAnchors[user] first and falls back to the neutral
// anchor when that is VNULL3 or absent (GameTT/iMissionInternal.cpp).
struct SCameraAnchors
{
	CVec3 vNeutral;
	std::vector<CVec3> players;
	SCameraAnchors() : vNeutral( VNULL3 ) {  }
};

// No player index beyond this is ever asked for: a map with a couple of
// players never needs more, and a bad caller must not be able to ask for a
// vector of a few billion entries.
const int nMaxCameraAnchorPlayers = 1024;

void GetCameraAnchors( const SLoadMapInfo &rMap, SCameraAnchors *pOut );
// An exact put: vCameraAnchor and playersCameraAnchors become exactly the given
// values, the vector resized to players.size(). That is what undo needs to
// bring back a vector a set had grown. Never called on open, so a map keeps
// the size its file gave the vector (the MFC editor resized it on open; the
// editor does not copy that, spec C8).
bool PutCameraAnchors( SLoadMapInfo *pMap, const SCameraAnchors &rAnchors );
// Sets one player's anchor in a value being built. Pads with VNULL3 up to
// nPlayer + 1 and never shrinks. False for a negative or absurd player.
bool SetPlayerCameraAnchor( SCameraAnchors *pAnchors, int nPlayer, const CVec3 &vAnchor );
// Makes one player's anchor unset, in place; the vector keeps its size. A
// player past the end is already unset and changes nothing.
bool ClearPlayerCameraAnchor( SCameraAnchors *pAnchors, int nPlayer );
}
#endif // __MAP_RECORDS_H__
