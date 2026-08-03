#include "StdAfx.h"

#include "../Misc/Spline.h"
#include "../Formats/fmtTerrain.h"
#include "VSO_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const float CVSOBuilder::DEFAULT_WIDTH = 120.0f;
const float CVSOBuilder::DEFAULT_HEIGHT = 0.0f;
const float CVSOBuilder::DEFAULT_STEP = 30.0f;
const float CVSOBuilder::DEFAULT_OPACITY = 1.0f;

int CVSOBuilder::SliceSpline( const CAnalyticBSpline2 &spline,
															std::list<SVectorStripeObjectPoint> *pPoints,
															float *pfRest,
															const float fStep )
{
	const float fSplineLength = spline.GetLengthAdaptive( 1 );
	const float dt = fStep / ( fSplineLength * 10.0f );
	int nCounter = 0;
	float fLen = *pfRest + fStep;
	float fLastT, fT = 0;
	CVec2 vLastPos, vPos = spline.Get( fT );
	while ( 1 ) 
	{
		fLen -= fStep;
		while ( fLen < fStep ) 
		{
			vLastPos = vPos;
			fT += dt;
			vPos = spline.Get( fT );
			fLen += fabs( vLastPos - vPos );
			if ( fT > 1 ) 
				break;
		}
		if ( (fT > 1) || (fLen < fStep) ) 
			break;
		SVectorStripeObjectPoint point;
		point.vPos = spline( fT );
		point.vNorm = spline.GetDiff1( fT );
		point.vNorm.Set( -point.vNorm.y, point.vNorm.x, 0 );
		point.bKeyPoint = false;
		Normalize( &point.vNorm );
		point.fRadius = spline.GetCurvatureRadius( fT );
		pPoints->push_back( point );
		fLastT = fT;
		++nCounter;
	}
	pPoints->back().bKeyPoint = true;
	*pfRest = spline.GetLength( fLastT, 1.0f, 100 );
	return nCounter;
}
/*
int CVSOBuilder::SliceSpline( const CAnalyticBSpline2 &spline,
															std::list<SVectorStripeObjectPoint> *pPoints,
															float *pfRest,
															const float fStep )
{
	int nCounter = 0;
	const float dt = spline.GetStep( fStep );
	float t = *pfRest == 0 ? dt : spline.GetStep( fStep - *pfRest );
	for ( ; t <= 1; t += dt, ++nCounter )
	{
		pPoints->push_back( SVectorStripeObjectPoint() );
		SVectorStripeObjectPoint &point = pPoints->back();
		point.vPos = spline( t );
		point.vNorm = spline.GetDiff1( t );
		point.vNorm.Set( -point.vNorm.y, point.vNorm.x, 0 );
		point.bKeyPoint = false;
		Normalize( &point.vNorm );
		point.fRadius = spline.GetCurvatureRadius( t );
	}
	pPoints->back().bKeyPoint = true;
	*pfRest = spline.GetLength() - spline.GetLength( t - dt );
	return nCounter;
}
*/
void CVSOBuilder::SampleCurve( const std::vector<CVec3> &rControlPoints,
															 std::vector<SVectorStripeObjectPoint> *pPoints,
															 float fStep, 
															 float fWidth,
															 float fOpacity )
{
	NI_ASSERT_T( rControlPoints.size() > 1 != 0,
							 NStr::Format( "Invalid size: %d\n", rControlPoints.size() ) );
	NI_ASSERT_T( pPoints != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPoints ) );

	std::vector<CVec3> plots;
	plots.push_back( rControlPoints[0] - 
									 ( rControlPoints[1] - rControlPoints[0] ) );
	for( std::vector<CVec3>::const_iterator controlPointsIterator = rControlPoints.begin(); controlPointsIterator != rControlPoints.end(); ++controlPointsIterator )
	{
		plots.push_back( *controlPointsIterator );
	}
	plots.push_back( rControlPoints[rControlPoints.size() - 1] + 
									 ( rControlPoints[rControlPoints.size() - 1] - rControlPoints[rControlPoints.size() - 2] ) );

	const float fSplineStep = fStep;
	float fRestLength = fSplineStep - 1e-8f;
	std::list<SVectorStripeObjectPoint> points;
	CAnalyticBSpline2 spline;
	int nCounter = 0;
	for ( int nPlotsIndex = 0; nPlotsIndex != ( plots.size() - 3 ); ++nPlotsIndex )
	{
		spline.Setup( plots[nPlotsIndex], plots[nPlotsIndex + 1], plots[nPlotsIndex + 2], plots[nPlotsIndex + 3] );
		nCounter += SliceSpline( spline, &points, &fRestLength, fSplineStep );
	}
	if ( !points.empty() )
	{
		points.front().bKeyPoint = true;
	}

	for ( std::list<SVectorStripeObjectPoint>::const_iterator pointIterator = points.begin(); pointIterator != points.end(); ++pointIterator )
	{
		pPoints->push_back( *pointIterator );
		( *pPoints )[pPoints->size() - 1].fWidth = fWidth;
		( *pPoints )[pPoints->size() - 1].fOpacity = fOpacity;
	}
}

