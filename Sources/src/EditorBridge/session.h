#ifndef __EDITOR_BRIDGE_SESSION_H__
#define __EDITOR_BRIDGE_SESSION_H__
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "../RandomMapGen/MapInfo_Types.h"
#include "../MapFile/MapOverlay.h"
#include "../MapFile/MapGeometry.h"
#include "bridge.h"

class CEditorWorld;
struct IObjectsDB;
struct SGDBObjectDesc;
interface ITerrainEditor;
interface IAIEditor;
struct SEditorSession;

// One entry of the session's edit log (04-05, research Pattern 3): an edit the
// bridge derives or compounds keeps its own undo record here, next to the
// engine state it must stay consistent with, and the core holds only its
// token. Revert puts the state from before the edit back, Reapply the state
// after it - both from what the record stored, never by running the edit's
// function again (D-03). Each returns false with the reason in szMessage.
struct IEditRecord
{
	virtual ~IEditRecord() {  }
	virtual bool Revert( SEditorSession *pSession ) = 0;
	virtual bool Reapply( SEditorSession *pSession ) = 0;
};

// One editing session.
//
// The snapshot is the map exactly as read, frame indices still packed. The
// working copy has UnpackFrameIndices applied, which picks a random visual
// variant per type - so it is what the engine is built from and never what is
// written back for an object the editor did not touch.
struct SEditorSession
{
	CMapInfo snapshot;
	CMapInfo working;
	std::string szMapPath;
	std::string szDataRoot;
	std::string szMessage;
	// The engine object for a snapshot link ID. An object whose type the
	// database does not know is absent here and listed in unknownLinkIDs; it
	// stays in the snapshot and is written back untouched.
	std::unordered_map<int, CPtr<IRefCount> > byLinkID;
	std::vector<int> unknownLinkIDs;
	// The reinforcement groups as the file had them when the map was opened
	// (04-09). A group put may keep any script ID - and as many copies of it -
	// that the file's own group held, however odd (a duplicate, a value out of
	// range), so an undo can put back what an edit took out; only what an edit
	// ADDS is held to 0..32000, once.
	std::unordered_map< int, std::vector<int> > openedGroups;
	// The link IDs named by CMapInfo::bridges, and the ones of those that got an
	// engine object. A span is built only through the bridge it belongs to, so
	// the two differing means a bridge in the file has a span the engine has not
	// got - which is worth reporting rather than silently drawing a gap.
	int nBridgeSpansInMap;
	int nBridgeSpansPlaced;
	// Spans whose stored HP is negative: a bridge the mission builds later. The
	// engine will not take an object with negative HP, so it is created whole
	// and listed here for whatever draws it. The snapshot keeps the real HP, so
	// a save is unaffected.
	std::vector<int> futureBuildLinkIDs;
	// Every paint of this session, by token (its index). A paint is undone by
	// putting back `before` and redone by putting back `after` - never by
	// running the function again, which would pick new random cross artwork
	// and, through the preprocessing pass, depend on everything painted since.
	struct SPaintRecord
	{
		NMapOverlay::SPaintUndo before, after;
	};
	std::vector<SPaintRecord> paints;
	std::vector<int> appliedPaints;		// undo takes the back
	std::vector<int> undonePaints;		// redo takes the back; a new paint clears it
	// The edit log, by token (its index), the paints' stacks generalised: the
	// roads and rivers (04-05) and, after them, the bridges, fences and
	// entrenchments log their edits here. Undo takes the back of appliedEdits,
	// redo the back of undoneEdits, and a new edit clears undoneEdits. Cleared
	// whenever a map opens or closes, so a token is valid only for the map it
	// was handed out on.
	std::vector< std::unique_ptr<IEditRecord> > edits;
	std::vector<int> appliedEdits;
	std::vector<int> undoneEdits;
	// Roads (0) and rivers (1): the engine's nID of each record of the saved
	// list, by list position. The engine picks its own random nID for every
	// AddRoad and AddRiver (TerrainEditor.cpp), so the saved nID (the bridge's,
	// D-03) and the engine's differ after an edit; at open they are the file's,
	// read from the engine's own list, which the terrain loaded in file order.
	std::vector<int> vsoEngineIDs[2];
	// Deleted objects, by link ID, for BkEditorRestoreObject: the snapshot's
	// record and the working copy's, which differ in the frame index.
	struct STombstone
	{
		NMapOverlay::SDeletedObject snapshot, working;
		// Whether the engine held it. One it never held - outside the map, or a
		// span set aside from every bridge - goes back into the map alone.
		bool bPlaced;
		STombstone() : bPlaced( false ) {  }
	};
	std::unordered_map<int, STombstone> tombstones;
	// One above every link ID this session has handed out or deleted, so an add
	// never takes the ID of an object a later undo will restore.
	int nLinkIDFloor;
	// The engine's object layer: the map objects with visuals that the scene
	// draws and IScene::Pick finds. Made in BkEditorStart, deleted in
	// BkEditorStop.
	CEditorWorld *pWorld;
	// byLinkID the other way round, for turning a picked object into its link
	// ID. Rebuilt whenever byLinkID changes (UpdateSessionWorld).
	std::unordered_map<IRefCount*, int> linkByAI;
	bool bEngineStarted;
	bool bMapOpen;
	// Degrees of yaw offset from the game's own 45 (D-12), wrapped into
	// [0, 360) by BkEditorSetYaw. SetSessionCamera adds this to the constant
	// the game always places its mission camera at; 0 is the game's own view.
	float fYawOffsetDegrees;
	// D-29's squad-icon fallback (user-requested addition, 03-09 Task 4): a
	// single soldier with no icon.tga of its own borrows the icon.tga of a
	// squad that lists it as a member. Soldier name -> squad name, built once
	// per session (the whole object database is scanned to fill it) the
	// first time BkEditorObjectPicture needs it; empty and unbuilt until
	// then. Deterministic when more than one squad lists the same soldier:
	// the alphabetically first squad name wins.
	std::unordered_map<std::string, std::string> squadIconOwnerBySoldier;
	bool bSquadIconOwnerMapBuilt;
	SEditorSession() : nBridgeSpansInMap( 0 ), nBridgeSpansPlaced( 0 ), nLinkIDFloor( 0 ), pWorld( 0 ), bEngineStarted( false ), bMapOpen( false ), fYawOffsetDegrees( 0.0f ), bSquadIconOwnerMapBuilt( false ) {  }
};

