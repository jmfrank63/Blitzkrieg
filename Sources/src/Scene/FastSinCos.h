#ifndef __FASTSINCOS_H__
#define __FASTSINCOS_H__
#pragma ONCE
#define MAX_CIRCLE_ANGLE      32768
#define HALF_MAX_CIRCLE_ANGLE (MAX_CIRCLE_ANGLE/2)
namespace NFastSinCos
{
	inline void FloatToInt( int *int_pointer, const float f ) 
	{
		__asm  fld  f
		__asm  mov  edx,int_pointer
		__asm  FRNDINT
		__asm  fistp dword ptr [edx];
	}
};
inline const int FSinCosMakeAngle( const float fAngle )
{
	const float f = fAngle * HALF_MAX_CIRCLE_ANGLE / FP_PI;
	int i;
	NFastSinCos::FloatToInt( &i, f );
	return i;
}
inline const int FSinCosMakeAngleChecked( const float fAngle )
{
	const float f = fAngle * HALF_MAX_CIRCLE_ANGLE / FP_PI;
	int i;
	NFastSinCos::FloatToInt( &i, f );
	return i % MAX_CIRCLE_ANGLE;
}
const float FCos( const float fAngle );
const float FSin( const float fAngle );
const float FCosChecked( const float fAngle );
const float FSinChecked( const float fAngle );
const float FCosCalibrated( const int nAngle );
const float FSinCalibrated( const int nAngle );
#endif // __FASTSINCOS_H__
