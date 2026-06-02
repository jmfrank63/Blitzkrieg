 /*
GameSpy Peer SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2001 GameSpy Industries, Inc

18002 Skypark Circle
Irvine, California 92614
949.798.4200 (Tel)
949.798.4299 (Fax)
devsupport@gamespy.com
*/

#ifndef _PEER_H_
#define _PEER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*************
** INCLUDES **
*************/
#if defined(applec) || defined(THINK_C) || defined(__MWERKS__) && !defined(__KATANA__) && !defined(__mips64)
	#include "::nonport.h"
	#include "::Chat:chat.h"
	#include "::queryreporting:gqueryreporting.h"
	#include "::CEngine:goaceng.h"
#else
	#include "../nonport.h"
	#include "../Chat/chat.h"
	#include "../queryreporting/gqueryreporting.h"
	#include "../CEngine/goaceng.h"
#endif


/************
** DEFINES **
************/
#define PEER_ADD        0  // a server is being added
#define PEER_UPDATE     1  // a server has been updated
#define PEER_REMOVE     2  // a server has been removed
#define PEER_CLEAR      3  // all the servers have been cleared

#define PEER_IN_USE     0  // the nick is already being used
#define PEER_INVALID    1  // the nick contains invalid characters

#define PEER_PASSWORD_LEN     24

#define PEER_FLAG_STAGING     0x01  // s
#define PEER_FLAG_READY       0x02  // r
#define PEER_FLAG_PLAYING     0x04  // g
#define PEER_FLAG_AWAY        0x08  // a
#define PEER_FLAG_OP          0x10
#define PEER_FLAG_VOICE       0x20

#define PEER_KEEP_REPORTING      0  // Continue reporting.
#define PEER_STOP_REPORTING      1  // Stop reporting.  Cannot be used with other options.
#define PEER_REPORT_INFO         2  // Reports all info to the GOAInfo callback (as if it were not playing).
#define PEER_REPORT_PLAYERS      4  // Reports all info to the GOAPlayers callback (as if it were not playing).

/**********
** TYPES **
**********/
typedef void * PEER;

typedef enum
{
	PEERFalse,
	PEERTrue
} PEERBool;

typedef enum
{
	TitleRoom,  // The main room for a game.
	GroupRoom,  // A room which is, in general, for a particular type of gameplay (team, dm, etc.).
	StagingRoom,  // A room where players meet before starting a game.
	NumRooms
} RoomType;

typedef enum
{
	NormalMessage,
	ActionMessage,
	NoticeMessage
} MessageType;

typedef enum
{
	PEERJoinSuccess,     // The room was joined.

	PEERFullRoom,        // The room is full.
	PEERInviteOnlyRoom,  // The room is invite only.
	PEERBannedFromRoom,  // The local user is banned from the room.
	PEERBadPassword,     // An incorrect password (or none) was given for a passworded room.

	PEERAlreadyInRoom,   // The local user is already in or entering a room of the same type.
	PEERNoTitleSet,      // Can't join a room if no title is set.
	PEERNoConnection,    // Can't join a room if there's no chat connection.

	PEERJoinFailed       // Generic failure.
} PEERJoinResult;

/**************
** CALLBACKS **
**************/
typedef void (* peerDisconnectedCallback)
(
	PEER peer,  // The peer object.
	const char * reason,  // The reason for the disconnection.
	void * param  // User-data.
);

typedef void (* peerRoomMessageCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room that the message was in.
	const char * nick,  // The nick of the player who sent the message.
	const char * message,  // The text of the message.
	MessageType messageType,  // The type of message.
	void * param  // User-data.
);

typedef void (* peerRoomUTMCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room that the UTM was in.
	const char * nick,  // The nick of the player who sent the UTM.
	const char * command, // The UTM command for this message.
	const char * parameters,  // Any parameters for this UTM.
	PEERBool authenticated,  // True if this has been authenticated by the server.
	void * param  // User-data.
);

typedef void (* peerRoomNameChangedCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room that the name changed in.
	void * param  // User-data
);

typedef void (* peerRoomModeChangedCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room that the name changed in.
	CHATChannelMode * mode,  // The current mode for this room.
	void * param  // User-data
);

