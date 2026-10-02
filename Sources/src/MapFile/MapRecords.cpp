// The record-level overlay of the M2 collections; see MapRecords.h. Style of
// MapOverlay.cpp: no engine, no object database, in-place edits only.
#include "StdAfx.h"
#include <iterator>
#include <cmath>
#include "MapRecords.h"

namespace NMapRecords
{
namespace {
// The list edits every collection shares: by position, in place, the position
// range-checked. std::list positions are reached with std::advance, so one
// template serves the vectors and the lists alike.
template<class TList>
typename TList::iterator ListAt( TList *pList, int nIndex )
{
	typename TList::iterator it = pList->begin();
	std::advance( it, nIndex );
	return it;
}

template<class TList, class TValue>
bool InsertAt( TList *pList, int nIndex, const TValue &rValue )
{
	const int nSize = int( pList->size() );
	if ( nIndex < -1 || nIndex > nSize )
		return false;
	pList->insert( ListAt( pList, nIndex < 0 ? nSize : nIndex ), rValue );
	return true;
}

template<class TList, class TValue>
bool ReplaceAt( TList *pList, int nIndex, const TValue &rValue )
{
	if ( nIndex < 0 || nIndex >= int( pList->size() ) )
		return false;
	*ListAt( pList, nIndex ) = rValue;
	return true;
}

template<class TList, class TValue>
bool EraseAt( TList *pList, int nIndex, TValue *pErased )
{
	if ( nIndex < 0 || nIndex >= int( pList->size() ) )
		return false;
	typename TList::iterator it = ListAt( pList, nIndex );
	if ( pErased != 0 )
		*pErased = *it;
	pList->erase( it );
	return true;
}

TVSOList* VsoList( SLoadMapInfo *pMap, EVsoKind eKind )
{
	if ( pMap == 0 )
		return 0;
	return eKind == VSO_ROAD ? &pMap->terrain.roads3 : ( eKind == VSO_RIVER ? &pMap->terrain.rivers : 0 );
}

// The object with this link ID, objects before scenarioObjects. Link ID 0 is
// the "no link" value and never names an object (C11).
SMapObjectInfo* FindByLinkID( SLoadMapInfo *pMap, int nLinkID )
{
	if ( pMap == 0 || nLinkID == 0 )
		return 0;
	std::vector<SMapObjectInfo> *lists[2] = { &pMap->objects, &pMap->scenarioObjects };
	for ( int nList = 0; nList < 2; ++nList )
		for ( size_t i = 0; i < lists[nList]->size(); ++i )
			if ( (*lists[nList])[i].link.nLinkID == nLinkID )
				return &(*lists[nList])[i];
	return 0;
}
}

void GetCameraAnchors( const SLoadMapInfo &rMap, SCameraAnchors *pOut )
{
	if ( pOut == 0 )
		return;
	pOut->vNeutral = rMap.vCameraAnchor;
	pOut->players = rMap.playersCameraAnchors;
}

bool PutCameraAnchors( SLoadMapInfo *pMap, const SCameraAnchors &rAnchors )
{
	if ( pMap == 0 )
		return false;
	pMap->vCameraAnchor = rAnchors.vNeutral;
	pMap->playersCameraAnchors = rAnchors.players;
	return true;
}

bool SetPlayerCameraAnchor( SCameraAnchors *pAnchors, int nPlayer, const CVec3 &vAnchor )
{
	if ( pAnchors == 0 || nPlayer < 0 || nPlayer >= nMaxCameraAnchorPlayers )
		return false;
	// Pads with VNULL3, never shrinks: a map that already names more players
	// keeps every one of them.
	if ( int( pAnchors->players.size() ) < nPlayer + 1 )
		pAnchors->players.resize( nPlayer + 1, VNULL3 );
	pAnchors->players[nPlayer] = vAnchor;
	return true;
}

bool ClearPlayerCameraAnchor( SCameraAnchors *pAnchors, int nPlayer )
{
	if ( pAnchors == 0 || nPlayer < 0 || nPlayer >= nMaxCameraAnchorPlayers )
		return false;
	if ( nPlayer < int( pAnchors->players.size() ) )
		pAnchors->players[nPlayer] = VNULL3;
	return true;
}

bool PutScriptFile( SLoadMapInfo *pMap, const std::string &szName )
{
	if ( pMap == 0 )
		return false;
	pMap->szScriptFile = szName;
	return true;
}

namespace {
// A Windows device name (IN-B02): "<dir>\\CON.lua" is the console, not a file,
// whatever follows the first dot. Case-insensitive, as Windows is.
bool IsWindowsDeviceName( const std::string &szName )
{
	std::string szStem = szName.substr( 0, szName.find( '.' ) );
	for ( size_t i = 0; i < szStem.size(); ++i )
		if ( szStem[i] >= 'a' && szStem[i] <= 'z' )
			szStem[i] = char( szStem[i] - 'a' + 'A' );
	if ( szStem == "CON" || szStem == "PRN" || szStem == "AUX" || szStem == "NUL" )
		return true;
	return szStem.size() == 4 && ( szStem.compare( 0, 3, "COM" ) == 0 || szStem.compare( 0, 3, "LPT" ) == 0 ) && szStem[3] >= '1' && szStem[3] <= '9';
}
}

bool IsBareScriptName( const std::string &szName )
{
	if ( szName.empty() )
		return true;
	if ( IsWindowsDeviceName( szName ) )
		return false;
	if ( szName.size() > 63 || szName[0] == '.' || szName.find( ".." ) != std::string::npos )
		return false;
	for ( size_t i = 0; i < szName.size(); ++i )
	{
		const char c = szName[i];
		const bool bOk = ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' || c == '.';
		if ( !bOk )
			return false;
	}
	// The game appends ".lua" itself (Scripts.cpp), so a name that carries it
	// would look for "x.lua.lua".
	if ( szName.size() >= 4 )
	{
		const std::string szEnd = szName.substr( szName.size() - 4 );
		if ( ( szEnd[0] == '.' ) && ( szEnd[1] == 'l' || szEnd[1] == 'L' ) && ( szEnd[2] == 'u' || szEnd[2] == 'U' ) && ( szEnd[3] == 'a' || szEnd[3] == 'A' ) )
			return false;
	}
	return true;
}

bool InsertScriptArea( SLoadMapInfo *pMap, int nIndex, const SScriptArea &rArea )
{
	return pMap != 0 && InsertAt( &pMap->scriptAreas, nIndex, rArea );
}

bool ReplaceScriptArea( SLoadMapInfo *pMap, int nIndex, const SScriptArea &rArea )
{
	return pMap != 0 && ReplaceAt( &pMap->scriptAreas, nIndex, rArea );
}

bool EraseScriptArea( SLoadMapInfo *pMap, int nIndex, SScriptArea *pErased )
{
	return pMap != 0 && EraseAt( &pMap->scriptAreas, nIndex, pErased );
}

bool IsAreaNameFree( const SLoadMapInfo &rMap, const std::string &szName, int nIgnoreIndex )
{
	if ( szName.empty() )
		return false;
	for ( size_t i = 0; i < rMap.scriptAreas.size(); ++i )
		if ( int( i ) != nIgnoreIndex && rMap.scriptAreas[i].szName == szName )
			return false;
	return true;
}

bool PutReinforcementGroup( SLoadMapInfo *pMap, int nGroupID, const std::vector<int> &rIDs )
{
	if ( pMap == 0 || nGroupID < 0 )
		return false;
	pMap->reinforcements.groups[nGroupID].ids = rIDs;
	return true;
}

bool EraseReinforcementGroup( SLoadMapInfo *pMap, int nGroupID, std::vector<int> *pErased )
{
	if ( pMap == 0 )
		return false;
	std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::iterator it = pMap->reinforcements.groups.find( nGroupID );
	if ( it == pMap->reinforcements.groups.end() )
		return false;
	if ( pErased != 0 )
		*pErased = it->second.ids;
	pMap->reinforcements.groups.erase( it );
	return true;
}

int FirstFreeGroupID( const SLoadMapInfo &rMap, int nFrom )
{
	int nID = nFrom < 0 ? 0 : nFrom;
	while ( rMap.reinforcements.groups.find( nID ) != rMap.reinforcements.groups.end() )
		++nID;
	return nID;
}

bool InsertStartCommand( SLoadMapInfo *pMap, int nIndex, const SAIStartCommand &rCommand )
{
	return pMap != 0 && InsertAt( &pMap->startCommandsList, nIndex, rCommand );
}

bool ReplaceStartCommand( SLoadMapInfo *pMap, int nIndex, const SAIStartCommand &rCommand )
{
	return pMap != 0 && ReplaceAt( &pMap->startCommandsList, nIndex, rCommand );
}

bool EraseStartCommand( SLoadMapInfo *pMap, int nIndex, SAIStartCommand *pErased )
{
	return pMap != 0 && EraseAt( &pMap->startCommandsList, nIndex, pErased );
}

bool InsertReservePosition( SLoadMapInfo *pMap, int nIndex, const SBattlePosition &rPosition )
{
	return pMap != 0 && InsertAt( &pMap->reservePositionsList, nIndex, rPosition );
}

bool ReplaceReservePosition( SLoadMapInfo *pMap, int nIndex, const SBattlePosition &rPosition )
{
	return pMap != 0 && ReplaceAt( &pMap->reservePositionsList, nIndex, rPosition );
}

bool EraseReservePosition( SLoadMapInfo *pMap, int nIndex, SBattlePosition *pErased )
{
	return pMap != 0 && EraseAt( &pMap->reservePositionsList, nIndex, pErased );
}

void GetAIGeneralSide( const SLoadMapInfo &rMap, int nSide, SAIGeneralSidePut *pOut )
{
	if ( pOut == 0 )
		return;
	const std::vector<SAIGeneralSideInfo> &rSides = rMap.aiGeneralMapInfo.sidesInfo;
	pOut->nSideCount = int( rSides.size() );
	pOut->nSide = nSide;
	pOut->info = ( nSide >= 0 && nSide < int( rSides.size() ) ) ? rSides[nSide] : SAIGeneralSideInfo();
}

bool PutAIGeneralSide( SLoadMapInfo *pMap, const SAIGeneralSidePut &rPut )
{
	if ( pMap == 0 || rPut.nSideCount < 0 || rPut.nSideCount > nMaxAIGeneralSides )
		return false;
	std::vector<SAIGeneralSideInfo> &rSides = pMap->aiGeneralMapInfo.sidesInfo;
	// Lower sides the map lacked come out empty; a smaller count drops the sides
	// above it, which is how an undo takes back the ones a put created.
	rSides.resize( rPut.nSideCount );
	if ( rPut.nSide >= 0 && rPut.nSide < rPut.nSideCount )
		rSides[rPut.nSide] = rPut.info;
	return true;
}

bool InsertVso( SLoadMapInfo *pMap, EVsoKind eKind, int nIndex, const SVectorStripeObject &rVso )
{
	TVSOList *pList = VsoList( pMap, eKind );
	return pList != 0 && InsertAt( pList, nIndex, rVso );
}

bool ReplaceVso( SLoadMapInfo *pMap, EVsoKind eKind, int nIndex, const SVectorStripeObject &rVso )
{
	TVSOList *pList = VsoList( pMap, eKind );
	return pList != 0 && ReplaceAt( pList, nIndex, rVso );
}

bool EraseVso( SLoadMapInfo *pMap, EVsoKind eKind, int nIndex, SVectorStripeObject *pErased )
{
	TVSOList *pList = VsoList( pMap, eKind );
	return pList != 0 && EraseAt( pList, nIndex, pErased );
}

int NextVsoID( const SLoadMapInfo &rMap )
{
	int nMax = 0;
	for ( size_t i = 0; i < rMap.terrain.roads3.size(); ++i )
		nMax = Max( nMax, rMap.terrain.roads3[i].nID );
	for ( size_t i = 0; i < rMap.terrain.rivers.size(); ++i )
		nMax = Max( nMax, rMap.terrain.rivers[i].nID );
	return nMax + 1;
}

bool InsertBridgeEntry( SLoadMapInfo *pMap, int nIndex, const std::vector<int> &rLinkIDs )
{
	return pMap != 0 && InsertAt( &pMap->bridges, nIndex, rLinkIDs );
}

bool ReplaceBridgeEntry( SLoadMapInfo *pMap, int nIndex, const std::vector<int> &rLinkIDs )
{
	return pMap != 0 && ReplaceAt( &pMap->bridges, nIndex, rLinkIDs );
}

bool EraseBridgeEntry( SLoadMapInfo *pMap, int nIndex, std::vector<int> *pErased )
{
	return pMap != 0 && EraseAt( &pMap->bridges, nIndex, pErased );
}

bool InsertEntrenchment( SLoadMapInfo *pMap, int nIndex, const SEntrenchmentInfo &rEntrenchment )
{
	return pMap != 0 && InsertAt( &pMap->entrenchments, nIndex, rEntrenchment );
}

bool ReplaceEntrenchment( SLoadMapInfo *pMap, int nIndex, const SEntrenchmentInfo &rEntrenchment )
{
	return pMap != 0 && ReplaceAt( &pMap->entrenchments, nIndex, rEntrenchment );
}

bool EraseEntrenchment( SLoadMapInfo *pMap, int nIndex, SEntrenchmentInfo *pErased )
{
	return pMap != 0 && EraseAt( &pMap->entrenchments, nIndex, pErased );
}

bool SetObjectScriptID( SLoadMapInfo *pMap, int nLinkID, int nScriptID )
{
	if ( nScriptID < -1 || nScriptID > 32000 )
		return false;
	SMapObjectInfo *pObject = FindByLinkID( pMap, nLinkID );
	if ( pObject == 0 )
		return false;
	pObject->nScriptID = nScriptID;
	return true;
}

bool SetObjectHP( SLoadMapInfo *pMap, int nLinkID, float fHP )
{
	if ( !std::isfinite( fHP ) )
		return false;
	SMapObjectInfo *pObject = FindByLinkID( pMap, nLinkID );
	if ( pObject == 0 )
		return false;
	pObject->fHP = fHP;
	return true;
}

bool SetObjectPlayer( SLoadMapInfo *pMap, int nLinkID, int nPlayer )
{
	if ( pMap == 0 || nPlayer < 0 || nPlayer >= int( pMap->diplomacies.size() ) )
		return false;
	SMapObjectInfo *pObject = FindByLinkID( pMap, nLinkID );
	if ( pObject == 0 )
		return false;
	pObject->nPlayer = nPlayer;
	return true;
}

bool SetObjectAngle( SLoadMapInfo *pMap, int nLinkID, float fAngleDegrees )
{
	if ( !std::isfinite( fAngleDegrees ) )
		return false;
	SMapObjectInfo *pObject = FindByLinkID( pMap, nLinkID );
	if ( pObject == 0 )
		return false;
	// The MFC properties dialog's own turn (SEditorMApObject.cpp:384-386):
	// degrees to the record's 65536-direction, rounded once.
	pObject->nDir = int( ( fAngleDegrees * 65536.0f ) / 360.0f + 0.5f );
	return true;
}

bool SetObjectFormation( SLoadMapInfo *pMap, int nLinkID, int nFormation )
{
	// -1 is what a record that never carried a formation holds (fmtMap's own
	// default), so an undo can put it back; anything below is the caller's.
	if ( nFormation < -1 )
		return false;
	SMapObjectInfo *pObject = FindByLinkID( pMap, nLinkID );
	if ( pObject == 0 )
		return false;
	pObject->nFrameIndex = nFormation;
	return true;
}

bool SetObjectLink( SLoadMapInfo *pMap, int nLinkID, int nLinkWith )
{
	if ( pMap == 0 )
		return false;
	SMapObjectInfo *pObject = FindByLinkID( pMap, nLinkID );
	if ( pObject == 0 )
		return false;
	// 0 unlinks; a host must be an object of the map and never the passenger
	// itself (a self-link would make the game's loaders chase a cycle).
	if ( nLinkWith != 0 )
	{
		if ( nLinkWith == nLinkID )
			return false;
		if ( FindByLinkID( pMap, nLinkWith ) == 0 )
			return false;
	}
	pObject->link.nLinkWith = nLinkWith;
	return true;
}
}
