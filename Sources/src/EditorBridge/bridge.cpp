// The C ABI's implementation: startup, shutdown, and the firewall that keeps
// C++ exceptions on this side of the boundary.
//
// Startup is the game's (Game/GameMain.cpp) without CMainLoop and its menu
// screens: load the modules, open the data storage, read consts.xml, create the
// object database, start the engine on the caller's window, set the mode.
#include "StdAfx.h"
#include "bridge.h"
#include "session.h"
#include "world.h"
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
	// What the game does after its mode is set (Game/GameMain.cpp:795-801). The
	// scene's screen transform - IScene::Pick and GetPos2 - is the viewport
	// times this projection times the view, and without it the projection is
	// the identity: measured, a world unit came out 720 pixels wide and nothing
	// was ever under the middle of the screen.
	const RECT rcScreen = pGFX->GetScreenRect();
	SHMatrix matProjection;
	CreateOrthographicProjectionMatrixRH( &matProjection, float( rcScreen.right - rcScreen.left ), float( rcScreen.bottom - rcScreen.top ),
	                                      1, 1024 * 8 + float( rcScreen.bottom - rcScreen.top ) * 2 );
	pGFX->SetCullMode( GFXC_CW );		// the right-handed coordinate system
	pGFX->SetProjectionTransform( matProjection );
	pGFX->EnableLighting( false );
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
		// The MFC editor's switch (MainFrm.cpp:471): without it CWorldBase::Update
		// hands the AI's notifications on only when a game segment is due
		// (WorldBase.cpp:574), and an edit would draw a segment late or never.
		SetGlobalVar( "editor", 1 );
		pSession->pWorld = new CEditorWorld;
		pSession->pWorld->Init( GetSingletonGlobal() );
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
			pOut->map_type = rMap.nType;
			pOut->attacking_side = rMap.nAttackingSide;
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

BkEditorStatus BkEditorObjects( BkEditorSession *pSession, BkEditorObjectRecord *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return ReadSessionObjects( pSession, pOut, nCapacity, pnCount ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorDiplomacy( BkEditorSession *pSession, int nPlayer, int *pnValue )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnValue == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nPlayer < 0 || nPlayer >= int( pSession->snapshot.diplomacies.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		*pnValue = pSession->snapshot.diplomacies[nPlayer];
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

BkEditorStatus BkEditorRestoreObject( BkEditorSession *pSession, int nLinkID )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( RestoreObjectInSession( pSession, nLinkID, &bRefused ) )
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
		// The map keeps a BYTE, so anything else would be truncated into some
		// other value on its way in.
		if ( nValue < 0 || nValue > 2 )
		{
			pSession->szMessage = NStr::Format( "%d is no diplomacy: 0 and 1 are the two sides, 2 is neutral", nValue );
			return BK_EDITOR_BAD_ARGUMENT;
		}
		return SetSessionDiplomacy( pSession, nPlayer, nValue ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorPaint( BkEditorSession *pSession, const BkEditorPaintCell *pCells, int nCount, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nCount < 0 || ( nCount > 0 && pCells == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		std::vector<NMapOverlay::SPaintCell> cells;
		cells.reserve( nCount );
		for ( int i = 0; i < nCount; ++i )
		{
			NMapOverlay::SPaintCell cell;
			cell.nX = pCells[i].x;
			cell.nY = pCells[i].y;
			cell.tile = pCells[i].tile;
			cell.noise = 0;		// PaintIntoSession fills it from the engine
			cells.push_back( cell );
		}
		int nToken = -1;
		if ( !PaintIntoSession( pSession, cells, &nToken ) )
			return BK_EDITOR_REFUSED;
		if ( pnToken != 0 )
			*pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

// Undo and redo of a paint, by the token BkEditorPaint handed out.
BkEditorStatus BkEditorUndoPaint( BkEditorSession *pSession, int nToken )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( UndoPaintInSession( pSession, nToken, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorRedoPaint( BkEditorSession *pSession, int nToken )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( RedoPaintInSession( pSession, nToken, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorWorldToTile( BkEditorSession *pSession, float wx, float wy, int *pnX, int *pnY )
{
	if ( pnX != 0 ) *pnX = -1;
	if ( pnY != 0 ) *pnY = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnX == 0 || pnY == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return WorldToTile( pSession, wx, wy, pnX, pnY ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorTerrainMatchesEngine( BkEditorSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return TerrainMatchesEngine( pSession ) ? BK_EDITOR_OK : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorWorldMatchesMap( BkEditorSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return WorldMatchesSession( pSession ) ? BK_EDITOR_OK : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorCatalogue( BkEditorSession *pSession, BkEditorCatalogueEntry *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		return ReadCatalogue( pSession, pOut, nCapacity, pnCount ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorSetCamera( BkEditorSession *pSession, float wx, float wy )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetSessionCamera( pSession, wx, wy ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorFrame( BkEditorSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return DrawSessionFrame( pSession ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorScreenToWorld( BkEditorSession *pSession, float sx, float sy, float *pwx, float *pwy )
{
	if ( pwx != 0 ) *pwx = 0.0f;
	if ( pwy != 0 ) *pwy = 0.0f;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pwx == 0 || pwy == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		return ScreenToWorld( pSession, sx, sy, pwx, pwy ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorObjectAt( BkEditorSession *pSession, float sx, float sy, int *pnLinkID )
{
	if ( pnLinkID != 0 )
		*pnLinkID = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnLinkID == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( ObjectAt( pSession, sx, sy, pnLinkID, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
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
		if ( nSide < 0 || nSide > 1 )
		{
			pSession->szMessage = NStr::Format( "%d is no side: the attacking side is 0 or 1", nSide );
			return BK_EDITOR_BAD_ARGUMENT;
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
		// The session holds the world by a plain pointer. Its destructor empties
		// the scene and drops the map objects, which refer to the AI objects
		// the session holds.
		delete pSession->pWorld;
		pSession->pWorld = 0;
		delete pSession;
	}
	catch ( ... )
	{
		return BK_EDITOR_FAILED;
	}
	return BK_EDITOR_OK;
}
}
