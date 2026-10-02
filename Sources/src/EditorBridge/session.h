#ifndef __EDITOR_BRIDGE_SESSION_H__
#define __EDITOR_BRIDGE_SESSION_H__
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#include "../RandomMapGen/MapInfo_Types.h"
#include "../RandomMapGen/VA_Types.h"
#include "../AILogic/UnitCreation.h"
#include "../MapFile/MapOverlay.h"
#include "../MapFile/MapGeometry.h"
#include "bridge.h"

class CEditorWorld;
struct IObjectsDB;
struct SGDBObjectDesc;
interface ITerrainEditor;
interface IAIEditor;
struct SEditorSession;

// The Layers menu's starting state (session_layers.cpp): the MFC editor's own
// (TemplateEditorFrame1.cpp:379) - see BkEditorLayer.
unsigned LayerDefaultBits();

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
	// The user RMG root mounted over the data (session_rmg.cpp MountRmgRoot,
	// D-09): the root it was mounted from, in the engine's own spelling, and
	// the storage itself - kept so a write can ask whether a file is the
	// user's own (in this storage) or shipped (only in a layer below it).
	std::string szRmgMountedRoot;
	CPtr<IDataStorage> pRmgStorage;
	// The party table (partys.xml), read once per session for the flag swap
	// (M3, D-26): a flag re-owned to player N becomes Flag_<the party's
	// general side>, exactly the MFC properties' own swap. Empty until the
	// first flag edit needs it; a partys.xml the storage does not have keeps
	// it empty and every swap answers neutral.
	std::vector<CUnitCreation::SPartyDependentInfo> partyTable;
	bool bPartyTableRead;
	// The reinforcement groups as the file had them when the map was opened
	// (04-09). A group put may keep any script ID - and as many copies of it -
	// that the file's own group held, however odd (a duplicate, a value out of
	// range), so an undo can put back what an edit took out; only what an edit
	// ADDS is held to 0..32000, once.
	std::unordered_map< int, std::vector<int> > openedGroups;
	// The script file the map named when it was opened (04-10, D-20). A value
	// read from a file is kept verbatim until the user changes it - it may be
	// a path or carry ".lua" - so a put may always bring it back (an undo),
	// while a value NEW to the map must be a bare name.

	std::string szScriptFileAtOpen;
	// How many script areas held each name when the map was opened (04-10, D-21).
	// A file may name two areas alike; an edit may not make a NEW duplicate, but an
	// undo must be able to put an area of the file back beside its twin.
	std::unordered_map<std::string, int> openedAreaNames;
	// The script areas themselves as the file held them (WR-A04): one put back
	// exactly - an undo of a delete or an edit - skips the size and centre rules,
	// so a file's own odd area (off the map, a negative size) survives its undo.
	std::vector<SScriptArea> openedAreas;
	// The start commands and reserve positions the file held when the map was
	// opened (04-11, D-17, D-18). An add - the undo of a delete - may put back
	// exactly one of these whatever it names, so the rules an edit is held to do
	// not make an undo of a file's own odd record fail as a drift.
	std::vector<SAIStartCommand> openedStartCommands;
	std::vector<SBattlePosition> openedReservePositions;
	// The AI general's sides as the file had them when the map was opened (04-12,
	// D-19): a parcel or a mobile script ID of these is always accepted back by a
	// put, however odd, so an undo of an edit of a file's own data cannot fail.
	std::vector<SAIGeneralSideInfo> openedAISides;
	// The camera anchors as the file had them when the map was opened (WR-B03):
	// a slot put back to exactly the file's own value - an undo of an edit of a
	// legacy map's off-map anchor - is not held to the on-the-map rule.
	CVec3 vOpenedNeutralAnchor;
	std::vector<CVec3> openedPlayerAnchors;
	// The unit-creation names the file held when the map was opened (M3, D-30):
	// a put may always bring one back, however odd, so an undo of an edit of a
	// file's own data cannot fail the way a new value is checked.
	std::set<std::string> openedUCParties, openedUCAircraft, openedUCSquads;
	// "Hide checked" (04-09, D-16): the script IDs the view holds back, sorted
	// and unique, and the link IDs of the objects-list entries they name that
	// are hidden now (their visuals at opacity 0, and picking skips them). A
	// view setting: never saved, never in any history, forgotten with the map.
	std::vector<int> hiddenScriptIDs;
	std::vector<int> hiddenLinkIDs;
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
		// The host's passengers (M3, D-27): the records whose nLinkWith named
		// this object go with it - deleted first, each with its own tombstone,
		// and restored after the host (their links point at it), last deleted
		// first. Empty for a passengerless object.
		std::vector<STombstone> passengers;
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
	// M3 heights (D-18): the profile pattern and the level mask the stroke
	// machine applies, rebuilt only when the brush or the speed moves, and
	// the stroke-start cache (the click modes' frozen targets, taken on the
	// step that carries bStrokeStart). VNULL3 in vClickRefStroke means no
	// stroke is open.
	int nHeightsBrush;
	float fHeightsSpeed;
	SVAPattern heightsPattern;
	SVAPattern heightsLevelMask;
	bool bHeightsPatternValid;
	CVec3 vClickRefStroke;
	float fClickTileHeight;
	bool bClickTileValid;
	float fClickAverageHeight;
	// The terrain-mode toggles (M3, D-20): Instant Update off by default,
	// Fit To Grid ON by default, exactly the MFC's own initial states
	// (TemplateEditorFrame1.cpp:5249/5287).
	bool bInstantUpdate;
	bool bFitToGrid;
	// The Layers menu (M3, D-32, session_layers.cpp): the state each layer was
	// last asked for, one bit per BkEditorLayer, and the fire-range mode with
	// the AI group that shows it (-1 for none). Renderer state: never in the
	// map, never in the history. Re-applied to the engine after every map the
	// session builds (InstallMapInSession), which is the desync fix.
	unsigned nLayerBits;
	int nFireRangeMode;
	std::string szFireRangeFilter;
	int nFireRangeGroup;
	SEditorSession() : nBridgeSpansInMap( 0 ), nBridgeSpansPlaced( 0 ), nLinkIDFloor( 0 ), pWorld( 0 ), bEngineStarted( false ), bMapOpen( false ), fYawOffsetDegrees( 0.0f ), bSquadIconOwnerMapBuilt( false ),
									 bPartyTableRead( false ), nHeightsBrush( 0 ), fHeightsSpeed( 0.0f ), bHeightsPatternValid( false ), vClickRefStroke( VNULL3 ), fClickTileHeight( 0.0f ), bClickTileValid( false ), fClickAverageHeight( 0.0f ),
									 bInstantUpdate( false ), bFitToGrid( true ), nLayerBits( LayerDefaultBits() ), nFireRangeMode( 0 ), nFireRangeGroup( -1 ) {  }
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

