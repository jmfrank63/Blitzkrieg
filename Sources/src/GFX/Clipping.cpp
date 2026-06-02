#include "StdAfx.h"

#include "Clipping.h"
bool ClipAARect( const CTRect<float> &bounds, CTRect<float> *pRect, CTRect<float> *pMaps )
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
		const float fAlpha = ( bounds.x1 - pRect->x1 ) / ( pRect->x2 - pRect->x1 );
		NI_ASSERT_SLOW_T( (fAlpha >= 0) && (fAlpha <= 1), NStr::Format("Whong alpha coeff (%g) during AA rect clipping", fAlpha) );
		pRect->x1 = bounds.x1;
		pMaps->x1 = pMaps->x1 + fAlpha*( pMaps->x2 - pMaps->x1 );
	}
	if ( dwAll & CLIP_CS_RIGHT )
	{
		const float fAlpha = ( bounds.x2 - pRect->x1 ) / ( pRect->x2 - pRect->x1 );
		NI_ASSERT_SLOW_T( (fAlpha >= 0) && (fAlpha <= 1), NStr::Format("Whong alpha coeff (%g) during AA rect clipping", fAlpha) );
		pRect->x2 = bounds.x2;
		pMaps->x2 = pMaps->x1 + fAlpha*( pMaps->x2 - pMaps->x1 );
	}
	if ( dwAll & CLIP_CS_TOP )
	{
		const float fAlpha = ( bounds.y1 - pRect->y1 ) / ( pRect->y2 - pRect->y1 );
		NI_ASSERT_SLOW_T( (fAlpha >= 0) && (fAlpha <= 1), NStr::Format("Whong alpha coeff (%g) during AA rect clipping", fAlpha) );
		pRect->y1 = bounds.y1;
		pMaps->y1 = pMaps->y1 + fAlpha*( pMaps->y2 - pMaps->y1 );
	}
	if ( dwAll & CLIP_CS_BOTTOM )
	{
		const float fAlpha = ( bounds.y2 - pRect->y1 ) / ( pRect->y2 - pRect->y1 );
		NI_ASSERT_SLOW_T( (fAlpha >= 0) && (fAlpha <= 1), NStr::Format("Whong alpha coeff (%g) during AA rect clipping", fAlpha) );
		pRect->y2 = bounds.y2;
		pMaps->y2 = pMaps->y1 + fAlpha*( pMaps->y2 - pMaps->y1 );
	}
	return !pRect->IsEmpty();
}
