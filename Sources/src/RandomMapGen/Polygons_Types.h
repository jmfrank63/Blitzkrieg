#if !defined(__Polygons__Types__)
#define __Polygons__Types__

#include "..\formats\fmtVSO.h"
#include "..\formats\fmtVSO.h"

extern const float RMGC_MINIMAL_VIS_POINT_DISTANCE;	//2.0f

template <class TYPE>
inline const TYPE GetPointType( const CVec2 &vec, TYPE *pTargetType )
{
	NI_ASSERT_T( false, "Can convert only to CVec2 and CVec3!" );
}
template <>
inline const CVec2 GetPointType<CVec2>( const CVec2 &vec, CVec2 *pVec2 )
{
	return vec;
}
template <>
inline const CVec3 GetPointType<CVec3>( const CVec2 &vec, CVec3 *pVec3 )
{
	return CVec3( vec.x, vec.y, 0.0f );
}

template <class TYPE>
inline const TYPE GetPointType( const CVec3 &vec, TYPE *pTargetType )
{
	NI_ASSERT_T( false, "Can convert only to CVec2 and CVec3!" );
}
template <>
inline const CVec2 GetPointType<CVec2>( const CVec3 &vec, CVec2 *pVec2 )
{
	return CVec2( vec.x, vec.y );
}
template <>
inline const CVec3 GetPointType<CVec3>( const CVec3 &vec, CVec3 *pVec3 )
{
	return vec;
}

template <class TYPE>
struct SInRangeFunctional
{
	float fRange;
	SInRangeFunctional() : fRange( RMGC_MINIMAL_VIS_POINT_DISTANCE * RMGC_MINIMAL_VIS_POINT_DISTANCE ) {}
	SInRangeFunctional( float _fRange ) : fRange( _fRange ) {}

	bool operator()( const TYPE &rElement1, const TYPE &rElement2 ) const
	{
		return fabs2( rElement1 - rElement2 ) <= fRange;
	}
};

enum EClassifyEdge
{
	CE_UNKNOWN	= 0,
	
	CE_LEFT			= 1,
	CE_RIGHT		= 2,
	CE_BEYONG		= 3,
	CE_BEHIND		= 4,
	CE_BETWEEN	= 5,
	CE_BEGIN		= 6,
	CE_END			= 7,
	
	CE_COUNT		= 8,
};
extern const EClassifyEdge NEGATIVE_CLASSIFY_EDGE[CE_COUNT + 1];

enum EClassifyPolygon
{
	CP_UNKNOWN	= 0,
	
	CP_INSIDE		= 1,
	CP_OUTSIDE	= 2,
	CP_BOUNDARY = 3,
	CP_VERTEX		= 4,
	
	CP_COUNT		= 5,
};
extern const EClassifyPolygon NEGATIVE_CLASSIFY_POLYGON[CP_COUNT + 1];

enum EClassifyIntersection
{
	CI_UNKNOWN				= 0,
	
	CI_COLLINEAR			= 1,
	CI_PARALLEL				= 2,			
	CI_SKEW						= 3,
	CI_SKEW_CROSS			= 4,
	CI_SKEW_NO_CROSS	= 5,	

	CI_COUNT					= 6,
};

enum EClassifyRotation
{
	CR_UNKNOWN					= 0,
	
	CR_CLOCKWISE				= 1,
	CR_COUNTERCLOCKWISE	= 2,
	CR_LINE							= 3,

	CR_COUNT						= 4,
};
extern const EClassifyRotation NEGATIVE_CLASSIFY_ROTATION[CR_COUNT + 1];

inline EClassifyEdge GetNegativeClassifyEdge( EClassifyEdge classifyEdge )
{
	return NEGATIVE_CLASSIFY_EDGE[static_cast<int>(classifyEdge)];
}

inline EClassifyPolygon GetNegativeClassifyPolygon( EClassifyPolygon classifyPolygon )
{
	return NEGATIVE_CLASSIFY_POLYGON[static_cast<int>(classifyPolygon)];
}

inline EClassifyRotation GetNegativeClassifyRotation( EClassifyRotation classifyRotation )
{
	return NEGATIVE_CLASSIFY_ROTATION[static_cast<int>(classifyRotation)];
}

inline EClassifyEdge GetClassifyNormal()
{
	return CE_LEFT;
}

inline EClassifyEdge GetClassifyEdgeInnerSpace( EClassifyRotation classifyRotation )
{
	if ( classifyRotation == CR_CLOCKWISE )
	{
		return CE_RIGHT;
	}
	else if (  classifyRotation == CR_COUNTERCLOCKWISE )
	{
		return CE_LEFT;
	}
	else
	{
		return CE_UNKNOWN;
	}
}

template<class PointType>
EClassifyEdge ClassifyEdge( const PointType &rvBegin, const PointType &rvEnd, const PointType &v )
{
	const PointType v10 = rvEnd - rvBegin;
	const PointType v20 = v - rvBegin;
	const float s1020 = v10.x * v20.y - v10.y * v20.x;
	if ( s1020 > 0.0f )
	{
		return CE_LEFT;
	}
	else if ( s1020 < 0.0f )
	{
		return CE_RIGHT;
	}
	else if ( ( ( v10.x * v20.x ) < 0.0f ) || ( ( v10.y * v20.y ) < 0.0f ) )
	{
		return CE_BEHIND;
	}
	else if ( fabs2( v10.x, v10.y ) < fabs2( v20.x, v20.y ) )
	{
		return CE_BEYONG;
	}
	else if ( ( v.x == rvBegin.x ) && ( v.y == rvBegin.y ) )
	{
		return CE_BEGIN;
	}
	else if ( ( v.x == rvEnd.x ) && ( v.y == rvEnd.y ) )
	{
		return CE_END;
	}
	return CE_BETWEEN;
}

