#ifndef __EDITOR_BRIDGE_SESSION_H__
#define __EDITOR_BRIDGE_SESSION_H__
#include <string>
#include <vector>
#include <unordered_map>
#include "../RandomMapGen/MapInfo_Types.h"
#include "../MapFile/MapOverlay.h"

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
	bool bEngineStarted;
	bool bMapOpen;
	SEditorSession() : nBridgeSpansInMap( 0 ), nBridgeSpansPlaced( 0 ), bEngineStarted( false ), bMapOpen( false ) {  }
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
bool SetSessionDiplomacy( SEditorSession *pSession, int nPlayer, int nDiplomacy );

// Reads the object's current record out of the snapshot, so a caller changing
// one of its three editable fields can leave the other two alone.
const SMapObjectInfo* FindSnapshotObject( const SEditorSession &rSession, int nLinkID );

#endif // __EDITOR_BRIDGE_SESSION_H__