// The open path's own tail (session.cpp): reset of every per-map table, the
// working copy, the engine rebuilt from it, the camera on the middle. Shared
// with NewMapInSession; pszPath is what the terrain loader names its sidecar
// files after.
bool InstallMapInSession( SEditorSession *pSession, const CMapInfo &read, const char *pszPath );
// File > New (M3, D-23): CMapInfo::Create of the size (patches, 1..32 per
// axis) and the season (the dialog's 0..3), every tile the season's most
// common tile, zero altitudes with the season's shades, the mod stamp named
// by the caller (the active mod's name/version, empty for none) - then the
// same install the open path uses, ending never-saved (the session holds no
// path). False with the reason in szMessage; a failure before the install
// leaves the previous map exactly as it was, one during it leaves the
// session with no map open, exactly an open's own failure rule.
bool NewMapInSession( SEditorSession *pSession, int nSizeX, int nSizeY, int nSeason, const char *pszName,
                      const std::string &rszModName, const std::string &rszModVersion );

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

// The batch move (M3, D-25, session.cpp): every member of `pnLinkIDs` moved
// by one (fDx, fDy) delta in MAP units, as ONE edit of the log - `pnToken`
// names it for undoEdit/redoEdit. One bad member (unknown or shared link ID,
// an unknown type, a destination off the terrain) refuses the whole move and
// nothing changes; so does an engine refusal part-way, which puts the members
// that already moved back first.
bool MoveObjectsInSession( SEditorSession *pSession, const int *pnLinkIDs, int nCount, float fDx, float fDy, bool *pbRefused, int *pnToken );

// The multi-selection picks (M3, D-25, session.cpp): the link IDs of every
// pickable object in a screen rectangle (the rubber band; the scene's own
// rectangle pick) and in a rectangle of tiles (the Ctrl band; the engine's own
// GetTileIndex of each record's drawn position decides). A soldier answers his
// squad's link ID; bridges, entrenchments and objects held back by Hide
// checked answer nothing. Both are two-pass reads: *pnCount is always the
// total, and a buffer too short is refused with nothing written past capacity.
bool PickObjectsInSession( SEditorSession *pSession, float fSx0, float fSy0, float fSx1, float fSy1, int *pnOut, int nCapacity, int *pnCount, bool *pbRefused );
bool PickObjectsInTilesInSession( SEditorSession *pSession, int nTx0, int nTy0, int nTx1, int nTy1, int *pnOut, int nCapacity, int *pnCount, bool *pbRefused );

// The properties' fields, links and the flag swap (M3, D-26/D-27, session.cpp).
//
// SetObjectFieldsInSession applies the masked fields of `edit` to ONE object's
// record (both copies, then the engine re-placed) as ONE edit of the log -
// one deactivate commit is one undo step. Mask bits: 1 player (the flag swap:
// a FLAG re-owned becomes Flag_<the map's unit-creation party's general side>,
// partys.xml naming the general side, "neutral" when anything is unknown),
// 2 hp (finite, the record's own 0..1), 4 angle (DEGREES, the MFC properties'
// unit, turned into the record's direction with the MFC's own formula), and
// 8 formation (the squad record's frame index; refused for any other kind -
// a unit record's frame index is its segment index, never a formation).
// A refusal changes nothing.
bool SetObjectFieldsInSession( SEditorSession *pSession, int nLinkID, const BkEditorObjectFieldsEdit *pEdit, bool *pbRefused, int *pnToken );

// CheckForInserting's rules (ObjectPlacerState.cpp:1325-1424) answered as a
// question about ONE passenger and ONE host: *pnType is 0 garrison, 1 train
// coupling, 2 tow; refused naming the rule when none holds. Infantry only as
// passengers; a building needs its stats and a free slot (slots + rest +
// medical); a trench piece takes infantry (the MFC's own checks are commented
// out there); a vehicle needs an entrance point and passenger room; a tractor
// or carrier tows an artillery gun with crew points it out-pulls; train cars
// couple with train cars.
bool CanLinkInSession( SEditorSession *pSession, int nSource, int nTarget, int *pnType, bool *pbRefused );

// The drop's link (D-27): the passenger record's nLinkWith becomes the host's
// link ID on both copies (the engine's garrison follows when the map loads,
// exactly the MFC editor's own save/load route), and a garrison moves the
// passenger beside the host (the MFC's GetCenter - 30, +30). ONE edit of the
// log. Refused with CanLink's reason, changing nothing.
bool SetLinkInSession( SEditorSession *pSession, int nSource, int nTarget, bool *pbRefused, int *pnToken );

