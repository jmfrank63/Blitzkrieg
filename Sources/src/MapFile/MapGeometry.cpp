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

// ---------------------------------------------------------------------------
// Entrenchments (04-08, D-13)

namespace {
typedef CTPoint<int> TPathPoint;

// No click is farther out than this (world units): keeps every GPoint an int.
const float fMaxTrenchCoordinate = 1.0e6f;
// A longer path is a mistake, not a trench (the MFC editor had no limit).
const size_t nMaxTrenchPath = 2048;
// Twelve 30-degree steps are a full turn; an arc that has not closed by then
// never will (the MFC loop would spin).
const size_t nMaxArcSteps = 24;

// The project's two-argument fabs (Misc/Tools.h:600) is the Euclidean length
// of ( x, y ): hypot, never two absolute values (Pitfall 20).
float Length( float x, float y )
{
	return float( std::hypot( x, y ) );
}

// CAngle's += and -= (RoadDrawState.cpp:184-237): fmod into [0, 2 pi). An
// angle is wrapped only where the MFC code runs one of those operators; a
// CAngle constructed from GetLineAngle keeps the value as it is.
float WrapAngle( float fValue )
{
	fValue = std::fmod( fValue, FP_2PI );
	return fValue < 0 ? FP_2PI + fValue : fValue;
}

// GetLineAngle: the direction from ( x0, y0 ) to ( x1, y1 ) in [0, 2 pi],
// through Normalize (unchanged when already a unit vector or too short) and
// acos. The cosine is held to [-1, 1] so a rounding of the normalised x past
// 1 is not a NaN (the one guard added); a zero-length step still is one.
float LineAngle( float x0, float y0, float x1, float y1 )
{
	float x = x1 - x0, y = y1 - y0;
	const float u = x * x + y * y;
	if ( !( std::fabs( u - 1.0f ) < FP_EPSILON ) && !( u < FP_EPSILON2 ) )
	{
		const float fInverse = float( 1.0f / std::sqrt( u ) );
		x *= fInverse;
		y *= fInverse;
	}
	float fCosine = x / Length( x, y );
	if ( fCosine > 1.0f )
		fCosine = 1.0f;
	else if ( fCosine < -1.0f )
		fCosine = -1.0f;
	float fAngle = std::acos( fCosine );
	if ( y < 0 )
		fAngle = FP_2PI - fAngle;
	return fAngle;
}

float LineAngle( const TPathPoint &a, const TPathPoint &b )
{
	return LineAngle( float( a.x ), float( a.y ), float( b.x ), float( b.y ) );
}

// SplitLineToSegrments: the integer points from vBegin towards vEnd, fWidth
// apart (each one truncated, the next measured from the truncated one), the
// begin point first when one piece fits at all. False when the line would
// hold more than nMaxTrenchPath points.
bool SplitLine( const CVec2 &vBegin, const CVec2 &vEnd, float fWidth, std::vector<TPathPoint> *pPoints )
{
	pPoints->clear();
	TPathPoint current( int( vBegin.x ), int( vBegin.y ) );
	if ( vBegin.x == vEnd.x && vBegin.y == vEnd.y )
		return true;
	const float fAngle = LineAngle( vBegin.x, vBegin.y, vEnd.x, vEnd.y );
	const float fAll = Length( vEnd.x - vBegin.x, vEnd.y - vBegin.y );
	const float fCos = std::cos( fAngle ), fSin = std::sin( fAngle );
	CVec2 vAdd( fCos * fWidth + float( current.x ), fSin * fWidth + float( current.y ) );
	if ( Length( vAdd.x - float( current.x ), vAdd.y - float( current.y ) ) < fAll )
		pPoints->push_back( current );
	while ( Length( vBegin.x - vAdd.x, vBegin.y - vAdd.y ) < fAll )
	{
		if ( pPoints->size() > nMaxTrenchPath )
			return false;
		current = TPathPoint( int( vAdd.x ), int( vAdd.y ) );
		pPoints->push_back( current );
		vAdd = CVec2( fCos * fWidth + float( current.x ), fSin * fWidth + float( current.y ) );
	}
	return true;
}

// CConnector: nothing for a turn under 30 degrees (the plain difference of the
// two angles, unwrapped, as the MFC compares them); otherwise the arc points
// both ways round - the first 15 degrees on from fBeginAngle, then 30 each,
// until the heading is within 30 degrees of fEndAngle, each point fSegment on
// from the one before (truncated) - and the shorter of the two (anticlockwise
// on a tie). False for an arc that never closes.
bool Connector( const TPathPoint &begin, float fBeginAngle, float fEndAngle, float fSegment, std::vector<TPathPoint> *pPoints )
{
	pPoints->clear();
	const float fStep = FP_PI / 6.0f, fFirstStep = FP_PI / 12.0f;
	if ( std::fabs( fEndAngle - fBeginAngle ) < fStep )
		return true;
	std::vector<TPathPoint> ways[2];
	for ( int nWay = 0; nWay < 2; ++nWay )
	{
		std::vector<TPathPoint> &rWay = ways[nWay];
		float fAngle = fBeginAngle;
		while ( std::fabs( fEndAngle - fAngle ) > fStep )
		{
			if ( rWay.size() >= nMaxArcSteps )
				return false;
			const float fTurn = rWay.empty() ? fFirstStep : fStep;
			fAngle = WrapAngle( nWay == 0 ? fAngle + fTurn : fAngle - fTurn );
			TPathPoint p( int( fSegment * std::cos( fAngle ) ), int( fSegment * std::sin( fAngle ) ) );
			const TPathPoint &rFrom = rWay.empty() ? begin : rWay.back();
			p.x += rFrom.x;
			p.y += rFrom.y;
			rWay.push_back( p );
		}
	}
	*pPoints = ways[1].size() > ways[0].size() ? ways[0] : ways[1];
	return true;
}

void AppendAfterFirst( const std::vector<TPathPoint> &rPoints, std::vector<TPathPoint> *pPath )
{
	for ( size_t i = 1; i < rPoints.size(); ++i )
		pPath->push_back( rPoints[i] );
}

bool Refuse( std::string *pWhy, const char *pszWhy )
{
	if ( pWhy != 0 )
		*pWhy = pszWhy;
	return false;
}

bool TrenchWidthUsable( float fWidth )
{
	return std::isfinite( fWidth ) && fWidth >= 4.0f && fWidth <= 100000.0f;
}

SPlannedPiece TrenchPiece( int nX, int nY, int nType, float fAngle )
{
	CVec3 vPos( float( nX ), float( nY ), 0.0f );
	Vis2AI( &vPos );
	SPlannedPiece piece;
	piece.vPos = CVec3( vPos.x, vPos.y, 0.0f );
	piece.nPackedType = nType;
	piece.nDir = int( fAngle / FP_2PI * 65535 );
	return piece;
}
}

