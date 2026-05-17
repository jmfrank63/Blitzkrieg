#include "stdafx.h"

#include "RailroadGraph.h"
#include "..\Formats\fmtTerrain.h"
#include "..\Formats\fmtMap.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CRailroadGraph theRailRoadGraph;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( IEdge );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CEdgePoint															*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CEdgePoint );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint::CEdgePoint( const CEdgePoint &edgePoint )
{
	pEdge = edgePoint.pEdge;
	nPart = edgePoint.nPart;
	fT = edgePoint.fT;

	NI_ASSERT_T( pEdge != 0, "EdgePoint with NULL edge" );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdgePoint::Init( IEdge *_pEdge, const int _nPart, const float _fT )
{
	pEdge = _pEdge;
	nPart =_nPart;
	fT = _fT;

	NI_ASSERT_T( pEdge != 0, "EdgePoint with NULL edge" );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CEdgePoint::Get2DPoint() const 
{ 
	return pEdge->GetCoordinate( nPart, fT );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CEdgePoint::GetTangent() const
{
	CVec2 vResult = pEdge->GetTangent( nPart, fT );
	Normalize( &vResult );
	return vResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdgePoint::Reverse( IEdge *pReversedEdge )
{
	NI_ASSERT_T( pEdge->GetNParts() == pReversedEdge->GetNParts(), "Wrong edge passed" );
	
	nPart = pReversedEdge->GetNParts() - nPart - 1;
	pEdge = pReversedEdge;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEdgePoint::IsLastPointOfEdge() const
{
	return pEdge->IsLastPoint( nPart, fT );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEdgePoint::IsEqual( CEdgePoint *pEdgePoint ) const
{
	return nPart == pEdgePoint->nPart && fabs( fT - pEdgePoint->fT ) < 0.00001f;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CSplineEdge															*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CSplineEdge );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CSplineEdge::N_PARTS_FOR_LENGTH_CALCULATING = 100;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSplineEdge::CSplineEdge( const SVectorStripeObject &edgeDescriptor )
{
	CVec3 p0, p1, p2, p3;
	Vis2AI( &p3, edgeDescriptor.controlpoints[0] );
	p0 = p1 = p2 = p3;

	const int nControlPointsSize = edgeDescriptor.controlpoints.size();
	edgeParts.resize( nControlPointsSize + 1 );
	for ( int i = 1; i < nControlPointsSize; ++i )
	{
		p0 = p1; p1 = p2; p2 = p3; Vis2AI( &p3, edgeDescriptor.controlpoints[i] );
		edgeParts[i-1].spline.Setup( p0, p1, p2, p3 );
		edgeParts[i-1].fTBegin = 0.0f;
		edgeParts[i-1].fTEnd = 1.0f;
	}

	p0 = p1; p1 = p2; p2 = p3;
	edgeParts[nControlPointsSize-1].spline.Setup( p0, p1, p2, p3 );
	edgeParts[nControlPointsSize-1].fTBegin = 0.0f;
	edgeParts[nControlPointsSize-1].fTEnd = 1.0f;

	p0 = p1; p1 = p2; p2 = p3; 
	edgeParts[nControlPointsSize].spline.Setup( p0, p1, p2, p3 );
	edgeParts[nControlPointsSize].fTBegin = 0.0f;
	edgeParts[nControlPointsSize].fTEnd = 1.0f;

	CalculateEdgeLength();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSplineEdge::Init( CEdgePoint *p1, CEdgePoint *p2 )
{
	if ( p1->GetEdge() != p2->GetEdge() )
		p2->Reverse( p1->GetEdge() );
	
	NI_ASSERT_T( p1->GetEdge() == p2->GetEdge(), "Can't construct edge from two points of different edges" );

	IEdge *pEdgeInterface = p1->GetEdge();
	NI_ASSERT_T( dynamic_cast<CSplineEdge*>(pEdgeInterface) != 0, "Can't initialize spline edge by zero edge" );
	CSplineEdge *pEdge = static_cast<CSplineEdge*>(pEdgeInterface);

	if ( p1->nPart == p2->nPart )
	{
		edgeParts.resize( 1 );
		edgeParts[0].spline = pEdge->edgeParts[p1->nPart].spline;
		edgeParts[0].fTBegin = p1->fT;
		edgeParts[0].fTEnd = p2->fT;
	}
	else if ( p1->nPart < p2->nPart )
	{
		edgeParts.resize( p2->nPart - p1->nPart + 1 );

		for ( int i = p1->nPart; i <= p2->nPart; ++i )
		{
			edgeParts[i - p1->nPart].spline = pEdge->edgeParts[i].spline;
			edgeParts[i - p1->nPart].fTBegin = 0.0f;
			edgeParts[i - p1->nPart].fTEnd = 1.0f;
		}

		edgeParts[0].fTBegin = p1->fT;
		edgeParts[p2->nPart - p1->nPart].fTEnd = p2->fT;
	}
	else
	{
		edgeParts.resize( p1->nPart - p2->nPart + 1 );
		
		for ( int i = p1->nPart; i >= p2->nPart; --i )
		{
			edgeParts[p1->nPart - i].spline = pEdge->edgeParts[i].spline;
			edgeParts[p1->nPart - i].fTBegin = 1.0f;
			edgeParts[p1->nPart - i].fTEnd = 0.0f;
		}

		edgeParts[0].fTBegin = p1->fT;
		edgeParts[p1->nPart - p2->nPart].fTEnd = p2->fT;
	}

	CalculateEdgeLength();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IEdge* CSplineEdge::CreateReversedEdge() const
{
	CSplineEdge *pReversedEdge = new CSimpleSplineEdge();
	pReversedEdge->v1 = v2;
	pReversedEdge->v2 = v1;
	pReversedEdge->fEdgeLength = fEdgeLength;

	const int nParts = GetNParts();
	pReversedEdge->edgeParts.resize( nParts );
	for ( int i = 0; i < nParts; ++i )
	{
		pReversedEdge->edgeParts[i].spline = edgeParts[nParts - i - 1].spline;
		pReversedEdge->edgeParts[i].fTBegin = edgeParts[nParts - i - 1].fTEnd;
		pReversedEdge->edgeParts[i].fTEnd = edgeParts[nParts - i - 1].fTBegin;
	}

	return pReversedEdge;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint* CSplineEdge::CreateFirstEdgePoint()
{
	return new CEdgePoint( this, 0, edgeParts[0].fTBegin );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint* CSplineEdge::CreateLastEdgePoint()
{
	const int nSize = edgeParts.size();
	return 
		new CEdgePoint( this, nSize - 1, edgeParts[nSize-1].fTEnd );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CSplineEdge::CalculateLengthOfEdgePart( const int nPart, const float fBegin, const float fEnd )
{
	float fT = fBegin;
	const float fTAdd = ( fEnd - fBegin ) / N_PARTS_FOR_LENGTH_CALCULATING;

	float fEdgePartLength = 0.0f;
	// �� ���������
	if ( fabs( fBegin - fEnd ) >= 1.0f / ( 4.0f * N_PARTS_FOR_LENGTH_CALCULATING ) )
	{
		for ( int i = 0; i <= N_PARTS_FOR_LENGTH_CALCULATING; ++i )
		{
			fEdgePartLength += fabs( edgeParts[nPart].spline.Get( fT ) - edgeParts[nPart].spline.Get( fT + fTAdd ) );
			fT += fTAdd;
		}
	}

	return fEdgePartLength;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSplineEdge::CalculateEdgeLength()
{
	fEdgeLength = 0.0f;

	for ( int i = 0; i < edgeParts.size(); ++i )
		fEdgeLength += CalculateLengthOfEdgePart( i, edgeParts[i].fTBegin, edgeParts[i].fTEnd );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CSplineEdge::GetFirst2DPoint() const
{
	return 
		edgeParts[0].spline.Get( edgeParts[0].fTBegin );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CSplineEdge::GetLast2DPoint() const
{
	const int nSize = edgeParts.size();
	return 
		edgeParts[ nSize - 1].spline.Get( edgeParts[nSize - 1].fTEnd );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSplineEdge::GetClosestPoints( const CVec2 &vPoint, std::list< CPtr<CEdgePoint> > *pPoints, float *pfMinDist, const float fTolerance )
{
	*pfMinDist = -1.0f;

	for ( int i = 0; i < edgeParts.size(); ++i )
	{
		CVec2 vLocalClosestPoint;
		float fLocalT;
		edgeParts[i].spline.GetClosestPoint( vPoint, &vLocalClosestPoint, &fLocalT, edgeParts[i].fTBegin, edgeParts[i].fTEnd );

		const float fLocalDist = fabs( vPoint - vLocalClosestPoint );

		// ����� ����� �����, ��� ��� ������
		if ( *pfMinDist == -1.0f || *pfMinDist - fLocalDist >= fTolerance )
		{
			pPoints->clear();
			pPoints->push_back( new CEdgePoint( this, i, fLocalT ) );
			*pfMinDist = fLocalDist;
		}
		// ����� ����� ����� �������
		else if ( fabs( *pfMinDist - fLocalDist ) < fTolerance )
		{
			if ( fLocalDist >= *pfMinDist )
				pPoints->push_back( new CEdgePoint( this, i, fLocalT ) );
			else
			{
				*pfMinDist = fLocalDist;

				// ������� ��� ������ ������� �����
				std::list< CPtr<CEdgePoint> >::iterator iter = pPoints->begin();
				while ( iter != pPoints->end() )
				{
					const CVec2 vLocalPoint = (*iter)->Get2DPoint();
					if ( fabs( vLocalPoint - vPoint ) >= *pfMinDist )
						iter = pPoints->erase( iter );
					else
						++iter;
				}

				pPoints->push_back( new CEdgePoint( this, i, fLocalT ) );
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CSplineEdge::GetLength( CEdgePoint *p1, CEdgePoint *p2 )
{
	NI_ASSERT_T( p1->GetEdge() == this && p2->GetEdge() == this, "Wrong points passed" );

	CPtr<CEdgePoint> pGarbage1 = p1;
	CPtr<CEdgePoint> pGarbage2 = p2;

	if ( p1->nPart == p2->nPart )
		return CalculateLengthOfEdgePart( p1->nPart, p1->fT, p2->fT );
	else
	{
		const int nAdd = Sign( p2->nPart - p1->nPart );
		// ����� ���� �� ������
		float fDist = 
			CalculateLengthOfEdgePart( p1->nPart, p1->fT, 
																 nAdd > 0 ? edgeParts[p1->nPart].fTEnd : edgeParts[p1->nPart].fTBegin ) +
			CalculateLengthOfEdgePart( p2->nPart, p2->fT, 
																 nAdd > 0 ? edgeParts[p2->nPart].fTBegin : edgeParts[p2->nPart].fTEnd );

		// ����� ����
		const int nStart = p1->nPart + nAdd;
		const int nFinish = p2->nPart;
		for ( int i = nStart; i != nFinish; i += nAdd )
			fDist += CalculateLengthOfEdgePart( i, edgeParts[i].fTBegin, edgeParts[i].fTEnd );

		return fDist;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint* CSplineEdge::MakeIndentOnOneSpline( const CVec2 &vPointToMeasureDist, const int nPart, const float fTBegin, const float fTEnd, float fDist )
{
	NI_ASSERT_T( nPart < edgeParts.size(), NStr::Format( "Wrong part (%d) passed", nPart ) );
	if ( fDist > -0.000001 && fDist < 0 )
		fDist = 0.0f;
	NI_ASSERT_T( fDist >= 0.0f, NStr::Format( "Negaitve distance passed (%g)", fDist ) );

	CAnalyticBSpline2 &spline = edgeParts[nPart].spline;
	float fLeft = fTBegin;
	float fRight = fTEnd;
	float fMiddle = 0.0f;
	float fMiddleLength = fabs( vPointToMeasureDist - spline.Get( fTEnd ) );
	float fOldLength = fMiddleLength + 10.0f;

	while ( fabs( fOldLength - fMiddleLength ) > 0.0001f )
	{
		fOldLength = fMiddleLength;
		fMiddle = ( fRight + fLeft ) / 2;
		fMiddleLength = fabs( vPointToMeasureDist - spline.Get( fMiddle ) );

		if ( fMiddleLength <= fDist )
			fLeft = fMiddle;
		else
			fRight = fMiddle;
	}

	return new CEdgePoint( this, nPart, fMiddle );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint* CSplineEdge::MakeIndent( const CVec2 &vPointToMeasureDist, CEdgePoint *p1, CEdgePoint *p2, float fDist )
{
	CPtr<CEdgePoint> pGarbage1 = p1;
	CPtr<CEdgePoint> pGarbage2 = p2;

	// ����� � �������� �������, �����������
	if ( p1->nPart > p2->nPart )
	{
		IEdge *pEdge = p1->GetEdge();
		const int v1 = pEdge->GetFirstNode();
		const int v2 = pEdge->GetLastNode();
		
		IEdge *pNewEdge = theRailRoadGraph.GetEdge( v2, v1 );
		p1->Reverse( pNewEdge );
		p2->Reverse( pNewEdge );
		CEdgePoint *pResult = pNewEdge->MakeIndent( vPointToMeasureDist, p1, p2, fDist );

		p1->Reverse( pEdge );
		p2->Reverse( pEdge );

		return pResult;
	}
	else
	{
		if ( p1->nPart == p2->nPart )
			return MakeIndentOnOneSpline( vPointToMeasureDist, p1->nPart, p1->fT, p2->fT, fDist );
		else 
		{
			const float fDistToSplineEnd =
				fabs( vPointToMeasureDist - edgeParts[p1->nPart].spline.Get( edgeParts[p1->nPart].fTEnd ) );

			if ( fDistToSplineEnd >= fDist )
				return MakeIndentOnOneSpline( vPointToMeasureDist, p1->nPart, p1->fT, edgeParts[p1->nPart].fTEnd, fDist );
			else
			{
				int nCurPart = p1->nPart;
				do
				{
					++nCurPart;
					if ( nCurPart >= edgeParts.size() )
						return CreateLastEdgePoint();

					float fDistToSplineEnd;
					if ( nCurPart < p2->nPart )
						fDistToSplineEnd = fabs( vPointToMeasureDist - edgeParts[nCurPart].spline.Get( edgeParts[nCurPart].fTEnd ) );
					else
						fDistToSplineEnd = fabs( vPointToMeasureDist - edgeParts[nCurPart].spline.Get( p2->fT ) );

					if ( fDistToSplineEnd >= fDist )
					{
						if ( nCurPart < p2->nPart )
							return MakeIndentOnOneSpline( vPointToMeasureDist, nCurPart, edgeParts[nCurPart].fTBegin, edgeParts[nCurPart].fTEnd, fDist );
						else
							return MakeIndentOnOneSpline( vPointToMeasureDist, nCurPart, edgeParts[nCurPart].fTBegin, p2->fT, fDist );
					}

					if ( nCurPart == p2->nPart )
						return p2;
				} while ( true );
			}
		}
	}

	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CSplineEdge::GetCoordinate( const int nPart, const float fT ) const
{
	NI_ASSERT_T( nPart < GetNParts(), NStr::Format( "Wrong number of nPart (%d)", nPart ) );
	return 
		edgeParts[nPart].spline.Get( fT );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CSplineEdge::GetTangent( const int nPart, const float fT ) const
{
	NI_ASSERT_T( nPart < GetNParts(), NStr::Format( "Wrong number of nPart (%d)", nPart ) );
	
	CVec2 vResult;
	// ��������� �� ���� �������, ����� �������� ������� �����������
	if ( fT >= 0.99f )
		vResult = edgeParts[nPart].spline.GetDiff1( 0.99f );
	else if ( fT <= 0.01f )
		vResult = edgeParts[nPart].spline.GetDiff1( 0.01f );
	else
		vResult = edgeParts[nPart].spline.GetDiff1( fT );

	if ( edgeParts[nPart].fTBegin > edgeParts[nPart].fTEnd )
		vResult = -vResult;

	Normalize( &vResult );
	return vResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CSplineEdge::GetTangentOfBegin() const
{
	return GetTangent( 0, edgeParts[0].fTBegin );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CSplineEdge::GetTangentOfEnd() const
{
	return
		GetTangent( GetNParts() - 1, edgeParts[GetNParts() - 1].fTEnd );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSplineEdge::IsLastPoint( const int nPart, const float fT ) const 
{ 
	NI_ASSERT_T( nPart < edgeParts.size(), NStr::Format( "Wrong part of edge (%d)", nPart ) );
	return nPart == edgeParts.size() - 1 && fabs( edgeParts[nPart-1].fTEnd - fT ) < 0.00001f; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CSimpleSplineEdge														*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IEdge* CSimpleSplineEdge::CreateEdge( CEdgePoint *p1, CEdgePoint *p2 )
{
	IEdge *pResult = new CSimpleSplineEdge( p1, p2 );
	pResult->SetNodesNumbers( GetFirstNode(), GetLastNode() );

	return pResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CZeroEdge															*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CZeroEdge::CZeroEdge( CEdgePoint *p1, CEdgePoint *p2 )
{
	vFirstPoint = p1->Get2DPoint();
	vDir = p2->Get2DPoint() - vFirstPoint;
	fTBegin = 0.0f;
	fTEnd = 1.0f;
	fLength = fabs( vDir );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IEdge* CZeroEdge::CreateReversedEdge() const
{
	CZeroEdge *pReversedEdge = new CZeroEdge();

	pReversedEdge->vFirstPoint = vFirstPoint;
	pReversedEdge->vDir = vDir;
	pReversedEdge->fTBegin = fTEnd;
	pReversedEdge->fTEnd = fTBegin;
	pReversedEdge->fLength = fLength;

	return pReversedEdge;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CZeroEdge::GetCoordinate( const int nPart, const float fT ) const
{
	NI_ASSERT_T( nPart == 0, NStr::Format( "Wrong part of edge (%d)", nPart ) );
	return vFirstPoint + vDir * fT;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CZeroEdge::GetTangent( const int nPart, const float fT ) const
{
	CVec2 vResult = vDir;
	Normalize( &vResult );
	if ( fTBegin > fTEnd )
		vResult = -vResult;

	Normalize( &vResult );
	return vResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint* CZeroEdge::CreateFirstEdgePoint()
{
	CEdgePoint *pResult = new CEdgePoint;
	pResult->nPart = 0;
	pResult->fT = fTBegin;
	pResult->pEdge = this;

	return pResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint* CZeroEdge::CreateLastEdgePoint()
{
	CEdgePoint *pResult = new CEdgePoint;
	pResult->nPart = 0;
	pResult->fT = fTEnd;
	pResult->pEdge = this;

	return pResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CZeroEdge::GetClosestPoints( const CVec2 &vPoint, std::list< CPtr<CEdgePoint> > *pPoints, float *pfMinDist, const float fTolerance )
{
	const float fDistToPoint1 = fabs( GetFirst2DPoint() - vPoint );
	const float fDistToPoint2 = fabs( GetLast2DPoint() - vPoint );

	*pfMinDist = Min( fDistToPoint1, fDistToPoint2 );
	if ( fabs( fDistToPoint1 - *pfMinDist ) < fTolerance + 0.00001f )
		pPoints->push_back( CreateFirstEdgePoint() );
	if ( fabs( fDistToPoint2 - *pfMinDist ) < fTolerance + 0.00001f )
		pPoints->push_back( CreateLastEdgePoint() );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint* CZeroEdge::MakeIndent( const CVec2 &vPointToMeasureDist, CEdgePoint *p1, CEdgePoint *p2, const float fDist )
{
	CEdgePoint *pResult = new CEdgePoint;
	pResult->pEdge = this;
	pResult->nPart = p2->nPart;
	pResult->fT = p2->fT;

	return pResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CZeroEdge::GetTangentOfBegin() const
{
	return GetTangent( 0, 0 );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CZeroEdge::GetTangentOfEnd() const
{
	return GetTangent( 0, 0 );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IEdge* CZeroEdge::CreateEdge( CEdgePoint *p1, CEdgePoint *p2 )
{
	IEdge *pResult = new CZeroEdge( p1, p2 );
	pResult->SetNodesNumbers( v1, v2 );
	return pResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CZeroEdge::IsLastPoint( const int nPart, const float fT ) const
{
	NI_ASSERT_T( nPart == 0, NStr::Format( "Wrong part of zero edge (%d)", nPart ) );
	return fabs( fT - fTEnd ) < 0.00001f;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CRailroad															*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CRailroad );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const static int N_SPLINE_POINTS = 50;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroad::AddIntersectionPoint( CEdgePoint *pPoint )
{
	if ( intersectionPoints.size() <= nIntersectionPoints )
		intersectionPoints.resize( nIntersectionPoints * 1.5 );

	intersectionPoints[nIntersectionPoints++] = pPoint;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroad::SetEdges( CRailroadGraph *pGraph )
{
//	std::vector< CPtr<CEdgePoint> > intersectionSave = intersectionPoints;
	std::sort( intersectionPoints.begin(), intersectionPoints.begin() + nIntersectionPoints, SEdgeLessFunctional() );

	int nCurNode = pGraph->GetNNodes();
	CPtr<CEdgePoint> point = new CEdgePoint( this, 0, 0.0f );

	for ( int i = 0; i < nIntersectionPoints; ++i )
	{
		intersectionPointToGraphNode[intersectionPoints[i]] = nCurNode + 1;
		
		CPtr<CSplineEdge> pNewEdge = new CSimpleSplineEdge( point, intersectionPoints[i] );
		pNewEdge->SetNodesNumbers( nCurNode, nCurNode + 1 );
		pGraph->AddEdge( pNewEdge );

		++nCurNode;
		point = intersectionPoints[i];
	}

	CPtr<CEdgePoint> point1 = new CEdgePoint( this, GetNParts() - 1, 1.0f );
	CPtr<CSplineEdge> pNewEdge = new CSimpleSplineEdge( point, point1 );
	pNewEdge->SetNodesNumbers( nCurNode, nCurNode + 1 );

	pGraph->AddEdge( pNewEdge );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CRailroad::GetNodeByIntersectionPoint( CEdgePoint *pPoint )
{
	NI_ASSERT_T( intersectionPointToGraphNode.find( pPoint ) != intersectionPointToGraphNode.end(), "Can't find intersection point" );
	return intersectionPointToGraphNode[pPoint];
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									  CRailroadGraphConstructor											*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CRailroadGraphConstructor::F_RAILROAD_WIDTH_2 = sqr( 10.0f );
void CRailroadGraphConstructor::SpliceRailroad( CRailroad *pRailroad, std::vector< CPtr<CEdgePoint> > *pPoints, int *pnLen )
{
	pPoints->resize( ( pRailroad->GetNParts() + 1 ) * N_SPLINE_POINTS );
	
	*pnLen = 0;
	for ( int i = 0; i < pRailroad->GetNParts(); ++i )
	{
		for ( float fT = 0.01f; fT < 1.0f; fT += 1.0f / N_SPLINE_POINTS )
		{
			(*pPoints)[*pnLen] = new CEdgePoint( pRailroad, i, fT );
			if ( *pnLen == 0 || (*pPoints)[*pnLen] != (*pPoints)[*pnLen - 1] )
				++(*pnLen);
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroadGraphConstructor::FindIntersections( CRailroad *pRailroad1, CRailroad *pRailroad2 )
{
	std::vector< CPtr<CEdgePoint> > points1;
	std::vector< CPtr<CEdgePoint> > points2;

	int nPoints1, nPoints2;

	SpliceRailroad( pRailroad1, &points1, &nPoints1 );
	SpliceRailroad( pRailroad2, &points2, &nPoints2 );

	typedef std::list< std::pair<int,int> > CPoints;
	CPoints points;
	bool bContinued = false;
	for ( int i = 0; i < nPoints1; ++i )
	{
		int nBestPoint = -1;
		float fBestDistance = 0.0f;
		for ( int j = 0; j < nPoints2; ++j )
		{
			const float fDist2 = fabs2( points1[i]->Get2DPoint() - points2[j]->Get2DPoint() );
			if ( fDist2 < F_RAILROAD_WIDTH_2 )
			{
				if ( nBestPoint == -1 || fDist2 < fBestDistance )
				{
					nBestPoint = j;
					fBestDistance = fDist2;
				}
			}
		}

		if ( nBestPoint != -1 )
		{
			if ( !points.empty() )
			{
				const CVec2 vPoint1 = points1[points.back().first]->Get2DPoint();
				const CVec2 vPoint2 = points2[points.back().second]->Get2DPoint();
			}
			if ( !bContinued ||
				   bContinued && fBestDistance < fabs2( points1[points.back().first]->Get2DPoint() - points2[points.back().second]->Get2DPoint() ) )
			{
				if ( !bContinued )
				{
					points.push_back( std::pair<int, int>( i, nBestPoint ) );
					bContinued = true;
				}
				else
					points.back() = std::pair<int, int>( i, nBestPoint );
			}
		}
		else
			bContinued = false;
	}

	for ( CPoints::const_iterator iter = points.begin(); iter != points.end(); ++iter )
	{
		intersections.push_back( CRailroadsIntersection( points1[iter->first], points2[iter->second] ) );

		pRailroad1->AddIntersectionPoint( points1[iter->first] );
		pRailroad2->AddIntersectionPoint( points2[iter->second] );

//		NStr::DebugTrace( "Added points: (%g, %g), (%g, %g)\n", points1[iter->first]->Get2DPoint().x, points1[iter->first]->Get2DPoint().y, points2[iter->second]->Get2DPoint().x, points2[iter->second]->Get2DPoint().y );
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroadGraphConstructor::AddIntersectionEdge( const CRailroadsIntersection &intersection, CRailroadGraph *pGraph )
{
	IEdge *pEdge1 = intersection.GetPoint1()->GetEdge();
	IEdge *pEdge2 = intersection.GetPoint2()->GetEdge();

	NI_ASSERT_T( dynamic_cast<CRailroad*>( pEdge1 ) != 0, "Wrong railroad intersection belongs to" );
	NI_ASSERT_T( dynamic_cast<CRailroad*>( pEdge2 ) != 0, "Wrong railroad intersection belongs to" );

	CRailroad *pRailroad1 = static_cast<CRailroad*>( pEdge1 );
	CRailroad *pRailroad2 = static_cast<CRailroad*>( pEdge2 );

	const int v1 = pRailroad1->GetNodeByIntersectionPoint( intersection.GetPoint1() );
	const int v2 = pRailroad2->GetNodeByIntersectionPoint( intersection.GetPoint2() );

	CPtr<IEdge> pZeroEdge = new CZeroEdge( intersection.GetPoint1(), intersection.GetPoint2() );
	pZeroEdge->SetNodesNumbers( v1, v2 );

	pGraph->AddEdge( pZeroEdge );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroadGraphConstructor::Construct( const STerrainInfo &terrain, CRailroadGraph *pGraph )
{
	for ( int i = 0; i < terrain.roads3.size(); ++i )
	{
		if ( terrain.roads3[i].eType == SVectorStripeObjectDesc::TYPE_RAILROAD )
			railroads.push_back( new CRailroad( terrain.roads3[i] ) );
	}

	for ( CRailroadsList::iterator iter = railroads.begin(); iter != railroads.end(); ++iter )
	{
		CRailroadsList::iterator iter_inner( iter );
		std::advance( iter_inner, 1 );
		while ( iter_inner != railroads.end() )
		{
			FindIntersections( *iter, *iter_inner );
			++iter_inner;
		}
	}

	for ( CRailroadsList::iterator iter = railroads.begin(); iter != railroads.end(); ++iter )
	{
//		NStr::DebugTrace( "Edges of railroad\n" );
		(*iter)->SetEdges( pGraph );
	}

//	NStr::DebugTrace( "Intersection points edges:\n" );
	for ( std::list<CRailroadsIntersection>::iterator iter = intersections.begin(); iter != intersections.end(); ++iter )
		AddIntersectionEdge( *iter, pGraph );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												CRailroadGraph														*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const DWORD CreateEdgeKey( const int v1, const int v2 )
{
	return ( DWORD(v1) << 16 ) | v2;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroadGraph::AddEdge( IEdge *pEdge )
{
	const int v1 = pEdge->GetFirstNode();
	const int v2 = pEdge->GetLastNode();
	NI_ASSERT_T( v1 >= 0 && v2 >= 0, "Wrong edge to add" );
	NI_ASSERT_T( v1 < 65535 && v2 < 65535, "Number of node is too big" );

	const DWORD dwEdgeNum = CreateEdgeKey( v1, v2 );
	edges[dwEdgeNum] = pEdge;
	pEdge->SetNodesNumbers( v1, v2 );

	CGraph::AddEdge( v1, v2 );

	const DWORD dwReversedEdgeNum = CreateEdgeKey( v2, v1 );
	IEdge *pReversedEdge = pEdge->CreateReversedEdge();
	pReversedEdge->SetNodesNumbers( v2, v1 );

	edges[dwReversedEdgeNum] = pReversedEdge;
	CGraph::AddEdge( v2, v1 );

	if ( Max( v1, v2 ) >= edgeNodes.size() )
		edgeNodes.resize( ( Max( v1, v2 ) + 1 ) * 1.5 );
	
	if ( edgeNodes[v1] == 0 )
		edgeNodes[v1] = pEdge->CreateFirstEdgePoint();
	if ( edgeNodes[v2] == 0 )
		edgeNodes[v2] = pEdge->CreateLastEdgePoint();

//	NStr::DebugTrace( "Edge (%g, %g) ( %g, %g ), (%d, %d) added\n", pEdge->GetFirst2DPoint().x, pEdge->GetFirst2DPoint().y, pEdge->GetLast2DPoint().x, pEdge->GetLast2DPoint().y, pEdge->GetFirstNode(), pEdge->GetLastNode() );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CRailroadGraph::GetEdgeLength( const int v1, const int v2 )
{
	NI_ASSERT_T( v1 >= 0 && v2 >= 0, "Wrong nodes" );

	const DWORD dwEdgeNum = CreateEdgeKey( v1, v2 );
	NI_ASSERT_T( edges.find( dwEdgeNum ) != edges.end(), NStr::Format( "The edge (%d, %d) not found", v1, v2 ) );

	return edges[dwEdgeNum]->GetLength();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroadGraph::GetClosestPoints( const CVec2 &vPoint, std::list< CPtr<CEdgePoint> > *pPoints, float *pfMinDist, const int vConnectionNode, const float fTolerance )
{
	*pfMinDist = -1.0f;

	for ( std::unordered_map< DWORD, CObj<IEdge> >::iterator iter = edges.begin(); iter != edges.end(); ++iter )
	{
		const DWORD dwNodesKey = iter->first;
		const int nV1 = dwNodesKey >> 16;
		const int nV2 = dwNodesKey & 0xffff;
		// ������ ������� ������ ������, ����� �� ��������� ������ �� ������ �����, � ��� �� ���������� ��������� � ����� ��������� �����
		if ( nV1 < nV2 && ( vConnectionNode == -1 || IsInOneGraphComponent( vConnectionNode, nV1 ) ) && 
				 edges[dwNodesKey]->GetLength() > 0 )
		{
			NI_ASSERT_T( vConnectionNode == -1 || IsInOneGraphComponent( vConnectionNode, nV2 ), "Wrong graph components" );
			
			std::list< CPtr<CEdgePoint> > pointsForEdge;
			float fLocalMinDist;
			IEdge *pEdge = iter->second;
			pEdge->GetClosestPoints( vPoint, &pointsForEdge, &fLocalMinDist );

			// ��� ����� �����, ��� ����� �� ����������
			if ( *pfMinDist == -1.0f || *pfMinDist - fLocalMinDist >= fTolerance )
			{
				*pfMinDist = fLocalMinDist;
				pPoints->clear();
				pPoints->splice( pPoints->end(), pointsForEdge );
			}
			// ���������� �� ���� �������� ���������
			else if ( fabs( fLocalMinDist - *pfMinDist ) <= fTolerance )
			{
				*pfMinDist = Min( fLocalMinDist, *pfMinDist );

				// ��������� ��� ������� ������ ����� �� ������
				std::list< CPtr<CEdgePoint> >::iterator iter = pPoints->begin();
				while ( iter != pPoints->end() )
				{
					const CVec2 vCurPoint = (*iter)->Get2DPoint();
					if ( fabs( vCurPoint - vPoint ) - *pfMinDist >= fTolerance )
						iter = pPoints->erase( iter );
					else
						++iter;
				}

				// �� ����� ����� �������� ������ ���������� �������
				for ( iter = pointsForEdge.begin(); iter != pointsForEdge.end(); ++iter )
				{
					const CVec2 vCurPoint = (*iter)->Get2DPoint();
					if ( fabs( vCurPoint - vPoint ) - *pfMinDist < fTolerance )
						pPoints->push_back( *iter );
				}
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint* CRailroadGraph::GetEdgePoint( const int v ) const
{
	NI_ASSERT_T( v < edgeNodes.size(), "Wrong number of node" );
	NI_ASSERT_T( edgeNodes[v] != 0, NStr::Format( "Edgepoint of the node (%d) hasn't been created", v ) );

	return edgeNodes[v];
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroadGraph::Clear()
{
	edges.clear();
	edgeNodes.clear();

	CGraph::Clear();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IEdge* CRailroadGraph::GetEdge( const int v1, const int v2 )
{
	const DWORD dwEdgeKey = CreateEdgeKey( v1, v2 );

	if ( edges.find( dwEdgeKey ) == edges.end() )
		return 0;
	else
	{
		IEdge *pEdge = edges[dwEdgeKey];
		NI_ASSERT_T( pEdge->GetFirstNode() == v1 && pEdge->GetLastNode() == v2, "Wrong edge in the graph" );

		return pEdge;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroadGraph::LookForPoint( const int v, const CVec2 &vDir, std::unordered_set<int> *pVisitedPoints, std::list<SPointInfo> *pPointsList )
{
	for ( std::list<int>::iterator iter = nodes[v].begin(); iter != nodes[v].end(); ++iter )
	{
		const int v1 = *iter;

		if ( pVisitedPoints->find( v1 ) == pVisitedPoints->end() )
		{
			const DWORD dwEdgeKey = CreateEdgeKey( v, v1 );
			NI_ASSERT_T( edges.find( dwEdgeKey ) != edges.end(), NStr::Format( "Edge not found (%d, %d)", v, v1 ) );

			IEdge *pEdge = edges[dwEdgeKey];

			if ( pEdge->GetLength() != 0 )
			{
				CVec2 vEdgeDir = pEdge->GetTangentOfBegin();
				Normalize( &vEdgeDir );
				const float fMulti = vDir * vEdgeDir;

				if ( fMulti > 0 )
				{
					pVisitedPoints->insert( v1 );
					pPointsList->push_back( SPointInfo( v1, vDir ) );
				}
			}
			else
			{
				pVisitedPoints->insert( v1 );
				pPointsList->push_back( SPointInfo( v1, vDir ) );
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRailroadGraph::GetMovablePoint( const int v, const CVec2 &vDir, std::unordered_set<int> *pVisitedPoints, std::list<SPointInfo> *pPointsList )
{
	NI_ASSERT_T( v < GetNNodes(), NStr::Format( "Wrong node passed (%d)", v ) );
	LookForPoint( v, vDir, pVisitedPoints, pPointsList );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEdgePoint* CRailroadGraph::MakeIndent( const CVec2 &vDir, CEdgePoint *pPoint, const float fDist )
{
	if ( fDist == 0.0f )
		return pPoint;
	CPtr<CEdgePoint> pGarbage = pPoint;

	IEdge *pEdge = 0;
	CPtr<CEdgePoint> pPointOnRightEdge;
	// �������� ����� �����
	if ( pPoint->GetTangent() * vDir >= 0 )
	{
		pEdge = GetEdge( pPoint->GetEdge()->GetFirstNode(), pPoint->GetEdge()->GetLastNode() );
		pPointOnRightEdge = pPoint;
	}
	// ������ �����
	else
	{
		pEdge = GetEdge( pPoint->GetEdge()->GetLastNode(), pPoint->GetEdge()->GetFirstNode() );
		pPointOnRightEdge = new CEdgePoint( *pPoint );
		pPointOnRightEdge->Reverse( pEdge );
	}
	const CVec2 vPointToMeasureDist = pPointOnRightEdge->Get2DPoint();

	CPtr<CEdgePoint> pLastNodePoint = pEdge->CreateLastEdgePoint();
	const CVec2 vLastNodePoint2D = pLastNodePoint->Get2DPoint();

	const float fDistToFirstNodePoint2 = fabs2( vPointToMeasureDist - vLastNodePoint2D );
	// ������� ����� �� ����� pEdge
	if ( fDistToFirstNodePoint2 >= sqr(fDist) )
	{
		CEdgePoint *pPoint = pEdge->MakeIndent( vPointToMeasureDist, pPointOnRightEdge, pLastNodePoint, fabs( fDist ) );
		return pPoint;
	}
	// ����� ������ ��������� ����
	else
	{
		std::list<int> movablePoints;

		int v1 = pEdge->GetLastNode();
		int v2 = 0;
		bool bFinished = false;
		
		std::unordered_set<int> visitedPoints;
		std::list<SPointInfo> points;
		points.push_back( SPointInfo( v1, vDir ) );
		visitedPoints.insert( v1 );

		while ( !bFinished && !points.empty() )
		{
			v1 = points.front().v;
			const CVec2 vNewDir = points.front().vDir;
			points.pop_front();

			std::list<SPointInfo>::iterator iter = points.end();
			--iter;
			GetMovablePoint( v1, vNewDir, &visitedPoints, &points );

			// ����� �� �������
			if ( points.empty() )
				return 0;
			else
			{
				++iter;
				while ( iter != points.end() )
				{
					v2 = iter->v;
					IEdge *pEdge = GetEdge( v1, v2 );

					const CVec2 vV2Point2D = pEdge->GetLast2DPoint();
					const float fLength = fabs( vPointToMeasureDist - vV2Point2D );

					if ( fLength > fDist )
						bFinished = true;
					else
					{
						if ( pEdge->GetLength() != 0 )
							iter->vDir = pEdge->GetTangentOfEnd();
					}

					++iter;
				}
			}
		}

		IEdge* pLastEdgePretendent = GetEdge( v1, v2 );
		CPtr<CEdgePoint> pFirstEdgePoint = pLastEdgePretendent->CreateFirstEdgePoint();
		CPtr<CEdgePoint> pLastEdgePoint = pLastEdgePretendent->CreateLastEdgePoint();
		CEdgePoint *pPoint = pLastEdgePretendent->MakeIndent( vPointToMeasureDist, pFirstEdgePoint, pLastEdgePoint, fDist );

		return pPoint;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float fabs( CEdgePoint *p1, CEdgePoint *p2 )
{
	// ����� ���������
	CPtr<CEdgePoint> pGarbageP1 = p1;
	CPtr<CEdgePoint> pGarbageP2 = p2;

	CPtr<IEdge> pEdge = p1->GetEdge();
	CPtr<IEdge> pOldP2Edge;
	if ( pEdge != p2->GetEdge() )
	{
		NI_ASSERT_T( pEdge->GetFirstNode() == p2->GetEdge()->GetLastNode() && pEdge->GetLastNode() == p2->GetEdge()->GetFirstNode(), "Can't fand distance between point on different edges" );
		pOldP2Edge = p2->GetEdge();
		p2->Reverse( pEdge );
	}

	const float fResult = pEdge->GetLength( p1, p2 );

	if ( pOldP2Edge != 0 )
		p2->Reverse( pOldP2Edge );

	return fResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
