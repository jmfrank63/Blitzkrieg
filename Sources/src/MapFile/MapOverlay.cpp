// The overlay rules of the spec's "Saving: the snapshot and the overlay",
// applied to a map in memory and needing neither the engine nor the object
// database. The bridge in plan 3 applies the same calls to its own snapshot;
// the tests here build their expected map with them.
#include "StdAfx.h"
#include "MapOverlay.h"
#include "../RandomMapGen/MapInfo_Types.h"
#include "../RandomMapGen/RMG_Types.h"
#include "../Formats/fmtTerrain.h"
#include "../RandomMapGen/Resource_Types.h"

namespace NMapOverlay
{
namespace {
// The two object lists, walked as one where a rule applies to both.
SMapObjectInfo* FindObject( SLoadMapInfo *pMap, int nLinkID, std::vector<SMapObjectInfo> **ppList, size_t *pnIndex )
{
	std::vector<SMapObjectInfo> *lists[2] = { &pMap->objects, &pMap->scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
			if ( (*lists[nList])[i].link.nLinkID == nLinkID )
			{
				if ( ppList ) *ppList = lists[nList];
				if ( pnIndex ) *pnIndex = i;
				return &(*lists[nList])[i];
			}
	return 0;
}

// Puts the region back and says no. The caller's record goes empty with it:
// nothing happened, so there is nothing to undo.
bool FailAndRestore( SLoadMapInfo *pMap, SPaintUndo *pCallersUndo, const SPaintUndo *pOurs )
{
	// Declared in MapOverlay.h, defined below.
	UndoPaint( pMap, *pOurs );
	if ( pCallersUndo )
		*pCallersUndo = SPaintUndo();
	return false;
}

void Record( const STerrainInfo &rTerrain, const CTRect<int> &r, SPaintUndo *pUndo )
{
	pUndo->rPatches = r;
	pUndo->tiles.clear();
	pUndo->patches.clear();
	for ( int y = r.miny * STerrainPatchInfo::nSizeY; y < r.maxy * STerrainPatchInfo::nSizeY; ++y )
		for ( int x = r.minx * STerrainPatchInfo::nSizeX; x < r.maxx * STerrainPatchInfo::nSizeX; ++x )
			pUndo->tiles.push_back( rTerrain.tiles[y][x] );
	for ( int y = r.miny; y < r.maxy; ++y )
		for ( int x = r.minx; x < r.maxx; ++x )
			pUndo->patches.push_back( rTerrain.patches[y][x] );
}

std::string Numbered( const char *pszWhat, int nIndex )
{
	char szBuffer[64];
	snprintf( szBuffer, sizeof szBuffer, "%s %d", pszWhat, nIndex );
	return szBuffer;
}
}

int NextLinkID( const SLoadMapInfo &rMap )
{
	CUsedLinkIDs used;   // std::set<int>, RandomMapGen/RMG_Types.h:26
	CMapInfo::GetUsedLinkIDs( rMap, &used );
	int nMax = 0;
	for ( CUsedLinkIDs::const_iterator it = used.begin(); it != used.end(); ++it )
		nMax = Max( nMax, *it );
	// GetUsedLinkIDs reads the object lists; a link ID held only by a reference
	// to an object already gone would not be in it, so take the objects' own
	// IDs into account too rather than trusting one source.
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
		nMax = Max( nMax, rMap.objects[i].link.nLinkID );
	for ( size_t i = 0; i < rMap.scenarioObjects.size(); ++i )
		nMax = Max( nMax, rMap.scenarioObjects[i].link.nLinkID );
	return nMax + 1;
}

// The spec's list, one loop each. They are not collapsed on purpose: the string
// each contributes is what the editor shows the player when it refuses.
void FindReferences( const SLoadMapInfo &rMap, int nLinkID, std::vector<std::string> *pReferences )
{
	if ( pReferences == 0 )
		return;
	pReferences->clear();

	for ( size_t i = 0; i < rMap.bridges.size(); ++i )
		for ( size_t j = 0; j < rMap.bridges[i].size(); ++j )
			if ( rMap.bridges[i][j] == nLinkID )
			{
				pReferences->push_back( Numbered( "bridge", int( i ) ) );
				break;
			}

	int nCommand = 0;
	for ( SLoadMapInfo::TStartCommandsList::const_iterator it = rMap.startCommandsList.begin();
	      it != rMap.startCommandsList.end(); ++it, ++nCommand )
	{
		bool bHit = it->linkID == nLinkID;
		for ( size_t j = 0; j < it->unitLinkIDs.size() && !bHit; ++j )
			bHit = it->unitLinkIDs[j] == nLinkID;
		if ( bHit )
			pReferences->push_back( Numbered( "start command", nCommand ) );
	}

	for ( std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator it = rMap.reinforcements.groups.begin();
	      it != rMap.reinforcements.groups.end(); ++it )
		for ( size_t j = 0; j < it->second.ids.size(); ++j )
			if ( it->second.ids[j] == nLinkID )
			{
				pReferences->push_back( Numbered( "reinforcement group", it->first ) );
				break;
			}

	// A passenger whose nLinkWith points at a vehicle holds that vehicle: the
	// spec refuses the vehicle's delete while the passenger is inside it.
	const std::vector<SMapObjectInfo> *lists[2] = { &rMap.objects, &rMap.scenarioObjects };
	const char *pszListName[2] = { "object", "scenario object" };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
		{
			const SMapObjectInfo &rObject = (*lists[nList])[i];
			if ( rObject.link.nLinkID != nLinkID && rObject.link.nLinkWith == nLinkID )
				pReferences->push_back( Numbered( pszListName[nList], int( i ) ) + " (" + rObject.szName + ")" );
		}
}

bool AddObject( SLoadMapInfo *pMap, const SAddObject &rAdd, int *pnLinkID )
{
	if ( pMap == 0 || rAdd.szName.empty() )
		return false;
	SMapObjectInfo object;
	object.szName = rAdd.szName;
	object.vPos = rAdd.vPos;
	object.nDir = rAdd.nDir;
	object.nPlayer = rAdd.nPlayer;
	object.nScriptID = -1;
	object.fHP = 1.0f;
	// Left unpacked: packing needs the object database to know the type, and
	// for a type it does not know it dereferences null. The bridge packs this
	// one object when it places it; see the spec's "Frame indices and unknown
	// types".
	object.nFrameIndex = 0;
	object.link.nLinkID = NextLinkID( *pMap );
	object.link.bIntention = false;
	object.link.nLinkWith = -1;
	( rAdd.bScenario ? pMap->scenarioObjects : pMap->objects ).push_back( object );
	if ( pnLinkID )
		*pnLinkID = object.link.nLinkID;
	return true;
}

bool MoveObject( SLoadMapInfo *pMap, const SMoveObject &rMove )
{
	if ( pMap == 0 )
		return false;
	SMapObjectInfo *pObject = FindObject( pMap, rMove.nLinkID, 0, 0 );
	if ( pObject == 0 )
		return false;
	// Three fields, and the record is otherwise the snapshot's: the packed
	// frame index, the HP, the script ID and the link all stay as they were.
	pObject->vPos = rMove.vPos;
	pObject->nDir = rMove.nDir;
	pObject->nPlayer = rMove.nPlayer;
	return true;
}

bool DeleteObject( SLoadMapInfo *pMap, int nLinkID, std::string *pRefusal )
{
	if ( pMap == 0 )
		return false;
	std::vector<std::string> references;
	FindReferences( *pMap, nLinkID, &references );
	if ( !references.empty() )
	{
		if ( pRefusal )
		{
			*pRefusal = "still referred to by ";
			for ( size_t i = 0; i < references.size(); ++i )
				*pRefusal += ( i == 0 ? "" : ", " ) + references[i];
		}
		return false;
	}
	std::vector<SMapObjectInfo> *pList = 0;
	size_t nIndex = 0;
	if ( FindObject( pMap, nLinkID, &pList, &nIndex ) == 0 )
	{
		if ( pRefusal ) *pRefusal = "no object with that link ID";
		return false;
	}
	// Erased, never renumbered: every other object keeps the link ID the rest
	// of the map refers to it by.
	pList->erase( pList->begin() + nIndex );
	return true;
}

bool SetDiplomacy( SLoadMapInfo *pMap, int nPlayer, BYTE nDiplomacy )
{
	if ( pMap == 0 || nPlayer < 0 || nPlayer >= int( pMap->diplomacies.size() ) )
		return false;
	pMap->diplomacies[nPlayer] = nDiplomacy;
	return true;
}
}

namespace NMapOverlay
{
// The spec's "Terrain edits", step by step. R is in patch coordinates, which is
// what CMapInfo::UpdateTerrainCrosses iterates (MapInfo_StaticMethods.cpp:450-463);
// handing it tile coordinates asks for a rectangle a few thousand patches wide.
CTRect<int> AffectedPatches( const STerrainInfo &rTerrain, const std::vector<SPaintCell> &rCells )
{
	const int nPatchesX = rTerrain.patches.GetSizeX(), nPatchesY = rTerrain.patches.GetSizeY();
	CTRect<int> r( nPatchesX, nPatchesY, 0, 0 );
	for ( size_t i = 0; i < rCells.size(); ++i )
	{
		// The 8-neighbourhood in cells, then the patches those cells fall in: a
		// painted cell on a patch border is read by the neighbouring patch's
		// crosses, so that patch is in the region too.
		const int nMinPatchX = Max( 0, ( rCells[i].nX - 1 ) ) / STerrainPatchInfo::nSizeX;
		const int nMaxPatchX = Min( nPatchesX * STerrainPatchInfo::nSizeX - 1, rCells[i].nX + 1 ) / STerrainPatchInfo::nSizeX;
		const int nMinPatchY = Max( 0, ( rCells[i].nY - 1 ) ) / STerrainPatchInfo::nSizeY;
		const int nMaxPatchY = Min( nPatchesY * STerrainPatchInfo::nSizeY - 1, rCells[i].nY + 1 ) / STerrainPatchInfo::nSizeY;
		r.minx = Min( r.minx, nMinPatchX );  r.maxx = Max( r.maxx, nMaxPatchX + 1 );
		r.miny = Min( r.miny, nMinPatchY );  r.maxy = Max( r.maxy, nMaxPatchY + 1 );
	}
	if ( r.minx > r.maxx || r.miny > r.maxy )
		return CTRect<int>( 0, 0, 0, 0 );
	return r;
}

bool Paint( SLoadMapInfo *pMap, const std::vector<SPaintCell> &rCells, SPaintUndo *pUndo )
{
	if ( pMap == 0 || rCells.empty() )
		return false;
	STerrainInfo &rTerrain = pMap->terrain;
	const CTRect<int> r = AffectedPatches( rTerrain, rCells );
	if ( r.maxx <= r.minx || r.maxy <= r.miny )
		return false;

	// Record the region before anything changes. The preprocessing pass below
	// rewrites tiles that were never painted, so the record has to cover all of
	// R and not just the painted cells. One is kept here whether or not the
	// caller asked for one, because a paint that fails halfway has to put the
	// region back with it.
	SPaintUndo undoForFailure;
	Record( rTerrain, r, &undoForFailure );
	if ( pUndo )
		*pUndo = undoForFailure;

	for ( size_t i = 0; i < rCells.size(); ++i )
	{
		if ( rCells[i].nX < 0 || rCells[i].nY < 0 ||
		     rCells[i].nX >= rTerrain.tiles.GetSizeX() || rCells[i].nY >= rTerrain.tiles.GetSizeY() )
			continue;
		rTerrain.tiles[rCells[i].nY][rCells[i].nX].tile = rCells[i].tile;
		rTerrain.tiles[rCells[i].nY][rCells[i].nX].noise = rCells[i].noise;
	}

	// Everything from here can still fail, and the cells above are already
	// written, so each way out puts the region back. "false" means the map was
	// not touched - a caller that took it at its word and saved would otherwise
	// write a paint that never happened. The undo record is emptied with it, so
	// a caller cannot undo a second time.
	//
	// The tileset and crosset the map names, read the way CMapInfo::
	// UpdateTerrainCrosses reads them (MapInfo_Methods.cpp:220-230). This needs
	// the registered data storage and nothing else - no renderer, no database.
	if ( GetSingleton<IDataStorage>() == 0 )
		return FailAndRestore( pMap, pUndo, &undoForFailure );
	STilesetDesc tilesetDesc;
	SCrossetDesc crossetDesc;
	LoadDataResource( rTerrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc );
	LoadDataResource( rTerrain.szCrossetDesc, "", false, 0, "crosset", crossetDesc );
	// CTerrainBuilder::ComparePriority indexes tileset.terrtypes without
	// checking (RandomMapGen/TerrainBuilder.cpp:34), so an unreadable tileset
	// reaches it as an empty vector and takes the process down. Refuse here and
	// let the caller say the descriptor is missing, which is a data problem
	// rather than a paint that failed.
	if ( tilesetDesc.terrtypes.empty() )
		return FailAndRestore( pMap, pUndo, &undoForFailure );
	if ( !CMapInfo::UpdateTerrainCrosses( &rTerrain, r, tilesetDesc, crossetDesc ) )
		return FailAndRestore( pMap, pUndo, &undoForFailure );
	return true;
}

void UndoPaint( SLoadMapInfo *pMap, const SPaintUndo &rUndo )
{
	if ( pMap == 0 )
		return;
	STerrainInfo &rTerrain = pMap->terrain;
	const CTRect<int> &r = rUndo.rPatches;
	size_t nTile = 0;
	for ( int y = r.miny * STerrainPatchInfo::nSizeY; y < r.maxy * STerrainPatchInfo::nSizeY; ++y )
		for ( int x = r.minx * STerrainPatchInfo::nSizeX; x < r.maxx * STerrainPatchInfo::nSizeX; ++x, ++nTile )
			if ( nTile < rUndo.tiles.size() )
				rTerrain.tiles[y][x] = rUndo.tiles[nTile];
	size_t nPatch = 0;
	for ( int y = r.miny; y < r.maxy; ++y )
		for ( int x = r.minx; x < r.maxx; ++x, ++nPatch )
			if ( nPatch < rUndo.patches.size() )
				rTerrain.patches[y][x] = rUndo.patches[nPatch];
}
}
