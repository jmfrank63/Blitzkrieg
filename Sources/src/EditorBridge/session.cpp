// Building the engine state a map is edited through.
//
// Kept apart from bridge.cpp because it is the one part of the bridge that is
// not about the C boundary: it is the MFC editor's map-open sequence with the
// MFC taken out and one guard put in.
#include "StdAfx.h"
#include <algorithm>
#include <cstring>
#include "session.h"
#include "world.h"
#include "../MapFile/MapFile.h"
#include "../MapFile/MapEquivalence.h"
#include "../MapFile/MapRecords.h"
#include "../Main/GameDB.h"
#include "../AILogic/AILogic.h"
#include "../Scene/Scene.h"
#include "../GFX/GFX.H"
#include "../Scene/Terrain.h"
#include "../Formats/fmtTerrain.h"
#include "../RandomMapGen/VA_Types.h"

// A sound and a tank pit are in the object database, and so in the catalogue,
// but neither is a map object. A map keeps its sounds in CMapInfo::soundsList
// (the MFC editor edits them in their own dialog, MapSoundInfo.cpp, and the
// game hands that list to IScene::InitMapSounds), and the game's
// CAILogic::AddObject returns 0 for a sound. A tank pit is dug by engineers
// during play; AddObject has no case for one and would treat its unit stats
// as a static object's. The MFC editor's palette leaves both out: it lists only
// paths under buildings, objects, squads and units, and drops tank_pit
// (TabSimpleObjectsDialog.cpp:158-174). Asking the engine anyway is what
// crashed the editor, in CheckStaticObject, reading a footprint neither has.
const char* WhyNotAMapObject( int nGameType )
{
	if ( nGameType == SGVOGT_SOUND )
		return "is a sound; a map keeps its sounds in their own list, not among its objects";
	if ( nGameType == SGVOGT_TANK_PIT )
		return "is a tank pit, which engineers dig during play; a map cannot hold one";
	return 0;
}

// D-05. Kept apart from WhyNotAMapObject on purpose: that one guards
// PlaceOneObject too, and a loaded map's bridge spans, trench pieces and fences
// must keep being placed in the engine.
const char* WhyNotPlacedByPalette( int nGameType )
{
	if ( nGameType == SGVOGT_ENTRENCHMENT )
		return "is a trench piece; entrenchments are drawn with the Entrenchment tool";
	if ( nGameType == SGVOGT_BRIDGE )
		return "is a bridge span; bridges are drawn with the Bridge tool";
	if ( nGameType == SGVOGT_FENCE )
		return "is a fence; fences are drawn with the Fence tool";
	return 0;
}

// Placing one object, as CTemplateEditorFrame::AddObjectByAI does it
// (TemplateEditorFrame1.cpp:2778-2810). Everything after that line in the MFC
// version is the editor's own bookkeeping - visual object, undo item, list
// entry - and belongs to whatever draws this, not here.
//
// AddNewObject's return value is not the answer to "did it work":
// CAIEditor::AddNewObject ends in an unconditional `return false`
// (AIEditorInternal.cpp:46). It reports success by writing the object out, and
// the MFC editor tests the pointer. Trusting the bool places nothing at all and
// says nothing about it.
IRefCount* PlaceOneObject( const SMapObjectInfo &rObject, const SGDBObjectDesc *pDesc, IAIEditor *pAIEditor )
{
	if ( pDesc == 0 || WhyNotAMapObject( pDesc->eGameType ) != 0 )
		return 0;
	SMapObjectInfo object = rObject;
	if ( object.fHP > 1.0f )
		object.fHP = 1.0f;
	if ( pDesc->eGameType == SGVOGT_SQUAD )
		object.nFrameIndex = 0;
	// The player is handed to the engine only for a building; everything else
	// goes in unowned and is given its player back by the editor above. The map
	// keeps the real value either way, so what gets saved is unaffected.
	const int nPlayer = object.nPlayer;
	if ( pDesc->eGameType != SGVOGT_BUILDING )
		object.nPlayer = 0;
	if ( !pAIEditor->IsObjectInsideOfMap( object ) )
		return 0;
	IRefCount *pAIObject = 0;
	pAIEditor->AddNewObject( object, &pAIObject );
	if ( pAIObject != 0 && pDesc->eGameType == SGVOGT_BUILDING )
		pAIEditor->SetPlayer( pAIObject, nPlayer );
	return pAIObject;
}

namespace {

// The snapshot keeps frame indices packed; the working copy is unpacked,
// because unpacking picks a random visual variant per type and that choice must
// never reach a file for an object the editor did not touch.
void MakeWorkingCopy( SEditorSession *pSession )
{
	pSession->working = pSession->snapshot;
	pSession->working.UnpackFrameIndices();
	// A map saved without altitudes gets a flat sheet, as the MFC editor does:
	// UpdateTerrainShades reads the altitudes and would divide by an empty one.
	STerrainInfo &rTerrain = pSession->working.terrain;
	if ( rTerrain.altitudes.GetSizeX() == 0 || rTerrain.altitudes.GetSizeY() == 0 )
	{
		rTerrain.altitudes.SetSizes( rTerrain.patches.GetSizeX() * 16 + 1, rTerrain.patches.GetSizeY() * 16 + 1 );
		rTerrain.altitudes.SetZero();
	}
	CMapInfo::UpdateTerrainShades( &rTerrain,
	                               CTRect<int>( 0, 0, rTerrain.altitudes.GetSizeX(), rTerrain.altitudes.GetSizeY() ),
	                               CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( pSession->working.nSeason ) ) );
}

// One pass over objects or scenarioObjects.
//
// The MFC editor writes pObjectsDB->GetDesc( name )->eGameType with no null
// check (TemplateEditorFrame1.cpp:1751 and 1783) before it ever reaches
// AddObjectByAI, which does check. GetDesc returns 0 for a name the database
// does not know, and the NI_ASSERT inside it compiles away in release, so
// opening a map that names an object the mod no longer ships crashes the
// editor. Here an unknown object is listed, left in the snapshot and never
// placed - which is also what lets it survive a save untouched.
//
// A bridge span is not placed here. It is placed through the bridge it belongs
// to, in the order CMapInfo::bridges lists it, so the spans of one bridge end
// up in the file's order; the editor collects them the same way and builds them
// after all the loose objects are in.
void PlaceObjects( SEditorSession *pSession, const std::vector<SMapObjectInfo> &rObjects,
                   IObjectsDB *pObjectsDB, IAIEditor *pAIEditor, std::vector<SMapObjectInfo> *pBridgeSpans )
{
	for ( std::vector<SMapObjectInfo>::const_iterator it = rObjects.begin(); it != rObjects.end(); ++it )
	{
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( it->szName.c_str() );
		if ( pDesc == 0 )
		{
			pSession->unknownLinkIDs.push_back( it->link.nLinkID );
			continue;
		}
		if ( pDesc->eGameType == SGVOGT_BRIDGE )
		{
			pBridgeSpans->push_back( *it );
			continue;
		}
		// Objects sharing a link ID (0, "none", most often) are all placed and
		// drawn, but only the last is kept here; RefuseSharedLinkID is what
		// keeps an edit from reaching the wrong one.
		if ( IRefCount *pAIObject = PlaceOneObject( *it, pDesc, pAIEditor ) )
			pSession->byLinkID[it->link.nLinkID] = pAIObject;
	}
}

// Bridges, after everything else (TemplateEditorFrame1.cpp:1948-1979).
//
// CMapInfo::bridges is a list of bridges, each a list of the link IDs of its
// spans in order, and the spans themselves are ordinary entries in objects or
// scenarioObjects that PlaceObjects set aside. Building them here rather than
// where they were found is what keeps a bridge's spans in the order the file
// gives them.
void BuildBridges( SEditorSession *pSession, const std::vector<SMapObjectInfo> &rSpans )
{
	const std::vector< std::vector<int> > &rBridges = pSession->working.bridges;
	// A link ID named twice - by one entry or by two - is one span, and
	// BuildOneBridge places it once (a second placement is the engine's
	// "Repeated link" and orphans the first object).
	std::vector<int> unique;
	for ( size_t nBridge = 0; nBridge < rBridges.size(); ++nBridge )
	{
		unique.insert( unique.end(), rBridges[nBridge].begin(), rBridges[nBridge].end() );
		pSession->nBridgeSpansPlaced += BuildOneBridge( pSession, rBridges[nBridge], rSpans );
	}
	std::sort( unique.begin(), unique.end() );
	pSession->nBridgeSpansInMap = int( std::unique( unique.begin(), unique.end() ) - unique.begin() );
}
}

// One bridge's spans, in the order its entry lists them (BuildBridges' per
// bridge part, 04-06: the Bridge tool builds a drawn, restored or rotated
// bridge the same way). A span stored with negative HP is one the mission
// builds during play. The engine will not take it that way, so - as the
// editor does - it is created at full HP and its link ID listed; the snapshot
// still holds the negative value, so what gets written back is unchanged. The
// snapshot is asked as well as rSpans, because the working copy of a bridge
// the Bridge tool toggled keeps HP 1.
int BuildOneBridge( SEditorSession *pSession, const std::vector<int> &rLinkIDs, const std::vector<SMapObjectInfo> &rSpans )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pObjectsDB == 0 || pAIEditor == 0 )
		return 0;
	int nPlaced = 0;
	for ( size_t nSpan = 0; nSpan < rLinkIDs.size(); ++nSpan )
	{
		const int nLinkID = rLinkIDs[nSpan];
		// A link ID the entry names twice, or another entry already placed, is
		// one object: placing it again is the engine's "Repeated link"
		// (asserted in a debug build) and would orphan the first engine object.
		if ( std::find( rLinkIDs.begin(), rLinkIDs.begin() + nSpan, nLinkID ) != rLinkIDs.begin() + nSpan ||
		     pSession->byLinkID.find( nLinkID ) != pSession->byLinkID.end() )
			continue;
		std::vector<SMapObjectInfo>::const_iterator it = rSpans.begin();
		for ( ; it != rSpans.end(); ++it )
			if ( it->link.nLinkID == nLinkID )
				break;
		if ( it == rSpans.end() )
			continue;
		SMapObjectInfo span = *it;
		const SMapObjectInfo *pSaved = FindSnapshotObject( *pSession, nLinkID );
		if ( span.fHP < 0 || ( pSaved != 0 && pSaved->fHP < 0 ) )
		{
			span.fHP = 1.0f;
			if ( std::find( pSession->futureBuildLinkIDs.begin(), pSession->futureBuildLinkIDs.end(), nLinkID ) == pSession->futureBuildLinkIDs.end() )
				pSession->futureBuildLinkIDs.push_back( nLinkID );
		}
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( span.szName.c_str() );
		if ( pDesc == 0 )
			continue;
		if ( IRefCount *pAIObject = PlaceOneObject( span, pDesc, pAIEditor ) )
		{
			pSession->byLinkID[nLinkID] = pAIObject;
			++nPlaced;
		}
	}
	return nPlaced;
}

bool OpenMapIntoSession( SEditorSession *pSession, const char *pszPath )
{
	if ( pSession == 0 )
		return false;
	if ( !pSession->bEngineStarted )
	{
		pSession->szMessage = "the engine is not started";
		return false;
	}
	if ( pszPath == 0 || *pszPath == 0 )
	{
		pSession->szMessage = "no map path";
		return false;
	}

	// Nothing is disturbed until the read has succeeded and the engine is known
	// to be there: every way out before the line that clears bMapOpen leaves the
	// session on whatever map it already had, which is what makes a broken file
	// the common, harmless failure. NMapFile::Read answers a file that throws
	// while it is read with false, so that is such a way out too.
	CMapInfo read;
	if ( !NMapFile::Read( pszPath, &read, &pSession->szMessage ) )
		return false;
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	IScene *pScene = GetSingleton<IScene>();
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pAIEditor == 0 || pScene == 0 || pObjectsDB == 0 )
	{
		pSession->szMessage = "the engine is missing the AI editor, the scene or the object database";
		return false;
	}

	// From here on the old map is gone: the engine is global, and it is cleared
	// and rebuilt in place, so there is nothing to go back to. Only a throw can
	// leave this path early, and it leaves the session with no map open.
	return InstallMapInSession( pSession, read, pszPath );
}

// Everything after a map has been read or built: the per-map tables are
// reset, the working copy is made, the engine is rebuilt from it and the
// camera placed on the middle. OpenMapIntoSession and NewMapInSession both
// end here; pszPath is what the terrain loader names its sidecar files after
// (the map's own path for an open, its name for a new one - nothing of a new
// map is on disk, exactly the MFC editor's own Load(m_currentMapName)).
bool InstallMapInSession( SEditorSession *pSession, const CMapInfo &read, const char *pszPath )
{
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	IScene *pScene = GetSingleton<IScene>();
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pAIEditor == 0 || pScene == 0 || pObjectsDB == 0 )
	{
		pSession->szMessage = "the engine is missing the AI editor, the scene or the object database";
		return false;
	}
	pSession->bMapOpen = false;
	pSession->byLinkID.clear();
	pSession->unknownLinkIDs.clear();
	pSession->futureBuildLinkIDs.clear();
	pSession->nBridgeSpansInMap = 0;
	pSession->nBridgeSpansPlaced = 0;
	pSession->snapshot = read;
	// F5 (M3, D-23): a map whose file lacks altitudes gets a flat sheet on
	// the snapshot too - the MFC editor's own load rule
	// (TemplateEditorFrame1.cpp:1658) - so a save writes the zeros rather
	// than an empty sheet back. MakeWorkingCopy gives the working copy the
	// same sheet it always did; with the snapshot holding one already, its
	// own branch simply does not fire.
	if ( pSession->snapshot.terrain.altitudes.GetSizeX() == 0 || pSession->snapshot.terrain.altitudes.GetSizeY() == 0 )
	{
		pSession->snapshot.terrain.altitudes.SetSizes( pSession->snapshot.terrain.patches.GetSizeX() * STerrainPatchInfo::nSizeX + 1,
		                                              pSession->snapshot.terrain.patches.GetSizeY() * STerrainPatchInfo::nSizeY + 1 );
		pSession->snapshot.terrain.altitudes.SetZero();
	}
	// 05-02 (Rule 1): the snapshot is a COPY of the read, and the copy's
	// SVertexAltitude padding bytes are whatever the heap held there - a
	// save wrote them, so every byte-for-byte proof of an altitude edit
	// since 05-01 rode on heap luck. Pin them to what a fresh read holds -
	// the file's own padding - by zeroing the three pad bytes of every
	// vertex once, here, at install: capture, restore and save all agree
	// from then on, whatever the allocator did. (SetZero already zeroed the
	// F5 sheet's records whole, so this is a no-op there.)
	{
		STerrainInfo::TVertexAltitudeArray2D &rSheet = pSession->snapshot.terrain.altitudes;
		for ( int nY = 0; nY < rSheet.GetSizeY(); ++nY )
			for ( int nX = 0; nX < rSheet.GetSizeX(); ++nX )
				memset( &rSheet[nY][nX].shade + 1, 0, sizeof( SVertexAltitude ) - 5 );
	}
	pSession->hiddenScriptIDs.clear();
	pSession->hiddenLinkIDs.clear();
	pSession->szScriptFileAtOpen = read.szScriptFile;
	pSession->openedAreaNames.clear();
	for ( size_t i = 0; i < read.scriptAreas.size(); ++i )
		++pSession->openedAreaNames[read.scriptAreas[i].szName];
	pSession->openedAreas.assign( read.scriptAreas.begin(), read.scriptAreas.end() );
	pSession->openedStartCommands.assign( read.startCommandsList.begin(), read.startCommandsList.end() );
	pSession->openedReservePositions.assign( read.reservePositionsList.begin(), read.reservePositionsList.end() );
	pSession->openedAISides = read.aiGeneralMapInfo.sidesInfo;
	{
		NMapRecords::SCameraAnchors anchors;
		NMapRecords::GetCameraAnchors( read, &anchors );
		pSession->vOpenedNeutralAnchor = anchors.vNeutral;
		pSession->openedPlayerAnchors = anchors.players;
	}
	pSession->openedGroups.clear();
	for ( std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator it = read.reinforcements.groups.begin(); it != read.reinforcements.groups.end(); ++it )
		pSession->openedGroups[it->first] = it->second.ids;
	pSession->szMapPath = pszPath != 0 ? pszPath : "";
	pSession->paints.clear();
	pSession->appliedPaints.clear();
	pSession->undonePaints.clear();
	ClearEditLog( pSession );
	pSession->vsoEngineIDs[0].clear();
	pSession->vsoEngineIDs[1].clear();
	pSession->tombstones.clear();
	pSession->nLinkIDFloor = NMapOverlay::NextLinkID( pSession->snapshot );
	pSession->linkByAI.clear();
	MakeWorkingCopy( pSession );

	// The old map's objects leave the world before the AI they refer to is
	// cleared. CWorldBase::Clear empties the scene and takes its terrain away
	// too, so it comes before the new terrain is set, never after.
	if ( pSession->pWorld != 0 )
		pSession->pWorld->Clear();

	// The AI editor first: the terrain it is initialised with is what
	// IsObjectInsideOfMap and AddNewObject answer against below.
	pAIEditor->Clear();
	pAIEditor->SetDiplomacies( pSession->working.diplomacies );
	pAIEditor->Init( pSession->working.terrain );
	// No war fog: the editor sees every object. The MFC editor switches it off
	// in both places once the map is in (TemplateEditorFrame1.cpp:1703 and
	// 1983). Both calls are toggles that answer with the new state, so each is
	// asked at most twice until it says the fog is off - an unbounded loop would
	// hang on a toggle that never answered as expected. In the scene the fog
	// also hides units from IScene::Pick (CCheckObjectVisibleFunctional).
	for ( int i = 0; i < 2; ++i )
		if ( pAIEditor->ToggleShow( 0 ) )			// true: the AI's fog is turned off
			break;

	{
		// ITerrain::Load takes the map's own path, as the MFC editor passes
		// szSelectedFileMapFullName: it derives the names of the files beside
		// the map from it.
		CPtr<ITerrain> pTerrain = CreateTerrain();
		pTerrain->Load( pSession->szMapPath.c_str(), pSession->working.terrain );
		pScene->SetTerrain( pTerrain );
	}
	// The engine's road and river nIDs, by list position: the terrain has just
	// loaded both lists from the working copy, in file order.
	ResetVsoEngineIDs( pSession );

	// The map's season, before a single object is built: CreateMapObject
	// hands the world's season to every map object, which picks its winter or
	// summer model and texture from it once, when it is made. Without this the
	// world keeps CWorldBase's SEASON_SUMMER and a winter map's units are drawn
	// in their summer paint. The game sets it at the same point, after the
	// terrain and before the mission's objects (iMissionInternal.cpp:1495), and
	// the MFC editor on every load (TemplateEditorFrame1.cpp:1683). It also sets
	// the scene's sun and World.Season. Every map the session holds - opened,
	// new, reloaded - comes in through here, so each one gets its own season
	// and never the previous map's.
	if ( pSession->pWorld != 0 )
		pSession->pWorld->SetSeason( pSession->working.nSeason );
	else
		pScene->SetSeason( pSession->working.nSeason );

	for ( int i = 0; i < 2; ++i )
		if ( !pScene->ToggleShow( SCENE_SHOW_WARFOG ) )	// false: the scene's fog is off
			break;

	std::vector<SMapObjectInfo> bridgeSpans;
	PlaceObjects( pSession, pSession->working.objects, pObjectsDB, pAIEditor, &bridgeSpans );
	PlaceObjects( pSession, pSession->working.scenarioObjects, pObjectsDB, pAIEditor, &bridgeSpans );
	BuildBridges( pSession, bridgeSpans );
	// The AI has queued a notification for every object it took; one update
	// turns them into map objects with visuals in the scene.
	UpdateSessionWorld( pSession );

	// The camera on the map's middle, placed as the game places its mission
	// camera, so a frame drawn before the caller's first BkEditorSetCamera
	// looks at the map rather than along CCamera's default placement. No
	// camera is not a failed open: the frame reports that itself.
	const float fMiddleX = float( pSession->working.terrain.tiles.GetSizeX() ) * fWorldCellSize / 2;
	const float fMiddleY = float( pSession->working.terrain.tiles.GetSizeY() ) * fWorldCellSize / 2;
	if ( !SetSessionCamera( pSession, fMiddleX, fMiddleY ) )
		pSession->szMessage.clear();

	pSession->bMapOpen = true;
	return true;
}