template<class PointType>
EClassifyIntersection ClassifyIntersect( const PointType &rvBegin0, const PointType &rvEnd0, const PointType &rvBegin1, const PointType &rvEnd1, float *pfIntercectionPoint )
{
	PointType edge1Normal( rvBegin1 );
	edge1Normal.x = ( rvEnd1 - rvBegin1 ).y;
	edge1Normal.y = ( rvBegin1 - rvEnd1 ).x;

	const PointType vEdge0 = rvEnd0 - rvBegin0;
	const float fDenominator = edge1Normal.x * vEdge0.x + edge1Normal.y * vEdge0.y;
	if ( fDenominator == 0.0f )
	{
		EClassifyEdge classifyEdge = ClassifyEdge( rvBegin1, rvEnd1, rvBegin0 );
		if ( ( classifyEdge == CE_LEFT ) || ( classifyEdge == CE_RIGHT ) )
		{
			return CI_PARALLEL;
		}
		else
		{
			return CI_COLLINEAR;
		}
	}
	else
	{
		if ( pfIntercectionPoint )
		{
			PointType v = rvBegin0 - rvBegin1;
			( *pfIntercectionPoint ) = ( ( -1.0f ) * ( edge1Normal.x * v.x +  edge1Normal.y * v.y ) ) / fDenominator;
		}
		return CI_SKEW;
	}
}

template<class PointType>
EClassifyIntersection ClassifyCross( const PointType &rvBegin0, const PointType &rvEnd0,  const PointType &rvBegin1, const PointType &rvEnd1, float *pfIntercectionPoint )
{
	float fInnerIntersectionPoint = 0.0f;
	EClassifyIntersection innerClassifyIntersection = ClassifyIntersect( rvBegin0, rvEnd0,  rvBegin1, rvEnd1, &fInnerIntersectionPoint );
	if ( pfIntercectionPoint )
	{
		( *pfIntercectionPoint ) = fInnerIntersectionPoint;
	}
	if ( innerClassifyIntersection != CI_SKEW )
	{
		return innerClassifyIntersection;
	}
	else
	{
		if ( ( fInnerIntersectionPoint < 0.0f ) || ( fInnerIntersectionPoint > 1.0f ) )
		{
			return CI_SKEW_NO_CROSS;
		}
		else
		{
			float fOuterIntersectionPoint = 0.0f;
			EClassifyIntersection	OuterClassifyIntersection = ClassifyIntersect( rvBegin1, rvEnd1,  rvBegin0, rvEnd0, &fOuterIntersectionPoint );
			if ( ( fOuterIntersectionPoint < 0.0f ) || ( fOuterIntersectionPoint > 1.0f ) )
			{
				return CI_SKEW_NO_CROSS;
			}
			else
			{
				return CI_SKEW_CROSS;
			}
		}
	}
}

/**
template<class Type, class PointType>
EClassifyIntersection ClassifyCross( const Type &rPolygon, const PointType &rvBegin, const PointType &rvEnd, Type::const_iterator *pBeginIterator, Type::const_iterator *pEndIterator, float *pfIntercectionPoint )
{
	if ( rPolygon.empty() )
	{
		return CI_UNKNOWN;
	}

	Type::const_iterator currentPointIterator0 = rPolygon.begin();
	Type::const_iterator currentPointIterator1 = rPolygon.begin();
	
	++currentPointIterator1;
	if ( currentPointIterator1 == rPolygon.end() )
	{
		EClassifyEdge classifyEdge = ClassifyEdge( rvBegin, rvEnd, ( *currentPointIterator0 ) );
		if ( ( classifyEdge == CE_BETWEEN ) ||  ( classifyEdge == CE_BEGIN ) || ( classifyEdge == CE_END ) )
		{
			if ( pBeginIterator )
			{
				( *pBeginIterator ) = currentPointIterator0;
			}
			if ( pEndIterator )
			{
				( *pEndIterator ) = currentPointIterator0;
			}
			if ( pfIntercectionPoint )
			{
				( *pfIntercectionPoint ) = 0;
			}
			return CI_SKEW_CROSS;
		}
		else
		{
			return CI_UNKNOWN;
		}
	}
	
	float fMinIntersectionPoint = 2.0f;
	while ( currentPointIterator0 != rPolygon.end() )
	{
		float fCrossPoint = 0.0f;
		EClassifyIntersection classifyIntersection = ClassifyCross( rvBegin, rvEnd, ( *currentPointIterator0 ), ( *currentPointIterator1 ), &fCrossPoint );
		if ( classifyIntersection == CI_SKEW_CROSS )
		{
			if ( fCrossPoint < fMinIntersectionPoint )
			{
				fMinIntersectionPoint = fCrossPoint;
				if ( pBeginIterator )
				{
					( *pBeginIterator ) = currentPointIterator0;
				}
				if ( pEndIterator )
				{
					( *pEndIterator ) = currentPointIterator1;
				}
				if ( pfIntercectionPoint )
				{
					PointType vCrossPoint = GetPointOnEdge( rvBegin, rvEnd, fMinIntersectionPoint );
					float fEdgeLength2 = fabs2( ( *currentPointIterator1 ) - ( *currentPointIterator0 ) );
					if ( fEdgeLength2 > 0 )
					{
						( *pfIntercectionPoint ) = sqrt( fabs2( vCrossPoint - ( *currentPointIterator0 ) ) / fEdgeLength2 );
					}					
					else
					{
						( *pfIntercectionPoint ) = 0.0f;
					}
				}
			}
		}
		++currentPointIterator0;
		++currentPointIterator1;
		if ( currentPointIterator1 == rPolygon.end() )
		{
			currentPointIterator1 = rPolygon.begin();
		}
	}
	if ( fMinIntersectionPoint <= 1.0f )
	{
		return CI_SKEW_CROSS;
	}
	else
	{
		return CI_UNKNOWN;
	}
}
/**/

template<class Type, class PointType>
EClassifyPolygon ClassifyConvexPolygon( const Type &rPolygon, const PointType &v )
{
	if ( rPolygon.empty() )
	{
		return CP_OUTSIDE;
	}

	Type::const_iterator currentPointIterator0 = rPolygon.begin();
	Type::const_iterator currentPointIterator1 = rPolygon.begin();
	
	++currentPointIterator1;
	if ( currentPointIterator1 == rPolygon.end() )
	{
		if ( ( currentPointIterator0->x == v.x ) && ( currentPointIterator0->y == v.y ) )
		{
			return CP_VERTEX;
		}
		else
		{
			return CP_OUTSIDE;
		}
	}
	
	EClassifyEdge classifyEdge = CE_UNKNOWN;
	while ( ( classifyEdge != CE_LEFT ) && ( classifyEdge != CE_RIGHT ) )
	{
		classifyEdge = ClassifyEdge( ( *currentPointIterator0 ), ( *currentPointIterator1 ), v );
		if ( classifyEdge == CE_BETWEEN )
		{
			return CP_BOUNDARY;
		}
		else if ( ( classifyEdge == CE_BEGIN ) || ( classifyEdge == CE_END ) )
		{
			return CP_VERTEX;
		}

		++currentPointIterator0;
		if ( currentPointIterator0 == rPolygon.end() )
		{
			return CP_OUTSIDE;
		}
		++currentPointIterator1;
		if ( currentPointIterator1 == rPolygon.end() )
		{
			currentPointIterator1 = rPolygon.begin();
		}
	}
	
	EClassifyEdge negativeClassifyEdge = GetNegativeClassifyEdge( classifyEdge );
	while ( currentPointIterator0 != rPolygon.end() )
	{
		classifyEdge = ClassifyEdge( ( *currentPointIterator0 ), ( *currentPointIterator1 ), v );
		if ( classifyEdge == negativeClassifyEdge )
		{
			return CP_OUTSIDE;
		}
		++currentPointIterator0;
		++currentPointIterator1;
		if ( currentPointIterator1 == rPolygon.end() )
		{
			currentPointIterator1 = rPolygon.begin();
		}
	}
	return CP_INSIDE;
}