// The properties' units list unlink (D-27): the record's nLinkWith back to 0
// (a palette-placed object is linked with nothing), both copies, the engine
// re-placed, ONE edit of the log. An already-unlinked object answers OK with
// no token.
bool UnlinkInSession( SEditorSession *pSession, int nLinkID, bool *pbRefused, int *pnToken );

// The Damage tool's hit (M3, D-29, session.cpp): `eMode` 0 damage, 1 heal,
// 2 repair to full; `fDelta` is the tool's percentage/100. The record's fHP
// moves by the MFC MapToolState's own clamps (SEditorMApObject's 0..1, and a
// floor of 0.01 for a technics or a human object - units and squads; anything
// else may reach 0), the engine's live object takes the same share of its
// fMaxHP through IAIEditor::DamageObject, and ONE edit of the log carries the
// whole record before and after. A record with no engine object of its own
// or a missing stats pointer is REFUSED - the MFC's unguarded FindByVis and
// pTmp->pRPG dereferences are NOT copied - and so is a record the editor
// cannot edit. Nothing changes on a refusal.
bool DamageObjectInSession( SEditorSession *pSession, int nLinkID, float fDelta, int eMode, bool *pbRefused, int *pnToken );

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
// Players and the Unit Creation Info (M3, D-30, session_records.cpp). The two
// player edits are ONE edit of the log each (SPlayersEdit lives in
// session_records.cpp): the diplomacies, the unit creation, the camera anchors
// and every re-owned object, put back raw. The unit-creation put is the same
// exact put the other records have (the entry and the vector's size), checked
// like MutableValidate; the choices are the lists its combos offer (0 parties,
// 1 aircraft, 2 paratroop squads).
bool AddPlayerToSession( SEditorSession *pSession, int nSide, bool *pbRefused, int *pnToken );
bool DeletePlayerFromSession( SEditorSession *pSession, int nPlayer, bool *pbRefused, int *pnToken );
bool ReadSessionUnitCreation( SEditorSession *pSession, int nPlayer, BkEditorUnitCreationRecord *pOut, bool *pbRefused );
bool SetSessionUnitCreation( SEditorSession *pSession, int nPlayer, const BkEditorUnitCreationRecord &rRecord, bool *pbRefused );
bool ListUnitCreationChoices( SEditorSession *pSession, int nKind, std::vector<std::string> *pNames );
// The party table (partys.xml, read once per session) and the flag type name a
// player's flags carry, session.cpp: what the properties' flag swap uses, and a
// player edit re-owning a flag after it.
bool ReadPartyTable( SEditorSession *pSession );
std::string FlagPartyName( SEditorSession *pSession, int nPlayer );
bool ReadSessionCameraAnchors( SEditorSession *pSession, BkEditorCameraAnchorRecord *pOut, bool *pbRefused );
bool SetSessionCameraAnchors( SEditorSession *pSession, const BkEditorCameraAnchorRecord &rAnchors, bool *pbRefused );
// The script file name (04-10, D-20). The read is refused for a value the record
// cannot hold (64 characters or more); the set accepts a bare name, None, or the
// value the file held at open (szScriptFileAtOpen).
bool ReadSessionScriptFile( SEditorSession *pSession, BkEditorScriptFileRecord *pOut, bool *pbRefused );
bool SetSessionScriptFile( SEditorSession *pSession, const char *pszName, bool *pbRefused );
// Script areas (04-10, D-21), AI units, through NMapRecords on both copies, the
// engine untouched. The read answers the total in *pnCount and returns false
// when the buffer was too short (nothing written past it); with *pbRefused for a
// map whose area names do not fit. Add, Set and Delete follow the rules
// bridge.h documents; nIndex -1 appends (Add only).
bool ReadSessionScriptAreas( SEditorSession *pSession, BkEditorScriptAreaRecord *pOut, int nCapacity, int *pnCount, bool *pbRefused );
bool AddScriptAreaToSession( SEditorSession *pSession, int nIndex, const BkEditorScriptAreaRecord &rRecord, bool *pbRefused );
bool SetScriptAreaInSession( SEditorSession *pSession, int nIndex, const BkEditorScriptAreaRecord &rRecord, bool *pbRefused );
bool DeleteScriptAreaFromSession( SEditorSession *pSession, int nIndex, bool *pbRefused );
// Start commands (04-11, D-17), AI units, through NMapRecords on both copies, the
// engine untouched. The reads answer the totals (*pnCount, pOut->unit_count) and
// return false with *pbRefused when a buffer was too short (nothing written past
// it). Add, Set and Delete follow the rules bridge.h documents; nIndex -1 appends
// (Add only). LoadActionCommands reads Data/Editor/actions.ini the way the MFC
// editor does (CAISCHelper::Initialize); false says why in *pszWhy.
struct SActionCommandEntry
{
	std::string szName;
	int nID;
};
bool LoadActionCommands( std::vector<SActionCommandEntry> *pOut, std::string *pszWhy );
bool ReadSessionActionCommands( SEditorSession *pSession, BkEditorActionCommand *pOut, int nCapacity, int *pnCount, int *pnDefaultIndex, bool *pbRefused );
bool ReadSessionStartCommand( SEditorSession *pSession, int nIndex, BkEditorStartCommandRecord *pOut, int *pUnits, int nUnitCapacity, bool *pbRefused );
bool AddStartCommandToSession( SEditorSession *pSession, int nIndex, const BkEditorStartCommandRecord &rRecord, const int *pUnits, bool *pbRefused );
bool SetStartCommandInSession( SEditorSession *pSession, int nIndex, const BkEditorStartCommandRecord &rRecord, const int *pUnits, bool *pbRefused );
bool DeleteStartCommandFromSession( SEditorSession *pSession, int nIndex, bool *pbRefused );
// Reserve positions (04-11, D-18): the MFC editor's role classification of an object
// type (0 nothing, 1 self-propelled gun, 2 towed gun, 3 truck able to tow - from the
// object's stats, never from a cast a missing stats class would make a guess of), and
// the records through NMapRecords on both copies, the engine untouched. The reads
// return false for an index out of range; Add, Set and Delete follow the rules
// bridge.h documents (ValidateReservePosition holds them), nIndex -1 appends (Add
// only).
int ReserveRoleOfName( const char *pszName );
bool ReadSessionReservePosition( SEditorSession *pSession, int nIndex, BkEditorReservePositionRecord *pOut );
bool AddReservePositionToSession( SEditorSession *pSession, int nIndex, const BkEditorReservePositionRecord &rRecord, bool *pbRefused );
bool SetReservePositionInSession( SEditorSession *pSession, int nIndex, const BkEditorReservePositionRecord &rRecord, bool *pbRefused );
bool DeleteReservePositionFromSession( SEditorSession *pSession, int nIndex, bool *pbRefused );
// The AI general (04-12, D-19), one side at a time, through NMapRecords on both copies,
// the engine untouched. The read writes the info always and each array up to its
// capacity (the count is the total), and returns false when one was too short (a sizing
// pass); a side at or above the side count reads empty with the current count. The put
// follows the rules bridge.h documents (ValidateAISide holds them); *pbRefused tells a
// rule break from a failure.
bool ReadSessionAIGeneralSide( SEditorSession *pSession, int nSide, BkEditorAISideInfo *pInfo, int *pnMobile, int nMobileCap, BkEditorAIParcel *pParcels, int nParcelCap, BkEditorAIPoint *pPoints, int nPointCap );
bool SetSessionAIGeneralSide( SEditorSession *pSession, int nSide, int nSideCount, const int *pnMobile, int nMobileCount, const BkEditorAIParcel *pParcels, int nParcelCount, const BkEditorAIPoint *pPoints, int nPointCount, bool *pbRefused );
// Reinforcement groups (04-09, D-16), through NMapRecords on both copies. The
// two reads answer the total in *pnCount and return false when the buffer was
// too short (nothing written past it); ReadSessionGroup also returns false
// with *pbRefused for a group that is not there. SetSessionGroup applies the
// script-ID rules (0..32000, once each, an ID the group holds is exempt).
bool ReadSessionGroupIDs( SEditorSession *pSession, int *pOut, int nCapacity, int *pnCount );
bool ReadSessionGroup( SEditorSession *pSession, int nID, int *pOut, int nCapacity, int *pnCount, bool *pbRefused );
bool SetSessionGroup( SEditorSession *pSession, int nID, const int *pIDs, int nCount, bool *pbRefused );
// Research Pitfall 8 (WR-A10): the note for an object a start command or a
// reserve position names whose script ID a reinforcement group holds - the
// game's LoadUnits holds it back, so the command or position finds nothing.
// Only objects with link ID nOnlyLinkID (0: any) and groups nOnlyGroup (-1:
// any) are looked at; "" when there is nothing to say.
std::string GroupHoldWarning( const SEditorSession &rSession, int nOnlyLinkID, int nOnlyGroup );
bool DeleteSessionGroup( SEditorSession *pSession, int nID, bool *pbRefused );
int FirstFreeGroupIDInSession( SEditorSession *pSession, int nFrom );
// Hide checked (04-09, D-16). SetSessionHiddenScriptIDs replaces the set (any
// integers: one no object carries hides nothing) and applies it; the entries of
// snapshot.objects whose script ID is in it - not scenarioObjects, matching the
// game - and whose link ID is not 0 leave the scene, and the ones that were
// hidden and no longer are come back. ApplyHiddenMarks
// re-applies the set (UpdateSessionWorld and a script ID edit call it: a
// restored object is hidden again, a newly scripted one hides). IsHiddenLink
// is what picking asks.
bool SetSessionHiddenScriptIDs( SEditorSession *pSession, const int *pIDs, int nCount );
void ApplyHiddenMarks( SEditorSession *pSession );
// Puts every hidden object back in the scene without forgetting which they
// are: the world's update moves and re-textures objects the scene must hold,
// so UpdateSessionWorld runs it between this and ApplyHiddenMarks.
void ShowHiddenForUpdate( SEditorSession *pSession );
bool IsHiddenLink( const SEditorSession &rSession, int nLinkID );

