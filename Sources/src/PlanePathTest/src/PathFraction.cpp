#include "StdAfx.h"

#include "PathFraction.h"

const int CPathFractionBSPline3D::nPresizion = 30;
const CPathFractionBSPline3D::CSplineLength & CPathFractionBSPline3D::GetCur( const float fDist, float * const fNewDist ) const
{
	float fDistSoFar= 0;
	for ( CSplines::const_iterator i = splines.begin(); i != splines.end(); ++i )
	{
		fDistSoFar += i->second;
		if ( fDistSoFar > fDist )						// current spline suits
		{
			*fNewDist = (fDist - fDistSoFar - i->second ) / i->second; // 0..1
			return *i;
		}
	}
	
	*fNewDist = (fDist - fDistSoFar - splines.back().second) / splines.back().second; // 0..1
	return splines.back();
}
void CPathFractionBSPline3D::Init( const CVec3 &x0, const CVec3 &x1, const CVec3 &_v0, const CVec3 &_v1, const float _fLength )
{
	CVec3 v0 = _v0;
	Normalize( &v0 );
	CVec3 v1 = _v1;
	Normalize( &v1 );
	const float fR = fabs( x0 - x1 ) / 4.0f;
	const CVec3 p1( x0 + fR * v0 );
	const CVec3 p2( x1 - fR * v1 );

	splines.resize( 5 );
	splines[0].first.Setup( x0, x0, x0, p1 );
	splines[1].first.Setup( x0, x0, p1, p2 );
	splines[2].first.Setup( x0, p1, p2, x1 );
	splines[3].first.Setup( p1, p2, x1, x1 );
	splines[4].first.Setup( p2, x1, x1, x1 );
	
	fSumLengh = 0;
	for ( int i = 0; i < 5; ++i )
	{
		const float fLen = splines[i].first.GetLength( nPresizion );
		fSumLengh += fLen;
		splines[i].second = fLen;
	}

	fLength = _fLength;
	const float fCoeff = fLength / fSumLengh;
	for ( int i = 0; i < 5; ++i )
			splines[i].second *= fCoeff;
}
CVec3 CPathFractionBSPline3D::GetPoint( const float fDist ) const 
{
	float fNewDist;
	const CSplineLength &sp =  GetCur( fDist, &fNewDist );
	if ( fNewDist > 1.0f )

		int a = 0;
	return sp.first.Get( fNewDist ); 
}
CVec3 CPathFractionBSPline3D::GetTangent( const float fDist ) const 
{ 
	float fNewDist;
	const CSplineLength &sp =  GetCur( fDist, &fNewDist );
	const CVec3 vDiff( sp.first.GetDiff1( fNewDist ) ); 
	
	if ( fabs2(vDiff) < 0.000001 )
	{
		if ( fNewDist == 0 )
			return sp.first.GetDiff1( fNewDist + sp.second / 1000.f );
		else
			return sp.first.GetDiff1( fNewDist - sp.second / 1000.f );
	}
	return vDiff;
}