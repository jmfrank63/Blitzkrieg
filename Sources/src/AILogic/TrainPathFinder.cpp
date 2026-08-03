#include "StdAfx.h"

#include "TrainPathFinder.h"
#include "PointChecking.h"
#include "RailRoadGraph.h"
#include "TrainPathUnit.h"
extern CRailroadGraph theRailRoadGraph;
void CTrainPathFinder::AnalyzePath( const int v1, const int v2, const float fDistToV1, CEdgePoint *pPoint )
{
	const int pointV1 = pPoint->GetEdge()->GetFirstNode();
	const int pointV2 = pPoint->GetEdge()->GetLastNode();

	if ( ( v1 == pointV1 || v1 == pointV2 ) && ( v2 == pointV1 || v2 == pointV2 ) )
	{
		const float fPathLen = fabs( pStartEdgePoint, pPoint );
		if ( fBestPathLen == -1.0f || fPathLen < fBestPathLen )
		{
			fBestPathLen = fPathLen;
			bestPath.clear();
			pFinishEdgePoint = pPoint;
		}
	}
	else
	{
		theRailRoadGraph.ComputePath( v1, pointV1 );
		if ( theRailRoadGraph.GetPathLength() != -1.0f )
		{
			if ( fBestPathLen == -1.0f || fDistToV1 + theRailRoadGraph.GetPathLength() < fBestPathLen )
			{
				const float fDistToFirstNode = fabs( pPoint, pPoint->GetEdge()->CreateFirstEdgePoint() );
				const float fFullPathLen = fDistToV1 + theRailRoadGraph.GetPathLength() + fDistToFirstNode;
				if ( fBestPathLen == -1.0f || fFullPathLen < fBestPathLen )
				{
					fBestPathLen = fFullPathLen;
					theRailRoadGraph.GetPath( &bestPath );
					int nSize = bestPath.size();
					pFinishEdgePoint = pPoint;
				}
			}
		}

		theRailRoadGraph.ComputePath( v1, pointV2 );
		if ( theRailRoadGraph.GetPathLength() != -1.0f )
		{
			if ( fBestPathLen == -1.0f || fDistToV1 + theRailRoadGraph.GetPathLength() < fBestPathLen )
			{
				const float fDistToSecondNode = fabs( pPoint, pPoint->GetEdge()->CreateLastEdgePoint() );
				const float fFullPathLen = fDistToV1 + theRailRoadGraph.GetPathLength() + fDistToSecondNode;
				if ( fBestPathLen == -1.0f || fFullPathLen < fBestPathLen )
				{
					fBestPathLen = fFullPathLen;
					theRailRoadGraph.GetPath( &bestPath );
					int nSize = bestPath.size();
					pFinishEdgePoint = pPoint;
				}
			}
		}
	}
}
void CTrainPathFinder::SetPathParameters( CTrainPathUnit *_pTrain, const CVec2 &_finishPoint )
{
	pTrain = _pTrain;
	finishPoint = _finishPoint;
	pStartEdgePoint = pTrain->GetCurEdgePoint();
}
bool CTrainPathFinder::CalculatePath()
{
	const int v1 = pStartEdgePoint->GetEdge()->GetFirstNode();
	const int v2 = pStartEdgePoint->GetEdge()->GetLastNode();
	const float fDistToV1 = pStartEdgePoint->GetEdge()->GetLength( pStartEdgePoint, pStartEdgePoint->GetEdge()->CreateFirstEdgePoint() );
	const float fDistToV2 = pStartEdgePoint->GetEdge()->GetLength( pStartEdgePoint, pStartEdgePoint->GetEdge()->CreateLastEdgePoint() );

	/*
	const float fDistToV1 = fabs( pStartEdgePoint, pStartEdgePoint->GetEdge()->CreateFirstEdgePoint() );
	const float fDistToV2 = fabs( pStartEdgePoint, pStartEdgePoint->GetEdge()->CreateLastEdgePoint() );
	*/
	
	std::list< CPtr<CEdgePoint> > edgePoints;
	float fMinDist;
	theRailRoadGraph.GetClosestPoints( finishPoint, &edgePoints, &fMinDist, v1 );
	bestPath.resize( 0 );

	fBestPathLen = -1.0f;
	for ( std::list< CPtr<CEdgePoint> >::iterator iter = edgePoints.begin(); iter != edgePoints.end(); ++iter )
	{
			AnalyzePath( v1, v2, fDistToV1, *iter );
			AnalyzePath( v2, v1, fDistToV2, *iter );
	}

	if ( fBestPathLen >= 0.0f )
	{
		if ( GetPathLength() == 0 )
		{
			if ( pStartEdgePoint->GetEdge()->GetLastNode() != pFinishEdgePoint->GetEdge()->GetLastNode() )
				pStartEdgePoint->Reverse( pFinishEdgePoint->GetEdge() );
			if ( pFinishEdgePoint->Less( *pStartEdgePoint ) )
			{
				const int v1 = pStartEdgePoint->GetEdge()->GetFirstNode();
				const int v2 = pStartEdgePoint->GetEdge()->GetLastNode();

				IEdge *pNewEdge = theRailRoadGraph.GetEdge( v2, v1 );
				pStartEdgePoint->Reverse( pNewEdge );
				pFinishEdgePoint->Reverse( pNewEdge );

				CVec2 temp = pStartEdgePoint->Get2DPoint();
				temp = pFinishEdgePoint->Get2DPoint();
			}
		}
		else
		{
			CPtr<IEdge> pStartEdge = pStartEdgePoint->GetEdge();
			if ( pStartEdge->GetLastNode() != bestPath.front() )
				pStartEdgePoint->Reverse( theRailRoadGraph.GetEdge( pStartEdge->GetLastNode(), bestPath.front() ) );

			CPtr<IEdge> pFinishEdge = pFinishEdgePoint->GetEdge();
			if ( pFinishEdge->GetFirstNode() != bestPath.back() )
				pFinishEdgePoint->Reverse( theRailRoadGraph.GetEdge( bestPath.back(), pFinishEdge->GetFirstNode() ) );
		}

		return true;
	}
	else
		return false;
}
const SVector CTrainPathFinder::GetStartTile() const
{
	return AICellsTiles::GetTile( pStartEdgePoint->Get2DPoint() );
}
const SVector CTrainPathFinder::GetFinishTile() const
{
	return AICellsTiles::GetTile( finishPoint );
}
CEdgePoint* CTrainPathFinder::GetStartEdgePoint() const
{
	return pStartEdgePoint;
}
CEdgePoint* CTrainPathFinder::GetFinishEdgePoint() const
{
	return pFinishEdgePoint;
}
void CTrainPathFinder::StartPathIterating()
{
	iter = bestPath.begin();
}
const int CTrainPathFinder::GetCurPathNode() const
{
	if ( iter == bestPath.end() )
		return -1;
	else
		return *iter;
}
void CTrainPathFinder::Iterate()
{
	if ( iter != bestPath.end() )
		++iter;
}
const int CTrainPathFinder::GetPathLength()	const 
{ 
	if ( fBestPathLen == -1.0f )
		return -1;
	else
		return bestPath.size(); 
}
