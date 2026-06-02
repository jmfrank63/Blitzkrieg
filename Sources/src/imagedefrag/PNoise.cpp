#include "StdAfx.h"
#include "PNoise.h"
#include "..\streamIO\RandomGen.h"
namespace NPerlinNoise
{
const int B = 256;
static bool isEmpty = true;

static int perm[B + B + 2];
static CVec3 grad[B + B + 2];
void Init()
{
  int   i, j, k;
  float s;
  CVec3 v;
  
  for( i = 0 ; i < B; ++i ) 
  {
    do 
    {                            // Choose uniformly in a cube
      for ( j = 0; j < 3; ++j )
        v[j] = Random( -1.0f, 1.0f );
      s = fabs2( v );
    } while (s > 1.0 || s < FP_EPSILON);              // If not in sphere try again
    Normalize( &v );
    grad[i] = v;
  }
  
  
  for( i = 0; i < B; ++i )
    perm[i] = i;
  for( i = B - 1; i > 0; i -= 2 ) 
  {
    k = perm[i];
    perm[i] = perm[j = Random( B )];
    perm[j] = k;
  }
  
  for( i = 0 ; i < B + 2; ++i )
  {
    perm[B + i] = perm[i];
    grad[B + i] = grad[i];
  }
}
inline void Setup( float val, int &b0, int &b1, float &r0, float &r1 )
{
  float t = val + 10000.0f;

  b0 = ((int)t) & (B - 1);
  b1 = (b0 + 1) & (B - 1);
  r0 = t - (int)t;
  r1 = r0 - 1.0f;
}
inline float At( const CVec3 &q, float rx, float ry, float rz ) 
{
  return rx * q.x + ry * q.y + rz * q.z;
}
inline float Scurve( float t )
{
  return t * t * (3.0f - 2.0f * t);
}
inline float Lerp( float t, float a, float b )
{
  return a + t * (b - a);
}
float Noise3( const CVec3 &rVec3 )
{
  int   bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
  float rx0, rx1, ry0, ry1, rz0, rz1, sx, sy, sz, a, b, c, d, u, v;
  int   i, j;

  if ( NPerlinNoise::isEmpty )
  {
		NPerlinNoise::Init();
    NPerlinNoise::isEmpty = false;
  }

  Setup( rVec3.x, bx0,bx1, rx0,rx1 );
  Setup( rVec3.y, by0,by1, ry0,ry1 );
  Setup( rVec3.z, bz0,bz1, rz0,rz1 );

  i = perm[ bx0 ];
  j = perm[ bx1 ];

  b00 = perm[ i + by0 ];
  b10 = perm[ j + by0 ];
  b01 = perm[ i + by1 ];
  b11 = perm[ j + by1 ];

  sx = Scurve( rx0 );
  sy = Scurve( ry0 );
  sz = Scurve( rz0 );


  u = At( grad[ b00 + bz0 ], rx0,ry0,rz0 );
  v = At( grad[ b10 + bz0 ], rx1,ry0,rz0 );
  a = Lerp( sx, u, v );

  u = At( grad[ b01 + bz0 ], rx0,ry1,rz0 );
  v = At( grad[ b11 + bz0 ], rx1,ry1,rz0 );
  b = Lerp( sx, u, v );

  c = Lerp( sy, a, b );          // interpolAte in y At lo x

  u = At( grad[ b00 + bz1 ], rx0,ry0,rz1 );
  v = At( grad[ b10 + bz1 ], rx1,ry0,rz1 );
  a = Lerp( sx, u, v );
  
  u = At( grad[ b01 + bz1 ], rx0,ry1,rz1 );
  v = At( grad[ b11 + bz1 ], rx1,ry1,rz1);
  b = Lerp( sx, u, v );
  
  d = Lerp( sy, a, b );          // interpolAte in y At hi x

  return 1.5 * Lerp( sz, c, d ); // interpolate in z
}
};
