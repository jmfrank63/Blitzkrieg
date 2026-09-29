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

// Every list below is edited by position. An index is 0..size-1 for a replace
// or an erase and 0..size for an insert, where -1 also appends (a new record
// appends, D-01: list order is kept). An erase can hand back the record it took
// out, so the caller can put it back at the same index; nothing is renumbered.

// The script file: a bare name, empty for "None". PutScriptFile is an exact put
// with no check - undo must be able to bring back whatever a file held, odd
// name included (Pitfall 12). IsBareScriptName is the check a NEW value passes:
// empty, or letters, digits, '_', '-' and '.' only, no leading dot, no "..",
// no ".lua" suffix (the game adds it), at most 63 characters - so it can never
// name a path.
bool PutScriptFile( SLoadMapInfo *pMap, const std::string &szName );
bool IsBareScriptName( const std::string &szName );

// Script areas. Positions and sizes are map (AI) units.
bool InsertScriptArea( SLoadMapInfo *pMap, int nIndex, const SScriptArea &rArea );
bool ReplaceScriptArea( SLoadMapInfo *pMap, int nIndex, const SScriptArea &rArea );
bool EraseScriptArea( SLoadMapInfo *pMap, int nIndex, SScriptArea *pErased = 0 );
// Case-sensitive (Pitfall 14). An empty name is never free. nIgnoreIndex is the
// area being renamed, or -1.
bool IsAreaNameFree( const SLoadMapInfo &rMap, const std::string &szName, int nIgnoreIndex );

// Reinforcement groups, keyed by group ID; ids are SCRIPT IDs of objects. A put
// creates the group or replaces its ids (order kept as given); the file writes
// the groups in ID order whatever order they were put in. False for a negative
// group ID, and for an erase of a group that is not there.
bool PutReinforcementGroup( SLoadMapInfo *pMap, int nGroupID, const std::vector<int> &rIDs );
bool EraseReinforcementGroup( SLoadMapInfo *pMap, int nGroupID, std::vector<int> *pErased = 0 );
// The first group ID at or above nFrom (clamped to 0) that no group uses (C9).
int FirstFreeGroupID( const SLoadMapInfo &rMap, int nFrom );

// Start commands (std::list) and reserve positions (std::list). A start
// command's target and a reserve position are map (AI) units.
bool InsertStartCommand( SLoadMapInfo *pMap, int nIndex, const SAIStartCommand &rCommand );
bool ReplaceStartCommand( SLoadMapInfo *pMap, int nIndex, const SAIStartCommand &rCommand );
bool EraseStartCommand( SLoadMapInfo *pMap, int nIndex, SAIStartCommand *pErased = 0 );
bool InsertReservePosition( SLoadMapInfo *pMap, int nIndex, const SBattlePosition &rPosition );
bool ReplaceReservePosition( SLoadMapInfo *pMap, int nIndex, const SBattlePosition &rPosition );
bool EraseReservePosition( SLoadMapInfo *pMap, int nIndex, SBattlePosition *pErased = 0 );

// The AI general, one side at a time. A put sets the side count and one side
// together, so an undo that put back the old count and the old side restores
// the vector's size exactly: creating side 3 on a map with one side creates
// sides 1 and 2 empty (C8, Pitfall 11), and undo shrinks them away again.
const int nMaxAIGeneralSides = 1024;
struct SAIGeneralSidePut
{
	int nSideCount;										// the size sidesInfo becomes
	int nSide;												// the side to set; -1 or nSideCount and above: none, resize only
	SAIGeneralSideInfo info;					// that side's record
	SAIGeneralSidePut() : nSideCount( 0 ), nSide( -1 ) {  }
};
// nSideCount is the current size; a side the map does not have reads empty.
void GetAIGeneralSide( const SLoadMapInfo &rMap, int nSide, SAIGeneralSidePut *pOut );
bool PutAIGeneralSide( SLoadMapInfo *pMap, const SAIGeneralSidePut &rPut );

// Roads and rivers (STerrainInfo::roads3 and ::rivers). Points are world (Vis)
// units. The stripe object is stored whole, derived by the bridge from control
// points and put here raw; undo and redo put the recorded value back.
enum EVsoKind { VSO_ROAD, VSO_RIVER };
bool InsertVso( SLoadMapInfo *pMap, EVsoKind eKind, int nIndex, const SVectorStripeObject &rVso );
bool ReplaceVso( SLoadMapInfo *pMap, EVsoKind eKind, int nIndex, const SVectorStripeObject &rVso );
bool EraseVso( SLoadMapInfo *pMap, EVsoKind eKind, int nIndex, SVectorStripeObject *pErased = 0 );
// The bridge's own ID for a new road or river (D-03): one above the highest nID
// over roads and rivers together, at least 1 - never the engine's random one.
int NextVsoID( const SLoadMapInfo &rMap );

// Bridges (link IDs of the spans, CMapInfo::bridges) and entrenchments
// (sections of link IDs of the pieces).
bool InsertBridgeEntry( SLoadMapInfo *pMap, int nIndex, const std::vector<int> &rLinkIDs );
bool ReplaceBridgeEntry( SLoadMapInfo *pMap, int nIndex, const std::vector<int> &rLinkIDs );
bool EraseBridgeEntry( SLoadMapInfo *pMap, int nIndex, std::vector<int> *pErased = 0 );
bool InsertEntrenchment( SLoadMapInfo *pMap, int nIndex, const SEntrenchmentInfo &rEntrenchment );
bool ReplaceEntrenchment( SLoadMapInfo *pMap, int nIndex, const SEntrenchmentInfo &rEntrenchment );
bool EraseEntrenchment( SLoadMapInfo *pMap, int nIndex, SEntrenchmentInfo *pErased = 0 );

// Two fields of an object, found by link ID in objects and then scenarioObjects.
// Link ID 0 is refused (hundreds of shipped objects carry it, C11) and so is an
// unknown link. A script ID is -1 (none) to 32000; an HP is any finite number -
// a bridge span the mission builds later holds a negative one.
bool SetObjectScriptID( SLoadMapInfo *pMap, int nLinkID, int nScriptID );
bool SetObjectHP( SLoadMapInfo *pMap, int nLinkID, float fHP );
}
#endif // __MAP_RECORDS_H__