typedef void (* peerPlayerMessageCallback)
(
	PEER peer,  // The peer object.
	const char * nick,  // The nick of the player who sent the message.
	const char * message,  // The text of the message.
	MessageType messageType,  // The type of message.
	void * param  // User-data
);

typedef void (* peerPlayerUTMCallback)
(
	PEER peer,  // The peer object.
	const char * nick,  // The nick of the player who sent the UTM.
	const char * command, // The UTM command for this message.
	const char * parameters,  // Any parameters for this UTM.
	PEERBool authenticated,  // True if this has been authenticated by the server.
	void * param  // User-data
);

typedef void (* peerReadyChangedCallback)
(
	PEER peer,  // The peer object.
	const char * nick,  // The nick of the player who's ready state changed.
	PEERBool ready,  // The player's new ready state.
	void * param  // User-data.
);

typedef void (* peerGameStartedCallback)
(
	PEER peer,  // The peer object.
	unsigned int IP,  // The IP of the host, in network-byte order. PANTS|09.11.00 - was unsigned long
	const char * message,  // A message that was passed into peerGameStart().
	void * param  // User-data.
);

typedef void (* peerPlayerJoinedCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room that the player joined.
	const char * nick,  // The nick of the player that joined.
	void * param  // User-data.
);

typedef void (* peerPlayerLeftCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room that the player left.
	const char * nick,  // The nick of the player that left.
	const char * reason,  // The reason the player left.
	void * param  // User-data.
);

typedef void (* peerKickedCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room that the player was kicked from.
	const char * nick,  // The nick of the player that did the kicking.
	const char * reason,  // An optional reason for the kick.
	void * param  // User-data.
);

typedef void (* peerNewPlayerListCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room.
	void * param  // User-data
);

typedef void (* peerPlayerChangedNickCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of the room the nick changed was in.
	const char * oldNick,  // The player's old nick.
	const char * newNick,  // The player's new nick.
	void * param  // User-data.
);

typedef void (* peerPlayerInfoCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room the info was gotten in.
	const char * nick,  // The nick of the player the info is for.
	unsigned int IP,  // The player's IP.
	int profileID,  // The player's profile ID.
	void * param  // User-data.
);

typedef void (* peerPlayerFlagsChangedCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room the flags were changed in.
	const char * nick,  // The player whose flags have changed.
	int oldFlags,  // The player's old flags.
	int newFlags,  // The player's new flags.
	void * param  // User-data
);

typedef void (* peerPingCallback)
(
	PEER peer,  // The peer object.
	const char * nick,  // The other player's nick.
	int ping,  // The ping.
	void * param  // User-data.
);

typedef void (* peerCrossPingCallback)
(
	PEER peer,  // The peer object.
	const char * nick1,  // The first player's nick.
	const char * nick2,  // The second player's nick.
	int crossPing,  // The cross-ping.
	void * param  // User-data.
);

typedef void (* peerGlobalKeyChangedCallback)
(
	PEER peer,  // The peer object.
	const char * nick,  // The player whose key changed.
	const char * key,  // The key.
	const char * value,  // The value.
	void * param  // User-data.
);

typedef void (* peerRoomKeyChangedCallback)
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room the player is in.
	const char * nick,  // The player whose key changed.
	const char * key,  // The key.
	const char * value,  // The value.
	void * param  // User-data.
);

typedef void (* peerGOACallback)
(
	PEER peer,  // The peer object.
	PEERBool playing,  // PEERTrue if the game is being played.
	char * outbuf,  // Output buffer.
	int maxlen,  // Max data that can be written to outbuf
	void * param // User-data
);