template<class Type, class PointType>
EClassifyPolygon ClassifyPolygon( const Type &rPolygon, const PointType &v )
{
	if ( rPolygon.empty() )
	{
		return CP_OUTSIDE;
	}

	Type::const_iterator currentPointIterator0 = rPolygon.begin();
	Type::const_iterator currentPointIterator1 = rPolygon.begin();

	++currentPointIterator1;
	if ( currentPointIterator1 == rPolygon.end() )
	{
		if ( ( currentPointIterator0->x == v.x ) && ( currentPointIterator0->y == v.y ) )
		{
			return CP_VERTEX;
		}
		else
		{
			return CP_OUTSIDE;
		}
	}
	
	EClassifyPolygon classifyPolygon = CP_OUTSIDE;
	while ( currentPointIterator0 != rPolygon.end() )
	{
		EClassifyEdge classifyEdge = ClassifyEdge( ( *currentPointIterator0 ), ( *currentPointIterator1 ), v );
		if ( classifyEdge == CE_BETWEEN )
		{
			return CP_BOUNDARY;
		}
		else if ( ( classifyEdge == CE_BEGIN ) || ( classifyEdge == CE_END ) )
		{
			return CP_VERTEX;
		}

		if ( ( currentPointIterator1->y <= v.y ) && ( v.y < currentPointIterator0->y ) &&
				 ( ( ( v.x - currentPointIterator1->x ) * ( currentPointIterator0->y - currentPointIterator1->y )  ) <
					 ( ( v.y - currentPointIterator1->y ) * ( currentPointIterator0->x - currentPointIterator1->x ) ) ) )
		{
			classifyPolygon = GetNegativeClassifyPolygon( classifyPolygon );
		}
		else if ( ( currentPointIterator0->y <= v.y ) && ( v.y < currentPointIterator1->y ) && 
							( ( ( v.x - currentPointIterator1->x ) * ( currentPointIterator0->y - currentPointIterator1->y ) ) >
								( ( v.y - currentPointIterator1->y ) * ( currentPointIterator0->x - currentPointIterator1->x ) ) ) )
		{
			classifyPolygon = GetNegativeClassifyPolygon( classifyPolygon );
		}

		++currentPointIterator0;
		++currentPointIterator1;
		if ( currentPointIterator1 == rPolygon.end() )
		{
			currentPointIterator1 = rPolygon.begin();
		}
	}
	return classifyPolygon;
}

template<class Type>
float GetPolygonPerimeter( const Type &rPolygon )
{
	if ( rPolygon.empty() )
	{
		return 0.0f;
	}

	Type::const_iterator currentPointIterator0 = rPolygon.begin();
	Type::const_iterator currentPointIterator1 = rPolygon.begin();
	++currentPointIterator1;
	if ( currentPointIterator1 == rPolygon.end() )
	{
		return 0.0f;
	}

	float fPerimeter = 0;
	while ( currentPointIterator0 != rPolygon.end() )
	{
		fPerimeter += fabs( ( *currentPointIterator1 ) - ( *currentPointIterator0 ) );

		++currentPointIterator0;
		++currentPointIterator1;
		if ( currentPointIterator1 == rPolygon.end() )
		{
			currentPointIterator1 = rPolygon.begin();
		}
	}
	return fPerimeter;
}

template<class Type>
float GetSignedPolygonSquare( const Type &rPolygon )
{
	if ( rPolygon.empty() )
	{
		return 0.0f;
	}

	Type::const_iterator currentPointIterator0 = rPolygon.begin();
	Type::const_iterator currentPointIterator1 = rPolygon.begin();
	Type::const_iterator currentPointIterator2 = rPolygon.begin();
	++currentPointIterator1;
	++currentPointIterator2;
	if ( currentPointIterator1 == rPolygon.end() )
	{
		return 0.0f;
	}
	++currentPointIterator2;
	if ( currentPointIterator2 == rPolygon.end() )
	{
		return 0.0f;
	}

	float fSquare = 0;
	while ( currentPointIterator0 != rPolygon.end() )
	{
		fSquare += ( currentPointIterator1->x * ( currentPointIterator2->y - currentPointIterator0->y ) );

		++currentPointIterator0;
		++currentPointIterator1;
		++currentPointIterator2;
		if ( currentPointIterator1 == rPolygon.end() )
		{
			currentPointIterator1 = rPolygon.begin();
		}
		if ( currentPointIterator2 == rPolygon.end() )
		{
			currentPointIterator2 = rPolygon.begin();
		}
	}
	return ( fSquare / 2.0f );
}

template<class PointType>
EClassifyRotation ClassifyRotation( const PointType &v0, const PointType &v1, const PointType &v2 )
{
	const EClassifyEdge classifyEdge = ClassifyEdge( v0, v1, v2 );
	if ( classifyEdge == CE_RIGHT )
	{
		return CR_CLOCKWISE;
	}
	if ( classifyEdge == CE_LEFT )
	{
		return CR_COUNTERCLOCKWISE;
	}
	return CR_LINE;
}

template<class Type>
EClassifyRotation ClassifyRotation( const Type &rPolygon )
{
	const float fPolygonSquare = GetSignedPolygonSquare( rPolygon );  
	if ( fPolygonSquare < 0  )
	{
		return CR_CLOCKWISE;
	}
	else if ( fPolygonSquare > 0 )
	{
		return CR_COUNTERCLOCKWISE;
	}
	return CR_LINE;
}

