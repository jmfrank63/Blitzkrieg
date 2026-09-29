// The record-level overlay of the M2 collections; see MapRecords.h. Style of
// MapOverlay.cpp: no engine, no object database, in-place edits only.
#include "StdAfx.h"
#include "MapRecords.h"

namespace NMapRecords
{
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
}
