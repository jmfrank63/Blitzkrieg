#ifndef _Plane_Path_Math_
#define _Plane_Path_Math_

#include "..\..\Misc\Geometry.h"
#include "..\..\Misc\Spline.h"

class CAnalyticBSpline3
{
	static const int N_PARTS_FOR_CLOSEST_POINT_SEARCHING;
	
	CAnalyticBSpline x, y, z;
public:
	CAnalyticBSpline3() {  }
	CAnalyticBSpline3( const CVec3 &p0, const CVec3 &p1, const CVec3 &p2, const CVec3 &p3 ) { Setup(p0, p1, p2, p3); }
	CAnalyticBSpline3( const CAnalyticBSpline3 &bs ) : x( bs.x ), y( bs.y ), z( bs.z ) {  }
	void Setup( const CVec3 &p0, const CVec3 &p1, const CVec3 &p2, const CVec3 &p3 )
	{
		x.Setup( p0.x, p1.x, p2.x, p3.x );
		y.Setup( p0.y, p1.y, p2.y, p3.y );
		z.Setup( p0.z, p1.z, p2.z, p3.z );
	}
	const CVec3 Get( const float t ) const { return CVec3( x(t), y(t), z(t) ); }
	const CVec3 operator()( const float t ) const { return Get( t ); }
	const CVec3 GetDiff1( const float t ) const { return CVec3( x.GetDiff1(t), y.GetDiff1(t), z.GetDiff1(t) ); }

	const float GetLength( const int nPres = 100 ) const
	{
		float fLen = 0;
		const float fTick = 1.0f / nPres;
		for ( float f = fTick; f <= 1.0f; f += fTick )
			fLen += fabs( Get(f - fTick) - Get(f) );
		return fLen;
	}
};
template <class TVEC>
class CBezierCurve
{
	TVEC p0, p2, p1, p3;

public:
	void Setup( const TVEC &_p0, const TVEC &_p1, const TVEC &_p2, const TVEC &_p3 )
	{
		p0 = _p0;
		p1 = _p1;
		p2 = _p2;
		p3 = _p3;
	}
	
	TVEC Get( const float fT ) const
	{
		const float fTNeg = 1 - fT;
		return p0 * fTNeg * fTNeg * fTNeg + 
					 p1 * fT * fTNeg * fTNeg +
					 p2 * fT * fT * fTNeg +
					 p3 * fT * fT * fT;
	}
	TVEC GetDiff1( const float fT ) const
	{
		const float fTNeg = 1 - fT;
		return ( p1 - 3 * p0 ) * fTNeg * fTNeg + 2 * ( p2 - p1 ) * fT *fTNeg + ( 3 * p3 - p2 ) * fT*fT;
	}

};


struct CDirectedCircle : public CCircle
{
	int nDir;												// direction of circle rotation

	CDirectedCircle() : nDir( 0 ) {  }
	CDirectedCircle ( const CVec2 &_center, const float _r, const int _nDir )
	: CCircle( _center, _r ), nDir( _nDir )
	{
	}
};
void GetDirectedCirclesByTangent( const CVec2 &tang, const CVec2 &p, const float r, CDirectedCircle *c1, CDirectedCircle *c2 );
WORD DirectedDirsDifference( const CVec2 &vStart, const CVec2 &vFinish, const int nDir );
WORD DirectedDirsDifference( const WORD wStart, const WORD wFinish, const int nDir );

bool GetDirectedCirclesTangentPoints( const CDirectedCircle &from, const CDirectedCircle &to, CVec2 *v1, CVec2 *v2 );


template <class TVEC> 
class CHermitCurve
{
	TVEC a, b, c, d;
public:
	void Setup( const TVEC &x0, const TVEC &x1, const TVEC &v0, const TVEC &v1 )
	{
		a = -2.0f * x1 + v0 + v1 + 2.0f * x0;
		b = 3.0f * x1 - 2.0f * v0 - v1 - 3.0f * x0;
		c = v0;
		d = x0;
	}
	TVEC Get( const float fT ) const { return fT * fT * fT * a + fT * fT * b + fT * c + d; }
	TVEC GetDiff1( const float fT ) const { return 3 * fT * fT * a + 2 * fT * b + c; }
};


#endif //_Plane_Path_Math_