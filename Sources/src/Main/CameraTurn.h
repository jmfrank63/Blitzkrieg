#ifndef __CAMERA_TURN_H__
#define __CAMERA_TURN_H__
#pragma ONCE
// The Game camera's quarter turns (Remastered only,
// docs/superpowers/specs/2026-10-26-camera-turn-design.md). Step k turns the
// view about the point at the screen centre so that the world looks turned by
// k quarter turns of (x, y) -> (-y, x), the turn that takes a building's side
// s to side s + 1 (M005's `<variant>_r1.._r3`). The step is a GFX.* global
// var like the zoom steps: a session setting no save carries, 0 (the 2003
// view) unless the turn key set it, and the camera, the terrain and the
// buildings all read it here.
//
// The header reads global vars through GetGlobalVar, which each module's
// StdAfx.h brings in (StreamIO/Globals.h).
namespace NCameraTurn
{
	const char * const pszStepVar = "GFX.World.CameraTurn";

	inline int Step()
	{
		return GetGlobalVar( pszStepVar, 0 ) & 3;
	}
	// What the step adds to the camera's yaw. A camera turned by -90 degrees
	// about its anchor sees the world turned by +90 degrees.
	inline float YawOffset( const int nStep )
	{
		return -float( nStep ) * FP_PI2;
	}
	// A map object's own facing in quarter turns: its map Direction (0..65535,
	// the AI's: 0 faces +y, 16384 -x, as a variant's 1.xml Direction 0 and 90)
	// to the nearest quarter. The 2003 game ignored it for buildings (their AI
	// object never turns), and every building in the shipped maps has 0.
	inline int Facing( const int nDirection )
	{
		return ( ( nDirection + 0x2000 ) >> 14 ) & 3;
	}
	// The turn of its variant a building is drawn with (Game and Map Editor):
	// its own facing plus the camera step. Turn k of a variant that shows side
	// s is side (s - 1 + k) mod 4 + 1, M005's convention.
	inline int DrawnTurn( const int nFacing, const int nStep )
	{
		return ( nFacing + nStep ) & 3;
	}
	// The folder of a variant's turn k: the variant itself at 0, else M005's
	// `<variant>_r<k>` where the data holds it (bk-views side writes it, from
	// the family's 2003 variant where one of the variant's colourway shows that
	// side), else the variant itself. The caller brings IDataStorage in.
	inline std::string ModelPath( const std::string &szVariant, const int nTurn )
	{
		if ( nTurn != 0 )
		{
			const std::string szTurned = NStr::Format( "%s_r%d", szVariant.c_str(), nTurn );
			if ( GetSingleton<IDataStorage>()->IsStreamExist( (szTurned + "\\1.san").c_str() ) )
				return szTurned;
		}
		return szVariant;
	}
};
#endif // __CAMERA_TURN_H__