template<class PointType>
float GetSignedAngle( const PointType &rvBegin, const PointType &rvEnd, const PointType &v )
{
	const PointType vBeginV = rvBegin - v;
	const PointType vEndV = rvEnd - v;
	const float fvBeginVAngle = GetPolarAngle( vBeginV );
	const float fvEndVAngle = GetPolarAngle( vEndV );
	if ( fvBeginVAngle == ( -1.0f ) || ( fvEndVAngle == ( -1.0f ) ) )
	{
		return FP_PI;
	}
	else
	{
		const float fEngle = fvEndVAngle - fvBeginVAngle;
		if ( ( fEngle == FP_PI ) || ( fEngle == ( -FP_PI ) ) )
		{
			return FP_PI;
		}
		else if ( fEngle < ( -FP_PI ) )
		{
			return ( fEngle + FP_2PI ); 
		}
		else if ( fEngle > FP_PI )
		{
			return FP_2PI - fEngle;
		}
		return fEngle;
	}

}

template<class Type>
Type GetPointOnEdge( const Type &rvBegin, const Type &rvEnd, float fPoint )
{
	return ( rvBegin + fPoint * ( rvEnd - rvBegin ) );
}


inline CVec2 CreateFromPolarCoord( float r, float a ) { return CVec2( r * cos( a ), r * sin( a ) ); }
inline CVec3 CreateFromPolarCoord( float r, float a, float fZ ) { return CVec3( r * cos( a ), r * sin( a ), fZ ); }
template<class PointType>
inline void RotatePoint( PointType *pPoint, float a ) { PointType point = PointType( ( pPoint->x * cos( a ) ) - ( pPoint->y * sin( a ) ), ( pPoint->x * sin( a ) ) + ( pPoint->y * cos( a ) ) ); ( *pPoint ) = point; }

template<class PointType>
float GetPolarAngle( const PointType &v )
{
	if ( ( v.x == 0.0f ) && ( v.y == 0.0f ) )
	{
		return -1.0f;
	}
	else if ( v.x == 0.0f )
	{
		return ( ( v.y > 0.0f ) ? ( FP_PI2 ) : ( FP_PI + FP_PI2 ) );
	}
	else
	{
		float a = atan( v.y / v.x );
		if ( v.x > 0.0f )
		{
			return ( ( v.y >= 0.0f ) ? ( a ) : ( FP_2PI + a ) );
		}
		else
		{
			return ( FP_PI + a );
		}
	}
}

template<class PointType>
float GetPolarLength( const PointType &v )
{
	return fabs( v.x, v.y );
}

template<class PointType>
PointType GetNormal( const PointType &v )
{
	PointType vNormal = v;
	vNormal.x = -v.y;
	vNormal.y = v.x;
	return vNormal;
}

template<class PointType>
void RotateEdgeToPI2( PointType *pvBegin, PointType *pvEnd )
{
	NI_ASSERT_T( ( pvBegin != 0 ) && ( pvEnd != 0 ),
							 NStr::Format( "Wrong parameters: pvBegin %x, pvEnd %x\n", pvBegin, pvEnd ) );

	const PointType m = 0.5f * ( ( *pvBegin ) + ( *pvEnd ) );
	const PointType v = ( *pvEnd ) - ( *pvBegin );
	PointType vNormal = GetNormal( v );
	( *pvBegin ) = m - 0.5f * vNormal;
	( *pvEnd ) = m + 0.5f * vNormal;
}

template<class PointType>
void FlipEdgeToPI( PointType *pvBegin, PointType *pvEnd )
{
	NI_ASSERT_T( ( pvBegin != 0 ) && ( pvEnd != 0 ),
							 NStr::Format( "Wrong parameters: pvBegin %x, pvEnd %x\n", pvBegin, pvEnd ) );

	const PointType vTemp = ( *pvEnd );
	( *pvEnd ) = ( *pvBegin );
	( *pvBegin ) = vTemp;
}