void CVSOBuilder::SmoothCurveWidth( std::vector<SVectorStripeObjectPoint> *pPoints )
{
	NI_ASSERT_T( pPoints != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPoints ) );

	std::vector<int> keyPoints;
	std::vector<float> widths;
	widths.reserve( pPoints->size() );
	for ( int nPointIndex = 0; nPointIndex < pPoints->size(); ++nPointIndex )
	{
		if ( ( *pPoints )[nPointIndex].bKeyPoint )
		{
			keyPoints.push_back( nPointIndex );
		}
		widths.push_back( ( *pPoints )[nPointIndex].fWidth );
	}

	std::vector<int> indices;
	indices.reserve( keyPoints.size() + 4 );
	indices.push_back( 0 );
	indices.push_back( 0 );
	for ( int nKeyPointIndex = 0; nKeyPointIndex < keyPoints.size(); ++nKeyPointIndex )
	{
		indices.push_back( nKeyPointIndex );
	}
	indices.push_back( keyPoints.size() - 1 );
	indices.push_back( keyPoints.size() - 1 );

	CAnalyticBSpline spline;
	for ( int nIndex = 0; nIndex != ( indices.size() - 3 ); ++nIndex )
	{
		const int idx0 = keyPoints[ indices[nIndex + 0] ];
		const int idx1 = keyPoints[ indices[nIndex + 1] ];
		const int idx2 = keyPoints[ indices[nIndex + 2] ];
		const int idx3 = keyPoints[ indices[nIndex + 3] ];
		if ( idx2 <= idx1 )
		{
			continue;
		}
		spline.Setup( ( *pPoints )[idx0].fWidth,
									( *pPoints )[idx1].fWidth,
									( *pPoints )[idx2].fWidth,
									( *pPoints )[idx3].fWidth );
		
		for ( int nInnerIndex = idx1; nInnerIndex != idx2; ++nInnerIndex )
		{
			const float fRatio = float( nInnerIndex - idx1 ) / float( idx2 - idx1 );
			widths[nInnerIndex] = spline( fRatio );
		}
	}

	for ( int nPointIndex = 0; nPointIndex < pPoints->size(); ++nPointIndex )
	{
		( *pPoints )[nPointIndex].fWidth = widths[nPointIndex];
	}
}

bool CVSOBuilder::UpdateZ( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, CVec3 *pPos )
{
	NI_ASSERT_T( pPos != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPos ) );
	
	return CVertexAltitudeInfo::GetHeight( rAltitude, pPos->x, pPos->y, &( pPos->z ) );
	/**
	if ( IAILogic *pAILogic = GetSingleton<IAILogic>() )
	{
		CVec2 vPos2;
		Vis2AI( &vPos2, pPos->x, pPos->y );
		
		pPos->z = AI2VisZ( pAILogic->GetZ( vPos2 ) );
		return true;
	}
	/**/
}

