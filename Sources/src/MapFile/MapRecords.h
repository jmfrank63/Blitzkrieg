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

// The properties' fields (M3, D-26), the same contract: in-place, found by
// link ID, false-and-untouched on bad input. A player is 0..diplomacies-1
// (the properties combo's own range). An angle is DEGREES, the MFC
// properties dialog's unit, turned into the record's 65536-direction with
// the MFC's own formula (SEditorMApObject.cpp:384-386). A formation is the
// squad record's frame index (the MFC properties' Formation combo writes the
// squad's, SEditorMApObject.cpp:522-547), 0 or greater. A link is the host's
// link ID, which must be an object of the map and never the passenger's own;
// 0 unlinks (a palette-placed object is linked with nothing, SAddObject::nLinkWith).
bool SetObjectPlayer( SLoadMapInfo *pMap, int nLinkID, int nPlayer );
bool SetObjectAngle( SLoadMapInfo *pMap, int nLinkID, float fAngleDegrees );
bool SetObjectFormation( SLoadMapInfo *pMap, int nLinkID, int nFormation );
bool SetObjectLink( SLoadMapInfo *pMap, int nLinkID, int nLinkWith );
// True when linking nLinkID to nLinkWith would close a loop: nLinkWith's own
// host chain leads back to nLinkID (or nLinkWith is nLinkID). SetObjectLink
// refuses such a link, and the bridge's link command asks it too.
bool WouldLinkCycle( SLoadMapInfo *pMap, int nLinkID, int nLinkWith );

// Players (M3, D-30). The map's diplomacies hold one entry per player and the
// neutral player LAST (0 and 1 are the two sides, 2 the neutral); the most a
// map holds is 16 players and the neutral. The per-player collections that
// follow the players by position are the unit creation (units) and the player
// camera anchors; the objects name their owner by index.
const int nMaxPlayerEntries = 17;
// The fewest entries a map keeps: two players and the neutral. The MFC allowed
// fewer; a mission needs its two sides, so the editor does not.
const int nMinPlayerEntries = 3;

// Adds a player of side nSide (0 or 1) just before the neutral entry - the MFC
// dialog's own insert (TabSimpleObjectsDiplomacyDialog.cpp:263). The player
// takes the neutral's index and the neutral moves up by one, so every owner
// index at or above the old neutral's moves up with it (an object of the neutral
// stays the neutral's: the MFC left it to take the new player's index, which no
// one meant). The unit creation and the player camera anchors gain a default /
// unset entry at the new index when the file held entries for every player
// (the MFC resized both to players-many, TabSimpleObjectsDialog.cpp:627, 3435);
// a vector shorter than that is left alone (the file's own size, as on open).
// False and untouched at nMaxPlayerEntries or without a neutral entry.
bool InsertPlayer( SLoadMapInfo *pMap, BYTE nSide );

// Deletes player nPlayer (0 .. diplomacies-2: never the neutral entry), the
// players above it moving down by one with their unit creation, their camera
// anchors and their objects. The deleted player's objects become the neutral's
// (D-30; TemplateEditorFrame1.cpp:6013's own rule for an owner out of range).
// False and untouched for the neutral, an index out of range, or when fewer than
// nMinPlayerEntries entries would remain.
bool ErasePlayer( SLoadMapInfo *pMap, int nPlayer );

// One player's unit creation (the MFC's Map Unit Creation Property, one entry of
// SUnitCreationInfo::units). A player the vector does not hold yet reads as the
// defaults the game's own Validate would fill in (party USSR, the default
// aircraft, relax time 20) and reads from a map exactly what the file holds
// otherwise. PutUnitCreation is an exact put: units becomes exactly nSlotCount
// long (padded with validated defaults, or cut), so an undo can bring back a
// vector a put grew. A slot count that does not reach nPlayer leaves the entry
// out (the vector does not hold that player: the state a read of it answered
// with the defaults for). False and untouched for a player outside
// 0..nMaxPlayerEntries-2, a slot count outside 0..nMaxPlayerEntries-1, or an
// entry that does not hold the five aircraft.
bool GetUnitCreation( const SLoadMapInfo &rMap, int nPlayer, SUnitCreation *pOut );
bool PutUnitCreation( SLoadMapInfo *pMap, int nPlayer, const SUnitCreation &rUnitCreation, int nSlotCount );
}
#endif // __MAP_RECORDS_H__