// Reads pszPath into the session and builds the engine state the editor draws
// and edits through: shades, the AI editor, the terrain in the scene, and one
// engine object per placed map object. Returns false and leaves the reason in
// szMessage when the map cannot be read or the engine is not there; the session
// then keeps the map it had open. A throw while the new map is being built
// leaves it with no map open (bMapOpen false).
//
// The order is the MFC editor's (TemplateEditorFrame1.cpp:1657-1790), which is
// the order the engine expects - the AI editor is initialised before the
// terrain reaches the scene, not after.
bool OpenMapIntoSession( SEditorSession *pSession, const char *pszPath );

// Closes whatever map is open, without touching the mod or the object
// database it is about to change under it: the world's objects leave the
// scene, the AI editor is cleared, and every per-map table (byLinkID,
// unknownLinkIDs, futureBuildLinkIDs, the paint history, tombstones,
// linkByAI) is reset - the same map-closing steps OpenMapIntoSession takes
// before it builds a new map in, reused here for BkEditorSetMod (03-08),
// which must close the map before swapping the MOD storage out from under
// it. A no-op when no map is open.
void CloseSessionMap( SEditorSession *pSession );

// Writes the session's map to pszPath. Returns false and leaves the reason in
// szMessage.
bool SaveSessionMap( SEditorSession *pSession, const char *pszPath );

// What the engine holds for one object. Not the same as the map's record: a
// non-building is handed to the engine with player 0 while the map keeps its
// real owner, so these are the values a rollback has to restore.
struct SEngineObjectState
{
	CVec2 vCenter;
	WORD wDir;
	int nPlayer;								// -1 when the object's kind has no owner
	SEngineObjectState() : vCenter( VNULL2 ), wDir( 0 ), nPlayer( -1 ) {  }
};

// Why an object of this game type cannot be put on a map, or 0 if it can.
// The reason reads after the object's name.
const char* WhyNotAMapObject( int nGameType );