bool CVSOBuilder::UpdateZ( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, SVectorStripeObject *pVectorStripeObject )
{
	NI_ASSERT_T( pVectorStripeObject != 0,
							 NStr::Format( "Wrong parameter: %x\n", pVectorStripeObject ) );
	
	bool bResult = true;
	for ( std::vector<CVec3>::iterator controlPointIterator = pVectorStripeObject->controlpoints.begin(); controlPointIterator != pVectorStripeObject->controlpoints.end(); ++controlPointIterator )
	{
		if ( !UpdateZ( rAltitude, &( *controlPointIterator ) ) )
		{
			bResult = false;
		}
	}
	for ( std::vector<SVectorStripeObjectPoint>::iterator pointIterator = pVectorStripeObject->points.begin(); pointIterator != pVectorStripeObject->points.end(); ++pointIterator )
	{
		if ( !UpdateZ( rAltitude, &( pointIterator->vPos ) ) )
		{
			bResult = false;
		}
	}
	return bResult;
}

bool CVSOBuilder::Update( SVectorStripeObject *pVectorStripeObject, bool bKeepKeyPoints, float fStep, float fWidth, float fOpacity )
{
	NI_ASSERT_T( pVectorStripeObject != 0,
							 NStr::Format( "Wrong parameter: %x\n", pVectorStripeObject ) );
	
	SBackupKeyPoints backupKeyPoints;
	if ( bKeepKeyPoints )
	{
		backupKeyPoints.SaveKeyPoints( ( *pVectorStripeObject ) );
	}

	pVectorStripeObject->points.clear();
	SampleCurve( pVectorStripeObject->controlpoints, &( pVectorStripeObject->points ), fStep, fWidth, fOpacity );

	if ( bKeepKeyPoints )
	{
		backupKeyPoints.LoadKeyPoints( pVectorStripeObject );
	}

	SmoothCurveWidth( &( pVectorStripeObject->points ) );
	return true;
}

bool CVSOBuilder::GetVSOPointPolygon( const SVectorStripeObject &rVectorStripeObject, int nPointIndex, std::vector<CVec3> *pPolygon, float fRelWidth )
{
	NI_ASSERT_T( pPolygon != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPolygon ) );
	
	NI_ASSERT_T( ( nPointIndex >= 0 ) && ( nPointIndex < rVectorStripeObject.points.size() ),
							 NStr::Format( "Invalid argument: %d (%d)\n", nPointIndex, rVectorStripeObject.points.size() ) );

	pPolygon->push_back( CVec3( rVectorStripeObject.points[nPointIndex].vPos.x + ( rVectorStripeObject.points[nPointIndex].vNorm.x * rVectorStripeObject.points[nPointIndex].fWidth * fRelWidth ),
															rVectorStripeObject.points[nPointIndex].vPos.y + ( rVectorStripeObject.points[nPointIndex].vNorm.y * rVectorStripeObject.points[nPointIndex].fWidth * fRelWidth ),
															0.0f ) );
	pPolygon->push_back( CVec3( rVectorStripeObject.points[nPointIndex + 1].vPos.x + ( rVectorStripeObject.points[nPointIndex + 1].vNorm.x * rVectorStripeObject.points[nPointIndex + 1].fWidth * fRelWidth ),
															rVectorStripeObject.points[nPointIndex + 1].vPos.y + ( rVectorStripeObject.points[nPointIndex + 1].vNorm.y * rVectorStripeObject.points[nPointIndex + 1].fWidth * fRelWidth ),
															0.0f ) );
	pPolygon->push_back( CVec3( rVectorStripeObject.points[nPointIndex + 1].vPos.x - ( rVectorStripeObject.points[nPointIndex + 1].vNorm.x * rVectorStripeObject.points[nPointIndex + 1].fWidth * fRelWidth ),
															rVectorStripeObject.points[nPointIndex + 1].vPos.y - ( rVectorStripeObject.points[nPointIndex + 1].vNorm.y * rVectorStripeObject.points[nPointIndex + 1].fWidth * fRelWidth ),
															0.0f ) );
	pPolygon->push_back( CVec3( rVectorStripeObject.points[nPointIndex].vPos.x - ( rVectorStripeObject.points[nPointIndex].vNorm.x * rVectorStripeObject.points[nPointIndex].fWidth * fRelWidth ),
															rVectorStripeObject.points[nPointIndex].vPos.y - ( rVectorStripeObject.points[nPointIndex].vNorm.y * rVectorStripeObject.points[nPointIndex].fWidth * fRelWidth ),
															0.0f ) );
	return true;
}

