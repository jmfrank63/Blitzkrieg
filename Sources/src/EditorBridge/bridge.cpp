// The C ABI's implementation: startup, shutdown, and the firewall that keeps
// C++ exceptions on this side of the boundary.
//
// Startup is the game's (Game/GameMain.cpp) without CMainLoop and its menu
// screens: load the modules, open the data storage, read consts.xml, create the
// object database, start the engine on the caller's window, set the mode.
#include "StdAfx.h"
#include "bridge.h"
#include "session.h"
#include "../MapFile/MapOverlay.h"
#include "../Main/iMain.h"
#include "../GFX/GFX.H"
#include "../Platform/Paths.h"
#include "../StreamIO/RandomGen.h"
#include "../Main/GameDB.h"

// Every module that links the engine statics defines these four and lets
// something fill them; see any Sources/src/*/GlobalsLoader.cpp. Here they are
// filled by NMain::EnsureGlobalHooks (Main/LoadDLLs.cpp), called from
// BkEditorStart before the first module is loaded. They live in the bridge
// rather than in each caller so that an editor linking this library does not
// have to know they exist.
typedef void* (STDCALL *GETTEMPRAWBUFFER_HOOK)( int nAmount, int nBufferIndex );
IRandomGen *g_pGlobalRandomGen = 0;
ISaveLoadSystem *g_pGlobalSaveLoadSystem = 0;
ISingleton *g_pGlobalSingleton = 0;
GETTEMPRAWBUFFER_HOOK g_pfnGlobalGetTempRawBuffer = 0;

struct BkEditorSession : public SEditorSession {  };

namespace {

// Every entry point that needs a session goes through this. The catch is not
// decoration: an engine throw escaping here would unwind out of this module and
// into a Zig caller.
template<class F>
BkEditorStatus Guarded( BkEditorSession *pSession, F body )
{
	if ( pSession == 0 )
		return BK_EDITOR_NO_SESSION;
	try
	{
		pSession->szMessage.clear();
		return body();
	}
	catch ( ... )
	{
		pSession->szMessage = "the engine threw";
		return BK_EDITOR_FAILED;
	}
}

// The renderer's own start, separated so a machine without a device is told
// apart from a machine where something else went wrong.
BkEditorStatus StartRenderer( BkEditorSession *pSession, void *pWindow )
{
	if ( !NMain::InitializeWithWindow( GFXNativeWindow( pWindow ) ) )
	{
		pSession->szMessage = "the renderer would not start on this window";
		return BK_EDITOR_NO_DEVICE;
	}
	IGFX *pGFX = GetSingleton<IGFX>();
	if ( pGFX == 0 )
	{
		pSession->szMessage = "no IGFX after a successful initialize";
		return BK_EDITOR_NO_DEVICE;
	}
	// The editor draws into the window it was handed, so the mode follows the
	// window rather than the profile's fullscreen settings.
	if ( !pGFX->SetMode( 0, 0, 32, -1, GFXFS_WINDOWED, 0 ) )
	{
		pSession->szMessage = "IGFX::SetMode failed";
		return BK_EDITOR_NO_DEVICE;
	}
	return BK_EDITOR_OK;
}
}