// File > New (M3, D-23): the engine builds the map and it opens through the
// same install the open path uses. Sizes are in patches per axis (1..32),
// nSeason is the dialog's 0..3 (Summer/Winter/Africa/Spring - REAL_SEASONS
// maps it to the value the map stores), pszName is what the terrain loader's
// sidecar files are named after (the MFC editor's own m_currentMapName), and
// the mod stamp is the active mod's name/version (empty for none), put on at
// creation the way the MFC editor's new map carries it.
bool NewMapInSession( SEditorSession *pSession, int nSizeX, int nSizeY, int nSeason, const char *pszName,
                      const std::string &rszModName, const std::string &rszModVersion )
{
	if ( pSession == 0 )
		return false;
	if ( !pSession->bEngineStarted )
	{
		pSession->szMessage = "the engine is not started";
		return false;
	}
	if ( nSizeX < 1 || nSizeX > 32 || nSizeY < 1 || nSizeY > 32 ||
	     nSeason < 0 || nSeason >= CMapInfo::SEASON_COUNT )
	{
		pSession->szMessage = "a new map is 1..32 patches per axis, the season Summer/Winter/Africa/Spring";
		return false;
	}
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	IScene *pScene = GetSingleton<IScene>();
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pAIEditor == 0 || pScene == 0 || pObjectsDB == 0 )
	{
		pSession->szMessage = "the engine is missing the AI editor, the scene or the object database";
		return false;
	}

	// The MFC editor's own sequence (OnFileNewMap, TemplateEditorFrame1.cpp:
	// 2479-2512): Create of the size and the season - two neutral players,
	// a single-player map, altitudes sized and zeroed - then every tile the
	// season's most common tile.
	CMapInfo read;
	if ( !CMapInfo::Create( &read, CTPoint<int>( nSizeX, nSizeY ), CMapInfo::REAL_SEASONS[nSeason],
	                        CMapInfo::SEASON_FOLDERS[nSeason], 2, CMapInfo::TYPE_SINGLE_PLAYER ) )
	{
		pSession->szMessage = "the engine would not create a map of that size and season";
		return false;
	}
	if ( !read.FillTerrain( CMapInfo::MOST_COMMON_TILES[nSeason] ) )
	{
		pSession->szMessage = "the season's tileset did not read, so the new map's tiles cannot be filled";
		return false;
	}
	// The shades the season's sun makes over the zero sheet (the MFC editor's
	// UpdateTerrainShades at :2529); Create has already sized and zeroed the
	// altitudes.
	if ( !CMapInfo::UpdateTerrainShades( &read.terrain,
	                                     CTRect<int>( 0, 0, read.terrain.altitudes.GetSizeX(), read.terrain.altitudes.GetSizeY() ),
	                                     CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( read.nSeason ) ) ) )
	{
		pSession->szMessage = "the new map's shades did not compute";
		return false;
	}
	// The mod stamp, at creation, the way the MFC editor's new map carries it
	// (OnFileNewMap's szNewMODKey handling at :2464-2476): empty with no mod.
	read.szMODName = rszModName;
	read.szMODVersion = rszModVersion;

	const std::string szTerrainName = ( pszName != 0 && *pszName != 0 ) ? pszName : "new map";
	if ( !InstallMapInSession( pSession, read, szTerrainName.c_str() ) )
		return false;
	// A new map is never-saved: the session holds no path for it. The name
	// went to the terrain loader only.
	pSession->szMapPath.clear();
	return true;
}

void CloseSessionMap( SEditorSession *pSession )
{
	if ( pSession == 0 || !pSession->bMapOpen )
		return;
	// The old map's objects leave the world before the AI they refer to is
	// cleared - the same order OpenMapIntoSession closes the previous map in.
	if ( pSession->pWorld != 0 )
		pSession->pWorld->Clear();
	if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
		pAIEditor->Clear();
	pSession->byLinkID.clear();
	pSession->unknownLinkIDs.clear();
	pSession->futureBuildLinkIDs.clear();
	pSession->paints.clear();
	pSession->appliedPaints.clear();
	pSession->undonePaints.clear();
	ClearEditLog( pSession );
	pSession->vsoEngineIDs[0].clear();
	pSession->vsoEngineIDs[1].clear();
	pSession->tombstones.clear();
	pSession->linkByAI.clear();
	pSession->hiddenScriptIDs.clear();
	pSession->hiddenLinkIDs.clear();
	pSession->bMapOpen = false;
}

void UpdateSessionWorld( SEditorSession *pSession )
{
	// Hidden objects are in the scene for the world's update and out again
	// after it (ApplyHiddenMarks, below).
	ShowHiddenForUpdate( pSession );
	if ( pSession->pWorld != 0 )
		pSession->pWorld->UpdateNow();
	pSession->linkByAI.clear();
	for ( std::unordered_map<int, CPtr<IRefCount> >::const_iterator it = pSession->byLinkID.begin(); it != pSession->byLinkID.end(); ++it )
		pSession->linkByAI[it->second.GetPtr()] = it->first;
	// After the update: a span the AI just built has its world object only now.
	ApplyBridgeMarks( pSession );
	ApplyHiddenMarks( pSession );
}

bool SaveSessionMap( SEditorSession *pSession, const char *pszPath )
{
	if ( pSession == 0 )
		return false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	if ( pszPath == 0 || *pszPath == 0 )
	{
		pSession->szMessage = "no map path";
		return false;
	}
	// The snapshot, with whatever the editor has changed already laid over it -
	// never pSession->working. The working copy has UnpackFrameIndices applied,
	// so writing it back would give every fence, entrenchment and bridge span on
	// the map a fresh random frame index, in a file the editor never edited.
	if ( !NMapFile::Write( pszPath, pSession->snapshot, &pSession->szMessage ) )
		return false;

	// D-19's safe save relies on this: the editor writes to a temporary path
	// and swaps it in only when the write is proven. Read the file back and
	// compare it with what was meant, so a write that silently produced
	// something else (a truncated stream, a stale handle) is refused here,
	// before the caller's temporary file ever replaces the user's map.
	CMapInfo readBack;
	std::string szReadError;
	if ( !NMapFile::Read( pszPath, &readBack, &szReadError ) )
	{
		pSession->szMessage = "the written map does not read back: " + szReadError;
		return false;
	}
	std::string szWhere;
	if ( !NMapFile::AreEquivalent( pSession->snapshot, readBack, &szWhere ) )
	{
		pSession->szMessage = "the written map reads back different at " + szWhere;
		return false;
	}
	return true;
}

namespace {

// The object's record in whichever of the two lists holds it.
SMapObjectInfo* FindIn( CMapInfo *pMap, int nLinkID )
{
	std::vector<SMapObjectInfo> *lists[2] = { &pMap->objects, &pMap->scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < (*lists[nList]).size(); ++i )
			if ( (*lists[nList])[i].link.nLinkID == nLinkID )
				return &(*lists[nList])[i];
	return 0;
}

// How many records of the map carry this link ID. A file does not promise one
// per object: 0 is RMGC_INVALID_LINK_ID_VALUE, "no link ID", and the MFC editor
// never needed more, because it knows its objects by their AI pointer and hands
// out link IDs only when it saves (AIToLink, TemplateEditorFrame1.cpp:3069).
// Measured on arnheim: 361 objects - river banks, ravines, flowers - all carry
// 0. Every one of them is placed and drawn, but byLinkID keeps one engine
// object per link ID, so it holds the last of them and FindIn finds the first:
// an edit of link ID 0 would change one record in the file and another object
// on screen.
int CountIn( const CMapInfo &rMap, int nLinkID )
{
	int nCount = 0;
	const std::vector<SMapObjectInfo> *lists[2] = { &rMap.objects, &rMap.scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < (*lists[nList]).size(); ++i )
			if ( (*lists[nList])[i].link.nLinkID == nLinkID )
				++nCount;
	return nCount;
}

// A link ID more than one object of the map carries names none of them: the
// session cannot tell which record the engine object it holds belongs to. So an
// edit of one is refused, not guessed at. The objects stay as they are and are
// saved as they were read.
bool RefuseSharedLinkID( SEditorSession *pSession, int nLinkID, bool *pbRefused )
{
	const int nCount = CountIn( pSession->snapshot, nLinkID );
	if ( nCount <= 1 )
		return false;
	pSession->szMessage = NStr::Format( "%d objects of the map share link ID %d, so the editor cannot tell which of them "
	                                    "it would change; they are kept as they are", nCount, nLinkID );
	if ( pbRefused ) *pbRefused = true;
	return true;
}

// The ABI is in floats because a map's positions are; IAIEditor::MoveObject is
// in shorts. Rounding happens here and nowhere else, so the snapshot and the
// engine can never end up one unit apart because two places rounded
// differently.
short ToEngineCoord( float f )
{
	return short( f < 0.0f ? f - 0.5f : f + 0.5f );
}

bool EngineIsAt( IAIEditor *pAIEditor, IRefCount *pObject, const CVec3 &vPos )
{
	const CVec2 vCenter = pAIEditor->GetCenter( pObject );
	return ToEngineCoord( vCenter.x ) == ToEngineCoord( vPos.x ) &&
	       ToEngineCoord( vCenter.y ) == ToEngineCoord( vPos.y );
}

bool EngineFacesDir( IAIEditor *pAIEditor, IRefCount *pObject, int nDir )
{
	return pAIEditor->GetDir( pObject ) == WORD( nDir );
}

bool EngineBelongsTo( IAIEditor *pAIEditor, IRefCount *pObject, int nPlayer )
{
	// -1 means the object's kind has no owner, which is not the same as
	// belonging to player -1: the engine cannot hold what was asked for.
	const int nEnginePlayer = pAIEditor->GetPlayer( pObject );
	return nEnginePlayer >= 0 && nEnginePlayer == nPlayer;
}

// What the engine is holding for an object, which is not always what the map
// says: PlaceOneObject hands the engine player 0 for everything that is not a
// building, and the map keeps the real owner. So a rollback has to put back
// what was read here, never what the snapshot happened to say.
SEngineObjectState ReadEngine( IAIEditor *pAIEditor, IRefCount *pObject )
{
	SEngineObjectState state;
	state.vCenter = pAIEditor->GetCenter( pObject );
	state.wDir = pAIEditor->GetDir( pObject );
	state.nPlayer = pAIEditor->GetPlayer( pObject );
	return state;
}
}

bool ReadEngineObject( const SEditorSession &rSession, int nLinkID, SEngineObjectState *pOut )
{
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	std::unordered_map<int, CPtr<IRefCount> >::const_iterator it = rSession.byLinkID.find( nLinkID );
	if ( pAIEditor == 0 || it == rSession.byLinkID.end() || pOut == 0 )
		return false;
	*pOut = ReadEngine( pAIEditor, it->second );
	return true;
}

const SMapObjectInfo* FindSnapshotObject( const SEditorSession &rSession, int nLinkID )
{
	return FindIn( const_cast<CMapInfo*>( &rSession.snapshot ), nLinkID );
}

bool ReadSessionObjects( SEditorSession *pSession, BkEditorObjectRecord *pOut, int nCapacity, int *pnCount )
{
	const CMapInfo &rMap = pSession->snapshot;
	const int nTotal = int( rMap.objects.size() + rMap.scenarioObjects.size() );
	*pnCount = nTotal;
	const std::vector<SMapObjectInfo> *lists[2] = { &rMap.objects, &rMap.scenarioObjects };
	int nOut = 0;
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size() && nOut < nCapacity; ++i, ++nOut )
		{
			const SMapObjectInfo &rObject = (*lists[nList])[i];
			BkEditorObjectRecord &rRecord = pOut[nOut];
			memset( &rRecord, 0, sizeof rRecord );
			rRecord.link_id = rObject.link.nLinkID;
			strncpy( rRecord.name, rObject.szName.c_str(), sizeof rRecord.name - 1 );
			rRecord.x = rObject.vPos.x;
			rRecord.y = rObject.vPos.y;
			rRecord.dir = rObject.nDir;
			rRecord.player = rObject.nPlayer;
			rRecord.scenario = nList;
			rRecord.known = std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(),
			                           rObject.link.nLinkID ) == pSession->unknownLinkIDs.end() ? 1 : 0;
			rRecord.script_id = rObject.nScriptID;
			rRecord.hp = rObject.fHP;
			rRecord.frame_index = rObject.nFrameIndex;
			rRecord.link_with = rObject.link.nLinkWith;
		}
	return nCapacity >= nTotal;
}

