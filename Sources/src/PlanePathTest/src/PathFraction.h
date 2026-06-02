#ifndef _plane_simple_path_fraction_
#define _plane_simple_path_fraction_

#include "PathFraction.h"
#include "IPlanePathFraction.h"
#include "PlanePathMath.h"

inline CVec3 ToVec3( const CVec2 &v ) { return CVec3(v,0); }
inline CVec3 ToVec3( const CVec3 &v ) { return v; }
class CPathFractionLine : public IPathFraction
{
	OBJECT_COMPLETE_METHODS( CPathFractionLine )

	CVec3 x0, x1;
	CVec3 v0;
	float fLength;
	float fLengthMultiply;
public:
	void Init( const CVec2 &_x0, const CVec2 &_x1, const float _fLength = -1 )
	{
		Init( ToVec3(_x0), ToVec3(_x1), _fLength );
	}
	
	void Init( const CVec3 &_x0, const CVec3 &_x1, const float _fLength = -1 )
	{
		x0 = _x0;
		x1 = _x1;
		v0 = x1 - x0;
		Normalize( &v0 );
		if ( _fLength == -1 )
		{
			fLengthMultiply = 1.0f;
			fLength = fabs(x0-x1);
		}
		else
		{
			fLength = _fLength;
			fLengthMultiply = fabs(x0-x1)/_fLength;
		}
	}
	virtual float STDCALL GetLength() const { return fLength; }
	virtual CVec3 STDCALL GetPoint( const float fDist ) const { return x0 + v0 * fLengthMultiply*fDist; }
	virtual CVec3 STDCALL GetTangent( const float fDist ) const { return v0; }
	virtual CVec3 STDCALL GetNormale( const float fDist ) const 
	{ 
		const CVec3 vXY = CVec3( -v0.y, v0.x, 0 );
		return v0^vXY; 
	}
	
	virtual CVec3 STDCALL GetStartPoint() const { return x0;}
	virtual CVec3 STDCALL GetStartTangent() const { return v0;}

	virtual CVec3 STDCALL GetEndPoint() const { return x1;}
	virtual CVec3 STDCALL GetEndTangent() const { return v0;}
};
template <class TAanliticCurve>
class CPathFractionAnaliticCurve : public IPathFraction
{
	OBJECT_COMPLETE_METHODS( CPathFractionAnaliticCurve )
	TAanliticCurve curve;
	float fLength;										// lenght is ecternal
	
public:
	void InitBySpeeds( const CVec2 &x0, const CVec2 &x1, const CVec2 &v0, const CVec2 &v1, const float fLenght )
	{
		InitBySpeeds( ToVec3(x0), ToVec3(x1), ToVec3(v0), ToVec3(v1), fLenght );
	}
	void InitByCircle( const WORD wFrom, const WORD wTo, const CDirectedCircle &circle, const float _fLenght )
	{
		const int nDiff = DirectedDirsDifference( wFrom, wTo, circle.nDir );

		InitBySpeeds( circle.center + GetVectorByDirection( wFrom ) * circle.r,
									circle.center + GetVectorByDirection( wFrom + circle.nDir *nOrientation* nDiff/3 ) * circle.r,
									circle.center + GetVectorByDirection( wTo   - circle.nDir *nOrientation* nDiff/3 ) * circle.r,
									circle.center + GetVectorByDirection( wTo ) * circle.r,
									_fLenght );
	}
	void InitByCoords( const CVec2 &x0, const CVec2 &x1, const CVec2 &v0, const CVec2 &v1, const float fLenght )
	{
		InitByCoords( ToVec3(x0), ToVec3(x1), ToVec3(v0), ToVec3(v1), fLenght );
	}

	void InitByCoords( const CVec3 &x0, const CVec3 &x1, const CVec3 &_v0, const CVec3 &_v1, const float _fLenght )
	{
		const float fR = fabs( x0 - x1 ) / 4.0f;
		CVec3 v0 = _v0;
		Normalize( &v0 );
		CVec3 v1 = _v1;
		Normalize( &v1 );
		curve.Setup( x0, x0 + fR * v0, x1 - fR * v1, x1 );
		fLength = _fLenght;
	}
	
	void InitBySpeeds( const CVec3 &x0, const CVec3 &x1, const CVec3 &v0, const CVec3 &v1, const float _fLenght )
	{
		curve.Setup( x0, x1, v0, v1 );
		fLength = _fLenght;
	}
	virtual CVec3 STDCALL GetPoint( const float fDist ) const { return ToVec3(curve.Get( fDist / fLength )); }
	virtual CVec3 STDCALL GetTangent( const float fDist ) const { return ToVec3(curve.GetDiff1( fDist / fLength )); }
	virtual float STDCALL GetLength() const { return fLength; }
};

class CPathFractionBezier3D : public CPathFractionAnaliticCurve< CBezierCurve<CVec3> >
{
	OBJECT_COMPLETE_METHODS( CPathFractionBezier3D )
	CBezierCurve<CVec3> bezier;
	float fLength;										// lenght is ecternal
public:
};
class CPathFractionBSPline3D : public IPathFraction
{
	OBJECT_COMPLETE_METHODS( CPathFractionBSPline3D )

	typedef std::pair<CAnalyticBSpline3,float> CSplineLength;
	typedef std::vector<CSplineLength> CSplines;
	CSplines splines;
	
	float fLength;												// desired curve length
	float fSumLengh;											// splines summary length

	static const int nPresizion;

	const CSplineLength & GetCur( const float fDist, float * const fNewDist ) const;
	
public:

	void Init( const CVec2 &x0, const CVec2 &x1, const CVec2 &v0, const CVec2 &v1, const float _fLength )
	{
		Init( ToVec3(x0), ToVec3(x1), ToVec3(v0), ToVec3(v1), _fLength );
	}

	void Init( const CVec3 &x0, const CVec3 &x1, const CVec3 &_v0, const CVec3 &_v1, const float _fLength );
	
	virtual CVec3 STDCALL GetPoint( const float fDist ) const;
	virtual CVec3 STDCALL GetTangent( const float fDist ) const;
	virtual float STDCALL GetLength() const { return fLength; }
};

#endif //_plane_simple_path_fraction_