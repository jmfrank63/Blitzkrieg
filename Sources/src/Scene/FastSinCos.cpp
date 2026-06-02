#include "StdAfx.h"

#include "FastSinCos.h"
#define QUARTER_MAX_CIRCLE_ANGLE (MAX_CIRCLE_ANGLE/4)
#define MASK_MAX_CIRCLE_ANGLE (MAX_CIRCLE_ANGLE - 1)
namespace NFastSinCos
{
	static float fast_cossin_table[MAX_CIRCLE_ANGLE];           // Declare table of fast cosinus and sinus
	struct SFastSinCosAutoMagic
	{
		SFastSinCosAutoMagic()
		{
			for ( int i = 0; i < MAX_CIRCLE_ANGLE; ++i )
			{
				fast_cossin_table[i] = (float)sin( double(i) * PI / HALF_MAX_CIRCLE_ANGLE );
			}
		}
	};
	static SFastSinCosAutoMagic fastsincosinit;
	const float GetFastCos( const int i )
	{
		if ( i < 0 )
			return NFastSinCos::fast_cossin_table[ ((-i) + QUARTER_MAX_CIRCLE_ANGLE) & MASK_MAX_CIRCLE_ANGLE ];
		else
			return NFastSinCos::fast_cossin_table[ (i + QUARTER_MAX_CIRCLE_ANGLE) & MASK_MAX_CIRCLE_ANGLE ];
	}
	const float GetFastSin( const int i )
	{
		if ( i < 0 )
			return NFastSinCos::fast_cossin_table[ (-((-i) & MASK_MAX_CIRCLE_ANGLE)) + MAX_CIRCLE_ANGLE ];
		else
			return NFastSinCos::fast_cossin_table[ i & MASK_MAX_CIRCLE_ANGLE ];
	}
};
const float FCos( const float fAngle )
{
	return NFastSinCos::GetFastCos( FSinCosMakeAngle(fAngle) );
}
const float FSin( const float fAngle )
{
	return NFastSinCos::GetFastSin( FSinCosMakeAngle(fAngle) );
}
const float FCosChecked( const float fAngle )
{
	return NFastSinCos::GetFastCos( FSinCosMakeAngleChecked(fAngle) );
}
const float FSinChecked( const float fAngle )
{
	return NFastSinCos::GetFastSin( FSinCosMakeAngleChecked(fAngle) );
}
const float FCosCalibrated( const int nAngle )
{
	return NFastSinCos::GetFastCos( nAngle );
}
const float FSinCalibrated( const int nAngle )
{
	return NFastSinCos::GetFastSin( nAngle );
}
