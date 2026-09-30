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
}
