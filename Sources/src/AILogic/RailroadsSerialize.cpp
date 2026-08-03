#include "StdAfx.h"

#include "RailroadGraph.h"
#include "SerializeOwner.h"
int CEdgePoint::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pEdge, &saver );
	saver.Add( 2, &nPart );
	saver.Add( 3, &fT );

	return 0;
}
int CSplineEdge::SEdgePart::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &spline );
	saver.Add( 2, &fTBegin );
	saver.Add( 3, &fTEnd );

	return 0;
}
int CSplineEdge::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &edgeParts );
	saver.Add( 2, &v1 );
	saver.Add( 3, &v2 );
	saver.Add( 4, &fEdgeLength );

	return 0;
}
int CSimpleSplineEdge::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CSplineEdge*>(this) );

	return 0;
}
int CZeroEdge::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &vFirstPoint );
	saver.Add( 2, &vDir );
	saver.Add( 3, &fTBegin );
	saver.Add( 4, &fTEnd );
	saver.Add( 5, &v1 );
	saver.Add( 6, &v2 );
	saver.Add( 7, &fLength );

	return 0;
}
int CRailroad::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CSplineEdge*>(this) );
	saver.Add( 2, &intersectionPoints );
	saver.Add( 3, &nIntersectionPoints );
	saver.Add( 4, &intersectionPointToGraphNode );

	return 0;
}
int CRailroadsIntersection::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &p1 );
	saver.Add( 2, &p2 );

	return 0;
}
int CRailroadGraphConstructor::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &railroads );
	saver.Add( 2, &intersections );

	return 0;
}
int CRailroadGraph::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CGraph*>(this) );
	saver.Add( 2, &edges );
	saver.Add( 3, &edgeNodes );

	return 0;
}
