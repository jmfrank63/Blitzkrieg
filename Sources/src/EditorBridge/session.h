#ifndef __EDITOR_BRIDGE_SESSION_H__
#define __EDITOR_BRIDGE_SESSION_H__
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "../RandomMapGen/MapInfo_Types.h"
#include "../MapFile/MapOverlay.h"
#include "bridge.h"

class CEditorWorld;
struct IObjectsDB;
struct SGDBObjectDesc;
interface ITerrainEditor;
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

// Reads the object's current record out of the snapshot, so a caller changing
// one of its three editable fields can leave the other two alone.
const SMapObjectInfo* FindSnapshotObject( const SEditorSession &rSession, int nLinkID );

// Fills pOut with one BkEditorObjectRecord per object in the snapshot, objects
// before scenarioObjects, in file order, and pnCount with the total - always
// the total, not how many fitted. Returns false when the buffer was too small.
bool ReadSessionObjects( SEditorSession *pSession, BkEditorObjectRecord *pOut, int nCapacity, int *pnCount );

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
// Every saved record against the engine's, found through vsoEngineIDs (never
// by nID): points, control points, widths and opacities. False naming the
// first difference in szMessage.
bool VsoMatchesEngine( SEditorSession *pSession );

#endif // __EDITOR_BRIDGE_SESSION_H__