// The terrain height at a world point, through CVSOBuilder::UpdateZ on the
// working copy's altitudes. False with the reason in szMessage off the map.
bool GroundHeightInSession( SEditorSession *pSession, float fX, float fY, float *pfZ );

// The terrain-mode toggles (M3, D-20): Instant Update Map Mode runs the
// objects-Z refresh over every height stroke's rectangle (the MFC's
// m_bNeedUpdateUnitHeights, TemplateEditorFrame1.cpp:5249), Fit Objects To
// Grid snaps non-unit objects on place and move (m_ifFitToAI,
// TemplateEditorFrame1.cpp:5287, default on like the MFC's). Set through
// BkEditorSetTerrainModes; the session reads them, nothing derives them.
bool SetTerrainModesInSession( SEditorSession *pSession, int bInstantUpdate, int bFitToGrid );

// Update Map (M3, D-20): the MFC's OnButtonUpdate composite
// (TemplateEditorFrame1.cpp:5138-5231) as one edit of the log - the engine's
// own UpdateAllHeights and UpdateTerrain, the full shade recompute on both
// copies, the full crosses recompute, the roads'/rivers'/sounds' z refresh
// and, when Fit To Grid is on, the snap of every sprite object with
// non-empty passability. pfnProgress (which may be null) is called once per
// step with the step number and the MFC's own total (7 fixed steps plus one
// per snapped object); it must not call back into the bridge. Undo restores
// everything the composite captured - altitudes, tiles and crosses, and every
// moved object's position - raw.
bool UpdateMapInSession( SEditorSession *pSession, void (*pfnProgress)( int nStep, int nTotal, void *pUser ), void *pUser,
                         bool *pbRefused, int *pnToken, IEditRecord **pNestedOut = 0 );