template<class Type, class PointType>
bool SplitByEdge( const Type &rSourcePolygon, const PointType &rvBegin, const PointType &rvEnd, Type *pLeftPolygon, Type *pRightPolygon )
{
	if ( pLeftPolygon )
	{
		pLeftPolygon->clear();
	}
	if( pRightPolygon )
	{
		pRightPolygon->clear();
	}

	if ( rSourcePolygon.empty() )
	{
		return true;
	}

	Type::const_iterator sourcePointIterator0 = rSourcePolygon.begin();
	Type::const_iterator sourcePointIterator1 = rSourcePolygon.begin();

	++sourcePointIterator1;
	if ( sourcePointIterator1 == rSourcePolygon.end() )
	{
		EClassifyEdge classifyEdge = ClassifyEdge( rvBegin, rvEnd, ( *sourcePointIterator0 ) );
		if ( classifyEdge != CE_LEFT )
		{
			if ( pRightPolygon )
			{
				pRightPolygon->push_back( ( *sourcePointIterator0 ) );
			}
		}
		if ( classifyEdge != CE_RIGHT )
		{
			if ( pLeftPolygon )
			{
				pLeftPolygon->push_back( ( *sourcePointIterator0 ) );
			}
		}
		return true;
	}
	
	while ( sourcePointIterator0 != rSourcePolygon.end() )
	{
		const EClassifyEdge beginClassifyEdge = ClassifyEdge( rvBegin, rvEnd, ( *sourcePointIterator0 ) );
		const EClassifyEdge endClassifyEdge = ClassifyEdge( rvBegin, rvEnd, ( *sourcePointIterator1 ) );

		float fIntersectPoint = 0.0f;
		const EClassifyIntersection classifyIntersect = ClassifyIntersect( rvBegin, rvEnd, ( *sourcePointIterator0 ), ( *sourcePointIterator1 ), &fIntersectPoint );

		PointType vIntersectionPoint;
		if ( classifyIntersect == CI_SKEW )
		{
			vIntersectionPoint = GetPointOnEdge( rvBegin, rvEnd, fIntersectPoint );
		}

		if ( beginClassifyEdge == CE_RIGHT )
		{
			if ( endClassifyEdge != CE_RIGHT )
			{
				NI_ASSERT_TF( classifyIntersect == CI_SKEW,
											NStr::Format( "Invalid Intersection: ( %g, %g )--( %g, %g ) with ( %g, %g )--( %g, %g )",
																		rvBegin.x, rvBegin.y, rvEnd.x, rvEnd.y,
																		sourcePointIterator0->x, sourcePointIterator0->x, sourcePointIterator1->x, sourcePointIterator1->y ),
											return false );
				if ( pRightPolygon )
				{
					pRightPolygon->push_back( vIntersectionPoint );
				}
				if ( pLeftPolygon )
				{
					if ( endClassifyEdge == CE_LEFT ) 
					{
						pLeftPolygon->push_back( vIntersectionPoint );
					}
					pLeftPolygon->push_back( *sourcePointIterator1 );
				}
			}
			else
			{
				if ( pRightPolygon )
				{
					pRightPolygon->push_back( *sourcePointIterator1 );
				}
			}
		}
		else if ( beginClassifyEdge == CE_LEFT )
		{
			if ( endClassifyEdge != CE_LEFT )
			{
				NI_ASSERT_TF( classifyIntersect == CI_SKEW,
											NStr::Format( "Invalid Intersection: ( %g, %g )--( %g, %g ) with ( %g, %g )--( %g, %g )",
																		rvBegin.x, rvBegin.y, rvEnd.x, rvEnd.y,
																		sourcePointIterator0->x, sourcePointIterator0->x, sourcePointIterator1->x, sourcePointIterator1->y ),
											return false );
				if ( pRightPolygon )
				{
					if ( endClassifyEdge == CE_RIGHT ) 
					{
						pRightPolygon->push_back( vIntersectionPoint );
					}
					pRightPolygon->push_back( *sourcePointIterator1 );

				}
				if ( pLeftPolygon )
				{
					pLeftPolygon->push_back( vIntersectionPoint );
				}
			}
			else
			{
				if ( pLeftPolygon )
				{
					pLeftPolygon->push_back( *sourcePointIterator1 );
				}
			}
		}
		else
		{
			if ( endClassifyEdge == CE_RIGHT )
			{
				if ( pRightPolygon )
				{
					pRightPolygon->push_back( *sourcePointIterator1 );
				}
			}
			else if ( endClassifyEdge == CE_LEFT )
			{
				if ( pLeftPolygon )
				{
					pLeftPolygon->push_back( *sourcePointIterator1 );
				}
			}
			else
			{
				if ( pRightPolygon )
				{
					pRightPolygon->push_back( *sourcePointIterator1 );
				}
				if ( pLeftPolygon )
				{
					pLeftPolygon->push_back( *sourcePointIterator1 );
				}
			}
		}

		++sourcePointIterator0;
		++sourcePointIterator1;
		if ( sourcePointIterator1 == rSourcePolygon.end() )
		{
			sourcePointIterator1 = rSourcePolygon.begin();
		}
	}
	return true;
}

template<class Type, class PointType>
bool CutByPolygonCore( const Type &rPolygon, const Type &rPolygonCore, Type *pCutPolygon )
{
	NI_ASSERT_TF( pCutPolygon != 0,
							  NStr::Format( "CutByPolygonCore() Wrong parameter: pCutPolygon %x\n", pCutPolygon ),
								return false );

	if ( rPolygonCore.empty() )
	{
		( *pCutPolygon ) = rPolygon;
		return true;
	}
	Type::const_iterator currentPointIterator0 = rPolygonCore.begin();
	Type::const_iterator currentPointIterator1 = rPolygonCore.begin();
	++currentPointIterator1;
	if ( currentPointIterator1 == rPolygonCore.end() )
	{
		( *pCutPolygon ) = rPolygon;
		return true;
	}
	EClassifyRotation classifyRotation = ClassifyRotation( rPolygonCore );
	if ( classifyRotation == CR_LINE )
	{
		( *pCutPolygon ) = rPolygon;
		return true;
	}
	Type polygon0;
	Type polygon1;
	polygon0.insert( polygon0.end(), rPolygon.begin(), rPolygon.end() );

	int nCount = 0;
	while ( currentPointIterator0 != rPolygonCore.end() )
	{
		PointType vBegin = ( *currentPointIterator0 );
		PointType vEnd = ( *currentPointIterator1 );
		if ( ( nCount & 0x01 ) > 0 )
		{
			if ( classifyRotation == CR_CLOCKWISE )
			{
				SplitByEdge( polygon1, vBegin, vEnd, static_cast<Type*>( 0 ), &polygon0 );
			}
			else
			{
				SplitByEdge( polygon1, vBegin, vEnd, &polygon0, static_cast<Type*>( 0 ) );
			}
			polygon1.clear();
		}
		else
		{
			if ( classifyRotation == CR_CLOCKWISE )
			{
				SplitByEdge( polygon0, vBegin, vEnd, static_cast<Type*>( 0 ), &polygon1 );
			}
			else
			{
				SplitByEdge( polygon0, vBegin, vEnd, &polygon1, static_cast<Type*>( 0 ) );
			}
			polygon0.clear();
		}
		++nCount;
		
		++currentPointIterator0;
		++currentPointIterator1;
		if ( currentPointIterator1 == rPolygonCore.end() )
		{
			currentPointIterator1 = rPolygonCore.begin();
		}
	}

	pCutPolygon->clear();
	if ( ( nCount & 0x01 ) > 0 )
	{
		pCutPolygon->insert( pCutPolygon->end(), polygon1.begin(), polygon1.end() );
	}
	else
	{
		pCutPolygon->insert( pCutPolygon->end(), polygon0.begin(), polygon0.end() );
	}

	return true;
}