// Why an object of this game type is not offered by the palette (D-05), or 0.
// A trench piece, a bridge span and a fence only make sense inside their own
// entry, which the Entrenchment, Bridge and Fence tools draw, so the catalogue
// reports them not placeable and BkEditorAddObject refuses them naming the
// tool. This is NOT WhyNotAMapObject, which also guards PlaceOneObject and
// must keep accepting these types: a loaded map's spans, pieces and fences
// still load, draw and move. The M2 tools add their objects through their own
// session functions, never AddObjectToSession.
const char* WhyNotPlacedByPalette( int nGameType );

// Reads an object's engine state. Returns false when the engine does not hold
// the object at all.
bool ReadEngineObject( const SEditorSession &rSession, int nLinkID, SEngineObjectState *pOut );

// The edits, each applied to the snapshot and to the engine together. Every one
// returns false with the reason in szMessage and leaves the session exactly as
// it found it; pbRefused, where it is offered, tells a refusal - the map or the
// engine saying no - apart from something going wrong.
//
// The engine cannot be asked whether an edit took: CAIEditor::AddNewObject,
// MoveObject and TurnObject all end in an unconditional `return false`
// (AIEditorInternal.cpp:46, 126, 161) and report by what they leave behind. So
// each of these reads the engine back afterwards and compares, which is the
// only honest way to know.
bool AddObjectToSession( SEditorSession *pSession, const NMapOverlay::SAddObject &rAdd, int *pnLinkID );
bool PlaceObjectInSession( SEditorSession *pSession, int nLinkID, const CVec3 &vPos, int nDir, int nPlayer, bool *pbRefused );
bool DeleteObjectFromSession( SEditorSession *pSession, int nLinkID, bool *pbRefused );
// Puts a deleted object back from its tombstone: the same record, link ID and
// place in its list, and a new engine object where it stood. Refused when
// nothing with that link ID was deleted, or the ID is in use again.
bool RestoreObjectInSession( SEditorSession *pSession, int nLinkID, bool *pbRefused );
bool SetSessionDiplomacy( SEditorSession *pSession, int nPlayer, int nDiplomacy );

// Fills pOut with the object database's descriptors and pnCount with how many
// there are - always the database's count, not how many fitted. Returns false
// when the buffer was too small, which the caller can tell from the count.
bool ReadCatalogue( SEditorSession *pSession, BkEditorCatalogueEntry *pOut, int nCapacity, int *pnCount );

// Why an object of this type cannot be added to a map on its own, or "" if it
// can: a soldier goes on a map only inside a squad. The reason reads after the
// object's name and names a squad to place instead where the database has one.
// Only an add asks this - a lone soldier a map already holds is kept as read.
std::string WhyNotPlacedAlone( IObjectsDB *pObjectsDB, const SGDBObjectDesc &rDesc );

// Runs one world update, so the scene holds a visual for every engine object
// the last edit made, moved or removed, and rebuilds linkByAI from byLinkID.
// Every edit that changes an engine object calls it once it has succeeded.
void UpdateSessionWorld( SEditorSession *pSession );

// The object under a screen point, as a link ID. Returns false with the reason
// in szMessage; pbRefused tells "nothing pickable there" apart from a failure.
bool ObjectAt( SEditorSession *pSession, float sx, float sy, int *pnLinkID, bool *pbRefused );

// The camera, one frame, and the two conversions picking needs.
bool SetSessionCamera( SEditorSession *pSession, float wx, float wy );
bool DrawSessionFrame( SEditorSession *pSession );
bool ScreenToWorld( SEditorSession *pSession, float sx, float sy, float *pwx, float *pwy );
// The other direction: a world point (the scene's units) to the screen point
// it draws at right now, through the terrain's own height at that point -
// see BkEditorWorldToScreen.
bool WorldToScreen( SEditorSession *pSession, float wx, float wy, float *psx, float *psy );
// World units (the scene's) to map units (the file's and the AI's): the
// engine's AI2Vis the other way round. See BkEditorScreenToWorld.
void WorldToMap( float wx, float wy, float *pmx, float *pmy );