// Fill Entire Map (M3, D-22): the MFC's OnFillArea
// (TemplateEditorFrame1.cpp:4921-4968) - every tile over patches*16 becomes
// the terrain type nTileIndex's own tile (exactly what a paint of that type
// writes, GetMapsIndex and all), the crosses recomputed over the whole map.
// The MFC's update-rectangle typo (`terrainRect.maxx =- 1`,
// TemplateEditorFrame1.cpp:4951-4952, which made its terrain update an empty
// (0,0,-1,-1) rect) is NOT copied: this updates the full map. One paint of
// the log; a refusal changes nothing.
bool FillEntireMapInSession( SEditorSession *pSession, int nTileIndex, bool *pbRefused, int *pnToken );

// Altitudes (M3, D-19). rHeights are the z values (world units) to write over
// the edit rectangle, in terrain-VERTEX coordinates, row-major; the shade of
// every vertex is the session's own business, exactly D-19's function: set
// the heights, then CMapInfo::UpdateTerrainShades over GrowForShades of the
// rectangle, on the snapshot, the working copy and the engine's own terrain
// alike - the MFC editor's whole-map shade recompute at save is not copied.
// The record the edit log keeps covers the GROWN region, so undo restores the
// ring's shades too; a refusal puts everything back raw and changes nothing:
// not the snapshot, not the working copy, not the engine, not the log.
// pnToken is -1 after a refusal. pbRefused marks the ordinary nos (a
// rectangle off the map); a false without it is a failure.
bool ApplyAltitudesInSession( SEditorSession *pSession, const CTRect<int> &rVertices,
                              const std::vector<float> &rHeights, bool *pbRefused, int *pnToken );
// The vertex heights (world z units) over a region, row-major, into rHeights
// (which is sized to the rectangle by the caller). False with the reason in
// szMessage when the region is off the map.
bool ReadAltitudesInSession( SEditorSession *pSession, const CTRect<int> &rVertices, std::vector<float> *pHeights );

// One step of one Heights-tool stroke (M3, D-18), the DrawShadeState machine
// (DrawShadeState.cpp:186-336) with the MFC taken out. A stroke is a series
// of these - one per mouse move - that share vClickRef and the stroke-start
// cache; each step is one edit of the log, so the core's gesture merging
// makes one drag one undo step.
struct SHeightsStroke
{
	int nAction;			// 0 raise, 1 lower, 2 level
	int nLevelMode;		// 0 zero, 1 click tile, 2 instant average (the MFC default), 3 click average
	int nBrush;				// 2..16, the MFC slider's own value; the pattern spans brush*2 vertices per axis
	float fHeightSpeed;			// world z units: the profile gradient's ceiling
	float fLevelRatioPercent;	// the level step, percent of the distance to the target
	CVec3 vPos;				// world (Vis) units: the cursor now
	CVec3 vClickRef;	// world (Vis) units: where the stroke began (the click modes' reference)
	int bStrokeStart;	// 1 on the first step of a stroke: the session takes the click reference's tile height and pattern average then, frozen for the stroke exactly as the MFC froze them at its last hover update (fTileHeight/fAverageHeight, DrawShadeState.cpp:151-162)
	int bCtrlHeld;			// 1: a height the IsValidHeight predicate refuses is kept anyway (the MFC's MK_CONTROL override)
	SHeightsStroke() : nAction( 0 ), nLevelMode( 2 ), nBrush( 3 ), fHeightSpeed( 1.0f ), fLevelRatioPercent( 3.0f ),
									 vPos( VNULL3 ), vClickRef( VNULL3 ), bStrokeStart( 0 ), bCtrlHeld( 0 ) {  }
};

// Applies one stroke step over the pattern the MFC's own gradient builds from
// editor\profile.tga (cached per brush and speed in the session): raise adds
// it, lower subtracts it, level moves each vertex inside the level mask
// toward the mode's target by the ratio percent. The click modes' targets are
// frozen at the stroke's start (bStrokeStart caches them). When the result
// fails CVertexAltitudeInfo::IsValidHeight over the edit rectangle grown by
// the shade kernel and Ctrl is not held, the step is rolled back - the
// pattern subtracted back, exactly DrawShadeState.cpp:261 - and refused with
// "invalid height"; a refused step changes nothing. pnToken is -1 after a
// refusal; pbRefused marks the ordinary nos (the cursor off the map, an
// invalid-height rollback, a click-tile stroke whose reference left the map).
bool ApplyHeightsStrokeInSession( SEditorSession *pSession, const SHeightsStroke &rStroke, bool *pbRefused, int *pnToken );

// Generate heights (M3, D-18): the MFC's own noise - NPerlinNoise::Init, a
// CHField of the altitudes' own size, CHField::fBmDefVals[nType] with
// featSize = fGranularity, and every altitude scaled into [fMinZ, fMaxZ] by
// the MFC's formula (TabTerrainAltitudesDialog.cpp:330-346). nType is one of
// TG_FBM, TG_HYBRID, TG_RIDGED (the MFC dialog's Hills, Rocks, Dunes); the
// hidden MULTI/HETERO radios are not features. One edit of the log over the
// whole vertex sheet.
bool GenerateHeightsInSession( SEditorSession *pSession, int nType, float fGranularity, float fMinZ, float fMaxZ,
                               bool *pbRefused, int *pnToken );

