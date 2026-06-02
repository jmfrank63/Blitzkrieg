#ifndef __BETASPLINE_H__
#define __BETASPLINE_H__


class CBetaSpline
{
private:
  float fBeta1, fBeta2;
  float invdelta;
  float fBeta1_3;
  float fBeta1_2;
  float fVolCoeffs[16];

  float b_2( const float t[3] ) const;
  float b_1( const float t[3] ) const;
  float b0( const float t[3] )  const;
  float b1( const float t[3] )  const;

  float db_2( const float t[3] ) const;
  float db_1( const float t[3] ) const;
  float db0( const float t[3] )  const;
  float db1( const float t[3] )  const;

  void  VolumeCoeffs( float b1, float b2 );

  float F00( const float t[2][4] ) const;
  float F11( const float t[2][4] ) const;
  float F22( const float t[2][4] ) const;
  float F33( const float t[2][4] ) const;
  float F01( const float s[2][4], const float t[2][4] ) const;
  float F02( const float s[2][4], const float t[2][4] ) const;
  float F03( const float s[2][4], const float t[2][4] ) const;
  float F12( const float s[2][4], const float t[2][4] ) const;
  float F13( const float s[2][4], const float t[2][4] ) const;
  float F23( const float s[2][4], const float t[2][4] ) const;

public:
  CBetaSpline() { Init( 1, 1 ); }
  void  Init( float fBeta1, float fBeta2 );

  CVec3 Value( float u, float v, const CVec3 ptControls[16] ) const;
  void  Derivative( CVec3 &ptDU, CVec3 &ptDV, float u, float v, const CVec3 ptCtrls[16] ) const;
  float Ave( const float ptControls[16] ) const;
  float Ave( float u0, float u1, float v0, float v1, const float ptControls[16] ) const;
};

#endif //  __BETASPLINE_H__