// True when every cell's tile is one the map's tileset has a terrain type for.
// False with the reason in szMessage; *pbBadTile says the caller named a tile
// the tileset lacks, rather than the engine having no terrain or tileset.
bool PaintTilesInTileset( SEditorSession *pSession, const std::vector<NMapOverlay::SPaintCell> &rCells, bool *pbBadTile );
// Every tile the map's tileset has a terrain type for, ascending, into pOut;
// pnCount is always the tileset's count, not how many fitted. False with the
// reason in szMessage when no map is open, there is no terrain, or the buffer
// was too short.
bool TilesetTiles( SEditorSession *pSession, unsigned char *pOut, int nCapacity, int *pnCount );
// The tile the engine holds at a cell. False with the reason in szMessage when
// no map is open or the cell is off the map.
bool EngineTile( SEditorSession *pSession, int nX, int nY, BYTE *pTile );
// Paints cells into the map and pushes the region they touched into the engine.
// Returns false with the reason in szMessage. pnToken names the paint for undo
// and redo; it is -1 when there was nothing to paint.
bool PaintIntoSession( SEditorSession *pSession, const std::vector<NMapOverlay::SPaintCell> &rCells, int *pnToken );
// Undo takes the newest applied paint, redo the most recently undone; any other
// token is refused. Both put the recorded region back raw, in both copies and
// the engine.
bool UndoPaintInSession( SEditorSession *pSession, int nToken, bool *pbRefused );
bool RedoPaintInSession( SEditorSession *pSession, int nToken, bool *pbRefused );

// Compares the engine's terrain against the copy that will be saved, in tiles
// and patch crosses. Returns false and names the first difference in szMessage.
// For the engine tier; the editor has no reason to call it.
bool TerrainMatchesEngine( SEditorSession *pSession );

// Compares what the world draws against what the session holds: every unit
// and squad in the world belongs to an engine object the session knows (a
// soldier through his squad), and every engine object the session knows has its map
// objects in the world (a squad through its soldiers). Returns false and names
// the first difference in szMessage. For the engine tier.
bool WorldMatchesSession( SEditorSession *pSession );

// The tile a world point falls in, through the engine's own conversion.
bool WorldToTile( SEditorSession *pSession, float wx, float wy, int *pnX, int *pnY );
// The same for an AI tile (half a world cell), ITerrainEditor::GetAITileIndex
// as the MFC Fences tab calls it (04-07): the index is written even for a point
// off the map, which is then refused.
bool WorldToAITile( SEditorSession *pSession, float wx, float wy, int *pnX, int *pnY );

// Reads the object's current record out of the snapshot, so a caller changing
// one of its three editable fields can leave the other two alone.
const SMapObjectInfo* FindSnapshotObject( const SEditorSession &rSession, int nLinkID );

// Fills pOut with one BkEditorObjectRecord per object in the snapshot, objects
// before scenarioObjects, in file order, and pnCount with the total - always
// the total, not how many fitted. Returns false when the buffer was too small.
bool ReadSessionObjects( SEditorSession *pSession, BkEditorObjectRecord *pOut, int nCapacity, int *pnCount );

// An object's script ID in the snapshot and the working copy together, the
// engine untouched (C7). False with the reason in szMessage and nothing
// changed; pbRefused tells a refusal (a value outside -1..32000, an unknown or
// shared link ID, link ID 0) from a failure.
bool SetSessionObjectScriptID( SEditorSession *pSession, int nLinkID, int nScriptID, bool *pbRefused );

// The map's own sound list - CMapInfo::sounds.sounds (SMapSoundInfo), the
// field that is actually serialised (CMapInfo::operator&, tag 17 /
// "MapSounds"). See BkEditorSounds' own comment (bridge.h) for why this is
// not CMapInfo::soundsList. pnCount is always the total, not how many
// fitted, matching ReadSessionObjects and ReadCatalogue.
bool ReadSessionSounds( SEditorSession *pSession, BkEditorSoundRecord *pOut, int nCapacity, int *pnCount );

// Adds/replaces/removes one sound in the snapshot and the working copy
// together; the engine is never touched (a sound only reaches the engine
// when a mission starts InitMapSounds, which this bridge's headless session
// never does). Each returns false with the reason in szMessage and leaves
// the session exactly as it found it; pbRefused tells a refusal (the
// record's own rules said no) apart from a caller bug bridge.cpp already
// turned away as BK_EDITOR_BAD_ARGUMENT before calling here.
bool AddSoundToSession( SEditorSession *pSession, int nIndex, const BkEditorSoundRecord &rRecord, bool *pbRefused );
bool SetSoundInSession( SEditorSession *pSession, int nIndex, const BkEditorSoundRecord &rRecord, bool *pbRefused );
bool DeleteSoundFromSession( SEditorSession *pSession, int nIndex, bool *pbRefused );

