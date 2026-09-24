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
#include "../Main/GameDB.h"
#include "../AILogic/AILogic.h"
#include "../Scene/Scene.h"
#include "../GFX/GFX.H"
#include "../Scene/Terrain.h"
#include "../Formats/fmtTerrain.h"
#include "../RandomMapGen/VA_Types.h"

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
//
// A span stored with negative HP is one the mission builds during play. The
// engine will not take it that way, so - as the editor does - it is created at
// full HP and its link ID listed; the snapshot still holds the negative value,
// so what gets written back is unchanged.
void BuildBridges( SEditorSession *pSession, const std::vector<SMapObjectInfo> &rSpans,
                   IObjectsDB *pObjectsDB, IAIEditor *pAIEditor )
{
	const std::vector< std::vector<int> > &rBridges = pSession->working.bridges;
	for ( size_t nBridge = 0; nBridge < rBridges.size(); ++nBridge )
	{
		for ( size_t nSpan = 0; nSpan < rBridges[nBridge].size(); ++nSpan )
		{
			const int nLinkID = rBridges[nBridge][nSpan];
			++pSession->nBridgeSpansInMap;
			std::vector<SMapObjectInfo>::const_iterator it = rSpans.begin();
			for ( ; it != rSpans.end(); ++it )
				if ( it->link.nLinkID == nLinkID )
					break;
			if ( it == rSpans.end() )
				continue;
			SMapObjectInfo span = *it;
			if ( span.fHP < 0 )
			{
				span.fHP = 1.0f;
				pSession->futureBuildLinkIDs.push_back( nLinkID );
			}
			const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( span.szName.c_str() );
			if ( pDesc == 0 )
				continue;
			if ( IRefCount *pAIObject = PlaceOneObject( span, pDesc, pAIEditor ) )
			{
				pSession->byLinkID[nLinkID] = pAIObject;
				++pSession->nBridgeSpansPlaced;
			}
		}
	}
}
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
	pSession->bMapOpen = false;
	pSession->byLinkID.clear();
	pSession->unknownLinkIDs.clear();
	pSession->futureBuildLinkIDs.clear();
	pSession->nBridgeSpansInMap = 0;
	pSession->nBridgeSpansPlaced = 0;
	pSession->snapshot = read;
	pSession->szMapPath = pszPath;
	pSession->paints.clear();
	pSession->appliedPaints.clear();
	pSession->undonePaints.clear();
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

	for ( int i = 0; i < 2; ++i )
		if ( !pScene->ToggleShow( SCENE_SHOW_WARFOG ) )	// false: the scene's fog is off
			break;

	std::vector<SMapObjectInfo> bridgeSpans;
	PlaceObjects( pSession, pSession->working.objects, pObjectsDB, pAIEditor, &bridgeSpans );
	PlaceObjects( pSession, pSession->working.scenarioObjects, pObjectsDB, pAIEditor, &bridgeSpans );
	BuildBridges( pSession, bridgeSpans, pObjectsDB, pAIEditor );
	// The AI has queued a notification for every object it took; one update
	// turns them into map objects with visuals in the scene.
	UpdateSessionWorld( pSession );

	pSession->bMapOpen = true;
	return true;
}

void UpdateSessionWorld( SEditorSession *pSession )
{
	if ( pSession->pWorld != 0 )
		pSession->pWorld->UpdateNow();
	pSession->linkByAI.clear();
	for ( std::unordered_map<int, CPtr<IRefCount> >::const_iterator it = pSession->byLinkID.begin(); it != pSession->byLinkID.end(); ++it )
		pSession->linkByAI[it->second.GetPtr()] = it->first;
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
	return NMapFile::Write( pszPath, pSession->snapshot, &pSession->szMessage );
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
		}
	return nCapacity >= nTotal;
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

	// Never below the floor: an ID a deleted object held may be wanted back by
	// its restore, and NextLinkID alone would hand the highest one out again.
	// Both copies get the ID explicitly, so they cannot disagree about it.
	NMapOverlay::SAddObject add = rAdd;
	add.nLinkID = Max( NMapOverlay::NextLinkID( pSession->snapshot ), pSession->nLinkIDFloor );
	int nLinkID = -1;
	if ( !NMapOverlay::AddObject( &pSession->snapshot, add, &nLinkID ) )
	{
		pSession->szMessage = "the map would not take the object";
		return false;
	}
	// The overlay leaves the frame index at 0 because packing needs the object
	// database. The bridge has it, so the one object it just added is packed
	// here - a fence or a span otherwise goes out with an index that means
	// something else.
	if ( SMapObjectInfo *pAdded = FindIn( &pSession->snapshot, nLinkID ) )
		CMapInfo::PackFrameIndex( pObjectsDB, pAdded );
	NMapOverlay::AddObject( &pSession->working, add, 0 );

	const SMapObjectInfo *pSnapshotObject = FindIn( &pSession->snapshot, nLinkID );
	IRefCount *pAIObject = pSnapshotObject != 0 ? PlaceOneObject( *pSnapshotObject, pDesc, pAIEditor ) : 0;
	if ( pAIObject == 0 )
	{
		// The engine would not have it - outside the map, most often - so the
		// snapshot must not keep it either, or the editor would save an object
		// it never showed.
		std::string szIgnored;
		NMapOverlay::DeleteObject( &pSession->snapshot, nLinkID, &szIgnored );
		NMapOverlay::DeleteObject( &pSession->working, nLinkID, &szIgnored );
		pSession->szMessage = "the engine would not place the object there";
		return false;
	}
	pSession->byLinkID[nLinkID] = pAIObject;
	pSession->nLinkIDFloor = nLinkID + 1;
	UpdateSessionWorld( pSession );
	if ( pnLinkID )
		*pnLinkID = nLinkID;
	return true;
}

bool PlaceObjectInSession( SEditorSession *pSession, int nLinkID, const CVec3 &vPos, int nDir, int nPlayer, bool *pbRefused )
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
	// The map decides first: something still referring to the object - a bridge,
	// a start command, a reinforcement group, a passenger - means no, and the
	// engine is never asked.
	//
	// Both records are kept, with their lists and places, for a restore.
	SEditorSession::STombstone tombstone;
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
	pSession->tombstones.erase( it );
	UpdateSessionWorld( pSession );
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
		*pnLinkID = it->second;
		return true;
	}
	pSession->szMessage = "nothing to pick there";
	*pbRefused = true;
	return false;
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

namespace {
// The engine's terrain, through the interface only Scene can hand out: a
// dynamic_cast from ITerrain to its sibling ITerrainEditor would cross the
// module boundary and come back null on the Itanium ABI.
ITerrainEditor* EngineTerrain()
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene != 0 ? pScene->GetTerrain() : 0;
	return pTerrain != 0 ? pTerrain->GetEditor() : 0;
}

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

namespace {
// Puts one recorded region back into both copies and the engine, raw: no
// preprocessing and no cross generation, so the engine lands on exactly the
// tiles and crosses the record holds.
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
	pCamera->SetAnchor( CVec3( wx, wy, 0.0f ) );
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

