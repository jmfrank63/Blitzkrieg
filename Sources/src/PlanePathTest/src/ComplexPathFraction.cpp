#include "StdAfx.h"
#include "ComplexPathFraction.h"

extern const int nOrientation = -1;


BASIC_REGISTER_CLASS( CPathFractionArcLine3D );
BASIC_REGISTER_CLASS( CPathFractionArc3D  );
BASIC_REGISTER_CLASS( CPathFractionArc  );
BASIC_REGISTER_CLASS( CPathFractionCircleLineCircle  );
BASIC_REGISTER_CLASS( CPathFractionCircleLineCircle3D  );
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	CPathFractionArcLine3D
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void CPathFractionArcLine3D::DoSubstitute( IPathFraction *pNext )
{
	substitute.resize( (pArc != 0) + (pLine != 0) );
	int nIndex = 0;
	if ( pArc )
	{
		pArc->DoSubstitute( 0 );
		substitute[nIndex++] = pArc;
	}

	if ( pLine )
		substitute[nIndex] = pLine;
	AfterSubstitute();
}
float CPathFractionArcLine3D::GetLength() const 
{ 
	return (pArc ? pArc->GetLength(): 0.0f) + (pLine? pLine->GetLength(): 0.0f); 
}
/////////////////////////////////////////////////////////////////////////////
void CPathFractionArcLine3D::Init( const CVec3 &x0, const CVec3 &_v0, const CVec3 &x1, const float fR/*circle radius*/ )
{
	CVec3 v0 = _v0;
	Normalize( &v0 );

	/*
	// check if path direction is almost match the initial speed;
	// in that case we don't need go circle, we only go with BSpline instead of line
	CVec3 x1x0 = x1-x0;
	const float fLength = fabs( x1x0 );
	Normalize( &x1x0 );
	
	//const CVec3 vProd = x1x0^v0;
	const float fProd = x1x0*v0;
	// if desired point is in front of and direction is almost the same as current
	if ( fProd > 0.0f && fabs(fProd - 1.0f) < 0.00001f ) 
	{
		CPathFractionBSPline3D *pLineTmp = new CPathFractionBSPline3D;
		pLine = pLineTmp;
		pLineTmp->Init( x0, x1, v0, x1x0, fLength );
		return;
	}
	*/

	CPtr<CPathFractionLine> pLineTmp = new CPathFractionLine;
	pLine = pLineTmp;

	CPtr<CPathFractionArc3D> pArc1, pArc2;

	CVec3 vT1, vT2;
	
	if ( TryCircle( x0, v0, x1, fR, 1, &vT1 ) )
		pArc1 = pArc;
	if ( TryCircle( x0, v0, x1, fR, -1, &vT2 ) )
		pArc2 = pArc;

	if ( !pArc1 && !pArc2 )
	{
		pLineTmp->Init( x0, x1 );
	}
	else if ( !pArc1 )
	{
		pArc = pArc2;
		pLineTmp->Init( vT2, x1 );
	}
	else if ( !pArc2 )
	{
		pArc = pArc1;
		pLineTmp->Init( vT1, x1 );
	}
	else if ( pArc1->GetLength() + fabs( vT1 - x1 ) > pArc2->GetLength() + fabs( vT2 -x1 ) )
	{
		pArc = pArc2;
		pLineTmp->Init( vT2, x1 );
	}
	else
	{
		pArc = pArc1;
		pLineTmp->Init( vT1, x1 );
	}

	if ( pArc && pLine->GetLength() < 10 )
		pLine = 0;
}
/////////////////////////////////////////////////////////////////////////////
bool CPathFractionArcLine3D::TryCircle( const CVec3 &x0, const CVec3 &v0, const CVec3 &x1, const float fR, const int nDir, CVec3 *vT )
{
	CVec3 x0x1 = x1 - x0;									
	// introduce new (_right_ if nDir == 1 or _left_ if nDir == -1 ) coordinate system ( i, j, k ), coordinate center (0,0) = x0.
	// i,y - manuver plane, k - normal to manuver plane

	// coordinate system first vector
	CVec3 i = v0;
	Normalize( &i );
	
	// k (normal to maneuver plane)
	CVec3 k = v0 ^ x0x1;
	if ( fabs2( k ) < 1e-8f )
	{
		if ( i * x0x1 > 0 )
		{
			*vT = x0;
			pArc = 0;
			return true;
		}
		else
			k.z = 1;
	}
	
	Normalize( &k );

	// j ( perpendicular to x0, in manuver plane)
	CVec3 j = k ^ i * nDir;
	
	{
		pArc = new CPathFractionArc3D;

		// vR0 - vector from x0 to circle center
		// this vector must satisfy nonequation x1x0 * R0 > 0
		CVec3 vO = j * fR;
		CDirectedCircle circle;
		circle.center = CVec2( 0, fR ); // circle center transformed to local coordinate system

		// transform x1 to new coordiante system ( a, b - its new coordinates )
		const CVec3 x1t( x0x1 * i, x0x1 * j, x0x1 * k );
		// create circle with center in x0+R0 with radius fR (in manuver plane)

		circle.r = fR;
		CVec2 t1t, t2t;													// tangent points to find (in new coordinate system)
		const bool bFound = FindTangentPoints( CVec2(x1t.x, x1t.y), circle, &t1t, &t2t );
		if ( !bFound )
			return false;
		// choose needed tangent point

		const CVec2 vR1t( t1t - circle.center );						// transformed radius to tangent point 1
		const CVec2 vR2t( t2t - circle.center );						// transformed radius to tangent point 2
		const CVec2 vTangent1( CVec2(x1t.x, x1t.y) - t1t );			// tangent line 1
		//const CVec2 vTangent2( x1t - t2t );			// tangent line 2

		const CVec2 vR0t( - circle.center );		// transformed radius vector of x1
		const CVec2 v0t( 1, 0 );

		// circle rotation sign
		const int nCircleSign = Sign( vR0t.y * v0t.x - vR0t.x * v0t.y ); // always -1

		// check if pair of ( vR1t & vTangent1 ) and ( v0 & R0 ) is of same allignment
		if ( Sign( vR1t.y * vTangent1.x - vR1t.x * vTangent1.y ) == nCircleSign )
		{
			// if it is so, then 1st tangent - is needed tangent
			const int nDiff = DirectedDirsDifference( vR0t, vR1t, nCircleSign );
			*vT = i * t1t.x + j * t1t.y + x0;
			pArc->Init( i, j, k, circle, x0, *vT, fR * 2.0f * PI * nDiff / 65535.0f, x1 - *vT, nDiff );
		}
		else
		{
			// else, 2nd line is needed tangent
			const int nDiff = DirectedDirsDifference( vR0t, vR2t, nCircleSign );
			*vT = i * t2t.x + j * t2t.y + x0;
			pArc->Init( i, j, k, circle, x0, *vT, fR * 2.0f * PI * nDiff / 65535.0f, x1 - *vT, nDiff );
		}
	}
	return true;
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	CPathFractionCircleLineCircle
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CVec3 CPathFractionArc3D::GetEndTangent() const
{
	return v1;
}
/////////////////////////////////////////////////////////////////////////////
CVec3 CPathFractionArc3D::GetPoint( const float fDist ) const
{
	const CVec2 vt = GetVectorByDirection( GetDirectionByVector( -circle.center ) + nDiff * fDist / fLength ) * circle.r;
	return CVec3( vt.x * i + vt.y * j + x0 + j * circle.r );
}
/////////////////////////////////////////////////////////////////////////////
CVec3 CPathFractionArc3D::GetNormale( const float fDist ) const
{
	const CVec2 vn( -GetVectorByDirection( GetDirectionByVector( -circle.center ) + nDiff * fDist / fLength ) );
	return vn.x * i + vn.y * j;
}
/////////////////////////////////////////////////////////////////////////////
CVec3 CPathFractionArc3D::GetTangent( const float fDist ) const
{
	const CVec2 vt = GetVectorByDirection( GetDirectionByVector( -circle.center ) + nDiff * fDist / fLength );
	return CVec3( - vt.y  * i + vt.x  * j );
}
/////////////////////////////////////////////////////////////////////////////
CVec3 CPathFractionArc3D::GetStartTangent() const
{
	return i;
}
/////////////////////////////////////////////////////////////////////////////
void CPathFractionArc3D::DoSubstitute( IPathFraction *pNext )
{
	substitute.resize( 1 );
	
	CPathFractionBSPline3D *pCurve = new CPathFractionBSPline3D;
	substitute[0] = pCurve;
	pCurve->Init( x0, x1, i, GetEndTangent(), fLength );
	AfterSubstitute();
}
/////////////////////////////////////////////////////////////////////////////
CVec3 CPathFractionArc3D::GetStartPoint() const
{
	return x0;
}
/////////////////////////////////////////////////////////////////////////////
CVec3 CPathFractionArc3D::GetEndPoint() const
{
	return x1;
}
/////////////////////////////////////////////////////////////////////////////
void CPathFractionArc3D::Init( const CVec3 &_i, const CVec3 &_j, const CVec3 &_k,
						 const CDirectedCircle &_circle, const CVec3 &_x0, const CVec3 &_x1, const float _fLength,
						 const CVec3 &_v1, const int _nDiff )
{
	i = _i;
	j = _j;
	k = _k;
	circle = _circle;
	x0 = _x0;
	x1 = _x1;
	fLength = _fLength;
	v1 = _v1;
	nDiff = _nDiff;
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	CPathFractionArc
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CVec3 CPathFractionArc::GetPoint( const float fDist ) const
{
	//CRAP{ WILL OPTIMIZE 
	const CVec2 vFromX( x0 );
	const CVec2 vFromR( vFromX - circle.center );
	const WORD wFrom = GetDirectionByVector( vFromR );
	//CRAP}
	
	const WORD wSingleAngle = fDist/ circle.r / ( 2.0f * PI ) * 65535;
	const WORD wTo = wFrom + circle.nDir * nOrientation * wSingleAngle;
	const CVec2 vToR( circle.r * GetVectorByDirection( wTo ) );

	return CVec3( circle.center + vToR, 0 );
}
/////////////////////////////////////////////////////////////////////////////
CVec3 CPathFractionArc::GetTangent( const float fDist ) const
{
	//CRAP{ WILL OPTIMIZE
	const CVec2 vFromX( x0 );
	const CVec2 vFromR( vFromX - circle.center );
	const WORD wFrom = GetDirectionByVector( vFromR );
	//CRAP}
	
	const WORD wSingleAngle = fDist/ circle.r / ( 2.0f * PI ) * 65535;
	const WORD wTo = wFrom + circle.nDir * nOrientation * wSingleAngle;
	const CVec2 vToR( circle.r * GetVectorByDirection( wTo ) );

	return CVec3( -nOrientation * vToR.y * circle.nDir, nOrientation * vToR.x * circle.nDir, 0 );
}
/////////////////////////////////////////////////////////////////////////////
void CPathFractionArc::DoSubstitute( IPathFraction *pNext )
{
	// if path fraction is short, then substitute with b-spline
	// if long, then substitute with angle/90 + 1 hermit curves
	const float fAngle = fLength / circle.r;
	//CRAP{ WILL OPTIMIZE
	CVec2 vFromX( x0 );
	CVec2 vFromR( vFromX - circle.center );
	CVec2 vFromV ( -nOrientation * vFromR.y * circle.nDir, nOrientation * vFromR.x * circle.nDir );
	WORD wFrom = GetDirectionByVector( vFromR );
	//CRAP}
	
	if ( fAngle > PI / 5.0f  ) // big angle
	{
		const int nCount = fAngle / ( PI / 5.0f ) + 1;
		const float fSingleLenght = fLength / nCount;
		
		const WORD wSingleAngle = fSingleLenght / circle.r / ( 2.0f * PI ) * 65535;
		
		substitute.resize( nCount );

		for ( int i = 0; i < nCount; ++i )
		{
			const WORD wTo = wFrom + circle.nDir * nOrientation * wSingleAngle;
			const CVec2 vToR( circle.r * GetVectorByDirection( wTo ) );
			const CVec2 vToX = circle.center + vToR;

			CVec2 vToV( -nOrientation * vToR.y * circle.nDir, nOrientation * vToR.x * circle.nDir );

			//CPathFractionAnaliticCurve<CBezierCurve<CVec3> > *pCurve = new CPathFractionAnaliticCurve<CBezierCurve<CVec3> >;
			//pCurve->InitByCircle( wFrom, wTo, circle, fSingleLenght ); 
			
			CPathFractionBSPline3D *pCurve = new CPathFractionBSPline3D;
			substitute[i] = pCurve;
			pCurve->Init( vFromX, vToX, vFromV, vToV, fSingleLenght );
			
			vFromX = vToX;
			vFromR = vToR;
			vFromV = vToV;
			wFrom = wTo;
		}
	}
	else if ( fLength < FP_EPSILON )
	{
		substitute.resize( 1 );
		CPathFractionLine *pF = new CPathFractionLine;
		substitute[0] = pF;
		pF->Init( x0, x0 + vFromV, 0 );
	}
	else // small angle
	{
		substitute.resize( 1 );
		CPathFractionBSPline3D *pF = new CPathFractionBSPline3D;

		const WORD wTo = wFrom + circle.nDir * nOrientation * fAngle / 2/PI * 65535;
		const CVec2 vToR( circle.r * GetVectorByDirection( wTo ) );
		const CVec2 vToX = circle.center + vToR;
		CVec2 vToV( -nOrientation * vToR.y * circle.nDir, nOrientation * vToR.x * circle.nDir );

		pF->Init( x0, x1, vFromV, vToV, fLength );
		substitute[0] = pF;
	}
	AfterSubstitute();
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	CPathFractionCircleLineCircle
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void CPathFractionCircleLineCircle::Init( const CVec2 &x0, const CVec2 &x1, const CVec2 &v0, const CVec2 &v1, const float fR0, const float fR1, const float _fZ )
{
	// create 2 circles, tangent to initial direction
	CDirectedCircle r1, r2;												// circles, tangent to initial direction
	CDirectedCircle o1, o2;												// circles, tangent to final direction

	CVec2 v0Normalized = v0;
	Normalize( &v0Normalized );
	GetDirectedCirclesByTangent( v0Normalized , x0, fR0, &r1, &r2 );
	//GetCirclesByTangent( v0Normalized , x0, fStartRadius, &r1, &r2 );

	// to filal direction
	CVec2 v1Normalized =	v1;
	Normalize( &v1Normalized );
	GetDirectedCirclesByTangent( v1Normalized , x1, fR1, &o1, &o2 );

	CPtr<CPathFractionCircleLineCircle> pPath;
	pPath = new CPathFractionCircleLineCircle;
	

	bool bInitted = false;
	TryPath( x0, x1, r1, o1, pPath, this, &bInitted );
	TryPath( x0, x1, r1, o2, pPath, this, &bInitted );

	TryPath( x0, x1, r2, o1, pPath, this, &bInitted );
	TryPath( x0, x1, r2, o2, pPath, this, &bInitted );
	fZ = _fZ;
}
/////////////////////////////////////////////////////////////////////////////
void CPathFractionCircleLineCircle::TryPath( const CVec2 &x0, const CVec2 &x1, 
																						 const CDirectedCircle &r1, const CDirectedCircle &o1, 
																						 CPathFractionCircleLineCircle *pPath, CPathFractionCircleLineCircle *pBest,
																							bool *bInitted ) const
{
	CVec2 v1, v2;

	if ( GetDirectedCirclesTangentPoints( r1, o1, &v1, &v2 ) )
	{
		pPath->Init( r1, x0, v1, o1, v2, x1 );
		if ( !*bInitted || pPath->GetLength() < pBest->GetLength() )
			*pBest = *pPath;

		*bInitted = true;
	}
}
/////////////////////////////////////////////////////////////////////////////
void CPathFractionCircleLineCircle::Init( const CDirectedCircle &_start, const CVec2 &_vStart1, const CVec2 &_vStart2,
						 const CDirectedCircle &_finish, const CVec2 &_vFinish1, const CVec2 &_vFinish2 )
{
	if ( !pStart || !pFinish || !pLine )
	{
		pStart = new CPathFractionArc;
		pFinish = new CPathFractionArc;
		pLine = new CPathFractionLine;
	}

	pStart->Init( _start, _vStart1, _vStart2 );
	pFinish->Init( _finish, _vFinish1, _vFinish2 );
	pLine->Init( CVec3(_vStart2,0), CVec3(_vFinish1,0) );
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	CPathFractionCircleLineCircle
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/*
void CPathFractionCircleLineCircle3D::Init( const CVec3 &x0, const CVec3 &x1,			// coordinates
					 const CVec3 &v0, const CVec3 &v1,			// directions
					 const float r0, const float r1 )			// turn radii
{
	pStart = new CPathFractionArcLine3D;
	pFinish = new CPathFractionArcLine3D;

	// iterative search.
	pStart->Init( x0, v0, x1, r0 ); // forward direction, ignore v1
	CVec3 vForwardArcFinish = pStart->GetArc() ? pStart->GetArc()->GetEndPoint() : pStart->GetLine()->GetStartPoint();

	pFinish->Init( x1, -v1, vForwardArcFinish, r1 );	// backward, ignore speed on arc vForwardArcFinish
	CVec3 vBackArcFinish = pFinish->GetArc() ? pFinish->GetArc()->GetEndPoint() : pFinish->GetLine()->GetStartPoint();

	// improved forward 
	pStart->Init( x0, v0, vBackArcFinish, r0 );

	// second iteration

	vForwardArcFinish = pStart->GetArc() ? pStart->GetArc()->GetEndPoint() : pStart->GetLine()->GetStartPoint();
	pFinish->Init( x1, -v1, vForwardArcFinish, r1 );	// backward, ignore speed on arc vForwardArcFinish
	vBackArcFinish = pFinish->GetArc() ? pFinish->GetArc()->GetEndPoint() : pFinish->GetLine()->GetStartPoint();
	pStart->Init( x0, v0, vBackArcFinish, r0 );

}
/////////////////////////////////////////////////////////////////////////////
void CPathFractionCircleLineCircle3D::DoSubstitute( IPathFraction *pNext )
{
	CPathFractionArc3D *pStartArc = pStart->GetArc();
	CPathFractionArc3D *pFinishArc = pFinish->GetArc();
	const bool bStartArc = pStartArc != 0;
	const bool bFinishArc = pFinishArc != 0;
	

	substitute.resize( bStartArc + 1 + bFinishArc );

	int nIndex = 0;
	if ( bStartArc )
	{
		substitute[nIndex++] = pStartArc;
		pStartArc->DoSubstitute( 0 );	
	}

	if ( bStartArc && bFinishArc )
	{
		CPathFractionBSPline3D * pSpline = new CPathFractionBSPline3D ;
		pFinishArc->Negate();
	
		pSpline->Init( pStartArc->GetEndPoint(),
									 pFinishArc->GetStartPoint(),
									 pStartArc->GetEndTangent(),
									 pFinishArc->GetStartTangent(), pStart->GetLine()->GetLength() );
		pSpline->Init( pStartArc->GetEndPoint(),
									 pFinishArc->GetEndPoint(),
									 pStartArc->GetEndTangent(),
									 -pFinishArc->GetEndTangent(), pStart->GetLine()->GetLength() );

		substitute[nIndex++] = pSpline;
	}
	else
		substitute[nIndex++] = pStart->GetLine();


	if ( bFinishArc )
	{
		substitute[nIndex] = pFinish->GetArc();
		if ( !bStartArc )
			substitute[nIndex]->Negate();
		pFinish->GetArc()->DoSubstitute( 0 );
	}
	AfterSubstitute();
}*/