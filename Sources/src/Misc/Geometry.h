#ifndef __GEOMETRY_H__
#define __GEOMETRY_H__
#pragma ONCE
#include <math.h>
#include "Tools.h"
#pragma pack( 4 )
class CVec2
{
public:
  union
  {
    float m[2];
    struct { float x, y; };
    struct { float u, v; };             // for texture coord
  };
public:
	CVec2() : x( 0.0f ), y( 0.0f ) {  }
  CVec2( const float _x, const float _y ) : x( _x ), y( _y ) {  }
  void Set( const float _x, const float _y ) { x = _x; y = _y; }
  float& operator[]( int i ) { return m[i]; };
  const float& operator[]( int i ) const { return m[i]; }
	bool operator==( const CVec2 &v ) const { return ( (v.x == x) && (v.y == y) ); }
	bool operator!=( const CVec2 &v ) const { return ( (v.x != x) || (v.y != y) ); }
  void Maximize( const CVec2 &v ) { x = Max( x, v.x ); y = Max( y, v.y ); }
  void Minimize( const CVec2 &v ) { x = Min( x, v.x ); y = Min( y, v.y ); }
  void Negate( const CVec2 &v ) { x = -v.x; y = -v.y; } // this = -v
  void Negate() { x = -x; y = -y; }     // this = -this
  void Add( const CVec2 &v1, const CVec2 &v2 ) { x = v1.x + v2.x; y = v1.y + v2.y; } // this = v1 + v2
  void Sub( const CVec2 &v1, const CVec2 &v2 ) { x = v1.x - v2.x; y = v1.y - v2.y; } // this = v1 - v2
  void Displace( const CVec2 &v1, const CVec2 &v2, const float t ) { x = v1.x + t*v2.x; y = v1.y + t*v2.y; } //  this = v1 + t*v2
  void Displace( const CVec2 &v, const float t ) { x += t*v.x; y += t*v.y; } //  this += t*v;
	void Lerp( const float t, const CVec2 &v1, const CVec2 &v2 ) { x = t*v2.x + (1 - t)*v1.x; y = t*v2.y + (1 - t)*v1.y; } //this = (1 - t)*v1 + t*v2
  void Interpolate( const CVec2 &v1, const CVec2 &v2, const float t ) { Lerp(t, v1, v2); }
	CVec2& CProduct( const CVec2& v ) {  const float oldX = x; x = oldX * v.x - y * v.y; y = oldX * v.y + y * v.x; return *this; }
	
	CVec2& operator^=( const CVec2& v ) {  const float oldX = x; x = oldX * v.x - y * v.y; y = oldX * v.y + y * v.x; return *this; }
  CVec2& operator+=( const CVec2 &v ) { x += v.x; y += v.y; return *this; }
  CVec2& operator-=( const CVec2 &v ) { x -= v.x; y -= v.y; return *this; }
  CVec2& operator*=( const float d ) { x *= d; y *= d; return *this; }
  CVec2& operator/=( const float d ) { float d1 = 1.0f / d; x *= d1; y *= d1; return *this; }
};
const CVec2 VNULL2 = CVec2( 0, 0 );
const CVec2 V2_AXIS_X = CVec2( 1, 0 );
const CVec2 V2_AXIS_Y = CVec2( 0, 1 );
inline const CVec2 operator-( const CVec2 &a) { return CVec2(-a.x, -a.y); }
inline const CVec2 operator+( const CVec2 &a, const CVec2 &b ) { return CVec2( a.x + b.x, a.y + b.y ); }
inline const CVec2 operator-( const CVec2 &a, const CVec2 &b ) { return CVec2( a.x - b.x, a.y - b.y ); }
inline float operator*( const CVec2 &a, const CVec2 &b ) { return ( a.x*b.x + a.y*b.y ); }
inline const CVec2 operator*( const CVec2 &a, const float b ) { return CVec2( a.x*b, a.y*b ); }
inline const CVec2 operator*( const float a, const CVec2 &b ) { return CVec2( b.x*a, b.y*a ); }
inline const CVec2 operator/( const CVec2 &a, const float b ) { float b1 = 1.0f/b; return CVec2( a.x*b1, a.y*b1 ); }
inline float fabs2( const CVec2 &a ) { return fabs2( a.x, a.y ); }
inline float fabs( const CVec2 &a ) { return fabs( a.x, a.y ); }
inline bool Normalize( CVec2 *pVec ) { return Normalize(pVec->x, pVec->y); }
inline const CVec2 operator^( const CVec2 &a, const CVec2 &b ) { CVec2 vRes( a ); vRes ^= b; return vRes; }
inline const CVec2 CProduct( const CVec2 &a, const CVec2 &b ) { CVec2 vRes( a ); vRes.CProduct( b ); return vRes; }
class CVec3
{
public:
  union
  {
    float m[3];
    struct { float x, y, z; };
    struct { float r, g, b; };          // for color components
    struct { float u, v, q; };          // for texture coord
  };
public:
	CVec3() : x( 0.0f ), y( 0.0f ), z( 0.0f ) {  }
  CVec3( const float _x, const float _y, const float _z = 0.0f ) : x( _x ), y( _y ), z( _z ) {  }
  CVec3( const CVec2 &v2, const float _z ) : x( v2.x ), y( v2.y ), z( _z ) {  }
  CVec3& operator=( const CVec2 &v ) { x = v.x; y = v.y; z = 0.0f; return *this; }
  void Set( const float _x, const float _y, const float _z ) { x = _x; y = _y; z = _z; }
  float& operator[]( int i ) { return m[i]; };
  const float& operator[]( int i ) const { return m[i]; }
	bool operator==( const CVec3 &v ) const { return ( (v.x == x) && (v.y == y) && (v.z == z) ); }
	bool operator!=( const CVec3 &v ) const { return ( (v.x != x) || (v.y != y) || (v.z != z) ); }
  void Maximize( const CVec3 &v ) { x = Max( x, v.x ); y = Max( y, v.y ); z = Max( z, v.z ); }
  void Minimize( const CVec3 &v ) { x = Min( x, v.x ); y = Min( y, v.y ); z = Min( z, v.z ); }
  void Negate( const CVec3 &v ) { x = -v.x; y = -v.y; z = -v.z; } // this = -v
  void Negate() { x = -x; y = -y; z = -z; }     // this = -this
  void Add( const CVec3 &v1, const CVec3 &v2 ) { x = v1.x + v2.x; y = v1.y + v2.y; z = v1.z + v2.z; } // this = v1 + v2
  void Sub( const CVec3 &v1, const CVec3 &v2 ) { x = v1.x - v2.x; y = v1.y - v2.y; z = v1.z - v2.z; } // this = v1 - v2
  void Displace( const CVec3 &v1, const CVec3 &v2, const float t ) { x = v1.x + t*v2.x; y = v1.y + t*v2.y; z = v1.z + t*v2.z; } //  this = v1 + t*v2
  void Displace( const CVec3 &v, const float t ) { x += t*v.x; y += t*v.y; z += t*v.z; } //  this += t*v;
  void Lerp( const float t, const CVec3 &v1, const CVec3 &v2 ) { x = t*v2.x + (1 - t)*v1.x; y = t*v2.y + (1 - t)*v1.y; z = t*v2.z + (1 - t)*v1.z; } //this = (1 - t)*v1 + t*v2
  void Interpolate( const CVec3 &v1, const CVec3 &v2, const float t ) { Lerp(t, v1, v2); }
  CVec3& operator+=( const CVec3 &v ) { x += v.x; y += v.y; z += v.z; return *this; }
  CVec3& operator-=( const CVec3 &v ) { x -= v.x; y -= v.y; z -= v.z; return *this; }
  CVec3& operator*=( const float d ) { x *= d; y *= d; z *= d; return *this; }
  CVec3& operator/=( const float d ) { float d1 = 1.0f / d; x *= d1; y *= d1; z *= d1; return *this; }
};

const CVec3 VNULL3 = CVec3( 0, 0, 0 );
const CVec3 V3_AXIS_X = CVec3( 1, 0, 0 );
const CVec3 V3_AXIS_Y = CVec3( 0, 1, 0 );
const CVec3 V3_AXIS_Z = CVec3( 0, 0, 1 );
inline const CVec3 operator-( const CVec3 &a) { return CVec3(-a.x, -a.y, -a.z); }
inline const CVec3 operator+( const CVec3 &a, const CVec3 &b ) { return CVec3( a.x + b.x, a.y + b.y, a.z + b.z ); }
inline const CVec3 operator-( const CVec3 &a, const CVec3 &b ) { return CVec3( a.x - b.x, a.y - b.y, a.z - b.z ); }
inline float operator*( const CVec3 &a, const CVec3 &b ) { return ( a.x*b.x + a.y*b.y + a.z*b.z ); }
inline const CVec3 operator*( const CVec3 &a, const float b ) { return CVec3( a.x*b, a.y*b, a.z*b ); }
inline const CVec3 operator*( const float a, const CVec3 &b ) { return CVec3( b.x*a, b.y*a, b.z*a ); }
inline const CVec3 operator/( const CVec3 &a, const float b ) { float b1 = 1.0f/b; return CVec3( a.x*b1, a.y*b1, a.z*b1 ); }
inline const CVec3 operator^( const CVec3 &a, const CVec3 &b ) { return CVec3( a.y*b.z - b.y*a.z, a.z*b.x - b.z*a.x, a.x*b.y - b.x*a.y ); }
inline float fabs2( const CVec3 &a ) { return fabs2( a.x, a.y, a.z ); }
inline float fabs( const CVec3 &a ) { return fabs( a.x, a.y, a.z ); }
inline float fabsxy2( const CVec3 &a ) { return fabs2( a.x, a.y ); }
inline float fabsxy( const CVec3 &a ) { return fabs( a.x, a.y ); }
inline bool Normalize( CVec3 *pVec ) { return Normalize(pVec->x, pVec->y, pVec->z); }