bool CVSOBuilder::GetPointsSequence( const SVSOCircle &rCircleBegin, const SVSOCircle &rCircleEnd, int nSegmentsCountBegin, int nSegmentsCountEnd, std::list<CVec2> *pPointsSequence )
{
	NI_ASSERT_TF( pPointsSequence != 0,
							  NStr::Format( "Wrong parameter: pPointsSequence %x\n", pPointsSequence ),
								return false );

	CVec2 vBeginTangentPoint( VNULL2 );
	CVec2 vEndTangentPoint( VNULL2 );

	if ( ( rCircleBegin.classifyRotation != rCircleEnd.classifyRotation ) &&
			 ( fabs( rCircleBegin.r - rCircleEnd.r ) < FP_EPSILON ) )
	{
		CVec2 vAdditionalRadius = GetNormal( rCircleEnd.center - rCircleBegin.center );
		Normalize( &vAdditionalRadius );
		if ( rCircleBegin.classifyRotation == CR_COUNTERCLOCKWISE )
		{
			vAdditionalRadius *= -rCircleBegin.r;
		}
		else
		{
			vAdditionalRadius *= rCircleBegin.r;
		}

		vBeginTangentPoint = rCircleBegin.center + vAdditionalRadius;
		vEndTangentPoint = rCircleEnd.center + vAdditionalRadius;
	}
	else
	{
		SVSOCircle newCircleEnd = rCircleEnd;
		if ( rCircleBegin.classifyRotation == newCircleEnd.classifyRotation )
		{
			newCircleEnd.r += rCircleBegin.r;
		}
		else
		{
			newCircleEnd.r -= rCircleBegin.r;
			if ( newCircleEnd.r < 0 )
			{
				newCircleEnd.classifyRotation = GetNegativeClassifyRotation( newCircleEnd.classifyRotation );
				newCircleEnd.r *= ( -1 );
			}
		}
		
		CVec2 vTangentPoint;
		if ( !newCircleEnd.GetTangentPoint( rCircleBegin.center, &vTangentPoint ) )
		{
			return false;
		}
		
		CVec2 vAdditionalRadius = vTangentPoint - newCircleEnd.center;
		if ( newCircleEnd.classifyRotation != rCircleEnd.classifyRotation )
		{
			vAdditionalRadius *= ( -1 );
		}
		vAdditionalRadius *= rCircleEnd.r / newCircleEnd.r;

		vBeginTangentPoint = rCircleBegin.center + vAdditionalRadius - ( vTangentPoint - newCircleEnd.center );
		vEndTangentPoint = rCircleEnd.center + vAdditionalRadius;
	}

	rCircleBegin.GetPointsSequence( vBeginTangentPoint, nSegmentsCountBegin, pPointsSequence );
	std::list<CVec2> endPointsSequence;
	rCircleEnd.GetPointsSequence( vEndTangentPoint, nSegmentsCountEnd, &endPointsSequence );
	for ( std::list<CVec2>::reverse_iterator pointIterator = endPointsSequence.rbegin(); pointIterator!= endPointsSequence.rend(); ++pointIterator )
	{
		pPointsSequence->push_back( *pointIterator );
	}
	return true;
}

