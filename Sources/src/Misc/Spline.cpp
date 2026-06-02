#include "stdafx.h"

#include "Spline.h"
const int CAnalyticBSpline2::N_PARTS_FOR_CLOSEST_POINT_SEARCHING = 10;
void CAnalyticBSpline2::GetClosestPoint( const CVec2 &vPoint, CVec2 *pvClosestPoint, float *pfT, const float fT0, const float fT1 )
{
	*pfT = -1.0f;
	*pvClosestPoint = VNULL2;
	float fBestDist2 = 0.0f;

	const float fAdd = ( fT1 - fT0 ) / N_PARTS_FOR_CLOSEST_POINT_SEARCHING;
	if ( fabs(fAdd) >= 1.0f / N_PARTS_FOR_CLOSEST_POINT_SEARCHING )
	{
		float fCurT = fT0;
		for ( int i = 0; i <= N_PARTS_FOR_CLOSEST_POINT_SEARCHING; ++i )
		{
			CVec2 vCandidate = Get( fCurT );
			const float fCandidateDist2 = fabs2( vCandidate - vPoint );

			if ( *pfT == -1.0f || fCandidateDist2 < fBestDist2 )
			{
				*pfT = fCurT;
				fBestDist2 = fCandidateDist2;
				*pvClosestPoint = vCandidate;
			}

			fCurT += fAdd;
		}
	}
	else
	{
		*pfT = fT0;
		*pvClosestPoint = Get( fT0 );
	}
}