// The M2 record functions (session_records.cpp). Each edits the snapshot and
// the working copy together through NMapRecords, leaves the engine alone unless
// its collection feeds it, and returns false with the reason in szMessage and
// the session exactly as it found it; pbRefused tells a refusal apart from a
// failure, as for the sounds.
//
// The camera anchors, world units. A vector of more than 32 entries is
// readable-refused and setting is refused: the file keeps it byte-exact.
bool ReadSessionCameraAnchors( SEditorSession *pSession, BkEditorCameraAnchorRecord *pOut, bool *pbRefused );
bool SetSessionCameraAnchors( SEditorSession *pSession, const BkEditorCameraAnchorRecord &rAnchors, bool *pbRefused );
// Reinforcement groups (04-09, D-16), through NMapRecords on both copies. The
// two reads answer the total in *pnCount and return false when the buffer was
// too short (nothing written past it); ReadSessionGroup also returns false
// with *pbRefused for a group that is not there. SetSessionGroup applies the
// script-ID rules (0..32000, once each, an ID the group holds is exempt).
bool ReadSessionGroupIDs( SEditorSession *pSession, int *pOut, int nCapacity, int *pnCount );
bool ReadSessionGroup( SEditorSession *pSession, int nID, int *pOut, int nCapacity, int *pnCount, bool *pbRefused );
bool SetSessionGroup( SEditorSession *pSession, int nID, const int *pIDs, int nCount, bool *pbRefused );
bool DeleteSessionGroup( SEditorSession *pSession, int nID, bool *pbRefused );
int FirstFreeGroupIDInSession( SEditorSession *pSession, int nFrom );
// The terrain height at a world point, through CVSOBuilder::UpdateZ on the
// working copy's altitudes. False with the reason in szMessage off the map.
bool GroundHeightInSession( SEditorSession *pSession, float fX, float fY, float *pfZ );

// The engine's terrain editor for the open map, or null (session.cpp).
ITerrainEditor* EngineTerrain();

// The edit log (session_vso.cpp). LogEdit takes the record, appends it,
// clears the redo stack and hands out its token. Undo takes only the newest
// applied edit, redo only the most recently undone: any other token is a
// refusal. ClearEditLog forgets every edit (a map opened or closed).
int LogEdit( SEditorSession *pSession, IEditRecord *pRecord );
bool UndoEditInSession( SEditorSession *pSession, int nToken, bool *pbRefused );
bool RedoEditInSession( SEditorSession *pSession, int nToken, bool *pbRefused );
void ClearEditLog( SEditorSession *pSession );

// Roads (kind 0, roads3) and rivers (kind 1), session_vso.cpp. Points are
// world (Vis) units, widths world units, opacity 0..1.
//
// ResetVsoEngineIDs reads the engine's nIDs of both lists after the terrain
// has loaded (list positions match the file's then); an open calls it, a
// close empties both.
void ResetVsoEngineIDs( SEditorSession *pSession );
// How many records the saved list holds, -1 for a kind that is neither.
int VsoCount( const SEditorSession &rSession, int nKind );
// The record at nIndex of the saved list, or null.
const SVectorStripeObject* SessionVso( const SEditorSession &rSession, int nKind, int nIndex );
// The season's descriptors of a kind (the map's season folder + Roads3D\ or
// Rivers\, as the MFC editor lists them): bare names, no extension, sorted.
bool VsoDescriptors( SEditorSession *pSession, int nKind, std::vector<std::string> *pNames );
// Adds a road or river drawn through the control points: derived once here
// (CVSOBuilder), put into both copies and the engine, logged. pnIndex is where
// it landed in the saved list. False with the reason in szMessage; pbRefused
// tells a refusal (an unknown descriptor, a point off the map, too short)
// from a failure. A refusal changes nothing.
bool AddVsoToSession( SEditorSession *pSession, int nKind, const std::string &szDesc, const std::vector<CVec3> &rPoints,
                      float fWidthTiles, float fOpacity, int *pnToken, int *pnIndex, bool *pbRefused );