bool TrenchPath( const STrenchPlanInput &rInput, const std::vector<CVec2> &rClicksVis, std::vector< CTPoint<int> > *pPath, std::string *pWhy )
{
	pPath->clear();
	if ( !TrenchWidthUsable( rInput.fLineWidth ) || !TrenchWidthUsable( rInput.fArcWidth ) )
		return Refuse( pWhy, "the entrenchment type has no usable piece length" );
	std::vector<TPathPoint> &rPath = *pPath;
	std::vector<TPathPoint> points, arc;
	for ( size_t nClick = 0; nClick < rClicksVis.size(); ++nClick )
	{
		const CVec2 &rClick = rClicksVis[nClick];
		if ( !Finite( rClick ) || std::fabs( rClick.x ) > fMaxTrenchCoordinate || std::fabs( rClick.y ) > fMaxTrenchCoordinate )
		{
			pPath->clear();
			return Refuse( pWhy, "a point of the trench is not on the map" );
		}
		// m_firstPoint is a GPoint: the click, truncated.
		const TPathPoint first( int( rClick.x ), int( rClick.y ) );
		bool bSplit = true;
		if ( rPath.size() > 1 )
		{
			const TPathPoint last = rPath.back();
			const float fLastAngle = LineAngle( rPath[rPath.size() - 2], last );
			const float fMouseAngle = LineAngle( last, first );
			if ( !Connector( last, fLastAngle, fMouseAngle, rInput.fArcWidth, &arc ) )
			{
				pPath->clear();
				return Refuse( pWhy, "the trench turns in a way the builder cannot follow" );
			}
			if ( arc.empty() )
			{
				// Under 30 degrees: the last direction carried on for the click's
				// distance.
				const float fDistance = Length( float( last.x - first.x ), float( last.y - first.y ) );
				const CVec2 vAdd( std::cos( fLastAngle ) * fDistance + float( last.x ), std::sin( fLastAngle ) * fDistance + float( last.y ) );
				bSplit = SplitLine( CVec2( float( last.x ), float( last.y ) ), vAdd, rInput.fLineWidth, &points );
				AppendAfterFirst( points, &rPath );
			}
			else
			{
				rPath.insert( rPath.end(), arc.begin(), arc.end() );
				const TPathPoint &rArcFirst = arc.front(), &rArcLast = arc.back();
				if ( Length( float( first.x - rArcFirst.x ), float( first.y - rArcFirst.y ) ) >= Length( float( first.x - rArcLast.x ), float( first.y - rArcLast.y ) ) )
				{
					float fArcAngle = arc.size() == 1 ? LineAngle( last, rArcLast ) : LineAngle( arc[arc.size() - 2], rArcLast );
					const float fFirstArcAngle = LineAngle( last, rArcFirst );
					if ( WrapAngle( fFirstArcAngle - fLastAngle ) < FP_PI2 )
						fArcAngle = WrapAngle( fArcAngle + FP_PI / 12.0f );
					else
						fArcAngle = WrapAngle( fArcAngle - FP_PI / 12.0f );
					const float fDistance = Length( float( first.x - rArcLast.x ), float( first.y - rArcLast.y ) );
					const CVec2 vAdd( std::cos( fArcAngle ) * fDistance + float( rArcLast.x ), std::sin( fArcAngle ) * fDistance + float( rArcLast.y ) );
					bSplit = SplitLine( CVec2( float( rArcLast.x ), float( rArcLast.y ) ), vAdd, rInput.fLineWidth, &points );
					AppendAfterFirst( points, &rPath );
				}
			}
		}
		else if ( rPath.empty() )
			rPath.push_back( first );
		else
		{
			bSplit = SplitLine( CVec2( float( rPath[0].x ), float( rPath[0].y ) ), CVec2( float( first.x ), float( first.y ) ), rInput.fLineWidth, &points );
			AppendAfterFirst( points, &rPath );
		}
		if ( !bSplit || rPath.size() > nMaxTrenchPath )
		{
			pPath->clear();
			return Refuse( pWhy, "that trench would be too long" );
		}
	}
	return true;
}

