#ifndef __AI_GEOMETRY_H__
#define __AI_GEOMETRY_H__

#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*								  Geometry for AI : vectors, lines								*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 V3_CAMERA_HOR = CVec3( -FP_SQRT_2/2.0f, FP_SQRT_2/2.0f, 0.0f ); //V3_AXIS_Y - V3_AXIS_X
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										  CVector																			*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVector
{
	int x, y;
	//
	SVector() {  }
	SVector( const int _x, const int _y ) : x( _x ), y( _y ) {  }
	SVector( const SVector &vec ) : x( vec.x ), y( vec.y ) {  }
	SVector( const CVec2 &vec ) : x( vec.x ), y( vec.y ) {  }

	const CVec2 ToCVec2() const { return CVec2( x, y ); }
	const CVec2 Norm() const 
	{ 
		if ( float revR = sqrt( x*x + y*y ) )
		{
			revR = 1 / revR;
			return CVec2( float(x)*revR, float(y)*revR ); 
		}
		else
			return VNULL2;
	}
	//
	void TurnLeft()	 { std::swap( x, y ); x = -x; }
	void TurnRight() { std::swap( x, y ); y = -y; }

	void TurnLeftUntilAxis()
 	{
		const short int signX = Sign( x );
		const short int signY = Sign( y );
	/*	
		if ( signX >= 0 && signY == -1 ) 	{ x = 1;  y = 0; return; }
		if ( signX == 1 && signY >= 0 )		{ x = 0;  y = 1; return; }
		if ( signX <= 0 && signY == 1 )		{ x = -1; y = 0; return; }
		if ( signX == -1 && signY <= 0 )	{ x = 0;  y = -1; return; }
	*/
		if ( signX != 0 && signX * signY >= 0 )
			x = 0, y = signX;
		else
			x = -signY, y = 0;
	}
	
	void TurnLeftUntil45()
	{
		const short int signX = Sign( x );
		const short int signY = Sign( y );

		if ( signX != 0 && signX * signY >= 0 )
		{
			if ( abs(x) > abs(y) )
				x = signX;
			else
				x = 0;
			y = signX;
		}
		else
		{
			if ( abs(y) > abs(x) )
				y = signY;
			else
				y = 0;

			x = -signY;
		}
	}

	void TurnRightUntilAxis()
	{
		const short int signX = Sign( x );
		const short int signY = Sign( y );
	/*	
		if ( signX >=0 && signY == 1 )		{ x = 1;  y = 0; return; }
		if ( signX == -1 && signY >= 0 )	{ x = 0;  y = 1; return; }
		if ( signX <= 0 && signY == -1 )	{ x = -1; y = 0; return; }
		if ( signX == 1 && signY <= 0 )		{ x = 0;  y = -1; return; }
	*/
		if ( signX != 0 && signX * signY <= 0 ) 
			x = 0, y = -signX;
		else
			y = 0, x = signY;
	}

	void TurnRightUntil45()
	{
		const short int signX = Sign( x );
		const short int signY = Sign( y );

		if ( signX != 0 && signX * signY <= 0 ) 
		{
			if ( abs(x) > abs(y) )
				x = signX;
			else
				x = 0;
			y = -signX;
		}
		else
		{
			if ( abs(y) > abs(x) )
				y = signY;
			else
				y = 0;
			x = signY;
		}
	}

	const bool operator!=( const SVector &vec ) const { return x != vec.x || y != vec.y; }
	const bool operator==( const SVector &vec )	const { return x == vec.x && y == vec.y; }
	const SVector operator-( const SVector &vec ) const { return SVector( x-vec.x, y-vec.y ); }
	const SVector operator+( const SVector &vec ) const { return SVector( x+vec.x, y+vec.y ); }
	SVector& operator+=( const SVector &vec ) { x += vec.x; y += vec.y; return *this; }
	SVector& operator-=( const SVector &vec ) { x -= vec.x; y -= vec.y; return *this; }
	const int operator*( const SVector &vec ) const { return x*vec.x + y*vec.y; }
	SVector& operator*=( const int n ) { x *= n; y *= n; return *this; }
	// деление нацело
	SVector& operator/=( const int n ) { const float coeff = 1.0f / float(n); x *= coeff; y *= coeff; return *this; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// только с достаточно близкими точками
inline long SquareOfDistance( const SVector &v1, const SVector &v2 )
{
	return square(long(v1.x-v2.x))+square(long(v1.y-v2.y));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline float SquareOfDistance( const CVec2 &v1, const CVec2 &v2 )
{
	return square(v1.x-v2.x) + square(v1.y-v2.y);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsAlmostZero( CVec2 vec )
{
	const static float eps = 0.0001f;
	return ( fabs( vec.x ) < eps  && fabs( vec.y ) < eps );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsAlmostZero( const float x, const float y )
{
	const static float eps = 0.0001f;
	return ( fabs( x ) < eps  && fabs( y ) < eps );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline int mDistance( const SVector &vec1, const SVector &vec2 )
{
	return Max( abs( vec1.x-vec2.x ), abs( vec1.y-vec2.y ) );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline CVec2 Project( const CVec2 &vec, const CVec2 &axis )
{
	if ( axis == VNULL2 )
		return VNULL2;
	else
		return axis*( (vec*axis) / fabs2(axis) );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline float DoubleTrSquare( const CVec2 &side1, const CVec2 &side2 )
{
	return side1.x*side2.y - side2.x*side1.y;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline CVec2 Norm( const CVec2 &vec )
{
	if ( vec == VNULL2 )
		return VNULL2;
	else
		return vec/fabs(vec);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*										  SLine																				*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLine
{
	int a, b, c;

	SLine() {  };
	SLine( int _a, int _b, int _c ) : a( _a ), b( _b ), c( _c ) {  }
	SLine( const SVector &ptStart, const SVector &ptFinish ) 
		: a( ptFinish.y - ptStart.y ), b( ptStart.x - ptFinish.x ), c( ptFinish.x*ptStart.y - ptStart.x*ptFinish.y ) {  }

	bool IsPointOnLine( const SVector &point )	const { return a*point.x + b*point.y + c == 0; }
	const int GetHPLineSign( const SVector &point ) const { return Sign( a*point.x + b*point.y + c ); }
	bool IsSegmIntersectLine( const SVector &ptStart, const SVector &ptFinish) const
	{
		const int t1 = Sign( GetHPLineSign( ptStart ) );
		const int t2 = Sign( GetHPLineSign( ptFinish ) );
		return (t1 >= 0) && (t2 <= 0) || (t1 <= 0) && (t2 >= 0);
	}
	const SLine GetPerpendicular( const SVector &point ) const { return SLine( -b, a, b*point.x - a*point.y ); }

	const SVector GetDirVector() const { return SVector( -b, a ); }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CBres																*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBres
{
	int xerr, yerr;
	int xlen, ylen, len;
	int xinc, yinc;
	
	SVector dir;
	//
	void Initialize()
	{
		xerr = 0;
		yerr = 0;
		
		xinc = Sign( xlen );
		yinc = Sign( ylen );
		xlen = abs( xlen ) + 1;
		ylen = abs( ylen ) + 1;
		len = Max( xlen, ylen );
	}

public:
	// дл€ того, чтобы выдавал направлени€
	void Init( const SVector &start, const SVector &finish)
	{ 
		xlen = finish.x - start.x;
		ylen = finish.y - start.y;

		Initialize(); 
	}

	// дл€ того, чтобы выдавал точки
	void InitPointByDirection( const SVector& start, const SVector &direction )
	{ 
		dir = start;
		xlen = direction.x;
		ylen = direction.y;

		Initialize(); 
	}
	
	// дл€ того, чтобы выдавал точки
	void InitPoint( const SVector &start, const SVector &finish)
	{
		dir = start;
		xlen = finish.x - start.x;
		ylen = finish.y - start.y;

		Initialize();
	}
	
	//
	void MakeStep()
	{
		xerr += xlen;
		if ( xerr >= len )
			dir.x = xinc, xerr -= len;
		else
			dir.x = 0;

		yerr += ylen;
		if ( yerr >= len )
			dir.y = yinc, yerr -= len;
		else
			dir.y = 0;
	}

	void MakePointStep()
	{
		xerr += xlen;
		if ( xerr >= len )
			dir.x += xinc, xerr -= len;

		yerr += ylen;
		if ( yerr >= len )
			dir.y += yinc, yerr -= len;
	}
	
	const SVector& GetDirection() const { return dir; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															SRect																*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRect
{
	union
	{
		struct { CVec2 v[4]; };
		struct { CVec2 v1, v2, v3, v4; };
	};
	CVec2 dir, dirPerp, center;
	float lengthAhead, lengthBack, width;

	SRect() { }

	bool IsIntersectProject( const CVec2 &v1, const CVec2 &v2, const CVec2 &v3, const CVec2 &v4, const CVec2 &dir, const float min, const float max ) const;

//public:
	void InitRect( const CVec2 &_v1, const CVec2 &_v2, const CVec2 &_v3, const CVec2 &_v4 );
	// задаЄтс€ половина реальной длины и ширины (как бы радиусы)
	void InitRect( const CVec2 &center, const CVec2 &dir, const float length, const float width );
	// задаютс€ длины вперЄд и назад, половина ширины
	void InitRect( const CVec2 &center, const CVec2 &dir, const float lengthAhead, const float lengthBack, const float width );

	bool IsIntersected( const SRect &rect ) const;
	
	// границы пр€моугольника не принадлежат ему
	bool IsPointInside( const CVec2 &point ) const;
	bool IsIntersectCircle( const CVec2 &circleCenter, const float r ) const;
	bool IsIntersectCircle( const CCircle &circle ) const { return IsIntersectCircle( circle.center, circle.r ); }

	const int GetSide( const WORD dirFromRectCenter ) const;
	const int GetSide( const CVec2 &point ) const;
	
	void Compress( const float fFactor );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float fabs( const SRect rect1, const SRect rect2 );
// угол, под которым rect виден из точки point
const WORD GetVisibleAngle( const CVec2 point, const SRect rect );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CBresZ															*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBresZ
{
	int xerr, yerr;
	int xlen, ylen, len;
	int xinc, yinc, zinc, rinc;

	int intPart, dxy;
	
	SVector dir;
	int z, r;

public:
	void InitXY( const SVector &start, const SVector &finish )
	{ 
		dir = start; 
		
		xlen = finish.x - start.x;
		ylen = finish.y - start.y;

		xerr = 0;
		yerr = 0;
		
		xinc = Sign( xlen );
		yinc = Sign( ylen );

		xlen = abs( xlen ) + 1;
		ylen = abs( ylen ) + 1;
		len = Max( xlen, ylen );

		NI_ASSERT_SLOW_T( xlen != 0 || ylen != 0, "Wrong line" );
	}

	void InitZ( const int startZ, const int finishZ, const SVector &startPoint, const SVector &finishPoint )
	{
		NI_ASSERT_T( startPoint != finishPoint, "Wrong ray" );
		
		const int zlen = finishZ-startZ;
		dxy = Max( abs( finishPoint.x-startPoint.x ), abs( finishPoint.y-startPoint.y ) );

		intPart = zlen / dxy;
		rinc = abs( zlen % dxy );
 		zinc = Sign( zlen );

		z = finishZ; 
		r = 0;
	}

	void InitZWithStep( const int startZ, const int finishZ, const SVector &startPoint, const SVector &finishPoint )
	{
		NI_ASSERT_T( startPoint != finishPoint, "Wrong ray" );
		
		const int zlen = finishZ-startZ;
		dxy = Max( abs( finishPoint.x-startPoint.x ), abs( finishPoint.y-startPoint.y ) );
		
		intPart = zlen / dxy;
		rinc = abs( zlen % dxy );
		zinc = Sign( zlen );

		z = finishZ+intPart;
		r = rinc;
	}

	void MakeStep()
	{
		xerr += xlen;
		yerr += ylen;

		if ( xerr >= len )
			dir.x += xinc, xerr -= len;
		if ( yerr >= len )
			dir.y += yinc, yerr -= len;

		z += intPart;
		r += rinc;
		if ( r >= dxy )
			z += zinc, r -= dxy;
	}
	
	const SVector& GetPoint() const { return dir; }
	const int GetZ() const { return z; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CBSplne																*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBSpline
{
	DECLARE_SERIALIZE;

	CVec2 a, b, c, d;
	CVec2 x, dx, d2x, d3x;
	CVec2 fw_dx, fw_d2x, fw_d3x;
	float t, tForward;
	BYTE cntToForward;
public:
	// дл€ вычислени€ сплайна
	const static float DELTA;
	// дл€ просмотра вперЄд на предмет залоканных тайлов на пути
	const static float DELTA_FORWARD;
	const static int N_OF_ITERATONS;
	const static int N_ITERS_TO_FORWARD;

	void Init( const CVec2 &p3, const CVec2 &p2, const CVec2 &p1, const CVec2 &p0 );

	void Iterate();

	const CVec2& GetPoint() const { return x; }
	const CVec2& GetDX() const { return dx; }
	const float GetReverseR() const;

	struct SForwardIter
	{
		float t;
		CVec2 x;
		CVec2 fw_dx, fw_d2x, fw_d3x;
	};

	// если pIter->t == -1, то дальше итерировать нельз€, т.к. сплайн кончилс€
	const void StartForwardIterating( SForwardIter *pIter );
	const void IterateForward( SForwardIter *pIter );
	
	void DumpState() const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const WORD GetDirectionByVector( CVec2 vec );
const WORD GetDirectionByVector( float x, float y );
const CVec2 GetVectorByDirection( const WORD dir );
const WORD GetZDirectionBy3DVector( const float x, const float y, const float z );
const WORD GetZDirectionBy3DVector( const CVec2 &vec, const float z );
// угол между между вектором и OXY
const WORD GetZAngle( const float x, const float y, float z );
// угол между между вектором и OXY
const WORD GetZAngle( const CVec2 &vec, const float z );
const WORD DirsDifference( const WORD dir1, const WORD dir2 );
const int DifferenceSign( const WORD dir1, const WORD dir2 );
// в угле от startAngleDir до finishAngleDir против часовой
bool IsInTheAngle( const WORD dir, const WORD startAngleDir, const WORD finishAngleDir );
// dir в минимальном угле мжду dir1 и dir2
bool IsInTheMinAngle( const WORD dir, const WORD dir1, const WORD dir2 );
// минимальное рассто€ние от точки до отрезка
const float GetDistanceToSegment( const CVec2 &vSegmentStart, const CVec2 &vSegmentEnd, const CVec2 &vPoint );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AI_GEOMETRY_H__
