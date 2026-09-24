#ifndef __EDITOR_BRIDGE_SESSION_H__
#define __EDITOR_BRIDGE_SESSION_H__
#include <string>
#include <vector>
#include <unordered_map>
#include "../RandomMapGen/MapInfo_Types.h"
#include "../MapFile/MapOverlay.h"
#include "bridge.h"

class CEditorWorld;

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
	SEditorSession() : nBridgeSpansInMap( 0 ), nBridgeSpansPlaced( 0 ), nLinkIDFloor( 0 ), pWorld( 0 ), bEngineStarted( false ), bMapOpen( false ) {  }
};

// Reads pszPath into the session and builds the engine state the editor draws
// and edits through: shades, the AI editor, the terrain in the scene, and one
// engine object per placed map object. Returns false and leaves the reason in
// szMessage; the session is then left with no map open.
//
// The order is the MFC editor's (TemplateEditorFrame1.cpp:1657-1790), which is
// the order the engine expects - the AI editor is initialised before the
// terrain reaches the scene, not after.
bool OpenMapIntoSession( SEditorSession *pSession, const char *pszPath );

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

#endif // __EDITOR_BRIDGE_SESSION_H__
