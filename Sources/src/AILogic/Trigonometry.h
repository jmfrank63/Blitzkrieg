#ifndef __TRIGONOMETRY_H__
#define __TRIGONOMETRY_H__
#pragma ONCE
namespace NTrg
{
	void Init();

	float Sin( float fAlpha );
	inline float Cos( const float fAlpha ) { return Sin( fAlpha + FP_PI2 ); }
	
	float ASin( float fSin );
	inline float ACos( const float fCos ) { return FP_PI2 - ASin( fCos ); }
};
#endif // __TRIGONOMETRY_H__