// Set Zero (M3, D-18): every height to 0, the shades recomputed, one edit of
// the log over the whole vertex sheet - the MFC's altitudes.SetZero() with
// the update the MFC ran after it.
bool SetZeroHeightsInSession( SEditorSession *pSession, bool *pbRefused, int *pnToken );

// The MFC's UpdateObjectsZ (TemplateEditorFrame1.cpp:4704-4742): every road,
// river and sound of the map gets its z back on the ground over the
// altitudes - CVSOBuilder::UpdateZ on both copies' own records and the
// engine's, UpdateRoad/UpdateRiver for the redraw. The MFC ignores its
// rectangle here (the body updates every record whatever it was handed) and
// so does this, which also keeps the z a pure function of the altitudes:
// an altitude undo that re-runs it reproduces the bytes it had. Used by
// Instant Update's per-stroke pass and Update Map's (D-20).
void UpdateObjectsZInSession( SEditorSession *pSession );

// The altitude edit of the log (session.cpp): the grown region before and
// after, put back raw. Shared by ApplyAltitudesInSession and the heights
// machine - one edit kind, one implementation.
struct SAltitudeEdit : public IEditRecord
{
	NMapOverlay::SAltitudeUndo before, after;

	virtual bool Revert( SEditorSession *pSession );
	virtual bool Reapply( SEditorSession *pSession );
};

// The batch move's edit of the log (M3, D-25, session.cpp): every member's
// whole record before and after, put back raw - one drag gesture's calls
// merge in the core, one undo step restores every member exactly, engine
// included. A squad record moves whole, so its soldiers keep their offsets
// by construction.
struct SMoveObjectsEdit : public IEditRecord
{
	struct SMovedMember
	{
		int nLinkID;
		SMapObjectInfo before, after;
		SMovedMember() : nLinkID( -1 ) {  }
	};
	std::vector<SMovedMember> members;

	virtual bool Revert( SEditorSession *pSession );
	virtual bool Reapply( SEditorSession *pSession );
};

// One object's whole record before and after a fields edit (M3, D-26), put
// back raw - the flag swap's name change undoes exactly through it, engine
// included. Shared by BkEditorSetObjectFields, BkEditorSetLink and
// BkEditorUnlink: every one of them changes one object's record whole.
struct SObjectFieldsEdit : public IEditRecord
{
	int nLinkID;
	SMapObjectInfo before, after;
	SObjectFieldsEdit() : nLinkID( -1 ) {  }

	virtual bool Revert( SEditorSession *pSession );
	virtual bool Reapply( SEditorSession *pSession );
};
// The engine's own terrain over the region, written raw in place, and the
// covering patches redrawn (session.cpp) - the MFC editor's own route through
// GetTerrainInfo's const_cast (DrawShadeState.cpp:204).
void PutEngineAltitudes( ITerrainEditor *pEngineTerrain, const NMapOverlay::SAltitudeUndo &rRegion );

// The VSO state the objects-Z refresh rewrites (the roads' and rivers'
// points, the sounds' z): captured whole, because the bytes undo owes are
// the map's own, not what a re-derivation would produce again. The engine's
// copy holds no sounds.
struct SVsoZState
{
	TVSOList roads3, rivers;
	std::vector<CVec3> soundPositions;
};

// The VSO-z capture and putback (session_terrain.cpp), shared by the Update
// Map composite and the fields composite - the fields heights pass runs the
// same objects-Z refresh over every road, river and sound.
void CaptureVsoZ( CMapInfo &rMap, SVsoZState *pState );
void CaptureEngineVsoZ( ITerrainEditor *pEngineTerrain, SVsoZState *pState );
void PutVsoZBack( SEditorSession *pSession, const SVsoZState &rSnapshot, const SVsoZState &rWorking, const SVsoZState &rEngine );

// The Update Map composite of the log (05-02, D-20): the altitudes (shades),
// the tiles and crosses, every object position the fit pass moved, and the
// VSO z refresh - all put back raw, both copies and the engine, so one undo
// step restores the whole composite.
struct SUpdateMapEdit : public IEditRecord
{
	NMapOverlay::SAltitudeUndo altitudesBefore, altitudesAfter;
	NMapOverlay::SPaintUndo tilesBefore, tilesAfter;
	std::vector<NMapOverlay::SMoveObject> movesBefore, movesAfter;
	SVsoZState vsoSnapshotBefore, vsoSnapshotAfter;
	SVsoZState vsoWorkingBefore, vsoWorkingAfter;
	SVsoZState vsoEngineBefore, vsoEngineAfter;

	virtual bool Revert( SEditorSession *pSession );
	virtual bool Reapply( SEditorSession *pSession );
};

// Puts one recorded paint region back into both copies and the engine, raw
// (session.cpp) - the paints' own undo route, shared with the composite.
bool PutRegionBack( SEditorSession *pSession, const NMapOverlay::SPaintUndo &rRegion );

// ---------------------------------------------------------------------------
// The Fields tool (M3, D-21, session_fields.cpp): one application of a field
// set over a drawn polygon as ONE edit of the log.
// ---------------------------------------------------------------------------

// One fields application. The polygon points are WORLD (Vis) units; z is
// ignored (the fill reads the terrain). The randomize params are the MFC
// dialog's own: min length in cells (>= 2, the dialog's edit rule), width
// 0..0.5, disturbance 0..1 - clamped here before the engine's
// RandomizeEdges sees them. The flags mirror the dialog's checkboxes; the
// season confirmation is the caller's (the app's YES/NO popup), the bridge
// does not gate on season - the MFC's dialog lived above PlaceField too.
struct SFieldApply
{
	std::string szFieldSet;         // storage-relative, as BkEditorListRmg lists them
	std::vector<CVec3> points;      // 3..64 after the MFC's own UniquePolygon+area rule
	bool bRandomize;
	float fMinLength;               // cells
	float fWidth;                   // 0..0.5
	float fDisturbance;             // 0..1
	bool bFillTerrain;
	bool bPlaceObjects;
	bool bModifyHeights;
	bool bUpdateMapAfter;
	bool bCanAddObjectFilter;       // gate the adds by this named object filter
	bool bCheckPassabilityOnly;     // report what would happen, change nothing
	std::string szObjectFilter;     // a name from BkEditorObjectFilters ("" = none)