template<class Type, class PointType>
bool GetVoronoyPolygon( const Type &rBoundingPolygon, const Type &rPoints, const PointType &rPoint, Type *pVoronoyPolygon )
{
	NI_ASSERT_TF( pVoronoyPolygon != 0,
							  NStr::Format( "Wrong parameter: pVoronoyPolygon %x\n", pVoronoyPolygon ),
								return false );

	if ( rBoundingPolygon.empty() )
	{
		return true;
	}

	Type polygon0;
	Type polygon1;
	polygon0.insert( polygon0.end(), rBoundingPolygon.begin(), rBoundingPolygon.end() );

	int nCount = 0;
	
	for ( Type::const_iterator pointIterator = rBoundingPolygon.begin(); pointIterator != rBoundingPolygon.end(); ++pointIterator )
	{
		if ( ( pointIterator->x != rPoint.x ) || ( pointIterator->y != rPoint.y ) )
		{
			PointType vBegin = rPoint;
			PointType vEnd = ( *pointIterator );
			RotateEdgeToPI2( &vBegin, &vEnd );
			
			if ( ( nCount & 0x01 ) > 0 )
			{
				SplitByEdge( polygon1, vBegin, vEnd, &polygon0, static_cast<Type*>( 0 ) );
				polygon1.clear();
			}
			else
			{
				SplitByEdge( polygon0, vBegin, vEnd, &polygon1, static_cast<Type*>( 0 ) );
				polygon0.clear();
			}
			++nCount;
		}
	}
	
	for ( Type::const_iterator pointIterator = rPoints.begin(); pointIterator != rPoints.end(); ++pointIterator )
	{
		if ( ( pointIterator->x != rPoint.x ) || ( pointIterator->y != rPoint.y ) )
		{
			PointType vBegin = rPoint;
			PointType vEnd = ( *pointIterator );
			RotateEdgeToPI2( &vBegin, &vEnd );
			
			if ( ( nCount & 0x01 ) > 0 )
			{
				SplitByEdge( polygon1, vBegin, vEnd, &polygon0, static_cast<Type*>( 0 ) );
				polygon1.clear();
			}
			else
			{
				SplitByEdge( polygon0, vBegin, vEnd, &polygon1, static_cast<Type*>( 0 ) );
				polygon0.clear();
			}
			++nCount;
		}
	}

	pVoronoyPolygon->clear();
	if ( ( nCount & 0x01 ) > 0 )
	{
		pVoronoyPolygon->insert( pVoronoyPolygon->end(), polygon1.begin(), polygon1.end() );
	}
	else
	{
		pVoronoyPolygon->insert( pVoronoyPolygon->end(), polygon0.begin(), polygon0.end() );
	}

	return true;
}

template<class Type, class PointType>
bool GetVoronoyPolygon( const Type &rBoundingPolygon, const PointType &rPoint, Type *pVoronoyPolygon )
{
	const Type points;
	return GetVoronoyPolygon( rBoundingPolygon, points, rPoint, pVoronoyPolygon );
}

template<class Type, class PointType>
void UniquePolygon( Type *pPolygon, float fRange )
{
	NI_ASSERT_T( pPolygon != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPolygon ) );

	pPolygon->erase( std::unique( pPolygon->begin(),
																pPolygon->end(),
																SInRangeFunctional<PointType>( fRange ) ),
									 pPolygon->end() );
}

template<class Type>
void GetPolygonBoundingBox( const Type &rPolygon, CTRect<float> *pBoundingBox )
{
	NI_ASSERT_T( pBoundingBox != 0,
							 NStr::Format( "Wrong parameter: %x\n", pBoundingBox ) );

	pBoundingBox->Set( 0.0f, 0.0f, 0.0f, 0.0f );
	if ( !rPolygon.empty() )
	{
		Type::const_iterator pointIterator = rPolygon.begin();
		pBoundingBox->Set( pointIterator->x, pointIterator->y, pointIterator->x, pointIterator->y );
		for ( ++pointIterator; pointIterator != rPolygon.end(); ++pointIterator )
		{
			if ( pointIterator->x < pBoundingBox->minx )
			{
				pBoundingBox->minx = pointIterator->x;
			}
			if ( pointIterator->x > pBoundingBox->maxx )
			{
				pBoundingBox->maxx = pointIterator->x;
			}
			if ( pointIterator->y < pBoundingBox->miny )
			{
				pBoundingBox->miny = pointIterator->y;
			}
			if ( pointIterator->y > pBoundingBox->maxy )
			{
				pBoundingBox->maxy = pointIterator->y;
			}
		}
	}
}

template<class Type>
inline CTRect<float> GetPolygonBoundingBox( const Type &rPolygon )
{
	CTRect<float> boundingBox( 0.0f, 0.0f, 0.0f, 0.0f );
	GetPolygonBoundingBox( rPolygon, &boundingBox );
	return boundingBox;
}

template<class PointType>
PointType GetRandomBetweenPoint( const PointType &rvBegin, const PointType &rvEnd, float fMinSideDistanceRatio, const CTPoint<float> &rShiftRatio, EClassifyEdge classifyEdge )
{
	NI_ASSERT_T( ( fMinSideDistanceRatio >= 0.0f ) && ( fMinSideDistanceRatio <= 0.5f ),
							 NStr::Format( "Invalid fMinSideDistanceRatio %g [0, 0.5]", fMinSideDistanceRatio ) );
	NI_ASSERT_T( rShiftRatio.min <= rShiftRatio.max,
							 NStr::Format( "Invalid fShiftRatio %g < %g", rShiftRatio.min, rShiftRatio.max ) );
	
	const PointType vEdge = rvEnd - rvBegin;
	
	PointType vNormal = GetNormal( vEdge );
	Normalize( &vNormal );
	
	const float fPoint = Random( fMinSideDistanceRatio, 1.0f - fMinSideDistanceRatio );
	PointType vPoint = GetPointOnEdge( rvBegin, rvEnd, fPoint );

	const float fShift = fabs( rvBegin - rvEnd ) * Random( rShiftRatio.min, rShiftRatio.max );
	if ( classifyEdge == CE_LEFT )
	{
		vPoint += vNormal * fShift;
	}
	else if ( classifyEdge == CE_RIGHT )
	{
		vPoint -= vNormal * fShift;
	}
	
	return vPoint;
}

