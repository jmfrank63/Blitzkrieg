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
#include "../Scene/Scene.h"
#include "../Scene/SceneScreenScale.h"
#include "../Scene/Terrain.h"
#include "../Image/Image.h"
#include "../Platform/Paths.h"
#include "../StreamIO/RandomGen.h"
#include "../StreamIO/GeneratedData.h"
#include "../StreamIO/ProfilePaths.h"
#include "../Main/GameDB.h"
#include "../Main/RPGStats.h"
// The shared managers BkEditorSetMod clears (03-08, mirroring
// CMainLoop::ClearResources(true)) other than the ones GFX.H already
// declares (IMeshManager, ITextureManager, IFontManager).
#include "../Scene/PFX.h"
#include "../Anim/Animation.h"
#include "../SFX/SFX.h"
#include "../Main/TextSystem.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

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

// The window the session was started on, which BkEditorResize reads the new
// size of. The caller owns it and keeps it alive for the session.
struct BkEditorSession : public SEditorSession
{
	void *pWindow;
	// The active mod (03-08, D-26): folder exactly as BkEditorSetMod was
	// given (never lower-cased), and mod.xml's own name/version - all empty
	// when none is active. BkEditorSaveMap reads these to stamp
	// szMODName/szMODVersion (D-28).
	std::string szModFolder, szModName, szModVersion;
	BkEditorSession() : pWindow( 0 ) {  }
};

namespace {

// The projection the game sets after every mode change (Game/GameMain.cpp:795-801).
// The scene's screen transform - IScene::Pick and GetPos2 - is the viewport
// times this projection times the view, and without it the projection is the
// identity: measured, a world unit came out 720 pixels wide and nothing was
// ever under the middle of the screen.
void SetScreenProjection( IGFX *pGFX )
{
	const RECT rcScreen = pGFX->GetScreenRect();
	SHMatrix matProjection;
	CreateOrthographicProjectionMatrixRH( &matProjection, float( rcScreen.right - rcScreen.left ), float( rcScreen.bottom - rcScreen.top ),
	                                      1, 1024 * 8 + float( rcScreen.bottom - rcScreen.top ) * 2 );
	pGFX->SetCullMode( GFXC_CW );		// the right-handed coordinate system
	pGFX->SetProjectionTransform( matProjection );
	pGFX->EnableLighting( false );
}

// GFX.World.BaseSizeX/Y from the screen's own size, the way
// Common/InterfaceScreenBase.cpp:704-711 publishes them for the Mission
// screen - a path the bridge's headless startup never runs. Without this,
// NSceneScreenScale::GetMaxZoomSteps and GetGameplayScale always see "no
// zoom possible" (both early-return on a base below 1x1).
void PublishWorldBase( IGFX *pGFX )
{
	const RECT rcScreen = pGFX->GetScreenRect();
	SetGlobalVar( "GFX.World.BaseSizeX", int( rcScreen.right - rcScreen.left ) );
	SetGlobalVar( "GFX.World.BaseSizeY", int( rcScreen.bottom - rcScreen.top ) );
}

// CInterfaceMission::ApplyZoomStep's recipe (GameTT/iMissionInternal.cpp:881-905),
// copied verbatim rather than re-derived: clamps to [0, GetMaxZoomSteps] for
// the window's current size, keeps the world point under (fSx, fSy) fixed on
// screen by shifting the camera's anchor by exactly what the re-projection
// moved it, and rebuilds the terrain mesh the way a zoom step in the game
// does (CScene::Draw's rebuild heuristic misses a zoom whose anchor does not
// move, e.g. a zoom at the screen's centre).
bool ZoomAtScreenPoint( BkEditorSession *pSession, int nSteps, float fSx, float fSy )
{
	IGFX *pGFX = pSession->bEngineStarted ? GetSingleton<IGFX>() : 0;
	IScene *pScene = pSession->bEngineStarted ? GetSingleton<IScene>() : 0;
	ICamera *pCamera = pSession->bEngineStarted ? GetSingleton<ICamera>() : 0;
	if ( pGFX == 0 || pScene == 0 || pCamera == 0 )
	{
		pSession->szMessage = "the engine is not started";
		return false;
	}
	const CTRect<float> rcScreen = pGFX->GetScreenRect();
	const int nClamped = Clamp( nSteps, 0, NSceneScreenScale::GetMaxZoomSteps( rcScreen ) );
	CVec3 vPosOld( 0, 0, 0 ), vPosNew( 0, 0, 0 );
	pScene->GetPos3( &vPosOld, CVec2( fSx, fSy ), true );
	SetGlobalVar( "GFX.World.ZoomSteps", nClamped );
	pScene->GetPos3( &vPosNew, CVec2( fSx, fSy ), true );
	pCamera->SetAnchor( pCamera->GetAnchor() + ( vPosOld - vPosNew ) );
	if ( ITerrain *pTerrain = pScene->GetTerrain() )
		pTerrain->ResetPosition();
	return true;
}

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

// A frame as an uncompressed 32-bit TGA: type 2, top-left origin (descriptor
// 0x28: 8 alpha bits and the top-to-bottom flag), BGRA, alpha forced opaque
// for the reason CMainLoop gives for its own screenshots - the frame's alpha is
// whatever the passes left behind.
bool WriteFrame( BkEditorSession *pSession, const char *pszPath, const SColor *pPixels, int nWidth, int nHeight )
{
	FILE *pFile = fopen( pszPath, "wb" );
	if ( pFile == 0 )
	{
		pSession->szMessage = std::string( "could not write " ) + pszPath;
		return false;
	}
	const unsigned char header[18] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                                   (unsigned char)( nWidth & 0xff ), (unsigned char)( nWidth >> 8 ),
	                                   (unsigned char)( nHeight & 0xff ), (unsigned char)( nHeight >> 8 ), 32, 0x28 };
	bool bWritten = fwrite( header, 1, sizeof header, pFile ) == sizeof header;
	std::vector<unsigned char> row( size_t( nWidth ) * 4 );
	for ( int y = 0; y < nHeight && bWritten; ++y )
	{
		for ( int x = 0; x < nWidth; ++x )
		{
			const SColor &color = pPixels[size_t( y ) * nWidth + x];
			row[x * 4 + 0] = (unsigned char)color.b;
			row[x * 4 + 1] = (unsigned char)color.g;
			row[x * 4 + 2] = (unsigned char)color.r;
			row[x * 4 + 3] = 255;
		}
		bWritten = fwrite( &row[0], 1, row.size(), pFile ) == row.size();
	}
	if ( fclose( pFile ) != 0 )
		bWritten = false;
	if ( !bWritten )
		pSession->szMessage = std::string( "could not write all of " ) + pszPath;
	return bWritten;
}

