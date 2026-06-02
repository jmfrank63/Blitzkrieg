#ifndef __RANDOMIZE_H__
#define __RANDOMIZE_H__

#pragma ONCE
inline void RandUniformlyInCircle( const float fR, CVec2 *pvRand )
{
	*pvRand = VNULL2;

	do
	{
		pvRand->x = fR * Random( -1.0f, 1.0f );
		pvRand->y = fR * Random( -1.0f, 1.0f );
	} while ( sqr( pvRand->x ) + sqr( pvRand->y ) > sqr( fR ) );
}
inline void RandQuadrInCircle(	const float fR, 
																CVec2 *pvRand, 
																const float fRatio=0.0f,
																CVec2 vTrajLine = VNULL2)
{
	const int temp = Random( 65536 );
	const CVec2 dir( GetVectorByDirection( temp ) );
	const float fRandR = fR * Random( 0.0f, 1.0f );

	
	if ( fRatio == 0.0f )
	{
		pvRand->x = dir.x * fRandR;
		pvRand->y = dir.y * fRandR;
	}
	else // для вытянутых траекторий
	{
		Normalize( &vTrajLine );
		*pvRand = vTrajLine* fRandR * dir.x * fRatio +
							CVec2( -vTrajLine.y, vTrajLine.x ) * fRandR * dir.y ;
	}
}
#endif // __RANDOMIZE_H__