// Edits of an existing road or river, each one logged edit (session_vso.cpp).
// A move names every control point (world units) and resamples keeping the key
// points' widths and opacities; a width (world units) or an opacity (0..1) is
// set at key point nKey in a mode (0 that point, 1 it and every later one, 2
// every point); an insert adds the midpoint after control point nControl
// (before it when it is the last) and a delete removes it - refused while only
// 2 remain. Each refuses a result the loaders could not take.
bool MoveVsoPointsInSession( SEditorSession *pSession, int nKind, int nIndex, const std::vector<CVec3> &rPoints, int *pnToken, bool *pbRefused );
bool SetVsoWidthInSession( SEditorSession *pSession, int nKind, int nIndex, int nKey, float fWidth, int nMode, int *pnToken, bool *pbRefused );
bool SetVsoOpacityInSession( SEditorSession *pSession, int nKind, int nIndex, int nKey, float fOpacity, int nMode, int *pnToken, bool *pbRefused );
bool InsertVsoPointInSession( SEditorSession *pSession, int nKind, int nIndex, int nControl, int *pnToken, bool *pbRefused );
bool DeleteVsoPointInSession( SEditorSession *pSession, int nKind, int nIndex, int nControl, int *pnToken, bool *pbRefused );
// The road or river under a world point (CMapInfo::TerrainHitTest, roads then
// rivers); nCycle skips that many earlier hits, wrapping. Refused when nothing
// is there.
bool PickVsoInSession( SEditorSession *pSession, float fX, float fY, int nCycle, int *pnKind, int *pnIndex, bool *pbRefused );
// Deletes the road or river at nIndex (the whole record), logged; for a river
// the AI's tiles are unlocked first (DeleteRiver with the record as saved).
bool DeleteVsoFromSession( SEditorSession *pSession, int nKind, int nIndex, int *pnToken, bool *pbRefused );
// Every saved record against the engine's, found through vsoEngineIDs (never
// by nID): points, control points, widths and opacities. False naming the
// first difference in szMessage.
bool VsoMatchesEngine( SEditorSession *pSession );

// Places one object in the engine, as CTemplateEditorFrame::AddObjectByAI
// does (session.cpp): the engine object, or null when the engine would not
// take it (outside the map, or a type it cannot place).
IRefCount* PlaceOneObject( const SMapObjectInfo &rObject, const SGDBObjectDesc *pDesc, IAIEditor *pAIEditor );
// One bridge's spans into the engine, in the order rLinkIDs lists them, from
// the working-copy records in rSpans (session.cpp): byLinkID gets each span
// the engine took, futureBuildLinkIDs each span stored with negative HP.
// Returns how many of rLinkIDs the engine took.
int BuildOneBridge( SEditorSession *pSession, const std::vector<int> &rLinkIDs, const std::vector<SMapObjectInfo> &rSpans );

// Bridges (session_groups.cpp, 04-06, D-10..D-12). A bridge is one entry of
// CMapInfo::bridges (the link IDs of its spans, in order) plus its span
// objects; every edit here changes both, in both copies and the engine, all or
// nothing, and logs itself in the edit log (SGroupEdit). Drags are world (Vis)
// units, span positions map (AI) units.
//
// A bridge type as the Bridge tool lists it: its name, direction (0 vertical,
// 1 horizontal), whether its rotated variant (BridgePartnerName) is in the
// object database, and whether it may be built during play (a WoodenBig_Heavy_
// type, RoadDrawState.cpp:1253).
struct SBridgeDescriptorInfo
{
	std::string szName;
	int nDirection;
	bool bHasPartner;
	bool bBuildDuringPlay;
	SBridgeDescriptorInfo() : nDirection( 0 ), bHasPartner( false ), bBuildDuringPlay( false ) {  }
};
// Every SGVOGT_BRIDGE type of the object database, sorted by name.
bool BridgeDescriptorsInSession( SEditorSession *pSession, std::vector<SBridgeDescriptorInfo> *pOut );
// A bridge type's plan inputs from its SBridgeRPGStats: the direction, the
// first line span's length (world units) and the origin of the begin span
// chosen with the seeded index (seed 0: the first begin). Refused (false, the
// reason in szMessage) for a name that is not a bridge type and for stats
// whose states, begins, lines or ends are empty (Pitfall 5: the index helpers
// divide by those lists' sizes).
bool BridgePlanInputFor( SEditorSession *pSession, const std::string &szDesc, NMapGeometry::SBridgePlanInput *pInput );
// The spans a drag would place (NMapGeometry::PlanBridge), changing nothing.
bool PlanBridgeInSession( SEditorSession *pSession, const std::string &szDesc, const CVec2 &vFirst, const CVec2 &vLast,
                          std::vector<NMapGeometry::SPlannedPiece> *pSpans, bool *pbRefused );