typedef struct PEERCallbacks
{
	peerDisconnectedCallback disconnected;
	peerRoomMessageCallback roomMessage;
	peerRoomUTMCallback roomUTM;
	peerRoomNameChangedCallback roomNameChanged;  // PANTS|09.11.00
	peerRoomModeChangedCallback roomModeChanged;  // PANTS|04.17.01
	peerPlayerMessageCallback playerMessage;
	peerPlayerUTMCallback playerUTM;
	peerReadyChangedCallback readyChanged;
	peerGameStartedCallback gameStarted;
	peerPlayerJoinedCallback playerJoined;
	peerPlayerLeftCallback playerLeft;
	peerKickedCallback kicked;
	peerNewPlayerListCallback newPlayerList;
	peerPlayerChangedNickCallback playerChangedNick;
	peerPlayerInfoCallback playerInfo;  // PANTS|01.08.01
	peerPlayerFlagsChangedCallback playerFlagsChanged;  // PANTS|03.12.01
	peerPingCallback ping;
	peerCrossPingCallback crossPing;
	peerGlobalKeyChangedCallback globalKeyChanged;
	peerRoomKeyChangedCallback roomKeyChanged;
	peerGOACallback GOABasic;
	peerGOACallback GOAInfo;
	peerGOACallback GOARules;
	peerGOACallback GOAPlayers;
	void * param;
} PEERCallbacks;

/************
** GENERAL **
************/
PEER peerInitialize
(
	PEERCallbacks * callbacks  // Global callbacks.
);

typedef void (* peerConnectCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // PEERTrue if success, PEERFalse if failure.
	void * param  // User-data.
);

typedef void (* peerNickErrorCallback)
(
	PEER peer,  // The peer object.
	int type,  // The type of nick error
	const char * nick,  // The bad nick.
	void * param  // User-data.
);

void peerConnect
(
	PEER peer,  // The peer object.
	const char * nick,  // The nick to connect with.
	int profileID,  // The profileID, or 0 if no profileID.
	peerNickErrorCallback nickErrorCallback,  // Called if nick error.
	peerConnectCallback connectCallback,  // Called on complete.
	void * param,  // User-data.
	PEERBool blocking  //  If PEERTrue, called synchronously.
);

void peerRetryWithNick
(
	PEER peer,
	const char * nick
);

PEERBool peerIsConnected
(
	PEER peer
);

PEERBool peerSetTitle
(
	PEER peer,  // The peer object.
	const char * title,  // The title to make current (ie., ut, gmtest).
	const char * qrSecretKey,  // The query&reporting secret key.
	const char * engineName,  // The engine name.
	const char * engineSecretKey,  // The engine secret key.
	int engineMaxUpdates,  // The maximum number of concurent updates
	PEERBool pingRooms[NumRooms],  // To do pings int a room, set it to PEERTrue.
	PEERBool crossPingRooms[NumRooms]  // To do cross-pings in a room, set it to PEERTrue.
);

void peerClearTitle
(
	PEER peer  // The peer object.
);

const char * peerGetTitle
(
	PEER peer  // The peer object.
);

void peerDisconnect
(
	PEER peer  // The peer object.
);

void peerShutdown
(
	PEER peer  // The peer object.
);

void peerThink
(
	PEER peer  // The peer object.
);

CHAT peerGetChat
(
	PEER peer  // The peer object.
);

const char * peerGetNick
(
	PEER peer  // The peer object.
);

unsigned int peerGetLocalIP
(
	PEER peer  // The peer object.
);

typedef void (* peerChangeNickCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // PEERTrue if success, PEERFalse if failure.
	const char * oldNick,  // The old nickname.
	const char * newNick,  // The new nickname.
	void * param  // User-data.
);

void peerChangeNick
(
	PEER peer,  // The peer object.
	const char * newNick,  // The nickname to which to change.
	peerChangeNickCallback callback,  // Called when finished.
	void * param,  // Passed to the callback.
	PEERBool blocking  // If PEERTrue, don't return until finished.
);

void peerStayInRoom
(
	PEER peer,  // The peer object.
	RoomType roomType  // Only TitleRoom is currently supproted.
);

void peerSetQuietMode
(
	PEER peer,  // The peer object.
	PEERBool quiet  // If PEERTrue, enable quiet mode.
);

void peerSetAwayMode
(
	PEER peer,  // The peer object.
	const char * reason  // The away reason.  If NULL or "", not away.
);

void peerParseQuery
(
	PEER peer,  // The peer object.
	char * query,  // String of query data.
	int len,  // The length of the string, not including the NUL.
	struct sockaddr * sender  // The address the query was received from.
);

/**********
** ROOMS **
**********/
typedef void (* peerJoinRoomCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // PEERTrue if success, PEERFalse if failure.
	PEERJoinResult result,  // The result of the attempt.
	RoomType roomType,  // The type of room joined/created.
	void * param  // User-data.
);

