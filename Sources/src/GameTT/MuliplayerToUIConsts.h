#if !defined(_MULTIPLAYER_TO_UI_CONSTS_)
#define _MULTIPLAYER_TO_UI_CONSTS_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
enum EMultiplayerToUICommands
{
	EMTUC_UPDATE_SERVER_INFO,
	EMTUC_DELETE_SERVER,
	EMTUC_CANT_RESOLVE_ADDRESS,
	
	EMTUC_GAME_INFO,
	EMTUC_WRONG_RESOURCES,
	EMTUC_WRONG_MAP,
	EMTUC_NO_MAP,
	EMTUC_WRONG_PASSWORD,
	EMTUC_GAME_IS_ALREADY_STARTED,
	EMTUC_WRONG_GAMEEXE_VERSION,

	EMTUC_CONNECTION_FAILED,
	EMTUC_UPDATE_PLAYER_INFO,
	EMTUC_PLAYER_LEFT,
	EMTUC_PLAYER_KICKED,
	EMTUC_AIM_KICKED,

	EMTUC_UPDATE_CHAT_PLAYER_INFO,
	EMTUC_PLAYER_CHANGED_NICK,
	EMTUC_PLAYER_LEFT_GAMESPY,	
	

	EMTUC_CREATE_STAGING_ROOM,
	EMTUC_START_GAME,
	EMTUC_CONFIGURE_STAGING_ROOM,
	EMTUC_GIVE_PASSWORD,
	EMTUC_ALLOW_START_GAME,								// with SNotificationSimpleParam == bool 
	EMTUC_START_WITH_SINGLE_PARTY,				// all player for sabotage mission ar in 1 party.

	EMTUC_SERVER_SETTINGS_CHANGED,				// // SServerNewSettings is attached
};
enum EUIToMultiplayerNotifications
{
	EUTMN_UNITIALIZED											= 0,
	EUTMN_GAMES_LIST_MODE									= 1,
	EUTMN_STAGING_ROOM_MODE								= 2,
	EUTMN_CHAT_MODE												= 3,
	EUTMN_LEAVE_CHAT_MOVE									= 4,

	EUTMN_PLAYER_READY										= 5,			// when player chaned ready state
	
	EUTMN_SIDE														= 6,				//WHEN player changes side
			
	EUTMN_CREATE													= 7,			//when creates game with settings ( sends SNewMapInfo )
	EUTMN_JOIN														= 8,
	EUTMN_PASSWORD												= 9,
	EUTMN_LEFT_GAME												= 10,
	EUTMN_KICK_PLAYER											= 11,
	
	EUTMN_SEND_CHAT_MESSAGE								= 12,
	EUTMN_PLAYER_RELATION_CHANGED					= 13,			//when changed relation. sends SUIRelationNotify

	EUTMN_START_GAME											= 14,
	EUTMN_AWAY														= 15,

	EUTMN_SERVER_SETTINGS_CHANGED					= 16,			// SServerNewSettings is attached
	EUTMN_CONNECT_TO_SERVER								= 17,
	EUTMN_CANCEL_CONNECT_TO_SERVER				= 18,

	EUTMN_ADDRESS_BOOK_MODE								= 19,
	EUTMN_SWITCH_MOD_OK										= 20,
};
#endif //_MULTIPLAYER_TO_UI_CONSTS_