// float -> signed char is UB for NaN and out-of-range values (UBSan traps);
// MSVC's _ftol produced 0x80000000 for those, i.e. a low byte of 0 — keep that.
inline BYTE floatToByte( const float fNumber )
{
	const float fScaled = fNumber * 127.0f;
	const int nScaled = ( fScaled >= -2147483648.0f && fScaled < 2147483648.0f ) ? static_cast<int>( fScaled ) : ( -2147483647 - 1 );
	return static_cast<BYTE>( static_cast<signed char>( nScaled ) );
}
inline float byteToFloat( const BYTE cNumber ) { return float( char( cNumber ) ) / 127.0f; }
inline DWORD Vec3ToDWORD( const CVec3 &v ) { return DWORD( floatToByte( v.x ) ) | ( DWORD( floatToByte( v.y ) ) << 8 ) | ( DWORD( floatToByte( v.z ) ) << 16 ); }
inline const CVec3 DWORDToVec3( DWORD dwVector ) { return CVec3( byteToFloat( dwVector & 0xff ), byteToFloat( (dwVector >> 8) & 0xff ), byteToFloat( (dwVector >> 16) & 0xff ) ); }
class CVec4
{
public:
  union
  {
    float m[4];
    struct { float x, y, z, w; };
    struct { float a, r, g, b; };				// for color components
    struct { float u, v, q; };					// for texture coord; w is shared with the vector view
  };
public:
	CVec4() : x( 0.0f ), y( 0.0f ), z( 0.0f ), w( 0.0f ) {  }
  CVec4( const float _x, const float _y, const float _z = 0.0f, const float _w = 0.0f ) : x( _x ), y( _y ), z( _z ), w( _w ) {  }
  CVec4( const CVec2 &v2, const float _z = 0.0f, const float _w = 0.0f ) : x( v2.x ), y( v2.y ), z( _z ), w( _w ) {  }
  CVec4( const CVec3 &v3, const float _w = 0.0f ) : x( v3.x ), y( v3.y ), z( v3.z ), w( _w ) {  }
  CVec4& operator=( const CVec2 &v ) { x = v.x; y = v.y; z = 0; w = 1; return *this; }
  CVec4& operator=( const CVec3 &v ) { x = v.x; y = v.y; z = v.z; w = 1; return *this; }
  void Set( const float _x, const float _y, const float _z, const float _w ) { x = _x; y = _y; z = _z; w = _w; }
  float& operator[]( int i ) { return m[i]; };
  const float& operator[]( int i ) const { return m[i]; }
	bool operator==( const CVec4 &v ) const { return ( (v.x == x) && (v.y == y) && (v.z == z) && (v.w == w) ); }
	bool operator!=( const CVec4 &v ) const { return ( (v.x != x) || (v.y != y) || (v.z != z) || (v.w != w) ); }
  void Maximize( const CVec4 &v ) { x = Max( x, v.x ); y = Max( y, v.y ); z = Max( z, v.z ); w = Max( w, v.w ); }
  void Minimize( const CVec4 &v ) { x = Min( x, v.x ); y = Min( y, v.y ); z = Min( z, v.z ); w = Min( w, v.w ); }
  void Negate( const CVec4 &v ) { x = -v.x; y = -v.y; z = -v.z; w = -v.w; } // this = -v
  void Negate() { x = -x; y = -y; z = -z; w = -w; }     // this = -this
  void Add( const CVec4 &v1, const CVec4 &v2 ) { x = v1.x + v2.x; y = v1.y + v2.y; z = v1.z + v2.z; w = v1.w + v2.w; } // this = v1 + v2
  void Sub( const CVec4 &v1, const CVec4 &v2 ) { x = v1.x - v2.x; y = v1.y - v2.y; z = v1.z - v2.z; w = v1.w - v2.w; } // this = v1 - v2
  void Displace( const CVec4 &v1, const CVec4 &v2, const float t ) { x = v1.x + t*v2.x; y = v1.y + t*v2.y; z = v1.z + t*v2.z; w = v1.w + t*v2.w; } //  this = v1 + t*v2
  void Displace( const CVec4 &v, const float t ) { x += t*v.x; y += t*v.y; z += t*v.z; w += t*v.w; } //  this += t*v;
  void Lerp( const float t, const CVec4 &v1, const CVec4 &v2 ) { x = t*v2.x + (1 - t)*v1.x; y = t*v2.y + (1 - t)*v1.y; z = t*v2.z + (1 - t)*v1.z; w = t*v2.w + (1 - t)*v1.w; } //this = (1 - t)*v1 + t*v2
  void Interpolate( const CVec4 &v1, const CVec4 &v2, const float t ) { Lerp(t, v1, v2); }
  CVec4& operator+=( const CVec4 &v ) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
  CVec4& operator-=( const CVec4 &v ) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
  CVec4& operator*=( const float d ) { x *= d; y *= d; z *= d; w *= d; return *this; }
  CVec4& operator/=( const float d ) { float d1 = 1.0f / d; x *= d1; y *= d1; z *= d1; w *= d1; return *this; }
};
const CVec4 VNULL4 = CVec4( 0, 0, 0, 0 );
const CVec4 V4_AXIS_X = CVec4( 1, 0, 0, 0 );
const CVec4 V4_AXIS_Y = CVec4( 0, 1, 0, 0 );
const CVec4 V4_AXIS_Z = CVec4( 0, 0, 1, 0 );
const CVec4 V4_AXIS_W = CVec4( 0, 0, 0, 1 );
inline const CVec4 operator-( const CVec4 &a) { return CVec4(-a.x, -a.y, -a.z, -a.w); }
inline const CVec4 operator+( const CVec4 &a, const CVec4 &b ) { return CVec4( a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w ); }
inline const CVec4 operator-( const CVec4 &a, const CVec4 &b ) { return CVec4( a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w ); }
inline float operator*( const CVec4 &a, const CVec4 &b ) { return ( a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w ); }
inline const CVec4 operator*( const CVec4 &a, const float b ) { return CVec4( a.x*b, a.y*b, a.z*b, a.w*b ); }
inline const CVec4 operator*( const float a, const CVec4 &b ) { return CVec4( b.x*a, b.y*a, b.z*a, b.w*a ); }
inline const CVec4 operator/( const CVec4 &a, const float b ) { float b1 = 1.0f/b; return CVec4( a.x*b1, a.y*b1, a.z*b1, a.w*b1 ); }
inline float fabs2( const CVec4 &a ) { return fabs2( a.x, a.y, a.z, a.w ); }
inline float fabs( const CVec4 &a ) { return fabs( a.x, a.y, a.z, a.w ); }
inline float fabsxyz2( const CVec4 &a ) { return fabs2( a.x, a.y, a.z ); }
inline float fabsxyz( const CVec4 &a ) { return fabs( a.x, a.y, a.z ); }
inline float fabsxy2( const CVec4 &a ) { return fabs2( a.x, a.y ); }
inline float fabsxy( const CVec4 &a ) { return fabs( a.x, a.y ); }
inline bool Normalize( CVec4 *pVec ) { return Normalize(pVec->x, pVec->y, pVec->z, pVec->w); }
class CLine2
{
	bool bNormalized;
public:
	float a, b, c;

	CLine2() : bNormalized( false ), a( 0.0f ), b( 0.0f ), c( 0.0f ) { }
	CLine2( const float _a, const float _b, const float _c ) : a( _a ), b( _b ), c( _c ), bNormalized( false ) { }
	CLine2( const CVec2 &p1, const CVec2 &p2 ) : a( p2.y - p1.y ), b( p1.x - p2.x ), c( p2.x*p1.y - p1.x*p2.y ), bNormalized( false ) {  }

	float DistToPoint( const CVec2 &point );
	void ProjectPoint( const CVec2 &point, CVec2 *result );
	const int GetSign( const CVec2 &point ) const { return Sign( a * point.x + b * point.y + c); }