void peerJoinTitleRoom
(
	PEER peer,  // The peer object.
	peerJoinRoomCallback callback,  // Called when finished.
	void * param,  // Passed to the callback.
	PEERBool blocking  // If PEERTrue, don't return until finished.
);

void peerJoinGroupRoom
(
	PEER peer,  // The peer object.
	int groupID,  // The ID for the group to join.
	peerJoinRoomCallback callback,  // Called when finished.
	void * param,  // Passed to the callback.
	PEERBool blocking // If PEERTrue, don't return until finished.
);

void peerJoinStagingRoom
(
	PEER peer,  // The peer object.
	GServer server,  // The server passed into peerlistingGamesCallback().
	const char password[PEER_PASSWORD_LEN],  // The password of the room being joined.  Can be NULL or "".
	peerJoinRoomCallback callback,  // Called when finished.
	void * param,  // Passed to the callback.
	PEERBool blocking   // If PEERTrue, don't return until finished.
);

void peerJoinStagingRoomByIP
(
	PEER peer,  // The peer object.
	unsigned int IP,  // The IP of the staging room host.
	const char password[PEER_PASSWORD_LEN],  // The password of the room being joined.  Can be NULL or "".
	peerJoinRoomCallback callback,  // Called when finished.
	void * param,  // Passed to the callback.
	PEERBool blocking   // If PEERTrue, don't return until finished.
);

void peerCreateStagingRoom
(
	PEER peer,  // The peer object.
	const char * name,  // The name of the room.
	int maxPlayers,  // The max number of players allowed in the room.
	const char password[PEER_PASSWORD_LEN],  // An optional password for the staging room
	peerJoinRoomCallback callback,  // Called when finished.
	void * param,  // Passed to the callback.
	PEERBool blocking // If PEERTrue, don't return until finished.
);

void peerCreateStagingRoomWithSocket
(
	PEER peer,  // The peer object.
	const char * name,  // The name of the room.
	int maxPlayers,  // The max number of players allowed in the room.
	const char password[PEER_PASSWORD_LEN],  // An optional password for the staging room
	SOCKET socket,  // The socket to be used for reporting.
	peerJoinRoomCallback callback,  // Called when finished.
	void * param,  // Passed to the callback.
	PEERBool blocking // If PEERTrue, don't return until finished.
);

void peerLeaveRoom
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room you want to leave (TitleRoom, GroupRoom, or StagingRoom).
	const char * reason  // The reason the player is leaving (can be NULL).  PANTS|03.13.01
);

typedef void (* peerListGroupRoomsCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // PEERTrue if success, PEERFalse if failure.
	int groupID,  // A unique ID for this group.
	GServer server,  // The server object for this group room.
	const char * name,  // The group room's name.
	int numWaiting,  // The number of players in the room.
	int maxWaiting,  // The maximum number of players allowed in the room.
	int numGames,  // The number of games either staging or running in the group.
	int numPlaying,  // The total number of players in games in the group.
	void * param  // User-data.
);

void peerListGroupRooms
(
	PEER peer,  // The peer object.
	peerListGroupRoomsCallback callback,  // Called for each group room.
	void * param,  // Passed to the callback.
	PEERBool blocking  // If PEERTrue, don't return until finished.
);

typedef void (* peerListingGamesCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // PEERTrue if success, PEERFalse if failure.
	const char * name,  // The name of the game being listed.
	GServer server,  // The server object for this game.
	PEERBool staging,  // If PEERTrue, this is a staging room and not a running game.
	int msg,  // The type of message this is.
	int progress,  // The percent of servers that have been added.
	void * param  // User-data.
);

void peerStartListingGames
(
	PEER peer,  // The peer object.
	const char * filter,  // A SQL-like rule filter.
	peerListingGamesCallback callback,  // Called when finished.
	void * param  // Passed to the callback.
);

void peerStopListingGames
(
	PEER peer  // The peer object.
);

void peerMessageRoom
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room to send the message to.
	const char * message,  // The message.
	MessageType messageType  // The type of message.
);

