#ifndef __SCENESCREENSCALE_H__
#define __SCENESCREENSCALE_H__
#pragma once

#include <cmath>

#include "Globals.h"

namespace NSceneScreenScale
{
	static const float LEGACY_GAMEPLAY_WIDTH = 1024.0f;
	static const float LEGACY_GAMEPLAY_HEIGHT = 768.0f;
	static const float ZOOM_MIN_VIEW_WIDTH = 640.0f;
	static const float ZOOM_MIN_VIEW_HEIGHT = 480.0f;

	inline float GetZoomStepFactor()
	{
		return GetGlobalVar( "GFX.World.ZoomFactor", 1.2f );
	}

	// Largest step count n >= 0 with factor^n within the D-09 zoom-in bound
	// (the un-zoomed view may shrink to a 640x480-effective viewport). Base
	// globals unset (menus, legacy path) means no zoom: 0. The base scale
	// here must match GetGameplayScale's FLOORED legacy_step -- the bound is
	// computed against the scale the renderer actually uses, and a fractional
	// fBaseScale would under-report the visible world on any base whose
	// min-axis ratio is not an exact integer (e.g. 1920x1080 or 3440x1440),
	// prematurely capping zoom-in.
	inline int GetMaxZoomSteps( const CTRect<float> &rcScreen )
	{
		const float fBaseW = float( GetGlobalVar( "GFX.World.BaseSizeX", 0 ) );
		const float fBaseH = float( GetGlobalVar( "GFX.World.BaseSizeY", 0 ) );
		if ( fBaseW < 1.0f || fBaseH < 1.0f )
			return 0;
		const float fBaseScale = Max( 1.0f, floorf( Min( fBaseW / LEGACY_GAMEPLAY_WIDTH, fBaseH / LEGACY_GAMEPLAY_HEIGHT ) ) );
		const float fFill = Max( 1.0f, Min( rcScreen.Width() / fBaseW, rcScreen.Height() / fBaseH ) );
		const float fBaseZoom = fBaseScale * fFill;
		const float visW = Max( rcScreen.Width() / fBaseZoom, 1.0f );
		const float visH = Max( rcScreen.Height() / fBaseZoom, 1.0f );
		const float fZMax = Min( visW / ZOOM_MIN_VIEW_WIDTH, visH / ZOOM_MIN_VIEW_HEIGHT );
		int nSteps = 0;
		float fZ = 1.0f;
		while ( nSteps < 8 )
		{
			const float fNext = fZ * GetZoomStepFactor();
			if ( fNext > fZMax )
				break;
			fZ = fNext;
			++nSteps;
		}
		return nSteps;
	}

	// Player zoom z = factor^steps, clamped at read time (D-15: a stale step
	// count re-clamps against the live screen; D-10: z never < 1). With
	// ZoomSteps == 0 this returns exactly 1.0f (powf(x,0)==1), so the
	// pre-phase scale product is bit-identical.
	inline float GetPlayerZoom( const CTRect<float> &rcScreen )
	{
		const int nSteps = GetGlobalVar( "GFX.World.ZoomSteps", 0 );
		const float fFactor = GetZoomStepFactor();
		return Min( powf( fFactor, nSteps ), powf( fFactor, GetMaxZoomSteps( rcScreen ) ) );
	}

