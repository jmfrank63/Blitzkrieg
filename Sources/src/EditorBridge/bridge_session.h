#ifndef __EDITOR_BRIDGE_BRIDGE_SESSION_H__
#define __EDITOR_BRIDGE_BRIDGE_SESSION_H__

// The complete BkEditorSession type (what the C ABI's opaque BkEditorSession*
// points at), extracted from bridge.cpp so resource_bridge.cpp can accept the
// same pointer and the shared Guarded template (guarded.h) sees the inheritance
// from SEditorSession rather than only the forward declaration bridge.h gives.
// The research's "do not duplicate" rule for the Guarded wrapper carries here
// too: a second BkEditorSession in resource_bridge.cpp would drift.
//
// The engine types in the fields (CPtr, IImage, STilesetDesc, IDataStorage)
// come in through the engine headers below - the same set bridge.cpp already
// pulled in before this extraction, so the compile cost is unchanged.

#include "session.h"
#include "../GFX/GFX.H"
#include "../Image/Image.h"
#include "../Formats/fmtTerrain.h"
#include <string>

struct BkEditorSession : public SEditorSession
{
	void *pWindow;
	// The active mod (03-08, D-26): folder exactly as BkEditorSetMod was
	// given (never lower-cased), and mod.xml's own name/version - all empty
	// when none is active. BkEditorSaveMap reads these to stamp
	// szMODName/szMODVersion (D-28).
	std::string szModFolder, szModName, szModVersion;
	// BkEditorTilePicture's cache (03-15 gap fix): the tileset last asked
	// about, by its storage name, with its texture decoded once and its
	// description as its own .xml has it. Dropped by BkEditorSetMod - the same
	// name may be another file in the new mod's storage.
	std::string szTileAtlasName;
	CPtr<IImage> pTileAtlas;
	STilesetDesc tileAtlasDesc;
	// The mod's data storage while a mod is active (the layer the data storage
	// holds as "MOD"), kept so the user RMG root can be remounted below it
	// (session_rmg.cpp MountRmgRoot).
	CPtr<IDataStorage> pModStorage;
	BkEditorSession() : pWindow( 0 ) {  }
};

#endif