bool CVSOBuilder::GetPointsSequence( const CVec2 &vBegin0, const CVec2 &vEnd0, float fRadius0, int nSegmentsCount0, bool bBegin0,
																		 const CVec2 &vBegin1, const CVec2 &vEnd1, float fRadius1, int nSegmentsCount1, bool bBegin1,
																		 std::list<CVec2> *pPointsSequence )
{
	NI_ASSERT_TF( pPointsSequence != 0,
							  NStr::Format( "Wrong parameter: pPointsSequence %x\n", pPointsSequence ),
								return false );

	SVSOCircle circle00;
	SVSOCircle circle01;
	SVSOCircle circle10;
	SVSOCircle circle11;

	circle00.CreateVSOCircleFromDirection( vBegin0, vEnd0, fRadius0, CR_CLOCKWISE, bBegin0 );
	circle01.CreateVSOCircleFromDirection( vBegin0, vEnd0, fRadius0, CR_COUNTERCLOCKWISE, bBegin0 );
	circle10.CreateVSOCircleFromDirection( vBegin1, vEnd1, fRadius1, CR_CLOCKWISE, bBegin1 );
	circle11.CreateVSOCircleFromDirection( vBegin1, vEnd1, fRadius1, CR_COUNTERCLOCKWISE, bBegin1 );

	std::list<CVec2> road0010points;
	std::list<CVec2> road0011points;
	std::list<CVec2> road0110points;
	std::list<CVec2> road0111points;

	float fPerimeter0010 = -1.0f;
	float fPerimeter0011 = -1.0f;
	float fPerimeter0110 = -1.0f;
	float fPerimeter0111 = -1.0f;

	if ( GetPointsSequence( circle00, circle10, nSegmentsCount0, nSegmentsCount1, &road0010points ) )
	{
		fPerimeter0010 = GetPolygonPerimeter( road0010points );
	}
	if ( GetPointsSequence( circle00, circle11, nSegmentsCount0, nSegmentsCount1, &road0011points ) )
	{
		fPerimeter0011 = GetPolygonPerimeter( road0011points );
	}
	if ( GetPointsSequence( circle01, circle10, nSegmentsCount0, nSegmentsCount1, &road0110points ) )
	{
		fPerimeter0110 = GetPolygonPerimeter( road0110points );
	}
	if ( GetPointsSequence( circle01, circle11, nSegmentsCount0, nSegmentsCount1, &road0111points ) )
	{
		fPerimeter0111 = GetPolygonPerimeter( road0111points );
	}
	
	int nIndex = -1;
	float nMinPerimeter = fPerimeter0010 + fPerimeter0011 + fPerimeter0110 + fPerimeter0111 + 1.0f;
	
	if ( ( fPerimeter0010 >= 0 ) && ( fPerimeter0010 < nMinPerimeter ) )
	{
		nMinPerimeter = fPerimeter0010;
		nIndex = 0;
	}
	if ( ( fPerimeter0011 >= 0 ) && ( fPerimeter0011 < nMinPerimeter ) )
	{
		nMinPerimeter = fPerimeter0011;
		nIndex = 1;
	}
	if ( ( fPerimeter0110 >= 0 ) && ( fPerimeter0110 < nMinPerimeter ) )
	{
		nMinPerimeter = fPerimeter0110;
		nIndex = 2;
	}
	if ( ( fPerimeter0111 >= 0 ) && ( fPerimeter0111 < nMinPerimeter ) )
	{
		nIndex = 3;
	}

	if ( nIndex < 0 )
	{
		return false;
	}
	if ( nIndex == 0 )
	{
		pPointsSequence->insert( pPointsSequence->end(), road0010points.begin(), road0010points.end() );
	}
	else if ( nIndex == 1 )
	{
		pPointsSequence->insert( pPointsSequence->end(), road0011points.begin(), road0011points.end() );
	}
	else if ( nIndex == 2 )
	{
		pPointsSequence->insert( pPointsSequence->end(), road0110points.begin(), road0110points.end() );
	}
	else
	{
		pPointsSequence->insert( pPointsSequence->end(), road0111points.begin(), road0111points.end() );
	}
	return true;
}

