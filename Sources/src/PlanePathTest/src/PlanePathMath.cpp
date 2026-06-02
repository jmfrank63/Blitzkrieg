#include "stdafx.h"

#include "PlanePathMath.h"
#include "..\..\Misc\Geometry.h"

const int nOrientation = -1;

void GetDirectedCirclesByTangent( const CVec2 &tang, const CVec2 &p, const float r, CDirectedCircle *c1, CDirectedCircle *c2 )
{
	const CVec2 v( -tang.y, tang.x );

	c1->r = r;
	c1->center = p + v * r;
	c1->nDir = -1;

	c2->r = r;
	c2->center = p - v * r;
	c2->nDir = 1;
}
WORD DirectedDirsDifference( const CVec2 &vStart, const CVec2 &vFinish, const int nDir )
{
	return DirectedDirsDifference( GetDirectionByVector(vStart), GetDirectionByVector(vFinish), nDir );
}
WORD DirectedDirsDifference( const WORD wStart, const WORD wFinish, const int nDir )
{
	return ( nDir == -1 ?  /*positive direction*/wFinish - wStart : /*negative direction*/wStart - wFinish );
}


bool GetInternalTangentPoints( const CCircle &c1, const CCircle &c2, CVec2 *v11, CVec2 *v21, CVec2 *v12, CVec2 *v22 )
{
	if ( fabs( c1.center - c2.center ) <= c1.r + c2.r )
		return false;
	
	
	CCircle expanded = c1;
	expanded.r += c2.r;
	FindTangentPoints( c2.center, expanded, v11, v12 );
	CVec2 vOffset1( *v11 - c1.center ), vOffset2( *v12 - c1.center );
	Normalize( &vOffset1 );
	Normalize( &vOffset2 );
	vOffset1 *= c2.r;
	vOffset2 *= c2.r;

	*v11 -= vOffset1;
	*v21 = c2.center - vOffset1;

	*v12 -= vOffset2;
	*v22 = c2.center - vOffset2;
	return true;
}


static void GetExternalTangentPointsFirstGreater( const CCircle &c1, const CCircle &c2, CVec2 *v11, CVec2 *v21, CVec2 *v12, CVec2 *v22 )
{
	CCircle reduced = c1;
	reduced.r -= c2.r;
	FindTangentPoints( c2.center, reduced, v11, v12 );
	CVec2 vOffset1( *v11 - c1.center ), vOffset2( *v12 - c1.center );
	Normalize( &vOffset1 );
	Normalize( &vOffset2 );
	vOffset1 *= c2.r;
	vOffset2 *= c2.r;

	*v11 += vOffset1;
	*v21 = c2.center + vOffset1;

	*v12 += vOffset2;
	*v22 = c2.center + vOffset2;
}
static bool GetExternalTangentPoints( const CCircle &c1, const CCircle &c2, CVec2 *v11, CVec2 *v21, CVec2 *v12, CVec2 *v22 )
{
	if ( !c1.r || !c2.r || c1.center == c2.center ) 
		return false;

	if ( c1.r == c2.r )
	{
		CVec2 vC1toC2( c2.center - c1.center );
		CVec2 vPerp ( -vC1toC2.y, vC1toC2.x );
		Normalize( &vPerp );
		
		vPerp *= c1.r;
		*v11 = c1.center + vPerp;
		*v12 = c1.center - vPerp;

		*v21 = c2.center + vPerp;
		*v22 = c2.center - vPerp;
	}
	else if ( c1.r > c2.r )
		GetExternalTangentPointsFirstGreater( c1, c2, v11, v21, v12, v22 );
	else //if ( c2.r > c1.r )
		GetExternalTangentPointsFirstGreater( c2, c1, v21, v11, v22, v12 );

	return true;
}
bool GetDirectedCirclesTangentPoints( const CDirectedCircle &from, const CDirectedCircle &to, CVec2 *v1, CVec2 *v2 )
{
	if ( from.center == to.center ) 
		return false;

	CVec2 v11, v12, v21, v22;

	if ( from.nDir == to.nDir )
	{
		if ( !GetExternalTangentPoints( from, to, &v11, &v21, &v12, &v22 ) ) 
			return false;
	}
	else if ( !GetInternalTangentPoints( from, to, &v11, &v21, &v12, &v22 ) )
		return false;

	const CVec2 vRadius( v11 - from.center );
	const CVec2 vTangent( v21 - v11 );
	if (  Sign( vRadius.y * vTangent.x - vRadius.x * vTangent.y ) == from.nDir )
	{
		*v1 = v11;
		*v2 = v21;
	}
	else
	{
		*v1 = v12;
		*v2 = v22;
	}
	return true;
}