void peerUTMRoom
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room to send the UTM to.
	const char * command,  // The command.
	const char * parameters,  // The UTM's parameters.
	PEERBool authenticate  // If true, the server will authenticate this UTM (should normally be false).
);

void peerSetPassword
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room in which to set the password.
	const char password[PEER_PASSWORD_LEN]  // The password to set.
);

void peerSetRoomName
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room in which to set the name.
	const char * name  // The new name
);

const char * peerGetRoomName
(
	PEER peer,  // The peer object.
	RoomType roomType  // The room to get the name for.
);

const char * peerGetRoomChannel
(
	PEER peer,  // The peer object.
	RoomType roomType  // The room to get the channel for.
);

PEERBool peerInRoom
(
	PEER peer,  // The peer object.
	RoomType roomType  // The room to check for.
);

void peerSetTitleRoomChannel
(
	PEER peer,  // The peer object.
	const char * channel  // The channel to use for the title room.
);

void peerSetUpdatesRoomChannel
(
	PEER peer,  // The peer object.
	const char * channel  // The channel to use for the updates room.
);

/************
** PLAYERS **
************/
typedef void (* peerEnumPlayersCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // PEERTrue if success, PEERFalse if failure.
	RoomType roomType,  // The room whose players are being enumerated.
	int index,  // The index of the player (0 to (N - 1)).  -1 when finished.
	const char * nick,  // The nick of the player.
	int flags,  // This player's flags (see #define's above).  PANTS|03.12.01
	void * param  // User-data.
);

void peerEnumPlayers
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room to enum the players in.
	peerEnumPlayersCallback callback,  // Called when finished.
	void * param  // Passed to callback.
);

void peerMessagePlayer
(
	PEER peer,  // The peer object.
	const char * nick,  // The nick of the player to send the message to.
	const char * message,  // The message to send.
	MessageType messageType  // The type of message.
);

void peerUTMPlayer
(
	PEER peer,  // The peer object.
	const char * nick,  // The nick of the player to send the UTM to.
	const char * command,  // The command.
	const char * parameters,  // The UTM's parameters.
	PEERBool authenticate  // If true, the server will authenticate this UTM (should normally be false).
);

PEERBool peerGetPlayerPing
(
	PEER peer,  // The peer object.
	const char * nick,  // The player to get the ping for.
	int * ping  // The player's ping is stored here, if we have it.
);

PEERBool peerGetPlayersCrossPing
(
	PEER peer,  // The peer object.
	const char * nick1,  // The first player.
	const char * nick2,  // The second player.
	int * crossPing  // The cross-ping is stored here, if we have it.
);

PEERBool peerGetPlayerInfoNoWait
(
	PEER peer,
	const char * nick,
	unsigned int * IP,
	int * profileID
);

typedef void (* peerGetPlayerProfileIDCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // PEERTrue if success, PEERFalse if failure.
	const char * nick,  // The player's nick.
	int profileID,  // The player's profile ID.
	void * param  // User-data.
);

void peerGetPlayerProfileID
(
	PEER peer,  // The peer object.
	const char * nick,  // The player's nick.
	peerGetPlayerProfileIDCallback callback,  // Called when finished.
	void * param,  // Passed to callback.
	PEERBool blocking  // If PEERTrue, don't return until finished.
);

typedef void (* peerGetPlayerIPCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // PEERTrue if success, PEERFalse if failure.
	const char * nick,  // The player's nick.
	unsigned int IP,  // The player's IP, in network byte order.  PANTS|09.11.00 - was unsigned long
	void * param  // User-data.
);

void peerGetPlayerIP
(
	PEER peer,  // The peer object.
	const char * nick,  // The player's nick.
	peerGetPlayerIPCallback callback,  // Called when finished.
	void * param,  // Passed to callback.
	PEERBool blocking  // If PEERTrue, don't return until finished.
);

PEERBool peerIsPlayerHost
(
	PEER peer,  // The peer object.
	const char * nick,  // The player's nick.
	RoomType roomType  // The room to check in.
);

PEERBool peerGetPlayerFlags
(
	PEER peer,
	const char * nick,
	RoomType roomType,
	int * flags
);

/*********
** GAME **
*********/
void peerSetReady
(
	PEER peer,  // The peer object.
	PEERBool ready  // Ready or not.
);

