#ifndef __EDITOR_BRIDGE_SESSION_H__
#define __EDITOR_BRIDGE_SESSION_H__
#include <string>
#include <vector>
#include <unordered_map>
#include "../RandomMapGen/MapInfo_Types.h"

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
	bool bEngineStarted;
	bool bMapOpen;
	SEditorSession() : bEngineStarted( false ), bMapOpen( false ) {  }
};

#endif // __EDITOR_BRIDGE_SESSION_H__