bool SetSessionObjectScriptID( SEditorSession *pSession, int nLinkID, int nScriptID, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession != 0 ) pSession->szMessage = "no map is open";
		return false;
	}
	if ( nScriptID < -1 || nScriptID > 32000 )
	{
		pSession->szMessage = "a script ID is -1 (none) or 0..32000";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	const SMapObjectInfo *pObject = FindSnapshotObject( *pSession, nLinkID );
	if ( nLinkID == 0 || pObject == 0 )
	{
		pSession->szMessage = nLinkID == 0 ? "an object with link ID 0 has no link ID to name it by, so its script ID is kept as it is"
		                                   : "no object with that link ID";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	// An object the database does not know is written back exactly as it was
	// read (the preservation invariant), so no edit reaches it - its script ID
	// neither, as PlaceObjectInSession and DeleteObjectFromSession refuse.
	if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nLinkID ) != pSession->unknownLinkIDs.end() )
	{
		pSession->szMessage = "the object database does not know this object's type; it is kept as it is";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( RefuseSharedLinkID( pSession, nLinkID, pbRefused ) )
		return false;
	// Both copies together; the snapshot first, and undone if the working copy
	// (which holds the same link IDs) somehow will not take it.
	const int nBefore = pObject->nScriptID;
	if ( !NMapRecords::SetObjectScriptID( &pSession->snapshot, nLinkID, nScriptID ) )
		return false;
	if ( !NMapRecords::SetObjectScriptID( &pSession->working, nLinkID, nScriptID ) )
	{
		NMapRecords::SetObjectScriptID( &pSession->snapshot, nLinkID, nBefore );
		return false;
	}
	// An object whose script ID a hidden group names is hidden now, one that
	// left it is shown.
	if ( !pSession->hiddenScriptIDs.empty() )
		ApplyHiddenMarks( pSession );
	pSession->szMessage = GroupHoldWarning( *pSession, nLinkID, -1 );
	return true;
}

bool ReadSessionSounds( SEditorSession *pSession, BkEditorSoundRecord *pOut, int nCapacity, int *pnCount )
{
	const std::vector<SMapSoundInfo> &rSounds = pSession->snapshot.sounds.sounds;
	const int nTotal = int( rSounds.size() );
	*pnCount = nTotal;
	const int nWrite = nTotal < nCapacity ? nTotal : nCapacity;
	for ( int i = 0; i < nWrite; ++i )
	{
		const SMapSoundInfo &rSound = rSounds[i];
		BkEditorSoundRecord &rRecord = pOut[i];
		memset( &rRecord, 0, sizeof rRecord );
		strncpy( rRecord.name, rSound.szName.c_str(), sizeof rRecord.name - 1 );
		rRecord.x = rSound.vPos.x;
		rRecord.y = rSound.vPos.y;
		rRecord.z = rSound.vPos.z;
		rRecord.repeat_ms = int( rSound.timeRepeat );
		rRecord.repeat_random_ms = int( rSound.timeRepeatRandom );
		rRecord.mute_in_combat = rSound.bMuteDuringCombat ? 1 : 0;
		rRecord.min_radius = rSound.nMinRadius;
		rRecord.max_radius = rSound.nMaxRadius;
	}
	return nCapacity >= nTotal;
}

namespace {
// record->name known and valid, on the map, and its own fields sane - the
// rules BkEditorAddSound/BkEditorSetSound document. false leaves
// pSession->szMessage set and pOut untouched; every caller of this treats
// that as a refusal (the caller-bug checks - null record, bad index,
// unterminated or non-finite fields - already ran in bridge.cpp before
// either of AddSoundToSession/SetSoundInSession got here).
bool ValidateSoundRecord( SEditorSession *pSession, const BkEditorSoundRecord &rRecord, SMapSoundInfo *pOut )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
	{
		pSession->szMessage = "the object database is not there";
		return false;
	}
	const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( rRecord.name );
	if ( pDesc == 0 || pDesc->eGameType != SGVOGT_SOUND )
	{
		pSession->szMessage = std::string( "\"" ) + rRecord.name + "\" is not a known sound";
		return false;
	}
	int nTileX = 0, nTileY = 0;
	if ( !WorldToTile( pSession, rRecord.x, rRecord.y, &nTileX, &nTileY ) )
		return false; // szMessage already set by WorldToTile
	if ( rRecord.repeat_ms < 0 || rRecord.repeat_random_ms < 0 ||
	     rRecord.min_radius < 0 || rRecord.max_radius < 0 || rRecord.min_radius > rRecord.max_radius )
	{
		pSession->szMessage = "a sound's times and radii must not be negative, and its minimum radius must not be above its maximum";
		return false;
	}
	pOut->szName = rRecord.name;
	pOut->vPos = CVec3( rRecord.x, rRecord.y, rRecord.z );
	pOut->timeRepeat = NTimer::STime( rRecord.repeat_ms );
	pOut->timeRepeatRandom = NTimer::STime( rRecord.repeat_random_ms );
	pOut->bMuteDuringCombat = rRecord.mute_in_combat != 0;
	pOut->nMinRadius = rRecord.min_radius;
	pOut->nMaxRadius = rRecord.max_radius;
	return true;
}
}

