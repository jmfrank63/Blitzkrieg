#ifndef __INTERSECTION_H__
#define __INTERSECTION_H__
#pragma ONCE
#define CLIP_CS_BOTTOM	0x0001
#define CLIP_CS_LEFT		0x0002
#define CLIP_CS_TOP			0x0004
#define CLIP_CS_RIGHT		0x0008
#define CLIP_CS_FAR			0x0010
#define CLIP_CS_NEAR		0x0020

#define CLIP_CS_BOTTOM_SHIFT	0
#define CLIP_CS_LEFT_SHIFT		1
#define CLIP_CS_TOP_SHIFT			2
#define CLIP_CS_RIGHT_SHIFT		3
#define CLIP_CS_FAR_SHIFT			4
#define CLIP_CS_NEAR_SHIFT		5
inline DWORD CheckLessZeroBit( const float fVal ) 
{ 
	return FP_BITS_CONST(fVal) & 0x80000000; 
}
inline const DWORD GetCSClipFlags( const float x1, const float y1, const float x2, const float y2, const float x, const float y )
{
	const float t1 = x - x1, t2 = y - y1, t3 = x2 - x, t4 = y2 - y;
	return (( FP_BITS_CONST(t1) >> 30 ) & 2 ) |
				 (( FP_BITS_CONST(t2) >> 29 ) & 4 ) |
				 (( FP_BITS_CONST(t3) >> 28 ) & 8 ) |
				 (( FP_BITS_CONST(t4) >> 31 )     );
}
template <class TYPE>
inline bool ClipAARect( const CTRect<TYPE> &bounds, CTRect<TYPE> *pRect, CTRect<TYPE> *pSatelliteRect )
{
	const DWORD dwClip0 = GetCSClipFlags( bounds.x1, bounds.y1, bounds.x2, bounds.y2, pRect->x1, pRect->y1 );
	const DWORD dwClip1 = GetCSClipFlags( bounds.x1, bounds.y1, bounds.x2, bounds.y2, pRect->x2, pRect->y2 );
	const DWORD dwAll = dwClip0 | dwClip1;
	if ( dwAll == 0 ) 
		return true;
	else if ( (dwClip0 & dwClip1) != 0 )
		return false;
	if ( dwAll & CLIP_CS_LEFT )
	{
		const float fAlpha = float( bounds.x1 - pRect->x1 ) / float( pRect->x2 - pRect->x1 );
		NI_ASSERT_SLOW_T( (fAlpha >= 0) && (fAlpha <= 1), NStr::Format("Whong alpha coeff (%g) during AA rect clipping", fAlpha) );
		pRect->x1 = bounds.x1;
		pSatelliteRect->x1 = pSatelliteRect->x1 + fAlpha*( pSatelliteRect->x2 - pSatelliteRect->x1 );
	}
	if ( dwAll & CLIP_CS_RIGHT )
	{
		const float fAlpha = float( bounds.x2 - pRect->x1 ) / float( pRect->x2 - pRect->x1 );
		NI_ASSERT_SLOW_T( (fAlpha >= 0) && (fAlpha <= 1), NStr::Format("Whong alpha coeff (%g) during AA rect clipping", fAlpha) );
		pRect->x2 = bounds.x2;
		pSatelliteRect->x2 = pSatelliteRect->x1 + fAlpha*( pSatelliteRect->x2 - pSatelliteRect->x1 );
	}
	if ( dwAll & CLIP_CS_TOP )
	{
		const float fAlpha = float( bounds.y1 - pRect->y1 ) / float( pRect->y2 - pRect->y1 );
		NI_ASSERT_SLOW_T( (fAlpha >= 0) && (fAlpha <= 1), NStr::Format("Whong alpha coeff (%g) during AA rect clipping", fAlpha) );
		pRect->y1 = bounds.y1;
		pSatelliteRect->y1 = pSatelliteRect->y1 + fAlpha*( pSatelliteRect->y2 - pSatelliteRect->y1 );
	}
	if ( dwAll & CLIP_CS_BOTTOM )
	{
		const float fAlpha = float( bounds.y2 - pRect->y1 ) / float( pRect->y2 - pRect->y1 );
		NI_ASSERT_SLOW_T( (fAlpha >= 0) && (fAlpha <= 1), NStr::Format("Whong alpha coeff (%g) during AA rect clipping", fAlpha) );
		pRect->y2 = bounds.y2;
		pSatelliteRect->y2 = pSatelliteRect->y1 + fAlpha*( pSatelliteRect->y2 - pSatelliteRect->y1 );
	}
	return !pRect->IsEmpty();
}
inline DWORD CheckViewVolume6( const CVec3 &vPos, const SPlane *pViewVolumePlanes )
{
	DWORD dwClipFlags = CheckLessZeroBit( pViewVolumePlanes[GFXCP_LEFT].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_LEFT_SHIFT );
	dwClipFlags |= CheckLessZeroBit( pViewVolumePlanes[GFXCP_RIGHT].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_RIGHT_SHIFT );
	dwClipFlags |= CheckLessZeroBit( pViewVolumePlanes[GFXCP_TOP].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_TOP_SHIFT );
	dwClipFlags |= CheckLessZeroBit( pViewVolumePlanes[GFXCP_BOTTOM].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_BOTTOM_SHIFT );
	dwClipFlags |= CheckLessZeroBit( pViewVolumePlanes[GFXCP_FAR].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_FAR_SHIFT );
	dwClipFlags |= CheckLessZeroBit( pViewVolumePlanes[GFXCP_NEAR].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_NEAR_SHIFT );
	return dwClipFlags;
}
inline DWORD CheckViewVolume4( const CVec3 &vPos, const SPlane *pViewVolumePlanes )
{
	DWORD dwClipFlags = CheckLessZeroBit( pViewVolumePlanes[GFXCP_LEFT].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_LEFT_SHIFT );
	dwClipFlags |= CheckLessZeroBit( pViewVolumePlanes[GFXCP_RIGHT].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_RIGHT_SHIFT );
	dwClipFlags |= CheckLessZeroBit( pViewVolumePlanes[GFXCP_TOP].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_TOP_SHIFT );
	dwClipFlags |= CheckLessZeroBit( pViewVolumePlanes[GFXCP_BOTTOM].GetDistanceToPoint(vPos) ) >> ( 31 - CLIP_CS_BOTTOM_SHIFT );
	return dwClipFlags;
}
#endif //__INTERSECTION_H__