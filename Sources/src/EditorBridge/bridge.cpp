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
#include "../MapFile/MapRecords.h"
#include "../Main/iMain.h"
#include "../GFX/GFX.H"
#include "../Scene/Scene.h"
#include "../Scene/SceneScreenScale.h"
#include "../Scene/Terrain.h"
#include "../Formats/fmtTerrain.h"
#include "../RandomMapGen/TerrainGenerator.h"
#include "../Image/Image.h"
#include "../Platform/Paths.h"
#include "../StreamIO/RandomGen.h"
#include "../StreamIO/GeneratedData.h"
#include "../StreamIO/SeasonData.h"
#include "../StreamIO/ProfilePaths.h"
#include "../Main/GameDB.h"
#include "../Main/RPGStats.h"
#include "../MapFile/MapFile.h"
#include "../RandomMapGen/LA_Types.h"
#include "../RandomMapGen/MiniMap_Types.h"
#include "../AILogic/AILogic.h"
#include "../AILogic/aiconsts.h"
#include "../AILogic/AITypes.h"
#include <fstream>
#include <map>
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

// The user RMG root (D-09) is mounted before anything reads or writes a
// composer file or lists a folder: a cheap compare of the root the platform's
// user root and the active mod name now, a remount only when it changed (the
// engine tests re-point the user root while a session lives).
static void EnsureRmgMount( BkEditorSession *pSession )
{
	MountRmgRoot( pSession, pSession->szModFolder, pSession->pModStorage, false );
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
	// null or "": the installation this executable runs from, as every engine
	// module derives its own roots (NPlatform::Paths, SDL_GetBasePath), never
	// the working directory, which a shortcut, the Start menu or another
	// shell's directory sets to anything. Asked of SDL afresh, not
	// Paths::BaseRoot(): an earlier start in this process may have pointed
	// that elsewhere.
	if ( pszDataRoot != 0 && pszDataRoot[0] != 0 )
		pSession->szDataRoot = pszDataRoot;
	else
	{
		const char *pszBase = SDL_GetBasePath();
		pSession->szDataRoot = pszBase != 0 ? pszBase : ".";
	}
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
			// As the game does: the picked installation's generated season
			// textures, if it has them, below any mod mounted later.
			NSeasonData::Mount( pStorage );
			RegisterSingleton( IDataStorage::tidTypeID, pStorage );
			// D-09 (costly - see bridge.h): the user's RMG files resolve through
			// the storage like shipped data, from the first moment.
			MountRmgRoot( pSession, std::string(), 0, true );
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

namespace {
// A map position the engine can be asked for. IAIEditor::MoveObject takes shorts, so a NaN or
// anything beyond about +/-32768 collapses to one garbage value (ToEngineCoord), the readback
// converts the request the same way and agrees with it, and the map would keep the request.
bool IsUsableCoordinate( float f )
{
	return std::isfinite( f ) && std::fabs( f ) <= 32000.0f;
}
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
		if ( !IsUsableCoordinate( x ) || !IsUsableCoordinate( y ) )
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
	if ( nWhich == 0 && ( !IsUsableCoordinate( x ) || !IsUsableCoordinate( y ) ) )
		return BK_EDITOR_BAD_ARGUMENT;
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
		if ( !IsUsableCoordinate( x ) || !IsUsableCoordinate( y ) )
			return BK_EDITOR_BAD_ARGUMENT;
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

BkEditorStatus BkEditorSetPlacementGhost( BkEditorSession *pSession, const char *pszName, float fWorldX, float fWorldY, int nDir )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !std::isfinite( fWorldX ) || !std::isfinite( fWorldY ) )
			return BK_EDITOR_BAD_ARGUMENT;
		return SetGhostInSession( pSession, pszName, fWorldX, fWorldY, nDir ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorClearPlacementGhost( BkEditorSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ClearGhostInSession( pSession );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorPlacementGhost( BkEditorSession *pSession, int *pnShown, float *pfX, float *pfY, int *pnDir )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		const IVisObj *pGhost = static_cast<const IVisObj*>( pSession->pGhost.GetPtr() );
		if ( pnShown != 0 )
			*pnShown = pGhost != 0 ? 1 : 0;
		if ( pfX != 0 )
			*pfX = pGhost != 0 ? pGhost->GetPosition().x : 0.0f;
		if ( pfY != 0 )
			*pfY = pGhost != 0 ? pGhost->GetPosition().y : 0.0f;
		if ( pnDir != 0 )
			*pnDir = pGhost != 0 ? pGhost->GetDirection() : 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSnapToGrid( BkEditorSession *pSession, const char *pszName, float fX, float fY, float *pfOutX, float *pfOutY )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
		if ( pObjectsDB == 0 )
		{
			pSession->szMessage = "the engine has no object database";
			return BK_EDITOR_REFUSED;
		}
		// A name the database does not know is the caller's bug, toggle or
		// no toggle - the question is checked before the fit is applied.
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( pszName );
		if ( pDesc == 0 )
		{
			pSession->szMessage = "the object database does not know that type";
			return BK_EDITOR_REFUSED;
		}
		// The input is the answer unless the placement rule says otherwise,
		// so a null out pointer with fit on still makes sense (nothing to
		// read, nothing to change).
		if ( pfOutX != 0 )
			*pfOutX = fX;
		if ( pfOutY != 0 )
			*pfOutY = fY;
		if ( !pSession->bFitToGrid )
			return BK_EDITOR_OK;
		if ( pDesc->eGameType != SGVOGT_BUILDING && pDesc->eGameType != SGVOGT_OBJECT )
			return BK_EDITOR_OK;
		const SObjectBaseRPGStats *pRPG = static_cast<const SObjectBaseRPGStats*>( pObjectsDB->GetRPGStats( pDesc ) );
		if ( pRPG == 0 )
			return BK_EDITOR_OK;
		CVec3 vPos( fX, fY, 0 );
		FitVisOrigin2AIGrid( &vPos, pRPG->GetOrigin( -1 ) );
		if ( pfOutX != 0 )
			*pfOutX = vPos.x;
		if ( pfOutY != 0 )
			*pfOutY = vPos.y;
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

BkEditorStatus BkEditorMoveObjects( BkEditorSession *pSession, const int *pnLinkIDs, int nCount, float fDx, float fDy, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnToken == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !std::isfinite( fDx ) || !std::isfinite( fDy ) )
			return BK_EDITOR_BAD_ARGUMENT;
		// The loop below reads pnLinkIDs[0..nCount): a count with no array behind it is the caller's
		// bug, answered before anything is read.
		if ( nCount < 0 || ( nCount > 0 && pnLinkIDs == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		// A link ID named twice is the caller's bug - the move cannot say
		// which copy it meant - and is answered before anything is looked up.
		for ( int i = 0; i < nCount; ++i )
			for ( int j = 0; j < i; ++j )
				if ( pnLinkIDs[i] == pnLinkIDs[j] )
					return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( MoveObjectsInSession( pSession, pnLinkIDs, nCount, fDx, fDy, &bRefused, pnToken ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
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

// Altitudes (M3, D-19). Both entries share the caller-bug checks: a null
// region or buffer, an empty or inverted rectangle, a count that does not
// match the rectangle, a non-finite height. Everything else - a region off
// the open map, no map at all - is an ordinary refusal, and a refusal
// changes nothing.
namespace {
bool AltitudeRegionWellFormed( const BkEditorAltitudeRegion *pRegion, long long *pnArea )
{
	if ( pRegion == 0 )
		return false;
	if ( pRegion->x1 <= pRegion->x0 || pRegion->y1 <= pRegion->y0 )
		return false;
	*pnArea = static_cast<long long>( pRegion->x1 - pRegion->x0 ) *
	          static_cast<long long>( pRegion->y1 - pRegion->y0 );
	return true;
}
}

BkEditorStatus BkEditorAltitudes( BkEditorSession *pSession, const BkEditorAltitudeRegion *pRegion,
                                  float *pHeights, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pHeights == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		long long nArea = 0;
		if ( !AltitudeRegionWellFormed( pRegion, &nArea ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( nArea > 0x7fffffff )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		std::vector<float> heights;
		if ( !ReadAltitudesInSession( pSession, CTRect<int>( pRegion->x0, pRegion->y0, pRegion->x1, pRegion->y1 ), &heights ) )
			return BK_EDITOR_REFUSED;
		*pnCount = int( heights.size() );
		const int nFit = Min( nCapacity, int( heights.size() ) );
		for ( int i = 0; i < nFit; ++i )
			pHeights[i] = heights[i];
		if ( nFit < int( heights.size() ) )
		{
			pSession->szMessage = NStr::Format( "the region holds %d vertices, the buffer has room for %d",
			                                    int( heights.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSetAltitudes( BkEditorSession *pSession, const BkEditorAltitudeRegion *pRegion,
                                     const float *pHeights, int nCount, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nCount < 0 || ( nCount > 0 && pHeights == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		long long nArea = 0;
		if ( !AltitudeRegionWellFormed( pRegion, &nArea ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( nArea > 0x7fffffff || nCount != nArea )
			return BK_EDITOR_BAD_ARGUMENT;
		for ( int i = 0; i < nCount; ++i )
			if ( !std::isfinite( pHeights[i] ) )
			{
				pSession->szMessage = NStr::Format( "height %d is not a finite number", i );
				return BK_EDITOR_BAD_ARGUMENT;
			}
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		std::vector<float> heights( pHeights, pHeights + nCount );
		bool bRefused = false;
		int nToken = -1;
		if ( !ApplyAltitudesInSession( pSession, CTRect<int>( pRegion->x0, pRegion->y0, pRegion->x1, pRegion->y1 ),
		                               heights, &bRefused, &nToken ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 )
			*pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

// The mod switch without the Guarded wrapper, for BkEditorNewMap's own
// "otherwise a folder name that must exist in BkEditorMods" rule: same
// validation, same refusal rules, same steps. Defined beside BkEditorSetMod.
static BkEditorStatus SetModCore( BkEditorSession *pSession, const char *pszFolder );

// File > New (M3, D-23): the engine builds the map, the mod the params name
// is switched to first (nothing to switch for "" or the active one), and the
// summary answers what was built. A size outside 1..32 or a season outside
// 0..3 is the caller's mistake; an unknown mod folder is an ordinary
// refusal - and neither changes anything.
namespace {
// The summary of whatever map the session holds now - the same fields
// BkEditorOpenMap answers, from the same places.
void FillMapSummary( SEditorSession *pSession, BkEditorMapSummary *pOut )
{
	// Sizes and counts come from the snapshot, which is the file as it was
	// read (or the map as it was built); the working copy differs only in
	// frame indices and in the altitudes a map without any gets.
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
}

BkEditorStatus BkEditorNewMap( BkEditorSession *pSession, const BkEditorNewMapParams *pParams, BkEditorMapSummary *pOut )
{
	if ( pOut != 0 )
		memset( pOut, 0, sizeof *pOut );
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pParams == 0 || pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		// "Always terminated" is the caller's contract; both arrays are read as C strings below.
		if ( memchr( pParams->szName, 0, sizeof pParams->szName ) == 0 ||
		     memchr( pParams->szModFolder, 0, sizeof pParams->szModFolder ) == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( pParams->size_x < 1 || pParams->size_x > 32 ||
		     pParams->size_y < 1 || pParams->size_y > 32 )
		{
			pSession->szMessage = "a new map is 1..32 patches per axis";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		if ( pParams->season < 0 || pParams->season > 3 )
		{
			pSession->szMessage = "the season is Summer, Winter, Africa or Spring";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		// The mod the params name: "" keeps the current one (RMGC_CURRENT_MOD_
		// FOLDER's own meaning), "none" is none, anything else must be
		// installed. Only a real change switches - the switch closes the open
		// map and rebuilds the database, none of which a new map of the active
		// mod needs.
		const std::string szModFolder = pParams->szModFolder;
		if ( szModFolder != "none" && !szModFolder.empty() && szModFolder != pSession->szModFolder )
		{
			const BkEditorStatus switched = SetModCore( pSession, szModFolder.c_str() );
			if ( switched != BK_EDITOR_OK )
				return switched;
		}
		else if ( szModFolder == "none" && !pSession->szModFolder.empty() )
		{
			const BkEditorStatus switched = SetModCore( pSession, "" );
			if ( switched != BK_EDITOR_OK )
				return switched;
		}
		if ( !NewMapInSession( pSession, pParams->size_x, pParams->size_y, pParams->season, pParams->szName,
		                       pSession->szModName, pSession->szModVersion ) )
			return pSession->bEngineStarted ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		// A fresh map opens unzoomed (D-15), exactly an open's own rule.
		SetGlobalVar( "GFX.World.ZoomSteps", 0 );
		if ( IScene *pScene = GetSingleton<IScene>() )
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
				pTerrain->ResetPosition();
		FillMapSummary( pSession, pOut );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorHeightsStroke( BkEditorSession *pSession, const BkEditorHeightsStrokeParams *pParams, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pParams == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( pParams->action < 0 || pParams->action > 2 || pParams->level_mode < 0 || pParams->level_mode > 3 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( pParams->brush < 2 || pParams->brush > 16 )
		{
			pSession->szMessage = "the heights brush is 2..16";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		if ( !std::isfinite( pParams->height_speed ) || !std::isfinite( pParams->level_ratio_percent ) ||
		     !std::isfinite( pParams->pos_x ) || !std::isfinite( pParams->pos_y ) ||
		     !std::isfinite( pParams->click_x ) || !std::isfinite( pParams->click_y ) )
			return BK_EDITOR_BAD_ARGUMENT;
		SHeightsStroke stroke;
		stroke.nAction = pParams->action;
		stroke.nLevelMode = pParams->level_mode;
		stroke.nBrush = pParams->brush;
		stroke.fHeightSpeed = pParams->height_speed;
		stroke.fLevelRatioPercent = pParams->level_ratio_percent;
		stroke.vPos = CVec3( pParams->pos_x, pParams->pos_y, 0 );
		stroke.vClickRef = CVec3( pParams->click_x, pParams->click_y, 0 );
		stroke.bStrokeStart = pParams->stroke_start;
		stroke.bCtrlHeld = pParams->ctrl_held;
		bool bRefused = false;
		int nToken = -1;
		if ( !ApplyHeightsStrokeInSession( pSession, stroke, &bRefused, &nToken ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 )
			*pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorGenerateHeights( BkEditorSession *pSession, int nType, float fGranularity, float fMinZ, float fMaxZ, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		// The MFC dialog's three names (TabTerrainAltitudesDialog.cpp:250-278);
		// the hidden MULTI/HETERO radios are not features (editor.rc:503-507).
		if ( nType != TG_FBM && nType != TG_HYBRID && nType != TG_RIDGED )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !std::isfinite( fGranularity ) || !std::isfinite( fMinZ ) || !std::isfinite( fMaxZ ) || fGranularity <= 0.0f )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		int nToken = -1;
		if ( !GenerateHeightsInSession( pSession, nType, fGranularity, fMinZ, fMaxZ, &bRefused, &nToken ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 )
			*pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSetZeroHeights( BkEditorSession *pSession, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		bool bRefused = false;
		int nToken = -1;
		if ( !SetZeroHeightsInSession( pSession, &bRefused, &nToken ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 )
			*pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorUpdateMap( BkEditorSession *pSession, BkEditorProgressFn pfnProgress, void *pUser, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		bool bRefused = false;
		int nToken = -1;
		if ( !UpdateMapInSession( pSession, pfnProgress, pUser, &bRefused, &nToken ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 )
			*pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorFillEntireMap( BkEditorSession *pSession, int nTileIndex, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nTileIndex < 0 || nTileIndex > 255 )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		int nToken = -1;
		if ( !FillEntireMapInSession( pSession, nTileIndex, &bRefused, &nToken ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 )
			*pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSetTerrainModes( BkEditorSession *pSession, int bInstantUpdate, int bFitToGrid )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		SetTerrainModesInSession( pSession, bInstantUpdate, bFitToGrid );
		return BK_EDITOR_OK;
	} );
}
// The Layers menu (M3, D-32): renderer state, the BkEditorSetMapType shape - no
// map data, no history, never dirty. The work is session_layers.cpp's.
BkEditorStatus BkEditorSetLayerShow( BkEditorSession *pSession, int nLayer, int bShown )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetLayerInSession( pSession, nLayer, bShown );
	} );
}

BkEditorStatus BkEditorSetWireframe( BkEditorSession *pSession, int bOn )
{
	return BkEditorSetLayerShow( pSession, BK_EDITOR_LAYER_WIREFRAME, bOn );
}

BkEditorStatus BkEditorLayers( BkEditorSession *pSession, unsigned *pnBits, unsigned *pnMask )
{
	if ( pnBits != 0 )
		*pnBits = 0;
	if ( pnMask != 0 )
		*pnMask = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnBits != 0 )
			*pnBits = pSession->nLayerBits;
		if ( pnMask != 0 )
			*pnMask = LayerAvailableMask() | ( 1u << BK_EDITOR_LAYER_UNIT_FIRE_RANGES );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSetFireRangeMode( BkEditorSession *pSession, int nMode, const char *pszFilter, const int *pnLinkIDs, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetFireRangeInSession( pSession, nMode, pszFilter, pnLinkIDs, nCount );
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

namespace {

// BkEditorObjectPicture's and BkEditorTilePicture's shared tail: pImage
// scaled down to fit nMaxSide (keeping its shape) when it does not already,
// then written out as RGBA8, top row first. pszWhat names the picture in a
// message. The real size is reported even when the capacity is too short, so
// a caller sizing a buffer from a REFUSED answer has it.
BkEditorStatus WritePicture( BkEditorSession *pSession, IImageProcessor *pImages, CPtr<IImage> pImage, const char *pszWhat,
                             int nMaxSide, unsigned char *pOutRgba, int nCapacityBytes, int *pnOutWidth, int *pnOutHeight )
{
	int nWidth = pImage->GetSizeX(), nHeight = pImage->GetSizeY();
	if ( nWidth <= 0 || nHeight <= 0 )
	{
		pSession->szMessage = NStr::Format( "%s's picture decoded to an empty image", pszWhat );
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
			pSession->szMessage = NStr::Format( "could not scale %s's picture", pszWhat );
			return BK_EDITOR_FAILED;
		}
		pImage = pScaled;
		nWidth = nScaledWidth;
		nHeight = nScaledHeight;
	}
	*pnOutWidth = nWidth;
	*pnOutHeight = nHeight;
	const int nNeeded = nWidth * nHeight * 4;
	if ( nNeeded > nCapacityBytes )
	{
		pSession->szMessage = NStr::Format( "%s's picture needs %d bytes and room was given for %d", pszWhat, nNeeded, nCapacityBytes );
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
}

void ForgetTileAtlas( BkEditorSession *pSession )
{
	pSession->szTileAtlasName.clear();
	pSession->pTileAtlas = 0;
	pSession->tileAtlasDesc = STilesetDesc();
}

// The engine's terrain for the open map (session.cpp's EngineTerrain, which
// is not exported): what the map was loaded into, and so the tileset it
// paints with.
ITerrainEditor *OpenMapTerrain( BkEditorSession *pSession )
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene != 0 ? pScene->GetTerrain() : 0;
	ITerrainEditor *pEditor = pTerrain != 0 ? pTerrain->GetEditor() : 0;
	if ( pEditor == 0 )
		pSession->szMessage = "the engine has no terrain";
	return pEditor;
}

// The first terrain type of rTileset that lists nTile, or -1 when none does -
// the same "has a terrain type" rule BkEditorTilesetTiles offers tiles by and
// BkEditorPaint checks them against.
int TerrainTypeOfTile( const STilesetDesc &rTileset, int nTile )
{
	for ( size_t t = 0; t < rTileset.terrtypes.size(); ++t )
	{
		const std::vector<SMainTileDesc> &rTiles = rTileset.terrtypes[t].tiles;
		for ( size_t k = 0; k < rTiles.size(); ++k )
			if ( rTiles[k].nIndex == nTile )
				return int( t );
	}
	return -1;
}

// Loads (once per tileset name, see BkEditorSession::szTileAtlasName) the
// open map's tileset description from its own .xml - the way CTerrain::LoadLocal
// reads it, before its CorrectUVMaps - and its texture: "_h.dds", else
// "_c.dds", else "_l.dds" (RandomMapGen/IB_Types.h's GetDDSImageExtention
// suffixes; the MFC tile palette and the minimap builder read the "_h" one).
bool LoadTileAtlas( BkEditorSession *pSession, ITerrainEditor *pTerrain )
{
	const std::string &szName = pTerrain->GetTerrainInfo().szTilesetDesc;
	if ( pSession->pTileAtlas != 0 && pSession->szTileAtlasName == szName )
		return true;
	ForgetTileAtlas( pSession );
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	IImageProcessor *pImages = GetImageProcessor();
	if ( pStorage == 0 || pImages == 0 )
	{
		pSession->szMessage = "the engine is not started";
		return false;
	}
	STilesetDesc desc;
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( ( szName + ".xml" ).c_str(), STREAM_ACCESS_READ );
		if ( pStream == 0 )
		{
			pSession->szMessage = NStr::Format( "the tileset description %s.xml is not in the data", szName.c_str() );
			return false;
		}
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "tileset", &desc );
	}
	static const char *const suffixes[] = { "_h.dds", "_c.dds", "_l.dds" };
	CPtr<IImage> pAtlas;
	for ( int i = 0; i < 3 && pAtlas == 0; ++i )
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( ( szName + suffixes[i] ).c_str(), STREAM_ACCESS_READ );
		if ( pStream == 0 )
			continue;
		CPtr<IDDSImage> pDDS = pImages->LoadDDSImage( pStream );
		if ( pDDS != 0 )
			pAtlas = pImages->Decompress( pDDS );
	}
	if ( pAtlas == 0 || pAtlas->GetSizeX() <= 0 || pAtlas->GetSizeY() <= 0 )
	{
		pSession->szMessage = NStr::Format( "the tileset texture %s_h.dds (or _c/_l) did not load", szName.c_str() );
		return false;
	}
	pSession->szTileAtlasName = szName;
	pSession->pTileAtlas = pAtlas;
	pSession->tileAtlasDesc = desc;
	return true;
}

// One tile's diamond out of the tileset texture (see BkEditorTilePicture):
// the bounding box of its four corners, each pixel read from the cell
// mirrored the way the corners name it (maps0 is the tile's top corner, maps3
// its bottom, maps2 its left, maps1 its right - a tile whose maps3 lies above
// its maps0 in the texture is drawn upside down, the MFC palette's FlipY
// case), with an alpha that covers the diamond and fades over one pixel at
// its edge.
CPtr<IImage> CutTile( IImageProcessor *pImages, IImage *pAtlas, const STileMapsDesc &rMaps )
{
	const int nAtlasWidth = pAtlas->GetSizeX(), nAtlasHeight = pAtlas->GetSizeY();
	float fMinX = rMaps.maps[0].x, fMaxX = fMinX, fMinY = rMaps.maps[0].y, fMaxY = fMinY;
	for ( int k = 1; k < 4; ++k )
	{
		fMinX = Min( fMinX, rMaps.maps[k].x );
		fMaxX = Max( fMaxX, rMaps.maps[k].x );
		fMinY = Min( fMinY, rMaps.maps[k].y );
		fMaxY = Max( fMaxY, rMaps.maps[k].y );
	}
	const int nLeft = Clamp( int( std::floor( fMinX * nAtlasWidth + 0.5f ) ), 0, nAtlasWidth );
	const int nRight = Clamp( int( std::floor( fMaxX * nAtlasWidth + 0.5f ) ), 0, nAtlasWidth );
	const int nTop = Clamp( int( std::floor( fMinY * nAtlasHeight + 0.5f ) ), 0, nAtlasHeight );
	const int nBottom = Clamp( int( std::floor( fMaxY * nAtlasHeight + 0.5f ) ), 0, nAtlasHeight );
	const int nWidth = nRight - nLeft, nHeight = nBottom - nTop;
	if ( nWidth <= 0 || nHeight <= 0 )
		return 0;
	CPtr<IImage> pTile = pImages->CreateImage( nWidth, nHeight );
	if ( pTile == 0 )
		return 0;
	const bool bFlipY = rMaps.maps[3].y < rMaps.maps[0].y;
	const bool bFlipX = rMaps.maps[1].x < rMaps.maps[2].x;
	const SColor *pSource = pAtlas->GetLFB();
	SColor *pTarget = pTile->GetLFB();
	const float fHalfWidth = nWidth * 0.5f, fHalfHeight = nHeight * 0.5f;
	const float fEdge = Min( fHalfWidth, fHalfHeight );
	for ( int y = 0; y < nHeight; ++y )
	{
		const int nSourceY = nTop + ( bFlipY ? nHeight - 1 - y : y );
		for ( int x = 0; x < nWidth; ++x )
		{
			const int nSourceX = nLeft + ( bFlipX ? nWidth - 1 - x : x );
			const SColor color = pSource[nSourceY * nAtlasWidth + nSourceX];
			const float fDistance = std::fabs( x + 0.5f - fHalfWidth ) / fHalfWidth + std::fabs( y + 0.5f - fHalfHeight ) / fHalfHeight;
			const float fCover = Clamp( ( 1.0f - fDistance ) * fEdge + 0.5f, 0.0f, 1.0f );
			pTarget[y * nWidth + x] = SColor( BYTE( fCover * 255.0f + 0.5f ), BYTE( color.r ), BYTE( color.g ), BYTE( color.b ) );
		}
	}
	return pTile;
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
		return WritePicture( pSession, pImages, pImage, pszName, nMaxSide, pOutRgba, nCapacityBytes, pnOutWidth, pnOutHeight );
	} );
}

BkEditorStatus BkEditorCloseMap( BkEditorSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		CloseSessionMap( pSession );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorDescribeTile( BkEditorSession *pSession, int nTile, BkEditorTile *pOut )
{
	if ( pOut != 0 )
	{
		memset( pOut, 0, sizeof *pOut );
		pOut->terrain_index = -1;
	}
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 || nTile < 0 || nTile > 255 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		ITerrainEditor *pTerrain = OpenMapTerrain( pSession );
		if ( pTerrain == 0 )
			return BK_EDITOR_REFUSED;
		// The engine's loaded description: its terrain types are the .xml's
		// own (only the corners were pulled in when it was loaded).
		const STilesetDesc &rTileset = pTerrain->GetTilesetDesc();
		const int nType = TerrainTypeOfTile( rTileset, nTile );
		if ( nType < 0 )
		{
			pSession->szMessage = NStr::Format( "tile %d is not in the map's tileset", nTile );
			return BK_EDITOR_REFUSED;
		}
		pOut->terrain_index = nType;
		// The terrain type's own variant count (D-35): what the MFC's tile
		// properties dialog shows (TabTileEditDialog.cpp:318), for every
		// tile the tileset lists, tile 0 included.
		pOut->variant_count = int( rTileset.terrtypes[nType].tiles.size() );
		CopyBoundedField( pOut->terrain, sizeof pOut->terrain, rTileset.terrtypes[nType].szName );
		CopyBoundedField( pOut->tileset, sizeof pOut->tileset, pTerrain->GetTerrainInfo().szTilesetDesc );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorTilePicture( BkEditorSession *pSession, int nTile,
                                    unsigned char *pOutRgba, int nCapacityBytes, int nMaxSide,
                                    int *pnOutWidth, int *pnOutHeight )
{
	if ( pnOutWidth != 0 ) *pnOutWidth = 0;
	if ( pnOutHeight != 0 ) *pnOutHeight = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOutRgba == 0 || pnOutWidth == 0 || pnOutHeight == 0 || nCapacityBytes < 0 ||
		     nMaxSide < 8 || nMaxSide > 256 || nTile < 0 || nTile > 255 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		ITerrainEditor *pTerrain = OpenMapTerrain( pSession );
		if ( pTerrain == 0 )
			return BK_EDITOR_REFUSED;
		if ( !LoadTileAtlas( pSession, pTerrain ) )
			return BK_EDITOR_REFUSED;
		const STilesetDesc &rTileset = pSession->tileAtlasDesc;
		if ( TerrainTypeOfTile( rTileset, nTile ) < 0 || nTile >= int( rTileset.tilemaps.size() ) )
		{
			pSession->szMessage = NStr::Format( "tile %d is not in the map's tileset", nTile );
			return BK_EDITOR_REFUSED;
		}
		IImageProcessor *pImages = GetImageProcessor();
		CPtr<IImage> pTile = CutTile( pImages, pSession->pTileAtlas, rTileset.tilemaps[nTile] );
		const std::string szWhat = NStr::Format( "tile %d", nTile );
		if ( pTile == 0 )
		{
			pSession->szMessage = szWhat + "'s corners span no pixels of the tileset texture";
			return BK_EDITOR_FAILED;
		}
		return WritePicture( pSession, pImages, pTile, szWhat.c_str(), nMaxSide, pOutRgba, nCapacityBytes, pnOutWidth, pnOutHeight );
	} );
}

// ---------------------------------------------------------------------------
// The Minimap panel's reads and Create Minimap Images (M3 05-07, D-14..D-17).
// ---------------------------------------------------------------------------

namespace {

// CMapInfo::CreateMiniMapImage's own averaging of one tile (MapInfo_Static
// Methods_MiniMapCreation.cpp:94-161, which the MFC panel repeats per terrain
// type in CMiniMapTerrain::UpdateColor): the mean colour of the rectangle the
// tile's four corners span in the tileset texture, integer arithmetic and all.
// The only addition is the clamp to the texture, which the engine's own loop
// leaves to the data being right.
unsigned int AverageTileColor( IImage *pAtlas, const STileMapsDesc &rMaps )
{
	const int nWidth = pAtlas->GetSizeX(), nHeight = pAtlas->GetSizeY();
	const CVec2 *pVertices = rMaps.maps;
	CTRect<int> colorRect( ( pVertices[0].x * nWidth + pVertices[2].x * nWidth ) / 2,
	                       ( pVertices[0].y * nHeight + pVertices[2].y * nHeight ) / 2,
	                       ( pVertices[1].x * nWidth + pVertices[3].x * nWidth ) / 2,
	                       ( pVertices[1].y * nHeight + pVertices[3].y * nHeight ) / 2 );
	colorRect.Normalize();
	colorRect.minx = Clamp( colorRect.minx, 0, nWidth );
	colorRect.maxx = Clamp( colorRect.maxx, 0, nWidth );
	colorRect.miny = Clamp( colorRect.miny, 0, nHeight );
	colorRect.maxy = Clamp( colorRect.maxy, 0, nHeight );
	const SColor *pPixels = pAtlas->GetLFB();
	DWORD dwRed = 0, dwGreen = 0, dwBlue = 0;
	for ( int y = colorRect.miny; y < colorRect.maxy; ++y )
	{
		for ( int x = colorRect.minx; x < colorRect.maxx; ++x )
		{
			const SColor &rColor = pPixels[y * nWidth + x];
			dwRed += rColor.r;
			dwGreen += rColor.g;
			dwBlue += rColor.b;
		}
	}
	const int nArea = colorRect.Width() * colorRect.Height();
	if ( nArea > 0 )
	{
		dwRed /= nArea;
		dwGreen /= nArea;
		dwBlue /= nArea;
	}
	return ( ( dwRed & 0xFF ) << 16 ) | ( ( dwGreen & 0xFF ) << 8 ) | ( dwBlue & 0xFF );
}

// Engine form of a path (backslashes: OpenFileStream and CreateFileStream
// split on them only), and the host form of the same (the OS's own).
std::string EnginePathOf( std::string szPath )
{
	for ( size_t i = 0; i < szPath.size(); ++i )
		if ( szPath[i] == '/' )
			szPath[i] = '\\';
	return szPath;
}

std::string HostPathOf( std::string szPath )
{
#if !defined(_WIN32)
	for ( size_t i = 0; i < szPath.size(); ++i )
		if ( szPath[i] == '\\' )
			szPath[i] = '/';
#endif
	return szPath;
}

// "<dir>\<name>" with the map's .bzm or .xml taken off; false for any other
// extension.
bool MapBaseOf( const std::string &szMapPath, std::string *pszBase )
{
	const std::string::size_type nDot = szMapPath.find_last_of( '.' );
	const std::string::size_type nSep = szMapPath.find_last_of( "/\\" );
	if ( nDot == std::string::npos || ( nSep != std::string::npos && nDot < nSep ) )
		return false;
	std::string szExtension = szMapPath.substr( nDot );
	NStr::ToLower( szExtension );
	if ( szExtension != ".bzm" && szExtension != ".xml" )
		return false;
	*pszBase = szMapPath.substr( 0, nDot );
	return true;
}

// True for a full path in either of the engine's forms: POSIX absolute, a
// leading backslash, or a drive letter.
bool IsFullPath( const std::string &szPath )
{
	return !szPath.empty() && ( szPath[0] == '/' || szPath[0] == '\\' || ( szPath.size() > 1 && szPath[1] == ':' ) );
}

// Whether the path lies inside the installation's Data folder (the shipped
// data, which is never written): the same prefix test the editor's own
// shipped-map rule starts with (core/shipped.zig, rule 1), case and separator
// blind, on the paths as the file system resolves them (the installation's
// root may be given relative, and a path may carry "..").
bool IsUnderInstalledData( const std::string &szPath )
{
	std::error_code error;
	const std::filesystem::path data = std::filesystem::weakly_canonical( std::filesystem::path( HostPathOf( NPlatform::Paths::BaseRoot() ) ) / "Data", error );
	if ( error )
		return false;
	const std::filesystem::path map = std::filesystem::weakly_canonical( std::filesystem::path( HostPathOf( szPath ) ), error );
	if ( error )
		return false;
	std::string szData = data.generic_string(), szMap = map.generic_string();
	for ( size_t i = 0; i < szData.size(); ++i )
		szData[i] = char( tolower( (unsigned char)szData[i] ) );
	for ( size_t i = 0; i < szMap.size(); ++i )
		szMap[i] = char( tolower( (unsigned char)szMap[i] ) );
	if ( !szData.empty() && szData[szData.size() - 1] != '/' )
		szData += '/';
	return szMap.compare( 0, szData.size(), szData ) == 0;
}

// A little-endian field of a file's header.
unsigned int HeaderWord( const unsigned char *p, int nBytes )
{
	unsigned int n = 0;
	for ( int i = nBytes - 1; i >= 0; --i )
		n = ( n << 8 ) | p[i];
	return n;
}

// The width and height a written .tga or .dds file's own header names, or
// false when the file is not there or too short to say.
bool ImageFileSize( const std::string &szHostPath, bool bDDS, int *pnWidth, int *pnHeight )
{
	std::ifstream file( szHostPath.c_str(), std::ios::binary );
	if ( !file )
		return false;
	unsigned char header[20] = { 0 };
	file.read( reinterpret_cast<char *>( header ), sizeof header );
	if ( file.gcount() < ( bDDS ? 20 : 16 ) )
		return false;
	if ( bDDS )
	{
		if ( header[0] != 'D' || header[1] != 'D' || header[2] != 'S' || header[3] != ' ' )
			return false;
		*pnHeight = int( HeaderWord( header + 12, 4 ) );
		*pnWidth = int( HeaderWord( header + 16, 4 ) );
	}
	else
	{
		*pnWidth = int( HeaderWord( header + 12, 2 ) );
		*pnHeight = int( HeaderWord( header + 14, 2 ) );
	}
	return true;
}

// The objects' markers, MiniMapTypes.cpp's CUnitsSelection::Update over the
// session's working copy (the engine's own copy: the frame indices unpacked).
void CollectMinimapUnits( BkEditorSession *pSession, std::vector<BkEditorMinimapUnit> *pUnits )
{
	pUnits->clear();
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
		return;
	const CMapInfo &rMap = pSession->working;
	const CTRect<int> aiRect( 0, 0, rMap.terrain.tiles.GetSizeX() * 2, rMap.terrain.tiles.GetSizeY() * 2 );
	const std::vector<SMapObjectInfo> *lists[2] = { &rMap.objects, &rMap.scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
	{
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
		{
			const SMapObjectInfo &rObject = ( *lists[nList] )[i];
			const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( rObject.szName.c_str() );
			if ( pDesc == 0 )
				continue;
			const bool bSquad = pDesc->eGameType == SGVOGT_SQUAD;
			const SObjectBaseRPGStats *pRPG = 0;
			if ( !bSquad && IsObjectHasPassability( pDesc->eGameType ) && ( pDesc->eGameType != SGVOGT_TERRAOBJ || rObject.nFrameIndex >= 0 ) )
				pRPG = static_cast<const SObjectBaseRPGStats *>( pObjectsDB->GetRPGStats( pDesc ) );
			CTRect<int> position( 0, 0, 0, 0 );
			CTPoint<int> center( 0, 0 );
			if ( pRPG != 0 )
			{
				const CVec2 &rOrigin = pRPG->GetOrigin( rObject.nFrameIndex );
				const CArray2D<BYTE> &rPassability = pRPG->GetPassability( rObject.nFrameIndex );
				const CTPoint<int> start( int( ( rObject.vPos.x - rOrigin.x + ( SAIConsts::TILE_SIZE / 2.0 ) ) / SAIConsts::TILE_SIZE ),
				                          int( ( rObject.vPos.y - rOrigin.y + ( SAIConsts::TILE_SIZE / 2.0 ) ) / SAIConsts::TILE_SIZE ) );
				position.minx = start.x;
				position.miny = start.y;
				position.maxx = start.x + rPassability.GetSizeX();
				position.maxy = start.y + rPassability.GetSizeY();
				center.x = position.minx + position.Width() / 2;
				center.y = position.miny + position.Height() / 2;
			}
			else
			{
				center.x = int( rObject.vPos.x / SAIConsts::TILE_SIZE );
				center.y = int( rObject.vPos.y / SAIConsts::TILE_SIZE );
				position.minx = center.x;
				position.miny = center.y;
				position.maxx = position.minx;
				position.maxy = position.miny;
			}
			if ( position.Width() < 5 )
			{
				position.minx = center.x - 2;
				position.maxx = center.x + 2;
			}
			if ( position.Height() < 5 )
			{
				position.miny = center.y - 2;
				position.maxy = center.y + 2;
			}
			if ( ValidateIndices( aiRect, &position ) < 0 )
				continue;
			BkEditorMinimapUnit unit;
			memset( &unit, 0, sizeof unit );
			unit.link_id = rObject.link.nLinkID;
			unit.x0 = position.minx;
			unit.y0 = position.miny;
			unit.x1 = position.maxx + 1;
			unit.y1 = position.maxy + 1;
			unit.color_index = ( rObject.nPlayer >= 0 && rObject.nPlayer < 17 ) ? rObject.nPlayer : 16;
			unit.squad = bSquad ? 1 : 0;
			pUnits->push_back( unit );
		}
	}
}

}

BkEditorStatus BkEditorTiles( BkEditorSession *pSession, const BkEditorTileRegion *pRegion,
                              unsigned char *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || pRegion == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( pRegion->x1 <= pRegion->x0 || pRegion->y1 <= pRegion->y0 )
			return BK_EDITOR_BAD_ARGUMENT;
		const long long nArea = ( (long long)pRegion->x1 - pRegion->x0 ) * ( (long long)pRegion->y1 - pRegion->y0 );
		if ( nArea > 0x7fffffff )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		const CArray2D<SMainTileInfo> &rTiles = pSession->snapshot.terrain.tiles;
		if ( pRegion->x0 < 0 || pRegion->y0 < 0 || pRegion->x1 > rTiles.GetSizeX() || pRegion->y1 > rTiles.GetSizeY() )
		{
			pSession->szMessage = NStr::Format( "the region %d,%d-%d,%d is not on the %dx%d map", pRegion->x0, pRegion->y0,
			                                    pRegion->x1, pRegion->y1, rTiles.GetSizeX(), rTiles.GetSizeY() );
			return BK_EDITOR_REFUSED;
		}
		*pnCount = int( nArea );
		if ( nCapacity < int( nArea ) )
		{
			pSession->szMessage = NStr::Format( "the region holds %d tiles, the buffer has room for %d", int( nArea ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		int nAt = 0;
		for ( int y = pRegion->y0; y < pRegion->y1; ++y )
			for ( int x = pRegion->x0; x < pRegion->x1; ++x )
				pOut[nAt++] = rTiles[y][x].tile;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorMinimapTileColors( BkEditorSession *pSession, unsigned int *pOut, int nCapacity, int *pnCount )
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
		ITerrainEditor *pTerrain = OpenMapTerrain( pSession );
		if ( pTerrain == 0 || !LoadTileAtlas( pSession, pTerrain ) )
			return BK_EDITOR_REFUSED;
		const STilesetDesc &rTileset = pSession->tileAtlasDesc;
		const int nTiles = Min( int( rTileset.tilemaps.size() ), 256 );
		*pnCount = nTiles;
		const int nFit = Min( nCapacity, nTiles );
		std::map<int, unsigned int> byRepresentative;
		for ( int nTile = 0; nTile < nFit; ++nTile )
		{
			// The MFC colours a terrain type by its first tile (UpdateColor).
			const int nType = TerrainTypeOfTile( rTileset, nTile );
			int nRepresentative = nTile;
			if ( nType >= 0 && !rTileset.terrtypes[nType].tiles.empty() )
			{
				const int nFirst = rTileset.terrtypes[nType].tiles[0].nIndex;
				if ( nFirst >= 0 && nFirst < int( rTileset.tilemaps.size() ) )
					nRepresentative = nFirst;
			}
			std::map<int, unsigned int>::const_iterator iFound = byRepresentative.find( nRepresentative );
			if ( iFound == byRepresentative.end() )
				iFound = byRepresentative.insert( std::make_pair( nRepresentative, AverageTileColor( pSession->pTileAtlas, rTileset.tilemaps[nRepresentative] ) ) ).first;
			pOut[nTile] = iFound->second;
		}
		if ( nFit < nTiles )
		{
			pSession->szMessage = NStr::Format( "the tileset has %d tiles, the buffer has room for %d", nTiles, nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorMinimapUnits( BkEditorSession *pSession, BkEditorMinimapUnit *pOut, int nCapacity, int *pnCount )
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
		std::vector<BkEditorMinimapUnit> units;
		CollectMinimapUnits( pSession, &units );
		*pnCount = int( units.size() );
		const int nFit = Min( nCapacity, int( units.size() ) );
		for ( int i = 0; i < nFit; ++i )
			pOut[i] = units[i];
		if ( nFit < int( units.size() ) )
		{
			pSession->szMessage = NStr::Format( "the map has %d markers, the buffer has room for %d", int( units.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorMinimapAreas( BkEditorSession *pSession, BkEditorMinimapArea *pOut, int nCapacity, int *pnCount )
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
		IAILogic *pAILogic = GetSingleton<IAILogic>();
		if ( pAILogic == 0 )
		{
			pSession->szMessage = "the AI is not there";
			return BK_EDITOR_REFUSED;
		}
		// CAILogic::UpdateShootAreas sets nothing for an area type it was
		// never told to show, so both outputs start at "none".
		SShootAreas *pAreas = 0;
		int nAreas = 0;
		pAILogic->UpdateShootAreas( &pAreas, &nAreas );
		std::vector<BkEditorMinimapArea> areas;
		if ( pAreas != 0 )
		{
			for ( int i = 0; i < nAreas; ++i )
			{
				for ( std::list<SShootArea>::const_iterator it = pAreas[i].areas.begin(); it != pAreas[i].areas.end(); ++it )
				{
					// The MFC draws every area but the line ones.
					if ( it->eType == SShootArea::ESAT_LINE )
						continue;
					BkEditorMinimapArea area;
					memset( &area, 0, sizeof area );
					area.kind = int( it->eType );
					area.cx = it->vCenter3D.x;
					area.cy = it->vCenter3D.y;
					area.radius = it->fMaxR;
					area.min_radius = it->fMinR;
					area.start_angle = it->wStartAngle;
					area.finish_angle = it->wFinishAngle;
					area.rgb = it->GetColor() & 0x00FFFFFF;
					areas.push_back( area );
				}
			}
		}
		*pnCount = int( areas.size() );
		const int nFit = Min( nCapacity, int( areas.size() ) );
		for ( int i = 0; i < nFit; ++i )
			pOut[i] = areas[i];
		if ( nFit < int( areas.size() ) )
		{
			pSession->szMessage = NStr::Format( "%d areas are shown, the buffer has room for %d", int( areas.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorMinimapImage( BkEditorSession *pSession, const char *pszMapPath,
                                     unsigned char *pOutRgba, int nCapacityBytes, int nMaxSide,
                                     int *pnOutWidth, int *pnOutHeight )
{
	if ( pnOutWidth != 0 ) *pnOutWidth = 0;
	if ( pnOutHeight != 0 ) *pnOutHeight = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszMapPath == 0 || *pszMapPath == 0 || pOutRgba == 0 || pnOutWidth == 0 || pnOutHeight == 0 ||
		     nCapacityBytes < 0 || nMaxSide < 8 || nMaxSide > 2048 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		IImageProcessor *pImages = GetImageProcessor();
		if ( pImages == 0 )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		std::string szBase;
		if ( !MapBaseOf( pszMapPath, &szBase ) )
		{
			pSession->szMessage = "the map path does not end in .bzm or .xml";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		szBase = EnginePathOf( szBase );
		// <map>.tga first, then <map>_h.dds, as CMiniMapDialog::UpdateControls
		// and CMiniMapTerrain::Update look. A file that is not there is a
		// null stream; one that will not decode is a null image.
		// The file is asked for first: OpenFileStream opens a storage on the
		// file's folder and has no answer for one that is not there.
		// A malformed picture (T-05-07-01) is a refusal, never a crash: the
		// decoders may throw, which here means "no picture".
		std::error_code error;
		CPtr<IImage> pImage;
		try
		{
			if ( std::filesystem::exists( HostPathOf( szBase + ".tga" ), error ) )
			{
				CPtr<IDataStream> pStream = OpenFileStream( szBase + ".tga", STREAM_ACCESS_READ );
				if ( pStream != 0 )
					pImage = pImages->LoadImage( pStream );
			}
			if ( pImage == 0 && std::filesystem::exists( HostPathOf( szBase + "_h.dds" ), error ) )
			{
				CPtr<IDataStream> pStream = OpenFileStream( szBase + "_h.dds", STREAM_ACCESS_READ );
				if ( pStream != 0 )
				{
					CPtr<IDDSImage> pDDS = pImages->LoadDDSImage( pStream );
					if ( pDDS != 0 )
						pImage = pImages->Decompress( pDDS );
				}
			}
		}
		catch ( ... )
		{
			pImage = 0;
		}
		if ( pImage != 0 && ( pImage->GetSizeX() <= 0 || pImage->GetSizeY() <= 0 ) )
			pImage = 0;
		if ( pImage == 0 )
		{
			pSession->szMessage = "the map has no minimap picture (<map>.tga or <map>_h.dds), or it will not decode";
			return BK_EDITOR_REFUSED;
		}
		return WritePicture( pSession, pImages, pImage, "the minimap picture", nMaxSide, pOutRgba, nCapacityBytes, pnOutWidth, pnOutHeight );
	} );
}

BkEditorStatus BkEditorCreateMiniMapImage( BkEditorSession *pSession, const char *pszMapPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszMapPath == 0 || *pszMapPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		std::string szBase;
		if ( !MapBaseOf( pszMapPath, &szBase ) )
		{
			pSession->szMessage = "the map path does not end in .bzm or .xml";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		if ( !IsFullPath( pszMapPath ) )
		{
			pSession->szMessage = "minimap images are written beside a saved map: give the map's full path (a shipped or never-saved map is saved with Save As first)";
			return BK_EDITOR_REFUSED;
		}
		if ( IsUnderInstalledData( pszMapPath ) )
		{
			pSession->szMessage = "the map is inside the installation's Data folder, which is never written: Save As a copy first";
			return BK_EDITOR_REFUSED;
		}
		const std::string szEngineMap = EnginePathOf( pszMapPath );
		const std::string szEngineBase = EnginePathOf( szBase );
		std::error_code error;
		if ( !std::filesystem::exists( HostPathOf( szEngineMap ), error ) )
		{
			pSession->szMessage = "the map file is not there to read: save the map first";
			return BK_EDITOR_REFUSED;
		}
		CMapInfo mapInfo;
		std::string szReadError;
		if ( !NMapFile::Read( szEngineMap.c_str(), &mapInfo, &szReadError ) )
		{
			pSession->szMessage = "the saved map does not read: " + szReadError;
			return BK_EDITOR_REFUSED;
		}
		mapInfo.UnpackFrameIndices();

		// What this writes, and the size of each: the MFC's own four
		// parameters, the DDS ones writing the engine's "_c/_l/_h" trio.
		struct SOutput { const char *pszName; const char *pszSuffix; bool bDDS; int nSize; };
		static const SOutput outputs[] =
		{
			{ "_large", ".tga", false, 512 }, { "_large", "_c.dds", true, 512 }, { "_large", "_l.dds", true, 512 }, { "_large", "_h.dds", true, 512 },
			{ "", ".tga", false, 256 }, { "", "_c.dds", true, 256 }, { "", "_l.dds", true, 256 }, { "", "_h.dds", true, 256 },
		};
		// Stale pictures from an earlier run must not pass the check below.
		for ( size_t i = 0; i < sizeof outputs / sizeof outputs[0]; ++i )
			std::filesystem::remove( HostPathOf( szEngineBase + outputs[i].pszName + outputs[i].pszSuffix ), error );

		CRMImageCreateParameterList imageCreateParameterList;
		imageCreateParameterList.push_back( SRMImageCreateParameter( szEngineBase + "_large", CTPoint<int>( 0x200, 0x200 ), true, false,
			SRMImageCreateParameter::INTERMISSION_IMAGE_BRIGHTNESS, SRMImageCreateParameter::INTERMISSION_IMAGE_CONSTRAST, SRMImageCreateParameter::INTERMISSION_IMAGE_GAMMA ) );
		imageCreateParameterList.push_back( SRMImageCreateParameter( szEngineBase, CTPoint<int>( 0x100, 0x100 ), true ) );
		imageCreateParameterList.push_back( SRMImageCreateParameter( szEngineBase + "_large", CTPoint<int>( 0x200, 0x200 ), false, false,
			SRMImageCreateParameter::INTERMISSION_IMAGE_BRIGHTNESS, SRMImageCreateParameter::INTERMISSION_IMAGE_CONSTRAST, SRMImageCreateParameter::INTERMISSION_IMAGE_GAMMA ) );
		imageCreateParameterList.push_back( SRMImageCreateParameter( szEngineBase, CTPoint<int>( 0x100, 0x100 ), false ) );
		if ( !mapInfo.CreateMiniMapImage( imageCreateParameterList ) )
		{
			pSession->szMessage = "CMapInfo::CreateMiniMapImage failed";
			return BK_EDITOR_FAILED;
		}
		// The BkEditorSaveMap habit: what was written is read back and
		// compared with what was meant before the call says OK.
		for ( size_t i = 0; i < sizeof outputs / sizeof outputs[0]; ++i )
		{
			const std::string szFile = szEngineBase + outputs[i].pszName + outputs[i].pszSuffix;
			int nWidth = 0, nHeight = 0;
			if ( !ImageFileSize( HostPathOf( szFile ), outputs[i].bDDS, &nWidth, &nHeight ) )
			{
				pSession->szMessage = "the minimap picture " + szFile + " was not written";
				return BK_EDITOR_FAILED;
			}
			if ( nWidth != outputs[i].nSize || nHeight != outputs[i].nSize )
			{
				pSession->szMessage = NStr::Format( "the minimap picture %s is %dx%d, not %dx%d", szFile.c_str(), nWidth, nHeight, outputs[i].nSize, outputs[i].nSize );
				return BK_EDITOR_FAILED;
			}
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
		return SetModCore( pSession, pszFolder );
	} );
}

static BkEditorStatus SetModCore( BkEditorSession *pSession, const char *pszFolder )
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
			ForgetTileAtlas( pSession );
			pSession->pModStorage = 0;
			MountRmgRoot( pSession, std::string(), 0, true );
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
		ForgetTileAtlas( pSession );
		// The mod's layer and the user's RMG root swap together (D-09): the root of
		// the new mod's own rmg folder sits below the mod layer.
		pSession->pModStorage = pModStorage;
		MountRmgRoot( pSession, szFolder, pModStorage, true );
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

BkEditorStatus BkEditorPickObjects( BkEditorSession *pSession, float fSx0, float fSy0, float fSx1, float fSy1,
                                    int *pnOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pnOut == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !std::isfinite( fSx0 ) || !std::isfinite( fSy0 ) || !std::isfinite( fSx1 ) || !std::isfinite( fSy1 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( PickObjectsInSession( pSession, fSx0, fSy0, fSx1, fSy1, pnOut, nCapacity, pnCount, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorPickObjectsInTiles( BkEditorSession *pSession, int nTx0, int nTy0, int nTx1, int nTy1,
                                           int *pnOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pnOut == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( PickObjectsInTilesInSession( pSession, nTx0, nTy0, nTx1, nTy1, pnOut, nCapacity, pnCount, &bRefused ) )
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

BkEditorStatus BkEditorCameraAnchors( BkEditorSession *pSession, BkEditorCameraAnchorRecord *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( ReadSessionCameraAnchors( pSession, pOut, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

namespace {
// The caller-bug checks BkEditorSetCameraAnchors documents as
// BK_EDITOR_BAD_ARGUMENT: a record, a count the ABI carries and finite
// coordinates. Everything past this (an anchor off the map, a file with too
// many anchors) is a refusal decided once, in session_records.cpp.
bool CameraAnchorsWellFormed( const BkEditorCameraAnchorRecord *pAnchors )
{
	if ( pAnchors == 0 )
		return false;
	if ( pAnchors->player_count < 0 || pAnchors->player_count > int( sizeof pAnchors->players / sizeof pAnchors->players[0] ) )
		return false;
	if ( !std::isfinite( pAnchors->neutral.x ) || !std::isfinite( pAnchors->neutral.y ) || !std::isfinite( pAnchors->neutral.z ) )
		return false;
	for ( int i = 0; i < pAnchors->player_count; ++i )
		if ( !std::isfinite( pAnchors->players[i].x ) || !std::isfinite( pAnchors->players[i].y ) || !std::isfinite( pAnchors->players[i].z ) )
			return false;
	return true;
}
}

BkEditorStatus BkEditorSetCameraAnchors( BkEditorSession *pSession, const BkEditorCameraAnchorRecord *pAnchors )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !CameraAnchorsWellFormed( pAnchors ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( SetSessionCameraAnchors( pSession, *pAnchors, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorGroundHeight( BkEditorSession *pSession, float fX, float fY, float *pfZ )
{
	if ( pfZ != 0 )
		*pfZ = 0.0f;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pfZ == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return GroundHeightInSession( pSession, fX, fY, pfZ ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorSetObjectScriptID( BkEditorSession *pSession, int nLinkID, int nScriptID )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( SetSessionObjectScriptID( pSession, nLinkID, nScriptID, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetObjectFields( BkEditorSession *pSession, int nLinkID, const BkEditorObjectFieldsEdit *pEdit, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnToken == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( SetObjectFieldsInSession( pSession, nLinkID, pEdit, &bRefused, pnToken ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorCanLink( BkEditorSession *pSession, int nSource, int nTarget, int *pnType )
{
	if ( pnType != 0 )
		*pnType = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnType == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( CanLinkInSession( pSession, nSource, nTarget, pnType, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetLink( BkEditorSession *pSession, int nSource, int nTarget, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnToken == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( SetLinkInSession( pSession, nSource, nTarget, &bRefused, pnToken ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorUnlink( BkEditorSession *pSession, int nLinkID, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnToken == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( UnlinkInSession( pSession, nLinkID, &bRefused, pnToken ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorAddPlayer( BkEditorSession *pSession, int nSide, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnToken == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( AddPlayerToSession( pSession, nSide, &bRefused, pnToken ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorDeletePlayer( BkEditorSession *pSession, int nPlayer, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnToken == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( DeletePlayerFromSession( pSession, nPlayer, &bRefused, pnToken ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorUnitCreation( BkEditorSession *pSession, int nPlayer, BkEditorUnitCreationRecord *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( ReadSessionUnitCreation( pSession, nPlayer, pOut, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

namespace {
// The caller-bug checks BkEditorSetUnitCreation documents as
// BK_EDITOR_BAD_ARGUMENT: a record whose names end inside their arrays, counts
// the arrays carry, a slot count of 0..16, finite points.
// Whether a NAME is a known one is the session's, a refusal.
bool UnitCreationWellFormed( int nPlayer, const BkEditorUnitCreationRecord *pRecord )
{
	if ( pRecord == 0 || nPlayer < 0 || nPlayer >= 16 )
		return false;
	if ( pRecord->slot_count < 0 || pRecord->slot_count > 16 )
		return false;
	if ( pRecord->appear_count < 0 || pRecord->appear_count > int( sizeof pRecord->appear / sizeof pRecord->appear[0] ) )
		return false;
	if ( memchr( pRecord->party, 0, sizeof pRecord->party ) == 0 || memchr( pRecord->paratroop_name, 0, sizeof pRecord->paratroop_name ) == 0 )
		return false;
	for ( int i = 0; i < 5; ++i )
		if ( memchr( pRecord->aircraft[i].name, 0, sizeof pRecord->aircraft[i].name ) == 0 )
			return false;
	for ( int i = 0; i < pRecord->appear_count; ++i )
		if ( !std::isfinite( pRecord->appear[i].x ) || !std::isfinite( pRecord->appear[i].y ) || !std::isfinite( pRecord->appear[i].z ) )
			return false;
	return true;
}
}

BkEditorStatus BkEditorSetUnitCreation( BkEditorSession *pSession, int nPlayer, const BkEditorUnitCreationRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !UnitCreationWellFormed( nPlayer, pRecord ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( SetSessionUnitCreation( pSession, nPlayer, *pRecord, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorUnitCreationChoices( BkEditorSession *pSession, int nKind, BkEditorUcName *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( pOut == 0 && nCapacity > 0 ) || nKind < 0 || nKind > 2 )
			return BK_EDITOR_BAD_ARGUMENT;
		std::vector<std::string> names;
		if ( !ListUnitCreationChoices( pSession, nKind, &names ) )
			return BK_EDITOR_FAILED;
		*pnCount = int( names.size() );
		const int nWrite = Min( int( names.size() ), nCapacity );
		for ( int i = 0; i < nWrite; ++i )
		{
			memset( &pOut[i], 0, sizeof pOut[i] );
			const size_t nCopy = Min( names[i].size(), sizeof pOut[i].name - 1 );
			memcpy( pOut[i].name, names[i].c_str(), nCopy );
		}
		if ( int( names.size() ) > nCapacity )
		{
			pSession->szMessage = NStr::Format( "the list holds %d names and room was given for %d", int( names.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorDamageObject( BkEditorSession *pSession, int nLinkID, float fDelta, int nMode, int *pnToken )
{
	if ( pnToken != 0 )
		*pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnToken == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		// A mode out of the three or a percentage out of 0..1 is the
		// caller's bug, answered before anything is looked up.
		if ( nMode < 0 || nMode > 2 || !std::isfinite( fDelta ) || fDelta < 0.0f || fDelta > 1.0f )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( DamageObjectInSession( pSession, nLinkID, fDelta, nMode, &bRefused, pnToken ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorScriptFile( BkEditorSession *pSession, BkEditorScriptFileRecord *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( ReadSessionScriptFile( pSession, pOut, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetScriptFile( BkEditorSession *pSession, const BkEditorScriptFileRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		// A name that is not terminated inside the record is a caller bug, not a
		// refusal: there is no string to judge.
		if ( pRecord == 0 || memchr( pRecord->name, 0, sizeof pRecord->name ) == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( SetSessionScriptFile( pSession, pRecord->name, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

namespace {
// A record the ABI takes as an area: present, its name terminated inside the
// record, a type the file has and finite numbers. What is past this (an empty or
// taken name, a centre off the map, a negative size) is a refusal decided once,
// in session_records.cpp.
// A world coordinate a drag or an area gesture hands the ABI: finite and
// within +-1e6 (a map is a few tens of thousands of world units across), so
// every float-to-int conversion behind it (PlanBridge's span count, the
// areas' Vis2AI, the fence tiles) is defined and gives the same answer on
// every platform (WR-A09).
const float fMaxGestureCoordinate = 1.0e6f;

bool SaneCoordinate( float fValue )
{
	return std::isfinite( fValue ) && std::fabs( fValue ) <= fMaxGestureCoordinate;
}

bool AreaRecordWellFormed( const BkEditorScriptAreaRecord *pRecord )
{
	if ( pRecord == 0 || memchr( pRecord->name, 0, sizeof pRecord->name ) == 0 )
		return false;
	if ( pRecord->type != 0 && pRecord->type != 1 )
		return false;
	return std::isfinite( pRecord->cx ) && std::isfinite( pRecord->cy ) && std::isfinite( pRecord->hx ) &&
	       std::isfinite( pRecord->hy ) && std::isfinite( pRecord->r );
}

SScriptArea AreaOf( const BkEditorScriptAreaRecord &rRecord )
{
	SScriptArea area;
	area.eType = rRecord.type == 0 ? SScriptArea::EAT_RECTANGLE : SScriptArea::EAT_CIRCLE;
	area.szName = rRecord.name;
	area.center = CVec2( rRecord.cx, rRecord.cy );
	area.vAABBHalfSize = CVec2( rRecord.hx, rRecord.hy );
	area.fR = rRecord.r;
	return area;
}

void FillAreaRecord( const SScriptArea &rArea, BkEditorScriptAreaRecord *pOut )
{
	memset( pOut, 0, sizeof *pOut );
	const size_t nLength = Min( rArea.szName.size(), sizeof pOut->name - 1 );
	memcpy( pOut->name, rArea.szName.c_str(), nLength );
	pOut->type = int( rArea.eType );
	pOut->cx = rArea.center.x;
	pOut->cy = rArea.center.y;
	pOut->hx = rArea.vAABBHalfSize.x;
	pOut->hy = rArea.vAABBHalfSize.y;
	pOut->r = rArea.fR;
}
}

BkEditorStatus BkEditorScriptAreas( BkEditorSession *pSession, BkEditorScriptAreaRecord *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( pOut == 0 && nCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( ReadSessionScriptAreas( pSession, pOut, nCapacity, pnCount, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorAddScriptArea( BkEditorSession *pSession, int nIndex, const BkEditorScriptAreaRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !AreaRecordWellFormed( pRecord ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < -1 || nIndex > int( pSession->snapshot.scriptAreas.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( AddScriptAreaToSession( pSession, nIndex, *pRecord, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetScriptArea( BkEditorSession *pSession, int nIndex, const BkEditorScriptAreaRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !AreaRecordWellFormed( pRecord ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.scriptAreas.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( SetScriptAreaInSession( pSession, nIndex, *pRecord, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorDeleteScriptArea( BkEditorSession *pSession, int nIndex )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.scriptAreas.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( DeleteScriptAreaFromSession( pSession, nIndex, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorScriptAreaFromVis( BkEditorSession *pSession, int nType, float fX0, float fY0, float fX1, float fY1,
                                          const char *pszName, BkEditorScriptAreaRecord *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 || pszName == 0 || strnlen( pszName, sizeof pOut->name ) >= sizeof pOut->name )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( ( nType != 0 && nType != 1 ) || !SaneCoordinate( fX0 ) || !SaneCoordinate( fY0 ) || !SaneCoordinate( fX1 ) || !SaneCoordinate( fY1 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		FillAreaRecord( NMapGeometry::AreaFromVis( nType, CVec2( fX0, fY0 ), CVec2( fX1, fY1 ), pszName ), pOut );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorScriptAreaMoved( BkEditorSession *pSession, const BkEditorScriptAreaRecord *pArea, float fX, float fY,
                                        BkEditorScriptAreaRecord *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 || !AreaRecordWellFormed( pArea ) || !SaneCoordinate( fX ) || !SaneCoordinate( fY ) )
			return BK_EDITOR_BAD_ARGUMENT;
		FillAreaRecord( NMapGeometry::MoveArea( AreaOf( *pArea ), CVec2( fX, fY ) ), pOut );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorScriptAreaResized( BkEditorSession *pSession, const BkEditorScriptAreaRecord *pArea, float fX, float fY,
                                          BkEditorScriptAreaRecord *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 || !AreaRecordWellFormed( pArea ) || !SaneCoordinate( fX ) || !SaneCoordinate( fY ) )
			return BK_EDITOR_BAD_ARGUMENT;
		FillAreaRecord( NMapGeometry::ResizeArea( AreaOf( *pArea ), CVec2( fX, fY ) ), pOut );
		return BK_EDITOR_OK;
	} );
}

// ---------------------------------------------------------------------------
// Start commands (04-11, D-17).
// ---------------------------------------------------------------------------

BkEditorStatus BkEditorActionCommands( BkEditorSession *pSession, BkEditorActionCommand *pOut, int nCapacity, int *pnCount, int *pnDefaultIndex )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || pnDefaultIndex == 0 || nCapacity < 0 || ( pOut == 0 && nCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( ReadSessionActionCommands( pSession, pOut, nCapacity, pnCount, pnDefaultIndex, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorStartCommandCount( BkEditorSession *pSession, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		*pnCount = int( pSession->snapshot.startCommandsList.size() );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorStartCommand( BkEditorSession *pSession, int nIndex, BkEditorStartCommandRecord *pOut, int *pUnits, int nUnitCapacity )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 || nUnitCapacity < 0 || ( pUnits == 0 && nUnitCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.startCommandsList.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( ReadSessionStartCommand( pSession, nIndex, pOut, pUnits, nUnitCapacity, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

namespace {
// The most units a command holds through the ABI: no shipped map comes near this.
const int nMaxStartCommandUnitsInCall = 4096;

// A record the ABI takes as a start command: present, a unit list that fits, the
// explosion flag a bool and finite numbers. What is past this (no unit, an unknown
// object or type, a point off the map) is a refusal decided once, in
// session_records.cpp.
bool StartCommandRecordWellFormed( const BkEditorStartCommandRecord *pRecord, const int *pUnits )
{
	if ( pRecord == 0 || pRecord->unit_count < 0 || pRecord->unit_count > nMaxStartCommandUnitsInCall )
		return false;
	if ( pRecord->unit_count > 0 && pUnits == 0 )
		return false;
	if ( pRecord->from_explosion != 0 && pRecord->from_explosion != 1 )
		return false;
	return std::isfinite( pRecord->x ) && std::isfinite( pRecord->y ) && std::isfinite( pRecord->number );
}
}

BkEditorStatus BkEditorAddStartCommand( BkEditorSession *pSession, int nIndex, const BkEditorStartCommandRecord *pRecord, const int *pUnits )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !StartCommandRecordWellFormed( pRecord, pUnits ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < -1 || nIndex > int( pSession->snapshot.startCommandsList.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( AddStartCommandToSession( pSession, nIndex, *pRecord, pUnits, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetStartCommand( BkEditorSession *pSession, int nIndex, const BkEditorStartCommandRecord *pRecord, const int *pUnits )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !StartCommandRecordWellFormed( pRecord, pUnits ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.startCommandsList.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( SetStartCommandInSession( pSession, nIndex, *pRecord, pUnits, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorDeleteStartCommand( BkEditorSession *pSession, int nIndex )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.startCommandsList.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( DeleteStartCommandFromSession( pSession, nIndex, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

// ---------------------------------------------------------------------------
// Reserve positions (04-11, D-18).
// ---------------------------------------------------------------------------

BkEditorStatus BkEditorReserveRole( BkEditorSession *pSession, const char *pszName, int *pnRole )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pnRole == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		*pnRole = ReserveRoleOfName( pszName );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorReservePositionCount( BkEditorSession *pSession, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		*pnCount = int( pSession->snapshot.reservePositionsList.size() );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorReservePosition( BkEditorSession *pSession, int nIndex, BkEditorReservePositionRecord *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.reservePositionsList.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		return ReadSessionReservePosition( pSession, nIndex, pOut ) ? BK_EDITOR_OK : BK_EDITOR_FAILED;
	} );
}

namespace {
bool ReservePositionRecordWellFormed( const BkEditorReservePositionRecord *pRecord )
{
	return pRecord != 0 && std::isfinite( pRecord->x ) && std::isfinite( pRecord->y );
}
}

BkEditorStatus BkEditorAddReservePosition( BkEditorSession *pSession, int nIndex, const BkEditorReservePositionRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !ReservePositionRecordWellFormed( pRecord ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < -1 || nIndex > int( pSession->snapshot.reservePositionsList.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( AddReservePositionToSession( pSession, nIndex, *pRecord, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetReservePosition( BkEditorSession *pSession, int nIndex, const BkEditorReservePositionRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !ReservePositionRecordWellFormed( pRecord ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.reservePositionsList.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( SetReservePositionInSession( pSession, nIndex, *pRecord, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorDeleteReservePosition( BkEditorSession *pSession, int nIndex )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.reservePositionsList.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		bool bRefused = false;
		if ( DeleteReservePositionFromSession( pSession, nIndex, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

namespace {
// The most script IDs a caller may hand BkEditorSetGroup: one per script ID
// there is, with room for a file's own duplicates. A larger count is a caller
// bug, and keeps the vector the put builds bounded (T-04-09-03).
const int nMaxGroupPutCount = 65536;
}

// ---------------------------------------------------------------------------
// The AI general (04-12, D-19).
// ---------------------------------------------------------------------------

BkEditorStatus BkEditorAIGeneralSide( BkEditorSession *pSession, int nSide, BkEditorAISideInfo *pInfo,
                                      int *pnMobile, int nMobileCapacity,
                                      BkEditorAIParcel *pParcels, int nParcelCapacity,
                                      BkEditorAIPoint *pPoints, int nPointCapacity )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pInfo == 0 || nSide < 0 || nMobileCapacity < 0 || nParcelCapacity < 0 || nPointCapacity < 0 ||
		     ( pnMobile == 0 && nMobileCapacity > 0 ) || ( pParcels == 0 && nParcelCapacity > 0 ) || ( pPoints == 0 && nPointCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		// A buffer too short is the sizing pass of a two-pass read, not a failure worth a message.
		return ReadSessionAIGeneralSide( pSession, nSide, pInfo, pnMobile, nMobileCapacity, pParcels, nParcelCapacity, pPoints, nPointCapacity )
			? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorSetAIGeneralSide( BkEditorSession *pSession, int nSide, int nSideCount,
                                         const int *pnMobile, int nMobileCount,
                                         const BkEditorAIParcel *pParcels, int nParcelCount,
                                         const BkEditorAIPoint *pPoints, int nPointCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nSide < 0 || nSide >= NMapRecords::nMaxAIGeneralSides || nSideCount < 0 || nSideCount > NMapRecords::nMaxAIGeneralSides ||
		     nMobileCount < 0 || nParcelCount < 0 || nPointCount < 0 ||
		     nMobileCount > nMaxGroupPutCount || nParcelCount > nMaxGroupPutCount || nPointCount > nMaxGroupPutCount ||
		     ( pnMobile == 0 && nMobileCount > 0 ) || ( pParcels == 0 && nParcelCount > 0 ) || ( pPoints == 0 && nPointCount > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		// Every parcel's point range is inside the points array and its direction is a WORD.
		for ( int i = 0; i < nParcelCount; ++i )
		{
			const BkEditorAIParcel &rParcel = pParcels[i];
			if ( rParcel.first_point < 0 || rParcel.point_count < 0 || rParcel.first_point > nPointCount ||
			     rParcel.point_count > nPointCount - rParcel.first_point || rParcel.defence_dir < 0 || rParcel.defence_dir > 65535 )
				return BK_EDITOR_BAD_ARGUMENT;
		}
		for ( int i = 0; i < nPointCount; ++i )
			if ( pPoints[i].dir < 0 || pPoints[i].dir > 65535 )
				return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( SetSessionAIGeneralSide( pSession, nSide, nSideCount, pnMobile, nMobileCount, pParcels, nParcelCount, pPoints, nPointCount, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorGroupIDs( BkEditorSession *pSession, int *pnOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( pnOut == 0 && nCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return ReadSessionGroupIDs( pSession, pnOut, nCapacity, pnCount ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorGroup( BkEditorSession *pSession, int nID, int *pnIDs, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nID < 0 || nCapacity < 0 || ( pnIDs == 0 && nCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( ReadSessionGroup( pSession, nID, pnIDs, nCapacity, pnCount, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetGroup( BkEditorSession *pSession, int nID, const int *pnIDs, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nID < 0 || nCount < 0 || nCount > nMaxGroupPutCount || ( pnIDs == 0 && nCount > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( SetSessionGroup( pSession, nID, pnIDs, nCount, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorDeleteGroup( BkEditorSession *pSession, int nID )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nID < 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( DeleteSessionGroup( pSession, nID, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorSetHiddenScriptIDs( BkEditorSession *pSession, const int *pnScriptIDs, int nCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nCount < 0 || nCount > nMaxGroupPutCount || ( pnScriptIDs == 0 && nCount > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return SetSessionHiddenScriptIDs( pSession, pnScriptIDs, nCount ) ? BK_EDITOR_OK : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorFirstFreeGroupID( BkEditorSession *pSession, int nFrom, int *pnID )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnID == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		*pnID = FirstFreeGroupIDInSession( pSession, nFrom );
		return BK_EDITOR_OK;
	} );
}

// The edit log's undo and redo, by the token an edit handed out.
BkEditorStatus BkEditorUndoEdit( BkEditorSession *pSession, int nToken )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( UndoEditInSession( pSession, nToken, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

BkEditorStatus BkEditorRedoEdit( BkEditorSession *pSession, int nToken )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( RedoEditInSession( pSession, nToken, &bRefused ) )
			return BK_EDITOR_OK;
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	} );
}

namespace {
bool IsVsoKind( int nKind )
{
	return nKind == 0 || nKind == 1;
}

// A bounded copy into a fixed C field, always terminated.
void CopyName( char *pOut, size_t nSize, const std::string &szName )
{
	const size_t nLen = Min( szName.size(), nSize - 1 );
	memcpy( pOut, szName.c_str(), nLen );
	pOut[nLen] = 0;
}
}

BkEditorStatus BkEditorVsoDescriptors( BkEditorSession *pSession, int nKind, BkEditorVsoDescriptor *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 ) || !IsVsoKind( nKind ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		std::vector<std::string> names;
		if ( !VsoDescriptors( pSession, nKind, &names ) )
			return BK_EDITOR_FAILED;
		*pnCount = int( names.size() );
		for ( int i = 0; i < int( names.size() ) && i < nCapacity; ++i )
			CopyName( pOut[i].name, sizeof pOut[i].name, names[i] );
		return nCapacity >= int( names.size() ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorVsoCount( BkEditorSession *pSession, int nKind, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || !IsVsoKind( nKind ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		*pnCount = VsoCount( *pSession, nKind );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorVso( BkEditorSession *pSession, int nKind, int nIndex, BkEditorVsoInfo *pInfo,
                            BkEditorVec3 *pControls, int nControlCap, BkEditorVsoKeyPoint *pKeys, int nKeyCap )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pInfo == 0 || !IsVsoKind( nKind ) || nControlCap < 0 || nKeyCap < 0 ||
		     ( nControlCap > 0 && pControls == 0 ) || ( nKeyCap > 0 && pKeys == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		memset( pInfo, 0, sizeof *pInfo );
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		const SVectorStripeObject *pVso = SessionVso( *pSession, nKind, nIndex );
		if ( pVso == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		pInfo->saved_id = pVso->nID;
		CopyName( pInfo->desc, sizeof pInfo->desc, pVso->szDescName );
		pInfo->control_count = int( pVso->controlpoints.size() );
		int nKeys = 0;
		for ( size_t i = 0; i < pVso->points.size(); ++i )
			if ( pVso->points[i].bKeyPoint )
				++nKeys;
		pInfo->key_count = nKeys;
		for ( int i = 0; i < pInfo->control_count && i < nControlCap; ++i )
		{
			pControls[i].x = pVso->controlpoints[i].x;
			pControls[i].y = pVso->controlpoints[i].y;
			pControls[i].z = pVso->controlpoints[i].z;
		}
		int nKey = 0;
		for ( size_t i = 0; i < pVso->points.size() && nKey < nKeyCap; ++i )
		{
			const SVectorStripeObjectPoint &rPoint = pVso->points[i];
			if ( !rPoint.bKeyPoint )
				continue;
			BkEditorVsoKeyPoint &rOut = pKeys[nKey++];
			rOut.x = rPoint.vPos.x;
			rOut.y = rPoint.vPos.y;
			rOut.z = rPoint.vPos.z;
			rOut.nx = rPoint.vNorm.x;
			rOut.ny = rPoint.vNorm.y;
			rOut.nz = rPoint.vNorm.z;
			rOut.width = rPoint.fWidth;
			rOut.opacity = rPoint.fOpacity;
		}
		return nControlCap >= pInfo->control_count && nKeyCap >= pInfo->key_count ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorAddVso( BkEditorSession *pSession, int nKind, const char *pszDesc, const BkEditorVec3 *pPoints, int nCount,
                               float fWidthTiles, float fOpacity, int *pnToken, int *pnIndex )
{
	if ( pnToken != 0 ) *pnToken = -1;
	if ( pnIndex != 0 ) *pnIndex = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !IsVsoKind( nKind ) || pszDesc == 0 || nCount < 0 || nCount > 1024 || ( nCount > 0 && pPoints == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !std::isfinite( fWidthTiles ) || !std::isfinite( fOpacity ) || fWidthTiles < 1.0f || fWidthTiles > 16.0f || fOpacity < 0.0f || fOpacity > 1.0f )
			return BK_EDITOR_BAD_ARGUMENT;
		std::vector<CVec3> points( nCount );
		for ( int i = 0; i < nCount; ++i )
		{
			if ( !std::isfinite( pPoints[i].x ) || !std::isfinite( pPoints[i].y ) || !std::isfinite( pPoints[i].z ) )
				return BK_EDITOR_BAD_ARGUMENT;
			points[i] = CVec3( pPoints[i].x, pPoints[i].y, pPoints[i].z );
		}
		if ( strnlen( pszDesc, 128 ) >= 128 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		int nToken = -1, nIndex = -1;
		bool bRefused = false;
		if ( !AddVsoToSession( pSession, nKind, pszDesc, points, fWidthTiles, fOpacity, &nToken, &nIndex, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 ) *pnToken = nToken;
		if ( pnIndex != 0 ) *pnIndex = nIndex;
		return BK_EDITOR_OK;
	} );
}

namespace {
// The checks every edit of an existing road or river shares, in the order
// the no-map table needs: the kind, the map, the index.
BkEditorStatus VsoEditPrologue( BkEditorSession *pSession, int nKind, int nIndex )
{
	if ( !IsVsoKind( nKind ) )
		return BK_EDITOR_BAD_ARGUMENT;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return BK_EDITOR_REFUSED;
	}
	if ( nIndex < 0 || nIndex >= VsoCount( *pSession, nKind ) )
		return BK_EDITOR_BAD_ARGUMENT;
	return BK_EDITOR_OK;
}

// A session edit's answer as a status, its token handed out on success.
BkEditorStatus VsoEditResult( bool bOk, bool bRefused, int nToken, int *pnToken )
{
	if ( !bOk )
		return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
	if ( pnToken != 0 )
		*pnToken = nToken;
	return BK_EDITOR_OK;
}

// The key point count of a record, for the range checks.
int KeyCount( const SVectorStripeObject &rVso )
{
	int nKeys = 0;
	for ( size_t i = 0; i < rVso.points.size(); ++i )
		if ( rVso.points[i].bKeyPoint )
			++nKeys;
	return nKeys;
}
}

BkEditorStatus BkEditorMoveVsoPoints( BkEditorSession *pSession, int nKind, int nIndex, const BkEditorVec3 *pPoints, int nCount, int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nCount < 0 || nCount > 1024 || ( nCount > 0 && pPoints == 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		std::vector<CVec3> points( nCount );
		for ( int i = 0; i < nCount; ++i )
		{
			if ( !std::isfinite( pPoints[i].x ) || !std::isfinite( pPoints[i].y ) || !std::isfinite( pPoints[i].z ) )
				return BK_EDITOR_BAD_ARGUMENT;
			points[i] = CVec3( pPoints[i].x, pPoints[i].y, pPoints[i].z );
		}
		const BkEditorStatus prologue = VsoEditPrologue( pSession, nKind, nIndex );
		if ( prologue != BK_EDITOR_OK )
			return prologue;
		if ( nCount != int( SessionVso( *pSession, nKind, nIndex )->controlpoints.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1;
		bool bRefused = false;
		const bool bOk = MoveVsoPointsInSession( pSession, nKind, nIndex, points, &nToken, &bRefused );
		return VsoEditResult( bOk, bRefused, nToken, pnToken );
	} );
}

BkEditorStatus BkEditorSetVsoWidth( BkEditorSession *pSession, int nKind, int nIndex, int nKey, float fWidth, int nMode, int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !std::isfinite( fWidth ) || fWidth <= 0.0f || nMode < 0 || nMode > 2 )
			return BK_EDITOR_BAD_ARGUMENT;
		const BkEditorStatus prologue = VsoEditPrologue( pSession, nKind, nIndex );
		if ( prologue != BK_EDITOR_OK )
			return prologue;
		if ( nKey < 0 || nKey >= KeyCount( *SessionVso( *pSession, nKind, nIndex ) ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1;
		bool bRefused = false;
		const bool bOk = SetVsoWidthInSession( pSession, nKind, nIndex, nKey, fWidth, nMode, &nToken, &bRefused );
		return VsoEditResult( bOk, bRefused, nToken, pnToken );
	} );
}

BkEditorStatus BkEditorSetVsoOpacity( BkEditorSession *pSession, int nKind, int nIndex, int nKey, float fOpacity, int nMode, int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !std::isfinite( fOpacity ) || fOpacity < 0.0f || fOpacity > 1.0f || nMode < 0 || nMode > 2 )
			return BK_EDITOR_BAD_ARGUMENT;
		const BkEditorStatus prologue = VsoEditPrologue( pSession, nKind, nIndex );
		if ( prologue != BK_EDITOR_OK )
			return prologue;
		if ( nKey < 0 || nKey >= KeyCount( *SessionVso( *pSession, nKind, nIndex ) ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1;
		bool bRefused = false;
		const bool bOk = SetVsoOpacityInSession( pSession, nKind, nIndex, nKey, fOpacity, nMode, &nToken, &bRefused );
		return VsoEditResult( bOk, bRefused, nToken, pnToken );
	} );
}

BkEditorStatus BkEditorInsertVsoPoint( BkEditorSession *pSession, int nKind, int nIndex, int nControl, int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		const BkEditorStatus prologue = VsoEditPrologue( pSession, nKind, nIndex );
		if ( prologue != BK_EDITOR_OK )
			return prologue;
		if ( nControl < 0 || nControl >= int( SessionVso( *pSession, nKind, nIndex )->controlpoints.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1;
		bool bRefused = false;
		const bool bOk = InsertVsoPointInSession( pSession, nKind, nIndex, nControl, &nToken, &bRefused );
		return VsoEditResult( bOk, bRefused, nToken, pnToken );
	} );
}

BkEditorStatus BkEditorDeleteVsoPoint( BkEditorSession *pSession, int nKind, int nIndex, int nControl, int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		const BkEditorStatus prologue = VsoEditPrologue( pSession, nKind, nIndex );
		if ( prologue != BK_EDITOR_OK )
			return prologue;
		if ( nControl < 0 || nControl >= int( SessionVso( *pSession, nKind, nIndex )->controlpoints.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1;
		bool bRefused = false;
		const bool bOk = DeleteVsoPointInSession( pSession, nKind, nIndex, nControl, &nToken, &bRefused );
		return VsoEditResult( bOk, bRefused, nToken, pnToken );
	} );
}

BkEditorStatus BkEditorPickVso( BkEditorSession *pSession, float fX, float fY, int nCycle, int *pnKind, int *pnIndex )
{
	if ( pnKind != 0 ) *pnKind = -1;
	if ( pnIndex != 0 ) *pnIndex = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnKind == 0 || pnIndex == 0 || !std::isfinite( fX ) || !std::isfinite( fY ) || nCycle < 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		int nKind = -1, nIndex = -1;
		bool bRefused = false;
		if ( !PickVsoInSession( pSession, fX, fY, nCycle, &nKind, &nIndex, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		*pnKind = nKind;
		*pnIndex = nIndex;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorDeleteVso( BkEditorSession *pSession, int nKind, int nIndex, int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !IsVsoKind( nKind ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= VsoCount( *pSession, nKind ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1;
		bool bRefused = false;
		if ( !DeleteVsoFromSession( pSession, nKind, nIndex, &nToken, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 ) *pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorVsoMatchesEngine( BkEditorSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return VsoMatchesEngine( pSession ) ? BK_EDITOR_OK : BK_EDITOR_FAILED;
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

namespace {
// A bridge type name as the ABI takes it: present and shorter than the
// records' 64-character fields.
bool BridgeNameFits( const char *pszDesc )
{
	return pszDesc != 0 && strnlen( pszDesc, 64 ) < 64;
}

bool FiniteDrag( float fX0, float fY0, float fX1, float fY1 )
{
	return SaneCoordinate( fX0 ) && SaneCoordinate( fY0 ) && SaneCoordinate( fX1 ) && SaneCoordinate( fY1 );
}
}

BkEditorStatus BkEditorBridgeDescriptors( BkEditorSession *pSession, BkEditorBridgeDescriptor *pOut, int nCapacity, int *pnCount )
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
		std::vector<SBridgeDescriptorInfo> descriptors;
		if ( !BridgeDescriptorsInSession( pSession, &descriptors ) )
			return BK_EDITOR_FAILED;
		*pnCount = int( descriptors.size() );
		for ( int i = 0; i < int( descriptors.size() ) && i < nCapacity; ++i )
		{
			BkEditorBridgeDescriptor &rOut = pOut[i];
			memset( &rOut, 0, sizeof rOut );
			CopyName( rOut.name, sizeof rOut.name, descriptors[i].szName );
			rOut.direction = descriptors[i].nDirection;
			rOut.has_partner = descriptors[i].bHasPartner ? 1 : 0;
			rOut.build_during_play_allowed = descriptors[i].bBuildDuringPlay ? 1 : 0;
		}
		return nCapacity >= int( descriptors.size() ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorPlanBridge( BkEditorSession *pSession, const char *pszDesc, float fX0, float fY0, float fX1, float fY1,
                                   BkEditorPlannedPiece *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 ) || !BridgeNameFits( pszDesc ) || !FiniteDrag( fX0, fY0, fX1, fY1 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		std::vector<NMapGeometry::SPlannedPiece> plan;
		bool bRefused = false;
		if ( !PlanBridgeInSession( pSession, pszDesc, CVec2( fX0, fY0 ), CVec2( fX1, fY1 ), &plan, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		*pnCount = int( plan.size() );
		for ( int i = 0; i < int( plan.size() ) && i < nCapacity; ++i )
		{
			pOut[i].x = plan[i].vPos.x;
			pOut[i].y = plan[i].vPos.y;
			pOut[i].type = plan[i].nPackedType;
			pOut[i].dir = plan[i].nDir;
		}
		return nCapacity >= int( plan.size() ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorDrawBridge( BkEditorSession *pSession, const char *pszDesc, float fX0, float fY0, float fX1, float fY1,
                                   int *pnToken, int *pnIndex )
{
	if ( pnToken != 0 ) *pnToken = -1;
	if ( pnIndex != 0 ) *pnIndex = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !BridgeNameFits( pszDesc ) || !FiniteDrag( fX0, fY0, fX1, fY1 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		int nToken = -1, nIndex = -1;
		bool bRefused = false;
		if ( !DrawBridgeInSession( pSession, pszDesc, CVec2( fX0, fY0 ), CVec2( fX1, fY1 ), &nToken, &nIndex, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 ) *pnToken = nToken;
		if ( pnIndex != 0 ) *pnIndex = nIndex;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorBridges( BkEditorSession *pSession, BkEditorBridgeInfo *pOut, int nCapacity, int *pnCount )
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
		std::vector<SBridgeInfo> bridges;
		ReadSessionBridges( *pSession, &bridges );
		*pnCount = int( bridges.size() );
		for ( int i = 0; i < int( bridges.size() ) && i < nCapacity; ++i )
		{
			BkEditorBridgeInfo &rOut = pOut[i];
			memset( &rOut, 0, sizeof rOut );
			CopyName( rOut.desc, sizeof rOut.desc, bridges[i].szDesc );
			rOut.span_count = bridges[i].nSpans;
			rOut.min_x = bridges[i].vMin.x;
			rOut.min_y = bridges[i].vMin.y;
			rOut.max_x = bridges[i].vMax.x;
			rOut.max_y = bridges[i].vMax.y;
			rOut.built_during_play = bridges[i].bBuiltDuringPlay ? 1 : 0;
		}
		return nCapacity >= int( bridges.size() ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorPickGroup( BkEditorSession *pSession, float fSx, float fSy, int *pnKind, int *pnIndex )
{
	if ( pnKind != 0 ) *pnKind = -1;
	if ( pnIndex != 0 ) *pnIndex = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnKind == 0 || pnIndex == 0 || !std::isfinite( fSx ) || !std::isfinite( fSy ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		bool bRefused = false;
		if ( !PickGroupInSession( pSession, fSx, fSy, pnKind, pnIndex, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorDeleteBridge( BkEditorSession *pSession, int nIndex, int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.bridges.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1;
		bool bRefused = false;
		if ( !DeleteBridgeFromSession( pSession, nIndex, &nToken, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 ) *pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

namespace {
// A bridge edit by index: the map check, the index check, the session call.
typedef bool ( *TBridgeIndexEdit )( SEditorSession *pSession, int nIndex, int *pnToken, bool *pbRefused );
BkEditorStatus BridgeIndexEdit( BkEditorSession *pSession, int nIndex, int *pnToken, TBridgeIndexEdit edit )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.bridges.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1;
		bool bRefused = false;
		if ( !edit( pSession, nIndex, &nToken, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 ) *pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}
}

BkEditorStatus BkEditorRotateBridge( BkEditorSession *pSession, int nIndex, int *pnToken )
{
	return BridgeIndexEdit( pSession, nIndex, pnToken, RotateBridgeInSession );
}

BkEditorStatus BkEditorToggleBridgeBuild( BkEditorSession *pSession, int nIndex, int *pnToken )
{
	return BridgeIndexEdit( pSession, nIndex, pnToken, ToggleBridgeBuildInSession );
}

BkEditorStatus BkEditorWorldToAITile( BkEditorSession *pSession, float fWx, float fWy, int *pnX, int *pnY )
{
	if ( pnX != 0 ) *pnX = -1;
	if ( pnY != 0 ) *pnY = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnX == 0 || pnY == 0 || !std::isfinite( fWx ) || !std::isfinite( fWy ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		return WorldToAITile( pSession, fWx, fWy, pnX, pnY ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorFenceDescriptors( BkEditorSession *pSession, BkEditorFenceDescriptor *pOut, int nCapacity, int *pnCount )
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
		std::vector<SFenceDescriptorInfo> descriptors;
		if ( !FenceDescriptorsInSession( pSession, &descriptors ) )
			return BK_EDITOR_FAILED;
		*pnCount = int( descriptors.size() );
		for ( int i = 0; i < int( descriptors.size() ) && i < nCapacity; ++i )
		{
			memset( &pOut[i], 0, sizeof pOut[i] );
			CopyName( pOut[i].name, sizeof pOut[i].name, descriptors[i].szName );
		}
		return nCapacity >= int( descriptors.size() ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorPlanFences( BkEditorSession *pSession, const char *pszDesc, float fX0, float fY0, float fX1, float fY1, int nCtrl,
                                   BkEditorPlannedPiece *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 ) || !BridgeNameFits( pszDesc ) || !FiniteDrag( fX0, fY0, fX1, fY1 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		std::vector<NMapGeometry::SPlannedPiece> plan;
		bool bRefused = false;
		if ( !PlanFencesInSession( pSession, pszDesc, CVec2( fX0, fY0 ), CVec2( fX1, fY1 ), nCtrl != 0, &plan, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		*pnCount = int( plan.size() );
		for ( int i = 0; i < int( plan.size() ) && i < nCapacity; ++i )
		{
			pOut[i].x = plan[i].vPos.x;
			pOut[i].y = plan[i].vPos.y;
			pOut[i].type = plan[i].nPackedType;
			pOut[i].dir = plan[i].nDir;
		}
		return nCapacity >= int( plan.size() ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorDrawFences( BkEditorSession *pSession, const char *pszDesc, float fX0, float fY0, float fX1, float fY1, int nCtrl,
                                   int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !BridgeNameFits( pszDesc ) || !FiniteDrag( fX0, fY0, fX1, fY1 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		int nToken = -1;
		bool bRefused = false;
		if ( !DrawFencesInSession( pSession, pszDesc, CVec2( fX0, fY0 ), CVec2( fX1, fY1 ), nCtrl != 0, &nToken, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 ) *pnToken = nToken;
		return BK_EDITOR_OK;
	} );
}

namespace {
// The clicks of a trench, or false for a bad argument: null points with a
// count, a count outside 0..256, a non-finite coordinate.
bool TrenchPoints( const BkEditorVec3 *pPoints, int nCount, std::vector<CVec2> *pOut )
{
	if ( nCount < 0 || nCount > 256 || ( nCount > 0 && pPoints == 0 ) )
		return false;
	pOut->clear();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !std::isfinite( pPoints[i].x ) || !std::isfinite( pPoints[i].y ) )
			return false;
		pOut->push_back( CVec2( pPoints[i].x, pPoints[i].y ) );
	}
	return true;
}
}

BkEditorStatus BkEditorPlanEntrenchment( BkEditorSession *pSession, const BkEditorVec3 *pPoints, int nCount,
                                         BkEditorPlannedPiece *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		std::vector<CVec2> points;
		if ( pnCount == 0 || nCapacity < 0 || ( nCapacity > 0 && pOut == 0 ) || !TrenchPoints( pPoints, nCount, &points ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		NMapGeometry::STrenchPlan plan;
		bool bRefused = false;
		if ( !PlanEntrenchmentInSession( pSession, points, &plan, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		*pnCount = int( plan.pieces.size() );
		for ( int i = 0; i < int( plan.pieces.size() ) && i < nCapacity; ++i )
		{
			pOut[i].x = plan.pieces[i].vPos.x;
			pOut[i].y = plan.pieces[i].vPos.y;
			pOut[i].type = plan.pieces[i].nPackedType;
			pOut[i].dir = plan.pieces[i].nDir;
		}
		return nCapacity >= int( plan.pieces.size() ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorDrawEntrenchment( BkEditorSession *pSession, const BkEditorVec3 *pPoints, int nCount, int nPlayer,
                                         int *pnToken, int *pnIndex )
{
	if ( pnToken != 0 ) *pnToken = -1;
	if ( pnIndex != 0 ) *pnIndex = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		std::vector<CVec2> points;
		if ( !TrenchPoints( pPoints, nCount, &points ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nPlayer < 0 || nPlayer >= int( pSession->snapshot.diplomacies.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1, nIndex = -1;
		bool bRefused = false;
		if ( !DrawEntrenchmentInSession( pSession, points, nPlayer, &nToken, &nIndex, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 ) *pnToken = nToken;
		if ( pnIndex != 0 ) *pnIndex = nIndex;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorEntrenchments( BkEditorSession *pSession, BkEditorEntrenchmentInfo *pOut, int nCapacity, int *pnCount )
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
		std::vector<SEntrenchmentSummary> trenches;
		ReadSessionEntrenchments( *pSession, &trenches );
		*pnCount = int( trenches.size() );
		for ( int i = 0; i < int( trenches.size() ) && i < nCapacity; ++i )
		{
			BkEditorEntrenchmentInfo &rOut = pOut[i];
			memset( &rOut, 0, sizeof rOut );
			rOut.piece_count = trenches[i].nPieces;
			rOut.section_count = trenches[i].nSections;
			rOut.player = trenches[i].nPlayer;
			rOut.min_x = trenches[i].vMin.x;
			rOut.min_y = trenches[i].vMin.y;
			rOut.max_x = trenches[i].vMax.x;
			rOut.max_y = trenches[i].vMax.y;
		}
		return nCapacity >= int( trenches.size() ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}

BkEditorStatus BkEditorDeleteEntrenchment( BkEditorSession *pSession, int nIndex, int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( !pSession->bMapOpen )
		{
			pSession->szMessage = "no map is open";
			return BK_EDITOR_REFUSED;
		}
		if ( nIndex < 0 || nIndex >= int( pSession->snapshot.entrenchments.size() ) )
			return BK_EDITOR_BAD_ARGUMENT;
		int nToken = -1;
		bool bRefused = false;
		if ( !DeleteEntrenchmentFromSession( pSession, nIndex, &nToken, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( pnToken != 0 ) *pnToken = nToken;
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
		ClearGhostInSession( pSession );
		delete pSession->pWorld;
		pSession->pWorld = 0;
		// The renderer outlives the session, and the overlay's function and
		// user data are the caller's, gone with its ImGui state: left in place,
		// the next frame or mode change would call into freed memory.
		if ( pSession->bEngineStarted )
		{
			if ( IGFX *pGFX = GetSingleton<IGFX>() )
				pGFX->SetOverlay( 0, 0 );
			// The audio device's mixer thread runs code in the SFX module, which
			// the module list unloads at exit; stopped here, as NMain::Finalize
			// does for the game, or on Linux that thread runs into the unmapped
			// module (SIGSEGV after the editor's last frame).
			if ( ISFX *pSFX = GetSingleton<ISFX>() )
				pSFX->Done();
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

// ---------------------------------------------------------------------------
// The Fields tool (M3, D-21) and the RMG folder scan (D-08).
// ---------------------------------------------------------------------------

BkEditorStatus BkEditorApplyField( BkEditorSession *pSession, const BkEditorFieldApplyParams *pParams,
                                   BkEditorFieldObjectReport *pOutReport, int nReportCapacity, int *pnReportCount,
                                   int *pnToken )
{
	if ( pnToken != 0 ) *pnToken = -1;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		EnsureRmgMount( pSession );
		if ( pParams == 0 || pnToken == 0 || pnReportCount == 0 || nReportCapacity < 0 || ( pOutReport == 0 && nReportCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		// Both names are read as C strings below: a caller that filled one to the brim has not
		// terminated it.
		if ( memchr( pParams->field_set, 0, sizeof pParams->field_set ) == 0 ||
		     memchr( pParams->object_filter, 0, sizeof pParams->object_filter ) == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		SFieldApply apply;
		apply.szFieldSet = pParams->field_set;
		const int nPoints = pParams->point_count;
		if ( nPoints < 3 || nPoints > 64 || pParams->points == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		apply.points.resize( nPoints );
		for ( int i = 0; i < nPoints; ++i )
			apply.points[i] = CVec3( pParams->points[i].x, pParams->points[i].y, 0.0f );
		apply.bRandomize = pParams->randomize != 0;
		apply.fMinLength = pParams->min_length;
		apply.fWidth = pParams->width;
		apply.fDisturbance = pParams->disturbance;
		apply.bFillTerrain = pParams->fill_terrain != 0;
		apply.bPlaceObjects = pParams->place_objects != 0;
		apply.bModifyHeights = pParams->modify_heights != 0;
		apply.bUpdateMapAfter = pParams->update_map_after != 0;
		apply.bCanAddObjectFilter = pParams->can_add_object_filter != 0;
		apply.bCheckPassabilityOnly = pParams->check_passability_only != 0;
		apply.szObjectFilter = pParams->object_filter;

		std::vector<SFieldObjectReport> report;
		bool bRefused = false;
		int nToken = -1;
		if ( !ApplyFieldInSession( pSession, apply, &report, &bRefused, &nToken ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		*pnReportCount = int( report.size() );
		const int nWrite = Min( int( report.size() ), nReportCapacity );
		for ( int i = 0; i < nWrite; ++i )
		{
			memset( &pOutReport[i], 0, sizeof pOutReport[i] );
			const size_t nCopy = Min( report[i].szName.size(), sizeof pOutReport[i].name - 1 );
			memcpy( pOutReport[i].name, report[i].szName.c_str(), nCopy );
			pOutReport[i].x = report[i].fX;
			pOutReport[i].y = report[i].fY;
			pOutReport[i].placed = report[i].bPlaced ? 1 : 0;
		}
		// The edit is applied and on the log by now, so the token is the caller's whatever the
		// report buffer held: answering REFUSED ("changes nothing") here would drop the token and
		// leave an edit on top of the log that nothing can undo. A buffer smaller than the report
		// is a truncation, which out_report_count (always the total) tells the caller.
		*pnToken = nToken;
		if ( int( report.size() ) > nReportCapacity )
			pSession->szMessage = "the report holds " + std::to_string( report.size() ) + " objects, room was given for " +
			                      std::to_string( nReportCapacity ) + "; the field was applied all the same";
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorFieldSetSeason( BkEditorSession *pSession, const char *pszName, int *pnSeason )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		EnsureRmgMount( pSession );
		if ( pszName == 0 || *pszName == 0 || pnSeason == 0 || strnlen( pszName, 256 ) >= 256 )
			return BK_EDITOR_BAD_ARGUMENT;
		int nSeason = -1;
		bool bRefused = false;
		if ( !FieldSetSeasonInSession( pSession, pszName, &nSeason, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		*pnSeason = nSeason;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorCreateRandomMap( BkEditorSession *pSession, const BkEditorRmgGenerateParams *pParams,
                                        BkEditorRmgGenerateResult *pResult )
{
	if ( pResult != 0 )
		memset( pResult, 0, sizeof *pResult );
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		EnsureRmgMount( pSession );
		if ( pParams == 0 || pResult == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		// Every text field has to end inside its buffer: a caller that filled
		// one to the brim has not terminated it.
		if ( strnlen( pParams->template_name, sizeof pParams->template_name ) >= sizeof pParams->template_name ||
		     strnlen( pParams->context_name, sizeof pParams->context_name ) >= sizeof pParams->context_name ||
		     strnlen( pParams->setting_name, sizeof pParams->setting_name ) >= sizeof pParams->setting_name ||
		     strnlen( pParams->map_name, sizeof pParams->map_name ) >= sizeof pParams->map_name )
			return BK_EDITOR_BAD_ARGUMENT;
		SRMGenerateParams params;
		params.szTemplate = pParams->template_name;
		params.szContext = pParams->context_name;
		params.szSetting = pParams->setting_name;
		params.szMapName = pParams->map_name;
		params.szModFolder = pSession->szModFolder;
		params.nLevel = pParams->level;
		params.nGraph = pParams->graph;
		params.nAngle = pParams->angle;
		params.bSaveAsBZM = pParams->save_as_bzm != 0;
		params.bWriteDDS = pParams->write_dds != 0;
		params.bOverwrite = pParams->overwrite != 0;
		params.bHasSeed = pParams->has_seed != 0;
		params.nSeed = pParams->seed;
		params.pfnProgress = pParams->progress_fn;
		params.pUser = pParams->user;
		SRMGenerateResult result;
		bool bRefused = false;
		if ( !CreateRandomMapInSession( pSession, params, &bRefused, &result ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( result.szGraphName.size() >= sizeof pResult->graph_name || result.szMapPath.size() >= sizeof pResult->map_path )
		{
			pSession->szMessage = "a name of the result does not fit the caller's buffer";
			return BK_EDITOR_FAILED;
		}
		pResult->seed = result.nSeed;
		pResult->graph = result.nGraph;
		pResult->angle = result.nAngle;
		memcpy( pResult->graph_name, result.szGraphName.c_str(), result.szGraphName.size() + 1 );
		memcpy( pResult->map_path, result.szMapPath.c_str(), result.szMapPath.size() + 1 );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorListStorageFiles( BkEditorSession *pSession, const char *pszFolder, const char *pszExtension,
                                         BkEditorRmgName *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		EnsureRmgMount( pSession );
		if ( pszFolder == 0 || pszExtension == 0 || pnCount == 0 || nCapacity < 0 || ( pOut == 0 && nCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		std::vector<std::string> names;
		bool bRefused = false;
		if ( !ListStorageFiles( pSession, pszFolder, pszExtension, &names, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		for ( size_t i = 0; i < names.size(); ++i )
			if ( names[i].size() >= sizeof pOut->name )
			{
				pSession->szMessage = "a file name does not fit the 191-character field: " + names[i];
				return BK_EDITOR_FAILED;
			}
		*pnCount = int( names.size() );
		const int nWrite = Min( int( names.size() ), nCapacity );
		for ( int i = 0; i < nWrite; ++i )
		{
			memset( &pOut[i], 0, sizeof pOut[i] );
			memcpy( pOut[i].name, names[size_t( i )].c_str(), names[size_t( i )].size() );
		}
		if ( int( names.size() ) > nCapacity )
		{
			pSession->szMessage = NStr::Format( "the folder holds %d files and room was given for %d", int( names.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorRmgTemplateGraphs( BkEditorSession *pSession, const char *pszTemplate,
                                          BkEditorRmgGraph *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		EnsureRmgMount( pSession );
		if ( pszTemplate == 0 || pnCount == 0 || nCapacity < 0 || ( pOut == 0 && nCapacity > 0 ) || strnlen( pszTemplate, 256 ) >= 256 )
			return BK_EDITOR_BAD_ARGUMENT;
		std::vector<SRMTemplateGraph> graphs;
		bool bRefused = false;
		if ( !ListTemplateGraphs( pSession, pszTemplate, &graphs, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		for ( size_t i = 0; i < graphs.size(); ++i )
			if ( graphs[i].szName.size() >= sizeof pOut->name )
			{
				pSession->szMessage = "a graph name does not fit the 191-character field: " + graphs[i].szName;
				return BK_EDITOR_FAILED;
			}
		*pnCount = int( graphs.size() );
		const int nWrite = Min( int( graphs.size() ), nCapacity );
		for ( int i = 0; i < nWrite; ++i )
		{
			memset( &pOut[i], 0, sizeof pOut[i] );
			memcpy( pOut[i].name, graphs[size_t( i )].szName.c_str(), graphs[size_t( i )].szName.size() );
			pOut[i].weight = graphs[size_t( i )].nWeight;
		}
		if ( int( graphs.size() ) > nCapacity )
		{
			pSession->szMessage = NStr::Format( "the template has %d graphs and room was given for %d", int( graphs.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorListRmg( BkEditorSession *pSession, int nKind, BkEditorRmgName *pOut, int nCapacity, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		EnsureRmgMount( pSession );
		if ( pnCount == 0 || nCapacity < 0 || ( pOut == 0 && nCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		std::vector<std::string> names;
		if ( !ListRmgFolder( pSession, nKind, &names ) )
			return BK_EDITOR_BAD_ARGUMENT;
		*pnCount = int( names.size() );
		const int nWrite = Min( int( names.size() ), nCapacity );
		for ( int i = 0; i < nWrite; ++i )
		{
			memset( &pOut[i], 0, sizeof pOut[i] );
			const size_t nCopy = Min( names[i].size(), sizeof pOut[i].name - 1 );
			memcpy( pOut[i].name, names[i].c_str(), nCopy );
		}
		if ( int( names.size() ) > nCapacity )
		{
			pSession->szMessage = NStr::Format( "the folder holds %d names and room was given for %d",
				int( names.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}


// ---------------------------------------------------------------------------
// The RMG composers (M3 05-09): containers and graphs through the engine's own
// serialisers, patch info and copy-in, the user RMG root.
// ---------------------------------------------------------------------------

namespace
{

// A record read that came out short: some count above its array's capacity.
// (A real refusal has every count 0 and was flagged refused.)
bool IsShort( const BkEditorRmgScripts &r )
{
	return r.id_count > r.id_capacity || r.area_count > r.area_capacity;
}

BkEditorStatus ReadStatus( bool bOk, bool bRefused, bool bShort )
{
	if ( bOk )
		return BK_EDITOR_OK;
	if ( bRefused || bShort )
		return BK_EDITOR_REFUSED;
	return BK_EDITOR_FAILED;
}

bool IsGoodCapacity( const void *pArray, int nCapacity )
{
	return nCapacity >= 0 && ( pArray != 0 || nCapacity == 0 );
}

}

BkEditorStatus BkEditorRmgReadContainer( BkEditorSession *pSession, const char *pszName, BkEditorRmgContainerRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pRecord == 0 || strnlen( pszName, 256 ) >= 256 ||
		     !IsGoodCapacity( pRecord->patches, pRecord->patch_capacity ) || !IsGoodCapacity( pRecord->indices, pRecord->index_capacity ) ||
		     !IsGoodCapacity( pRecord->scripts.ids, pRecord->scripts.id_capacity ) || !IsGoodCapacity( pRecord->scripts.areas, pRecord->scripts.area_capacity ) )
			return BK_EDITOR_BAD_ARGUMENT;
		EnsureRmgMount( pSession );
		bool bRefused = false;
		const bool bOk = ReadRmgContainerRecord( pSession, pszName, pRecord, &bRefused );
		const int nIndexTotal = pRecord->index_counts[0] + pRecord->index_counts[1] + pRecord->index_counts[2] + pRecord->index_counts[3];
		const bool bShort = pRecord->patch_count > pRecord->patch_capacity || nIndexTotal > pRecord->index_capacity || IsShort( pRecord->scripts );
		return ReadStatus( bOk, bRefused, bShort );
	} );
}

BkEditorStatus BkEditorRmgWriteContainer( BkEditorSession *pSession, const char *pszName, const BkEditorRmgContainerRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pRecord == 0 || strnlen( pszName, 256 ) >= 256 )
			return BK_EDITOR_BAD_ARGUMENT;
		EnsureRmgMount( pSession );
		bool bRefused = false, bBad = false;
		if ( WriteRmgContainerRecord( pSession, pszName, *pRecord, &bRefused, &bBad ) )
			return BK_EDITOR_OK;
		return bBad ? BK_EDITOR_BAD_ARGUMENT : ( bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED );
	} );
}

BkEditorStatus BkEditorRmgReadGraph( BkEditorSession *pSession, const char *pszName, BkEditorRmgGraphRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pRecord == 0 || strnlen( pszName, 256 ) >= 256 ||
		     !IsGoodCapacity( pRecord->nodes, pRecord->node_capacity ) || !IsGoodCapacity( pRecord->links, pRecord->link_capacity ) ||
		     !IsGoodCapacity( pRecord->scripts.ids, pRecord->scripts.id_capacity ) || !IsGoodCapacity( pRecord->scripts.areas, pRecord->scripts.area_capacity ) )
			return BK_EDITOR_BAD_ARGUMENT;
		EnsureRmgMount( pSession );
		bool bRefused = false;
		const bool bOk = ReadRmgGraphRecord( pSession, pszName, pRecord, &bRefused );
		const bool bShort = pRecord->node_count > pRecord->node_capacity || pRecord->link_count > pRecord->link_capacity || IsShort( pRecord->scripts );
		return ReadStatus( bOk, bRefused, bShort );
	} );
}

BkEditorStatus BkEditorRmgWriteGraph( BkEditorSession *pSession, const char *pszName, const BkEditorRmgGraphRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pRecord == 0 || strnlen( pszName, 256 ) >= 256 )
			return BK_EDITOR_BAD_ARGUMENT;
		EnsureRmgMount( pSession );
		bool bRefused = false, bBad = false;
		if ( WriteRmgGraphRecord( pSession, pszName, *pRecord, &bRefused, &bBad ) )
			return BK_EDITOR_OK;
		return bBad ? BK_EDITOR_BAD_ARGUMENT : ( bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED );
	} );
}

BkEditorStatus BkEditorRmgPatchInfoRead( BkEditorSession *pSession, const char *pszName, BkEditorRmgPatchInfo *pInfo )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pInfo == 0 || strnlen( pszName, 256 ) >= 256 ||
		     !IsGoodCapacity( pInfo->scripts.ids, pInfo->scripts.id_capacity ) || !IsGoodCapacity( pInfo->scripts.areas, pInfo->scripts.area_capacity ) )
			return BK_EDITOR_BAD_ARGUMENT;
		EnsureRmgMount( pSession );
		bool bRefused = false;
		const bool bOk = ReadRmgPatchInfo( pSession, pszName, pInfo, &bRefused );
		return ReadStatus( bOk, bRefused, IsShort( pInfo->scripts ) );
	} );
}

BkEditorStatus BkEditorRmgImportPatch( BkEditorSession *pSession, const char *pszSourcePath, int nApply, BkEditorRmgName *pOutName )
{
	if ( pOutName != 0 )
		memset( pOutName, 0, sizeof *pOutName );
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszSourcePath == 0 || pOutName == 0 || strnlen( pszSourcePath, 2048 ) >= 2048 || ( nApply != 0 && nApply != 1 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		EnsureRmgMount( pSession );
		std::string szName;
		bool bRefused = false;
		if ( !ImportRmgPatch( pSession, pszSourcePath, nApply == 1, &szName, &bRefused ) )
			return bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED;
		if ( szName.size() >= sizeof pOutName->name )
		{
			pSession->szMessage = "the destination name does not fit the caller's buffer";
			return BK_EDITOR_FAILED;
		}
		memcpy( pOutName->name, szName.c_str(), szName.size() + 1 );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorRmgRoot( BkEditorSession *pSession, char *pOut, int nCapacity )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 || nCapacity <= 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		const std::string szRoot = RmgRootHostPath( pSession->szModFolder );
		if ( int( szRoot.size() ) >= nCapacity )
		{
			pSession->szMessage = "the user RMG root does not fit the caller's buffer";
			return BK_EDITOR_REFUSED;
		}
		memcpy( pOut, szRoot.c_str(), szRoot.size() + 1 );
		return BK_EDITOR_OK;
	} );
}


// ---------------------------------------------------------------------------
// The Fields and Templates Composers (M3 05-10): field sets and templates
// through the engine's own serialisers, the season tilesets and the storage
// probe their Check! rules read.
// ---------------------------------------------------------------------------

BkEditorStatus BkEditorRmgReadFieldSet( BkEditorSession *pSession, const char *pszName, BkEditorRmgFieldSetRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pRecord == 0 || strnlen( pszName, 256 ) >= 256 ||
		     !IsGoodCapacity( pRecord->tile_shells, pRecord->tile_shell_capacity ) || !IsGoodCapacity( pRecord->tiles, pRecord->tile_capacity ) ||
		     !IsGoodCapacity( pRecord->object_shells, pRecord->object_shell_capacity ) || !IsGoodCapacity( pRecord->objects, pRecord->object_capacity ) )
			return BK_EDITOR_BAD_ARGUMENT;
		EnsureRmgMount( pSession );
		bool bRefused = false;
		const bool bOk = ReadRmgFieldSetRecord( pSession, pszName, pRecord, &bRefused );
		const bool bShort = pRecord->tile_shell_count > pRecord->tile_shell_capacity || pRecord->tile_total > pRecord->tile_capacity ||
		                    pRecord->object_shell_count > pRecord->object_shell_capacity || pRecord->object_total > pRecord->object_capacity;
		return ReadStatus( bOk, bRefused, bShort );
	} );
}

BkEditorStatus BkEditorRmgWriteFieldSet( BkEditorSession *pSession, const char *pszName, const BkEditorRmgFieldSetRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pRecord == 0 || strnlen( pszName, 256 ) >= 256 )
			return BK_EDITOR_BAD_ARGUMENT;
		EnsureRmgMount( pSession );
		bool bRefused = false, bBad = false;
		if ( WriteRmgFieldSetRecord( pSession, pszName, *pRecord, &bRefused, &bBad ) )
			return BK_EDITOR_OK;
		return bBad ? BK_EDITOR_BAD_ARGUMENT : ( bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED );
	} );
}

BkEditorStatus BkEditorRmgReadTemplate( BkEditorSession *pSession, const char *pszName, BkEditorRmgTemplateRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pRecord == 0 || strnlen( pszName, 256 ) >= 256 ||
		     !IsGoodCapacity( pRecord->fields, pRecord->field_capacity ) || !IsGoodCapacity( pRecord->graphs, pRecord->graph_capacity ) ||
		     !IsGoodCapacity( pRecord->vso, pRecord->vso_capacity ) || !IsGoodCapacity( pRecord->diplomacies, pRecord->diplomacy_capacity ) ||
		     !IsGoodCapacity( pRecord->units, pRecord->unit_capacity ) ||
		     !IsGoodCapacity( pRecord->scripts.ids, pRecord->scripts.id_capacity ) || !IsGoodCapacity( pRecord->scripts.areas, pRecord->scripts.area_capacity ) )
			return BK_EDITOR_BAD_ARGUMENT;
		EnsureRmgMount( pSession );
		bool bRefused = false;
		const bool bOk = ReadRmgTemplateRecord( pSession, pszName, pRecord, &bRefused );
		const bool bShort = pRecord->field_count > pRecord->field_capacity || pRecord->graph_count > pRecord->graph_capacity || pRecord->vso_count > pRecord->vso_capacity ||
		                    pRecord->diplomacy_count > pRecord->diplomacy_capacity || pRecord->unit_count > pRecord->unit_capacity || IsShort( pRecord->scripts );
		return ReadStatus( bOk, bRefused, bShort );
	} );
}

BkEditorStatus BkEditorRmgWriteTemplate( BkEditorSession *pSession, const char *pszName, const BkEditorRmgTemplateRecord *pRecord )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pRecord == 0 || strnlen( pszName, 256 ) >= 256 )
			return BK_EDITOR_BAD_ARGUMENT;
		EnsureRmgMount( pSession );
		bool bRefused = false, bBad = false;
		if ( WriteRmgTemplateRecord( pSession, pszName, *pRecord, &bRefused, &bBad ) )
			return BK_EDITOR_OK;
		return bBad ? BK_EDITOR_BAD_ARGUMENT : ( bRefused ? BK_EDITOR_REFUSED : BK_EDITOR_FAILED );
	} );
}

BkEditorStatus BkEditorRmgTileset( BkEditorSession *pSession, int nSeason, BkEditorRmgTerrainType *pOut, int nCapacity, int *pnCount )
{
	if ( pnCount != 0 )
		*pnCount = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( pOut == 0 && nCapacity > 0 ) || nSeason < 0 || nSeason > 3 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		EnsureRmgMount( pSession );
		std::vector<std::pair<std::string, int> > types;
		if ( !ListRmgTerrainTypes( pSession, nSeason, &types ) )
			return BK_EDITOR_REFUSED;
		*pnCount = int( types.size() );
		const int nWrite = Min( int( types.size() ), nCapacity );
		for ( int i = 0; i < nWrite; ++i )
		{
			memset( &pOut[i], 0, sizeof pOut[i] );
			const size_t nCopy = Min( types[size_t( i )].first.size(), sizeof pOut[i].name - 1 );
			memcpy( pOut[i].name, types[size_t( i )].first.c_str(), nCopy );
			pOut[i].variant_count = types[size_t( i )].second;
		}
		if ( int( types.size() ) > nCapacity )
		{
			pSession->szMessage = NStr::Format( "the tileset holds %d terrain types and room was given for %d", int( types.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorRmgFileExists( BkEditorSession *pSession, const char *pszName, const char *pszExtension, int *pnExists )
{
	if ( pnExists != 0 )
		*pnExists = 0;
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszName == 0 || pszExtension == 0 || pnExists == 0 || strnlen( pszName, 256 ) >= 256 || strnlen( pszExtension, 32 ) >= 32 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( !pSession->bEngineStarted )
		{
			pSession->szMessage = "the engine is not started";
			return BK_EDITOR_REFUSED;
		}
		EnsureRmgMount( pSession );
		bool bExists = false, bBad = false;
		if ( !RmgFileExists( pSession, pszName, pszExtension, &bExists, &bBad ) )
			return bBad ? BK_EDITOR_BAD_ARGUMENT : BK_EDITOR_FAILED;
		*pnExists = bExists ? 1 : 0;
		return BK_EDITOR_OK;
	} );
}