bool AddSoundToSession( SEditorSession *pSession, int nIndex, const BkEditorSoundRecord &rRecord, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	SMapSoundInfo info;
	if ( !ValidateSoundRecord( pSession, rRecord, &info ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	std::vector<SMapSoundInfo> &rSnap = pSession->snapshot.sounds.sounds;
	std::vector<SMapSoundInfo> &rWork = pSession->working.sounds.sounds;
	const int nCount = int( rSnap.size() );
	const int nAt = nIndex < 0 ? nCount : nIndex;
	if ( nAt < 0 || nAt > nCount || nAt > int( rWork.size() ) )
	{
		pSession->szMessage = "that index is out of range";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	rSnap.insert( rSnap.begin() + nAt, info );
	rWork.insert( rWork.begin() + nAt, info );
	return true;
}

bool SetSoundInSession( SEditorSession *pSession, int nIndex, const BkEditorSoundRecord &rRecord, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	SMapSoundInfo info;
	if ( !ValidateSoundRecord( pSession, rRecord, &info ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	std::vector<SMapSoundInfo> &rSnap = pSession->snapshot.sounds.sounds;
	std::vector<SMapSoundInfo> &rWork = pSession->working.sounds.sounds;
	if ( nIndex < 0 || nIndex >= int( rSnap.size() ) || nIndex >= int( rWork.size() ) )
	{
		pSession->szMessage = "no sound at that index";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	rSnap[nIndex] = info;
	rWork[nIndex] = info;
	return true;
}

bool DeleteSoundFromSession( SEditorSession *pSession, int nIndex, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	std::vector<SMapSoundInfo> &rSnap = pSession->snapshot.sounds.sounds;
	std::vector<SMapSoundInfo> &rWork = pSession->working.sounds.sounds;
	if ( nIndex < 0 || nIndex >= int( rSnap.size() ) || nIndex >= int( rWork.size() ) )
	{
		pSession->szMessage = "no sound at that index";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	rSnap.erase( rSnap.begin() + nIndex );
	rWork.erase( rWork.begin() + nIndex );
	return true;
}

bool AddObjectToSession( SEditorSession *pSession, const NMapOverlay::SAddObject &rAdd, int *pnLinkID )
{
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pObjectsDB == 0 || pAIEditor == 0 )
	{
		pSession->szMessage = "the engine is not there";
		return false;
	}
	const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( rAdd.szName.c_str() );
	if ( pDesc == 0 )
	{
		pSession->szMessage = "the object database does not know \"" + rAdd.szName + "\"";
		return false;
	}
	if ( const char *pszWhy = WhyNotAMapObject( pDesc->eGameType ) )
	{
		pSession->szMessage = "\"" + rAdd.szName + "\" " + pszWhy;
		return false;
	}
	if ( const char *pszWhy = WhyNotPlacedByPalette( pDesc->eGameType ) )
	{
		pSession->szMessage = "\"" + rAdd.szName + "\" " + pszWhy;
		return false;
	}
	const std::string szAloneWhy = WhyNotPlacedAlone( pObjectsDB, *pDesc );
	if ( !szAloneWhy.empty() )
	{
		pSession->szMessage = "\"" + rAdd.szName + "\" " + szAloneWhy;
		return false;
	}

	// Never below the floor: an ID a deleted object held may be wanted back by
	// its restore, and NextLinkID alone would hand the highest one out again.
	// Both copies get the ID explicitly, so they cannot disagree about it.
	NMapOverlay::SAddObject add = rAdd;
	// Today's values, set explicitly so the intent is visible: a palette add is
	// whole, has no script ID and takes its frame index from the packing below.
	add.nFrameIndex = 0;
	add.fHP = 1.0f;
	add.nScriptID = -1;
	// Linked with nothing, as the MFC editor wrote an object it had not linked: the
	// game lands a reinforcement only when this is 0 (see SAddObject::nLinkWith),
	// so a unit placed for a reinforcement group needs it.
	add.nLinkWith = 0;
	add.nLinkID = Max( NMapOverlay::NextLinkID( pSession->snapshot ), pSession->nLinkIDFloor );
	int nLinkID = -1;
	if ( !NMapOverlay::AddObject( &pSession->snapshot, add, &nLinkID ) )
	{
		pSession->szMessage = "the map would not take \"" + rAdd.szName + "\"";
		return false;
	}
	// The overlay leaves the frame index at 0 because packing needs the object
	// database. The bridge has it, so the one object it just added is packed
	// here - a fence or a span otherwise goes out with an index that means
	// something else.
	if ( SMapObjectInfo *pAdded = FindIn( &pSession->snapshot, nLinkID ) )
		CMapInfo::PackFrameIndex( pObjectsDB, pAdded );
	NMapOverlay::AddObject( &pSession->working, add, 0 );

	// The engine object comes from the working record, as OpenMapIntoSession
	// and RestoreObjectInSession build theirs: its frame index is a segment,
	// while the snapshot's was just packed into a type. Handing the engine the
	// packed one had it index a fence's segments with 65537 - FENCE_TYPE_NORMAL
	// | FENCE_DIRECTION_0 - and read an origin and a passability from far past
	// their end, and built a bridge span and an entrenchment from segments 1
	// and 2 where segment 0 was meant.
	const SMapObjectInfo *pWorkingObject = FindIn( &pSession->working, nLinkID );
	IRefCount *pAIObject = pWorkingObject != 0 ? PlaceOneObject( *pWorkingObject, pDesc, pAIEditor ) : 0;
	if ( pAIObject == 0 )
	{
		// The engine would not have it - outside the map, most often - so the
		// snapshot must not keep it either, or the editor would save an object
		// it never showed.
		std::string szIgnored;
		NMapOverlay::DeleteObject( &pSession->snapshot, nLinkID, &szIgnored );
		NMapOverlay::DeleteObject( &pSession->working, nLinkID, &szIgnored );
		pSession->szMessage = "the engine would not place \"" + rAdd.szName + "\" there";
		return false;
	}
	pSession->byLinkID[nLinkID] = pAIObject;
	pSession->nLinkIDFloor = nLinkID + 1;
	UpdateSessionWorld( pSession );
	if ( pnLinkID )
		*pnLinkID = nLinkID;
	return true;
}

bool PlaceObjectInSession( SEditorSession *pSession, int nLinkID, const CVec3 &vPosIn, int nDir, int nPlayer, bool *pbRefused )
{
	if ( pbRefused )
		*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	// The preservation invariant: an object the database does not know is
	// written back exactly as it was read, so no edit may reach it.
	if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nLinkID ) != pSession->unknownLinkIDs.end() )
	{
		pSession->szMessage = "the object database does not know this object's type; it is kept as it is";
		if ( pbRefused ) *pbRefused = true;
		return false;
	}
	if ( RefuseSharedLinkID( pSession, nLinkID, pbRefused ) )
		return false;
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	SMapObjectInfo *pObject = FindIn( &pSession->snapshot, nLinkID );
	if ( pAIEditor == 0 || pObject == 0 )
	{
		pSession->szMessage = "no object with that link ID";
		return false;
	}
	std::unordered_map<int, CPtr<IRefCount> >::const_iterator itEngine = pSession->byLinkID.find( nLinkID );
	if ( itEngine == pSession->byLinkID.end() )
	{
		// The map holds it but the engine never did - an object outside the map,
		// or one whose stats are missing. Moving it in the file alone would put
		// the two out of step, so it is refused rather than half-done.
		pSession->szMessage = "the engine does not hold that object";
		if ( pbRefused ) *pbRefused = true;
		return false;
	}

	const SMapObjectInfo before = *pObject;
	CVec3 vPos = vPosIn;
	IRefCount *pAIObject = itEngine->second;
	const SEngineObjectState engineBefore = ReadEngine( pAIEditor, pAIObject );

	NMapOverlay::SMoveObject move;
	move.nLinkID = nLinkID;
	move.vPos = vPos;
	move.nDir = nDir;
	move.nPlayer = nPlayer;
	NMapOverlay::MoveObject( &pSession->snapshot, move );
	NMapOverlay::MoveObject( &pSession->working, move );

	// Each field is asked for only when it is actually changing, and each is
	// read back only when it was asked for: an object that was never turned must
	// not be refused because its kind reports no direction.
	const bool bMoving = before.vPos.x != vPos.x || before.vPos.y != vPos.y;
	const bool bTurning = before.nDir != nDir;
	const bool bReowning = before.nPlayer != nPlayer;
	if ( bMoving )
		pAIEditor->MoveObject( pAIObject, ToEngineCoord( vPos.x ), ToEngineCoord( vPos.y ) );
	if ( bTurning )
		pAIEditor->TurnObject( pAIObject, WORD( nDir ) );
	if ( bReowning )
		pAIEditor->SetPlayer( pAIObject, nPlayer );

	// The engine silently does nothing when it will not have what it was asked
	// for - CAIUnit::CanSetNewCoord and IsRectInsideOfMap for a move or a turn,
	// an empty SetPlayerForEditor for an object nobody can own - so the only way
	// to know is to look at all three afterwards.
	if ( ( bMoving && !EngineIsAt( pAIEditor, pAIObject, vPos ) ) ||
	     ( bTurning && !EngineFacesDir( pAIEditor, pAIObject, nDir ) ) ||
	     ( bReowning && !EngineBelongsTo( pAIEditor, pAIObject, nPlayer ) ) )
	{
		// The engine goes back as well as the map. The three changes are applied
		// one after another, so a move that took followed by a turn that did not
		// would otherwise leave the engine showing the object somewhere the file
		// never records - which is the same disagreement this whole path exists
		// to prevent, only the other way round.
		if ( bMoving )
			pAIEditor->MoveObject( pAIObject, ToEngineCoord( engineBefore.vCenter.x ), ToEngineCoord( engineBefore.vCenter.y ) );
		if ( bTurning )
			pAIEditor->TurnObject( pAIObject, engineBefore.wDir );
		if ( bReowning && engineBefore.nPlayer >= 0 )
			pAIEditor->SetPlayer( pAIObject, engineBefore.nPlayer );

		NMapOverlay::SMoveObject back;
		back.nLinkID = nLinkID;
		back.vPos = before.vPos;
		back.nDir = before.nDir;
		back.nPlayer = before.nPlayer;
		NMapOverlay::MoveObject( &pSession->snapshot, back );
		NMapOverlay::MoveObject( &pSession->working, back );

		// A rollback that did not take is the one outcome worse than the refusal
		// it was undoing: the map and the engine now disagree and nothing here
		// can mend it, so it is reported as a failure rather than an ordinary no.
		const SEngineObjectState engineNow = ReadEngine( pAIEditor, pAIObject );
		if ( ToEngineCoord( engineNow.vCenter.x ) != ToEngineCoord( engineBefore.vCenter.x ) ||
		     ToEngineCoord( engineNow.vCenter.y ) != ToEngineCoord( engineBefore.vCenter.y ) ||
		     engineNow.wDir != engineBefore.wDir || engineNow.nPlayer != engineBefore.nPlayer )
		{
			pSession->szMessage = "the engine would not take that placement and could not be put back; reopen the map";
			return false;
		}
		pSession->szMessage = "the engine would not take that placement for the object";
		if ( pbRefused ) *pbRefused = true;
		return false;
	}
	UpdateSessionWorld( pSession );
	return true;
}

bool DeleteObjectFromSession( SEditorSession *pSession, int nLinkID, bool *pbRefused )
{
	if ( pbRefused )
		*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	// The preservation invariant: an object the database does not know is
	// written back exactly as it was read, so no edit may reach it.
	if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nLinkID ) != pSession->unknownLinkIDs.end() )
	{
		pSession->szMessage = "the object database does not know this object's type; it is kept as it is";
		if ( pbRefused ) *pbRefused = true;
		return false;
	}
	// Before anything else: with the ID shared, "not in byLinkID" below would
	// not mean "the engine holds nothing", and a found engine object might be
	// another record's.
	if ( RefuseSharedLinkID( pSession, nLinkID, pbRefused ) )
		return false;
	// M3 (D-27): the host's passengers go with it - the records whose
	// nLinkWith names this object are deleted first (each through this very
	// path, so their own references and passengers cascade the same way), and
	// the host's restore brings them back. The M2 refusals - a bridge span, a
	// trench piece - still refuse below, passengers or no passengers; the
	// overlay's own passenger refusal never fires, because by the time the
	// overlay sees the host, its passengers are gone.
	std::vector<int> passengers;
	{
		const std::vector<SMapObjectInfo> *lists[2] = { &pSession->snapshot.objects, &pSession->snapshot.scenarioObjects };
		for ( int nList = 0; nList < 2; ++nList )
			for ( size_t i = 0; i < lists[nList]->size(); ++i )
			{
				const SMapObjectInfo &rObject = ( *lists[nList] )[i];
				if ( rObject.link.nLinkID != nLinkID && rObject.link.nLinkWith == nLinkID )
					passengers.push_back( rObject.link.nLinkID );
			}
	}
	SEditorSession::STombstone tombstone;
	for ( size_t i = 0; i < passengers.size(); ++i )
	{
		bool bPassengerRefused = false;
		if ( !DeleteObjectFromSession( pSession, passengers[i], &bPassengerRefused ) )
		{
			// A passenger that cannot go (itself a referred span, say)
			// refuses the whole delete; the ones already taken are put back.
			for ( size_t j = tombstone.passengers.size(); j > 0; --j )
			{
				bool bIgnored = false;
				RestoreObjectInSession( pSession, tombstone.passengers[j - 1].snapshot.object.link.nLinkID, &bIgnored );
			}
			pSession->szMessage = NStr::Format( "the passenger of object %d cannot be deleted: %s", nLinkID, pSession->szMessage.c_str() );
			if ( pbRefused ) *pbRefused = true;
			return false;
		}
		tombstone.passengers.push_back( pSession->tombstones[passengers[i]] );
		pSession->tombstones.erase( passengers[i] );
	}
	// The map decides first: a bridge span, a trench piece or a vehicle with a
	// passenger means no, and the engine is never asked. Anything else that
	// names the object - start commands, reserve positions - is edited by the
	// map's cascade, the same on both copies because both lists are equal.
	//
	// Both records are kept, with their lists, places and cascades, for a
	// restore.
	std::string szRefusal;
	if ( !NMapOverlay::DeleteObject( &pSession->snapshot, nLinkID, &szRefusal, &tombstone.snapshot ) )
	{
		pSession->szMessage = szRefusal;
		if ( pbRefused ) *pbRefused = true;
		return false;
	}
	std::string szIgnored;
	NMapOverlay::DeleteObject( &pSession->working, nLinkID, &szIgnored, &tombstone.working );

	// With the ID the object's own, not being in byLinkID means the engine made
	// nothing for it: outside the map (never handed over), a span no bridge
	// names (set aside), or no stats, a missing player or objects switched off
	// (CAILogic::AddObject creates nothing and answers 0). None of those is
	// drawn, so the file alone is all there is to delete.
	std::unordered_map<int, CPtr<IRefCount> >::iterator itEngine = pSession->byLinkID.find( nLinkID );
	if ( itEngine != pSession->byLinkID.end() )
	{
		tombstone.bPlaced = true;
		if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
		{
			IRefCount *pAIObject = itEngine->second;
			// The ID is freed first, while the object - the formation, for a
			// squad - still holds it. A deleted object is not destroyed on the
			// spot (the updater and the graveyard keep it), and its link would
			// stay registered until the game's AI segment cleared it, which the
			// editor never runs; the restore below then registers the object
			// again under the same ID, "Repeated link" in CLinkObject::SetLink.
			// The MFC editor never meets this: its undo re-adds with link ID 0
			// and it hands out every link afresh when it saves.
			pAIEditor->ReleaseLink( pAIObject );
			if ( pAIEditor->IsFormation( pAIObject ) )
			{
				// A squad is its soldiers. CAIEditor::DeleteObject knows a unit and a
				// static object but not a formation, and asserts "Unknown object" on
				// one (AIEditorInternal.cpp:128-135). The MFC editor never hands it a
				// formation: a picked soldier selects every soldier of his squad
				// (ObjectPlacerState.cpp:1549-1570), and Delete removes each of those
				// with DeleteObject (ObjectPlacerState.cpp:710-719,
				// TemplateEditorFrame1.cpp:2334). The last soldier to go takes the
				// formation with him (CSoldier::PrepareToDelete, Soldier.cpp:688-692).
				//
				// The soldiers are copied out first: GetUnitsInFormation answers in a
				// temporary buffer, and each DeleteObject shrinks the formation.
				IRefCount **ppUnits = 0;
				int nUnits = 0;
				pAIEditor->GetUnitsInFormation( pAIObject, &ppUnits, &nUnits );
				std::vector< CPtr<IRefCount> > soldiers( ppUnits, ppUnits + nUnits );
				for ( size_t i = 0; i < soldiers.size(); ++i )
					pAIEditor->DeleteObject( soldiers[i] );
			}
			else
				pAIEditor->DeleteObject( pAIObject );
		}
		pSession->byLinkID.erase( itEngine );
	}
	pSession->tombstones[nLinkID] = tombstone;
	pSession->nLinkIDFloor = Max( pSession->nLinkIDFloor, nLinkID + 1 );
	UpdateSessionWorld( pSession );
	// What else changed, for the status bar; empty when only the object went.
	NMapOverlay::DescribeCascade( tombstone.snapshot.cascade, &pSession->szMessage );
	if ( !tombstone.passengers.empty() )
	{
		std::string szPassengers = NStr::Format( "%d passenger%s deleted with the host", int( tombstone.passengers.size() ), tombstone.passengers.size() == 1 ? "" : "s" );
		if ( pSession->szMessage.empty() )
			pSession->szMessage = szPassengers;
		else
			pSession->szMessage += "; " + szPassengers;
	}
	return true;
}

bool RestoreObjectInSession( SEditorSession *pSession, int nLinkID, bool *pbRefused )
{
	if ( pbRefused )
		*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	std::unordered_map<int, SEditorSession::STombstone>::iterator it = pSession->tombstones.find( nLinkID );
	if ( it == pSession->tombstones.end() )
	{
		pSession->szMessage = "no deleted object has that link ID";
		if ( pbRefused ) *pbRefused = true;
		return false;
	}
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pObjectsDB == 0 || pAIEditor == 0 )
	{
		pSession->szMessage = "the engine is not there";
		return false;
	}
	if ( !NMapOverlay::RestoreObject( &pSession->snapshot, it->second.snapshot ) )
	{
		pSession->szMessage = "the link ID is in use again";
		if ( pbRefused ) *pbRefused = true;
		return false;
	}
	NMapOverlay::RestoreObject( &pSession->working, it->second.working );
	// The engine object comes from the working record, whose frame index is
	// unpacked, the way OpenMapIntoSession builds it; the snapshot keeps its
	// packed index for the save.
	if ( it->second.bPlaced )
	{
		const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( it->second.working.object.szName.c_str() );
		IRefCount *pAIObject = pDesc != 0 ? PlaceOneObject( it->second.working.object, pDesc, pAIEditor ) : 0;
		if ( pAIObject == 0 )
		{
			std::string szIgnored;
			NMapOverlay::DeleteObject( &pSession->snapshot, nLinkID, &szIgnored );
			NMapOverlay::DeleteObject( &pSession->working, nLinkID, &szIgnored );
			pSession->szMessage = "the engine would not take the object back";
			if ( pbRefused ) *pbRefused = true;
			return false;
		}
		pSession->byLinkID[nLinkID] = pAIObject;
	}
	// The host's passengers come back last (M3, D-27), each from its own
	// tombstone, last deleted first - the delete order put each nested host
	// before its own passengers, so the reverse walk restores the deepest
	// passengers before their hosts.
	bool bAllPassengers = true;
	for ( size_t i = it->second.passengers.size(); i > 0; --i )
	{
		const int nPassengerID = it->second.passengers[i - 1].snapshot.object.link.nLinkID;
		SEditorSession::STombstone passenger = it->second.passengers[i - 1];
		pSession->tombstones[nPassengerID] = passenger;
		bool bPassengerRefused = false;
		if ( !RestoreObjectInSession( pSession, nPassengerID, &bPassengerRefused ) )
			bAllPassengers = false;
	}
	it = pSession->tombstones.find( nLinkID );
	if ( it != pSession->tombstones.end() )
		pSession->tombstones.erase( it );
	UpdateSessionWorld( pSession );
	if ( !bAllPassengers )
	{
		pSession->szMessage = "a passenger would not come back with the host";
		if ( pbRefused ) *pbRefused = true;
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// The batch move (M3, D-25). One call, one edit of the log: every member's
// whole record before and after, put back raw by Revert/Reapply the way the
// Update Map composite puts its fit pass's moves back - so one drag gesture
// (many calls, many tokens merged by the core) undoes as one step, and an
// undo of it restores every member exactly, engine included.
// ---------------------------------------------------------------------------

namespace {

bool PutMembersBack( SEditorSession *pSession, const std::vector<SMoveObjectsEdit::SMovedMember> &rMembers )
{
	for ( size_t i = 0; i < rMembers.size(); ++i )
	{
		bool bRefused = false;
		if ( !PlaceObjectInSession( pSession, rMembers[i].nLinkID, rMembers[i].before.vPos, rMembers[i].before.nDir, rMembers[i].before.nPlayer, &bRefused ) )
			return false;
	}
	return true;
}

}

bool SMoveObjectsEdit::Revert( SEditorSession *pSession )
{
	return PutMembersBack( pSession, members );
}

bool SMoveObjectsEdit::Reapply( SEditorSession *pSession )
{
	for ( size_t i = 0; i < members.size(); ++i )
	{
		bool bRefused = false;
		if ( !PlaceObjectInSession( pSession, members[i].nLinkID, members[i].after.vPos, members[i].after.nDir, members[i].after.nPlayer, &bRefused ) )
			return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// The properties' fields, links and the flag swap (M3, D-26/D-27). Every one
// of them changes ONE object's record whole - both copies, the engine
// re-placed - and logs SObjectFieldsEdit, so one deactivate commit is one
// undo step and a swap's name change undoes exactly.
// ---------------------------------------------------------------------------

// The engine object re-placed from `rRecord`, then the record written over
// the object's record in both copies (defined below).
bool PutObjectRecordBack( SEditorSession *pSession, const SMapObjectInfo &rRecord );

bool SObjectFieldsEdit::Revert( SEditorSession *pSession )
{
	return PutObjectRecordBack( pSession, before );
}

bool SObjectFieldsEdit::Reapply( SEditorSession *pSession )
{
	return PutObjectRecordBack( pSession, after );
}

// Re-places the engine object from `rRecord` - its position, direction and
// owner - and then writes the record over the object's record in both copies.
// The engine goes first: PlaceObjectInSession compares the snapshot's current
// record with what it is asked for to decide what to move, turn or re-own, so
// a record written before it would leave the engine where it was (an angle
// edit that never turned the object, an undo that never turned it back). A
// placement the engine refuses leaves the records as they were.
bool PutObjectRecordBack( SEditorSession *pSession, const SMapObjectInfo &rRecord )
{
	if ( FindIn( &pSession->snapshot, rRecord.link.nLinkID ) == 0 || FindIn( &pSession->working, rRecord.link.nLinkID ) == 0 )
	{
		pSession->szMessage = "no object with that link ID";
		return false;
	}
	const SMapObjectInfo *pCurrent = FindIn( &pSession->snapshot, rRecord.link.nLinkID );
	// A flag's owner is its type - the properties' swap renames it
	// Flag_<party> - and the engine holds a flag unowned, so the engine is
	// never asked to re-own one (it would refuse, and the swap with it).
	int nEnginePlayer = rRecord.nPlayer;
	{
		IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
		const SGDBObjectDesc *pDesc = pObjectsDB != 0 ? pObjectsDB->GetDesc( pCurrent->szName.c_str() ) : 0;
		if ( pDesc != 0 && pDesc->eGameType == SGVOGT_FLAG )
			nEnginePlayer = pCurrent->nPlayer;
	}
	bool bRefused = false;
	if ( !PlaceObjectInSession( pSession, rRecord.link.nLinkID, rRecord.vPos, rRecord.nDir, nEnginePlayer, &bRefused ) )
		return false;
	SMapObjectInfo *pSnapshot = FindIn( &pSession->snapshot, rRecord.link.nLinkID );
	SMapObjectInfo *pWorking = FindIn( &pSession->working, rRecord.link.nLinkID );
	if ( pSnapshot == 0 || pWorking == 0 )
	{
		pSession->szMessage = "no object with that link ID";
		return false;
	}
	*pSnapshot = rRecord;
	*pWorking = rRecord;
	return true;
}

// The party table (partys.xml), read once per session - the same serialiser
// the game's own unit creation reads it with (UnitCreation.cpp:87-91).
bool ReadPartyTable( SEditorSession *pSession )
{
	if ( pSession->bPartyTableRead )
		return true;
	pSession->bPartyTableRead = true;
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 )
		return false;
	CPtr<IDataStream> pStream = pStorage->OpenStream( "partys.xml", STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
	tree.Add( "PartyInfo", &pSession->partyTable );
	return true;
}

// The flag prefix + the general side of player `nPlayer`'s party, lowercased,
// exactly the MFC properties' swap (SEditorMApObject.cpp:426-447): the map's
// unit creation names the player's party, partys.xml names the party's
// general side; anything unknown answers "neutral", as the MFC does.
std::string FlagPartyName( SEditorSession *pSession, int nPlayer )
{
	std::string szPartyName;
	const SUnitCreationInfo &rUnitCreation = pSession->snapshot.unitCreation;
	if ( nPlayer >= 0 && nPlayer < int( rUnitCreation.units.size() ) )
		szPartyName = rUnitCreation.units[nPlayer].szPartyName;
	std::string szGeneral;
	ReadPartyTable( pSession );
	for ( size_t i = 0; i < pSession->partyTable.size(); ++i )
		if ( pSession->partyTable[i].szPartyName == szPartyName )
		{
			szGeneral = pSession->partyTable[i].szGeneralPartyName;
			break;
		}
	if ( szGeneral.empty() )
		szGeneral = "neutral";
	NStr::ToLower( szGeneral );
	return szGeneral;
}

bool SetObjectFieldsInSession( SEditorSession *pSession, int nLinkID, const BkEditorObjectFieldsEdit *pEdit, bool *pbRefused, int *pnToken )
{
	*pbRefused = false;
	*pnToken = -1;
	if ( pSession == 0 )
	{
		*pbRefused = true;
		return false;
	}
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		*pbRefused = true;
		return false;
	}
	if ( pEdit == 0 || pEdit->mask == 0 )
	{
		pSession->szMessage = "no field to change";
		return false;
	}
	if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nLinkID ) != pSession->unknownLinkIDs.end() )
	{
		pSession->szMessage = "the object database does not know this object's type; it is kept as it is";
		*pbRefused = true;
		return false;
	}
	if ( RefuseSharedLinkID( pSession, nLinkID, pbRefused ) )
		return false;
	const SMapObjectInfo *pRecord = FindSnapshotObject( *pSession, nLinkID );
	if ( pRecord == 0 )
	{
		pSession->szMessage = "no object with that link ID";
		*pbRefused = true;
		return false;
	}
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
	{
		pSession->szMessage = "the engine has no object database";
		return false;
	}
	const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( pRecord->szName.c_str() );
	// The record before the edit is copied NOW: pRecord points into the
	// snapshot, and PutObjectRecordBack below overwrites it in place - an
	// undo record taken after that would hold the edit itself.
	const SMapObjectInfo recordBefore = *pRecord;
	SMapObjectInfo after = recordBefore;
	if ( pEdit->mask & 1 )
	{
		if ( pEdit->player < 0 || pEdit->player >= int( pSession->snapshot.diplomacies.size() ) )
		{
			pSession->szMessage = NStr::Format( "%d is no player: the map holds %d", pEdit->player, int( pSession->snapshot.diplomacies.size() ) );
			*pbRefused = true;
			return false;
		}
		after.nPlayer = pEdit->player;
		// The flag swap: a re-owned flag becomes Flag_<the party's general
		// side>, and the engine re-places it under the new type. A flag type
		// the database does not know refuses - the MFC's AddObjectByAI would
		// have failed the same way.
		if ( pDesc != 0 && pDesc->eGameType == SGVOGT_FLAG )
		{
			std::string szFlagName = "Flag_" + FlagPartyName( pSession, pEdit->player );
			if ( szFlagName != pRecord->szName )
			{
				if ( pObjectsDB->GetDesc( szFlagName.c_str() ) == 0 )
				{
					pSession->szMessage = "the object database does not know \"" + szFlagName + "\"";
					*pbRefused = true;
					return false;
				}
				after.szName = szFlagName;
			}
		}
	}
	if ( pEdit->mask & 2 )
	{
		if ( !std::isfinite( pEdit->hp ) )
		{
			pSession->szMessage = "the health is not a number";
			*pbRefused = true;
			return false;
		}
		after.fHP = pEdit->hp;
	}
	if ( pEdit->mask & 4 )
	{
		if ( !std::isfinite( pEdit->angle ) )
		{
			pSession->szMessage = "the angle is not a number";
			*pbRefused = true;
			return false;
		}
		// The MFC properties dialog's own turn (SEditorMApObject.cpp:384-386).
		after.nDir = int( ( pEdit->angle * 65536.0f ) / 360.0f + 0.5f );
	}
	if ( pEdit->mask & 8 )
	{
		if ( pEdit->formation < 0 )
		{
			pSession->szMessage = "a formation index is 0 or greater";
			*pbRefused = true;
			return false;
		}
		// The squad record's frame index is the formation; any other kind's
		// is its segment or variant, which this edit must never touch.
		if ( pDesc == 0 || pDesc->eGameType != SGVOGT_SQUAD )
		{
			pSession->szMessage = "only a squad carries a formation";
			*pbRefused = true;
			return false;
		}
		after.nFrameIndex = pEdit->formation;
	}
	if ( after.szName == pRecord->szName && after.vPos.x == pRecord->vPos.x && after.vPos.y == pRecord->vPos.y &&
	     after.nDir == pRecord->nDir && after.nPlayer == pRecord->nPlayer && after.nScriptID == pRecord->nScriptID &&
	     after.fHP == pRecord->fHP && after.nFrameIndex == pRecord->nFrameIndex && after.link.nLinkWith == pRecord->link.nLinkWith )
	{
		// An edit that changes nothing records nothing; the core has usually
		// answered this itself.
		return true;
	}
	if ( !PutObjectRecordBack( pSession, after ) )
	{
		// The place refused: the records are as they were (PutObjectRecordBack
		// writes both copies only after its own lookups; the place path leaves
		// them untouched on a refusal), so nothing to roll back here.
		*pbRefused = true;
		return false;
	}
	SObjectFieldsEdit *pEditRecord = new SObjectFieldsEdit();
	pEditRecord->nLinkID = nLinkID;
	pEditRecord->before = recordBefore;
	pEditRecord->after = after;
	*pnToken = LogEdit( pSession, pEditRecord );
	return true;
}

// CheckForInserting (ObjectPlacerState.cpp:1325-1424) for ONE passenger and
// ONE host, naming the rule that says no.
bool CanLinkInSession( SEditorSession *pSession, int nSource, int nTarget, int *pnType, bool *pbRefused )
{
	*pbRefused = false;
	*pnType = 0;
	if ( pSession == 0 )
	{
		*pbRefused = true;
		return false;
	}
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		*pbRefused = true;
		return false;
	}
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
	{
		pSession->szMessage = "the engine has no object database";
		return false;
	}
	const SMapObjectInfo *pSource = FindSnapshotObject( *pSession, nSource );
	const SMapObjectInfo *pTarget = FindSnapshotObject( *pSession, nTarget );
	if ( pSource == 0 || pTarget == 0 )
	{
		pSession->szMessage = "no object with that link ID";
		*pbRefused = true;
		return false;
	}
	const SGDBObjectDesc *pSourceDesc = pObjectsDB->GetDesc( pSource->szName.c_str() );
	const SGDBObjectDesc *pTargetDesc = pObjectsDB->GetDesc( pTarget->szName.c_str() );
	if ( pSourceDesc == 0 || pTargetDesc == 0 )
	{
		pSession->szMessage = "the object database does not know one of the two types";
		*pbRefused = true;
		return false;
	}
	// The garrison rules first, the MFC's own order: the passenger must be
	// infantry, and the host's kind answers. The MFC judges the dragged
	// SOLDIERS (its selection expands a picked squad to them, and a soldier's
	// stats are SUnitBaseRPGStats); the map holds the squad, so a squad
	// record is garrison-capable here in its own right - the soldiers its
	// formations name are infantry by construction. The MFC compared the
	// dragged group's size against the host's total slots and never asked
	// how many stood inside already - the evident intent is a host that
	// takes as many as it has room for, so the occupancy (the records whose
	// nLinkWith names the host) is what the room is measured against here.
	int nOccupants = 0;
	{
		const std::vector<SMapObjectInfo> *lists[2] = { &pSession->snapshot.objects, &pSession->snapshot.scenarioObjects };
		for ( int nList = 0; nList < 2; ++nList )
			for ( size_t i = 0; i < lists[nList]->size(); ++i )
				if ( ( *lists[nList] )[i].link.nLinkWith == nTarget && ( *lists[nList] )[i].link.nLinkID != nSource )
					++nOccupants;
	}
	const SUnitBaseRPGStats *pSourceStats = dynamic_cast<const SUnitBaseRPGStats*>( pObjectsDB->GetRPGStats( pSourceDesc ) );
	const bool bSquadPassenger = pSourceDesc->eGameType == SGVOGT_SQUAD;
	std::string szWhy;
	bool bCan = false;
	if ( pSourceStats == 0 && !bSquadPassenger )
		szWhy = "\"" + pSource->szName + "\" is not a unit that can be garrisoned";
	else if ( pSourceStats != 0 && !pSourceStats->IsInfantry() )
		szWhy = "only infantry garrisons something: \"" + pSource->szName + "\" is not infantry";
	else if ( pTargetDesc->eGameType == SGVOGT_BUILDING )
	{
		const SBuildingRPGStats *pBuilding = dynamic_cast<const SBuildingRPGStats*>( pObjectsDB->GetRPGStats( pTargetDesc ) );
		if ( pBuilding == 0 )
			szWhy = "the database does not know \"" + pTarget->szName + "\" as a building";
		else if ( ( pBuilding->slots.size() + pBuilding->nRestSlots + pBuilding->nMedicalSlots ) < nOccupants + 1 )
			szWhy = "\"" + pTarget->szName + "\" has no free slot: a garrison needs one";
		else
			bCan = true;
	}
	else if ( pTargetDesc->eGameType == SGVOGT_ENTRENCHMENT )
	{
		// The MFC's own trench checks are commented out
		// (ObjectPlacerState.cpp:1358-1372): an infantry passenger links in.
		bCan = true;
	}
	else if ( pTargetDesc->eGameType == SGVOGT_UNIT )
	{
		const SMechUnitRPGStats *pVehicle = dynamic_cast<const SMechUnitRPGStats*>( pObjectsDB->GetRPGStats( pTargetDesc ) );
		if ( pVehicle == 0 )
			szWhy = "the database does not know \"" + pTarget->szName + "\" as a vehicle";
		else if ( pVehicle->vEntrancePoint == VNULL2 )
			szWhy = "\"" + pTarget->szName + "\" has no entrance point";
		else if ( pVehicle->nPassangers < nOccupants + 1 )
			szWhy = "\"" + pTarget->szName + "\" takes no more passengers";
		else
			bCan = true;
	}
	else
		szWhy = "\"" + pTarget->szName + "\" takes no passengers";
	if ( bCan )
	{
		*pnType = 0;
		return true;
	}
	// The tow fallback: a tractor or carrier onto an artillery gun with crew
	// points it out-pulls.
	const SMechUnitRPGStats *pTower = dynamic_cast<const SMechUnitRPGStats*>( pObjectsDB->GetRPGStats( pSourceDesc ) );
	if ( pTower != 0 &&
	     ( pTower->type == RPG_TYPE_TRN_CARRIER || pTower->type == RPG_TYPE_TRN_TRACTOR ) &&
	     pTower->vTowPoint != VNULL2 && pTower->fTowingForce > 0 &&
	     pTargetDesc->eGameType == SGVOGT_UNIT )
	{
		const SMechUnitRPGStats *pGun = dynamic_cast<const SMechUnitRPGStats*>( pObjectsDB->GetRPGStats( pTargetDesc ) );
		if ( pGun != 0 && IsArtillery( pGun->type ) && !pGun->vPeoplePoints.empty() )
		{
			if ( pTower->fTowingForce > pGun->fWeight )
			{
				*pnType = 2;
				return true;
			}
			szWhy = NStr::Format( "the tractor cannot tow the gun: it pulls %.0f and the gun weighs %.0f", pTower->fTowingForce, pGun->fWeight );
		}
		else if ( pGun != 0 && !IsArtillery( pGun->type ) )
			szWhy = "\"" + pTarget->szName + "\" is not artillery: a tractor tows a gun";
		else if ( pGun != 0 && pGun->vPeoplePoints.empty() )
			szWhy = "\"" + pTarget->szName + "\" has no crew points: a tractor tows a crewed gun";
	}
	// The train fallback: train cars couple with train cars.
	if ( pTower != 0 && IsTrain( pTower->type ) && pTargetDesc->eGameType == SGVOGT_UNIT )
	{
		const SMechUnitRPGStats *pCar = dynamic_cast<const SMechUnitRPGStats*>( pObjectsDB->GetRPGStats( pTargetDesc ) );
		if ( pCar != 0 && IsTrain( pCar->type ) )
		{
			*pnType = 1;
			return true;
		}
		if ( pCar != 0 && !IsTrain( pCar->type ) )
			szWhy = "train cars couple with train cars: \"" + pTarget->szName + "\" is not a train";
	}
	pSession->szMessage = szWhy.empty() ? "those two do not link" : szWhy;
	*pbRefused = true;
	return false;
}

bool SetLinkInSession( SEditorSession *pSession, int nSource, int nTarget, bool *pbRefused, int *pnToken )
{
	*pbRefused = false;
	*pnToken = -1;
	int nType = 0;
	if ( !CanLinkInSession( pSession, nSource, nTarget, &nType, pbRefused ) )
		return false;
	if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nSource ) != pSession->unknownLinkIDs.end() )
	{
		pSession->szMessage = "the object database does not know this object's type; it is kept as it is";
		*pbRefused = true;
		return false;
	}
	if ( RefuseSharedLinkID( pSession, nSource, pbRefused ) )
		return false;
	const SMapObjectInfo *pRecord = FindSnapshotObject( *pSession, nSource );
	if ( pRecord == 0 )
	{
		pSession->szMessage = "no object with that link ID";
		*pbRefused = true;
		return false;
	}
	// The record before the edit is copied NOW: pRecord points into the
	// snapshot, and PutObjectRecordBack below overwrites it in place - an
	// undo record taken after that would hold the edit itself.
	const SMapObjectInfo recordBefore = *pRecord;
	SMapObjectInfo after = recordBefore;
	after.link.nLinkWith = nTarget;
	if ( nType == 0 )
	{
		// A garrison stands beside its host, the MFC's own offset
		// (ObjectPlacerState.cpp:827-831). A tow or a coupling keeps the
		// passenger where it stands.
		const SMapObjectInfo *pHost = FindSnapshotObject( *pSession, nTarget );
		if ( pHost == 0 )
		{
			pSession->szMessage = "no object with that link ID";
			*pbRefused = true;
			return false;
		}
		after.vPos.x = pHost->vPos.x - 30.0f;
		after.vPos.y = pHost->vPos.y + 30.0f;
	}
	if ( !PutObjectRecordBack( pSession, after ) )
	{
		// The MFC moved a garrison's passenger beside its host and linked it
		// whether or not the engine took the move (ObjectPlacerState.cpp:820-
		// 827 never asks MoveObject's answer): a passenger the engine will
		// not stand there keeps its own place and is linked where it stands.
		if ( nType != 0 || ( after.vPos.x == recordBefore.vPos.x && after.vPos.y == recordBefore.vPos.y ) )
		{
			*pbRefused = true;
			return false;
		}
		after.vPos = recordBefore.vPos;
		if ( !PutObjectRecordBack( pSession, after ) )
		{
			*pbRefused = true;
			return false;
		}
		pSession->szMessage.clear();
	}
	SObjectFieldsEdit *pEditRecord = new SObjectFieldsEdit();
	pEditRecord->nLinkID = nSource;
	pEditRecord->before = recordBefore;
	pEditRecord->after = after;
	*pnToken = LogEdit( pSession, pEditRecord );
	return true;
}

