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
#include <iterator>
#include "session.h"
#include "world.h"
#include "../AILogic/AILogic.h"
#include "../Scene/Scene.h"
#include "../MapFile/MapRecords.h"
#include "../Formats/fmtTerrain.h"
#include "../Formats/fmtMapScriptPath.h"
#include "../RandomMapGen/VSO_Types.h"
#include "../Main/GameDB.h"
#include "../Main/RPGStats.h"

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
	// other edit of the vector refuse. A slot put back to the file's own value
	// at open is exempt too, so the undo of an edit of such an anchor goes
	// through (WR-B03).
	if ( wanted.vNeutral != current.vNeutral && wanted.vNeutral != pSession->vOpenedNeutralAnchor && !IsUnset( wanted.vNeutral ) &&
	     !OnTheMap( *pSession, wanted.vNeutral.x, wanted.vNeutral.y ) )
	{
		pSession->szMessage = "the neutral camera anchor is not on the map";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	for ( int i = 0; i < rAnchors.player_count; ++i )
	{
		const CVec3 &rWanted = wanted.players[i];
		if ( rWanted == CurrentSlot( current.players, i ) || rWanted == CurrentSlot( pSession->openedPlayerAnchors, i ) || IsUnset( rWanted ) )
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

// ---------------------------------------------------------------------------
// The script file (04-10, D-20).
// ---------------------------------------------------------------------------

bool ReadSessionScriptFile( SEditorSession *pSession, BkEditorScriptFileRecord *pOut, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	// The name as a save would write it: a map that still holds the output path of
	// the computer that generated it reads as its script's name, which is also what
	// Test in game and Save As copy by (NMapScriptPath::ToStored).
	const std::string rszName = NMapScriptPath::ToStored( pSession->snapshot.szScriptFile );
	if ( rszName.size() >= sizeof pOut->name )
	{
		pSession->szMessage = "this map's script file name is longer than the editor edits";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	memset( pOut, 0, sizeof *pOut );
	memcpy( pOut->name, rszName.c_str(), rszName.size() );
	return true;
}

bool SetSessionScriptFile( SEditorSession *pSession, const char *pszName, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	const std::string szWanted( pszName );
	// A name NEW to the map is None or a bare name; the value the file held is
	// exempt however odd, so the undo of an edit can bring a verbatim path back
	// (Pitfall 12). Only an empty or bare name can name a file beside the map:
	// the copies made for it are built from a fixed directory plus that name.
	if ( szWanted != pSession->szScriptFileAtOpen && szWanted != NMapScriptPath::ToStored( pSession->szScriptFileAtOpen ) && !NMapRecords::IsBareScriptName( szWanted ) )
	{
		pSession->szMessage = "a script is named without folder or .lua";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	const std::string szCurrent = pSession->snapshot.szScriptFile;
	// Both copies together, the engine untouched: the script loads only when a
	// mission starts, which this headless session never does.
	if ( !NMapRecords::PutScriptFile( &pSession->snapshot, szWanted ) )
		return false;
	if ( !NMapRecords::PutScriptFile( &pSession->working, szWanted ) )
	{
		NMapRecords::PutScriptFile( &pSession->snapshot, szCurrent );
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Script areas (04-10, D-21).
// ---------------------------------------------------------------------------

namespace {
void AreaToC( const SScriptArea &rArea, BkEditorScriptAreaRecord *pOut )
{
	memset( pOut, 0, sizeof *pOut );
	memcpy( pOut->name, rArea.szName.c_str(), rArea.szName.size() );
	pOut->type = int( rArea.eType );
	pOut->cx = rArea.center.x;
	pOut->cy = rArea.center.y;
	pOut->hx = rArea.vAABBHalfSize.x;
	pOut->hy = rArea.vAABBHalfSize.y;
	pOut->r = rArea.fR;
}

// The record as the file's area: every field as given, so a read and a put of the
// same record are exact and an undo puts back what an edit took out.
SScriptArea AreaFromC( const BkEditorScriptAreaRecord &rRecord )
{
	SScriptArea area;
	area.eType = rRecord.type == 0 ? SScriptArea::EAT_RECTANGLE : SScriptArea::EAT_CIRCLE;
	area.szName = rRecord.name;
	area.center = CVec2( rRecord.cx, rRecord.cy );
	area.vAABBHalfSize = CVec2( rRecord.hx, rRecord.hy );
	area.fR = rRecord.r;
	return area;
}

// True when the point (map units) is on the map: tiles * 64 map units across, the
// size OnTheMap measures in world units once converted.
bool OnTheMapInAIUnits( const SEditorSession &rSession, float fX, float fY )
{
	const float fWidth = rSession.working.terrain.tiles.GetSizeX() * fWorldCellSize * fAITileXCoeff1;
	const float fHeight = rSession.working.terrain.tiles.GetSizeY() * fWorldCellSize * fAITileYCoeff1;
	return std::isfinite( fX ) && std::isfinite( fY ) && fX >= 0.0f && fY >= 0.0f && fX < fWidth && fY < fHeight;
}

// The put's rules (T-04-10-05, Pitfall 14): a name, unique among the areas but
// the one being replaced (nIgnoreIndex, -1 for an add), a size that is not
// negative, and a centre on the map when the put moves it. A name the file itself
// held more than once when it was opened is allowed as often as the file held it,
// so an undo can put such an area back beside its twin. Says why in szMessage.
bool SameFloatBits( float fLeft, float fRight )
{
	return memcmp( &fLeft, &fRight, sizeof fLeft ) == 0;
}

// True when rArea is, bit for bit, an area the file held when it was opened.
bool IsOpenedArea( const SEditorSession &rSession, const SScriptArea &rArea )
{
	for ( size_t i = 0; i < rSession.openedAreas.size(); ++i )
	{
		const SScriptArea &rOpened = rSession.openedAreas[i];
		if ( rOpened.szName == rArea.szName && rOpened.eType == rArea.eType &&
		     SameFloatBits( rOpened.center.x, rArea.center.x ) && SameFloatBits( rOpened.center.y, rArea.center.y ) &&
		     SameFloatBits( rOpened.vAABBHalfSize.x, rArea.vAABBHalfSize.x ) && SameFloatBits( rOpened.vAABBHalfSize.y, rArea.vAABBHalfSize.y ) &&
		     SameFloatBits( rOpened.fR, rArea.fR ) )
			return true;
	}
	return false;
}

bool AreaPutAllowed( SEditorSession *pSession, const SScriptArea &rWanted, int nIgnoreIndex, const SScriptArea *pCurrent )
{
	// An area of the file's own, put back exactly (the undo of a delete or an
	// edit), keeps whatever it held: only the name rule below applies to it.
	const bool bOpened = IsOpenedArea( *pSession, rWanted );
	if ( rWanted.szName.empty() && !bOpened )
	{
		pSession->szMessage = "an area needs a name";
		return false;
	}
	if ( !bOpened && ( rWanted.vAABBHalfSize.x < 0.0f || rWanted.vAABBHalfSize.y < 0.0f || rWanted.fR < 0.0f ) )
	{
		pSession->szMessage = "an area's size is not negative";
		return false;
	}
	if ( !NMapRecords::IsAreaNameFree( pSession->snapshot, rWanted.szName, nIgnoreIndex ) )
	{
		int nOthers = 0;
		for ( size_t i = 0; i < pSession->snapshot.scriptAreas.size(); ++i )
			if ( int( i ) != nIgnoreIndex && pSession->snapshot.scriptAreas[i].szName == rWanted.szName )
				++nOthers;
		std::unordered_map<std::string, int>::const_iterator itOpened = pSession->openedAreaNames.find( rWanted.szName );
		const int nOpened = itOpened != pSession->openedAreaNames.end() ? itOpened->second : 0;
		if ( nOthers + 1 > nOpened )
		{
			pSession->szMessage = "an area named " + rWanted.szName + " exists";
			return false;
		}
	}
	const bool bMoved = pCurrent == 0 || pCurrent->center.x != rWanted.center.x || pCurrent->center.y != rWanted.center.y;
	if ( bMoved && !bOpened && !OnTheMapInAIUnits( *pSession, rWanted.center.x, rWanted.center.y ) )
	{
		pSession->szMessage = "the area's centre is not on the map";
		return false;
	}
	return true;
}
}

bool ReadSessionScriptAreas( SEditorSession *pSession, BkEditorScriptAreaRecord *pOut, int nCapacity, int *pnCount, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	const std::vector<SScriptArea> &rAreas = pSession->snapshot.scriptAreas;
	*pnCount = int( rAreas.size() );
	for ( size_t i = 0; i < rAreas.size(); ++i )
		if ( rAreas[i].szName.size() >= sizeof pOut->name )
		{
			pSession->szMessage = NStr::Format( "script area %d's name is longer than the editor edits", int( i ) );
			if ( pbRefused != 0 ) *pbRefused = true;
			return false;
		}
	const int nWrite = Min( nCapacity, int( rAreas.size() ) );
	for ( int i = 0; i < nWrite; ++i )
		AreaToC( rAreas[i], &pOut[i] );
	// A buffer too short is the sizing pass of a two-pass read, not a failure worth
	// a message: *pnCount is the total and nothing was written past it.
	if ( nCapacity < int( rAreas.size() ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	return true;
}

bool AddScriptAreaToSession( SEditorSession *pSession, int nIndex, const BkEditorScriptAreaRecord &rRecord, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	const SScriptArea wanted = AreaFromC( rRecord );
	if ( !AreaPutAllowed( pSession, wanted, -1, 0 ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	// Both copies together; the engine holds no areas.
	if ( !NMapRecords::InsertScriptArea( &pSession->snapshot, nIndex, wanted ) )
		return false;
	if ( !NMapRecords::InsertScriptArea( &pSession->working, nIndex, wanted ) )
	{
		NMapRecords::EraseScriptArea( &pSession->snapshot, nIndex < 0 ? int( pSession->snapshot.scriptAreas.size() ) - 1 : nIndex );
		return false;
	}
	return true;
}

bool SetScriptAreaInSession( SEditorSession *pSession, int nIndex, const BkEditorScriptAreaRecord &rRecord, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( nIndex < 0 || nIndex >= int( pSession->snapshot.scriptAreas.size() ) )
		return false;
	const SScriptArea current = pSession->snapshot.scriptAreas[nIndex];
	const SScriptArea wanted = AreaFromC( rRecord );
	if ( !AreaPutAllowed( pSession, wanted, nIndex, &current ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( !NMapRecords::ReplaceScriptArea( &pSession->snapshot, nIndex, wanted ) )
		return false;
	if ( !NMapRecords::ReplaceScriptArea( &pSession->working, nIndex, wanted ) )
	{
		NMapRecords::ReplaceScriptArea( &pSession->snapshot, nIndex, current );
		return false;
	}
	return true;
}

bool DeleteScriptAreaFromSession( SEditorSession *pSession, int nIndex, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	SScriptArea erased;
	if ( !NMapRecords::EraseScriptArea( &pSession->snapshot, nIndex, &erased ) )
		return false;
	if ( !NMapRecords::EraseScriptArea( &pSession->working, nIndex ) )
	{
		NMapRecords::InsertScriptArea( &pSession->snapshot, nIndex, erased );
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

std::string GroupHoldWarning( const SEditorSession &rSession, int nOnlyLinkID, int nOnlyGroup )
{
	// What names an object: every start command's units, every reserve
	// position's gun and truck.
	struct SNamed { int nLinkID; const char *pszBy; int nIndex; };
	std::vector<SNamed> named;
	int nCommand = 0;
	for ( SLoadMapInfo::TStartCommandsList::const_iterator it = rSession.snapshot.startCommandsList.begin(); it != rSession.snapshot.startCommandsList.end(); ++it, ++nCommand )
		for ( size_t u = 0; u < it->unitLinkIDs.size(); ++u )
		{
			const SNamed entry = { it->unitLinkIDs[u], "start command", nCommand };
			named.push_back( entry );
		}
	int nPosition = 0;
	for ( SLoadMapInfo::TReservePositionsList::const_iterator it = rSession.snapshot.reservePositionsList.begin(); it != rSession.snapshot.reservePositionsList.end(); ++it, ++nPosition )
	{
		const SNamed gun = { it->nArtilleryLinkID, "reserve position", nPosition };
		const SNamed truck = { it->nTruckLinkID, "reserve position", nPosition };
		named.push_back( gun );
		named.push_back( truck );
	}
	const std::vector<SMapObjectInfo> *lists[2] = { &rSession.snapshot.objects, &rSession.snapshot.scenarioObjects };
	for ( size_t n = 0; n < named.size(); ++n )
	{
		const int nLinkID = named[n].nLinkID;
		if ( nLinkID == 0 || ( nOnlyLinkID != 0 && nLinkID != nOnlyLinkID ) )
			continue;
		for ( int nList = 0; nList < 2; ++nList )
			for ( size_t o = 0; o < lists[nList]->size(); ++o )
			{
				const SMapObjectInfo &rObject = (*lists[nList])[o];
				if ( rObject.link.nLinkID != nLinkID || rObject.nScriptID < 0 )
					continue;
				for ( std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator it = rSession.snapshot.reinforcements.groups.begin();
				      it != rSession.snapshot.reinforcements.groups.end(); ++it )
					if ( ( nOnlyGroup < 0 || it->first == nOnlyGroup ) && CountOf( it->second.ids, rObject.nScriptID ) != 0 )
						return NStr::Format( "unit %d, which %s %d names, is held back by reinforcement group %d until a script brings it in; the %s may not find it",
						                     nLinkID, named[n].pszBy, named[n].nIndex, it->first, named[n].pszBy );
			}
	}
	return "";
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
	pSession->szMessage = GroupHoldWarning( *pSession, 0, nID );
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

// ---------------------------------------------------------------------------
// Hide checked (04-09, D-16).
// ---------------------------------------------------------------------------

namespace {
// One AI object's map object out of the scene or back in it. The MFC editor's
// own Hide checked did exactly this (TemplateEditorFrame1.cpp, WM_USER + 7:
// RemoveFromScene, AddToScene), which takes the shadow and the icons with it
// - a visual at opacity 0 left a tank's mesh shadow and its health bar
// standing where the tank had been.
void SetAIObjectHidden( SEditorSession *pSession, IRefCount *pAIObject, bool bHidden )
{
	SMapObject *pMapObject = pSession->pWorld->FindByAI( pAIObject );
	if ( bHidden )
		pSession->pWorld->HideMapObject( pMapObject );
	else
		pSession->pWorld->ShowMapObject( pMapObject );
}

// The engine object of a link ID: one visual, or a squad's soldiers'.
void SetLinkHidden( SEditorSession *pSession, int nLinkID, bool bHidden )
{
	std::unordered_map<int, CPtr<IRefCount> >::const_iterator it = pSession->byLinkID.find( nLinkID );
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	if ( it == pSession->byLinkID.end() || pAIEditor == 0 )
		return;
	IRefCount *pObject = it->second.GetPtr();
	if ( pAIEditor->IsFormation( pObject ) )
	{
		IRefCount **pUnits = 0;
		int nLength = 0;
		pAIEditor->GetUnitsInFormation( pObject, &pUnits, &nLength );
		for ( int i = 0; i < nLength; ++i )
			SetAIObjectHidden( pSession, pUnits[i], bHidden );
	}
	else
		SetAIObjectHidden( pSession, pObject, bHidden );
}
}

void ApplyHiddenMarks( SEditorSession *pSession )
{
	if ( pSession == 0 || pSession->pWorld == 0 )
		return;
	// The entries of the objects list a hidden script ID names, as the game
	// holds them back (LoadUnits reads mapInfo.objects only). Link ID 0 names no
	// one object (C11), so it is not hidden.
	std::vector<int> now;
	if ( !pSession->hiddenScriptIDs.empty() )
	{
		const std::vector<SMapObjectInfo> &rObjects = pSession->snapshot.objects;
		for ( size_t i = 0; i < rObjects.size(); ++i )
			if ( rObjects[i].link.nLinkID != 0 &&
			     std::binary_search( pSession->hiddenScriptIDs.begin(), pSession->hiddenScriptIDs.end(), rObjects[i].nScriptID ) )
				now.push_back( rObjects[i].link.nLinkID );
		std::sort( now.begin(), now.end() );
		now.erase( std::unique( now.begin(), now.end() ), now.end() );
	}
	// Shown again: the ones that were hidden and no longer are.
	for ( size_t i = 0; i < pSession->hiddenLinkIDs.size(); ++i )
		if ( !std::binary_search( now.begin(), now.end(), pSession->hiddenLinkIDs[i] ) )
			SetLinkHidden( pSession, pSession->hiddenLinkIDs[i], false );
	for ( size_t i = 0; i < now.size(); ++i )
		SetLinkHidden( pSession, now[i], true );
	pSession->hiddenLinkIDs = now;
}

void ShowHiddenForUpdate( SEditorSession *pSession )
{
	if ( pSession == 0 || pSession->pWorld == 0 )
		return;
	for ( size_t i = 0; i < pSession->hiddenLinkIDs.size(); ++i )
		SetLinkHidden( pSession, pSession->hiddenLinkIDs[i], false );
}

bool SetSessionHiddenScriptIDs( SEditorSession *pSession, const int *pIDs, int nCount )
{
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession != 0 ) pSession->szMessage = "no map is open";
		return false;
	}
	std::vector<int> ids( pIDs, pIDs + nCount );
	std::sort( ids.begin(), ids.end() );
	ids.erase( std::unique( ids.begin(), ids.end() ), ids.end() );
	pSession->hiddenScriptIDs = ids;
	ApplyHiddenMarks( pSession );
	return true;
}

bool IsHiddenLink( const SEditorSession &rSession, int nLinkID )
{
	return std::binary_search( rSession.hiddenLinkIDs.begin(), rSession.hiddenLinkIDs.end(), nLinkID );
}

// ---------------------------------------------------------------------------
// Start commands (04-11, D-17).
// ---------------------------------------------------------------------------

namespace {
// The most units a command holds through the ABI: no shipped map comes near this,
// and a longer list is a caller's mistake.
const int nMaxStartCommandUnits = 4096;

// CAISCHelper::DEFAULT_ACTION_COMMAND_INDEX: the entry a new command starts at.
const int nDefaultActionCommandIndex = 9;

// The record of a link ID in whichever list holds it, or null. The first one, as
// the game's link table would find it.
const SMapObjectInfo* FindObjectByLink( const CMapInfo &rMap, int nLinkID )
{
	for ( size_t i = 0; i < rMap.objects.size(); ++i )
		if ( rMap.objects[i].link.nLinkID == nLinkID )
			return &rMap.objects[i];
	for ( size_t i = 0; i < rMap.scenarioObjects.size(); ++i )
		if ( rMap.scenarioObjects[i].link.nLinkID == nLinkID )
			return &rMap.scenarioObjects[i];
	return 0;
}

// "" when the link ID can be a unit of a start command: above 0, naming an object
// of the map that is a unit or a squad the database knows. The game hands every
// unit to a group command; a building or a tree there is not what it expects.
std::string WhyNotAStartUnit( const CMapInfo &rMap, int nLinkID )
{
	if ( nLinkID <= 0 )
		return "a start command's unit is a link ID above 0";
	const SMapObjectInfo *pObject = FindObjectByLink( rMap, nLinkID );
	if ( pObject == 0 )
		return NStr::Format( "no object has link ID %d", nLinkID );
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc *pDesc = pObjectsDB != 0 ? pObjectsDB->GetDesc( pObject->szName.c_str() ) : 0;
	if ( pDesc == 0 )
		return NStr::Format( "the object database does not know the type of object %d (%s)", nLinkID, pObject->szName.c_str() );
	if ( pDesc->eGameType != SGVOGT_UNIT && pDesc->eGameType != SGVOGT_SQUAD )
		return NStr::Format( "object %d (%s) is not a unit or a squad", nLinkID, pObject->szName.c_str() );
	return "";
}

bool SameStartCommand( const SAIStartCommand &rLeft, const SAIStartCommand &rRight )
{
	return rLeft.cmdType == rRight.cmdType && rLeft.unitLinkIDs == rRight.unitLinkIDs && rLeft.linkID == rRight.linkID &&
	       rLeft.vPos.x == rRight.vPos.x && rLeft.vPos.y == rRight.vPos.y && rLeft.fromExplosion == rRight.fromExplosion && rLeft.fNumber == rRight.fNumber;
}

bool IsOpenedStartCommand( const SEditorSession &rSession, const SAIStartCommand &rCommand )
{
	for ( size_t i = 0; i < rSession.openedStartCommands.size(); ++i )
		if ( SameStartCommand( rSession.openedStartCommands[i], rCommand ) )
			return true;
	return false;
}

// The command as the file holds it. from_explosion of an add is the record's.
SAIStartCommand StartCommandFromC( const BkEditorStartCommandRecord &rRecord, const int *pUnits )
{
	SAIStartCommand command;
	command.cmdType = EActionCommand( rRecord.cmd_type );
	command.unitLinkIDs.assign( pUnits, pUnits + rRecord.unit_count );
	command.linkID = rRecord.link_id;
	command.vPos = CVec2( rRecord.x, rRecord.y );
	command.fromExplosion = rRecord.from_explosion != 0;
	command.fNumber = rRecord.number;
	return command;
}

void StartCommandToC( const SAIStartCommand &rCommand, BkEditorStartCommandRecord *pOut )
{
	memset( pOut, 0, sizeof *pOut );
	pOut->cmd_type = int( rCommand.cmdType );
	pOut->link_id = rCommand.linkID;
	pOut->x = rCommand.vPos.x;
	pOut->y = rCommand.vPos.y;
	pOut->from_explosion = rCommand.fromExplosion ? 1 : 0;
	pOut->number = rCommand.fNumber;
	pOut->unit_count = int( rCommand.unitLinkIDs.size() );
}

// The command at nIndex of the snapshot's list, or null. The list is a std::list,
// so this walks; a map holds a handful.
const SAIStartCommand* StartCommandAt( const CMapInfo &rMap, int nIndex )
{
	if ( nIndex < 0 || nIndex >= int( rMap.startCommandsList.size() ) )
		return 0;
	SLoadMapInfo::TStartCommandsList::const_iterator it = rMap.startCommandsList.begin();
	std::advance( it, nIndex );
	return &*it;
}

// The message A3 asks for: a unit of the command that the game holds back for a
// reinforcement group, whose start command it may not find when the mission starts.
std::string HeldUnitWarning( const SEditorSession &rSession, const SAIStartCommand &rCommand )
{
	for ( size_t i = 0; i < rCommand.unitLinkIDs.size(); ++i )
	{
		const int nLinkID = rCommand.unitLinkIDs[i];
		for ( size_t o = 0; o < rSession.snapshot.objects.size(); ++o )
		{
			const SMapObjectInfo &rObject = rSession.snapshot.objects[o];
			if ( rObject.link.nLinkID != nLinkID || rObject.nScriptID < 0 )
				continue;
			for ( std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator it = rSession.snapshot.reinforcements.groups.begin();
			      it != rSession.snapshot.reinforcements.groups.end(); ++it )
				if ( CountOf( it->second.ids, rObject.nScriptID ) != 0 )
					return NStr::Format( "unit %d is held back by reinforcement group %d until a script brings it in; its start command may not find it", nLinkID, it->first );
		}
	}
	return "";
}

// The put's rules (T-04-11-02, T-04-11-03): what the set or add CHANGES is judged, so
// a file's own odd command can be edited and an undo can put one back. Says why in
// szMessage.
bool StartCommandAllowed( SEditorSession *pSession, const SAIStartCommand &rWanted, const SAIStartCommand *pCurrent )
{
	if ( IsOpenedStartCommand( *pSession, rWanted ) )
		return true;
	if ( rWanted.unitLinkIDs.empty() )
	{
		pSession->szMessage = "a start command needs at least one unit";
		return false;
	}
	for ( size_t i = 0; i < rWanted.unitLinkIDs.size(); ++i )
	{
		const int nUnit = rWanted.unitLinkIDs[i];
		const int nWanted = CountOf( rWanted.unitLinkIDs, nUnit );
		const int nHeld = pCurrent != 0 ? CountOf( pCurrent->unitLinkIDs, nUnit ) : 0;
		if ( nHeld != 0 && nWanted <= nHeld )
			continue;
		const std::string szWhy = WhyNotAStartUnit( pSession->snapshot, nUnit );
		if ( !szWhy.empty() )
		{
			pSession->szMessage = szWhy;
			return false;
		}
		if ( nWanted > 1 )
		{
			pSession->szMessage = NStr::Format( "unit %d is in the start command twice", nUnit );
			return false;
		}
	}
	if ( rWanted.linkID < 0 && ( pCurrent == 0 || pCurrent->linkID != rWanted.linkID ) )
	{
		pSession->szMessage = "a start command's target is a link ID above 0, or 0 for none";
		return false;
	}
	if ( rWanted.linkID > 0 && ( pCurrent == 0 || pCurrent->linkID != rWanted.linkID ) && FindObjectByLink( pSession->snapshot, rWanted.linkID ) == 0 )
	{
		pSession->szMessage = NStr::Format( "the target object %d is not on the map", rWanted.linkID );
		return false;
	}
	if ( pCurrent == 0 || pCurrent->cmdType != rWanted.cmdType )
	{
		std::vector<SActionCommandEntry> actions;
		std::string szWhy;
		if ( !LoadActionCommands( &actions, &szWhy ) )
		{
			pSession->szMessage = szWhy;
			return false;
		}
		bool bListed = false;
		for ( size_t i = 0; i < actions.size(); ++i )
			bListed = bListed || actions[i].nID == int( rWanted.cmdType );
		if ( !bListed )
		{
			pSession->szMessage = NStr::Format( "%d is not an action type Data\\Editor\\actions.ini lists", int( rWanted.cmdType ) );
			return false;
		}
	}
	const bool bMoved = pCurrent == 0 || pCurrent->vPos.x != rWanted.vPos.x || pCurrent->vPos.y != rWanted.vPos.y;
	if ( bMoved && !OnTheMapInAIUnits( *pSession, rWanted.vPos.x, rWanted.vPos.y ) )
	{
		pSession->szMessage = "the start command's target point is not on the map";
		return false;
	}
	return true;
}
}

// The action types of an actions.ini text the way the MFC editor's table read them
// (CIniFile::LoadTables, which CAISCHelper::Initialize goes through): lines trimmed,
// ';' comments and blank lines skipped, the first row only, an entry with no value
// dropped, a name listed again keeping its first place and taking its last value,
// each value read as atoi does. The game's StreamIO port has no ini table to ask
// (OpenIniDataTable answers nothing there), so the text is read here.
static void ParseActionsIni( const std::string &szText, std::vector<SActionCommandEntry> *pOut )
{
	pOut->clear();
	int nRows = 0;
	size_t nAt = 0;
	while ( nAt <= szText.size() )
	{
		size_t nEnd = szText.find( '\n', nAt );
		if ( nEnd == std::string::npos )
			nEnd = szText.size();
		std::string szLine = szText.substr( nAt, nEnd - nAt );
		nAt = nEnd + 1;
		NStr::TrimBoth( szLine );
		if ( szLine.empty() || szLine[0] == ';' )
			continue;
		if ( szLine[0] == '[' && szLine[szLine.size() - 1] == ']' )
		{
			++nRows;
			continue;
		}
		if ( nRows != 1 )
			continue;
		const size_t nEquals = szLine.find( '=' );
		if ( nEquals == std::string::npos )
			continue;
		std::string szName = szLine.substr( 0, nEquals ), szValue = szLine.substr( nEquals + 1 );
		NStr::TrimBoth( szName );
		NStr::TrimBoth( szValue );
		if ( szName.empty() || szValue.empty() )
			continue;
		const int nID = atoi( szValue.c_str() );
		size_t nKnown = 0;
		while ( nKnown < pOut->size() && ( *pOut )[nKnown].szName != szName )
			++nKnown;
		if ( nKnown < pOut->size() )
			( *pOut )[nKnown].nID = nID;
		else
		{
			SActionCommandEntry entry;
			entry.szName = szName;
			entry.nID = nID;
			pOut->push_back( entry );
		}
	}
}

bool LoadActionCommands( std::vector<SActionCommandEntry> *pOut, std::string *pszWhy )
{
	pOut->clear();
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 )
	{
		*pszWhy = "the engine is not started";
		return false;
	}
	CPtr<IDataStream> pStream = pStorage->OpenStream( "editor\\actions.ini", STREAM_ACCESS_READ );
	if ( pStream == 0 )
	{
		*pszWhy = "the action list Data\\Editor\\actions.ini is not in the data";
		return false;
	}
	const int nSize = pStream->GetSize();
	std::string szText;
	if ( nSize > 0 )
	{
		szText.resize( nSize );
		szText.resize( pStream->Read( &szText[0], nSize ) );
	}
	ParseActionsIni( szText, pOut );
	if ( pOut->empty() )
	{
		*pszWhy = "Data\\Editor\\actions.ini lists no action types";
		return false;
	}
	return true;
}

bool ReadSessionActionCommands( SEditorSession *pSession, BkEditorActionCommand *pOut, int nCapacity, int *pnCount, int *pnDefaultIndex, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	*pnCount = 0;
	*pnDefaultIndex = 0;
	std::vector<SActionCommandEntry> actions;
	if ( !LoadActionCommands( &actions, &pSession->szMessage ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	*pnCount = int( actions.size() );
	*pnDefaultIndex = int( actions.size() ) > nDefaultActionCommandIndex ? nDefaultActionCommandIndex : int( actions.size() ) - 1;
	const int nWrite = Min( nCapacity, int( actions.size() ) );
	for ( int i = 0; i < nWrite; ++i )
	{
		memset( &pOut[i], 0, sizeof pOut[i] );
		const size_t nLength = Min( actions[i].szName.size(), sizeof pOut[i].name - 1 );
		memcpy( pOut[i].name, actions[i].szName.c_str(), nLength );
		pOut[i].id = actions[i].nID;
	}
	if ( nCapacity < int( actions.size() ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	return true;
}

bool ReadSessionStartCommand( SEditorSession *pSession, int nIndex, BkEditorStartCommandRecord *pOut, int *pUnits, int nUnitCapacity, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	const SAIStartCommand *pCommand = StartCommandAt( pSession->snapshot, nIndex );
	if ( pCommand == 0 )
		return false;
	StartCommandToC( *pCommand, pOut );
	const int nWrite = Min( nUnitCapacity, int( pCommand->unitLinkIDs.size() ) );
	for ( int i = 0; i < nWrite; ++i )
		pUnits[i] = pCommand->unitLinkIDs[i];
	// A buffer too short is the sizing pass of a two-pass read, not a failure worth
	// a message: pOut->unit_count is the total and nothing was written past it.
	if ( nUnitCapacity < int( pCommand->unitLinkIDs.size() ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	return true;
}

bool AddStartCommandToSession( SEditorSession *pSession, int nIndex, const BkEditorStartCommandRecord &rRecord, const int *pUnits, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	const SAIStartCommand wanted = StartCommandFromC( rRecord, pUnits );
	if ( !StartCommandAllowed( pSession, wanted, 0 ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	// Both copies together; the engine holds no start commands until a mission starts.
	if ( !NMapRecords::InsertStartCommand( &pSession->snapshot, nIndex, wanted ) )
		return false;
	if ( !NMapRecords::InsertStartCommand( &pSession->working, nIndex, wanted ) )
	{
		NMapRecords::EraseStartCommand( &pSession->snapshot, nIndex < 0 ? int( pSession->snapshot.startCommandsList.size() ) - 1 : nIndex );
		return false;
	}
	pSession->szMessage = HeldUnitWarning( *pSession, wanted );
	return true;
}

bool SetStartCommandInSession( SEditorSession *pSession, int nIndex, const BkEditorStartCommandRecord &rRecord, const int *pUnits, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	const SAIStartCommand *pHeld = StartCommandAt( pSession->snapshot, nIndex );
	if ( pHeld == 0 )
		return false;
	const SAIStartCommand current = *pHeld;
	SAIStartCommand wanted = StartCommandFromC( rRecord, pUnits );
	// D-17: the explosion flag is the file's; a set never changes it.
	wanted.fromExplosion = current.fromExplosion;
	if ( !StartCommandAllowed( pSession, wanted, &current ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( !NMapRecords::ReplaceStartCommand( &pSession->snapshot, nIndex, wanted ) )
		return false;
	if ( !NMapRecords::ReplaceStartCommand( &pSession->working, nIndex, wanted ) )
	{
		NMapRecords::ReplaceStartCommand( &pSession->snapshot, nIndex, current );
		return false;
	}
	pSession->szMessage = HeldUnitWarning( *pSession, wanted );
	return true;
}

bool DeleteStartCommandFromSession( SEditorSession *pSession, int nIndex, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	SAIStartCommand erased;
	if ( !NMapRecords::EraseStartCommand( &pSession->snapshot, nIndex, &erased ) )
		return false;
	if ( !NMapRecords::EraseStartCommand( &pSession->working, nIndex ) )
	{
		NMapRecords::InsertStartCommand( &pSession->snapshot, nIndex, erased );
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Reserve positions (04-11, D-18).
// ---------------------------------------------------------------------------

namespace {
// The stats of a mechanical unit by object name, or null: the object is not a
// unit, the database does not know it, or its stats are not a mechanical unit's (a
// soldier's). A dynamic_cast, as the MFC editor takes it: the typed stats lookup
// casts without looking in a build with its asserts compiled out.
const SMechUnitRPGStats* MechStatsOf( const char *pszName )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 || pszName == 0 )
		return 0;
	const SGDBObjectDesc *pDesc = pObjectsDB->GetDesc( pszName );
	if ( pDesc == 0 || pDesc->eGameType != SGVOGT_UNIT )
		return 0;
	return dynamic_cast<const SMechUnitRPGStats*>( pObjectsDB->GetRPGStats( pDesc ) );
}

// True for a squad: the soldiers of a formation are the game's to place, and a
// reserve position casts its link to a unit.
bool IsSquadName( const char *pszName )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc *pDesc = pObjectsDB != 0 && pszName != 0 ? pObjectsDB->GetDesc( pszName ) : 0;
	return pDesc != 0 && pDesc->eGameType == SGVOGT_SQUAD;
}

bool SameReservePosition( const SBattlePosition &rLeft, const SBattlePosition &rRight )
{
	return rLeft.nArtilleryLinkID == rRight.nArtilleryLinkID && rLeft.nTruckLinkID == rRight.nTruckLinkID && rLeft.vPos.x == rRight.vPos.x && rLeft.vPos.y == rRight.vPos.y;
}

bool IsOpenedReservePosition( const SEditorSession &rSession, const SBattlePosition &rPosition )
{
	for ( size_t i = 0; i < rSession.openedReservePositions.size(); ++i )
		if ( SameReservePosition( rSession.openedReservePositions[i], rPosition ) )
			return true;
	return false;
}

SBattlePosition ReservePositionFromC( const BkEditorReservePositionRecord &rRecord )
{
	return SBattlePosition( rRecord.artillery_link_id, rRecord.truck_link_id, CVec2( rRecord.x, rRecord.y ) );
}

const SBattlePosition* ReservePositionAt( const CMapInfo &rMap, int nIndex )
{
	if ( nIndex < 0 || nIndex >= int( rMap.reservePositionsList.size() ) )
		return 0;
	SLoadMapInfo::TReservePositionsList::const_iterator it = rMap.reservePositionsList.begin();
	std::advance( it, nIndex );
	return &*it;
}

// Why the object of nLinkID cannot be a gun (bGun) or a truck: "" when it can, and
// then *ppStats its mechanical stats and *pnRole its role. The messages name the
// object and what is wrong, the way the MFC editor's click order silently ignored it.
std::string WhyNotInReservePosition( const CMapInfo &rMap, int nLinkID, bool bGun, int *pnRole, const SMechUnitRPGStats **ppStats )
{
	const char *pszWho = bGun ? "artillery" : "truck";
	const SMapObjectInfo *pObject = FindObjectByLink( rMap, nLinkID );
	if ( pObject == 0 )
		return NStr::Format( "no object has link ID %d", nLinkID );
	if ( IsSquadName( pObject->szName.c_str() ) )
		return NStr::Format( "object %d (%s) is a squad, and a squad cannot be %s: a reserve position names a single unit", nLinkID, pObject->szName.c_str(), pszWho );
	const SMechUnitRPGStats *pStats = MechStatsOf( pObject->szName.c_str() );
	if ( pStats == 0 )
		return NStr::Format( "object %d (%s) is not a vehicle or a gun, so it cannot be %s", nLinkID, pObject->szName.c_str(), pszWho );
	*pnRole = ReserveRoleOfName( pObject->szName.c_str() );
	*ppStats = pStats;
	if ( bGun && *pnRole != 1 && *pnRole != 2 )
		return NStr::Format( "object %d (%s) is not artillery: a reserve position holds a self-propelled or a towed gun", nLinkID, pObject->szName.c_str() );
	if ( !bGun && *pnRole != 3 )
		return NStr::Format( "object %d (%s) is not a truck that can tow", nLinkID, pObject->szName.c_str() );
	return "";
}

// T-04-11-01. What the put CHANGES is judged: the gun and the truck together when
// either changes (the roles, the MFC towing check), the place when it moves - so a
// file's own odd position can be moved and put back, and a position the map held when
// it was opened is always accepted back (an undo of a delete). Says why in szMessage.
bool ValidateReservePosition( SEditorSession *pSession, const SBattlePosition &rWanted, const SBattlePosition *pCurrent )
{
	if ( IsOpenedReservePosition( *pSession, rWanted ) )
		return true;
	const bool bRolesChanged = pCurrent == 0 || pCurrent->nArtilleryLinkID != rWanted.nArtilleryLinkID || pCurrent->nTruckLinkID != rWanted.nTruckLinkID;
	if ( bRolesChanged )
	{
		if ( rWanted.nArtilleryLinkID <= 0 )
		{
			pSession->szMessage = rWanted.nTruckLinkID == 0 ? "a reserve position needs a gun" : "the gun's link ID is above 0: link ID 0 names no gun";
			return false;
		}
		if ( rWanted.nTruckLinkID < 0 )
		{
			pSession->szMessage = "a truck's link ID is above 0, or 0 for none";
			return false;
		}
		int nGunRole = 0;
		const SMechUnitRPGStats *pGun = 0;
		std::string szWhy = WhyNotInReservePosition( pSession->snapshot, rWanted.nArtilleryLinkID, true, &nGunRole, &pGun );
		if ( !szWhy.empty() )
		{
			pSession->szMessage = szWhy;
			return false;
		}
		if ( rWanted.nTruckLinkID == 0 )
		{
			if ( nGunRole == 2 )
			{
				pSession->szMessage = "a towed gun needs a truck";
				return false;
			}
		}
		else
		{
			if ( nGunRole != 2 )
			{
				pSession->szMessage = "a self-propelled gun takes no truck";
				return false;
			}
			int nTruckRole = 0;
			const SMechUnitRPGStats *pTruck = 0;
			szWhy = WhyNotInReservePosition( pSession->snapshot, rWanted.nTruckLinkID, false, &nTruckRole, &pTruck );
			if ( !szWhy.empty() )
			{
				pSession->szMessage = szWhy;
				return false;
			}
			// The MFC editor's towing check (ObjectPlacerState.cpp): the truck pulls more
			// than the gun weighs.
			if ( !( pTruck->fTowingForce > pGun->fWeight ) )
			{
				pSession->szMessage = NStr::Format( "the truck %d cannot tow the gun %d: it pulls %.0f and the gun weighs %.0f", rWanted.nTruckLinkID, rWanted.nArtilleryLinkID, pTruck->fTowingForce, pGun->fWeight );
				return false;
			}
		}
	}
	const bool bMoved = pCurrent == 0 || pCurrent->vPos.x != rWanted.vPos.x || pCurrent->vPos.y != rWanted.vPos.y;
	if ( bMoved && !OnTheMapInAIUnits( *pSession, rWanted.vPos.x, rWanted.vPos.y ) )
	{
		pSession->szMessage = "the reserve position is not on the map";
		return false;
	}
	return true;
}
}

int ReserveRoleOfName( const char *pszName )
{
	const SMechUnitRPGStats *pStats = MechStatsOf( pszName );
	if ( pStats == 0 )
		return 0;
	// The classes the MFC editor's click order (ObjectPlacerState.cpp) and its
	// SaveReservePosition tell apart: a carrier or a tractor is a truck; an artillery
	// piece with crew places is towed and one without sits on its own wheels or
	// tracks; a self-propelled or armoured unit and a super train are guns that drive.
	if ( pStats->type == RPG_TYPE_TRN_CARRIER || pStats->type == RPG_TYPE_TRN_TRACTOR )
		return 3;
	if ( IsArtillery( pStats->type ) )
		return pStats->vPeoplePoints.empty() ? 1 : 2;
	if ( IsSPG( pStats->type ) || IsArmor( pStats->type ) || pStats->type == RPG_TYPE_TRAIN_SUPER )
		return 1;
	return 0;
}

bool ReadSessionReservePosition( SEditorSession *pSession, int nIndex, BkEditorReservePositionRecord *pOut )
{
	const SBattlePosition *pPosition = ReservePositionAt( pSession->snapshot, nIndex );
	if ( pPosition == 0 )
		return false;
	memset( pOut, 0, sizeof *pOut );
	pOut->artillery_link_id = pPosition->nArtilleryLinkID;
	pOut->truck_link_id = pPosition->nTruckLinkID;
	pOut->x = pPosition->vPos.x;
	pOut->y = pPosition->vPos.y;
	return true;
}

bool AddReservePositionToSession( SEditorSession *pSession, int nIndex, const BkEditorReservePositionRecord &rRecord, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	const SBattlePosition wanted = ReservePositionFromC( rRecord );
	if ( !ValidateReservePosition( pSession, wanted, 0 ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	// Both copies together; the engine holds no reserve positions until a mission starts.
	if ( !NMapRecords::InsertReservePosition( &pSession->snapshot, nIndex, wanted ) )
		return false;
	if ( !NMapRecords::InsertReservePosition( &pSession->working, nIndex, wanted ) )
	{
		NMapRecords::EraseReservePosition( &pSession->snapshot, nIndex < 0 ? int( pSession->snapshot.reservePositionsList.size() ) - 1 : nIndex );
		return false;
	}
	return true;
}

bool SetReservePositionInSession( SEditorSession *pSession, int nIndex, const BkEditorReservePositionRecord &rRecord, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	const SBattlePosition *pHeld = ReservePositionAt( pSession->snapshot, nIndex );
	if ( pHeld == 0 )
		return false;
	const SBattlePosition current = *pHeld;
	const SBattlePosition wanted = ReservePositionFromC( rRecord );
	if ( !ValidateReservePosition( pSession, wanted, &current ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( !NMapRecords::ReplaceReservePosition( &pSession->snapshot, nIndex, wanted ) )
		return false;
	if ( !NMapRecords::ReplaceReservePosition( &pSession->working, nIndex, wanted ) )
	{
		NMapRecords::ReplaceReservePosition( &pSession->snapshot, nIndex, current );
		return false;
	}
	return true;
}

bool DeleteReservePositionFromSession( SEditorSession *pSession, int nIndex, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	SBattlePosition erased;
	if ( !NMapRecords::EraseReservePosition( &pSession->snapshot, nIndex, &erased ) )
		return false;
	if ( !NMapRecords::EraseReservePosition( &pSession->working, nIndex ) )
	{
		NMapRecords::InsertReservePosition( &pSession->snapshot, nIndex, erased );
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// The AI general (04-12, D-19).
// ---------------------------------------------------------------------------

namespace {
bool SameBits( float fLeft, float fRight )
{
	return memcmp( &fLeft, &fRight, sizeof fLeft ) == 0;
}

// Two parcels are the same when every number is, bit for bit: the file's own data is
// compared as it is, NaN included, so a parcel that came from the file is always found.
bool SameParcel( const SAIGeneralParcelInfo &rLeft, const SAIGeneralParcelInfo &rRight )
{
	if ( rLeft.eType != rRight.eType || !SameBits( rLeft.vCenter.x, rRight.vCenter.x ) || !SameBits( rLeft.vCenter.y, rRight.vCenter.y ) ||
	     !SameBits( rLeft.fRadius, rRight.fRadius ) || rLeft.wDefenceDirection != rRight.wDefenceDirection ||
	     rLeft.reinforcePoints.size() != rRight.reinforcePoints.size() )
		return false;
	for ( size_t i = 0; i < rLeft.reinforcePoints.size(); ++i )
	{
		const SAIGeneralParcelInfo::SReinforcePointInfo &rLeftPoint = rLeft.reinforcePoints[i];
		const SAIGeneralParcelInfo::SReinforcePointInfo &rRightPoint = rRight.reinforcePoints[i];
		if ( !SameBits( rLeftPoint.vCenter.x, rRightPoint.vCenter.x ) || !SameBits( rLeftPoint.vCenter.y, rRightPoint.vCenter.y ) || rLeftPoint.wDir != rRightPoint.wDir )
			return false;
	}
	return true;
}

bool HoldsParcel( const SAIGeneralSideInfo &rSide, const SAIGeneralParcelInfo &rParcel )
{
	for ( size_t i = 0; i < rSide.parcels.size(); ++i )
		if ( SameParcel( rSide.parcels[i], rParcel ) )
			return true;
	return false;
}

const SAIGeneralSideInfo* SideAt( const std::vector<SAIGeneralSideInfo> &rSides, int nSide )
{
	return nSide >= 0 && nSide < int( rSides.size() ) ? &rSides[nSide] : 0;
}

// The side as the snapshot holds it now, empty for one it does not have.
SAIGeneralSideInfo CurrentSide( const SEditorSession &rSession, int nSide )
{
	const SAIGeneralSideInfo *pSide = SideAt( rSession.snapshot.aiGeneralMapInfo.sidesInfo, nSide );
	return pSide != 0 ? *pSide : SAIGeneralSideInfo();
}

// T-04-12-01. The put's rules: what it ADDS is judged - a type 1 or 2, a radius above 0,
// a centre on the map, numbers that are numbers, and mobile script IDs 0..32000 that appear
// once. A parcel the side holds now or held when the file was opened is exempt, and so is a
// script ID as often as either held it, so an undo can put a file's own odd data back.
// Says why in szMessage.
bool ValidateAISide( SEditorSession *pSession, int nSide, const SAIGeneralSideInfo &rWanted )
{
	const SAIGeneralSideInfo current = CurrentSide( *pSession, nSide );
	const SAIGeneralSideInfo *pOpened = SideAt( pSession->openedAISides, nSide );
	for ( size_t i = 0; i < rWanted.parcels.size(); ++i )
	{
		const SAIGeneralParcelInfo &rParcel = rWanted.parcels[i];
		if ( HoldsParcel( current, rParcel ) || ( pOpened != 0 && HoldsParcel( *pOpened, rParcel ) ) )
			continue;
		if ( rParcel.eType != SAIGeneralParcelInfo::EPATCH_DEFENCE && rParcel.eType != SAIGeneralParcelInfo::EPATCH_REINFORCE )
		{
			pSession->szMessage = NStr::Format( "parcel %d is of type %d: a parcel is a defence (1) or a reinforce (2) parcel", int( i ), rParcel.eType );
			return false;
		}
		if ( !std::isfinite( rParcel.fRadius ) || !( rParcel.fRadius > 0.0f ) )
		{
			pSession->szMessage = NStr::Format( "parcel %d needs a radius above 0", int( i ) );
			return false;
		}
		if ( !OnTheMapInAIUnits( *pSession, rParcel.vCenter.x, rParcel.vCenter.y ) )
		{
			pSession->szMessage = NStr::Format( "parcel %d is not on the map", int( i ) );
			return false;
		}
		for ( size_t j = 0; j < rParcel.reinforcePoints.size(); ++j )
		{
			const CVec2 &rPoint = rParcel.reinforcePoints[j].vCenter;
			if ( !std::isfinite( rPoint.x ) || !std::isfinite( rPoint.y ) )
			{
				pSession->szMessage = NStr::Format( "point %d of parcel %d is not a number", int( j ), int( i ) );
				return false;
			}
		}
	}
	for ( size_t i = 0; i < rWanted.mobileScriptIDs.size(); ++i )
	{
		const int nScriptID = rWanted.mobileScriptIDs[i];
		const int nWanted = CountOf( rWanted.mobileScriptIDs, nScriptID );
		int nHeld = CountOf( current.mobileScriptIDs, nScriptID );
		if ( pOpened != 0 )
			nHeld = Max( nHeld, CountOf( pOpened->mobileScriptIDs, nScriptID ) );
		if ( nWanted <= nHeld )
			continue;
		if ( nScriptID < nMinScriptID || nScriptID > nMaxScriptID )
		{
			pSession->szMessage = "a mobile script ID is 0..32000";
			return false;
		}
		if ( nWanted > 1 )
		{
			pSession->szMessage = NStr::Format( "script ID %d appears twice in the mobile list", nScriptID );
			return false;
		}
	}
	return true;
}

// The side the flattened arrays describe. Every point range was checked by the caller.
SAIGeneralSideInfo SideFromC( const int *pnMobile, int nMobileCount, const BkEditorAIParcel *pParcels, int nParcelCount, const BkEditorAIPoint *pPoints )
{
	SAIGeneralSideInfo side;
	side.mobileScriptIDs.assign( pnMobile, pnMobile + nMobileCount );
	side.parcels.resize( nParcelCount );
	for ( int i = 0; i < nParcelCount; ++i )
	{
		SAIGeneralParcelInfo &rParcel = side.parcels[i];
		rParcel.eType = pParcels[i].type;
		rParcel.vCenter = CVec2( pParcels[i].cx, pParcels[i].cy );
		rParcel.fRadius = pParcels[i].radius;
		rParcel.wDefenceDirection = WORD( pParcels[i].defence_dir );
		rParcel.reinforcePoints.resize( pParcels[i].point_count );
		for ( int j = 0; j < pParcels[i].point_count; ++j )
		{
			const BkEditorAIPoint &rPoint = pPoints[pParcels[i].first_point + j];
			rParcel.reinforcePoints[j] = SAIGeneralParcelInfo::SReinforcePointInfo( CVec2( rPoint.x, rPoint.y ), WORD( rPoint.dir ) );
		}
	}
	return side;
}
}

bool ReadSessionAIGeneralSide( SEditorSession *pSession, int nSide, BkEditorAISideInfo *pInfo, int *pnMobile, int nMobileCap, BkEditorAIParcel *pParcels, int nParcelCap, BkEditorAIPoint *pPoints, int nPointCap )
{
	const SAIGeneralSideInfo side = CurrentSide( *pSession, nSide );
	memset( pInfo, 0, sizeof *pInfo );
	pInfo->side_count = int( pSession->snapshot.aiGeneralMapInfo.sidesInfo.size() );
	pInfo->mobile_count = int( side.mobileScriptIDs.size() );
	pInfo->parcel_count = int( side.parcels.size() );
	int nPoints = 0;
	for ( size_t i = 0; i < side.parcels.size(); ++i )
		nPoints += int( side.parcels[i].reinforcePoints.size() );
	pInfo->point_count = nPoints;
	for ( int i = 0; i < nMobileCap && i < pInfo->mobile_count; ++i )
		pnMobile[i] = side.mobileScriptIDs[i];
	int nFirst = 0;
	for ( int i = 0; i < pInfo->parcel_count; ++i )
	{
		const SAIGeneralParcelInfo &rParcel = side.parcels[i];
		if ( i < nParcelCap )
		{
			BkEditorAIParcel &rOut = pParcels[i];
			memset( &rOut, 0, sizeof rOut );
			rOut.type = rParcel.eType;
			rOut.cx = rParcel.vCenter.x;
			rOut.cy = rParcel.vCenter.y;
			rOut.radius = rParcel.fRadius;
			rOut.defence_dir = int( rParcel.wDefenceDirection );
			rOut.first_point = nFirst;
			rOut.point_count = int( rParcel.reinforcePoints.size() );
		}
		for ( size_t j = 0; j < rParcel.reinforcePoints.size(); ++j )
		{
			if ( nFirst + int( j ) < nPointCap )
			{
				BkEditorAIPoint &rPoint = pPoints[nFirst + j];
				rPoint.x = rParcel.reinforcePoints[j].vCenter.x;
				rPoint.y = rParcel.reinforcePoints[j].vCenter.y;
				rPoint.dir = int( rParcel.reinforcePoints[j].wDir );
			}
		}
		nFirst += int( rParcel.reinforcePoints.size() );
	}
	return nMobileCap >= pInfo->mobile_count && nParcelCap >= pInfo->parcel_count && nPointCap >= pInfo->point_count;
}

bool SetSessionAIGeneralSide( SEditorSession *pSession, int nSide, int nSideCount, const int *pnMobile, int nMobileCount, const BkEditorAIParcel *pParcels, int nParcelCount, const BkEditorAIPoint *pPoints, int nPointCount, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	const SAIGeneralSideInfo wanted = SideFromC( pnMobile, nMobileCount, pParcels, nParcelCount, pPoints );
	if ( nSide >= nSideCount && ( nMobileCount != 0 || nParcelCount != 0 ) )
	{
		pSession->szMessage = NStr::Format( "side %d is not one of the map's %d sides, so it holds nothing", nSide, nSideCount );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( nSide < nSideCount && !ValidateAISide( pSession, nSide, wanted ) )
	{
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	// A smaller count drops the sides above it. That is how an undo takes back
	// the sides a put created - which are empty - so a side above the count
	// that holds anything, other than the side this put names, is refused
	// rather than dropped.
	const std::vector<SAIGeneralSideInfo> &rSides = pSession->snapshot.aiGeneralMapInfo.sidesInfo;
	for ( int i = nSideCount; i < int( rSides.size() ); ++i )
		if ( i != nSide && ( !rSides[i].mobileScriptIDs.empty() || !rSides[i].parcels.empty() ) )
		{
			pSession->szMessage = NStr::Format( "side %d holds parcels or script IDs, so the side count cannot drop to %d", i, nSideCount );
			if ( pbRefused != 0 ) *pbRefused = true;
			return false;
		}
	NMapRecords::SAIGeneralSidePut before;
	NMapRecords::GetAIGeneralSide( pSession->snapshot, nSide, &before );
	NMapRecords::SAIGeneralSidePut put;
	put.nSideCount = nSideCount;
	put.nSide = nSide;
	put.info = wanted;
	// Both copies together; the engine holds no AI general until a mission starts. A put
	// that fails on the second copy puts the old count and the old side back on the first.
	if ( !NMapRecords::PutAIGeneralSide( &pSession->snapshot, put ) )
		return false;
	if ( !NMapRecords::PutAIGeneralSide( &pSession->working, put ) )
	{
		NMapRecords::PutAIGeneralSide( &pSession->snapshot, before );
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Players and the Unit Creation Info (M3, D-30)
// ---------------------------------------------------------------------------

namespace {
// The most unit-creation entries and appear points the ABI's record carries.
const int nMaxUnitCreationSlots = 16;
const int nMaxAppearPoints = 32;
const int nMaxFormationSize = 32;
const int nMaxPlaneCount = 255;

const char *const UC_AIRCRAFT_TYPE_NAMES[5] = { "Scouts", "Fighters", "Paradropers", "Bombers", "Attack planes" };

// The state of the map a player edit changes besides the owners: the table, the
// unit creation and the player camera anchors, held whole so an undo puts back
// the exact vectors (their sizes included) the file had.
struct SPlayersState
{
	std::vector<BYTE> diplomacies;
	std::vector<SUnitCreation> units;
	std::vector<CVec3> anchors;
};

// One object whose owner (and, for a flag, whose type name) an edit changed.
// The place is the list and the index in it: both copies hold their objects in
// the same order, and an undo happens with the map exactly as the edit left it.
struct SOwnerChange
{
	int nList;
	int nIndex;
	int nLinkID;
	int nOwnerBefore, nOwnerAfter;
	std::string szNameBefore, szNameAfter;
	SOwnerChange() : nList( 0 ), nIndex( 0 ), nLinkID( 0 ), nOwnerBefore( 0 ), nOwnerAfter( 0 ) {  }
};

std::vector<SMapObjectInfo>* ObjectList( CMapInfo *pMap, int nList )
{
	return nList == 0 ? &pMap->objects : &pMap->scenarioObjects;
}

void CapturePlayersState( const CMapInfo &rMap, SPlayersState *pState )
{
	pState->diplomacies = rMap.diplomacies;
	pState->units = rMap.unitCreation.units;
	pState->anchors = rMap.playersCameraAnchors;
}

void PutPlayersStateRaw( CMapInfo *pMap, const SPlayersState &rState )
{
	pMap->diplomacies = rState.diplomacies;
	pMap->unitCreation.units = rState.units;
	pMap->playersCameraAnchors = rState.anchors;
}

// Both copies hold the member where the change names it, as the same object.
bool MemberIsThere( SEditorSession *pSession, const SOwnerChange &rMember )
{
	CMapInfo *maps[2] = { &pSession->snapshot, &pSession->working };
	for ( int i = 0; i < 2; ++i )
	{
		std::vector<SMapObjectInfo> *pList = ObjectList( maps[i], rMember.nList );
		if ( rMember.nIndex < 0 || rMember.nIndex >= int( pList->size() ) || (*pList)[rMember.nIndex].link.nLinkID != rMember.nLinkID )
			return false;
	}
	return true;
}

void PutOwnerRaw( CMapInfo *pMap, const SOwnerChange &rMember, bool bAfter )
{
	SMapObjectInfo &rObject = (*ObjectList( pMap, rMember.nList ))[rMember.nIndex];
	rObject.nPlayer = bAfter ? rMember.nOwnerAfter : rMember.nOwnerBefore;
	rObject.szName = bAfter ? rMember.szNameAfter : rMember.szNameBefore;
}

// The records of the members, both copies, and the engine's owner of each:
// a flag's owner is its type (the engine holds flags unowned, as
// PutObjectRecordBack knows), anything else is handed the new index.
bool PutOwners( SEditorSession *pSession, const std::vector<SOwnerChange> &rMembers, bool bAfter )
{
	for ( size_t i = 0; i < rMembers.size(); ++i )
		if ( !MemberIsThere( pSession, rMembers[i] ) )
		{
			pSession->szMessage = "the objects no longer match the player edit; reopen the map";
			return false;
		}
	IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	for ( size_t i = 0; i < rMembers.size(); ++i )
	{
		const SOwnerChange &rMember = rMembers[i];
		PutOwnerRaw( &pSession->snapshot, rMember, bAfter );
		PutOwnerRaw( &pSession->working, rMember, bAfter );
		const SGDBObjectDesc *pDesc = pObjectsDB != 0 ? pObjectsDB->GetDesc( ( bAfter ? rMember.szNameBefore : rMember.szNameAfter ).c_str() ) : 0;
		if ( pDesc != 0 && pDesc->eGameType == SGVOGT_FLAG )
			continue;
		std::unordered_map<int, CPtr<IRefCount> >::const_iterator itEngine = pSession->byLinkID.find( rMember.nLinkID );
		if ( pAIEditor != 0 && itEngine != pSession->byLinkID.end() )
		{
			IRefCount *pAIObject = itEngine->second;
			pAIEditor->SetPlayer( pAIObject, bAfter ? rMember.nOwnerAfter : rMember.nOwnerBefore );
		}
	}
	return true;
}

// The table, the unit creation and the anchors of a state, both copies, and the
// engine's diplomacies.
void PutTable( SEditorSession *pSession, const SPlayersState &rState )
{
	PutPlayersStateRaw( &pSession->snapshot, rState );
	PutPlayersStateRaw( &pSession->working, rState );
	if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
		pAIEditor->SetDiplomacies( pSession->snapshot.diplomacies );
}

// A state and its owners put in, the bigger of the two tables in force while
// the engine's owners move: an owner is always an index of the table then.
bool ApplyPlayersState( SEditorSession *pSession, const SPlayersState &rState, const std::vector<SOwnerChange> &rMembers, bool bAfter )
{
	const bool bTableFirst = rState.diplomacies.size() >= pSession->snapshot.diplomacies.size();
	if ( bTableFirst )
	{
		PutTable( pSession, rState );
		if ( !PutOwners( pSession, rMembers, bAfter ) )
			return false;
	}
	else
	{
		if ( !PutOwners( pSession, rMembers, bAfter ) )
			return false;
		PutTable( pSession, rState );
	}
	UpdateSessionWorld( pSession );
	return true;
}

// ONE player edit of the log (D-30): the table, unit creation and anchors
// before and after, and every object whose owner moved. Revert and Reapply put
// them back raw - nothing is derived again.
struct SPlayersEdit : public IEditRecord
{
	SPlayersState before, after;
	std::vector<SOwnerChange> members;

	virtual bool Revert( SEditorSession *pSession )
	{
		return ApplyPlayersState( pSession, before, members, false );
	}
	virtual bool Reapply( SEditorSession *pSession )
	{
		return ApplyPlayersState( pSession, after, members, true );
	}
};

// The owners of every object of both lists, with the names, before an edit.
struct SOwnerSnapshot
{
	std::vector<int> owners[2];
	std::vector<std::string> names[2];
};

void CaptureOwners( CMapInfo *pMap, SOwnerSnapshot *pOut )
{
	for ( int nList = 0; nList < 2; ++nList )
	{
		const std::vector<SMapObjectInfo> &rList = *ObjectList( pMap, nList );
		pOut->owners[nList].clear();
		pOut->names[nList].clear();
		for ( size_t i = 0; i < rList.size(); ++i )
		{
			pOut->owners[nList].push_back( rList[i].nPlayer );
			pOut->names[nList].push_back( rList[i].szName );
		}
	}
}

// What an edit that changed the snapshot's owners moved: the objects whose owner
// differs from `rBefore`, a flag renamed to its new owner's party.
void CollectOwnerChanges( SEditorSession *pSession, const SOwnerSnapshot &rBefore, std::vector<SOwnerChange> *pMembers )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	for ( int nList = 0; nList < 2; ++nList )
	{
		std::vector<SMapObjectInfo> &rList = *ObjectList( &pSession->snapshot, nList );
		for ( size_t i = 0; i < rList.size() && i < rBefore.owners[nList].size(); ++i )
		{
			if ( rList[i].nPlayer == rBefore.owners[nList][i] )
				continue;
			SOwnerChange member;
			member.nList = nList;
			member.nIndex = int( i );
			member.nLinkID = rList[i].link.nLinkID;
			member.nOwnerBefore = rBefore.owners[nList][i];
			member.nOwnerAfter = rList[i].nPlayer;
			member.szNameBefore = rBefore.names[nList][i];
			member.szNameAfter = rBefore.names[nList][i];
			// A flag follows its owner: Flag_<the party's general side>, the
			// properties' own swap; a type the database lacks keeps its name.
			const SGDBObjectDesc *pDesc = pObjectsDB != 0 ? pObjectsDB->GetDesc( member.szNameBefore.c_str() ) : 0;
			if ( pDesc != 0 && pDesc->eGameType == SGVOGT_FLAG )
			{
				const std::string szFlag = "Flag_" + FlagPartyName( pSession, member.nOwnerAfter );
				if ( pObjectsDB->GetDesc( szFlag.c_str() ) != 0 )
					member.szNameAfter = szFlag;
			}
			pMembers->push_back( member );
		}
	}
}

// Logs a player edit whose after-state is the snapshot as the NMapRecords call
// left it: the snapshot goes back to `before` (records only), then the state is
// applied the way an undo's redo would - both copies and the engine at once.
bool CommitPlayersEdit( SEditorSession *pSession, const SPlayersState &rBefore, const SOwnerSnapshot &rOwnersBefore, bool *pbRefused, int *pnToken )
{
	std::unique_ptr<SPlayersEdit> pEdit( new SPlayersEdit() );
	pEdit->before = rBefore;
	CapturePlayersState( pSession->snapshot, &pEdit->after );
	CollectOwnerChanges( pSession, rOwnersBefore, &pEdit->members );
	// The snapshot's records back to what they were, so the apply is the same
	// path an undo's redo takes.
	PutPlayersStateRaw( &pSession->snapshot, pEdit->before );
	for ( size_t i = 0; i < pEdit->members.size(); ++i )
		PutOwnerRaw( &pSession->snapshot, pEdit->members[i], false );
	if ( !ApplyPlayersState( pSession, pEdit->after, pEdit->members, true ) )
	{
		// Back to the state before; the apply's own refusal names why.
		PutPlayersStateRaw( &pSession->snapshot, pEdit->before );
		PutPlayersStateRaw( &pSession->working, pEdit->before );
		for ( size_t i = 0; i < pEdit->members.size(); ++i )
		{
			if ( MemberIsThere( pSession, pEdit->members[i] ) )
			{
				PutOwnerRaw( &pSession->snapshot, pEdit->members[i], false );
				PutOwnerRaw( &pSession->working, pEdit->members[i], false );
			}
		}
		if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
			pAIEditor->SetDiplomacies( pSession->snapshot.diplomacies );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	*pnToken = LogEdit( pSession, pEdit.release() );
	return true;
}
}

bool AddPlayerToSession( SEditorSession *pSession, int nSide, bool *pbRefused, int *pnToken )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	*pnToken = -1;
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession != 0 ) pSession->szMessage = "no map is open";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( nSide < 0 || nSide > 1 )
	{
		pSession->szMessage = NStr::Format( "%d is no side for a player: 0 and 1 are the two sides", nSide );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( pSession->snapshot.diplomacies.empty() || int( pSession->snapshot.diplomacies.size() ) >= NMapRecords::nMaxPlayerEntries )
	{
		pSession->szMessage = pSession->snapshot.diplomacies.empty() ? "the map has no neutral player to add a player before" : "a map holds 16 players and the neutral";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	SPlayersState before;
	CapturePlayersState( pSession->snapshot, &before );
	SOwnerSnapshot owners;
	CaptureOwners( &pSession->snapshot, &owners );
	if ( !NMapRecords::InsertPlayer( &pSession->snapshot, BYTE( nSide ) ) )
	{
		pSession->szMessage = "the player could not be added";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	return CommitPlayersEdit( pSession, before, owners, pbRefused, pnToken );
}

bool DeletePlayerFromSession( SEditorSession *pSession, int nPlayer, bool *pbRefused, int *pnToken )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	*pnToken = -1;
	if ( pSession == 0 || !pSession->bMapOpen )
	{
		if ( pSession != 0 ) pSession->szMessage = "no map is open";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	const int nEntries = int( pSession->snapshot.diplomacies.size() );
	if ( nPlayer < 0 || nPlayer >= nEntries - 1 )
	{
		pSession->szMessage = nPlayer == nEntries - 1 ? "the neutral player cannot be deleted" : NStr::Format( "%d is no player: the map holds %d", nPlayer, Max( nEntries - 1, 0 ) );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( nEntries - 1 < NMapRecords::nMinPlayerEntries )
	{
		pSession->szMessage = "a map keeps at least two players and the neutral";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	SPlayersState before;
	CapturePlayersState( pSession->snapshot, &before );
	SOwnerSnapshot owners;
	CaptureOwners( &pSession->snapshot, &owners );
	if ( !NMapRecords::ErasePlayer( &pSession->snapshot, nPlayer ) )
	{
		pSession->szMessage = "the player could not be deleted";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	return CommitPlayersEdit( pSession, before, owners, pbRefused, pnToken );
}

namespace {
bool NameFits( const std::string &rszName )
{
	return rszName.size() < sizeof( ( (BkEditorUnitCreationRecord*)0 )->party );
}

void CopyName( char *pDest, size_t nSize, const std::string &rszName )
{
	memset( pDest, 0, nSize );
	memcpy( pDest, rszName.c_str(), Min( rszName.size(), nSize - 1 ) );
}

// The aircraft and the paratroop squads a unit-creation combo offers: the MFC
// CUCHelper::Initialize's own scan of the object database - a unit under a folder
// with "aviation" in its name, a squad under one with "squads".
void CollectUnitCreationLists( std::vector<std::string> *pAircraft, std::vector<std::string> *pSquads )
{
	IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
	if ( pObjectsDB == 0 )
		return;
	const SGDBObjectDesc *pDescs = pObjectsDB->GetAllDescs();
	const int nDescs = pObjectsDB->GetNumDescs();
	for ( int i = 0; i < nDescs; ++i )
	{
		std::vector<std::string> parts;
		NStr::SplitString( pDescs[i].szPath, parts, '\\' );
		for ( size_t j = 0; j < parts.size(); ++j )
		{
			std::string szPart = parts[j];
			NStr::ToLower( szPart );
			if ( szPart.find( "aviation" ) != std::string::npos && pDescs[i].eGameType == SGVOGT_UNIT )
			{
				if ( pAircraft != 0 ) pAircraft->push_back( pDescs[i].szKey );
				break;
			}
			else if ( szPart.find( "squads" ) != std::string::npos && pDescs[i].eGameType == SGVOGT_SQUAD )
			{
				if ( pSquads != 0 ) pSquads->push_back( pDescs[i].szKey );
				break;
			}
		}
	}
}

bool IsIn( const std::vector<std::string> &rList, const std::string &rszName )
{
	return std::find( rList.begin(), rList.end(), rszName ) != rList.end();
}

// A name the put may keep: the entry's own, one the file held at open, or a
// known one. An empty name is never kept (the reader would refill it).
bool NameAllowed( const std::string &rszName, const std::string &rszCurrent, const std::set<std::string> &rOpened, const std::vector<std::string> &rKnown )
{
	if ( rszName.empty() )
		return false;
	return rszName == rszCurrent || rOpened.count( rszName ) != 0 || IsIn( rKnown, rszName );
}

void ToCUnitCreation( const SUnitCreation &rUnit, int nSlotCount, BkEditorUnitCreationRecord *pOut )
{
	memset( pOut, 0, sizeof *pOut );
	pOut->slot_count = nSlotCount;
	CopyName( pOut->party, sizeof pOut->party, rUnit.szPartyName );
	for ( int i = 0; i < 5; ++i )
	{
		CopyName( pOut->aircraft[i].name, sizeof pOut->aircraft[i].name, rUnit.aviation.aircrafts[i].szName );
		pOut->aircraft[i].formation_size = rUnit.aviation.aircrafts[i].nFormationSize;
		pOut->aircraft[i].count = rUnit.aviation.aircrafts[i].nPlanes;
	}
	CopyName( pOut->paratroop_name, sizeof pOut->paratroop_name, rUnit.aviation.szParadropSquadName );
	pOut->paratroop_count = rUnit.aviation.nParadropSquadCount;
	pOut->relax_time = rUnit.aviation.nRelaxTime;
	int nPoint = 0;
	for ( std::list<CVec3>::const_iterator it = rUnit.aviation.vAppearPoints.begin(); it != rUnit.aviation.vAppearPoints.end() && nPoint < nMaxAppearPoints; ++it, ++nPoint )
		pOut->appear[nPoint] = ToC( *it );
	pOut->appear_count = nPoint;
}

bool SameVec3Bits( const CVec3 &rLeft, const CVec3 &rRight )
{
	return memcmp( &rLeft, &rRight, sizeof rLeft ) == 0;
}
}

bool ReadSessionUnitCreation( SEditorSession *pSession, int nPlayer, BkEditorUnitCreationRecord *pOut, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	const std::vector<SUnitCreation> &rUnits = pSession->snapshot.unitCreation.units;
	if ( nPlayer < 0 || nPlayer >= nMaxUnitCreationSlots )
	{
		pSession->szMessage = NStr::Format( "%d is no player: a map has 0 to 15", nPlayer );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( int( rUnits.size() ) > nMaxUnitCreationSlots )
	{
		pSession->szMessage = "this map has more unit-creation entries than the editor edits";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	SUnitCreation unit;
	if ( !NMapRecords::GetUnitCreation( pSession->snapshot, nPlayer, &unit ) )
		return false;
	// The record's fixed arrays: an entry that does not fit is the file's, kept
	// byte-exact, and not editable here.
	bool bFits = int( unit.aviation.aircrafts.size() ) == 5 && int( unit.aviation.vAppearPoints.size() ) <= nMaxAppearPoints &&
	             NameFits( unit.szPartyName ) && NameFits( unit.aviation.szParadropSquadName );
	for ( size_t i = 0; bFits && i < unit.aviation.aircrafts.size(); ++i )
		bFits = NameFits( unit.aviation.aircrafts[i].szName );
	if ( !bFits )
	{
		pSession->szMessage = "this player's unit creation does not fit the record (a name of 64 characters or more, other than five aircraft slots, or more than 32 appear points)";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	ToCUnitCreation( unit, int( rUnits.size() ), pOut );
	return true;
}

bool SetSessionUnitCreation( SEditorSession *pSession, int nPlayer, const BkEditorUnitCreationRecord &rRecord, bool *pbRefused )
{
	if ( pbRefused != 0 ) *pbRefused = false;
	if ( !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return false;
	}
	const int nSlots = int( pSession->snapshot.unitCreation.units.size() );
	if ( nSlots > nMaxUnitCreationSlots )
	{
		pSession->szMessage = "this map has more unit-creation entries than the editor edits";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	// A player is one of the table's, or one the vector already holds.
	const int nPlayers = Max( int( pSession->snapshot.diplomacies.size() ) - 1, nSlots );
	if ( nPlayer < 0 || nPlayer >= nPlayers || nPlayer >= nMaxUnitCreationSlots )
	{
		pSession->szMessage = NStr::Format( "%d is no player: the map holds %d", nPlayer, nPlayers );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	// What the snapshot holds now, and how many slots: the put below is put back exactly, on
	// both copies, when the working copy refuses it after the snapshot took it.
	SUnitCreation current;
	NMapRecords::GetUnitCreation( pSession->snapshot, nPlayer, &current );
	// A slot count that does not reach the player only sets the vector's size: the
	// entry is the defaults (an undo of a put that grew the vector), not stored.
	if ( rRecord.slot_count <= nPlayer )
	{
		const SUnitCreation unused;
		if ( !NMapRecords::PutUnitCreation( &pSession->snapshot, nPlayer, unused, rRecord.slot_count ) )
			return false;
		if ( !NMapRecords::PutUnitCreation( &pSession->working, nPlayer, unused, rRecord.slot_count ) )
		{
			NMapRecords::PutUnitCreation( &pSession->snapshot, nPlayer, current, nSlots );
			return false;
		}
		return true;
	}
	if ( int( current.aviation.aircrafts.size() ) != 5 )
	{
		pSession->szMessage = "this player's unit creation does not hold five aircraft slots; it is kept as it is";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}

	SUnitCreation wanted = current;
	wanted.szPartyName = rRecord.party;
	for ( int i = 0; i < 5; ++i )
	{
		wanted.aviation.aircrafts[i].szName = rRecord.aircraft[i].name;
		wanted.aviation.aircrafts[i].nFormationSize = rRecord.aircraft[i].formation_size;
		wanted.aviation.aircrafts[i].nPlanes = rRecord.aircraft[i].count;
	}
	wanted.aviation.szParadropSquadName = rRecord.paratroop_name;
	wanted.aviation.nParadropSquadCount = rRecord.paratroop_count;
	wanted.aviation.nRelaxTime = rRecord.relax_time;
	wanted.aviation.vAppearPoints.clear();
	for ( int i = 0; i < rRecord.appear_count; ++i )
		wanted.aviation.vAppearPoints.push_back( FromC( rRecord.appear[i] ) );

	// MutableValidate's rules and the manipulators' combos. Only a field that
	// differs from what the entry holds is held to them: a file's own odd value
	// stays, and an undo can always put it back.
	std::vector<std::string> parties, aircraft, squads;
	ListUnitCreationChoices( pSession, 0, &parties );
	CollectUnitCreationLists( &aircraft, &squads );
	if ( !NameAllowed( wanted.szPartyName, current.szPartyName, pSession->openedUCParties, parties ) )
	{
		pSession->szMessage = "the party \"" + wanted.szPartyName + "\" is not in partys.xml";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	for ( int i = 0; i < 5; ++i )
	{
		const SUCAircraft &rWanted = wanted.aviation.aircrafts[i];
		const SUCAircraft &rCurrent = current.aviation.aircrafts[i];
		if ( !NameAllowed( rWanted.szName, rCurrent.szName, pSession->openedUCAircraft, aircraft ) )
		{
			pSession->szMessage = NStr::Format( "%s: \"%s\" is no aircraft of the object database", UC_AIRCRAFT_TYPE_NAMES[i], rWanted.szName.c_str() );
			if ( pbRefused != 0 ) *pbRefused = true;
			return false;
		}
		if ( rWanted.nFormationSize != rCurrent.nFormationSize && ( rWanted.nFormationSize < 1 || rWanted.nFormationSize > nMaxFormationSize ) )
		{
			pSession->szMessage = NStr::Format( "%s: formation size %d is outside 1..%d", UC_AIRCRAFT_TYPE_NAMES[i], rWanted.nFormationSize, nMaxFormationSize );
			if ( pbRefused != 0 ) *pbRefused = true;
			return false;
		}
		if ( rWanted.nPlanes != rCurrent.nPlanes && ( rWanted.nPlanes < 0 || rWanted.nPlanes > nMaxPlaneCount ) )
		{
			pSession->szMessage = NStr::Format( "%s: count %d is outside 0..%d", UC_AIRCRAFT_TYPE_NAMES[i], rWanted.nPlanes, nMaxPlaneCount );
			if ( pbRefused != 0 ) *pbRefused = true;
			return false;
		}
	}
	if ( !NameAllowed( wanted.aviation.szParadropSquadName, current.aviation.szParadropSquadName, pSession->openedUCSquads, squads ) )
	{
		pSession->szMessage = "the paratroop squad \"" + wanted.aviation.szParadropSquadName + "\" is no squad of the object database";
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	if ( wanted.aviation.nParadropSquadCount != current.aviation.nParadropSquadCount && ( wanted.aviation.nParadropSquadCount < 0 || wanted.aviation.nParadropSquadCount > nMaxPlaneCount ) )
	{
		pSession->szMessage = NStr::Format( "the paratroop squads count %d is outside 0..%d", wanted.aviation.nParadropSquadCount, nMaxPlaneCount );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	// The file reader turns a relax time of 0 or less into 20 (Validate), so a
	// smaller value would not survive a save: it is refused instead.
	if ( wanted.aviation.nRelaxTime != current.aviation.nRelaxTime && wanted.aviation.nRelaxTime < 1 )
	{
		pSession->szMessage = NStr::Format( "the relax time %d is below 1 second", wanted.aviation.nRelaxTime );
		if ( pbRefused != 0 ) *pbRefused = true;
		return false;
	}
	for ( std::list<CVec3>::const_iterator it = wanted.aviation.vAppearPoints.begin(); it != wanted.aviation.vAppearPoints.end(); ++it )
	{
		bool bHeld = false;
		for ( std::list<CVec3>::const_iterator itCurrent = current.aviation.vAppearPoints.begin(); itCurrent != current.aviation.vAppearPoints.end(); ++itCurrent )
			if ( SameVec3Bits( *it, *itCurrent ) )
				bHeld = true;
		if ( !bHeld && !OnTheMapInAIUnits( *pSession, it->x, it->y ) )
		{
			pSession->szMessage = NStr::Format( "the appear point (%.1f, %.1f) is off the map", it->x, it->y );
			if ( pbRefused != 0 ) *pbRefused = true;
			return false;
		}
	}

	if ( !NMapRecords::PutUnitCreation( &pSession->snapshot, nPlayer, wanted, rRecord.slot_count ) )
	{
		pSession->szMessage = "the unit creation could not be put";
		return false;
	}
	if ( !NMapRecords::PutUnitCreation( &pSession->working, nPlayer, wanted, rRecord.slot_count ) )
	{
		// The snapshot took the put and the working copy did not: back to what the snapshot held,
		// the way every other collection setter in this file restores it.
		NMapRecords::PutUnitCreation( &pSession->snapshot, nPlayer, current, nSlots );
		pSession->szMessage = "the unit creation could not be put";
		return false;
	}
	return true;
}

bool ListUnitCreationChoices( SEditorSession *pSession, int nKind, std::vector<std::string> *pNames )
{
	pNames->clear();
	if ( nKind == 0 )
	{
		ReadPartyTable( pSession );
		for ( size_t i = 0; i < pSession->partyTable.size(); ++i )
			pNames->push_back( pSession->partyTable[i].szPartyName );
		return true;
	}
	if ( nKind == 1 )
		CollectUnitCreationLists( pNames, 0 );
	else if ( nKind == 2 )
		CollectUnitCreationLists( 0, pNames );
	else
		return false;
	return true;
}
