// The overlay rules of the spec's "Saving: the snapshot and the overlay",
// applied to a map in memory and needing neither the engine nor the object
// database. The bridge in plan 3 applies the same calls to its own snapshot;
// the tests here build their expected map with them.
#include "StdAfx.h"
#include "MapOverlay.h"
#include "MapRecords.h"
#include <algorithm>
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

// Why a delete of nLinkID is refused, or false if it is not. Only what the
// game's loaders assert on and what M3's links depend on refuses: a bridge span
// (LoadBridges asserts every link), a trench piece (LoadEntrenchments does),
// and a vehicle a passenger's nLinkWith still points at. Everything else that
// names the object is edited by the cascade. Link ID 0 is "no link ID" and is
// never a reference.
bool WhyRefused( const SLoadMapInfo &rMap, int nLinkID, std::string *pReason )
{
	if ( nLinkID == 0 )
		return false;
	std::vector<std::string> referrers, pieces;
	for ( size_t i = 0; i < rMap.bridges.size(); ++i )
		for ( size_t j = 0; j < rMap.bridges[i].size(); ++j )
			if ( rMap.bridges[i][j] == nLinkID )
			{
				referrers.push_back( Numbered( "bridge", int( i ) ) );
				break;
			}
	for ( size_t i = 0; i < rMap.entrenchments.size(); ++i )
	{
		bool bHit = false;
		const std::vector<SEntrenchmentInfo::TSegment> &rSections = rMap.entrenchments[i].sections;
		for ( size_t j = 0; j < rSections.size() && !bHit; ++j )
			for ( size_t k = 0; k < rSections[j].size() && !bHit; ++k )
				bHit = rSections[j][k] == nLinkID;
		if ( bHit )
			pieces.push_back( Numbered( "entrenchment", int( i ) ) );
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
				referrers.push_back( Numbered( pszListName[nList], int( i ) ) + " (" + rObject.szName + ")" );
		}
	if ( referrers.empty() && pieces.empty() )
		return false;
	if ( pReason )
	{
		pReason->clear();
		if ( !referrers.empty() )
		{
			*pReason = "still referred to by ";
			for ( size_t i = 0; i < referrers.size(); ++i )
				*pReason += ( i == 0 ? "" : ", " ) + referrers[i];
		}
		if ( !pieces.empty() )
		{
			*pReason += pReason->empty() ? "still part of " : "; still part of ";
			for ( size_t i = 0; i < pieces.size(); ++i )
				*pReason += ( i == 0 ? "" : ", " ) + pieces[i];
		}
	}
	return true;
}

// Takes nLinkID out of every start command that names it, in list order: out of
// the units (a command left with no unit is erased, as the MFC editor does -
// RemoveObjectFromAIStartCommand) and, as a target, set to link ID 0
// (RMGC_INVALID_LINK_ID_VALUE, C3) rather than left dangling. A command that
// names none of it is not touched, so it stays byte for byte.
void RemoveFromStartCommands( SLoadMapInfo *pMap, int nLinkID, SCascade *pCascade )
{
	size_t nPosition = 0;
	for ( SLoadMapInfo::TStartCommandsList::iterator it = pMap->startCommandsList.begin();
	      it != pMap->startCommandsList.end(); )
	{
		std::vector<int> &rUnits = it->unitLinkIDs;
		const bool bUnit = std::find( rUnits.begin(), rUnits.end(), nLinkID ) != rUnits.end();
		const bool bTarget = it->linkID == nLinkID;
		if ( !bUnit && !bTarget )
		{
			++it;
			++nPosition;
			continue;
		}
		SStartCommandChange change;
		change.nPosition = nPosition;
		change.before = *it;
		change.bUnitRemoved = bUnit;
		if ( bUnit )
			rUnits.erase( std::remove( rUnits.begin(), rUnits.end(), nLinkID ), rUnits.end() );
		if ( bUnit && rUnits.empty() )
		{
			// Nobody left to command: the command goes, whatever its target was.
			change.bErased = true;
			it = pMap->startCommandsList.erase( it );
		}
		else
		{
			if ( bTarget )
			{
				it->linkID = 0;
				change.bTargetCleared = true;
			}
			++it;
			++nPosition;
		}
		pCascade->startCommands.push_back( change );
	}
}