	SFieldApply() : bRandomize( false ), fMinLength( 2.0f ), fWidth( 0.0f ), fDisturbance( 0.0f ),
		bFillTerrain( true ), bPlaceObjects( true ), bModifyHeights( true ),
		bUpdateMapAfter( false ), bCanAddObjectFilter( false ), bCheckPassabilityOnly( false ) {}
};

// One placed (or refused placement of a) field object in the report.
struct SFieldObjectReport
{
	std::string szName;
	float fX, fY;                   // AI (map) units, as FillObjectSet left them
	bool bPlaced;                   // false: the passability or the filter held it back
};

// The composite edit: the tiles and crosses, the altitudes (shades), every
// object the application added, and - when update_map_after - the whole
// Update Map composite nested inside (one token, one undo step). Undo puts
// the update back first, then removes the added objects, then the tiles and
// the altitudes: the exact reverse of the apply.
struct SFieldEdit : public IEditRecord
{
	NMapOverlay::SPaintUndo tilesBefore, tilesAfter;
	NMapOverlay::SAltitudeUndo altitudesBefore, altitudesAfter;
	SVsoZState vsoSnapshotBefore, vsoSnapshotAfter; // the heights pass's objects-Z refresh rewrites these
	SVsoZState vsoWorkingBefore, vsoWorkingAfter;
	SVsoZState vsoEngineBefore, vsoEngineAfter;
	std::vector<SMapObjectInfo> addedRecords; // whole records, own link IDs; undo removes in reverse, redo re-adds them exact
	std::unique_ptr<IEditRecord> updateMap; // the nested composite, when it ran

	virtual bool Revert( SEditorSession *pSession );
	virtual bool Reapply( SEditorSession *pSession );
};

// The application itself. The report (which may be null) answers every
// object FillObjectSet produced and whether it was placed. A refusal changes
// nothing; a mid-pipeline failure puts the before-captures and the objects
// already added back and refuses.
bool ApplyFieldInSession( SEditorSession *pSession, const SFieldApply &rApply,
	std::vector<SFieldObjectReport> *pReport, bool *pbRefused, int *pnToken );

// The field set's season, the CMapInfo::GetSelectedSeason answer for the
// loaded set - the app compares it with the map's and shows the YES/NO
// confirmation (the MFC's IDS_INVALID_FIELD_SEASON flow) before applying.
bool FieldSetSeasonInSession( SEditorSession *pSession, const std::string &rszName, int *pnSeason, bool *pbRefused );

// The RMG storage-folder scan (session_rmg.cpp, D-08): bare storage-relative
// names, lowercased, .xml stripped, sorted, deduped. Kind: 0 field sets,
// 1 templates, 2 graphs, 3 containers, 4 settings, 5 chapters. A missing
// folder is an empty list, not an error.
bool ListRmgFolder( SEditorSession *pSession, int nKind, std::vector<std::string> *pNames );

// Create Random Map (session_rmg.cpp, M3 D-01..D-05): one generation through
// the engine's own CMapInfo::CreateRandomMap. The names are storage-relative
// (BkEditorListRmg's); the map name is bare; the output root is built here from
// the platform's user root (and the active mod's folder) and never comes from
// the caller. pfnProgress is told (step, total, user) once per generator step
// and must not call back into the bridge.
struct SRMGenerateParams
{
	std::string szTemplate;
	std::string szContext;
	std::string szSetting;				// empty or RMGC_ANY_SETTING_NAME: any setting
	std::string szMapName;
	std::string szModFolder;			// the session's active mod (the bridge fills it; never the caller)
	int nLevel;								// 0..2
	int nGraph;								// -1: the generator picks
	int nAngle;								// -1: the generator picks, 0..3
	bool bSaveAsBZM;
	bool bWriteDDS;
	bool bOverwrite;
	bool bHasSeed;
	unsigned int nSeed;
	void (*pfnProgress)( int nStep, int nTotal, void *pUser );
	void *pUser;
	SRMGenerateParams() : nLevel( 0 ), nGraph( -1 ), nAngle( -1 ), bSaveAsBZM( true ), bWriteDDS( false ), bOverwrite( false ), bHasSeed( false ), nSeed( 0 ), pfnProgress( 0 ), pUser( 0 ) {  }
};
struct SRMGenerateResult
{
	unsigned int nSeed;
	int nGraph;
	int nAngle;
	std::string szGraphName;
	std::string szMapPath;				// the map file, in the host's own separators
	SRMGenerateResult() : nSeed( 0 ), nGraph( -1 ), nAngle( -1 ) {  }
};
bool CreateRandomMapInSession( SEditorSession *pSession, const SRMGenerateParams &rParams, bool *pbRefused, SRMGenerateResult *pResult );

// The mounted storages' files under a folder that end in an extension (M3,
// D-13's Export lists, MainFrm.cpp OnTool0-3's own enumeration): the names
// as the storage holds them with the extension kept, lower-cased, backslashes,
// sorted and deduped. The folder is storage-relative with its trailing
// backslash; the extension is whatever the name ends in (".xml", "context.xml").
// False, with the reason in szMessage and *pbRefused set, for a folder or an
// extension that is not plain.
bool ListStorageFiles( SEditorSession *pSession, const std::string &rszFolder, const std::string &rszExtension, std::vector<std::string> *pNames, bool *pbRefused );