extern "C" {

const char *BkEditorLastMessage( BkEditorSession *pSession )
{
	// Defined for a null session on purpose: a failed start hands the caller a
	// null session and a reason in the same breath.
	if ( pSession == 0 )
		return "the session could not be created";
	return pSession->szMessage.c_str();
}

BkEditorStatus BkEditorStart( void *pWindow, const char *pszDataRoot, BkEditorSession **ppOut )
{
	if ( ppOut == 0 )
		return BK_EDITOR_BAD_ARGUMENT;
	*ppOut = 0;
	if ( pWindow == 0 )
		return BK_EDITOR_BAD_ARGUMENT;

	BkEditorSession *pSession = 0;
	try
	{
		pSession = new BkEditorSession;
	}
	catch ( ... )
	{
		return BK_EDITOR_FAILED;
	}
	*ppOut = pSession;
	pSession->szDataRoot = pszDataRoot != 0 ? pszDataRoot : ".";

	return Guarded( pSession, [pSession, pWindow]() -> BkEditorStatus
	{
		// The editor is told which installation to edit rather than inferring
		// one from its own location: it does not live beside the game the way
		// Game.exe does. Everything else - ModuleRoot, DataRoot, ShaderRoot -
		// derives from this one root, exactly as it does for the game.
		NPlatform::Paths::SetRoots( pSession->szDataRoot.c_str(), NPlatform::Paths::UserRoot().c_str() );
		// Before any module is loaded, not after: a module's own static
		// initializers run inside dlopen and read globals that coalesce onto this
		// executable's copies. See the note on the declaration.
		NMain::EnsureGlobalHooks();
		if ( NMain::LoadAllModules( NPlatform::Paths::ModuleRoot().c_str() ) <= 0 )
		{
			pSession->szMessage = "no engine modules loaded from " + NPlatform::Paths::ModuleRoot();
			return BK_EDITOR_DATA_MISSING;
		}
		{
			CPtr<IDataStorage> pStorage = OpenStorage( NPlatform::Paths::DataArchivePattern().c_str(),
			                                           STREAM_ACCESS_READ, STORAGE_TYPE_MOD );
			if ( pStorage == 0 )
			{
				pSession->szMessage = "no data storage at " + NPlatform::Paths::DataArchivePattern();
				return BK_EDITOR_DATA_MISSING;
			}
			RegisterSingleton( IDataStorage::tidTypeID, pStorage );
		}
		{
			CTableAccessor table = NDB::OpenDataTable( "consts.xml" );
			NMain::SetupGlobalVarConsts( table );
		}
		{
			CPtr<IObjectsDB> pGDB = CreateObjectsDB();
			RegisterSingleton( IObjectsDB::tidTypeID, pGDB );
			GetSLS()->SetGDB( pGDB );
		}
		const BkEditorStatus renderer = StartRenderer( pSession, pWindow );
		if ( renderer != BK_EDITOR_OK )
			return renderer;
		// After the renderer, because LoadDB reads textures.
		if ( !GetSingleton<IObjectsDB>()->LoadDB() )
		{
			pSession->szMessage = "the object database would not load";
			return BK_EDITOR_DATA_MISSING;
		}
		pSession->bEngineStarted = true;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorOpenMap( BkEditorSession *pSession, const char *pszPath, BkEditorMapSummary *pOut )
{
	if ( pOut != 0 )
		memset( pOut, 0, sizeof *pOut );
	return Guarded( pSession, [pSession, pszPath, pOut]() -> BkEditorStatus
	{
		if ( pszPath == 0 || *pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !OpenMapIntoSession( pSession, pszPath ) )
			return pSession->bEngineStarted ? BK_EDITOR_DATA_MISSING : BK_EDITOR_NO_SESSION;
		if ( pOut != 0 )
		{
			// Sizes and counts come from the snapshot, which is the file as it
			// was read; the working copy differs only in frame indices and in
			// the altitudes a map without any gets.
			const CMapInfo &rMap = pSession->snapshot;
			pOut->width_tiles = rMap.terrain.tiles.GetSizeX();
			pOut->height_tiles = rMap.terrain.tiles.GetSizeY();
			pOut->season = rMap.nSeason;
			pOut->player_count = int( rMap.diplomacies.size() );
			pOut->object_count = int( rMap.objects.size() + rMap.scenarioObjects.size() );
			pOut->unknown_object_count = int( pSession->unknownLinkIDs.size() );
			// These three come from the session rather than the file: they say
			// what the engine ended up holding, which is the only way a caller
			// can tell a bridge that was built from one that was collected and
			// forgotten.
			pOut->placed_object_count = int( pSession->byLinkID.size() );
			pOut->bridge_span_count = pSession->nBridgeSpansInMap;
			pOut->bridge_span_placed = pSession->nBridgeSpansPlaced;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorAddObject( BkEditorSession *pSession, const char *pszName,
                                  float x, float y, int nDir, int nPlayer, int *pnLinkID )
{
	if ( pnLinkID != 0 )
		*pnLinkID = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || *pszName == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		NMapOverlay::SAddObject add;
		add.szName = pszName;
		add.vPos = CVec3( x, y, 0.0f );
		add.nDir = nDir;
		add.nPlayer = nPlayer;
		return AddObjectToSession( pSession, add, pnLinkID ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

// The three that change one field of a placed object each. They read the other
// two back out of the snapshot rather than making the caller pass everything it
// is not changing.
namespace {
BkEditorStatus ChangeOneField( BkEditorSession *pSession, int nLinkID, int nWhich, float x, float y, int nValue )
{
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return BK_EDITOR_REFUSED;
	}
	const SMapObjectInfo *pObject = FindSnapshotObject( *pSession, nLinkID );
	if ( pObject == 0 )
	{
		pSession->szMessage = "no object with that link ID";
		return BK_EDITOR_REFUSED;
	}
	CVec3 vPos = pObject->vPos;
	int nDir = pObject->nDir, nPlayer = pObject->nPlayer;
	if ( nWhich == 0 ) { vPos.x = x; vPos.y = y; }
	else if ( nWhich == 1 ) nDir = nValue;
	else nPlayer = nValue;
	bool bRefused = false;
	if ( PlaceObjectInSession( pSession, nLinkID, vPos, nDir, nPlayer, &bRefused ) )
		return BK_EDITOR_OK;
	return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
}
}

BkEditorStatus BkEditorPlaceObject( BkEditorSession *pSession, int nLinkID, float x, float y, int nDir, int nPlayer )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( PlaceObjectInSession( pSession, nLinkID, CVec3( x, y, 0.0f ), nDir, nPlayer, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorEngineObjectState( BkEditorSession *pSession, int nLinkID, BkEditorObjectState *pOut )
{
	if ( pOut != 0 )
		memset( pOut, 0, sizeof *pOut );
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		SEngineObjectState state;
		if ( !ReadEngineObject( *pSession, nLinkID, &state ) )
		{
			pSession->szMessage = "the engine does not hold that object";
			return BK_EDITOR_REFUSED;
		}
		pOut->x = state.vCenter.x;
		pOut->y = state.vCenter.y;
		pOut->dir = state.wDir;
		pOut->player = state.nPlayer;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorMoveObject( BkEditorSession *pSession, int nLinkID, float x, float y )
{
	return Guarded( pSession, [=]() { return ChangeOneField( pSession, nLinkID, 0, x, y, 0 ); } );
}

BkEditorStatus BkEditorTurnObject( BkEditorSession *pSession, int nLinkID, int nDir )
{
	return Guarded( pSession, [=]() { return ChangeOneField( pSession, nLinkID, 1, 0.0f, 0.0f, nDir ); } );
}

BkEditorStatus BkEditorSetObjectPlayer( BkEditorSession *pSession, int nLinkID, int nPlayer )
{
	return Guarded( pSession, [=]() { return ChangeOneField( pSession, nLinkID, 2, 0.0f, 0.0f, nPlayer ); } );
}

BkEditorStatus BkEditorDeleteObject( BkEditorSession *pSession, int nLinkID )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( DeleteObjectFromSession( pSession, nLinkID, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetDiplomacy( BkEditorSession *pSession, int nPlayer, int nValue )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return SetSessionDiplomacy( pSession, nPlayer, nValue ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorSetMapType( BkEditorSession *pSession, int nType )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		// Snapshot only, and the working copy with it so the two never disagree.
		// The engine is not told: it has no notion of what kind of mission this
		// is, and the game reads it from the file.
		pSession->snapshot.nType = nType;
		pSession->working.nType = nType;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSetAttackingSide( BkEditorSession *pSession, int nSide )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		pSession->snapshot.nAttackingSide = nSide;
		pSession->working.nAttackingSide = nSide;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSaveMap( BkEditorSession *pSession, const char *pszPath )
{
	return Guarded( pSession, [pSession, pszPath]() -> BkEditorStatus
	{
		if ( pszPath == 0 || *pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return SaveSessionMap( pSession, pszPath ) ? BK_EDITOR_OK : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorStop( BkEditorSession *pSession )
{
	// Safe on null and safe twice: the caller reaches here on every path out,
	// including the ones where the start never finished.
	if ( pSession == 0 )
		return BK_EDITOR_OK;
	try
	{
		delete pSession;
	}
	catch ( ... )
	{
		return BK_EDITOR_FAILED;
	}
	return BK_EDITOR_OK;
}
}
