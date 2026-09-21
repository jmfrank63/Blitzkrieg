// The overlay rules of the spec's "Saving: the snapshot and the overlay",
// applied to a map in memory and needing neither the engine nor the object
// database. The bridge in plan 3 applies the same calls to its own snapshot;
// the tests here build their expected map with them.
#include "StdAfx.h"
#include "MapOverlay.h"
#include "../RandomMapGen/MapInfo_Types.h"
#include "../RandomMapGen/RMG_Types.h"

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
