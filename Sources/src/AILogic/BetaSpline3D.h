#ifndef __BETASPLINE3D_H__
#define __BETASPLINE3D_H__

#pragma ONCE
class CBetaSpline3D
{
	DECLARE_SERIALIZE;

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
  CBetaSpline3D() { }
  void Init( float fBeta1, float fBeta2 );

  const float Value( float u, float v, const float ptControls[16] ) const;
  void GetNormale( CVec3 *pvNormale, float u, float v, const float ptCtrls[16] ) const;
};
#endif //  __BETASPLINE3D_H__