PEERBool peerGetReady
(
	PEER peer,  // The peer object.
	const char * nick, // The player's nick.
	PEERBool * ready  // The player's ready state gets stored in here.
);

PEERBool peerAreAllReady
(
	PEER peer  // The peer object.
);

void peerStartGame
(
	PEER peer,  // The peer object.
	const char * message,  // A message to send to everyone.
	int reportingOptions  // Bitfield flags used to set reporting options.
);

PEERBool peerStartReporting
(
	PEER peer  // The peer object.
);

PEERBool peerStartReportingWithSocket
(
	PEER peer,  // The peer object.
	SOCKET socket  // The socket to be used for reporting.
);

void peerStartPlaying
(
	PEER peer  // The peer object.
);

PEERBool peerIsPlaying
(
	PEER peer  // The peer object.
);

void peerStopGame
(
	PEER peer  // The peer object.
);

void peerStateChanged
(
	PEER peer  // The peer object.
);

/*********
** KEYS **
*********/
void peerSetGlobalKeys
(
	PEER peer,  // The peer object.
	int num,  // The number of keys to set.
	const char ** keys,  // The keys to set.
	const char ** values  // The values for the keys.
);

typedef void (* peerGetGlobalKeysCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // If PEERFalse, unable to get the keys.
	const char * nick,  // The player the keys are for.
	int num,  // The number of keys.
	const char ** keys,  // The keys got.
	const char ** values,  // The values for the keys.
	void * param  // User-data.
);

void peerGetPlayerGlobalKeys
(
	PEER peer,  // The peer object.
	const char * nick,  // The player to get the keys for.
	int num,  // The number of keys.
	const char ** keys,  // The keys to get.
	peerGetGlobalKeysCallback callback,  // Called with the keys.
	void * param,  // Passed to callback.
	PEERBool blocking  // If PEERTrue, don't return until finished.
);

void peerGetRoomGlobalKeys
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room to get the keys in.
	int num,  // The number of keys.
	const char ** keys,  // The keys to get.
	peerGetGlobalKeysCallback callback,  // Called with the keys.
	void * param,  // Passed to callback.
	PEERBool blocking  // If PEERTrue, don't return until finished.
);

void peerSetRoomKeys
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room to set the keys in.
	const char * nick,  // The player to set the keys on (NULL or "" for the room).
	int num,  // The number of keys.
	const char ** keys,  // The keys to set.
	const char ** values  // The values to set.
);

typedef void (* peerGetRoomKeysCallback)
(
	PEER peer,  // The peer object.
	PEERBool success,  // If PEERFalse, unable to get the keys.
	RoomType roomType,  // The room the keys are in.
	const char * nick,  // The player the keys are for, or NULL for the room.
	int num,  // The number of keys.
	char ** keys,  // The keys.
	char ** values,  // The values for the keys.
	void * param  // User-data.
);

void peerGetRoomKeys
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room to get the keys in.
	const char * nick,  // The player to get the keys for.
	int num,  // The number of keys.
	const char ** keys,  // The keys to get.
	peerGetRoomKeysCallback callback,  // Called with the keys.
	void * param,  // Passed to callback.
	PEERBool blocking  // If PEERTrue, don't return until finished.
);

void peerSetGlobalWatchKeys
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room to set the watch keys for.
	int num,  // The number of keys.
	const char ** keys,  // The keys to watch for.
	PEERBool addKeys  // If PEERTrue, add these keys to the existing global watch keys for this room.
);

void peerSetRoomWatchKeys
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The type of room to set the watch keys for.
	int num,  // The number of keys.
	const char ** keys,  // The keys to watch for.
	PEERBool addKeys  // If PEERTrue, add these keys to the existing room watch keys for this room.
);

const char * peerGetGlobalWatchKey
(
	PEER peer,  // The peer object.
	const char * nick,  // The player to get the key for.
	const char * key  // The key to get.
);

const char * peerGetRoomWatchKey
(
	PEER peer,  // The peer object.
	RoomType roomType,  // The room to get the key in.
	const char * nick,  // The player to get the key for.
	const char * key  // The key to get.
);

#ifdef __cplusplus
}
#endif

#endif
