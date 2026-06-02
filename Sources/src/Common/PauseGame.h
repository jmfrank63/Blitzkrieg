#ifndef __PAUSE_GAME_H__
#define __PAUSE_GAME_H__
enum EGamePauseType
{
	PAUSE_TYPE_NO_PAUSE						= -1,		// no pause
	PAUSE_TYPE_USER_PAUSE					= 0,		// user pause (SPACE pressed)
	PAUSE_TYPE_INACTIVE						= 1,		// application lost focus (ALT+TAB for example)
	PAUSE_TYPE_PREMISSION					= 2,		// pause between mission started and first 2 frames displayed
	PAUSE_TYPE_MENU								= 3,		// ESC menu initiated
	PAUSE_TYPE_MP_NO_SEGMENT_DATA	= 100,	// no segment data in multiplayer mode
	PAUSE_TYPE_MP_LAGG						= 101,	// LAGG in multiplayer mode
	PAUSE_TYPE_MP_TIMEOUT					= 102,	// 
	PAUSE_TYPE_MP_LOADING					= 103,
	PAUSE_TYPE_NO_CONTROL					= 100,
};
#endif // __PAUSE_GAME_H__