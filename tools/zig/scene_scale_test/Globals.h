// The engine's Globals.h for scene_screen_scale_test.cpp: SceneScreenScale.h reads its options through
// GetGlobalVar, and the test runs without the engine's variable table, so every variable has its default,
// as in a Game whose config sets none of them.
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

inline int GetGlobalVar( const char *, int defval ) { return defval; }
inline float GetGlobalVar( const char *, float defval ) { return defval; }

#endif