// Draws a bridge: the planned spans become objects (the snapshot holds the
// packed type, the working copy and the engine a seeded concrete index; HP 1,
// no script ID, player 0, direction 0, fresh link IDs) and a new bridges
// entry at the end of the list, as one logged edit. pnIndex is the entry's
// index. Refused, changing nothing, for a bad type, a drag along the wrong
// axis, and a span the engine will not place (off the map).
bool DrawBridgeInSession( SEditorSession *pSession, const std::string &szDesc, const CVec2 &vFirst, const CVec2 &vLast,
                          int *pnToken, int *pnIndex, bool *pbRefused );
// One bridges entry as the tools see it: the type (its first span's name),
// the span count, the box of the spans' positions (map units; a span the map
// does not hold is left out) and whether it is built during play (a span with
// negative HP in the saved map).
struct SBridgeInfo
{
	std::string szDesc;
	int nSpans;
	CVec2 vMin, vMax;
	bool bBuiltDuringPlay;
	SBridgeInfo() : nSpans( 0 ), vMin( VNULL2 ), vMax( VNULL2 ), bBuiltDuringPlay( false ) {  }
};
void ReadSessionBridges( const SEditorSession &rSession, std::vector<SBridgeInfo> *pOut );
// The bridge or entrenchment under a screen point: *pnKind 1 and the bridges
// index, or 2 and the entrenchments index. The scene's own pick and linkByAI,
// as ObjectAt, but a span or trench piece is what it looks for (ObjectAt
// passes them over, so the M1 Select tool never takes one). Refused when
// neither is there.
bool PickGroupInSession( SEditorSession *pSession, float sx, float sy, int *pnKind, int *pnIndex, bool *pbRefused );
// Deletes bridge nIndex whole: the entry, then every span (both copies, the
// engine), one logged edit whose undo puts the spans back and then the entry
// at the same index. Refused for a bridge with a span the editor could not put
// back (a type the database does not know, a span the engine never held, a
// link ID the map shares), which is kept as read.
bool DeleteBridgeFromSession( SEditorSession *pSession, int nIndex, int *pnToken, bool *pbRefused );
// Rotates bridge nIndex (D-11): its type's _01/_02 partner, planned about the
// old centre (the mean of its first and last spans) along the partner's axis
// with the same span count, swapped in at the same index as one logged edit,
// all or nothing; built during play carries over. Refused for a type with no
// partner in the object database ("no rotated variant of ..."), a span of the
// new bridge the engine will not place (off the map), and a bridge whose
// spans the editor could not put back.
bool RotateBridgeInSession( SEditorSession *pSession, int nIndex, int *pnToken, bool *pbRefused );
// D-12: toggles bridge nIndex between intact and built during play - the
// snapshot's HP of every span 1 or -1 (the working copy and the engine keep
// 1), futureBuildLinkIDs and the mark with it - as one logged edit. Refused
// unless its spans are a WoodenBig_Heavy_ type (RoadDrawState.cpp:1253).
bool ToggleBridgeBuildInSession( SEditorSession *pSession, int nIndex, int *pnToken, bool *pbRefused );
// C1: the built-during-play mark, as the MFC editor draws it
// (TemplateEditorFrame1.cpp:1972): SetSpecular( 0xFF0000FF ) on every span in
// futureBuildLinkIDs, 0 on every other span of every bridge. UpdateSessionWorld
// calls it after each world update, so an open, an undo, a redo and a rotate
// all show it.
void ApplyBridgeMarks( SEditorSession *pSession );