// BkEditorTestMapPath's file_name: a bare name (no separator of either kind,
// checked here rather than left to IsRelativeDataName, which allows multiple
// components) ending in ".bzm", and a relative data name by the engine's own
// rule - reused rather than re-derived (security: path traversal through a
// caller-chosen file name).
bool IsBareTestMapName( const std::string &szName )
{
	if ( szName.find( '\\' ) != std::string::npos || szName.find( '/' ) != std::string::npos )
		return false;
	if ( szName.size() < 5 || szName.compare( szName.size() - 4, 4, ".bzm" ) != 0 )
		return false;
	return NPlatform::Paths::IsRelativeDataName( szName );
}

// A bare mod folder name (03-08's BkEditorSetMod/BkEditorMods): no separator
// of either kind (checked directly, since IsRelativeDataName alone would
// accept a multi-component relative path), not "." or "..", not over 63
// characters (BkEditorMod::folder's capacity), and a relative data name by
// the engine's own rule - reused rather than re-derived, the same reasoning
// IsBareTestMapName gives for a test map's file name.
bool IsBareModFolderName( const std::string &szFolder )
{
	if ( szFolder.empty() || szFolder.size() > 63 )
		return false;
	if ( szFolder.find( '\\' ) != std::string::npos || szFolder.find( '/' ) != std::string::npos )
		return false;
	if ( szFolder == "." || szFolder == ".." )
		return false;
	return NPlatform::Paths::IsRelativeDataName( szFolder );
}

// Truncates rszValue into a fixed buffer the way ReadCatalogue's own name
// copy does: never refused for length, just cut, because a longer mod name
// or version than any shipped one is a display detail, not a reason to fail
// the whole read.
void CopyBoundedField( char *pField, size_t nCapacity, const std::string &rszValue )
{
	const size_t nCopy = rszValue.size() < nCapacity - 1 ? rszValue.size() : nCapacity - 1;
	memcpy( pField, rszValue.c_str(), nCopy );
	pField[nCopy] = 0;
}

// <BaseRoot>mods\<folder>\, backslash-separated and trailing one - OpenStorage's
// own convention (BkEditorTestMapPath, GeneratedData.h), matching
// CICChangeMOD::Exec's szMODPath (MainLoopCommands.cpp:393).
std::string ModEngineDir( const std::string &szFolder )
{
	std::string szBase = NPlatform::Paths::BaseRoot();
	for ( char &c : szBase )
		if ( c == '/' ) c = '\\';
	return szBase + "mods\\" + szFolder + "\\";
}