bool UnlinkInSession( SEditorSession *pSession, int nLinkID, bool *pbRefused, int *pnToken )
{
	*pbRefused = false;
	*pnToken = -1;
	if ( pSession == 0 )
	{
		*pbRefused = true;
		return false;
	}
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		*pbRefused = true;
		return false;
	}
	if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nLinkID ) != pSession->unknownLinkIDs.end() )
	{
		pSession->szMessage = "the object database does not know this object's type; it is kept as it is";
		*pbRefused = true;
		return false;
	}
	if ( RefuseSharedLinkID( pSession, nLinkID, pbRefused ) )
		return false;
	const SMapObjectInfo *pRecord = FindSnapshotObject( *pSession, nLinkID );
	if ( pRecord == 0 )
	{
		pSession->szMessage = "no object with that link ID";
		*pbRefused = true;
		return false;
	}
	if ( pRecord->link.nLinkWith == 0 )
		return true; // nothing linked: OK, no token
	// The record before the edit is copied NOW: pRecord points into the
	// snapshot, and PutObjectRecordBack below overwrites it in place - an
	// undo record taken after that would hold the edit itself.
	const SMapObjectInfo recordBefore = *pRecord;
	SMapObjectInfo after = recordBefore;
	after.link.nLinkWith = 0;
	if ( !PutObjectRecordBack( pSession, after ) )
	{
		*pbRefused = true;
		return false;
	}
	SObjectFieldsEdit *pEditRecord = new SObjectFieldsEdit();
	pEditRecord->nLinkID = nLinkID;
	pEditRecord->before = recordBefore;
	pEditRecord->after = after;
	*pnToken = LogEdit( pSession, pEditRecord );
	return true;
}

bool MoveObjectsInSession( SEditorSession *pSession, const int *pnLinkIDs, int nCount, float fDx, float fDy, bool *pbRefused, int *pnToken )
{
	*pbRefused = false;
	*pnToken = -1;
	if ( pSession == 0 )
	{
		*pbRefused = true;
		return false;
	}
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		*pbRefused = true;
		return false;
	}
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		*pbRefused = true;
		return false;
	}
	if ( pnLinkIDs == 0 || nCount <= 0 )
	{
		pSession->szMessage = "no objects to move";
		*pbRefused = true;
		return false;
	}
	// Before anything moves, every member is what an edit may reach and every
	// destination is a place the engine would take - one bad member refuses
	// the whole move and nothing is touched (the M1 rule: the map never holds
	// half a move).
	std::vector<SMoveObjectsEdit::SMovedMember> members;
	members.reserve( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		const int nLinkID = pnLinkIDs[i];
		if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nLinkID ) != pSession->unknownLinkIDs.end() )
		{
			pSession->szMessage = "the object database does not know this object's type; it is kept as it is";
			*pbRefused = true;
			return false;
		}
		if ( RefuseSharedLinkID( pSession, nLinkID, pbRefused ) )
			return false;
		const SMapObjectInfo *pRecord = FindSnapshotObject( *pSession, nLinkID );
		if ( pRecord == 0 )
		{
			pSession->szMessage = "no object with that link ID";
			*pbRefused = true;
			return false;
		}
		for ( size_t j = 0; j < members.size(); ++j )
			if ( members[j].nLinkID == nLinkID )
			{
				pSession->szMessage = NStr::Format( "link ID %d is named twice", nLinkID );
				*pbRefused = true;
				return false;
			}
		SMoveObjectsEdit::SMovedMember member;
		member.nLinkID = nLinkID;
		member.before = *pRecord;
		member.after = *pRecord;
		member.after.vPos.x += fDx;
		member.after.vPos.y += fDy;
		// The destination is checked with the engine's own partition, the same
		// answer a single move's place gets from the engine's AddObject: off
		// the terrain is refused.
		CVec3 vWorld;
		AI2Vis( &vWorld, member.after.vPos.x, member.after.vPos.y, 0.0f );
		int nTileX = 0, nTileY = 0;
		if ( !pEngineTerrain->GetTileIndex( vWorld, &nTileX, &nTileY ) )
		{
			pSession->szMessage = NStr::Format( "object %d would leave the map", nLinkID );
			*pbRefused = true;
			return false;
		}
		members.push_back( member );
	}
	for ( size_t i = 0; i < members.size(); ++i )
	{
		bool bRefused = false;
		if ( !PlaceObjectInSession( pSession, members[i].nLinkID, members[i].after.vPos, members[i].after.nDir, members[i].after.nPlayer, &bRefused ) )
		{
			// The destinations were checked; an engine refusal here is the
			// single-move path's own answer, and it has put nothing back.
			// The members that already moved go back to their before records
			// raw, so the refusal changes nothing.
			for ( size_t j = 0; j < i; ++j )
			{
				bool bIgnored = false;
				PlaceObjectInSession( pSession, members[j].nLinkID, members[j].before.vPos, members[j].before.nDir, members[j].before.nPlayer, &bIgnored );
			}
			*pbRefused = true;
			return false;
		}
	}
	SMoveObjectsEdit *pEdit = new SMoveObjectsEdit();
	pEdit->members.swap( members );
	*pnToken = LogEdit( pSession, pEdit );
	return true;
}