	inline float GetGameplayScale( const CTRect<float> &rcScreen )
	{
		const float fWidth = Max( rcScreen.Width(), 1.0f );
		const float fHeight = Max( rcScreen.Height(), 1.0f );
		// Whole steps only. The terrain is one point sampled tileset atlas, so a
		// fractional scale spreads a 64x32 tile over 72.3x36.2 pixels and the
		// pixel straddling a tile edge takes its colour from the neighbouring
		// atlas cell: a one pixel seam on a lattice of exactly the scaled tile
		// size. Measured on a 1440x868 window, whose scale is 1.13, the seams
		// autocorrelate at 36 pixels across and 72 down - the scaled half tile
		// and full tile. Rounding the vertices, which is what this used to rely
		// on, keeps the mesh watertight but cannot help: the span each tile
		// covers still varies between 36 and 37 pixels, so the sampling phase
		// moves from tile to tile. An integer scale removes the fraction itself.
		// The whole-step rule above applies to the legacy base factor; the fill
		// factor on top of it may be fractional because Task "fractional-safe
		// terrain sampling" removed the seam mechanism. When the world base
		// globals are unset (menus, the ELK editor, the legacy path) this
		// reduces to the old whole-step rule.
		const float fBaseW = float( GetGlobalVar( "GFX.World.BaseSizeX", 0 ) );
		const float fBaseH = float( GetGlobalVar( "GFX.World.BaseSizeY", 0 ) );
		if ( fBaseW < 1.0f || fBaseH < 1.0f )
			return Max( 1.0f, floorf( Min( fWidth / LEGACY_GAMEPLAY_WIDTH, fHeight / LEGACY_GAMEPLAY_HEIGHT ) ) );
		const float fLegacyStep = Max( 1.0f, floorf( Min( fBaseW / LEGACY_GAMEPLAY_WIDTH, fBaseH / LEGACY_GAMEPLAY_HEIGHT ) ) );
		const float fFill = Max( 1.0f, Min( fWidth / fBaseW, fHeight / fBaseH ) );
		return fLegacyStep * fFill * GetPlayerZoom( rcScreen );
	}

	// Scale-supplied overload for hot per-vertex loops (CTerrain::ReBuildMeshes):
	// GetGameplayScale does two GetGlobalVar hash lookups, and calling it once
	// per vertex over a whole terrain rebuild is measurably slower than
	// computing it once per rebuild and passing the result down. Not cached as
	// a header static -- the caller re-derives it once per rebuild, so it never
	// goes stale across a mode change.
	inline void ScaleGameplayScreenPoint( float *pfX, float *pfY, const CTRect<float> &rcScreen, float fScale )
	{
		if ( fScale <= 1.001f )
			return;

		const float fCenterX = rcScreen.x1 + rcScreen.Width() * 0.5f;
		const float fCenterY = rcScreen.y1 + rcScreen.Height() * 0.5f;
		// Snap to whole pixels. The terrain is built at integer screen
		// coordinates and every vertex is scaled here by width/1024 against
		// height/768 -- 1.13 on a 1440x868 window, never a whole number. That
		// put each tile edge at a fraction of a pixel, and because the tiles are
		// point sampled out of one tileset the pixel straddling an edge took its
		// colour from the neighbouring tile in the atlas: a one pixel seam.
		// Which seams showed depended on the fractional part, so scrolling
		// sideways made vertical lines come and go and scrolling up and down did
		// the same to horizontal ones.
		// Rounding is a pure function of the coordinate, so two tiles sharing an
		// edge still land on the same pixel and the terrain stays watertight.
		*pfX = floorf( fCenterX + ( *pfX - fCenterX ) * fScale + 0.5f );
		*pfY = floorf( fCenterY + ( *pfY - fCenterY ) * fScale + 0.5f );
	}

	inline void ScaleGameplayScreenPoint( float *pfX, float *pfY, const CTRect<float> &rcScreen )
	{
		ScaleGameplayScreenPoint( pfX, pfY, rcScreen, GetGameplayScale( rcScreen ) );
	}

	inline void UnscaleGameplaySpritePoint( float *pfX, float *pfY, const CVec3 &vSpriteCenter, const CTRect<float> &rcScreen )
	{
		const float fScale = GetGameplayScale( rcScreen );
		if ( fScale <= 1.001f )
			return;

		*pfX = vSpriteCenter.x + ( *pfX - vSpriteCenter.x ) / fScale;
		*pfY = vSpriteCenter.y + ( *pfY - vSpriteCenter.y ) / fScale;
	}

	template <class TVertex>
	inline void ScaleGameplayScreenVertex( TVertex *pVertex, const CTRect<float> &rcScreen, float fScale )
	{
		ScaleGameplayScreenPoint( &pVertex->x, &pVertex->y, rcScreen, fScale );
	}

	template <class TVertex>
	inline void ScaleGameplayScreenVertex( TVertex *pVertex, const CTRect<float> &rcScreen )
	{
		ScaleGameplayScreenVertex( pVertex, rcScreen, GetGameplayScale( rcScreen ) );
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