// Fences (session_groups.cpp, 04-07, D-14): a run of ordinary fence objects
// placed by one drag, no bridges-style entry. One logged edit (the same
// SGroupEdit, records only), all or nothing; afterwards each fence is an
// object like any other (move, delete).
//
// A fence type as the Fence tool lists it: its name.
struct SFenceDescriptorInfo
{
	std::string szName;
};
// Every SGVOGT_FENCE type of the object database with stats, sorted by name.
bool FenceDescriptorsInSession( SEditorSession *pSession, std::vector<SFenceDescriptorInfo> *pOut );
// A fence type's plan inputs: the origin of the centre segment (seeded, the
// first) of each of the four directions and the map's extent in AI tiles.
// Refused (false, the reason in szMessage) for a name that is not a fence type
// and for stats with fewer than four directions or a direction with no centre
// segment (the index helpers divide by those lists' sizes, T-04-07-01).
bool FencePlanInputFor( SEditorSession *pSession, const std::string &szDesc, NMapGeometry::SFencePlanInput *pInput );
// The fences a drag (world units) would place (NMapGeometry::PlanFences over
// the engine's AI tiles), changing nothing. pbRefused as PlanBridgeInSession.
bool PlanFencesInSession( SEditorSession *pSession, const std::string &szDesc, const CVec2 &vFirst, const CVec2 &vLast, bool bCtrl,
                          std::vector<NMapGeometry::SPlannedPiece> *pFences, bool *pbRefused );
// Places the planned fences: objects (the snapshot holds the packed type, the
// working copy and the engine the seeded first centre segment; HP 1, player 0,
// script ID -1, fresh link IDs) as one logged edit. Refused, changing nothing,
// for a bad type, a run off the map and a fence the engine will not place.
bool DrawFencesInSession( SEditorSession *pSession, const std::string &szDesc, const CVec2 &vFirst, const CVec2 &vLast, bool bCtrl,
                          int *pnToken, bool *pbRefused );

// Entrenchments (session_groups.cpp, 04-08, D-13): one entry of
// CMapInfo::entrenchments (sections of piece link IDs) plus its piece objects,
// drawn and deleted as a whole through the same SGroupEdit as a bridge, all or
// nothing; the entry goes before its pieces and comes back after them. No
// engine grouping call: the MFC editor makes none, the game groups the pieces
// in LoadEntrenchments.
//
// The builder's inputs from the "Entrenchment" stats (the MFC editor's fixed
// descriptor): the first line and first arc segment's GetVisAABBHalfSize().x
// * 2. Refused for a database without the type and for stats whose line,
// fireplace, terminator or arc list is empty or names a missing segment
// (Pitfall 5).
bool EntrenchmentPlanInputFor( SEditorSession *pSession, NMapGeometry::STrenchPlanInput *pInput );
// The entrenchment clicks (world units) would commit
// (NMapGeometry::PlanEntrenchment), changing nothing. pbRefused as
// PlanBridgeInSession; more than 256 points is refused.
bool PlanEntrenchmentInSession( SEditorSession *pSession, const std::vector<CVec2> &rPoints, NMapGeometry::STrenchPlan *pPlan, bool *pbRefused );
// Draws the entrenchment: its pieces become objects (the snapshot holds the
// packed piece type, the working copy and the engine a seeded concrete
// segment; HP 1, script ID -1, nPlayer, fresh link IDs in the plan's order) and
// a new entrenchments entry at the end of the list, as one logged edit.
// pnIndex is the entry's index. Refused, changing nothing, for the plan's
// refusals and a piece the engine will not place (off the map).
bool DrawEntrenchmentInSession( SEditorSession *pSession, const std::vector<CVec2> &rPoints, int nPlayer, int *pnToken, int *pnIndex, bool *pbRefused );
// One entrenchments entry as the tools see it: its piece and section counts,
// the player of its first piece the map holds and the box of its pieces'
// positions (map units; a piece the map does not hold is left out).
struct SEntrenchmentSummary
{
	int nPieces, nSections, nPlayer;
	CVec2 vMin, vMax;
	SEntrenchmentSummary() : nPieces( 0 ), nSections( 0 ), nPlayer( 0 ), vMin( VNULL2 ), vMax( VNULL2 ) {  }
};
void ReadSessionEntrenchments( const SEditorSession &rSession, std::vector<SEntrenchmentSummary> *pOut );
// Deletes entrenchment nIndex whole: the entry, then every piece (both copies,
// the engine), one logged edit whose undo puts the pieces back and then the
// entry at the same index. Refused, as a bridge is, for an entrenchment with a
// piece the editor could not put back; it is kept as read.
bool DeleteEntrenchmentFromSession( SEditorSession *pSession, int nIndex, int *pnToken, bool *pbRefused );

#endif // __EDITOR_BRIDGE_SESSION_H__
