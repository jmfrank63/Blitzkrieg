#ifndef __WAR_FOG_TRACER_H__
#define __WAR_FOG_TRACER_H__

#pragma ONCE
#include "AIStaticMap.h"
extern CStaticMap theStaticMap;
template<class T>
class CWarFogTracer
{
	T &warFog;
	
	SVector center;
	int r;
	float fR2;
	float fSightPower2;

	WORD wUnitDir;
	WORD wVisionAngle;
	WORD bAngleLimited;
	WORD wMinAngle;
	WORD wMaxAngle;

	bool bPlane;
	
	SVector dirToPoint;

	void TraceRay( const int x, const int y );
	void OctupleTrace( const int x, const int y );
	int TraceToPoint( const SVector &center, const SVector &finishPoint );

	void Init( const SVector &center, const int r, const WORD wUnitDir, const WORD wVisionAngle, bool bAngleLimited, const WORD wMinAngle, const WORD wMaxAngle, bool bPlane, const float fSightPower );
public:
	CWarFogTracer( T &warFog, const SVector &center, const int r, const WORD wUnitDir, const WORD wVisionAngle, bool bAngleLimited, const WORD wMinAngle, const WORD wMaxAngle, bool bPlane, const float fSightPower );
	CWarFogTracer( T &warFog, const SFogInfo &fogInfo );

	CWarFogTracer( const SVector &tileToTrace, T &warFog, const SFogInfo &fogInfo );
};
template<class T>
CWarFogTracer<T>::CWarFogTracer( const SVector &tileToTrace, T &_warFog, const SFogInfo &fogInfo )
:	warFog( _warFog )
{
	if ( fogInfo.r <= 0 )
		return;

	center = fogInfo.center;
	if ( !theStaticMap.IsTileInside( center ) || !theStaticMap.IsTileInside( tileToTrace ) )
		return;
	
	r = fogInfo.r;
	fR2 = sqr( r );

	if ( fR2 < sqr( tileToTrace.x - center.x ) + sqr( tileToTrace.y - center.y ) )
		return;

	fSightPower2 = sqr( fogInfo.fSightPower );
	wUnitDir = fogInfo.wUnitDir;
	wVisionAngle = fogInfo.wVisionAngle;
	bAngleLimited = fogInfo.bAngleLimited;
	wMinAngle = fogInfo.wMinAngle;
	wMaxAngle = fogInfo.wMaxAngle;
	bPlane = fogInfo.bPlane;

	TraceRay( tileToTrace.x - center.x, tileToTrace.y - center.y );
}
template<class T>
void CWarFogTracer<T>::Init( const SVector &_center, const int _r, const WORD _wUnitDir, const WORD _wVisionAngle, bool _bAngleLimited, const WORD _wMinAngle, const WORD _wMaxAngle, bool _bPlane, const float _fSightPower )
{
	if ( _r <= 0 )
		return;
	
	center = _center;
	r = _r;
	fR2 = sqr( r );
	fSightPower2 = sqr( _fSightPower );
	wUnitDir = _wUnitDir;
	wVisionAngle = _wVisionAngle;
	bAngleLimited = _bAngleLimited;
	wMinAngle = _wMinAngle;
	wMaxAngle = _wMaxAngle;
	bPlane = _bPlane;

	if ( theStaticMap.IsTileInside( center ) )
		warFog.VisitPoint( center, SAIConsts::VIS_POWER, 0, fR2, fSightPower2 );

	int x = 0, y = r;
	int d = 3 - 2*y;

	TraceRay(  0,  r );
	TraceRay(  0, -r );
	TraceRay(  r,  0 );
	TraceRay( -r,  0 );

	do 
	{
		if ( d < 0 )
			d += 4*x + 6;
		else
		{
			d += 4*(x - y) + 10;
			--y;
			OctupleTrace( x, y );
		}
		++x;
		
		OctupleTrace( x, y );
	}	while ( x <= y );
}
template<class T>
CWarFogTracer<T>::CWarFogTracer( T &_warFog, const SVector &center, const int r, const WORD wUnitDir, const WORD wVisionAngle, bool bAngleLimited, const WORD wMinAngle, const WORD wMaxAngle, bool bPlane, const float fSightPower )
:	warFog( _warFog )
{
	Init( center, r, wUnitDir, wVisionAngle, bAngleLimited, wMinAngle, wMaxAngle, bPlane, fSightPower );
}
template<class T>
CWarFogTracer<T>::CWarFogTracer( T &_warFog, const SFogInfo &fogInfo )
: warFog( _warFog )
{
	Init( fogInfo.center, fogInfo.r, fogInfo.wUnitDir, fogInfo.wVisionAngle, fogInfo.bAngleLimited, fogInfo.wMinAngle, fogInfo.wMaxAngle, fogInfo.bPlane, fogInfo.fSightPower );
}
template<class T>
void CWarFogTracer<T>::TraceRay( const int x, const int y )
{
	if ( wVisionAngle < 32768 || bAngleLimited )
	{
		const WORD dir( GetDirectionByVector( CVec2( x, y ) ) );
		if ( DirsDifference( dir, wUnitDir ) > wVisionAngle || !IsInTheAngle( dir, wMinAngle, wMaxAngle ) )
			return;
	}

	const SVector finishPoint( center.x + x, center.y + y );

	if ( warFog.CanTraceRay( finishPoint ) )
		TraceToPoint( center, finishPoint );
}
template<class T>
void CWarFogTracer<T>::OctupleTrace( const int x, const int y )
{
	TraceRay(  x,  y );
	TraceRay( -x,  y );
	TraceRay(  x, -y );
	TraceRay( -x, -y );
	
	TraceRay(  y,  x );
	TraceRay(  y, -x );
	TraceRay( -y,  x );
	TraceRay( -y, -x ); 
}
extern CStaticMap theStaticMap;
template<class T>
int CWarFogTracer<T>::TraceToPoint( const SVector &center, const SVector &finishPoint )
{
	CBres bres;
	bres.InitPoint( center, finishPoint );
	int vis = SAIConsts::VIS_POWER;
	dirToPoint = finishPoint - center;

	do
	{
		bres.MakePointStep();
		if ( !theStaticMap.IsTileInside( bres.GetDirection() ) )
			break;

		if ( !warFog.VisitPoint( bres.GetDirection(), vis, sqr( bres.GetDirection().x - center.x ) + sqr( bres.GetDirection().y - center.y ), fR2, fSightPower2 ) )
			break;

		if ( !bPlane )
			vis -= theStaticMap.GetDissipation( bres.GetDirection() );

		if ( !bPlane && theStaticMap.IsOneWayTransp( bres.GetDirection() ) && dirToPoint * theStaticMap.GetOneWayTransp( bres.GetDirection() ) < 0 )
			return vis;
	} while ( vis > 0  &&  bres.GetDirection() != finishPoint );

	return vis;
}
#endif // __WAR_FOG_TRACER_H__