bool ObjectAt( SEditorSession *pSession, float sx, float sy, int *pnLinkID, bool *pbRefused )
{
	*pbRefused = false;
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pScene == 0 || pAIEditor == 0 || pSession->pWorld == 0 )
	{
		pSession->szMessage = "there is no scene";
		return false;
	}
	// The MFC editor's pick (TemplateEditorFrame1.cpp:3384-3400), without the
	// entrenchment exception it makes for a move already under way. The first
	// object the session knows is the answer, as the MFC editor takes the first
	// of what its pick leaves (ObjectPlacerState.cpp:453-454).
	std::pair<IVisObj*, CVec2> *pObjects = 0;
	int nCount = 0;
	pScene->Pick( CVec2( sx, sy ), &pObjects, &nCount, SGVOGT_UNKNOWN );
	for ( int i = 0; i < nCount; ++i )
	{
		IVisObj *pVisObj = pObjects[i].first;
		if ( !pSession->pWorld->IsExistByVis( pVisObj ) )
			continue;
		SMapObject *pMapObject = pSession->pWorld->FindByVis( pVisObj );
		if ( pMapObject == 0 || pMapObject->pDesc == 0 || pMapObject->pAIObj == 0 )
			continue;
		const EObjGameType eType = pMapObject->pDesc->eGameType;
		if ( eType == SGVOGT_BRIDGE || eType == SGVOGT_ENTRENCHMENT )
			continue;
		std::unordered_map<IRefCount*, int>::const_iterator it = pSession->linkByAI.find( pMapObject->pAIObj );
		// A soldier is drawn and picked on his own, but the map holds his squad:
		// the MFC editor goes from one to the other the same way
		// (ObjectPlacerState.cpp:409).
		if ( it == pSession->linkByAI.end() )
			if ( IRefCount *pFormation = pAIEditor->GetFormationOfUnit( pMapObject->pAIObj ) )
				it = pSession->linkByAI.find( pFormation );
		if ( it == pSession->linkByAI.end() )
			continue;
		// An object "Hide checked" holds back is not there to be picked, whether
		// or not the scene still finds its invisible visual (assumption A6).
		if ( IsHiddenLink( *pSession, it->second ) )
			continue;
		*pnLinkID = it->second;
		return true;
	}
	pSession->szMessage = "nothing to pick there";
	*pbRefused = true;
	return false;
}

// ---------------------------------------------------------------------------
// Multi-selection reads and the batch move (M3, D-25). A squad is one map
// record: the map holds the squad, its soldiers are the engine's objects
// beside it (see ObjectAt's note), so "a click on a squad member selects the
// whole squad" is the pick answering the squad's own link ID, and a batch
// move that moves the squad record once keeps every soldier's offset by
// construction - the formation re-places whole.
// ---------------------------------------------------------------------------

namespace {

// The one link ID a pick answers for the object a visual belongs to: the
// object's own, or its squad's when the visual is a lone soldier's. A record
// the session does not know, a bridge, an entrenchment, an object held back
// by Hide checked (A6) and an object of a type the database does not know
// answer nothing - the same rules ObjectAt picks by.
bool PickableLink( SEditorSession *pSession, IVisObj *pVisObj, int *pnLinkID )
{
	if ( pSession->pWorld == 0 || !pSession->pWorld->IsExistByVis( pVisObj ) )
		return false;
	SMapObject *pMapObject = pSession->pWorld->FindByVis( pVisObj );
	if ( pMapObject == 0 || pMapObject->pDesc == 0 || pMapObject->pAIObj == 0 )
		return false;
	const EObjGameType eType = pMapObject->pDesc->eGameType;
	if ( eType == SGVOGT_BRIDGE || eType == SGVOGT_ENTRENCHMENT )
		return false;
	std::unordered_map<IRefCount*, int>::const_iterator it = pSession->linkByAI.find( pMapObject->pAIObj );
	// A soldier is drawn and picked on his own, but the map holds his squad,
	// as ObjectAt answers it (ObjectPlacerState.cpp:409).
	if ( it == pSession->linkByAI.end() )
	{
		if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
			if ( IRefCount *pFormation = pAIEditor->GetFormationOfUnit( pMapObject->pAIObj ) )
				it = pSession->linkByAI.find( pFormation );
	}
	if ( it == pSession->linkByAI.end() )
		return false;
	if ( IsHiddenLink( *pSession, it->second ) )
		return false;
	*pnLinkID = it->second;
	return true;
}

// A game type the tile-rectangle pick passes over, per D-25: spans and
// entrenchment pieces are their groups' business (M2). The screen pick runs
// the same rule through PickableLink, which is where the MFC's pick leaves
// them out too (TemplateEditorFrame1.cpp:3384-3400).
bool NotAGroupPiece( const SGDBObjectDesc *pDesc )
{
	return pDesc != 0 && pDesc->eGameType != SGVOGT_BRIDGE && pDesc->eGameType != SGVOGT_ENTRENCHMENT;
}

// True when the tile-rectangle pick may name the record at all: the session
// knows the type (an unknown object is kept as it is and cannot be moved),
// the link ID is the record's own, and the kind is not a bridge or an
// entrenchment.
bool TilePickable( SEditorSession *pSession, const SMapObjectInfo &rObject )
{
	if ( rObject.link.nLinkID <= 0 )
		return false;
	if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), rObject.link.nLinkID ) != pSession->unknownLinkIDs.end() )
		return false;
	if ( CountIn( pSession->snapshot, rObject.link.nLinkID ) > 1 )
		return false;
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	return pObjectsDB != 0 && NotAGroupPiece( pObjectsDB->GetDesc( rObject.szName.c_str() ) );
}

// The answer a pick hands out: link IDs without duplicates, in the order the
// pick found them (the MFC's m_pickedObjects keeps the pick's own order, which
// the Selector's cycle walks). Two-pass: *pnCount is always the total.
bool PickLinksOut( std::vector<int> &rLinks, int *pnOut, int nCapacity, int *pnCount, bool *pbRefused )
{
	*pbRefused = false;
	*pnCount = int( rLinks.size() );
	if ( nCapacity < 0 || size_t( nCapacity ) < rLinks.size() )
	{
		*pbRefused = true;
		return false;
	}
	for ( size_t i = 0; i < rLinks.size(); ++i )
		pnOut[i] = rLinks[i];
	return true;
}

void AddPickLink( std::vector<int> &rLinks, int nLinkID )
{
	if ( std::find( rLinks.begin(), rLinks.end(), nLinkID ) == rLinks.end() )
		rLinks.push_back( nLinkID );
}

}

// The screen-rectangle pick of the MFC's rubber band (ObjectPlacerState.cpp:920):
// the scene's own rectangle pick (CScene::Pick over a rectangle), which takes
// an object only when the CENTRE of its picture lies inside the rectangle - a
// sprite's picture box (Anim/SpriteAnimation.cpp IsHit over a rectangle), a
// mesh's bounding-sphere centre (MeshVisObj.cpp) - not one whose picture
// merely meets it, as BkEditorObjectAt's point pick does. PickableLink then
// applies ObjectAt's own filters (a soldier answers his squad, bridges and
// entrenchments answer nothing). The rectangle is in screen units, normalized
// here, as the MFC normalizes its own.
bool PickObjectsInSession( SEditorSession *pSession, float fSx0, float fSy0, float fSx1, float fSy1, int *pnOut, int nCapacity, int *pnCount, bool *pbRefused )
{
	*pbRefused = false;
	*pnCount = 0;
	if ( pSession == 0 )
	{
		*pbRefused = true;
		return false;
	}
	if ( !pSession->bMapOpen || pSession->pWorld == 0 )
	{
		pSession->szMessage = "no map is open";
		*pbRefused = true;
		return false;
	}
	IScene *pScene = GetSingleton<IScene>();
	if ( pScene == 0 )
	{
		pSession->szMessage = "there is no scene";
		*pbRefused = true;
		return false;
	}
	CTRect<float> rect( CVec2( Min( fSx0, fSx1 ), Min( fSy0, fSy1 ) ), CVec2( Max( fSx0, fSx1 ), Max( fSy0, fSy1 ) ) );
	std::pair<IVisObj*, CVec2> *pObjects = 0;
	int nNum = 0;
	pScene->Pick( rect, &pObjects, &nNum, SGVOGT_UNKNOWN );
	std::vector<int> links;
	for ( int i = 0; i < nNum; ++i )
	{
		int nLinkID = -1;
		if ( PickableLink( pSession, pObjects[i].first, &nLinkID ) )
			AddPickLink( links, nLinkID );
	}
	return PickLinksOut( links, pnOut, nCapacity, pnCount, pbRefused );
}

// The tile-rectangle pick of the MFC's Ctrl rubber band (ObjectPlacerState.cpp:937-962):
// every editable record whose tile position falls inside the rectangle of
// tiles, bridges and entrenchments passed over. The tiles are the world-cell
// tiles BkEditorWorldToTile answers (y measured from the terrain's far edge);
// a record is inside when the engine's own GetTileIndex of its drawn position
// lands in the rectangle, so the conversion is the engine's own both ways.
bool PickObjectsInTilesInSession( SEditorSession *pSession, int nTx0, int nTy0, int nTx1, int nTy1, int *pnOut, int nCapacity, int *pnCount, bool *pbRefused )
{
	*pbRefused = false;
	*pnCount = 0;
	if ( pSession == 0 )
	{
		*pbRefused = true;
		return false;
	}
	if ( !pSession->bMapOpen || pSession->pWorld == 0 )
	{
		pSession->szMessage = "no map is open";
		*pbRefused = true;
		return false;
	}
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "there is no terrain";
		*pbRefused = true;
		return false;
	}
	const int nLeft = Min( nTx0, nTx1 ), nRight = Max( nTx0, nTx1 );
	const int nTop = Min( nTy0, nTy1 ), nBottom = Max( nTy0, nTy1 );
	std::vector<int> links;
	const std::vector<SMapObjectInfo> *lists[2] = { &pSession->snapshot.objects, &pSession->snapshot.scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
	{
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
		{
			const SMapObjectInfo &rObject = ( *lists[nList] )[i];
			if ( !TilePickable( pSession, rObject ) )
				continue;
			CVec3 vWorld;
			AI2Vis( &vWorld, rObject.vPos.x, rObject.vPos.y, 0.0f );
			int nTileX = 0, nTileY = 0;
			if ( !pEngineTerrain->GetTileIndex( vWorld, &nTileX, &nTileY ) )
				continue;
			if ( nTileX < nLeft || nTileX > nRight || nTileY < nTop || nTileY > nBottom )
				continue;
			AddPickLink( links, rObject.link.nLinkID );
		}
	}
	return PickLinksOut( links, pnOut, nCapacity, pnCount, pbRefused );
}

bool SetSessionDiplomacy( SEditorSession *pSession, int nPlayer, int nDiplomacy )
{
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	if ( nDiplomacy < 0 || nDiplomacy > 2 )
	{
		pSession->szMessage = "no such diplomacy";
		return false;
	}
	if ( !NMapOverlay::SetDiplomacy( &pSession->snapshot, nPlayer, BYTE( nDiplomacy ) ) )
	{
		pSession->szMessage = "no such player";
		return false;
	}
	NMapOverlay::SetDiplomacy( &pSession->working, nPlayer, BYTE( nDiplomacy ) );
	// The engine takes the whole table at once, so it is handed the map's.
	if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
		pAIEditor->SetDiplomacies( pSession->snapshot.diplomacies );
	return true;
}

// The engine's terrain, through the interface only Scene can hand out: a
// dynamic_cast from ITerrain to its sibling ITerrainEditor would cross the
// module boundary and come back null on the Itanium ABI.
ITerrainEditor* EngineTerrain()
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene != 0 ? pScene->GetTerrain() : 0;
	return pTerrain != 0 ? pTerrain->GetEditor() : 0;
}

namespace {

// The overlay's regions are half-open patch rectangles (AffectedPatches, and
// SPaintUndo after it); the engine's two region calls each take something
// else, and neither says so in its signature.
//
// CTerrain::Update iterates top..bottom and left..right inclusive
// (Scene/TerrainEditor.cpp), as the MFC editor calls it: patches.GetSizeX() - 1
// for the whole map (TemplateEditorFrame1.cpp:5175). Handed the half-open
// rectangle it would preprocess and regenerate one patch row and column beyond
// the region the map changed - and read past the patch array at the far edge.
CTRect<int> InclusivePatches( const CTRect<int> &r )
{
	return CTRect<int>( r.minx, r.miny, r.maxx - 1, r.maxy - 1 );
}

// CAIEditor::UpdateTerrain takes a half-open rectangle in TILES: it doubles
// each bound into AI tiles and takes 2 * x2 - 1 as the last
// (AILogic/AIEditorInternal.cpp:417-423), and the MFC editor hands it
// ( 0, 0, tiles.GetSizeX(), tiles.GetSizeY() ) (TemplateEditorFrame1.cpp:5167).
CTRect<int> RegionTiles( const CTRect<int> &r )
{
	return CTRect<int>( r.minx * STerrainPatchInfo::nSizeX, r.miny * STerrainPatchInfo::nSizeY,
	                    r.maxx * STerrainPatchInfo::nSizeX, r.maxy * STerrainPatchInfo::nSizeY );
}

// The engine's own tiles and patches over a region, in SPaintUndo's layout
// (NMapOverlay::CaptureRegion's, which only reads a map): what
// ITerrainEditor::RestoreRegion puts back, crosses and their artwork included,
// so a paint that is refused after the engine was touched leaves it exactly as
// it was.
void CaptureEngineRegion( const STerrainInfo &rEngine, const CTRect<int> &rPatches, NMapOverlay::SPaintUndo *pOut )
{
	pOut->rPatches = rPatches;
	pOut->tiles.clear();
	pOut->patches.clear();
	for ( int y = rPatches.miny * STerrainPatchInfo::nSizeY; y < rPatches.maxy * STerrainPatchInfo::nSizeY; ++y )
		for ( int x = rPatches.minx * STerrainPatchInfo::nSizeX; x < rPatches.maxx * STerrainPatchInfo::nSizeX; ++x )
			pOut->tiles.push_back( rEngine.tiles[y][x] );
	for ( int y = rPatches.miny; y < rPatches.maxy; ++y )
		for ( int x = rPatches.minx; x < rPatches.maxx; ++x )
			pOut->patches.push_back( rEngine.patches[y][x] );
}
}

// A tile the tileset has no terrain type for reaches CTerrain::SetTile, whose
// CTerrainBuilder::HasNoise indexes tileset.terrtypes with the -1 that
// GetTerrainType answers for it (RandomMapGen/TerrainBuilder.cpp), so every
// cell is checked against the tileset the engine loaded for the map before
// anything is painted.
//
// Cost: cells x terrain types x tiles per paint, a linear scan with no index
// from a tile number back to its terrain type. A brush (a handful of cells,
// checked once per stroke) is fine; a large fill painting thousands of cells
// against a tileset with many terrain types would want a tile-number-to-type
// lookup built once from rTileset instead of this triple loop repeated per
// cell (plan 5 Task 3 carried; no fill tool exists yet, so no behaviour
// change here).
bool PaintTilesInTileset( SEditorSession *pSession, const std::vector<NMapOverlay::SPaintCell> &rCells, bool *pbBadTile )
{
	*pbBadTile = false;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	const STilesetDesc &rTileset = pEngineTerrain->GetTilesetDesc();
	if ( rTileset.terrtypes.empty() )
	{
		pSession->szMessage = "the map's tileset has no terrain types";
		return false;
	}
	for ( size_t i = 0; i < rCells.size(); ++i )
	{
		bool bFound = false;
		for ( size_t t = 0; t < rTileset.terrtypes.size() && !bFound; ++t )
		{
			const std::vector<SMainTileDesc> &rTiles = rTileset.terrtypes[t].tiles;
			for ( size_t k = 0; k < rTiles.size() && !bFound; ++k )
				bFound = rTiles[k].nIndex == int( rCells[i].tile );
		}
		if ( !bFound )
		{
			pSession->szMessage = NStr::Format( "tile %d is not in the map's tileset (cell %d,%d)", int( rCells[i].tile ), rCells[i].nX, rCells[i].nY );
			*pbBadTile = true;
			return false;
		}
	}
	return true;
}