template<class Type, class PointType>
bool RandomizeEdges( const Type &rSourceSequence, int nDepth, float fMinSideDistanceRatio, const CTPoint<float> &rShiftRatio, Type *pRandomizedSequence, float fMinEdgeLength, float fMaxEdgeLength, bool bPolygon )
{
	NI_ASSERT_TF( pRandomizedSequence != 0,
							  NStr::Format( "Wrong parameter: pRandomizedSequence %x\n", pRandomizedSequence ),
								return false );

	if ( rSourceSequence.empty() )
	{
		return true;
	}
	else
	{
		Type::const_iterator testSourcePointIterator = rSourceSequence.begin();
		++testSourcePointIterator;
		if ( testSourcePointIterator == rSourceSequence.end() )
		{
			return true;
		}
	}

	const float fMinEdgeLength2 = fabs2( fMinEdgeLength );
	const float fMaxEdgeLength2 = fabs2( fMaxEdgeLength );
	CTPoint<float> onLineShiftRatio( 0.0f, 0.0f );
	Type sequence0;
	Type sequence1;
	sequence0.insert( sequence0.end(), rSourceSequence.begin(), rSourceSequence.end() );
	
	int nCount = 0;
	while ( nCount < nDepth )
	{
		bool bNewPointsNotInserted = true;
		if ( ( nCount & 0x01 ) > 0 )
		{
			sequence0.clear();
			Type::const_iterator sourcePointIterator0 = sequence1.begin();
			Type::const_iterator sourcePointIterator1 = sequence1.begin();
			++sourcePointIterator1;
			EClassifyEdge classifyEdge = ( Random( 2 ) > 0 ) ? CE_LEFT : CE_RIGHT;
			while ( sourcePointIterator0 != sequence1.end() )
			{
				sequence0.push_back( ( *sourcePointIterator0 ) );
				const float fEdgeLength2 = fabs2( ( *sourcePointIterator1 ) - ( *sourcePointIterator0 ) );
				if ( fEdgeLength2 > fMaxEdgeLength2 )
				{
					sequence0.push_back( GetRandomBetweenPoint( ( *sourcePointIterator0 ), ( *sourcePointIterator1 ), fMinSideDistanceRatio, onLineShiftRatio, classifyEdge ) );
					classifyEdge = GetNegativeClassifyEdge( classifyEdge );
					bNewPointsNotInserted = false;
				}
				else if ( fEdgeLength2 > fMinEdgeLength2 )
				{
					sequence0.push_back( GetRandomBetweenPoint( ( *sourcePointIterator0 ), ( *sourcePointIterator1 ), fMinSideDistanceRatio, rShiftRatio, classifyEdge ) );
					classifyEdge = GetNegativeClassifyEdge( classifyEdge );
					bNewPointsNotInserted = false;
				}					
				++sourcePointIterator0;
				++sourcePointIterator1;
				
				if ( sourcePointIterator1 == sequence1.end() )
				{
					if ( bPolygon )
					{
						sourcePointIterator1 = sequence1.begin();
					}
					else
					{
						sequence0.push_back( ( *sourcePointIterator0 ) );
						break;
					}
				}
			}
		}
		else
		{
			sequence1.clear();
			Type::const_iterator sourcePointIterator0 = sequence0.begin();
			Type::const_iterator sourcePointIterator1 = sequence0.begin();
			++sourcePointIterator1;
			EClassifyEdge classifyEdge = ( Random( 2 ) > 0 ) ? CE_LEFT : CE_RIGHT;
			while ( sourcePointIterator0 != sequence0.end() )
			{
				sequence1.push_back( ( *sourcePointIterator0 ) );
				const float fEdgeLength2 = fabs2( ( *sourcePointIterator1 ) - ( *sourcePointIterator0 ) );
				if ( fEdgeLength2 > fMaxEdgeLength2 )
				{
					sequence1.push_back( GetRandomBetweenPoint( ( *sourcePointIterator0 ), ( *sourcePointIterator1 ), fMinSideDistanceRatio, onLineShiftRatio, classifyEdge ) );
					classifyEdge = GetNegativeClassifyEdge( classifyEdge );
					bNewPointsNotInserted = false;
				}
				else  if ( fEdgeLength2 > fMinEdgeLength2 )
				{
					sequence1.push_back( GetRandomBetweenPoint( ( *sourcePointIterator0 ), ( *sourcePointIterator1 ), fMinSideDistanceRatio, rShiftRatio, classifyEdge ) );
					classifyEdge = GetNegativeClassifyEdge( classifyEdge );
					bNewPointsNotInserted = false;
				}
				++sourcePointIterator0;
				++sourcePointIterator1;
				
				if ( sourcePointIterator1 == sequence0.end() )
				{
					if ( bPolygon )
					{
						sourcePointIterator1 = sequence0.begin();
					}
					else
					{
						sequence1.push_back( ( *sourcePointIterator0 ) );
						break;
					}
				}
			}
		}
		++nCount;
		if	( bNewPointsNotInserted )
		{
			break;
		}
	}

	pRandomizedSequence->clear();
	if ( ( nCount & 0x01 ) > 0 )
	{
		pRandomizedSequence->insert( pRandomizedSequence->end(), sequence1.begin(), sequence1.end() );
	}
	else
	{
		pRandomizedSequence->insert( pRandomizedSequence->end(), sequence0.begin(), sequence0.end() );
	}

	return true;
}

template<class Type, class PointType>
bool EnlargePolygonCore( const Type &rBoundingPolygon, const Type &rPolygon, float fDistance, Type *pEnlargedPolygon )
{
	NI_ASSERT_TF( pEnlargedPolygon != 0,
							  NStr::Format( "Wrong parameter: pEnlargedPolygon %x\n", pEnlargedPolygon ),
								return false );

	Type::const_iterator currentPointIterator0 = rPolygon.begin();
	Type::const_iterator currentPointIterator1 = rPolygon.begin();
	++currentPointIterator1;
	if ( currentPointIterator1 == rPolygon.end() )
	{
		pEnlargedPolygon->clear();
		return false;
	}
	else
	{
		Type::const_iterator currentPointIterator2 = currentPointIterator1;
		++currentPointIterator2;
		if ( currentPointIterator2 == rPolygon.end() )
		{
			pEnlargedPolygon->clear();
			return false;
		}
	}

	EClassifyRotation classifyRotation = ClassifyRotation( rPolygon );
	if ( classifyRotation == CR_LINE )
	{
		pEnlargedPolygon->clear();
		return false;
	}
	else if ( classifyRotation == CR_COUNTERCLOCKWISE )
	{
		fDistance *= ( -1 );
	}

	Type polygon0;
	Type polygon1;
	polygon0.insert( polygon0.end(), rBoundingPolygon.begin(), rBoundingPolygon.end() );

	int nCount = 0;
	while ( currentPointIterator0 != rPolygon.end() )
	{
		PointType vBegin = ( *currentPointIterator0 );
		PointType vEnd = ( *currentPointIterator1 );
		PointType vNormal = GetNormal( vEnd - vBegin );
		Normalize( &vNormal );

		vBegin += vNormal * fDistance;
		vEnd += vNormal * fDistance;

		if ( ( nCount & 0x01 ) > 0 )
		{
			if ( classifyRotation == CR_CLOCKWISE )
			{
				SplitByEdge( polygon1, vBegin, vEnd, static_cast<Type*>( 0 ), &polygon0 );
			}
			else
			{
				SplitByEdge( polygon1, vBegin, vEnd, &polygon0, static_cast<Type*>( 0 ) );
			}
			polygon1.clear();
		}
		else
		{
			if ( classifyRotation == CR_CLOCKWISE )
			{
				SplitByEdge( polygon0, vBegin, vEnd, static_cast<Type*>( 0 ), &polygon1 );
			}
			else
			{
				SplitByEdge( polygon0, vBegin, vEnd, &polygon1, static_cast<Type*>( 0 ) );
			}
			polygon0.clear();
		}
		++nCount;
		
		++currentPointIterator0;
		++currentPointIterator1;
		if ( currentPointIterator1 == rPolygon.end() )
		{
			currentPointIterator1 = rPolygon.begin();
		}
	}

	pEnlargedPolygon->clear();
	if ( ( nCount & 0x01 ) > 0 )
	{
		pEnlargedPolygon->insert( pEnlargedPolygon->end(), polygon1.begin(), polygon1.end() );
	}
	else
	{
		pEnlargedPolygon->insert( pEnlargedPolygon->end(), polygon0.begin(), polygon0.end() );
	}

	return true;
}