// Reads folder's own mod.xml, the same way the game's mod-list screen does
// (GameTT/InterfaceIMModsList.cpp:49-64) and CICChangeMOD::Exec mounts it
// (MainLoopCommands.cpp:396-407): false when folder's data has no mod.xml at
// all - not an installed mod, which is not a failure to report, only
// something BkEditorMods leaves out and BkEditorSetMod refuses.
bool ReadModXml( const std::string &szFolder, BkEditorMod *pOut )
{
	const std::string szPattern = ModEngineDir( szFolder ) + "data\\*.pak";
	CPtr<IDataStorage> pMOD = OpenStorage( szPattern.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
	if ( pMOD == 0 )
		return false;
	CPtr<IDataStream> pStream = pMOD->OpenStream( "mod.xml", STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	std::string szName = "MyMOD", szVersion = "1.0";
	{
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
		saver.Add( "MODName", &szName );
		saver.Add( "MODVersion", &szVersion );
	}
	memset( pOut, 0, sizeof *pOut );
	CopyBoundedField( pOut->folder, sizeof pOut->folder, szFolder );
	CopyBoundedField( pOut->name, sizeof pOut->name, szName );
	CopyBoundedField( pOut->version, sizeof pOut->version, szVersion );
	return true;
}

// Every installed mod, sorted by folder name: std::filesystem over
// <BaseRoot>mods (a real OS directory listing, not through the storage
// layer - there is no wildcard for "every subdirectory"), each candidate
// read the way ReadModXml reads one. A missing mods directory - nothing
// installed - is an empty list, not a failure: std::filesystem::directory_iterator's
// own error_code overload leaves it empty rather than throwing.
std::vector<BkEditorMod> ListInstalledMods()
{
	std::vector<BkEditorMod> mods;
	std::error_code error;
	const std::filesystem::path modsDir = std::filesystem::path( NPlatform::Paths::BaseRoot() ) / "mods";
	std::filesystem::directory_iterator it( modsDir, error );
	if ( error )
		return mods;
	for ( const std::filesystem::directory_entry &entry : it )
	{
		std::error_code entryError;
		if ( !entry.is_directory( entryError ) || entryError )
			continue;
		BkEditorMod mod;
		if ( ReadModXml( entry.path().filename().string(), &mod ) )
			mods.push_back( mod );
	}
	std::sort( mods.begin(), mods.end(), []( const BkEditorMod &a, const BkEditorMod &b )
	{
		return strcmp( a.folder, b.folder ) < 0;
	} );
	return mods;
}

// FilesInspector, the shared managers other than IGFX, and the object
// database - the steps CICChangeMOD::Exec takes once the MOD storage has
// changed (MainLoopCommands.cpp:422-431), minus ResetStack (no interface
// stack here) and the font IGFX::SetFont restores (the editor's own overlay
// lives on that device - see bridge.h's BkEditorSetMod comment). false with
// the reason in pSession->szMessage when the object database would not
// reload.
bool ReloadAfterModChange( BkEditorSession *pSession, IDataStorage *pStorage )
{
	GetSingleton<IFilesInspector>()->Clear();
	GetSingleton<IFilesInspector>()->InspectStorage( pStorage );
	GetSingleton<IParticleManager>()->Clear( ISharedManager::CLEAR_ALL );
	GetSingleton<IAnimationManager>()->Clear( ISharedManager::CLEAR_ALL );
	GetSingleton<ISoundManager>()->Clear( ISharedManager::CLEAR_ALL );
	GetSingleton<IFontManager>()->Clear( ISharedManager::CLEAR_ALL );
	GetSingleton<IMeshManager>()->Clear( ISharedManager::CLEAR_ALL );
	GetSingleton<ITextureManager>()->Clear( ISharedManager::CLEAR_ALL );
	GetSingleton<ITextManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
	if ( !GetSingleton<IObjectsDB>()->LoadDB() )
	{
		pSession->szMessage = "the object database would not reload";
		return false;
	}
	// D-29's squad-icon fallback (03-09 Task 4) is a cache over the object
	// database just reloaded above - a name it already answered for the old
	// mod may mean something else, or nothing, in the new one.
	pSession->squadIconOwnerBySoldier.clear();
	pSession->bSquadIconOwnerMapBuilt = false;
	return true;
}

// A window opened on a non-primary display otherwise jumps to
// GraphicsEngineGpu.cpp's SelectedDisplay default (GFX.Monitor.Index unset,
// which resolves to SDL_GetDisplays()'s first entry) the moment SetMode
// below runs. Measured directly (03-13 Task 2): the editor's own hidden
// window, moved onto this development machine's second display (an
// AirPlay-mirrored TV - Johannes's own second-monitor setup, see project
// memory) before BkEditorStart, reported SDL_GetDisplayForWindow one display
// lower right after SetMode returned. Publishing the window's own display as
// GFX.Monitor.Index first - the same global a profile's GFX.Monitor setting
// would set - keeps SelectedDisplay on the display the caller actually
// opened the window on.
void KeepWindowOnItsOwnDisplay( void *pWindow )
{
	const SDL_DisplayID windowDisplay = SDL_GetDisplayForWindow( static_cast<SDL_Window*>( pWindow ) );
	if ( windowDisplay == 0 )
		return;
	int nCount = 0;
	SDL_DisplayID *pDisplays = SDL_GetDisplays( &nCount );
	if ( pDisplays == 0 )
		return;
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pDisplays[i] == windowDisplay )
		{
			SetGlobalVar( "GFX.Monitor.Index", i );
			break;
		}
	}
	SDL_free( pDisplays );
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
	// Windowed, whatever the profile's fullscreen setting says, and at the
	// window's own size: the editor draws into the window it was handed, so
	// the screen is the window and a mouse position is a screen position. The
	// window has no high pixel density, so its size in points is its size in
	// pixels, and the explicit size is one the window already has.
	int nWidth = 0, nHeight = 0;
	if ( !SDL_GetWindowSize( static_cast<SDL_Window*>( pWindow ), &nWidth, &nHeight ) || nWidth <= 0 || nHeight <= 0 )
	{
		pSession->szMessage = "the window has no size";
		return BK_EDITOR_NO_DEVICE;
	}
	KeepWindowOnItsOwnDisplay( pWindow );
	if ( !pGFX->SetMode( nWidth, nHeight, 32, -1, GFXFS_WINDOWED, 0 ) )
	{
		pSession->szMessage = "IGFX::SetMode failed";
		return BK_EDITOR_NO_DEVICE;
	}
	SetScreenProjection( pGFX );
	PublishWorldBase( pGFX );
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
	pSession->pWindow = pWindow;

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
	const BkEditorStatus status = Guarded( pSession, [pSession, pszPath, pOut]() -> BkEditorStatus
	{
		if ( pszPath == 0 || *pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !OpenMapIntoSession( pSession, pszPath ) )
			return pSession->bEngineStarted ? BK_EDITOR_DATA_MISSING : BK_EDITOR_NO_SESSION;
		// A fresh map opens unzoomed (D-15); the app restores a remembered
		// view itself, after this call, through BkEditorSetZoom.
		SetGlobalVar( "GFX.World.ZoomSteps", 0 );
		if ( IScene *pScene = GetSingleton<IScene>() )
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
				pTerrain->ResetPosition();
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
	// A throw means the engine was being rebuilt for the new map, which
	// OpenMapIntoSession has already marked as no map open. Said again here so
	// that BK_EDITOR_FAILED means exactly that, whatever threw.
	if ( status == BK_EDITOR_FAILED && pSession != 0 )
		pSession->bMapOpen = false;
	return status;
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
		// Before anything is painted: a tile the tileset lacks is the caller's
		// mistake, and the engine would index its terrain types with -1 for it.
		bool bBadTile = false;
		if ( !PaintTilesInTileset( pSession, cells, &bBadTile ) )
			return bBadTile ? BK_EDITOR_BAD_ARGUMENT : BK_EDITOR_REFUSED;
		int nToken = -1;
		if ( !PaintIntoSession( pSession, cells, &nToken ) )
			return BK_EDITOR_REFUSED;
		if ( pnToken != 0 )
			*pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorEngineTile( BkEditorSession *pSession, int nX, int nY, unsigned char *pOut )
{
	if ( pOut != 0 )
		*pOut = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		BYTE tile = 0;
		if ( !EngineTile( pSession, nX, nY, &tile ) )
			return BK_EDITOR_REFUSED;
		*pOut = tile;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorTilesetTiles( BkEditorSession *pSession, unsigned char *pOut, int nCapacity, int *pnCount )
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
		return TilesetTiles( pSession, pOut, nCapacity, pnCount ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
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

// D-29's squad-icon fallback (user-requested addition, 03-09 Task 4): a
// single soldier with no icon.tga of its own borrows the icon.tga of a squad
// that lists it as a member (e.g. Allies_Bren -> gb_bren_43). Scans every
// SGVOGT_SQUAD object's own RPG stats (SSquadRPGStats::memberNames, the same
// <Members> list the squad's own data reads) once, filling
// pSession->squadIconOwnerBySoldier soldier name -> squad name. When more
// than one squad lists the same soldier the alphabetically first squad name
// wins - std::string's operator< - so the choice never depends on catalogue
// order, only on the squads' own names.
void BuildSquadIconOwnerMap( SEditorSession *pSession, IObjectsDB *pObjectsDB )
{
	pSession->bSquadIconOwnerMapBuilt = true;
	const SGDBObjectDesc *pDescs = pObjectsDB->GetAllDescs();
	const int nDescs = pObjectsDB->GetNumDescs();
	for ( int i = 0; i < nDescs; ++i )
	{
		if ( pDescs[i].eGameType != SGVOGT_SQUAD )
			continue;
		const SSquadRPGStats *pSquad = NGDB::GetRPGStats<SSquadRPGStats>( pObjectsDB, &pDescs[i] );
		if ( pSquad == 0 )
			continue;
		const std::string &szSquadName = pDescs[i].szKey;
		for ( size_t m = 0; m < pSquad->memberNames.size(); ++m )
		{
			const std::string &szMember = pSquad->memberNames[m];
			std::unordered_map<std::string, std::string>::iterator itExisting = pSession->squadIconOwnerBySoldier.find( szMember );
			if ( itExisting == pSession->squadIconOwnerBySoldier.end() || szSquadName < itExisting->second )
				pSession->squadIconOwnerBySoldier[szMember] = szSquadName;
		}
	}
}

BkEditorStatus BkEditorObjectPicture( BkEditorSession *pSession, const char *pszName,
                                      unsigned char *pOutRgba, int nCapacityBytes, int nMaxSide,
                                      int *pnOutWidth, int *pnOutHeight )
{
	if ( pnOutWidth != 0 ) *pnOutWidth = 0;
	if ( pnOutHeight != 0 ) *pnOutHeight = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || *pszName == 0 || pOutRgba == 0 || pnOutWidth == 0 || pnOutHeight == 0 ||
		     nCapacityBytes < 0 || nMaxSide < 8 || nMaxSide > 256 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
		IDataStorage *pStorage = GetSingleton<IDataStorage>();
		IImageProcessor *pImages = GetImageProcessor();
		if ( pObjectsDB == 0 || pStorage == 0 || pImages == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( pszName );
		if ( pDesc == 0 )
		{
			pSession->szMessage = NStr::Format( "the object database does not know \"%s\"", pszName );
			return BK_EDITOR_BAD_ARGUMENT;
		}
		const std::string szIconPath = pDesc->szPath + "\\icon.tga";
		CPtr<IDataStream> pStream = pStorage->OpenStream( szIconPath.c_str(), STREAM_ACCESS_READ );
		CPtr<IImage> pImage = pStream != 0 ? pImages->LoadImage( pStream ) : 0;
		if ( pImage == 0 )
		{
			// User-requested addition (03-09 Task 4): pszName may be a single
			// soldier with no icon.tga of its own - borrow a squad's that lists
			// it as a member, e.g. Allies_Bren -> gb_bren_43.
			if ( !pSession->bSquadIconOwnerMapBuilt )
				BuildSquadIconOwnerMap( pSession, pObjectsDB );
			std::unordered_map<std::string, std::string>::const_iterator itSquad = pSession->squadIconOwnerBySoldier.find( pszName );
			const SGDBObjectDesc *pSquadDesc = itSquad != pSession->squadIconOwnerBySoldier.end() ? pObjectsDB->GetDesc( itSquad->second.c_str() ) : 0;
			if ( pSquadDesc != 0 )
			{
				const std::string szSquadIconPath = pSquadDesc->szPath + "\\icon.tga";
				CPtr<IDataStream> pSquadStream = pStorage->OpenStream( szSquadIconPath.c_str(), STREAM_ACCESS_READ );
				pImage = pSquadStream != 0 ? pImages->LoadImage( pSquadStream ) : 0;
			}
		}
		if ( pImage == 0 )
		{
			pSession->szMessage = NStr::Format( "%s has no picture", pszName );
			return BK_EDITOR_REFUSED;
		}
		int nWidth = pImage->GetSizeX(), nHeight = pImage->GetSizeY();
		if ( nWidth <= 0 || nHeight <= 0 )
		{
			pSession->szMessage = NStr::Format( "%s's icon.tga decoded to an empty image", pszName );
			return BK_EDITOR_FAILED;
		}
		// Only scaled down, and only when it does not already fit: the shipped
		// icons are the MFC palette's own thumbnails and most are already small.
		if ( nWidth > nMaxSide || nHeight > nMaxSide )
		{
			const float fScale = Min( float( nMaxSide ) / float( nWidth ), float( nMaxSide ) / float( nHeight ) );
			const int nScaledWidth = Max( 1, int( float( nWidth ) * fScale + 0.5f ) );
			const int nScaledHeight = Max( 1, int( float( nHeight ) * fScale + 0.5f ) );
			CPtr<IImage> pScaled = pImages->CreateScaleBySize( pImage, nScaledWidth, nScaledHeight, ISM_LANCZOS3 );
			if ( pScaled == 0 )
			{
				pSession->szMessage = NStr::Format( "could not scale %s's picture", pszName );
				return BK_EDITOR_FAILED;
			}
			pImage = pScaled;
			nWidth = nScaledWidth;
			nHeight = nScaledHeight;
		}
		// The real size, even if capacity turns out too short below: a caller
		// sizing a buffer from a REFUSED answer needs it.
		*pnOutWidth = nWidth;
		*pnOutHeight = nHeight;
		const int nNeeded = nWidth * nHeight * 4;
		if ( nNeeded > nCapacityBytes )
		{
			pSession->szMessage = NStr::Format( "%s's picture needs %d bytes and room was given for %d", pszName, nNeeded, nCapacityBytes );
			return BK_EDITOR_REFUSED;
		}
		// SColor's r/g/b/a accessors already name the right channel regardless
		// of the union's in-memory byte order (WriteFrame's own b,g,r,a TGA row
		// relies on the same accessors) - written out here as r,g,b,a because
		// that is what an SDL_GPU R8G8B8A8_UNORM texture wants.
		const SColor *pPixels = pImage->GetLFB();
		for ( int i = 0; i < nWidth * nHeight; ++i )
		{
			pOutRgba[i * 4 + 0] = (unsigned char)pPixels[i].r;
			pOutRgba[i * 4 + 1] = (unsigned char)pPixels[i].g;
			pOutRgba[i * 4 + 2] = (unsigned char)pPixels[i].b;
			pOutRgba[i * 4 + 3] = (unsigned char)pPixels[i].a;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorMods( BkEditorSession *pSession, BkEditorMod *pOut, int nCapacity, int *pnCount )
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
		const std::vector<BkEditorMod> mods = ListInstalledMods();
		*pnCount = int( mods.size() );
		const int nWrite = int( mods.size() ) < nCapacity ? int( mods.size() ) : nCapacity;
		for ( int i = 0; i < nWrite; ++i )
			pOut[i] = mods[i];
		if ( int( mods.size() ) > nCapacity )
		{
			pSession->szMessage = NStr::Format( "there are %d mods and room was given for %d", int( mods.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSetMod( BkEditorSession *pSession, const char *pszFolder )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		IDataStorage *pStorage = GetSingleton<IDataStorage>();
		if ( pStorage == 0 )
		{
			pSession->szMessage = "the data storage is not there";
			return BK_EDITOR_REFUSED;
		}
		const std::string szFolder = pszFolder != 0 ? pszFolder : std::string();
		// null/"" clears the mod - always valid, nothing to look up.
		if ( szFolder.empty() )
		{
			CloseSessionMap( pSession );
			pStorage->RemoveStorage( "MOD" );
			RemoveGlobalVar( "MOD.Active" );
			RemoveGlobalVar( "MOD.Name" );
			RemoveGlobalVar( "MOD.Folder" );
			RemoveGlobalVar( "MOD.Version" );
			if ( !ReloadAfterModChange( pSession, pStorage ) )
				return BK_EDITOR_FAILED;
			pSession->szModFolder.clear();
			pSession->szModName.clear();
			pSession->szModVersion.clear();
			return BK_EDITOR_OK;
		}
		if ( !IsBareModFolderName( szFolder ) )
		{
			pSession->szMessage = "\"" + szFolder + "\" is not a bare mod folder name";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		// Read and validate before anything changes: a refusal below must
		// leave the session's mod, its open map and the object database
		// exactly as they were (bridge.h's own contract for this call).
		BkEditorMod mod;
		if ( !ReadModXml( szFolder, &mod ) )
		{
			pSession->szMessage = "no installed mod named \"" + szFolder + "\"";
			return BK_EDITOR_REFUSED;
		}
		const std::string szPattern = ModEngineDir( szFolder ) + "data\\*.pak";
		CPtr<IDataStorage> pModStorage = OpenStorage( szPattern.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
		if ( pModStorage == 0 )
		{
			pSession->szMessage = "no installed mod named \"" + szFolder + "\"";
			return BK_EDITOR_REFUSED;
		}
		CloseSessionMap( pSession );
		pStorage->RemoveStorage( "MOD" );
		pStorage->AddStorage( pModStorage, "MOD" );
		SetGlobalVar( "MOD.Active", 1 );
		SetGlobalVar( "MOD.Name", mod.name );
		// The folder under mods\, for anything that wants to find layouts
		// restyled for it - kept as given, the same comment CICChangeMOD::Exec
		// makes about its own MOD.Folder (MainLoopCommands.cpp:410-411).
		SetGlobalVar( "MOD.Folder", szFolder.c_str() );
		SetGlobalVar( "MOD.Version", mod.version );
		if ( !ReloadAfterModChange( pSession, pStorage ) )
			return BK_EDITOR_FAILED;
		pSession->szModFolder = szFolder;
		pSession->szModName = mod.name;
		pSession->szModVersion = mod.version;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorActiveMod( BkEditorSession *pSession, BkEditorMod *pOut )
{
	if ( pOut != 0 )
		memset( pOut, 0, sizeof *pOut );
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		CopyBoundedField( pOut->folder, sizeof pOut->folder, pSession->szModFolder );
		CopyBoundedField( pOut->name, sizeof pOut->name, pSession->szModName );
		CopyBoundedField( pOut->version, sizeof pOut->version, pSession->szModVersion );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSetCamera( BkEditorSession *pSession, float wx, float wy )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetSessionCamera( pSession, wx, wy ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorViewState( BkEditorSession *pSession, BkEditorView *pOut )
{
	if ( pOut != 0 )
		memset( pOut, 0, sizeof *pOut );
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		IGFX *pGFX = pSession->bEngineStarted ? GetSingleton<IGFX>() : 0;
		ICamera *pCamera = pSession->bEngineStarted ? GetSingleton<ICamera>() : 0;
		if ( pGFX == 0 || pCamera == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		const CTRect<float> rcScreen = pGFX->GetScreenRect();
		const int nMax = NSceneScreenScale::GetMaxZoomSteps( rcScreen );
		// A stale step count (a window shrunk since it was set) re-clamps at
		// read time rather than being trusted, the same guarantee
		// NSceneScreenScale::GetPlayerZoom gives its own callers.
		const int nSteps = Clamp( GetGlobalVar( "GFX.World.ZoomSteps", 0 ), 0, nMax );
		const CVec3 vAnchor = pCamera->GetAnchor();
		pOut->anchor_x = vAnchor.x;
		pOut->anchor_y = vAnchor.y;
		pOut->zoom_steps = nSteps;
		pOut->max_zoom_steps = nMax;
		pOut->scale = NSceneScreenScale::GetGameplayScale( rcScreen );
		// The game's own yaw plus whatever BkEditorSetYaw last set (D-12).
		pOut->yaw_degrees = 45.0f + pSession->fYawOffsetDegrees;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorZoomAt( BkEditorSession *pSession, int nDeltaSteps, float fSx, float fSy )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !std::isfinite( fSx ) || !std::isfinite( fSy ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		const int nCurrent = GetGlobalVar( "GFX.World.ZoomSteps", 0 );
		return ZoomAtScreenPoint( pSession, nCurrent + nDeltaSteps, fSx, fSy ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorSetZoom( BkEditorSession *pSession, int nSteps )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		IGFX *pGFX = pSession->bEngineStarted ? GetSingleton<IGFX>() : 0;
		if ( pGFX == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		const RECT rcScreen = pGFX->GetScreenRect();
		const float fCentreX = float( rcScreen.right - rcScreen.left ) / 2.0f;
		const float fCentreY = float( rcScreen.bottom - rcScreen.top ) / 2.0f;
		return ZoomAtScreenPoint( pSession, nSteps, fCentreX, fCentreY ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorSetYaw( BkEditorSession *pSession, float fDegrees )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !std::isfinite( fDegrees ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		// Every value names a real angle, so this wraps rather than refuses -
		// the way BkEditorEngineObjectState's caller-facing angles never do,
		// but a yaw offset has no "out of range" the way a zoom step does.
		float fWrapped = fmodf( fDegrees, 360.0f );
		if ( fWrapped < 0.0f )
			fWrapped += 360.0f;
		pSession->fYawOffsetDegrees = fWrapped;
		// Re-place at the current anchor, the same way BkEditorResize does
		// after a placement change that only the distance/pitch/yaw affects,
		// not the anchor itself.
		if ( ICamera *pCamera = GetSingleton<ICamera>() )
		{
			const CVec3 vAnchor = pCamera->GetAnchor();
			SetSessionCamera( pSession, vAnchor.x, vAnchor.y );
		}
		if ( IScene *pScene = GetSingleton<IScene>() )
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
				pTerrain->ResetPosition();
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorFrame( BkEditorSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return DrawSessionFrame( pSession ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorSetOverlay( BkEditorSession *pSession, BkEditorOverlay overlay, void *pUser )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		IGFX *pGFX = pSession->bEngineStarted ? GetSingleton<IGFX>() : 0;
		if ( pGFX == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		if ( !pGFX->SetOverlay( overlay, pUser ) )
		{
			pSession->szMessage = "this renderer has no overlay";
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorGpuDevice( BkEditorSession *pSession, void **ppDevice, unsigned int *pnFormat )
{
	if ( ppDevice != 0 ) *ppDevice = 0;
	if ( pnFormat != 0 ) *pnFormat = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( ppDevice == 0 || pnFormat == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		IGFX *pGFX = pSession->bEngineStarted ? GetSingleton<IGFX>() : 0;
		if ( pGFX == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		if ( !pGFX->GetGpuDevice( ppDevice, pnFormat ) )
		{
			*ppDevice = 0;
			*pnFormat = 0;
			pSession->szMessage = "this renderer has no SDL GPU device";
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorResize( BkEditorSession *pSession, int nWidth, int nHeight )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nWidth <= 0 || nHeight <= 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		IGFX *pGFX = pSession->bEngineStarted ? GetSingleton<IGFX>() : 0;
		if ( pGFX == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		// The window is the size; the arguments say which size the caller
		// thinks it is. A mismatch is a caller that has not caught up with the
		// window yet, and adopting either size silently would put the screen
		// and the mouse out of step.
		int nWindowWidth = 0, nWindowHeight = 0;
		SDL_GetWindowSize( static_cast<SDL_Window*>( pSession->pWindow ), &nWindowWidth, &nWindowHeight );
		if ( nWidth != nWindowWidth || nHeight != nWindowHeight )
		{
			pSession->szMessage = NStr::Format( "%dx%d is not the window's size, %dx%d", nWidth, nHeight, nWindowWidth, nWindowHeight );
			return BK_EDITOR_BAD_ARGUMENT;
		}
		// FollowWindowSize (below) resizes the renderer to the window's size
		// in points, the same size BkEditorStart's own SetMode used (no
		// SDL_WINDOW_HIGH_PIXEL_DENSITY - host.zig's own comment). A window
		// whose backing pixel size differs from its point size - a high pixel
		// density the editor does not scale for in M1 - would resize the
		// renderer to fewer pixels than the window's surface actually has,
		// so the refusal here is explicit rather than a silently blurry
		// M1 editor.
		int nPixelWidth = 0, nPixelHeight = 0;
		SDL_GetWindowSizeInPixels( static_cast<SDL_Window*>( pSession->pWindow ), &nPixelWidth, &nPixelHeight );
		if ( nPixelWidth != nWindowWidth || nPixelHeight != nWindowHeight )
		{
			pSession->szMessage = "the window has a high pixel density, which the editor does not support in M1";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		// Not SetMode: that picks the profile's display and re-centres, clamps
		// and re-shows the window on it, and keeps a requested size the window
		// was clamped away from.
		if ( !pGFX->FollowWindowSize() )
		{
			pSession->szMessage = "IGFX::FollowWindowSize failed";
			return BK_EDITOR_FAILED;
		}
		SetScreenProjection( pGFX );
		PublishWorldBase( pGFX );
		// The placement's distance depends on the screen's height, so the
		// camera is placed again at its anchor, as the game does after a
		// resolution change (GameTT/iMissionInternal.cpp, CMD_LOAD_FINISHED).
		if ( ICamera *pCamera = GetSingleton<ICamera>() )
		{
			const CVec3 vAnchor = pCamera->GetAnchor();
			SetSessionCamera( pSession, vAnchor.x, vAnchor.y );
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorScreenSize( BkEditorSession *pSession, int *pnWidth, int *pnHeight )
{
	if ( pnWidth != 0 ) *pnWidth = 0;
	if ( pnHeight != 0 ) *pnHeight = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnWidth == 0 || pnHeight == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		IGFX *pGFX = pSession->bEngineStarted ? GetSingleton<IGFX>() : 0;
		if ( pGFX == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		const RECT rcScreen = pGFX->GetScreenRect();
		*pnWidth = rcScreen.right - rcScreen.left;
		*pnHeight = rcScreen.bottom - rcScreen.top;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorCaptureFrame( BkEditorSession *pSession, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == 0 || pszPath[0] == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		IGFX *pGFX = pSession->bEngineStarted ? GetSingleton<IGFX>() : 0;
		IImageProcessor *pImages = pSession->bEngineStarted ? GetImageProcessor() : 0;
		if ( pGFX == 0 || pImages == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		// Not TakeScreenShot: that reads the scene, and the overlay is drawn
		// over the scene only on its way to the window.
		if ( !pGFX->CaptureNextFrame( true ) )
		{
			pSession->szMessage = "this renderer cannot capture a presented frame";
			return BK_EDITOR_REFUSED;
		}
		if ( !DrawSessionFrame( pSession ) )
		{
			pGFX->CaptureNextFrame( false );
			return BK_EDITOR_REFUSED;
		}
		const RECT rcScreen = pGFX->GetScreenRect();
		const int nWidth = rcScreen.right - rcScreen.left, nHeight = rcScreen.bottom - rcScreen.top;
		CPtr<IImage> pImage = pImages->CreateImage( nWidth, nHeight );
		// The read-back failure path left the capture armed, so a caller that
		// tried once and gave up left every later frame paying the capture's
		// cost for a request nobody would ever read (T-03-13-02). Disarmed
		// here the same way the DrawSessionFrame failure above already is.
		// IGFX has no accessor for GraphicsEngineGpu's own fail() message
		// (adding one would mean a new virtual method on every IGFX backend,
		// not a bridge-local change): pImage == 0 (allocation) and a false
		// ReadCapturedFrame (readback) get their own distinct message instead
		// of one line silently covering both.
		if ( pImage == 0 || !pGFX->ReadCapturedFrame( pImage ) )
		{
			pGFX->CaptureNextFrame( false );
			pSession->szMessage = pImage == 0
				? "the frame's image could not be allocated"
				: "the renderer would not read the presented frame back";
			return BK_EDITOR_REFUSED;
		}
		return WriteFrame( pSession, pszPath, pImage->GetLFB(), nWidth, nHeight ) ? BK_EDITOR_OK : BK_EDITOR_FAILED;
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

BkEditorStatus BkEditorWorldToScreen( BkEditorSession *pSession, float wx, float wy, float *psx, float *psy )
{
	if ( psx != 0 ) *psx = 0.0f;
	if ( psy != 0 ) *psy = 0.0f;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( psx == 0 || psy == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !std::isfinite( wx ) || !std::isfinite( wy ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen || GetSingleton<ICamera>() == 0 )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return WorldToScreen( pSession, wx, wy, psx, psy ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorWorldToMap( BkEditorSession *pSession, float wx, float wy, float *pmx, float *pmy )
{
	if ( pmx != 0 ) *pmx = 0.0f;
	if ( pmy != 0 ) *pmy = 0.0f;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pmx == 0 || pmy == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		WorldToMap( wx, wy, pmx, pmy );
		return BK_EDITOR_OK;
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

BkEditorStatus BkEditorSounds( BkEditorSession *pSession, BkEditorSoundRecord *pOut, int nCapacity, int *pnCount )
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
		return ReadSessionSounds( pSession, pOut, nCapacity, pnCount ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

namespace {
// record's own name and position fields are within the ABI's own bounds -
// the caller-bug checks BkEditorAddSound/BkEditorSetSound document as
// BK_EDITOR_BAD_ARGUMENT. Everything past this (a name the database does
// not know as a sound, an off-map position, a bad time or radius) is a
// refusal, checked once, in session.cpp's ValidateSoundRecord, which both
// AddSoundToSession and SetSoundInSession call.
bool SoundRecordWellFormed( const BkEditorSoundRecord *pRecord )
{
	if ( pRecord == 0 )
		return false;
	if ( strnlen( pRecord->name, sizeof pRecord->name ) >= sizeof pRecord->name || pRecord->name[0] == 0 )
		return false;
	if ( !std::isfinite( pRecord->x ) || !std::isfinite( pRecord->y ) || !std::isfinite( pRecord->z ) )
		return false;
	return true;
}
}

BkEditorStatus BkEditorAddSound( BkEditorSession *pSession, int nIndex, const BkEditorSoundRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !SoundRecordWellFormed( pRecord ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		const int nCount = int( pSession->snapshot.sounds.sounds.size() );
		if ( nIndex < -1 || nIndex > nCount )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( AddSoundToSession( pSession, nIndex, *pRecord, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetSound( BkEditorSession *pSession, int nIndex, const BkEditorSoundRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !SoundRecordWellFormed( pRecord ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		const int nCount = int( pSession->snapshot.sounds.sounds.size() );
		if ( nIndex < 0 || nIndex >= nCount )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( SetSoundInSession( pSession, nIndex, *pRecord, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorDeleteSound( BkEditorSession *pSession, int nIndex )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		const int nCount = int( pSession->snapshot.sounds.sounds.size() );
		if ( nIndex < 0 || nIndex >= nCount )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( DeleteSoundFromSession( pSession, nIndex, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
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
		// D-28: stamped only when a mod is active and only when it actually
		// differs, so a save that changes nothing else does not mark the two
		// fields dirty for no reason. With no mod active they are left exactly
		// as SaveSessionMap will read and write them - the preservation
		// invariant (bridge.h's own BkEditorSaveMap comment).
		if ( !pSession->szModFolder.empty() &&
		     ( pSession->snapshot.szMODName != pSession->szModName || pSession->snapshot.szMODVersion != pSession->szModVersion ) )
		{
			pSession->snapshot.szMODName = pSession->szModName;
			pSession->snapshot.szMODVersion = pSession->szModVersion;
			pSession->working.szMODName = pSession->szModName;
			pSession->working.szMODVersion = pSession->szModVersion;
		}
		return SaveSessionMap( pSession, pszPath ) ? BK_EDITOR_OK : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorPaths( BkEditorSession *pSession, BkEditorPathSet *pOut )
{
	return Guarded( pSession, [pSession, pOut]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		const std::string &szBase = NPlatform::Paths::BaseRoot();
		const std::string &szUser = NPlatform::Paths::UserRoot();
		if ( szBase.size() >= sizeof( pOut->base_root ) || szUser.size() >= sizeof( pOut->user_root ) )
		{
			pSession->szMessage = "a root does not fit the caller's buffer";
			return BK_EDITOR_REFUSED;
		}
		memcpy( pOut->base_root, szBase.c_str(), szBase.size() + 1 );
		memcpy( pOut->user_root, szUser.c_str(), szUser.size() + 1 );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorTestMapPath( BkEditorSession *pSession, const char *pszProfile, const char *pszModFolder,
                                    const char *pszFileName, char *pOut, int nCapacity )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( nCapacity > 0 )
			pOut[0] = 0;
		if ( pszProfile == 0 || *pszProfile == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		const std::string szFileName = pszFileName != 0 ? pszFileName : std::string();
		if ( !IsBareTestMapName( szFileName ) )
		{
			pSession->szMessage = "\"" + szFileName + "\" is not a bare name ending in .bzm";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		// Lower-cased before ModKey, which keeps case: "Some Mod" and "some
		// mod" must be the same generated-data folder, the way the game's own
		// -mod= parsing already lower-cases the whole command line
		// (GameMain.cpp's ProcessCommandLine calls NStr::ToLower on szParams
		// before this bridge ever sees a mod name).
		std::string szMod = pszModFolder != 0 ? pszModFolder : std::string();
		NStr::ToLower( szMod );
		const std::string szModKey = NGeneratedData::ModKey( szMod );
		std::string szDir = ( std::filesystem::path( NProfile::GeneratedDirectory( pszProfile ) ) / szModKey / "maps" ).string();
		for ( char &c : szDir )
			if ( c == '/' ) c = '\\';
		const std::string szFull = szDir + "\\" + szFileName;
		if ( int( szFull.size() ) >= nCapacity )
		{
			pSession->szMessage = "the caller's buffer is too short for " + szFull;
			return BK_EDITOR_REFUSED;
		}
		NGeneratedData::CreateParentDirectories( szFull );
		// The game loads the newer of a same-stem .xml/.bzm pair
		// (GameTT/iMissionInternal.cpp), so a stale sibling from an older
		// test copy must not outrank the one this call is about to write.
		const std::string szStem = szFileName.substr( 0, szFileName.size() - 4 );
		std::string szSibling = szDir + "\\" + szStem + ".xml";
#if !defined(_WIN32)
		for ( char &c : szSibling )
			if ( c == '\\' ) c = '/';
#endif
		std::error_code error;
		std::filesystem::remove( szSibling, error );
		memcpy( pOut, szFull.c_str(), szFull.size() + 1 );
		return BK_EDITOR_OK;
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
		// The renderer outlives the session, and the overlay's function and
		// user data are the caller's, gone with its ImGui state: left in place,
		// the next frame or mode change would call into freed memory.
		if ( pSession->bEngineStarted )
		{
			if ( IGFX *pGFX = GetSingleton<IGFX>() )
				pGFX->SetOverlay( 0, 0 );
		}
		delete pSession;
	}
	catch ( ... )
	{
		return BK_EDITOR_FAILED;
	}
	return BK_EDITOR_OK;
}
}