// One template's graphs with their weights, in the template's own order (the
// graphs_list.txt export's lines, MainFrm.cpp:975-981). The name is
// storage-relative as ListRmgFolder lists it. False, refused, for a name the
// data does not hold.
struct SRMTemplateGraph
{
	std::string szName;
	int nWeight;
};
bool ListTemplateGraphs( SEditorSession *pSession, const std::string &rszTemplate, std::vector<SRMTemplateGraph> *pGraphs, bool *pbRefused );

// The user RMG storage root (M3 05-09, D-09 - COSTLY, see bridge.h): mounts
// <UserRoot>rmg/ (or <UserRoot>mods/<rszModFolder>/rmg/) as the "RMG_USER"
// layer of the data storage, over Data and below the mod layer. The mod layer
// (pModStorage, null for none) is taken off and put back above it, which is
// why every caller names the mod storage it holds. Remounts only when the
// root changed (the platform's user root can be re-pointed, as the tests do)
// unless bForce. Nothing is created on disk. False when the storage is not
// there.
bool MountRmgRoot( SEditorSession *pSession, const std::string &rszModFolder, IDataStorage *pModStorage, bool bForce );

// The user RMG root as the host spells it (no trailing separator), for the
// session's mod folder.
std::string RmgRootHostPath( const std::string &rszModFolder );

// The composer records (session_rmg.cpp), the bridge.h structs filled and
// written through the engine's own serialisers. pbRefused is set for an
// ordinary no (message in szMessage), pbBadArgument for a caller bug. Reads
// never change anything; a write changes only the user RMG root.
bool ReadRmgContainerRecord( SEditorSession *pSession, const std::string &rszName, BkEditorRmgContainerRecord *pRecord, bool *pbRefused );
bool WriteRmgContainerRecord( SEditorSession *pSession, const std::string &rszName, const BkEditorRmgContainerRecord &rRecord, bool *pbRefused, bool *pbBadArgument );
bool ReadRmgGraphRecord( SEditorSession *pSession, const std::string &rszName, BkEditorRmgGraphRecord *pRecord, bool *pbRefused );
bool WriteRmgGraphRecord( SEditorSession *pSession, const std::string &rszName, const BkEditorRmgGraphRecord &rRecord, bool *pbRefused, bool *pbBadArgument );
bool ReadRmgPatchInfo( SEditorSession *pSession, const std::string &rszName, BkEditorRmgPatchInfo *pInfo, bool *pbRefused );
bool ImportRmgPatch( SEditorSession *pSession, const std::string &rszSourcePath, bool bApply, std::string *pszName, bool *pbRefused );

// The Fields and Templates Composers' records (M3 05-10, session_rmg.cpp), as
// the container and graph ones above. A template's write puts the
// QuickLoadMapInfo entry beside the Template entry, as the MFC's
// SaveTemplatesList did.
bool ReadRmgFieldSetRecord( SEditorSession *pSession, const std::string &rszName, BkEditorRmgFieldSetRecord *pRecord, bool *pbRefused );
bool WriteRmgFieldSetRecord( SEditorSession *pSession, const std::string &rszName, const BkEditorRmgFieldSetRecord &rRecord, bool *pbRefused, bool *pbBadArgument );
bool ReadRmgTemplateRecord( SEditorSession *pSession, const std::string &rszName, BkEditorRmgTemplateRecord *pRecord, bool *pbRefused );
bool WriteRmgTemplateRecord( SEditorSession *pSession, const std::string &rszName, const BkEditorRmgTemplateRecord &rRecord, bool *pbRefused, bool *pbBadArgument );
// The season's tileset terrain types (names and tile counts), no map needed.
bool ListRmgTerrainTypes( SEditorSession *pSession, int nSeason, std::vector<std::pair<std::string, int> > *pTypes );
// Whether name + extension is in the storage stack.
bool RmgFileExists( SEditorSession *pSession, const std::string &rszName, const std::string &rszExtension, bool *pbExists, bool *pbBadArgument );

// Puts one recorded altitude region back into both copies and the engine,
// raw (session.cpp) - SAltitudeEdit's own route, shared with the composite.
bool PutAltitudeEditBack( SEditorSession *pSession, const NMapOverlay::SAltitudeUndo &rRegion );

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

// The Layers menu (M3, D-32, session_layers.cpp). Each answers a BkEditorStatus
// with the reason in szMessage; none touches map data.
// The layers this renderer can drive (a bit per BkEditorLayer).
unsigned LayerAvailableMask();
// One layer to a state: the engine is driven there and the bit remembered.
BkEditorStatus SetLayerInSession( SEditorSession *pSession, int nLayer, int bShown );
// The remembered state put back onto the engine after a map is built into it
// (the scene's flags and the terrain's own grid and noise are not carried over
// to a new terrain), and the frame-time part of the wire frame.
void ReapplyLayersInSession( SEditorSession *pSession );
void ApplyWireframeForFrame( SEditorSession *pSession );
// Whether a frame has to update the world first (passability marks follow the
// camera, the shoot areas follow the units).
bool LayersNeedWorldUpdate( const SEditorSession *pSession );
// The fire-range group leaves before the AI that owns its units is cleared;
// the mode goes back to off (the caller re-asks it for the new map).
void DropFireRangeInSession( SEditorSession *pSession );
BkEditorStatus SetFireRangeInSession( SEditorSession *pSession, int nMode, const char *pszFilter, const int *pnLinkIDs, int nCount );
// The named object filter's condition lists (filters.cpp): shipped, user's
// over it; false for a name neither file holds.
bool ReadObjectFilterLists( const std::string &rszName, std::vector< std::vector<std::string> > *pLists );

#endif // __EDITOR_BRIDGE_SESSION_H__