bool CVSOBuilder::FindPath( const CVec2 &vBegin0, const CVec2 &vEnd0, bool bBegin0,
														const CVec2 &vBegin1, const CVec2 &vEnd1, bool bBegin1,
														float fRadius, int nSegmentsCount, float fMinEdgeLength, float fDistance, float fDisturbance,
														std::list<CVec2> *pPointsSequence, const std::vector<std::vector<CVec2> > &rLockedPolygons, std::list<CVec2> *pUsedPoints,
														int nDepth )
{
	if ( nDepth < 0 )
	{
		return false;
	}
	if ( !GetPointsSequence( vBegin0, vEnd0, fRadius, nSegmentsCount, bBegin0, vBegin1, vEnd1, fRadius, nSegmentsCount, bBegin1, pPointsSequence ) )
	{
		return false;
	}
	RandomizeEdges<std::list<CVec2>, CVec2>( ( *pPointsSequence ), 10, fDistance, CTPoint<float>( 0.0f, fDisturbance ), pPointsSequence, fMinEdgeLength, 32 * 16 * fWorldCellSize, false );
	UniquePolygon<std::list<CVec2>, CVec2>( pPointsSequence, RMGC_MINIMAL_VIS_POINT_DISTANCE );
	return true;

	/**
	std::list<CVec2>::const_iterator currentPathIterator0 = currentPath0.begin();
	std::list<CVec2>::const_iterator currentPathIterator1 = currentPath0.begin();
	++currentPathIterator1;
	while ( currentPathIterator1 != currentPath0.end() )
	{
		for ( std::vector<std::vector<CVec2> >::const_iterator lockedPolygonIterator = rLockedPolygons.begin(); lockedPolygonIterator != rLockedPolygons.end(); ++lockedPolygonIterator )
		{
			if ( lockedPolygonIterator->size() < 2 )
			{
				continue;
			}
			
			std::vector<CVec2>::const_iterator currentPointIterator0;
			std::vector<CVec2>::const_iterator currentPointIterator1;
			float fCrossPoint;
			EClassifyIntersection classifyIntersection = ClassifyCross( ( *lockedPolygonIterator ), ( *currentPathIterator0 ), ( *currentPathIterator1 ), &currentPointIterator0, &currentPointIterator1, &fCrossPoint );
			if ( classifyIntersection == CI_SKEW_CROSS )
			{
				bool bBeginUsed = false;
				bool bEndUsed = false;
				for ( std::list<CVec2>::const_iterator usedPointsIterator = pUsedPoints->begin(); usedPointsIterator != pUsedPoints->end(); ++usedPointsIterator )
				{
					if ( ( *usedPointsIterator ) == ( *currentPointIterator0 ) )
					{
						bBeginUsed = true;
					}
					if ( ( *usedPointsIterator ) == ( *currentPointIterator1 ) )
					{
						bEndUsed = true;
					}
					if ( bBeginUsed && bEndUsed )
					{
						return false;
					}
				}

				CVec2 v0;
				CVec2 v1;
				CVec2 v2;
				if ( ( fCrossPoint < 0.5f ) && ( !bBeginUsed ) )
				{
					if ( currentPointIterator0 == lockedPolygonIterator->begin() )
					{
						v0 = ( *( lockedPolygonIterator->rbegin() ) );
					}
					else
					{
						--currentPointIterator0;
						v0 = ( *currentPointIterator0 );
						++currentPointIterator0;
					}
					v1 = ( *currentPointIterator0 );
					v2 = ( *currentPointIterator1 );
				}
				else
				{
					v0 = ( *currentPointIterator0 );
					v1 = ( *currentPointIterator1 );
					if ( currentPointIterator1 == ( --( lockedPolygonIterator->end() ) ) )
					{
						v2 = ( *( lockedPolygonIterator->begin() ) );
					}
					else
					{
						++currentPointIterator1;
						v2 = ( *currentPointIterator1 );
						--currentPointIterator1;
					}
				}
				pUsedPoints->push_front( v1 );

				CVec2 v01 = ( v0 - v1 );
				CVec2 v21 = ( v2 - v1 );
				Normalize( &v01 );
				Normalize( &v21 );
				CVec2 vNormal = ( v01 + v21 );
				Normalize( &vNormal );
				EClassifyRotation classifyRotation0 = ClassifyRotation( *lockedPolygonIterator );
				EClassifyRotation classifyRotation1 = ClassifyRotation( v0, v1, v2 );
				if ( classifyRotation0 == classifyRotation1 )
				{
					vNormal *= ( -1 );
				}
				CVec2 vNewBegin = v1 + vNormal * fRadius;
				CVec2 vNewEnd0 = vNewBegin + GetNormal( vNormal );
				CVec2 vNewEnd1 = vNewBegin - GetNormal( vNormal );

				GetClassifyEdgeInnerSpace( classifyRotation0 );
				if ( ClassifyEdge( ( *currentPathIterator0 ), ( *currentPathIterator1 ), v1 ) == CE_RIGHT )
				{
					CVec2 vTemp = vNewEnd0;
					vNewEnd0 = vNewEnd1;
					vNewEnd1 = vTemp;
				}
				currentPath0.clear();
				std::list<CVec2> currentPath1;				
				if ( FindPath( vBegin0, vEnd0, bBegin0,
											 vNewBegin, vNewEnd0, true,
											 fRadius, nSegmentsCount, fMinEdgeLength,
											 &currentPath0, rLockedPolygons, pUsedPoints,
											 nDepth - 1 ) &&
						 FindPath( vNewBegin, vNewEnd1, true,
											 vBegin1, vEnd1, bBegin1,
											 fRadius, nSegmentsCount, fMinEdgeLength,
											 &currentPath1, rLockedPolygons, pUsedPoints,
											 nDepth - 1 ) )
				{
					pPointsSequence->insert( pPointsSequence->end(), currentPath0.begin(), currentPath0.end() );
					currentPath1.erase( currentPath1.begin() );
					pPointsSequence->insert( pPointsSequence->end(), currentPath1.begin(), currentPath1.end() );
					return true;
				}
				else
				{
					return false;
				}
			}
		}
		++currentPathIterator0;
		++currentPathIterator1;
	}
	
	pPointsSequence->insert( pPointsSequence->end(), currentPath0.begin(), currentPath0.end() );
	return true;
	/**/
}

