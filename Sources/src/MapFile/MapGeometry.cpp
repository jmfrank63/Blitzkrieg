// The M2 group tools' geometry; see MapGeometry.h. Style of MapOverlay.cpp and
// MapRecords.cpp: plain numbers, no engine, no object database.
#include "StdAfx.h"
#include <cmath>
#include "MapGeometry.h"
#include "../Formats/fmtTerrain.h"

namespace NMapGeometry
{
namespace {
bool Finite( const CVec2 &v )
{
	return std::isfinite( v.x ) && std::isfinite( v.y );
}
}

bool PlanBridge( const SBridgePlanInput &rInput, const CVec2 &vFirstVis, const CVec2 &vLastVis,
                 std::vector<SPlannedPiece> *pSpans, std::string *pWhy )
{
	std::string szWhy;
	if ( pSpans != 0 )
		pSpans->clear();
	if ( !std::isfinite( rInput.fSpanLength ) || rInput.fSpanLength <= 0.0f )
		szWhy = "this bridge type has no span length";
	else if ( rInput.nDirection != BRIDGE_HORIZONTAL && rInput.nDirection != BRIDGE_VERTICAL )
		szWhy = "this bridge type has no direction";
	else if ( !Finite( vFirstVis ) || !Finite( vLastVis ) || !Finite( rInput.vBeginOrigin ) )
		szWhy = "the drag is not a point on the map";
	if ( !szWhy.empty() )
	{
		if ( pWhy != 0 ) *pWhy = szWhy;
		return false;
	}
	const bool bHorizontal = rInput.nDirection == BRIDGE_HORIZONTAL;
	const float fDx = std::fabs( vLastVis.x - vFirstVis.x );
	const float fDy = std::fabs( vLastVis.y - vFirstVis.y );
	// The drag's longer axis must be the type's: a drag along the other one is
	// the other variant's (rotate draws that one).
	if ( bHorizontal ? fDy > fDx : fDx > fDy )
	{
		if ( pWhy != 0 )
			*pWhy = bHorizontal ? "this bridge runs horizontally: drag it along the other axis or rotate the type"
			                    : "this bridge runs vertically: drag it along the other axis or rotate the type";
		return false;
	}

	// Locked to the axis, then ordered.
	CVec2 vFirst = vFirstVis, vLast = vLastVis;
	if ( bHorizontal )
		vLast.y = vFirst.y;
	else
		vLast.x = vFirst.x;
	if ( bHorizontal ? vFirst.x > vLast.x : vFirst.y > vLast.y )
	{
		const CVec2 vSwap = vFirst;
		vFirst = vLast;
		vLast = vSwap;
	}
	const float fL = rInput.fSpanLength;
	CVec3 vStart( vFirst.x, vFirst.y, 0.0f );
	FitVisOrigin2AIGrid( &vStart, rInput.vBeginOrigin );
	// As GetPointsForBridge counts them: from the drag's own first point, not
	// the fitted start.
	const float fRun = bHorizontal ? vLast.x - vFirst.x : vLast.y - vFirst.y;
	const int nParts = int( fRun / fL );
	if ( nParts < 0 || nParts > 4096 )
	{
		if ( pWhy != 0 ) *pWhy = "that bridge would be too long";
		return false;
	}

	std::vector<CVec2> world;
	world.push_back( CVec2( vStart.x, vStart.y ) );
	for ( int i = 0; i < nParts; ++i )
	{
		const float fAlong = ( float( i ) + 0.5f ) * fL;
		world.push_back( bHorizontal ? CVec2( vStart.x + fAlong, vStart.y ) : CVec2( vStart.x, vStart.y + fAlong ) );
	}
	const float fEnd = float( nParts ) * fL;
	world.push_back( bHorizontal ? CVec2( vStart.x + fEnd, vStart.y ) : CVec2( vStart.x, vStart.y + fEnd ) );

	if ( pSpans == 0 )
		return true;
	for ( size_t i = 0; i < world.size(); ++i )
	{
		SPlannedPiece piece;
		Vis2AI( &piece.vPos, world[i].x, world[i].y, 0.0f );
		piece.vPos.z = 0.0f;
		const bool bFirst = i == 0;
		const bool bLast = i + 1 == world.size();
		if ( bFirst && bHorizontal )
			piece.vPos.x -= 0.1f;
		if ( bLast && !bHorizontal )
			piece.vPos.y += 0.1f;
		piece.nPackedType = bFirst ? BRIDGE_SPAN_BEGIN : bLast ? BRIDGE_SPAN_END : BRIDGE_SPAN_CENTER;
		piece.nDir = 0;
		pSpans->push_back( piece );
	}
	return true;
}

void RotatedBridgeDrag( const SBridgePlanInput &rInput, const CVec2 &vCentreVis, int nSpans, CVec2 *pFirstVis, CVec2 *pLastVis )
{
	const int nParts = nSpans > 2 ? nSpans - 2 : 0;
	const float fL = rInput.fSpanLength;
	const float fFrom = -float( nParts ) * fL / 2.0f;
	const float fTo = fFrom + ( float( nParts ) + 0.5f ) * fL;
	if ( rInput.nDirection == BRIDGE_HORIZONTAL )
	{
		*pFirstVis = CVec2( vCentreVis.x + fFrom, vCentreVis.y );
		*pLastVis = CVec2( vCentreVis.x + fTo, vCentreVis.y );
	}
	else
	{
		*pFirstVis = CVec2( vCentreVis.x, vCentreVis.y + fFrom );
		*pLastVis = CVec2( vCentreVis.x, vCentreVis.y + fTo );
	}
}

std::string BridgePartnerName( const std::string &szName )
{
	if ( szName.size() < 3 )
		return "";
	const std::string szTail = szName.substr( szName.size() - 3 );
	const std::string szHead = szName.substr( 0, szName.size() - 3 );
	if ( szTail == "_01" )
		return szHead + "_02";
	if ( szTail == "_02" )
		return szHead + "_01";
	return "";
}

void RasterizeLine( const CTPoint<int> &from, const CTPoint<int> &to, std::vector< CTPoint<int> > *pTiles )
{
	pTiles->clear();
	long X1 = from.x, Y1 = from.y;
	const long X2 = to.x, Y2 = to.y;
	long DeltaY = Y2 - Y1;
	long dirY = 1;
	if ( DeltaY < 0 )
	{
		dirY = -1;
		DeltaY = -DeltaY;
	}
	long DeltaX = X2 - X1;
	long dirX = 1;
	if ( DeltaX < 0 )
	{
		dirX = -1;
		DeltaX = -DeltaX;
	}
	if ( DeltaX == 0 )
	{
		for ( ;; )
		{
			pTiles->push_back( CTPoint<int>( int( X1 ), int( Y1 ) ) );
			if ( Y1 == Y2 )
				return;
			Y1 += dirY;
		}
	}
	if ( DeltaY == 0 )
	{
		for ( ;; )
		{
			pTiles->push_back( CTPoint<int>( int( X1 ), int( Y1 ) ) );
			if ( X1 == X2 )
				return;
			X1 += dirX;
		}
	}
	if ( DeltaX >= DeltaY )
	{
		long e = -DeltaX;
		const long denom = DeltaX * 2;
		const long t = DeltaY * 2;
		for ( ;; )
		{
			pTiles->push_back( CTPoint<int>( int( X1 ), int( Y1 ) ) );
			if ( X1 == X2 )
				return;
			X1 += dirX;
			e += t;
			if ( e >= 0 )
			{
				Y1 += dirY;
				e -= denom;
			}
		}
	}
	else
	{
		long e = -DeltaY;
		const long denom = DeltaY * 2;
		const long t = DeltaX * 2;
		for ( ;; )
		{
			pTiles->push_back( CTPoint<int>( int( X1 ), int( Y1 ) ) );
			if ( Y1 == Y2 )
				return;
			Y1 += dirY;
			e += t;
			if ( e >= 0 )
			{
				X1 += dirX;
				e -= denom;
			}
		}
	}
}

namespace {
// The tile's cell in the MFC refusal box: patches * 16 cells a side, an AI
// tile being half a cell.
bool CellOnMap( const CTPoint<int> &rTile, const SFencePlanInput &rInput )
{
	const int nCellX = rTile.x >> 1, nCellY = rTile.y >> 1;
	return nCellX >= 0 && nCellY >= 0 && nCellX < rInput.nTilesX / 2 && nCellY < rInput.nTilesY / 2;
}

SPlannedPiece FenceAt( const SFencePlanInput &rInput, int nTileX, int nTileY, int nDir )
{
	CVec3 vPos( float( nTileX * nAITileSize ), float( nTileY * nAITileSize ), 0.0f );
	AI2Vis( &vPos );
	FitVisOrigin2AIGrid( &vPos, rInput.vOrigin[nDir] );
	Vis2AI( &vPos );
	SPlannedPiece piece;
	piece.vPos = CVec3( vPos.x, vPos.y, 0.0f );
	piece.nPackedType = ( 1 << nDir ) | nFenceTypeNormal;
	piece.nDir = 0;
	return piece;
}
}

bool PlanFences( const SFencePlanInput &rInput, const CTPoint<int> &firstTile, const CTPoint<int> &lastTile, bool bCtrl,
                 std::vector<SPlannedPiece> *pFences, std::string *pWhy )
{
	if ( pFences != 0 )
		pFences->clear();
	std::string szWhy;
	if ( rInput.nTilesX < 2 || rInput.nTilesY < 2 || rInput.nTilesX > 65536 || rInput.nTilesY > 65536 )
		szWhy = "the map has no extent";
	else
		for ( int i = 0; i < 4; ++i )
			if ( !Finite( rInput.vOrigin[i] ) )
				szWhy = "this fence type has no origin for one of its directions";
	if ( szWhy.empty() && ( !CellOnMap( firstTile, rInput ) || !CellOnMap( lastTile, rInput ) ) )
		szWhy = "the fence run leaves the map";
	if ( !szWhy.empty() )
	{
		if ( pWhy != 0 ) *pWhy = szWhy;
		return false;
	}
	std::vector<SPlannedPiece> fences;
	if ( firstTile.x == lastTile.x && firstTile.y == lastTile.y )
	{
		fences.push_back( FenceAt( rInput, firstTile.x, firstTile.y, bCtrl ? 1 : 0 ) );
	}
	else
	{
		const int nDx = firstTile.x > lastTile.x ? firstTile.x - lastTile.x : lastTile.x - firstTile.x;
		const int nDy = firstTile.y > lastTile.y ? firstTile.y - lastTile.y : lastTile.y - firstTile.y;
		const bool bHorizontal = !( nDx < nDy );
		CTPoint<int> last = lastTile;
		if ( bHorizontal )
			last.y = firstTile.y;
		else
			last.x = firstTile.x;
		int nDir = 0, nShiftX = 0, nShiftY = 0;
		if ( bHorizontal )
		{
			if ( firstTile.x > last.x )
				nDir = 1;
			else
			{
				nDir = 3;
				nShiftX = 2;
			}
		}
		else
		{
			if ( firstTile.y > last.y )
			{
				nDir = 0;
				nShiftY = -2;
			}
			else
				nDir = 2;
		}
		std::vector< CTPoint<int> > tiles;
		RasterizeLine( firstTile, last, &tiles );
		for ( size_t i = 0; i < tiles.size(); i += 2 )
			fences.push_back( FenceAt( rInput, tiles[i].x + nShiftX, tiles[i].y + nShiftY, nDir ) );
	}
	const float fMaxX = float( rInput.nTilesX * nAITileSize ), fMaxY = float( rInput.nTilesY * nAITileSize );
	for ( size_t i = 0; i < fences.size(); ++i )
		if ( fences[i].vPos.x < 0.0f || fences[i].vPos.y < 0.0f || fences[i].vPos.x >= fMaxX || fences[i].vPos.y >= fMaxY )
		{
			if ( pWhy != 0 ) *pWhy = "the fence run leaves the map";
			return false;
		}
	if ( pFences != 0 )
		*pFences = fences;
	return true;
}
}