// Every tile index the tileset the engine loaded for the map has a terrain
// type for, once each and in ascending order: exactly the tiles
// PaintTilesInTileset lets through, read from the same place.
bool TilesetTiles( SEditorSession *pSession, unsigned char *pOut, int nCapacity, int *pnCount )
{
	*pnCount = 0;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	const STilesetDesc &rTileset = pEngineTerrain->GetTilesetDesc();
	bool bHas[256];
	memset( bHas, 0, sizeof bHas );
	for ( size_t t = 0; t < rTileset.terrtypes.size(); ++t )
	{
		const std::vector<SMainTileDesc> &rTiles = rTileset.terrtypes[t].tiles;
		for ( size_t k = 0; k < rTiles.size(); ++k )
		{
			// A paint cell's tile is an unsigned char, so an index past it can
			// never be painted and is not offered.
			if ( rTiles[k].nIndex >= 0 && rTiles[k].nIndex < 256 )
				bHas[rTiles[k].nIndex] = true;
		}
	}
	int nCount = 0;
	for ( int nTile = 0; nTile < 256; ++nTile )
	{
		if ( !bHas[nTile] )
			continue;
		if ( nCount < nCapacity )
			pOut[nCount] = (unsigned char)nTile;
		++nCount;
	}
	*pnCount = nCount;
	if ( nCount > nCapacity )
	{
		pSession->szMessage = NStr::Format( "the tileset has %d tiles, the buffer room for %d", nCount, nCapacity );
		return false;
	}
	return true;
}

bool EngineTile( SEditorSession *pSession, int nX, int nY, BYTE *pTile )
{
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	const CArray2D<SMainTileInfo> &rTiles = pEngineTerrain->GetTerrainInfo().tiles;
	if ( nX < 0 || nY < 0 || nX >= rTiles.GetSizeX() || nY >= rTiles.GetSizeY() )
	{
		pSession->szMessage = NStr::Format( "cell %d,%d is not on the map", nX, nY );
		return false;
	}
	*pTile = rTiles[nY][nX].tile;
	return true;
}

bool PaintIntoSession( SEditorSession *pSession, const std::vector<NMapOverlay::SPaintCell> &rCells, int *pnToken )
{
	*pnToken = -1;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	if ( rCells.empty() )
		return true;
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pEngineTerrain == 0 || pAIEditor == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}

	// The region is taken before the paint, from the copy that will be saved:
	// it depends only on which cells are named, and both copies have the same
	// terrain size.
	const CTRect<int> rPatches = NMapOverlay::AffectedPatches( pSession->snapshot.terrain, rCells );

	// The engine keeps its own STerrainInfo, loaded when the map opened, so
	// painting the bridge's copies would leave it showing the old tiles.
	//
	// The noise flag is left at 0 and not asked of the caller: whether a tile is
	// noisy belongs to the tile in the tileset, and the preprocessing pass that
	// both sides run ends in CTerrainBuilder::SetNoise, which writes
	// HasNoise(tile) over the whole region regardless
	// (RandomMapGen/TerrainBuilder.cpp:257-264). A value passed in here would be
	// overwritten without a word, which is why BkEditorPaintCell does not have
	// the field.
	//
	// A cell off the map is refused rather than skipped: NMapOverlay::Paint
	// ignores one silently, and a brush that ran off the edge would come back
	// saying it had painted. Every cell is checked before the engine is touched,
	// so a refusal leaves nothing behind.
	std::vector<NMapOverlay::SPaintCell> cells = rCells;
	const CArray2D<SMainTileInfo> &rEngineTiles = pEngineTerrain->GetTerrainInfo().tiles;
	for ( size_t i = 0; i < cells.size(); ++i )
	{
		if ( cells[i].nX < 0 || cells[i].nY < 0 ||
		     cells[i].nX >= rEngineTiles.GetSizeX() || cells[i].nY >= rEngineTiles.GetSizeY() )
		{
			pSession->szMessage = NStr::Format( "cell %d,%d is not on the map", cells[i].nX, cells[i].nY );
			return false;
		}
		cells[i].noise = 0;
	}

	// What the engine holds over the region before SetTile changes it, for the
	// two ways a paint can still be refused below. Putting it back raw is exact;
	// SetTile and Update again would run the preprocessing pass and roll new
	// cross artwork.
	NMapOverlay::SPaintUndo engineBefore;
	CaptureEngineRegion( pEngineTerrain->GetTerrainInfo(), rPatches, &engineBefore );
	for ( size_t i = 0; i < cells.size(); ++i )
		pEngineTerrain->SetTile( cells[i].nX, cells[i].nY, cells[i].tile );

	NMapOverlay::SPaintUndo undo;
	if ( !NMapOverlay::Paint( &pSession->snapshot, cells, &undo ) )
	{
		// Paint puts the map back itself when it refuses, so there is nothing to
		// undo here - but the engine has the new tiles already, and it has to go
		// back with the map or the editor would draw a paint the file never got.
		pEngineTerrain->RestoreRegion( engineBefore.rPatches, engineBefore.tiles, engineBefore.patches );
		pSession->szMessage = "the map would not take that paint (a cell outside it, or no tileset)";
		return false;
	}
	// The same deterministic function on the copy the engine was built from, so
	// the two cannot drift; TerrainMatchesEngine is what catches it if they do.
	NMapOverlay::SPaintUndo workingUndo;
	if ( !NMapOverlay::Paint( &pSession->working, cells, &workingUndo ) )
	{
		NMapOverlay::UndoPaint( &pSession->snapshot, undo );
		pEngineTerrain->RestoreRegion( engineBefore.rPatches, engineBefore.tiles, engineBefore.patches );
		pSession->szMessage = "the map would not take that paint";
		return false;
	}

	// The engine now runs its own Update over the region: the same preprocessing
	// pass and the same cross generation the overlay just ran - same input, same
	// function, so the two land on the same answer, and TerrainMatchesEngine is
	// what says so rather than this comment.
	pEngineTerrain->Update( InclusivePatches( rPatches ) );
	pAIEditor->UpdateTerrain( RegionTiles( rPatches ), pSession->working.terrain );

	SEditorSession::SPaintRecord record;
	record.before = undo;
	NMapOverlay::CaptureRegion( pSession->snapshot, undo.rPatches, &record.after );
	pSession->paints.push_back( record );
	*pnToken = int( pSession->paints.size() ) - 1;
	pSession->appliedPaints.push_back( *pnToken );
	pSession->undonePaints.clear();
	return true;
}

// Puts one recorded region back into both copies and the engine, raw: no
// preprocessing and no cross generation, so the engine lands on exactly the
// tiles and crosses the record holds. Declared in session.h since 05-02:
// the Update Map composite's undo rides the same route.
bool PutRegionBack( SEditorSession *pSession, const NMapOverlay::SPaintUndo &rRegion )
{
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pEngineTerrain == 0 || pAIEditor == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	NMapOverlay::UndoPaint( &pSession->snapshot, rRegion );
	NMapOverlay::UndoPaint( &pSession->working, rRegion );
	pEngineTerrain->RestoreRegion( rRegion.rPatches, rRegion.tiles, rRegion.patches );
	// The AI's passability follows the tiles.
	pAIEditor->UpdateTerrain( RegionTiles( rRegion.rPatches ), pSession->working.terrain );
	return true;
}

bool UndoPaintInSession( SEditorSession *pSession, int nToken, bool *pbRefused )
{
	*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	if ( pSession->appliedPaints.empty() || pSession->appliedPaints.back() != nToken )
	{
		pSession->szMessage = "paints are undone newest first";
		*pbRefused = true;
		return false;
	}
	if ( !PutRegionBack( pSession, pSession->paints[nToken].before ) )
		return false;
	pSession->appliedPaints.pop_back();
	pSession->undonePaints.push_back( nToken );
	return true;
}

bool RedoPaintInSession( SEditorSession *pSession, int nToken, bool *pbRefused )
{
	*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	if ( pSession->undonePaints.empty() || pSession->undonePaints.back() != nToken )
	{
		pSession->szMessage = "paints are redone in the order they were undone";
		*pbRefused = true;
		return false;
	}
	if ( !PutRegionBack( pSession, pSession->paints[nToken].after ) )
		return false;
	pSession->undonePaints.pop_back();
	pSession->appliedPaints.push_back( nToken );
	return true;
}

// ---------------------------------------------------------------------------
// Altitudes (M3, D-19)
// ---------------------------------------------------------------------------

namespace {
// The patch rectangle covering a vertex rectangle, half-open like the
// overlay's own regions, clamped to the map's patch count - a vertex
// rectangle at the far edge (the last vertex is one past the last cell)
// would otherwise step one patch over the end, exactly what
// InclusivePatches/RegionTiles exist to stop for paints.
CTRect<int> VertexPatches( const STerrainInfo &rTerrain, const CTRect<int> &rVertices )
{
	return CTRect<int>( rVertices.minx / STerrainPatchInfo::nSizeX, rVertices.miny / STerrainPatchInfo::nSizeY,
	                    Min( ( rVertices.maxx - 1 ) / STerrainPatchInfo::nSizeX + 1, rTerrain.patches.GetSizeX() ),
	                    Min( ( rVertices.maxy - 1 ) / STerrainPatchInfo::nSizeY + 1, rTerrain.patches.GetSizeY() ) );
}
}

// The engine's own terrain over the region, written raw in place - the MFC
// editor's own route, through GetTerrainInfo's const_cast
// (DrawShadeState.cpp:204), because ITerrainEditor has a per-vertex shade
// call but no per-vertex height one - and the covering patches redrawn, as
// the MFC's pTerrainEditor->Update did after a shade change. Declared in
// session.h since 05-02: the heights machine in session_terrain.cpp pushes
// the same way.
void PutEngineAltitudes( ITerrainEditor *pEngineTerrain, const NMapOverlay::SAltitudeUndo &rRegion )
{
	STerrainInfo &rEngine = const_cast<STerrainInfo&>( pEngineTerrain->GetTerrainInfo() );
	NMapOverlay::UndoTerrainAltitudeRegion( &rEngine, rRegion );
	pEngineTerrain->Update( InclusivePatches( VertexPatches( rEngine, rRegion.rVertices ) ) );
}

// One altitude region edit of the log: the grown region before and after,
// put back raw into both copies and the engine - heights, shades and padding
// bytes, never recomputed (D-03's rule, SPaintUndo's own). The record type
// itself and PutEngineAltitudes live in session.h/session_terrain.cpp's
// world since M3's heights machine (05-02) logs the same edit.
bool PutAltitudeEditBack( SEditorSession *pSession, const NMapOverlay::SAltitudeUndo &rRegion );

bool SAltitudeEdit::Revert( SEditorSession *pSession )
{
	return PutAltitudeEditBack( pSession, before );
}
bool SAltitudeEdit::Reapply( SEditorSession *pSession )
{
	return PutAltitudeEditBack( pSession, after );
}

bool PutAltitudeEditBack( SEditorSession *pSession, const NMapOverlay::SAltitudeUndo &rRegion )
{
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	NMapOverlay::UndoAltitudeRegion( &pSession->snapshot, rRegion );
	NMapOverlay::UndoAltitudeRegion( &pSession->working, rRegion );
	PutEngineAltitudes( pEngineTerrain, rRegion );
	// When Instant Update is on, the stroke this record came from moved the
	// roads', rivers' and sounds' z with the terrain (05-02, D-20). The z is
	// a pure function of the altitudes, so putting the altitudes back and
	// re-deriving it lands on exactly the bytes the stroke found - the
	// Update Map composite's undo and redo ride the same rule.
	if ( pSession->bInstantUpdate )
		UpdateObjectsZInSession( pSession );
	return true;
}

bool ApplyAltitudesInSession( SEditorSession *pSession, const CTRect<int> &rVertices,
                              const std::vector<float> &rHeights, bool *pbRefused, int *pnToken )
{
	*pnToken = -1;
	if ( pbRefused )
		*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession )
			pSession->szMessage = "no map is open";
		return false;
	}
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}

	// A map saved without altitudes gets its sheet here, as the open path
	// builds the working copy's (the MFC editor's own load rule,
	// TemplateEditorFrame1.cpp:1658): the edit is what turns the implicit
	// flat sheet into a real one.
	STerrainInfo &rSnapshot = pSession->snapshot.terrain;
	if ( rSnapshot.altitudes.GetSizeX() == 0 || rSnapshot.altitudes.GetSizeY() == 0 )
	{
		rSnapshot.altitudes.SetSizes( rSnapshot.patches.GetSizeX() * STerrainPatchInfo::nSizeX + 1,
		                              rSnapshot.patches.GetSizeY() * STerrainPatchInfo::nSizeY + 1 );
		rSnapshot.altitudes.SetZero();
	}
	if ( rVertices.minx < 0 || rVertices.miny < 0 ||
	     rVertices.maxx <= rVertices.minx || rVertices.maxy <= rVertices.miny ||
	     rVertices.maxx > rSnapshot.altitudes.GetSizeX() || rVertices.maxy > rSnapshot.altitudes.GetSizeY() )
	{
		pSession->szMessage = NStr::Format( "vertices %d,%d..%d,%d are not on the map",
		                                    rVertices.minx, rVertices.miny, rVertices.maxx, rVertices.maxy );
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}
	const size_t nCount = size_t( rVertices.maxx - rVertices.minx ) * size_t( rVertices.maxy - rVertices.miny );
	if ( rHeights.size() != nCount )
	{
		pSession->szMessage = NStr::Format( "the region holds %d vertices, %d heights were given",
		                                    int( nCount ), int( rHeights.size() ) );
		if ( pbRefused )
			*pbRefused = true;
		return false;
	}

	const STerrainInfo &rEngineRead = pEngineTerrain->GetTerrainInfo();
	if ( rEngineRead.altitudes.GetSizeX() != rSnapshot.altitudes.GetSizeX() ||
	     rEngineRead.altitudes.GetSizeY() != rSnapshot.altitudes.GetSizeY() )
	{
		pSession->szMessage = "the engine's terrain is a different size from the map's";
		return false;
	}

	// The region as D-19 defines it: the edit rectangle grown by the shade
	// kernel, because a height edit changes every neighbouring vertex's
	// shade. Everything is captured over that region, before anything moves.
	const CTRect<int> rGrown = NMapOverlay::GrowForShades( pSession->snapshot, rVertices );
	const SGFXLightDirectional sunlight = CVertexAltitudeInfo::GetSunLight(
		static_cast<CMapInfo::SEASON>( pSession->working.nSeason ) );
	NMapOverlay::SAltitudeUndo before, workingBefore, engineBefore;
	NMapOverlay::CaptureAltitudeRegion( pSession->snapshot, rGrown, &before );
	NMapOverlay::CaptureAltitudeRegion( pSession->working, rGrown, &workingBefore );
	NMapOverlay::CaptureTerrainAltitudeRegion( rEngineRead, rGrown, &engineBefore );

	// Whole records built from the snapshot's own storage - bitwise, the
	// raw-struct padding rule (a member-wise copy would carry the heap's
	// bytes into the file) - with the caller's heights in them; the shades
	// the record carried do not survive the next step, which is the point.
	std::vector<SVertexAltitude> values( nCount );
	size_t nValue = 0;
	for ( int y = rVertices.miny; y < rVertices.maxy; ++y )
		for ( int x = rVertices.minx; x < rVertices.maxx; ++x, ++nValue )
		{
			memcpy( &values[nValue], &rSnapshot.altitudes[y][x], sizeof( SVertexAltitude ) );
			values[nValue].fHeight = rHeights[nValue];
		}

	// The deterministic function on the copy that will be saved, then on the
	// copy the engine was built from; a failure puts everything back raw.
	STerrainInfo &rWorking = pSession->working.terrain;
	if ( !NMapOverlay::SetAltitudeRegion( &pSession->snapshot, rVertices, values, 0 ) ||
	     !CMapInfo::UpdateTerrainShades( &rSnapshot, rGrown, sunlight ) )
	{
		NMapOverlay::UndoAltitudeRegion( &pSession->snapshot, before );
		PutEngineAltitudes( pEngineTerrain, engineBefore );
		pSession->szMessage = "the map would not take that altitude edit";
		return false;
	}
	if ( !NMapOverlay::SetAltitudeRegion( &pSession->working, rVertices, values, 0 ) ||
	     !CMapInfo::UpdateTerrainShades( &rWorking, rGrown, sunlight ) )
	{
		NMapOverlay::UndoAltitudeRegion( &pSession->snapshot, before );
		NMapOverlay::UndoAltitudeRegion( &pSession->working, workingBefore );
		PutEngineAltitudes( pEngineTerrain, engineBefore );
		pSession->szMessage = "the map would not take that altitude edit";
		return false;
	}

	// The engine's own copy gets what the function just wrote, raw, and the
	// covering patches redrawn - the MFC editor's whole-map shade recompute
	// at save is deliberately not here (D-19).
	NMapOverlay::SAltitudeUndo after;
	NMapOverlay::CaptureAltitudeRegion( pSession->snapshot, rGrown, &after );
	PutEngineAltitudes( pEngineTerrain, after );

	SAltitudeEdit *pEdit = new SAltitudeEdit();
	pEdit->before = before;
	pEdit->after = after;
	*pnToken = LogEdit( pSession, pEdit );
	return true;
}

