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
#include "world.h"
#include "../AILogic/AILogic.h"
#include "../Scene/Scene.h"
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
	const std::string &rszName = pSession->snapshot.szScriptFile;
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
	if ( szWanted != pSession->szScriptFileAtOpen && !NMapRecords::IsBareScriptName( szWanted ) )
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
bool AreaPutAllowed( SEditorSession *pSession, const SScriptArea &rWanted, int nIgnoreIndex, const SScriptArea *pCurrent )
{
	if ( rWanted.szName.empty() )
	{
		pSession->szMessage = "an area needs a name";
		return false;
	}
	if ( rWanted.vAABBHalfSize.x < 0.0f || rWanted.vAABBHalfSize.y < 0.0f || rWanted.fR < 0.0f )
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
	if ( bMoved && !OnTheMapInAIUnits( *pSession, rWanted.center.x, rWanted.center.y ) )
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
