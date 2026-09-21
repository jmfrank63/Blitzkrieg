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
#include "../Scene/Terrain.h"
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
