#ifndef __MISSION_INTERFACE_ESCAPE_MENU_H__
#define __MISSION_INTERFACE_ESCAPE_MENU_H__


enum EEScapeMenuConstants
{

	SP_ESCAPE_MENU_MAIN										= 99998,
	MP_ESCAPE_MENU_MAIN										= 99999,
	
	SP_END_MISSION												= 99997,
	MP_END_MISSION												= 99996,

	ESCAPE_WIN_MISSION										= 99995,

	SP_FAIL_MISSION												= 99994,
	MP_FAIL_MISSION												= 99993,

	BUTTON_ESCAPE_SAVE										= 10000,
	BUTTON_ESCAPE_LOAD										= 10001,
	BUTTON_ESCAPE_OPTIONS									= 10002,
	BUTTON_ESCAPE_HELP										= 10003,
	BUTTON_ESCAPE_OBJECTIVES							= 10004,
	BUTTON_ESCAPE_END_MISSION							= 10005,
	
	
	BUTTON_ESCAPE_RESTART_MISSION					= 10008,
	BUTTON_ESCAPE_EXIT_TO_MAINMENU				= 10009,
	BUTTON_ESCAPE_EXIT_TO_WINDOWS					= 10010,
	
	BUTTON_ESCAPE_EXIT_TO_ROOT_ESCAPE			= 10011,
	
	STATIC_MP_FAIL												= 10012,
	STATIC_SP_FAIL												= 10013,
	
	
	STATIC_MP_WIN													= 10014,	//
	STATIC_SP_WIN													= 10014,	// YES, THESE ARE THE SAME
	
	BUTTON_OBJECTIVES_BACK								= 10015,
	BUTTON_HELP_BACK											= 10016,
	

	BUTTON_SP_END_WIN_MISSION							= 10007,
	BUTTON_MP_END_WIN_MISSION							= 10017,
	
	BUTTON_WIN_WIN_MISSION								= 10018,

	BUTTON_ESCAPE_RETURN_TO_GAME					= 10006,
	
	BUTTON_WIN_RETURN_TO_GAME							= 10021,
	BUTTON_SP_LOOSE_RETURN_TO_GAME				= 10019,
	BUTTON_MP_LOOSE_RETURN_TO_GAME				= 10020,
	
	BUTTON_SINGLE_OBJECTIVE_BACK					= 10021,
	BUTTON_WIN_BACK_TO_MISSION						= 10022,
	
	BUTTON_MP_TIMEOUT_CALL								= 10023,
	BUTTON_MP_TIMEOUT_CANCEL							= 10024,

	MISSION_HELP_DIALOG										= 3000,
	MISSION_OBJECTIVES_DIALOG							= 4000,
	MISSION_SINGLE_OBJECTIVE_DIALOG				= 6000,
	
	MISSION_BUTTON_SINGLE_OBJECTIVE				= 6001,		// TUTORIAL



	/*
	save game
load game
options
help
show objectives
end mission…
return to mission
finish mission
restart mission
exit to main menu
exit to windows
pervious menu
<win message>
finish mission
return to mission
<fail message>
finish mission
load game
restart mission
exit to main menu
exit to windows
return to mission
*/

};

#endif //__MISSION_INTERFACE_ESCAPE_MENU_H__