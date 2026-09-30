// The M2 record functions of the editing session: the camera anchors and the
// ground height. One place for every collection the editor edits below the
// object level, so a later plan adds a record kind here beside these.
//
// Each function follows the sound list's contract (session.cpp): the snapshot
// and the working copy are edited together, the engine is left alone unless
// the collection feeds it, a refusal changes nothing, and pbRefused tells a
// refusal - the record's own rules saying no - apart from a failure.
#include "StdAfx.h"
#include <algorithm>
#include <cmath>
#include "session.h"
#include "../MapFile/MapRecords.h"
#include "../Formats/fmtTerrain.h"
#include "../RandomMapGen/VSO_Types.h"

namespace {
// The most anchors the ABI carries. A file with more is read-refused and
// set-refused, and saves byte-exact while nobody edits it.
const int nMaxAnchorSlots = 32;

BkEditorVec3 ToC( const CVec3 &vPos )
{
	BkEditorVec3 out;
	out.x = vPos.x;
	out.y = vPos.y;
	out.z = vPos.z;
	return out;
}

CVec3 FromC( const BkEditorVec3 &rPos )
{
	return CVec3( rPos.x, rPos.y, rPos.z );
}

// True when (x, y) is a point of the map, in world units: the map is
// tiles * fWorldCellSize across in each direction (the size CVertexAltitudeInfo
// itself measures in).
bool OnTheMap( const SEditorSession &rSession, float fX, float fY )
{
	const float fWidth = rSession.working.terrain.tiles.GetSizeX() * fWorldCellSize;
	const float fHeight = rSession.working.terrain.tiles.GetSizeY() * fWorldCellSize;
	return std::isfinite( fX ) && std::isfinite( fY ) && fX >= 0.0f && fY >= 0.0f && fX < fWidth && fY < fHeight;
}

bool IsUnset( const CVec3 &vAnchor )
{
	return vAnchor == VNULL3;
}

// The slot's current value, VNULL3 past the end of the vector: a slot the
// vector does not hold yet reads as unset, which is what padding writes.
CVec3 CurrentSlot( const std::vector<CVec3> &rPlayers, int nSlot )
{
	return nSlot < int( rPlayers.size() ) ? rPlayers[nSlot] : VNULL3;
}
}