bool ReadAltitudesInSession( SEditorSession *pSession, const CTRect<int> &rVertices, std::vector<float> *pHeights )
{
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession )
			pSession->szMessage = "no map is open";
		return false;
	}
	const STerrainInfo::TVertexAltitudeArray2D &rAltitudes = pSession->working.terrain.altitudes;
	if ( rAltitudes.GetSizeX() == 0 || rAltitudes.GetSizeY() == 0 )
	{
		pSession->szMessage = "the map has no altitudes";
		return false;
	}
	if ( rVertices.minx < 0 || rVertices.miny < 0 ||
	     rVertices.maxx <= rVertices.minx || rVertices.maxy <= rVertices.miny ||
	     rVertices.maxx > rAltitudes.GetSizeX() || rVertices.maxy > rAltitudes.GetSizeY() )
	{
		pSession->szMessage = NStr::Format( "vertices %d,%d..%d,%d are not on the map",
		                                    rVertices.minx, rVertices.miny, rVertices.maxx, rVertices.maxy );
		return false;
	}
	pHeights->clear();
	pHeights->reserve( size_t( rVertices.maxx - rVertices.minx ) * size_t( rVertices.maxy - rVertices.miny ) );
	for ( int y = rVertices.miny; y < rVertices.maxy; ++y )
		for ( int x = rVertices.minx; x < rVertices.maxx; ++x )
			pHeights->push_back( rAltitudes[y][x].fHeight );
	return true;
}

bool TerrainMatchesEngine( SEditorSession *pSession )
{
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	const STerrainInfo &rEngine = pEngineTerrain->GetTerrainInfo();
	// The snapshot, not the working copy: the snapshot is what gets written, so
	// this asks the question the editor actually cares about - does the engine
	// show what the file will hold. Shades are left out on purpose; the working
	// copy has UpdateTerrainShades applied and the snapshot may not, and paint
	// does not touch them.
	const STerrainInfo &rMap = pSession->snapshot.terrain;
	if ( rEngine.tiles.GetSizeX() != rMap.tiles.GetSizeX() || rEngine.tiles.GetSizeY() != rMap.tiles.GetSizeY() )
	{
		pSession->szMessage = "the engine's terrain is a different size from the map's";
		return false;
	}
	for ( int y = 0; y < rMap.tiles.GetSizeY(); ++y )
		for ( int x = 0; x < rMap.tiles.GetSizeX(); ++x )
			if ( rEngine.tiles[y][x].tile != rMap.tiles[y][x].tile ||
			     rEngine.tiles[y][x].noise != rMap.tiles[y][x].noise )
			{
				pSession->szMessage = NStr::Format( "tile %d,%d: engine has %d/%d, the map has %d/%d",
				                                    x, y, int( rEngine.tiles[y][x].tile ), int( rEngine.tiles[y][x].noise ),
				                                    int( rMap.tiles[y][x].tile ), int( rMap.tiles[y][x].noise ) );
				return false;
			}
	if ( rEngine.patches.GetSizeX() != rMap.patches.GetSizeX() || rEngine.patches.GetSizeY() != rMap.patches.GetSizeY() )
	{
		pSession->szMessage = "the engine has a different number of patches from the map";
		return false;
	}
	for ( int y = 0; y < rMap.patches.GetSizeY(); ++y )
		for ( int x = 0; x < rMap.patches.GetSizeX(); ++x )
		{
			const STerrainPatchInfo &rEnginePatch = rEngine.patches[y][x];
			const STerrainPatchInfo &rMapPatch = rMap.patches[y][x];
			if ( rEnginePatch.basecrosses.size() != rMapPatch.basecrosses.size() )
			{
				pSession->szMessage = NStr::Format( "patch %d,%d: engine has %d crosses, the map has %d",
				                                    x, y, int( rEnginePatch.basecrosses.size() ), int( rMapPatch.basecrosses.size() ) );
				return false;
			}
			// Where the crosses are and what they join, but not which artwork was
			// drawn for them. STileTypeDesc::GetMapsIndex picks the variant with
			// rand() against the probability ranges in the tileset
			// (Formats/fmtTerrain.h:84-92), so the engine and the map each roll
			// their own and the indices differ by design - measured as
			// "engine has 67/157, the map has 67/147", the same joined tile with
			// a different picture of it. Everything that decides the shape of the
			// terrain is compared; only the roll is not.
			for ( size_t i = 0; i < rMapPatch.basecrosses.size(); ++i )
				if ( rEnginePatch.basecrosses[i].tile != rMapPatch.basecrosses[i].tile ||
				     rEnginePatch.basecrosses[i].x != rMapPatch.basecrosses[i].x ||
				     rEnginePatch.basecrosses[i].y != rMapPatch.basecrosses[i].y ||
				     rEnginePatch.basecrosses[i].flags != rMapPatch.basecrosses[i].flags )
				{
					pSession->szMessage = NStr::Format( "patch %d,%d cross %d: engine joins tile %d at %d,%d flags %d, the map joins tile %d at %d,%d flags %d",
					                                    x, y, int( i ),
					                                    int( rEnginePatch.basecrosses[i].tile ), int( rEnginePatch.basecrosses[i].x ),
					                                    int( rEnginePatch.basecrosses[i].y ), int( rEnginePatch.basecrosses[i].flags ),
					                                    int( rMapPatch.basecrosses[i].tile ), int( rMapPatch.basecrosses[i].x ),
					                                    int( rMapPatch.basecrosses[i].y ), int( rMapPatch.basecrosses[i].flags ) );
					return false;
				}
		}
	return true;
}

bool WorldMatchesSession( SEditorSession *pSession )
{
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( pAIEditor == 0 || pSession->pWorld == 0 )
	{
		pSession->szMessage = "there is no world";
		return false;
	}
	std::vector<SMapObject*> objects;
	pSession->pWorld->GetObjects( &objects );
	for ( size_t i = 0; i < objects.size(); ++i )
	{
		SMapObject *pMO = objects[i];
		if ( pMO == 0 || pMO->pAIObj == 0 || pMO->pDesc == 0 )
			continue;
		// Units and squads only. The world also draws objects that share a
		// link ID - on arnheim 361 terrain objects (river banks, ravines,
		// flowers) all carry 0, "no link ID", and byLinkID keeps one engine
		// object per ID - which is a separate question from whether a deleted
		// unit leaves anything behind. Edits of those are refused
		// (RefuseSharedLinkID).
		if ( pMO->pDesc->eGameType != SGVOGT_UNIT && pMO->pDesc->eGameType != SGVOGT_SQUAD )
			continue;
		IRefCount *pOwner = pMO->pAIObj;
		if ( pSession->linkByAI.find( pOwner ) == pSession->linkByAI.end() )
			if ( IRefCount *pFormation = pAIEditor->GetFormationOfUnit( pOwner ) )
				pOwner = pFormation;
		if ( pSession->linkByAI.find( pOwner ) == pSession->linkByAI.end() )
		{
			pSession->szMessage = NStr::Format( "the world draws %s, which no object of the map is",
			                                    pMO->pDesc != 0 ? pMO->pDesc->szKey.c_str() : "an object without a description" );
			return false;
		}
	}
	for ( std::unordered_map<int, CPtr<IRefCount> >::const_iterator it = pSession->byLinkID.begin(); it != pSession->byLinkID.end(); ++it )
	{
		IRefCount *pAIObject = it->second;
		if ( pAIEditor->IsFormation( pAIObject ) )
		{
			IRefCount **ppUnits = 0;
			int nUnits = 0;
			pAIEditor->GetUnitsInFormation( pAIObject, &ppUnits, &nUnits );
			for ( int i = 0; i < nUnits; ++i )
				if ( !pSession->pWorld->IsExistByAI( ppUnits[i] ) )
				{
					pSession->szMessage = NStr::Format( "a soldier of the squad with link ID %d is not in the world", it->first );
					return false;
				}
		}
		// A bridge span is drawn through the world's own span list, not as a map
		// object (CWorldBase::AIUpdateBridges).
		else if ( !pSession->pWorld->IsExistByAI( pAIObject ) && pSession->pWorld->FindSpanByAI( pAIObject ) == 0 )
		{
			pSession->szMessage = NStr::Format( "the object with link ID %d is not in the world", it->first );
			return false;
		}
	}
	// The engine's own link table agrees: every ID the session holds an object
	// under names that object there, and a deleted object's ID names nothing,
	// so that its restore can register it again. IDs of 0 and below are never
	// registered, and a shared one names whichever object came last.
	for ( std::unordered_map<int, CPtr<IRefCount> >::const_iterator it = pSession->byLinkID.begin(); it != pSession->byLinkID.end(); ++it )
		if ( it->first > 0 && CountIn( pSession->snapshot, it->first ) == 1 && pAIEditor->ObjectByLink( it->first ) != it->second )
		{
			pSession->szMessage = NStr::Format( "the engine does not have the object with link ID %d under that ID", it->first );
			return false;
		}
	for ( std::unordered_map<int, SEditorSession::STombstone>::const_iterator it = pSession->tombstones.begin(); it != pSession->tombstones.end(); ++it )
		if ( it->first > 0 && pAIEditor->ObjectByLink( it->first ) != 0 )
		{
			pSession->szMessage = NStr::Format( "the engine still has an object under link ID %d, which was deleted", it->first );
			return false;
		}
	return true;
}

bool WorldToTile( SEditorSession *pSession, float wx, float wy, int *pnX, int *pnY )
{
	if ( pSession == 0 || !pSession->bMapOpen || pnX == 0 || pnY == 0 )
		return false;
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	if ( !pEngineTerrain->GetTileIndex( CVec3( wx, wy, 0.0f ), pnX, pnY ) )
	{
		pSession->szMessage = "that point is not on the map";
		return false;
	}
	return true;
}

bool WorldToAITile( SEditorSession *pSession, float wx, float wy, int *pnX, int *pnY )
{
	if ( pSession == 0 || !pSession->bMapOpen || pnX == 0 || pnY == 0 )
		return false;
	ITerrainEditor *pEngineTerrain = EngineTerrain();
	if ( pEngineTerrain == 0 )
	{
		pSession->szMessage = "the engine has no terrain";
		return false;
	}
	// The fence tool's mapping (RoadDrawState.cpp:571-572): half a world cell,
	// rounded, not the truncation CMapInfo::GetAITileIndices does.
	if ( !pEngineTerrain->GetAITileIndex( CVec3( wx, wy, 0.0f ), pnX, pnY ) )
	{
		pSession->szMessage = "that point is not on the map";
		return false;
	}
	return true;
}

bool SetSessionCamera( SEditorSession *pSession, float wx, float wy )
{
	if ( pSession == 0 || !pSession->bEngineStarted )
		return false;
	ICamera *pCamera = GetSingleton<ICamera>();
	if ( pCamera == 0 )
	{
		pSession->szMessage = "there is no camera";
		return false;
	}
	// Placed the way the game places its mission camera
	// (GameTT/iMissionInternal.cpp, SetMissionCameraPlacement), not only moved:
	// CCamera's own default looks along yaw 0 at pitch 45, and the terrain is
	// laid out in screen space for yaw 45 and pitch 30 (CTerrain::MovePatches
	// steps its patches by fixed pixel offsets from where the map's corner
	// lands). Objects go through the view matrix and drew in place; the ground
	// was laid out thousands of pixels away and clipped, and stayed black.
	IGFX *pGFX = GetSingleton<IGFX>();
	const RECT rcScreen = pGFX != 0 ? pGFX->GetScreenRect() : RECT();
	const float fGameplayCameraHeight = float( rcScreen.bottom - rcScreen.top );
	// D-12: fYawOffsetDegrees is 0 until BkEditorSetYaw sets it, so this is
	// exactly the game's own placement until then.
	pCamera->SetPlacement( CVec3( wx, wy, 0.0f ), 1024 * 4 + fGameplayCameraHeight, -ToRadian( 90.0f + 30.0f ), ToRadian( 45.0f + pSession->fYawOffsetDegrees ) );
	pCamera->Update();
	return true;
}

bool DrawSessionFrame( SEditorSession *pSession )
{
	if ( pSession == 0 || !pSession->bEngineStarted )
		return false;
	IGFX *pGFX = GetSingleton<IGFX>();
	IScene *pScene = GetSingleton<IScene>();
	ICamera *pCamera = GetSingleton<ICamera>();
	if ( pGFX == 0 || pScene == 0 || pCamera == 0 )
	{
		pSession->szMessage = "the renderer is not started";
		return false;
	}
	// The game's own frame without the interface over it
	// (GameTT/iMissionInternal.cpp:2666-2671). BeginScene returning false is a
	// lost device rather than a bug, so it is a refusal and not a failure.
	if ( !pGFX->BeginScene() )
	{
		pSession->szMessage = "the device would not begin a scene";
		return false;
	}
	pGFX->Clear( 0, 0, GFXCLEAR_ALL, 0 );
	pScene->Draw( pCamera );
	pGFX->EndScene();
	pGFX->Flip();
	return true;
}

bool ScreenToWorld( SEditorSession *pSession, float sx, float sy, float *pwx, float *pwy )
{
	if ( pSession == 0 || !pSession->bEngineStarted || pwx == 0 || pwy == 0 )
		return false;
	IScene *pScene = GetSingleton<IScene>();
	if ( pScene == 0 )
	{
		pSession->szMessage = "there is no scene";
		return false;
	}
	// GetPos3 answers against the terrain the camera is looking at, so it needs
	// a camera that has been updated - which SetSessionCamera and
	// DrawSessionFrame both do.
	CVec3 vWorld( VNULL3 );
	pScene->GetPos3( &vWorld, CVec2( sx, sy ) );
	*pwx = vWorld.x;
	*pwy = vWorld.y;
	return true;
}

bool WorldToScreen( SEditorSession *pSession, float wx, float wy, float *psx, float *psy )
{
	if ( pSession == 0 || !pSession->bEngineStarted || psx == 0 || psy == 0 )
		return false;
	IScene *pScene = GetSingleton<IScene>();
	if ( pScene == 0 )
	{
		pSession->szMessage = "there is no scene";
		return false;
	}
	// z=0, matching ScreenToWorld/GetPos3: IAILogic::GetIntersectionWithTerrain
	// - the real terrain ray-cast GetPos3 tries first - does not succeed in
	// this bridge's headless session (measured: it always falls through to
	// GetPos3's own z=0-plane algebraic fallback, regardless of the point's
	// true height), so ScreenToWorld's x,y already assume z=0. Using the
	// terrain's real height here instead would draw the brush outline off
	// the very ground a click resolves against - self-consistency with the
	// rest of the picking pipeline (ScreenToWorld, WorldToTile, ObjectAt)
	// matters more than a height this bridge cannot round-trip anyway.
	CVec2 vScreen( 0, 0 );
	pScene->GetPos2( &vScreen, CVec3( wx, wy, 0.0f ) );
	*psx = vScreen.x;
	*psy = vScreen.y;
	return true;
}

void WorldToMap( float wx, float wy, float *pmx, float *pmy )
{
	CVec3 vMap( VNULL3 );
	Vis2AIFast( &vMap, wx, wy, 0.0f );
	*pmx = vMap.x;
	*pmy = vMap.y;
}