bool CVSOBuilder::MergeVSO( SVectorStripeObject *pVSO0, bool bVSO0Begin,
														SVectorStripeObject *pVSO1, bool bVSO1Begin )
{
	SBackupKeyPoints backupKeyPoints0;
	SBackupKeyPoints backupKeyPoints1;
	backupKeyPoints0.SaveKeyPoints( *pVSO0 );
	backupKeyPoints1.SaveKeyPoints( *pVSO1 );

	CVec3 vPointToAdd0 = VNULL3;
	CVec3 vPointToAdd1 = VNULL3;
	float fWidthToAdd0 = DEFAULT_WIDTH;
	float fWidthToAdd1 = DEFAULT_WIDTH;
	
	if ( bVSO1Begin )
	{
		backupKeyPoints1.SetBeginOpacity( 0.0f );

		if ( pVSO1->controlpoints.size() < 3 )
		{
			vPointToAdd0 = ( *( pVSO1->controlpoints.begin() ) + *( pVSO1->controlpoints.rbegin() ) ) / 2.0f;
			vPointToAdd1 = *( pVSO1->controlpoints.rbegin() );

			fWidthToAdd0= ( pVSO1->points.begin()->fWidth + pVSO1->points.rbegin()->fWidth ) / 2.0f;
			fWidthToAdd1= pVSO1->points.rbegin()->fWidth;
		}
		else
		{
			vPointToAdd0 = *( pVSO1->controlpoints.begin() + 1 );
			vPointToAdd1 = *( pVSO1->controlpoints.begin() + 2 );
			
			bool bFirst = true;
			bool bWidthToAdd0 = false;
			bool bWidthToAdd1 = false;
			for ( std::vector<SVectorStripeObjectPoint>::iterator pointIterator = pVSO1->points.begin(); pointIterator != pVSO1->points.end(); ++pointIterator )
			{
				if ( pointIterator->bKeyPoint )
				{
					if ( bFirst )
					{
						bFirst = false;
					}
					else if ( !bWidthToAdd0 )
					{
						fWidthToAdd0 = pointIterator->fWidth;
						bWidthToAdd0 = true;
					}
					else if ( !bWidthToAdd1 )
					{
						fWidthToAdd1= pointIterator->fWidth;
						bWidthToAdd1 = true;
					}
					if ( bWidthToAdd0 && bWidthToAdd1 )
					{
						break;
					}
				}
			}
		}
	}
	else
	{
		backupKeyPoints1.SetRBeginOpacity( 0.0f );
		
		if ( pVSO1->controlpoints.size() < 3 )
		{
			vPointToAdd0 = ( *( pVSO1->controlpoints.begin() ) + *( pVSO1->controlpoints.rbegin() ) ) / 2.0f;
			vPointToAdd1 = *( pVSO1->controlpoints.begin() );

			fWidthToAdd0= ( pVSO1->points.begin()->fWidth + pVSO1->points.rbegin()->fWidth ) / 2.0f;
			fWidthToAdd1= pVSO1->points.begin()->fWidth;
		}
		else
		{
			vPointToAdd0 = *( pVSO1->controlpoints.rbegin() + 1 );
			vPointToAdd1 = *( pVSO1->controlpoints.rbegin() + 2 );
			
			bool bFirst = true;
			bool bWidthToAdd0 = false;
			bool bWidthToAdd1 = false;
			for ( std::vector<SVectorStripeObjectPoint>::reverse_iterator pointIterator = pVSO1->points.rbegin(); pointIterator != pVSO1->points.rend(); ++pointIterator )
			{
				if ( pointIterator->bKeyPoint )
				{
					if ( bFirst )
					{
						bFirst = false;
					}
					else if ( !bWidthToAdd0 )
					{
						fWidthToAdd0 = pointIterator->fWidth;
						bWidthToAdd0 = true;
					}
					else if ( !bWidthToAdd1 )
					{
						fWidthToAdd1= pointIterator->fWidth;
						bWidthToAdd1 = true;
					}
					if ( bWidthToAdd0 && bWidthToAdd1 )
					{
						break;
					}
				}
			}
		}
	}

	if ( bVSO0Begin )
	{
		backupKeyPoints0.InsertToBegin( fWidthToAdd0, pVSO0->points.begin()->fOpacity );
		backupKeyPoints0.InsertToBegin( fWidthToAdd1, 0.0f );
		pVSO0->controlpoints.insert( pVSO0->controlpoints.begin(), vPointToAdd0 );
		pVSO0->controlpoints.insert( pVSO0->controlpoints.begin(), vPointToAdd1 );
	}
	else
	{
		backupKeyPoints0.InsertToRBegin( fWidthToAdd0, pVSO0->points.rbegin()->fOpacity );
		backupKeyPoints0.InsertToRBegin( fWidthToAdd1, 0.0f );
		pVSO0->controlpoints.push_back( vPointToAdd0 );
		pVSO0->controlpoints.push_back( vPointToAdd1 );
	}
	
	Update( pVSO0, true, DEFAULT_STEP, DEFAULT_WIDTH, DEFAULT_OPACITY );
	backupKeyPoints0.LoadKeyPoints( pVSO0 );
	Update( pVSO0, true, DEFAULT_STEP, DEFAULT_WIDTH, DEFAULT_OPACITY );

	Update( pVSO1, true, DEFAULT_STEP, DEFAULT_WIDTH, DEFAULT_OPACITY );
	backupKeyPoints1.LoadKeyPoints( pVSO1 );
	Update( pVSO1, true, DEFAULT_STEP, DEFAULT_WIDTH, DEFAULT_OPACITY );
	
	return true;
}

float CVSOBuilder::GetVSOEdgeHeght( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const SVectorStripeObject &rVectorStripeObject, bool bBegin, bool bFirst )
{
	float fHeight = 0.0f;
	bool bFirstFound = false;
	for ( int nPointIndex = 0; nPointIndex < rVectorStripeObject.points.size(); ++nPointIndex )
	{
		CVec3 vPos;
		if ( bBegin )
		{
			vPos = rVectorStripeObject.points[nPointIndex].vPos;
		}
		else
		{
			vPos = rVectorStripeObject.points[rVectorStripeObject.points.size() - nPointIndex - 1].vPos;
		}
		CVertexAltitudeInfo::GetHeight( rAltitude, vPos.x, vPos.y, &fHeight );
		if ( fHeight != 0.0f )
		{
			if ( bFirstFound || bFirst )
			{
				break;
			}
			bFirstFound = true;
		}
	}
	return fHeight;
}

