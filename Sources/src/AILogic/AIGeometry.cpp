#include "stdafx.h"

#include "..\Main\RPGStats.h"
const WORD GetDirectionByVector( float x, float y )
{
	if ( IsAlmostZero( x, y ) )
		return 0;
	
	float add = 49152.0f;

	if ( x <= 0 && y > 0 )
	{
		add = 0.0f;

		std::swap( x, y );
		y = -y;
	}
	else if ( y <= 0 && x < 0 )
	{
		add = 16384.0f;
		
		x = -x;
		y = -y;
	}
	else if ( x >= 0 && y < 0 )
	{
		add = 32768.0f;
		
		std::swap( x, y );
		x = -x;
	}
		
	NI_ASSERT_SLOW_TF( x >= 0 && y >= 0, NStr::Format("Wrong vector {%g, %g}", x, y), return 0 );

	if ( x + y != 0 )
		return 16384.0f * y / ( x + y ) + add;
	else
		return 0;
}
const WORD GetDirectionByVector( CVec2 vec )
{
	return GetDirectionByVector( vec.x, vec.y );
}
const CVec2 GetVectorByDirection( const WORD dir )
{
	const float fDir = float(dir % 16384) / 16384.0f;
	CVec2 result( 1-fDir, fDir );

	if ( dir < 16384 )
	{
		result.y = -result.y;
		std::swap( result.x, result.y );
	}
	else if ( dir < 32768 )
	{
		result.x = -result.x;
		result.y = -result.y;
	}
	else if ( dir < 49152 )
	{
		result.x = -result.x;
		std::swap( result.x, result.y );
	}

	Normalize( &result );
	return result;
}
const WORD DirsDifference( const WORD dir1, const WORD dir2 )
{
	const	WORD clockWise = dir1-dir2;
	const	WORD antiClockWise = dir2-dir1;

	return Min( clockWise, antiClockWise );
}
const int DifferenceSign( const WORD dir1, const WORD dir2 )
{
	const	WORD clockWise = dir1-dir2;
	const	WORD antiClockWise = dir2-dir1;

	return Sign(int(antiClockWise) - int(clockWise));
}
bool IsInTheAngle( const WORD dir, const WORD startAngleDir, const WORD finishAngleDir )
{
	return 
		WORD( dir - startAngleDir ) + WORD( finishAngleDir - dir ) == WORD( finishAngleDir - startAngleDir );
}
bool IsInTheMinAngle( const WORD dir, const WORD dir1, const WORD dir2 )
{
	return
		(int)DirsDifference( dir, dir1 ) + (int)DirsDifference( dir, dir2 ) == DirsDifference( dir1, dir2 );
}
const WORD GetZDirectionBy3DVector( const float x, const float y, const float z )
{
	return GetDirectionByVector( fabs( x, y ), z );
}
const WORD GetZDirectionBy3DVector( const CVec2 &vec, const float z )
{
	return GetZDirectionBy3DVector( vec.x, vec.y, z );
}
const WORD GetZAngle( const float x, const float y, float z )
{
	if ( z < 0 )
		z = 0;

	const WORD wZDir = GetZDirectionBy3DVector( x, y, z );
	return Min( DirsDifference( wZDir, 16384 * 3 ), DirsDifference( wZDir, 16384 ) );
}
const WORD GetZAngle( const CVec2 &vec, const float z )
{
	return GetZAngle( vec.x, vec.y, z );
}
const float GetDistanceToSegment( const CVec2 &vSegmentStart, const CVec2 &vSegmentEnd, const CVec2 &vPoint )
{
	CLine2 line( vSegmentStart, vSegmentEnd );
	
	CVec2 vNormal;
	line.ProjectPoint( vPoint, &vNormal );

	const float fDiff1 = fabs( vSegmentStart - vNormal );
	const float fDiff2 = fabs( vSegmentEnd - vNormal );
	const float fDiff3 = fabs( vSegmentEnd - vSegmentStart );
	
	if ( fDiff3 < fDiff2 + fDiff1 ) // нормаль от точки не падает на отрезок
	{
		const float fDist1 = fabs( vSegmentStart - vPoint );
		const float fDist2 = fabs( vSegmentEnd - vPoint );
		return Min( fDist1, fDist2 );
	}
	else 
	{
		return fabs( vNormal - vPoint );
	}
}
void SRect::InitRect( const CVec2 &_v1, const CVec2 &_v2, const CVec2 &_v3, const CVec2 &_v4 )
{
	v1 = _v1;
	v2 = _v2;
	v3 = _v3;
	v4 = _v4;
	center = ( v1 + v3 ) * 0.5;

	dir = v4 - v1;

	lengthBack = lengthAhead = fabs( dir );
	Normalize( &dir );

	dirPerp.x = -dir.y;
	dirPerp.y = dir.x;

	width = fabs( v2 - v1 );
}
void SRect::InitRect( const CVec2 &_center, const CVec2 &_dir, const float length, const float _width )
{
	center = _center;
	dir = _dir;
	Normalize( &dir );

	dirPerp.x = -dir.y;
	dirPerp.y = dir.x;

	lengthBack = lengthAhead = length;
	width = _width;

	const CVec2 pointBack = center - dir * length;
	const CVec2 pointForward = center + dir * length;

	v1 = pointBack - dirPerp * width;
	v2 = pointBack + dirPerp * width;
	v3 = pointForward + dirPerp * width;
	v4 = pointForward - dirPerp * width;
}
void SRect::InitRect( const CVec2 &_center, const CVec2 &_dir, const float _lengthAhead, const float _lengthBack, const float _width )
{
	center = _center;
	dir = _dir;
	Normalize( &dir );
	dirPerp.x = -dir.y;
	dirPerp.y = dir.x;

	lengthBack = _lengthBack;
	lengthAhead = _lengthAhead;
	width = _width;

	const CVec2 pointBack = center - dir * lengthBack;
	const CVec2 pointForward = center + dir * lengthAhead;

	v1 = pointBack - dirPerp * width;
	v2 = pointBack + dirPerp * width;
	v3 = pointForward + dirPerp * width;
	v4 = pointForward - dirPerp * width;
}
bool SRect::IsIntersectProject( const CVec2 &v1, const CVec2 &v2, const CVec2 &v3, const CVec2 &v4, const CVec2 &dir, const float min, const float max ) const
{
	const float proj1 = v1 * dir;
	const float proj2 = v2 * dir;
	const float proj3 = v3 * dir;
	const float proj4 = v4 * dir;

	const float min12 = Min( proj1, proj2 );
	const float min34 = Min( proj3, proj4 );
	const float max12 = Max( proj1, proj2 );
	const float max34 = Max( proj3, proj4 );

	return 
		!( Min( min12, min34 ) >= max || Max( max12, max34 ) <= min );
}
bool SRect::IsIntersected( const SRect &rect ) const
{
	return 
		IsIntersectProject( v1 - rect.center, v2 - rect.center, v3 - rect.center, v4 - rect.center, rect.dir, -rect.lengthBack, rect.lengthAhead ) &&
		IsIntersectProject( v1 - rect.center, v2 - rect.center, v3 - rect.center, v4 - rect.center, rect.dirPerp, -rect.width, rect.width ) &&
		IsIntersectProject( rect.v1 - center, rect.v2 - center, rect.v3 - center, rect.v4 - center, dir, -lengthBack, lengthAhead ) &&
		IsIntersectProject( rect.v1 - center, rect.v2 - center, rect.v3 - center, rect.v4 - center, dirPerp, -width, width );
}
bool SRect::IsPointInside( const CVec2 &point ) const
{
	const CVec2 center( ( v1.x + v2.x + v3.x + v4.x ) / 4, ( v1.y + v2.y + v3.y + v4.y ) / 4 );
	const short int rightSign = Sign( STriangle( v1, v2, center ) );
	
	if ( rightSign == 0 )
		return fabs2( point - center ) < 0.001f;

	return 
		Sign( STriangle ( v1, v2, point ) ) == rightSign && Sign( STriangle ( v2, v3, point ) ) == rightSign &&
		Sign( STriangle ( v3, v4, point ) ) == rightSign && Sign( STriangle ( v4, v1, point ) ) == rightSign;
}
bool SRect::IsIntersectCircle( const CVec2 &circleCenter, const float r ) const
{
	if ( IsPointInside( circleCenter ) )
		return true;
	
	const CVec2 vNewCenter = circleCenter - center;
	const CVec2 vLocalCoordCenter( vNewCenter.x * dir.x + vNewCenter.y * dir.y, -vNewCenter.x * dir.y + vNewCenter.y * dir.x );

	float fDist = 0;

	if ( vLocalCoordCenter.x < -lengthBack )
		fDist += sqr( vLocalCoordCenter.x - (-lengthBack) );
	else if ( vLocalCoordCenter.x > lengthAhead )
		fDist += sqr( vLocalCoordCenter.x - lengthAhead );

	if ( vLocalCoordCenter.y < -width )
		fDist += sqr( vLocalCoordCenter.y - (-width) );
	else if ( vLocalCoordCenter.y > width )
		fDist += sqr( vLocalCoordCenter.y - width );

	return fDist <= sqr( r );
}
const int SRect::GetSide( const WORD dirFromRectCenter ) const
{
	const WORD diff = dirFromRectCenter - GetDirectionByVector( dir );

	if ( diff <= 8192 )
		return RPG_FRONT;
	else if ( diff > 8192 && diff <= 24576 )
		return RPG_LEFT;
	else if ( diff > 24576 && diff <= 40960 )
		return RPG_BACK;
	else if ( diff > 40960 && diff <= 57344 )
		return RPG_RIGHT;
	else
		return RPG_FRONT;
}
const int SRect::GetSide( const CVec2 &point ) const
{
	return GetSide( GetDirectionByVector( point - center ) );
}
void SRect::Compress( const float fFactor )
{
	lengthAhead *= fFactor;
	lengthBack *= fFactor;
	width *= fFactor;
	
	InitRect( center, dir, lengthAhead, lengthBack, width );
}
const float fabs( const SRect rect1, const SRect rect2 )
{
	CVec2 dir( rect1.center - rect2.center );
	const float dist = fabs( dir );
	Normalize( &dir );
	
	const float f1_1 = ( rect1.v1 - rect1.center ) * dir + dist;
	const float f1_2 = ( rect1.v2 - rect1.center ) * dir + dist;
	const float f1_3 = ( rect1.v3 - rect1.center ) * dir + dist;
	const float f1_4 = ( rect1.v4 - rect1.center ) * dir + dist;

	const float f2_1 = ( rect2.v1 - rect2.center ) * dir;
	const float f2_2 = ( rect2.v2 - rect2.center ) * dir;
	const float f2_3 = ( rect2.v3 - rect2.center ) * dir;
	const float f2_4 = ( rect2.v4 - rect2.center ) * dir;

	const float segm1Min = Min( Min( f1_1, f1_2 ), Min( f1_3, f1_4 ) );
	const float segm1Max = Max( Max( f1_1, f1_2 ), Max( f1_3, f1_4 ) );

	const float segm2Min = Min( Min( f2_1, f2_2 ), Min( f2_3, f2_4 ) );
	const float segm2Max = Max( Max( f2_1, f2_2 ), Max( f2_3, f2_4 ) );

	if ( segm1Max < segm2Min || segm2Max < segm1Min )
		return Min( fabs( segm1Max - segm2Min ), fabs( segm2Max - segm1Min ) );
	else
		return 0;
}
const WORD GetVisibleAngle( const CVec2 point, const SRect rect )
{
	const WORD wAngle1 = GetDirectionByVector( rect.v1 - point );
	const WORD wAngle2 = GetDirectionByVector( rect.v2 - point );
	const WORD wAngle3 = GetDirectionByVector( rect.v3 - point );
	const WORD wAngle4 = GetDirectionByVector( rect.v4 - point );

	const WORD diff1 = DirsDifference( wAngle2, wAngle1 );
	const WORD diff2 = DirsDifference( wAngle3, wAngle1 );
	const WORD diff3 = DirsDifference( wAngle4, wAngle1 );
	const WORD diff4 = DirsDifference( wAngle3, wAngle2 );
	const WORD diff5 = DirsDifference( wAngle4, wAngle2 );
	const WORD diff6 = DirsDifference( wAngle4, wAngle3 );

	return Max( Max ( Max( diff1, diff2 ), Max( diff3, diff4 ) ), Max( diff5, diff6 ) );
}
const float CBSpline::DELTA = 0.02f;
const float CBSpline::DELTA_FORWARD = 0.08f;
const int CBSpline::N_OF_ITERATONS = 1 / CBSpline::DELTA;
const int CBSpline::N_ITERS_TO_FORWARD = CBSpline::DELTA_FORWARD / CBSpline::DELTA;
void CBSpline::Init( const CVec2 &p3, const CVec2 &p2, const CVec2 &p1, const CVec2 &p0 )
{
	a = 1.0f/6.0f * ( -p3 + 3 * p2 - 3 * p1 + p0 );
	b = 1.0f/6.0f * ( 3 * p3 - 6 * p2 + 3 * p1 );
	c = 1.0f/6.0f * ( -3 * p3 + 3 * p1 );
	d = 1.0f/6.0f * ( p3 + 4 * p2 + p1 );

	d3x = a * sqr( DELTA ) * DELTA;
	dx = d3x + b * sqr( DELTA );
	d2x = 2 * ( 2 * d3x + dx );
	dx += c * DELTA;
	d3x *= 6;
	x = d;

	fw_d3x = a * sqr( DELTA_FORWARD ) * DELTA_FORWARD;
	fw_dx = fw_d3x + b * sqr( DELTA_FORWARD );
	fw_d2x = 2 * ( 2 * fw_d3x + fw_dx );
	fw_dx += c * DELTA_FORWARD;
	fw_d3x *= 6;

	t = tForward = 0;
	cntToForward = 0;
/*
	dx = a * sqr( del ) * del + b * sqr( del ) + c * del;
	d2x = 6 * a * sqr( del ) * del + 2 * b * sqr( del );
	d3x = 6 * a * sqr( del ) * del;
*/
}
void CBSpline::Iterate()
{
	x += dx;
	dx += d2x;
	d2x += d3x;
	t += DELTA;

	++cntToForward;
	if ( cntToForward == N_ITERS_TO_FORWARD )
	{
		fw_dx += fw_d2x;
		fw_d2x += fw_d3x;
		tForward += DELTA_FORWARD;
		cntToForward = 0;
	}
}
const float CBSpline::GetReverseR() const 
{ 
	const CVec2 first = 3 * a * sqr( t ) + 2 * b * t + c;
	const CVec2 second = 6 * a * t + 2 * b;
	const float tanLen = fabs( first );

	return fabs( first.x * second.y - first.y * second.x ) / ( sqr( tanLen ) * tanLen );
}
const void CBSpline::StartForwardIterating( SForwardIter *pIter )
{
	pIter->t = tForward;
	pIter->x = x;
	pIter->fw_dx = fw_dx;
	pIter->fw_d2x = fw_d2x;
	pIter->fw_d3x = fw_d3x;
}
const void CBSpline::IterateForward( SForwardIter *pIter )
{
	if ( pIter->t != -1 && pIter->t + DELTA_FORWARD <= 1 )
	{
		pIter->t += DELTA_FORWARD;
		pIter->x += pIter->fw_dx;
		pIter->fw_dx += pIter->fw_d2x;
		pIter->fw_d2x += pIter->fw_d3x;
	}
	else
		pIter->t = -1;
}
int CBSpline ::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &a );
	saver.Add( 2, &b );
	saver.Add( 3, &c );
	saver.Add( 4, &d );
	saver.Add( 5, &x );
	saver.Add( 6, &dx );
	saver.Add( 7, &d2x );
	saver.Add( 8, &d3x );
	saver.Add( 9, &fw_dx );
	saver.Add( 10, &fw_d2x );
	saver.Add( 11, &fw_d3x );
	saver.Add( 12, &t );
	saver.Add( 13, &tForward );
	saver.Add( 14, &cntToForward );
	return 0;
}
