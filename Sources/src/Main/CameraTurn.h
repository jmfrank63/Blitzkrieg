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
};
#endif // __CAMERA_TURN_H__
