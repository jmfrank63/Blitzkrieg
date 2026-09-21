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

// One pass over objects or scenarioObjects.
//
// The MFC editor writes pObjectsDB->GetDesc( name )->eGameType with no null
// check (TemplateEditorFrame1.cpp:1751 and 1783). GetDesc returns 0 for a name
// the database does not know, and the NI_ASSERT inside it compiles away in
// release, so opening a map that names an object the mod no longer ships
// crashes the editor. Here an unknown object is listed, left in the snapshot
// and never placed - which is also what lets it survive a save untouched.
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
		// Bridges are placed as linked spans and not one object at a time; the
		// MFC editor collects them the same way and builds them afterwards.
		if ( pDesc->eGameType == SGVOGT_BRIDGE )
		{
			pBridgeSpans->push_back( *it );
			continue;
		}
		if ( !pAIEditor->IsObjectInsideOfMap( *it ) )
			continue;
		IRefCount *pAIObject = 0;
		if ( pAIEditor->AddNewObject( *it, &pAIObject ) && pAIObject != 0 )
			pSession->byLinkID[it->link.nLinkID] = pAIObject;
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

	pSession->bMapOpen = true;
	return true;
}