bool ReadSessionCameraAnchors( SEditorSession *pSession, BkEditorCameraAnchorRecord *pOut, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	NMapRecords::SCameraAnchors anchors;
	NMapRecords::GetCameraAnchors( pSession->snapshot, &anchors );
	if ( int( anchors.players.size() ) > nMaxAnchorSlots )
	{
		pSession->szMessage = "this map has more camera anchors than the editor edits";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	memset( pOut, 0, sizeof *pOut );
	pOut->neutral = ToC( anchors.vNeutral );
	pOut->player_count = int( anchors.players.size() );
	for ( int i = 0; i < pOut->player_count; ++i )
		pOut->players[i] = ToC( anchors.players[i] );
	return true;
}

bool SetSessionCameraAnchors( SEditorSession *pSession, const BkEditorCameraAnchorRecord &rAnchors, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	// bridge.cpp already turned away a count outside 0..32 as a caller bug;
	// the guard stays because the engine's asserts are compiled out.
	if ( rAnchors.player_count < 0 || rAnchors.player_count > nMaxAnchorSlots )
	{
		pSession->szMessage = "a camera anchor record holds 0 to 32 players";
		return false;
	}
	NMapRecords::SCameraAnchors current;
	NMapRecords::GetCameraAnchors( pSession->snapshot, &current );
	if ( int( current.players.size() ) > nMaxAnchorSlots )
	{
		pSession->szMessage = "this map has more camera anchors than the editor edits";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}

	NMapRecords::SCameraAnchors wanted;
	wanted.vNeutral = FromC( rAnchors.neutral );
	wanted.players.resize( rAnchors.player_count, VNULL3 );
	for ( int i = 0; i < rAnchors.player_count; ++i )
		wanted.players[i] = FromC( rAnchors.players[i] );

	// Only a slot this call changes is checked against the map: an anchor a
	// file already held off the map stays as it is, and must not make every
	// other edit of the vector refuse.
	if ( wanted.vNeutral != current.vNeutral && !IsUnset( wanted.vNeutral ) && !OnTheMap( *pSession, wanted.vNeutral.x, wanted.vNeutral.y ) )
	{
		pSession->szMessage = "the neutral camera anchor is not on the map";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	for ( int i = 0; i < rAnchors.player_count; ++i )
	{
		const CVec3 &rWanted = wanted.players[i];
		if ( rWanted == CurrentSlot( current.players, i ) || IsUnset( rWanted ) )
			continue;
		if ( !OnTheMap( *pSession, rWanted.x, rWanted.y ) )
		{
			pSession->szMessage = NStr::Format( "the camera anchor of player %d is not on the map", i );
			if ( pbRefused != 0 ) *pbRefused = true;
			return false;
		}
	}

	// Both copies together, the engine untouched: the anchors matter only when
	// a mission starts, which this headless session never does.
	if ( !NMapRecords::PutCameraAnchors( &pSession->snapshot, wanted ) )
		return false;
	if ( !NMapRecords::PutCameraAnchors( &pSession->working, wanted ) )
	{
		NMapRecords::PutCameraAnchors( &pSession->snapshot, current );
		return false;
	}
	return true;
}

bool GroundHeightInSession( SEditorSession *pSession, float fX, float fY, float *pfZ )
{
	if ( pSession == 0 || pfZ == 0 )
		return false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	if ( !OnTheMap( *pSession, fX, fY ) )
	{
		pSession->szMessage = "that point is not on the map";
		return false;
	}
	const STerrainInfo::TVertexAltitudeArray2D &rAltitudes = pSession->working.terrain.altitudes;
	if ( rAltitudes.GetSizeX() < 2 || rAltitudes.GetSizeY() < 2 )
	{
		pSession->szMessage = "the map has no terrain heights";
		return false;
	}
	// The same call a road, a river and the MFC editor's own anchors use, so
	// they all agree on the ground.
	CVec3 vPoint( fX, fY, 0.0f );
	if ( !CVSOBuilder::UpdateZ( rAltitudes, &vPoint ) )
	{
		pSession->szMessage = "the terrain height there could not be read";
		return false;
	}
	*pfZ = vPoint.z;
	return true;
}

// ---------------------------------------------------------------------------
// Reinforcement groups (04-09, D-16).
// ---------------------------------------------------------------------------

namespace {
const int nMinScriptID = 0;
const int nMaxScriptID = 32000;

// The script IDs of group nID, or false when there is no such group.
bool FindGroup( const CMapInfo &rMap, int nID, std::vector<int> *pIDs )
{
	std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator it = rMap.reinforcements.groups.find( nID );
	if ( it == rMap.reinforcements.groups.end() )
		return false;
	if ( pIDs != 0 )
		*pIDs = it->second.ids;
	return true;
}

int CountOf( const std::vector<int> &rIDs, int nValue )
{
	return int( std::count( rIDs.begin(), rIDs.end(), nValue ) );
}

// The put's rule (Pitfall 9, T-04-09-01): every script ID the put ADDS is
// 0..32000 and appears once. One the group holds now, or held when the file was
// opened, is exempt - as many times as it held it - so a file's own odd data (a
// duplicate, an ID out of range) can be put back by an undo, whichever edit of
// the group it follows.
bool GroupPutAllowed( SEditorSession *pSession, int nID, const std::vector<int> &rCurrent, const std::vector<int> &rWanted )
{
	std::unordered_map< int, std::vector<int> >::const_iterator itOpened = pSession->openedGroups.find( nID );
	for ( size_t i = 0; i < rWanted.size(); ++i )
	{
		const int nScriptID = rWanted[i];
		const int nWanted = CountOf( rWanted, nScriptID );
		int nHeld = CountOf( rCurrent, nScriptID );
		if ( itOpened != pSession->openedGroups.end() )
			nHeld = Max( nHeld, CountOf( itOpened->second, nScriptID ) );
		if ( nWanted <= nHeld )
			continue;
		if ( nScriptID < nMinScriptID || nScriptID > nMaxScriptID )
		{
			pSession->szMessage = "a script ID in a group is 0..32000";
			return false;
		}
		if ( nWanted > 1 )
		{
			pSession->szMessage = NStr::Format( "script ID %d appears twice in the group", nScriptID );
			return false;
		}
	}
	return true;
}
}

bool ReadSessionGroupIDs( SEditorSession *pSession, int *pOut, int nCapacity, int *pnCount )
{
	std::vector<int> ids;
	for ( std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator it = pSession->snapshot.reinforcements.groups.begin();
	      it != pSession->snapshot.reinforcements.groups.end(); ++it )
		ids.push_back( it->first );
	std::sort( ids.begin(), ids.end() );
	*pnCount = int( ids.size() );
	const int nWrite = Min( nCapacity, int( ids.size() ) );
	for ( int i = 0; i < nWrite; ++i )
		pOut[i] = ids[i];
	return nCapacity >= int( ids.size() );
}

bool ReadSessionGroup( SEditorSession *pSession, int nID, int *pOut, int nCapacity, int *pnCount, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	std::vector<int> ids;
	if ( !FindGroup( pSession->snapshot, nID, &ids ) )
	{
		*pnCount = -1;
		pSession->szMessage = NStr::Format( "there is no reinforcement group %d", nID );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	*pnCount = int( ids.size() );
	const int nWrite = Min( nCapacity, int( ids.size() ) );
	for ( int i = 0; i < nWrite; ++i )
		pOut[i] = ids[i];
	// A buffer too short is the sizing pass of a two-pass read, not a failure
	// worth a message: *pnCount is the total and nothing was written past it.
	if ( nCapacity < int( ids.size() ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	return true;
}

bool SetSessionGroup( SEditorSession *pSession, int nID, const int *pIDs, int nCount, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	std::vector<int> current;
	FindGroup( pSession->snapshot, nID, &current );
	const std::vector<int> wanted( pIDs, pIDs + nCount );
	if ( !GroupPutAllowed( pSession, nID, current, wanted ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	// Both copies together; the engine holds no groups.
	if ( !NMapRecords::PutReinforcementGroup( &pSession->snapshot, nID, wanted ) )
		return false;
	if ( !NMapRecords::PutReinforcementGroup( &pSession->working, nID, wanted ) )
	{
		NMapRecords::PutReinforcementGroup( &pSession->snapshot, nID, current );
		return false;
	}
	return true;
}

bool DeleteSessionGroup( SEditorSession *pSession, int nID, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	std::vector<int> current;
	if ( !FindGroup( pSession->snapshot, nID, &current ) )
	{
		pSession->szMessage = NStr::Format( "there is no reinforcement group %d", nID );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( !NMapRecords::EraseReinforcementGroup( &pSession->snapshot, nID ) )
		return false;
	NMapRecords::EraseReinforcementGroup( &pSession->working, nID );
	return true;
}

int FirstFreeGroupIDInSession( SEditorSession *pSession, int nFrom )
{
	return NMapRecords::FirstFreeGroupID( pSession->snapshot, nFrom );
}