	void Normalize()
	{
		if ( !bNormalized )
		{
			const float fCoeff = 1.0f / fabs( a, b );
			a *= fCoeff;
			b *= fCoeff;
			c *= fCoeff;

			bNormalized = true;
		}
	}
};
class CSegment
{
public:
	CVec2 p1, p2, dir;
	CSegment() { }
	CSegment( const CVec2 &_p1, const CVec2 &_p2 ) 
		: p1( _p1 ), p2( _p2 ), dir( _p2 - _p1 ) {  }
	const float GetDistToPoint( const CVec2 &point ) const;
	void GetClosestPoint( const CVec2 &point, CVec2 *result ) const;
};
class CCircle
{
public:
	CVec2 center;
	float r;
	CCircle() : center( VNULL2 ), r( 0.0f ) { }
	CCircle( const CVec2 &_center, const float _r ) 
		: center( _center ), r( _r ) {  }
	const CVec2& GetCenter() const { return center; }
};
inline void GetCirclesByTangent( const CVec2 &tang, const CVec2 &p, const float r, CCircle *c1, CCircle *c2 );
inline bool FindTangentPoints( const CVec2 &p, const CCircle &c, CVec2 *p1, CVec2 *p2 );
inline float STriangle( const CVec2 &p1, const CVec2 &p2, const CVec2 &p3 );
struct SPlane
{
public:
  union
  {
    struct { CVec3 n; float d; };
    struct { float a, b, c; };			// d is shared with the normal/distance view
		struct { CVec4 vec4; };
  };
public:
	SPlane() : vec4( VNULL4 ) {  }
	SPlane( float _a, float _b, float _c, float _d ) { Set( _a, _b, _c, _d ); }
  SPlane( const CVec3 &vNormale, const float fDist ) : n( vNormale ), d( fDist ) {  }
  SPlane( const CVec4 &v ) : vec4( v ) {  }
	SPlane( const SPlane &plane ) : vec4( plane.vec4 ) {  }
  bool Set( const CVec3 &pt0, const CVec3 &pt1, const CVec3 &pt2, bool bNormalize );
  void Set( const CVec3 &pt0, const CVec3 &pt1, const CVec3 &pt2 );
  bool Set( float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, bool bNormalize );
  void Set( float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2 );
  void Set( const CVec3 &vNormale, const float fDist ) { n = vNormale; d = fDist; }
	void Set( float _a, float _b, float _c, float _d );
	bool Set( float _a, float _b, float _c, float _d, bool bNormalize );
  void RecalcDist( const CVec3 &pt ) { d = -( n * pt ); }
	float GetDistanceToPoint( const CVec3 &pt ) const { return ( n*pt + d ); }
	bool IsPointOnPlane( const CVec3 &pt ) const { return n*pt == -d; }
	bool IsPointOverPlane( const CVec3 &pt ) const { return n*pt > -d; }
	bool IsPointUnderPlane( const CVec3 &pt ) const { return n*pt < -d; }
  DWORD CheckPointUnderPlane( const CVec3 &pt ) const;
};
struct SHMatrix
{
public :
	union
	{
		float m[4][4];
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		struct
		{
			float xx, xy, xz, xw;
			float yx, yy, yz, yw;
			float zx, zy, zz, zw;
			float wx, wy, wz, ww;
		};
		struct
		{
			CVec4 x, y, z, w;
		};
	};
public :
	SHMatrix()
		: _11( 0.0f ), _12( 0.0f ), _13( 0.0f ), _14( 0.0f ),
		  _21( 0.0f ), _22( 0.0f ), _23( 0.0f ), _24( 0.0f ),
		  _31( 0.0f ), _32( 0.0f ), _33( 0.0f ), _34( 0.0f ),
		  _41( 0.0f ), _42( 0.0f ), _43( 0.0f ), _44( 0.0f ) { }
	SHMatrix( float __11, float __12, float __13, float __14,
		        float __21, float __22, float __23, float __24,
		        float __31, float __32, float __33, float __34,
						float __41, float __42, float __43, float __44 )
						: _11(__11), _12(__12), _13(__13), _14(__14),
						  _21(__21), _22(__22), _23(__23), _24(__24),
		          _31(__31), _32(__32), _33(__33), _34(__34),
							_41(__41), _42(__42), _43(__43), _44(__44) {  }
	SHMatrix( const float (&_m)[4][4] )
		: _11( _m[0][0] ), _12( _m[0][1] ), _13( _m[0][2] ), _14( _m[0][3] ),
		  _21( _m[1][0] ), _22( _m[1][1] ), _23( _m[1][2] ), _24( _m[1][3] ),
		  _31( _m[2][0] ), _32( _m[2][1] ), _33( _m[2][2] ), _34( _m[2][3] ),
		  _41( _m[3][0] ), _42( _m[3][1] ), _43( _m[3][2] ), _44( _m[3][3] ) { }
	SHMatrix( const float *_m )
		: _11( _m[0] ), _12( _m[1] ), _13( _m[2] ), _14( _m[3] ),
		  _21( _m[4] ), _22( _m[5] ), _23( _m[6] ), _24( _m[7] ),
		  _31( _m[8] ), _32( _m[9] ), _33( _m[10] ), _34( _m[11] ),
		  _41( _m[12] ), _42( _m[13] ), _43( _m[14] ), _44( _m[15] ) { }
	SHMatrix( const class CQuat &quat ) { Set( quat ); }
	SHMatrix( const CVec3 &vPos, const class CQuat &quat ) { Set( vPos, quat ); }
	void Set( const class CQuat &quat );
	void Set( const CVec3 &vPos, const class CQuat &quat );
	void Set( float __11, float __12, float __13, float __14,
		        float __21, float __22, float __23, float __24,
		        float __31, float __32, float __33, float __34,
		        float __41, float __42, float __43, float __44 );
	const CVec3& GetXAxis3() const { return *reinterpret_cast<const CVec3*>( &_11 ); }
	const CVec3& GetYAxis3() const { return *reinterpret_cast<const CVec3*>( &_21 ); }
	const CVec3& GetZAxis3() const { return *reinterpret_cast<const CVec3*>( &_31 ); }
	const CVec4& GetXAxis4() const { return *reinterpret_cast<const CVec4*>( &_11 ); }
	const CVec4& GetYAxis4() const { return *reinterpret_cast<const CVec4*>( &_21 ); }
	const CVec4& GetZAxis4() const { return *reinterpret_cast<const CVec4*>( &_31 ); }
	const CVec4& GetWAxis4() const { return *reinterpret_cast<const CVec4*>( &_41 ); }
	const CVec3 GetTrans3() const { return CVec3( _14, _24, _34 ); }
	const CVec4 GetTrans4() const { return CVec4( _14, _24, _34, _44 ); }
	void RotateVector( CVec3 *pResult, const CVec3 &pt ) const;
	void RotateHVector( CVec3 *pResult, const CVec3 &pt ) const;
	void RotateHVector( CVec4 *pResult, const CVec3 &pt ) const;
	void RotateHVector( CVec4 *pResult, const CVec4 &pt ) const;
	void RotateHVectorTransposed( CVec4 *pResult, const CVec4 &pt ) const;
	bool HomogeneousInverse( const SHMatrix &m );
};
const SHMatrix MNULL = SHMatrix( 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
const SHMatrix MONE  = SHMatrix( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 );
class CQuat
{
  union
  {
    struct { float i, j, k, s; };
    struct { float x, y, z, w; };
    struct { CVec3 n; float r; };
		struct { CVec4 vec4; };
  };
  CQuat( float fX, float fY, float fZ, float fW, int, int ) : x( fX ), y( fY ), z( fZ ), w( fW ) {  }
public:
  CQuat( float fAngle, float fAxisX, float fAxisY, float fAxisZ, const bool bNormalizeAxis = false );
  CQuat( float fAngle, const CVec3 &vAxis, const bool bNormalizeAxis = false );
	CQuat( const CVec4 &quat ) { vec4 = quat; }
	CQuat() : x( 0.0f ), y( 0.0f ), z( 0.0f ), w( 0.0f ) {  }
	void FromAngleAxis( float fAngle, const CVec3 &vAxis, const bool bNormalizeAxis = false );
	void FromAngleAxis( float fAngle, float fAxisX, float fAxisY, float fAxisZ, const bool bNormalizeAxis = false );
	void FromEulerMatrix( const SHMatrix &m );
  void FromEulerAngles( float yaw, float pitch, float roll );
	void FromComponents( float _x, float _y, float _z, float _w ) { x = _x; y = _y; z = _z; w = _w; }
	void FromComponents( const CVec4 &_vec4 ) { vec4 = _vec4; }
	void DecompAngleAxis( float *pfAngle, CVec3 *pvAxis ) const;
	void DecompAngleAxis( float *pfAngle, float *pfAxisX, float *pfAxisY, float *pfAxisZ ) const;
	void DecompEulerMatrix( SHMatrix *pMatrix ) const;
	void DecompReversedEulerMatrix( SHMatrix *pMatrix ) const;
  bool Normalize() { return ::Normalize(x, y, z, w); }
  void Maximize( const CQuat &v ) { x = Max( x, v.x ); y = Max( y, v.y ); z = Max( z, v.z ); w = Max( w, v.w ); }
  void Minimize( const CQuat &v ) { x = Min( x, v.x ); y = Min( y, v.y ); z = Min( z, v.z ); w = Min( w, v.w ); }
  void Negate( const CQuat &q ) { x = -q.x; y = -q.y; z = -q.z; w = -q.w; } // this = -v
  void Negate() { x = -x; y = -y; z = -z; w = -w; }     // this = -this
	bool Inverse( const CQuat &q );
	bool Inverse();
	void UnitInverse( const CQuat &q ) { x = -q.x; y = -q.y; z = -q.z; w = q.w; }
	void UnitInverse() { x = -x; y = -y; z = -z; }
  void UnitInverseX() { x = -x; }
  void UnitInverseY() { y = -y; }
  void UnitInverseZ() { z = -z; }
  void Deriv( const CQuat &q, const CVec3 &v );
  friend const CQuat operator*( const CQuat &a, const CQuat &b );
  friend const CQuat operator/( const CQuat &a, const CQuat &b );
  CQuat& operator*=( const CQuat &quat );
	CQuat& operator/=( const CQuat &quat );
	const CQuat operator*( const float c ) const;
	const CQuat operator+( const CQuat &q ) const { return CQuat( x + q.x, y + q.y, z + q.z, w + q.w, 0, 0 ); }
	const CQuat operator-() const { return CQuat( -x, -y, -z, -w, 0, 0 ); }        // unary minus
	float Dot( const CQuat &quat ) const { return x*quat.x + y*quat.y + z*quat.z + w*quat.w; }
	void MinimizeRotationAngle() { if ( w < 0 ) { x = -x; y = -y; z = -z; w = -w; } }
  const CQuat Exp() const;
  const CQuat Log() const;
	void Slerp( const float factor, const CQuat &p, const CQuat &q );
  void Interpolate( const CQuat &p, const CQuat &q, const float t ) { Slerp(t, p, q); }
  const CVec3 Rotate( const CVec3 &r ) const;
  void Rotate( CVec3 *pRes, const CVec3 &vec ) const;
  const CVec3 GetXAxis() const;
  const CVec3 GetYAxis() const;
  const CVec3 GetZAxis() const;
  void GetXAxis( CVec3 *pResult ) const;
  void GetYAxis( CVec3 *pResult ) const;
  void GetZAxis( CVec3 *pResult ) const;
  friend float fabs2( const CQuat &q ) { return fabs2( q.x, q.y, q.z, q.w ); }
  friend float fabs( const CQuat &q ) { return static_cast<float>( sqrt( fabs2(q) ) ); }
};
const CQuat QNULL = CQuat( 0, 1, 0, 0 );
template <class TYPE>
class CTPoint
{
public:
	typedef CTPoint<TYPE> TPoint;
	union
	{
		struct { TYPE x, y; };
		struct { TYPE u, v; };
		struct { TYPE s, t; };
		struct { TYPE a, b; };
		struct { TYPE min, max; };
	};
public:
	CTPoint() {  }
	CTPoint( TYPE _x, TYPE _y ) : x( _x ), y( _y ) {  }
	CTPoint( const TPoint &pt ) : x( pt.x ), y( pt.y ) {  }
	bool operator==( const TPoint &v ) const { return ( (v.x == x) && (v.y == y) ); }
	bool operator!=( const TPoint &v ) const { return ( (v.x != x) || (v.y != y) ); }
  TPoint& operator+=( const TPoint &v ) { x += v.x; y += v.y; return *this; }
  TPoint& operator-=( const TPoint &v ) { x -= v.x; y -= v.y; return *this; }
  TPoint& operator*=( const TYPE d ) { x *= d; y *= d; return *this; }
};
template<class T>
inline float fabs( const CTPoint<T> &pt ) { return fabs( pt.x, pt.y ); }
template<class T>
inline float fabs2( const CTPoint<T> &pt ) { return fabs2( pt.x, pt.y ); }
template<class T>
inline const CTPoint<T> operator-( const CTPoint<T> &a) { return CTPoint<T>(-a.x, -a.y); }
template<class T>
inline const CTPoint<T> operator+( const CTPoint<T> &a, const CTPoint<T> &b ) { return CTPoint<T>( a.x + b.x, a.y + b.y ); }
template<class T>
inline const CTPoint<T> operator-( const CTPoint<T> &a, const CTPoint<T> &b ) { return CTPoint<T>( a.x - b.x, a.y - b.y ); }
template<class T>
inline const CTPoint<T> operator*( const CTPoint<T> &a, const float b ) { return CTPoint<T>( a.x*b, a.y*b ); }
template<class T>
inline const CTPoint<T> operator*( const T a, const CTPoint<T> &b ) { return CTPoint<T>( b.x*a, b.y*a ); }
template<class T>
inline const CTPoint<T> operator*( const CTPoint<T> &b, const T a ) { return CTPoint<T>( b.x*a, b.y*a ); }
template <class TYPE>
class CTRect
{
public:
	typedef CTRect<TYPE> TRect;
	typedef CTPoint<TYPE> TPoint;
	union                                 // left (minimal) x
	{
		TYPE left;
		TYPE x1;
		TYPE minx;
	};
	union                                 // top (minimal) y
	{
		TYPE top;
		TYPE y1;
		TYPE miny;
	};
	union                                 // right (maximal) x
	{
		TYPE right;
		TYPE x2;
		TYPE maxx;
	};
	union                                 // bottom (maximal) y
	{
		TYPE bottom;
		TYPE y2;
		TYPE maxy;
	};
public:
	CTRect() : minx( 0 ), miny( 0 ), maxx( 0 ), maxy( 0 ) {  }
	CTRect( const TYPE &_minx, const TYPE &_miny, const TYPE &_maxx, const TYPE &_maxy ) : minx( _minx ), miny( _miny ), maxx( _maxx ), maxy( _maxy ) {  }
	CTRect( const TPoint &vLT, const TPoint &vRB ) : minx( vLT.x ), miny( vLT.y ), maxx( vRB.x ), maxy( vRB.y ) {  }
	CTRect( const CVec2 &vLT, const CVec2 &vRB ) : minx( vLT.x ), miny( vLT.y ), maxx( vRB.x ), maxy( vRB.y ) {  }
	CTRect( const TRect &rect ) : x1( rect.x1 ), y1( rect.y1 ), x2( rect.x2 ), y2( rect.y2 ) {  }
	CTRect( const RECT &rect ) : x1( rect.left ), y1( rect.top ), x2( rect.right ), y2( rect.bottom ) {  }
	const TRect& operator=( const TRect &rect ) { x1 = rect.x1; y1 = rect.y1; x2 = rect.x2; y2 = rect.y2; return *this; }
	void Set( const TYPE &_x1, const TYPE &_y1, const TYPE &_x2, const TYPE &_y2 ) { x1 = _x1; y1 = _y1; x2 = _x2; y2 = _y2; }
	void Set( const CVec2 &vLT, const CVec2 &vRB ) { minx = vLT.x; miny = vLT.y; maxx = vRB.x; maxy = vRB.y; }
	void Set( const TPoint &vLT, const TPoint &vRB ) { minx = vLT.x; miny = vLT.y; maxx = vRB.x; maxy = vRB.y; }
	void Set( const TRect &rect ) { x1 = rect.x1; y1 = rect.y1; x2 = rect.x2; y2 = rect.y2; }
	void SetEmpty() { minx = miny = maxx = maxy = 0; }
	void operator*=( float fScale ) { x1 *= fScale; y1 *= fScale; x2 *= fScale; y2 *= fScale; }
	operator RECT() const { RECT rect = { long(x1), long(y1), long(x2), long(y2) }; return rect; }
	const TYPE Width() const { return ( maxx - minx ); }
	const TYPE Height() const { return ( maxy - miny ); }
	const TYPE GetSizeX() const { return Width(); }
	const TYPE GetSizeY() const { return Height(); }
	const TPoint GetSize() const { return TPoint( Width(), Height() ); }
	const TYPE GetArea() const { return Width() * Height(); }
	const TPoint GetLeftTop() const { return TPoint( minx, miny ); }
	const TPoint GetRightTop() const { return TPoint( maxx, miny ); }
	const TPoint GetLeftBottom() const { return TPoint( minx, maxy ); }
	const TPoint GetRightBottom() const { return TPoint( maxx, maxy ); }
	const TPoint GetCenter() const { return TPoint( (minx + maxx) / TYPE(2), (miny + maxy) / TYPE(2) ); }
	bool operator==( const TRect &rc ) { return (x1 == rc.x1) && (y1 == rc.y1) && (x2 == rc.x2) && (y2 == rc.y2); }
	bool operator==( const RECT &rc ) { return (left == rc.left) && (top == rc.top) && (right == rc.right) && (bottom == rc.bottom); }
	bool operator!=( const TRect &rc ) { return (x1 != rc.x1) || (y1 != rc.y1) || (x2 != rc.x2) || (y2 != rc.y2); }
	bool operator!=( const RECT &rc ) { return (left != rc.left) || (top != rc.top) || (right != rc.right) || (bottom != rc.bottom); }
	bool IsEmpty() const { return ( (minx >= maxx) || (miny >= maxy) ); }
	bool IsInside( const CVec2 &pt ) const { return (pt.x >= minx) && (pt.x <= maxx) && (pt.y >= miny) && (pt.y <= maxy); }
	bool IsInside( const TPoint &pt ) const { return (pt.x >= minx) && (pt.x <= maxx) && (pt.y >= miny) && (pt.y <= maxy); }
	bool IsInside( const TYPE &x, const TYPE &y ) const { return (x >= minx) && (x <= maxx) && (y >= miny) && (y <= maxy); }
	bool IsInside( const TRect &rect ) const { return (rect.minx >= minx) && (rect.maxx <= maxx) && (rect.miny >= miny) && (rect.maxy <= maxy); }
	bool IsIntersect( const TRect &rc ) const { return ( Max(minx, rc.minx) < Min(maxx, rc.maxx) ) && ( Max(miny, rc.miny) < Min(maxy, rc.maxy) ); }
	bool IsIntersectEdges( const TRect &rc ) const { return ( Max(minx, rc.minx) <= Min(maxx, rc.maxx) ) && ( Max(miny, rc.miny) <= Min(maxy, rc.maxy) ); }
	void Intersect( const TRect &rect )
	{
		minx = Max( minx, rect.minx ); maxx = Min( maxx, rect.maxx );
		miny = Max( miny, rect.miny ); maxy = Min( maxy, rect.maxy );
	}
	void Union( const TRect &rect )
	{
		if ( IsEmpty() )
			*this = rect;
		else if ( !rect.IsEmpty() )
		{
			minx = Min( minx, rect.minx ); maxx = Max( maxx, rect.maxx );
			miny = Min( miny, rect.miny ); maxy = Max( maxy, rect.maxy );
		}
	}
	void Inflate( const TYPE &halfX, const TYPE &halfY ) { x1 -= halfX; y1 -= halfY; x2 += halfX; y2 += halfY; }
	void Deflate( const TYPE &halfX, const TYPE &halfY ) { x1 += halfX; y1 += halfY; x2 -= halfX; y2 -= halfY; }
	void MoveTo( const TYPE &x, const TYPE &y ) { x2 += x - x1; y2 += y - y1; x1 = x; y1 = y; }
	void MoveTo( const TPoint &pt ) { MoveTo(pt.x, pt.y); }
	void MoveTo( const CVec2 &pt ) { MoveTo(pt.x, pt.y); }
	void Move( const TYPE &dx, const TYPE &dy ) { x1 += dx; y1 += dy; x2 += dx; y2 += dy; }
	void Move( const TPoint &pt ) { Move(pt.x, pt.y); }
	void Move( const CVec2 &pt ) { Move(pt.x, pt.y); }
	void Normalize() { Set( Min( minx, maxx ), Min( miny, maxy ), Max( minx, maxx ), Max( miny, maxy ) ); }
};
template <int nMaxNumMatrices>
class CMatrixStack
{
	SHMatrix matrices[nMaxNumMatrices + 1];
	int nCurrentMatrix;
public:
	CMatrixStack() : nCurrentMatrix( 0 ) { for ( int i=0; i<nMaxNumMatrices + 1; ++i ) Identity( &matrices[i] ); }
	bool IsEmpty() const { return nCurrentMatrix == 0; }
	void Clear() { nCurrentMatrix = 0; }
	void Pop( int nAmount = 1 ) { if ( nCurrentMatrix >= nAmount ) nCurrentMatrix -= nAmount; else nCurrentMatrix = 0; }
	const SHMatrix& Get() const { return matrices[nCurrentMatrix]; }
	const SHMatrix& operator()() const { return matrices[nCurrentMatrix]; }
	void Set( const SHMatrix &matrix );
	void Push43( const SHMatrix &matrix );
	void Push( const SHMatrix &matrix );
	void Push( const CVec3 &pos );
	void Push( const CQuat &rot );
	void Push( const CVec3 &pos, const CQuat &rot );
	void Push( float x, float y, float z );
	void Push( float val );
};
class CBresenham2
{
	int x1, y1;
	const int x2, y2;
	const int xlen, ylen, len;
	const int xinc, yinc;
	int xerr, yerr;
public:
	CBresenham2( int _x1, int _y1, int _x2, int _y2 )
		: x1( _x1 ), y1( _y1 ), x2( _x2 ), y2( _y2 ),
		  xlen( abs(x2 - x1) + 1 ), ylen( abs(y2 - y1) + 1 ), len( Max(xlen, ylen) ),
		  xinc( Sign(x2 - x1) ), yinc( Sign(y2 - y1) ),
			xerr( 0 ), yerr( 0 ) {  }
	void Next()
	{
		xerr += xlen;
		if ( xerr >= len )
			x1 += xinc, xerr -= len;
		yerr += ylen;
		if ( yerr >= len )
			y1 += yinc, yerr -= len;
	}
	bool IsEnd() const { return (x1 == x2) && (y1 == y2); }
	int GetX() const { return x1; }
	int GetY() const { return y1; }
};
template <class TFunctional>
	void MakeLine2( int x1, int y1, int x2, int y2, TFunctional &func )
{
	CBresenham2 line( x1, y1, x2, y2 );
	func( line.GetX(), line.GetY() );
	while ( !line.IsEnd() )
	{
		line.Next();
		func( line.GetX(), line.GetY() );
	}
}
template <class TFunctional>
	void ScanLine2( int x1, int y1, int x2, int y2, TFunctional &func )
{
	CBresenham2 line( x1, y1, x2, y2 );
	if ( func( line.GetX(), line.GetY() ) == false )
		return;
	while ( !line.IsEnd() )
	{
		line.Next();
		if ( func( line.GetX(), line.GetY() ) == false )
			break;
	}
}
class CBresenham3
{
	int x1, y1, z1;
	const int x2, y2, z2;
	const int xlen, ylen, zlen, len;
	const int xinc, yinc, zinc;
	int xerr, yerr, zerr;
public:
	CBresenham3( int _x1, int _y1, int _z1, int _x2, int _y2, int _z2 )
		: x1( _x1 ), y1( _y1 ), z1( _z1 ), x2( _x2 ), y2( _y2 ), z2( _z2 ),
		  xlen( abs(x2 - x1) + 1 ), ylen( abs(y2 - y1) + 1 ), zlen( abs(z2 - z1) + 1 ), len( Max(Max(xlen, ylen), zlen) ),
		  xinc( Sign(x2 - x1) ), yinc( Sign(y2 - y1) ), zinc( Sign(z2 - z1) ),
			xerr( 0 ), yerr( 0 ), zerr( 0 ) {  }
	void Next()
	{
		xerr += xlen;
		if ( xerr >= len )
			x1 += xinc, xerr -= len;
		yerr += ylen;
		if ( yerr >= len )
			y1 += yinc, yerr -= len;
		zerr += zlen;
		if ( zerr >= len )
			z1 += zinc, zerr -= len;
	}
	bool IsEnd() const { return (x1 == x2) && (y1 == y2) && (z1 == z2); }
	int GetX() const { return x1; }
	int GetY() const { return y1; }
	int GetZ() const { return z1; }
};
template <class TFunctional>
	void MakeLine3( int x1, int y1, int z1, int x2, int y2, int z2, TFunctional &func )
{
	CBresenham3 line( x1, y1, z1, x2, y2, z2 );
	func( line.GetX(), line.GetY(), line.GetZ() );
	while ( !line.IsEnd() )
	{
		line.Next();
		func( line.GetX(), line.GetY(), line.GetZ() );
	}
}
template <class TFunctional>
	void ScanLine3( int x1, int y1, int z1, int x2, int y2, int z2, TFunctional &func )
{
	CBresenham3 line( x1, y1, z1, x2, y2, z2 );
	if ( func( line.GetX(), line.GetY(), line.GetZ() ) == false )
		return;
	while ( !line.IsEnd() )
	{
		line.Next();
		if ( func( line.GetX(), line.GetY(), line.GetZ() ) == false )
			break;
	}
}
#pragma pack()
template <class TFunctional>
void BresenhamCircle( int nCenterX, int nCenterY, int nRadius, TFunctional &func )
{
	int x = 0, y = nRadius;
	int d = 3 - 2*y;
	do
	{
		if ( d < 0 )
			d += 4*x + 6;
		else
		{
			d += 4*(x - y) + 10;
			--y;
		}
		++x;
		func( nCenterX - x, nCenterY + y );
		func( nCenterX + x, nCenterY + y );
		func( nCenterX - x, nCenterY - y );
		func( nCenterX + x, nCenterY - y );
		func( nCenterX - y, nCenterY + x );
		func( nCenterX + y, nCenterY + x );
		func( nCenterX - y, nCenterY - x );
		func( nCenterX + y, nCenterY - x );
	}	while ( x <= y );
	func( nCenterX - nRadius, nCenterY );
	func( nCenterX + nRadius, nCenterY );
	func( nCenterX, nCenterY - nRadius );
	func( nCenterX, nCenterY + nRadius );
}
inline bool SPlane::Set( const CVec3 &pt0, const CVec3 &pt1, const CVec3 &pt2, bool bNormalize )
{
  CVec3 v1( pt1.x - pt0.x, pt1.y - pt0.y, pt1.z - pt0.z ), v2( pt2.x - pt0.x, pt2.y - pt0.y, pt2.z - pt0.z );
  if ( bNormalize && ( !Normalize(&v1) || !Normalize(&v2) ) )
    return false;
	n = v1 ^ v2;
  if ( bNormalize && !Normalize(&n) )
    return false;
	d = -( pt0 * n );
  return true;
}
inline void SPlane::Set( const CVec3 &pt0, const CVec3 &pt1, const CVec3 &pt2 )
{
  CVec3 v1( pt1.x - pt0.x, pt1.y - pt0.y, pt1.z - pt0.z ), v2( pt2.x - pt0.x, pt2.y - pt0.y, pt2.z - pt0.z );
	n = v1 ^ v2;
	d = -( pt0 * n );
}
inline bool SPlane::Set( float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, bool bNormalize )
{
  CVec3 pt0( x0, y0, z0 ), v1( x1 - x0, y1 - y0, z1 - z0 ), v2( x2 - x0, y2 - y0, z2 - z0 );
  if ( bNormalize && ( !Normalize(&v1) || !Normalize(&v2) ) )
    return false;
	n = v1 ^ v2;
  if ( bNormalize && !Normalize(&n) )
    return false;
	d = -( pt0 * n );
  return true;
}
inline void SPlane::Set( float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2 )
{
  CVec3 pt0( x0, y0, z0 ), v1( x1 - x0, y1 - y0, z1 - z0 ), v2( x2 - x0, y2 - y0, z2 - z0 );
	n = v1 ^ v2;
	d = -( pt0 * n );
}
inline void SPlane::Set( float _a, float _b, float _c, float _d )
{
	n.Set( _a, _b, _c );
	d = _d;
}
inline bool SPlane::Set( float _a, float _b, float _c, float _d, bool bNormalize )
{
	Set( _a, _b, _c, _d );
	if ( bNormalize )
	{
		float fLen = fabs2( n );
		if ( fLen < 1e-6f )
			return false;
		fLen = 1.0f / float( sqrt( fLen ) );
		n *= fLen;
		d *= fLen;
	}
	return true;
}
inline DWORD SPlane::CheckPointUnderPlane( const CVec3 &pt ) const
{
  float fDist = n*pt + d;
  return ( bit_cast<DWORD>(fDist) & 0x80000000 );
}
inline void Identity( SHMatrix *pRes )
{
	MemSetDWord( reinterpret_cast<DWORD*>(pRes), 0, 16 );
	pRes->_11 = pRes->_22 = pRes->_33 = pRes->_44 = 1.0f;
}
inline void SHMatrix::Set( const CQuat &quat )
{
	quat.DecompEulerMatrix( this );
	_14 = _24 = _34 = _41 = _42 = _43 = 0;
	_44 = 1;
}
inline void SHMatrix::Set( const CVec3 &vPos, const CQuat &quat )
{
	quat.DecompEulerMatrix( this );
	_14 = vPos.x; _24 = vPos.y; _34 = vPos.z;
	_41 = _42 = _43 = 0;
	_44 = 1;
}
inline void SHMatrix::Set( float __11, float __12, float __13, float __14,
                           float __21, float __22, float __23, float __24,
                           float __31, float __32, float __33, float __34,
                           float __41, float __42, float __43, float __44 )
{
	_11 = __11; _12 = __12; _13 = __13; _14 = __14;
	_21 = __21; _22 = __22; _23 = __23; _24 = __24;
	_31 = __31; _32 = __32; _33 = __33; _34 = __34;
	_41 = __41; _42 = __42; _43 = __43; _44 = __44;
}
inline void Transpose( SHMatrix *p, const SHMatrix &m )
{
	p->Set( m._11, m._21, m._31, m._41,
		      m._12, m._22, m._32, m._42,
					m._13, m._23, m._33, m._43,
					m._14, m._24, m._34, m._44 );
}
inline void Transpose( SHMatrix *p )
{
	float t = p->_12; p->_12 = p->_21; p->_21 = t;
	t = p->_13; p->_13 = p->_31; p->_31 = t;
	t = p->_14; p->_14 = p->_41; p->_41 = t;
	t = p->_23; p->_23 = p->_32; p->_32 = t;
	t = p->_24; p->_24 = p->_42; p->_42 = t;
	t = p->_34; p->_34 = p->_43; p->_43 = t;
}
inline void Multiply( SHMatrix *p, const SHMatrix &a, const SHMatrix &b )
{
	p->_11 = a._11*b._11 + a._12*b._21 + a._13*b._31 + a._14*b._41;
	p->_12 = a._11*b._12 + a._12*b._22 + a._13*b._32 + a._14*b._42;
	p->_13 = a._11*b._13 + a._12*b._23 + a._13*b._33 + a._14*b._43;
	p->_14 = a._11*b._14 + a._12*b._24 + a._13*b._34 + a._14*b._44;

	p->_21 = a._21*b._11 + a._22*b._21 + a._23*b._31 + a._24*b._41;
	p->_22 = a._21*b._12 + a._22*b._22 + a._23*b._32 + a._24*b._42;
	p->_23 = a._21*b._13 + a._22*b._23 + a._23*b._33 + a._24*b._43;
	p->_24 = a._21*b._14 + a._22*b._24 + a._23*b._34 + a._24*b._44;

	p->_31 = a._31*b._11 + a._32*b._21 + a._33*b._31 + a._34*b._41;
	p->_32 = a._31*b._12 + a._32*b._22 + a._33*b._32 + a._34*b._42;
	p->_33 = a._31*b._13 + a._32*b._23 + a._33*b._33 + a._34*b._43;
	p->_34 = a._31*b._14 + a._32*b._24 + a._33*b._34 + a._34*b._44;

	p->_41 = a._41*b._11 + a._42*b._21 + a._43*b._31 + a._44*b._41;
	p->_42 = a._41*b._12 + a._42*b._22 + a._43*b._32 + a._44*b._42;
	p->_43 = a._41*b._13 + a._42*b._23 + a._43*b._33 + a._44*b._43;
	p->_44 = a._41*b._14 + a._42*b._24 + a._43*b._34 + a._44*b._44;
}
inline void Multiply43( SHMatrix *p, const SHMatrix &a, const SHMatrix &b )
{
	p->_11 = a._11*b._11 + a._12*b._21 + a._13*b._31;
	p->_12 = a._11*b._12 + a._12*b._22 + a._13*b._32;
	p->_13 = a._11*b._13 + a._12*b._23 + a._13*b._33;
	p->_14 = a._11*b._14 + a._12*b._24 + a._13*b._34 + a._14;

	p->_21 = a._21*b._11 + a._22*b._21 + a._23*b._31;
	p->_22 = a._21*b._12 + a._22*b._22 + a._23*b._32;
	p->_23 = a._21*b._13 + a._22*b._23 + a._23*b._33;
	p->_24 = a._21*b._14 + a._22*b._24 + a._23*b._34 + a._24;

	p->_31 = a._31*b._11 + a._32*b._21 + a._33*b._31;
	p->_32 = a._31*b._12 + a._32*b._22 + a._33*b._32;
	p->_33 = a._31*b._13 + a._32*b._23 + a._33*b._33;
	p->_34 = a._31*b._14 + a._32*b._24 + a._33*b._34 + a._34;

	p->_41 = a._41*b._11 + a._42*b._21 + a._43*b._31;
	p->_42 = a._41*b._12 + a._42*b._22 + a._43*b._32;
	p->_43 = a._41*b._13 + a._42*b._23 + a._43*b._33;
	p->_44 = a._41*b._14 + a._42*b._24 + a._43*b._34 + a._44;
}
inline void MultiplyTranslate( SHMatrix *p, const SHMatrix &a, const CVec3 &b )
{
	p->_11 = a._11;
	p->_12 = a._12;
	p->_13 = a._13;
	p->_14 = a._11*b.x + a._12*b.y + a._13*b.z + a._14;

	p->_21 = a._21;
	p->_22 = a._22;
	p->_23 = a._23;
	p->_24 = a._21*b.x + a._22*b.y + a._23*b.z + a._24;

	p->_31 = a._31;
	p->_32 = a._32;
	p->_33 = a._33;
	p->_34 = a._31*b.x + a._32*b.y + a._33*b.z + a._34;

	p->_41 = a._41;
	p->_42 = a._42;
	p->_43 = a._43;
	p->_44 = a._41*b.x + a._42*b.y + a._43*b.z + a._44;
}
inline void MultiplyScale( SHMatrix *p, const SHMatrix &a, const float fX, const float fY, const float fZ )
{
	p->_11 = a._11*fX;
	p->_12 = a._12*fY;
	p->_13 = a._13*fZ;
	p->_14 = a._14;

	p->_21 = a._21*fX;
	p->_22 = a._22*fY;
	p->_23 = a._23*fZ;
	p->_24 = a._24;

	p->_31 = a._31*fX;
	p->_32 = a._32*fY;
	p->_33 = a._33*fZ;
	p->_34 = a._34;

	p->_41 = a._41*fX;
	p->_42 = a._42*fY;
	p->_43 = a._43*fZ;
	p->_44 = a._44;
}
inline const SHMatrix operator*( const SHMatrix &a, const SHMatrix &b )
{
  SHMatrix ret;
  Multiply( &ret, a, b );
  return ret;
}
inline void SHMatrix::RotateVector( CVec3 *pResult, const CVec3 &pt ) const
{
	const float x = _11*pt.x + _12*pt.y + _13*pt.z;
	const float y = _21*pt.x + _22*pt.y + _23*pt.z;
	const float z = _31*pt.x + _32*pt.y + _33*pt.z;
	pResult->Set( x, y, z );
}
inline void SHMatrix::RotateHVector( CVec3 *pResult, const CVec3 &pt ) const
{
	const float x = _11*pt.x + _12*pt.y + _13*pt.z + _14;
	const float y = _21*pt.x + _22*pt.y + _23*pt.z + _24;
	const float z = _31*pt.x + _32*pt.y + _33*pt.z + _34;
	pResult->Set( x, y, z );
}
inline void SHMatrix::RotateHVector( CVec4 *pResult, const CVec3 &pt ) const
{
	const float x = _11*pt.x + _12*pt.y + _13*pt.z + _14;
	const float y = _21*pt.x + _22*pt.y + _23*pt.z + _24;
	const float z = _31*pt.x + _32*pt.y + _33*pt.z + _34;
	const float w = _41*pt.x + _42*pt.y + _43*pt.z + _44;
	pResult->Set( x, y, z, w );
}
inline void SHMatrix::RotateHVector( CVec4 *pResult, const CVec4 &pt ) const
{
	const float x = _11*pt.x + _12*pt.y + _13*pt.z + _14*pt.w;
	const float y = _21*pt.x + _22*pt.y + _23*pt.z + _24*pt.w;
	const float z = _31*pt.x + _32*pt.y + _33*pt.z + _34*pt.w;
	const float w = _41*pt.x + _42*pt.y + _43*pt.z + _44*pt.w;
	pResult->Set( x, y, z, w );
}
inline void SHMatrix::RotateHVectorTransposed( CVec4 *pResult, const CVec4 &pt ) const
{
	const float x = _11*pt.x + _21*pt.y + _31*pt.z + _41*pt.w;
	const float y = _12*pt.x + _22*pt.y + _32*pt.z + _42*pt.w;
	const float z = _13*pt.x + _23*pt.y + _33*pt.z + _43*pt.w;
	const float w = _14*pt.x + _24*pt.y + _34*pt.z + _44*pt.w;
	pResult->Set( x, y, z, w );
}
inline bool SHMatrix::HomogeneousInverse( const SHMatrix &m )
{
	float det =	m._11*(m._22*m._33 - m._23*m._32) + m._21*(m._13*m._32 - m._12*m._33) + m._31*(m._12*m._23 - m._13*m._22);
	if ( det == 0 )
		return false;                       // singular matrix found !
	det = 1.0f/det;
	_11 = ( m._22*m._33 - m._23*m._32 ) * det;
	_12 = ( m._13*m._32 - m._12*m._33 ) * det;
	_13 = ( m._12*m._23 - m._13*m._22 ) * det;
	_14 = -( m._14*_11 + m._24*_12 + m._34*_13 );
	_21 = ( m._23*m._31 - m._21*m._33 ) * det;
	_22 = ( m._11*m._33 - m._13*m._31 ) * det;
	_23 = ( m._13*m._21 - m._11*m._23 ) * det;
	_24 = -( m._14*_21 + m._24*_22 + m._34*_23 );
	_31 = ( m._21*m._32 - m._22*m._31 ) * det;
	_32 = ( m._12*m._31 - m._11*m._32 ) * det;
	_33 = ( m._11*m._22 - m._12*m._21 ) * det;
	_34 = -( m._14*_31 + m._24*_32 + m._34*_33 );
	_41 = _42 = _43 = 0.0f;
	_44 = 1.0f;
	return false;
}
inline bool Invert( SHMatrix *pRes, const SHMatrix &m )
{
	const float m3344 = m._33 * m._44 - m._43 * m._34;
	const float m2344 = m._23 * m._44 - m._43 * m._24;
	const float m2334 = m._23 * m._34 - m._33 * m._24;
	const float m3244 = m._32 * m._44 - m._42 * m._34;
	const float m2244 = m._22 * m._44 - m._42 * m._24;
	const float m2234 = m._22 * m._34 - m._32 * m._24;
	const float m3243 = m._32 * m._43 - m._42 * m._33;
	const float m2243 = m._22 * m._43 - m._42 * m._23;
	const float m2233 = m._22 * m._33 - m._32 * m._23;
	const float m1344 = m._13 * m._44 - m._43 * m._14;
	const float m1334 = m._13 * m._34 - m._33 * m._14;
	const float m1244 = m._12 * m._44 - m._42 * m._14;
	const float m1234 = m._12 * m._34 - m._32 * m._14;
	const float m1243 = m._12 * m._43 - m._42 * m._13;
	const float m1233 = m._12 * m._33 - m._32 * m._13;
	const float m1324 = m._13 * m._24 - m._23 * m._14;
	const float m1224 = m._12 * m._24 - m._22 * m._14;
	const float m1223 = m._12 * m._23 - m._22 * m._13;
	pRes->_11 =  m._22 * m3344 - m._32 * m2344 + m._42 * m2334;
	pRes->_21 = -m._21 * m3344 + m._31 * m2344 - m._41 * m2334;
	pRes->_31 =  m._21 * m3244 - m._31 * m2244 + m._41 * m2234;
	pRes->_41 = -m._21 * m3243 + m._31 * m2243 - m._41 * m2233;

	pRes->_12 = -m._12 * m3344 + m._32 * m1344 - m._42 * m1334;
	pRes->_22 =  m._11 * m3344 - m._31 * m1344 + m._41 * m1334;
	pRes->_32 = -m._11 * m3244 + m._31 * m1244 - m._41 * m1234;
	pRes->_42 =  m._11 * m3243 - m._31 * m1243 + m._41 * m1233;

	pRes->_13 =  m._12 * m2344 - m._22 * m1344 + m._42 * m1324;
	pRes->_23 = -m._11 * m2344 + m._21 * m1344 - m._41 * m1324;
	pRes->_33 =  m._11 * m2244 - m._21 * m1244 + m._41 * m1224;
	pRes->_43 = -m._11 * m2243 + m._21 * m1243 - m._41 * m1223;

	pRes->_14 = -m._12 * m2334 + m._22 * m1334 - m._32 * m1324;
	pRes->_24 =  m._11 * m2334 - m._21 * m1334 + m._31 * m1324;
	pRes->_34 = -m._11 * m2234 + m._21 * m1234 - m._31 * m1224;
	pRes->_44 =  m._11 * m2233 - m._21 * m1233 + m._31 * m1223;
	float fDet = m._11*pRes->_11 + m._21*pRes->_12 + m._31*pRes->_13 + m._41*pRes->_14;
	if ( fDet == 0 )
		return false;												// singular matrix found !
	fDet = 1.0f / fDet;
	pRes->_11 *= fDet;	pRes->_21 *= fDet;	pRes->_31 *= fDet;	pRes->_41 *= fDet;
	pRes->_12 *= fDet;	pRes->_22 *= fDet;	pRes->_32 *= fDet;	pRes->_42 *= fDet;
	pRes->_13 *= fDet;	pRes->_23 *= fDet;	pRes->_33 *= fDet;	pRes->_43 *= fDet;
	pRes->_14 *= fDet;	pRes->_24 *= fDet;	pRes->_34 *= fDet;	pRes->_44 *= fDet;

	return true;
}
inline float Det( float _11 ) { return _11; }
inline float Det( float _11, float _12, float _21, float _22 ) { return _11*_22 - _12*_21; }

inline float Det( float _11, float _12, float _13,
	                float _21, float _22, float _23,
	                float _31, float _32, float _33 )
{
	return _11*( _22*_33 - _32*_23 ) -
		     _21*( _12*_33 - _32*_13 ) +
		     _31*( _12*_23 - _22*_13 );
}

inline float Det( float _11, float _12, float _13, float _14,
	                float _21, float _22, float _23, float _24,
	                float _31, float _32, float _33, float _34,
	                float _41, float _42, float _43, float _44 )
{
	const float M3344 = _33*_44 - _43*_34;
	const float M2344 = _23*_44 - _43*_24;
	const float M2334 = _23*_34 - _33*_24;
	const float M1344 = _13*_44 - _43*_14;
	const float M1334 = _13*_34 - _33*_14;
	const float M1324 = _13*_24 - _23*_14;

	return _11 * ( _22*M3344 - _32*M2344 + _42*M2334 ) -
		     _21 * ( _12*M3344 - _32*M1344 + _42*M1334 ) +
		     _31 * ( _12*M2344 - _22*M1344 + _42*M1324 ) -
		     _41 * ( _12*M2334 - _22*M1334 + _32*M1324 );
}
inline float Det( const SHMatrix &m )
{
	return Det( m._11, m._12, m._13, m._14,
		          m._21, m._22, m._23, m._24,
							m._31, m._32, m._33, m._34,
							m._41, m._42, m._43, m._44 );
}
inline void CreatePerspectiveProjectionMatrixLH( SHMatrix *pRes, float fov, float fAspect, float fNear, float fFar )
{
	const float c = static_cast<float>( cos( fov*0.5 ) );
	const float s = static_cast<float>( sin( fov*0.5 ) );
	const float Q = s * fFar / ( fFar - fNear );
	Zero( *pRes );
	pRes->_11 = c / s / fAspect;
	pRes->_22 = c / s;
	pRes->_33 = Q / s;
	pRes->_34 = -Q*fNear / s;
	pRes->_43 = s / s;
}
inline void CreatePerspectiveProjectionMatrixRH( SHMatrix *pRes, float fov, float fAspect, float fNear, float fFar )
{
	const float c = static_cast<float>( cos( fov*0.5 ) );
	const float s = static_cast<float>( sin( fov*0.5 ) );
	const float Q = s * fFar / ( fFar - fNear );
	Zero( *pRes );
	pRes->_11 = c / s / fAspect;
	pRes->_22 = c / s;
	pRes->_33 = -Q / s;
	pRes->_34 = -Q*fNear / s;
	pRes->_43 = -s / s;
}
inline void CreateOrthographicProjectionMatrixLH( SHMatrix *pRes, float fWidth, float fHeight, float fNear, float fFar )
{
	Zero( *pRes );
	pRes->_11 = 2.0f / fWidth;
	pRes->_14 = 0;
	pRes->_22 = 2.0f / fHeight;
	pRes->_24 = 0;
	pRes->_33 = 1.0f / ( fFar - fNear );
	pRes->_34 = -fNear / ( fFar - fNear );
	pRes->_44 = 1.0f;
}
inline void CreateOrthographicProjectionMatrixRH( SHMatrix *pRes, float fWidth, float fHeight, float fNear, float fFar )
{
	Zero( *pRes );
	pRes->_11 = 2.0f / fWidth;
	pRes->_14 = 0;
	pRes->_22 = 2.0f / fHeight;
	pRes->_24 = 0;
	pRes->_33 = -1.0f / ( fFar - fNear );
	pRes->_34 = -fNear / ( fFar - fNear );
	pRes->_44 = 1.0f;
}
inline void CreateViewMatrix( SHMatrix *pRes, const CVec3 &vX, const CVec3 &vY, const CVec3 &vZ, const CVec3 &vO )
{
	pRes->_11 = vX.x;  pRes->_12 = vX.y;  pRes->_13 = vX.z;  pRes->_14 = -( vX * vO );
	pRes->_21 = vY.x;  pRes->_22 = vY.y;  pRes->_23 = vY.z;  pRes->_24 = -( vY * vO );
	pRes->_31 = vZ.x;  pRes->_32 = vZ.y;  pRes->_33 = vZ.z;  pRes->_34 = -( vZ * vO );
	pRes->_41 = pRes->_42 = pRes->_43 = 0.0f;
	pRes->_44 = 1.0f;
}
inline void CreateViewMatrixRH( SHMatrix *pRes, const CVec3 &pos, const CQuat &rot )
{
	CVec3 vX, vY, vZ;
	rot.GetXAxis( &vX );
	rot.GetYAxis( &vY );
	rot.GetZAxis( &vZ );
	CreateViewMatrix( pRes, vX, -vY, -vZ, pos );
}
inline void CreateViewMatrixLH( SHMatrix *pRes, const CVec3 &pos, const CQuat &rot )
{
	CVec3 vX, vY, vZ;
	rot.GetXAxis( &vX );
	rot.GetYAxis( &vY );
	rot.GetZAxis( &vZ );
	CreateViewMatrix( pRes, vX, vY, vZ, pos );
}
inline void CreateViewMatrixRH( SHMatrix *pRes, const CVec3 &pos, const SHMatrix &rot )
{
	CVec3 vX, vY, vZ;
	rot.RotateVector( &vX, CVec3(1, 0, 0) );
	rot.RotateVector( &vY, CVec3(0, 1, 0) );
	rot.RotateVector( &vZ, CVec3(0, 0, 1) );
	CreateViewMatrix( pRes, vX, -vY, -vZ, pos );
}
inline void CreateViewMatrixLH( SHMatrix *pRes, const CVec3 &pos, const SHMatrix &rot )
{
	CVec3 vX, vY, vZ;
	rot.RotateVector( &vX, CVec3(1, 0, 0) );
	rot.RotateVector( &vY, CVec3(0, 1, 0) );
	rot.RotateVector( &vZ, CVec3(0, 0, 1) );
	CreateViewMatrix( pRes, vX, vY, vZ, pos );
}
inline void CreateViewMatrixRH( SHMatrix *pRes, const CVec3 &vFrom, const CVec3 &vTo, const CVec3 &vUp )
{
	CVec3 vX, vY = vUp, vZ = vTo - vFrom;
	Normalize( &vZ );
	vX = vY ^ vZ;
	Normalize( &vX );
	vY = vZ ^ vX;
	Normalize( &vY );
	CreateViewMatrix( pRes, vX, -vY, -vZ, vFrom );
}
inline void CreateViewMatrixLH( SHMatrix *pRes, const CVec3 &vFrom, const CVec3 &vTo, const CVec3 &vUp )
{
	CVec3 vX, vY = vUp, vZ = vTo - vFrom;
	Normalize( &vZ );
	vX = vY ^ vZ;
	Normalize( &vX );
	vY = vZ ^ vX;
	Normalize( &vY );
	CreateViewMatrix( pRes, vX, vY, vZ, vFrom );
}
const float FP_QUAT_EPSILON = 1e-04f;  // cutoff for sin(angle) near zero
inline void CQuat::FromAngleAxis( float fAngle, const CVec3 &vAxis, const bool bNormalizeAxis )
{
  fAngle *= 0.5f;
  const float fSinAlpha = bNormalizeAxis ? sin( fAngle ) / fabs( vAxis ) : sin( fAngle );
  x = vAxis.x * fSinAlpha;
  y = vAxis.y * fSinAlpha;
  z = vAxis.z * fSinAlpha;
  w = cos( fAngle );
}
inline void CQuat::FromAngleAxis( float fAngle, float fAxisX, float fAxisY, float fAxisZ, const bool bNormalizeAxis )
{
  fAngle *= 0.5f;
  const float fSinAlpha = bNormalizeAxis ? sin( fAngle ) / fabs( fAxisX, fAxisY, fAxisZ ) : sin( fAngle );
  x = fAxisX * fSinAlpha;
  y = fAxisY * fSinAlpha;
  z = fAxisZ * fSinAlpha;
  w = cos( fAngle );
}
inline void CQuat::FromEulerMatrix( const SHMatrix &m )
{
  float qs2 = 0.25f * (m.xx + m.yy + m.zz + 1);
  float qx2 = qs2 - 0.5f * (m.yy + m.zz);
  float qy2 = qs2 - 0.5f * (m.zz + m.xx);
  float qz2 = qs2 - 0.5f * (m.xx + m.yy);
  int n = (qs2 > qx2 ) ?
                        ((qs2 > qy2) ? ((qs2 > qz2) ? 0 : 3) : ((qy2 > qz2) ? 2 : 3)) :
                        ((qx2 > qy2) ? ((qx2 > qz2) ? 1 : 3) : ((qy2 > qz2) ? 2 : 3));
  float tmp;
  switch ( n )
  {
    case 0:
      w = static_cast<float>( sqrt( qs2 ) );
      tmp = 0.25f / w;
      x = ( m.zy - m.yz ) * tmp;
      y = ( m.xz - m.zx ) * tmp;
      z = ( m.yx - m.xy ) * tmp;
      break;
    case 1:
      x = static_cast<float>( sqrt( qx2 ) );
      tmp = 0.25f / x;
      w = ( m.zy - m.yz ) * tmp;
      y = ( m.xy + m.yx ) * tmp;
      z = ( m.xz + m.zx ) * tmp;
      break;
    case 2:
      y = static_cast<float>( sqrt( qy2 ) );
      tmp = 0.25f / y;
      w = ( m.xz - m.zx ) * tmp;
      z = ( m.yz + m.zy ) * tmp;
      x = ( m.yx + m.xy ) * tmp;
      break;
    case 3:
      z = static_cast<float>( sqrt( qz2 ) );
      tmp = 0.25f / z;
      w = ( m.yx - m.xy ) * tmp;
      x = ( m.zx + m.xz ) * tmp;
      y = ( m.zy + m.yz ) * tmp;
      break;
  }
  MinimizeRotationAngle();
  ::Normalize( x, y, z, w );
}
inline void CQuat::FromEulerAngles( float yaw, float pitch, float roll )
{
  const float fHalfYaw = yaw * 0.5f;
  const float fHalfPitch = pitch * 0.5f;
  const float fHalfRoll = roll * 0.5f;

  const float fCosYaw = cos( fHalfYaw );
  const float fSinYaw = sin( fHalfYaw );
  const float fCosPitch = cos( fHalfPitch );
  const float fSinPitch = sin( fHalfPitch );
  const float fCosRoll = cos( fHalfRoll );
  const float fSinRoll = sin( fHalfRoll );

  x = fSinRoll*fCosPitch*fCosYaw - fCosRoll*fSinPitch*fSinYaw;
  y = fCosRoll*fSinPitch*fCosYaw + fSinRoll*fCosPitch*fSinYaw;
  z = fCosRoll*fCosPitch*fSinYaw - fSinRoll*fSinPitch*fCosYaw;
  w = fCosRoll*fCosPitch*fCosYaw + fSinRoll*fSinPitch*fSinYaw;
}
inline CQuat::CQuat( float fAngle, float fAxisX, float fAxisY, float fAxisZ, const bool bNormalizeAxis )
{
	FromAngleAxis( fAngle, fAxisX, fAxisY, fAxisZ, bNormalizeAxis );
}
inline CQuat::CQuat( float fAngle, const CVec3 &vAxis, const bool bNormalizeAxis )
{
	FromAngleAxis( fAngle, vAxis, bNormalizeAxis );
}
inline void CQuat::Deriv( const CQuat &q, const CVec3 &v )
{
  x = 0.5f * (  q.w*v.x - q.z*v.y + q.y*v.z );
  y = 0.5f * (  q.z*v.x + q.w*v.y - q.x*v.z );
  z = 0.5f * ( -q.y*v.x + q.x*v.y + q.w*v.z );
  w = 0.5f * ( -q.x*v.x - q.y*v.y - q.z*v.z );
}
inline const CVec3 CQuat::GetXAxis() const
{
	return CVec3( w*w - (x*x + y*y + z*z) + 2.0f*x*x, (z*w + x*y)*2.0f, (-y*w + x*z)*2.0f );
}
inline const CVec3 CQuat::GetYAxis() const
{
	return CVec3( (-z*w + y*x)*2.0f, w*w - (x*x + y*y + z*z) + 2.0f*y*y, (x*w + y*z)*2.0f );
}
inline const CVec3 CQuat::GetZAxis() const
{
	return CVec3( (y*w + z*x)*2.0f, (-x*w + z*y)*2.0f, w*w - (x*x + y*y + z*z) + 2.0f*z*z );
}
inline void CQuat::GetXAxis( CVec3 *pRes ) const
{
	pRes->x = w*w - (x*x + y*y + z*z) + 2.0f*x*x;
	pRes->y = (z*w + x*y)*2.0f;
	pRes->z = (-y*w + x*z)*2.0f;
}
inline void CQuat::GetYAxis( CVec3 *pRes ) const
{
	pRes->x = (-z*w + y*x)*2.0f;
	pRes->y = w*w - (x*x + y*y + z*z) + 2.0f*y*y;
	pRes->z = (x*w + y*z)*2.0f;
}
inline void CQuat::GetZAxis( CVec3 *pRes ) const
{
	pRes->x = (y*w + z*x)*2.0f;
	pRes->y = (-x*w + z*y)*2.0f;
	pRes->z = w*w - (x*x + y*y + z*z) + 2.0f*z*z;
}
inline const CVec3 CQuat::Rotate( const CVec3 &r ) const
{
	const CVec3 L( x, y, z );
	return ( r*(w*w - L*L) + (2.0f*w)*(L^r) + (2.0f*(L*r))*L );
}
inline void CQuat::Rotate( CVec3 *pRes, const CVec3 &vec ) const
{
	const CVec3 L( x, y, z );
	*pRes = ( vec*(w*w - L*L) + (2.0f*w)*(L^vec) + (2.0f*(L*vec))*L );
}
inline const CQuat operator*( const CQuat &a, const CQuat &b )
{
	return CQuat( a.w*b.x + b.w*a.x + (a.y*b.z - a.z*b.y),
  							a.w*b.y + b.w*a.y + (a.z*b.x - a.x*b.z),
  							a.w*b.z + b.w*a.z + (a.x*b.y - a.y*b.x),
                a.w*b.w - (a.x*b.x + a.y*b.y + a.z*b.z), 0, 0 );
}
inline const CQuat operator/( const CQuat &a, const CQuat &b )
{
	CQuat q1;
	q1.UnitInverse( b );
	return q1 * a;
}
inline CQuat& CQuat::operator*=( const CQuat &a )
{
  float xtmp = a.w*x + w*a.x + (a.y*z - a.z*y);
  float ytmp = a.w*y + w*a.y + (a.z*x - a.x*z);
  float ztmp = a.w*z + w*a.z + (a.x*y - a.y*x);
  float wtmp = a.w*w - ( a.x*x + a.y*y + a.z*z );
  x = xtmp; y = ytmp; z = ztmp; w = wtmp;

  return *this;
}
inline CQuat& CQuat::operator/=( const CQuat &q )
{
	CQuat q1;
	q1.UnitInverse( q );
	(*this) *= q1;
  return *this;
}
inline const CQuat CQuat::operator*( const float c ) const
{
  return CQuat( c*x, c*y, c*z, c*w, 0, 0 );
}
inline bool CQuat::Inverse( const CQuat &q )
{
  float norm = fabs2( q.x, q.y, q.z, q.w );
  if ( norm > FP_EPSILON2 )
  {
    norm = 1.0f / static_cast<float>( sqrt( norm ) );
    x = -q.x * norm;
    y = -q.y * norm;
    z = -q.z * norm;
    w =  q.w * norm;
    return true;
  }
  else
    return false;
}
inline bool CQuat::Inverse()
{
  float norm = fabs2( x, y, z, w );
  if ( norm > FP_EPSILON2 )
  {
    norm = 1.0f / static_cast<float>( sqrt( norm ) );
    x *= -norm;
    y *= -norm;
    z *= -norm;
    w *=  norm;
    return true;
  }
  else
    return false;
}
inline void CQuat::DecompAngleAxis( float *pfAngle, CVec3 *pvAxis ) const
{
  float len = fabs2( x, y, z );
  if ( len > 1e-8f )
  {
    *pfAngle = 2.0f * acos( w );
    len = 1.0f / float( sqrt(len) );
    pvAxis->x = x * len;
    pvAxis->y = y * len;
    pvAxis->z = z * len;
  }
  else
  {
    *pfAngle = 0.0f;
    pvAxis->x = 1.0f;
    pvAxis->y = 0.0f;
    pvAxis->z = 0.0f;
  }
}
inline void CQuat::DecompAngleAxis( float *pfAngle, float *pfAxisX, float *pfAxisY, float *pfAxisZ ) const
{
  float len = x*x + y*y + z*z;
  if ( len > 1e-8f )
  {
    *pfAngle = 2.0f * acos( w );
    len = 1.0f / float( sqrt(len) );
    *pfAxisX = x * len;
    *pfAxisY = y * len;
    *pfAxisZ = z * len;
  }
  else
  {
    *pfAngle = 0.0f;
    *pfAxisX = 1.0f;
    *pfAxisY = 0.0f;
    *pfAxisZ = 0.0f;
  }
}
inline void CQuat::DecompEulerMatrix( SHMatrix *pRes ) const
{
  const float tx  = x + x;
  const float ty  = y + y;
  const float tz  = z + z;
  const float twx = tx*w;
  const float twy = ty*w;
  const float twz = tz*w;
  const float txx = tx*x;
  const float txy = ty*x;
  const float txz = tz*x;
  const float tyy = ty*y;
  const float tyz = tz*y;
  const float tzz = tz*z;

	pRes->_11 = 1.0f - (tyy + tzz);
	pRes->_12 = txy - twz;
	pRes->_13 = txz + twy;

	pRes->_21 = txy + twz;
	pRes->_22 = 1.0f - (txx + tzz);
	pRes->_23 = tyz - twx;

	pRes->_31 = txz - twy;
	pRes->_32 = tyz + twx;
	pRes->_33 = 1.0f - (txx + tyy);
}
inline void CQuat::DecompReversedEulerMatrix( SHMatrix *pRes ) const
{
  const float tx  = -( x + x );
  const float ty  = -( y + y );
  const float tz  = -( z + z );
  const float twx =  tx*w;
  const float twy =  ty*w;
  const float twz =  tz*w;
  const float txx = -tx*x;
  const float txy = -ty*x;
  const float txz = -tz*x;
  const float tyy = -ty*y;
  const float tyz = -tz*y;
  const float tzz = -tz*z;

	pRes->_11 = 1.0f - (tyy + tzz);
	pRes->_12 = txy - twz;
	pRes->_13 = txz + twy;

	pRes->_21 = txy + twz;
	pRes->_22 = 1.0f - (txx + tzz);
	pRes->_23 = tyz - twx;

	pRes->_31 = txz - twy;
	pRes->_32 = tyz + twx;
	pRes->_33 = 1.0f - (txx + tyy);
}
inline const CQuat CQuat::Exp() const
{
  const double angle = fabs( x, y, z );
  const double sn = sin( angle );
  CQuat result;

  result.w = float( cos(angle) );

  if ( fabs(sn) >= FP_QUAT_EPSILON )
  {
    const float coeff = float( sn / angle );
    result.x = coeff * x;
    result.y = coeff * y;
    result.z = coeff * z;
  }
  else
  {
    result.x = x;
    result.y = y;
    result.z = z;
  }

  return result;
}
inline const CQuat CQuat::Log() const
{
  if ( fabs(w) < 1.0f )
  {
    const double angle = acos( w );
    const double sn = sin( angle );
    if ( fabs(sn) >= FP_QUAT_EPSILON )
    {
      const float coeff = float( angle / sn );
      return CQuat( coeff*x, coeff*y, coeff*z, 0, 0, 0 );
    }
  }

  return CQuat( x, y, z, 0, 0, 0 );
}
inline void CQuat::Slerp( const float factor, const CQuat &p, const CQuat &q )
{
	float scale0, scale1;
  CQuat q1( q );
	float cosom = p.x*q.x + p.y*q.y + p.z*q.z + p.w*q.w;
  if ( cosom < 0.0 )
	{
		cosom = -cosom;
		q1.Negate( q );
  }
  if ( (1.0 - cosom) > FP_QUAT_EPSILON )  // standard case (slerp)
	{
    const float omega = acos( cosom );
    const float sinom = static_cast<float>( 1.0/sin( omega ) );
    scale0 = float( sin((1.0 - factor) * omega) * sinom );
    scale1 = float( sin(factor * omega) * sinom );
  }
	else                                  // "p" and "q" quaternions are very close. so we can do a linear interpolation
	{
    scale0 = 1.0f - factor;
    scale1 = factor;
  }
  x = scale0*p.x + scale1*q1.x;
  y = scale0*p.y + scale1*q1.y;
  z = scale0*p.z + scale1*q1.z;
  w = scale0*p.w + scale1*q1.w;
}
template <int nMaxNumMatrices>
inline void CMatrixStack<nMaxNumMatrices>::Set( const SHMatrix &matrix )
{
	nCurrentMatrix = 1;
	matrices[nCurrentMatrix] = matrix;
}
template <int nMaxNumMatrices>
inline void CMatrixStack<nMaxNumMatrices>::Push( const SHMatrix &matrix )
{
	NI_ASSERT_SLOW_T( nCurrentMatrix + 1 < nMaxNumMatrices + 1, "Can't push more matrices - matrix stack are full" );
	Multiply( &matrices[nCurrentMatrix + 1], matrices[nCurrentMatrix], matrix );
	nCurrentMatrix++;
}
template <int nMaxNumMatrices>
inline void CMatrixStack<nMaxNumMatrices>::Push43( const SHMatrix &matrix )
{
	NI_ASSERT_SLOW_T( nCurrentMatrix + 1 < nMaxNumMatrices + 1, "Can't push more matrices - matrix stack are full" );
	Multiply43( &matrices[nCurrentMatrix + 1], matrices[nCurrentMatrix], matrix );
	nCurrentMatrix++;
}
template <int nMaxNumMatrices>
inline void CMatrixStack<nMaxNumMatrices>::Push( const CVec3 &pos )
{
	NI_ASSERT_SLOW_T( nCurrentMatrix + 1 < nMaxNumMatrices + 1, "Can't push more matrices - matrix stack are full" );
	MultiplyTranslate( &matrices[nCurrentMatrix + 1], matrices[nCurrentMatrix], pos );
	nCurrentMatrix++;
}
template <int nMaxNumMatrices>
inline void CMatrixStack<nMaxNumMatrices>::Push( const CQuat &rot )
{
	Push43( SHMatrix(rot) );
}
template <int nMaxNumMatrices>
inline void CMatrixStack<nMaxNumMatrices>::Push( const CVec3 &pos, const CQuat &rot )
{
	SHMatrix matrix( pos, rot );
	Push43( matrix );
}
template <int nMaxNumMatrices>
inline void CMatrixStack<nMaxNumMatrices>::Push( float x, float y, float z )
{
	NI_ASSERT_SLOW_T( nCurrentMatrix + 1 < nMaxNumMatrices + 1, "Can't push more matrices - matrix stack are full" );
	MultiplyScale( &matrices[nCurrentMatrix + 1], matrices[nCurrentMatrix], x, y, z );
	nCurrentMatrix++;
}
template <int nMaxNumMatrices>
inline void CMatrixStack<nMaxNumMatrices>::Push( float val )
{
	NI_ASSERT_SLOW_T( nCurrentMatrix + 1 < nMaxNumMatrices + 1, "Can't push more matrices - matrix stack are full" );
	MultiplyScale( &matrices[nCurrentMatrix + 1], matrices[nCurrentMatrix], val, val, val );
	nCurrentMatrix++;
}

/* ===========================================================================
**                            GPoint
** =========================================================================*/
using GSPos = signed int;

struct SPoint
{
  GSPos x, y;
};

struct  GPoint : public SPoint
{

  GPoint();
  GPoint( const SPoint& );
  GPoint( GSPos x, GSPos y );


  void set( GSPos x, GSPos y );


  GPoint  operator -  () const;
  void    operator += ( const GPoint& p );
  void    operator -= ( const GPoint& p );
  void    operator += ( const GSPos n );
  void    operator -= ( const GSPos n );
  void    operator *= ( const GSPos n );
  void    operator /= ( const GSPos n );


  GPoint operator + ( const GPoint& p2 ) const;
  GPoint operator - ( const GPoint& p2 ) const;
  GPoint operator + ( const GSPos   n  ) const;
  GPoint operator - ( const GSPos   n  ) const;
  GPoint operator * ( const GSPos   n  ) const;
  GPoint operator / ( const GSPos   n  ) const;


  bool operator == ( const GPoint& p2 ) const;
  bool operator != ( const GPoint& p2 ) const;
  bool operator <  ( const GPoint& p2 ) const;
  bool operator <= ( const GPoint& p2 ) const;
  bool operator >  ( const GPoint& p2 ) const;
  bool operator >= ( const GPoint& p2 ) const;


  GPoint( const POINT& pt );
  GPoint( const SIZE& pt );
  operator POINT() const;
  operator SIZE() const;
};


GPoint operator + ( const GSPos   n,  const GPoint& p );
GPoint operator - ( const GSPos   n,  const GPoint& p );
GPoint operator * ( const GSPos   n,  const GPoint& p );




inline GPoint::GPoint() { x = y = 0; }
inline GPoint::GPoint( const SPoint& pt ) { x = pt.x; y = pt.y; }
inline GPoint::GPoint( GSPos X, GSPos Y ) { x = X; y = Y; }

inline void GPoint::set( GSPos new_x, GSPos new_y )
  { x = new_x;  y = new_y; }

inline GPoint GPoint::operator - () const
  { return GPoint((GSPos) -x,(GSPos) -y); }

inline void GPoint::operator += ( const GPoint& p )
  { x+=p.x; y+=p.y; }

inline void GPoint::operator -= ( const GPoint& p )
  { x-=p.x; y-=p.y; }

inline void GPoint::operator += ( const GSPos n ) { x+=n; y+=n; }
inline void GPoint::operator -= ( const GSPos n ) { x-=n; y-=n; }
inline void GPoint::operator *= ( const GSPos n ) { x*=n; y*=n; }
inline void GPoint::operator /= ( const GSPos n ) { x/=n; y/=n; }

inline GPoint GPoint::operator - ( const GPoint& p2 ) const
  { GPoint t(*this);  t -= p2;  return t;  }

inline GPoint GPoint::operator + ( const GPoint& p2 ) const
  { GPoint t(*this);  t += p2;  return t;  }

inline GPoint GPoint::operator + ( const GSPos n ) const
  { return GPoint( (GSPos)(x+n), (GSPos)(y+n) ); }

inline GPoint operator + ( const GSPos n, const GPoint& p ) { return p+n; }

inline GPoint GPoint::operator - ( const GSPos n ) const
  { return GPoint( (GSPos)(x-n), (GSPos)(y-n) ); }

inline GPoint operator - ( const GSPos n, const GPoint& p )
  { return GPoint( (GSPos)(n-p.x), (GSPos)(n-p.y) ); }

inline GPoint GPoint::operator * ( const GSPos n ) const
  { return GPoint( (GSPos)(x*n), (GSPos)(y*n) ); }

inline GPoint operator * ( const GSPos n, const GPoint& p ) { return p*n; }

inline GPoint GPoint::operator / ( const GSPos n ) const
  { return GPoint( (GSPos)(x/n),  (GSPos)(y/n) ); }

inline bool GPoint::operator == ( const GPoint& p2 ) const
  { return x == p2.x && y == p2.y; }

inline bool GPoint::operator != ( const GPoint& p2 ) const
  { return !(*this == p2); }

inline bool GPoint::operator <  ( const GPoint& p2 ) const
  { return x < p2.x || (x == p2.x && y < p2.y); }

inline bool GPoint::operator >  ( const GPoint& p2 ) const
  { return x > p2.x || (x == p2.x && y > p2.y); }

inline bool GPoint::operator <= ( const GPoint& p2 ) const
  { return !(*this > p2); }

inline bool GPoint::operator >= ( const GPoint& p2 ) const
  { return !(*this < p2); }




inline GPoint::GPoint( const POINT& pt ) { x = (GSPos)pt.x; y = (GSPos)pt.y; }
inline GPoint::GPoint( const SIZE&  pt ) { x = (GSPos)pt.cx; y = (GSPos)pt.cy; }
inline GPoint::operator POINT() const
  { POINT pt;  pt.x = x; pt.y = y;  return pt; }
inline GPoint::operator SIZE() const
  { SIZE pt;  pt.cx = x; pt.cy = y;  return pt; }


/* ===========================================================================
**                            GRect
** =========================================================================*/

class  GRect
{
 public:

  GPoint origin, size;



  GRect();
  GRect( GSPos x1, GSPos y1, GSPos x2, GSPos y2 ); // left-top, right-bottom
  GRect( const GPoint& p0, const GPoint& p1 );     // opposite corners



  void set( GSPos x1, GSPos y1, GSPos x2, GSPos y2 ); // left-top, right-bottom
  void set( GPoint p0, GPoint p1 );                   // opposite corners

  void Empty();                                       // make rectangle empty



  GSPos         left        () const;                 // getters
  GSPos         top         () const;
  GSPos         right       () const;
  GSPos         bottom      () const;

  GSPos         width       () const;
  GSPos         height      () const;

  void          left        ( GSPos );                // setters -
  void          top         ( GSPos );                // another margins are
  void          right       ( GSPos );                // not changed
  void          bottom      ( GSPos );

  void          width       ( GSPos );
  void          height      ( GSPos );



  const GPoint& left_top    () const;
  GPoint        left_bottom () const;
  GPoint        right_bottom() const;
  GPoint        right_top   () const;
  GPoint        center      () const;
  const GPoint& Size        () const;

  void          left_top    ( const GPoint& );
  void          left_bottom ( const GPoint& );
  void          right_bottom( const GPoint& );
  void          right_top   ( const GPoint& );
  void          center      ( const GPoint& );
  void          Size        ( const GPoint& );



  bool          isEmpty     () const;
  bool          contains    ( const GPoint& p ) const;
  bool          contains    ( GSPos x, GSPos y ) const;
  bool          contains    ( const GRect& r ) const;



  void          move         ( GSPos deltaX, GSPos deltaY );
  void          grow         ( GSPos deltaX, GSPos deltaY );
  void          intersect    ( const GRect& r );
  void          Union        ( const GRect& r );

  void          move_origin_x( GSPos new_pos );
  void          move_origin_y( GSPos new_pos );
  void          move_origin  ( GSPos new_x, GSPos new_y );
  void          move_origin  ( const GPoint& new_pos );

  void          split_vertically  ( GSPos  shift_from_left,
                                    GRect& left,
                                    GRect& right
                                  ) const;

  void          split_horizontally( GSPos  shift_from_top,
                                    GRect& top,
                                    GRect& bottom
                                  ) const;



  void operator += ( const GSPos   p );
  void operator -= ( const GSPos   p );
  void operator *= ( const GSPos   p );
  void operator /= ( const GSPos   p );
  void operator += ( const GPoint& p );
  void operator -= ( const GPoint& p );
  void operator &= ( const GRect&  r );       // intersection
  void operator |= ( const GRect&  r );       // union

  bool operator == ( const GRect& ) const;
  bool operator != ( const GRect& ) const;

  GRect operator + ( const GPoint& ) const;
  GRect operator - ( const GPoint& ) const;
  GRect operator * ( const GSPos   ) const;
  GRect operator / ( const GSPos   ) const;
  GRect operator | ( const GRect&  ) const;
  GRect operator & ( const GRect&  ) const;



  bool clip_line( GSPos& x0, GSPos& y0, GSPos& x1, GSPos& y1 ) const;

  bool clip_hline( GSPos& x0, GSPos y0, GSPos& x1 ) const;

  bool clip_vline( GSPos x0, GSPos& y0, GSPos& y1 ) const;



  GRect( const RECT& r );
  operator RECT() const;

  GPoint a() const;
  GPoint b() const;

  GSPos a_x() const;
  GSPos a_y() const;
  GSPos b_x() const;
  GSPos b_y() const;

  void move_left  ( GSPos new_x );
  void move_right ( GSPos new_x );
  void move_top   ( GSPos new_y );
  void move_bottom( GSPos new_y );

  int in( GSPos x, GSPos y ) const;
  int in( const GPoint& p ) const;

};


GRect operator + ( const GPoint& , const GRect&  );
GRect operator - ( const GPoint& , const GRect&  );
GRect operator * ( const GSPos   , const GRect&  );



inline void GRect::set( GSPos x1, GSPos y1, GSPos x2, GSPos y2 )
  { origin.x = x1;  origin.y = y1;  size.x = (GSPos)(x2-x1+1);  size.y = (GSPos)(y2-y1+1); }

inline GRect::GRect() {}
inline GRect::GRect( GSPos x1, GSPos y1, GSPos x2, GSPos y2 ) { set(x1,y1,x2,y2); }
inline GRect::GRect( const GPoint& p0, const GPoint& p1 ) { set(p0,p1); }

inline void GRect::Empty() { size.x = size.y = 0; }

inline GSPos GRect::left  () const { return origin.x; }
inline GSPos GRect::top   () const { return origin.y; }
inline GSPos GRect::right () const { return (GSPos)(origin.x+size.x-1); }
inline GSPos GRect::bottom() const { return (GSPos)(origin.y+size.y-1); }
inline GSPos GRect::width () const { return size.x; }
inline GSPos GRect::height() const { return size.y; }

inline void  GRect::right ( GSPos n ) { size.x = (GSPos)(n - origin.x + 1); }
inline void  GRect::bottom( GSPos n ) { size.y = (GSPos)(n - origin.y + 1); }
inline void  GRect::width ( GSPos n ) { size.x = n; }
inline void  GRect::height( GSPos n ) { size.y = n; }

inline const GPoint& GRect::left_top    () const { return origin; }
inline GPoint        GRect::left_bottom () const
  { return GPoint( left(), bottom() ); }

inline GPoint        GRect::right_bottom() const
  { return GPoint( right(), bottom() ); }

inline GPoint        GRect::right_top   () const
  { return GPoint( right(), top() ); }

inline GPoint        GRect::center      () const
  { return origin + size/2; }

inline const GPoint& GRect::Size() const { return size; }

inline void GRect::left_top    ( const GPoint& p ) { left(p.x);  top(p.y); }
inline void GRect::left_bottom ( const GPoint& p ) { left(p.x);  bottom(p.y); }
inline void GRect::right_bottom( const GPoint& p ) { right(p.x); bottom(p.y); }
inline void GRect::right_top   ( const GPoint& p ) { right(p.x); top(p.y); }
inline void GRect::Size        ( const GPoint& p ) { size = p; }

inline void GRect::move( GSPos dx, GSPos dy )     { origin.x += dx;  origin.y += dy; }
inline void GRect::move_origin_x( GSPos new_pos ) { origin.x = new_pos; }
inline void GRect::move_origin_y( GSPos new_pos ) { origin.y = new_pos; }
inline void GRect::move_origin  ( GSPos new_x, GSPos new_y )
  { move_origin_x(new_x);  move_origin_y(new_y); }

inline void GRect::move_origin  ( const GPoint& new_pos )
  { move_origin( new_pos.x, new_pos.y ); }

inline bool GRect::isEmpty() const { return size.x <= 0 || size.y <= 0; }
inline bool GRect::contains( const GPoint& p ) const { return contains(p.x,p.y); }

inline void GRect::operator += ( const GSPos   p ) { origin += p; }
inline void GRect::operator -= ( const GSPos   p ) { origin -= p; }
inline void GRect::operator *= ( const GSPos   p ) { origin *= p; size *= p; }
inline void GRect::operator /= ( const GSPos   p ) { origin /= p; size /= p; }
inline void GRect::operator += ( const GPoint& p ) { origin += p; }
inline void GRect::operator -= ( const GPoint& p ) { origin -= p; }
inline void GRect::operator &= ( const GRect&  r ) { intersect(r); }
inline void GRect::operator |= ( const GRect&  r ) { Union(r); }

inline bool GRect::operator == ( const GRect& r2 ) const
  { return origin == r2.origin && size == r2.size; }

inline bool GRect::operator != ( const GRect& r2 ) const
  { return !(*this==r2); }

inline GRect GRect::operator + ( const GPoint& p ) const
  { GRect t( *this );  t += p;  return t; }

inline GRect operator + ( const GPoint& p, const GRect& r ) { return r+p; }

inline GRect GRect::operator - ( const GPoint& p ) const
  { GRect t( *this );  t -= p;  return t; }

inline GRect operator - ( const GPoint& p, const GRect& r ) { return r-p; }

inline GRect GRect::operator * ( const GSPos p ) const
  { GRect t( *this );  t *= p;  return t; }

inline GRect operator * ( const GSPos   p, const GRect& r ) { return r*p; }

inline GRect GRect::operator / ( const GSPos p ) const
  { GRect t( *this );  t /= p;  return t; }

inline GRect GRect::operator | ( const GRect& r2 ) const
  { GRect t( *this ); t |= r2; return t; }

inline GRect GRect::operator & ( const GRect& r2 ) const
  { GRect t( *this ); t &= r2; return t; }

inline GPoint GRect::a() const { return left_top(); }
inline GPoint GRect::b() const { return right_bottom(); }

inline GSPos GRect::a_x() const { return left(); }
inline GSPos GRect::a_y() const { return top(); }
inline GSPos GRect::b_x() const { return right(); }
inline GSPos GRect::b_y() const { return bottom(); }

inline void GRect::move_left  ( GSPos n ) { left(n); }
inline void GRect::move_right ( GSPos n ) { right(n); }
inline void GRect::move_top   ( GSPos n ) { top(n); }
inline void GRect::move_bottom( GSPos n ) { bottom(n); }

inline int GRect::in( GSPos x, GSPos y ) const { return contains(x,y); }
inline int GRect::in( const GPoint& p  ) const { return contains(p); }
inline void GetCirclesByTangent( const CVec2 &tang, const CVec2 &p, const float r, CCircle *c1, CCircle *c2 )
{
	const CVec2 v( -tang.y, tang.x );

	c1->r = r;
	c1->center = p + v * r;

	c2->r = r;
	c2->center = p - v * r;
}
inline bool FindTangentPoints( const CVec2 &p, const CCircle &c, CVec2 *p1, CVec2 *p2 )
{
	const CVec2 v = c.center - p;
	const float hyp2 = fabs2( v );
	const float r2 = sqr( c.r );

	if ( hyp2 < r2 )
		return false;

	if ( hyp2 == r2 )
	{
		*p1 = p;
		*p2 = p;
	}
	else
	{
		const float leg2 = hyp2 - r2;
		const float cossin = float( sqrt( leg2 ) ) * c.r / hyp2;
		const float cos2 = leg2 / hyp2;

		p1->x = v.x * cos2 - v.y * cossin + p.x;
		p1->y = v.x * cossin + v.y * cos2 + p.y;

		p2->x = p1->x + 2 * v.y * cossin;
		p2->y = p1->y - 2 * v.x * cossin;
	}

	return true;
}
inline void CLine2::ProjectPoint( const CVec2 &point, CVec2 *result )
{
	float fK;
	if ( bNormalized )
		fK = a * point.x + b * point.y + c;
	else
		fK = ( a * point.x + b * point.y + c ) / fabs2( a, b );

	result->x = point.x - fK * a;
	result->y = point.y - fK * b;
}
inline float CLine2::DistToPoint( const CVec2 &point )
{
	Normalize();
	return ( a * point.x + b * point.y + c );
}
inline float STriangle( const CVec2 &p1, const CVec2 &p2, const CVec2 &p3 )
{
	return p1.x * ( p2.y - p3.y ) + p2.x * ( p3.y - p1.y ) + p3.x * ( p1.y - p2.y );
}
inline const float CSegment::GetDistToPoint( const CVec2 &point ) const
{
	if ( ( point - p1 ) * dir <= 0 )
		return fabs( point - p1 );
	else if ( ( point - p2 ) * dir >= 0 )
		return fabs( point - p2 );
	else
		return fabs( STriangle( p1, p2, point ) ) / fabs( p2 - p1 );
}
inline void CSegment::GetClosestPoint( const CVec2 &point, CVec2 *result ) const
{
	if ( ( point - p1 ) * dir <= 0 )
		*result = p1;
	else if ( ( point - p2 ) * dir >= 0 )
		*result = p2;
	else
		CLine2( p1, p2 ).ProjectPoint( point, result );
}
#endif // __GEOMETRY_H__
