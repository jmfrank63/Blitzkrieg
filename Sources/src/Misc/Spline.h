#ifndef __SPLINE_H__
#define __SPLINE_H__
#pragma ONCE
class CAnalyticBSpline
{
	float a3, a2, a1, a0;
public:
	CAnalyticBSpline() {  }
	CAnalyticBSpline( const float p0, const float p1, const float p2, const float p3 ) { Setup(p0, p1, p2, p3); }
	CAnalyticBSpline( const CAnalyticBSpline &bs ) { a3 = bs.a3; a2 = bs.a2; a1 = bs.a1; a0 = bs.a0; }
	void Setup( const float p0, const float p1, const float p2, const float p3 )
	{
		a3 = ( -p0 + 3.0f*(p1 - p2) + p3 ) * (1.0f/6.0f);
		a2 = ( p0 - 2.0f*p1 + p2 ) * (1.0f/2.0f);
		a1 = ( -p0 + p2 ) * (1.0f/2.0f);
		a0 = ( p0 + 4.0f*p1 + p2 ) * (1.0f/6.0f);
	}
	float Get( const float t ) const { return ((a3*t + a2)*t + a1)*t + a0; }
	float operator()( const float t ) const { return Get( t ); }

	float GetDiff1( const float t ) const { return (a3*3.0f*t + a2*2.0f)*t + a1; }
	float GetDiff2( const float t ) const { return a3*6.0f*t + a2*2.0f; }
	float GetDiff3( const float t ) const { return a3*6.0f; }
};
class CAnalyticBSpline2
{
	static const int N_PARTS_FOR_CLOSEST_POINT_SEARCHING;
	
	CAnalyticBSpline x, y;
public:
	CAnalyticBSpline2() {  }
	CAnalyticBSpline2( const CVec3 &p0, const CVec3 &p1, const CVec3 &p2, const CVec3 &p3 ) { Setup(p0, p1, p2, p3); }
	CAnalyticBSpline2( const CVec2 &p0, const CVec2 &p1, const CVec2 &p2, const CVec2 &p3 ) { Setup(p0, p1, p2, p3); }
	CAnalyticBSpline2( const CAnalyticBSpline2 &bs ) : x( bs.x ), y( bs.y ) {  }
	void Setup( const CVec3 &p0, const CVec3 &p1, const CVec3 &p2, const CVec3 &p3 )
	{
		x.Setup( p0.x, p1.x, p2.x, p3.x );
		y.Setup( p0.y, p1.y, p2.y, p3.y );
	}
	void Setup( const CVec2 &p0, const CVec2 &p1, const CVec2 &p2, const CVec2 &p3 )
	{
		x.Setup( p0.x, p1.x, p2.x, p3.x );
		y.Setup( p0.y, p1.y, p2.y, p3.y );
	}
	const CVec2 Get( const float t ) const { return CVec2( x(t), y(t) ); }
	const CVec2 operator()( const float t ) const { return Get( t ); }

	const CVec2 GetDiff1( const float t ) const { return CVec2( x.GetDiff1(t), y.GetDiff1(t) ); }
	const CVec2 GetDiff2( const float t ) const { return CVec2( x.GetDiff2(t), y.GetDiff2(t) ); }
	const CVec2 GetDiff3( const float t ) const { return CVec2( x.GetDiff3(t), y.GetDiff3(t) ); }
	float GetLength( const int nNumSteps = 100 ) const
	{
		const float fStep = 1.0f / float( nNumSteps );
		CVec2 vLastPos, vPos = Get( 0 );
		float fLen = 0;
		for ( float fT = 0; fT <= 1; fT += fStep )
		{
			vLastPos = vPos;
			vPos = Get( fT );
			fLen += fabs( vLastPos - vPos );
		}
		return fLen;
	}
	float GetLength( const float t, const int nNumSteps = 100 ) const
	{
		if ( t < 1e-4 ) 
			return fabs( Get(t) - Get(0) );
		const float fStep = t / float( nNumSteps );
		CVec2 vLastPos, vPos = Get( 0 );
		float fLen = 0;
		for ( float fT = 0; fT <= t; fT += fStep )
		{
			vLastPos = vPos;
			vPos = Get( fT );
			fLen += fabs( vLastPos - vPos );
		}
		return fLen;
	}
	const float GetLength( const float fT1, const float fT2, const int nNumSteps = 100 ) const
	{
		if ( fT2 - fT1 < 1e-4 ) 
			return fabs( Get(fT2) - Get(fT1) );
		const float fStep = (fT2 - fT1) / float( nNumSteps );
		CVec2 vLastPos, vPos = Get( fT1 );
		float fLen = 0;
		for ( float fT = fT1; fT <= fT2; fT += fStep )
		{
			vLastPos = vPos;
			vPos = Get( fT );
			fLen += fabs( vLastPos - vPos );
		}
		return fLen;
	}
	float GetLengthAdaptive( const float fTolerance, const int nStepLimit = 100 ) const
	{
		float fLen1 = GetLength( 100 );
		float fLen2 = GetLength( 200 );
		int nStepCounter = 2;
		while ( (fabs(fLen1 - fLen2) > fTolerance) && (nStepCounter < nStepLimit) ) 
		{
			++nStepCounter;
			fLen1 = fLen2;
			fLen2 = GetLength( 100 * nStepCounter );
		}
		return fLen2;
	}
	float GetStep( const float fStep ) const { return fStep / GetLength(); }
	float GetCurvatureRadius( const float t ) const
	{
		const float dx = x.GetDiff1( t );
		const float dy = y.GetDiff1( t );
		const float denominator = fabs( dx*y.GetDiff2(t) - dy*x.GetDiff2(t) );
		return denominator < 1e-8f ? 1e37f : pow( fabs2( dx ) + fabs2( dy ), 3.0/2.0 ) / denominator;
	}
	const CVec2 GetCurvatureCenter( const float t ) const
	{
		const float dx = x.GetDiff1( t );
		const float dy = y.GetDiff1( t );
		const float denominator = dx*y.GetDiff2(t) - dy*x.GetDiff2(t);
		if ( fabs(denominator) < 1e-8f )
			return VNULL2;
		const float fCoeff = ( fabs2( dx ) + fabs2( dy ) ) / denominator;
		return CVec2( x(t) - fCoeff*dy, y(t) + fCoeff*dx );
	}
	
	void GetClosestPoint( const CVec2 &vPoint, CVec2 *pvClosestPoint, float *pfT, const float fT0 = 0.0f, const float fT1 = 1.0f );
};
#endif // __SPLINE_H__
