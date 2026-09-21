// Building the engine state a map is edited through.
//
// Kept apart from bridge.cpp because it is the one part of the bridge that is
// not about the C boundary: it is the MFC editor's map-open sequence with the
// MFC taken out and one guard put in.
#include "StdAfx.h"
#include "session.h"
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

	// Nothing is disturbed until the read succeeds: a failed open leaves the
	// session on whatever map it already had.
	CMapInfo read;
	if ( !NMapFile::Read( pszPath, &read, &pSession->szMessage ) )
		return false;

	pSession->bMapOpen = false;
	pSession->byLinkID.clear();
	pSession->unknownLinkIDs.clear();
	pSession->futureBuildLinkIDs.clear();
	pSession->nBridgeSpansInMap = 0;
	pSession->nBridgeSpansPlaced = 0;
	pSession->snapshot = read;
	pSession->szMapPath = pszPath;
	MakeWorkingCopy( pSession );

	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	IScene *pScene = GetSingleton<IScene>();
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pAIEditor == 0 || pScene == 0 || pObjectsDB == 0 )
	{
		pSession->szMessage = "the engine is missing the AI editor, the scene or the object database";
		return false;
	}

	// The AI editor first: the terrain it is initialised with is what
	// IsObjectInsideOfMap and AddNewObject answer against below.
	pAIEditor->Clear();
	pAIEditor->SetDiplomacies( pSession->working.diplomacies );
	pAIEditor->Init( pSession->working.terrain );

	{
		// ITerrain::Load takes the map's own path, as the MFC editor passes
		// szSelectedFileMapFullName: it derives the names of the files beside
		// the map from it.
		CPtr<ITerrain> pTerrain = CreateTerrain();
		pTerrain->Load( pSession->szMapPath.c_str(), pSession->working.terrain );
		pScene->SetTerrain( pTerrain );
	}

	std::vector<SMapObjectInfo> bridgeSpans;
	PlaceObjects( pSession, pSession->working.objects, pObjectsDB, pAIEditor, &bridgeSpans );
	PlaceObjects( pSession, pSession->working.scenarioObjects, pObjectsDB, pAIEditor, &bridgeSpans );
	BuildBridges( pSession, bridgeSpans, pObjectsDB, pAIEditor );

	pSession->bMapOpen = true;
	return true;
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

	int nLinkID = -1;
	if ( !NMapOverlay::AddObject( &pSession->snapshot, rAdd, &nLinkID ) )
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
	NMapOverlay::AddObject( &pSession->working, rAdd, 0 );

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
	return true;
}

bool DeleteObjectFromSession( SEditorSession *pSession, int nLinkID, bool *pbRefused )
{
	if ( pbRefused )
		*pbRefused = false;
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
	// The map decides first: something still referring to the object - a bridge,
	// a start command, a reinforcement group, a passenger - means no, and the
	// engine is never asked.
	std::string szRefusal;
	if ( !NMapOverlay::DeleteObject( &pSession->snapshot, nLinkID, &szRefusal ) )
	{
		pSession->szMessage = szRefusal;
		if ( pbRefused ) *pbRefused = true;
		return false;
	}
	std::string szIgnored;
	NMapOverlay::DeleteObject( &pSession->working, nLinkID, &szIgnored );

	std::unordered_map<int, CPtr<IRefCount> >::iterator itEngine = pSession->byLinkID.find( nLinkID );
	if ( itEngine != pSession->byLinkID.end() )
	{
		if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
			pAIEditor->DeleteObject( itEngine->second );
		pSession->byLinkID.erase( itEngine );
	}
	return true;
}

bool SetSessionDiplomacy( SEditorSession *pSession, int nPlayer, int nDiplomacy )
{
	if ( pSession == 0 || !pSession->bMapOpen )
		return false;
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
}

bool PaintIntoSession( SEditorSession *pSession, const std::vector<NMapOverlay::SPaintCell> &rCells )
{
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
	// saying it had painted.
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
		pEngineTerrain->SetTile( cells[i].nX, cells[i].nY, cells[i].tile );
	}

	NMapOverlay::SPaintUndo undo;
	if ( !NMapOverlay::Paint( &pSession->snapshot, cells, &undo ) )
	{
		// Paint puts the map back itself when it refuses, so there is nothing to
		// undo here - but the engine has the new tiles already, and it has to go
		// back with the map or the editor would draw a paint the file never got.
		for ( size_t i = 0; i < cells.size(); ++i )
			pEngineTerrain->SetTile( cells[i].nX, cells[i].nY, pSession->snapshot.terrain.tiles[cells[i].nY][cells[i].nX].tile );
		pEngineTerrain->Update( rPatches );
		pSession->szMessage = "the map would not take that paint (a cell outside it, or no tileset)";
		return false;
	}
	// The same deterministic function on the copy the engine was built from, so
	// the two cannot drift; TerrainMatchesEngine is what catches it if they do.
	NMapOverlay::SPaintUndo workingUndo;
	if ( !NMapOverlay::Paint( &pSession->working, cells, &workingUndo ) )
	{
		NMapOverlay::UndoPaint( &pSession->snapshot, undo );
		for ( size_t i = 0; i < cells.size(); ++i )
			pEngineTerrain->SetTile( cells[i].nX, cells[i].nY, pSession->snapshot.terrain.tiles[cells[i].nY][cells[i].nX].tile );
		pEngineTerrain->Update( rPatches );
		pSession->szMessage = "the map would not take that paint";
		return false;
	}

	// The engine now runs its own Update over the region: the same preprocessing
	// pass and the same cross generation the overlay just ran - same input, same
	// function, so the two land on the same answer, and TerrainMatchesEngine is
	// what says so rather than this comment.
	pEngineTerrain->Update( rPatches );
	pAIEditor->UpdateTerrain( rPatches, pSession->working.terrain );
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