// Erases every reserve position that names nLinkID as its artillery or its
// truck (MFC RemoveObjectFromReservePositions, C3), in list order.
void RemoveFromReservePositions( SLoadMapInfo *pMap, int nLinkID, SCascade *pCascade )
{
	size_t nPosition = 0;
	for ( SLoadMapInfo::TReservePositionsList::iterator it = pMap->reservePositionsList.begin();
	      it != pMap->reservePositionsList.end(); )
	{
		if ( it->nArtilleryLinkID != nLinkID && it->nTruckLinkID != nLinkID )
		{
			++it;
			++nPosition;
			continue;
		}
		SReservePositionChange change;
		change.nPosition = nPosition;
		change.before = *it;
		it = pMap->reservePositionsList.erase( it );
		pCascade->reservePositions.push_back( change );
	}
}

// The group IDs whose ids hold nScriptID, ascending, so what a message says does
// not depend on the hash map's order.
std::vector<int> GroupsHolding( const SLoadMapInfo &rMap, int nScriptID )
{
	std::vector<int> groups;
	for ( std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator it = rMap.reinforcements.groups.begin();
	      it != rMap.reinforcements.groups.end(); ++it )
		if ( std::find( it->second.ids.begin(), it->second.ids.end(), nScriptID ) != it->second.ids.end() )
			groups.push_back( it->first );
	std::sort( groups.begin(), groups.end() );
	return groups;
}

// The AI general sides whose mobile reinforcements name nScriptID, ascending.
std::vector<int> SidesHolding( const SLoadMapInfo &rMap, int nScriptID )
{
	std::vector<int> sides;
	const std::vector<SAIGeneralSideInfo> &rSides = rMap.aiGeneralMapInfo.sidesInfo;
	for ( size_t i = 0; i < rSides.size(); ++i )
		if ( std::find( rSides[i].mobileScriptIDs.begin(), rSides[i].mobileScriptIDs.end(), nScriptID ) != rSides[i].mobileScriptIDs.end() )
			sides.push_back( int( i ) );
	return sides;
}

std::string ScriptIDNote( int nScriptID, const char *pszWho, int nWho )
{
	char szBuffer[128];
	snprintf( szBuffer, sizeof szBuffer, "script ID %d is still used by %s %d", nScriptID, pszWho, nWho );
	return szBuffer;
}

