#include "StdAfx.h"
#include "BetaSpline.h"


void CBetaSpline::Init( float _fBeta1, float _fBeta2 )
{
  fBeta1 = _fBeta1;
  fBeta2 = _fBeta2;
  invdelta = 1.0f / ( 2.0f * fBeta1 * fBeta1 * fBeta1
        + 4.0f * fBeta1 * fBeta1
        + 4.0f * fBeta1 + fBeta2 + 2.0f );

  VolumeCoeffs( _fBeta1, _fBeta2 );
  fBeta1_3 = 2 * fBeta1 * fBeta1 * fBeta1 * invdelta;
  fBeta1_2 = 2 * fBeta1 * fBeta1 * invdelta;

  fBeta1 *= 2 * invdelta;
  fBeta2 *= invdelta;
}
inline float CBetaSpline::b_2( const float t[3] ) const
{
  return fBeta1_3 * square( 1 - t[0] ) * ( 1 - t[0] );
}
inline float CBetaSpline::b_1( const float t[3] ) const
{
  return fBeta1_3 * t[0] * ( t[1] - 3*t[0] + 3 ) + 
         fBeta1_2 * ( t[2] - 3*t[1] + 2 ) + 
         fBeta1 * ( t[2] - 3*t[0] + 2 ) + 
         fBeta2 * ( 2 * t[2] - 3 * t[1] + 1);
}
inline float CBetaSpline::b0( const float t[3] ) const
{
  return fBeta1_2 * t[1] * ( 3 - t[0] ) + fBeta1 * t[0] * ( 3 - t[1] ) + fBeta2 * t[1] * ( 3 - 2*t[0] ) + invdelta * 2 * ( 1 - t[2] );
}
inline float CBetaSpline::b1( const float t[3] ) const
{
  return 2 * t[2] * invdelta;
}
inline float CBetaSpline::db_2( const float t[3] ) const
{
  return -3.0f * fBeta1_3 * square( 1 - t[0] );
}
inline float CBetaSpline::db_1( const float t[3] ) const
{
  return 3.0f * (fBeta1_3 * square( t[0] - 1 ) + fBeta1_2 * (t[0] - 2) * t[0] 
                  + fBeta1 * (t[1] - 1) + fBeta2 * 2 * (t[0] - 1) * t[0]);
}
inline float CBetaSpline::db0( const float t[3] ) const
{
  return 3.0f * (fBeta1 + (fBeta1_2 * 2 + fBeta2 * 2) * t[0] - (invdelta * 2 + fBeta1 + fBeta1_2 + fBeta2 * 2) * t[1] );
}
inline float CBetaSpline::db1( const float t[3] ) const
{
  return 6 * t[1] * invdelta;
}
CVec3 CBetaSpline::Value( float u, float v, const CVec3 ptCtrls[16] ) const
{
  float t[3];
  t[0] = u;
  t[1] = u * u;
  t[2] = t[1] * u;
  float bj[4] = { b_2( t ), b_1( t ), b0( t ), b1( t ) };

  t[0] = v;
  t[1] = v * v;
  t[2] = t[1] * v;
  float bl[4] = { b_2( t ), b_1( t ), b0( t ), b1( t ) };

  CVec3 pt =  bj[0] * ( bl[0] * ptCtrls[0] + bl[1] * ptCtrls[4] + bl[2] * ptCtrls[8] + bl[3] * ptCtrls[12] );
  pt += bj[1] * ( bl[0] * ptCtrls[1] + bl[1] * ptCtrls[5] + bl[2] * ptCtrls[9]  + bl[3] * ptCtrls[13] );
  pt += bj[2] * ( bl[0] * ptCtrls[2] + bl[1] * ptCtrls[6] + bl[2] * ptCtrls[10] + bl[3] * ptCtrls[14] );
  pt += bj[3] * ( bl[0] * ptCtrls[3] + bl[1] * ptCtrls[7] + bl[2] * ptCtrls[11] + bl[3] * ptCtrls[15] );

  return pt;
}
void CBetaSpline::Derivative( CVec3 &ptDU, CVec3 &ptDV, float u, float v, const CVec3 ptCtrls[16] ) const
{
  float t[3];
  t[0] = u;
  t[1] = u * u;
  t[2] = t[1] * u;
  float bj[4]  = { b_2( t ), b_1( t ), b0( t ), b1( t ) };
  float dbj[4] = { db_2( t ), db_1( t ), db0( t ), db1( t ) };

  t[0] = v;
  t[1] = v * v;
  t[2] = t[1] * v;
  float bl[4] = { b_2( t ), b_1( t ), b0( t ), b1( t ) };
  float dbl[4] = { db_2( t ), db_1( t ), db0( t ), db1( t ) };

  ptDU  = dbj[0] * ( bl[0] * ptCtrls[0] + bl[1] * ptCtrls[4] + bl[2] * ptCtrls[8] + bl[3] * ptCtrls[12] );
  ptDU += dbj[1] * ( bl[0] * ptCtrls[1] + bl[1] * ptCtrls[5] + bl[2] * ptCtrls[9]  + bl[3] * ptCtrls[13] );
  ptDU += dbj[2] * ( bl[0] * ptCtrls[2] + bl[1] * ptCtrls[6] + bl[2] * ptCtrls[10] + bl[3] * ptCtrls[14] );
  ptDU += dbj[3] * ( bl[0] * ptCtrls[3] + bl[1] * ptCtrls[7] + bl[2] * ptCtrls[11] + bl[3] * ptCtrls[15] );

  ptDV  = bj[0] * ( dbl[0] * ptCtrls[0] + dbl[1] * ptCtrls[4] + dbl[2] * ptCtrls[8] + dbl[3] * ptCtrls[12] );
  ptDV += bj[1] * ( dbl[0] * ptCtrls[1] + dbl[1] * ptCtrls[5] + dbl[2] * ptCtrls[9]  + dbl[3] * ptCtrls[13] );
  ptDV += bj[2] * ( dbl[0] * ptCtrls[2] + dbl[1] * ptCtrls[6] + dbl[2] * ptCtrls[10] + dbl[3] * ptCtrls[14] );
  ptDV += bj[3] * ( dbl[0] * ptCtrls[3] + dbl[1] * ptCtrls[7] + dbl[2] * ptCtrls[11] + dbl[3] * ptCtrls[15] );
}
void CBetaSpline::VolumeCoeffs( float b1, float b2 )
{
  float d = square( invdelta ) / 4.0f;
  float b12 = b1  * b1;
  float b13 = b12 * b1;
  float b14 = b13 * b1;
  float b15 = b14 * b1;
  float b16 = b15 * b1;

  fVolCoeffs[0] = d * b16;
  fVolCoeffs[1] = d * ( 3*b16 + 5*b15 + 3*b14 + b2*b13 );
  fVolCoeffs[2] = d * ( 3*b15 + 5*b14 + b2*b13 + 3*b13 );
  fVolCoeffs[3] = d * b13;

  fVolCoeffs[4 + 0] = fVolCoeffs[1];
  fVolCoeffs[4 + 1] = d * ( 9*b16 + 30*b15 + 43*b14 + 6*b2*b13 + 30*b13 + 10*b2*b12 + 9*b12 + 6*b2*b1 + b2*b2 );
  fVolCoeffs[4 + 2] = d * ( 9*b15 + 30*b14 + 3*b2*b13 + 43*b13 + 8*b2*b12 + 30*b12 + 8*b2*b1 + 9*b1 + b2*b2 + 3*b2 );
  fVolCoeffs[4 + 3] = d * ( 3*b13 + 5*b12 + 3*b1 + b2 );

  fVolCoeffs[8 + 0] = fVolCoeffs[2];
  fVolCoeffs[8 + 1] = fVolCoeffs[4 + 2];
  fVolCoeffs[8 + 2] = d * ( 9*b14 + 30*b13 + 6*b2*b12 + 43*b12 + 10*b2*b1 + 30*b1 + b2*b2 + 6*b2 + 9 );
  fVolCoeffs[8 + 3] = d * ( 3*b12 + 5*b1 + b2 + 3 );

  fVolCoeffs[12 + 0] = fVolCoeffs[3];
  fVolCoeffs[12 + 1] = fVolCoeffs[4 + 3];
  fVolCoeffs[12 + 2] = fVolCoeffs[8 + 3];
  fVolCoeffs[12 + 3] = d;
}
float CBetaSpline::Ave( const float ptControls[16] ) const
{
  float ret = 0;
  for( int i = 0; i < 16; ++i )
    ret += fVolCoeffs[i] * ptControls[i];
  return ret;
}
float CBetaSpline::Ave( float u0, float u1, float v0, float v1, const float ptControls[16] ) const
{
  float u[2][4];
  float v[2][4];

  u[0][0] = u0;
  u[0][1] = u[0][0] * u0;
  u[0][2] = u[0][1] * u0;
  u[0][3] = u[0][2] * u0;
  u[1][0] = u1;
  u[1][1] = u[1][0] * u1;
  u[1][2] = u[1][1] * u1;
  u[1][3] = u[1][2] * u1;

  v[0][0] = v0;
  v[0][1] = v[0][0] * v0;
  v[0][2] = v[0][1] * v0;
  v[0][3] = v[0][2] * v0;
  v[1][0] = v1;
  v[1][1] = v[1][0] * v1;
  v[1][2] = v[1][1] * v1;
  v[1][3] = v[1][2] * v1;

  const float d = 1.0f / 169;
  float ret = 0.25f * d * ptControls[0] * F00( u ) * F00( v );
  ret -= 0.5f * d * ptControls[1] * F01( u, v );
  ret += 0.5f * d * ptControls[2] * F02( u, v );
  ret -= 0.25f * d * ptControls[3] * F03( u, v );

  ret -= 0.5f * d * ptControls[4] * F01( v, u );
  ret += d * ptControls[5] * F11( v ) * F11( u );
  ret -= d * ptControls[6] * F12( u, v );
  ret += 0.5f * d * ptControls[7] * F13( u, v );

  ret += 0.5f * d * ptControls[8] * F02( v, u );
  ret -= d * ptControls[9] * F12( v, u );
  ret += d * ptControls[10] * F22( u ) * F22( v );
  ret -= 0.5f * d * ptControls[11] * F23( u, v );

  ret -= 0.25f * d * ptControls[12] * F03( v, u );
  ret += 0.5f * d * ptControls[13] * F13( v, u );
  ret -= 0.5f * d * ptControls[14] * F23( v, u );
  ret += 0.25f * d * ptControls[15] * F33( u ) * F33( v );

  return ret;
}
inline float CBetaSpline::F00( const float t[2][4] ) const
{
  return -4*t[0][0] + 6*t[0][1] - 4*t[0][2] + t[0][3] - t[1][0]*(-4 + 6*t[1][0] - 4*t[1][1] + t[1][2]);
}
inline float CBetaSpline::F11( const float t[2][4] ) const
{
  return 9*t[0][0] - 5*t[0][2] + 2*t[0][3] - 9*t[1][0] + 5*t[1][2] - 2*t[1][3];
}
inline float CBetaSpline::F22( const float t[2][4] ) const
{
  return 2*t[0][3] - 3*t[0][2] - 3*t[0][1] - 2*t[0][0] + t[1][0]*(-2*t[1][2] + 3*t[1][1] + 3*t[1][0] + 2);
}
inline float CBetaSpline::F33( const float t[2][4] ) const
{
  return t[0][3] - t[1][3];
}
inline float CBetaSpline::F01( const float s[2][4], const float t[2][4] ) const
{
  return (9*s[0][0] - 5*s[0][2] + 2*s[0][3] - 9*s[1][0] + 5*s[1][2] - 2*s[1][3]) * 
    (-4*t[0][0] + 6*t[0][1] - 4*t[0][2] + t[0][3] - t[1][0] * (-4 + 6*t[1][0] - 4*t[1][1] + t[1][2]) );
}
inline float CBetaSpline::F02( const float s[2][4], const float t[2][4] ) const
{
  return (-2*s[0][0] - 3*s[0][1] - 3*s[0][2] + 2*s[0][3] + s[1][0] * (2 + 3*s[1][0] + 3*s[1][1] - 2*s[1][2])) * 
    (-4*t[0][0] + 6*t[0][1] - 4*t[0][2] + t[0][3] - t[1][0] * (-4 + 6*t[1][0] - 4*t[1][1] + t[1][2]));
}
inline float CBetaSpline::F03( const float s[2][4], const float t[2][4] ) const
{
  return (s[0][3] - s[1][3]) * 
    (-4*t[0][0] + 6*t[0][1] - 4*t[0][2] + t[0][3] - t[1][0]*(-4 + 6*t[1][0] - 4*t[1][1] + t[1][2]));
}
inline float CBetaSpline::F12( const float s[2][4], const float t[2][4] ) const
{
   return (-2*s[0][0] - 3*s[0][1] - 3*s[0][2] + 2*s[0][3] + s[1][0] * (2 + 3*s[1][0] + 3*s[1][1] - 2*s[1][2])) 
     * (9*t[0][0] - 5*t[0][2] + 2*t[0][3] - 9*t[1][0] + 5*t[1][2] - 2*t[1][3]);
}
inline float CBetaSpline::F13( const float s[2][4], const float t[2][4] ) const
{
  return (s[0][3] - s[1][3]) * (9*t[0][0] - 5*t[0][2] + 2*t[0][3] - 9*t[1][0] + 5*t[1][2] - 2*t[1][3]);
}
inline float CBetaSpline::F23( const float s[2][4], const float t[2][4] ) const
{
  return (s[0][3] - s[1][3]) * 
    (-2*t[0][0] - 3*t[0][1] - 3*t[0][2] + 2*t[0][3] + t[1][0] * (2 + 3*t[1][0] + 3*t[1][1] - 2*t[1][2]));
}
