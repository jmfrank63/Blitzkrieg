#ifndef __SCENESCREENSCALE_H__
#define __SCENESCREENSCALE_H__
#pragma once

namespace NSceneScreenScale
{
	static const float LEGACY_GAMEPLAY_WIDTH = 1024.0f;
	static const float LEGACY_GAMEPLAY_HEIGHT = 768.0f;

	inline float GetGameplayScale( const CTRect<float> &rcScreen )
	{
		const float fWidth = Max( rcScreen.Width(), 1.0f );
		const float fHeight = Max( rcScreen.Height(), 1.0f );
		return Max( 1.0f, Min( fWidth / LEGACY_GAMEPLAY_WIDTH, fHeight / LEGACY_GAMEPLAY_HEIGHT ) );
	}

	inline void ScaleGameplayScreenPoint( float *pfX, float *pfY, const CTRect<float> &rcScreen )
	{
		const float fScale = GetGameplayScale( rcScreen );
		if ( fScale <= 1.001f )
			return;

		const float fCenterX = rcScreen.x1 + rcScreen.Width() * 0.5f;
		const float fCenterY = rcScreen.y1 + rcScreen.Height() * 0.5f;
		*pfX = fCenterX + ( *pfX - fCenterX ) * fScale;
		*pfY = fCenterY + ( *pfY - fCenterY ) * fScale;
	}

	template <class TVertex>
	inline void ScaleGameplayScreenVertex( TVertex *pVertex, const CTRect<float> &rcScreen )
	{
		ScaleGameplayScreenPoint( &pVertex->x, &pVertex->y, rcScreen );
	}

	inline CTRect<float> GetGameplayScreenRect( const CTRect<float> &rcScreen )
	{
		const float fScale = GetGameplayScale( rcScreen );
		if ( fScale <= 1.001f )
			return rcScreen;

		const float fWidth = Max( rcScreen.Width() / fScale, 1.0f );
		const float fHeight = Max( rcScreen.Height() / fScale, 1.0f );
		const float fCenterX = rcScreen.x1 + rcScreen.Width() * 0.5f;
		const float fCenterY = rcScreen.y1 + rcScreen.Height() * 0.5f;
		return CTRect<float>( fCenterX - fWidth * 0.5f, fCenterY - fHeight * 0.5f,
							 fCenterX + fWidth * 0.5f, fCenterY + fHeight * 0.5f );
	}

	inline bool CreateGameplayProjectionMatrix( SHMatrix *pMatrix, const CTRect<float> &rcScreen )
	{
		const float fScale = GetGameplayScale( rcScreen );
		if ( fScale <= 1.001f )
			return false;

		const float fProjectionWidth = Max( rcScreen.Width() / fScale, 1.0f );
		const float fProjectionHeight = Max( rcScreen.Height() / fScale, 1.0f );
		CreateOrthographicProjectionMatrixRH( pMatrix, fProjectionWidth, fProjectionHeight, 1, 1024*8 + fProjectionHeight*2 );
		return true;
	}
}

#endif // __SCENESCREENSCALE_H__
