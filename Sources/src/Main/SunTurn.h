#ifndef __SUN_TURN_H__
#define __SUN_TURN_H__
#pragma ONCE
// The sun's place in the world (Remastered only,
// docs/superpowers/specs/2026-10-26-camera-turn-design.md, "One sun, picked as
// a mission starts"). The world has one light source. It stands at one of four
// quarter positions: sun turn t is the 2003 sun turned t times by
// (x, y) -> (-y, x) about the vertical, 0 where 2003 had it. A mission picks
// its turn as it starts, another one than the mission before it had, and keeps
// it: the mission's saves carry it. Like the camera step it is local view
// state, a GFX.* global var: it never enters the game state, the commands or
// the network stream, so its pick must not draw from the game's synchronised
// random numbers.
//
// What the camera at step c shows of a world under sun turn t is what step
// c + t shows of the same world turned back by t quarters under the 2003 sun:
// every picture made for "the sun as camera step l sees it" (a building's
// `<variant>_r<k>_l<l>`, its shadow `_r<k>_s<l>`, a prop's `<pack>_l<l>`) is
// the one for light index l = (c + t) mod 4, and the picture's own turn k stays
// facing + c. Light index 0 is the light of the 2003 pictures.
//
// The header reads global vars through GetGlobalVar, which each module's
// StdAfx.h brings in (StreamIO/Globals.h); a test may provide its own.
#include "Remastered.h"
namespace NCameraTurn
{
	const char * const pszSunVar = "GFX.World.SunTurn";
	// The player's profile var (IUserProfile, config.cfg) that holds the sun turn
	// of the last mission the player started with the Remastered option on.
	const char * const pszLastSunVar = "Remastered.SunTurn.Last";

	// Whether the player may turn the camera: the player's own Remastered option
	// (NRemastered::pszOptionVar). Unlike NRemastered::Enabled() it holds in
	// multiplayer and in a replay of any mode, because the step is local view
	// state: it never enters the game state, the commands or the network stream.
	// The sun's turn goes by the same rule.
	inline bool Allowed()
	{
		return GetGlobalVar( NRemastered::pszOptionVar, 0 ) != 0;
	}
	// The sun turn the world is drawn under: the mission's where the player's
	// Remastered option holds, else 0, the 2003 light, whatever the mission
	// picked or a save carried. The editors never set it.
	inline int SunTurn()
	{
		return Allowed() ? GetGlobalVar( pszSunVar, 0 ) & 3 : 0;
	}
	// The light index of the pictures camera step nStep shows under sun turn
	// nSun (above).
	inline int LightIndex( const int nStep, const int nSun )
	{
		return ( nStep + nSun ) & 3;
	}
	inline int LightIndex( const int nStep )
	{
		return LightIndex( nStep, SunTurn() );
	}
	// Where the data holds no shadow for light index nLight, the shadow an
	// object showing its turn nTurn casts is a step-0 shadow laid flat on the
	// ground turned by nLight quarter turns: that of its turn nTurn - nLight,
	// the object as it stands in the world turned back by the sun's turn (its
	// own facing less the sun's turn, the step dropping out).
	inline int LaidShadowTurn( const int nTurn, const int nLight )
	{
		return ( nTurn - nLight ) & 3;
	}
	// A ground direction of the 2003 sun (its x and y; the height stays) under
	// sun turn nSun.
	template <class TFloat>
	inline void TurnSun( TFloat *pfX, TFloat *pfY, const int nSun )
	{
		for ( int i = nSun & 3; i > 0; --i )
		{
			const TFloat fX = *pfX;
			*pfX = -*pfY;
			*pfY = fX;
		}
	}
	// The light a surface of unit normal (fNX, fNY, fNZ) takes from a sun whose
	// light travels along the unit vector (fSunX, fSunY, fSunZ), as the map's
	// terrain shades and the rivers hold it (CVertexAltitudeInfo::UpdateShades):
	// the cosine, held between 0.6 and 1.
	inline float GroundLight( const float fNX, const float fNY, const float fNZ, const float fSunX, const float fSunY, const float fSunZ )
	{
		const float fLight = -( fNX * fSunX + fNY * fSunY + fNZ * fSunZ );
		return fLight < 0.6f ? 0.6f : fLight > 1.0f ? 1.0f : fLight;
	}
	// A terrain vertex's shade (0 to 255) under sun turn nSun. nBaked is the
	// shade the map holds, its normal lit by the 2003 sun as the Map Editor
	// computed it (or set by hand since); it is moved by what the turn changes
	// in the light model's own answer for the normal, and stays within the
	// model's range. Turn 0 and flat ground keep the map's shade as it is.
	inline int TurnedShade( const int nBaked, const float fNX, const float fNY, const float fNZ, const float fSunX, const float fSunY, const float fSunZ, const int nSun )
	{
		if ( ( nSun & 3 ) == 0 )
			return nBaked;
		float fX = fSunX, fY = fSunY;
		TurnSun( &fX, &fY, nSun );
		const float fChange = ( GroundLight( fNX, fNY, fNZ, fX, fY, fSunZ ) - GroundLight( fNX, fNY, fNZ, fSunX, fSunY, fSunZ ) ) * 255.0f;
		const int nChange = int( fChange < 0 ? fChange - 0.5f : fChange + 0.5f );
		if ( nChange == 0 )
			return nBaked;
		const int nLeast = nBaked < 153 ? nBaked : 153;				// 0.6 of 255
		const int nShade = nBaked + nChange;
		return nShade < nLeast ? nLeast : nShade > 255 ? 255 : nShade;
	}
	// The sun turn of a mission that starts: 0 where the player may not turn the
	// camera (the 2003 look); else one of the four by nRandom (any number that is
	// not the game's synchronised random: the pick is the player's own), never
	// the turn nPrevious of the mission the player started before it (-1:
	// none), so that two games in a row never look the same. The first pick is
	// even over the four, a later one even over the other three.
	inline int PickSunTurn( const bool bAllowed, const int nPrevious, const unsigned int nRandom )
	{
		if ( !bAllowed )
			return 0;
		if ( nPrevious < 0 || nPrevious > 3 )
			return int( nRandom % 4 );
		return ( nPrevious + 1 + int( nRandom % 3 ) ) & 3;
	}
	// A turn forced from outside (the BK_SUN_TURN environment variable, for the
	// tiers and for looking at one sun): "0" to "3", anything else is no force
	// (-1).
	inline int ForcedSunTurn( const char *pszValue )
	{
		if ( pszValue == 0 || pszValue[0] < '0' || pszValue[0] > '3' || pszValue[1] != 0 )
			return -1;
		return pszValue[0] - '0';
	}
};
#endif // __SUN_TURN_H__