template<class Type, class PointType>
float PolygonDistance( const Type &rPolygon, const PointType &v )
{
	if ( rPolygon.empty() )
	{
		return 0.0;
	}

	Type::const_iterator currentPointIterator0 = rPolygon.begin();
	Type::const_iterator currentPointIterator1 = rPolygon.begin();

	++currentPointIterator1;
	if ( currentPointIterator1 == rPolygon.end() )
	{
		return fabs( v - ( *currentPointIterator0 ) );
	}
	
	EClassifyPolygon classifyPolygon = ClassifyPolygon( rPolygon, v );
	
	if ( ( classifyPolygon == CP_BOUNDARY ) || ( classifyPolygon == CP_VERTEX ) )
	{
		return 0.0f;
	}

	float fDistance = fabs( v  - ( *currentPointIterator0 ) );
	while ( currentPointIterator0 != rPolygon.end() )
	{

		const float fVertexDistance = fabs( v - ( *currentPointIterator0 ) );
		if ( fVertexDistance < fDistance )
		{
			fDistance = fVertexDistance;
		}
		
		const float dotPoduct_v0_10 = ( v - ( *currentPointIterator0 ) ) * ( ( *currentPointIterator1 ) - ( *currentPointIterator0 ) );
		const float dotPoduct_v1_01 = ( v - ( *currentPointIterator1 ) ) * ( ( *currentPointIterator0 ) - ( *currentPointIterator1 ) );
		
		if ( ( dotPoduct_v0_10 > FP_EPSILON ) && ( dotPoduct_v1_01 > FP_EPSILON ) )
		{
			const float fEdgeDistance = fVertexDistance * sqrt( 1 - fabs2( dotPoduct_v0_10 / ( fVertexDistance * fabs( ( *currentPointIterator1 ) - ( *currentPointIterator0 ) ) ) ) );	
			if ( fEdgeDistance < fDistance )
			{
				fDistance = fEdgeDistance;
			}
		}
		
		++currentPointIterator0;
		++currentPointIterator1;
		if ( currentPointIterator1 == rPolygon.end() )
		{
			currentPointIterator1 = rPolygon.begin();
		}
	}
	return ( fDistance * ( ( classifyPolygon == CP_INSIDE ) ? ( 1.0f ) : ( -1.0f ) ) );
}

template<class Type>
bool GetBoundingPolygon( const SVectorStripeObject &rVectorStripeObject, Type *pBoundingPolygon )
{
	NI_ASSERT_TF( pBoundingPolygon != 0,
							  NStr::Format( "Wrong parameter: pBoundingPolygon %x\n", pBoundingPolygon ),
								return false );
	pBoundingPolygon->clear();
	for ( std::vector<SVectorStripeObjectPoint>::const_iterator pointIterator = rVectorStripeObject.points.begin(); pointIterator != rVectorStripeObject.points.end(); ++pointIterator )
	{
		if ( pointIterator->bKeyPoint )
		{
			pBoundingPolygon->push_back( pointIterator->vPos + pointIterator->vNorm * pointIterator->fWidth );
		}
	}
	for ( std::vector<SVectorStripeObjectPoint>::const_reverse_iterator pointIterator = rVectorStripeObject.points.rbegin(); pointIterator != rVectorStripeObject.points.rend(); ++pointIterator )
	{
		if ( pointIterator->bKeyPoint )
		{
			pBoundingPolygon->push_back( pointIterator->vPos - pointIterator->vNorm * pointIterator->fWidth );
		}
	}
	return true;
}
#endif // #if !defined(__Polygons__Types__)

/**
bool NPGeometry::CPolygon::Split( int nPointIndex, CPolygon *pNewPolygon )
{
	NI_ASSERT_T( pNewPolygon != 0,
							 NStr::Format( "Invalid parameter %x", pNewPolygon ) );
	
	pNewPolygon->Clear();
	if ( nCurrentPointIndex < nPointIndex )
	{
		pNewPolygon->points.reserve( nPointIndex - nCurrentPointIndex + 1 );
		pNewPolygon->points.insert( pNewPolygon->points.begin(), points.begin() + nCurrentPointIndex, points.begin() + nPointIndex + 1 );
		if ( nCurrentPointIndex < ( nPointIndex + 1 ) )
		{
			points.erase( points.begin() + nCurrentPointIndex + 1, points.begin() + nPointIndex );
		}
	}
	else
	{
		pNewPolygon->points.reserve( ( points.size() - nCurrentPointIndex + 1 ) +  nPointIndex + 1 );
		pNewPolygon->points.insert( pNewPolygon->points.begin(), points.begin() + nCurrentPointIndex, points.end() );
		pNewPolygon->points.insert( pNewPolygon->points.end(), points.begin(), points.begin() + nPointIndex + 1 );

		points.erase( points.begin() + nCurrentPointIndex + 1, points.end() );
		points.erase( points.begin(), points.begin() + nPointIndex );
		nCurrentPointIndex -= nPointIndex;
	}
	pNewPolygon->nCurrentPointIndex = pNewPolygon->points.size() - 1;
	return true;
}
/**/