// Reinforcement groups and the AI general's mobile reinforcements name SCRIPT
// IDs, which other objects and the Lua script may share, so a delete never edits
// them. It says so when the last object carrying a script ID they name is gone.
// Called after the object has been removed.
void NoteScriptIDStillNamed( const SLoadMapInfo &rMap, int nScriptID, SCascade *pCascade )
{
	if ( nScriptID < 0 )
		return;
	const std::vector<SMapObjectInfo> *lists[2] = { &rMap.objects, &rMap.scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
			if ( (*lists[nList])[i].nScriptID == nScriptID )
				return;
	const std::vector<int> groups = GroupsHolding( rMap, nScriptID );
	for ( size_t i = 0; i < groups.size(); ++i )
		pCascade->notes.push_back( ScriptIDNote( nScriptID, "reinforcement group", groups[i] ) );
	const std::vector<int> sides = SidesHolding( rMap, nScriptID );
	for ( size_t i = 0; i < sides.size(); ++i )
		pCascade->notes.push_back( ScriptIDNote( nScriptID, "the AI general of side", sides[i] ) );
}

// "2", "2 and 5", "2, 5 and 7".
std::string JoinNumbers( const std::vector<int> &rNumbers )
{
	std::string szOut;
	for ( size_t i = 0; i < rNumbers.size(); ++i )
	{
		char szNumber[32];
		snprintf( szNumber, sizeof szNumber, "%d", rNumbers[i] );
		if ( i > 0 )
			szOut += i + 1 == rNumbers.size() ? " and " : ", ";
		szOut += szNumber;
	}
	return szOut;
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
// each contributes is what the editor shows the player.
//
// Link ID 0 is "no link ID" (RMGC_INVALID_LINK_ID_VALUE): hundreds of shipped
// objects carry it, start commands and reserve positions use it for "none", so
// it is never a reference and finds nothing (C11). Reinforcement groups and the
// AI general's mobile reinforcements hold SCRIPT IDs, not link IDs, so they are
// matched against the object's script ID - never its link ID.
void FindReferences( const SLoadMapInfo &rMap, int nLinkID, std::vector<std::string> *pReferences )
{
	if ( pReferences == 0 )
		return;
	pReferences->clear();
	if ( nLinkID == 0 )
		return;

	for ( size_t i = 0; i < rMap.bridges.size(); ++i )
		for ( size_t j = 0; j < rMap.bridges[i].size(); ++j )
			if ( rMap.bridges[i][j] == nLinkID )
			{
				pReferences->push_back( Numbered( "bridge", int( i ) ) );
				break;
			}

	for ( size_t i = 0; i < rMap.entrenchments.size(); ++i )
	{
		bool bHit = false;
		const std::vector<SEntrenchmentInfo::TSegment> &rSections = rMap.entrenchments[i].sections;
		for ( size_t j = 0; j < rSections.size() && !bHit; ++j )
			for ( size_t k = 0; k < rSections[j].size() && !bHit; ++k )
				bHit = rSections[j][k] == nLinkID;
		if ( bHit )
			pReferences->push_back( Numbered( "entrenchment", int( i ) ) );
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

	int nPosition = 0;
	for ( SLoadMapInfo::TReservePositionsList::const_iterator it = rMap.reservePositionsList.begin();
	      it != rMap.reservePositionsList.end(); ++it, ++nPosition )
		if ( it->nArtilleryLinkID == nLinkID || it->nTruckLinkID == nLinkID )
			pReferences->push_back( Numbered( "reserve position", nPosition ) );

	const SMapObjectInfo *pObject = FindObject( const_cast<SLoadMapInfo*>( &rMap ), nLinkID, 0, 0 );
	if ( pObject != 0 && pObject->nScriptID >= 0 )
	{
		const std::vector<int> groups = GroupsHolding( rMap, pObject->nScriptID );
		for ( size_t i = 0; i < groups.size(); ++i )
			pReferences->push_back( Numbered( "reinforcement group", groups[i] ) );
		const std::vector<int> sides = SidesHolding( rMap, pObject->nScriptID );
		for ( size_t i = 0; i < sides.size(); ++i )
			pReferences->push_back( Numbered( "AI general side", sides[i] ) );
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
	object.nScriptID = rAdd.nScriptID;
	object.fHP = rAdd.fHP;
	// Left as the caller gave it, which is 0 unless it knows better: packing
	// needs the object database to know the type, and for a type it does not
	// know it dereferences null. The bridge packs this one object when it
	// places it; see the spec's "Frame indices and unknown types".
	object.nFrameIndex = rAdd.nFrameIndex;
	// A given link ID is the caller's promise that it is free - the bridge's
	// floor, which never hands out an ID a restore may need back. One in use is
	// refused rather than doubled.
	if ( rAdd.nLinkID >= 0 && FindObject( pMap, rAdd.nLinkID, 0, 0 ) != 0 )
		return false;
	object.link.nLinkID = rAdd.nLinkID >= 0 ? rAdd.nLinkID : NextLinkID( *pMap );
	object.link.bIntention = false;
	object.link.nLinkWith = rAdd.nLinkWith;
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

bool DeleteObject( SLoadMapInfo *pMap, int nLinkID, std::string *pRefusal, SDeletedObject *pDeleted )
{
	if ( pMap == 0 )
		return false;
	std::string szReason;
	if ( WhyRefused( *pMap, nLinkID, &szReason ) )
	{
		if ( pRefusal )
			*pRefusal = szReason;
		return false;
	}
	std::vector<SMapObjectInfo> *pList = 0;
	size_t nIndex = 0;
	if ( FindObject( pMap, nLinkID, &pList, &nIndex ) == 0 )
	{
		if ( pRefusal ) *pRefusal = "no object with that link ID";
		return false;
	}
	SDeletedObject deleted;
	deleted.object = (*pList)[nIndex];
	deleted.bScenario = pList == &pMap->scenarioObjects;
	deleted.nIndex = nIndex;
	// Erased, never renumbered: every other object keeps the link ID the rest
	// of the map refers to it by.
	pList->erase( pList->begin() + nIndex );
	// Link ID 0 is no link ID: a start command listing 0 is not naming this
	// object, so the cascade never runs for it.
	if ( nLinkID != 0 )
	{
		RemoveFromStartCommands( pMap, nLinkID, &deleted.cascade );
		RemoveFromReservePositions( pMap, nLinkID, &deleted.cascade );
		NoteScriptIDStillNamed( *pMap, deleted.object.nScriptID, &deleted.cascade );
	}
	if ( pDeleted )
		*pDeleted = deleted;
	return true;
}

bool RestoreObject( SLoadMapInfo *pMap, const SDeletedObject &rDeleted )
{
	if ( pMap == 0 || FindObject( pMap, rDeleted.object.link.nLinkID, 0, 0 ) != 0 )
		return false;
	// Reverse order of application: a later erase shifted the positions after it,
	// so the last change is undone first.
	const SCascade &rCascade = rDeleted.cascade;
	for ( size_t i = rCascade.reservePositions.size(); i-- > 0; )
	{
		const SReservePositionChange &rChange = rCascade.reservePositions[i];
		NMapRecords::InsertReservePosition( pMap, int( Min( rChange.nPosition, pMap->reservePositionsList.size() ) ), rChange.before );
	}
	for ( size_t i = rCascade.startCommands.size(); i-- > 0; )
	{
		const SStartCommandChange &rChange = rCascade.startCommands[i];
		const int nPosition = int( Min( rChange.nPosition, pMap->startCommandsList.size() ) );
		if ( rChange.bErased )
			NMapRecords::InsertStartCommand( pMap, nPosition, rChange.before );
		else
			NMapRecords::ReplaceStartCommand( pMap, nPosition, rChange.before );
	}
	std::vector<SMapObjectInfo> &rList = rDeleted.bScenario ? pMap->scenarioObjects : pMap->objects;
	const size_t nIndex = Min( rDeleted.nIndex, rList.size() );
	rList.insert( rList.begin() + nIndex, rDeleted.object );
	return true;
}

void DescribeCascade( const SCascade &rCascade, std::string *pOut )
{
	if ( pOut == 0 )
		return;
	pOut->clear();
	// The positions the player knows are the ones before the delete: an erase
	// shifted every later position down by one, so add back what went before.
	std::vector<int> removedFrom, erased, cleared, reserves;
	int nErased = 0;
	for ( size_t i = 0; i < rCascade.startCommands.size(); ++i )
	{
		const SStartCommandChange &rChange = rCascade.startCommands[i];
		const int nOriginal = int( rChange.nPosition ) + nErased;
		if ( rChange.bErased )
		{
			erased.push_back( nOriginal );
			++nErased;
		}
		else if ( rChange.bUnitRemoved )
			removedFrom.push_back( nOriginal );
		if ( rChange.bTargetCleared && !rChange.bErased )
			cleared.push_back( nOriginal );
	}
	nErased = 0;
	for ( size_t i = 0; i < rCascade.reservePositions.size(); ++i )
	{
		reserves.push_back( int( rCascade.reservePositions[i].nPosition ) + nErased );
		++nErased;
	}
	std::vector<std::string> parts;
	if ( !removedFrom.empty() )
		parts.push_back( std::string( "removed from start command" ) + ( removedFrom.size() > 1 ? "s " : " " ) + JoinNumbers( removedFrom ) );
	if ( !erased.empty() )
		parts.push_back( std::string( "start command" ) + ( erased.size() > 1 ? "s " : " " ) + JoinNumbers( erased ) + " erased" );
	if ( !cleared.empty() )
		parts.push_back( std::string( cleared.size() > 1 ? "targets of start commands " : "target of start command " ) + JoinNumbers( cleared ) + " cleared" );
	if ( !reserves.empty() )
		parts.push_back( std::string( "reserve position" ) + ( reserves.size() > 1 ? "s " : " " ) + JoinNumbers( reserves ) + " erased" );
	for ( size_t i = 0; i < parts.size(); ++i )
		*pOut += ( i == 0 ? "also " : "; " ) + parts[i];
	for ( size_t i = 0; i < rCascade.notes.size(); ++i )
		*pOut += ( pOut->empty() ? "" : "; " ) + rCascade.notes[i];
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

void CaptureRegion( const SLoadMapInfo &rMap, const CTRect<int> &rPatches, SPaintUndo *pOut )
{
	if ( pOut != 0 )
		Record( rMap.terrain, rPatches, pOut );
}
}