bool PlanEntrenchment( const STrenchPlanInput &rInput, const std::vector<CVec2> &rPointsVis, STrenchPlan *pPlan, std::string *pWhy )
{
	STrenchPlan plan;
	if ( pPlan != 0 )
		*pPlan = plan;
	if ( !TrenchPath( rInput, rPointsVis, &plan.path, pWhy ) )
		return false;
	const std::vector<TPathPoint> &p = plan.path;
	if ( p.size() < 2 )
		return Refuse( pWhy, "the trench is shorter than one piece: click farther on, then double-click" );
	const size_t nLast = p.size() - 1;

	// The terminators first, as the commit makes them: the begin one turned
	// + pi (a CAngle +=, so wrapped), the end one along the last step.
	const float fBegin = WrapAngle( LineAngle( p[0], p[1] ) + FP_PI );
	const float fEnd = LineAngle( p[nLast - 1], p[nLast] );
	if ( !std::isfinite( fBegin ) || !std::isfinite( fEnd ) )
		return Refuse( pWhy, "two points of the trench are the same" );
	plan.pieces.push_back( TrenchPiece( p[0].x, p[0].y, TRENCH_TERMINATOR, fBegin ) );
	plan.pieces.push_back( TrenchPiece( p[nLast].x, p[nLast].y, TRENCH_TERMINATOR, fEnd ) );

	bool bSwitcher = false;
	bool bEndIfSection = false;
	std::vector<int> section( 1, 0 );
	for ( size_t i = 0; i < nLast; ++i )
	{
		float fAngle = LineAngle( p[i], p[i + 1] );
		const float fPrevious = i > 1 ? LineAngle( p[i - 1], p[i] ) : fAngle;
		if ( !std::isfinite( fAngle ) || !std::isfinite( fPrevious ) )
			return Refuse( pWhy, "two points of the trench are the same" );
		int nType = 0;																// 0 straight, 1 arc, 2 arc turned
		if ( Length( float( p[i + 1].x - p[i].x ), float( p[i + 1].y - p[i].y ) ) > double( rInput.fLineWidth ) * 0.9 )
			nType = 0;
		else
			nType = WrapAngle( fPrevious - fAngle ) > FP_PI ? 1 : 2;
		int nPacked = TRENCH_ARC;
		if ( nType == 0 )
		{
			nPacked = bSwitcher ? TRENCH_LINE : TRENCH_FIREPLACE;
			bSwitcher = !bSwitcher;
		}
		if ( nType == 2 )
			fAngle = WrapAngle( fAngle + FP_PI );
		// GPoint's + and / 2: integer, truncated.
		const TPathPoint centre( ( p[i].x + p[i + 1].x ) / 2, ( p[i].y + p[i + 1].y ) / 2 );
		const int nIndex = int( plan.pieces.size() );
		plan.pieces.push_back( TrenchPiece( centre.x, centre.y, nPacked, fAngle ) );
		if ( !bEndIfSection && nType != 0 )
			bEndIfSection = true;
		if ( bEndIfSection && nType == 0 )
		{
			plan.sections.push_back( section );
			section.clear();
			bEndIfSection = false;
		}
		section.push_back( nIndex );
	}
	if ( !section.empty() )
		plan.sections.push_back( section );
	plan.sections.back().push_back( 1 );
	if ( rInput.fMapWidth > 0.0f && rInput.fMapHeight > 0.0f )
		for ( size_t i = 0; i < plan.pieces.size(); ++i )
		{
			const CVec3 &rPos = plan.pieces[i].vPos;
			if ( rPos.x < 0.0f || rPos.y < 0.0f || rPos.x >= rInput.fMapWidth || rPos.y >= rInput.fMapHeight )
				return Refuse( pWhy, "the trench leaves the map" );
		}
	if ( pPlan != 0 )
		*pPlan = plan;
	return true;
}
}
